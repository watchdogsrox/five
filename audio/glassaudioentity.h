// 
// audio/glassaudioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
//
#ifndef AUD_GLASSAUDIOENTITY_H
#define AUD_GLASSAUDIOENTITY_H

#include "asyncprobes.h"
#include "physics/inst.h"

#include "audio_channel.h"

#include "breakableglass/glassmanager.h"
#include "entity/entity.h"
#include "audioentity.h"
#include "spatialdata/sphere.h"
#include "audioengine/soundset.h"

#define MAX_NUM_GLASS_AREAS 20
#define MAX_NUMBER_GLASS_EVENTS 10
#define MAX_NUM_GLASS_SHARDS 10

#define GLASS_PARABOL_DEFINITION 3
class audCachedBrokenGlassTestResults : public audCachedMultipleTestResults
{
public:
	virtual void ClearAll();
	virtual bool AreAllComplete();
	virtual bool NewTestComplete();
	virtual void GetAllProbeResults();
	audCachedLos m_MainLineTest[GLASS_PARABOL_DEFINITION];
};

class audBreakableGlassEvent
{
public:

	void Init();
	void BrokenGlassGroundTest();
	void Clear();
	void IssueAsyncLineTestProbes();
	void UpdateBrokenGlassGroundTest();

	void SetParabolPoint(u32 index, Vec3V_Ref pPoint);
	void SetHitPosition(Vec3V_Ref hitPosition) { m_HitPosition = hitPosition;}
	void SetHitMagnitude(Vec3V_Ref hitMagnitude ){ m_HitMagnitude = hitMagnitude;}
	void SetGlassHandle(bgGlassHandle handle ){ m_GlassHandle = handle;}
	void SetHasFoundSolution(bool solution){ m_HasFoundSolution = solution;}
	//void SetGlassEntity(fwEntity* glassEnt){ m_GlassEntity = glassEnt;}

	Vec3V_Out GetParabolPoint (u32 index);
	Vec3V_Out GetHitMagnitude() { return m_HitMagnitude;}
	Vec3V_Out GetHitPosition() { return m_HitPosition;}
	f32 CalculateTestDuration();

	bgGlassHandle GetGlassHandle() { return m_GlassHandle;}
	audCachedBrokenGlassTestResults&	GetCachedLineTest()	{ return m_CachedLineTest;}
	bool HasFoundASolution() { return m_HasFoundSolution;}
private:
	bool ProcessBrokenGlassGroundTest();

	atRangeArray<Vec3V,GLASS_PARABOL_DEFINITION>	m_ParabolPoints;
	audCachedBrokenGlassTestResults					m_CachedLineTest;
	//fwEntity*										m_GlassEntity;
	Vec3V											m_HitPosition,m_HitMagnitude;

	f32												m_FallenRatio;
	bgGlassHandle									m_GlassHandle;
	bool											m_HasFoundSolution;

	static audCurve									sm_FallenRatioToAmplitude;

	BANK_ONLY(void LogBrokenGlassGroundTest(f32 testDuration););
	BANK_ONLY(void LogUpdateBrokenGlassGroundTest(););
};

struct areaDefinition
{
	f32 remainingGlass;
	spdSphere area;
	bgGlassHandle glassHandle;
};

struct vehicleAreaDefinition
{
	f32 remainingGlass;
	spdSphere area;
	RegdConstPhy vehicle;
};

struct glassShardDefinition
{
	RegdEnt ground;
	phHandle shard;
	f32 impactVel;
	u32 lastUpdateTime;
	bool alreadyPlayedVHit;
	bool alreadyPlayedHHit;
};

class audSmashableGlassEvent
{
public:

	audSmashableGlassEvent();
	~audSmashableGlassEvent();

	void Init();

	void Shutdown() {};

	void ReportPolyHitGround(const Vector3 &position, const CVehicle *veh);
	void ReportSmash(const CEntity*	entity, const Vector3& position, const s32 numInstantPieces, bool forced = false);
	void ReportGlassCrack(const CEntity* entity,const Vector3 &position);

	static void InitClass();
	static void FrameReset(){sm_NumSoundsThisFrame = 0;	}

private:

	bool						m_HasHitGround;

	static audSoundSet			sm_SmashableGlassSoundset;

	static u32					sm_NumSoundsThisFrame;
	static u32					sm_MaxSoundsThisFrame;
	static u32					sm_NumPiecesThreshold;
};

class audGlassAudioEntity : public naAudioEntity
{
public:
	AUDIO_ENTITY_NAME(audGlassAudioEntity);

	audGlassAudioEntity();
	~audGlassAudioEntity();

	static void InitClass();

	void Init();
	void Update();

	void Shutdown()
	{
		audEntity::Shutdown();
	}
	
	void PlayShardCollision(CEntity* ground,f32 impactVel,phInst *shard);
	void AddBrokenGlassArea(bgGlassHandle handle,Vec3V_Ref hitPosition,Vec3V_Ref centre,f32 fallenRatio,bool isScriptedArea = false);
	void AddVehBrokenGlassArea(const CVehicle *vehicle,Vec3V_ConstRef centre,f32 radius);
	void DecreaseNumberOfGlassEvents();
	void ClearAll(bool clearScriptedOnly = false);
	static void ScriptHandleBreakableGlassEvent(Vec3V_Ref centrePosition, f32 areaRadius);

	f32 GetGlassiness(Vec3V_In pedPos,bool isMainPlayer,f32 glassinessReduction = 0.0f);
	u32 GetNumberOfGlassEvents() {return m_NumberOfGlassEvents;}

	BANK_ONLY(void LogHandleBreakableGlassEvent(Vec3V_Ref hitPosition,Vec3V_Ref hitMagnitude, bgGlassHandle handle, bool firstContact););
	BANK_ONLY(static void AddWidgets(bkBank &bank););
	BANK_ONLY(static f32 sm_OverridenTestDuration;);
	BANK_ONLY(static bool sm_DrawAreaClaculationInfo;);
	BANK_ONLY(static bool sm_OverrideTestDuration;);
	BANK_ONLY(static bool sm_ShowTestInfo;);
	BANK_ONLY(static bool sm_ShowGlassInfo;);
	BANK_ONLY(static bool sm_ShowGlassShardInfo;);
	BANK_ONLY(static bool sm_OutputLogInfo;);
	BANK_ONLY(static bool sm_ForceClear;);
	static f32 sm_MinTestDist;
	static f32 sm_MaxTestDist;
private:

	void AddBrokenGlassEvent(fwEntity* brokenEntity, Vec3V_Ref hitPosition,Vec3V_Ref hitMagnitude, bgGlassHandle handle);
	void ApproximateArea(Vec3V_Ref hitPosition,Vec3V_Ref hitMagnitude, bgGlassHandle handle);
	void PlayGlassBreak(Vec3V_Ref VhitPos, 	bgGlassHandle handle);
	void CleanUpGlass(bgGlassHandle in_handle);
	bool UpdateExistingAreas(bgGlassHandle handle,const spdSphere &newArea);
	bool UpdateVehExistingAreas(const CVehicle* vehicle,const spdSphere &newArea);

	static void HandleBreakableGlassEvent(bgContactReport* pReport);
	static void HandleCleanUpGlassEvent(bgGlassHandle in_handle);

	atRangeArray<glassShardDefinition,MAX_NUM_GLASS_SHARDS> m_BrokenGlassShards;
	atRangeArray<areaDefinition,MAX_NUM_GLASS_AREAS> m_BrokenGlassAreas;
	atRangeArray<audBreakableGlassEvent,MAX_NUMBER_GLASS_EVENTS> m_GlassEvents;
	
	atRangeArray<vehicleAreaDefinition,MAX_NUM_GLASS_AREAS> m_BrokenVehGlassAreas;


	audSoundSet	m_GlassSoundSet;



	u32 m_NumShardsUpdating; 

	u32 m_NumCurrentAreas;
	u32 m_NumCurrentVehAreas;
	u32 m_NumberOfGlassEvents;
	audCurve m_FallenRatioToAreaRadius;
	audCurve m_DistanceToGlassiness;
	audCurve m_GroundDistSqdToPreDelay;

	static f32 sm_DistanceThreshold;
	//Glass Shards
	static f32 sm_VerticalVelThresh;
	static f32 sm_HorizontalVelThresh;
	static f32 sm_VerticalDotThresh;
	static f32 sm_HorizontalDotThresh;
	static f32 sm_PaneSizeThreshold;
	static f32 sm_CrackRatioThreshold;
	static f32 sm_FallenRatioCrackThreshold;
	static f32 sm_FallenRatioSmallBreakThreshold;

	static u32 sm_TimeThreshToCleanUp;
	static u32 sm_TimeThresholdToTriggerNextBreakSound;
	static u32 sm_TimeSinceLastContact;

	BANK_ONLY(void LogAddBrokenGlassEvent(bgGlassHandle handle););
	BANK_ONLY(void GlassDebugInfo(););
};

extern audGlassAudioEntity g_GlassAudioEntity;

#endif // AUD_GLASSAUDIOENTITY_H

