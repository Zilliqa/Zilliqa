/** ==========================================================================
* 2015 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
* with no warranties. This code is yours to share, use and modify with no
* strings attached and no restrictions or obligations.
* 
* For more information see g3log/LICENSE or refer refer to http://unlicense.org
* ============================================================================*/

#pragma once

// For convenience:  If you don't want to do a recursive search and replace in your source code
// for replacing g2log.hpp for g3log/g3log.hpp then you can choose to add this header file to your
// code. It will get the necessary includes
//
//
// Btw: replacing g2log for g3log include is easy on Linux
//  find . -name "*.cpp*" -print | xargs sed -i -e 's/\g2log\.hpp/\g3log\/g3log\.hpp/g'

#include <g3log/g3log.hpp>
#include <g3log/logworker.hpp>
#include <g3log/loglevels.hpp>
#include <g3log/filesink.hpp>

