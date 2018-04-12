/** ==========================================================================
* 2016 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
* with no warranties. This code is yours to share, use and modify with no
* strings attached and no restrictions or obligations.
 *
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
* ============================================================================*/

#include <gtest/gtest.h>
#include <g3log/g3log.hpp>
#include <g3log/time.hpp>
#include <iostream>
#include <ctime>
#include <cstdlib>
#include <g3log/generated_definitions.hpp>
#include <testing_helpers.h>
#include <g3log/filesink.hpp>
namespace {
   // https://www.epochconverter.com/
   // epoc value for: Thu, 27 Apr 2017 06:22:49 GMT
   time_t k2017_April_27th = 1493274147;
   auto kTimePoint_2017_April_27th = std::chrono::system_clock::from_time_t(k2017_April_27th);
   std::chrono::time_point<std::chrono::system_clock> k1970_January_1st = {};
   const std::string kFile = __FILE__;
   const int kLine = 123;
   const std::string kFunction = "MyTest::Foo";
   const LEVELS kLevel = INFO;
   const std::string testdirectory = "./";


}


TEST(Message, DefaultLogDetals_toString) {
   using namespace g3;
   LogMessage msg{kFile, kLine, kFunction, kLevel};
   auto details = LogMessage::DefaultLogDetailsToString(msg);
   auto details2 = msg._logDetailsToStringFunc(msg);
   EXPECT_EQ(details, details2);
}

TEST(Message, Default_toString) {
   using namespace g3;
   LogMessage msg{kFile, kLine, kFunction, kLevel};
   auto details = LogMessage::DefaultLogDetailsToString(msg);
   auto output = msg.toString();
   testing_helpers::verifyContent(output, details);
}


TEST(Message, UseOverride_4_DetailsWithThreadID_toString) {
   using namespace g3;
   LogMessage msg{kFile, kLine, kFunction, kLevel};
   msg.overrideLogDetailsFunc(&LogMessage::FullLogDetailsToString);
   auto output = msg.toString();

   std::ostringstream thread_id_oss;
   thread_id_oss << std::this_thread::get_id();
   testing_helpers::verifyContent(output, thread_id_oss.str());
   testing_helpers::verifyContent(output, kFile);
   testing_helpers::verifyContent(output, kLevel.text);
   testing_helpers::verifyContent(output, kFunction);
   testing_helpers::verifyContent(output, std::to_string(kLine));
   std::cout << output << std::endl;
}

TEST(Message, UseLogCall_4_DetailsWithThreadID_toString) {
   using namespace g3;
   LogMessage msg{kFile, kLine, kFunction, kLevel};
   auto output = msg.toString(&LogMessage::FullLogDetailsToString);

   std::ostringstream thread_id_oss;
   thread_id_oss << std::this_thread::get_id();
   testing_helpers::verifyContent(output, thread_id_oss.str());
   testing_helpers::verifyContent(output, kFile);
   testing_helpers::verifyContent(output, kLevel.text);
   testing_helpers::verifyContent(output, kFunction);
   testing_helpers::verifyContent(output, std::to_string(kLine));
   std::cout << output << std::endl;
}



TEST(Message, DefaultFormattingToLogFile) {
   using namespace g3;
   std::string file_content;
   {
      testing_helpers::RestoreFileLogger logger(testdirectory);
      LOG(WARNING) << "testing";
      logger.reset(); // force flush of logger (which will trigger a shutdown)
      file_content = testing_helpers::readFileToText(logger.logFile()); // logger is already reset
   }
   
   std::ostringstream thread_id_oss;
   thread_id_oss << " [" << std::this_thread::get_id() << " ";
   EXPECT_FALSE(testing_helpers::verifyContent(file_content, thread_id_oss.str()));
}



TEST(Message, FullFormattingToLogFile) {
   using namespace g3;
   std::string file_content;
   {
      testing_helpers::RestoreFileLogger logger(testdirectory);
      logger._handle->call(&FileSink::overrideLogDetails, &LogMessage::FullLogDetailsToString);

      LOG(WARNING) << "testing";
      logger.reset(); // force flush of logger (which will trigger a shutdown)
      file_content = testing_helpers::readFileToText(logger.logFile()); // logger is already reset
   }
   
   std::ostringstream thread_id_oss;
   thread_id_oss << " [" << std::this_thread::get_id() << " ";
   EXPECT_TRUE(testing_helpers::verifyContent(file_content, thread_id_oss.str()));
}



TEST(Message, CppSupport) {
   // ref: http://www.cplusplus.com/reference/clibrary/ctime/strftime/
   // ref: http://en.cppreference.com/w/cpp/io/manip/put_time
   //  Day Month Date Time Year: is written as "%a %b %d %H:%M:%S %Y" and formatted output as : Wed Sep 19 08:28:16 2012
   // --- WARNING: The try/catch setup does NOT work,. but for fun and for fake-clarity I leave it
   // ---  For formatting options to std::put_time that are NOT YET implemented on Windows fatal errors/assert will occurr
   // ---  the last example is such an example.
   try {
      std::cout << g3::localtime_formatted(std::chrono::system_clock::now(), "%a %b %d %H:%M:%S %Y")  << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(1));
      std::cout << g3::localtime_formatted(std::chrono::system_clock::now(), "%%Y/%%m/%%d %%H:%%M:%%S = %Y/%m/%d %H:%M:%S")  << std::endl;
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
      std::cerr << "Formatting options skipped due to VS2012, C++11 non-conformance for" << std::endl;
      std::cerr << " some formatting options. The skipped code was:\n\t\t %EX %Ec, \n(see http://en.cppreference.com/w/cpp/io/manip/put_time for details)"  << std::endl;
#else
      std::cout << "C++11 new formatting options:\n" << g3::localtime_formatted(std::chrono::system_clock::now(), "%%EX: %EX\n%%z: %z\n%%Ec: %Ec")  << std::endl;
#endif
   }
// This does not work. Other kinds of fatal exits (on Windows) seems to be used instead of exceptions
// Maybe a signal handler catch would be better? --- TODO: Make it better, both failing and correct
   catch (...) {
      ADD_FAILURE() << "On this platform the library does not support given (C++11?) specifiers";
      return;
   }
   ASSERT_TRUE(true); // no exception. all good
}



TEST(Message, GetFractional_Empty_buffer_ExpectDefaults) {
   auto fractional = g3::internal::getFractional("", 0);
   const auto expected = g3::internal::Fractional::NanosecondDefault;
   EXPECT_EQ(fractional, expected);
   fractional = g3::internal::getFractional("", 100);
   EXPECT_EQ(fractional, expected);
}

TEST(Message, GetFractional_MilliSeconds) {
   auto fractional = g3::internal::getFractional("%f3", 0);
   const auto expected = g3::internal::Fractional::Millisecond;
   EXPECT_EQ(fractional, expected);
}

TEST(Message, GetFractional_Microsecond) {
   auto fractional = g3::internal::getFractional("%f6", 0);
   const auto expected = g3::internal::Fractional::Microsecond;
   EXPECT_EQ(fractional, expected);
}

TEST(Message, GetFractional_Nanosecond) {
   auto fractional = g3::internal::getFractional("%f9", 0);
   const auto expected = g3::internal::Fractional::Nanosecond;
   EXPECT_EQ(fractional, expected);
}

TEST(Message, GetFractional_NanosecondDefault) {
   auto fractional = g3::internal::getFractional("%f", 0);
   const auto expected = g3::internal::Fractional::NanosecondDefault;
   EXPECT_EQ(fractional, expected);
}

TEST(Message, GetFractional_All) {
   std::string formatted = "%f, %f9, %f6, %f3";
   auto fractional = g3::internal::getFractional(formatted, 0);
   auto expected = g3::internal::Fractional::NanosecondDefault;
   EXPECT_EQ(fractional, expected);

   // ns
   fractional = g3::internal::getFractional(formatted, 4);
   expected = g3::internal::Fractional::Nanosecond;
   EXPECT_EQ(fractional, expected);

   // us
   fractional = g3::internal::getFractional(formatted, 9);
   expected = g3::internal::Fractional::Microsecond;
   EXPECT_EQ(fractional, expected);

   // ms
   fractional = g3::internal::getFractional(formatted, 14);
   expected = g3::internal::Fractional::Millisecond;
   EXPECT_EQ(fractional, expected);
}



TEST(Message, FractionalToString_SizeCheck) {
   auto value = g3::internal::to_string(kTimePoint_2017_April_27th, g3::internal::Fractional::Nanosecond);
   EXPECT_EQ("000000000", value);
   value = g3::internal::to_string(kTimePoint_2017_April_27th, g3::internal::Fractional::NanosecondDefault);
   EXPECT_EQ("000000000", value);

   // us
   value = g3::internal::to_string(kTimePoint_2017_April_27th, g3::internal::Fractional::Microsecond);
   EXPECT_EQ("000000", value);
// ms
   value = g3::internal::to_string(kTimePoint_2017_April_27th, g3::internal::Fractional::Millisecond);
   EXPECT_EQ("000", value);
}

TEST(Message, FractionalToStringNanoPadded) {

   auto value = g3::internal::to_string(k1970_January_1st, g3::internal::Fractional::Nanosecond);
   EXPECT_EQ("000000000", value);
   // 0000000012
   value = g3::internal::to_string(k1970_January_1st, g3::internal::Fractional::NanosecondDefault);
   EXPECT_EQ("000000000", value);
}

TEST(Message, FractionalToString12NanoPadded) {
   auto value = g3::internal::to_string(k1970_January_1st, g3::internal::Fractional::Nanosecond);
   EXPECT_EQ("000000000", value);
   // 0000000012
   value = g3::internal::to_string(k1970_January_1st, g3::internal::Fractional::NanosecondDefault);
   EXPECT_EQ("000000000", value);
}


TEST(Message, FractionalToStringMicroPadded) {
   auto value = g3::internal::to_string(k1970_January_1st, g3::internal::Fractional::Microsecond);
   EXPECT_EQ("000000", value);
   value = g3::internal::to_string(k1970_January_1st, g3::internal::Fractional::Microsecond);
   EXPECT_EQ("000000", value);

}


TEST(Message, FractionalToStringMilliPadded) {
   auto value = g3::internal::to_string(k1970_January_1st, g3::internal::Fractional::Millisecond);
   EXPECT_EQ("000", value);
   value = g3::internal::to_string(k1970_January_1st, g3::internal::Fractional::Millisecond);
   EXPECT_EQ("000", value);
}


#if !(defined(WIN32) || defined(_WIN32) || defined(__WIN32__))

TEST(Message, localtime_formatted) {
   char* tz = nullptr;

   std::shared_ptr<void> RaiiTimeZoneReset(nullptr, [&](void*) {
      if (tz)
         setenv("TZ", tz, 1);
      else
         unsetenv("TZ");
      tzset();

   });
   tz = getenv("TZ");
   setenv("TZ", "", 1);
   tzset();


   auto time_point = std::chrono::system_clock::from_time_t(k2017_April_27th);
   auto format = g3::localtime_formatted(time_point, "%Y-%m-%d %H:%M:%S"); // %Y/%m/%d
   std::string expected = {"2017-04-27 06:22:27"};
   EXPECT_EQ(expected, format);

   auto us_format = g3::localtime_formatted(time_point, g3::internal::time_formatted); // "%H:%M:%S %f6";
   EXPECT_EQ("06:22:27 000000", us_format);

   auto ns_format = g3::localtime_formatted(time_point, "%H:%M:%S %f");
   EXPECT_EQ("06:22:27 000000000", ns_format);

   auto ms_format = g3::localtime_formatted(time_point, "%H:%M:%S %f3");
   EXPECT_EQ("06:22:27 000", ms_format);

}
#endif // timezone 

#if defined(CHANGE_G3LOG_DEBUG_TO_DBUG)
TEST(Level, G3LogDebug_is_DBUG) {
 LOG(DBUG) << "DBUG equals G3LOG_DEBUG";
 LOG(G3LOG_DEBUG) << "G3LOG_DEBUG equals DBUG";
}
#else
TEST(Level, G3LogDebug_is_DEBUG) {
 LOG(DEBUG) << "DEBUG equals G3LOG_DEBUG";
 LOG(G3LOG_DEBUG) << "G3LOG_DEBUG equals DEBUG";
}
#endif


#ifdef G3_DYNAMIC_LOGGING
namespace {
   using LevelsContainer = std::map<int, g3::LoggingLevel>;
   const LevelsContainer g_test_log_level_defaults = {
	  {G3LOG_DEBUG.value, {G3LOG_DEBUG}},
      {INFO.value, {INFO}},
      {WARNING.value, {WARNING}},
      {FATAL.value, {FATAL}}
   };

   const LevelsContainer g_test_all_disabled = {
	  {G3LOG_DEBUG.value, {G3LOG_DEBUG,false}},
      {INFO.value, {INFO, false}},
      {WARNING.value, {WARNING, false}},
      {FATAL.value, {FATAL, false}}
   };


   bool mapCompare (LevelsContainer const& lhs, LevelsContainer const& rhs) {
      auto pred = [] (auto a, auto b) {
         return (a.first == b.first) &&
                (a.second == b.second);
      };

      return lhs.size() == rhs.size()
             && std::equal(lhs.begin(), lhs.end(), rhs.begin(), pred);
   }
} // anonymous
TEST(Level, Default) {
   g3::only_change_at_initialization::reset();
   auto defaults = g3::log_levels::getAll();
   EXPECT_EQ(defaults.size(), g_test_log_level_defaults.size());
   EXPECT_TRUE(mapCompare(defaults, g_test_log_level_defaults));
}

TEST(Level, DefaultChanged_only_change_at_initialization) {
   g3::only_change_at_initialization::reset();
   std::shared_ptr<void> RaiiLeveReset(nullptr, [&](void*) {
      g3::only_change_at_initialization::reset();
   });

   g3::only_change_at_initialization::addLogLevel(INFO, false);
   auto defaults = g3::log_levels::getAll();
   EXPECT_EQ(defaults.size(), g_test_log_level_defaults.size());
   EXPECT_FALSE(mapCompare(defaults, g_test_log_level_defaults));

   const LevelsContainer defaultsWithInfoChangged = {
      {G3LOG_DEBUG.value, {G3LOG_DEBUG, true}},
      {INFO.value, {INFO, false}},
      {WARNING.value, {WARNING, true}},
      {FATAL.value, {FATAL, true}}
   };
   EXPECT_TRUE(mapCompare(defaults, defaultsWithInfoChangged));
}

TEST(Level, DefaultChanged_log_levels) {
   g3::only_change_at_initialization::reset();
   std::shared_ptr<void> RaiiLeveReset(nullptr, [&](void*) {
      g3::only_change_at_initialization::reset();
   });

   g3::log_levels::disable(INFO);
   auto defaults = g3::log_levels::getAll();
   EXPECT_EQ(defaults.size(), g_test_log_level_defaults.size());
   EXPECT_FALSE(mapCompare(defaults, g_test_log_level_defaults));

   const LevelsContainer defaultsWithInfoChangged = {
      {G3LOG_DEBUG.value, {G3LOG_DEBUG, true}},
      {INFO.value, {INFO, false}},
      {WARNING.value, {WARNING, true}},
      {FATAL.value, {FATAL, true}}
   };
   EXPECT_TRUE(mapCompare(defaults, defaultsWithInfoChangged));
}

TEST(Level, Reset) {
   std::shared_ptr<void> RaiiLeveReset(nullptr, [&](void*) {
      g3::only_change_at_initialization::reset();
   });

   g3::log_levels::disableAll();
   auto all_levels = g3::log_levels::getAll();
   EXPECT_TRUE(mapCompare(all_levels, g_test_all_disabled));

   g3::only_change_at_initialization::reset();
   all_levels = g3::log_levels::getAll();
   EXPECT_TRUE(mapCompare(all_levels, g_test_log_level_defaults));



}



TEST(Level, AllDisabled) {
   g3::only_change_at_initialization::reset();
   std::shared_ptr<void> RaiiLeveReset(nullptr, [&](void*) {
      g3::only_change_at_initialization::reset();
   });


   auto all_levels = g3::log_levels::getAll();
   EXPECT_EQ(all_levels.size(), g_test_all_disabled.size());
   EXPECT_FALSE(mapCompare(all_levels, g_test_all_disabled));

   g3::log_levels::disableAll();
   all_levels = g3::log_levels::getAll();
   EXPECT_TRUE(mapCompare(all_levels, g_test_all_disabled));
}


TEST(Level, setHighestLogLevel_high_end) {
   g3::only_change_at_initialization::reset();
   std::shared_ptr<void> RaiiLeveReset(nullptr, [&](void*) {
      g3::only_change_at_initialization::reset();
   });


   g3::log_levels::enableAll();
   g3::log_levels::disable(FATAL);
   g3::log_levels::setHighest(FATAL);


   LevelsContainer expected = {
      {G3LOG_DEBUG.value, {G3LOG_DEBUG, false}},
      {INFO.value, {INFO, false}},
      {WARNING.value, {WARNING, false}},
      {FATAL.value, {FATAL, true}}
   };

   auto all_levels = g3::log_levels::getAll();
   EXPECT_TRUE(mapCompare(all_levels, expected)) << g3::log_levels::to_string();
}


TEST(Level, setHighestLogLevel_low_end) {
   g3::only_change_at_initialization::reset();
   std::shared_ptr<void> RaiiLeveReset(nullptr, [&](void*) {
      g3::only_change_at_initialization::reset();
   });


   g3::log_levels::disableAll();
   g3::log_levels::setHighest(G3LOG_DEBUG);


   LevelsContainer expected = {
      {G3LOG_DEBUG.value,{G3LOG_DEBUG, true}},
      {INFO.value, {INFO, true}},
      {WARNING.value, {WARNING, true}},
      {FATAL.value, {FATAL, true}}
   };

   auto all_levels = g3::log_levels::getAll();
   EXPECT_TRUE(mapCompare(all_levels, expected)) << g3::log_levels::to_string();
}


TEST(Level, setHighestLogLevel_middle) {
   g3::only_change_at_initialization::reset();
   std::shared_ptr<void> RaiiLeveReset(nullptr, [&](void*) {
      g3::only_change_at_initialization::reset();
   });


   g3::log_levels::enableAll();
   g3::log_levels::setHighest(WARNING);


   LevelsContainer expected = {
      {G3LOG_DEBUG.value, {G3LOG_DEBUG, false}},
      {INFO.value, {INFO, false}},
      {WARNING.value, {WARNING, true}},
      {FATAL.value, {FATAL, true}}
   };

   auto all_levels = g3::log_levels::getAll();
   EXPECT_TRUE(mapCompare(all_levels, expected));
}




TEST(Level, setHighestLogLevel_StepWiseDisableAll) {
   g3::only_change_at_initialization::reset();
   std::shared_ptr<void> RaiiLeveReset(nullptr, [&](void*) {
      g3::only_change_at_initialization::reset();
   });

   LevelsContainer changing_levels = {
      {G3LOG_DEBUG.value, {G3LOG_DEBUG, true}},
      {INFO.value, {INFO, true}},
      {WARNING.value, {WARNING, true}},
      {FATAL.value, {FATAL, true}}
   };

   auto all_levels = g3::log_levels::getAll();
   EXPECT_TRUE(mapCompare(all_levels, g_test_log_level_defaults));

   size_t counter = 0;
   for (auto& lvl : changing_levels) {
      g3::log_levels::setHighest(lvl.second.level);
      all_levels = g3::log_levels::getAll();

      ASSERT_TRUE(mapCompare(all_levels, changing_levels)) <<
            "counter: " << counter << "\nsystem:\n" <<
            g3::log_levels::to_string(all_levels) <<
            "\nexpected:\n" <<
            g3::log_levels::to_string(changing_levels);

      ++counter;
      if (counter != changing_levels.size()) {
         // for next round this level will be disabled
         lvl.second.status = false;
      }
   }


   // in the end all except the last should be disabled
   auto mostly_disabled = g_test_all_disabled;
   mostly_disabled[FATAL.value].status = true;
   EXPECT_TRUE(mapCompare(changing_levels, mostly_disabled));

   all_levels = g3::log_levels::getAll();
   EXPECT_TRUE(mapCompare(all_levels, mostly_disabled)) <<
         "\nsystem:\n" <<
         g3::log_levels::to_string(all_levels) <<
         "\nexpected:\n" <<
         g3::log_levels::to_string(mostly_disabled);
}

TEST(Level, Print) {
   g3::only_change_at_initialization::reset();
   std::string expected = std::string{"name: DEBUG level: 100 status: 1\n"}
                          + "name: INFO level: 300 status: 1\n"
                          + "name: WARNING level: 500 status: 1\n"
                          + "name: FATAL level: 1000 status: 1\n";
   EXPECT_EQ(g3::log_levels::to_string(), expected);
}

TEST(Level, AddOneEnabled_option1) {
   std::shared_ptr<void> RaiiLeveReset(nullptr, [&](void*) {
      g3::only_change_at_initialization::reset();
   });


   LEVELS MYINFO {WARNING.value + 1, "MyInfoLevel"};
   g3::only_change_at_initialization::addLogLevel(MYINFO, true);

   auto modified = g_test_log_level_defaults;
   modified[MYINFO.value] = MYINFO;

   auto all_levels = g3::log_levels::getAll();
   EXPECT_TRUE(mapCompare(modified, all_levels)) << "\nsystem:\n" <<
         g3::log_levels::to_string(all_levels) <<
         "\nexpected:\n" <<
         g3::log_levels::to_string(modified);

}

TEST(Level, AddOneEnabled_option2) {
   std::shared_ptr<void> RaiiLeveReset(nullptr, [&](void*) {
      g3::only_change_at_initialization::reset();
   });


   LEVELS MYINFO {WARNING.value + 1, "MyInfoLevel"};
   g3::only_change_at_initialization::addLogLevel(MYINFO);

   auto modified = g_test_log_level_defaults;
   modified[MYINFO.value] = MYINFO;

   auto all_levels = g3::log_levels::getAll();
   EXPECT_TRUE(mapCompare(modified, all_levels)) << "\nsystem:\n" <<
         g3::log_levels::to_string(all_levels) <<
         "\nexpected:\n" <<
         g3::log_levels::to_string(modified);

}




TEST(Level, Addlevel_using_addLevel) {
   std::shared_ptr<void> RaiiLeveReset(nullptr, [&](void*) {
      g3::only_change_at_initialization::reset();
   });

   LEVELS MYINFO {WARNING.value + 1, "MyInfoLevel"};
   auto status = g3::log_levels::getStatus(MYINFO);
   EXPECT_EQ(status, g3::log_levels::status::Absent);

   g3::only_change_at_initialization::addLogLevel(MYINFO);
   status = g3::log_levels::getStatus(MYINFO);
   EXPECT_EQ(status, g3::log_levels::status::Enabled);
}

TEST(Level, Addlevel_using_addLogLevel_disabled) {
   std::shared_ptr<void> RaiiLeveReset(nullptr, [&](void*) {
      g3::only_change_at_initialization::reset();
   });

   LEVELS MYINFO {WARNING.value + 1, "MyInfoLevel"};
   auto status = g3::log_levels::getStatus(MYINFO);
   EXPECT_EQ(status, g3::log_levels::status::Absent);

   g3::only_change_at_initialization::addLogLevel(MYINFO, false);
   status = g3::log_levels::getStatus(MYINFO);
   EXPECT_EQ(status, g3::log_levels::status::Disabled);
}

TEST(Level, Addlevel__disabled) {
   std::shared_ptr<void> RaiiLeveReset(nullptr, [&](void*) {
      g3::only_change_at_initialization::reset();
   });

   LEVELS MYINFO {WARNING.value + 1, "MyInfoLevel"};
   auto status = g3::log_levels::getStatus(MYINFO);
   EXPECT_EQ(status, g3::log_levels::status::Absent);

   g3::log_levels::enable(MYINFO);
   status = g3::log_levels::getStatus(MYINFO);
   EXPECT_EQ(status, g3::log_levels::status::Absent);

   g3::log_levels::set(MYINFO, true);
   status = g3::log_levels::getStatus(MYINFO);
   EXPECT_EQ(status, g3::log_levels::status::Absent);

   g3::only_change_at_initialization::addLogLevel(MYINFO, false);
   status = g3::log_levels::getStatus(MYINFO);
   EXPECT_EQ(status, g3::log_levels::status::Disabled);
}

TEST(Level, Addlevel__enabled) {
   std::shared_ptr<void> RaiiLeveReset(nullptr, [&](void*) {
      g3::only_change_at_initialization::reset();
   });

   LEVELS MYINFO {WARNING.value + 1, "MyInfoLevel"};
   auto status = g3::log_levels::getStatus(MYINFO);
   EXPECT_EQ(status, g3::log_levels::status::Absent);


   g3::only_change_at_initialization::addLogLevel(MYINFO);
   status = g3::log_levels::getStatus(MYINFO);
   EXPECT_EQ(status, g3::log_levels::status::Enabled);
}

#endif // G3_DYNAMIC_LOGGING

