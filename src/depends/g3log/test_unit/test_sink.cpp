/** ==========================================================================
* 2013 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
* with no warranties. This code is yours to share, use and modify with no
* strings attached and no restrictions or obligations.
 * 
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
* ============================================================================*/

#include <gtest/gtest.h>
#include <iostream>
#include <atomic>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <string>
#include <future>
#include <g3log/generated_definitions.hpp>
#include "testing_helpers.h"
#include "g3log/logmessage.hpp"
#include "g3log/logworker.hpp"


using namespace testing_helpers;
using namespace std;
TEST(Sink, OneSink) {
using namespace g3;
   AtomicBoolPtr flag = make_shared < atomic<bool >> (false);
   AtomicIntPtr count = make_shared < atomic<int >> (0);
   {
      auto worker = g3::LogWorker::createLogWorker();
      auto handle = worker->addSink(std::make_unique<ScopedSetTrue>(flag, count), &ScopedSetTrue::ReceiveMsg);
      EXPECT_FALSE(flag->load());
      EXPECT_TRUE(0 == count->load());
      LogMessagePtr message{std::make_unique<LogMessage>("test", 0, "test", DEBUG)};
      message.get()->write().append("this message should trigger an atomic increment at the sink");
      worker->save(message);
   }
   EXPECT_TRUE(flag->load());
   EXPECT_TRUE(1 == count->load());
}


namespace {
   typedef std::shared_ptr<std::atomic<bool >> AtomicBoolPtr;
   typedef std::shared_ptr<std::atomic<int >> AtomicIntPtr;
   typedef vector<AtomicBoolPtr> BoolList;
   typedef vector<AtomicIntPtr> IntVector;
}

TEST(ConceptSink, OneHundredSinks) {
   using namespace g3;
   BoolList flags;
   IntVector counts;

   size_t NumberOfItems = 100;
   for (size_t index = 0; index < NumberOfItems; ++index) {
      flags.push_back(make_shared < atomic<bool >> (false));
      counts.push_back(make_shared < atomic<int >> (0));
   }

   {
      RestoreFileLogger logger{"./"};
      g3::LogWorker* worker = logger._scope->get(); //g3LogWorker::createLogWorker();
      size_t index = 0;
      for (auto& flag : flags) {
         auto& count = counts[index++];
         // ignore the handle
         worker->addSink(std::make_unique<ScopedSetTrue>(flag, count), &ScopedSetTrue::ReceiveMsg);
      }
      LOG(G3LOG_DEBUG) << "start message";
      LogMessagePtr message1{std::make_unique<LogMessage>("test", 0, "test", DEBUG)};
      LogMessagePtr message2{std::make_unique<LogMessage>("test", 0, "test", DEBUG)};
      auto& write1 = message1.get()->write();
      write1.append("Hello to 100 receivers :)");
      worker->save(message1);
      
      auto& write2 = message2.get()->write();
      write2.append("Hello to 100 receivers :)");
      worker->save(message2);
      LOG(INFO) << "end message";
      logger.reset();
   }
   // at the curly brace above the ScopedLogger will go out of scope and all the 
   // 100 logging receivers will get their message to exit after all messages are
   // are processed
   size_t index = 0;
   for (auto& flag : flags) {
      auto& count = counts[index++];
      ASSERT_TRUE(flag->load()) << ", count : " << (index - 1);
      ASSERT_TRUE(4 == count->load()) << ", count : " << (index - 1);
   }

   cout << "test one hundred sinks is finished finished\n";
}

struct VoidReceiver {
   std::atomic<int>* _atomicCounter;
   explicit VoidReceiver(std::atomic<int>* counter) : _atomicCounter(counter){}
   
   void receiveMsg(std::string msg){ /*ignored*/}
   void incrementAtomic(){
     (*_atomicCounter)++;
   }
};

TEST(ConceptSink, VoidCall__NoCall_ExpectingNoAdd) {
   std::atomic<int> counter{0};
   {
      std::unique_ptr<g3::LogWorker> worker{g3::LogWorker::createLogWorker()};
      auto handle = worker->addSink(std::make_unique<VoidReceiver>(&counter), &VoidReceiver::receiveMsg);
   }  
   EXPECT_EQ(counter, 0);
}

TEST(ConceptSink, VoidCall__OneCall_ExpectingOneAdd) {
   std::atomic<int> counter{0};
   {
      std::unique_ptr<g3::LogWorker> worker{g3::LogWorker::createLogWorker()};
      auto handle = worker->addSink(std::make_unique<VoidReceiver>(&counter), &VoidReceiver::receiveMsg);
      std::future<void> ignored = handle->call(&VoidReceiver::incrementAtomic);
   }  
   EXPECT_EQ(counter, 1);
}

TEST(ConceptSink, VoidCall__TwoCalls_ExpectingTwoAdd) {
   std::atomic<int> counter{0};
   {
      std::unique_ptr<g3::LogWorker> worker{g3::LogWorker::createLogWorker()};
      auto handle = worker->addSink(std::make_unique<VoidReceiver>(&counter), &VoidReceiver::receiveMsg);
      auto  voidFuture1 = handle->call(&VoidReceiver::incrementAtomic);
      auto  voidFuture2 = handle->call(&VoidReceiver::incrementAtomic);
      voidFuture1.wait();
      EXPECT_TRUE(counter >= 1);
   }  
   EXPECT_EQ(counter, 2);
}


struct IntReceiver {
   std::atomic<int>* _atomicCounter;
   explicit IntReceiver(std::atomic<int>* counter) : _atomicCounter(counter){}
   
   void receiveMsgDoNothing(std::string msg){ /*ignored*/}
   void receiveMsgIncrementAtomic(std::string msg){ incrementAtomic(); }
   int incrementAtomic(){
     (*_atomicCounter)++;
     int value = *_atomicCounter;
     return value;
   }
};

TEST(ConceptSink, IntCall__TwoCalls_ExpectingTwoAdd) {
   std::atomic<int> counter{0};
   {
      std::unique_ptr<g3::LogWorker> worker{g3::LogWorker::createLogWorker()};
      auto handle = worker->addSink(std::make_unique<IntReceiver>(&counter), &IntReceiver::receiveMsgDoNothing);
      std::future<int> intFuture1 = handle->call(&IntReceiver::incrementAtomic);
      EXPECT_EQ(intFuture1.get(), 1);
      EXPECT_EQ(counter, 1);

     auto intFuture2 = handle->call(&IntReceiver::incrementAtomic);
     EXPECT_EQ(intFuture2.get(), 2);

   }  
   EXPECT_EQ(counter, 2);
}


 
void DoLogCalls(std::atomic<bool>*  doWhileTrue, size_t counter) {
   while(doWhileTrue->load()) {
      LOG(INFO) << "Calling from #" << counter;
      std::this_thread::yield();
   }
} 


TEST(ConceptSink, CannotCallSpawnTaskOnNullptrWorker) {
  auto FailedHelloWorld = []{ std::cout << "Hello World" << std::endl; };
  kjellkod::Active* active = nullptr;
  auto failed = g3::spawn_task(FailedHelloWorld, active);
  EXPECT_ANY_THROW(failed.get());
}

TEST(ConceptSink, DISABLED_AggressiveThreadCallsDuringShutdown) {
   std::atomic<bool> keepRunning{true};

   std::vector<std::thread> threads;
   const size_t numberOfThreads = std::thread::hardware_concurrency() * 4;
   threads.reserve(numberOfThreads);

   g3::internal::shutDownLogging();
    
   // Avoid annoying printouts at log shutdown
   stringstream cerr_buffer; 
   testing_helpers::ScopedOut guard1(std::cerr, &cerr_buffer);

   // these threads will continue to write to a logger
   // while the receiving logger is instantiated, and destroyed repeatedly
   for (size_t caller = 0; caller < numberOfThreads; ++ caller) {
      threads.push_back(std::thread(DoLogCalls, &keepRunning, caller));
   }


   std::atomic<int> atomicCounter{0};
   size_t numberOfCycles = 25;
   std::cout << "Create logger, delete active logger, " << numberOfCycles << " times\n\tWhile " << numberOfThreads << " threads are continously doing LOG calls" << std::endl;
   std::cout << "Create/Destroy Times #";
   for (size_t create = 0; create < numberOfCycles; ++create) {
      std::cout << create << " ";

      std::unique_ptr<g3::LogWorker> worker{g3::LogWorker::createLogWorker()};
      auto handle = worker->addSink(std::make_unique<IntReceiver>(&atomicCounter), &IntReceiver::receiveMsgIncrementAtomic);
      g3::initializeLogging(worker.get());
  
     // wait till some LOGS streaming in
      atomicCounter = 0;
      while(atomicCounter.load() < 10) {
         std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
   } // g3log worker exists:  1) shutdownlogging 2) flush of queues and shutdown of sinks


  // exit the threads
  keepRunning = false;
  for (auto& t : threads) {
    t.join();
  }
  std::cout << "\nAll threads are joined " << std::endl;
}


