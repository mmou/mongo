/**
 *    Copyright (C) 2013 10gen Inc.
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
#include "mongo/bson/json.h"
#include "mongo/unittest/unittest.h"

namespace mongo {

namespace {

/**
 * Given a mutable element, replaces value with "***". If already "***", replaces it with "XXX".
 * Used by curop_test.cpp to determine if a value is being redacted more than once.
 */
std::string safeRedactFieldValue(mutablebson::Element* current) {
    BSONType t = current->getType();
    if (t == String) {
        ASSERT_TRUE(std::strncmp(current->getValue().valuestrsafe(), "***", 3) != 0);
    }
    return simpleRedactFieldValue(current);
}


TEST(redactDocumentForLogging, BasicRedactAll) {
    static const char jsonSample[] = "{field1: \"value1\"}";
    static const char redactedSample[] = "{field1: \"***\"}";

    mongo::BSONObj obj = mongo::fromjson(jsonSample);
    mongo::BSONObj robj = mongo::fromjson(redactedSample);

    mutablebson::Document doc(obj);
    mongo::redactDocumentForLogging(&doc, safeRedactFieldValue);
    ASSERT_EQUALS(doc.getObject(), robj);
}

TEST(redactDocumentForLogging, BasicRedactSome) {
    static const char jsonSample[] =
        "{field1: \"value1\","
        "field2: \"value2\"}";
    static const char redactedSample[] =
        "{field1: \"value1\","
        "field2: \"***\"}";

    mongo::BSONObj obj = mongo::fromjson(jsonSample);
    mongo::BSONObj robj = mongo::fromjson(redactedSample);

    mutablebson::Document doc(obj);
    mongo::redactDocumentForLogging(&doc, safeRedactFieldValue, std::vector<std::string>{"field2"});
    ASSERT_EQUALS(doc.getObject(), robj);
}

TEST(redactDocumentForLogging, RedactEmptyObjects) {
    static const char jsonSample[] = "{}";
    static const char redactedSample[] = "{}";

    mongo::BSONObj obj = mongo::fromjson(jsonSample);
    mongo::BSONObj robj = mongo::fromjson(redactedSample);

    mutablebson::Document doc(obj);
    mongo::redactDocumentForLogging(&doc, safeRedactFieldValue);
    ASSERT_EQUALS(doc.getObject(), robj);
}


TEST(redactDocumentForLogging, RedactEmptyNestedObjects) {
    static const char jsonSample[] = "{field1: {}}";

    static const char redactedSample[] = "{field1: {}}";

    mongo::BSONObj obj = mongo::fromjson(jsonSample);
    mongo::BSONObj robj = mongo::fromjson(redactedSample);

    mutablebson::Document doc(obj);
    mongo::redactDocumentForLogging(&doc, safeRedactFieldValue);
    ASSERT_EQUALS(doc.getObject(), robj);
}


TEST(redactDocumentForLogging, RedactEmptyNestedArrays) {
    static const char jsonSample[] = "{field1: []}";

    static const char redactedSample[] = "{field1: []}";

    mongo::BSONObj obj = mongo::fromjson(jsonSample);
    mongo::BSONObj robj = mongo::fromjson(redactedSample);

    mutablebson::Document doc(obj);
    mongo::redactDocumentForLogging(&doc, safeRedactFieldValue);
    ASSERT_EQUALS(doc.getObject(), robj);
}

TEST(redactDocumentForLogging, RedactEmptyNestedObjectsArrays) {
    static const char jsonSample[] = "{field1: [{}, []]}";

    static const char redactedSample[] = "{field1: [{}, []]}";

    mongo::BSONObj obj = mongo::fromjson(jsonSample);
    mongo::BSONObj robj = mongo::fromjson(redactedSample);

    mutablebson::Document doc(obj);
    mongo::redactDocumentForLogging(&doc, safeRedactFieldValue);
    ASSERT_EQUALS(doc.getObject(), robj);
}

TEST(redactDocumentForLogging, NestedObjectsRedactAll) {
    static const char jsonSample[] =
        "{field1:"
        "{field1: \"value1\","
        "field2: \"value2\"},"
        "field2:"
        "{field1:"
        "{field1: \"value1\","
        "field2:"
        "{field1: \"value1\"}"
        "}"
        "},"
        "field3: \"value3\"}";

    static const char redactedSample[] =
        "{field1:"
        "{field1: \"***\","
        "field2: \"***\"},"
        "field2:"
        "{field1:"
        "{field1: \"***\","
        "field2:"
        "{field1: \"***\"}"
        "}"
        "},"
        "field3: \"***\"}";

    mongo::BSONObj obj = mongo::fromjson(jsonSample);
    mongo::BSONObj robj = mongo::fromjson(redactedSample);

    mutablebson::Document doc(obj);
    mongo::redactDocumentForLogging(&doc, safeRedactFieldValue);
    ASSERT_EQUALS(doc.getObject(), robj);
}


TEST(redactDocumentForLogging, NestedObjectsRedactSomeA) {
    static const char jsonSample[] =
        "{field1:"
        "{field1: \"value1\","
        "field2: \"value2\"},"
        "field2:"
        "{field1:"
        "{field1: \"value1\","
        "field2:"
        "{field1: \"value1\"}"
        "}"
        "},"
        "field3: \"value3\"}";

    static const char redactedSample[] =
        "{field1:"
        "{field1: \"***\","
        "field2: \"***\"},"
        "field2:"
        "{field1:"
        "{field1: \"value1\","
        "field2:"
        "{field1: \"***\"}"
        "}"
        "},"
        "field3: \"value3\"}";

    mongo::BSONObj obj = mongo::fromjson(jsonSample);
    mongo::BSONObj robj = mongo::fromjson(redactedSample);

    mutablebson::Document doc(obj);
    mongo::redactDocumentForLogging(
        &doc, safeRedactFieldValue, std::vector<std::string>{"field1", "field2.field1.field2"});
    ASSERT_EQUALS(doc.getObject(), robj);
}

TEST(redactDocumentForLogging, NestedObjectsRedactSomeB) {
    static const char jsonSample[] =
        "{field1:"
        "{field1: \"value1\","
        "field2: \"value2\"},"
        "field2:"
        "{field1:"
        "{field1: \"value1\","
        "field2:"
        "{field1: \"value1\"}"
        "}"
        "},"
        "field3: \"value3\"}";

    static const char redactedSample[] =
        "{field1:"
        "{field1: \"value1\","
        "field2: \"***\"},"
        "field2:"
        "{field1:"
        "{field1: \"***\","
        "field2:"
        "{field1: \"value1\"}"
        "}"
        "},"
        "field3: \"value3\"}";

    mongo::BSONObj obj = mongo::fromjson(jsonSample);
    mongo::BSONObj robj = mongo::fromjson(redactedSample);

    mutablebson::Document doc(obj);
    mongo::redactDocumentForLogging(
        &doc,
        safeRedactFieldValue,
        std::vector<std::string>{"field1.field2", "field2.field1.field1"});
    ASSERT_EQUALS(doc.getObject(), robj);
}

TEST(redactDocumentForLogging, CommentsNeverRedact) {
    static const char jsonSample[] =
        "{field1:"
        "{$comment: \"value1\","
        "field2: \"value2\"},"
        "field2:"
        "{field1:"
        "{$comment: \"value1\","
        "field2:"
        "{field1: \"value1\"}"
        "}"
        "},"
        "field3: \"value3\"}";

    static const char redactedSample[] =
        "{field1:"
        "{$comment: \"value1\","
        "field2: \"***\"},"
        "field2:"
        "{field1:"
        "{$comment: \"value1\","
        "field2:"
        "{field1: \"***\"}"
        "}"
        "},"
        "field3: \"***\"}";

    mongo::BSONObj obj = mongo::fromjson(jsonSample);
    mongo::BSONObj robj = mongo::fromjson(redactedSample);

    mutablebson::Document doc(obj);
    mongo::redactDocumentForLogging(&doc, safeRedactFieldValue);
    ASSERT_EQUALS(doc.getObject(), robj);
}

TEST(redactDocumentForLogging, ArraysRedactAll) {
    static const char jsonSample[] =
        "{field1:"
        "{field1: [\"a\", \"b\", \"c\", \"d\"],"
        "field2: \"value2\"},"
        "field2:"
        "{field1:"
        "{field1: \"value1\","
        "field2:"
        "{field1: [\"a\", \"b\"]}"
        "}"
        "},"
        "field3: \"value3\"}";

    static const char redactedSample[] =
        "{field1:"
        "{field1: [\"***\", \"***\", \"***\", \"***\"],"
        "field2: \"***\"},"
        "field2:"
        "{field1:"
        "{field1: \"***\","
        "field2:"
        "{field1: [\"***\", \"***\"]}"
        "}"
        "},"
        "field3: \"***\"}";

    mongo::BSONObj obj = mongo::fromjson(jsonSample);
    mongo::BSONObj robj = mongo::fromjson(redactedSample);

    mutablebson::Document doc(obj);
    mongo::redactDocumentForLogging(&doc, safeRedactFieldValue);
    ASSERT_EQUALS(doc.getObject(), robj);
}

TEST(redactDocumentForLogging, ArraysRedactSome) {
    static const char jsonSample[] =
        "{field1:"
        "{field1: [\"a\", \"b\", \"c\", \"d\"],"
        "field2: \"value2\"},"
        "field2:"
        "{field1:"
        "{field1: \"value1\","
        "field2:"
        "{field1: [\"a\", \"b\"]}"
        "}"
        "},"
        "field3: \"value3\"}";

    static const char redactedSample[] =
        "{field1:"
        "{field1: [\"a\", \"b\", \"c\", \"d\"],"
        "field2: \"***\"},"
        "field2:"
        "{field1:"
        "{field1: \"***\","
        "field2:"
        "{field1: [\"***\", \"***\"]}"
        "}"
        "},"
        "field3: \"value3\"}";

    mongo::BSONObj obj = mongo::fromjson(jsonSample);
    mongo::BSONObj robj = mongo::fromjson(redactedSample);

    mutablebson::Document doc(obj);
    mongo::redactDocumentForLogging(
        &doc, safeRedactFieldValue, std::vector<std::string>{"field1.field2", "field2.field1"});
    ASSERT_EQUALS(doc.getObject(), robj);
}


TEST(redactDocumentForLogging, ObjectsArraysRedactAll) {
    static const char jsonSample[] =
        "{field1:"
        "{field1: [{field1: [{field1: \"value1\"}, [\"a\", \"b\"]]}, {field2: [{field1: "
        "\"value1\"}, "
        "[\"a\", {field2: \"value2\"}]]}, {field3: [\"a\", {field1: \"value1\"}]}],"
        "field2: [[\"a\", [\"a\", \"b\"], {field3: \"value3\"}]]}}";

    static const char redactedSample[] =
        "{field1:"
        "{field1: [{field1: [{field1: \"***\"}, [\"***\", \"***\"]]}, {field2: [{field1: \"***\"}, "
        "[\"***\", {field2: \"***\"}]]}, {field3: [\"***\", {field1: \"***\"}]}],"
        "field2: [[\"***\", [\"***\", \"***\"], {field3: \"***\"}]]}}";

    mongo::BSONObj obj = mongo::fromjson(jsonSample);
    mongo::BSONObj robj = mongo::fromjson(redactedSample);

    mutablebson::Document doc(obj);
    mongo::redactDocumentForLogging(&doc, safeRedactFieldValue);
    ASSERT_EQUALS(doc.getObject(), robj);
}

TEST(redactDocumentForLogging, ObjectsArraysRedactSome) {
    static const char jsonSample[] =
        "{field1:"
        "{field1: [{field1: [{field1: \"value1\"}, [\"a\", \"b\"]]}, {field2: [{field1: "
        "\"value1\"}, "
        "[\"a\", {field2: \"value2\"}]]}, {field3: [\"a\", {field1: \"value1\"}]}],"
        "field2: [[\"a\", [\"a\", \"b\"], {field3: \"value3\"}]]}}";

    static const char redactedSample[] =
        "{field1:"
        "{field1: [{field1: [{field1: \"***\"}, [\"a\", \"b\"]]}, {field2: [{field1: \"value1\"}, "
        "[\"a\", "
        "{field2: \"value2\"}]]}, {field3: [\"***\", {field1: \"***\"}]}],"
        "field2: [[\"***\", [\"***\", \"***\"], {field3: \"***\"}]]}}";

    mongo::BSONObj obj = mongo::fromjson(jsonSample);
    mongo::BSONObj robj = mongo::fromjson(redactedSample);

    mutablebson::Document doc(obj);
    mongo::redactDocumentForLogging(&doc,
                                    safeRedactFieldValue,
                                    std::vector<std::string>{"field1.field1.field1.field1",
                                                             "field1.field1.field3",
                                                             "field1.field2"});
    ASSERT_EQUALS(doc.getObject(), robj);
}

TEST(redactDocumentForLogging, IgnoreNonexistentFields) {
    static const char jsonSample[] =
        "{field1:"
        "{field1: [{field1: [{field1: \"value1\"}, [\"a\", \"b\"]]}, {field2: [{field1: "
        "\"value1\"}, "
        "[\"a\", {field2: \"value2\"}]]}, {field3: [\"a\", {field1: \"value1\"}]}],"
        "field2: [[\"a\", [\"a\", \"b\"], {field3: \"value3\"}]]}}";

    static const char redactedSample[] =
        "{field1:"
        "{field1: [{field1: [{field1: \"value1\"}, [\"a\", \"b\"]]}, {field2: [{field1: "
        "\"value1\"}, "
        "[\"a\", {field2: \"value2\"}]]}, {field3: [\"a\", {field1: \"value1\"}]}],"
        "field2: [[\"a\", [\"a\", \"b\"], {field3: \"value3\"}]]}}";


    mongo::BSONObj obj = mongo::fromjson(jsonSample);
    mongo::BSONObj robj = mongo::fromjson(redactedSample);

    mutablebson::Document doc(obj);
    mongo::redactDocumentForLogging(&doc,
                                    safeRedactFieldValue,
                                    std::vector<std::string>{"field1.field1.field1.field2",
                                                             "field1.field1.field3.field5",
                                                             "field1.x"});
    ASSERT_EQUALS(doc.getObject(), robj);
}

}  // namespace
}  // namespace