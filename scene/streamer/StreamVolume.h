/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/streamer/StreamVolume.h
// PURPOSE : interface for pinning and requesting a specific volume of map
// AUTHOR :  Ian Kiigan
// CREATED : 23/08/11
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_STREAMER_STREAMVOLUME_H_
#define _SCENE_STREAMER_STREAMVOLUME_H_

#include "atl/array.h"
#include "atl/string.h"
#include "camera/viewports/ViewportManager.h"
#include "grcore/viewport.h"
#include "renderer/PostScan.h"
#include "scene/RegdRefTypes.h"
#include "scene/streamer/SceneStreamerList.h"
#include "script/thread.h"
#include "spatialdata/aabb.h"
#include "spatialdata/sphere.h"
#include "spatialdata/transposedplaneset.h"
#include "streaming/streamingdefs.h"
#include "fwscene/stores/boxstreamerassets.h"
#include "fwscene/stores/boxstreamersearch.h"
#include "vectormath/vec3v.h"
#include "fwmaths/vectorutil.h"

namespace rage
{
	class bkBank;
}
class CEntity;

// used for finding intersecting entities for player switch pan
class CStreamVolumeLineSegHelper
{
public:
	void Set(Mat34V_In rageCamMatrix, Vec3V_In offset, const float fFarCip)
	{
		grcViewport viewport = gVpMan.GetCurrentViewport()->GetGrcViewport();
		viewport.SetWorldIdentity();
		viewport.SetCameraMtx(rageCamMatrix);
		viewport.SetFarClip(fFarCip);

		m_planeSet = GenerateSweptBox2D(viewport, offset);
	}
	inline bool IntersectsAABB(const spdAABB& aabb) { return m_planeSet.IntersectsAABB(aabb); }

private:
	spdTransposedPlaneSet4 m_planeSet;

};

// represents a volume with a number of asset store and entity dependencies
// TODO split this up into a base class plus several derived classes (e.g. sphere, aabb, frustum, line)
struct CStreamVolume
{
public:
	enum eVolType
	{
		TYPE_SPHERE,
		TYPE_FRUSTUM,
		TYPE_LINE
	};

	void InitAsSphere(const spdSphere& sceneVol, fwBoxStreamerAssetFlags assetFlags, bool bSceneStreamingEnabled, bool bMinimal, bool bMinimalMoverCollision);
	void InitAsFrustum(Vec3V_In vPos, Vec3V_In vDir, float fFarClip, fwBoxStreamerAssetFlags assetFlags, bool bSceneStreamingEnabled, bool bMinimal);
	void InitAsLine(Vec3V_In vPos1, Vec3V_In vPos2, fwBoxStreamerAssetFlags assetFlags, bool bSceneStreamingEnabled, float fMaxBoxStreamerZ);

	bool IsTypeSphere() const							{ return m_volumeType==TYPE_SPHERE; }
	bool IsTypeFrustum() const							{ return m_volumeType==TYPE_FRUSTUM; }
	bool IsTypeLine() const								{ return m_volumeType==TYPE_LINE; }

	void Shutdown();
	bool IsActive() const								{ return m_bActive; }
	
	Vec3V_Out GetPos() const							{ return m_sphere.GetCenter(); }
	spdSphere& GetSphere()								{ return m_sphere; }
	spdSphere& GetMapDataSphere()						{ return m_mapDataSphere; }
	spdSphere& GetStaticBoundsSphere()					{ return m_staticBoundsSphere; }
	Vec3V_Out GetDir() const							{ return m_vFrustumDir; }
	float GetFarClip() const							{ return m_fFarClip; }

	fwBoxStreamerAssetFlags GetAssetFlags() const		{ return m_assetFlags; }
	const atArray<u32>& GetRequiredStaticBounds() const	{ return m_reqdStaticBounds; }
	const atArray<u32>& GetRequiredImaps() const		{ return m_reqdImaps; }
	bool UsesSceneStreaming() const						{ return m_bSceneStreamingEnabled; }
	bool HasBeenProcessedByVisibility() const			{ return m_bProcessedByVisibility; }
	bool HasLoaded() const;
	void SetEntitiesLoaded(bool bLoaded)				{ m_bAllEntitiesLoaded = bLoaded; }
	Vec3V_Out GetPos1()	const							{ return m_vLinePos1; }
	Vec3V_Out GetPos2() const							{ return m_vLinePos2; }
	void SetLodMask(u32 lodMask)						{ m_lodMask = lodMask; }
	u32 GetLodMask() const								{ return m_lodMask; }
	bool RequiresLodLevel(u32 lodLevel)					{ return ((m_lodMask & (1 << lodLevel)) != 0); }

	void SetScriptInfo(scrThreadId scriptThreadId, const char* pszName);
	bool IsOwnedByScript(scrThreadId scriptThreadId) const { return m_bCreatedByScript && scriptThreadId==m_scriptThreadId; }
	const char* GetOwningScriptName() const { return m_scriptName.c_str(); }
	bool WasCreatedByScript() const { return m_bCreatedByScript; }
	void CheckForValidResults();

	void ExtractEntitiesFromMapData(atArray<CEntity*>& slodList);
	void UpdateLoadedState(s32 volumeSlot);

	bool UsesCameraMtx() const { return m_bUsesCameraMatrix; }
	Mat34V_Out GetCameraMtx() const { return m_cameraMatrix; }
	void SetCameraMtx(Mat34V_In rageCamMatrix) { m_bUsesCameraMatrix=true; m_cameraMatrix=rageCamMatrix; }
	void SetLineSeg(Mat34V_In rageCamMatrix, Vec3V_In offset, const float fFarCip) { m_lineSegHelper.Set(rageCamMatrix, offset, fFarCip); }
	
	static void InitTunables();
private:
	
	void InitCommon(fwBoxStreamerAssetFlags assetFlags, bool bSceneStreamingEnabled, bool bMinimal);
	void SetActive(bool bActive)						{ m_bActive = bActive; }
	void SetProcessedByVisibility(bool bProcessed)		{ m_bProcessedByVisibility = bProcessed; }

	bool m_bActive;
	bool m_bUsesCameraMatrix;
	fwBoxStreamerAssetFlags m_assetFlags;		// what asset stores does the volume require?
	spdSphere m_sphere;
	spdSphere m_mapDataSphere;
	spdSphere m_staticBoundsSphere;
	atArray<u32> m_reqdStaticBounds;
	atArray<u32> m_reqdImaps;
	bool m_bSceneStreamingEnabled;
	eVolType m_volumeType;
	bool m_bCreatedByScript;
	scrThreadId m_scriptThreadId;
	atString m_scriptName;
	Vec3V m_vFrustumDir;
	float m_fFarClip;
	bool m_bAllEntitiesLoaded;
	bool m_bProcessedByVisibility;
	bool m_bMinimalMapStreaming;
	Vec3V m_vLinePos1;
	Vec3V m_vLinePos2;
	Vec3V m_vBoxStreamerLinePos1;
	Vec3V m_vBoxStreamerLinePos2;
	u32 m_lodMask;
	Mat34V m_cameraMatrix;

	CStreamVolumeLineSegHelper m_lineSegHelper;

	static bool ms_disableHeist4Hack;

	//////////////////////////////////////////////////////////////////////////
#if __BANK
public:
	void DebugDraw();
#endif	//__BANK
#if !__NO_OUTPUT
public:
	const char* GetDebugOwner();
#endif	//!__FINAL
	//////////////////////////////////////////////////////////////////////////

	friend class CStreamVolumeMgr;
};

// for registering and de-registering streaming volumes
class CStreamVolumeMgr
{
public:

	enum
	{
		VOLUME_SLOT_PRIMARY = 0,		// used by script, load scene
		VOLUME_SLOT_SECONDARY,			// used only for player switching

		VOLUME_SLOT_TOTAL
	};

	static void RegisterSphere(const spdSphere& sceneVol, fwBoxStreamerAssetFlags assetFlags, bool bSceneStreamingEnabled, s32 volumeSlot, bool bMinimal=false, bool bMinimalMoverCollision=false);
	static void RegisterFrustum(Vec3V_In vPos, Vec3V_In vDir, float fFarClip, fwBoxStreamerAssetFlags assetFlags, bool bSceneStreamingEnabled, s32 volumeSlot, bool bMinimal=false);
	static void RegisterLine(Vec3V_In vPos1, Vec3V_In vPos2, fwBoxStreamerAssetFlags assetFlags, bool bSceneStreamingEnabled, s32 volumeSlot, float fMaxBoxStreamerZ=FLT_MAX);
	static void	Deregister(s32 volumeSlot);
	static void DeregisterForScript(scrThreadId scriptThreadId);

	static void AddSearches(atArray<fwBoxStreamerSearch>& searchList, fwBoxStreamerAssetFlags supportedAssetFlags);

	static bool IsVolumeActive(s32 volumeSlot)											{ return m_volumes[volumeSlot].IsActive(); }
	static bool HasLoaded(s32 volumeSlot)												{ return m_volumes[volumeSlot].HasLoaded(); }
	static void SetVolumeScriptInfo(scrThreadId scriptThreadId, const char* pszName)	{ m_volumes[VOLUME_SLOT_PRIMARY].SetScriptInfo(scriptThreadId, pszName); }
	static CStreamVolume& GetVolume(s32 volumeSlot)										{ return m_volumes[volumeSlot]; }

	static bool IsAnyVolumeActive() { return IsVolumeActive(VOLUME_SLOT_PRIMARY) || IsVolumeActive(VOLUME_SLOT_SECONDARY); }

	static void UpdateVolumesLoadedState();

private:
	static CStreamVolume m_volumes[VOLUME_SLOT_TOTAL];

	//////////////////////////////////////////////////////////////////////////

#if !__FINAL
public:
	static int GetStreamingIndicesCount();
	static void GetStreamingIndices(atArray<strIndex>& streamingIndices);
	static bool IsThereAVolumeOwnedBy(scrThreadId scriptThreadId);
private:
	static void UpdateScriptMemUse();
	static atArray<strIndex> ms_streamingIndices;
#endif	//!__FINAL

#if __BANK
public:
	static void	AddWidgets(bkBank* pBank);
	static void	DebugDraw();

private:
	static void DebugSetPos1OfLine();
	static void DebugRegisterSphereVolume();
	static void DebugRegisterFrustumVolume();
	static void DebugRegisterLineVolume();
	static void DebugDeregisterVolume();
	static u32	DebugGetLodMask();
	static bool ms_bShowActiveVolumes;
	static bool ms_bShowUnloadedEntities;
	static s32	ms_radius;
	static bool	ms_bStaticBoundsMover;
	static bool ms_bMinimalVolume;
	static bool ms_bStaticBoundsWeapon;
	static bool ms_bMapDataHigh;
	static bool ms_bMapDataHighCritical;
	static bool ms_bMapDataMedium;
	static bool ms_bMapDataLow;
	static bool ms_bMapDataAll;
	static bool ms_bUseSceneStreaming;
	static bool ms_bPrintEntities;
	static Vec3V ms_vDebugLinePos1;
	static Vec3V ms_vDebugLinePos2;
	static s32 ms_nDbgSlot;
	static bool ms_abLodLevels[LODTYPES_DEPTH_TOTAL];
#endif	//__BANK
	//////////////////////////////////////////////////////////////////////////
};

#endif	//_SCENE_STREAMER_STREAMVOLUME_H_
