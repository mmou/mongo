/**
*    Copyright (C) 2014 10gen Inc.
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

#pragma once

#include <functional>
#include <string>
#include <vector>

namespace mongo {

namespace mutablebson {
class Document;
class Element;
}

/**
 * Call this to redact a mutable BSON object.
 * redactFields is a vector of fields to redact. They can use dot notation;
 * they must be "full path names." If no fields are specified, the default
 * is to redact all fields (except $comment). $comment fields are NEVER
 * redacted. See unit tests for usage examples and expected behavior.
 * redactFields is a function that takes in an element and returns the
 * redacted value for that element. (Side note, redactFields is not
 * expected to be very large, maybe 1-3 elements.)
 */
void redactDocumentForLogging(
    mutablebson::Document* cmdObj,
    const std::function<std::string(mutablebson::Element*)>& getRedactedValue,
    const std::vector<std::string>& redactFields = {""});

/**
 * Given a (any) mutable element, simply returns "***".
 */
std::string simpleRedactFieldValue(mutablebson::Element* current);

/**
 * Given a (any) mutable element, returns SHA1 hash of value as string.
 */
std::string hashRedactFieldValue(mutablebson::Element* current);
}