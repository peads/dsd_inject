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
#define SEM_RESOURCES 128

#include "utils.h"

#define INSERT_DATA "INSERT INTO imbedata (date_recorded, data) VALUES (?, ?);"
#define INSERT_INFO "INSERT INTO imbedata (date_recorded, data) VALUES (%s, (data of size: %zu));"

extern const char *db_pass;
extern const char *db_host;
extern const char *db_user;
extern const char *schema;

void writeInsertToDatabase(void *buf, size_t nbyte) {

    const time_t insertTime = time(NULL);
    int status;

    MYSQL_BIND bind[2];
    MYSQL_STMT *stmt;
    MYSQL *conn;
    MYSQL_TIME *dateDecoded;

    struct tm *timeinfo;
    timeinfo = localtime(&insertTime);
    dateDecoded = generateMySqlTimeFromTm(timeinfo);

    conn = initializeMySqlConnection(bind);

    char buffer[26];
    strftime(buffer, 26, "%Y-%m-%dT%H:%M:%S%:%z", timeinfo);
    fprintf(stderr, INSERT_INFO "\n", buffer, nbyte);

    stmt = generateMySqlStatment(INSERT_DATA, conn, &status, 57);
    if (status != 0) {
        doExit(conn);
    }

    bind[0].buffer_type = MYSQL_TYPE_DATETIME;
    bind[0].buffer = (char *) dateDecoded;
    bind[0].length = 0;
    bind[0].is_null = 0;

    bind[1].buffer_type = MYSQL_TYPE_BLOB;
    bind[1].buffer = (char *) &buf;
    bind[1].buffer_length = nbyte;
    bind[1].length = &nbyte;
    bind[1].is_null = 0;

    status = mysql_stmt_bind_param(stmt, bind);
    if (status != 0) {
        doExit(conn);
    }
    status = mysql_stmt_execute(stmt);
    if (status != 0) {
        doExit(conn);
    }

    mysql_stmt_close(stmt);
    mysql_close(conn);

    time_t idx = insertTime - updateStartTime;
    struct updateArgs *dbArgs = updateHash[idx >= 0 ? (idx % SIX_DAYS_IN_SECONDS) : 0];

    strftime(buffer, 26, "%Y-%m-%dT%H:%M:%S%:%z\n", dbArgs->timeinfo);
    fprintf(stderr, "Using function pointer to call update with time: %s", buffer);

    (dbArgs->write)(dbArgs->frequency, dbArgs->timeinfo, dbArgs->nbyte);

    free((void *) dateDecoded);
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
        pthread_t pid = 0;
        struct insertArgs *args = malloc(sizeof(struct insertArgs));
        args->buf = "/home/peads/dsd_inject/read_rtl_fm_loop.sh";
        args->pid = pid;

        pthread_create(&args->pid, NULL, startUpdatingFrequency, (void *) args);
        pthread_detach(pid);
    }

    pthread_t pid = 0;
    struct insertArgs *args = malloc(sizeof(struct insertArgs));
    args->buf = malloc(nbyte * sizeof(char));
    args->nbyte = nbyte;
    args->pid = pid;

    memcpy((char *) args->buf, buf, nbyte);

    pthread_create(&args->pid, NULL, run, (void *) args);
    pthread_detach(pid);

    return next_write(fildes, buf, nbyte, offset);
}
