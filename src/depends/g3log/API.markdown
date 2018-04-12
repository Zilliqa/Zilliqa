# API description
Most of the API that you need for using g3log is described in this readme. For more API documentation and examples please continue to read the [API readme](API.markdown). Examples of what you will find here are: 

* Logging API: LOG calls
* Contract API: CHECK calls
* Logging levels 
  * disable/enabled levels at runtime
  * custom logging levels
* Sink [creation](#sink_creation) and utilization 
* Custom [log formatting](#log_formatting) 
  * Overriding the Default File Sink's file header
  * Overriding the Default FileSink's log formatting
  * Adding thread ID to the log formatting
  * Override log formatting in a default and custom sinks
  * Override the log formatting in the default sink
* LOG [flushing](#log_flushing)
* G3log and G3Sinks [usage example](#g3log-and-sink-usage-code-example)
* Support for [dynamic message sizing](#dynamic_message_sizing)
* Fatal handling
  * [Linux/*nix](#fatal_handling_linux)
  * <strike>[TOWRITE: Windows](#fatal_handling_windows)</strike>
  * <strike>[TOWRITE: Custom fatal handling](#fatal_custom_handling)</strike>
  * <strike>[TOWRITE: Pre fatal hook](#fatal_pre_hook)</strike>
  * <strike>[TOWRITE: Override of signal handling](#fatal_signalhandler_override)</strike>
  * <strike>[TOWRITE: Disable fatal handling](#fatal_handling_disabled)</strike>
* Build Options


## Logging API: LOG calls
It is optional to use either streaming ```LOG(INFO) << "some text" ``` or printf-like syntax ```LOGF(WARNING, "some number %d", 123); ```

Conditional logging is made with ```LOG_IF(INFO, <boolean-expression>) << " some text"  ``` or ```LOGF_IF(WARNING, <boolean-expression>) << " some text".```  Only if the expressions evaluates to ```true``` will the logging take place. 

Example:
```LOG_IF(INFO, 1 != 200) << " some text";```   or ```LOG_IF(FATAL, SomeFunctionCall()) << " some text";```

*<a name="fatal_logging">A call using FATAL</a>  logging level, such as the ```LOG_IF(FATAL,...)``` example above, will after logging the message at ```FATAL```level also kill the process.  It is essentially the same as a ```CHECK(<boolea-expression>) << ...``` with the difference that the ```CHECK(<boolean-expression)``` triggers when the expression evaluates to ```false```.*

## Contract API: CHECK calls
The contract API follows closely the logging API with ```CHECK(<boolean-expression>) << ...``` for streaming  or  (*) ```CHECKF(<boolean-expression>, ...);``` for printf-style.


If the ```<boolean-expression>``` evaluates to false then the the message for the failed contract will be logged in FIFO order with previously made messages. The process will then shut down after the message is sent to the sinks and the sinks have dealt with the fatal contract message. 


(\* * ```CHECK_F(<boolean-expression>, ...);``` was the the previous API for printf-like CHECK. It is still kept for backwards compatability but is exactly the same as ```CHECKF``` *)


## Logging levels 
 The default logging levels are ```DEBUG```, ```INFO```, ```WARNING``` and ```FATAL``` (see FATAL usage [above](#fatal_logging)). The logging levels are defined in [loglevels.hpp](src/g3log/loglevels.hpp).

 For some windows framework there is a clash with the ```DEBUG``` logging level. One of the CMake [Build options](#build_options) can be used to then change offending default level from ```DEBUG``` TO ```DBUG```.

 **CMake option: (default OFF) ** ```cmake -DCHANGE_G3LOG_DEBUG_TO_DBUG=ON ..``` 

  ### disable/enabled levels at runtime
  Logging levels can be disabled at runtime. The logic for this happens in
  [loglevels.hpp](src/g3log/loglevels.hpp), [loglevels.cpp](src/loglevels.cpp) and [g3log.hpp](src/g3log/g3log.hpp).

  There is a cmake option to enable the dynamic enable/disable of levels. 
  When the option is enabled there will be a slight runtime overhead for each ```LOG``` call when the enable/disable status is checked. For most intent and purposes this runtime overhead is negligable. 

  There is **no** runtime overhead for internally checking if a level is enabled//disabled if the cmake option is turned off. If the dynamic logging cmake option is turned off then all logging levels are enabled.

**CMake option: (default OFF)** ```cmake -DUSE_DYNAMIC_LOGGING_LEVELS=ON  ..``` 


  ### custom logging levels
  Custom logging levels can be created and used. When defining a custom logging level you set the value for it as well as the text for it. You can re-use values for other levels such as *INFO*, *WARNING* etc or have your own values.

   Any value with equal or higher value than the *FATAL* value will be considered a *FATAL* logging level. 

  Example:
  ```
  // In CustomLoggingLevels.hpp
  #include <g3log/loglevels.hpp>

  // all values with a + 1 higher than their closest equivalet
  // they could really have the same value as well.

  const LEVELS FYI {DEBUG.value + 1, {"For Your Information"}}; 
  const LEVELS CUSTOM {INFO.value + 1, {"CUSTOM"}}; 
  const LEVELS SEVERE {WARNING.value +1, {"SEVERE"}};
  const LEVELS DEADLY {FATAL.value + 1, {"DEADLY"}}; 
  ```



  
## Sink <a name="sink_creation">creation</a> and utilization 
The default sink for g3log is the one as used in g2log. It is a simple file sink with a limited API. The details for the default file sink can be found in [filesink.hpp](src/g3log/filesink.hpp), [filesink.cpp](src/filesink.cpp), [filesinkhelper.ipp](src/filesinkhelper.ipp)

More sinks can be found at [g3sinks](http://www.github.com/KjellKod/g3sinks) (log rotate, log rotate with filtering on levels)

A logging sink is not required to be a subclass of a specific type. The only requirement of a logging sink is that it can receive a logging message of 


### Using the default sink
Sink creation is defined in [logworker.hpp](src/g3log/logworker.hpp) and used in [logworker.cpp](src/logworker.cpp). For in-depth knowlege regarding sink implementation details you can look at [sinkhandle.hpp](src/g3log/sinkhandle.hpp) and [sinkwrapper.hpp](src/g3log/sinkwrapper.hpp)
```
  std::unique_ptr<FileSinkHandle> addDefaultLogger(
            const std::string& log_prefix
            , const std::string& log_directory
            , const std::string& default_id = "g3log");
```

With the default id left as is (i.e. "g3log") a creation of the logger in the unit test "test_filechange" would look like this
```
  const std::string directory = "./";
  const std::string name = "(ReplaceLogFile)";
  auto worker = g3::LogWorker::createLogWorker();
  auto handle = worker->addDefaultLogger(name, directory);
```
The resulting filename would be something like: 
```
   ./(ReplaceLogFile).g3log.20160217-001406.log
```


## Custom LOG <a name="log_formatting">formatting</a>
### Overriding the Default File Sink's file header
The default file header can be customized in the default file sink in calling 
```
   FileSink::overrideLogHeader(std::string);
```


### Overriding the Default FileSink's log formatting
The default log formatting is defined in `LogMessage.hpp`
```
   static std::string DefaultLogDetailsToString(const LogMessage& msg);
```

### Adding thread ID to the log formatting
An "all details" log formatting function is also defined - this one also adds the "calling thread's ID"
```
   static std::string FullLogDetailsToString(const LogMessage& msg);
```

### Override log formatting in default and custom sinks
The default log formatting look can be overriden by any sink. 
If the sink receiving function calls `toString()` then the default log formatting will be used.
If the sink receiving function calls `toString(&XFunc)` then the `XFunc`will be used instead (see `LogMessage.h/cpp` for code details if it is not clear). (`XFunc` is a place holder for *your* formatting function of choice). 

The API for the function-ptr to pass in is 
```
std::string (*) (const LogMessage&)
```
or for short as defined in `LogMessage.h`
```
using LogDetailsFunc = std::string (*) (const LogMessage&);
```

### Override the log formatting in the default sink
For convenience the *Default* sink has a function
for doing exactly this
```
  void overrideLogDetails(LogMessage::LogDetailsFunc func);
```


Example code for replacing the default log formatting for "full details" formatting (it adds thread ID)

```
   auto worker = g3::LogWorker::createLogWorker();
   auto handle= worker->addDefaultLogger(argv[0], path_to_log_file);
   g3::initializeLogging(worker.get());
   handle->call(&g3::FileSink::overrideLogDetails, &LogMessage::FullLogDetailsToString);
```

See [test_message.cpp](http://www.github.com/KjellKod/g3log/test_unit/test_message.cpp) for details and testing


Example code for overloading the formatting of a custom sink. The log formatting function will be passed into the 
`LogMessage::toString(...)` this will override the default log formatting

Example
```
namespace {
      std::string MyCustomFormatting(const LogMessage& msg) {
        ... how you want it ...
      }
    }

   void MyCustomSink::ReceiveLogEntry(LogMessageMover message) {
      std::string formatted = message.get().toString(&MyCustomFormatting) << std::flush;
   }
...
...
 auto worker = g3::LogWorker::createLogWorker();
 auto sinkHandle = worker->addSink(std::make_unique<MyCustomSink>(),
                                     &MyCustomSink::ReceiveLogMessage);
 // ReceiveLogMessage(...) will used the custom formatting function "MyCustomFormatting(...)
    
```





## LOG <a name="log_flushing">flushing</a> 
The default file sink will flush each log entry as it comes in. For different flushing policies please take a look at g3sinks [logrotate and LogRotateWithFilters](http://www.github.com/KjellKod/g3sinks/logrotate).

At shutdown all enqueued logs will be flushed to the sink.  
At a discovered fatal event (SIGSEGV et.al) all enqueued logs will be flushed to the sink.

A programmatically triggered abrupt process exit such as a call to   ```exit(0)``` will of course not get the enqueued log entries flushed. Similary  a bug that does not trigger a fatal signal but a process exit will also not get the enqueued log entries flushed.  G3log can catch several fatal crashes and it deals well with RAII exits but magic is so far out of its' reach.

# G3log and Sink Usage Code Example
Example usage where a [logrotate sink (g3sinks)](https://github.com/KjellKod/g3sinks) is added. In the example it is shown how the logrotate API is called. The logrotate limit is changed from the default to instead be 10MB. The limit is changed by calling the sink handler which passes the function call through to the actual logrotate sink object.
```

// main.cpp
#include <g3log/g3log.hpp>
#include <g3log/logworker.h>
#include <g3sinks/logrotate.hpp>
#include <memory>

int main(int argc, char**argv) {
   using namespace g3;
   std::unique_ptr<LogWorker> logworker{ LogWorker::createLogWorker() };
   auto sinkHandle = logworker->addSink(std::make_unique<LogRotate>(),
                                          &LogRotate::save);
   
   // initialize the logger before it can receive LOG calls
   initializeLogging(logworker.get());            
            
   // You can call in a thread safe manner public functions on the logrotate sink
   // The call is asynchronously executed on your custom sink.
   const int k10MBInBytes = 10 * 1024 * 1024;
   std::future<void> received = sinkHandle->call(&LogRotate::setMaxLogSize, k10MBInBytes);
   
   // Run the main part of the application. This can be anything of course, in this example
   // we'll call it "RunApplication". Once this call exits we are in shutdown mode
   RunApplication();

   // If the LogWorker is initialized then at scope exit the g3::shutDownLogging() will be 
   // called automatically. 
   //  
   // This is important since it protects from LOG calls from static or other entities that will go out of
   // scope at a later time. 
   //
   // It can also be called manually if for some reason your setup is different then the one highlighted in
   // this example
   g3::shutDownLogging();
}
```


## Dynamic Message Sizing <a name="dynamic_message_sizing"></a>
The default build uses a fixed size buffer for formatting messages. The size of this buffer is 2048 bytes. If an incoming message results in a formatted message that is greater than 2048 bytes, it will be bound to 2048 bytes and will have the string ```[...truncated...]``` appended to the end of the bound message. There are cases where one would like to dynamically change the size at runtime. For example, when debugging payloads for a server, it may be desirable to handle larger message sizes in order to examine the whole payload. Rather than forcing the developer to rebuild the server, dynamic message sizing could be used along with a config file which defines the message size at runtime.

This feature supported as a CMake option:

**CMake option: (default OFF)** ```cmake -DUSE_G3_DYNAMIC_MAX_MESSAGE_SIZE=ON ..```

The following is an example of changing the size for the message.

```
    g3::only_change_at_initialization::setMaxMessageSize(10000);
```


## Fatal handling
The default behaviour for G3log is to catch several fatal events before they force the process to exit. After <i>catching</i> a fatal event a stack dump is generated and all log entries, up to the point of the stack dump are together with the dump flushed to the sink(s).


  ### <a name="fatal_handling_linux">Linux/*nix</a> 
  The default fatal handling on Linux deals with fatal signals. At the time of writing these signals were   ```SIGABRT, SIGFPE, SIGILL, SIGILL, SIGSEGV, SIGSEGV, SIGTERM```.  The Linux fatal handling is handled in [crashhandler.hpp](src/g3log/crashhandler.hpp) and [crashhandler_unix.cpp](src/crashhandler_unix.cpp)



   A signal that commonly is associated with voluntarily process exit is ```SIGINT``` (ctrl + c) G3log does not deal with it. 

   The fatal signals can be [disabled](#fatal_handling_disabled) or  [changed/added ](#fatal_signalhandler_override). 

   An example of a Linux stackdump as shown in the output from the fatal example <i>g3log-FATAL-sigsegv</i>.
    ```
    ***** FATAL SIGNAL RECEIVED ******* 
    "Received fatal signal: SIGSEGV(11)     PID: 6571

    ***** SIGNAL SIGSEGV(11)

    ******* STACKDUMP *******
            stack dump [1]  ./g3log-FATAL-sigsegv() [0x42a500]
            stack dump [2]  /lib/x86_64-linux-gnu/libpthread.so.0+0x10340 [0x7f83636d5340]

            stack dump [3]  ./g3log-FATAL-sigsegv : example_fatal::tryToKillWithAccessingIllegalPointer(std::unique_ptr<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >, std::default_delete<std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > > >)+0x119 [0x4107b9]
            stack dump [4]  ./g3log-FATAL-sigsegvmain+0xdec [0x40e51c]
            stack dump [5]  /lib/x86_64-linux-gnu/libc.so.6__libc_start_main+0xf5 [0x7f8363321ec5]
            stack dump [6]  ./g3log-FATAL-sigsegv() [0x40ffa2]

    Exiting after fatal event  (FATAL_SIGNAL). Fatal type:  SIGSEGV
    Log content flushed sucessfully to sink

    "
    g3log g3FileSink shutdown at: 16:33:18

    ```


  <strikte>
   ### <a name="fatal_handling_windows">TOWRITE: Windows</a>
  Windows fatal handling also deals with fatal signals just like Linux. In addition to fatal signals it also deals with unhandled exceptions, vectored exceptions.  Windows fatal handling is handled in [crashhandler.hpp](src/g3log/crashhandler.hpp), [crashhandler_windows.cpp](src/crashhandler_windows.cpp), [stacktrace_windows.hpp](src/g3log/stacktrace_windows.hpp), [stacktrace_windows.cpp](src/stacktrace_windows.cpp)
   

  An example of a Windows stackdump as shown in the output from the fatal example <i>g3log-FATAL-sigsegv</i>. 
    
    .... MISSING CONTENT..... since my Windows computer is gone!

   </strike> 
   




   ### <strike><a name="fatal_custom_handling">TOWRITE: Custom fatal handling</a></strike> 
   ### <strike><a name="fatal_pre_hook">TOWRITE: Pre fatal hook</a> </strike> 
   ### <strike><a name="fatal_signalhandler_override">TOWRITE: Override of signal handling</a> </strike> 
   ### <strike><a name="fatal_handling_disabled">TOWRITE: Disable fatal handling</a> </strike> 



  ## <a name="build_options">Build Options</a>
  The build options are defined in the file [Options.cmake](Options.cmake)

  build options are generated and saved to a header file. This avoid having to set the define options in the client source code



# Say Thanks
This logger is available for free and all of its source code is public domain.  
Writing API documentation is probably the most boring task for a developer. 
Did it help you? Could it be better? Please suggest changes, send me feedback or even better: open a pull request.

You could also contribute by saying thanks with a a donation. It would go a long way not only to show your support but also to boost continued development of this logger and its API documentation

[![Donate](https://img.shields.io/badge/Donate-PayPal-green.svg)](https://www.paypal.me/g3log/25)

* $5 for a cup of coffee
* $10 for pizza 
* $25 for a lunch or two
* $100 for a date night with my wife (which buys family credit for evening coding)
* $$$ for upgrading my development environment
* $$$$ :)

Cheers
Kjell
