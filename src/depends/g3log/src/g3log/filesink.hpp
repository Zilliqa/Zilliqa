/** ==========================================================================
 * 2013 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
 * with no warranties. This code is yours to share, use and modify with no
 * strings attached and no restrictions or obligations.
 *
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
 * ============================================================================*/
#pragma once

#include <string>
#include <memory>

#include "g3log/logmessage.hpp"
namespace g3 {

   class FileSink {
   public:
      FileSink(const std::string &log_prefix, const std::string &log_directory, const std::string &logger_id="g3log");
      virtual ~FileSink();

      void fileWrite(LogMessageMover message);
      std::string changeLogFile(const std::string &directory, const std::string &logger_id);
      std::string fileName();
      void overrideLogDetails(LogMessage::LogDetailsFunc func);
      void overrideLogHeader(const std::string& change);


   private:
      LogMessage::LogDetailsFunc _log_details_func;

      std::string _log_file_with_path;
      std::string _log_prefix_backup; // needed in case of future log file changes of directory
      std::unique_ptr<std::ofstream> _outptr;
      std::string _header;
      bool _firstEntry;

      void addLogFileHeader();
      std::ofstream &filestream() {
         return *(_outptr.get());
      }


      FileSink &operator=(const FileSink &) = delete;
      FileSink(const FileSink &other) = delete;

   };
} // g3

