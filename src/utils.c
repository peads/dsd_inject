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
// Created by Patrick Eads on 1/15/23.
//
#define DATE_STRING "%04d-%02d-%02d %02d:%02d:%02d"

#include "utils.h"
#include <unistd.h>
#include <semaphore.h>

extern char *db_pass;
extern char *db_host;
extern char *db_user;
extern char *schema;

ssize_t (*next_write)(int fildes, const void *buf, size_t nbyte, off_t offset) = NULL;
static struct updateArgs *updateHash[SIX_DAYS_IN_SECONDS] = {NULL};
static pthread_t pidHash[SIX_DAYS_IN_SECONDS] = {0};
time_t updateStartTime;
int isRunning = 0;
FILE *fd;
static sem_t sem;

void onExit(void) {
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

void *insertDataThread(void *ctx) {

    struct insertArgs *args = (struct insertArgs *) ctx;
    const time_t insertTime = time(NULL);

    sem_wait(&sem);

    writeInsertToDatabase(insertTime, args->buf, args->nbyte);

    sem_post(&sem);

    free(args->buf);
    free(ctx);

    pthread_exit(&args->pid);
}

void doExit(MYSQL *con) {

    fprintf(stderr, "MY_SQL error: %s\n", mysql_error(con));
    if (con != NULL) {
        mysql_close(con);
    }
    exit(-1);
}

MYSQL *initializeMySqlConnection() {

    MYSQL *conn;
    conn = mysql_init(NULL);

    if (conn == NULL || mysql_real_connect(
            conn,
            db_host, //$DB_HOST
            db_user, //$DB_USER
            db_pass, //$DB_PASS
            schema, //$SCHEMA
            0, NULL, 0) == NULL) {

        doExit(conn);
    }
    OUTPUT_DEBUG_STDERR(stderr, "host: %s, user: %s, schema: %s", db_host, db_user, schema);

    return conn;
}

MYSQL_TIME *generateMySqlTimeFromTm(const struct tm *timeinfo) {

    OUTPUT_DEBUG_STDERR(stderr, "%s", "Entering utils::generateMySqlTimeFromTm");
    MYSQL_TIME *dateDecoded = malloc(sizeof(*dateDecoded));

    dateDecoded->year = timeinfo->tm_year + 1900; // struct tm stores year as years since 1900
    dateDecoded->month = timeinfo->tm_mon + 1; // struct tm stores month as months since January (0-11)
    dateDecoded->day = timeinfo->tm_mday;
    dateDecoded->hour = timeinfo->tm_hour;
    dateDecoded->minute = timeinfo->tm_min;
    dateDecoded->second = timeinfo->tm_sec;
    OUTPUT_DEBUG_STDERR(stderr, "%s", "Returning from  utils::generateMySqlTimeFromTm");

    return dateDecoded;
}

MYSQL_STMT *generateMySqlStatment(char *statement, MYSQL *conn, long size) {
    int status = -1;
    OUTPUT_DEBUG_STDERR(stderr, "%s", "Entering utils::generateMySqlStatment");
    MYSQL_STMT *stmt = mysql_stmt_init(conn);

    if (stmt != NULL) {
        status = mysql_stmt_prepare(stmt, statement, size);
        if (status == 0) { 
                OUTPUT_DEBUG_STDERR(stderr, "%s", "Returning  utils::generateMySqlStatment");
                return stmt;
        }
    }
    doExit(conn);
    return NULL;
}

void writeFrequencyPing(char *frequency, unsigned long nbyte) {
    OUTPUT_INFO_STDERR(stderr, "%s", "UPSERTING FREQUENCY PING");

    int status;
    MYSQL_STMT *stmt;
    MYSQL_BIND bind[1];
    MYSQL *conn = initializeMySqlConnection();

    memset(&bind[0], 0, sizeof(MYSQL_BIND));
    memset(bind, 0, sizeof(bind));

    unsigned long length = LENGTH_OF(INSERT_FREQUENCY);
    OUTPUT_DEBUG_STDERR(stderr, "Insert length of string: %u", length);
    OUTPUT_DEBUG_STDERR(stderr, "writeFrequencyPing :: "  INSERT_FREQUENCY_INFO, frequency);

    stmt = generateMySqlStatment(INSERT_FREQUENCY, conn, length);

    bind[0].buffer_type = MYSQL_TYPE_DECIMAL;
    bind[0].buffer = frequency;
    bind[0].buffer_length = nbyte;
    bind[0].length = &nbyte;
    bind[0].is_null = 0;

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
}

void writeUpdate(char *frequency, struct tm *timeinfo, unsigned long nbyte) {
    OUTPUT_DEBUG_STDERR(stderr, "%s", "UPDATING FREQUENCY");
    
    int status;
    MYSQL_BIND bind[3];
    MYSQL *conn = initializeMySqlConnection();
    MYSQL_TIME *dateDemod = generateMySqlTimeFromTm(timeinfo);
    unsigned long length = LENGTH_OF(UPDATE_FREQUENCY);
    MYSQL_STMT *stmt = generateMySqlStatment(UPDATE_FREQUENCY, conn, length);

    memset(bind, 0, sizeof(bind));
    memset(&bind[0], 0, sizeof(MYSQL_BIND));
    memset(&bind[1], 0, sizeof(MYSQL_BIND));
    memset(&bind[2], 0, sizeof(MYSQL_BIND));
    
    bind[0].buffer_type = MYSQL_TYPE_DATETIME;
    bind[0].buffer = (char *) dateDemod;
    bind[0].length = 0;
    bind[0].is_null = 0;

    bind[1].buffer_type = MYSQL_TYPE_DECIMAL;
    bind[1].buffer = frequency;
    bind[1].buffer_length = nbyte;
    bind[1].length = &nbyte;
    bind[1].is_null = 0;
    
    bind[2].buffer_type = MYSQL_TYPE_DECIMAL;
    bind[2].buffer = frequency;
    bind[2].buffer_length = nbyte;
    bind[2].length = &nbyte;
    bind[2].is_null = 0;

    //memcpy(&bind[2], &bind[0], sizeof(MYSQL_BIND));
    MYSQL_TIME *ts = bind[0].buffer;
    OUTPUT_DEBUG_STDERR(stderr, 
            "writeUpdate :: " DATE_STRING,
             ts->year, ts->month, ts->day,
             ts->hour, ts->minute, ts->second);

    char buffer[36];
    ts = bind[2].buffer;
    sprintf(buffer, DATE_STRING,ts->year, ts->month, ts->day,
                 ts->hour, ts->minute, ts->second);
    OUTPUT_DEBUG_STDERR(stderr, 
        "writeUpdate :: " UPDATE_FREQUENCY_INFO, 
        buffer, frequency, buffer);

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
}

void *notifyInsertThread(void *ctx) {
   
    struct notifyArgs *nargs = (struct notifyArgs *) ctx;
    time_t idx = nargs->idx;
    struct updateArgs *args = nargs->args;
    int i = 0;

    OUTPUT_INFO_STDERR(stderr, "notifyInsertThread :: DATE: %d-%d-%dT%d:%d:%d", 
        args->timeinfo.tm_year + 1900, 
        args->timeinfo.tm_mon + 1, 
        args->timeinfo.tm_mday, 
        args->timeinfo.tm_hour, 
        args->timeinfo.tm_min, 
        args->timeinfo.tm_sec);

    pthread_t pid;

    do {
        pid = pidHash[idx];

        OUTPUT_DEBUG_STDERR(stderr, "Searching pid: %lu at: %lu", pid, idx);
        if (pid > 0 ) {

            OUTPUT_DEBUG_STDERR(stderr, "%s", "SIGNALS AWAY"); 
            updateHash[idx] = args;
            OUTPUT_DEBUG_STDERR(stderr, "Struct added to hash at: %ld", idx);
            //pthread_kill(pid, SIGUSR2);
            return NULL;
        }

        sleep(1);
        OUTPUT_DEBUG_STDERR(stderr, "notifyInsertThread :: Waited: %d seconds", i);
    } while (pid <= 0 && i++ < 10);
    
    OUTPUT_INFO_STDERR(stderr, "%s", "notifyInsertThread :: Failed notification for update");
    pthread_exit(&nargs->pid);
}

void *waitForUpdate(void *ctx) {
    time_t idx = *((time_t *) ctx);
    int i = 0; 
    struct updateArgs *dbArgs;

    do {
        OUTPUT_DEBUG_STDERR(stderr, "waitForUpdate :: pid: %lu @ INDEX: %lu", pidHash[idx], idx);

        dbArgs = updateHash[idx];

        if (dbArgs != NULL) {
            OUTPUT_DEBUG_STDERR(stderr, "waitForUpdate :: DATE: %d-%d-%dT%d:%d:%d", 
                dbArgs->timeinfo.tm_year + 1900, 
                dbArgs->timeinfo.tm_mon + 1, 
                dbArgs->timeinfo.tm_mday, 
                dbArgs->timeinfo.tm_hour, 
                dbArgs->timeinfo.tm_min, 
                dbArgs->timeinfo.tm_sec);
            writeUpdate(dbArgs->frequency, &dbArgs->timeinfo, dbArgs->nbyte);
            return NULL;
        }
        sleep(1);
        OUTPUT_DEBUG_STDERR(stderr, "waitForUpdate :: Waited: %d seconds", i);
    } while (NULL == dbArgs && i++ < 10);

    OUTPUT_INFO_STDERR(stderr, "%s", "waitForUpdate :: Failed waiting to update");
    return NULL;
}

void writeInsertToDatabase(time_t insertTime, void *buf, size_t nbyte) {

    int status = 0;
 
    if (status != 0) {
        exit(-1);
    }

    MYSQL_BIND bind[2];
    MYSQL_STMT *stmt;
    MYSQL *conn;
    MYSQL_TIME *dateDecoded;
    
    struct tm *timeinfo;
    timeinfo = localtime(&insertTime);
    dateDecoded = generateMySqlTimeFromTm(timeinfo);


    memset(bind, 0, sizeof(*bind));
    bind[0].buffer_type = MYSQL_TYPE_DATETIME;
    bind[0].buffer = dateDecoded;
    bind[0].length = 0;
    bind[0].is_null = 0;

    bind[1].buffer_type = MYSQL_TYPE_BLOB;
    bind[1].buffer = (char *) &buf;
    bind[1].buffer_length = nbyte;
    bind[1].length = &nbyte;
    bind[1].is_null = 0;

    conn = initializeMySqlConnection();

    char buffer[26];
    strftime(buffer, 26, "%Y-%m-%dT%H:%M:%S:%z\n", timeinfo);

    unsigned long length = LENGTH_OF(INSERT_DATA) ;
    OUTPUT_DEBUG_STDERR(stderr, "Length of string: %lu", length);
    OUTPUT_DEBUG_STDERR(stderr, INSERT_INFO "\n", buffer, nbyte);
    
    stmt = generateMySqlStatment(INSERT_DATA, conn, length);

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

    time_t idx = (insertTime - updateStartTime) % SIX_DAYS_IN_SECONDS;
    pthread_t pid = 0;
    pthread_create(&pid, NULL, waitForUpdate, &idx);
    pthread_detach(pid);
    pidHash[idx] = pid;
    OUTPUT_DEBUG_STDERR(stderr, "writeInsertToDatabase :: pid: %lu @ INDEX: %lu", pidHash[idx], idx);
    free((void *) dateDecoded);
}

void *startUpdatingFrequency(void *ctx) {

    if (isRunning) {
        return NULL;
    }
    OUTPUT_DEBUG_STDERR(stderr, "%s", "Entering inject::startUpdatingFrequency");
    updateStartTime = time(NULL);
    isRunning = 1;
    char *portname = (char *) ctx;
    unsigned long size = 6 + strchr(portname, '\0') - ((char *) portname);
    char cmd[size];
    strcpy(cmd, portname);

    OUTPUT_DEBUG_STDERR(stderr, "%s size: %lu", cmd, size);

    fd = popen(cmd, "r");
    if (fd == NULL) {
        exit(-1);
    }

    int ret;

    do {
        struct updateArgs *args = malloc(sizeof(struct updateArgs));

        struct tm timeinfo;
        int year = 0;
        int month = 0;
        int mantissa = 0;
        int characteristic = 0;
        int tzHours = 0;
        int tzMin = 0;

        ret = fscanf(fd, "%d-%d-%dT%d:%d:%d+%d:%d;%d.%d\n",
                     &year,
                     &month,
                     &timeinfo.tm_mday,
                     &timeinfo.tm_hour,
                     &timeinfo.tm_min,
                     &timeinfo.tm_sec,
                     &tzHours,
                     &tzMin,
                     &characteristic,
                     &mantissa);
        OUTPUT_DEBUG_STDERR(stderr, "vars set: %d\n", ret);

        if (ret == 10) {
            time_t loopTime = mktime(&timeinfo);
            OUTPUT_DEBUG_STDERR(stderr, "DELTA TIME: %ld", loopTime - updateStartTime);

            timeinfo.tm_year = year - 1900;
            timeinfo.tm_mon = month - 1;
            timeinfo.tm_isdst = 0;
            
            args->timeinfo = timeinfo;
            
            char frequency[8];
            sprintf(frequency, "%d.%d", characteristic, mantissa);

            OUTPUT_DEBUG_STDERR(stderr, "Size of string: %ld\n", 1 + strchr(frequency, '\0') - frequency);
            //unsigned long nbyte = 8;

            unsigned long last = strchr(frequency, '\0') - frequency;
            unsigned long nbyte = 1 + last;
            strcpy(args->frequency, frequency);
            args->frequency[last] = '\0';
            args->nbyte = nbyte;
            OUTPUT_DEBUG_STDERR(stderr, "FREQUENCY: %s", args->frequency);
            
            writeFrequencyPing(args->frequency, nbyte);

            time_t idx = (loopTime - updateStartTime) % SIX_DAYS_IN_SECONDS;
            struct notifyArgs *nargs = malloc(sizeof(struct notifyArgs));

            nargs->idx = idx;
            nargs->args = args;
            nargs->pid = 0;
            pthread_create(&nargs->pid, NULL, notifyInsertThread, nargs);
            pthread_detach(nargs->pid);
        }
    } while (isRunning && ret != EOF);

    return NULL;
}

