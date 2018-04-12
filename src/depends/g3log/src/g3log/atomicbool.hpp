/** ==========================================================================
* 2015 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
* with no warranties. This code is yours to share, use and modify with no
* strings attached and no restrictions or obligations.
*
* For more information see g3log/LICENSE or refer refer to http://unlicense.org
* ============================================================================*/


#pragma once

#include <atomic>

namespace g3 {
   /// As suggested in: http://stackoverflow.com/questions/13193484/how-to-declare-a-vector-of-atomic-in-c
   struct atomicbool {
    private:
      std::atomic<bool> value_;
    public:
      atomicbool(): value_ {false} {}
      atomicbool(const bool& value): value_ {value} {}
      atomicbool(const std::atomic<bool>& value) : value_ {value.load(std::memory_order_acquire)} {}
      atomicbool(const atomicbool& other): value_ {other.value_.load(std::memory_order_acquire)} {}

      atomicbool& operator=(const atomicbool& other) {
         value_.store(other.value_.load(std::memory_order_acquire), std::memory_order_release);
         return *this;
      }

      atomicbool& operator=(const bool other) {
         value_.store(other, std::memory_order_release);
         return *this;
      }

   bool operator==(const atomicbool& rhs)  const {
      return (value_.load(std::memory_order_acquire) == rhs.value_.load(std::memory_order_acquire));
   }

      bool value() {return value_.load(std::memory_order_acquire);}
      std::atomic<bool>& get() {return value_;}
   };
} // g3
// explicit whitespace/EOF for VS15 