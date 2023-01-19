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

#define SEM_RESOURCES 8
#define LENGTH_OF(arr) (sizeof(arr) / sizeof(*(arr)))
#define SIX_DAYS_IN_SECONDS 518400
#define INSERT_FREQUENCY_INFO "INSERT INTO frequencydata (frequency) VALUES (%s);"
#define INSERT_FREQUENCY "insert into frequencydata (`frequency`) values (?) on duplicate key update `date_modified`=NOW();"
#define UPDATE_FREQUENCY "update LOW_PRIORITY `imbedata` set `date_decoded`=?, `frequency`=? where `date_recorded`=?;"
#define UPDATE_FREQUENCY_INFO "UPDATE imbedata SET date_decoded=%s, frequency=%s WHERE date_recorded=%s;"
#define INSERT_DATA "INSERT INTO imbedata (date_recorded, data) VALUES (?, ?);"
#define INSERT_INFO "INSERT INTO imbedata (date_recorded, data) VALUES (%s, (data of size: %zu));"

struct insertArgs {
    void *buf;
    size_t nbyte;
    pthread_t pid;
};

struct updateArgs {
    char *frequency;
    struct tm *timeinfo;
    unsigned long nbyte;
    pthread_t pid;
};

/* util functions */
//void doExit(MYSQL *conn);
//
//void onSignal(int sig);
//
//void initializeSignalHandlers();
//
//char *getEnvVarOrDefault(char *name, char *def);
//
//void initializeEnv();
//
//MYSQL *initializeMySqlConnection(MYSQL_BIND *bind);
//
//MYSQL_TIME *generateMySqlTimeFromTm(const struct tm *timeinfo);
//
//MYSQL_STMT *generateMySqlStatment(char *statement, MYSQL *conn, int *status, long size);
//
//void *startUpdatingFrequency(void *argv);
//
//void writeUpdate(char *frequency, struct tm *timeinfo, unsigned long nbyte);
//
void writeInsertToDatabase(void *buf, size_t nbyte);
#endif //UTILS_H

