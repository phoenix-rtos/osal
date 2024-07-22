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
#include "os-impl-console.h"
#include "os-impl-tasks.h"

#include "os-shared-idmap.h"
#include "os-shared-printf.h"
#include "os-shared-common.h"

/*
 * By default the console output is always asynchronous
 * (equivalent to "OS_UTILITY_TASK_ON" being set)
 *
 * This option was removed from osconfig.h and now is
 * assumed to always be on.
 */
#define OS_CONSOLE_ASYNC         true
#define OS_CONSOLE_TASK_PRIORITY OS_UTILITYTASK_PRIORITY

/* Tables where the OS object information is stored */
OS_impl_console_internal_record_t OS_impl_console_table[OS_MAX_CONSOLES];

/********************************************************************/
/*                 CONSOLE OUTPUT                                   */
/********************************************************************/

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
void OS_ConsoleWakeup_Impl(const OS_object_token_t *token)
{
    OS_impl_console_internal_record_t *local;

    local = OS_OBJECT_TABLE_GET(OS_impl_console_table, *token);

    /* post the sem for the utility task to run */
    semaphoreUp(&local->data_sem);
}

/*----------------------------------------------------------------
 *
 *  Purpose: Local Helper function
 *           Implements the console output task
 *
 *-----------------------------------------------------------------*/
static void OS_ConsoleTask_Entry(void *arg)
{
    OS_VoidPtrValueWrapper_t           local_arg;
    OS_impl_console_internal_record_t *local;
    OS_object_token_t                  token;

    /* cppcheck-suppress unreadVariable // intentional use of other union member */
    local_arg.opaque_arg = arg;
    if (OS_ObjectIdGetById(OS_LOCK_MODE_REFCOUNT, OS_OBJECT_TYPE_OS_CONSOLE, local_arg.id, &token) == OS_SUCCESS)
    {
        local = OS_OBJECT_TABLE_GET(OS_impl_console_table, token);

        /* Loop forever (unless shutdown is set) */
        while (OS_SharedGlobalVars.GlobalState != OS_SHUTDOWN_MAGIC_NUMBER)
        {
            OS_ConsoleOutput_Impl(&token);
            semaphoreDown(&local->data_sem, 0);
        }
        OS_ObjectIdRelease(&token);
    }

    endthread();
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_ConsoleCreate_Impl(const OS_object_token_t *token)
{
    OS_impl_console_internal_record_t *local;
    OS_console_internal_record_t      *console;
    int                                status;
    OS_VoidPtrValueWrapper_t           local_arg = {0};

    console = OS_OBJECT_TABLE_GET(OS_console_table, *token);
    local   = OS_OBJECT_TABLE_GET(OS_impl_console_table, *token);

    if (token->obj_idx != 0)
    {
        return OS_ERR_NOT_IMPLEMENTED;
    }

    if (!console->IsAsync)
    {
        return OS_SUCCESS;
    }

    if (semaphoreCreate(&local->data_sem, 0) < 0)
    {
        return OS_SEM_FAILURE;
    }

    /* cppcheck-suppress unreadVariable // intentional use of other union member */
    local_arg.id = OS_ObjectIdFromToken(token);
    status       = beginthread(OS_ConsoleTask_Entry, OS_PriorityRemap(OS_CONSOLE_TASK_PRIORITY), local->stack, OS_CONSOLE_TASK_STACKSIZE,
                               local_arg.opaque_arg);
    if (status != 0)
    {
        semaphoreDone(&local->data_sem);
        OS_DEBUG("Error: Cannot create console task: %d\n", status);
        return OS_ERROR;
    }

    return OS_SUCCESS;
}
