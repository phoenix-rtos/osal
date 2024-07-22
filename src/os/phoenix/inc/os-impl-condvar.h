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
 *
 * \ingroup  phoenix
 *
 */

#ifndef OS_IMPL_CONDVAR_H
#define OS_IMPL_CONDVAR_H

#include "osconfig.h"
#include <sys/threads.h>

/* CondVares */
typedef struct
{
    handle_t mut;
    handle_t cv;
} OS_impl_condvar_internal_record_t;

/* Tables where the OS object information is stored */
extern OS_impl_condvar_internal_record_t OS_impl_condvar_table[OS_MAX_CONDVARS];

#endif /* OS_IMPL_CONDVAR_H */
