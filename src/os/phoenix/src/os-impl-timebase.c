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
 * \ingroup  phoenix
 * \author   lukasz.leczkowski@phoenix-rtos.com
 *
 * This file contains the OSAL Timebase API for POSIX systems.
 *
 * This implementation depends on the POSIX Timer API which may not be available
 * in older versions of the Linux kernel. It was developed and tested on
 * RHEL 5 ./ CentOS 5 with Linux kernel 2.6.18
 */

/****************************************************************************************
                                    INCLUDE FILES
 ***************************************************************************************/

#include "os-phoenix.h"
#include "os-impl-timebase.h"
#include "os-impl-tasks.h"

#include "os-shared-timebase.h"
#include "os-shared-idmap.h"
#include "os-shared-common.h"

#include <unistd.h>

/****************************************************************************************
                                EXTERNAL FUNCTION PROTOTYPES
 ***************************************************************************************/

/****************************************************************************************
                                INTERNAL FUNCTION PROTOTYPES
 ***************************************************************************************/

/****************************************************************************************
                                     DEFINES
 ***************************************************************************************/

/****************************************************************************************
                                     GLOBALS
 ***************************************************************************************/

OS_impl_timebase_internal_record_t OS_impl_timebase_table[OS_MAX_TIMEBASES];

/****************************************************************************************
                                INTERNAL FUNCTIONS
 ***************************************************************************************/

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
void OS_TimeBaseLock_Impl(const OS_object_token_t *token)
{
    OS_impl_timebase_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);

    mutexLock(impl->handler_mutex);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
void OS_TimeBaseUnlock_Impl(const OS_object_token_t *token)
{
    OS_impl_timebase_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);

    mutexUnlock(impl->handler_mutex);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *           Pends on the semaphore for the next timer tick
 *
 *-----------------------------------------------------------------*/
static uint32 OS_TimeBase_WaitImpl(osal_id_t timebase_id)
{
    OS_object_token_t                   token;
    OS_impl_timebase_internal_record_t *impl;
    uint32                              tick_time;

    tick_time = 0;

    if (OS_ObjectIdGetById(OS_LOCK_MODE_NONE, OS_OBJECT_TYPE_OS_TIMEBASE, timebase_id, &token) == OS_SUCCESS)
    {
        impl = OS_OBJECT_TABLE_GET(OS_impl_timebase_table, token);

        /*
         * Pend for the tick arrival
         */
        semaphoreDown(&impl->tick_sem, 0);

        /*
         * Determine how long this tick was.
         * Note that there are plenty of ways this become wrong if the timer
         * is reset right around the time a tick comes in.  However, it is
         * impossible to guarantee the behavior of a reset if the timer is running.
         * (This is not an expected use-case anyway; the timer should be set and forget)
         */
        if (impl->reset_flag == 0)
        {
            tick_time = impl->timer.interval_time;
        }
        else
        {
            tick_time        = impl->timer.start_time;
            impl->reset_flag = 0;
        }
    }

    return tick_time;
}
/****************************************************************************************
                      Entry point for helper thread
****************************************************************************************/

/*----------------------------------------------------------------
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *
 *-----------------------------------------------------------------*/

static void OS_TimeBase_CallbackThreadEntry(void *arg)
{
    OS_VoidPtrValueWrapper_t local_arg;

    /* cppcheck-suppress unreadVariable // intentional use of other union member */
    local_arg.opaque_arg = arg;
    OS_TimeBase_CallbackThread(local_arg.id);

    endthread();
}

/****************************************************************************************
                                 Timer thread
****************************************************************************************/

/*----------------------------------------------------------------
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *
 *-----------------------------------------------------------------*/
static void OS_TimeBase_TimerThread(void *arg)
{
    OS_impl_timebase_internal_record_t *impl;
    OS_impl_timebase_internal_timer_t  *timer;
    int                                 status;

    impl  = arg;
    timer = &impl->timer;

    mutexLock(timer->mutex);

    while (!timer->active && !timer->finish_rq)
    {
        /* Wait for the application to set the timer */
        condWait(timer->cond, timer->mutex, 0);
    }

    if (!timer->finish_rq)
    {
        /* Timer is now active, wait start_time */
        status = condWait(timer->cond, timer->mutex, timer->start_time);
        if ((status == -ETIME) && (timer->active))
        {
            /* Timeout, notify waiting thread */
            semaphoreUp(&impl->tick_sem);

            while (timer->active && (timer->interval_time > 0))
            {
                status = condWait(timer->cond, timer->mutex, timer->interval_time);
                if (status == -ETIME)
                {
                    /* Timeout, notify waiting thread */
                    semaphoreUp(&impl->tick_sem);
                }
            }
        }
    }

    timer->finished = true;
    condSignal(timer->cond);
    mutexUnlock(timer->mutex);

    endthread();
}

/****************************************************************************************
                                INITIALIZATION FUNCTION
 ***************************************************************************************/

/******************************************************************************
 *
 *  Purpose:  Initialize the timer implementation layer
 *
 *-----------------------------------------------------------------*/
int32 OS_Phoenix_TimeBaseAPI_Impl_Init(void)
{
    memset(OS_impl_timebase_table, 0, sizeof(OS_impl_timebase_table));

    /*
     * Phoenix provides 1us timer resolution
     */
    OS_SharedGlobalVars.TicksPerSecond = 1000000;

    /*
     * Calculate microseconds per tick
     */
    OS_SharedGlobalVars.MicroSecPerTick = 1000000 / OS_SharedGlobalVars.TicksPerSecond;

    return OS_SUCCESS;
}

/****************************************************************************************
                                   Time Base API
 ***************************************************************************************/

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TimeBaseCreate_Impl(const OS_object_token_t *token)
{
    int                                 status;
    OS_impl_timebase_internal_record_t *local;
    OS_timebase_internal_record_t      *timebase;
    OS_VoidPtrValueWrapper_t            arg;

    local    = OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);
    timebase = OS_OBJECT_TABLE_GET(OS_timebase_table, *token);

    /*
     * Set up the necessary OS constructs
     *
     * If an external sync function is used then there is nothing to do here -
     * we simply call that function and it should synchronize to the time source.
     *
     * If no external sync function is provided then this will set up a POSIX
     * timer to locally simulate the timer tick using the CPU clock.
     */

    /*
     * The handler_mutex deals with access to the callback list for this timebase
     */

    status = mutexCreate(&local->handler_mutex);
    if (status != 0)
    {
        OS_DEBUG("Error creating mutex: %d\n", status);
        return OS_TIMER_ERR_INTERNAL;
    }

    local->simulate_flag = (timebase->external_sync == NULL);
    if (local->simulate_flag)
    {
        timebase->external_sync = OS_TimeBase_WaitImpl;

        /*
         * The tick_sem is a simple semaphore posted by the thread and taken by the
         * timebase helper task (created later).
         */
        status = semaphoreCreate(&local->tick_sem, 0);
        if (status != 0)
        {
            OS_DEBUG("Error creating semaphore: %d\n", status);
            return OS_TIMER_ERR_INTERNAL;
        }

        /*
         * Create resources for the timer thread
         */
        status = mutexCreate(&local->timer.mutex);
        if (status != 0)
        {
            OS_DEBUG("Error creating mutex: %d\n", status);
            return OS_TIMER_ERR_INTERNAL;
        }

        status = condCreate(&local->timer.cond);
        if (status != 0)
        {
            OS_DEBUG("Error creating condition: %d\n", status);
            return OS_TIMER_ERR_INTERNAL;
        }

        local->timer.active    = false;
        local->timer.finish_rq = false;
        local->timer.finished  = false;

        memset(&arg, 0, sizeof(arg));

        /* cppcheck-suppress unreadVariable // intentional use of other union member */
        arg.id = OS_ObjectIdFromToken(token);

        status = beginthreadex(OS_TimeBase_TimerThread, 1, local->timer_stack, OS_TIMEBASE_THREAD_STACK_SIZE, local,
                               &local->timer_thread);
    }

    /*
     * Spawn a dedicated time base handler thread
     *
     * Note the thread will not actually start running until this function exits and releases
     * the global table lock.
     */

    if (status == 0)
    {
        /* cppcheck-suppress unreadVariable // intentional use of other union member */
        arg.id = OS_ObjectIdFromToken(token);
        status = beginthreadex(OS_TimeBase_CallbackThreadEntry, 0, local->thread_stack, OS_TIMEBASE_THREAD_STACK_SIZE,
                               arg.opaque_arg, &local->handler_thread);
    }

    /* check if task creation failed */
    if (status != 0)
    {
        /* Provide some feedback as to why this failed */
        OS_printf("beginthreadex failed: %d\n", status);
        resourceDestroy(local->handler_mutex);
        if (local->simulate_flag)
        {
            semaphoreDone(&local->tick_sem);
            resourceDestroy(local->timer.mutex);
            resourceDestroy(local->timer.cond);
        }
        return OS_TIMER_ERR_INTERNAL;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TimeBaseSet_Impl(const OS_object_token_t *token, uint32 start_time, uint32 interval_time)
{
    OS_impl_timebase_internal_record_t *local;
    OS_timebase_internal_record_t      *timebase;

    local    = OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);
    timebase = OS_OBJECT_TABLE_GET(OS_timebase_table, *token);

    /* There is only something to do here if we are generating a simulated tick */
    if (local->simulate_flag)
    {
        mutexLock(local->timer.mutex);
        if (start_time <= 0)
        {
            interval_time = 0; /* cannot have interval without start */
        }

        local->timer.interval_time = interval_time;

        /*
         ** The defined behavior is to not arm the timer if the start time is zero
         ** If the interval time is zero, then the timer will not be re-armed.
         */

        if (start_time > 0)
        {
            local->timer.start_time = start_time;
        }

        if (local->timer.interval_time > 0)
        {
            timebase->accuracy_usec = local->timer.interval_time;
        }
        else
        {
            timebase->accuracy_usec = local->timer.start_time;
        }
    }

    if (!local->reset_flag)
    {
        local->reset_flag = true;
    }

    if (local->simulate_flag)
    {
        local->timer.active = true;
        condSignal(local->timer.cond);
        mutexUnlock(local->timer.mutex);
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TimeBaseDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_timebase_internal_record_t *local;

    local = OS_OBJECT_TABLE_GET(OS_impl_timebase_table, *token);

    /*
    ** Delete the timer task
    */
    if (local->simulate_flag)
    {
        mutexLock(local->timer.mutex);

        local->timer.active    = false;
        local->timer.finish_rq = true;
        condSignal(local->timer.cond);

        mutexUnlock(local->timer.mutex);
    }

    signalPost(getpid(), local->handler_thread, signal_cancel);

    /* Cleanup resources */
    resourceDestroy(local->handler_mutex);

    if (local->simulate_flag)
    {
        /* Wait for the timer thread to exit */
        mutexLock(local->timer.mutex);
        while (!local->timer.finished)
        {
            condWait(local->timer.cond, local->timer.mutex, 0);
        }
        mutexUnlock(local->timer.mutex);

        /* Now we know the timer thread is not active and can cleanup it's resources safely */
        semaphoreDone(&local->tick_sem);
        resourceDestroy(local->timer.mutex);
        resourceDestroy(local->timer.cond);
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TimeBaseGetInfo_Impl(const OS_object_token_t *token, OS_timebase_prop_t *timer_prop)
{
    return OS_SUCCESS;
}
