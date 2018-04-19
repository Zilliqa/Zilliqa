/** ==========================================================================
* 2012 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
* with no warranties. This code is yours to share, use and modify with no
* strings attached and no restrictions or obligations.
*
* For more information see g3log/LICENSE or refer refer to http://unlicense.org
* ============================================================================*/

#include "g3log/loglevels.hpp"
#include <cassert>

#include <iostream>

namespace g3 {
   namespace internal {
      bool wasFatal(const LEVELS& level) {
         return level.value >= FATAL.value;
      }

#ifdef G3_DYNAMIC_LOGGING
      const std::map<int, LoggingLevel> g_log_level_defaults = {
	     {G3LOG_DEBUG.value,{G3LOG_DEBUG}},
         {INFO.value, {INFO}},
         {WARNING.value, {WARNING}},
         {FATAL.value, {FATAL}}
      };

      std::map<int, g3::LoggingLevel> g_log_levels = g_log_level_defaults;
#endif
   } // internal

#ifdef G3_DYNAMIC_LOGGING
   namespace only_change_at_initialization {

      void addLogLevel(LEVELS lvl, bool enabled) {
         int value = lvl.value;
         internal::g_log_levels[value] = {lvl, enabled};
      }


      void addLogLevel(LEVELS level) {
         addLogLevel(level, true);
      }

      void reset() {
         g3::internal::g_log_levels = g3::internal::g_log_level_defaults;
      }
   } // only_change_at_initialization


   namespace log_levels {

      void setHighest(LEVELS enabledFrom) {
         auto it = internal::g_log_levels.find(enabledFrom.value);
         if (it != internal::g_log_levels.end()) {
            for (auto& v : internal::g_log_levels) {
               if (v.first < enabledFrom.value) {
                  disable(v.second.level);
               } else {
                  enable(v.second.level);
               }

            }
         }
      }


      void set(LEVELS level, bool enabled) {
         auto it = internal::g_log_levels.find(level.value);
         if (it != internal::g_log_levels.end()) {
            internal::g_log_levels[level.value] = {level, enabled};
         }
      }


      void disable(LEVELS level) {
         set(level, false);
      }

      void enable(LEVELS level) {
         set(level, true);
      }


      void disableAll() {
         for (auto& v : internal::g_log_levels) {
            v.second.status = false;
         }
      }

      void enableAll() {
         for (auto& v : internal::g_log_levels) {
            v.second.status = true;
         }
      }


      std::string to_string(std::map<int, g3::LoggingLevel> levelsToPrint) {
         std::string levels;
         for (auto& v : levelsToPrint) {
            levels += "name: " + v.second.level.text + " level: " + std::to_string(v.first) + " status: " + std::to_string(v.second.status.value()) + "\n";
         }
         return levels;
      }

      std::string to_string() {
         return to_string(internal::g_log_levels);
      }


      std::map<int, g3::LoggingLevel> getAll() {
         return internal::g_log_levels;
      }

      // status : {Absent, Enabled, Disabled};
      status getStatus(LEVELS level) {
         const auto it = internal::g_log_levels.find(level.value);
         if (internal::g_log_levels.end() == it) {
            return status::Absent;
         }

         return (it->second.status.get().load() ? status::Enabled : status::Disabled);

      }
   } // log_levels

#endif


   bool logLevel(LEVELS log_level) {
#ifdef G3_DYNAMIC_LOGGING
      int level = log_level.value;
      bool status = internal::g_log_levels[level].status.value();
      return status;
#endif
      return true;
   }
} // g3
