#include "ReplayIPLManager.h"

#if GTA_REPLAY

#include "fwscene/stores/mapdatastore.h"
#include "system/pix.h"
#include "ReplayController.h"

bool			ReplayIPLManager::ms_bDoExit = false;
bool			ReplayIPLManager::ms_bDoUpdate = false;
bool			ReplayIPLManager::ms_bDirty = false;
bool			ReplayIPLManager::ms_bRecordingRayfire = false;
IPLChangeData	ReplayIPLManager::ms_IPLChangeData;
atArray<u32>	ReplayIPLManager::ms_IPLStateBackup;
atArray<u32>	ReplayIPLManager::ms_LastIPLSet;
atArray<u32>	ReplayIPLManager::ms_EnabledIPLGroups;

CPacketIPL		ReplayIPLManager::ms_iplPacket;
bool			ReplayIPLManager::ms_iplMismatch = false;
bool			ReplayIPLManager::ms_InitialSetupComplete = false;

// DON'T COMMIT
//OPTIMISATIONS_OFF()

void ReplayIPLManager::OnRecordStart()
{
	if(ms_LastIPLSet.size())
	{
		ms_LastIPLSet.Reset();
	}
}

void ReplayIPLManager::CollectData()
{
	PIX_AUTO_TAG(1,"ReplayIPLManager-CollectData");
	ms_iplMismatch = false;

	// If we're recording a rayfire, don't record any IPL changes until it's over
	if(ms_bRecordingRayfire == false)
	{
		fwMapDataStore::GetStore().GetEnabledIplGroups(ms_EnabledIPLGroups, true);

		ms_iplPacket.Store(ms_EnabledIPLGroups, ms_LastIPLSet);

		// Does the new list equate to the last list?
		if( ms_EnabledIPLGroups.size() != ms_LastIPLSet.size() )
		{
			ms_iplMismatch = true;
		}
		if( ms_iplMismatch == false )
		{
			const u32 *pOld = ms_LastIPLSet.GetElements();
			const u32 *pNew = ms_EnabledIPLGroups.GetElements();
			if(memcmp(pOld,pNew, sizeof(u32) * ms_EnabledIPLGroups.size()) != 0)
			{
				ms_iplMismatch = true;
			}
		}
	}
}


void ReplayIPLManager::RecordData(ReplayController& controller, bool blockChange)
{
	if( (ms_iplMismatch || blockChange) && ms_bRecordingRayfire == false )
	{
		controller.RecordPacket<CPacketIPL>(ms_iplPacket);
		ms_LastIPLSet = ms_EnabledIPLGroups;
	}
}


s32 ReplayIPLManager::GetMemoryUsageForFrame(bool blockChange)
{
	return ms_iplMismatch || blockChange ? sizeof(CPacketIPL) : 0;
}


void ReplayIPLManager::OnEntry()
{
	ms_IPLChangeData.m_Count = 0;

	// Save IPL state
	fwMapDataStore::GetStore().GetEnabledIplGroups(ms_IPLStateBackup);
}

void ReplayIPLManager::OnExit()
{
	// Flush render thread to remove any refs on any imaps we're about to remove. This causes a rendering "glitch", but should be OK at this point because the screen is obscured/blanked.
	gRenderThreadInterface.Flush();

	// Restore IPL state
	fwMapDataStore::GetStore().SetEnabledIplGroups(ms_IPLStateBackup);
	
	ms_InitialSetupComplete = false;
}

void ReplayIPLManager::Process()
{
	if(ms_bDirty)
	{
		// Update IPLS
		atArray<u32> iplArray;
		iplArray.CopyFrom(ms_IPLChangeData.m_IPLHashes, (unsigned short)ms_IPLChangeData.m_Count);
		fwMapDataStore::GetStore().SetEnabledIplGroups(iplArray, true);
		ms_bDirty = false;
	}
	ms_InitialSetupComplete = true;
}

void ReplayIPLManager::SubmitNewIPLData(u32 count, const u32 *pData)
{
	memcpy(ms_IPLChangeData.m_IPLHashes, pData, count*sizeof(u32));
	ms_IPLChangeData.m_Count = count;
	ms_bDirty = true;
}

#endif	//GTA_REPLAY
