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

#include <mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "global.h"

struct updateArgs {
    char *frequency;
    time_t t;
    unsigned long nbyte;
};

const char *db_pass;
const char *db_host;
const char *db_user;
const char *schema;

static int isRunning = 0;
static int pidCount = 0;
static sem_t sem;
static sem_t sem1;
static pthread_t pids[MAX_PIDS];

static void writeUpdate(char *frequency, time_t t, unsigned long nbyte);
static void writeFrequencyPing(char *frequency, unsigned long nbyte);
static void *runFrequencyUpdatingThread(void *ctx);
#endif //UTILS_H

