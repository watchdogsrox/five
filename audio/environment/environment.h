// 
// audio/environment/environment.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_NAENVIRONMENT_H
#define AUD_NAENVIRONMENT_H

#include "atl/array.h"
#include "audio/asyncprobes.h"
#include "audio/gameobjects.h"
#include "audioengine/environment.h"
#include "audioengine/curve.h"
#include "audioengine/curverepository.h"
#include "audioengine/category.h"
#include "audioengine/soundset.h"
#include "audioengine/widgets.h"
#include "audioeffecttypes/biquadfiltereffect.h"
#include "bank/bank.h"
#include "fwscene/search/Search.h"
#include "math/float16.h"
#include "system/param.h"
#include "pathserver/PathServer.h"
#include "scene/world/gameWorld.h"

class naEnvironmentGroup;

namespace rage
{
	class audSound;
}

#define AUD_NUM_SECTORS 81
#define AUD_NUM_SECTORS_ACROSS 9
#define AUD_HALF_NUM_SECTORS_ACROSS 4
// center_sector = (num_sectors-1)/2
#define AUD_CENTER_SECTOR 40

const u8 g_MaxInterestingLocalObjects = 64;
const u8 g_MaxInterestingLocalTrees = 64;
const u32 g_MaxResonatingObjects = 64;
const u32 g_MaxWindSoundsHistory = 10;


#if 1//!USE_AUDMESH_SECTORS
// f32 to save a few LHS
//////////////////////////////////////////////////////////////////////////
// These numbers are wrong and based on old world dimensions.
// Since the world representation code has no concept of sectors anymore, please
// review these to avoid unexpected behaviour. The correct world dimensions
// are in scene/world/WorldLimits.h
//
const f32 g_fAudNumWorldSectorsX = 100.0f;
const f32 g_fAudNumWorldSectorsY = 100.0f;

const u32 g_audNumWorldSectorsX = 100;
const u32 g_audNumWorldSectorsY = 100;

const u32 g_audNumWorldSectors = (g_audNumWorldSectorsX * g_audNumWorldSectorsY);
const f32 g_audWidthOfSector = 150.0f;
const f32 g_audDepthOfSector =  150.0f;
const u32 g_audMaxCachedSectorOps = 64;
// For the off line environmental metrics calculation.
const s32 g_audWorldSectorsMinX = -7500; 
const s32 g_audWorldSectorsMinY = -7500; 
#define DSECTOR_HYPOTENUSE 424.26f // WARNING : 2 * SECTOR_HYPOTENUSE, if we change the values above we have to recalculate this value.
//////////////////////////////////////////////////////////////////////////
#endif

#define NUMBER_OF_WATER_TEST 1000

#define AUD_MAX_LINKED_ROOMS 6 

struct audShockwave
{
	audShockwave() : radius(0.f), radiusChangeRate(0.f), maxRadius(0.f), intensity(1.f), intensityChangeRate(0.f), distanceToRadiusFactor(1.f), delay(0.f), lastUpdateTime(0)
	{

	}
	Vector3 centre;
	f32 radius;
	f32 radiusChangeRate;
	f32 maxRadius;
	f32 intensity;
	f32 intensityChangeRate;
	f32 distanceToRadiusFactor;
	f32 delay;
	u32 lastUpdateTime;
	bool centered;
};

enum
{
	kInvalidShockwaveId = -1,
	kMaxRegisteredShockwaves = 8,
};

enum audAmbienceDirection
{
	AUD_AMB_DIR_NORTH = 0,
	AUD_AMB_DIR_EAST,
	AUD_AMB_DIR_SOUTH,
	AUD_AMB_DIR_WEST,
	AUD_AMB_DIR_LOCAL,
	AUD_AMB_DIR_MAX,
};
enum audFBSectors
{
	AUD_SECTOR_FL= 0,
	AUD_SECTOR_FR,
	AUD_SECTOR_RL,
	AUD_SECTOR_RR, 
	MAX_FB_SECTORS 
};
enum audTreeTypes 
{
	AUD_BUSHES = 0,
	AUD_SMALL_TREES,
	AUD_BIG_TREES,
	MAX_AUD_TREE_TYPES
};

enum audEnvironmentSounds
{
	AUD_ENV_SOUND_UNKNOWN,
	AUD_ENV_SOUND_WIND,
};

struct audSector
{
	audSector()
	{
		numHighwayNodes = 0;
		tallestBuilding = 0;
		numBuildings = 0;
		numTrees = 0;
		isWaterSector = false;
	}
	
	u8 numHighwayNodes;
	u8 tallestBuilding;
	u8 numBuildings;
	u8 numTrees:7;
	bool isWaterSector:1;
};

struct audParseNodeCallbackData
{
	s32 numHighwayNodes;
	s32 numNodes;
	Vector3 minPos;
	Vector3 maxPos;
};

struct audRainXfade
{
	f32 waterRain;
	f32 buildingRain;
	f32 treeRain;
	f32 heavyRain;
	s32 pan;
	f32 x;
	f32 y;
};

struct audResonanceSoundInfo
{
	audSound* 		resonanceSound;
	audMetadataRef  resoSoundRef;
	f32		  		attackTime;
	f32		  		releaseTime;
	f32		  		resonanceIntensity;
	f32				slowmoResoPhase; 
	f32				slowmoResoVolLin; 
};

struct audShockwaveSoundInfo
{	
	f32		  		attackTime;
	f32		  		releaseTime;
	f32				prevVol;
};

struct audInterestingObjectInfo
{
	RegdEnt object;
	audResonanceSoundInfo resoSoundInfo;
	audShockwaveSoundInfo shockwaveSoundInfo;
	naEnvironmentGroup* envGroup;
	audSound* rainSound;
	audSound* windSound;
	audSound* randomAmbientSound;
	s32 macsComponent;
	bool stillValid;
	BANK_ONLY(bool underCover);
};

struct audInterestingTreesInfo
{
	RegdEnt tree;	
	audFBSectors sector;
	audTreeTypes type; 
	audSound *windSound;
};
struct audTreesBgInfo
{
	f32 bushesFitness;
	f32 treesFitness;
	f32 bigTreesFitness;
	u32 numBushes;
	u32 numTrees;
	u32 numBigTrees;
};
struct audBgTreeSounds
{
	f32 fitness;
	f32 avgDistance;
	BANK_ONLY(f32 linVol;)
	audTreeTypes type; 
	audSimpleSmoother volSmoother;
	audSound *sound;
}; 

struct audEntityResonance
{
	void Clear() { entity = NULL; resoSnd = NULL; component = 0; }
	const CEntity * entity; //NB: we're not reference tracking this pointer so be careful not to dereference it unless it's known to be safe
	audSound * resoSnd;
	u32 component;
};
struct audWindSoundsHistory
{
	audMetadataRef soundRef;
	u32 time;
};
class naEnvironment
{
public:
	naEnvironment();
	~naEnvironment();

	void Init();
	void Shutdown();
	void PurgeInteriors();

// Game-thread GTA-specific functions
	void RegisterDeafeningSound(Vector3 &pos, f32 volume);
	void GetDeafeningStrength(f32* leftEar, f32* rightEar);
	bool IsDeafeningOn();
	void UpdateLocalEnvironment();
	void UpdateLocalReverbEnvironment();
	void LocalObjectsList_StartAsyncUpdate();
	void LocalObjectsList_EndAsyncUpdate();
	void UpdateLocalEnvironmentObjects();
	void Update();
	static bool ParseNodeCallback(CPathNode * pNode, void * pData);
	f32 GetRollOffFactor();
	bool AreWeInAnInterior(bool *isInteriorASubway=NULL, f32 *playerInteriorRatio=NULL);
	bool AreWeAPassengerInATaxi() const;
	void GetEchoSuitablilityForSourcePosition(const Vector3& sourcePosition, f32* suitability);
	s32 GetBestEchoForSourcePosition(const Vector3& sourcePosition, s32* oppositeEcho=NULL);
	Vector3* GetEchoPosition() { return m_EchoPosition; }

	void ResetHasNaveMeshInfoBeenUpdated() { m_NavmeshInformationUpdatedThisFrame = false; };

	bool HasNavMeshInfoBeenUpdated() const {return m_NavmeshInformationUpdatedThisFrame;};


	bool ShouldTriggerWindGustEnd();

	void CalculateEchoDelayAndAttenuation(u32 echoIndex, const Vector3& sourcePosition, u32* predelay, f32* extraAttenuation, Vector3* projectedPosition, u32 depth=0);
	void GetInteriorGunSoundMetricsForExterior(f32& returnVolume, f32& returnHold);
	void SetLevelReflectionNormal(const Vector3& levelReflection, s32 radialIndex);
	void SetUpperReflectionNormal(const Vector3& upperReflection, s32 radialIndex);
#if __BANK
	// audio world sector stuff
	void AddBuildingToWorld(const Vector3 &pos, CEntity* entity);
	void AddTreeToWorld(const Vector3 &pos, CEntity* entity);
	void RemoveBuildingFromWorld(const Vector3 &pos);
	void RemoveTreeFromWorld(const Vector3 &pos);
	void GenerateAudioWorldSectorsInfo();
	void CheckForWaterSector();
	void ProcessWaterTest(const Vector3 &startPoint, const Vector3 &deltaMovement);
	void CleanUpWaterTest();
	void AddWaterQuad(const Vector3 &v1, const Vector3 &v2, const Vector3 &v3, const Vector3 &v4);
#endif
	f32 ComputeWorldSectorIndex(const Vector3 &pos) const;
	void ComputeRainMixForSector(const u32 sectorIndex, const f32 rainVol, audRainXfade *directionMixes);

	f32	GetBuiltUpFactorOfSector(u32 sector);

	f32 GetBuildingHeightForSector(const u32 sectorIndex) const;
	f32 GetBuildingCountForSector(const u32 sectorIndex) const;
	f32 GetTreeCountForSector(const u32 sectorIndex) const;
	f32 GetWaterAmountForSector(const u32 sectorIndex) const;
	f32 GetHighwayAmountForSector(const u32 sectorIndex) const;

	f32 GetAveragedBuildingHeightForIndex(u32 index);
	f32 GetAveragedBuildingCountForIndex(u32 index);
	f32 GetAveragedTreeCountForIndex(u32 index);
	f32 GetAveragedWaterAmountForIndex(u32 index);
	f32 GetAveragedHighwayAmountForIndex(u32 index);

	bool IsCameraShelteredFromRain() const;

	f32 GetVehicleCountForDirection(audAmbienceDirection sector) const { return m_VehicleSegmentCounts[sector]; }
	f32 GetVehicleDensityForDirection(audAmbienceDirection sector) const { return m_VehicleAmountToVehicleDensity.CalculateValue(m_VehicleSegmentCounts[sector]); }
	f32 GetHostilePedCountForDirection(audAmbienceDirection sector) const { return m_HostilePedSegmentCount[sector]; }
	f32 GetExteriorPedCountForDirection(audAmbienceDirection sector) const { return m_ExteriorPedSegmentCounts[sector]; }
	f32 GetInteriorPedCountForDirection(audAmbienceDirection sector) const { return m_InteriorPedSegmentCounts[sector]; }
	f32 GetInteriorOccludedPedCountForDirection(audAmbienceDirection sector) const { return m_InteriorOccludedPedSegmentCounts[sector]; }
	f32 GetExteriorPedPanicCountForDirection(audAmbienceDirection sector) const { return m_ExteriorPedPanicCount[sector]; }
	f32 GetInteriorPedPanicCountForDirection(audAmbienceDirection sector) const { return m_InteriorPedPanicCount[sector]; }

	inline f32 GetAveragePedZPos() const {return m_AveragePedZPos;}
	inline f32 GetPedFractionMale() const {return m_PedFractionMale;}

	f32 GetBuiltUpFactorForDirection(audAmbienceDirection sector) const { return sector >= AUD_AMB_DIR_LOCAL ? m_BuiltUpFactor : m_DirectionalBuiltUpFactor[sector]; }
	f32 GetBuildingDensityForDirection(audAmbienceDirection sector) const { return sector >= AUD_AMB_DIR_LOCAL ? m_BuildingDensityFactor : m_DirectionalBuildingDensity[sector]; }
	f32 GetTreeDensityForDirection(audAmbienceDirection sector) const { return sector >= AUD_AMB_DIR_LOCAL ? m_TreeDensityFactor : m_DirectionalTreeDensity[sector]; }
	f32 GetWaterFactorForDirection(audAmbienceDirection sector) const { return sector >= AUD_AMB_DIR_LOCAL ? m_WaterFactor : m_DirectionalWaterFactor[sector]; }
	f32 GetHighwayFactorForDirection(audAmbienceDirection sector) const { return sector >= AUD_AMB_DIR_LOCAL ? m_HighwayFactor : m_DirectionalHighwayFactor[sector]; }

	bool IsPlayerInVehicle(f32 *inCarRatio = NULL, bool *isPlayerVehicleATrain=NULL) const;
	u32 GetLastTimeWeWereAPassengerInATaxi() { return m_LastTimeWeWereAPassengerInATaxi; }

	fwInteriorLocation GetRoomIntLocFromPortalIntLoc(const fwInteriorLocation portalIntLoc) const;

	// This is used by the interior code to get the interior game object for a given room.
	// Loops through the rooms trying to match the hashkey, should only be used when initially setting the interior information, otherwise use the GetInteriorSettings... fn's
	void FindInteriorSettingsForRoom(const u32 interiorHash, const u32 roomHash, const InteriorSettings*& intSettings, const InteriorRoom*& intRoom) const;

	// Used by the audio code to get the information we stored in the MloModelInfo when initially setting up the interior.
	void GetInteriorSettingsForEntity(const CEntity* entity, const InteriorSettings*& intSettings, const InteriorRoom*& intRoom);
	void GetInteriorSettingsForInteriorLocation(const fwInteriorLocation intLoc, const InteriorSettings*& intSettings, const InteriorRoom*& intRoom);
	void GetInteriorSettingsForInterior(const CInteriorInst* intInst, const s32 roomIdx, const InteriorSettings*& intSettings, const InteriorRoom*& intRoom);

	void GetDebugInteriorSettings(const InteriorSettings*& intSettings, const InteriorRoom*& intRoom) const;

	u32 GetRoomToneHash() {return m_RoomToneHash;}
	u32 GetRainType() {return m_RainType;}

	bool IsRoomInLinkedRoomList(const InteriorSettings* linkedInteriorSettings, const InteriorRoom* linkedIntRoom);

#if __BANK
	static void ProcessSectors(bool process){sm_ProcessSectors = process;}   
	static void DrawAudioWorldSectors(bool draw);
	static bool IsProcessinWorldAudioSectors() {return sm_ProcessSectors;}
#endif 
#if __DEV
	static void DebugDraw();
#endif

	f32 GetVectorToNorthXYAngle(const Vector3& v);
	f32 GetVectorToUpAngle(const Vector3& v);

	void SetScriptedSpecialEffectMode(const audSpecialEffectMode mode) { m_ScriptedSpecialEffectMode = mode; }

private:
	void UpdateInsideCarMetric();

	void UpdateLinkedRoomList(CInteriorInst* pIntInst);
	void AddLinkedRoom(CInteriorInst* pIntInst, s32 roomIdx);
	void ClearLinkedRooms();

	BANK_ONLY(void ProcessCachedWorldSectorOps(););
	BANK_ONLY(void WriteAudioWorldSectorsData(););
	BANK_ONLY(void AbortAudWorldSectorsGeneration(););
	BANK_ONLY(void InitAudioWorldSectorsGenerator(););

	void UpdateDeafeningState();
	
	void UpdateEchoPositions();
	void UpdateNavmeshSourceEnvironment();

	void UpdateMetrics();
	void UpdateBuiltUpMetrics(f32 ratioCtoR, f32 ratioCtoU, u32 xCrossfadeIndex, u32 yCrossfadeIndex, u32 diagonalCrossfadeIndex);
	void UpdateBuildingDensityMetrics(f32 ratioCtoR, f32 ratioCtoU, u32 xCrossfadeIndex, u32 yCrossfadeIndex, u32 diagonalCrossfadeIndex);
	void UpdateTreeDensityMetrics(f32 ratioCtoR, f32 ratioCtoU, u32 xCrossfadeIndex, u32 yCrossfadeIndex, u32 diagonalCrossfadeIndex);
	void UpdateWaterMetrics(f32 ratioCtoR, f32 ratioCtoU, u32 xCrossfadeIndex, u32 yCrossfadeIndex, u32 diagonalCrossfadeIndex);
	void UpdateHighwayMetrics(f32 ratioCtoR, f32 ratioCtoU, u32 xCrossfadeIndex, u32 yCrossfadeIndex, u32 diagonalCrossfadeIndex);
	void MapDirectionalMetricsToSpeakers();

	void ComputeRainMixForSector(const u32 sectorX, const u32 sectorY, audRainXfade &rain) const;
	f32  GetBuildingHeightForSector(u32 sectorX, u32 sectorY);
	f32  GetBuildingCountForSector(u32 sectorX, u32 sectorY);
	f32  GetTreeCountForSector(u32 sectorX, u32 sectorY);
	f32  GetWaterAmountForSector(u32 sectorX, u32 sectorY);
	f32  GetHighwayAmountForSector(u32 sectorX, u32 sectorY);
	
	f32  GetAveragedBuildingHeightFromXYOffset(f32 xOffset, f32 yOffset);
	f32  GetAveragedBuildingCountFromXYOffset(f32 xOffset, f32 yOffset);
	f32  GetAveragedTreeCountFromXYOffset(f32 xOffset, f32 yOffset);
	f32  GetAveragedWaterAmountFromXYOffset(f32 xOffset, f32 yOffset);
	f32  GetAveragedHighwayAmountFromXYOffset(f32 xOffset, f32 yOffset);
	
	CEntity *GetInterestingLocalObjectCached(u32 index) ;
	naEnvironmentGroup *GetInterestingLocalEnvGroupCached(u32 index) ;
	void GetInterestingLocalObjectInfo(u32 index, audInterestingObjectInfo &objectInfo);

	u32 GetNumInterestingLocalObjectsCached() const;

public:

#if __BANK
	static void AddWidgets(bkBank &bank);
	void DrawDebug() const;
	void DrawAudioWorldSectors();
	void DrawWeaponEchoes();
	void IncrementDeclinedPathServerRequestCount() { m_DeclinedPathServerRequestsThisFrame++; }
	void IncrementAcceptedPathServerRequestCount() { m_AcceptedPathServerRequestsThisFrame++; }
	void ResetPathServerRequestsCounter() { m_DeclinedPathServerRequestsThisFrame = 0; m_AcceptedPathServerRequestsThisFrame = 0; }
	static void CreateRAVEObjectsForCurrentInteriorAndRooms();
	static void CreateRAVEObjectsForCurrentInterior();
	static void CreateRAVEObjectsForCurrentRoom();
	static void CreateRAVEInteriorRoomObject(const char* instanceName, const char* roomName);
#endif

	s32 AllocateShockwave();
	void FreeShockwave(const s32 shockwaveId);
	void UpdateShockwave(s32 &shockwaveId, const audShockwave &shockwaveData);
	void UpdateInterestingObjects();
	void UpdateInterestingTrees();
	void UpdateBgTreeSound(const u8 soundIdx, const u8 sector, const u32 numTrees,const u8 type, const f32 avgDistance,Vec3V_In soundDir);

	void ForceAmbientMetricsUpdate() { m_ForceAmbientMetricsUpdate = true; }

	f32 GetSpeakerReverbWet(u32 speaker, u32 reverb);

	f32 GetUnderCoverFactor() {return m_PlayerUnderCoverFactor;}
	void CalculateUnderCoverFactor();

	CEntity *GetInterestingLocalObject(u32 index) ;
	naEnvironmentGroup *GetInterestingLocalEnvGroup(u32 index);
	bool IsObjectUnderCover(u32 index);

	const CEntity *GetInterestingLocalObject(u32 index) const;
	const audInterestingObjectInfo *GetInterestingLocalObjectInfo(u32 index) const;

	const ModelAudioCollisionSettings * GetInterestingLocalObjectMaterial(u32 index) const;

	void SpikeResonanceObjects(const f32 impulse, const Vector3 &pos, const f32 maxDist, const CEntity* entity = NULL);

	CInteriorInst* GetListenerInteriorInstance() { return m_ListenerInteriorInstance; }
	int GetInteriorRoomId() { return m_InteriorRoomId; }	// This is the roomIdx for the CInteriorInst

	const InteriorSettings * GetListenerInteriorSettingsPtr() const { return m_ListenerInteriorSettings; }
	const InteriorRoom* GetListenerInteriorRoomPtr() const { return m_ListenerInteriorRoom; }

	fwInteriorLocation GetLinkedRoom(const u32 idx)
	{ 
		if(naVerifyf(idx < AUD_MAX_LINKED_ROOMS, "Requesting out of bounds linked room"))
		{
			return m_LinkedRoom[idx];
		}
		fwInteriorLocation invalidLocation;
		return invalidLocation;
	}

	bool ShouldUsePortalFactorForWeather() { return m_UsePortalInfoForWeather; }

	void GetReverbFromSEMetric(const f32 sourceEnvironmentWet, const f32 sourceEnvironmentSize, f32* reverbSmall, f32* reverbMedium, f32* reverbLarge);
	f32 GetSourceEnvironmentReverbWetFromSEMetric(const u32 sourceEnvironment);
	f32 GetSourceEnvironmentReverbSizeFromSEMetric(const u32 sourceEnvironment);

private:
	void UpdateResonance(const u32 objectIndex);
	void UpdateRainObject(const u32 objectIndex);
	void CreateRainObjectEnvironmentGroup(const u32 objectIndex);
	void UpdateShockwaves();
	void UpdateSectorFitness(const u8 sector);
	void PlayBgTreesWind(const u8 sector, const u8 soundIdx, const u8 newSoundIdx,const audMetadataRef soundRef,const u8 type,const f32 fitness,const f32 avgDistance,const u32 numTrees,Vec3V_In soundDir,bool change);
	void GetWindTreeSectorInfo(audMetadataRef &soundRef, const u8 sector,audTreeTypes &type,f32 &fitness,f32 &avgDistance,Vec3V_InOut soundDir,u32 &numTrees);

	void UpdateMichaelSpeacialAbilityForResoObjects(const u32 objectIndexm,f32 &resoLinVol);
	static f32 ProcessResoVolume(const f32 inputSignal, const f32 attack, const f32 release, const f32 state);

	static bool IsObjectDestroyed(CEntity* pEntity);
	static bool FindCloseObjectsCB(fwEntity* entity, void* data);
	void AddInterestingObject(CEntity *obj ,const u32 shockwaveSoundHash,const u32 resoSoundHash,s32 component,bool checkCovered = false);
	void AddLocalTree(CEntity *tree);
	static bool ms_localObjectSearchLaunched;
	static fwSearch ms_localObjectSearch;

//*****************************************************
	// GTA-specific, game-thread variables
	audCurve m_BuiltUpToRolloff;

	// Deafening stuff
	u8 m_DeafeningState[2];
	u32 m_DeafeningPhaseEndTime[2];
	f32 m_DeafeningMaxStrength[2];
	f32 m_DeafeningMinStrength[2];
	audCurve m_DeafeningAttenuationCurve;
	audCurve m_DeafeningFilterCurve;
	audCurve m_DeafeningReverbCurve;
	audCurve m_DeafeningVolumeToStrengthCurve;
	audCurve m_DeafeningDistanceRolloffCurve;
	f32 m_EarStrength[2];

	// Local environment metric
	Vector3 m_LevelReflectionNormal[24];
	Vector3 m_UpperReflectionNormal[8];

	f32	m_PlayerUnderCoverFactor;
	audSmoother m_PlayerUnderCoverSmoother;

	// Echo specific metrics
	f32		m_EchoSuitability[8];
	f32		m_EchoDistance[8];
	audCurve m_DistanceToEchoCurve;
	Vector3 m_EchoPosition[8];
	Vector3 m_EchoDirection[8];
	audCurve m_DistanceToDelayCurve;
	audCurve m_SilentWhenEchoCoLocatedCurve;
	audCurve m_EchoGoodnessLinearVolumeCurve;
	audCurve m_EchoGoodnessDbVolumeCurve;
	audCurve m_AngleOfIncidenceToSuitabilityCurve;
	audCurve m_OutdoorInteriorWeaponVolCurve;
	audCurve m_OutdoorInteriorWeaponHoldCurve;

	// Local reverb environment metrics
	TPathHandle m_SourceEnvironmentPathHandle;

	u32 m_NumValidPolys;
	u32 m_SourceEnvironmentMetrics[MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES];
	f32 m_SourceEnvironmentPolyAreas[MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES];
	Vector3 m_SourceEnvironmentPolyCentroids[MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES];
	f32 m_SourceEnvironmentPolyReverbWet[MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES][g_NumSourceReverbs];
	Vector3 m_ListenerPositionLastSourceEnvironmentUpdate;
	u32 m_SourceEnvironmentTimeLastUpdated;

	audCurve m_SourceEnvironmentToReverbWetCurve;
	audCurve m_SourceEnvironmentToReverbSizeCurve;
	audCurve m_SourceEnvironmentSizeToWetScaling;

	atFixedBitSet<MAX_SURROUNDING_POLYS_FOR_AUDIO_PROPERTIES>				m_CoveredPolys;

	f32 m_SpeakerReverbWets[4][3];
	audCurve m_SourceReverbCrossfadeCurves[g_NumSourceReverbs];
	audCurve m_AreaToListenerReverbCurve;
	audCurve m_DistanceToListenerReverbCurve;
	audSimpleSmoother m_SpeakerReverbWetSmoothers[4][3];

	// @OCC TODO - Figure out interior detection based on camera so we don't get wonky crap when player in 1 room camera in another
	bool m_IsListenerInHead;
	bool m_ForceAmbientMetricsUpdate;

	// world sector stuff
	u32 m_NumBuildingAddsThisFrame, m_NumTreeAddsThisFrame;
	u32 m_NumBuildingRemovesThisFrame, m_NumTreeRemovesThisFrame;
#if __BANK
	u32 m_WSBuildingAddIndices[g_audMaxCachedSectorOps];
	s32 m_WSBuildingAddHeights[g_audMaxCachedSectorOps];
	u32 m_WSTreeAddIndices[g_audMaxCachedSectorOps];
	u32 m_WSTreeRemoveIndices[g_audMaxCachedSectorOps];
	u32 m_WSBuildingRemoveIndices[g_audMaxCachedSectorOps];
#endif

	// BuiltUpFactor stuff
	f32 m_BuiltUpFactor;
	audSimpleSmoother m_BuiltUpSmoother;
	audCurve m_HeightToBuiltUpFactor;

	// BuildingDensity stuff
	f32 m_BuildingDensityFactor;
	audSimpleSmoother m_BuildingDensitySmoother;
	audCurve m_BuildingNumToDensityFactor;

	// TreeDensity stuff
	f32 m_TreeDensityFactor;
	audSimpleSmoother m_TreeDensitySmoother;
	audCurve m_TreeNumToDensityFactor;

	// WaterFactor stuff
	f32 m_WaterFactor;
	audSimpleSmoother m_WaterSmoother;
	audCurve m_WaterAmountToWaterFactor;

	// Highway stuff
	f32 m_HighwayFactor;
	audSimpleSmoother m_HighwaySmoother;
	audCurve m_HighwayAmountToHighwayFactor;

	// Vehicle stuff
	audCurve m_VehicleAmountToVehicleDensity;
	audCurve m_VehicleDistanceSqToVehicleAmount;
	audCurve m_VehicleSpeedToVehicleAmount;

	atRangeArray<f32,AUD_NUM_SECTORS> m_BuildingHeights;
	atRangeArray<f32,AUD_NUM_SECTORS> m_BuildingCount;
	atRangeArray<f32,AUD_NUM_SECTORS> m_TreeCount;
	atRangeArray<f32,AUD_NUM_SECTORS> m_WaterAmount;
	atRangeArray<f32,AUD_NUM_SECTORS> m_HighwayAmount;
	atRangeArray<f32,AUD_NUM_SECTORS> m_BuiltUpFactorPerZone; //Built up factor for each zone, we have to move it to being pre-calculated.

	audSimpleSmoother m_BuiltUpDirectionalSmoother[4];
	audSimpleSmoother m_BuildingDensityDirectionalSmoother[4];
	audSimpleSmoother m_TreeDensityDirectionalSmoother[4];
	audSimpleSmoother m_WaterDirectionalSmoother[4];
	audSimpleSmoother m_HighwayDirectionalSmoother[4];

	f32 m_DirectionalBuiltUpFactor[4];
	f32 m_DirectionalBuildingDensity[4];
	f32 m_DirectionalTreeDensity[4];
	f32 m_DirectionalWaterFactor[4];
	f32 m_DirectionalHighwayFactor[4];

	audSimpleSmoother m_SpeakerBuiltUpSmoother[4];
	audSimpleSmootherDiscrete m_BuiltUpDampingFactorSmoother;

	u32 m_LastTimeWeWereAPassengerInATaxi;

public:
	f32 m_SpeakerBuiltUpFactors[4];
	f32 m_SpeakerBlockedFactor[4];
	f32 m_SpeakerBlockedLinearVolume[4];
	f32 m_SpeakerOWOFactor[4];

	// Interesting local objecst : Shockwave|Rain|Wind|Resonance
	static f32 sm_HeliShockwaveRadius;
	static f32 sm_TrainDistanceThreshold;
	static f32 sm_TrainShockwaveRadius;
	static f32 sm_MeleeResoImpulse;
	static f32 sm_PistolResoImpulse;
	static f32 sm_RifleResoImpulse;
	static f32 sm_WeaponsResonanceDistance;
	static f32 sm_ExplosionsResonanceImpulse;
	static f32 sm_ExplosionsResonanceDistance;
	static f32 sm_VehicleCollisionResonanceImpulse;
	static f32 sm_VehicleCollisionResonanceDistance;

	BANK_ONLY(static bool sm_DebugGustWhistle;)
	BANK_ONLY(static bool sm_DebugGustRustle;)
private:

	audSimpleSmoother m_InterestingObjectRainSmoother;

	audSmoother m_CarInteriorFilterSmoother;
	f32 m_CarInteriorFilterCutoff;

	audEntityResonance m_ResonatingObjects[g_MaxResonatingObjects];

	atRangeArray<audInterestingObjectInfo,g_MaxInterestingLocalObjects> m_InterestingLocalObjects;
	atRangeArray<audInterestingObjectInfo,g_MaxInterestingLocalObjects> m_InterestingLocalObjectsCached;
	atRangeArray<audWindSoundsHistory,g_MaxWindSoundsHistory> m_WindTriggeredSoundsHistory;

	// wind through trees
	atRangeArray<audInterestingTreesInfo,g_MaxInterestingLocalTrees> m_LocalTrees;
	atRangeArray<audTreesBgInfo,MAX_FB_SECTORS> m_BackGroundTrees; 
	atRangeArray<atRangeArray<audBgTreeSounds,MAX_FB_SECTORS>,2> m_BackGroundTreesSounds;
	atRangeArray<f32,MAX_AUD_TREE_TYPES> m_BackGroundTreesFitness;
	atRangeArray<audCurve,MAX_AUD_TREE_TYPES> m_NumTreesToVolume;
	atRangeArray<audCurve,MAX_AUD_TREE_TYPES> m_TreesDistanceToVolume;
	atRangeArray<u8,MAX_FB_SECTORS> m_CurrentSoundIdx;

	u32 m_NextInterestingObjectRandTime[g_MaxInterestingLocalObjects];

	u32 m_NextWindObjectTime;

	u32 m_NumInterestingLocalObjectsCached;
	u32 m_NumLocalTrees;

	f32 m_WaterHeightAtListener;

	// N, E, S, W, local
	f32 m_VehicleSegmentCounts[5];
	f32 m_ExteriorPedSegmentCounts[4];
	f32 m_HostilePedSegmentCount[4];
	f32 m_InteriorPedSegmentCounts[4];
	f32 m_InteriorOccludedPedSegmentCounts[4];
	f32 m_ExteriorPedPanicCount[4];
	f32 m_InteriorPedPanicCount[4];
	f32 m_AveragePedZPos;
	f32 m_PedFractionMale;

	bool m_IsPlayerInVehicle;
	f32 m_PlayerInCarRatio;
	audSimpleSmoother m_PlayerInCarSmoother;
	bool m_IsPlayerVehicleATrain;

	bool m_NavmeshInformationUpdatedThisFrame;


	bool m_PlayerIsInASubwayStationOrSubwayTunnel;
	bool m_PlayerIsInASubwayOrTunnel;
	bool m_PlayerWasInASubwayOrTunnelLastFrame;
	bool m_PlayerIsInAnInterior;
	f32 m_PlayerInteriorRatio;
	audSimpleSmoother m_PlayerInteriorRatioSmoother;
	u32 m_RoomToneHash;
	u32 m_RainType;

	audSpecialEffectMode m_ScriptedSpecialEffectMode;

	
#if RSG_BANK
	audSpecialEffectMode m_SpecialEffectModeOverride;
	bool m_OverrideSpecialEffectMode;
#endif

	audShockwave m_RegisteredShockwaves[kMaxRegisteredShockwaves];
	atBitSet m_RegisteredShockwavesAllocation;

	bool m_UsePortalInfoForWeather;

	atRangeArray<fwInteriorLocation, AUD_MAX_LINKED_ROOMS> m_LinkedRoom;

	u32 m_LastHighwaySectorX;
	u32 m_LastHighwaySectorY;
	bool m_RefreshHighwayFactor;
	u32 m_HighwayZoneToRefresh;

	// CInteriorInst interior code info for portal calculations
	CInteriorInst* m_ListenerInteriorInstance;
	int m_InteriorRoomId;	

	// Audio interior info
	const InteriorSettings* m_ListenerInteriorSettings;
	const InteriorRoom* m_ListenerInteriorRoom;

	// cached los results, processed via the CWorldProbeAsync class.
	audCachedLosMultipleResults m_CachedLos;

	// Wind through trees.  
	audSoundSet	m_WindBushesSoundset;
	audSoundSet	m_WindTreesSoundset;
	audSoundSet	m_WindBigTreesSoundset;

	u32 m_NumRainPropSounds;

	static audCurve sm_HDotToSlowmoResoVol, sm_VDotToSlowmoResoVol,sm_SpecialAbToFrequency;
	static audCurve sm_CityGustDotToVolSpikeForBgTrees;
	static audCurve sm_CountrySideGustDotToVolSpikeForBgTrees;

	static f32 sm_SlowMoLFOFrequencyMin;
	static f32 sm_SlowMoLFOFrequencyMax;
	static f32	sm_SqdDistanceToPlayRainSounds; 
	static f32	sm_BushesMinRadius; 
	static f32	sm_SmallTreesMinRadius; 
	static f32	sm_BigTreesMinRadius; 
	static f32 sm_TreesSoundDist;
	static f32 sm_DistancePlateau;

	static f32 sm_BigTreesFitness;
	static f32 sm_TreesFitness;
	static f32 sm_BushesFitness;
	static f32 sm_RadiusScale;

	static f32 sm_RainOnPropsVolumeOffset;

	static u32 sm_MinWindObjTime;
	static u32 sm_MaxWindObjtTime;
	static u32 sm_MinTimeToRetrigger;
	static u32 sm_WindTimeVariance;
	static u32 sm_RainOnPopsPreDealy;
	static u32 sm_MaxNumRainProps;
	static u32 sm_MinNumberOfTreesToTriggerGustEnd;
  
	static u8 sm_NumTreesForAvgDist;

	static u16 sm_ObjOcclusionUpdateTime;
	static bool sm_ProcessSectors;

#if __BANK
	atRangeArray<Vector3,NUMBER_OF_WATER_TEST>	m_WaterTestLines;
	f32											m_WaterLevel;

	u32 m_AcceptedPathServerRequestsThisFrame;
	u32 m_DeclinedPathServerRequestsThisFrame;
	

	static f32 sm_NumWaterProbeSets;

	static bool sm_DrawBgTreesInfo;
	static bool sm_MuteBgFL;
	static bool sm_MuteBgFR;
	static bool sm_MuteBgRL;
	static bool sm_MuteBgRR;
	static bool sm_DisplayResoIntensity;
	static bool sm_UseCoverAproximation;
	static bool sm_DebugCoveredObjects;
	static bool sm_DrawAudioWorldSectors;
	static bool sm_IsWaitingToInit;
	static bool sm_LogicStarted;
	static u32 sm_CurrentSectorX;
	static u32 sm_CurrentSectorY;
#endif


	// DEBUG
//	void AssertIfVeryDifferent(f32 oldValue, f32 newValue);
};

#if 1 //!USE_AUDMESH_SECTORS
extern atRangeArray<audSector, g_audNumWorldSectors> g_AudioWorldSectors;
#endif

#endif //AUD_NAENVIRONMENT_H


