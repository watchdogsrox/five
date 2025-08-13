#include "security/ragesecengine.h"
#ifndef SECURITY_VIDEOMODIFICATIONPLUGIN_H
#define SECURITY_VIDEOMODIFICATIONPLUGIN_H

#if USE_RAGESEC

#if RAGE_SEC_TASK_VIDEO_MODIFICATION_CHECK

bool VideoModificationPlugin_Create();
bool VideoModificationPlugin_Configure();
bool VideoModificationPlugin_Init();
bool VideoModificationPlugin_Work();

#endif // RAGE_SEC_TASK_VIDEO_MODIFICATION_CHECK

#endif  // USE_RAGESEC

#endif  // SECURITY_PERIODICTASKS_H