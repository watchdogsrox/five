#include "security/ragesecengine.h"
#ifndef SECURITY_DLLNAMECHECKPLUGIN_H
#define SECURITY_DLLNAMECHECKPLUGIN_H

#if USE_RAGESEC

#if RAGE_SEC_TASK_DLLNAMECHECK

bool DllNamePlugin_Init();
bool DllNamePlugin_Work();

#endif // RAGE_SEC_TASK_DLLNAMECHECK

#endif  // USE_RAGESEC

#endif  // SECURITY_DLLNAMEPLUGIN_H