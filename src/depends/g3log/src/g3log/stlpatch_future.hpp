/** ==========================================================================
* 2013  This is PUBLIC DOMAIN to use at your own risk and comes
* with no warranties. This code is yours to share, use and modify with no
* strings attached and no restrictions or obligations.
*
* For more information see g3log/LICENSE or refer refer to http://unlicense.org
*
*
* 2013/12/28 Bugfix for Visual Studio 2013 which does not handle well
* std::packaged_task<void()>. Thanks to Michael Rasmussen (lap777)
* Ref: workarounds at http://connect.microsoft.com/VisualStudio/feedback/details/791185/std-packaged-task-t-where-t-is-void-or-a-reference-class-are-not-movable
* ============================================================================*/



#pragma once
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__)) && !defined(__MINGW32__)
namespace std {

   template<class... _ArgTypes>
   class packaged_task<void(_ArgTypes...)>
   {
      promise<void> _my_promise;
      function<void(_ArgTypes...)> _my_func;

   public:
      packaged_task() {
      }

      template<class _Fty2>
      explicit packaged_task(_Fty2 &&_Fnarg)
         : _my_func(_Fnarg) {
      }

      packaged_task(packaged_task &&_Other)
         : _my_promise(move(_Other._my_promise)),
           _my_func(move(_Other._my_func)) {
      }

      packaged_task &operator=(packaged_task && _Other) {
         _my_promise = move(_Other._my_promise);
         _my_func = move(_Other._my_func);
         return (*this);
      }

      packaged_task(const packaged_task &) = delete;
      packaged_task &operator=(const packaged_task &) = delete;

      ~packaged_task() {
      }

      void swap(packaged_task &_Other) {
         swap(_my_promise, _Other._my_promise);
         swap(_my_func, _Other._my_func);
      }

      explicit operator bool() const {
         return _my_func != false;
      }

      bool valid() const {
         return _my_func != false;
      }

      future<void> get_future() {
         return _my_promise.get_future();
      }

      void operator()(_ArgTypes... _Args) {
         _my_func(forward<_ArgTypes>(_Args)...);
         _my_promise.set_value();
      }

      void reset() {
         _my_promise.swap(promise<void>());
         _my_func.swap(function<void(_ArgTypes...)>());
      }
   };

}; // namespace std
#endif // defined(WIN32) ...
