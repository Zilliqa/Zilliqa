/** ==========================================================================
* 2013 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
* with no warranties. This code is yours to share, use and modify with no
* strings attached and no restrictions or obligations.
 *
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
* ============================================================================*/

#pragma once

#include "g3log/logmessage.hpp"

namespace g3 {
   namespace internal {

      struct SinkWrapper {
         virtual ~SinkWrapper() { }
         virtual void send(LogMessageMover msg) = 0;
      };
   }
}

