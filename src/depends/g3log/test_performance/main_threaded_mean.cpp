/** ==========================================================================
* 2011 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
* with no warranties. This code is yours to share, use and modify with no
* strings attached and no restrictions or obligations.
 *
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
* ============================================================================*/

// through CMakeLists.txt   #define of GOOGLE_GLOG_PERFORMANCE and G3LOG_PERFORMANCE
#include "performance.h"
#include <thread>
#include <iostream>
#include <algorithm>

#if defined(G3LOG_PERFORMANCE)
const std::string title = "G3LOG";
#elif defined(GOOGLE_GLOG_PERFORMANCE)
const std::string title = "GOOGLE__GLOG";
#else
#error G3LOG_PERFORMANCE or GOOGLE_GLOG_PERFORMANCE was not defined
#endif

#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
const std::string g_path = "./";
#else
const std::string g_path = "/tmp/";
#endif
using namespace g3_test;

int main(int argc, char **argv)
{
#ifdef G3_DYNAMIC_LOGGING
   std::cerr << "G3_DYNAMIC_LOGGING is enabled" << std::endl;
#else
   std::cerr << "G3_DYNAMIC_LOGGING is DISABLED" << std::endl;
#endif

   size_t number_of_threads = 0;
   if (argc == 2)
   {
      number_of_threads = atoi(argv[1]);
   }
   if (argc != 2 || number_of_threads == 0)
   {
      std::cerr << "USAGE is: " << argv[0] << " number_threads" << std::endl;
      return 1;
   }

   std::ostringstream thread_count_oss;
   thread_count_oss << number_of_threads;
   const std::string  g_prefix_log_name = title + "-performance-" + thread_count_oss.str() + "threads-MEAN_LOG";
   const std::string  g_measurement_dump = g_path + g_prefix_log_name + "_RESULT.txt";

   std::ostringstream oss;
   const uint64_t us_to_s = 1000000;
   oss << "\n\n" << title << " performance " << number_of_threads << " threads MEAN times\n";
   oss << "Each thread running #: " << g_loop << " * " << g_iterations << " iterations of log entries" << std::endl;  // worst mean case is about 10us per log entry
   const uint64_t xtra_margin = 2;
   oss << "*** It can take som time. Please wait: Approximate wait time on MY PC was:  " << number_of_threads*  (uint64_t) (g_iterations * 10 * xtra_margin / us_to_s ) << " seconds" << std::endl;
   writeTextToFile(g_measurement_dump, oss.str(), kAppend);
   oss.str(""); // clear the stream

#if defined(G3LOG_PERFORMANCE)
   auto worker = g3::LogWorker::createLogWorker();
   auto handle= worker->addDefaultLogger(g_prefix_log_name, g_path);
   g3::initializeLogging(worker.get());


#elif defined(GOOGLE_GLOG_PERFORMANCE)
   google::InitGoogleLogging(argv[0]);
#endif
   auto start_time = std::chrono::high_resolution_clock::now();

   std::thread *threads = new std::thread[number_of_threads];
   // kiss: just loop, create threads, store them then join
   // could probably do this more elegant with lambdas
   for (size_t idx = 0; idx < number_of_threads; ++idx)
   {
      std::ostringstream count;
      count << idx + 1;
      std::string thread_name =  title + "_T" + count.str();
      std::cout << "Creating thread: " << thread_name << std::endl;
      threads[idx] = std::thread(doLogWrites, thread_name);
   }
   for (size_t idx = 0; idx < number_of_threads; ++idx)
   {
      threads[idx].join();
   }
   auto application_end_time = std::chrono::high_resolution_clock::now();
   delete [] threads;

#if defined(G3LOG_PERFORMANCE)
   worker.reset(); // will flush anything in the queue to file
#elif defined(GOOGLE_GLOG_PERFORMANCE)
   google::ShutdownGoogleLogging();
#endif

   auto worker_end_time = std::chrono::high_resolution_clock::now();
   uint64_t application_time_us = std::chrono::duration_cast<microsecond>(application_end_time - start_time).count();
   uint64_t total_time_us = std::chrono::duration_cast<microsecond>(worker_end_time - start_time).count();

   oss << "\n" << number_of_threads << "*" << g_iterations << " log entries took: [" << total_time_us / 1000000 << " s] to write to disk" << std::endl;
   oss << "[Application(" << number_of_threads << "):\t\t:" << application_time_us / 1000 << " ms]" << std::endl;
   oss << "[Background thread to finish\t:" << total_time_us / uint64_t(1000 ) << " ms]" << std::endl;
   oss << "\nAverage time per log entry:" << std::endl;
   oss << "[Application: " << application_time_us / (number_of_threads * g_iterations) << " us]" << std::endl;
   oss << "[Background+Application: " << total_time_us / (number_of_threads * g_iterations) << " us]" << std::endl;
   writeTextToFile(g_measurement_dump, oss.str(), kAppend);
   std::cout << "Result can be found at:" << g_measurement_dump << std::endl;

   return 0;
}
