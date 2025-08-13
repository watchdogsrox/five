#include "ReplayMgrProxy.h"

#if GTA_REPLAY

#include "Replay.h"

bool CReplayMgrProxy::IsEditModeActive()
{
	return CReplayMgr::IsEditModeActive();
}

#endif // GTA_REPLAY

