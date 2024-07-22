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

#include "os-impl-queues.h"
#include "os-shared-queue.h"
#include "os-shared-idmap.h"

/* Tables where the OS object information is stored */
OS_impl_queue_internal_record_t OS_impl_queue_table[OS_MAX_QUEUES];

/****************************************************************************************
                                MESSAGE QUEUE API
 ***************************************************************************************/

/*---------------------------------------------------------------------------------------
   Name: OS_Phoenix_QueueAPI_Impl_Init

   Purpose: Initialize the Queue data structures

 ----------------------------------------------------------------------------------------*/
int32 OS_Phoenix_QueueAPI_Impl_Init(void)
{
    memset(OS_impl_queue_table, 0, sizeof(OS_impl_queue_table));
    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_QueueCreate_Impl(const OS_object_token_t *token, uint32 flags)
{
    OS_impl_queue_internal_record_t *impl;
    OS_queue_internal_record_t      *queue;

    impl  = OS_OBJECT_TABLE_GET(OS_impl_queue_table, *token);
    queue = OS_OBJECT_TABLE_GET(OS_queue_table, *token);

    /* Allocate all necessary memory during queue creation */
    impl->messages = malloc(queue->max_depth * sizeof(OS_impl_queue_internal_message_t));
    if (impl->messages == NULL)
    {
        OS_DEBUG("Error: OS_QueueCreate failed: out of memory\n");
        return OS_ERROR;
    }

    impl->data_block = malloc(queue->max_depth * queue->max_size);
    if (impl->data_block == NULL)
    {
        OS_DEBUG("Error: OS_QueueCreate failed: out of memory\n");
        free(impl->messages);
        return OS_ERROR;
    }

    if (condCreate(&impl->cond) != 0)
    {
        OS_DEBUG("Error: OS_QueueCreate failed: condCreate\n");
        free(impl->messages);
        free(impl->data_block);
        return OS_ERROR;
    }

    if (mutexCreate(&impl->lock) != 0)
    {
        OS_DEBUG("Error: OS_QueueCreate failed: mutexCreate\n");
        free(impl->messages);
        free(impl->data_block);
        resourceDestroy(impl->cond);
        return OS_ERROR;
    }

    impl->head         = 0;
    impl->tail         = -1;
    impl->current_size = 0;

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_QueueDelete_Impl(const OS_object_token_t *token)
{
    OS_impl_queue_internal_record_t *impl;

    impl = OS_OBJECT_TABLE_GET(OS_impl_queue_table, *token);

    free(impl->messages);
    free(impl->data_block);
    resourceDestroy(impl->cond);
    resourceDestroy(impl->lock);

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_QueueGet_Impl(const OS_object_token_t *token, void *data, size_t size, size_t *size_copied, int32 timeout)
{
    OS_impl_queue_internal_record_t  *impl;
    OS_queue_internal_record_t       *queue;
    OS_impl_queue_internal_message_t *msg;
    time_t                            timeoutUs;

    impl  = OS_OBJECT_TABLE_GET(OS_impl_queue_table, *token);
    queue = OS_OBJECT_TABLE_GET(OS_queue_table, *token);

    if (timeout != OS_PEND)
    {
        timeoutUs = timeout * 1000;
    }
    else
    {
        timeoutUs = 0;
    }

    mutexLock(impl->lock);
    while (impl->current_size == 0)
    {
        if (condWait(impl->cond, impl->lock, timeoutUs) == -ETIME)
        {
            mutexUnlock(impl->lock);
            return OS_QUEUE_TIMEOUT;
        }
    }

    msg = &impl->messages[impl->head];

    *size_copied = msg->size;
    memcpy(data, msg->data, msg->size);

    impl->head = (impl->head + 1) % queue->max_depth;
    impl->current_size--;

    mutexUnlock(impl->lock);

    return OS_SUCCESS;
}

/*----------------------------------------------------------------
 *
 *  Purpose: Implemented per internal OSAL API
 *           See prototype for argument/return detail
 *
 *-----------------------------------------------------------------*/
int32 OS_QueuePut_Impl(const OS_object_token_t *token, const void *data, size_t size, uint32 flags)
{
    OS_impl_queue_internal_record_t  *impl;
    OS_queue_internal_record_t       *queue;
    OS_impl_queue_internal_message_t *msg;

    impl  = OS_OBJECT_TABLE_GET(OS_impl_queue_table, *token);
    queue = OS_OBJECT_TABLE_GET(OS_queue_table, *token);

    mutexLock(impl->lock);
    if (impl->current_size == queue->max_depth)
    {
        mutexUnlock(impl->lock);
        return OS_QUEUE_FULL;
    }

    impl->tail = (impl->tail + 1) % queue->max_depth;
    msg        = &impl->messages[impl->tail];

    msg->size = size;
    msg->data = (void *)((cpuaddr)impl->data_block + (impl->tail * queue->max_size));
    memcpy(msg->data, data, size);

    impl->current_size++;

    condSignal(impl->cond);

    mutexUnlock(impl->lock);

    return OS_SUCCESS;
}
