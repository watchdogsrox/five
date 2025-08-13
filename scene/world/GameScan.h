#ifndef _INC_GAMESCAN_H_
#define _INC_GAMESCAN_H_

#define FULL_SPHERICAL_SCENE_STREAMING (RSG_PC || RSG_DURANGO || RSG_ORBIS)

// framework headers
#include "fwscene/scan/Scan.h"
#include "fwscene/world/SceneGraph.h"
#include "fwscene/world/InteriorSceneGraphNode.h"

class PvsViewer;
class CRenderPhase;
class CRenderPhaseScanned;

class CGameScan
{
private:
	
	static u32		ms_hdStreamVisibilityBit;
	static u32		ms_lodStreamVisibilityBit;
	static u32		ms_streamVolumeVisibilityBit1;
	static u32		ms_streamVolumeVisibilityBit2;
	static u32		ms_interiorStreamVisibilityBit;
	static u32		ms_streamingVisibilityMask;
	static bool		ms_gbufIsActive;

	static fwRoomSceneGraphNode* FindRoomSceneNode(const fwInteriorLocation interiorLocation);
	static fwSceneGraphNode* FindStartingSceneNode(const CRenderPhaseScanned* renderPhase);
	
	static void SetupScan();

	//////////////////////////////////////////////////////////////////////////
	// SCENE STREAMING THRU VISIBILITY - cullshape setup

#if FULL_SPHERICAL_SCENE_STREAMING
	// NG specific - monolithic spherical streamer
	static void SetupSceneStreamingCullShapes_NextGen(bool bAllowSphericalSceneStreaming);
	static void SetupExteriorStreamCullShape(const CRenderPhaseScanned* gbufPhase, bool bLowDetailOnly);
#else
	// CG specific - multiple cullshapes
	static void SetupSceneStreamingCullShapes_CurrentGen();
	static void SetupHdStreamCullShape(const CRenderPhaseScanned* gbufPhase);
	static void SetupLodStreamCullShape(const CRenderPhaseScanned* gbufPhase);
	static void SetupVelocityVectorStreamer(const CRenderPhaseScanned* gbufPhase, u32 renderPhaseId);
#endif

	// common
	static void SetupStreamingVolumeCullShapes(const CRenderPhaseScanned* gbufPhase, s32 volumeSlot, u32 renderPhaseId);
	static void SetupInteriorStreamCullShape(const CRenderPhaseScanned* gbufPhase);
	//////////////////////////////////////////////////////////////////////////

public:
	static void Init();
	static void Update();
	
	static void KickVisibilityJobs();
	static void PerformScan();

	static u32 GetHdStreamVisibilityBit()		{ return ms_hdStreamVisibilityBit; }
	static u32 GetLodStreamVisibilityBit()		{ return ms_lodStreamVisibilityBit; }
	static u32 GetStreamVolumeVisibilityBit1()	{ return ms_streamVolumeVisibilityBit1; }
	static u32 GetStreamVolumeVisibilityBit2()	{ return ms_streamVolumeVisibilityBit2; }
	static u32 GetInteriorStreamVisibilityBit()	{ return ms_interiorStreamVisibilityBit; }
	static u32 GetStreamingVisibilityMask()		{ return ms_streamingVisibilityMask; }
	static u32 GetStreamVolumeMask()			{ return (1 << ms_streamVolumeVisibilityBit1) | (1 << ms_streamVolumeVisibilityBit2); }
};

#endif // !defined _INC_GAMESCAN_H_
