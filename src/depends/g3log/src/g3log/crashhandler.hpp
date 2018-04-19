#pragma once

/** ==========================================================================
 * 2011 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
 * with no warranties. This code is yours to share, use and modify with no
 * strings attached and no restrictions or obligations.
 *
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
 * ============================================================================*/
#include <cstdio>
#include <string>
#include <map>
#include "g3log/loglevels.hpp"
#include "g3log/generated_definitions.hpp"

// kjell. Separera på crashhandler.hpp och crashhanlder_internal.hpp
// implementationsfilen kan vara den samma
namespace g3 {

   // PUBLIC API:
   /** Install signal handler that catches FATAL C-runtime or OS signals
     See the wikipedia site for details http://en.wikipedia.org/wiki/SIGFPE
     See the this site for example usage: http://www.tutorialspoint.com/cplusplus/cpp_signal_handling
     SIGABRT  ABORT (ANSI), abnormal termination
     SIGFPE   Floating point exception (ANSI)
     SIGILL   ILlegal instruction (ANSI)
     SIGSEGV  Segmentation violation i.e. illegal memory reference
     SIGTERM  TERMINATION (ANSI)  */
   void installCrashHandler();


#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
   typedef unsigned long SignalType;
   /// SIGFPE, SIGILL, and SIGSEGV handling must be installed per thread
   /// on Windows. This is automatically done if you do at least one LOG(...) call
   /// you can also use this function call, per thread so make sure these three
   /// fatal signals are covered in your thread (even if you don't do a LOG(...) call
   void installSignalHandlerForThread();
#else
   typedef int SignalType;
   /// Probably only needed for unit testing. Resets the signal handling back to default
   /// which might be needed in case it was previously overridden
   /// The default signals are: SIGABRT, SIGFPE, SIGILL, SIGSEGV, SIGTERM
   void restoreSignalHandlerToDefault();


   std::string signalToStr(int signal_number);

   // restore to whatever signal handler was used before signal handler installation 
   void restoreSignalHandler(int signal_number);


   /// Overrides the existing signal handling for custom signals
   /// For example: usage of zcmq relies on its own signal handler for SIGTERM
   ///     so users of g3log with zcmq should then use the @ref overrideSetupSignals
   ///     , likely with the original set of signals but with SIGTERM removed
   /// 
   /// call example:
   ///  g3::overrideSetupSignals({ {SIGABRT, "SIGABRT"}, {SIGFPE, "SIGFPE"},{SIGILL, "SIGILL"},
   //                          {SIGSEGV, "SIGSEGV"},});
   void overrideSetupSignals(const std::map<int, std::string> overrideSignals);
#endif


   namespace internal {
      /** return whether or any fatal handling is still ongoing
       *  this is used by g3log::fatalCallToLogger
       *  only in the case of Windows exceptions (not fatal signals)
       *  are we interested in changing this from false to true to
       *  help any other exceptions handler work with 'EXCEPTION_CONTINUE_SEARCH'*/
      bool shouldBlockForFatalHandling();

      /** \return signal_name Ref: signum.hpp and \ref installSignalHandler
      *  or for Windows exception name */
      std::string exitReasonName(const LEVELS& level, g3::SignalType signal_number);

      /** return calling thread's stackdump*/
      std::string stackdump(const char* dump = nullptr);

      /** Re-"throw" a fatal signal, previously caught. This will exit the application
       * This is an internal only function. Do not use it elsewhere. It is triggered
       * from g3log, g3LogWorker after flushing messages to file */
      void exitWithDefaultSignalHandler(const LEVELS& level, g3::SignalType signal_number);
   } // end g3::internal
} // g3
