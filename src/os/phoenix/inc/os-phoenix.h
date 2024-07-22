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

#ifndef OS_PHOENIX_H
#define OS_PHOENIX_H

/****************************************************************************************
                                    COMMON INCLUDE FILES
****************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include "os-shared-globaldefs.h"
#include "osapi-task.h"

/****************************************************************************************
                                     DEFINES
****************************************************************************************/

/****************************************************************************************
                                    TYPEDEFS
****************************************************************************************/

/****************************************************************************************
                                   GLOBAL DATA
****************************************************************************************/

/****************************************************************************************
                       PHOENIX IMPLEMENTATION FUNCTION PROTOTYPES
****************************************************************************************/

int32 OS_Phoenix_BinSemAPI_Impl_Init(void);
int32 OS_Phoenix_TaskAPI_Impl_Init(void);
int32 OS_Phoenix_QueueAPI_Impl_Init(void);
int32 OS_Phoenix_CountSemAPI_Impl_Init(void);
int32 OS_Phoenix_MutexAPI_Impl_Init(void);
int32 OS_Phoenix_TimeBaseAPI_Impl_Init(void);
int32 OS_Phoenix_ModuleAPI_Impl_Init(void);
int32 OS_Phoenix_StreamAPI_Impl_Init(void);
int32 OS_Phoenix_DirAPI_Impl_Init(void);
int32 OS_Phoenix_FileSysAPI_Impl_Init(void);
int32 OS_Phoenix_CondVarAPI_Impl_Init(void);

int OS_PriorityRemap(osal_priority_t InputPri);

int32 OS_Phoenix_TableMutex_Init(osal_objtype_t idtype);

#endif /* OS_PHOENIX_H */
