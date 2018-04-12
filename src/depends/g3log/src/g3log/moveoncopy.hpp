/** ==========================================================================
* 2013 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
* with no warranties. This code is yours to share, use and modify with no
* strings attached and no restrictions or obligations.
 *
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
* ============================================================================*/

#pragma once
namespace g3 {

   // A straightforward technique to move around packaged_tasks.
   //  Instances of std::packaged_task are MoveConstructible and MoveAssignable, but
   //  not CopyConstructible or CopyAssignable. To put them in a std container they need
   //  to be wrapped and their internals "moved" when tried to be copied.

   template<typename Moveable>
   struct MoveOnCopy {
      mutable Moveable _move_only;

      explicit MoveOnCopy(Moveable &&m) : _move_only(std::move(m)) {}
      MoveOnCopy(MoveOnCopy const &t) : _move_only(std::move(t._move_only)) {}
      MoveOnCopy(MoveOnCopy &&t) : _move_only(std::move(t._move_only)) {}

      MoveOnCopy &operator=(MoveOnCopy const &other) {
         _move_only = std::move(other._move_only);
         return *this;
      }

      MoveOnCopy &operator=(MoveOnCopy && other) {
         _move_only = std::move(other._move_only);
         return *this;
      }

      void operator()() {
         _move_only();
      }

      Moveable &get() {
         return _move_only;
      }
      
      Moveable release() {
         return std::move(_move_only);
      }
   };

} // g3
