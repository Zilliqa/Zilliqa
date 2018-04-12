/** ==========================================================================
 * 2011 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
 * with no warranties. This code is yours to share, use and modify with no
 * strings attached and no restrictions or obligations.
 *
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
 * ============================================================================*/

#if !(defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
#error "crashhandler_windows.cpp used but not on a windows system"
#endif

#include <windows.h>
#include <csignal>
#include <sstream>
#include <atomic>
#include <process.h> // getpid
#include "g3log/crashhandler.hpp"
#include "g3log/stacktrace_windows.hpp"
#include "g3log/logcapture.hpp"

#define getpid _getpid

namespace {
   std::atomic<bool> gBlockForFatal {true};
   LPTOP_LEVEL_EXCEPTION_FILTER g_previous_unexpected_exception_handler = nullptr;

#if !(defined(DISABLE_FATAL_SIGNALHANDLING))
   thread_local bool g_installed_thread_signal_handler = false;
#endif

#if !(defined(DISABLE_VECTORED_EXCEPTIONHANDLING))
   void *g_vector_exception_handler = nullptr;
#endif



   // Restore back to default fatal event handling
   void ReverseToOriginalFatalHandling() {
      SetUnhandledExceptionFilter (g_previous_unexpected_exception_handler);

#if !(defined(DISABLE_VECTORED_EXCEPTIONHANDLING))
      RemoveVectoredExceptionHandler (g_vector_exception_handler);
#endif

#if !(defined(DISABLE_FATAL_SIGNALHANDLING))
      if (SIG_ERR == signal(SIGABRT, SIG_DFL))
         perror("signal - SIGABRT");

      if (SIG_ERR == signal(SIGFPE, SIG_DFL))
         perror("signal - SIGABRT");

      if (SIG_ERR == signal(SIGSEGV, SIG_DFL))
         perror("signal - SIGABRT");

      if (SIG_ERR == signal(SIGILL, SIG_DFL))
         perror("signal - SIGABRT");

      if (SIG_ERR == signal(SIGTERM, SIG_DFL))
         perror("signal - SIGABRT");
#endif
   }



   // called for fatal signals SIGABRT, SIGFPE, SIGSEGV, SIGILL, SIGTERM
   void signalHandler(int signal_number) {
      using namespace g3::internal;
      std::string dump = stacktrace::stackdump();

      std::ostringstream fatal_stream;
      fatal_stream << "\n***** Received fatal signal " << g3::internal::exitReasonName(g3::internal::FATAL_SIGNAL, signal_number);
      fatal_stream << "(" << signal_number << ")\tPID: " << getpid() << std::endl;


      LogCapture trigger(FATAL_SIGNAL, static_cast<g3::SignalType>(signal_number), dump.c_str());
      trigger.stream() << fatal_stream.str();

      // Trigger debug break point, if we're in debug. This breakpoint CAN cause a slowdown when it happens.
      // Be patient. The "Debug" dialog should pop-up eventually if you doing it in Visual Studio.
      // For fatal signals only, not exceptions.
      // This is a way to tell the IDE (if in dev mode) that it can stop at this breakpoint
      // Note that at this time the fatal log event with stack trace is NOT yet flushed to the logger
      // This call will do nothing unless we're in DEBUG and "DEBUG_BREAK_AT_FATAL_SIGNAL" is enabled
      // ref: g3log/Options.cmake
#if (!defined(NDEBUG) && defined(DEBUG_BREAK_AT_FATAL_SIGNAL))
      __debugbreak();
#endif
   } // scope exit - message sent to LogWorker, wait to die...



   // Unhandled exception catching
   LONG WINAPI exceptionHandling(EXCEPTION_POINTERS *info, const std::string &handler) {
      std::string dump = stacktrace::stackdump(info);

      std::ostringstream fatal_stream;
      const g3::SignalType exception_code = info->ExceptionRecord->ExceptionCode;
      fatal_stream << "\n***** " << handler << ": Received fatal exception " << g3::internal::exitReasonName(g3::internal::FATAL_EXCEPTION, exception_code);
      fatal_stream << "\tPID: " << getpid() << std::endl;

      const auto fatal_id = static_cast<g3::SignalType>(exception_code);
      LogCapture trigger(g3::internal::FATAL_EXCEPTION, fatal_id, dump.c_str());
      trigger.stream() << fatal_stream.str();
      // FATAL Exception: It doesn't necessarily stop here. we pass on continue search
      // if no one else will catch that then it's goodbye anyhow.
      // The RISK here is if someone is catching this and returning "EXCEPTION_EXECUTE_HANDLER"
      // but does not shutdown then the software will be running with g3log shutdown.
      // .... However... this must be seen as a bug from standard handling of fatal exceptions
      // https://msdn.microsoft.com/en-us/library/6wxdsc38.aspx
      return EXCEPTION_CONTINUE_SEARCH;
   }


   // Unhandled exception catching
   LONG WINAPI unexpectedExceptionHandling(EXCEPTION_POINTERS *info) {
      ReverseToOriginalFatalHandling();
      return exceptionHandling(info, "Unexpected Exception Handler");
   }


   /// Setup through (Windows API) AddVectoredExceptionHandler
   /// Ref: http://blogs.msdn.com/b/zhanli/archive/2010/06/25/c-tips-addvectoredexceptionhandler-addvectoredcontinuehandler-and-setunhandledexceptionfilter.aspx
#if !(defined(DISABLE_VECTORED_EXCEPTIONHANDLING))
   LONG WINAPI vectorExceptionHandling(PEXCEPTION_POINTERS p) {
      const g3::SignalType exception_code = p->ExceptionRecord->ExceptionCode;
      if (false == stacktrace::isKnownException(exception_code)) {
         // The unknown exception is ignored. Since it is not a Windows
         // fatal exception generated by the OS we leave the
         // responsibility to deal with this by the client software.
         return EXCEPTION_CONTINUE_SEARCH;
      } else {
         ReverseToOriginalFatalHandling();
         return exceptionHandling(p, "Vectored Exception Handler");
      }
   }
#endif




} // end anonymous namespace


namespace g3 {
   namespace internal {
      // For windows exceptions this might ONCE be set to false, in case of a
      // windows exceptions and not a signal
      bool shouldBlockForFatalHandling() {
         return gBlockForFatal;
      }


      /// Generate stackdump. Or in case a stackdump was pre-generated and
      /// non-empty just use that one.   i.e. the latter case is only for
      /// Windows and test purposes
      std::string stackdump(const char *dump) {
         if (nullptr != dump && !std::string(dump).empty()) {
            return {dump};
         }

         return stacktrace::stackdump();
      }



      /// string representation of signal ID or Windows exception id
      std::string exitReasonName(const LEVELS &level, g3::SignalType fatal_id) {
         if (level == g3::internal::FATAL_EXCEPTION) {
            return stacktrace::exceptionIdToText(fatal_id);
         }

         switch (fatal_id) {
         case SIGABRT: return "SIGABRT"; break;
         case SIGFPE: return "SIGFPE"; break;
         case SIGSEGV: return "SIGSEGV"; break;
         case SIGILL: return "SIGILL"; break;
         case SIGTERM: return "SIGTERM"; break;
         default:
            std::ostringstream oss;
            oss << "UNKNOWN SIGNAL(" << fatal_id << ")";
            return oss.str();
         }
      }


      // Triggered by g3log::LogWorker after receiving a FATAL trigger
      // which is LOG(FATAL), CHECK(false) or a fatal signal our signalhandler caught.
      // --- If LOG(FATAL) or CHECK(false) the signal_number will be SIGABRT
      void exitWithDefaultSignalHandler(const LEVELS &level, g3::SignalType fatal_signal_id) {

         ReverseToOriginalFatalHandling();
         // For windows exceptions we want to continue the possibility of
         // exception handling now when the log and stacktrace are flushed
         // to sinks. We therefore avoid to kill the process here. Instead
         //  it will be the exceptionHandling functions above that
         // will let exception handling continue with: EXCEPTION_CONTINUE_SEARCH
         if (g3::internal::FATAL_EXCEPTION == level) {
            gBlockForFatal = false;
            return;
         }

         // for a signal however, we exit through that fatal signal
         const int signal_number = static_cast<int>(fatal_signal_id);
         raise(signal_number);
      }



      void installSignalHandler() {
         g3::installSignalHandlerForThread();
      }


   } // end g3::internal


   ///  SIGFPE, SIGILL, and SIGSEGV handling must be installed per thread
   /// on Windows. This is automatically done if you do at least one LOG(...) call
   /// you can also use this function call, per thread so make sure these three
   /// fatal signals are covered in your thread (even if you don't do a LOG(...) call
   void installSignalHandlerForThread() {
#if !(defined(DISABLE_FATAL_SIGNALHANDLING))
      if (!g_installed_thread_signal_handler) {
         g_installed_thread_signal_handler = true;
         if (SIG_ERR == signal(SIGTERM, signalHandler))
            perror("signal - SIGTERM");
         if (SIG_ERR == signal(SIGABRT, signalHandler))
            perror("signal - SIGABRT");
         if (SIG_ERR == signal(SIGFPE, signalHandler))
            perror("signal - SIGFPE");
         if (SIG_ERR == signal(SIGSEGV, signalHandler))
            perror("signal - SIGSEGV");
         if (SIG_ERR == signal(SIGILL, signalHandler))
            perror("signal - SIGILL");
      }
#endif
   }

   void installCrashHandler() {
      internal::installSignalHandler();
      g_previous_unexpected_exception_handler = SetUnhandledExceptionFilter(unexpectedExceptionHandling);

#if !(defined(DISABLE_VECTORED_EXCEPTIONHANDLING))
      // const size_t kFirstExceptionHandler = 1;
      // kFirstExeptionsHandler is kept here for documentation purposes.
      // The last exception seems more like what we want.
      const size_t kLastExceptionHandler = 0;
      g_vector_exception_handler = AddVectoredExceptionHandler(kLastExceptionHandler, vectorExceptionHandling);
#endif
   }

} // end namespace g3
