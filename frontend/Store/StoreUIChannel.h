#ifndef STOREUI_CHANNEL_H 
#define STOREUI_CHANNEL_H

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(storeUI)

#define storeUIAssert(cond)						RAGE_ASSERT(storeUI,cond)
#define storeUIAssertf(cond,fmt,...)			RAGE_ASSERTF(storeUI,cond,fmt,##__VA_ARGS__)
#define storeUIFatalAssertf(cond,fmt,...)		RAGE_FATALASSERTF(storeUI,cond,fmt,##__VA_ARGS__)
#define storeUIVerifyf(cond,fmt,...)			RAGE_VERIFYF(storeUI,cond,fmt,##__VA_ARGS__)
#define storeUIErrorf(fmt,...)					RAGE_ERRORF(storeUI,fmt,##__VA_ARGS__)
#define storeUIWarningf(fmt,...)				RAGE_WARNINGF(storeUI,fmt,##__VA_ARGS__)
#define storeUIDisplayf(fmt,...)				RAGE_DISPLAYF(storeUI,fmt,##__VA_ARGS__)
#define storeUIDebugf1(fmt,...)					RAGE_DEBUGF1(storeUI,fmt,##__VA_ARGS__)
#define storeUIDebugf2(fmt,...)					RAGE_DEBUGF2(storeUI,fmt,##__VA_ARGS__)
#define storeUIDebugf3(fmt,...)					RAGE_DEBUGF3(storeUI,fmt,##__VA_ARGS__)
#define storeUILogf(severity,fmt,...)			RAGE_LOGF(storeUI,severity,fmt,##__VA_ARGS__)

#endif // STOREUI_CHANNEL_H