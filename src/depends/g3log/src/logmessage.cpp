/** ==========================================================================
* 2012 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
* with no warranties. This code is yours to share, use and modify with no
* strings attached and no restrictions or obligations.
*
* For more information see g3log/LICENSE or refer refer to http://unlicense.org
* ============================================================================*/

#include "g3log/logmessage.hpp"
#include "g3log/crashhandler.hpp"
#include "g3log/time.hpp"
#include <mutex>




namespace g3 {

   std::string LogMessage::splitFileName(const std::string& str) {
      size_t found;
      found = str.find_last_of("(/\\");
      return str.substr(found + 1);
   }


   // helper for fatal signal
   std::string  LogMessage::fatalSignalToString(const LogMessage& msg) {
      std::string out; // clear any previous text and formatting
      out.append(msg.timestamp()
                 + "\n\n***** FATAL SIGNAL RECEIVED ******* \n"
                 + msg.message() + '\n');
      return out;
   }


   // helper for fatal exception (windows only)
   std::string  LogMessage::fatalExceptionToString(const LogMessage& msg) {
      std::string out; // clear any previous text and formatting
      out.append(msg.timestamp()
                 + "\n\n***** FATAL EXCEPTION RECEIVED ******* \n"
                 + msg.message() + '\n');
      return out;
   }


   // helper for fatal LOG
   std::string LogMessage::fatalLogToString(const LogMessage& msg) {
      auto out = msg._logDetailsToStringFunc(msg);
      static const std::string fatalExitReason = {"EXIT trigger caused by LOG(FATAL) entry: "};
      out.append("\n\t*******\t " + fatalExitReason + "\n\t" + '"' + msg.message() + '"');
      return out;
   }

   // helper for fatal CHECK
   std::string LogMessage::fatalCheckToString(const LogMessage& msg) {
      auto out = msg._logDetailsToStringFunc(msg);
      static const std::string contractExitReason = {"EXIT trigger caused by broken Contract:"};
      out.append("\n\t*******\t " + contractExitReason + " CHECK(" + msg.expression() + ")\n\t"
                 + '"' + msg. message() + '"');
      return out;
   }

   // helper for setting the normal log details in an entry
   std::string LogMessage::DefaultLogDetailsToString(const LogMessage& msg) {
      std::string out;
      out.append(msg.timestamp() + "\t"
                 + msg.level() 
                 + " [" 
                 + msg.file() 
                 + "->" 
                 + msg.function() 
                 + ":" + msg.line() + "]\t");
      return out;
   }


   std::string LogMessage::FullLogDetailsToString(const LogMessage& msg) {
      std::string out;
      out.append(msg.timestamp() + "\t"
                 + msg.level() 
                 + " [" 
                 + msg.threadID() 
                 + " "
                 + msg.file() 
                 + "->"+ msg.function() 
                 + ":" + msg.line() + "]\t");
      return out;
   }


   // helper for normal
   std::string LogMessage::normalToString(const LogMessage& msg) {
      auto out = msg._logDetailsToStringFunc(msg);
      out.append(msg.message() + '\n');
      return out;
   }



 // end static functions section

   void LogMessage::overrideLogDetailsFunc(LogDetailsFunc func) const{
      _logDetailsToStringFunc = func;
   }



   // Format the log message according to it's type
   std::string LogMessage::toString(LogDetailsFunc formattingFunc) const {
      overrideLogDetailsFunc(formattingFunc);

      if (false == wasFatal()) {
         return LogMessage::normalToString(*this);
      }

      const auto level_value = _level.value;
      if (internal::FATAL_SIGNAL.value == _level.value) {
         return LogMessage::fatalSignalToString(*this);
      }

      if (internal::FATAL_EXCEPTION.value == _level.value) {
         return LogMessage::fatalExceptionToString(*this);
      }

      if (FATAL.value == _level.value) {
         return LogMessage::fatalLogToString(*this);
      }

      if (internal::CONTRACT.value == level_value) {
         return LogMessage::fatalCheckToString(*this);
      }

      // What? Did we hit a custom made level?
      auto out = _logDetailsToStringFunc(*this);
      static const std::string errorUnknown = {"UNKNOWN or Custom made Log Message Type"};
      out.append("\t*******" + errorUnknown + "\n\t" + message() + '\n');
      return out;
   }



   std::string LogMessage::timestamp(const std::string& time_look) const {
      return g3::localtime_formatted(to_system_time(_timestamp), time_look);
   }


// By copy, not by reference. See this explanation for details:
// http://stackoverflow.com/questions/3279543/what-is-the-copy-and-swap-idiom
   LogMessage& LogMessage::operator=(LogMessage other) {
      swap(*this, other);
      return *this;
   }


   LogMessage::LogMessage(std::string file, const int line,
                          std::string function, const LEVELS level)
      : _logDetailsToStringFunc(LogMessage::DefaultLogDetailsToString)
      , _timestamp(std::chrono::high_resolution_clock::now())
      , _call_thread_id(std::this_thread::get_id())
      , _file(LogMessage::splitFileName(file))
#if defined(G3_LOG_FULL_FILENAME)
      , _file(file)
#endif
      , _file_path(file)
      , _line(line)
      , _function(function)
      , _level(level) {
   }


   LogMessage::LogMessage(const std::string& fatalOsSignalCrashMessage)
      : LogMessage( {""}, 0, {""}, internal::FATAL_SIGNAL) {
      _message.append(fatalOsSignalCrashMessage);
   }

   LogMessage::LogMessage(const LogMessage& other)
      : _logDetailsToStringFunc(other._logDetailsToStringFunc)
      , _timestamp(other._timestamp)
      , _call_thread_id(other._call_thread_id)
      , _file(other._file)
      , _file_path(other._file_path)
      , _line(other._line)
      , _function(other._function)
      , _level(other._level)
      , _expression(other._expression)
      , _message(other._message) {
   }

   LogMessage::LogMessage(LogMessage&& other)
      : _logDetailsToStringFunc(other._logDetailsToStringFunc)
      , _timestamp(other._timestamp)
      , _call_thread_id(other._call_thread_id)
      , _file(std::move(other._file))
      , _file_path(std::move(other._file_path))
      , _line(other._line)
      , _function(std::move(other._function))
      , _level(other._level)
      , _expression(std::move(other._expression))
      , _message(std::move(other._message)) {
   }



   std::string LogMessage::threadID() const {
      std::ostringstream oss;
      oss << _call_thread_id;
      return oss.str();
   }



   FatalMessage::FatalMessage(const LogMessage& details, g3::SignalType signal_id)
      : LogMessage(details), _signal_id(signal_id) { }



   FatalMessage::FatalMessage(const FatalMessage& other)
      : LogMessage(other), _signal_id(other._signal_id) {}


   LogMessage  FatalMessage::copyToLogMessage() const {
      return LogMessage(*this);
   }

   std::string FatalMessage::reason() const {
      return internal::exitReasonName(_level, _signal_id);
   }


} // g3
