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
#ifndef UTILS_H
#define UTILS_H

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

#ifndef SEM_RESOURCES
#define SEM_RESOURCES 8
#endif

#if defined(__USE_XOPEN_EXTENDED) || defined(__USE_MISC)
#undef __USE_MISC
#undef __USE_XOPEN_EXTENDED
#endif

#include <signal.h>

#ifndef TRACE
#define OUTPUT_DEBUG_STDERR(file, msg, subs ...)  //
#else
#define OUTPUT_DEBUG_STDERR(file, msg, subs ...)  fprintf(file, msg "\n", subs)
#endif

#if !(defined(DEBUG) || defined(TRACE))
#define OUTPUT_INFO_STDERR(file, msg, subs ...)  //
#else
#define OUTPUT_INFO_STDERR(file, msg, subs ...)  fprintf(file, msg "\n", subs)
#endif

#define LENGTH_OF(arr) (sizeof(arr) / sizeof(*(arr)))

struct thread_args {
    void *buf;
    size_t nbyte;
    pthread_t pid;
};

extern sem_t sem;

/* util functions */
void doExit(MYSQL *con);

void onSignal(int sig);

void initializeSignalHandlers();

char *getEnvVarOrDefault(char *name, char *def);

void initializeEnv();

void onExitSuper(void);

MYSQL *initializeMySqlConnection(MYSQL_BIND *bind);

MYSQL_TIME *generateMySqlTime(const time_t *t);

MYSQL_TIME *generateMySqlTimeFromTm(const struct tm *timeinfo);

MYSQL_STMT *generateMySqlStatment(char *statement, MYSQL *conn, int *status, long size);

void writeToDatabase(void *buf, size_t nbyte);

#endif //UTILS_H
