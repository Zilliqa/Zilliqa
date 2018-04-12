/** ==========================================================================
 * 2014 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
 * with no warranties. This code is yours to share, use and modify with no
 * strings attached and no restrictions or obligations.
 * 
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
 * ============================================================================*/


#include <g3log/g3log.hpp>
#include <g3log/logworker.hpp>
#include "tester_sharedlib.h"

struct RuntimeLoadedLib : public SomeLibrary {

   RuntimeLoadedLib() {
      LOG(INFO) << "Library was created";
      LOGF(INFO, "Ready for testing");
   }

   ~RuntimeLoadedLib() {
      LOG(G3LOG_DEBUG) << "Library destroyed";
   }

   void action() {
      LOG(WARNING) << "Action, action, action. Safe for LOG calls by runtime dynamically loaded libraries";
   }
};

struct RealLibraryFactory : public LibraryFactory {
   SomeLibrary* CreateLibrary() {
      return new RuntimeLoadedLib;
   }
};

RealLibraryFactory testRealFactory;

