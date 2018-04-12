/** ==========================================================================
 * 2012 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
 * with no warranties. This code is yours to share, use and modify with no
 * strings attached and no restrictions or obligations.
 *
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
 * ============================================================================*/


#include <gtest/gtest.h>
#include <memory>
#include <fstream>
#include <string>
#include <memory>
#include <future>
#include <queue>


#include <thread>
#include "g3log/g3log.hpp"
#include "g3log/logworker.hpp"
#include "testing_helpers.h"

using namespace testing_helpers;


namespace { // anonymous
   const char* name_path_1 = "./(some_fake_DirectoryOrName_1_)";
   const std::string kReplaceFileName = "(ReplaceLogFile)";
   g3::LogWorker* g_logger_ptr = nullptr;
   g3::SinkHandle<g3::FileSink>* g_filesink_handler = nullptr;
   LogFileCleaner* g_cleaner_ptr = nullptr;

   std::string setLogNameAndAddCount(std::string new_file_to_create, std::string logger_id = "g3log") {
      static std::mutex m;
      static int count;
      std::string add_count;
      std::lock_guard<std::mutex> lock(m);
      {
         add_count = std::to_string(++count) + "_";
         auto future_new_log = g_filesink_handler->call(&g3::FileSink::changeLogFile, new_file_to_create + add_count, logger_id);
         auto new_log = future_new_log.get();
         if (!new_log.empty()) {
            g_cleaner_ptr->addLogToClean(new_log);
         } else {
            std::cout << "\nFailed to set filename: " << new_file_to_create << std::endl;
         }
         return new_log;
      }
      return add_count;
   }

   std::string setLogName(std::string new_file_to_create, std::string logger_id = "g3log") {
      auto future_new_log = g_filesink_handler->call(&g3::FileSink::changeLogFile, new_file_to_create, logger_id);
      auto new_log = future_new_log.get();
      if (!new_log.empty()) g_cleaner_ptr->addLogToClean(new_log);
      return new_log;
   }

   std::string getLogName() {
      return g_filesink_handler->call(&g3::FileSink::fileName).get();
   }

} // anonymous

TEST(TestOf_GetFileName, Expecting_ValidLogFile) {

   LOG(INFO) << "test_filechange, Retrieving file name: ";
   ASSERT_NE(g_logger_ptr, nullptr);
   ASSERT_FALSE(getLogName().empty());
}

TEST(TestOf_ChangingLogFile, Expecting_NewLogFileUsed) {
   auto old_log = getLogName();
   std::string name = setLogNameAndAddCount(name_path_1);
   auto new_log = setLogName(name);
   ASSERT_NE(old_log, new_log);
}

TEST(TestOf_ChangingLogFile_Id, Expecting_NewLogFileUsed1) {
   auto old_log = getLogName();
   setLogNameAndAddCount(name_path_1);
   auto new_log = setLogName("foo", "new_logger_id");
   ASSERT_NE(old_log, new_log);
   std::string new_name = getLogName();
   auto expected_part_of__new_name = std::string("foo") + kReplaceFileName + ".new_logger_id";
   auto extracted_name = new_name.substr(0, expected_part_of__new_name.size());
   ASSERT_EQ(extracted_name.c_str(), expected_part_of__new_name);
}

TEST(TestOf_ChangingLogFile_NoId, Expecting_NewLogFileUsed2) {
   auto old_log = getLogName();
   setLogNameAndAddCount(name_path_1);
   auto new_log = setLogName("foo", "");
   ASSERT_NE(old_log, new_log);
   std::string new_name = getLogName();
   auto expected_part_of__new_name = std::string("foo") + kReplaceFileName;
   auto extracted_name = new_name.substr(0, expected_part_of__new_name.size());
   ASSERT_EQ(extracted_name.c_str(), expected_part_of__new_name);
}

TEST(TestOf_ManyThreadsChangingLogFileName, Expecting_EqualNumberLogsCreated) {
   auto old_log = g_filesink_handler->call(&g3::FileSink::fileName).get();
   if (!old_log.empty()) g_cleaner_ptr->addLogToClean(old_log);

   LOG(INFO) << "SoManyThreadsAllDoingChangeFileName";
   std::vector<std::thread> threads;
   auto max = 2;
   auto size = g_cleaner_ptr->size();
   for (auto count = 0; count < max; ++count) {
      std::string drive = ((count % 2) == 0) ? "./_threadEven_" : "./_threaOdd_";
      std::string logger_id = std::to_string(count);
      threads.push_back(std::thread(setLogNameAndAddCount, drive, logger_id));
   }
   for (auto& thread : threads)
      thread.join();

   // check that all logs were created
   ASSERT_EQ(size + max, g_cleaner_ptr->size());
}

TEST(TestOf_IllegalLogFileName, Expecting_NoChangeToOriginalFileName) {
   std::string original = getLogName();
   auto perhaps_a_name = setLogName("XY:/"); // does not exist
   ASSERT_TRUE(perhaps_a_name.empty());
   std::string post_illegal = getLogName();
   ASSERT_STREQ(original.c_str(), post_illegal.c_str());
}

TEST(TestOf_SinkHandleDifferentId, Expecting_DifferentId) {
   auto sink = std::make_unique<g3::FileSink>("AnotherLogFile", name_path_1, "logger_id");
   auto name = sink->fileName();
   ASSERT_STREQ( name.substr(0, 26).c_str(), "./AnotherLogFile.logger_id");
   g_cleaner_ptr->addLogToClean(name);
}

TEST(TestOf_LegalLogFileNam,  With_parenthesis) {
   std::string original = getLogName();
   auto perhaps_a_name = setLogName("(test)"); // does not exist
   EXPECT_NE(original, perhaps_a_name);
   std::string post_legal = getLogName();
   EXPECT_TRUE(std::string::npos != post_legal.find("(test)")) << "filename was: " << post_legal;
}


int main(int argc, char* argv[]) {
   LogFileCleaner cleaner;
   g_cleaner_ptr = &cleaner;
   int return_value = 1;
   std::stringstream cerrDump;

   std::string last_log_file;
   {

      testing_helpers::ScopedOut scopedCerr(std::cerr, &cerrDump);

      auto worker = g3::LogWorker::createLogWorker();
      auto handle = worker->addDefaultLogger(kReplaceFileName, name_path_1);
      g_logger_ptr = worker.get();
      g_filesink_handler = handle.get();
      last_log_file = g_filesink_handler->call(&g3::FileSink::fileName).get();
      std::cout << "log file at: " << last_log_file << std::endl;
      cleaner.addLogToClean(last_log_file);


      g3::initializeLogging(g_logger_ptr);
      LOG(INFO) << "test_filechange demo*" << std::endl;

      testing::InitGoogleTest(&argc, argv);
      return_value = RUN_ALL_TESTS();

      last_log_file = g_filesink_handler->call(&g3::FileSink::fileName).get();
      std::cout << "log file at: " << last_log_file << std::endl;
      //g3::internal::shutDownLogging();
   }
   std::cout << "FINISHED WITH THE TESTING" << std::endl;
   // cleaning up
   cleaner.addLogToClean(last_log_file);
   return return_value;
}