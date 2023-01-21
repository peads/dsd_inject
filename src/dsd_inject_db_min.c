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

#include "utils.h"

extern time_t updateStartTime;
extern FILE *fd;

const char *db_pass;
const char *db_host;
const char *db_user;
const char *schema;

extern void *insertDataThread(void *ctx);
extern void *startUpdatingFrequency(void *ctx);
extern void onExit(void);
extern ssize_t (*next_write)(int fildes, const void *buf, size_t nbyte, off_t offset);

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

char *getEnvVarOrDefault(char *name, char *def) {

    char *result = getenv(name);

    if (result) {
        return result;
    }
    return def;
}

void initializeEnv() {
    updateStartTime = time(NULL);

    fprintf(stderr, "Semaphore resources: %d", SEM_RESOURCES);

    db_pass = getenv("DB_PASS");
    if (db_pass) {
        db_host = getEnvVarOrDefault("DB_HOST", "127.0.0.1");
        db_user = getEnvVarOrDefault("DB_USER", "root");
        schema = getEnvVarOrDefault("SCHEMA", "scanner");
    } else {
        fprintf(stderr, "%s", "No database user password defined.");
        exit(-1);
    }
}

ssize_t write(int fildes, const void *buf, size_t nbyte, off_t offset) {

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

        pthread_t upid = 0;
        const char *fileDes = "$PWD/dsd_inject/read_rtl_fm_loop.sh"; //"$PWD/db-out"
        pthread_create(&upid, NULL, startUpdatingFrequency, (void *) fileDes);
        pthread_detach(upid);
    }

    pthread_t pid = 0;
    struct insertArgs *args = malloc(sizeof(struct insertArgs));
    args->buf = malloc(nbyte * sizeof(char));
    args->nbyte = nbyte;

    memcpy(args->buf, buf, nbyte);

    pthread_create(&pid, NULL, insertDataThread, (void *) args);
    pthread_detach(pid);
    args->pid = pid;

    return next_write(fildes, buf, nbyte, offset);
}
