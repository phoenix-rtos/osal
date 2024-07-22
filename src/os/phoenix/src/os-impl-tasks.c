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

#include <unistd.h>

#include "os-phoenix.h"
#include "bsp-impl.h"

#include "os-impl-tasks.h"

#include "os-shared-task.h"
#include "os-shared-idmap.h"

#define OS_PHOENIX_MAX_PRIORITY 7

/* Tables where the OS object information is stored */
OS_impl_task_internal_record_t OS_impl_task_table[OS_MAX_TASKS];

__thread osal_id_t OS_thread_local_task_id = 0;

/*
 * Local Function Prototypes
 */

/*----------------------------------------------------------------------------
 * Name: OS_PriorityRemap
 *
 * Purpose: Remaps the OSAL priority into one that is viable for this OS
 *
 * Note: This implementation assumes that InputPri has already been verified
 * to be within the range of [0,OS_MAX_TASK_PRIORITY]
 *
----------------------------------------------------------------------------*/
int OS_PriorityRemap(osal_priority_t InputPri)
{
    int OutputPri;

    /* Phoenix uses a 0-7 priority range */
    OutputPri = InputPri / ((OS_MAX_TASK_PRIORITY + 1) / (OS_PHOENIX_MAX_PRIORITY + 1));

    return OutputPri;
}

/*---------------------------------------------------------------------------------------
 *  Name: OS_PhoenixTaskEntry
 *
 *  Purpose: A Simple Phoenix-compatible entry point that calls the real task function
 *
 *  NOTES: This wrapper function is only used locally by OS_TaskCreate below
 *
 *---------------------------------------------------------------------------------------*/
static void OS_PhoenixTaskEntry(void *arg)
{
    OS_VoidPtrValueWrapper_t local_arg;

    /* cppcheck-suppress unreadVariable // intentional use of other union member */
    local_arg.opaque_arg    = arg;
    OS_thread_local_task_id = local_arg.id;
    OS_TaskEntryPoint(local_arg.id);
}

/*
 *********************************************************************************
 *          TASK API
 *********************************************************************************
 */

/*---------------------------------------------------------------------------------------
 *  Name: OS_Phoenix_TaskAPI_Impl_Init
 *
 *  Purpose: Local helper routine, not part of OSAL API.
 *
 *----------------------------------------------------------------------------------------*/
int32 OS_Phoenix_TaskAPI_Impl_Init(void)
{
    memset(OS_impl_task_table, 0, sizeof(OS_impl_task_table));
    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskCreate_Impl(const OS_object_token_t *token, uint32 flags)
{
    OS_VoidPtrValueWrapper_t        arg;
    int                             result;
    OS_impl_task_internal_record_t *impl;
    OS_task_internal_record_t      *task;
    unsigned int                    priority;

    impl = OS_OBJECT_TABLE_GET(OS_impl_task_table, *token);
    task = OS_OBJECT_TABLE_GET(OS_task_table, *token);

    if (task->stack_pointer == NULL)
    {
        /*
         * Phoenix does not provide a way to deallocate
         * a taskInit-provided stack when a task exits.
         *
         * So in this case we will find the leftover heap
         * buffer when OSAL reuses this local record block.
         *
         * If that leftover heap buffer is big enough it
         * can be used directly.  Otherwise it needs to be
         * re-created.
         */

        if (task->stack_size > impl->heap_block_size)
        {
            free(impl->heap_block);
            impl->heap_block_size = 0;

            impl->heap_block = malloc(task->stack_size);
            if (impl->heap_block == NULL)
            {
                return OS_ERROR;
            }

            impl->heap_block_size = task->stack_size;
            task->stack_pointer   = impl->heap_block;
        }
    }

    memset(&arg, 0, sizeof(arg));

    priority = OS_PriorityRemap(task->priority);

    /* cppcheck-suppress unreadVariable // intentional use of other union member */
    arg.id = OS_ObjectIdFromToken(token);

    result =
        beginthreadex(OS_PhoenixTaskEntry, priority, task->stack_pointer, task->stack_size, arg.opaque_arg, &impl->id);

    return (result != 0) ? OS_ERROR : OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskDetach_Impl(const OS_object_token_t *token)
{
    return OS_ERR_NOT_IMPLEMENTED;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskMatch_Impl(const OS_object_token_t *token)
{
    OS_impl_task_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_task_table, *token);

    if (gettid() != impl->id)
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
int32 OS_TaskDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_task_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_task_table, *token);

    if (gettid() == impl->id)
    {
        endthread();
    }
    else
    {
        signalPost(getpid(), impl->id, signal_cancel);
        threadJoin(impl->id, 0);
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
void OS_TaskExit_Impl()
{
    endthread();
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskDelay_Impl(uint32 millisecond)
{
    struct timespec delay;
    struct timespec remaining;
    int             status;

    delay.tv_sec  = millisecond / 1000;
    delay.tv_nsec = (millisecond % 1000) * 1000000L;

    status = nanosleep(&delay, &remaining);
    while ((status == -1) && (errno == EINTR))
    {
        delay  = remaining;
        status = nanosleep(&delay, &remaining);
    }

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskSetPriority_Impl(const OS_object_token_t *token, osal_priority_t new_priority)
{
    /* TODO */
    return OS_ERR_NOT_IMPLEMENTED;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskRegister_Impl(osal_id_t global_task_id)
{
    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
osal_id_t OS_TaskGetId_Impl(void)
{
    return OS_thread_local_task_id;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskGetInfo_Impl(const OS_object_token_t *token, OS_task_prop_t *task_prop)
{
    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_TaskValidateSystemData_Impl(const void *sysdata, size_t sysdata_size)
{
    if (sysdata == NULL || sysdata_size != sizeof(handle_t))
    {
        return OS_INVALID_POINTER;
    }
    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
bool OS_TaskIdMatchSystemData_Impl(void *ref, const OS_object_token_t *token, const OS_common_record_t *obj)
{
    const handle_t                 *target = (const handle_t *)ref;
    OS_impl_task_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_task_table, *token);

    return (*target == impl->id);
}
