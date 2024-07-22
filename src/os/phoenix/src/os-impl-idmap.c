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
 */

/****************************************************************************************
                                    INCLUDE FILES
 ***************************************************************************************/

#include "os-phoenix.h"
#include "bsp-impl.h"
#include <sched.h>

#include "os-shared-idmap.h"
#include "os-impl-idmap.h"

static OS_impl_objtype_lock_t OS_global_task_table_lock;
static OS_impl_objtype_lock_t OS_queue_table_lock;
static OS_impl_objtype_lock_t OS_bin_sem_table_lock;
static OS_impl_objtype_lock_t OS_mutex_table_lock;
static OS_impl_objtype_lock_t OS_count_sem_table_lock;
static OS_impl_objtype_lock_t OS_stream_table_lock;
static OS_impl_objtype_lock_t OS_dir_table_lock;
static OS_impl_objtype_lock_t OS_timebase_table_lock;
static OS_impl_objtype_lock_t OS_timecb_table_lock;
static OS_impl_objtype_lock_t OS_module_table_lock;
static OS_impl_objtype_lock_t OS_filesys_table_lock;
static OS_impl_objtype_lock_t OS_console_lock;
static OS_impl_objtype_lock_t OS_condvar_lock;

OS_impl_objtype_lock_t *const OS_impl_objtype_lock_table[OS_OBJECT_TYPE_USER] = {
    [OS_OBJECT_TYPE_UNDEFINED]   = NULL,
    [OS_OBJECT_TYPE_OS_TASK]     = &OS_global_task_table_lock,
    [OS_OBJECT_TYPE_OS_QUEUE]    = &OS_queue_table_lock,
    [OS_OBJECT_TYPE_OS_COUNTSEM] = &OS_count_sem_table_lock,
    [OS_OBJECT_TYPE_OS_BINSEM]   = &OS_bin_sem_table_lock,
    [OS_OBJECT_TYPE_OS_MUTEX]    = &OS_mutex_table_lock,
    [OS_OBJECT_TYPE_OS_STREAM]   = &OS_stream_table_lock,
    [OS_OBJECT_TYPE_OS_DIR]      = &OS_dir_table_lock,
    [OS_OBJECT_TYPE_OS_TIMEBASE] = &OS_timebase_table_lock,
    [OS_OBJECT_TYPE_OS_TIMECB]   = &OS_timecb_table_lock,
    [OS_OBJECT_TYPE_OS_MODULE]   = &OS_module_table_lock,
    [OS_OBJECT_TYPE_OS_FILESYS]  = &OS_filesys_table_lock,
    [OS_OBJECT_TYPE_OS_CONSOLE]  = &OS_console_lock,
    [OS_OBJECT_TYPE_OS_CONDVAR]  = &OS_condvar_lock,
};

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
void OS_Lock_Global_Impl(osal_objtype_t idtype)
{
    OS_impl_objtype_lock_t *impl;
    int                     ret;

    impl = OS_impl_objtype_lock_table[idtype];

    if (impl != NULL)
    {
        ret = mutexLock(impl->mutex);
        if (ret != 0)
        {
            OS_DEBUG("mutexLock failed: %d", ret);
        }
    }
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
void OS_Unlock_Global_Impl(osal_objtype_t idtype)
{
    OS_impl_objtype_lock_t *impl;
    int                     ret;

    impl = OS_impl_objtype_lock_table[idtype];

    if (impl != NULL)
    {
        /* Notify any waiting threads that the state _may_ have changed */
        ret = condBroadcast(impl->cond);
        if (ret != 0)
        {
            OS_DEBUG("condBroadcast failed: %d\n", ret);
            /* unexpected but keep going (not critical) */
        }

        ret = mutexUnlock(impl->mutex);
        if (ret != 0)
        {
            OS_DEBUG("mutexUnlock failed: %d\n", ret);
        }
    }
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
void OS_WaitForStateChange_Impl(osal_objtype_t objtype, uint32 attempts)
{
    OS_impl_objtype_lock_t *impl;
    time_t                  waitus;

    impl = OS_impl_objtype_lock_table[objtype];

    if (attempts <= 10)
    {
        /* Wait an increasing amount of time, starting at 10ms */
        waitus = attempts * attempts * 10000;
    }
    else
    {
        /* wait 1 second */
        waitus = 1000000;
    }

    condWait(impl->cond, impl->mutex, waitus);
}

/*---------------------------------------------------------------------------------------
   Name: OS_Phoenix_TableMutex_Init

   Purpose: Initialize the mutex that the OS API uses for the shared state tables

   returns: OS_SUCCESS or OS_ERROR
---------------------------------------------------------------------------------------*/
int32 OS_Phoenix_TableMutex_Init(osal_objtype_t idtype)
{
    int                     ret;
    OS_impl_objtype_lock_t *impl;

    impl = OS_impl_objtype_lock_table[idtype];
    if (impl == NULL)
    {
        return OS_SUCCESS;
    }

    ret = mutexCreate(&impl->mutex);
    if (ret != 0)
    {
        OS_DEBUG("mutexCreate failed: %d\n", ret);
        return OS_ERROR;
    }

    /* create a condition variable with default attributes.
     * This will be broadcast every time the object table changes */
    ret = condCreate(&impl->cond);
    if (ret != 0)
    {
        OS_DEBUG("condCreate failed: %d\n", ret);
        return OS_ERROR;
    }

    return OS_SUCCESS;
}
