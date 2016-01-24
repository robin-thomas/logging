
/*
 * Copyright (c) 2015, Robin Thomas.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * The name of Robin Thomas or any other contributors to this software
 * should not be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * Author: Robin Thomas <robinthomas17@gmail.com>
 *
 */

#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/wait.h>

#include "log.h"


// Declarations
bool logging::is_logging_initialized = false;
logging::Log * logging::log = NULL;

const char * logging::log_level_str[logging::TOTAL_LOG_LEVELS] = { 
  "FATAL",
  "ERROR",
  "WARNING",
  "INFO",
  "DEBUG",
};


// Initialize the logging library.
void
logging::init_logging(const std::string& path,
                      logging::log_level_t level,
                      bool sigsegv_handling,
                      bool fatal_handling)
{
  if (logging::is_logging_initialized) {
    fprintf(stderr, "You called init_logging() twice!\n");
    return;
  }

  try {
    log = new logging::Log(path, level,
                           sigsegv_handling, fatal_handling);
  } catch (const char * err) {
    throw err;
  }

  logging::is_logging_initialized = true;
}


// Stop the logging.
void 
logging::stop_logging(void)
{
  if (!logging::is_logging_initialized) {
    fprintf(stderr, "You should call init_logging() before stop_logging()!\n");
    return;
  }

  if (logging::log) {
    delete logging::log;
    logging::log = NULL;
  }

  logging::is_logging_initialized = false;
}


// Constructor
logging::Log::Log(const std::string& path,
                  logging::log_level_t level,
                  bool sigsegv_handling,
                  bool fatal_handling)
{
  // Default values.
  this->path = path;
  this->outfp = NULL;
  this->log_level = level;
  this->log_buf_size = 0;
  this->kill_runner = false;
  this->fatal_handling = fatal_handling;

  // Create the log buffer.
  this->log_buf = new char[LOG_BUF_SIZE];
  if (this->log_buf == NULL) {
    throw "Unable to create log buffer";
  }

  pthread_mutex_init(&(this->mutex), NULL);

  // sanity check, and set the log output.
  if (path.empty() ||
      path.at(0) != '/') {
    fprintf(stderr, "No valid log path specified. Redirecting to stderr\n");
    outfp = stderr;
  } else {
    try {
      set_log_file(path);
    } catch (const char* err) {
      throw err;
    }
  }

  // Handle SIGSEGV if required.
  if (sigsegv_handling) {
    signal(SIGSEGV, detect_sigsegv);
  }
  signal(SIGUSR1, sigusr1_handler);

  // Runnner thread to flush log buffer every 5 min.
  pthread_create(&(this->runner_id), NULL, Runner, NULL);
}


// Destructor
logging::Log::~Log()
{
  this->destroy_runner();
  sleep(1);
  pthread_kill(this->runner_id, SIGUSR1);
  pthread_join(this->runner_id, NULL);
  this->flush_buffer();
  this->do_cleanup();
  signal(SIGSEGV, SIG_DFL);
  signal(SIGUSR1, SIG_DFL);
}


// Ask the runner thread to destroy.
void
logging::Log::destroy_runner(void)
{
  pthread_mutex_lock(&(this->mutex));
  this->kill_runner = true;
  pthread_mutex_unlock(&(this->mutex));
}


// Function to flush the log buffer.
void
logging::Log::flush_buffer(void)
{
  pthread_mutex_lock(&(this->mutex));

  if (this->log_buf_size > 0) {
    this->log_buf[this->log_buf_size] = '\0';
    fputs(this->log_buf, this->outfp);
    this->log_buf_size = 0;
  }

  pthread_mutex_unlock(&(this->mutex));
}


// Flush the log buffer, and write a string to the log file.
void
logging::Log::write_to_log(char * str,
                           size_t len)
{
  this->flush_buffer();
  str[len] = '\0';
  pthread_mutex_lock(&(this->mutex));
  fputs(str, this->outfp);
  pthread_mutex_unlock(&(this->mutex));
}


// Free all memory and destroy mutex.
void
logging::Log::do_cleanup(void)
{
  pthread_mutex_lock(&(this->mutex));
  if (this->kill_runner) {
    if (this->outfp != stderr) {
      fclose(this->outfp);
    }
    delete[] this->log_buf;
  }
  pthread_mutex_unlock(&(this->mutex));
  if (this->kill_runner) {
    pthread_mutex_destroy(&(this->mutex));
  }
}


// Set the path to log file.
void
logging::Log::set_log_file(const std::string& path)
{
  FILE * fp = NULL;
  if ((fp = fopen(path.c_str(), "a")) == NULL) {
    outfp = stderr;
    throw "Unable to create log file";
  }

  outfp = fp;
  setvbuf(outfp, NULL, _IOLBF, 0);
}


// Get the current time.
void
logging::get_current_time(char ** time_str)
{
  time_t t;
  time(&t);
  struct tm* tm = localtime(&t);
  sprintf(*time_str, "%02d-%02d-%04d %02d:%02d:%02d",
                      tm->tm_mday, tm->tm_mon + 1,
                      tm->tm_year + 1900, tm->tm_hour,
                      tm->tm_min, tm->tm_sec);
}


// Function to log a message depending on its severity.
void
logging::Log::log_msg(LOG_FUNC_SIGNATURE,
                      const char *fmt,
                      ...)
{
  if (level <= this->log_level) {
    char tmp[LOG_BUF_SIZE];
    memset(tmp, 0, sizeof tmp);
    va_list args;
    va_start(args, fmt);
    vsprintf(tmp, fmt, args);
    va_end(args);

    // Get current time
    char * time_str = new char[TIME_BUF_SIZE];
    get_current_time(&time_str);

    // Generate the complete log message.
    char log_str[LOG_BUF_SIZE];
    memset(log_str, 0, sizeof log_str);
    int str_size = sprintf(log_str, "%s, %7s Thread %5d, %s:%d => %s\n",
                           time_str, get_level_str(level), uid,
                           file_name, line, tmp);
    delete[] time_str;

    if (level == FATAL) {
      this->write_to_log(log_str, str_size);
      if (this->fatal_handling) {
        pthread_mutex_lock(&(this->mutex));
        fprintf(logging::log->outfp, "\n*** FATAL Error detected; stack trace: ***\n");
        size_t size = 0;
        char **str = get_stack_trace(&size);
        for (size_t i = 0; i < size; ++i) {
          fprintf(this->outfp, "@\t%s\n", str[i]);
        }
        free(str);
        pthread_mutex_unlock(&(this->mutex));

        logging::stop_logging();
        exit(EXIT_STATUS_FATAL);
      }
    } else if (log_buf_size + str_size >= LOG_BUF_SIZE ||
               level == ERROR) {
      this->write_to_log(log_str, str_size);
    } else {
      // Buffer the log messages if not required instantly.
      pthread_mutex_lock(&(this->mutex));
      memcpy(this->log_buf + this->log_buf_size, log_str, str_size);
      this->log_buf_size += str_size;
      pthread_mutex_unlock(&(this->mutex));
    }
  }
}


/*
 * Runner thread that flushes the log buffer
 * every 5 minutes.
 */
void *
logging::Runner(void *arg)
{
  while (1) {
    sleep(300);     // Sleep for 5 minutes.

    pthread_mutex_lock(&(logging::log->mutex));
    if (logging::log->kill_runner) {
      pthread_mutex_unlock(&(logging::log->mutex));
      return NULL;
    }
    pthread_mutex_unlock(&(logging::log->mutex));

    logging::log->flush_buffer();
  }

  return NULL;
}


// Get the logging severity level in string form.
const char *
logging::get_level_str(logging::log_level_t level)
{
  if (level >= 0 && level < TOTAL_LOG_LEVELS) {
    return log_level_str[level];
  }
  return NULL;
}


// Get the current thread id.
uint16_t
logging::Log::get_thread_id(void)
{
  return pthread_self();
}


// Get the current stack trace.
char **
logging::get_stack_trace(size_t *size)
{
  void *arr[STACK_TRACE_LIMIT];
  *size = backtrace(arr, STACK_TRACE_LIMIT);
  char **str  = backtrace_symbols(arr, *size);
  return str;
}


// Detect SIGSEGV and log stack trace.
void
logging::detect_sigsegv(int sig_no)
{
  // Get the current time.
  char * time_str = new char[TIME_BUF_SIZE];
  get_current_time(&time_str);

  logging::log->destroy_runner();
  logging::log->flush_buffer();

  // Get the stack trace, and log it.
  size_t size = 0;
  char ** str = logging::get_stack_trace(&size);
  pthread_mutex_lock(&(logging::log->mutex));
  fprintf(logging::log->outfp, "\n*** Aborted at %s ***\n", time_str);
  fprintf(logging::log->outfp, "*** SIGSEGV received by PID %d; stack trace: ***\n",
                               logging::log->get_thread_id());
  for (size_t i = 0; i < size; ++i) {
    fprintf(logging::log->outfp, "@\t%s\n", str[i]);
  }
  free(str);
  delete[] time_str;
  pthread_mutex_unlock(&(logging::log->mutex));

  logging::stop_logging();
  exit(EXIT_STATUS_SIGSEGV);
}


void
logging::sigusr1_handler(int sig_no)
{

}

// Check to see whether the log buffer is empty or not.
bool
logging::is_log_buf_empty(void)
{
  if (logging::log->log_buf_size > 0) {
    return false;
  } else {
    return true;
  }
}


// Check to see whether a string is in log buffer.
bool
logging::str_in_log_buf(const char * str)
{
  if (strstr(logging::log->log_buf, str)) {
    return true;
  } else {
    return false;
  }
}
