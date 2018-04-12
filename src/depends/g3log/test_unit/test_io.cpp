/** ==========================================================================re
 * 2011 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
 * with no warranties. This code is yours to share, use and modify with no
 * strings attached and no restrictions or obligations.
 *
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
 * ============================================================================*/

#include <gtest/gtest.h>
#include "g3log/g3log.hpp"
#include "g3log/logworker.hpp"
#include "testing_helpers.h"
#include "g3log/loglevels.hpp"
#include "g3log/generated_definitions.hpp"

#include <memory>
#include <string>
#include <cstdio>
#include <thread>
#include <chrono>
#include <exception>
#include <algorithm>

namespace {
   const std::string log_directory = "./";
   const std::string t_info = "test INFO ";
   const std::string t_info2 = "test INFO 123";
   const std::string t_debug = "test DEBUG ";
   const std::string t_debug3 = "test DEBUG 1.123456";
   const std::string t_warning = "test WARNING ";
   const std::string t_warning3 = "test WARNING yello";

   std::atomic<size_t> g_fatal_counter = {0};
   void fatalCounter() {
      ++g_fatal_counter;
   }

} // end anonymous namespace


using namespace testing_helpers;


/// THIS MUST BE THE FIRST UNIT TEST TO RUN! If any unit test run before this
/// one then it could fail. For dynamic levels all levels are turned on only AT
/// instantiation so we do different test for dynamic logging levels
///
/// TODO : (gtest issue)
///Move out to separate unit test binary to ensure reordering of tests does not happen
#ifdef G3_DYNAMIC_LOGGING
TEST(Initialization, No_Logger_Initialized___LevelsAreONByDefault) {
   EXPECT_FALSE(g3::internal::isLoggingInitialized());
   EXPECT_TRUE(g3::logLevel(G3LOG_DEBUG));
   EXPECT_TRUE(g3::logLevel(INFO));
   EXPECT_TRUE(g3::logLevel(WARNING));
   EXPECT_TRUE(g3::logLevel(FATAL));
   EXPECT_EQ(G3LOG_DEBUG.value, 100);
   EXPECT_EQ(INFO.value, 300);
   EXPECT_EQ(WARNING.value, 500);
   EXPECT_EQ(FATAL.value, 1000);
   EXPECT_EQ(g3::internal::CONTRACT.value, 2000);
}

TEST(Initialization, No_Logger_Initialized___Expecting_LOG_calls_to_be_Still_OKish) {
   EXPECT_FALSE(g3::internal::isLoggingInitialized());
   EXPECT_TRUE(g3::logLevel(INFO));
   EXPECT_TRUE(g3::logLevel(FATAL));
   EXPECT_TRUE(g3::logLevel(G3LOG_DEBUG));
   EXPECT_TRUE(g3::logLevel(WARNING));
   std::string err_msg1 = "Hey. I am not instantiated but I still should not crash. (I am g3logger)";
   std::string err_msg3_ignored = "This uninitialized message should be ignored";
   try {
      LOG(INFO) << err_msg1; // nothing happened. level not ON
      LOG(INFO) << err_msg3_ignored; // nothing happened. level not ON

   } catch (std::exception& e) {
      ADD_FAILURE() << "Should never have thrown even if it is not instantiated. Ignored exception:  " << e.what();
   }

   RestoreFileLogger logger(log_directory); // now instantiate the logger

   std::string good_msg1 = "This message could have pulled in the uninitialized_call message";
   LOG(INFO) << good_msg1;
   auto content = logger.resetAndRetrieveContent(); // this synchronizes with the LOG(INFO) call if debug level would be ON.
   ASSERT_TRUE(verifyContent(content, err_msg1)) << "Content: [" << content << "]";
   ASSERT_FALSE(verifyContent(content, err_msg3_ignored)) << "Content: [" << content << "]";
   ASSERT_TRUE(verifyContent(content, good_msg1)) << "Content: [" << content << "]";
}
#else
TEST(Initialization, No_Logger_Initialized___Expecting_LOG_calls_to_be_Still_OKish) {
   EXPECT_FALSE(g3::internal::isLoggingInitialized());
   EXPECT_TRUE(g3::logLevel(INFO));
   EXPECT_TRUE(g3::logLevel(FATAL));
   EXPECT_TRUE(g3::logLevel(G3LOG_DEBUG));
   EXPECT_TRUE(g3::logLevel(WARNING));
   std::string err_msg1 = "Hey. I am not instantiated but I still should not crash. (I am g3logger)";
   std::string err_msg3_ignored = "This uninitialized message should be ignored";

   try {
      LOG(INFO) << err_msg1;
      LOG(INFO) << err_msg3_ignored;

   } catch (std::exception& e) {
      ADD_FAILURE() << "Should never have thrown even if it is not instantiated: " << e.what();
   }

   RestoreFileLogger logger(log_directory); // now instantiate the logger

   std::string good_msg1 = "This message will pull in also the uninitialized_call message";
   LOG(INFO) << good_msg1;
   auto content = logger.resetAndRetrieveContent(); // this synchronizes with the LOG(INFO) call.
   ASSERT_TRUE(verifyContent(content, err_msg1)) << "Content: [" << content << "]";
   ASSERT_FALSE(verifyContent(content, err_msg3_ignored)) << "Content: [" << content << "]";
   ASSERT_TRUE(verifyContent(content, good_msg1)) << "Content: [" << content << "]";
}
#endif // #ifdef G3_DYNAMIC_LOGGING

TEST(Basics, Levels_StdFind) {
   std::vector<LEVELS> levels = {INFO, WARNING, FATAL};
   auto info = INFO;
   auto warning = WARNING;
   auto debug = DEBUG;
   auto found_info = std::find(levels.begin(), levels.end(), info);
   EXPECT_TRUE(found_info != levels.end());

   bool wasFound = (levels.end() != std::find(levels.begin(), levels.end(), info));
   EXPECT_TRUE(wasFound);

   auto wasNotFound = (levels.end() == std::find(levels.begin(), levels.end(), debug));
   EXPECT_TRUE(wasNotFound);

   auto foundWarningIterator = std::find(levels.begin(), levels.end(), WARNING);
   EXPECT_TRUE(foundWarningIterator != levels.end());

   foundWarningIterator = std::find(levels.begin(), levels.end(), warning);
   EXPECT_TRUE(foundWarningIterator != levels.end());

   auto wasNotFoundIterator = std::find(levels.begin(), levels.end(), DEBUG);
   EXPECT_FALSE(wasNotFoundIterator != levels.end());
}


TEST(Basics, Levels_Operator) {
   auto info = INFO;
   auto warning = WARNING;
   EXPECT_NE(INFO, WARNING);
   EXPECT_EQ(info, INFO);
   EXPECT_TRUE(INFO == INFO);
   EXPECT_FALSE(info == WARNING);
   EXPECT_TRUE(info != WARNING);
   EXPECT_FALSE(info != INFO);
}

TEST(Basics, Shutdown) {
   std::string file_content;
   {
      RestoreFileLogger logger(log_directory);
      LOG(INFO) << "Not yet shutdown. This message should make it";
      logger.reset(); // force flush of logger (which will trigger a shutdown)
      LOG(INFO) << "Logger is shutdown,. this message will not make it (but it's safe to try)";
      file_content = readFileToText(logger.logFile()); // logger is already reset
      SCOPED_TRACE("LOG_INFO"); // Scope exit be prepared for destructor failure
   }
   EXPECT_TRUE(verifyContent(file_content, "Not yet shutdown. This message should make it"));
   EXPECT_FALSE(verifyContent(file_content, "Logger is shutdown,. this message will not make it (but it's safe to try)"));
}

TEST(Basics, Shutdownx2) {
   std::string file_content;
   {
      RestoreFileLogger logger(log_directory);
      LOG(INFO) << "Not yet shutdown. This message should make it";
      logger.reset(); // force flush of logger (which will trigger a shutdown)
      g3::internal::shutDownLogging(); // already called in reset, but safe to call again
      LOG(INFO) << "Logger is shutdown,. this message will not make it (but it's safe to try)";
      file_content = readFileToText(logger.logFile()); // already reset
      SCOPED_TRACE("LOG_INFO"); // Scope exit be prepared for destructor failure
   }
   EXPECT_TRUE(verifyContent(file_content, "Not yet shutdown. This message should make it"));
   EXPECT_FALSE(verifyContent(file_content, "Logger is shutdown,. this message will not make it (but it's safe to try)"));
}

TEST(Basics, ShutdownActiveLogger) {
   std::string file_content;
   {
      RestoreFileLogger logger(log_directory);
      LOG(INFO) << "Not yet shutdown. This message should make it";
      EXPECT_TRUE(g3::internal::shutDownLoggingForActiveOnly(logger._scope->get()));
      LOG(INFO) << "Logger is shutdown,. this message will not make it (but it's safe to try)";
      file_content = logger.resetAndRetrieveContent();
      SCOPED_TRACE("LOG_INFO"); // Scope exit be prepared for destructor failure
   }
   EXPECT_TRUE(verifyContent(file_content, "Not yet shutdown. This message should make it")) << "\n\n\n***************************\n" << file_content;
   EXPECT_FALSE(verifyContent(file_content, "Logger is shutdown,. this message will not make it (but it's safe to try)"));
}

TEST(Basics, DoNotShutdownActiveLogger) {
   std::string file_content;
   {
      RestoreFileLogger logger(log_directory);
      LOG(INFO) << "Not yet shutdown. This message should make it";
      std::unique_ptr<g3::LogWorker> duplicateLogWorker{g3::LogWorker::createLogWorker()};
      EXPECT_FALSE(g3::internal::shutDownLoggingForActiveOnly(duplicateLogWorker.get()));
      LOG(INFO) << "Logger is (NOT) shutdown,. this message WILL make it";
      file_content = logger.resetAndRetrieveContent();
      SCOPED_TRACE("LOG_INFO"); // Scope exit be prepared for destructor failure
   }
   EXPECT_TRUE(verifyContent(file_content, "Not yet shutdown. This message should make it"));
   EXPECT_TRUE(verifyContent(file_content, "Logger is (NOT) shutdown,. this message WILL make it")) << file_content;
}


TEST(LOGTest, LOG) {
   std::string file_content;
   {
      RestoreFileLogger logger(log_directory);
      EXPECT_TRUE(g3::logLevel(INFO));
      EXPECT_TRUE(g3::logLevel(FATAL));
      LOG(INFO) << "test LOG(INFO)";
      logger.reset(); // force flush of logger
      file_content = readFileToText(logger.logFile());
      SCOPED_TRACE("LOG_INFO"); // Scope exit be prepared for destructor failure
   }
   EXPECT_TRUE(verifyContent(file_content, "test LOG(INFO)"));
   EXPECT_TRUE(g3::logLevel(INFO));
   EXPECT_TRUE(g3::logLevel(FATAL));
}



// printf-type log


TEST(LogTest, LOG_F) {
   std::string file_content;
   {
      RestoreFileLogger logger(log_directory);
      std::cout << "logfilename: " << logger.logFile() << std::flush << std::endl;

      LOGF(INFO, std::string(t_info + "%d").c_str(), 123);
      LOGF(G3LOG_DEBUG, std::string(t_debug + "%f").c_str(), 1.123456);
      LOGF(WARNING, std::string(t_warning + "%s").c_str(), "yello");
      logger.reset(); // force flush of logger
      file_content = readFileToText(logger.logFile());
      SCOPED_TRACE("LOG_INFO"); // Scope exit be prepared for destructor failure
   }
   ASSERT_TRUE(verifyContent(file_content, t_info2));
   ASSERT_TRUE(verifyContent(file_content, t_debug3));
   ASSERT_TRUE(verifyContent(file_content, t_warning3));
}




// stream-type log
TEST(LogTest, LOG) {
   std::string file_content;
   {
      RestoreFileLogger logger(log_directory);
      LOG(INFO) << t_info << 123;
      LOG(G3LOG_DEBUG) << t_debug << std::setprecision(7) << 1.123456f;
      LOG(WARNING) << t_warning << "yello";
      logger.reset(); // force flush of logger
      file_content = readFileToText(logger.logFile());
      SCOPED_TRACE("LOG_INFO"); // Scope exit be prepared for destructor failure
   }
   ASSERT_TRUE(verifyContent(file_content, t_info2));
   ASSERT_TRUE(verifyContent(file_content, t_debug3));
   ASSERT_TRUE(verifyContent(file_content, t_warning3));
}

TEST(LogTest, LOG_after_if) {
   std::string file_content;
   {
      RestoreFileLogger logger(log_directory);
      if(false == file_content.empty()) 
         LOG(INFO) << "This-should-NOT-show-up";
      else
        LOG(INFO) << "This-should-show-up";

      logger.reset(); // force flush of logger
      file_content = readFileToText(logger.logFile());
   }

   ASSERT_FALSE(verifyContent(file_content, "This-should-NOT-show-up"));
   ASSERT_TRUE(verifyContent(file_content, "This-should-show-up"));
}


TEST(LogTest, LOG_after_if_with_parentesis) {
   std::string file_content;
   {
      RestoreFileLogger logger(log_directory);
      if(false == file_content.empty()) {
         LOG(INFO) << "This-should-NOT-show-up";
      } else {
        LOG(INFO) << "This-should-show-up";
      }

      logger.reset(); // force flush of logger
      file_content = readFileToText(logger.logFile());
   }

   ASSERT_FALSE(verifyContent(file_content, "This-should-NOT-show-up"));
   ASSERT_TRUE(verifyContent(file_content, "This-should-show-up"));
}



TEST(LogTest, LOG_F_IF) {
   std::string file_content;
   {
      RestoreFileLogger logger(log_directory);
      LOGF_IF(INFO, (2 == 2), std::string(t_info + "%d").c_str(), 123);
      LOGF_IF(G3LOG_DEBUG, (2 != 2), std::string(t_debug + "%f").c_str(), 1.123456);
      logger.reset(); // force flush of logger
      file_content = readFileToText(logger.logFile());
      SCOPED_TRACE("LOG_IF"); // Scope exit be prepared for destructor failure
   }
   ASSERT_TRUE(verifyContent(file_content, t_info2));
   ASSERT_FALSE(verifyContent(file_content, t_debug3));
}


TEST(LogTest, LOG_IF) {
   std::string file_content;
   {
      RestoreFileLogger logger(log_directory);
      LOG_IF(INFO, (2 == 2)) << t_info << 123;
      LOG_IF(G3LOG_DEBUG, (2 != 2)) << t_debug << std::setprecision(7) << 1.123456f;
      logger.reset(); // force flush of logger
      file_content = readFileToText(logger.logFile());
      SCOPED_TRACE("LOG_IF"); // Scope exit be prepared for destructor failure
   }
   EXPECT_TRUE(verifyContent(file_content, t_info2));
   EXPECT_FALSE(verifyContent(file_content, t_debug3));
}
TEST(LogTest, LOGF__FATAL) {
   RestoreFileLogger logger(log_directory);
   ASSERT_FALSE(mockFatalWasCalled());
   LOGF(FATAL, "This message should throw %d", 0);
   EXPECT_TRUE(mockFatalWasCalled());
   EXPECT_TRUE(verifyContent(mockFatalMessage(), "EXIT trigger caused by "));
   EXPECT_TRUE(verifyContent(mockFatalMessage(), "This message should throw 0")) << "\n****" << mockFatalMessage();
   EXPECT_TRUE(verifyContent(mockFatalMessage(), "FATAL"));

   auto file_content = logger.resetAndRetrieveContent();
   EXPECT_TRUE(verifyContent(file_content, "This message should throw 0")) << "\n****" << file_content;
   EXPECT_TRUE(verifyContent(file_content, "FATAL"));
}

#ifndef DISABLE_FATAL_SIGNALHANDLING

TEST(LogTest, FatalSIGTERM__UsingDefaultHandler) {
   RestoreFileLogger logger(log_directory);
   g_fatal_counter.store(0);
   g3::setFatalPreLoggingHook(fatalCounter);
   raise(SIGTERM);
   logger.reset();
   EXPECT_EQ(g_fatal_counter.load(), size_t{1});
}

#if !(defined(WIN32) || defined(_WIN32) || defined(__WIN32__))   
namespace {
   std::atomic<size_t> customFatalCounter = {0};
   std::atomic<int> lastEncounteredSignal = {0};
   void customSignalHandler(int signal_number, siginfo_t* info, void* unused_context) {
      lastEncounteredSignal.store(signal_number);
	  ++customFatalCounter;
   }
   void installCustomSIGTERM() {
      struct sigaction action;
	  memset(&action, 0, sizeof(action));
	  sigemptyset(&action.sa_mask);
	  action.sa_sigaction = &customSignalHandler;
	  action.sa_flags = SA_SIGINFO;
	  sigaction(SIGTERM, &action, nullptr);
   }


   std::atomic<bool>  oldSigTermCheck = {false};
   void customOldSignalHandler(int signal_number, siginfo_t* info, void* unused_context) {
      lastEncounteredSignal.store(signal_number);
      oldSigTermCheck.store(true);
   }
   void installCustomOldSIGTERM() {
      struct sigaction action;
     memset(&action, 0, sizeof(action));
     sigemptyset(&action.sa_mask);
     action.sa_sigaction = &customOldSignalHandler;
     action.sa_flags = SA_SIGINFO;
     sigaction(SIGTERM, &action, nullptr);
   }

} // anonymous

// Override of signal handling and testing of it should be fairly easy to port to windows
// ref: https://github.com/KjellKod/g3log/blob/master/src/crashhandler_windows.cpp
// what is missing is the override of signals and custom installation of signals
// ref: https://github.com/KjellKod/g3log/blob/master/src/crashhandler_unix.cpp
//      functions: void restoreSignalHandlerToDefault()
//                 void overrideSetupSignals(const std::map<int, std::string> overrideSignals)
//                 void restoreSignalHandler(int signal_number)
//
// It would require some adding of unit test (see the test below)
// and good Windows experience. Since I am not currently working much on the Windows
// side I am reaching out to the community for this one:
//
//
// For the test to work the following code should be added in this test
//void customSignalHandler(int signal_number) {
//	lastEncounteredSignal.store(signal_number);
//	++customFatalCounter;
//}
//
//void installCustomSIGTERM() {
//	ASSERT_TRUE(SIG_ERR != signal(SIGTERM, customSignalHandler));
//} 


TEST(LogTest, FatalSIGTERM__UsingCustomHandler) {
   RestoreFileLogger logger(log_directory);
   g_fatal_counter.store(0);
   g3::setFatalPreLoggingHook(fatalCounter);
   installCustomSIGTERM();
   g3::overrideSetupSignals({ {SIGABRT, "SIGABRT"}, {SIGFPE, "SIGFPE"}, {SIGILL, "SIGILL"}});

   installCustomSIGTERM();
   EXPECT_EQ(customFatalCounter.load(), size_t{0});
   EXPECT_EQ(lastEncounteredSignal.load(), 0);

   raise(SIGTERM);
   logger.reset();
   EXPECT_EQ(g_fatal_counter.load(), size_t{0});
   EXPECT_EQ(lastEncounteredSignal.load(), SIGTERM);
   EXPECT_EQ(customFatalCounter.load(), size_t{1});
}

TEST(LogTest, FatalSIGTERM__VerifyingOldCustomHandler) {
   RestoreFileLogger logger(log_directory);
   g_fatal_counter.store(0);
   customFatalCounter.store(0);
   lastEncounteredSignal.store(0);

   g3::setFatalPreLoggingHook(fatalCounter);
   installCustomOldSIGTERM();
   g3::overrideSetupSignals({ {SIGABRT, "SIGABRT"}, {SIGFPE, "SIGFPE"}, {SIGILL, "SIGILL"}, {SIGTERM, "SIGTERM"}});
   g3::restoreSignalHandler(SIGTERM); // revert SIGTERM installation

   EXPECT_EQ(customFatalCounter.load(), size_t{0});
   EXPECT_EQ(lastEncounteredSignal.load(), 0);
   EXPECT_FALSE(oldSigTermCheck.load());
   raise(SIGTERM);
   logger.reset();
   EXPECT_EQ(g_fatal_counter.load(), size_t{0});
   EXPECT_EQ(lastEncounteredSignal.load(), SIGTERM);
   EXPECT_TRUE(oldSigTermCheck.load()); 
}





#endif

#endif

TEST(LogTest, LOG_preFatalLogging_hook) {
   {
      RestoreFileLogger logger(log_directory);
      ASSERT_FALSE(mockFatalWasCalled());
      g_fatal_counter.store(0);
      g3::setFatalPreLoggingHook(fatalCounter);
      LOG(FATAL) << "This message is fatal";
      logger.reset();
      EXPECT_EQ(g_fatal_counter.load(), size_t{1});
   }
   {
      // Now with no fatal pre-logging-hook
      RestoreFileLogger logger(log_directory);
      ASSERT_FALSE(mockFatalWasCalled());
      g_fatal_counter.store(0);
      LOG(FATAL) << "This message is fatal";
      EXPECT_EQ(g_fatal_counter.load(), size_t{0});
   }
}



TEST(LogTest, LOG_FATAL) {
   RestoreFileLogger logger(log_directory);
   ASSERT_FALSE(mockFatalWasCalled());



   LOG(FATAL) << "This message is fatal";
   EXPECT_TRUE(mockFatalWasCalled());
   EXPECT_TRUE(verifyContent(mockFatalMessage(), "EXIT trigger caused by "));
   EXPECT_TRUE(verifyContent(mockFatalMessage(), "This message is fatal"))
         << "\ncontent: [[" << mockFatalMessage() << "]]";
   EXPECT_TRUE(verifyContent(mockFatalMessage(), "FATAL"));

   logger.reset();
   std::string file_content = readFileToText(logger.logFile());
   EXPECT_TRUE(verifyContent(file_content, "This message is fatal"));
   EXPECT_TRUE(verifyContent(file_content, "FATAL"));
   EXPECT_TRUE(verifyContent(file_content, "EXIT trigger caused by "));
}
TEST(LogTest, LOGF_IF__FATAL) {
   RestoreFileLogger logger(log_directory);
   EXPECT_FALSE(mockFatalWasCalled());
   LOGF_IF(FATAL, (2 < 3), "This message %s be worse", "could");
   EXPECT_TRUE(mockFatalWasCalled());
   EXPECT_TRUE(verifyContent(mockFatalMessage(), "EXIT trigger caused by ")) << "\n" << mockFatalMessage();
   EXPECT_TRUE(verifyContent(mockFatalMessage(), "FATAL"));
   EXPECT_TRUE(verifyContent(mockFatalMessage(), "This message could be worse"));

   logger.reset();
   std::string file_content = readFileToText(logger.logFile());
   EXPECT_TRUE(verifyContent(file_content, "EXIT trigger caused by "));
   EXPECT_TRUE(verifyContent(file_content, "FATAL"));
   EXPECT_TRUE(verifyContent(file_content, "This message could be worse"));
}
TEST(LogTest, LOG_IF__FATAL) {
   RestoreFileLogger logger(log_directory);
   LOG_IF(WARNING, (0 != t_info.compare(t_info))) << "This message should NOT be written";
   EXPECT_FALSE(mockFatalWasCalled());
   LOG_IF(FATAL, (0 != t_info.compare(t_info2))) << "This message should throw. xyz ";
   EXPECT_TRUE(mockFatalWasCalled());

   EXPECT_TRUE(verifyContent(mockFatalMessage(), "EXIT trigger caused by "));
   EXPECT_TRUE(verifyContent(mockFatalMessage(), "FATAL"));
   EXPECT_TRUE(verifyContent(mockFatalMessage(), "This message should throw. xyz "));

   logger.reset();
   std::string file_content = readFileToText(logger.logFile());
   EXPECT_TRUE(verifyContent(file_content, "EXIT trigger caused by "));
   EXPECT_TRUE(verifyContent(file_content, "FATAL"));
   EXPECT_TRUE(verifyContent(file_content, "This message should throw. xyz "));
}
TEST(LogTest, LOG_IF__FATAL__NO_THROW) {
   RestoreFileLogger logger(log_directory);
   LOG_IF(FATAL, (2 > 3)) << "This message%sshould NOT throw";
   ASSERT_FALSE(mockFatalWasCalled());
}


// CHECK_F
TEST(CheckTest, CHECK_F__thisWILL_PrintErrorMsg) {
   RestoreFileLogger logger(log_directory);
   EXPECT_TRUE(mockFatalMessage().empty());
   EXPECT_FALSE(mockFatalWasCalled());

   CHECK(1 == 2);
   EXPECT_FALSE(mockFatalMessage().empty());
   EXPECT_TRUE(mockFatalWasCalled());

   logger.reset();
   std::string file_content = readFileToText(logger.logFile());
   EXPECT_TRUE(verifyContent(mockFatalMessage(), "EXIT trigger caused by "));
   EXPECT_TRUE(verifyContent(file_content, "CONTRACT"))  << "**** " << mockFatalMessage();
}


TEST(CHECK_F_Test, CHECK_F__thisWILL_PrintErrorMsg) {
   RestoreFileLogger logger(log_directory);
   std::string msg = "This message is added to throw %s and %s";
   std::string arg1 = "message";
   std::string arg3 = "log";

   CHECK_F(1 >= 2, msg.c_str(), arg1.c_str(), arg3.c_str());
   logger.reset();
   std::string file_content = readFileToText(logger.logFile());
   EXPECT_TRUE(verifyContent(mockFatalMessage(), "EXIT trigger caused by "));
   EXPECT_TRUE(verifyContent(file_content, "CONTRACT"));
}


TEST(CHECK_Test, CHECK__thisWILL_PrintErrorMsg) {
   RestoreFileLogger logger(log_directory);
   std::string msg = "This message is added to throw message and log";
   CHECK(1 >= 2) << msg;

   logger.reset();
   std::string file_content = readFileToText(logger.logFile());
   EXPECT_TRUE(verifyContent(mockFatalMessage(), "EXIT trigger caused by "));
   EXPECT_TRUE(verifyContent(file_content, "CONTRACT"));
   EXPECT_TRUE(verifyContent(file_content, msg));
}
TEST(CHECK, CHECK_ThatWontThrow) {
   RestoreFileLogger logger(log_directory);
   std::string msg = "This %s should never appear in the %s";
   std::string msg3 = "This message should never appear in the log";
   CHECK(1 == 1);
   CHECK_F(1 == 1, msg.c_str(), "message", "log");
   logger.reset();
   EXPECT_FALSE(mockFatalWasCalled());

   std::string file_content = readFileToText(logger.logFile());
   EXPECT_FALSE(verifyContent(file_content, msg3));
   EXPECT_FALSE(verifyContent(mockFatalMessage(), msg3));
}



TEST(CustomLogLevels, AddANonFatal) {
   RestoreFileLogger logger(log_directory);
   const LEVELS MYINFO {WARNING.value + 1, {"MY_INFO_LEVEL"}};
#ifdef G3_DYNAMIC_LOGGING
   g3::only_change_at_initialization::addLogLevel(MYINFO, true);
#endif
   LOG(MYINFO) << "Testing my own custom level"; auto line = __LINE__;
   logger.reset();
   std::string file_content = readFileToText(logger.logFile());
   std::string expected;
   expected += "MY_INFO_LEVEL [test_io.cpp->" + std::string(__FUNCTION__) + ":" + std::to_string(line);
   EXPECT_TRUE(verifyContent(file_content, expected)) << file_content
         << "\n\nExpected: \n" << expected;
}

TEST(CustomLogLevels, AddFatal) {
   RestoreFileLogger logger(log_directory);
   const LEVELS DEADLY {FATAL.value + 1, {"DEADLY"}};
   EXPECT_TRUE(g3::internal::wasFatal(DEADLY));
   g_fatal_counter.store(0);
   ASSERT_FALSE(mockFatalWasCalled());
   g3::setFatalPreLoggingHook(fatalCounter);
#ifdef G3_DYNAMIC_LOGGING
   g3::only_change_at_initialization::addLogLevel(DEADLY, true);
#endif

   LOG(DEADLY) << "Testing my own custom level"; auto line = __LINE__;
   logger.reset();
   ASSERT_TRUE(mockFatalWasCalled());
   EXPECT_EQ(size_t{1}, g_fatal_counter.load());

   std::string file_content = readFileToText(logger.logFile());
   std::string expected;
   expected += "DEADLY [test_io.cpp->" + std::string(__FUNCTION__) + ":" + std::to_string(line);
   EXPECT_TRUE(verifyContent(file_content, expected)) << file_content
         << "\n\nExpected: \n" << expected;
   g_fatal_counter.store(0); // restore
}


#ifdef G3_DYNAMIC_LOGGING
namespace {
   // Restore dynamic levels if turned off

   struct RestoreDynamicLoggingLevels {
      RestoreDynamicLoggingLevels() {
      };
      ~RestoreDynamicLoggingLevels() {
         g3::only_change_at_initialization::reset();
         g3::only_change_at_initialization::addLogLevel(G3LOG_DEBUG, false);
         g3::only_change_at_initialization::addLogLevel(INFO, false);
         g3::only_change_at_initialization::addLogLevel(WARNING, false);
         g3::only_change_at_initialization::addLogLevel(FATAL, false);
      }
   };
} // anonymous


TEST(CustomLogLevels, AddANonFatal__ThenReset) {
   RestoreFileLogger logger(log_directory);
   const LEVELS MYINFO {WARNING.value + 2, {"MY_INFO_LEVEL"}};
   EXPECT_FALSE(g3::logLevel(MYINFO));
   g3::only_change_at_initialization::addLogLevel(MYINFO, true);
   EXPECT_TRUE(g3::logLevel(MYINFO));
   g3::only_change_at_initialization::reset();
   EXPECT_FALSE(g3::logLevel(MYINFO));
}


TEST(CustomLogLevels, AddANonFatal__DidNotAddItToEnabledValue1) {
   RestoreFileLogger logger(log_directory);
   const LEVELS MYINFO {WARNING.value + 2, {"MY_INFO_LEVEL"}};
   LOG(MYINFO) << "Testing my own custom level"; auto line = __LINE__;
   logger.reset();

   std::string file_content = readFileToText(logger.logFile());
   std::string expected;
   expected += "MY_INFO_LEVEL [test_io.cpp:" + std::to_string(line);
   EXPECT_FALSE(verifyContent(file_content, expected)) << file_content
         << "\n\nExpected: \n" << expected  << "\nLevels:\n" << g3::log_levels::to_string();
}

TEST(CustomLogLevels, AddANonFatal__DidNotAddItToEnabledValue2) {
   RestoreFileLogger logger(log_directory);
   const LEVELS MYINFO {WARNING.value + 2, {"MY_INFO_LEVEL"}};
   EXPECT_FALSE(g3::logLevel(MYINFO));
   LOG(MYINFO) << "Testing my own custom level"; auto line = __LINE__;
   logger.reset();

   std::string file_content = readFileToText(logger.logFile());
   std::string expected;
   expected += "MY_INFO_LEVEL [test_io.cpp:" + std::to_string(line);
   EXPECT_FALSE(verifyContent(file_content, expected)) << file_content
         << "\n\nExpected: \n" << expected  << "\nLevels:\n" << g3::log_levels::to_string();
}

TEST(CustomLogLevels, AddANonFatal__DidtAddItToEnabledValue) {
   RestoreFileLogger logger(log_directory);
   const LEVELS MYINFO {WARNING.value + 3, {"MY_INFO_LEVEL"}};
   g3::only_change_at_initialization::addLogLevel(MYINFO, true);
   LOG(MYINFO) << "Testing my own custom level"; auto line = __LINE__;
   logger.reset();
   std::string file_content = readFileToText(logger.logFile());
   std::string expected;
   expected += "MY_INFO_LEVEL [test_io.cpp->" + std::string(__FUNCTION__) + ":" + std::to_string(line);
   EXPECT_TRUE(verifyContent(file_content, expected)) << file_content
         << "\n\nExpected: \n" << expected;
}


TEST(DynamicLogging, DynamicLogging_IS_ENABLED) {
   RestoreDynamicLoggingLevels raiiLevelRestore;

   ASSERT_TRUE(g3::logLevel(G3LOG_DEBUG));
   ASSERT_TRUE(g3::logLevel(INFO));
   ASSERT_TRUE(g3::logLevel(WARNING));
   ASSERT_TRUE(g3::logLevel(FATAL)); // Yes FATAL can be turned off. Thereby rendering it ineffective.
   g3::only_change_at_initialization::addLogLevel(G3LOG_DEBUG, false);
   ASSERT_FALSE(g3::logLevel(G3LOG_DEBUG));
   ASSERT_TRUE(g3::logLevel(INFO));
   ASSERT_TRUE(g3::logLevel(WARNING));
   ASSERT_TRUE(g3::logLevel(FATAL)); // Yes FATAL can be turned off. Thereby rendering it ineffective.

   g3::only_change_at_initialization::addLogLevel(INFO, false);
   ASSERT_FALSE(g3::logLevel(G3LOG_DEBUG));
   ASSERT_FALSE(g3::logLevel(INFO));
   ASSERT_TRUE(g3::logLevel(WARNING));
   ASSERT_TRUE(g3::logLevel(FATAL)); // Yes FATAL can be turned off. Thereby rendering it ineffective.

   g3::only_change_at_initialization::addLogLevel(WARNING, false);
   ASSERT_FALSE(g3::logLevel(G3LOG_DEBUG));
   ASSERT_FALSE(g3::logLevel(INFO));
   ASSERT_FALSE(g3::logLevel(WARNING));
   ASSERT_TRUE(g3::logLevel(FATAL)); // Yes FATAL can be turned off. Thereby rendering it ineffective.

   g3::only_change_at_initialization::addLogLevel(FATAL, false);
   ASSERT_FALSE(g3::logLevel(G3LOG_DEBUG));
   ASSERT_FALSE(g3::logLevel(INFO));
   ASSERT_FALSE(g3::logLevel(WARNING));
   ASSERT_FALSE(g3::logLevel(FATAL)); // Yes FATAL can be turned off. Thereby rendering it ineffective.
}
TEST(DynamicLogging, DynamicLogging_No_Logs_If_Disabled) {
   {
      RestoreFileLogger logger(log_directory);
      ASSERT_TRUE(g3::logLevel(G3LOG_DEBUG));
      ASSERT_TRUE(g3::logLevel(INFO));
      ASSERT_TRUE(g3::logLevel(WARNING));
      ASSERT_TRUE(g3::logLevel(FATAL));
   }

   RestoreDynamicLoggingLevels raiiLevelRestore;

   std::string msg_debugOn = "This %s SHOULD  appear in the %s";
   std::string msg_debugOff = "This message should never appear in the log";
   try {
      {
         RestoreFileLogger logger(log_directory);
         LOGF(G3LOG_DEBUG, msg_debugOn.c_str(), "msg", "log");
         auto content = logger.resetAndRetrieveContent();
         ASSERT_TRUE(verifyContent(content, "This msg SHOULD  appear in the log")) << "Content: [" << content << "]";
      }

      {
         RestoreFileLogger logger(log_directory);
         g3::only_change_at_initialization::addLogLevel(G3LOG_DEBUG, false);
         EXPECT_FALSE(g3::logLevel(G3LOG_DEBUG));
         LOG(G3LOG_DEBUG) << msg_debugOff;
         auto content = logger.resetAndRetrieveContent();
         ASSERT_FALSE(verifyContent(content, "This message should never appear in the log")) << "Content: [" << content << "]";
      }

   } catch (std::exception const& e) {
      ADD_FAILURE() << "Should never have thrown: " << e.what();
   }
}
TEST(DynamicLogging, DynamicLogging_No_Fatal_If_Disabled) {
   RestoreFileLogger logger(log_directory);
   RestoreDynamicLoggingLevels raiiLevelRestore;
   ASSERT_TRUE(g3::logLevel(G3LOG_DEBUG));
   ASSERT_TRUE(g3::logLevel(INFO));
   ASSERT_TRUE(g3::logLevel(WARNING));
   ASSERT_TRUE(g3::logLevel(FATAL));

   std::string msg1 = "This IS fatal (not crash, since it is unit test";

   LOG(FATAL) << msg1;
   EXPECT_TRUE(mockFatalWasCalled());
   EXPECT_FALSE(mockFatalMessage().empty());
   EXPECT_TRUE(verifyContent(mockFatalMessage(), msg1));

   clearMockFatal();
   EXPECT_FALSE(mockFatalWasCalled());


   g3::only_change_at_initialization::addLogLevel(FATAL, false);
   std::string msg3 = "This is NOT fatal (not crash, since it is unit test. FATAL is disabled";
   LOG(FATAL) << msg3;
   EXPECT_FALSE(mockFatalWasCalled());
   EXPECT_TRUE(mockFatalMessage().empty());
}


TEST(DynamicLogging, DynamicLogging_Check_WillAlsoBeTurnedOffWhen_Fatal_Is_Disabled) {
   RestoreFileLogger logger(log_directory);
   RestoreDynamicLoggingLevels raiiLevelRestore;
   ASSERT_TRUE(g3::logLevel(FATAL));

   std::string msg1 = "dummy message to check if CHECK worked when fatal is enabled";
   std::string msg3 = "dummy message to check if CHECK worked when fatal is disabled";
   LOG(FATAL) << msg1;
   EXPECT_TRUE(mockFatalWasCalled());
   EXPECT_TRUE(verifyContent(mockFatalMessage(), msg1));

   clearMockFatal();
   EXPECT_FALSE(mockFatalWasCalled());

   // Disable also CHECK calls
   g3::only_change_at_initialization::addLogLevel(FATAL, false);
   ASSERT_FALSE(g3::logLevel(FATAL));
   LOG(FATAL) << msg3;
   EXPECT_FALSE(mockFatalWasCalled());
}




#else
TEST(DynamicLogging, DynamicLogging_IS_NOT_ENABLED) {
   ASSERT_TRUE(g3::logLevel(G3LOG_DEBUG));
   //g3::addLogLevel(G3LOG_DEBUG, false);  this line will not compile since G3_DYNAMIC_LOGGING is not enabled. Kept for show.
   //ASSERT_FALSE(g3::logLevel(G3LOG_DEBUG));
}
#endif // Dynamic logging



