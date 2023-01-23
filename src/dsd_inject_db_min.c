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

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "global.h"

extern void initializeEnv();
extern void *runInsertThread(void *ctx);
extern void addPid(pthread_t pid);

static ssize_t (*next_write)(int fildes, const void *buf, size_t nbyte, off_t offset) = NULL;

ssize_t write(int fildes, const void *buf, size_t nbyte, off_t offset) {
    pthread_t pid = 0;
    if (NULL == next_write) {
        initializeEnv();

        fprintf(stderr, "%s", "wrapping write\n");
        next_write = dlsym(RTLD_NEXT, "write");
        const char *msg = dlerror();

        fprintf(stderr, "%s", "setting atexit\n");

        if (msg != NULL) {
            fprintf(stderr, "\nwrite: dlwrite failed: %s::Exiting\n", msg);
            exit(-1);
        }
    }

    struct insertArgs *args = malloc(sizeof(struct insertArgs)); 
    args->buf = malloc(nbyte * sizeof(char) + 1);
    args->nbyte = nbyte;
    pid = 0;
    memcpy(args->buf, buf, nbyte);
    pthread_create(&pid, NULL, runInsertThread, args);
    addPid(pid);

    return next_write(fildes, buf, nbyte, offset);
}

