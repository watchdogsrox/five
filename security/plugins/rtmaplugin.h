#include "security/ragesecengine.h"
#ifndef SECURITY_RTMAPLUGIN_H
#define SECURITY_RTMAPLUGIN_H

#if USE_RAGESEC

#if RAGE_SEC_TASK_RTMA
// Rage headers
#include "system/criticalsection.h"
bool RtmaPlugin_Init();
bool RtmaPlugin_Work();

#endif // RAGE_SEC_TASK_RTMA

#endif  // USE_RAGESEC

#endif  // SECURITY_RTMAPLUGIN_H