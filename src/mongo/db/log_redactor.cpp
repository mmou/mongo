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

#include "mongo/db/log_redactor.h"

#include "mongo/bson/mutable/document.h"
#include "mongo/crypto/crypto.h"
#include "mongo/util/hex.h"

namespace mongo {

namespace {

/**
 * Given a mutable element that is not an object or array, redacts its value. It replaces the value
 * with the output of the function getRedactedValue, which takes in the current Element. It never
 * redacts fields with the field name '$comment'.
 */
void redactFieldValue(mutablebson::Element* current,
                      const std::function<std::string(mutablebson::Element*)>& getRedactedValue) {
    StringData thisFieldName = current->getFieldName();

    if (thisFieldName != "$comment") {  // never redact
        current->setValueString(StringData(getRedactedValue(current)));
    }
}

/**
 * Recursively iterates through all fields of a given mutable element, and redacts
 * the specified fields using the specified getRedactedValue function.
 * matchingPath is either the full path up to the current element, or the full path up the
 * first element at which it matches a field in redactFields (in other words, we do not
 * append to matchingPath anymore after a complete match between the current matchingPath
 * and some member of redactFields is found.)
 * isArrayMember is true if the current element is a member of an array.
 */
void redactDocumentForLoggingHelper(
    mutablebson::Element* current,
    const std::function<std::string(mutablebson::Element*)>& getRedactedValue,
    const std::vector<std::string>& redactFields,
    const std::string& matchingPath,
    bool isArrayMember = false) {
    std::string separator = (matchingPath.length() == 0) ? "" : ".";
    std::string separatedMatchingPath = matchingPath + separator;

    BSONType type = current->getType();
    if (!(type == Object || type == Array)) {
        return redactFieldValue(current, getRedactedValue);
    }

    mutablebson::Element newCurrent = current->leftChild();
    current = &newCurrent;
    while (current->ok()) {
        type = current->getType();

        // check if in an array, and is an object/array
        if (isArrayMember && (type == Object || type == Array)) {
            // tunnel down, since array field names are meaningless (they are just indices)
            redactDocumentForLoggingHelper(
                current, getRedactedValue, redactFields, matchingPath, (type == Array));
            *current = current->rightSibling();
            continue;
        }

        // check if complete match with some element in redactFields
        if (std::find(redactFields.begin(), redactFields.end(), matchingPath) !=
            redactFields.end()) {
            // then redact.
            redactDocumentForLoggingHelper(
                current, getRedactedValue, redactFields, matchingPath, (type == Array));
            *current = current->rightSibling();
            continue;
        }

        // check if partial prefix match with some element in redactFields
        StringData thisFieldName = current->getFieldName();
        if (std::find_if(
                redactFields.begin(),
                redactFields.end(),
                [&matchingPath, &thisFieldName, &separator](
                    const std::string& redactField) -> bool {
                    // if matchingPath is a prefix of some element in redactFields
                    if (std::equal(matchingPath.begin(), matchingPath.end(), redactField.begin())) {
                        // return true if appending this FieldName to matchingPath will still be a
                        // prefix of this element in redactFields (without creating any new strings)
                        return redactField.compare(matchingPath.length() + separator.length(),
                                                   thisFieldName.size(),
                                                   thisFieldName.rawData(),
                                                   thisFieldName.size()) == 0;
                    }
                    return false;
                }) != redactFields.end()) {
            // then redact.
            std::string newMatchingPath = separatedMatchingPath + thisFieldName.toString();
            redactDocumentForLoggingHelper(current,
                                           getRedactedValue,
                                           redactFields,
                                           std::move(newMatchingPath),
                                           (type == Array));
        }
        *current = current->rightSibling();
    }
}

}  // namespace

std::string simpleRedactFieldValue(mutablebson::Element* current) {
    return "***";
}

std::string hashRedactFieldValue(mutablebson::Element* current) {
    BSONElement element = current->getValue();
    const unsigned char* value = reinterpret_cast<const unsigned char*>(element.value());
    unsigned char hash[20];
    if (crypto::sha1(value, element.valuesize(), hash)) {
        return toHexLower(reinterpret_cast<const void*>(hash), 20);
        //        return std::string((char*)hash, 20);
    } else {
        return "***";
    }
}

void redactDocumentForLogging(
    mutablebson::Document* cmdObj,
    const std::function<std::string(mutablebson::Element*)>& getRedactedValue,
    const std::vector<std::string>& redactFields) {
    mutablebson::Element current = cmdObj->root();
    if (current.getType() == Object) {  // should always be true
        redactDocumentForLoggingHelper(&current, getRedactedValue, redactFields, "");
    }
}

}  // namespace
