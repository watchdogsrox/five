#include "ReplayMeshSetManager.h"

#if GTA_REPLAY

#include "system/pix.h"
#include "ReplayController.h"

atFixedArray<ReplayMovieMeshSet, MOVIE_MESH_MAX_SETS>	ReplayMeshSetManager::ms_MeshSetInfo;
atFixedArray<ReplayMovieMeshSet, MOVIE_MESH_MAX_SETS>	ReplayMeshSetManager::ms_LastMeshSetInfo;
bool													ReplayMeshSetManager::ms_bDirty = false;

CPacketMeshSet											ReplayMeshSetManager::ms_meshSetPacket;
bool													ReplayMeshSetManager::ms_meshSetMismatch = false;

void ReplayMeshSetManager::OnRecordStart()
{
	ms_LastMeshSetInfo.Reset();
}

void ReplayMeshSetManager::GetMeshSet(atFixedArray<ReplayMovieMeshSet, MOVIE_MESH_MAX_SETS> &info)
{
	info.Reset();
	for (int i = 0; i < MOVIE_MESH_MAX_SETS; i++)
	{
		MovieMeshSet &thisSet = g_movieMeshMgr.GetSet(i);
		ReplayMovieMeshSet	thisReplaySet;
		if(thisSet.m_handle != INVALID_MOVIE_MESH_SET_HANDLE)
		{
			thisReplaySet.SetName(thisSet.m_loadRequestName.c_str());
			info.Append() = thisReplaySet;
		}
	}
}

void ReplayMeshSetManager::CollectData()
{
	PIX_AUTO_TAG(1,"ReplayMeshSetManager-CollectData");

	ms_meshSetMismatch = false;

	// Get list of currently active sets
	//atFixedArray<ReplayMovieMeshSet, MOVIE_MESH_MAX_SETS>	movieMeshSetInfo;
	GetMeshSet(ms_MeshSetInfo);

	ms_meshSetPacket.Store(ms_MeshSetInfo);

	// Compare to last set
	if(ms_LastMeshSetInfo.size() != ms_MeshSetInfo.size())
	{
		ms_meshSetMismatch = true;
	}

	if( ms_meshSetMismatch == false )
	{
		// Check if the elements have changed
		for(int i=0;i<ms_MeshSetInfo.size();i++)
		{
			ReplayMovieMeshSet &thisSet = ms_MeshSetInfo[i];
			ReplayMovieMeshSet &otherSet = ms_LastMeshSetInfo[i];

			u32	thisHash = atStringHash(thisSet.m_SetName);
			u32	otherHash = atStringHash(otherSet.m_SetName);
			if(thisHash != otherHash)
			{
				ms_meshSetMismatch = true;
			}
		}
	}
}


void ReplayMeshSetManager::RecordData(ReplayController& controller, bool blockChange)
{
	if(ms_meshSetMismatch || blockChange)
	{
		controller.RecordPacket<CPacketMeshSet>(ms_meshSetPacket);
		ms_LastMeshSetInfo = ms_MeshSetInfo;
	}
}


s32 ReplayMeshSetManager::GetMemoryUsageForFrame(bool blockChange)
{
	return ms_meshSetMismatch || blockChange ? sizeof(CPacketMeshSet) : 0;
}


void ReplayMeshSetManager::OnEntry()
{
	// Save Anything that needs to be put back
}

void ReplayMeshSetManager::OnExit()
{
	// Put back anything that was saved
}

void ReplayMeshSetManager::Process()
{
	if(ms_bDirty)
	{
		// Remove any sets that aren't needed now
		RemoveUnusedSets();
		// Add in sets that are
		AddRequiredSets();
		ms_bDirty = false;
	}
}

void ReplayMeshSetManager::SubmitNewMeshSetData(const atFixedArray<ReplayMovieMeshSet, MOVIE_MESH_MAX_SETS> &info)
{
	ms_MeshSetInfo = info;
	ms_bDirty = true;
}


void ReplayMeshSetManager::RemoveUnusedSets()
{
	// Loop through the sets loaded and see if we have them in our array of required sets
	for (int i = 0; i < MOVIE_MESH_MAX_SETS; i++)
	{
		bool bFound = false;
		MovieMeshSet &thisSet = g_movieMeshMgr.GetSet(i);
		if(thisSet.m_handle != INVALID_MOVIE_MESH_SET_HANDLE)
		{
			// See if this is in our list of required sets
			for(int j=0;j<ms_MeshSetInfo.size();j++)
			{
				ReplayMovieMeshSet &thisReplaySet = ms_MeshSetInfo[j];
				MovieMeshSetHandle thisHandle = atStringHash(thisReplaySet.m_SetName);
				if(thisHandle == thisSet.m_handle)
				{
					bFound = true;
					break;
				}
			}

			if( !bFound )
			{
				g_movieMeshMgr.ReleaseSet(thisSet.m_handle);
			}
		}
	}

}

void ReplayMeshSetManager::AddRequiredSets()
{
	// Loop through the sets we want to load, and load them if they're not already loaded.
	for(int i=0;i<ms_MeshSetInfo.size();i++)
	{
		ReplayMovieMeshSet &thisReplaySet = ms_MeshSetInfo[i];
		MovieMeshSetHandle thisHandle = atStringHash(thisReplaySet.m_SetName);
		bool bFound = false;
		for (int j=0; j < MOVIE_MESH_MAX_SETS; j++)
		{
			MovieMeshSet &thisSet = g_movieMeshMgr.GetSet(j);
			if( thisHandle == thisSet.m_handle )
			{
				bFound = true;
				break;
			}
		}

		if(!bFound)
		{
			g_movieMeshMgr.LoadSet(thisReplaySet.m_SetName);
		}
	}
}

#endif	//GTA_REPLAY
