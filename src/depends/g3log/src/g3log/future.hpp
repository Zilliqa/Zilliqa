#pragma once
/** ==========================================================================
* 2012 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
* with no warranties. This code is yours to share, use and modify with no
* strings attached and no restrictions or obligations.
 *
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
* ============================================================================
* Filename:g3future.hpp
*          Helper functionality to put packaged_tasks in standard container. This
*          is especially helpful for background thread processing a la async but through
*          an actor pattern (active object), thread pool or similar.
* Created: 2012 by Kjell Hedström
*
* COMMUNITY THANKS:
* The code below is in large thanks to exemplifying code snippets from StackOverflow
* question/answer: http://stackoverflow.com/questions/6230893/developing-c-concurrency-library-with-futures-or-similar-paradigm
* and a discussion between Lars Gullik Bjønnes and Jonathan Wakely's at: http://gcc.gnu.org/ml/gcc-help/2011-11/msg00052.html
*
* Both are highly recommended reads if you are interested in c++ concurrency library
*  - Kjell, 2012
*
* PUBLIC DOMAIN and NOT under copywrite protection.
* ********************************************* */



#include <future>
#include "g3log/active.hpp"
#include "g3log/moveoncopy.hpp"
#include "g3log/stlpatch_future.hpp"

namespace g3 {
   // Generic helper function to avoid repeating the steps for managing
   // asynchronous task job (by active object) that returns a future results
   // could of course be made even more generic if done more in the way of
   // std::async, ref: http://en.cppreference.com/w/cpp/thread/async
   //
   // Example usage:
   //  std::unique_ptr<Active> bgWorker{Active::createActive()};
   //  ...
   //  auto msg_call=[=](){return ("Hello from the Background");};
   //  auto future_msg = g3::spawn_task(msg_lambda, bgWorker.get());
   template <typename Func, class BgWorker>
   std::future<typename std::result_of<Func()>::type> spawn_task(Func func, BgWorker *worker)
   {
      typedef typename std::result_of<Func()>::type result_type;
      typedef std::packaged_task<result_type()> task_type;

      if (nullptr == worker) {
         auto p = std::make_shared<std::promise<result_type>>();
         std::future<result_type> future_result = p->get_future();
         p->set_exception(std::make_exception_ptr(std::runtime_error("nullptr instantiated worker")));
         return future_result;
      }

      task_type task(std::move(func));

      std::future<result_type> result = task.get_future();
      worker->send(MoveOnCopy<task_type>(std::move(task)));
      return std::move(result);
   }
} // end namespace g3
