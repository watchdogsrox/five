#ifndef __REPLAY_HIDE_MANAGER_H__
#define __REPLAY_HIDE_MANAGER_H__

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/Misc/HidePacket.h"

class CEntity;
class ReplayController;

class ReplayHideManager
{
public:

	static void	OnRecordStart();
	static void OnBlockChange();

	static void	AddNewHide(CEntity *pEntity, bool bVisible);
	static void RecordData(ReplayController& controller, bool blockChange);
	static s32	GetMemoryUsageForFrame(bool blockChange);

	static void OnEntry();			// Called at start of playback, Saves current state
	static void OnExit();			// Called at end of playback, Re-sets state before replay
	static void	Process();			// Called each frame during playback

	static bool IsNonReplayTrackedObjectType(CEntity *pEntity);

private:

	static void	ForAllNonReplayTrackedObjects( void(*cbFn)(CEntity *pEntity) );

	// Callbacks
	static void	CaptureAllCutsceneHidesCB(CEntity *pEntity);
	static void	RemoveAllReplayCutsceneHidesCB(CEntity *pEntity);

	static atArray<CutsceneNonRPObjectHide> m_CutsceneNoneRPTrackedObjectHides;
	static atArray<CutsceneNonRPObjectHide>	m_CutsceneNoneRPTrackedObjectHidesBlockChange;
};

#endif // GTA_REPLAY

#endif	//__REPLAY_HIDE_MANAGER_H__
