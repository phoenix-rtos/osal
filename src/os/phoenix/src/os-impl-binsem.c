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
 * Purpose: This file contains some of the OS APIs abstraction layer
 *    implementation for POSIX
 */

/****************************************************************************************
                                    INCLUDE FILES
 ***************************************************************************************/

#include "os-phoenix.h"
#include "os-shared-idmap.h"
#include "os-shared-binsem.h"
#include "os-impl-binsem.h"

/* Tables where the OS object information is stored */
OS_impl_binsem_internal_record_t OS_impl_bin_sem_table[OS_MAX_BIN_SEMAPHORES];

/****************************************************************************************
                               BINARY SEMAPHORE API
 ***************************************************************************************/

/*
 * Note that Phoenix does not provide VxWorks-style binary semaphores that the OSAL API is modeled after.
 * Instead, semaphores are simulated using mutexes, condition variables, and a bit of internal state.
 */

/*---------------------------------------------------------------------------------------
   Name: OS_Posix_BinSemAPI_Impl_Init

   Purpose: Initialize the Binary Semaphore data structures

 ----------------------------------------------------------------------------------------*/
int32 OS_Phoenix_BinSemAPI_Impl_Init(void)
{
    memset(OS_impl_bin_sem_table, 0, sizeof(OS_impl_bin_sem_table));
    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_BinSemCreate_Impl(const OS_object_token_t *token, uint32 initial_value, uint32 options)
{
    int                               ret;
    OS_impl_binsem_internal_record_t *sem;

    /*
     * This preserves a bit of pre-existing functionality that was particular to binary sems:
     * if the initial value is greater than 1 it just silently used 1 without error.
     * (by contrast the counting semaphore will return an error)
     */
    if (initial_value > 1)
    {
        initial_value = 1;
    }

    sem = OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token);
    memset(sem, 0, sizeof(*sem));

    /*
     ** Initialize the mutex that is used with the condition variable
     */
    ret = mutexCreate(&sem->mut);
    if (ret != 0)
    {
        OS_DEBUG("Error: mutexCreate failed: %s\n", strerror(ret));
        return OS_SEM_FAILURE;
    }

    /*
     ** Initialize the condition variable
     */
    ret = condCreate(&sem->cv);
    if (ret != 0)
    {
        OS_DEBUG("Error: condCreate failed: %s\n", strerror(ret));
        return OS_SEM_FAILURE;
    }

    sem->current_value = initial_value;

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_BinSemDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_binsem_internal_record_t *sem;

    sem = OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token);

    (void)resourceDestroy(sem->cv);
    (void)resourceDestroy(sem->mut);

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_BinSemGive_Impl(const OS_object_token_t *token)
{
    OS_impl_binsem_internal_record_t *sem;

    sem = OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token);

    /*
     * Note there is a possibility that another thread is concurrently taking this sem,
     * and has just checked the current_value but not yet inside the cond_wait call.
     *
     * To address this possibility - the lock must be taken here.  This is unfortunate
     * as it means there may be a task switch when _giving_ a binary semaphore.  But the
     * alternative of having a BinSemGive not wake up the other thread is a bigger issue.
     *
     * Note: This lock should be readily available, with only minimal delay if any.
     * If a long delay occurs here, it means something is fundamentally wrong.
     */

    /* Lock the mutex ( not the table! ) */
    if (mutexLock(sem->mut) < 0)
    {
        return OS_SEM_FAILURE;
    }

    /* Binary semaphores are always set as "1" when given */
    sem->current_value = 1;

    /* unblock one thread that is waiting on this sem */
    (void)condSignal(sem->cv);

    (void)mutexUnlock(sem->mut);

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_BinSemFlush_Impl(const OS_object_token_t *token)
{
    OS_impl_binsem_internal_record_t *sem;

    sem = OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token);

    /* Lock the mutex ( not the table! ) */
    if (mutexLock(sem->mut) < 0)
    {
        return OS_SEM_FAILURE;
    }

    /* increment the flush counter.  Any other threads that are
     * currently pending in SemTake() will see the counter change and
     * return _without_ modifying the semaphore count.
     */
    ++sem->flush_request;

    /* unblock all threads that are be waiting on this sem */
    (void)condBroadcast(sem->cv);

    (void)mutexUnlock(sem->mut);

    return OS_SUCCESS;
}

/*---------------------------------------------------------------------------------------
   Name: OS_GenericBinSemTake_Impl

   Purpose: Helper function that takes a simulated binary semaphore with a "timespec" timeout
            If the value is zero this will block until either the value
            becomes nonzero (via SemGive) or the semaphore gets flushed.

---------------------------------------------------------------------------------------*/
static int32 OS_GenericBinSemTake_Impl(const OS_object_token_t *token, time_t timeoutUs, bool indefinite)
{
    sig_atomic_t                      flush_count;
    int32                             return_code;
    OS_impl_binsem_internal_record_t *sem;

    sem = OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token);

    /* Lock the mutex ( not the table! ) */
    if (mutexLock(sem->mut) < 0)
    {
        return OS_SEM_FAILURE;
    }

    return_code = OS_SUCCESS;

    /*
     * Note that for vxWorks compatibility, we need to stop pending on the semaphore
     * and return from this function under two possible circumstances:
     *
     *  a) the semaphore count was nonzero (may be pre-existing or due to a give)
     *     this is the normal case, we should decrement the count by 1 and return.
     *  b) the semaphore got "flushed"
     *     in this case ALL tasks are un-blocked and we do NOT decrement the count.
     */

    /*
     * first take a local snapshot of the flush request counter,
     * if it changes, we know that someone else called SemFlush.
     */
    flush_count = sem->flush_request;

    while ((sem->current_value == 0) && (sem->flush_request == flush_count) && (return_code == OS_SUCCESS))
    {
        /* Must pend until something changes */
        if (indefinite)
        {
            /* wait forever */
            (void)condWait(sem->cv, sem->mut, 0);
        }
        else
        {
            if (timeoutUs == 0)
            {
                /* just return with timeout */
                return_code = OS_SEM_TIMEOUT;
            }
            else
            {
                if (condWait(sem->cv, sem->mut, timeoutUs) == -ETIME)
                {
                    return_code = OS_SEM_TIMEOUT;
                }
            }
        }
    }

    /* If the flush counter did not change, set the value to zero */
    if ((return_code == OS_SUCCESS) && (sem->flush_request == flush_count))
    {
        sem->current_value = 0;
    }

    mutexUnlock(sem->mut);

    return return_code;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_BinSemTake_Impl(const OS_object_token_t *token)
{
    return (OS_GenericBinSemTake_Impl(token, 0, true));
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_BinSemTimedWait_Impl(const OS_object_token_t *token, uint32 msecs)
{
    time_t timeoutUs;

    timeoutUs = msecs * 1000;

    return (OS_GenericBinSemTake_Impl(token, timeoutUs, false));
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_BinSemGetInfo_Impl(const OS_object_token_t *token, OS_bin_sem_prop_t *sem_prop)
{
    OS_impl_binsem_internal_record_t *sem;

    sem = OS_OBJECT_TABLE_GET(OS_impl_bin_sem_table, *token);

    /* put the info into the structure */
    sem_prop->value = sem->current_value;
    return OS_SUCCESS;
}
