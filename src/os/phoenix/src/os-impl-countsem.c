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
#include "os-impl-countsem.h"
#include "os-shared-countsem.h"
#include "os-shared-idmap.h"

/****************************************************************************************
                                     DEFINES
 ***************************************************************************************/

#define SEM_VALUE_MAX 0x7FFFFFFF

/****************************************************************************************
                                   GLOBAL DATA
 ***************************************************************************************/

/* Tables where the OS object information is stored */
OS_impl_countsem_internal_record_t OS_impl_count_sem_table[OS_MAX_COUNT_SEMAPHORES];

/****************************************************************************************
                               COUNTING SEMAPHORE API
 ***************************************************************************************/

/*---------------------------------------------------------------------------------------
   Name: OS_Phoenix_CountSemAPI_Impl_Init

   Purpose: Initialize the Counting Semaphore data structures

---------------------------------------------------------------------------------------*/
int32 OS_Phoenix_CountSemAPI_Impl_Init(void)
{
    memset(OS_impl_count_sem_table, 0, sizeof(OS_impl_count_sem_table));
    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_CountSemCreate_Impl(const OS_object_token_t *token, uint32 sem_initial_value, uint32 options)
{
    OS_impl_countsem_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_count_sem_table, *token);

    if (sem_initial_value > SEM_VALUE_MAX)
    {
        return OS_INVALID_SEM_VALUE;
    }

    if (semaphoreCreate(&impl->id, sem_initial_value) < 0)
    {
        return OS_SEM_FAILURE;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_CountSemDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_countsem_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_count_sem_table, *token);

    if (semaphoreDone(&impl->id) < 0)
    {
        return OS_SEM_FAILURE;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_CountSemGive_Impl(const OS_object_token_t *token)
{
    OS_impl_countsem_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_count_sem_table, *token);

    if (semaphoreUp(&impl->id) < 0)
    {
        return OS_SEM_FAILURE;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_CountSemTake_Impl(const OS_object_token_t *token)
{
    OS_impl_countsem_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_count_sem_table, *token);

    if (semaphoreDown(&impl->id, 0) < 0)
    {
        return OS_SEM_FAILURE;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_CountSemTimedWait_Impl(const OS_object_token_t *token, uint32 msecs)
{
    int32                               return_code;
    int                                 result;
    OS_impl_countsem_internal_record_t *impl;
    time_t                              usecs;

    impl  = OS_OBJECT_TABLE_GET(OS_impl_count_sem_table, *token);
    usecs = msecs * 1000;

    if (usecs == 0)
    {
        /* If timeout is zero, try to take the semaphore and wait shortest possible time */
        usecs = 1;
    }

    result = semaphoreDown(&impl->id, usecs);
    if (result == 0)
    {
        return_code = OS_SUCCESS;
    }
    else if (result == -ETIME)
    {
        return_code = OS_SEM_TIMEOUT;
    }
    else
    {
        /* unspecified failure */
        return_code = OS_SEM_FAILURE;
    }

    return return_code;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_CountSemGetInfo_Impl(const OS_object_token_t *token, OS_count_sem_prop_t *count_prop)
{
    /* PHOENIX does not provide an API to get the value */
    return OS_ERR_NOT_IMPLEMENTED;
}
