/** ==========================================================================
 * 2014 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
 * with no warranties. This code is yours to share, use and modify with no
 * strings attached and no restrictions or obligations.
 * 
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
 * ============================================================================*/


#pragma once

struct SomeLibrary {

   SomeLibrary() {};

   virtual ~SomeLibrary() {};
   virtual void action() = 0;
};

class LibraryFactory {
public:
   virtual SomeLibrary* CreateLibrary() = 0;
};
