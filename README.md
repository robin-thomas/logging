# C/C++ Logging library

The logging library supports various logging severity levels (in the increasing order of their severity): **DEBUG**, **INFO**, **WARNING**, **ERROR**, **FATAL**. Given a log message of severity type A and the filesystem logging severity level B, the message shall be logged only if A >= B. For example, if the filesystem logging severity level is FATAL, then only FATAL log messages shall be logged. They can be accessed like logging::FATAL.

To start using the logging library, it has to be initialized first. It can done by calling:

```
void logging::init_logging(const std::string& path,
                           logging::log_level_t level,
                           bool sigsegv_handling /* default => true */,
                           bool fatal_handling /* default => true */);
```

* **path** => path of the directory where the log file shall be created. The name of the log file shall be *log.txt*. If no                  path is specified (*empty string*), logs shall be redirected to **stderr**.

* **level** => logging severity level (*logging::INFO or logging:: ERROR and so on*) of the  system. The log messages shall be                logged only if the log message has a higher severity level than this value.

* **sigsegv_handling** => set it to *true* if the library has to catch SIGSEGV signals. Stack trace of the current thread shall                           also be logged. If this value is *false*, SIGSEGV won’t be caught. Unless you have manually set a                              SIGSEGV handler, it shall result in a *Segmentation Fault*, whose default action is to terminate the                           system.

* **fatal_handling** => set it to *true* if the library has to terminate the system when FATAL messages are logged. A stack                            trace is also logged. If this value is *false*, only the FATAL log message shall be logged. It won’t                           log the stack trace and nor shall terminate the system.

To stop logging, call :
```
void logging::stop_logging(void);
```

Once the use of the library is over, `logging::stop_logging()` should be called. The library shall prevent `logging::init_logging()` from being called multiple times, without accompanied by individual `logging::stop_logging()` calls.

FATAL errors are those errors the filesystem cannot recover from. In those conditions, the only choice is to terminate the filesystem. So in case a FATAL log message is encountered, it shall log the message along with the stack trace, and then terminate the filesystem with status code *EXIT_STATUS_FATAL*. There is also a switch to turn OFF this feature, so that fatal messages shall be logged, but won’t terminate the filesystem.

The library currently supports logging using macros like Fatal, Error, Warning, Info and Debug. All of them have the format of the *C printf()* function. As an example, 
```
Error(“Testing error log, status = %d\n”, status);
```
All these macros can be used only after calling `logging::init_logging()`.

The library also supports conditional logging, which is apt for situations where you need to log only under certain conditions. You need to specify the severity level of the log message, as well the condition which needs to be evaluated true for the message to be logged. It can done by using the **LOG_IF** macro.
```
LOG_IF(logging severity level, condition, …);
```
The … follows the same format as that of the `printf()` function. As an example,
```
LOG_IF(logging::INFO, status > 0, “Testing info, status = %d\n”, status);
```
If the condition is false, the message won’t be logged.

The library also supports *buffered logging*, in which log messages of severity levels DEBUG, INFO and WARNING are buffered, and are not instantly written to log file. ERROR and FATAL log messages are not buffered. In case an ERROR or FATAL log message occurs or when the buffer is almost full (*for any severity level*), the buffer is flushed and the new log messages are written straight to the log file (*instead of the buffer*). The default size of the buffer is 4k. The buffer is also flushed every 5 minutes (*if the buffer is not empty*).

In case a SIGSEGV (*Segmentation Fault*) occurs while the filesystem is running, the logging library shall detect it and log the stack trace, and gracefully terminate the filesystem with status code *EXIT_STATUS_SIGSEGV*. There is also a switch to turn this feature OFF, so that when SIGSEGV occurs, it shall terminate the system (*default action*).

The library is also supplied with a set of unit tests to make sure that the library shall run properly. It’s also tested with Valgrind to make sure there are no memory leaks.

To use the library, include the header file log.h in your C/C++ file.
```
#include “log.h”
```
While compiling, link it with *-llog* and *-lpthread* flags, to use the library:
```
g++ file.cc -L. -llog -lpthread -o file_bin
```

To create the library manually and run the unit tests, run the following commands.
```
g++ -c log.cc -Wall -Werror
ar rvs liblog.a log.o
g++ log_unittests.cc -L. -llog -lpthread -o log_test -Wall -Werror
./log_test
```

In case there are no problems, you shall get the below image.
![Logging library unit tests](http://imgur.com/download/pdiIXIL/)
