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

#include "utils.h"

const char *db_pass;
const char *db_host;
const char *db_user;
const char *schema;

void doExit(MYSQL *con) {

    fprintf(stderr, "MY_SQL error: %s\n", mysql_error(con));
    if (con != NULL) {
        mysql_close(con);
    }
    exit(1);
}

void initializeEnv() {
    sem_init(&sem, 0, 128);

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

void onExitSuper(void) {

    int status;

    if ((status = sem_close(&sem)) != 0) {
        fprintf(stderr, "unable to unlink semaphore. status: %s\n", strerror(status));
    } else {
        fprintf(stderr, "%s", "semaphore destroyed\n");
    }
}

char *getEnvVarOrDefault(char *name, char *def) {

    char *result = getenv(name);

    if (result) {
        return result;
    }
    return def;
}

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

MYSQL *initializeMySqlConnection(MYSQL_BIND *bind) {

    MYSQL *conn;
    conn = mysql_init(NULL);
    memset(bind, 0, sizeof(*bind));

    if (conn == NULL || mysql_real_connect(
            conn,
            db_host, //$DB_HOST
            db_user, //$DB_USER
            db_pass, //$DB_PASS
            schema, //$SCHEMA
            0, NULL, 0) == NULL) {

        doExit(conn);
    }

    return conn;
}

MYSQL_TIME *generateMySqlTimeFromTm(const struct tm *timeinfo) {

    MYSQL_TIME *dateDecoded = malloc(sizeof(*dateDecoded));

    dateDecoded->year = timeinfo->tm_year + 1900; // struct tm stores year as years since 1900
    dateDecoded->month = timeinfo->tm_mon + 1; // struct tm stores month as months since January (0-11)
    dateDecoded->day = timeinfo->tm_mday;
    dateDecoded->hour = timeinfo->tm_hour;
    dateDecoded->minute = timeinfo->tm_min;
    dateDecoded->second = timeinfo->tm_sec;

    return dateDecoded;
}

MYSQL_TIME *generateMySqlTime(const time_t *t) {

    struct tm *timeinfo;
    timeinfo = localtime(t);

    return generateMySqlTimeFromTm(timeinfo);
}

MYSQL_STMT *generateMySqlStatment(char *statement, MYSQL *conn, int *status, long size) {

    MYSQL_STMT *stmt;

    stmt = mysql_stmt_init(conn);
    *status = mysql_stmt_prepare(stmt,statement,size);
    return stmt;
}
