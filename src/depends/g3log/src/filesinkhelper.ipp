/** ==========================================================================
 * 2013 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
 * with no warranties. This code is yours to share, use and modify with no
 * strings attached and no restrictions or obligations.
 *
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
 * ============================================================================*/

#pragma once


#include <memory>
#include <string>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>


namespace g3 {
   namespace internal {
      static const std::string file_name_time_formatted = "%Y%m%d-%H%M%S";

      // check for filename validity -  filename should not be part of PATH
      bool isValidFilename(const std::string &prefix_filename) {
         std::string illegal_characters("/,|<>:#$%{}[]\'\"^!?+* ");
         size_t pos = prefix_filename.find_first_of(illegal_characters, 0);
         if (pos != std::string::npos) {
            std::cerr << "Illegal character [" << prefix_filename.at(pos) << "] in logname prefix: " << "[" << prefix_filename << "]" << std::endl;
            return false;
         } else if (prefix_filename.empty()) {
            std::cerr << "Empty filename prefix is not allowed" << std::endl;
            return false;
         }

         return true;
      }

      std::string prefixSanityFix(std::string prefix) {
         prefix.erase(std::remove_if(prefix.begin(), prefix.end(), ::isspace), prefix.end());
         prefix.erase(std::remove(prefix.begin(), prefix.end(), '/'), prefix.end());
         prefix.erase(std::remove(prefix.begin(), prefix.end(), '\\'), prefix.end());
         prefix.erase(std::remove(prefix.begin(), prefix.end(), '.'), prefix.end());
         prefix.erase(std::remove(prefix.begin(), prefix.end(), ':'), prefix.end());
         if (!isValidFilename(prefix)) {
            return
            {
            };
         }
         return prefix;
      }

      std::string pathSanityFix(std::string path, std::string file_name) {
         // Unify the delimeters,. maybe sketchy solution but it seems to work
         // on at least win7 + ubuntu. All bets are off for older windows
         std::replace(path.begin(), path.end(), '\\', '/');

         // clean up in case of multiples
         auto contains_end = [&](std::string & in) -> bool {
            size_t size = in.size();
            if (!size) return false;
            char end = in[size - 1];
            return (end == '/' || end == ' ');
         };

         while (contains_end(path)) {
            path.erase(path.size() - 1);
         }

         if (!path.empty()) {
            path.insert(path.end(), '/');
         }

         path.insert(path.size(), file_name);
         return path;
      }

      std::string header(const std::string& headerFormat) {
         std::ostringstream ss_entry;
         //  Day Month Date Time Year: is written as "%a %b %d %H:%M:%S %Y" and formatted output as : Wed Sep 19 08:28:16 2012
         auto now = std::chrono::system_clock::now();
         ss_entry << "\t\tg3log created log at: " << g3::localtime_formatted(now, "%a %b %d %H:%M:%S %Y") << "\n";
         ss_entry << headerFormat;         
         return ss_entry.str();
      }

      std::string createLogFileName(const std::string &verified_prefix, const std::string &logger_id) {
         std::stringstream oss_name;
         oss_name << verified_prefix << ".";
         if( logger_id != "" ) {
            oss_name << logger_id << ".";
         }
         auto now = std::chrono::system_clock::now();
         oss_name << g3::localtime_formatted(now, file_name_time_formatted);
         oss_name << ".log";
         return oss_name.str();
      }

      bool openLogFile(const std::string &complete_file_with_path, std::ofstream &outstream) {
         std::ios_base::openmode mode = std::ios_base::out; // for clarity: it's really overkill since it's an ofstream
         mode |= std::ios_base::trunc;
         outstream.open(complete_file_with_path, mode);
         if (!outstream.is_open()) {
            std::ostringstream ss_error;
            ss_error << "FILE ERROR:  could not open log file:[" << complete_file_with_path << "]";
            ss_error << "\n\t\t std::ios_base state = " << outstream.rdstate();
            std::cerr << ss_error.str().c_str() << std::endl;
            outstream.close();
            return false;
         }
         return true;
      }

      std::unique_ptr<std::ofstream> createLogFile(const std::string &file_with_full_path) {
         std::unique_ptr<std::ofstream> out(new std::ofstream);
         std::ofstream &stream(*(out.get()));
         bool success_with_open_file = openLogFile(file_with_full_path, stream);
         if (false == success_with_open_file) {
            out.release();
         }
         return out;
      }


   }
}
