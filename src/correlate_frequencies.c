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
#define LOCK_STATEMENT      "select * from `imbedata` where `date_recorded`=? for update"
#define MAX_BUF_SIZE 34
#define MAX_SQL_ERROR_ARGS 1

static int isRunning = 0;
sem_t sem;
int fd; 

void writeUpdateDatabase(char *freq, size_t nbyte, char *date) {
    struct tm *timeinfo = malloc(sizeof(*timeinfo));

    int *year = malloc(sizeof(int*));
    int *month = malloc(sizeof(int*));

    sscanf(date,  "%d-%d-%dT%d:%d:%d", //+%d:%d", //"%d%d%d%d-%d%d-%d%d%c%d%d:%d%d:%d%d+%d%d:%d%d", 
        year, month, &timeinfo->tm_mday, &timeinfo->tm_hour, &timeinfo->tm_min, &timeinfo->tm_sec);
    
    timeinfo->tm_year = *year - 1900;
    timeinfo->tm_mon = *month - 1;
    timeinfo->tm_isdst = 0;
    
    int status;
    MYSQL_STMT *stmt;
    MYSQL_TIME *dateDemod = generateMySqlTimeFromTm(timeinfo); 
    MYSQL_BIND dateDemodBind;
    MYSQL_BIND frequencyBind;
    MYSQL_BIND bind[1];
    memset(&dateDemodBind, 0, sizeof(dateDemodBind)); 
    memset(&frequencyBind, 0, sizeof(frequencyBind));  
    
    dateDemodBind.buffer_type = MYSQL_TYPE_DATETIME;
    dateDemodBind.buffer = (char *) dateDemod;
    dateDemodBind.length = 0;
    dateDemodBind.is_null = 0;

    memcpy(&bind[0], &dateDemodBind, sizeof(dateDemodBind));  
    
    OUTPUT_DEBUG_STDERR(stderr, "%s", "Initializing db connection");
    MYSQL *conn = initializeMySqlConnection(bind);
    
    char frequency[nbyte];
    strncpy(frequency, freq, nbyte);
    frequency[nbyte - 1] = '\0';
    
    frequencyBind.buffer_type = MYSQL_TYPE_DECIMAL;
    frequencyBind.buffer = (char *) &frequency;
    frequencyBind.buffer_length = nbyte;
    frequencyBind.length = &nbyte;
    frequencyBind.is_null = 0;

    OUTPUT_DEBUG_STDERR(stderr, "Got %s MHz. With size %ld", frequency, nbyte);

    OUTPUT_DEBUG_STDERR(stderr, "%s", "Generating prepared statement");
    OUTPUT_INFO_STDERR(stderr, INSERT_INFO, frequency);
    stmt = generateMySqlStatment(INSERT_STATEMENT, conn, &status, 96);
    if (status != 0) {
        doExit(conn);
    }

    memcpy(&bind[0], &frequencyBind, sizeof(frequencyBind));

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
    OUTPUT_INFO_STDERR(stderr, UPDATE_INFO, date, frequency, date);
    stmt = generateMySqlStatment(UPDATE_STATEMENT, conn, &status, 145);
    
    MYSQL_BIND bnd[3];
    memset(bnd, 0, sizeof(bnd));
    memcpy(&bnd[0], &dateDemodBind, sizeof(dateDemodBind));
    memcpy(&bnd[1], &frequencyBind, sizeof(frequencyBind));
    memcpy(&bnd[2], &dateDemodBind, sizeof(dateDemodBind));

    OUTPUT_DEBUG_STDERR(stderr, "Frequency copied: %s", (char *) bnd[1].buffer);

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

    OUTPUT_INFO_STDERR(stderr, "Rows affected: %llu", mysql_stmt_affected_rows(stmt)); 
    OUTPUT_DEBUG_STDERR(stderr, "%s", "Closing statement");
    mysql_stmt_close(stmt);

    OUTPUT_DEBUG_STDERR(stderr, "%s", "Closing database connection");
    mysql_close(conn);
    
    free(year);
    free(month);
}

void *run(void *ctx) {
    OUTPUT_DEBUG_STDERR(stderr, "%s", "Fetching args");
    struct thread_args *args = (struct thread_args *)ctx;
    char *token = strtok(args->buf, ";");
    char *frequency;
    char *date;

    date = malloc(MAX_BUF_SIZE);
    strcpy(date, (char *) token);
    
    token = strtok(NULL, ";");
    frequency = malloc(MAX_BUF_SIZE);
    strcpy(frequency, (char *) token);

    pthread_t pid = args->pid;  
    OUTPUT_DEBUG_STDERR(stderr,"Write thread spawned, pid: %ld", *(long *) pid);

    if (frequency != NULL && atof(frequency) > 0.0) {
        OUTPUT_DEBUG_STDERR(stderr,"date: %s", date);
        OUTPUT_DEBUG_STDERR(stderr,"freq: %s", frequency);
        sem_wait(&sem);
        writeUpdateDatabase(frequency, 8, date);
        sem_post(&sem);
    }
    
    OUTPUT_DEBUG_STDERR(stderr, "%s", "Thread ending");
    pthread_exit(&pid);
}

void onExit(void) {
    isRunning = 0;
    close(fd);
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

        OUTPUT_DEBUG_STDERR(stderr, "%s", "Setting atexit");
        atexit(onExit);
 
        sem_close(&sem);
        sem_init(&sem, 0, 2);
    }

    struct thread_args *args = malloc(sizeof(struct thread_args));
    ssize_t nbyte = 0;
    char *portname = argv[1];

    OUTPUT_DEBUG_STDERR(stderr, "Opening file: %s", portname);
    fd = open(portname, O_RDONLY | O_NOCTTY | O_SYNC);

    OUTPUT_DEBUG_STDERR(stderr, "%s", "Entering main loop");
    while (isRunning && nbyte >= 0) {
        pthread_t pid = 0;
        char buf[MAX_BUF_SIZE];

        OUTPUT_DEBUG_STDERR(stderr, "%s", "Reading file");
        nbyte = read(fd, buf, MAX_BUF_SIZE);
        OUTPUT_DEBUG_STDERR(stderr, "Read: %ld bytes", nbyte);

        args->buf = buf;
        args->nbyte = nbyte;
        pthread_create(&pid, NULL, run, (void *) args);
        args->pid = pid;
        pthread_detach(pid);
        OUTPUT_DEBUG_STDERR(stderr, "Spawning write thread, pid: %ld", *(long *) pid);
    }
}
