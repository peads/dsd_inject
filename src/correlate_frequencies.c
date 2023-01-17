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
// Created by Patrick Eads on 1/16/23.
//
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include "utils.h"

#define BUF_SIZE 42
static int isRunning = 0;

void writeToDatabase(const void *buf, size_t nbyte) {

    const time_t startTime = time(NULL);
    int status;

    MYSQL_BIND bind[2];
    MYSQL_STMT *stmt;
    MYSQL *conn;
    MYSQL_TIME *dateDecoded;

    dateDecoded = generateMySqlTime(&startTime);

    conn = initializeMySqlConnection(bind);

    stmt = generateMySqlStatment(conn, &status);
    if (status != 0) {
        doExitStatement(conn, startTime, nbyte);
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
        doExitStatement(conn, startTime, nbyte);
    }
    status = mysql_stmt_execute(stmt);
    if (status != 0) {
        doExitStatement(conn, startTime, nbyte);
    }

    mysql_stmt_close(stmt);
    mysql_close(conn);

    free((void *) dateDecoded);
}

void *run(void *ctx) {
    static char *portname = "$PWD/db-out";
    struct thread_args *args = (struct thread_args *) ctx;
    const pthread_t pid = args->pid;
    free(ctx);

    while (isRunning) {
        int fd = open(portname, O_RDONLY | O_NOCTTY | O_SYNC);
        char buf[BUF_SIZE];

        int n = read(fd, buf, sizeof buf);
        writeToDatabase(buf, BUF_SIZE);
    }

    pthread_exit(pid);
}

int main(void) {
    pthread_t pid = 0;
    struct thread_args *args = malloc(sizeof(struct thread_args));
//    args->buf = malloc(BUF_SIZE * sizeof(char));
//    args->nbyte = BUF_SIZE;
    args->pid = pid;

//    memcpy((char *) args->buf, buf, nbyte);

    pthread_create(&args->pid, NULL, run, (void *) args);
    pthread_detach(pid);

    isRunning = 1;
}
