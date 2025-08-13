// 
// animation/anim_channel.h 
// 
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved. 
// 

#include "fwanimation/anim_channel.h"

#if !__NO_OUTPUT
#define animEntityDebugf(pEnt, fmt,...) if (pEnt) { animDebugf3("[%u:%u] - %s(%p): " fmt, fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount(), pEnt->GetModelName(), pEnt,##__VA_ARGS__); }
#define animTaskDebugf(pTask, fmt,...) if (pTask) { animDebugf3("[%u:%u] - Task:%p %s %s(%p): " fmt, fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount(), pTask, pTask->GetName().c_str() , pTask->GetEntity() ? pTask->GetEntity()->GetModelName() : "No entity", pTask->GetEntity(),##__VA_ARGS__); }
#else
#define animEntityDebugf(pEnt,fmt,...) do {} while(false)
#define animTaskDebugf(pTask,fmt,...) do {} while(false)
#endif //__NO_OUTPUT