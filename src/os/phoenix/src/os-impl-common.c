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

#include "os-impl-tasks.h"
#include "os-impl-queues.h"
#include "os-impl-binsem.h"
#include "os-impl-countsem.h"
#include "os-impl-mutex.h"

#include "os-shared-common.h"
#include "os-shared-idmap.h"

/****************************************************************************************
                                     GLOBAL DATA
 ***************************************************************************************/

struct
{
    semaphore_t sem;
    bool        initialized;
} Phoenix_GlobalVars = {
    .initialized = false,
};

/*---------------------------------------------------------------------------------------
   Name: OS_API_Impl_Init

   Purpose: Initialize the tables that the OS API uses to keep track of information
            about objects

   returns: OS_SUCCESS or OS_ERROR
---------------------------------------------------------------------------------------*/
int32 OS_API_Impl_Init(osal_objtype_t idtype)
{
    int32 return_code;

    if (!Phoenix_GlobalVars.initialized)
    {
        semaphoreCreate(&Phoenix_GlobalVars.sem, 0);
        Phoenix_GlobalVars.initialized = true;
    }

    return_code = OS_Phoenix_TableMutex_Init(idtype);
    if (return_code != OS_SUCCESS)
    {
        return return_code;
    }

    switch (idtype)
    {
        case OS_OBJECT_TYPE_OS_TASK:
            return_code = OS_Phoenix_TaskAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_QUEUE:
            return_code = OS_Phoenix_QueueAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_BINSEM:
            return_code = OS_Phoenix_BinSemAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_COUNTSEM:
            return_code = OS_Phoenix_CountSemAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_MUTEX:
            return_code = OS_Phoenix_MutexAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_MODULE:
            return_code = OS_Phoenix_ModuleAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_TIMEBASE:
            return_code = OS_Phoenix_TimeBaseAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_STREAM:
            return_code = OS_Phoenix_StreamAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_DIR:
            return_code = OS_Phoenix_DirAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_FILESYS:
            return_code = OS_Phoenix_FileSysAPI_Impl_Init();
            break;
        case OS_OBJECT_TYPE_OS_CONDVAR:
            return_code = OS_Phoenix_CondVarAPI_Impl_Init();
            break;
        default:
            break;
    }

    return return_code;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
void OS_IdleLoop_Impl(void)
{
    semaphoreDown(&Phoenix_GlobalVars.sem, 0);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
void OS_ApplicationShutdown_Impl(void)
{
    semaphoreUp(&Phoenix_GlobalVars.sem);
}
