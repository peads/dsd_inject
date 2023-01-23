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
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#if defined(__USE_XOPEN_EXTENDED) || defined(__USE_MISC)
#undef __USE_MISC
#undef __USE_XOPEN_EXTENDED
#endif

#include <signal.h>
#include "utils.h"

extern sem_t sem;
extern sem_t sem1;
extern int isRunning;

extern const char *db_pass;
extern const char *db_host;
extern const char *db_user;
extern const char *schema;

extern void *runFrequencyUpdatingThread(void *ctx);
extern void initializeEnv();
extern void *runInsertThread(void *ctx);

static ssize_t (*next_write)(int fildes, const void *buf, size_t nbyte, off_t offset) = NULL;

void onExit(void) {
    int status;
    isRunning = 0;

    if ((status = sem_close(&sem)) != 0) {
        fprintf(stderr, "unable to unlink semaphore. status: %s\n", strerror(status));
    } else {
        fprintf(stderr, "%s", "semaphore destroyed\n");
    }
    
    if ((status = sem_close(&sem1)) != 0) {
        fprintf(stderr, "unable to unlink semaphore. status: %s\n", strerror(status));
    } else {
        fprintf(stderr, "%s", "semaphore destroyed\n");
    }

    next_write = NULL;
}
void onSignal(int sig) {

    fprintf(stderr, "\n\nCaught Signal: %s\n", strsignal(sig));
    if (SIGUSR1 != sig && SIGUSR2 != sig) {
        exit(-1);
    }
}

void initializeSignalHandlers() {

    fprintf(stderr, "%s", "initializing signal handlers");

    struct sigaction *sigInfo = malloc(sizeof(*sigInfo));
    struct sigaction *sigHandler = malloc(sizeof(*sigHandler));

    sigHandler->sa_handler = onSignal;
    sigemptyset(&(sigHandler->sa_mask));
    sigHandler->sa_flags = 0;

    int sigs[] = {SIGABRT, SIGALRM, SIGFPE, SIGHUP, SIGILL,
                  SIGINT, SIGPIPE, SIGQUIT, SIGSEGV, SIGTERM,
                  SIGUSR1, SIGUSR2};

    int i = 0;
    for (; i < (int) LENGTH_OF(sigs); ++i) {
        const int sig = sigs[i];
        const char *name = strsignal(sig);

        fprintf(stderr, "Initializing signal handler for signal: %s\n", name);

        sigaction(sig, NULL, sigInfo);
        const int res = sigaction(sig, sigHandler, sigInfo);

        if (res == -1) {
            fprintf(stderr, "\n\nWARNING: Unable to initialize handler for %s\n\n", name);
            exit(-1);
        } else {
            fprintf(stderr, "Successfully initialized signal handler for signal: %s\n", name);
        }
    }

    free(sigInfo);
    free(sigHandler);
}

ssize_t write(int fildes, const void *buf, size_t nbyte, off_t offset) {
    pthread_t pid = 0;
    if (NULL == next_write) {
        initializeEnv();
        initializeSignalHandlers();
 
        fprintf(stderr, "%s", "wrapping write\n");
        next_write = dlsym(RTLD_NEXT, "write");
        const char *msg = dlerror();

        fprintf(stderr, "%s", "setting atexit\n");
        atexit(onExit);

        if (msg != NULL) {
            fprintf(stderr, "\nwrite: dlwrite failed: %s::Exiting\n", msg);
            exit(-1);
        }

        pthread_t pid = 0;
        const char fileDes[] = "/home/peads/fm-err-out";
        pthread_create(&pid, NULL, runFrequencyUpdatingThread, (void *) fileDes);
        pthread_detach(pid);
    }

    struct insertArgs *args = malloc(sizeof(struct insertArgs)); 
    args->buf = malloc(nbyte * sizeof(char) + 1);
    args->nbyte = nbyte;
    pid = 0;
    memcpy(args->buf, buf, nbyte);
    pthread_create(&pid, NULL, runInsertThread, args);

    return next_write(fildes, buf, nbyte, offset);
}

