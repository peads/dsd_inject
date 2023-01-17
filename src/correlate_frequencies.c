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
#define INSERT_STATEMENT    "insert into frequencydata (frequency) " \
                            "values (?) on duplicate key update `date_modified`=NOW()"
#define INSERT_ERROR        "INSERT INTO frequencydata (frequency) " \
                            "VALUES (%f);"
#define MAX_BUF_SIZE 34
#define MAX_SQL_ERROR_ARGS 1
static int isRunning = 0;

void doExitStatement(MYSQL *conn, ...) {

    va_list ptr;
    va_start(ptr, conn);
    int max = va_arg(ptr, int);

    if (max != MAX_SQL_ERROR_ARGS) {
        fprintf(stderr,
                "WARNING: Incorrect number of variadic parameters passed to correlate_frequency::doExitStatement\n"
                "Expected: %d Got: %d\n", MAX_SQL_ERROR_ARGS, max);
    } else {
        const float frequency = *va_arg(ptr, float*);
    }
    doExit(conn);
}

void writeToDatabase(void *buf, size_t nbyte) {
    OUTPUT_DEBUG_STDERR(stderr, "%s", "Entering correlate_frequencies::writeToDatabase");
    
    float *frequency = ((float *) buf);
    const time_t startTime = time(NULL);
    int status;

    MYSQL_BIND bind[1];
    MYSQL_STMT *stmt;
    MYSQL *conn;

    OUTPUT_DEBUG_STDERR(stderr, "Got %f MHz. With size %ld", *frequency, nbyte);
    OUTPUT_DEBUG_STDERR(stderr, "%s", "Initializing db connection");
    conn = initializeMySqlConnection(bind);

    OUTPUT_DEBUG_STDERR(stderr, "%s", "Generating prepared statement");
    OUTPUT_DEBUG_STDERR(stderr, INSERT_ERROR, *frequency);
    stmt = generateMySqlStatment(conn, &status);
    if (status != 0) {
        doExit(conn);
    }

    bind[0].buffer_type = MYSQL_TYPE_DECIMAL;
    bind[0].buffer = frequency;
    bind[0].length = 0;
    bind[0].is_null = 0;

    OUTPUT_DEBUG_STDERR(stderr, "%s", "Binding parameters");
    status = mysql_stmt_bind_param(stmt, bind);
    if (status != 0) {
        doExit(conn);
    }

    OUTPUT_DEBUG_STDERR(stderr, "%s", "Executing prepared statement");
    status = mysql_stmt_execute(stmt);
    if (status != 0) {
        doExit(conn);
    }

    OUTPUT_DEBUG_STDERR(stderr, "%s", "Closing statement");
    mysql_stmt_close(stmt);
    OUTPUT_DEBUG_STDERR(stderr, "%s", "Closing database connection");
    mysql_close(conn);

    OUTPUT_DEBUG_STDERR(stderr, "%s", "Entering correlate_frequencies::writeToDatabase");
}

void *run(void *ctx) {
    OUTPUT_DEBUG_STDERR(stderr, "%s", "Read thread spawned");
    struct thread_args *args = (struct thread_args *)ctx;
    char *portname = (char *)args->buf;

    OUTPUT_DEBUG_STDERR(stderr, "%s", "Fetching args");
    pthread_t pid = args->pid;
    int nbyte = 0;
    OUTPUT_DEBUG_STDERR(stderr, "Opening file: %s", portname);
    int fd = open(portname, O_RDONLY | O_NOCTTY | O_SYNC);
    
    OUTPUT_DEBUG_STDERR(stderr, "%s", "Freeing args");
    free(ctx);

    while (isRunning && nbyte >= 0) {
        OUTPUT_DEBUG_STDERR(stderr, "%s", "Entering main loop");

        char buf[MAX_BUF_SIZE];

        OUTPUT_DEBUG_STDERR(stderr, "%s", "Reading file");
        nbyte = read(fd, buf, MAX_BUF_SIZE);
        OUTPUT_DEBUG_STDERR(stderr, "Read: %d bytes", nbyte);
        
        char *token = strtok(buf, ";");
        char *date = (char *) token;
 
        token = strtok(NULL, ";");
        float freq = atof((char *) token);
        
        if (freq > 0.0) {
            OUTPUT_DEBUG_STDERR(stderr,"date: %s", date);
            OUTPUT_DEBUG_STDERR(stderr,"freq: %f", freq);
            writeToDatabase(&freq, nbyte);
        }
    }


    OUTPUT_DEBUG_STDERR(stderr, "%s", "Thread ending");
    pthread_exit(&pid);
}

void onExit(void) {
    isRunning = 0;
    onExitSuper();
}

int main(int argc, char *argv[]) {
    OUTPUT_DEBUG_STDERR(stderr, "%s", "Entering correlate_frequencies::main");
    if (argc <= 1) {
        fprintf(stderr, "Too few argument. Expected: 2 Got: %d\n", argc);
        return -1;
    }

    if (!isRunning) {
        initializeEnv();
        initializeSignalHandlers();
        isRunning = 1;

        pthread_t pid = 0;
        struct thread_args *args = malloc(sizeof(struct thread_args));
        args->buf = malloc(sizeof(args[1]));
//    args->nbyte = MAX_BUF_SIZE;

        OUTPUT_DEBUG_STDERR(stderr, "%s", "Setting exit");
        atexit(onExit);

        memcpy((char *) args->buf, (void *) argv[1], sizeof(args[1]));


        pthread_create(&pid, NULL, run, (void *) args);
        args->pid = pid;
        OUTPUT_DEBUG_STDERR(stderr, "Spawning read thread pid: %ld", *(long *) pid);
        pthread_join(pid, NULL);
    }
}
