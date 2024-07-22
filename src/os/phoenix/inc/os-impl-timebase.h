/************************************************************************
 * NASA Docket No. GSC-18,719-1, and identified as “core Flight System: Bootes”
 *
 * Copyright (c) 2020 United States Government as represented by the
 * Administrator of the National Aeronautics and Space Administration.
 * All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ************************************************************************/

/**
 * \file
 *
 * \ingroup  phoenix
 *
 */

#ifndef OS_IMPL_TIMEBASE_H
#define OS_IMPL_TIMEBASE_H

#include "osconfig.h"
#include <sys/threads.h>
#include <signal.h>
#include <stdbool.h>

#define OS_TIMEBASE_THREAD_STACK_SIZE 4096

typedef struct
{
    volatile bool   active;
    volatile bool   finished;
    volatile bool   finish_rq;
    volatile time_t start_time;
    volatile time_t interval_time;
    handle_t        mutex;
    handle_t        cond;
} OS_impl_timebase_internal_timer_t;

typedef struct
{
    int                               handler_thread;
    int                               timer_thread;
    handle_t                          handler_mutex;
    semaphore_t                       tick_sem;
    bool                              reset_flag;
    bool                              simulate_flag;
    OS_impl_timebase_internal_timer_t timer;
    uint8                             thread_stack[OS_TIMEBASE_THREAD_STACK_SIZE] __attribute__((aligned(8)));
    uint8                             timer_stack[OS_TIMEBASE_THREAD_STACK_SIZE] __attribute__((aligned(8)));
} OS_impl_timebase_internal_record_t;

/****************************************************************************************
                                   GLOBAL DATA
 ***************************************************************************************/

extern OS_impl_timebase_internal_record_t OS_impl_timebase_table[OS_MAX_TIMEBASES];

#endif /* OS_IMPL_TIMEBASE_H */
