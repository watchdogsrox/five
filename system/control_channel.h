#ifndef INC_CONTROL_CHANNEL_H__
#define INC_CONTROL_CHANNEL_H__

#include "diag/channel.h"

RAGE_DECLARE_CHANNEL(Control)

#define controlAssert(cond)						RAGE_ASSERT(Control,cond)
#define controlAssertf(cond,fmt,...)				RAGE_ASSERTF(Control,cond,fmt,##__VA_ARGS__)
#define controlFatalAssertf(cond,fmt,...)		RAGE_FATALASSERTF(Control,cond,fmt,##__VA_ARGS__)
#define controlVerify(cond)						RAGE_VERIFY(Control,cond)
#define controlVerifyf(cond,fmt,...)				RAGE_VERIFYF(Control,cond,fmt,##__VA_ARGS__)
#define controlErrorf(fmt,...)					RAGE_ERRORF(Control,fmt,##__VA_ARGS__)
#define controlWarningf(fmt,...)					RAGE_WARNINGF(Control,fmt,##__VA_ARGS__)
#define controlDisplayf(fmt,...)					RAGE_DISPLAYF(Control,fmt,##__VA_ARGS__)
#define controlDebugf1(fmt,...)					RAGE_DEBUGF1(Control,fmt,##__VA_ARGS__)
#define controlDebugf2(fmt,...)					RAGE_DEBUGF2(Control,fmt,##__VA_ARGS__)
#define controlDebugf3(fmt,...)					RAGE_DEBUGF3(Control,fmt,##__VA_ARGS__)
#define controlLogf(severity,fmt,...)			RAGE_LOGF(Control,severity,fmt,##__VA_ARGS__)

#endif // INC_CONTROL_CHANNEL_H__
