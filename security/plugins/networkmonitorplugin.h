#include "security/ragesecengine.h"
#ifndef SECURITY_NETWORKMONITORPLUGIN_H
#define SECURITY_NETWORKMONITORPLUGIN_H

#if USE_RAGESEC

#if RAGE_SEC_TASK_NETWORK_MONITOR_CHECK

bool NetworkMonitorPlugin_Init();
bool NetworkMonitorPlugin_Work();

#endif // RAGE_SEC_TASK_NETWORK_MONITOR_CHECK

#endif  // USE_RAGESEC

#endif  // SECURITY_PERIODICTASKS_H