//
// by Jia, Apr 2019
//

#include "PDBClient.h"
#include "RedditComment.h"
#include <RedditAuthor.h>
#include <RedditSub.h>
#include <RedditFeatures.h>
#include <RedditSubsAndComments.h>
#include <RedditJoin.h>
#include <RedditJoinSubsAndComments.h>
#include <RedditPositiveLabelSelection.h>
#include <RedditNegativeLabelSelection.h>
#include <ScanUserSet.h>
#include <WriteUserSet.h>
#include <string>
#include <random>
#include <chrono>

using namespace pdb;

void classify(pdb::Handle<reddit::Comment> comment){
   if (comment->score > 1) {
       comment->label = 1;
   } else{
       comment->label = -1;
   }
}

void parseInputJSONFile(PDBClient &pdbClient, std::string fileName, int blockSizeInMB) {

  // the error message is put there
  string errMsg;
  std ::ifstream inFile(fileName.c_str());
  std :: string line;
  bool end = false;
  bool rollback = false;

  long total = 1000000;
  long sent = 0;
  long i = 0;
  pdb::makeObjectAllocatorBlock((size_t)blockSizeInMB * (size_t)1024 * (size_t)1024, true);
  pdb::Handle<pdb::Vector<pdb::Handle<reddit::Comment>>> storeMe = pdb::makeObject<pdb::Vector<pdb::Handle<reddit::Comment>>> ();
  while (!end) {
      if (!rollback) {
          if(!std::getline(inFile, line)){
             end = true;
             if (! pdbClient.sendData<reddit::Comment> (std::pair<std::string, std::string>("comments", "redditDB"), storeMe, errMsg)) {
                 std::cout << "Failed to send data to dispatcher server" << std::endl;
                 return;
             }
             pdbClient.flushData (errMsg);
             std::cout << "Dispatched " << storeMe->size() << " comments." << std::endl;
             break;
          }
      }
      rollback = false; 
      try {
          pdb::Handle<reddit::Comment> comment = pdb::makeObject<reddit::Comment>(i, line);
          classify(comment);
          storeMe->push_back(comment);
          i++;
      }
      catch (pdb::NotEnoughSpace &n) {
          if (! pdbClient.sendData<reddit::Comment> (std::pair<std::string, std::string>("comments", "redditDB"), storeMe, errMsg)) {
             std::cout << "Failed to send data to dispatcher server" << std::endl;
             return;
          }
          std::cout << "Dispatched " << storeMe->size() << " comments." << std::endl;
          sent = sent+storeMe->size();
          std::cout << "sent " << sent << " objects in total" << std::endl;
          if (sent >= total) { 
              end = true;
//              return;
          }
          rollback = true; 
          pdb::makeObjectAllocatorBlock((size_t)blockSizeInMB * (size_t)1024 * (size_t)1024, true);
          storeMe = pdb::makeObject<pdb::Vector<pdb::Handle<reddit::Comment>>> ();

      }
   }
   pdbClient.flushData (errMsg);
}



int main(int argc, char* argv[]) {
 
  // we put the error here
  string errMsg;

  // make sure we have the arguments
  if(argc < 5) {

    std::cout << "Usage : ./LoadRedditCommentsWithPartition managerIP managerPort inputFileName whetherToPartitionData, whetherToRegisterLibraries\n";
    std::cout << "managerIP - IP of the manager\n";
    std::cout << "managerPort - Port of the manager\n";
    std::cout << "inputFileName - The file to load for reddit comments data, which is a set of JSON objects\n";
    std::cout << "whetherToPrepartitionData - Y yes, N no\n";
    std::cout << "whetherToRegisterLibraries - Y yes, N no\n";    
  }

  //  get the manager address
  std::string managerIp = std::string(argv[1]);
  int32_t port = std::stoi(argv[2]);
  std::string inputFileName = std::string(argv[3]);
  bool whetherToPartitionData = true;
  if (strcmp(argv[4], "N")==0) {
      whetherToPartitionData = false;
  }
  bool whetherToRegisterLibraries = true;
  if (strcmp(argv[5], "N")==0) {
      whetherToRegisterLibraries = false;
  }

  // make a client
  pdb::PDBLoggerPtr clientLogger = make_shared<pdb::PDBLogger>("clientLog");
  CatalogClient catalogClient(port, managerIp, clientLogger);
  PDBClient pdbClient(port, managerIp, clientLogger, false, true);
  pdbClient.registerType("libraries/libRedditComment.so", errMsg);
  pdbClient.registerType("libraries/libRedditAuthor.so", errMsg);
  pdbClient.registerType("libraries/libRedditSub.so", errMsg);
  pdbClient.registerType("libraries/libRedditFeatures.so", errMsg);
  pdbClient.registerType("libraries/libRedditSubsAndComments.so", errMsg);
  pdbClient.registerType("libraries/libRedditJoin.so", errMsg);
  pdbClient.registerType("libraries/libRedditJoinSubsAndComments.so", errMsg);
  pdbClient.registerType("libraries/libRedditPositiveLabelSelection.so", errMsg);
  pdbClient.registerType("libraries/libRedditNegativeLabelSelection.so", errMsg);


  // now, create a new database
  pdbClient.createDatabase("redditDB", errMsg);
  
  Handle<LambdaIdentifier> myLambda1 = nullptr;

  ///construct dispatching computations
  pdb::makeObjectAllocatorBlock(64 * 1024 * 1024, true);

  // make a scan set
  Handle<Computation> input1 = makeObject<ScanUserSet<reddit::Comment>>("redditDB", "comments");
  Handle<Computation> input2 = makeObject<ScanUserSet<reddit::Author>>("redditDB", "authors");

  // make a selection
  Handle<Computation> select = makeObject<reddit::PositiveLabelSelection>();
  select->setInput(input1);

  // join previous ranks with links
  Handle<Computation> join = makeObject<reddit::JoinAuthorsWithComments>();

  join->setInput(0, select);
  join->setInput(1, input2);

  // make another scan set
  Handle<Computation> input3 = makeObject<ScanUserSet<reddit::Sub>>("redditDB", "subreddits");

  //make another selection
  Handle<Computation> select1 = makeObject<reddit::NegativeLabelSelection>();
  select1->setInput(input1);

  Handle<Computation> join1 = makeObject<reddit::JoinSubsWithComments>();

  join1->setInput(0, select1);
  join1->setInput(1, input3);


  // write it out
  Handle<Computation> myWriteSet = makeObject<WriteUserSet<reddit::Features>>("redditDB", "features");
  myWriteSet->setInput(join);
  Handle<Computation> myWriteSet1 = makeObject<WriteUserSet<reddit::SubsAndComments>>("redditDB", "subsAndComments");
  myWriteSet1->setInput(join1);

  Handle<Vector<Handle<Computation>>> sinks = makeObject<Vector<Handle<Computation>>>();
  sinks->push_back(myWriteSet);
  sinks->push_back(myWriteSet1);
 

  Handle<Vector<Handle<Computation>>> computations = makeObject<Vector<Handle<Computation>>>();
  computations->push_back(myWriteSet);
  computations->push_back(myWriteSet1);
  computations->push_back(join);
  computations->push_back(join1);
  computations->push_back(select);
  computations->push_back(select1);
  computations->push_back(input1);
  computations->push_back(input2);
  computations->push_back(input3); 

  // now, create the output set
  pdbClient.removeSet("redditDB", "comments", errMsg);
  pdbClient.createSet<reddit::Comment>("redditDB", "comments", errMsg, (size_t)64*(size_t)1024*(size_t)1024, "comments", computations, "redditAdaptiveJoin");

  // parse the input file 
  parseInputJSONFile(pdbClient, inputFileName, 64); }