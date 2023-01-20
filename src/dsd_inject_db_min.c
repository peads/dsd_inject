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
extern struct updateArgs *updateHash[];
extern int isRunning;
extern FILE *fd;

const char *db_pass;
const char *db_host;
const char *db_user;
const char *schema;

static sem_t sem;
static ssize_t (*next_write)(int fildes, const void *buf, size_t nbyte, off_t offset) = NULL;

void onSignal(int sig) {

    fprintf(stderr, "\n\nCaught Signal: %s\n", strsignal(sig));

    exit(-1);
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

void *run(void *ctx) {

    struct insertArgs *args = (struct insertArgs *) ctx;

    sem_wait(&sem);

    writeInsertToDatabase(args->buf, args->nbyte);

    sem_post(&sem);

    free(args->buf);
    free(ctx);

    pthread_exit(&args->pid);
}

static void onExit(void) {
    int status;
    isRunning = 0;

    if ((status = sem_close(&sem)) != 0) {
        fprintf(stderr, "unable to unlink semaphore. status: %s\n", strerror(status));
    } else {
        fprintf(stderr, "%s", "semaphore destroyed\n");
    }

    if ((status = pclose(fd)) != 0) {
        fprintf(stderr, "Error closing awk script. status %s\n", strerror(status));
    }

    next_write = NULL;
}

void *startUpdatingFrequency(void *ctx) {

    if (isRunning) {
        return NULL;
    }

    isRunning = 1;
    struct insertArgs *args = (struct insertArgs *) ctx;
    char *portname = args->buf;
    unsigned long size = 6 + strchr(portname, '\0') - ((char *) portname);
    char cmd[size];
    strcpy(cmd, portname);
    strcat(cmd, " 2>&1");
    OUTPUT_DEBUG_STDERR(stderr, "%s size: %lu\n", cmd, size);

    fd = popen(cmd, "r");
    if (fd == NULL) {
        exit(-1);
    }

    free(ctx);
    int ret;

    do {
        struct updateArgs *args = malloc(sizeof(struct updateArgs));
        args->frequency = malloc(8 * sizeof(char));
        args->timeinfo = malloc(sizeof(struct tm));

        struct tm *timeinfo = args->timeinfo;
        int *year = malloc(sizeof(int *));
        int *month = malloc(sizeof(int *));
        int *mantissa = malloc(sizeof(int *));
        int *characteristic = malloc(sizeof(int *));
        int *tzHours = malloc(sizeof(int));
        int *tzMin = malloc(sizeof(int));

        ret = fscanf(fd, "%d-%d-%dT%d:%d:%d+%d:%d;%d.%d\n",
                     year,
                     month,
                     &timeinfo->tm_mday,
                     &timeinfo->tm_hour,
                     &timeinfo->tm_min,
                     &timeinfo->tm_sec,
                     tzHours,
                     tzMin,
                     characteristic,
                     mantissa);

        OUTPUT_DEBUG_STDERR(stderr, "vars set: %d\n", ret);
        
        if (ret != 10) {

            timeinfo->tm_year = *year - 1900;
            timeinfo->tm_mon = *month - 1;
            timeinfo->tm_isdst = 0;

            char frequency[8];
            sprintf(frequency, "%d.%d", *characteristic, *mantissa);

            OUTPUT_DEBUG_STDERR(stderr, "Size of string: %ld", 1 + strchr(frequency, '\0') - frequency);
            //unsigned long nbyte = 8;

            unsigned long last = strchr(frequency, '\0') - frequency;
            unsigned long nbyte = 1 + last;
            char freq[nbyte];
            strcpy(freq, frequency);
            freq[last] = '\0';
            args->nbyte = nbyte;
            args->frequency = freq;

                time_t idx = (mktime(timeinfo) - updateStartTime) % SIX_DAYS_IN_SECONDS;
                struct updateArgs *dbArgs = updateHash[idx];
                if (dbArgs != NULL) {
                    //writeUpdate(freq, timeinfo, nbyte); 
                } else {
                    char buffer[26];
                    strftime(buffer, 26, "%Y-%m-%dT%H:%M:%S%:%z\n", timeinfo);
                    fprintf(stderr, 
                        "inject::startUpdatingFrequency Struct added to hash at: %ld, %s\n", idx, buffer);
                    updateHash[idx] = args;
                }
            } else {
                free(year);
                free(month);
                free(mantissa);
                free(characteristic);
                free(tzHours);
                free(tzMin);
                free(args->timeinfo);
                free(args);
            }
    } while (isRunning && ret != EOF);

    pthread_exit(&args->pid);
}

ssize_t write(int fildes, const void *buf, size_t nbyte, off_t offset) {

    if (NULL == next_write) {
        initializeEnv();
        initializeSignalHandlers();
        sem_init(&sem, 0, SEM_RESOURCES);
        
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
        struct insertArgs *uargs = malloc(sizeof(struct insertArgs));
        uargs->buf = "/home/peads/dsd_inject/read_rtl_fm_loop.sh";
        uargs->pid = upid;

        pthread_create(&uargs->pid, NULL, startUpdatingFrequency, (void *) uargs);
        pthread_detach(upid);
    }

    pthread_t pid = 0;
    struct insertArgs *args = malloc(sizeof(struct insertArgs));
    args->buf = malloc(nbyte * sizeof(char));
    args->nbyte = nbyte;

    memcpy(args->buf, buf, nbyte);

    pthread_create(&pid, NULL, run, (void *) args);
    pthread_detach(pid);
    args->pid = pid;

    return next_write(fildes, buf, nbyte, offset);
}
