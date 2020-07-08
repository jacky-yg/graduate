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
    string value="test11111111111111111";
    db->Put(leveldb::WriteOptions(), key, value);

    leveldb::DB *db2;
    leveldb::Options option2;
    option.create_if_missing = true;
    leveldb::DB::Open(option,"/tmp2/leveldb_t2",&db2);
    string key2="t2";
    string value2="test11111111111111111";
    db2->Put(leveldb::WriteOptions(), key2, value2);

 
    string res;
    db->Get(leveldb::ReadOptions(), key, &res);
    cout << "get value = " << res << endl;


    string res2;
    db2->Get(leveldb::ReadOptions(), key2, &res2);
    cout << "get value2 = " << res2 << endl;
    delete db;
    delete db2;
    return true;
}
