#pragma once
/** ==========================================================================
 * 2011 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
 * with no warranties. This code is yours to share, use and modify with no
 * strings attached and no restrictions or obligations.
 *
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
 * ============================================================================
 * Filename:g3logworker.h  Framework for Logging and Design By Contract
 * Created: 2011 by Kjell Hedström
 *
 * PUBLIC DOMAIN and Not copywrited. First published at KjellKod.cc
 * ********************************************* */
#include "g3log/g3log.hpp"
#include "g3log/sinkwrapper.hpp"
#include "g3log/sinkhandle.hpp"
#include "g3log/filesink.hpp"
#include "g3log/logmessage.hpp"
#include <memory>

#include <memory>
#include <string>
#include <vector>


namespace g3 {
   class LogWorker;
   struct LogWorkerImpl;
   using FileSinkHandle = g3::SinkHandle<g3::FileSink>;

   /// Background side of the LogWorker. Internal use only
   struct LogWorkerImpl final {
      typedef std::shared_ptr<g3::internal::SinkWrapper> SinkWrapperPtr;
      std::vector<SinkWrapperPtr> _sinks;
      std::unique_ptr<kjellkod::Active> _bg; // do not change declaration order. _bg must be destroyed before sinks

      LogWorkerImpl();
      ~LogWorkerImpl() = default;

      void bgSave(g3::LogMessagePtr msgPtr);
      void bgFatal(FatalMessagePtr msgPtr);

      LogWorkerImpl(const LogWorkerImpl&) = delete;
      LogWorkerImpl& operator=(const LogWorkerImpl&) = delete;
   };



   /// Front end of the LogWorker.  API that is usefule is
   /// addSink( sink, default_call ) which returns a handle to the sink. See below and REAME for usage example
   /// save( msg ) : internal use
   /// fatal ( fatal_msg ) : internal use
   class LogWorker final {
      LogWorker() = default;
      void addWrappedSink(std::shared_ptr<g3::internal::SinkWrapper> wrapper);

      LogWorkerImpl _impl;
      LogWorker(const LogWorker&) = delete;
      LogWorker& operator=(const LogWorker&) = delete;


    public:
      ~LogWorker();

      /// Creates the LogWorker with no sinks. See exampel below on @ref addSink for how to use it
      /// if you want to use the default file logger then see below for @ref addDefaultLogger
      static std::unique_ptr<LogWorker> createLogWorker();

      
      /**
      A convenience function to add the default g3::FileSink to the log worker
       @param log_prefix that you want
       @param log_directory where the log is to be stored.
       @return a handle for API access to the sink. See the README for example usage

       @verbatim
       Example:
       using namespace g3;
       std::unique_ptr<LogWorker> logworker {LogWorker::createLogWorker()};
       auto handle = addDefaultLogger("my_test_log", "/tmp");
       initializeLogging(logworker.get()); // ref. g3log.hpp

       std::future<std::string> log_file_name = sinkHandle->call(&FileSink::fileName);
       std::cout << "The filename is: " << log_file_name.get() << std::endl;
       //   something like: /tmp/my_test_log.g3log.20150819-100300.log
       */
       std::unique_ptr<FileSinkHandle> addDefaultLogger(const std::string& log_prefix, const std::string& log_directory, const std::string& default_id = "g3log");



      /// Adds a sink and returns the handle for access to the sink
      /// @param real_sink unique_ptr ownership is passed to the log worker
      /// @param call the default call that should receive either a std::string or a LogMessageMover message
      /// @return handle to the sink for API access. See usage example below at @ref addDefaultLogger
      template<typename T, typename DefaultLogCall>
      std::unique_ptr<g3::SinkHandle<T>> addSink(std::unique_ptr<T> real_sink, DefaultLogCall call) {
         using namespace g3;
         using namespace g3::internal;
         auto sink = std::make_shared<Sink<T>> (std::move(real_sink), call);
         addWrappedSink(sink);
         return std::make_unique<SinkHandle<T>> (sink);
      }



      /// internal:
      /// pushes in background thread (asynchronously) input messages to log file
      void save(LogMessagePtr entry);

      /// internal:
      //  pushes a fatal message on the queue, this is the last message to be processed
      /// this way it's ensured that all existing entries were flushed before 'fatal'
      /// Will abort the application!
      void fatal(FatalMessagePtr fatal_message);


   };
} // g3
