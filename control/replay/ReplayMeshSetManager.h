#ifndef __REPLAY_MESH_SET_MANAGER_H__
#define __REPLAY_MESH_SET_MANAGER_H__

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/Misc/MeshSetPacket.h"
#include "vfx/misc/MovieMeshManager.h"

class ReplayController;

class ReplayMeshSetManager
{
public:
	static void	OnRecordStart();								// Clears our stored MeshSet compare list
	static void	CollectData();									// Called each frame to collect data in a packet
	static void RecordData(ReplayController& controller, bool blockChange);// Called to record the packet in the replay clip
	static s32  GetMemoryUsageForFrame(bool blockChange);

	static void OnEntry();										// Called at start of playback, Saves current state
	static void OnExit();										// Called at end of playback, Re-sets state before replay

	static void	Process();										// Called each frame during playback

	static void SubmitNewMeshSetData(const atFixedArray<ReplayMovieMeshSet, MOVIE_MESH_MAX_SETS> &info);

private:
	static void	RemoveUnusedSets();
	static void	AddRequiredSets();
	static void	GetMeshSet(atFixedArray<ReplayMovieMeshSet, MOVIE_MESH_MAX_SETS> &info);

	static atFixedArray<ReplayMovieMeshSet, MOVIE_MESH_MAX_SETS>	ms_MeshSetInfo;
	static atFixedArray<ReplayMovieMeshSet, MOVIE_MESH_MAX_SETS>	ms_LastMeshSetInfo;

	static bool	ms_bDirty;

	static CPacketMeshSet	ms_meshSetPacket;
	static bool				ms_meshSetMismatch;

};

#endif	//GTA_REPLAY

#endif //__REPLAY_MESH_SET_MANAGER_H__

