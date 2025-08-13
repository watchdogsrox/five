#ifndef _GESTUREMANAGER_H_
#define _GESTUREMANAGER_H_

// Rage headers

#include "diag/channel.h"
#include "fwanimation/animdefines.h"

// Gta headers

#include "task/System/TaskHelpers.h"

RAGE_DECLARE_CHANNEL(gesture)

#define gestureAssert(cond)						RAGE_ASSERT(gesture,cond)
#define gestureAssertf(cond,fmt,...)			RAGE_ASSERTF(gesture,cond,fmt,##__VA_ARGS__)
#define gestureVerifyf(cond,fmt,...)			RAGE_VERIFYF(gesture,cond,fmt,##__VA_ARGS__)
#define gestureErrorf(fmt,...)					RAGE_ERRORF(gesture,fmt,##__VA_ARGS__)
#define gestureWarningf(fmt,...)				RAGE_WARNINGF(gesture,fmt,##__VA_ARGS__)
#define gestureDisplayf(fmt,...)				RAGE_DISPLAYF(gesture,fmt,##__VA_ARGS__)
#define gestureDebugf1(fmt,...)					RAGE_DEBUGF1(gesture,fmt,##__VA_ARGS__)
#define gestureDebugf2(fmt,...)					RAGE_DEBUGF2(gesture,fmt,##__VA_ARGS__)
#define gestureDebugf3(fmt,...)					RAGE_DEBUGF3(gesture,fmt,##__VA_ARGS__)
#define gestureLogf(severity,fmt,...)			RAGE_LOGF(gesture,severity,fmt,##__VA_ARGS__)
#define gestureCondLogf(cond,severity,fmt,...)	RAGE_CONDLOGF(cond,gesture,severity,fmt,##__VA_ARGS__)

#if __BANK
#define gestureManagerDebugf(fmt,...)			XPARAM(debugGestures); do { if(PARAM_debugGestures.Get()) { gestureDisplayf(fmt,##__VA_ARGS__); } } while(false)
#else
#define gestureManagerDebugf(fmt,...)			do { } while(false)
#endif

class CGestureManager
{
public:

	/* Vehicles */

	void RequestVehicleGestureClipSet(fwMvClipSetId clipSetId);
	bool CanUseVehicleGestureClipSet(fwMvClipSetId clipSetId) const;

	/* Scenarios */

	void RequestScenarioGestureClipSet(fwMvClipSetId clipSetId);
	bool CanUseScenarioGestureClipSet(fwMvClipSetId clipSetId) const;

	/* Static operations */

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

protected:

	/* Destructor */

	virtual ~CGestureManager();

	/* Constructors */

	CGestureManager();
};

extern CGestureManager *g_pGestureManager;

#endif //_GESTUREMANAGER_H_
