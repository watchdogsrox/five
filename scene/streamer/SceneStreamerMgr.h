/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/streamer/SceneStreamerMgr.h
// PURPOSE : Primary interface to scene streaming / loading systems
// AUTHOR :  Ian Kiigan
// CREATED : 06/08/10
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_STREAMER_SCENESTREAMERMGR_H_
#define _SCENE_STREAMER_SCENESTREAMERMGR_H_

#include "atl/inlist.h"
#include "data/base.h"
#include "fwtl/LinkList.h"
#include "scene/debug/SceneStreamerDebug.h"
#include "scene/Entity.h"
#include "scene/FocusEntity.h"
#include "scene/streamer/SceneLoader.h"
#include "scene/streamer/SceneStreamer.h"
#include "scene/streamer/SceneStreamerList.h"
#include "system/task.h"
#include "vector/vector3.h"


namespace rage {
	class bkGroup;
}

class CGtaPvsEntry;
class CInteriorInst;
class CPortalInst;
class CRenderPhase;
class CPostScan;

// currently supported scene streamers
enum
{
	SCENESTREAMERMGR_STREAMER_MAIN = 0,
	SCENESTREAMERMGR_STREAMER_NULL,

	SCENESTREAMERMGR_STREAMER_TOTAL
};

// currently supported scene loaders
enum
{
	SCENESTREAMERMGR_LOADER_BLOCKING = 0,
	SCENESTREAMERMGR_LOADER_NONBLOCKING,

	SCENESTREAMERMGR_LOADER_TOTAL
};



// primary interface for all scene streaming and loadscene
class CSceneStreamerMgr : public datBase
{
public:
	// scene streaming driven by visibility
	void PreScanUpdate();
	void Process();
	CSceneStreamerBase* GetStreamer()								{ return &m_streamer; }
	bool IsRejectedTimedEntity(CEntity* pEntity, const bool visibleInGbuf);
	
	// scene loading driven by world searching etc
	void LoadScene(const Vector3& vPos)								{ ms_sceneLoader.LoadScene(vPos); }
	void StartLoadScene(const Vector3& vPos)						{ ms_sceneLoader.StartLoadScene(vPos); }
	bool UpdateLoadScene()											{ return ms_sceneLoader.UpdateLoadScene(); }
	void StopLoadScene()											{ ms_sceneLoader.StopLoadScene(); }
	bool IsLoading()												{ return ms_sceneLoader.IsLoading(); }
	void LoadInteriorRoom(CInteriorInst* pInterior, u32 nRoomIdx)	{ ms_sceneLoader.LoadInteriorRoom(pInterior, nRoomIdx); }

	Vec3V_Out GetHdStreamSphereCentre()								{ return ms_vSphericalStreamingPos; }
	Vec3V_Out GetLodStreamSphereCentre()							{ return ms_vSphericalStreamingPos; }
	Vec3V_Out GetIntStreamSphereCentre()							{ return ms_vSphericalStreamingPos; }

	static void ForceSceneStreamerToTrackWithCameraPosThisFrame() { ms_bScriptForceSphericalStreamerTrackWithCamera = true; }

	void UpdateSphericalStreamerPosAndRadius();

	float GetHdStreamSphereRadius();
	float GetLodStreamSphereRadius()								{ return ms_fStreamRadius_LOD; }
	float GetIntStreamSphereRadius();
	

	CStreamingLists &GetStreamingLists()							{ return m_StreamingLists; }

	void Init();
	void Shutdown();

	static float GetCachedNeededScore() { return ms_fCachedNeededScore; }

#if __BANK
	void AddWidgets(bkGroup &group);
	bool *GetStreamMapSwapsPtr()									{ return &ms_bStreamMapSwaps; }
	bool *GetStreamByProximityPtr()									{ return &ms_bStreamByProximity_HD; }
#endif // __BANK

#if !__NO_OUTPUT
	void DumpPvsAndBucketStats();
	void DumpStreamingInterests(bool toTTY = true);
	static void StreamerDebugPrintf(bool toTTY, const char *format, ...);
#endif // !__FINAL

private:
	void GenerateLists();
	void IssueRequestsForLists();
	void CheckForMapSwapCompletion();
	bool AllDepsSatisfied(strLocalIndex modelIndex);
	void CacheNeededScore();

#if GTA_REPLAY
	void BlockAndLoad();
#endif

#if STREAMING_VISUALIZE
	void UpdateStreamingVisualizeVisibility();
#endif // STREAMING_VISUALIZE


	static CSceneLoader			ms_sceneLoader;
	static bool					ms_bStreamFromPVS;
	static bool					ms_bStreamByProximity_HD;
	static bool					ms_bStreamMapSwaps;
	static bool					ms_bStreamVolumes;
	static bool					ms_bStreamRequiredInteriors;
	static float				ms_fStreamRadius_HD;
	static float				ms_fStreamShortRadius_HD;
	static float				ms_fStreamRadius_Interior;
	static float				ms_fStreamShortRadius_Interior;
	static float				ms_fStreamRadius_LOD;

	static Vec3V				ms_vSphericalStreamingPos;
	static bool					ms_bScriptForceSphericalStreamerTrackWithCamera;

	static float				ms_fCachedNeededScore;
#if GTA_REPLAY
	static u32					ms_ReplayTimeoutMS;
#endif

	atArray<CSceneStreamerList>	m_lists;
	CSceneStreamer				m_streamer;
	CStreamingLists				m_StreamingLists;

	// Don't request anything until all requests in these buckets have been fulfilled
	StreamBucket				m_BucketCutoff;

#if __BANK
	friend class CSceneStreamerDebug;
#endif //__BANK
};

class CSceneStreamerMgrWrapper
{
public:
	static void PreScanUpdate();
};

extern CSceneStreamerMgr g_SceneStreamerMgr;

#endif //_SCENE_STREAMER_SCENESTREAMERMGR_H_
