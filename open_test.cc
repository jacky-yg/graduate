#include <iostream>
#include "stdlib.h"
#include "leveldb/db.h"

int main(){

leveldb::DB* db_key = NULL;
leveldb::Options options;
options.create_if_missing = true;

leveldb::Status status = level::DB::Open(options, "./gradudate/en_key.db"); 
}