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


#ifdef INSERT_STATEMENT
#undef INSERT_STATEMENT
#endif

#ifdef INSERT_ERROR
#undef INSERT_ERROR
#endif
#define INSERT_STATEMENT    "INSERT INTO frequencydata (frequency) VALUES (?);"
#define INSERT_ERROR        "INSERT INTO frequencydata (frequency) " \
                            "VALUES (%f);"
#define MAX_BUF_SIZE 42
#define MAX_SQL_ERROR_ARGS 1
static int isRunning = 0;

void doExitStatement(MYSQL *conn, ...) {

    va_list ptr;
    va_start(ptr, conn);
    int max = va_arg(ptr, int);

    if (max != MAX_SQL_ERROR_ARGS) {
        fprintf(stderr,
                "WARNING: Incorrect number of variadic parameters passed to correlate_frequency::doExitStatement\n"
                "Expected: %d Got: %d", MAX_SQL_ERROR_ARGS, max);
    }else{
        const float frequency = *va_arg(ptr, float*);

        fprintf(stderr, INSERT_ERROR, frequency);
    }
    doExit(conn);
}

void writeToDatabase(const void *buf, size_t nbyte) {

    const time_t startTime = time(NULL);
    int status;

    MYSQL_BIND bind[1];
    MYSQL_STMT *stmt;
    MYSQL *conn;

    conn = initializeMySqlConnection(bind);

    stmt = generateMySqlStatment(conn, &status);
    if (status != 0) {
        doExitStatement(conn, buf);
    }

    bind[0].buffer_type = MYSQL_TYPE_DECIMAL;
    bind[0].buffer = (float *) &buf;
    bind[0].length = 0;
    bind[0].is_null = 0;

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
}

void *run(void *ctx) {
    static char *portname = "$PWD/db-out";
    struct thread_args *args = (struct thread_args *) ctx;
    pthread_t pid = args->pid;
    free(ctx);

    while (isRunning) {
        int fd = open(portname, O_RDONLY | O_NOCTTY | O_SYNC);
        char buf[MAX_BUF_SIZE];

        int nbyte = read(fd, buf, MAX_BUF_SIZE);
        OUTPUT_DEBUG_STDERR(stderr, "frequency: %f size: %d", (float) *buf, nbyte);
//        writeToDatabase(buf, nbyte);
    }

    pthread_exit(&pid);
}

void onExit(void) {
    isRunning = 0;
    onExitSuper();
}

int main(void) {
    pthread_t pid = 0;
    struct thread_args *args = malloc(sizeof(struct thread_args));
//    args->buf = malloc(MAX_BUF_SIZE * sizeof(char));
//    args->nbyte = MAX_BUF_SIZE;
    args->pid = pid;

    atexit(onExit);

//    memcpy((char *) args->buf, buf, nbyte);

    pthread_create(&args->pid, NULL, run, (void *) args);
    pthread_detach(pid);

    isRunning = 1;
}
