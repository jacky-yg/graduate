// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "leveldb/table_builder.h"
#include<stdio.h>
#include<stdlib.h>
#include<sys/wait.h>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <unistd.h>
#include "leveldb/comparator.h"
#include "leveldb/env.h"
#include "leveldb/filter_policy.h"
#include "leveldb/options.h"
#include "table/block_builder.h"
#include "table/filter_block.h"
#include "table/format.h"
#include "util/coding.h"
#include "util/crc32c.h"
#include "aes_gcm.h"



#define AAD_SIZE 32
#define TAG_SIZE 16		/* Valid values are 16, 12, or 8 */
#define KEY_SIZE GCM_256_KEY_LEN
#define IV_SIZE  GCM_IV_DATA_LEN

namespace leveldb {
using namespace std;
ofstream out;
std::string file_name;

struct TableBuilder::Rep {
  Rep(const Options& opt, WritableFile* f)
      : options(opt),
        index_block_options(opt),
        file(f),
        offset(0),
        data_block(&options),
        index_block(&index_block_options),
        num_entries(0),
        closed(false),
        filter_block(opt.filter_policy == nullptr
                         ? nullptr
                         : new FilterBlockBuilder(opt.filter_policy)),
        pending_index_entry(false) {
    index_block_options.block_restart_interval = 1;
  }

  Options options;
  Options index_block_options;
  WritableFile* file;
  uint64_t offset;
  Status status;
  BlockBuilder data_block;
  BlockBuilder index_block;
  std::string last_key;
  int64_t num_entries;
  bool closed;  // Either Finish() or Abandon() has been called.
  FilterBlockBuilder* filter_block;



  // We do not emit the index entry for a block until we have seen the
  // first key for the next data block.  This allows us to use shorter
  // keys in the index block.  For example, consider a block boundary
  // between the keys "the quick brown fox" and "the who".  We can use
  // "the r" as the key for the index block entry since it is >= all
  // entries in the first block and < all entries in subsequent
  // blocks.
  //
  // Invariant: r->pending_index_entry is true only if data_block is empty.
  bool pending_index_entry;
  BlockHandle pending_handle;  // Handle to add to index block

  std::string compressed_output;
};

TableBuilder::TableBuilder(const Options& options, WritableFile* file, std::string fname)
    : rep_(new Rep(options, file)) {
  file_name = fname;
  //std::cout<<file_name<<std::endl;
  if (rep_->filter_block != nullptr) {
    rep_->filter_block->StartBlock(0);
  }
}

TableBuilder::TableBuilder(const Options& options, WritableFile* file)
    : rep_(new Rep(options, file)) {

  if (rep_->filter_block != nullptr) {
    rep_->filter_block->StartBlock(0);
  }
}


TableBuilder::~TableBuilder() {
  out.open("/home/jacky/leveldb_encryption/test/buffer.txt",ios::out|ios::app|ios::trunc);
  assert(rep_->closed);  // Catch errors where caller forgot to call Finish()
  delete rep_->filter_block;
  delete rep_;
}

Status TableBuilder::ChangeOptions(const Options& options) {
  // Note: if more fields are added to Options, update
  // this function to catch changes that should not be allowed to
  // change in the middle of building a Table.
  if (options.comparator != rep_->options.comparator) {
    return Status::InvalidArgument("changing comparator while building table");
  }

  // Note that any live BlockBuilders point to rep_->options and therefore
  // will automatically pick up the updated options.
  rep_->options = options;
  rep_->index_block_options = options;
  rep_->index_block_options.block_restart_interval = 1;
  return Status::OK();
}

const Slice TableBuilder::Encrypt(const Slice& val) { // 加密value 20200622
  struct gcm_key_data gkey;
  struct gcm_context_data gctx;
  uint8_t ct[val.size()], pt[val.size()], pt2[val.size()];	// Cipher text and plain text
  uint8_t iv[IV_SIZE], aad[AAD_SIZE], key[KEY_SIZE];	// Key and authentication data

  uint8_t tag1[TAG_SIZE], tag2[TAG_SIZE];	// Authentication tags for encode and decode


  //20200407 key generator


  /*for (int i=0; i<KEY_SIZE; i++)
  {
	 srand (time(NULL));
     char t,c;
     t = rand() % 26;   // generate a random number
     c = 'a' + t;
     key[i]=c;            // Convert to a character from a-z
  }*/
  memset(key, 1, KEY_SIZE);
  memset(iv, 0, IV_SIZE);
  memset(aad, 0, AAD_SIZE);





  memcpy(pt,val.data(),val.size());
  aes_gcm_pre_256(key, &gkey);
  aes_gcm_enc_256(&gkey, &gctx, ct, pt, val.size(), iv, aad, AAD_SIZE, tag1, TAG_SIZE);

  std::string value( reinterpret_cast<char const*>(ct),val.size()); // 轉 string

  const Slice encrypted_val(value); //生成 slice


  //將key寫到file暫存 2020/07/02
  std::string skey( reinterpret_cast<char const*>(key),KEY_SIZE) ;

  //if (out.is_open())
	  out<<skey<<"\n";


  return encrypted_val;
}

void TableBuilder::Add(const Slice& key, const Slice& value) {
  //將key寫到file暫存 2020/07/02
  if (!out.is_open())
	  out.open("/home/jacky/leveldb_encryption/test/buffer.txt",fstream::out|fstream::app);
  else
	  out<<key.ToString()<<"\n";

  Rep* r = rep_;
  assert(!r->closed);
  if (!ok()) return;
  const Slice en_val = Encrypt(value);




  /*std::cout<<"num_entries:"<<r->num_entries<<std::endl;
  std::cout<<"current key:"<<key.ToString()<<std::endl;
  std::cout<<"last key:"<<r->last_key<<std::endl;*/
  if (r->num_entries > 0) {
    assert(r->options.comparator->Compare(key, Slice(r->last_key)) > 0);
  }

  if (r->pending_index_entry) {
    assert(r->data_block.empty());
    r->options.comparator->FindShortestSeparator(&r->last_key, key);
    std::string handle_encoding;
    r->pending_handle.EncodeTo(&handle_encoding);
    r->index_block.Add(r->last_key, Slice(handle_encoding));
    r->pending_index_entry = false;
  }

  if (r->filter_block != nullptr) {
    r->filter_block->AddKey(key);
  }

  r->last_key.assign(key.data(), key.size());
  r->num_entries++;
  r->data_block.Add(key, en_val);

  const size_t estimated_block_size = r->data_block.CurrentSizeEstimate();
  if (estimated_block_size >= r->options.block_size) {
    Flush();
  }
}

void TableBuilder::Flush() {
  Rep* r = rep_;
  assert(!r->closed);
  if (!ok()) return;
  if (r->data_block.empty()) return;
  assert(!r->pending_index_entry);
  WriteBlock(&r->data_block, &r->pending_handle);
  if (ok()) {
    r->pending_index_entry = true;
    r->status = r->file->Flush();
  }
  if (r->filter_block != nullptr) {
    r->filter_block->StartBlock(r->offset);
  }
}

void TableBuilder::WriteBlock(BlockBuilder* block, BlockHandle* handle) {
  // File format contains a sequence of blocks where each block has:
  //    block_data: uint8[n]
  //    type: uint8
  //    crc: uint32
  assert(ok());
  Rep* r = rep_;
  Slice raw = block->Finish();

  Slice block_contents;
  CompressionType type = r->options.compression;
  // TODO(postrelease): Support more compression options: zlib?
  switch (type) {
    case kNoCompression:
      block_contents = raw;
      break;

    case kSnappyCompression: {
      std::string* compressed = &r->compressed_output;
      if (port::Snappy_Compress(raw.data(), raw.size(), compressed) &&
          compressed->size() < raw.size() - (raw.size() / 8u)) {
        block_contents = *compressed;
      } else {
        // Snappy not supported, or compressed less than 12.5%, so just
        // store uncompressed form
        block_contents = raw;
        type = kNoCompression;
      }
      break;
    }
  }
  WriteRawBlock(block_contents, type, handle);
  r->compressed_output.clear();
  block->Reset();
}

void TableBuilder::WriteRawBlock(const Slice& block_contents,
                                 CompressionType type, BlockHandle* handle) {
  Rep* r = rep_;
  handle->set_offset(r->offset);
  handle->set_size(block_contents.size());
  r->status = r->file->Append(block_contents);
  if (r->status.ok()) {
    char trailer[kBlockTrailerSize];
    trailer[0] = type;
    uint32_t crc = crc32c::Value(block_contents.data(), block_contents.size());
    crc = crc32c::Extend(crc, trailer, 1);  // Extend crc to cover block type
    EncodeFixed32(trailer + 1, crc32c::Mask(crc));
    r->status = r->file->Append(Slice(trailer, kBlockTrailerSize));
    if (r->status.ok()) {
      r->offset += block_contents.size() + kBlockTrailerSize;
    }
  }
}

Status TableBuilder::status() const { return rep_->status; }

Status TableBuilder::Finish() {
  Rep* r = rep_;
  Flush();
  assert(!r->closed);
  r->closed = true;

  BlockHandle filter_block_handle, metaindex_block_handle, index_block_handle;

  // Write filter block
  if (ok() && r->filter_block != nullptr) {
    WriteRawBlock(r->filter_block->Finish(), kNoCompression,
                  &filter_block_handle);
  }

  // Write metaindex block
  if (ok()) {
    BlockBuilder meta_index_block(&r->options);
    if (r->filter_block != nullptr) {
      // Add mapping from "filter.Name" to location of filter data
      std::string key = "filter.";
      key.append(r->options.filter_policy->Name());
      std::string handle_encoding;
      filter_block_handle.EncodeTo(&handle_encoding);
      meta_index_block.Add(key, handle_encoding);
    }

    // TODO(postrelease): Add stats and other meta blocks
    WriteBlock(&meta_index_block, &metaindex_block_handle);
  }

  // Write index block
  if (ok()) {
    if (r->pending_index_entry) {
      r->options.comparator->FindShortSuccessor(&r->last_key);
      std::string handle_encoding;
      r->pending_handle.EncodeTo(&handle_encoding);
      r->index_block.Add(r->last_key, Slice(handle_encoding));
      r->pending_index_entry = false;
    }
    WriteBlock(&r->index_block, &index_block_handle);
  }

  // Write footer
  if (ok()) {
    Footer footer;
    footer.set_metaindex_handle(metaindex_block_handle);
    footer.set_index_handle(index_block_handle);
    std::string footer_encoding;
    footer.EncodeTo(&footer_encoding);
    r->status = r->file->Append(footer_encoding);
    if (r->status.ok()) {
      r->offset += footer_encoding.size();
    }
  }

  out.close(); //關檔 2020/07/02



  //execl("","readbuffer", fstr, (char *)0);

  pid_t childpid;
  int i;
    //std::cout<<"table_builder:"<<file_name<<std::endl;
    //String^ str = gcnew String(file_name.c_str());
	if (fork() == 0){
		const char *fstr = file_name.c_str();
		//child process
		if (execl("/home/jacky/leveldb_encryption/build/readbuffer",fstr,(char*)NULL) <0 ){
			perror("error on exec");
			exit(0);
		}
	}else{
		//parent process
		childpid = wait(NULL);
		printf("execv done\n\n");
	}

  return r->status;

}

void TableBuilder::Abandon() {
  Rep* r = rep_;
  assert(!r->closed);
  r->closed = true;
}

uint64_t TableBuilder::NumEntries() const { return rep_->num_entries; }

uint64_t TableBuilder::FileSize() const { return rep_->offset; }

}  // namespace leveldb
