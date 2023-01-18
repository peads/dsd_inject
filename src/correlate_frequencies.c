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

#define INSERT_STATEMENT    "insert into frequencydata (`frequency`) " \
                            "values (?) on duplicate key update `date_modified`=NOW();"
#define INSERT_INFO         "INSERT INTO frequencydata (frequency) " \
                            "VALUES (%s);"
#define UPDATE_STATEMENT    "update `imbedata` set `date_decoded`=?, `frequency`=? where `date_recorded`=?;"
#define UPDATE_INFO         "update imbedata set (date_decoded, frequency) values (%s, %s) " \
                            "where date_recorded=%s;"
#define MAX_BUF_SIZE 34
#define MAX_SQL_ERROR_ARGS 1
static int isRunning = 0;

void writeUpdateDatabase(char *freq, size_t nbyte, char *date) {
    OUTPUT_DEBUG_STDERR(stderr, "%s", "Entering correlate_frequencies::writeToDatabase");

    char frequency[nbyte];
    strncpy(frequency, freq, nbyte);
    frequency[nbyte - 1] = '\0';

    struct tm *timeinfo = malloc(sizeof(*timeinfo));

    int *year = malloc(sizeof(int*));
    int *month = malloc(sizeof(int*));

    sscanf(date,  "%d-%d-%dT%d:%d:%d", //+%d:%d", //"%d%d%d%d-%d%d-%d%d%c%d%d:%d%d:%d%d+%d%d:%d%d", 
        year, month, &timeinfo->tm_mday, &timeinfo->tm_hour, &timeinfo->tm_min, &timeinfo->tm_sec);
    
    timeinfo->tm_year = *year - 1900;
    timeinfo->tm_mon = *month - 1;
    
    MYSQL_TIME *dateDemod = generateMySqlTimeFromTm(timeinfo);

    int status;

    MYSQL_BIND bind[1];
    MYSQL_STMT *stmt;
    MYSQL *conn;

    OUTPUT_DEBUG_STDERR(stderr, "Got %s MHz. With size %ld", frequency, nbyte);
    OUTPUT_DEBUG_STDERR(stderr, "%s", "Initializing db connection");
    conn = initializeMySqlConnection(bind);

    OUTPUT_DEBUG_STDERR(stderr, "%s", "Generating prepared statement");
    OUTPUT_INFO_STDERR(stderr, INSERT_INFO, frequency);
    stmt = generateMySqlStatment(INSERT_STATEMENT, conn, &status, 96);
    if (status != 0) {
        doExit(conn);
    }

    bind[0].buffer_type = MYSQL_TYPE_DECIMAL;
    bind[0].buffer = (char *) &frequency;
    bind[0].buffer_length = nbyte;
    bind[0].length = &nbyte;
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
    OUTPUT_INFO_STDERR(stderr, "Rows affected: %llu", mysql_stmt_affected_rows(stmt)); 
    
    OUTPUT_DEBUG_STDERR(stderr, "%s", "Closing statement");
    mysql_stmt_close(stmt);
    
    OUTPUT_DEBUG_STDERR(stderr, "%s", "Generating prepared statement");
    //OUTPUT_INFO_STDERR(stderr, UPDATE_INFO, dateDemod, frequency, dateDemod);
    stmt = generateMySqlStatment(UPDATE_STATEMENT, conn, &status, 145);
    
    MYSQL_BIND bnd[3];
    memset(bnd, 0, sizeof(bnd));

    //OUTPUT_DEBUG_STDERR(stderr, "%d-%d-%dT%d:%d:%d", 
    //    timeinfo->tm_year + 1900,
    //    timeinfo->tm_mon + 1,
    //    timeinfo->tm_mday,
    //    timeinfo->tm_hour,
    //    timeinfo->tm_min,
    //    timeinfo->tm_sec);
    OUTPUT_DEBUG_STDERR(stderr, "%d-%d-%dT%d:%d:%d",
        dateDemod->year,
        dateDemod->month,
        dateDemod->day,
        dateDemod->hour,
        dateDemod->minute,
        dateDemod->second); 
    bnd[0].buffer_type = MYSQL_TYPE_DATETIME;
    bnd[0].buffer = (char *) dateDemod;
    bnd[0].length = 0;
    bnd[0].is_null = 0;

    //bnd[1].buffer_type = MYSQL_TYPE_DECIMAL;
    //bnd[1].buffer = (char *) &frequency;
    //bnd[1].buffer_length = nbyte;
    //bnd[1].length = &nbyte;
    //bnd[1].is_null = 0;
    //
    //bnd[2].buffer_type = MYSQL_TYPE_DATETIME;
    //bnd[2].buffer = (char *) dateDemod;
    //bnd[2].length = 0;
    //bnd[2].is_null = 0;
    
    memcpy(&bnd[1], &bind[0], sizeof(bind[0]));
    memcpy(&bnd[2], &bnd[0], sizeof(bnd[0]));

    OUTPUT_DEBUG_STDERR(stderr, "%s", "Binding parameters");
    status = mysql_stmt_bind_param(stmt, bnd);
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
    OUTPUT_INFO_STDERR(stderr, "Rows affected: %llu", mysql_stmt_affected_rows(stmt)); 

    OUTPUT_DEBUG_STDERR(stderr, "%s", "Closing database connection");
    mysql_close(conn);
    
    free(year);
    free(month);
}

void *run(void *ctx) {
    OUTPUT_DEBUG_STDERR(stderr, "%s", "Read thread spawned");
    struct thread_args *args = (struct thread_args *)ctx;
    char *portname = (char *)args->buf;

    OUTPUT_DEBUG_STDERR(stderr, "%s", "Fetching args");
    pthread_t pid = args->pid;
    ssize_t nbyte = 0;
    OUTPUT_DEBUG_STDERR(stderr, "Opening file: %s", portname);
    int fd = open(portname, O_RDONLY | O_NOCTTY | O_SYNC);
    
    OUTPUT_DEBUG_STDERR(stderr, "%s", "Freeing args");
    free(ctx);

    OUTPUT_DEBUG_STDERR(stderr, "%s", "Entering main loop");
    while (isRunning && nbyte >= 0) {

        char buf[MAX_BUF_SIZE];

        OUTPUT_DEBUG_STDERR(stderr, "%s", "Reading file");
        nbyte = read(fd, buf, MAX_BUF_SIZE);
        OUTPUT_DEBUG_STDERR(stderr, "Read: %ld bytes", nbyte);
        
        char *token = strtok(buf, ";");
        char *date = (char *) token;
 
        token = strtok(NULL, ";");

        float freq = atof(token);
        
        if (freq > 0.0) {
            OUTPUT_DEBUG_STDERR(stderr,"date: %s", date);
            OUTPUT_DEBUG_STDERR(stderr,"freq: %f", freq);
            writeUpdateDatabase(token, 8, date);
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
