/** ==========================================================================
* 2013 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
* with no warranties. This code is yours to share, use and modify with no
* strings attached and no restrictions or obligations.
 *
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
* ============================================================================*/

#pragma once


#include "g3log/sinkwrapper.hpp"
#include "g3log/active.hpp"
#include "g3log/future.hpp"
#include "g3log/logmessage.hpp"

#include <memory>
#include <functional>
#include <type_traits>

namespace g3 {
   namespace internal {
      typedef std::function<void(LogMessageMover) > AsyncMessageCall;

      /// The asynchronous Sink has an active object, incoming requests for actions
      //  will be processed in the background by the specific object the Sink represents.
      //
      // The Sink will wrap either
      //     a Sink with Message object receiving call
      // or  a Sink with a LogEntry (string) receving call
      //
      // The Sink can also be used through the SinkHandler to call Sink specific function calls
      // Ref: send(Message) deals with incoming log entries (converted if necessary to string)
      // Ref: send(Call call, Args... args) deals with calls
      //           to the real sink's API

      template<class T>
      struct Sink : public SinkWrapper {
         std::unique_ptr<T> _real_sink;
         std::unique_ptr<kjellkod::Active> _bg;
         AsyncMessageCall _default_log_call;

         template<typename DefaultLogCall >
         Sink(std::unique_ptr<T> sink, DefaultLogCall call)
            : SinkWrapper(),
         _real_sink {std::move(sink)},
         _bg(kjellkod::Active::createActive()),
         _default_log_call(std::bind(call, _real_sink.get(), std::placeholders::_1)) {
         }


         Sink(std::unique_ptr<T> sink, void(T::*Call)(std::string) )
            : SinkWrapper(),
         _real_sink {std::move(sink)},
         _bg(kjellkod::Active::createActive()) {
            std::function<void(std::string)> adapter = std::bind(Call, _real_sink.get(), std::placeholders::_1);
            _default_log_call = [ = ](LogMessageMover m) {
               adapter(m.get().toString());
            };
         }

         virtual ~Sink() {
            _bg.reset(); // TODO: to remove
         }

         void send(LogMessageMover msg) override {
            _bg->send([this, msg] {
               _default_log_call(msg);
            });
         }

         template<typename Call, typename... Args>
         auto async(Call call, Args &&... args)-> std::future< typename std::result_of<decltype(call)(T, Args...)>::type> {
            return g3::spawn_task(std::bind(call, _real_sink.get(), std::forward<Args>(args)...), _bg.get());
         }
      };
   } // internal
} // g3

