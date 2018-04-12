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
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>

#if defined(G3LOG_PERFORMANCE)
const std::string title {
   "G3LOG"
};
#elif defined(GOOGLE_GLOG_PERFORMANCE)
const std::string title {
   "GOOGLE__GLOG"
};
#else
#error G3LOG_PERFORMANCE or GOOGLE_GLOG_PERFORMANCE was not defined
#endif


#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
const std::string g_path {
   "./"
};
#else
const std::string g_path {
   "/tmp/"
};
#endif



using namespace g3_test;


//
// OK: The code below isn't pretty but it works. Lots and lots of log entries
// to keep track of!
//
int main(int argc, char** argv)
{
   size_t number_of_threads {0};
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

   const std::string  g_prefix_log_name = title + "-performance-" + thread_count_oss.str() + "threads-WORST_LOG";
   const std::string  g_measurement_dump = g_path + g_prefix_log_name + "_RESULT.txt";
   const std::string  g_measurement_bucket_dump = g_path + g_prefix_log_name + "_RESULT_buckets.txt";
   const uint64_t us_to_ms {
      1000
   };
   const uint64_t us_to_s {
      1000000
   };


   std::ostringstream oss;
   oss << "\n\n" << title << " performance " << number_of_threads << " threads WORST (PEAK) times\n";
   oss << "Each thread running #: " << g_loop << " * " << g_iterations << " iterations of log entries" << std::endl;  // worst mean case is about 10us per log entry
   const uint64_t xtra_margin {
      2
   };
   oss << "*** It can take som time. Please wait: Approximate wait time on MY PC was:  " << number_of_threads * (uint64_t)(g_iterations * 10 * xtra_margin / us_to_s) << " seconds" << std::endl;
   writeTextToFile(g_measurement_dump, oss.str(), kAppend);
   oss.str(""); // clear the stream

#if defined(G3LOG_PERFORMANCE)
   auto worker = g3::LogWorker::createLogWorker();
   auto handle= worker->addDefaultLogger(g_prefix_log_name, g_path);
   g3::initializeLogging(worker.get());

#elif defined(GOOGLE_GLOG_PERFORMANCE)
   google::InitGoogleLogging(argv[0]);
#endif

   std::thread* threads = new std::thread[number_of_threads];
   std::vector<uint64_t>* threads_result = new std::vector<uint64_t>[number_of_threads];

   // kiss: just loop, create threads, store them then join
   // could probably do this more elegant with lambdas
   for (uint64_t idx = 0; idx < number_of_threads; ++idx)
   {
      threads_result[idx].reserve(g_iterations);
   }

   auto start_time = std::chrono::high_resolution_clock::now();
   for (uint64_t idx = 0; idx < number_of_threads; ++idx)
   {
      std::ostringstream count;
      count << idx + 1;
      std::string thread_name =  title + "_T" + count.str();
      std::cout << "Creating thread: " << thread_name << std::endl;
      threads[idx] = std::thread(measurePeakDuringLogWrites, thread_name, std::ref(threads_result[idx]));
   }
   // wait for thread finishing
   for (uint64_t idx = 0; idx < number_of_threads; ++idx)
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

   oss << "\n" << number_of_threads << "*" << g_iterations << " log entries took: [" << total_time_us / us_to_s << " s] to write to disk" << std::endl;
   oss << "[Application(" << number_of_threads << "_threads+overhead time for measurement):\t" << application_time_us / us_to_ms << " ms]" << std::endl;
   oss << "[Background thread to finish:\t\t\t\t" << total_time_us / us_to_ms << " ms]" << std::endl;
   oss << "\nAverage time per log entry:" << std::endl;
   oss << "[Application: " << application_time_us / (number_of_threads * g_iterations) << " us]" << std::endl;

   for (uint64_t idx = 0; idx < number_of_threads; ++idx)
   {
      std::vector<uint64_t> &t_result = threads_result[idx];
      uint64_t worstUs = (*std::max_element(t_result.begin(), t_result.end()));
      oss << "[Application t" << idx + 1 << " worst took: " <<  worstUs / uint64_t(1000) << " ms  (" << worstUs << " us)] " << std::endl;
   }
   writeTextToFile(g_measurement_dump, oss.str(), kAppend);
   std::cout << "Result can be found at:" << g_measurement_dump << std::endl;

   // now split the result in buckets of 10ms each so that it's obvious how the peaks go
   std::vector<uint64_t> all_measurements;
   all_measurements.reserve(g_iterations * number_of_threads);
   for (uint64_t idx = 0; idx < number_of_threads; ++idx)
   {
      std::vector<uint64_t> &t_result = threads_result[idx];
      all_measurements.insert(all_measurements.end(), t_result.begin(), t_result.end());
   }
   delete [] threads_result; // finally get rid of them

   std::sort (all_measurements.begin(), all_measurements.end());
   std::map<uint64_t, uint64_t> value_amounts;
   std::map<uint64_t, uint64_t> value_amounts_for_0ms_bucket;

   for (auto iter = all_measurements.begin(); iter != all_measurements.end(); ++iter)
   {
      uint64_t value = (*iter) / us_to_ms; // convert to ms
      ++value_amounts[value]; // asuming uint64_t is default 0 when initialized

      if (0 == value) {
         ++value_amounts_for_0ms_bucket[*iter];
      }
   }

   oss.str("");
   oss << "Number of values rounded to milliseconds and put to [millisecond bucket] were dumped to file: " << g_measurement_bucket_dump << std::endl;
   if (1 == value_amounts.size()) {
      oss << "Format:  bucket of us inside bucket0 for ms\nFormat:bucket_of_ms, number_of_values_in_bucket\n\n" << std::endl;
      oss << "\n";
   }
   else {
      oss << "Format:bucket_of_ms, number_of_values_in_bucket\n\n" << std::endl;
   }
   std::cout << oss.str() << std::endl;

   //
   // If all values are for the 0ms bucket then instead show us buckets
   //
   if (1 == value_amounts.size()) {
      oss << "\n\n***** Microsecond bucket measurement for all measurements that went inside the '0 millisecond bucket' ****\n";
      for (auto us_bucket : value_amounts_for_0ms_bucket) {
         oss << us_bucket.first << "\t" << us_bucket.second << std::endl;
      }
      oss << "\n\n***** Millisecond bucket measurement ****\n";
   }

   for (auto ms_bucket : value_amounts)
   {
      oss << ms_bucket.first << "\t, " << ms_bucket.second << std::endl;
   }
   writeTextToFile(g_measurement_bucket_dump, oss.str(), kAppend,  false);


   return 0;
}
