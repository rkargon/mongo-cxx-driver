#include <cassert>
#include <cstdio>
#include <iostream>
#include <streambuf>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

#include "bson_streambuf.hpp"

bson_output_streambuf::bson_output_streambuf(mongocxx::collection *coll){
    this->coll = coll;
}

bson_output_streambuf::~bson_output_streambuf(){
    if (data != nullptr){
        delete[] data;
    }
}

int bson_output_streambuf::underflow(){
    return EOF;
}

int bson_output_streambuf::overflow(int ch){
    int result = EOF;
    assert(ch >= 0 && ch <= UCHAR_MAX);
    result = insert(ch);
    return result;
}

int bson_output_streambuf::insert(int ch){
    bytes_read++;

    // for first four bytes, build int32 that contains document size
    if (bytes_read <= 4){
        len += (ch << (8 * (bytes_read-1)));
    }
    // once doc size is received, allocate space.
    if (bytes_read == 4){
        // TODO perhaps fail unless 0 < len <= MAX_BSON_SIZE?
        data = new uint8_t[len];
        *(size_t *)data = len;
    }

    if (bytes_read > 4){
        data[bytes_read-1] = (uint8_t)ch;
    }

    if (bytes_read == len){
        // insert document, reset data and length
        bsoncxx::document::view v(data, len);
        coll->insert_one(v);
        delete[] data;
        data = nullptr;
        bytes_read = 0;
        len = 0;
    } else if (bytes_read > len && len >= 4){
        return EOF;
    }
    return ch;
}

int main(int argc, char const *argv[]) {
    // init database
    mongocxx::instance inst{};
    mongocxx::client conn{mongocxx::uri{}};
    auto collection = conn["testdb"]["testcollection"];

     // set up test BSON document
    std::string json_str = "{\"a\": 1, \"b\":[1,2,3], \"c\": {\"a\": 1}}";
    auto bson_obj = bsoncxx::from_json(json_str);
    const uint8_t *data = bson_obj.view().data();

    // set up stream
    bson_output_streambuf doc_streambuf(&collection);
    std::ostream doc_stream(&doc_streambuf);

    // print coll's document count before insert
    auto cursor = collection.find({});
    int size = std::distance(cursor.begin(), cursor.end());
    std::cout << "Before insert: " << size << std::endl;

    // write document to stream
    int len = *(int *)data;
    doc_stream.write((const char *)data, len);

    // print coll's document count after insert
    cursor = collection.find({});
    size = std::distance(cursor.begin(), cursor.end());
    std::cout << "After 1st insert: " << size << std::endl;

    doc_stream.write((const char *)data, len);

    // print coll's document count after insert
    cursor = collection.find({});
    size = std::distance(cursor.begin(), cursor.end());
    std::cout << "After 2nd insert: " << size << std::endl;
     return 0;
 }
