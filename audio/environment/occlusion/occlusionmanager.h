//
// audio/environment/occlusion/occlusionmanager.h
//
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
//

#ifndef AUD_NAOCCLUSIONMANAGER_H
#define AUD_NAOCCLUSIONMANAGER_H

// Game
#include "audio/audiodefines.h"
#include "audio/asyncprobes.h"
#include "audio/audio_channel.h"
#include "audio/environment/occlusion/occlusioninteriorinfo.h"
#include "audio/environment/occlusion/occlusionpathnode.h"
#include "audio/environment/environment.h"
#include "Task/System/TaskHelpers.h"

// Rage
#include "audioengine/widgets.h"
#include "bank/bank.h"
#include "system/FileMgr.h"

const s32 g_audNumLevelProbeRadials		= 24;
const s32 g_audNumHeightProbeRadials	= 8;
const s32 g_audNumMidProbeRadials		= 8;
const s32 g_audNumMidProbeElevations	= 4;
const s32 g_audNumVertProbeRadials		= 4;
const s32 g_audNumVertProbeElevations	= 4;

const s32 g_audNumOccZoneRadials		= 8;	// 0 = N, 1 = NE, 2 = E, 3 = SE, 4 = S, 5 = SW, 6 = W, 7 = NW
const s32 g_audNumOccZoneElevations		= 4;	// 0 = UP, 1 = SLIGHTLY UP, 2 = SLIGHLY DOWN, 3 = DOWN

// A vertical compass that defines the different elevations at which we send the probes.  See g_OcclusionScanPositionZValues.
// We send out different amounts of probes for different elevations so we have no enum for the probe radials.
enum naOcclusionProbeElevation	
{
	//							Degrees	from x-y plane	Per Elevation Array Indices
	AUD_OCC_PROBE_ELEV_N = 0,	// 85					Vertical[0]
	AUD_OCC_PROBE_ELEV_NNW,		// 67.5					Vertical[1]
	AUD_OCC_PROBE_ELEV_NW,		// 45					Mid[0]					
	AUD_OCC_PROBE_ELEV_WWN,		// 22.5					Mid[1]
	AUD_OCC_PROBE_ELEV_W,		// 0					Level arrays are 1-dimensional    
	AUD_OCC_PROBE_ELEV_WWS,		// -22.5				Mid[2]
	AUD_OCC_PROBE_ELEV_SW,		// -45					Mid[3]
	AUD_OCC_PROBE_ELEV_SSW,		// -67.5				Vertical[2]
	AUD_OCC_PROBE_ELEV_S,		// -85					Vertical[3]
	AUD_NUM_OCC_PROBE_ELEV_INDICES,
};

#if __BANK
XPARAM(audOcclusionBuildEnabled);

enum naOcclusionBuildStep
{
	AUD_OCC_BUILD_STEP_START = 0,
	AUD_OCC_BUILD_STEP_INTRA_LOAD,
	AUD_OCC_BUILD_STEP_INTRA_PORTALS,
	AUD_OCC_BUILD_STEP_INTRA_PATHS,
	AUD_OCC_BUILD_STEP_INTRA_UNLOAD,
	AUD_OCC_BUILD_STEP_INTER_PATHS,
	AUD_OCC_BUILD_STEP_INTER_UNLOAD,
	AUD_OCC_BUILD_STEP_TUNNEL_PATHS,
	AUD_OCC_BUILD_STEP_TUNNEL_UNLOAD,
	AUD_OCC_BUILD_STEP_FINISH,
	NUM_AUD_OCC_BUILD_STEPS
};

struct naOcclusionFailure
{
	u32 uniqueInteriorHash;
	naOcclusionBuildStep buildStep;
};
#endif

class naEnvironmentGroup;
class naOcclusionPortalInfo;

namespace rage {
	class fwPortalCorners;
}

// We're using the portal based occlusion when in different rooms, and the bounding box system when listener and source are both outside.
class naOcclusionManager
{

// Start Game Thread ***************************************************************************************************************************************
public:

	naOcclusionManager(){};
	~naOcclusionManager(){};

	void Init();
	void Shutdown();

	// Update is done on the game thread from audNorthAudioEngine::StartUpdate() because it's accessing regdent's and using probes to find doors in exterior scenes
	// We're also accessing the glass frag data which may be modified during the audio thread, so we need to access that on the game thread as well.
	void Update();

	// This will set up the interior information for newly streamed in occlusion metadata
	void UpdateInteriorMetadata();

// We issue and cache the LOS probes on the game thread, storing the distances and materials.  Then on the audio thread we process that data.
// We don't need to worry about thread issues when using async method because the audCachedLos class stores the probe data until the game thread where we copy.
private:

	// Update all the window and door data we need to determine how much blockage there is based on the attached entities
	void UpdatePortalInfoList();

	// Send out probes at different radials and elevations and record the position of the closest occlusive intersection (based on material).  
	// We store the Level, Mid, and Vertical probe results so we can look at multiple intersections later to determine what types of materials are in our way.
	// Vertical -	2 probes/frame = shared across the 4 vertical elevations.		4 radials at each elevation AUD_ELEVATION_N, _NNW, _SSW, _S
	// Mid -		4 probes/frame = 1 for each of the 4 mid elevations.			8 radials at each elevation AUD_ELEVATION_NW, _WWN, _WWS, _SW
	// Level -		3 probes/frame so we cover level every 8 frames					24 radials at elevation AUD_ELEVATION_W
	// Heights -	1 probe/frame that goes straight up (90 degrees) 20m, starting g_HeightScanDistance from the listener position along 8 radials.  not used.
	void UpdateOcclusionProbes();
	void UpdateOcclusionProbesAsync();	// Only uses 1 intersection due to CWorldProbeAsync limitations
	void ProcessAsyncOcclusionProbes();
	void IssueAsyncOcclusionProbes();

#if __BANK
	void DebugAudioImpact(const Vector3& pos, const Vector3& start_pos, const s32 radialIndex, const naOcclusionProbeElevation elevationIndex);
#endif

public:

private:

	Vector3 m_LocalEnvironmentScanPositionLastFrame;

	// Probes.  See audOcclusionElevationIndex for how the different elevation arrays are split up
	atRangeArray<Vector3, g_audNumLevelProbeRadials> m_RelativeScanPositions;
	atMultiRangeArray<Vector3, g_audNumMidProbeElevations, g_audNumMidProbeRadials> m_RelativeMidScanPositions;
	atMultiRangeArray<Vector3, g_audNumVertProbeElevations, g_audNumVertProbeRadials> m_RelativeVerticalScanPositions;
	atRangeArray<audOcclusionIntersections, g_audNumLevelProbeRadials> m_OcclusionIntersectionLevel;
	atMultiRangeArray<audOcclusionIntersections, g_audNumMidProbeElevations, g_audNumMidProbeRadials> m_OcclusionIntersectionMid;
	atMultiRangeArray<audOcclusionIntersections, g_audNumVertProbeElevations, g_audNumVertProbeRadials> m_OcclusionIntersectionVertical;

	// cached los results, processed via the WorldProbe class.
	audCachedLosMultipleResults m_CachedLos;

	u8 m_CurrentLocalEnvironmentUpdateIndex;

	atRangeArray<f32, g_audNumLevelProbeRadials> m_LevelWallDistances;
	atRangeArray<f32, g_audNumLevelProbeRadials> m_LevelYLimits;
	atRangeArray<f32, g_audNumLevelProbeRadials> m_LevelXLimits;
	atRangeArray<f32, g_audNumLevelProbeRadials> m_LevelZLimits;

	atMultiRangeArray<f32, g_audNumMidProbeElevations, g_audNumMidProbeRadials> m_MidWallDistances;
	atMultiRangeArray<f32, g_audNumMidProbeElevations, g_audNumMidProbeRadials> m_MidYLimits;
	atMultiRangeArray<f32, g_audNumMidProbeElevations, g_audNumMidProbeRadials> m_MidXLimits;
	atMultiRangeArray<f32, g_audNumMidProbeElevations, g_audNumMidProbeRadials> m_MidZLimits;

	atMultiRangeArray<f32 ,g_audNumVertProbeElevations, g_audNumVertProbeRadials> m_VerticalWallDistances;
	atMultiRangeArray<f32, g_audNumVertProbeElevations, g_audNumVertProbeRadials> m_VerticalYLimits;
	atMultiRangeArray<f32, g_audNumVertProbeElevations, g_audNumVertProbeRadials> m_VerticalXLimits;
	atMultiRangeArray<f32, g_audNumVertProbeElevations, g_audNumVertProbeRadials> m_VerticalZLimits;

#if __BANK
	// Used to lock when writing debug probe data
	sysCriticalSectionToken m_Lock;
#endif

// End Game Thread *****************************************************************************************************************************************

// Start Audio Game Thread *********************************************************************************************************************************
// Process the data we got from the probes
// Update all the occlusionGroups, which are what's linked to our sounds
// Provide all the helper functions to get the occlusionFactor for the different occlusion methods, bounding-box, material, and portal
public:

	// Called before the audio controller update.  Sorts out all the data we got from the probes on the game thread
	void UpdateOcclusionMetrics();

	// Called from the audio controller, processes each occlusion group and keeps data for PathServer requests
	void UpdateEnvironmentGroups(audEnvironmentGroupInterface** environmentGroupList, u32 maxGroups, u32 timeInMs);

	// The globalOcclusionDampingFactor is set on the BASE category in audNorthAudioEngine::UpdateCategory()
	// When setting, use the minimum dampingFactor value so if multiple places are changing this variable we'll use the one that wants the least amount of occlusion.
	f32 GetGlobalOcclusionDampingFactor() { return m_GlobalOcclusionDampingFactor; }
	void SetGlobalOcclusionDampingFactor(const f32 dampingFactor);

	// Bounding box occlusion is used when both the listener and sound source are outside.
	//		1.	In UpdateOcclusionProbes() we send out probes and store the closest intersections with a material we consider to be occlusive (i.e. ignore chainlink fences).
	//		2.	In UpdateOcclusionMetrics() we calculate the m_OcclusionBoundingBoxes, see that fn for what's being calculated.
	//		3.	The occlusionGroups then get an average of the zones occlusion data based on the sources angle from the listener position.
	f32 GetBoundingBoxOcclusionFactorForPosition(const Vector3& pos);

	// Returns an index into the global portal information array based on the portal parameters, and creates one if it doesn't exist.
	naOcclusionPortalInfo* GetPortalInfoFromParameters(const u32 intProxyHashkey, const s32 roomIdx, const u32 portalIdx);
	void RemovePortalInfoEntry(const naOcclusionPortalInfo* portalInfoToRemove);

	// Uses the position of the intProxy to modify the namehash, so we can differentiate between different proxies with the same name
	u32 GetUniqueProxyHashkey(const CInteriorProxy* intProxy) const ;
	CInteriorProxy* GetInteriorProxyFromUniqueHashkey(const u32 hashkey);

	// Gets the interpolated distance, final panning play position, and occlusion values based on the portal blockage and angles formed by the different paths
	// Returns true if there were paths built for the interior combination and we were able to get good data
	bool ComputeOcclusionPathMetrics(naEnvironmentGroup* environmentGroup, Vector3& playPos, f32& pathDistance, f32& occlusionFactor);

	// Looks through the 'current' portal and sets the destination values taking into account link and non-link portals.  Ignores mirror portals.
	// For linked interiors, the linkedIntRoomLimboPortalIdxPtr is the portal index of room 0 that matches the portal in the pCurrentIntInst that we're coming from.
	void GetPortalDestinationInfo(const CInteriorInst* currentIntInst, const s32 currentRoomIdx, const s32 startPortalIdx, 
								CInteriorInst** destIntInstPtr, s32* destRoomIdxPtr, s32* linkedIntRoomLimboPortalIdxPtr = NULL);
	
	// Returns how occluded a portal is that's leading outside, based on the InteriorSettings, distance, etc.
	f32 CalculateExteriorBlockageForPortal(const Vector3& listenerPosition, const InteriorSettings* interiorSettings, const InteriorRoom* intRoom, 
								const fwPortalCorners* portalCorners, f32 currentPathDistance = 0.0f);

	// We also have exterior occlusion metrics for less specific positions where we just want to think about the general direction
	f32 GetExteriorOcclusionForDirection(audAmbienceDirection direction);
	f32 GetExteriorOcclusionFor3DPosition(const Vector3& position);
	f32 GetExteriorOcclusionFor2DPosition(const Vector2& position);

	// Based on how far away walls are from you, See UpdateBlockedFactors() and UpdateExteriorOcclusion() for details
	f32 GetBlockedFactorForDirection(audAmbienceDirection sector) const;
	f32 GetBlockedFactorFor3DPosition(const Vector3& position);
	f32 GetBlockedFactorFor2DPosition(const Vector2& position);

	// BlockedLinearVolume = 1.0f - (BlockedInDirection * ExteriorOccludedInDirection)
	// I'm not sure why you'd ever use this, you'd be better off either using the portals or the probes
	f32 GetBlockedLinearVolumeForDirection(audAmbienceDirection sector) const;

	//	Returns [0,1] how occluded the 'outside' world is; if the player is in the car then this tells you how open the car is,
	//	if the player is in an interior then this gives you a value based on the m_ExteriorOcclusionFactors
	f32 GetOutsideWorldOcclusion() { return m_OutsideWorldOcclusion; }
	f32 GetOutsideWorldOcclusionIgnoringInteriors() { return m_OutsideWorldOcclusionIgnoringInteriors; }

	// The audAmbienceDirection with the least occlusion based on the portals, used for a very general idea of how much exterior occlusion to use when inside
	f32 GetOutsideWorldOcclusionAfterPortals() { return m_OutsideWorldOcclusionAfterPortals; }

	// Because of vehicles' speed, we do custom metrics so they don't pop in and out.
	f32 ComputeVehicleOcclusion(const CVehicle* veh, const bool isInAir);

	// We store the distances to the closest occlusive intersection for each probe, and this will return that distance.
	f32 GetWallDistance(naOcclusionProbeElevation elevationIndex, s32 radialIndex);

	const Vector3& GetRelativeScanPosition(s32 radialIndex);	// position for radialIndex of 24 level radials

	bool AreOcclusionPathsLoadedForInterior(CInteriorProxy* intProxy);

	// The key is generated from the interior and room hashes.  We build it in such a way that Int1->Int2 will be different than Int2->Int1.
	// We also ensure that when outside (NULL CInteriorInst*'s and interiors with a roomIdx of 0) generate the same key
	u32 GenerateOcclusionPathKey(const CInteriorInst* listenerIntInst, s32 listenerRoomIdx, const CInteriorInst* envGroupIntInst, s32 envGroupRoomIdx, const u32 pathDepth);

	bool ComputeIsEntityADoor(const CEntity* entity);
	bool ComputeIsEntityGlass(const CEntity* entity);

	// Called at the end of the environmentgroupmanager so we can clean up anything we've been using to update the paths
	void FinishEnvironmentGroupListUpdate();

	// Called from audNorthAudioEngine::FinishUpdate() after the audio thread has completed.  That way the scene graph isn't locked.
	void FinishUpdate();
	
	void AddInteriorToMetadataLoadList(const CInteriorProxy* intProxy);

	static u8 GetMaxOcclusionFullPathDepth() { return sm_MaxOcclusionFullPathDepth; }
	static u8 GetMaxOcclusionPathDepth() { return sm_MaxOcclusionPathDepth; }

	static bool GetIsPortalOcclusionEnabled() { return sm_IsPortalOcclusionEnabled; }
	static void SetIsPortalOcclusionEnabled(const bool isEnabled) { sm_IsPortalOcclusionEnabled = isEnabled; }

#if __BANK
	f32 ComputeMaxDoorOcclusion(const CEntity* entity);

	static void AddWidgets(bkBank& bank);

	// Occlusion Builder functions
	// We compute the paths over several frames since we're waiting for doors to load in.  We also check against this to make sure we're only loading doors in FileLoader.cpp
	void SetIsBuildingOcclusionPaths(const bool isBuilding) { m_IsBuildingOcclusionPaths = isBuilding; }
	bool GetIsBuildingOcclusionPaths() { return m_IsBuildingOcclusionPaths; }

	bool GetIsOcclusionBuildEnabled() { return m_AudioOcclusionBuildToolEnabled; }

	// Resets all the pools to larger sizes, cleans out all the pools and maps, and if it's a new build resets all the progress.
	void SetToBuildOcclusionPaths(const bool isNewBuild);

	// Accessed via the 'BuildPortalPaths' RAG button under Audio->Occlusion
	// Will write all possible portal paths determined by the portal depth and distance constraints to audOcclusionPaths.dat
	bool BuildOcclusionPaths();

	bool IsInteriorInDoNotUseList(const u32 intProxyHashkey) const;

	naOcclusionPortalInfo* CreatePortalInfoEntry(const CInteriorProxy* intProxy, const s32 roomIdx, const u32 portalIdx);

	u32 GetKeyForPathNode(naOcclusionPathNode* pathNode);
	naOcclusionPathNode* GetPathNodeFromKey(const u32 key);

	bool ArePortalEntitiesLoaded(const CInteriorProxy* intProxy);

	bool ShouldAllowInteriorToLoad(const CInteriorProxy* intProxy) const;
	u32 GetSwappedHashForInterior(u32 destInteriorHash) const;
#endif
 
private:

	// Work out how much we've moved since last frame, as we'll be offsetting the ones we've not updated by that, as a cheapo best guess.
	// Only ever push things out, don't pull them with us - so if we've got increasing x, push x-positive ones out, but don't pull x-negatives in.
	void UpdateOcclusionMetricsForNewCameraPosition();

	// The bounding box positions are calculated by finding the greatest X, Y, and Z coordinates separately (first X, then Y, then Z) among the different probes.
	// The box concept is if you connect all the positions, anything inside will not be occluded, and everything outside will be occluded.
	// The bounding boxes could also be considered zones, since they are a computation of probes between the current radial and the next radial as well as multiple elevations.
	// example:  m_OcclusionBoundingBoxes[1][1] is the value computed from the probes spanning 45-90 degrees at each of the 
	// Mid[0], Mid[1], and Vertical[1] probes for Z, and Mid[0], Mid[1], and Level probes for X and Y.  Also it would use Level probes on either side of 2 for the extended bounding boxes
	void UpdateOcclusionBoundingBoxes();

	// BlockedFactor is a metric based on distance from geometry determined by probes.
	// exteriorOcclusionFactor is a metric based on how many portals are in a certain direction when in an interior, 
	// it also factors their size and openness into that metric, as well as the exteriorAudibility of the interior.
	// You can have a blocked factor of 0.0 with a exteriorOcclusionFactor of 1.0 if you're looking at a distant wall with no windows or doors
	// You can have an exteriorOcclusionFactor of 0.0 with a blocked factor of 1.0 if you're right up against a wall with lots of windows around you.
	void UpdateExteriorOcclusion();
	void UpdateExteriorOcclusionWithInterior(CInteriorInst* intInst, const Vector3& cameraPos, f32 exteriorOcclusionFactor[][g_audNumOccZoneRadials]);
	void UpdateBlockedFactors();

	// Based on how far in a vehicle you are or on the ExteriorOcclusion metrics
	void UpdateOutsideWorldOcclusion();

	//
	// Bounding Box and exterior occlusion helper functions
	//

	void GetOcclusionZoneIndices(const Vector3& cameraPosition, const Vector3& position, f32& radialRatio, 
		s32& occlusionZoneRadialIndex, f32& elevationRatio, s32& occlusionZoneElevationIndex);
	s32 GetSecondaryRadialOcclusionZoneIndex(s32 primaryRadialOcclusionZoneIndex, f32 radialRatio);
	s32 GetSecondaryElevationOcclusionZoneIndex(s32 primaryElevationOcclusionZoneIndex, f32 elevationRatio);
	f32 GetAmountToUseSecondaryRadialOcclusionZone(f32 ratio);
	f32 GetAmountToUseSecondaryElevationOcclusionZone(s32 primaryElevationOcclusionZoneIndex, f32 ratio);
	f32 GetOcclusionFactorForRadialAndElevation(s32 occlusionZoneRadialIndex, s32 occlusionZoneElevationIndex, const Vector3& position);
	f32	GetExteriorOcclusionFactorForZone(u32 elevationIndex, u32 radialIndex) { return m_ExteriorOcclusionFactor[elevationIndex][radialIndex]; }

	// Because the probes have different amounts of radials for different elevations, we store the various data in separate arrays per elevation
	// This will get turn the global audOcclusionElevationIndex into whatever that corresponds to in the per elevation arrays
	s32 GetArrayElevationIndexFromOcclusionElevationIndex(naOcclusionProbeElevation elevationIndex);

	f32 GetLevelWallDistance(s32 radialIndex);
	f32 GetMidWallDistance(naOcclusionProbeElevation elevationIndex, s32 radialIndex);
	f32 GetVerticalWallDistance(naOcclusionProbeElevation elevationIndex, s32 radialIndex);

	//
	// Portal based occlusion
	//

	void UpdateLoadedInteriorList();

	bool LoadOcclusionDataForInterior(const CInteriorProxy* intProxy);
	void UnloadOcclusionDataForInterior(const CInteriorProxy* intProxy);

	// Functions used to build the portal path trees from RAG
#if __BANK

	// Occlusion path build process:
	//	1.	Teleport the player to the middle of the interior to build and load all interiors that can be reached by traveling through all combinations of portals up to the
	//		sm_MaxOcclusionPathDepth and sm_MaxOcclusionPathDistance.  Keep force-loading all the CInteriorInst's until we reach a state where all the portal and portal entity information is loaded.
	//		Then wait a 100 more frames for all the entities (windows/doors) in the exterior scene to finish loading in.
	//	2.	Build all the paths within each interior (intra-interior) starting at a path depth of 1 up to sm_MaxOcclusionPathDepth,
	//		this includes all outside->inside paths and inside->outside paths.  Then teleport to the next interior until all intra-interior paths are built.
	//		EXAMPLE:
	//		2a.	We build paths at a depth of 1 so we have paths for Room A to Room B.
	//		2b.	Then when we build paths at a depth of 2 Room C to Room B, after traversing the 1st portal from Room C to Room A, we then just use the
	//			paths we've already built from Room A to Room B (store a naOcclusionPathNode* to the A->B path).
	//	3.  Once we have all the intra paths built, we can build paths that go between interiors (inter-interior) starting with a path depth of 1 up to sm_MaxOcclusionPathDepth.
	//		EXAMPLE:
	//		3a.	We traverse the 1st portal from Int A Room A to Int A Room 0 (outside), and then we use the paths we've already built from Int B Room 0 (outside) to Int B Room B.
	//		3b.	Then as we get to the max path depth, we can go from Int A Room B to Int A Room A, and then use the paths we've already built from Int A Room A to Int B Room B.
	//
	//	We write out all the information between each frame so in the event of a crash, we can pick up from where we left off.


	naOcclusionBuildStep UpdateOcclusionBuildProcess();

	bool IsInteriorInBuildList(const u32 intProxyHashkey) const;
	bool IsBuildStepEnabledForInterior(const CInteriorProxy* intProxy, const naOcclusionBuildStep buildStep) const;
	bool IsInteriorInIPLSwapList(const CInteriorProxy* intProxy) const;

	u32 GetNumFailedAttemptsAtBuildStep(naOcclusionBuildStep buildStep, u32 intProxyHash);

	void GoToNextInteriorProxy();

	void TeleportPedToCurrentInterior(const CInteriorProxy* intProxy);
	void TeleportPedToCurrentPortal(const CInteriorProxy* intProxy);

	bool LoadInteriorsForPathBuild(const naOcclusionBuildStep buildStep, CInteriorProxy* intProxy, CInteriorProxy::eProxy_State requestedState, u8 maxDepth);

	// Recursively goes through all the linked interiors until all interiors within the sm_MaxOcclusionPathDepth are loaded.
	// The reason for doing this is when we build paths, we need all doors/windows to be loaded, as well as the interiors in order
	// to determine how many rooms there are, where they go, etc.
	bool LoadInteriorAndLinkedInteriors(const naOcclusionBuildStep buildStep, CInteriorProxy* intProxy, CInteriorProxy::eProxy_State requestedState, 
										u8 currentDepth, const u8 maxDepth, const Vec3V& currentPos, f32 currentDistance = 0.0f);
	void UnloadInteriorsForPathBuild();

	// We need all portals to be instanced in order to get matching 'linked' MLO's, which we use to determine paths and doors.
	bool LoadInteriorPortalsAndPortalObjects(CInteriorProxy* intProxy, CInteriorProxy::eProxy_State requestedState);

	bool VerifyShouldBuildPathsForInterior(const CInteriorProxy* intProxy, const naOcclusionBuildStep buildStep);
	bool IsPathDataBuiltForInterior(const CInteriorProxy* intProxy, const naOcclusionBuildStep buildStep);

	// Goes through every combination of interiors and rooms and builds path data for each combination
	void BuildIntraInteriorPaths(const CInteriorProxy* intProxy);
	void BuildInterInteriorPaths(const CInteriorProxy* intProxy);

	void CreateInteriorInfoEntries();
	void RecomputeMetadataMap();

	void BuildOcclusionPathTreeForCombination(const CInteriorInst* startIntInst, const s32 startRoomIdx, 
												const CInteriorInst* finishIntInst, const s32 finishRoomIdx, 
												const u32 pathDepth, const bool buildInterInteriorPaths);

	void CreateMetadataContainers();
	void LoadMetadataContainersFromFile();

	// Get a list of which interiors we've failed at previously from file
	void CreateFailureList();

	// Determine which interiors we shouldn't build due to previous failures, and write those out to file for the audOcclusionBuilder tool to read
	void CreateDoNotBuildList();

	// Create a list of all the interiors the tool has specified we should build
	void CreateInteriorBuildList();

	// List of all interior names, hashkeys, mapslotindices, and position for use with the tool.
	void CreateInteriorList();

	// Writes which build step and interior we're at in the build process, so if we crash, on the next launch of the game we know where we crashed.
	void WriteOcclusionBuildProgress();

	// Keeps track of which interiors actually finished building their respective path trees and portal infos
	void WriteInteriorToBuiltPathsFile(const CInteriorProxy* intProxy, const naOcclusionBuildStep buildStep);

	// Writes the path settings, portalinfo array, and path tree to the .pso.meta assets
	void WriteOcclusionDataForInteriorToFile(const CInteriorProxy* intProxy, const bool isBuildingIntraInteriorPaths);

	// Goes through all possibilities of paths needed and writes the ones that exist to a file
	void WritePathTreesForInteriorToFile(const CInteriorProxy* intProxy, parTreeNode* rootNode);

	char* LoadNextOcclusionPathFileLine(FileHandle& fileHandle);

	// Displays how much memory we're using to store the portal path information
	void PrintMemoryInfo();

	// Loads the next set of "do not load" ipl swap interiors
	void UpdateActiveIPLSwapInteriorList();
	bool IsAnySwapInteriorLoaded() const;
	bool IncrementSwapSet();
	void BuildIPLSwapHashkeyMap();

	// Occlusion builder functions
	void DrawDebug();
	void UpdateDebugSelectedOcclusionGroup(audEnvironmentGroupInterface** occlusionGroupListPtr);
	void DrawLoadedOcclusionPaths();
#endif

public:

	static const u8 sm_MaxOcclusionPathDepth = 5;			// Amount of portals to travel through before we consider it fully occluded
	static const u8 sm_MaxOcclusionFullPathDepth = 3;		// Depth we build all paths, after which we only build essential paths and only look to use them in rare situations
	static const f32 sm_MaxOcclusionPathDistance;			// Path distance before we consider it fully occluded

#if __BANK
	static bool sm_EnableEmitterOcclusion;
	static bool sm_EnableAmbientOcclusion;
	static bool sm_EnableSpawnEffectsOcclusion;
	static bool sm_EnableRequestedEffectsOcclusion;
	static bool sm_EnableCollisionOcclusion;
#endif

private:

	// m_OcclusionBoundingBoxes and m_ExteriorOcclusionFactor use zones defined by these radials and elevations.
	// Each radial/elevation covers the degrees between itself and the next radial, i.e. radial 1 covers 45-89.9 degrees, and elevation 3 covers 135-179.9

	// Occlusion Zone Radials		Occlusion Zone Elevations

	//    7   0   1						0   1      
	//     \  |  /						|  /		
	// 	    \ | /						| /	  
	//  6 _ _\|/_ _ 2					|/_ _ 2		  
	//  	 /|\		                |\
	//      / | \						| \
	//     /  |  \						|  \
	//    5	  4   3							3	   
		
	// OcclusionZones/BoundingBoxes = Processed / Computed data from the probes are turned into the zones/boxes
	atMultiRangeArray<Vector3, g_audNumOccZoneElevations, g_audNumOccZoneRadials> m_OcclusionBoundingBoxes;		
	atMultiRangeArray<Vector3, g_audNumOccZoneElevations, g_audNumOccZoneRadials> m_OcclusionBoundingBoxesExtend;

	// Based on portals leading outside, how open they are, etc.
	atMultiRangeArray<f32, g_audNumOccZoneElevations, g_audNumOccZoneRadials> m_ExteriorOcclusionFactor;
	atMultiRangeArray<audSimpleSmoother,g_audNumOccZoneElevations , g_audNumOccZoneRadials> m_ExteriorOcclusionSmoother;

	audCurve m_AngleFactorToOcclusionFactor;
	audCurve m_DistanceToAngleFactorDamping;
	audCurve m_GlassBrokenAreaToBlockage;
	audCurve m_VerticalAngleToVehicleOcclusion;

	f32 m_GlobalOcclusionDampingFactor;

	f32 m_OutsideWorldOcclusion;
	f32 m_OutsideWorldOcclusionIgnoringInteriors;
	f32 m_OutsideWorldOcclusionAfterPortals;
	audSimpleSmoother m_OutsideWorldOcclusionSmoother;
	audSimpleSmoother m_OutsideWorldOcclusionSmootherIgnoringInteriors;
	
	audCurve m_DistanceToBlockedCurve;
	audCurve m_AverageBlockedToBlockedFactor;
	atRangeArray<f32, 4> m_BlockedInDirection;
	atRangeArray<audSimpleSmoother, 4> m_BlockedInDirectionSmoother;
	audCurve m_BlockedToLinearVolume;
	atRangeArray<f32, 4> m_BlockedLinearVolume;

	atMap<const CInteriorProxy*, naOcclusionInteriorInfo*> m_LoadedInteriorInfos;

	naOcclusionPathNodeData m_OcclusionPathNodeDataRoot;

	u32 m_LastFrameProbeUsedToDetectDoor;

	static atFixedArray<const CInteriorProxy*, 64> sm_InteriorMetadataLoadList;

	static bool sm_IsPortalOcclusionEnabled;	// Uses -noPortalOcclusion to turn the portal occlusion system off, defaults to true

#if __BANK
	atMap<CInteriorProxy*, naOcclusionInteriorMetadata> m_InteriorMetadata;
	atMap<u32, u32> m_SwappedInteriorHashkey;
	s32 m_CurrentIntProxySlot;
	s32 m_CurrentRoomIdx;
	s32 m_CurrentPortalIdx;
	s32 m_CurrentInteriorLoadStartFrame;
	u32 m_CurrentOcclusionBuildStep;
	u32 m_NumAttemptsAtCurrentBuildStep;
	u32 m_InteriorUnloadStartFrame;
	u32 m_InteriorLoadStartFrame;
	u32 m_SwapInteriorSetIndex;
	atArray<u32> m_InteriorDoNotUseList;
	atArray<u32> m_InteriorBuildList;
	atArray<u32> m_OcclusionPathKeyTracker;
	atArray<u32> m_ActiveInteriorSwapList;
	atArray<naOcclusionFailure> m_FailureList;
	bool m_HasStartedWaitingForPortalToLoad;
	bool m_HasInteriorUnloadTimerStarted;
	bool m_HasInteriorLoadTimerStarted;
	bool m_IsBuildingOcclusionPaths;
	bool m_AudioOcclusionBuildToolEnabled;
	bool m_AudioOcclusionBuildToolStartBuild;
	bool m_AudioOcclusionBuildToolContinueBuild;
	bool m_AudioOcclusionBuildToolBuildInteriorList;
	bool m_HasStartedLoadScene;
	bool m_FreezeInteriorLoading;
#endif

#if __ASSERT
	atMap<const CInteriorProxy*, u32> m_FailedInteriorLoadFrameCountMap;
#endif

};

#endif // AUD_NAOCCLUSIONMANAGER_H
