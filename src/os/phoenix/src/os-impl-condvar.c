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

#include <sys/time.h>

#include "os-phoenix.h"
#include "os-shared-condvar.h"
#include "os-shared-idmap.h"
#include "os-impl-condvar.h"
#include "os-impl-gettime.h"

/* Tables where the OS object information is stored */
OS_impl_condvar_internal_record_t OS_impl_condvar_table[OS_MAX_CONDVARS];

/****************************************************************************************
                                  CONDVAR API
 ***************************************************************************************/

/*----------------------------------------------------------------
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *
 *-----------------------------------------------------------------*/
int32 OS_Phoenix_CondVarAPI_Impl_Init(void)
{
    memset(OS_impl_condvar_table, 0, sizeof(OS_impl_condvar_table));
    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_CondVarCreate_Impl(const OS_object_token_t *token, uint32 options)
{
    int32                              return_code;
    int                                status;
    OS_impl_condvar_internal_record_t *impl;
    struct condAttr                    attr;

    return_code = OS_SUCCESS;
    impl        = OS_OBJECT_TABLE_GET(OS_impl_condvar_table, *token);
    attr.clock  = OSAL_COND_SOURCE_CLOCK;

    /*
    ** create the underlying mutex
    */
    status = mutexCreate(&impl->mut);
    if (status != 0)
    {
        OS_DEBUG("Error: CondVar mutex could not be created. ID = %lu: %s\n",
                 OS_ObjectIdToInteger(OS_ObjectIdFromToken(token)), strerror(status));
        return_code = OS_ERROR;
    }
    else
    {
        /*
        ** create the condvar
        */
        status = condCreateWithAttr(&impl->cv, &attr);
        if (status != 0)
        {
            resourceDestroy(impl->mut);

            OS_DEBUG("Error: CondVar could not be created. ID = %lu: %s\n",
                     OS_ObjectIdToInteger(OS_ObjectIdFromToken(token)), strerror(status));
            return_code = OS_ERROR;
        }
    }

    return return_code;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_CondVarDelete_Impl(const OS_object_token_t *token)
{
    int32                              return_code;
    int                                status;
    OS_impl_condvar_internal_record_t *impl;

    return_code = OS_SUCCESS;
    impl        = OS_OBJECT_TABLE_GET(OS_impl_condvar_table, *token);

    status = resourceDestroy(impl->cv);
    if (status != 0)
    {
        return_code = OS_ERROR;
    }

    status = resourceDestroy(impl->mut);
    if (status != 0)
    {
        return_code = OS_ERROR;
    }

    return return_code;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_CondVarUnlock_Impl(const OS_object_token_t *token)
{
    int                                status;
    OS_impl_condvar_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_condvar_table, *token);

    status = mutexUnlock(impl->mut);
    if (status != 0)
    {
        return OS_ERROR;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_CondVarLock_Impl(const OS_object_token_t *token)
{
    int                                status;
    OS_impl_condvar_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_condvar_table, *token);

    status = mutexLock(impl->mut);
    if (status != 0)
    {
        return OS_ERROR;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_CondVarSignal_Impl(const OS_object_token_t *token)
{
    int                                status;
    OS_impl_condvar_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_condvar_table, *token);

    status = condSignal(impl->cv);
    if (status != 0)
    {
        return OS_ERROR;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_CondVarBroadcast_Impl(const OS_object_token_t *token)
{
    int                                status;
    OS_impl_condvar_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_condvar_table, *token);

    status = condBroadcast(impl->cv);
    if (status != 0)
    {
        return OS_ERROR;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_CondVarWait_Impl(const OS_object_token_t *token)
{
    int                                status;
    OS_impl_condvar_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_condvar_table, *token);

    status = condWait(impl->cv, impl->mut, 0);

    if (status != 0)
    {
        return OS_ERROR;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_CondVarTimedWait_Impl(const OS_object_token_t *token, const OS_time_t *abs_wakeup_time)
{
    int                                status;
    OS_impl_condvar_internal_record_t *impl;
    time_t                             timeout;

    impl    = OS_OBJECT_TABLE_GET(OS_impl_condvar_table, *token);
    timeout = OS_TimeGetTotalMicroseconds(*abs_wakeup_time);

    status = condWait(impl->cv, impl->mut, timeout);

    if (status == -ETIME)
    {
        return OS_ERROR_TIMEOUT;
    }
    if (status != 0)
    {
        return OS_ERROR;
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_CondVarGetInfo_Impl(const OS_object_token_t *token, OS_condvar_prop_t *condvar_prop)
{
    return OS_SUCCESS;
}
