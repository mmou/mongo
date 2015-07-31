/**
 *    Copyright (C) 2015 MongoDB Inc.
 *
 *    This program is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the GNU Affero General Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#include "mongo/platform/basic.h"

#include "mongo/client/remote_command_runner_impl.h"

#include "mongo/base/status_with.h"
#include "mongo/db/commands.h"
#include "mongo/db/namespace_string.h"
#include "mongo/db/query/cursor_responses.h"
#include "mongo/db/query/getmore_request.h"
#include "mongo/executor/network_connection_hook.h"
#include "mongo/rpc/get_status_from_command_result.h"
#include "mongo/rpc/protocol.h"
#include "mongo/util/assert_util.h"

namespace mongo {
namespace {

using executor::RemoteCommandRequest;
using executor::RemoteCommandResponse;

/**
 * Calculates the timeout for a network operation expiring at "expDate", given
 * that it is now "nowDate".
 *
 * Returns 0ms to indicate no expiration date, a number of milliseconds until "expDate", or
 * ErrorCodes::ExceededTimeLimit if "expDate" is not later than "nowDate".
 */
StatusWith<Milliseconds> getTimeoutMillis(const Date_t expDate, const Date_t nowDate) {
    if (expDate == RemoteCommandRequest::kNoExpirationDate) {
        return Milliseconds(0);
    }
    if (expDate <= nowDate) {
        return {ErrorCodes::ExceededTimeLimit,
                str::stream() << "Went to run command, but it was too late. "
                                 "Expiration was set to " << dateToISOStringUTC(expDate)};
    }
    return expDate - nowDate;
}

/**
 * Updates command output document with status.
 */
BSONObj getCommandResultFromStatus(const Status& status) {
    BSONObjBuilder result;
    Command::appendCommandStatus(result, status);
    return result.obj();
}

/**
 * Peeks at error in cursor. If an error has occurred, converts the {$err: "...", code: N}
 * cursor error to a Status.
 */
Status getStatusFromCursorResult(DBClientCursor& cursor) {
    BSONObj error;
    if (!cursor.peekError(&error)) {
        return Status::OK();
    }

    BSONElement e = error.getField("code");
    return Status(e.isNumber() ? ErrorCodes::fromInt(e.numberInt()) : ErrorCodes::UnknownError,
                  getErrField(error).valuestrsafe());
}

/**
 * Downconverts the specified find command to a find protocol operation and sends it to the
 * server on the specified connection.
 */
Status runDownconvertedFindCommand(DBClientConnection* conn,
                                   const std::string& dbname,
                                   const BSONObj& cmdObj,
                                   BSONObj* output) {
    const NamespaceString nss(dbname, cmdObj.firstElement().String());
    if (!nss.isValid()) {
        return {ErrorCodes::InvalidNamespace,
                str::stream() << "Invalid collection name: " << nss.ns()};
    }
    const std::string& ns = nss.ns();

    // It is a little heavy handed to use LiteParsedQuery to convert the command object to
    // query() arguments but we get validation and consistent behavior with the find
    // command implementation on the remote server.
    auto lpqStatus = LiteParsedQuery::makeFromFindCommand(nss, cmdObj, false);
    if (!lpqStatus.isOK()) {
        *output = getCommandResultFromStatus(lpqStatus.getStatus());
        return lpqStatus.getStatus();
    }

    auto& lpq = lpqStatus.getValue();

    Query query(lpq->getFilter());
    if (!lpq->getSort().isEmpty()) {
        query.sort(lpq->getSort());
    }
    if (!lpq->getHint().isEmpty()) {
        query.hint(lpq->getHint());
    }
    if (!lpq->getMin().isEmpty()) {
        query.minKey(lpq->getMin());
    }
    if (!lpq->getMax().isEmpty()) {
        query.minKey(lpq->getMax());
    }
    if (lpq->isExplain()) {
        query.explain();
    }
    if (lpq->isSnapshot()) {
        query.snapshot();
    }
    int nToReturn = lpq->getLimit().value_or(0) * -1;
    int nToSkip = lpq->getSkip().value_or(0);
    const BSONObj* fieldsToReturn = &lpq->getProj();
    int queryOptions = lpq->getOptions();
    int batchSize = lpq->getBatchSize().value_or(0);

    std::unique_ptr<DBClientCursor> cursor =
        conn->query(ns, query, nToReturn, nToSkip, fieldsToReturn, queryOptions, batchSize);

    if (!cursor) {
        return {ErrorCodes::HostUnreachable,
                str::stream() << "cursor initialization failed due to connection problems with "
                              << conn->getServerAddress()};
    }

    cursor->decouple();

    Status status = getStatusFromCursorResult(*cursor);
    if (!status.isOK()) {
        *output = getCommandResultFromStatus(status);
        return status;
    }

    BSONArrayBuilder batch;
    while (cursor->moreInCurrentBatch()) {
        batch.append(cursor->next());
    }

    BSONObjBuilder result;
    appendCursorResponseObject(cursor->getCursorId(), ns, batch.arr(), &result);
    Command::appendCommandStatus(result, Status::OK());
    *output = result.obj();
    return Status::OK();
}

/**
 * Downconverts the specified getMore command to legacy getMore operation and sends it to the
 * server on the specified connection.
 */
Status runDownconvertedGetMoreCommand(DBClientConnection* conn,
                                      const std::string& dbname,
                                      const BSONObj& cmdObj,
                                      BSONObj* output) {
    StatusWith<GetMoreRequest> parseResult = GetMoreRequest::parseFromBSON(dbname, cmdObj);
    if (!parseResult.isOK()) {
        const Status& status = parseResult.getStatus();
        *output = getCommandResultFromStatus(status);
        return status;
    }

    const GetMoreRequest& req = parseResult.getValue();
    const std::string& ns = req.nss.ns();

    std::unique_ptr<DBClientCursor> cursor =
        conn->getMore(ns, req.cursorid, req.batchSize.value_or(0));

    if (!cursor) {
        return {ErrorCodes::HostUnreachable,
                str::stream() << "cursor initialization failed due to connection problems with "
                              << conn->getServerAddress()};
    }

    cursor->decouple();

    Status status = getStatusFromCursorResult(*cursor);
    if (!status.isOK()) {
        *output = getCommandResultFromStatus(status);
        return status;
    }

    BSONArrayBuilder batch;
    while (cursor->moreInCurrentBatch()) {
        batch.append(cursor->next());
    }

    BSONObjBuilder result;
    appendGetMoreResponseObject(cursor->getCursorId(), ns, batch.arr(), &result);
    Command::appendCommandStatus(result, Status::OK());
    *output = result.obj();
    return Status::OK();
}

}  // namespace

RemoteCommandRunnerImpl::RemoteCommandRunnerImpl(
    int messagingTags, std::unique_ptr<executor::NetworkConnectionHook> hook)
    : _connPool(messagingTags, std::move(hook)) {}

RemoteCommandRunnerImpl::~RemoteCommandRunnerImpl() {
    invariant(!_active);
}

void RemoteCommandRunnerImpl::startup() {
    _active = true;
}

void RemoteCommandRunnerImpl::shutdown() {
    if (!_active) {
        return;
    }

    _connPool.closeAllInUseConnections();
    _active = false;
}

StatusWith<RemoteCommandResponse> RemoteCommandRunnerImpl::runCommand(
    const RemoteCommandRequest& request) {
    try {
        const Date_t requestStartDate = Date_t::now();
        const auto timeoutMillis = getTimeoutMillis(request.expirationDate, requestStartDate);
        if (!timeoutMillis.isOK()) {
            return StatusWith<RemoteCommandResponse>(timeoutMillis.getStatus());
        }

        ConnectionPool::ConnectionPtr conn(
            &_connPool, request.target, requestStartDate, timeoutMillis.getValue());

        BSONObj output;
        BSONObj metadata;

        // If remote server does not support either find or getMore commands, down convert
        // to using DBClientInterface::query()/getMore().
        // Perform down conversion based on wire protocol version.

        // 'commandName' will be an empty string if the command object is an empty BSON
        // document.
        StringData commandName = request.cmdObj.firstElement().fieldNameStringData();
        const auto isFindCmd = commandName == LiteParsedQuery::kFindCommandName;
        const auto isGetMoreCmd = commandName == GetMoreRequest::kGetMoreCommandName;
        const auto isFindOrGetMoreCmd = isFindCmd || isGetMoreCmd;

        // We are using the wire version to check if we need to downconverting find/getMore
        // requests because coincidentally, the find/getMore command is only supported by
        // servers that also accept OP_COMMAND.
        bool supportsFindAndGetMoreCommands = rpc::supportsWireVersionForOpCommandInMongod(
            conn.get()->getMinWireVersion(), conn.get()->getMaxWireVersion());

        if (!isFindOrGetMoreCmd || supportsFindAndGetMoreCommands) {
            rpc::UniqueReply commandResponse =
                conn.get()->runCommandWithMetadata(request.dbname,
                                                   request.cmdObj.firstElementFieldName(),
                                                   request.metadata,
                                                   request.cmdObj);

            output = commandResponse->getCommandReply().getOwned();
            metadata = commandResponse->getMetadata().getOwned();
        } else if (isFindCmd) {
            runDownconvertedFindCommand(conn.get(), request.dbname, request.cmdObj, &output);
        } else if (isGetMoreCmd) {
            runDownconvertedGetMoreCommand(conn.get(), request.dbname, request.cmdObj, &output);
        }

        const Date_t requestFinishDate = Date_t::now();
        conn.done(requestFinishDate);

        return StatusWith<RemoteCommandResponse>(
            RemoteCommandResponse(std::move(output),
                                  std::move(metadata),
                                  Milliseconds(requestFinishDate - requestStartDate)));
    } catch (const DBException& ex) {
        return StatusWith<RemoteCommandResponse>(ex.toStatus());
    } catch (const std::exception& ex) {
        return StatusWith<RemoteCommandResponse>(
            ErrorCodes::UnknownError,
            str::stream() << "Sending command " << request.cmdObj << " on database "
                          << request.dbname << " over network to " << request.target.toString()
                          << " received exception " << ex.what());
    }
}

}  // namespace mongo
