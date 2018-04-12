/** ==========================================================================
 * 2013 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
 * with no warranties. This code is yours to share, use and modify with no
 * strings attached and no restrictions or obligations.
 *
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
 * ============================================================================*/

#pragma once

#include "g3log/loglevels.hpp"
#include "g3log/crashhandler.hpp"

#include <string>
#include <sstream>
#include <cstdarg>
#include <csignal>
#ifdef _MSC_VER
# include <sal.h>
#endif

/**
 * Simple struct for capturing log/fatal entries. At destruction the captured message is
 * forwarded to background worker.
 * As a safety precaution: No memory allocated here will be moved into the background
 * worker in case of dynamic loaded library reasons
*/
struct LogCapture {
   /// Called from crash handler when a fatal signal has occurred (SIGSEGV etc)
   LogCapture(const LEVELS &level, g3::SignalType fatal_signal, const char *dump = nullptr);


   /**
    * @file, line, function are given in g3log.hpp from macros
    * @level INFO/DEBUG/WARNING/FATAL
    * @expression for CHECK calls
    * @fatal_signal for failed CHECK:SIGABRT or fatal signal caught in the signal handler
    */
   LogCapture(const char *file, const int line, const char *function, const LEVELS &level, const char *expression = "", g3::SignalType fatal_signal = SIGABRT, const char *dump = nullptr);


   // At destruction the message will be forwarded to the g3log worker.
   // In the case of dynamically (at runtime) loaded libraries, the important thing to know is that
   // all strings are copied, so the original are not destroyed at the receiving end, only the copy
   virtual ~LogCapture();




   // Use "-Wall" to generate warnings in case of illegal printf format.
   //      Ref:  http://www.unixwiz.net/techtips/gnu-c-attributes.html
#ifndef __GNUC__
#define  __attribute__(x) // Disable 'attributes' if compiler does not support 'em
#endif 
#ifdef _MSC_VER 
#	if _MSC_VER >= 1400
#		define G3LOG_FORMAT_STRING _Printf_format_string_
#	else
#		define G3LOG_FORMAT_STRING __format_string
#	endif
#else
#	define G3LOG_FORMAT_STRING
#endif
   void capturef(G3LOG_FORMAT_STRING const char *printf_like_message, ...) __attribute__((format(printf, 2, 3))); // 2,3 ref:  http://www.codemaestro.com/reviews/18


   /// prettifying API for this completely open struct
   std::ostringstream &stream() {
      return _stream;
   }



   std::ostringstream _stream;
   std::string _stack_trace;
   const char* _file;
   const int _line;
   const char* _function;
   const LEVELS &_level;
   const char* _expression;
   const g3::SignalType _fatal_signal;

};
//} // g3
