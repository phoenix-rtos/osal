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
#include "os-shared-mutex.h"
#include "os-shared-idmap.h"
#include "os-impl-mutex.h"

/* Tables where the OS object information is stored */
OS_impl_mutex_internal_record_t OS_impl_mutex_table[OS_MAX_MUTEXES];

/****************************************************************************************
                                  MUTEX API
 ***************************************************************************************/

/*----------------------------------------------------------------
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *
 *-----------------------------------------------------------------*/
int32 OS_Phoenix_MutexAPI_Impl_Init(void)
{
    memset(OS_impl_mutex_table, 0, sizeof(OS_impl_mutex_table));
    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_MutSemCreate_Impl(const OS_object_token_t *token, uint32 options)
{
    int                              return_code;
    OS_impl_mutex_internal_record_t *impl;
    struct lockAttr                  attr;

    impl      = OS_OBJECT_TABLE_GET(OS_impl_mutex_table, *token);
    attr.type = PH_LOCK_RECURSIVE;

    /*
    ** Try to create the mutex
    */
    return_code = mutexCreateWithAttr(&impl->id, &attr);
    if (return_code != 0)
    {
        OS_DEBUG("Error: Mutex could not be created. ID = %lu: %s\n", OS_ObjectIdToInteger(OS_ObjectIdFromToken(token)),
                 strerror(return_code));
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
int32 OS_MutSemDelete_Impl(const OS_object_token_t *token)
{
    int                              result;
    OS_impl_mutex_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_mutex_table, *token);

    result = resourceDestroy(impl->id);

    if (result != 0)
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
int32 OS_MutSemGive_Impl(const OS_object_token_t *token)
{
    int                              result;
    OS_impl_mutex_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_mutex_table, *token);

    /*
     ** Unlock the mutex
     */
    result = mutexUnlock(impl->id);
    if (result != 0)
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
int32 OS_MutSemTake_Impl(const OS_object_token_t *token)
{
    int                              result;
    OS_impl_mutex_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_mutex_table, *token);

    /*
    ** Lock the mutex
    */
    result = mutexLock(impl->id);
    if (result != 0)
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
int32 OS_MutSemGetInfo_Impl(const OS_object_token_t *token, OS_mut_sem_prop_t *mut_prop)
{
    return OS_SUCCESS;
}
