/*
 * This file is part of the dsd_inject distribution (https://github.com/peads/dsd_inject).
 * Copyright (c) 2023 Patrick Eads.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
//
// Created by Patrick Eads on 1/11/23.
//

#define _GNU_SOURCE

#include <mysql.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <semaphore.h>

#if defined(__USE_XOPEN_EXTENDED) || defined(__USE_MISC)
#undef __USE_MISC
#undef __USE_XOPEN_EXTENDED
#endif

#include <signal.h>

#ifndef INJECT_H
#define INJECT_H

#ifndef TRACE
#define OUTPUT_DEBUG_STDERR(file, msg, subs)  //
#else
#define OUTPUT_DEBUG_STDERR(file, msg, subs)  fprintf(file, msg, subs)
#endif

#if !(defined(DEBUG) || defined(TRACE))
#define OUTPUT_INFO_STDERR(file, msg, subs)  //
#else
#define OUTPUT_INFO_STDERR(file, msg, subs)  fprintf(file, msg, subs)
#endif

#define LENGTH_OF(arr) (sizeof(arr) / sizeof(*(arr)))
#define INSERT_STATEMENT    "INSERT INTO imbedata (date_decoded, data) VALUES (?, ?);"
#define INSERT_ERROR        "INSERT INTO imbedata (date_decoded, data) " \
                            "VALUES (%zu, (data of size: %zu));"

struct thread_args {
    void *buf;
    size_t nbyte;
    pthread_t pid;
};

static ssize_t (*next_write)(int fildes, const void *buf, size_t nbyte, off_t offset) = NULL;
static sem_t sem;

/* main functions */
void *run(void *ctx);
void writeToDatabase(const void *buf, size_t nbyte);
ssize_t write(int fildes, const void *buf, size_t nbyte, off_t offset);

/* util functions */
void doExit(MYSQL *con);
void doExitStatement(MYSQL *conn, time_t date, size_t size);
void onSignal(int sig);
void initializeSignalHandlers();
char *getEnvVarOrDefault(char *name, char *def);
void initializeEnv();
void onExit(void);
MYSQL *initializeMySqlConnection(MYSQL_BIND *bind);
MYSQL_TIME *generateMySqlTime(const time_t *t);
MYSQL_STMT *generateMySqlStatment(MYSQL *conn, int *status);

#endif //INJECT_H
