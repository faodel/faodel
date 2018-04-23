// Copyright 2018 National Technology & Engineering Solutions of Sandia, 
// LLC (NTESS). Under the terms of Contract DE-NA0003525 with NTESS,  
// the U.S. Government retains certain rights in this software. 

//
//  Test: mpi_kelpie_dht
//  Purpose: This is set of simple tests to see if we can setup/use a dht



#include <mpi.h>

#include "gtest/gtest.h"

#include "common/Common.hh"
#include "lunasa/Lunasa.hh"
#include "opbox/OpBox.hh"
#include "opbox/services/dirman/DirectoryManager.hh"
#include "kelpie/Kelpie.hh"

#include "webhook/Server.hh"

#include "support/Globals.hh"

using namespace std;
using namespace faodel;
using namespace opbox;

//Globals holds mpi info and manages connections (see ping example for info)
Globals G;

//Parameters used in this experiment
struct Params {
  int num_rows;
  int num_cols;
  int ldo_size;
};
Params P = { 2, 10, 20*1024 };



string default_config_string = R"EOF(
# Note: node_role is defined when we determine if this is a client or a server
# default to using mpi, but allow override in config file pointed to by FAODEL_CONFIG
nnti.transport.name                           mpi
config.additional_files.env_name.if_defined   FAODEL_CONFIG

dirman.root_role rooter
dirman.type centralized

target.dirman.host_root



# MPI tests will need to have a standard networking base
#kelpie.type standard

#bootstrap.debug true
#webhook.debug true
#opbox.debug true
#dirman.debug true
#kelpie.debug true

)EOF";


class MPIDHTTest : public testing::Test {
protected:
  virtual void SetUp() {
    dht_full  = kelpie::Connect("dht:/dht_full");
    dht_front = kelpie::Connect("dht:/dht_front_half");
    dht_back  = kelpie::Connect("dht:/dht_back_half");
    dht_single_self  = kelpie::Connect("dht:/dht_single_self");
    dht_single_other = kelpie::Connect("dht:/dht_single_other");
    my_id = net::GetMyID();
  }
  virtual void TearDown(){}
  kelpie::Pool dht_full;
  kelpie::Pool dht_front;
  kelpie::Pool dht_back;
  kelpie::Pool dht_single_self;
  kelpie::Pool dht_single_other;
  faodel::nodeid_t my_id;
  int rc;
};

lunasa::DataObject generateLDO(int num_words, uint32_t start_val){
  lunasa::DataObject ldo(0, num_words*sizeof(int), lunasa::DataObject::AllocatorType::eager);
  int *x = static_cast<int *>(ldo.GetDataPtr());
  for(int i=0; i <num_words; i++)
    x[i]=start_val+i;
  return ldo;
}

bool ldosEqual(const lunasa::DataObject &ldo1, lunasa::DataObject &ldo2) {
  if(ldo1.GetMetaSize() != ldo2.GetMetaSize()) { cout<<"Meta size mismatch\n"; return false; }
  if(ldo1.GetDataSize() != ldo2.GetDataSize()) { cout<<"Data size mismatch\n"; return false; }

  if(ldo1.GetMetaSize() > 0) {
    auto *mptr1 = ldo1.GetMetaPtr<char *>();
    auto *mptr2 = ldo2.GetMetaPtr<char *>();
    for(int i=ldo1.GetMetaSize()-1; i>=0; i--)
      if(mptr1[i] != mptr2[i]) { cout <<"Meta mismatch at offset "<<i<<endl; return false;}
  }

  if(ldo1.GetDataSize() > 0) {
    auto *dptr1 = ldo1.GetDataPtr<char *>();
    auto *dptr2 = ldo2.GetDataPtr<char *>();
    for(int i=ldo1.GetDataSize()-1; i>=0; i--)
      if(dptr1[i] != dptr2[i]) { cout <<"Data mismatch at offset "<<i<<endl; return false; }
  }
  return true;  
}

vector<pair<kelpie::Key,lunasa::DataObject>>  generateKVs(std::string prefix) {

  static int ldos_generated=0;
  vector<pair<kelpie::Key,lunasa::DataObject>> items;
  for(int i=0; i<P.num_rows; i++) {
    for(int j=0; j<P.num_cols; j++) {
      kelpie::Key key("row_"+prefix+"_"+to_string(i), "col_"+prefix+"_"+to_string(j));
      auto ldo = generateLDO( P.ldo_size/sizeof(int), (ldos_generated<<16));
      items.push_back( {key,ldo} ); 
      ldos_generated++;
    }
  }
  return items;
}

// Most tests generate and publish objects
vector<pair<kelpie::Key, lunasa::DataObject>> generateAndPublish(kelpie::Pool dht, std::string key_prefix) {

  kelpie::rc_t rc;
  
  //Generate a set of objects 
  auto kvs = generateKVs(key_prefix);

  //Launch publishes, but block until they complete
  atomic<int> num_left(kvs.size());
  for(auto &kv : kvs) {
    rc = dht.Publish(kv.first, kv.second,
                     [&num_left] (kelpie::rc_t result, kelpie::kv_row_info_t &ri, kelpie::kv_col_info_t &ci) {
                       EXPECT_EQ(0, result);
                       num_left--;
                     } );
    EXPECT_EQ(0, rc);
  }
  while(num_left) {sched_yield(); } //TODO: add a timeout

  return kvs;
}
void checkInfo(kelpie::Pool dht, const vector<pair<kelpie::Key, lunasa::DataObject>> &kvs,
               bool check_availability, kelpie::Availability expected_availability) {

  kelpie::rc_t rc;
  kelpie::kv_col_info_t col_info;
  for(auto &kv : kvs) {
    rc = dht.Info(kv.first, &col_info);
    EXPECT_EQ(0, rc);
    //cout <<"Col info "<<col_info.str()<<endl;
    if(check_availability) {
      EXPECT_EQ(expected_availability, col_info.availability);
    }
    EXPECT_EQ(kv.second.GetMetaSize()+kv.second.GetDataSize(), col_info.num_bytes);
  }
}
void checkNeed(kelpie::Pool dht, const vector<pair<kelpie::Key, lunasa::DataObject>> &kvs) {
  kelpie::rc_t rc;
  
  for(auto &kv : kvs) {
    lunasa::DataObject ldo;
    uint64_t expected_size= kv.second.GetUserSize();
    rc = dht.Need(kv.first, expected_size, &ldo); EXPECT_EQ(0, rc);
    EXPECT_EQ(kv.second.GetDataSize(), ldo.GetDataSize());
    EXPECT_TRUE(ldosEqual(kv.second, ldo));    
  }
}
void checkWantBounded(kelpie::Pool dht, const vector<pair<kelpie::Key, lunasa::DataObject>> &kvs) {

  kelpie::rc_t rc;
  lunasa::DataObject ldos[kvs.size()];
  
  atomic<int> num_left(kvs.size());
  int spot=0;
  for(auto &kv : kvs) {
    uint64_t expected_size= kv.second.GetMetaSize() + kv.second.GetDataSize();
    rc = dht.Want(kv.first, expected_size,
                  [&num_left, &ldos, spot] (bool success, kelpie::Key key, lunasa::DataObject user_ldo,
                                            const kelpie::kv_row_info_t &ri,
                                            const kelpie::kv_col_info_t &ci) {
                                   EXPECT_TRUE(success);
                                   EXPECT_EQ(kelpie::Availability::InLocalMemory, ci.availability);
                                   ldos[spot]=user_ldo;
                                   num_left--;
                               } );
    EXPECT_EQ(0, rc);
    spot++;
  }
  while(num_left!=0) { sched_yield(); } //TODO: add a timeout
  spot=0;
  for(auto &kv : kvs) {
    EXPECT_TRUE(ldosEqual(kv.second, ldos[spot]));
    spot++;
  }

}
void checkWantUnbounded(kelpie::Pool dht, const vector<pair<kelpie::Key, lunasa::DataObject>> &kvs) {

  kelpie::rc_t rc;
  lunasa::DataObject ldos[kvs.size()];
  
  atomic<int> num_left(kvs.size());
  int spot=0;
  for(auto &kv : kvs) {
    rc = dht.Want(kv.first, 
                  [&num_left, &ldos, spot] (bool success, kelpie::Key key, lunasa::DataObject user_ldo,
                                            const kelpie::kv_row_info_t &ri,
                                            const kelpie::kv_col_info_t &ci) {
                                   EXPECT_TRUE(success);
                                   EXPECT_EQ(kelpie::Availability::InLocalMemory, ci.availability);
                                   ldos[spot]=user_ldo;
                                   num_left--;
                                   //cout<<"Unbound "<<key.str()<<" writing to spot "<<spot<<endl;
                               } );
    EXPECT_EQ(0, rc);
    spot++;
  }
  while(num_left!=0) { sched_yield(); } //TODO: add a timeout
  spot=0;
  for(auto &kv : kvs) {
    bool ok = ldosEqual(kv.second, ldos[spot]);
    EXPECT_TRUE(ok);
    spot++;
  }

}

// Sanity check: This test just checks to make sure the dhts are setup correctly
TEST_F(MPIDHTTest, CheckDHTs) {

  auto di  = dht_full.GetDirectoryInfo();
  auto dif = dht_front.GetDirectoryInfo();
  auto dib = dht_back.GetDirectoryInfo();
  auto dis = dht_single_self.GetDirectoryInfo();
  auto dio = dht_single_other.GetDirectoryInfo();
  
  EXPECT_EQ(G.mpi_size,   di.children.size());
  EXPECT_EQ(G.mpi_size/2, dif.children.size());
  EXPECT_EQ(G.mpi_size - G.mpi_size/2, dib.children.size());
  EXPECT_EQ(1, dis.children.size());
  EXPECT_EQ(1, dio.children.size());

  if(dis.children.size()==1) EXPECT_EQ(my_id, dis.children[0].node);
  if(dio.children.size()==1) EXPECT_EQ(G.nodes[G.mpi_size-1], dio.children[0].node);
}


// Self DHT test: Write to a single-node DHT that is local and read results. This
// is useful for verifying that the basic operations work, even if they aren't going
// out to the network.
TEST_F(MPIDHTTest, BasicSingleSelfTest) {

  kelpie::Pool dht = dht_single_self; //alias
  
  //Generate a set of objects and publish
  auto kvs = generateAndPublish(dht, "single_self_data");
  checkInfo(dht, kvs, true, kelpie::Availability::InLocalMemory);
  checkNeed(dht, kvs); //Do Need on all ops  
  checkWantBounded(dht, kvs); //Do Wants, specifying length
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 
}


//
TEST_F(MPIDHTTest, BasicSingleOtherNeed) {

  kelpie::Pool dht = dht_single_other; //alias
  
  auto kvs = generateAndPublish(dht, "single_other_dataA");
  checkInfo(dht, kvs, true, kelpie::Availability::InRemoteMemory);
  checkNeed(dht, kvs); //Do Need on all ops
  
  //Values should be local now, so these should just be like self
  checkWantBounded(dht, kvs); //Do Wants, specifying length
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 
}

TEST_F(MPIDHTTest, BasicSingleOtherWantBounded) {

  kelpie::Pool dht = dht_single_other; //alias
  
  auto kvs = generateAndPublish(dht, "single_other_dataB");
  checkInfo(dht, kvs, true, kelpie::Availability::InRemoteMemory);
  checkWantBounded(dht, kvs); //Do Wants, specifying length
   
  //Values should be local now, so these should just be like self
  checkNeed(dht, kvs); //Do Need on all ops
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 
}

TEST_F(MPIDHTTest, BasicSingleOtherWantUnbounded) {

  kelpie::Pool dht = dht_single_other; //alias
  
  auto kvs = generateAndPublish(dht, "single_other_dataC");
  checkInfo(dht, kvs, true, kelpie::Availability::InRemoteMemory);
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 
   
  //Values should be local now, so these should just be like self
  checkNeed(dht, kvs); //Do Need on all ops
  checkWantBounded(dht, kvs); //Do Wants, specifying length
}

TEST_F(MPIDHTTest, BasicFullNeed) {

  kelpie::Pool dht = dht_full; //alias
  
  //Generate a set of objects and publish
  auto kvs = generateAndPublish(dht, "full_data1");
  checkInfo(dht, kvs, false, kelpie::Availability::InRemoteMemory);
  checkNeed(dht, kvs); //Do Need on all ops

  //Values should be local now, so these should just be like self
  checkWantBounded(dht, kvs); //Do Wants, specifying length
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 
}

TEST_F(MPIDHTTest, BasicFullWantBounded) {

  kelpie::Pool dht = dht_full; //alias
  
  //Generate a set of objects and publish
  auto kvs = generateAndPublish(dht, "full_data2");
  checkInfo(dht, kvs, false, kelpie::Availability::InRemoteMemory);
  checkWantBounded(dht, kvs); //Do Wants, specifying length

  //Values should be local now, so these should just be like self
  checkNeed(dht, kvs); //Do Need on all ops
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 
}

TEST_F(MPIDHTTest, BasicFullWantUnbounded) {

  kelpie::Pool dht = dht_full; //alias
  
  //Generate a set of objects and publish
  auto kvs = generateAndPublish(dht, "full_data3");
  checkInfo(dht, kvs, false, kelpie::Availability::InRemoteMemory);
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 

  //Values should be local now, so these should just be like self
  checkNeed(dht, kvs); //Do Need on all ops
  checkWantBounded(dht, kvs); //Do Wants, specifying length

}


TEST_F(MPIDHTTest, BasicHalf1Need) {

  kelpie::Pool dht = dht_front; //alias
  
  //Generate a set of objects and publish
  auto kvs = generateAndPublish(dht, "half1_data1");
  checkInfo(dht, kvs, false, kelpie::Availability::InRemoteMemory);
  checkNeed(dht, kvs); //Do Need on all ops

  //Values should be local now, so these should just be like self
  checkWantBounded(dht, kvs); //Do Wants, specifying length
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 
}

TEST_F(MPIDHTTest, BasicHalf1WantBounded) {

  kelpie::Pool dht = dht_front; //alias
  
  //Generate a set of objects and publish
  auto kvs = generateAndPublish(dht, "half1_data2");
  checkInfo(dht, kvs, false, kelpie::Availability::InRemoteMemory);
  checkWantBounded(dht, kvs); //Do Wants, specifying length

  //Values should be local now, so these should just be like self
  checkNeed(dht, kvs); //Do Need on all ops
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 
}

TEST_F(MPIDHTTest, BasicHalf1WantUnbounded) {

  kelpie::Pool dht = dht_front; //alias
  
  //Generate a set of objects and publish
  auto kvs = generateAndPublish(dht, "half1_data3");
  checkInfo(dht, kvs, false, kelpie::Availability::InRemoteMemory);
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 

  //Values should be local now, so these should just be like self
  checkNeed(dht, kvs); //Do Need on all ops
  checkWantBounded(dht, kvs); //Do Wants, specifying length

}

TEST_F(MPIDHTTest, BasicHalf2Need) {

  kelpie::Pool dht = dht_back; //alias
  
  //Generate a set of objects and publish
  auto kvs = generateAndPublish(dht, "half2_data1");
  checkInfo(dht, kvs, false, kelpie::Availability::InRemoteMemory);
  checkNeed(dht, kvs); //Do Need on all ops

  //Values should be local now, so these should just be like self
  checkWantBounded(dht, kvs); //Do Wants, specifying length
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 
}

TEST_F(MPIDHTTest, BasicHalf2WantBounded) {

  kelpie::Pool dht = dht_back; //alias
  
  //Generate a set of objects and publish
  auto kvs = generateAndPublish(dht, "half2_data2");
  checkInfo(dht, kvs, false, kelpie::Availability::InRemoteMemory);
  checkWantBounded(dht, kvs); //Do Wants, specifying length

  //Values should be local now, so these should just be like self
  checkNeed(dht, kvs); //Do Need on all ops
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 
}

TEST_F(MPIDHTTest, BasicHalf2WantUnbounded) {

  kelpie::Pool dht = dht_back; //alias
  
  //Generate a set of objects and publish
  auto kvs = generateAndPublish(dht, "half2_data3");
  checkInfo(dht, kvs, false, kelpie::Availability::InRemoteMemory);
  checkWantUnbounded(dht, kvs); //Do Wants, without lengths 

  //Values should be local now, so these should just be like self
  checkNeed(dht, kvs); //Do Need on all ops
  checkWantBounded(dht, kvs); //Do Wants, specifying length

}



void targetLoop(){
  //G.dump();
}

int main(int argc, char **argv){

  ::testing::InitGoogleTest(&argc, argv);

  //Insert any Op registrations here

  faodel::Configuration config(default_config_string);
  config.AppendFromReferences();
  G.StartAll(argc, argv, config);


  if(G.mpi_rank==0){
    //Register the dht
    opbox::DirectoryInfo dir_info1("dht:/dht_full",         "This is My DHT");
    opbox::DirectoryInfo dir_info2("dht:/dht_front_half",   "This DHT is on the first half of ranks");
    opbox::DirectoryInfo dir_info3("dht:/dht_back_half",    "This DHT is on the second half of ranks");
    opbox::DirectoryInfo dir_info4("dht:/dht_single_self",  "Single node, same as writer");
    opbox::DirectoryInfo dir_info5("dht:/dht_single_other", "Single node, different than writer");
    
    for(int i=0; i<G.mpi_size; i++){
      dir_info1.Join(G.nodes[i]);
      if(i<(G.mpi_size/2)) dir_info2.Join(G.nodes[i]);
      else                 dir_info3.Join(G.nodes[i]);
      if(i==0)             dir_info4.Join(G.nodes[i]);
      if(i==G.mpi_size-1)  dir_info5.Join(G.nodes[i]);
    }
    opbox::dirman::HostNewDir(dir_info1);
    opbox::dirman::HostNewDir(dir_info2);
    opbox::dirman::HostNewDir(dir_info3);
    opbox::dirman::HostNewDir(dir_info4);
    opbox::dirman::HostNewDir(dir_info5);
    
    faodel::nodeid_t n2 = net::GetMyID();

    int rc = RUN_ALL_TESTS();
    sleep(1);
  } else {
    targetLoop();
    sleep(1);
  }
  G.StopAll();
}
