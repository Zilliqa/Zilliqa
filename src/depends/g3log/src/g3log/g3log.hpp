/** ==========================================================================
 * 2011 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
 * with no warranties. This code is yours to share, use and modify with no
 * strings attached and no restrictions or obligations.
 *
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
 * ============================================================================
 *
 * Filename:g3log.hpp  Framework for Logging and Design By Contract
 * Created: 2011 by Kjell Hedström
 *
 * PUBLIC DOMAIN and Not copywrited since it was built on public-domain software and influenced
 * at least in "spirit" from the following sources
 * 1. kjellkod.cc ;)
 * 2. Dr.Dobbs, Petru Marginean:  http://drdobbs.com/article/printableArticle.jhtml?articleId=201804215&dept_url=/caddpp/
 * 3. Dr.Dobbs, Michael Schulze: http://drdobbs.com/article/printableArticle.jhtml?articleId=225700666&dept_url=/cpp/
 * 4. Google 'glog': http://google-glog.googlecode.com/svn/trunk/doc/glog.html
 * 5. Various Q&A at StackOverflow
 * ********************************************* */


#pragma once

#include "g3log/loglevels.hpp"
#include "g3log/logcapture.hpp"
#include "g3log/logmessage.hpp"
#include "g3log/generated_definitions.hpp"

#include <string>
#include <functional>


#if !(defined(__PRETTY_FUNCTION__))
#define __PRETTY_FUNCTION__   __FUNCTION__
#endif

// thread_local doesn't exist before VS2013
// it exists on VS2015
#if !(defined(thread_local)) && defined(_MSC_VER) && _MSC_VER < 1900
#define thread_local __declspec(thread)
#endif


/** namespace for LOG() and CHECK() frameworks
 * History lesson:   Why the names 'g3' and 'g3log'?:
 * The framework was made in my own free time as PUBLIC DOMAIN but the
 * first commercial project to use it used 'g3' as an internal denominator for
 * the current project. g3 as in 'generation 2'. I decided to keep the g3 and g3log names
 * to give credit to the people in that project (you know who you are :) and I guess also
 * for 'sentimental' reasons. That a big influence was Google's glog is just a happy
 * coincidence or subconscious choice. Either way g3log became the name for this logger.
 *
 * --- Thanks for a great 2011 and good luck with 'g3' --- KjellKod
 */
namespace g3 {
   class LogWorker;
   struct LogMessage;
   struct FatalMessage;

   /** Should be called at very first startup of the software with \ref g3LogWorker
    *  pointer. Ownership of the \ref g3LogWorker is the responsibility of the caller */
   void initializeLogging(LogWorker *logger);


   /** setFatalPreLoggingHook() provides an optional extra step before the fatalExitHandler is called
    *
    * Set a function-hook before a fatal message will be sent to the logger
    * i.e. this is a great place to put a break point, either in your debugger
    * or programatically to catch LOG(FATAL), CHECK(...) or an OS fatal event (exception or signal)
    * This will be reset to default (does nothing) at initializeLogging(...);
    *
    * Example usage:
    * Windows: g3::setFatalPreLoggingHook([]{__debugbreak();}); // remember #include <intrin.h>
    *         WARNING: '__debugbreak()' when not running in Debug in your Visual Studio IDE will likely
    *                   trigger a recursive crash if used here. It should only be used when debugging
    *                   in your Visual Studio IDE. Recursive crashes are handled but are unnecessary.
    *
    * Linux:   g3::setFatalPreLoggingHook([]{ raise(SIGTRAP); });
    */
   void setFatalPreLoggingHook(std::function<void(void)>  pre_fatal_hook);

   /** If the @ref setFatalPreLoggingHook is not enough and full fatal exit handling is needed then
    * use "setFatalExithandler".  Please see g3log.cpp and crashhandler_windows.cpp or crashhandler_unix for
    * example of restoring signal and exception handlers, flushing the log and shutting down.
    */
   void setFatalExitHandler(std::function<void(FatalMessagePtr)> fatal_call);


#ifdef G3_DYNAMIC_MAX_MESSAGE_SIZE
  // only_change_at_initialization namespace is for changes to be done only during initialization. More specifically
  // items here would be called prior to calling other parts of g3log
  namespace only_change_at_initialization {
    // Sets the MaxMessageSize to be used when capturing log messages. Currently this value is set to 2KB. Messages
    // Longer than this are bound to 2KB with the string "[...truncated...]" at the end. This function allows
    // this limit to be changed.
    void setMaxMessageSize(size_t max_size);
  }
#endif /* G3_DYNAMIC_MAX_MESSAGE_SIZE */

   // internal namespace is for completely internal or semi-hidden from the g3 namespace due to that it is unlikely
   // that you will use these
   namespace internal {
      /// @returns true if logger is initialized
      bool isLoggingInitialized();

      // Save the created LogMessage to any existing sinks
      void saveMessage(const char *message, const char *file, int line, const char *function, const LEVELS &level,
                       const char *boolean_expression, int fatal_signal, const char *stack_trace);

      // forwards the message to all sinks
      void pushMessageToLogger(LogMessagePtr log_entry);


      // forwards a FATAL message to all sinks,. after which the g3logworker
      // will trigger crashhandler / g3::internal::exitWithDefaultSignalHandler
      //
      // By default the "fatalCall" will forward a FatalMessageptr to this function
      // this behavior can be changed if you set a different fatal handler through
      // "setFatalExitHandler"
      void pushFatalMessageToLogger(FatalMessagePtr message);


      // Saves the created FatalMessage to any existing sinks and exits with
      // the originating fatal signal,. or SIGABRT if it originated from a broken contract.
      // By default forwards to: pushFatalMessageToLogger, see "setFatalExitHandler" to override
      //
      // If you override it then you probably want to call "pushFatalMessageToLogger" after your
      // custom fatal handler is done. This will make sure that the fatal message the pushed
      // to sinks as well as shutting down the process
      void fatalCall(FatalMessagePtr message);

      // Shuts down logging. No object cleanup but further LOG(...) calls will be ignored.
      void shutDownLogging();

      // Shutdown logging, but ONLY if the active logger corresponds to the one currently initialized
      bool shutDownLoggingForActiveOnly(LogWorker *active);

   } // internal
} // g3

#define INTERNAL_LOG_MESSAGE(level) LogCapture(__FILE__, __LINE__, static_cast<const char*>(__PRETTY_FUNCTION__), level)

#define INTERNAL_CONTRACT_MESSAGE(boolean_expression)  \
   LogCapture(__FILE__, __LINE__, __PRETTY_FUNCTION__, g3::internal::CONTRACT, boolean_expression)


// LOG(level) is the API for the stream log
#define LOG(level) if(!g3::logLevel(level)){ } else INTERNAL_LOG_MESSAGE(level).stream()


// 'Conditional' stream log
#define LOG_IF(level, boolean_expression)  \
   if(true == (boolean_expression))  \
      if(g3::logLevel(level))  INTERNAL_LOG_MESSAGE(level).stream()

// 'Design By Contract' stream API. For Broken Contracts:
//         unit testing: it will throw std::runtime_error when a contract breaks
//         I.R.L : it will exit the application by using fatal signal SIGABRT
#define CHECK(boolean_expression)        \
   if (false == (boolean_expression))  INTERNAL_CONTRACT_MESSAGE(#boolean_expression).stream()


/** For details please see this
 * REFERENCE: http://www.cppreference.com/wiki/io/c/printf_format
 * \verbatim
 *
  There are different %-codes for different variable types, as well as options to
    limit the length of the variables and whatnot.
    Code Format
    %[flags][width][.precision][length]specifier
 SPECIFIERS
 ----------
 %c character
 %d signed integers
 %i signed integers
 %e scientific notation, with a lowercase “e”
 %E scientific notation, with a uppercase “E”
 %f floating point
 %g use %e or %f, whichever is shorter
 %G use %E or %f, whichever is shorter
 %o octal
 %s a string of characters
 %u unsigned integer
 %x unsigned hexadecimal, with lowercase letters
 %X unsigned hexadecimal, with uppercase letters
 %p a pointer
 %n the argument shall be a pointer to an integer into which is placed the number of characters written so far

For flags, width, precision etc please see the above references.
EXAMPLES:
{
   LOGF(INFO, "Characters: %c %c \n", 'a', 65);
   LOGF(INFO, "Decimals: %d %ld\n", 1977, 650000L);      // printing long
   LOGF(INFO, "Preceding with blanks: %10d \n", 1977);
   LOGF(INFO, "Preceding with zeros: %010d \n", 1977);
   LOGF(INFO, "Some different radixes: %d %x %o %#x %#o \n", 100, 100, 100, 100, 100);
   LOGF(INFO, "floats: %4.2f %+.0e %E \n", 3.1416, 3.1416, 3.1416);
   LOGF(INFO, "Width trick: %*d \n", 5, 10);
   LOGF(INFO, "%s \n", "A string");
   return 0;
}
And here is possible output
:      Characters: a A
:      Decimals: 1977 650000
:      Preceding with blanks:       1977
:      Preceding with zeros: 0000001977
:      Some different radixes: 100 64 144 0x64 0144
:      floats: 3.14 +3e+000 3.141600E+000
:      Width trick:    10
:      A string  \endverbatim */
#define LOGF(level, printf_like_message, ...)                 \
   if(!g3::logLevel(level)){ } else INTERNAL_LOG_MESSAGE(level).capturef(printf_like_message, ##__VA_ARGS__)

// Conditional log printf syntax
#define LOGF_IF(level,boolean_expression, printf_like_message, ...) \
   if(true == (boolean_expression))                                     \
      if(g3::logLevel(level))  INTERNAL_LOG_MESSAGE(level).capturef(printf_like_message, ##__VA_ARGS__)

// Design By Contract, printf-like API syntax with variadic input parameters.
// Throws std::runtime_eror if contract breaks
#define CHECKF(boolean_expression, printf_like_message, ...)    \
   if (false == (boolean_expression))  INTERNAL_CONTRACT_MESSAGE(#boolean_expression).capturef(printf_like_message, ##__VA_ARGS__)

// Backwards compatible. The same as CHECKF. 
// Design By Contract, printf-like API syntax with variadic input parameters.
// Throws std::runtime_eror if contract breaks
#define CHECK_F(boolean_expression, printf_like_message, ...)    \
   if (false == (boolean_expression))  INTERNAL_CONTRACT_MESSAGE(#boolean_expression).capturef(printf_like_message, ##__VA_ARGS__)


