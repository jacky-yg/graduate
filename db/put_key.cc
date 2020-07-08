#include "leveldb/db.h"
#include <iostream>

using namespace std;
//using namespace leveldb;

int main(){
    leveldb::DB *db;
    leveldb::Options option;
    option.create_if_missing = true;
    leveldb::DB::Open(option,"/en/leveldb_key",&db);
}

