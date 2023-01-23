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
#include <mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "utils.h"

const char *db_pass;
const char *db_host;
const char *db_user;
const char *schema;

static void writeUpdate(char *frequency, time_t t, unsigned long nbyte);
static void writeFrequencyPing(char *frequency, unsigned long nbyte);
static void *runFrequencyUpdatingThread(void *ctx);

static int isRunning = 0;
static int pidCount = 0;
static sem_t sem;
static sem_t sem1;
static pthread_t pids[MAX_PIDS] = {-1};

static void onExit(void) {
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
    fprintf(stderr, "%s", "Awaiting quiescence\n");
    int i = 0;
    for (; i < MAX_PIDS; ++i) {
        pthread_t pid = pids[i];
        if (pid > 0) {
            OUTPUT_DEBUG_STDERR(stderr, "Found pid: %lu", pid);
            pthread_join(pid, NULL);
        }
    }
    fprintf(stderr, "%s", "\n");
}

static long createIndex() {
    
    if (pidCount++ < MAX_PIDS) {
        return pidCount;
    }
    
    pidCount = 0;
    return 0;
}

void addPid(pthread_t pid) {
    OUTPUT_INFO_STDERR(stderr, "Adding pid: %lu", pid);

    unsigned long idx = createIndex();
    pids[idx] = pid;

    OUTPUT_INFO_STDERR(stderr, "Added pid: %lu @ index: %lu", pid, idx);
}

static char *getEnvVarOrDefault(char *name, char *def) {

    char *result = getenv(name);

    if (result) {
        return result;
    }
    return def;
}

void initializeEnv() {
    
    if (isRunning) {
        return;
    }

    atexit(onExit);

    OUTPUT_DEBUG_STDERR(stderr, "Semaphore resources: %d\n", SEM_RESOURCES);
    sem_init(&sem, 0, SEM_RESOURCES);
    sem_init(&sem1, 0, SEM_RESOURCES);

    db_pass = getenv("DB_PASS");
    if (db_pass) {
        db_host = getEnvVarOrDefault("DB_HOST", "127.0.0.1");
        db_user = getEnvVarOrDefault("DB_USER", "root");
        schema = getEnvVarOrDefault("SCHEMA", "scanner");
    } else {
        fprintf(stderr, "%s\n", "No database user password defined in envionment.");
        exit(-1);
    }

    pthread_t pid = 0;
    char *fileDes = "/home/peads/fm-err-out";
    pthread_create(&pid, NULL, runFrequencyUpdatingThread, (void *) fileDes);
    addPid(pid);
}

static void doExit(MYSQL *con) {

    fprintf(stderr, "MY_SQL error: %s\n", mysql_error(con));
    if (con != NULL) {
        mysql_close(con);
    }
    exit(-1);
}

static void generateMySqlTimeFromTm(MYSQL_TIME *dateDecoded, const struct tm *timeinfo) {

    OUTPUT_DEBUG_STDERR(stderr, "%s", "Entering utils::generateMySqlTimeFromTm");

    dateDecoded->year = timeinfo->tm_year + 1900; // struct tm stores year as years since 1900
    dateDecoded->month = timeinfo->tm_mon + 1; // struct tm stores month as months since January (0-11)
    dateDecoded->day = timeinfo->tm_mday;
    dateDecoded->hour = timeinfo->tm_hour;
    dateDecoded->minute = timeinfo->tm_min;
    dateDecoded->second = timeinfo->tm_sec;
    OUTPUT_DEBUG_STDERR(stderr, "%s", "Returning from  utils::generateMySqlTimeFromTm");
}

static void writeFrequencyPing(char *frequency, unsigned long nbyte) {
    OUTPUT_INFO_STDERR(stderr, "%s", "UPSERTING FREQUENCY PING");

    int status;
    MYSQL_BIND bind[1];
    MYSQL *conn = mysql_init(NULL);
    mysql_real_connect(conn, db_host, db_user, db_pass, schema, 0, NULL, 0);

    memset(bind, 0, sizeof(bind));

    unsigned long length = LENGTH_OF(INSERT_FREQUENCY);
    OUTPUT_DEBUG_STDERR(stderr, "Insert length of string: %u", length);
    OUTPUT_DEBUG_STDERR(stderr, "writeFrequencyPing :: "  INSERT_FREQUENCY_INFO, frequency);
    
    MYSQL_STMT *stmt = mysql_stmt_init(conn);
    mysql_stmt_prepare(stmt, INSERT_FREQUENCY, length);    


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

static void writeUpdate(char *frequency, time_t t, unsigned long nbyte) {
    OUTPUT_INFO_STDERR(stderr, "%s", "UPDATING FREQUENCY");
    
    int status;
    MYSQL_BIND bind[3];
    MYSQL *conn = mysql_init(NULL);
    mysql_real_connect(conn, db_host, db_user, db_pass, schema, 0, NULL, 0);

    MYSQL_TIME *dateDemod = malloc(sizeof(MYSQL_TIME));

    struct tm *timeinfo;
    timeinfo = localtime(&t);
    generateMySqlTimeFromTm(dateDemod, timeinfo);
    unsigned long length = LENGTH_OF(UPDATE_FREQUENCY);
    MYSQL_STMT *stmt = mysql_stmt_init(conn); 
    mysql_stmt_prepare(stmt, UPDATE_FREQUENCY, length);

    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_DATETIME;
    bind[0].buffer = dateDemod;
    bind[0].length = 0;
    bind[0].is_null = 0;

    bind[1].buffer_type = MYSQL_TYPE_DECIMAL;
    bind[1].buffer = frequency;
    bind[1].buffer_length = nbyte;
    bind[1].length = &nbyte;
    bind[1].is_null = 0;
    
    bind[2].buffer_type = MYSQL_TYPE_DATETIME;
    bind[2].buffer = dateDemod;
    bind[2].length = 0;
    bind[2].is_null = 0;

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
    
    free(dateDemod);
}

static void writeInsert(const void *buf, size_t nbyte) {
    OUTPUT_INFO_STDERR(stderr, "%s\n", "INSERTING DATA");

    int status = 0;
 
    if (status != 0) {
        exit(-1);
    }

    MYSQL_BIND bind[1];
    MYSQL *conn = mysql_init(NULL);
    mysql_real_connect(conn, db_host, db_user, db_pass, schema, 0, NULL, 0);

    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_BLOB;
    bind[0].buffer = (char *) &buf;
    bind[0].buffer_length = nbyte;
    bind[0].length = &nbyte;
    bind[0].is_null = 0;

    unsigned long length = LENGTH_OF(INSERT_DATA) ;
    OUTPUT_DEBUG_STDERR(stderr, "Length of string: %lu", length);
    
    MYSQL_STMT *stmt = mysql_stmt_init(conn); 
    mysql_stmt_prepare(stmt, INSERT_DATA, length);

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

static void sshiftLeft(char *s, int n)
{
   char* s2 = s + n;
   while ( *s2 )
   {
      *s = *s2;
      ++s;
      ++s2;
   }
   *s = '\0';
}

static double parseRmsFloat(char *s) {
    unsigned long nbyte = 1 + strchr(strchr(s, ' ') + 1, ' ') - s;
    s[nbyte - 1] = '\0';
    double result = atof(s);

    return result;
}

static void parseFrequency(char *frequency, char *token) {
    char characteristic[7];
    char mantissa[7];

    token[6] = '\0';
    unsigned long nbyte = 1 + strchr(token, '\0') - token;

    strcpy(characteristic, token);
    strcpy(mantissa, token);
    
    characteristic[(nbyte - 1) / 2] = '\0'; 

    sshiftLeft(mantissa, 3);
    sprintf(frequency, "%s.%s", characteristic, mantissa);
}

static void parseLineData(char *frequency, double *avgRms, double *squelch, char *buffer) {
    int i = 0;
    char *token = strtok(buffer, ",");

    while (token != NULL) {
        switch (i++) {
            case 0:
                parseFrequency(frequency, token);
                break;
            case 1:
                *avgRms = parseRmsFloat(token);
                break;
            case 4:
                if (*squelch < 0.0) {
                    *squelch = parseRmsFloat(token);
                }
                break;
            case 2:
            case 3:
            default:
                break;                        
        }
        token = strtok(NULL, ",");
    }
}

void *runInsertThread(void *ctx) {
    sem_wait(&sem);
    struct insertArgs *args = (struct insertArgs *) ctx;

    writeInsert(args->buf, args->nbyte);

    sem_post(&sem);
    free(args->buf);
    free(args);
    
    return NULL;
}

static void *runUpdateThread(void *ctx) {
    sem_wait(&sem1);
    struct updateArgs *args = (struct updateArgs *) ctx;

    writeUpdate(args->frequency, args->t, args->nbyte);

    sem_post(&sem1);
    
    return NULL;
}

static void *runPingThread(void *ctx) {
    char *frequency = (char *) ctx;

    writeFrequencyPing(frequency, 8);

    return NULL; 
}

static void *runFrequencyUpdatingThread(void *ctx) {

    char *portname = (char *) ctx;
    char buffer[255];
    unsigned long last = strchr(portname, '\0') - portname;
    unsigned long nbyte = 1 + last;
    char filename[nbyte];

    strcpy(filename, portname);
    filename[last] = '\0';

    OUTPUT_DEBUG_STDERR(stderr, "FILENAME: %s", filename);

    FILE *fd = fopen(filename, "r");
    int bufSize = 0;
    char ret;
    char frequency[8];
    double squelch = -1.0;
    double avgRms = 0.0;
 
    while (!(feof(fd))) {
        if ((ret = fgetc(fd)) != '\n') {
            buffer[bufSize++] = ret;
        } else {
            if (bufSize >= 255) {
                continue;
            }
            time_t t = time(NULL);

            buffer[bufSize] = '\0';
            bufSize = 0;
            
            parseLineData(frequency, &avgRms, &squelch, buffer);
            if (avgRms >= squelch) {
                pthread_t pid = 0;
                pthread_create(&pid, NULL, runPingThread, frequency); 
                addPid(pid);

                struct updateArgs *args = malloc(sizeof(struct updateArgs));
                args->frequency = frequency;
                args->t = t;
                args->nbyte = 8;
                pid = 0;
                pthread_create(&pid, NULL, runUpdateThread, args);
                addPid(pid);
            }
        }
    }
    fclose(fd);

    return NULL;
}

