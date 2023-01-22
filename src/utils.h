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
#ifndef UTILS_H
#define UTILS_H

#ifndef TRACE
#define OUTPUT_DEBUG_STDERR(file, msg, subs ...)  //
#else
#define OUTPUT_DEBUG_STDERR(file, msg, subs ...)  fprintf(file, msg "\n", subs)
#endif

#if !(defined(DEBUG) || defined(TRACE))
#define OUTPUT_INFO_STDERR(file, msg, subs ...)  //
#else
#define OUTPUT_INFO_STDERR(file, msg, subs ...)  fprintf(file, msg "\n", subs)
#endif

#define MAX_BUF 255
#define WHILE_READ "while read var; do echo  $var; done <"
#define SEM_RESOURCES 12
#define LENGTH_OF(arr) (sizeof(arr) / sizeof(*(arr)))
#define SIX_DAYS_IN_SECONDS 518400
#define INSERT_FREQUENCY_INFO "INSERT INTO frequencydata (frequency) VALUES (%s);"
#define INSERT_FREQUENCY "insert into frequencydata (`frequency`) values (?) on duplicate key update `date_modified`=NOW();"

#define UPDATE_FREQUENCY "UPDATE LOW_PRIORITY imbedata SET `date_decoded`=?, `frequency`=? WHERE `date_recorded`=?;"
#define UPDATE_FREQUENCY_INFO "UPDATE imbedata SET date_decoded=%s, frequency=%s WHERE date_recorded=%s;"

#define INSERT_DATA "INSERT INTO imbedata (data) VALUES (?);"

#define DATE_STRING "%04d-%02d-%02d %02d:%02d:%02d"

struct insertArgs {
    void *buf;
    size_t nbyte;
    pthread_t pid;
};

struct updateArgs {
    char *frequency;
    time_t t;
    unsigned long nbyte;
    pthread_t pid;
};
#endif //UTILS_H

