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

#include "mongo/db/jsobj.h"
#include "mongo/db/repl/read_after_optime_args.h"
#include "mongo/unittest/unittest.h"

namespace mongo {
namespace repl {
namespace {

TEST(ReadAfterParse, BasicFullSpecification) {
    ReadAfterOpTimeArgs readAfterOpTime;
    ASSERT_OK(readAfterOpTime.initialize(BSON(
        "find"
        << "test" << ReadAfterOpTimeArgs::kRootFieldName
        << BSON(ReadAfterOpTimeArgs::kOpTimeFieldName
                << BSON(ReadAfterOpTimeArgs::kOpTimestampFieldName
                        << Timestamp(20, 30) << ReadAfterOpTimeArgs::kOpTermFieldName << 2)))));

    ASSERT_EQ(Timestamp(20, 30), readAfterOpTime.getOpTime().getTimestamp());
    ASSERT_EQ(2, readAfterOpTime.getOpTime().getTerm());
    ASSERT_FALSE(readAfterOpTime.isReadCommitted());
}

TEST(ReadAfterParse, ReadCommittedFullSpecification) {
    ReadAfterOpTimeArgs readAfterOpTime;
    ASSERT_OK(readAfterOpTime.initialize(BSON("find"
                                              << "test" << ReadAfterOpTimeArgs::kRootFieldName
                                              << BSON(ReadAfterOpTimeArgs::kOpTimeFieldName << BSON(
                                                          ReadAfterOpTimeArgs::kOpTimestampFieldName
                                                          << Timestamp(20, 30)
                                                          << ReadAfterOpTimeArgs::kOpTermFieldName
                                                          << 2)) << "committed" << true)));

    ASSERT_EQ(Timestamp(20, 30), readAfterOpTime.getOpTime().getTimestamp());
    ASSERT_EQ(2, readAfterOpTime.getOpTime().getTerm());
    ASSERT(readAfterOpTime.isReadCommitted());
}

TEST(ReadAfterParse, Empty) {
    ReadAfterOpTimeArgs readAfterOpTime;
    ASSERT_OK(readAfterOpTime.initialize(BSON("find"
                                              << "test")));

    ASSERT(readAfterOpTime.getOpTime().getTimestamp().isNull());
}

TEST(ReadAfterParse, BadRootType) {
    ReadAfterOpTimeArgs readAfterOpTime;
    ASSERT_NOT_OK(
        readAfterOpTime.initialize(BSON("find"
                                        << "test" << ReadAfterOpTimeArgs::kRootFieldName << "x")));
}

TEST(ReadAfterParse, BadOpTimeType) {
    ReadAfterOpTimeArgs readAfterOpTime;
    ASSERT_NOT_OK(
        readAfterOpTime.initialize(BSON("find"
                                        << "test" << ReadAfterOpTimeArgs::kRootFieldName
                                        << BSON(ReadAfterOpTimeArgs::kOpTimeFieldName << 2))));
}

TEST(ReadAfterParse, OpTimeRequiredIfRootPresent) {
    ReadAfterOpTimeArgs readAfterOpTime;
    ASSERT_NOT_OK(readAfterOpTime.initialize(BSON("find"
                                                  << "test" << ReadAfterOpTimeArgs::kRootFieldName
                                                  << BSONObj())));
}

TEST(ReadAfterParse, NoOpTimeTS) {
    ReadAfterOpTimeArgs readAfterOpTime;
    ASSERT_NOT_OK(
        readAfterOpTime.initialize(BSON("find"
                                        << "test" << ReadAfterOpTimeArgs::kRootFieldName
                                        << BSON(ReadAfterOpTimeArgs::kOpTimeFieldName << BSON(
                                                    ReadAfterOpTimeArgs::kOpTermFieldName << 2)))));
}

TEST(ReadAfterParse, NoOpTimeTerm) {
    ReadAfterOpTimeArgs readAfterOpTime;
    ASSERT_NOT_OK(
        readAfterOpTime.initialize(BSON("find"
                                        << "test" << ReadAfterOpTimeArgs::kRootFieldName
                                        << BSON(ReadAfterOpTimeArgs::kOpTimeFieldName << BSON(
                                                    ReadAfterOpTimeArgs::kOpTermFieldName << 2)))));
}

TEST(ReadAfterParse, BadOpTimeTSType) {
    ReadAfterOpTimeArgs readAfterOpTime;
    ASSERT_NOT_OK(readAfterOpTime.initialize(
        BSON("find"
             << "test" << ReadAfterOpTimeArgs::kRootFieldName
             << BSON(ReadAfterOpTimeArgs::kOpTimeFieldName
                     << BSON(ReadAfterOpTimeArgs::kOpTimestampFieldName
                             << BSON("x" << 1) << ReadAfterOpTimeArgs::kOpTermFieldName << 2)))));
}

TEST(ReadAfterParse, BadOpTimeTermType) {
    ReadAfterOpTimeArgs readAfterOpTime;
    ASSERT_NOT_OK(readAfterOpTime.initialize(BSON(
        "find"
        << "test" << ReadAfterOpTimeArgs::kRootFieldName
        << BSON(ReadAfterOpTimeArgs::kOpTimeFieldName
                << BSON(ReadAfterOpTimeArgs::kOpTimestampFieldName
                        << Timestamp(1, 0) << ReadAfterOpTimeArgs::kOpTermFieldName << "y")))));
}

}  // unnamed namespace
}  // namespace repl
}  // namespace mongo
