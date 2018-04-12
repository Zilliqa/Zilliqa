/** ==========================================================================
* 2013 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
* with no warranties. This code is yours to share, use and modify with no
* strings attached and no restrictions or obligations.
 * 
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
* ============================================================================*/


#include <g3log/g3log.hpp>
#include <g3log/logworker.hpp>
#include <g3log/logmessage.hpp>

#include "testing_helpers.h"


#include <gtest/gtest.h>
#include <iostream>
#include <fstream>

using namespace std;
using namespace g3;

namespace testing_helpers {

   std::string g_mockFatal_message = {};
   int g_mockFatal_signal = -1;
   bool g_mockFatalWasCalled = false;

   std::string mockFatalMessage() {
      return g_mockFatal_message;
   }

   int mockFatalSignal() {
      return g_mockFatal_signal;
   }

   bool mockFatalWasCalled() {
      return g_mockFatalWasCalled;
   }

   void mockFatalCall(FatalMessagePtr fatal_message) {
      g_mockFatal_message = fatal_message.get()->toString();
      g_mockFatal_signal = fatal_message.get()->_signal_id;
      g_mockFatalWasCalled = true;
      LogMessagePtr message{fatal_message.release()};
      g3::internal::pushMessageToLogger(message); //fatal_message.copyToLogMessage());
   }

   void clearMockFatal() {
      g_mockFatal_message.clear();
      g_mockFatal_signal = -1;
      g_mockFatalWasCalled = false;
   }

   bool removeFile(std::string path_to_file) {
      return (0 == std::remove(path_to_file.c_str()));
   }

   bool verifyContent(const std::string &total_text, std::string msg_to_find) {
      std::string content(total_text);
      size_t location = content.find(msg_to_find);
      return (location != std::string::npos);
   }

   std::string readFileToText(std::string filename) {
      std::ifstream in;
      in.open(filename.c_str(), std::ios_base::in);
      if (!in.is_open()) {
         return
         {
         }; // error just return empty string - test will 'fault'
      }
      std::ostringstream oss;
      oss << in.rdbuf();
      return oss.str();
      // RAII of std::ifstream will automatically close the file
   }

   size_t LogFileCleaner::size() {
      return logs_to_clean_.size();
   }

   
   LogFileCleaner::~LogFileCleaner() {
      std::lock_guard<std::mutex> lock(g_mutex);
      {
         for (const auto& file : logs_to_clean_) {
            if (!removeFile(file)) {
               ADD_FAILURE() << "UNABLE to remove: " << file << std::endl;
            }
         }
         logs_to_clean_.clear();
      } // mutex
   }

   void LogFileCleaner::addLogToClean(std::string path_to_log) {
      std::lock_guard<std::mutex> lock(g_mutex);
      {
         if (std::find(logs_to_clean_.begin(), logs_to_clean_.end(), path_to_log.c_str()) == logs_to_clean_.end())
            logs_to_clean_.push_back(path_to_log);
      }
   }

   ScopedLogger::ScopedLogger() : _currentWorker(g3::LogWorker::createLogWorker()) {}
   ScopedLogger::~ScopedLogger() {}

   g3::LogWorker* ScopedLogger::get() {
      return _currentWorker.get();
   }

   RestoreFileLogger::RestoreFileLogger(std::string directory)
   : _scope(new ScopedLogger), _handle(_scope->get()->addSink(std::make_unique<g3::FileSink>("UNIT_TEST_LOGGER", directory), &g3::FileSink::fileWrite)) {
      using namespace g3;
      g3::initializeLogging(_scope->_currentWorker.get());
      clearMockFatal();
      setFatalExitHandler(&mockFatalCall);

      auto filename = _handle->call(&FileSink::fileName);
      if (!filename.valid()) ADD_FAILURE();
      _log_file = filename.get();

#ifdef G3_DYNAMIC_LOGGING
      g3::only_change_at_initialization::addLogLevel(INFO, true);
      g3::only_change_at_initialization::addLogLevel(G3LOG_DEBUG, true);
      g3::only_change_at_initialization::addLogLevel(WARNING, true);
      g3::only_change_at_initialization::addLogLevel(FATAL, true);
#endif
   }

   RestoreFileLogger::~RestoreFileLogger() {
      g3::internal::shutDownLogging(); // is done at reset. Added for test clarity
      reset();

      if (!removeFile(_log_file))
         ADD_FAILURE();
   }

   std::string RestoreFileLogger::logFile() {
      if (_scope) {
         // beware for race condition
         // example: 
         //         LOG(INFO) << ... 
         //     auto file =    logger.logFile()
         //     auto content = ReadContentFromFile(file)
         // ... it is not guaranteed that the content will contain (yet) the LOG(INFO)
         std::future<std::string> filename = _handle->call(&g3::FileSink::fileName);
         _log_file = filename.get();
      }
      return _log_file;
   }

   // Beware of race between LOG(...) and this function. 
   // since LOG(...) passes two queues but the handle::call only passes one queue 
   // the handle::call can happen faster
   std::string RestoreFileLogger::resetAndRetrieveContent() {
      std::future<std::string> filename = _handle->call(&g3::FileSink::fileName);
      reset(); // flush all queues to sinks
      EXPECT_TRUE(filename.valid());
      auto file = filename.get();
      return readFileToText(file);
   }
} // testing_helpers
