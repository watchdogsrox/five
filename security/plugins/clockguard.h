#include "security/ragesecengine.h"
#ifndef SECURITY_CLOCKGUARDPLUGIN_H
#define SECURITY_CLOCKGUARDPLUGIN_H

#if USE_RAGESEC

#if RAGE_SEC_TASK_CLOCK_GUARD

bool ClockGuardPlugin_Create();
bool ClockGuardPlugin_Configure();
bool ClockGuardPlugin_Init();
bool ClockGuardPlugin_Work();

#endif // RAGE_SEC_TASK_CLOCK_GUARD

#endif  // USE_RAGESEC

#endif  // SECURITY_PERIODICTASKS_H