#ifndef __REPLAY_IPL_MANAGER_H__
#define __REPLAY_IPL_MANAGER_H__

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY
	
#include "control/replay/Misc/IPLPacket.h"

class ReplayController;

class ReplayIPLManager
{
public:

	static void	OnRecordStart();				// Clears our stored IPL compare list
	static void	CollectData();					// Called each frame to collect data in a packet
	static void RecordData(ReplayController& controller, bool blockChange); // Called to record the packet in the replay clip
	static s32	GetMemoryUsageForFrame(bool blockChange);

	static void OnEntry();						// Called at start of playback, Saves current state

	static void SetOnExit()						// Set OnExit to be called at the next CStreaming::Update()
	{
		ms_bDoExit = true;
	}

	static void SetDoProcess()					// Set Process to be called at the next CStreaming::Update()
	{
		ms_bDoUpdate = true;
	}

	static void DoOnExit()
	{
		if( ms_bDoExit )
		{
			OnExit();
			ms_bDoExit = false;
		}
	}

	static void DoProcess()
	{
		if( ms_bDoUpdate )
		{
			Process();
			ms_bDoUpdate = false;
		}
	}

	static void SubmitNewIPLData(u32 count, const u32 *pData);

	static void	SetRayfireRecording(bool val)
	{
		ms_bRecordingRayfire = val;
	}
	static bool IsInitialSetupComplete() { return ms_InitialSetupComplete; }

private:
	static void OnExit();						// Called at end of playback, Re-sets state before replay (Note: Must be called from inside CStreaming::Update())
	static void	Process();						// Called each frame during playback (Note: Must be called from inside CStreaming::Update())

	// Flags for the deferred setting funcs
	static bool				ms_bDoExit;
	static bool				ms_bDoUpdate;
	
	static bool				ms_bDirty;
	static bool				ms_bRecordingRayfire;
	static IPLChangeData	ms_IPLChangeData;
	static atArray<u32>		ms_IPLStateBackup;
	static atArray<u32>		ms_LastIPLSet;
	static atArray<u32>		ms_EnabledIPLGroups;

	static CPacketIPL		ms_iplPacket;
	static bool				ms_iplMismatch;
	static bool				ms_InitialSetupComplete;
};

#endif	//GTA_REPLAY

#endif	//__REPLAY_IPL_MANAGER_H__
