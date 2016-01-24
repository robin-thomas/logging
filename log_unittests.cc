
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



#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "log.h"

#define RED   "\x1B[31m"
#define GREEN "\x1B[32m"
#define RESET "\033[0m"

int main() {

  const char * log_file = "/tmp/log_test";
  int pass_count = 0;
  int fail_count = 0;

  fprintf(stderr, "\n");
  fprintf(stderr, "Logging Tests\n*************\n\n");

  // Testing to see init_logging should be called only once.
  {
    int out_pipe[2];
    int stderr_bk = dup(STDERR_FILENO);
    if (pipe(out_pipe)) {
      fprintf(stderr, RED "[FAIL]" RESET " Failed to create pipe.\n");
      exit(1);
    }
    dup2(out_pipe[1], STDERR_FILENO);
    close(out_pipe[1]);
    int flags = fcntl(STDERR_FILENO, F_GETFL, 0);
    fcntl(STDERR_FILENO, F_SETFL, flags | O_NONBLOCK);

    logging::init_logging("", logging::INFO);
    logging::init_logging("", logging::INFO);
    logging::stop_logging();

    char buf[LOG_BUF_SIZE];
    memset(buf, 0, LOG_BUF_SIZE);
    size_t len = read(out_pipe[0], buf, LOG_BUF_SIZE);
    dup2(stderr_bk, STDERR_FILENO);
    close(out_pipe[0]);

    if (len > 0) {
      fprintf(stderr, GREEN "[PASS]" RESET " Checking init_logging() called only once.\n");
      ++pass_count;
    } else {
      fprintf(stderr, RED "[FAIL]" RESET " Checking init_logging() called only once.\n");
      ++fail_count;
    }
  }

  // Testing to see stop_logging should be called only once.
  {
    int out_pipe[2];
    int stderr_bk = dup(STDERR_FILENO);
    if (pipe(out_pipe)) {
      fprintf(stderr, RED "[FAIL]" RESET " Failed to create pipe.\n");
      exit(1);
    }
    dup2(out_pipe[1], STDERR_FILENO);
    close(out_pipe[1]);

    logging::init_logging("", logging::INFO);
    logging::stop_logging();
    logging::stop_logging();

    char buf[LOG_BUF_SIZE];
    memset(buf, 0, LOG_BUF_SIZE);
    size_t len = read(out_pipe[0], buf, LOG_BUF_SIZE);
    dup2(stderr_bk, STDERR_FILENO);

    if (len > 0) {
      fprintf(stderr, GREEN "[PASS]" RESET " Checking stop_logging() called only once.\n");
      ++pass_count;
    } else {
      fprintf(stderr, RED "[FAIL]" RESET " Checking stop_logging() called only once.\n");
      ++fail_count;
    }
  }

  // Testing to see init_logging to be called before stop_logging.
  {
    int out_pipe[2];
    int stderr_bk = dup(STDERR_FILENO);
    if (pipe(out_pipe)) {
      fprintf(stderr, RED "[FAIL]" RESET " Failed to create pipe.\n");
      exit(1);
    }
    dup2(out_pipe[1], STDERR_FILENO);
    close(out_pipe[1]);

    logging::stop_logging();

    char buf[LOG_BUF_SIZE];
    memset(buf, 0, LOG_BUF_SIZE);
    size_t len = read(out_pipe[0], buf, LOG_BUF_SIZE);
    dup2(stderr_bk, STDERR_FILENO);

    if (len > 0) {
      fprintf(stderr, GREEN "[PASS]" RESET " Checking init_logging() called before stop_logging().\n");
      ++pass_count;
    } else {
      fprintf(stderr, RED "[FAIL]" RESET " Checking init_logging() called before stop_logging().\n");
      ++fail_count;
    }
  }

  // Testing to see init_logging to be called before any logging.
  {
    const char * err_str = "Testing error";
    int out_pipe[2];
    int stderr_bk = dup(STDERR_FILENO);
    if (pipe(out_pipe)) {
      fprintf(stderr, RED "[FAIL]" RESET " Failed to create pipe.\n");
      exit(1);
    }
    dup2(out_pipe[1], STDERR_FILENO);
    close(out_pipe[1]);

    Error(err_str);

    char buf[LOG_BUF_SIZE];
    memset(buf, 0, LOG_BUF_SIZE);
    read(out_pipe[0], buf, LOG_BUF_SIZE);
    dup2(stderr_bk, STDERR_FILENO);

    if (strstr(buf, err_str)) {
      fprintf(stderr, RED "[FAIL]" RESET " Checking init_logging() called before logging.\n");
      ++fail_count;
    } else {
      fprintf(stderr, GREEN "[PASS]" RESET " Checking init_logging() called before logging.\n");
      ++pass_count;
    }
  }

  // Testing to see redirecting to stderr.
  {
    const char * info_str = "Testing info to stderr";
    int out_pipe[2];
    int stderr_bk = dup(STDERR_FILENO);
    if (pipe(out_pipe)) {
      fprintf(stderr, RED "[FAIL]" RESET " Failed to create pipe.\n");
      exit(1);
    }
    dup2(out_pipe[1], STDERR_FILENO);
    close(out_pipe[1]);

    logging::init_logging("", logging::INFO);
    Info(info_str);
    logging::stop_logging();

    char buf[LOG_BUF_SIZE];
    memset(buf, 0, LOG_BUF_SIZE);
    read(out_pipe[0], buf, LOG_BUF_SIZE);
    dup2(stderr_bk, STDERR_FILENO);

    if (strstr(buf, info_str)) {
      fprintf(stderr, GREEN "[PASS]" RESET " Checking logging redirect to stderr.\n");
      ++pass_count;
    } else {
      fprintf(stderr, RED "[FAIL]" RESET " Checking logging redirect to stderr.\n");
      ++fail_count;
    }
  }

  // Testing to see log file is created and writable.
  {
    logging::init_logging(log_file, logging::INFO);
    Info("Testing info");
    logging::stop_logging();

    struct stat st;
    if (stat(log_file, &st) == 0) {
      if (st.st_size > 0) {
        fprintf(stderr, GREEN "[PASS]" RESET " Checking log creation and writability.\n");
        ++pass_count;
      } else {
        fprintf(stderr, RED "[FAIL]" RESET " Checking log creation and writability.\n");
        ++fail_count;
      }
      remove(log_file);
    } else {
      fprintf(stderr, RED "[FAIL]" RESET " Checking log creation and writability.\n");
      ++fail_count;
    }
  }

  // Testing to see logging severity levels.
  {
    const char * str = "Testing info to stderr";
    int out_pipe[2];
    int stderr_bk = dup(STDERR_FILENO);
    if (pipe(out_pipe)) {
      fprintf(stderr, RED "[FAIL]" RESET " Failed to create pipe.\n");
      exit(1);
    }
    dup2(out_pipe[1], STDERR_FILENO);
    close(out_pipe[1]);

    logging::init_logging("", logging::FATAL);
    Error(str);
    Warning(str);
    Info(str);
    Debug(str);
    logging::stop_logging();

    char buf[LOG_BUF_SIZE];
    memset(buf, 0, LOG_BUF_SIZE);
    read(out_pipe[0], buf, LOG_BUF_SIZE);
    dup2(stderr_bk, STDERR_FILENO);

    if (strstr(buf, str)) {
      fprintf(stderr, RED "[FAIL]" RESET " Checking logging severity levels.\n");
      ++fail_count;
    } else {
      fprintf(stderr, GREEN "[PASS]" RESET " Checking logging severity levels.\n");
      ++pass_count;
    }
  }

  // Testing to see SIGSEGV is catched and logged successfully.
  {
    pid_t child = fork();
    if (child == 0) {
      logging::init_logging(log_file, logging::INFO);
      raise(SIGSEGV);
      exit(1);
    } else {
      int status;
      waitpid(child, &status, 0);
      if (WEXITSTATUS(status) != EXIT_STATUS_SIGSEGV) {
        fprintf(stderr, RED "[FAIL]" RESET " Checking SIGSEGV signal handling.\n");
        ++fail_count;
      } else {
        struct stat st;
        if (stat(log_file, &st) == 0 && st.st_size > 0) {
          fprintf(stderr, GREEN "[PASS]" RESET " Checking SIGSEGV signal handling.\n");
          ++pass_count;
        } else {
          fprintf(stderr, RED "[FAIL]" RESET " Checking SIGSEGV signal handling.\n");
          ++fail_count;
        }
        remove(log_file);
      }
    }
  }

  // Testing to see switch to turn OFF SIGSEGV handling.
  {
    pid_t child = fork();
    if (child == 0) {
      logging::init_logging(log_file, logging::INFO, false);
      raise(SIGSEGV);
      exit(1);
    } else {
      int status;
      waitpid(child, &status, 0);
      if (WEXITSTATUS(status) != EXIT_STATUS_SIGSEGV) {
        fprintf(stderr, GREEN "[PASS]" RESET " Checking switch to turn OFF SIGSEGV handling.\n");
        ++pass_count;
      } else {
        fprintf(stderr, RED "[FAIL]" RESET " Checking switch to turn OFF SIGSEGV handling.\n");
        ++fail_count;
      }
      remove(log_file);
    }
  }

  // Testing to see FATAL log messages are logged and exited.
  {
    pid_t child = fork();
    if (child == 0) {
      logging::init_logging(log_file, logging::INFO);
      Fatal("Testing fatal");
      exit(1);
    } else {
      int status;
      waitpid(child, &status, 0);
      if (WEXITSTATUS(status) != EXIT_STATUS_FATAL) {
        fprintf(stderr, RED "[FAIL]" RESET " Checking FATAL message logging.\n");
        ++fail_count;
      } else {
        struct stat st;
        if (stat(log_file, &st) == 0 && st.st_size > 0) {
          fprintf(stderr, GREEN "[PASS]" RESET " Checking FATAL message logging.\n");
          ++pass_count;
        } else {
          fprintf(stderr, RED "[FAIL]" RESET " Checking FATAL message logging.\n");
          ++fail_count;
        }
        remove(log_file);
      }
    }
  }

  // Testing to see switch to turn OFF FATAL log exiting.
  {
    pid_t child = fork();
    if (child == 0) {
      logging::init_logging(log_file, logging::FATAL,
                            false, false);
      Fatal("Testing fatal");
      logging::stop_logging();
      exit(1);
    } else {
      int status;
      waitpid(child, &status, 0);
      if (WEXITSTATUS(status) == EXIT_STATUS_FATAL) {
        fprintf(stderr, RED "[FAIL]" RESET " Checking switch to turn OFF FATAL log handling.\n");
        ++fail_count;
      } else {
        fprintf(stderr, GREEN "[PASS]" RESET " Checking switch to turn OFF FATAL log handling.\n");
        ++pass_count;
      }
      remove(log_file);
    }
  }

  // Testing for conditional logging.
  {
    const char * info_str_1 = "Testing info 1";
    const char * info_str_2 = "Testing info 2";
    int out_pipe[2];
    int stderr_bk = dup(STDERR_FILENO);
    if (pipe(out_pipe)) {
      fprintf(stderr, RED "[FAIL]" RESET " Failed to create pipe.\n");
      exit(1);
    }
    dup2(out_pipe[1], STDERR_FILENO);
    close(out_pipe[1]);

    logging::init_logging("", logging::INFO);
    LOG_IF(logging::INFO, false, info_str_1);
    LOG_IF(logging::INFO, true, info_str_2);
    logging::stop_logging();

    char buf[LOG_BUF_SIZE];
    memset(buf, 0, LOG_BUF_SIZE);
    read(out_pipe[0], buf, LOG_BUF_SIZE);
    dup2(stderr_bk, STDERR_FILENO);

    if (!strstr(buf, info_str_1) &&
        strstr(buf, info_str_2)) {
      fprintf(stderr, GREEN "[PASS]" RESET " Checking conditional logging.\n");
      ++pass_count;
    } else {
      fprintf(stderr, RED "[FAIL]" RESET " Checking conditional logging.\n");
      ++fail_count;
    }
  }

  // Testing for buffered logging.
  {
    const char * info_str = "Testing info";
    int out_pipe[2];
    int stderr_bk = dup(STDERR_FILENO);
    if (pipe(out_pipe)) {
      fprintf(stderr, RED "[FAIL]" RESET " Failed to create pipe.\n");
      exit(1);
    }
    dup2(out_pipe[1], STDERR_FILENO);
    close(out_pipe[1]);

    logging::init_logging("", logging::INFO);
    Info(info_str);
    bool check = logging::str_in_log_buf(info_str);

    char buf[LOG_BUF_SIZE];
    memset(buf, 0, LOG_BUF_SIZE);
    read(out_pipe[0], buf, LOG_BUF_SIZE);
    logging::stop_logging();
    dup2(stderr_bk, STDERR_FILENO);

    if (check && !strstr(buf, info_str)) {
      fprintf(stderr, GREEN "[PASS]" RESET " Checking buffered logging.\n");
      ++pass_count;
    } else {
      fprintf(stderr, RED "[FAIL]" RESET " Checking buffered logging.\n");
      ++fail_count;
    }
  }


  if (fail_count == 0) {
    fprintf(stderr, GREEN "\nAll logging tests passed successfully!\n\n" RESET);
  } else {
    fprintf(stderr, RED "\n%.2f%% of the logging tests failed!\n\n" RESET, ((float) fail_count / (pass_count + fail_count)) * 100.0);
  }

  return fail_count;
}
