#include "leveldb/db.h"
#include <iostream>
 
using namespace std;
//using namespace leveldb;
 
int main(){
    leveldb::DB *db;
    leveldb::Options option;
    option.create_if_missing = true;
    leveldb::DB::Open(option,"/tmp/leveldb_t",&db);
    string key="t2";
    string value="test";
    db->Put(leveldb::WriteOptions(), key, value);
 
    string res;
    db->Get(leveldb::ReadOptions(), key, &res);
    cout << "get value = " << res << endl;
    delete db;
    return true;
}
