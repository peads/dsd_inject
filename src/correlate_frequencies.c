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

#define SEM_RESOURCES 4
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include "utils.h"

#define INSERT_STATEMENT        "insert into frequencydata (`frequency`) " \
                                "values (?) on duplicate key update `date_modified`=NOW();"
#define INSERT_INFO             "INSERT INTO frequencydata (frequency) " \
                                "VALUES (%s);"

#define UPDATE_STATEMENT        "update `imbedata` set `date_decoded`=?, `frequency`=? where `date_recorded`=?;"
#define UPDATE_STATEMENT_LP     "update LOW_PRIORITY `imbedata` set `date_decoded`=?, `frequency`=? where `date_recorded`=?;"
#define UPDATE_INFO             "update imbedata set (date_decoded, frequency) values (%s, %s) where date_recorded=%s;"
#define MAX_BUF_SIZE 34
#define MAX_SQL_ERROR_ARGS 1

static int isRunning = 0;
sem_t sem;
FILE *fd;
int countThreads = 0; 

void writeUpdateDatabase(char *frequency, struct tm *timeinfo) {
    sem_wait(&sem);
    
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
    
    MYSQL *conn = initializeMySqlConnection(bind);
 
    //OUTPUT_INFO_STDERR(stderr, "Size of string: %ld", 1 + strchr(frequency, '\0') - frequency);

    //unsigned long nbyte = 8;
    unsigned long last = strchr(frequency, '\0') - frequency;
    unsigned long nbyte = 1 + last;
    char freq[nbyte];
    strcpy(freq, frequency);
    freq[last] = '\0';

    frequencyBind.buffer_type = MYSQL_TYPE_DECIMAL;
    frequencyBind.buffer = &freq;
    frequencyBind.buffer_length = nbyte;
    frequencyBind.length = &nbyte;
    frequencyBind.is_null = 0;
    
    OUTPUT_INFO_STDERR(stderr, INSERT_INFO, frequency);
    stmt = generateMySqlStatment(INSERT_STATEMENT, conn, &status, 98);
    if (status != 0) {
        doExit(conn);
    }

    memcpy(&bind[0], &frequencyBind, sizeof(frequencyBind));

    status = mysql_stmt_bind_param(stmt, bind);
    if (status != 0) {
        doExit(conn);
    }

    status = mysql_stmt_execute(stmt);
    if (status != 0) {
        doExit(conn);
    }

    mysql_stmt_close(stmt);
    
    stmt = generateMySqlStatment(UPDATE_STATEMENT_LP, conn, &status, 92);
    
    MYSQL_BIND bnd[3];
    memset(bnd, 0, sizeof(bnd));
    memcpy(&bnd[0], &dateDemodBind, sizeof(dateDemodBind));
    memcpy(&bnd[1], &frequencyBind, sizeof(frequencyBind));
    memcpy(&bnd[2], &dateDemodBind, sizeof(dateDemodBind));

    status = mysql_stmt_bind_param(stmt, bnd);
    if (status != 0) {
        doExit(conn);
    }

    status = mysql_stmt_execute(stmt);
    if (status != 0) {
        doExit(conn);
    }

    mysql_stmt_close(stmt);
    mysql_close(conn);

    sem_post(&sem);
}

void onExit(void) {
    isRunning = 0;
    fclose(fd);
    onExitSuper();
}

int main(int argc, char *argv[]) {
    //OUTPUT_DEBUG_STDERR(stderr, "%s", "Entering correlate_frequencies::main");
    if (argc <= 1) {
        fprintf(stderr, "Too few argument. Expected: 2 Got: %d\n", argc);
        return -1;
    }

    if (!isRunning) {
        initializeEnv();
        initializeSignalHandlers();
        isRunning = 1;

        //OUTPUT_DEBUG_STDERR(stderr, "%s", "Setting atexit");
        atexit(onExit);

        char *portname = argv[1];
        int size = 6 + strchr(portname, '\0') - ((char *) portname);
        char cmd[size];
        strcpy(cmd, portname);
        strcat(cmd, " 2>&1");
        fprintf(stderr, "%s size: %d\n", cmd, size);
        
        fd = popen(cmd, "r");
        if (fd == NULL)
            exit(-1);
    }

    struct thread_args *args = malloc(sizeof(struct thread_args));
    args->buf = malloc(MAX_BUF_SIZE * sizeof(char));

    struct tm *timeinfo = malloc(sizeof(*timeinfo));
    int *year = malloc(sizeof(int*));
    int *month = malloc(sizeof(int*));
    int *mantissa = malloc(sizeof(int*));
    int *characteristic = malloc(sizeof(int*));
    int *tzHours = malloc(sizeof(int));
    int *tzMin = malloc(sizeof(int));
    int ret;

    do {
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
         
        fprintf(stderr, "vars set: %d\n", ret);
        
        timeinfo->tm_year = *year - 1900;
        timeinfo->tm_mon = *month - 1;
        timeinfo->tm_isdst = 0;

        char frequency[8];
        sprintf(frequency, "%d.%d", *characteristic, *mantissa);

        writeUpdateDatabase(frequency, timeinfo);
    } while (isRunning && ret != EOF);

    free(year);
    free(month);
    free(mantissa);
    free(characteristic);
    free(tzHours);
    free(tzMin);
    free(args->buf);
    free(args);
}

