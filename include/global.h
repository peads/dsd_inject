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
#ifndef GLOBAL_H
#define GLOBAL_H

#ifndef TRACE
#define PRINT_TRACE(file, msg, subs ...)  //
#else
#define PRINT_TRACE(file, msg, subs ...)  fprintf(file, msg "\n", subs)
#endif

#if !(defined(DEBUG) || defined(TRACE))
#define PRINT_DEBUG(file, msg, subs ...)  //
#else
#define PRINT_DEBUG(file, msg, subs ...)  fprintf(file, msg "\n", subs)
#endif

#endif //GLOBAL_H

