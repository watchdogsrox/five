// 
// audio/smashableaudioentity.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "audio/glassaudioentity.h"
#include "audio/northaudioengine.h"
#include "scene/world/GameWorld.h"
#include "Peds/ped.h"
#include "profile/profiler.h"
#include "audioengine/soundset.h"

#include "debugaudio.h"

#include "breakableglass/piecegeometry.h"
#include "breakableglass/breakable.h"
#include "physics/levelnew.h"

AUDIO_OPTIMISATIONS()
AUDIO_VEHICLES_OPTIMISATIONS()
//OPTIMISATIONS_OFF()


audCurve audBreakableGlassEvent::sm_FallenRatioToAmplitude;

audSoundSet audSmashableGlassEvent::sm_SmashableGlassSoundset;
u32 audSmashableGlassEvent::sm_MaxSoundsThisFrame = 20;
u32 audSmashableGlassEvent::sm_NumSoundsThisFrame = 0;
u32 audSmashableGlassEvent::sm_NumPiecesThreshold = 50;

#if __BANK
f32 audGlassAudioEntity::sm_OverridenTestDuration = 0.f;
bool audGlassAudioEntity::sm_OverrideTestDuration = false;
bool audGlassAudioEntity::sm_DrawAreaClaculationInfo = false;
bool audGlassAudioEntity::sm_ShowTestInfo = false;
bool audGlassAudioEntity::sm_ShowGlassInfo = false;
bool audGlassAudioEntity::sm_ShowGlassShardInfo = false;
bool audGlassAudioEntity::sm_OutputLogInfo = false;
bool audGlassAudioEntity::sm_ForceClear = false;
#endif
f32 audGlassAudioEntity::sm_DistanceThreshold = 625.f;
f32 audGlassAudioEntity::sm_MinTestDist = 0.5f;
f32 audGlassAudioEntity::sm_MaxTestDist = 100.f;

f32 audGlassAudioEntity::sm_PaneSizeThreshold = 1.8f;
f32 audGlassAudioEntity::sm_CrackRatioThreshold = 1.5f;
f32 audGlassAudioEntity::sm_FallenRatioCrackThreshold = 0.5f;
f32 audGlassAudioEntity::sm_FallenRatioSmallBreakThreshold = 7.f;

u32 audGlassAudioEntity::sm_TimeThresholdToTriggerNextBreakSound = 250;
u32 audGlassAudioEntity::sm_TimeSinceLastContact = 0;
//Glass Shards
f32 audGlassAudioEntity::sm_VerticalVelThresh = 0.8f;
f32 audGlassAudioEntity::sm_HorizontalVelThresh = 0.5f;
f32 audGlassAudioEntity::sm_VerticalDotThresh = 0.4f;
f32 audGlassAudioEntity::sm_HorizontalDotThresh = 0.97f;

u32 audGlassAudioEntity::sm_TimeThreshToCleanUp = 500;

audGlassAudioEntity g_GlassAudioEntity;

PF_PAGE(GlassEventTimers,	"audGlassEventTimers");
PF_GROUP(breakableGlassEvent);
PF_LINK(GlassEventTimers, breakableGlassEvent);
PF_TIMER(BrokenGlassGroundTest,breakableGlassEvent);
PF_TIMER(CalculateTestDuration,breakableGlassEvent);
PF_TIMER(UpdateBrokenGlassGroundTest,breakableGlassEvent);
PF_TIMER(HandleBreakableGlassEvent,breakableGlassEvent);
PF_TIMER(ScriptHandleBreakableGlassEvent,breakableGlassEvent);
PF_TIMER(AddBrokenGlassEvent,breakableGlassEvent);

PF_PAGE(GlassAudioTimers,	"audGlassAudioTimers");
PF_GROUP(GlassAudioEntity);
PF_LINK(GlassAudioTimers, GlassAudioEntity);
PF_TIMER(UpdateGlass,GlassAudioEntity);
PF_TIMER(GetGlassiness,GlassAudioEntity);
PF_TIMER(AddBrokenGlassArea,GlassAudioEntity);
PF_TIMER(UpdateExistingAreas,GlassAudioEntity);
PF_TIMER(CleanUpGlass,GlassAudioEntity);

EXT_PF_GROUP(TriggerSoundsGroup);
PF_COUNTER(audSmashableGlassEvent,TriggerSoundsGroup);

enum { audScriptedGlassHandle = -2};

//===================================================================================================================
bool audCachedBrokenGlassTestResults::NewTestComplete()
{
	for( s32 i = 0; i < GLASS_PARABOL_DEFINITION; i++)
	{
		if((!m_MainLineTest[i].m_hShapeTest.GetResultsWaitingOrReady() || m_MainLineTest[i].m_bHasProbeResults) && !m_MainLineTest[i].m_bHasBeenProcessed) 
		{
			m_MainLineTest[i].m_bHasBeenProcessed = true;
			m_CurrentIndex = i;
			return true;
		}
	}
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
void audCachedBrokenGlassTestResults::ClearAll()
{
	m_HasFoundTheSolution = false;
	m_ResultPosition = Vector3(0.f,0.f,0.f);
	m_ResultNormal = Vector3(0.f,0.f,0.f);
	for( s32 i = 0; i < GLASS_PARABOL_DEFINITION; i++)
	{
		m_MainLineTest[i].Clear();
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audCachedBrokenGlassTestResults::GetAllProbeResults()
{
	for( u32 i = 0; i < GLASS_PARABOL_DEFINITION; i++)
	{
		m_MainLineTest[i].GetProbeResult();
	}
}
//-------------------------------------------------------------------------------------------------------------------
bool audCachedBrokenGlassTestResults::AreAllComplete()
{
	for( u32 i = 0; i < GLASS_PARABOL_DEFINITION; i++)
	{
		if(m_MainLineTest[i].m_hShapeTest.GetResultsWaitingOrReady() && !m_MainLineTest[i].m_bHasProbeResults)
		{
			return false;
		}
	}
	return true;
}
//===================================================================================================================
//
//
//===================================================================================================================
void audBreakableGlassEvent::Init()
{
	//m_GlassEntity = NULL;
	m_HitPosition.ZeroComponents();
	m_HitMagnitude.ZeroComponents();
	m_GlassHandle = bgGlassHandle_Invalid;
	m_FallenRatio = 0.f;
	for(u32 i=0; i< GLASS_PARABOL_DEFINITION; i++)
	{
		m_ParabolPoints[i].ZeroComponents();
	}
	sm_FallenRatioToAmplitude.Init(ATSTRINGHASH("FALLEN_RATIO_TO_AMPLITUDE", 0x1A5718E1));
}
void audBreakableGlassEvent::Clear()
{
	//m_GlassEntity = NULL;
	m_HitPosition.ZeroComponents();
	m_HitMagnitude.ZeroComponents();
	m_GlassHandle = bgGlassHandle_Invalid;
	m_FallenRatio = 0.f;
	for(u32 i=0; i< GLASS_PARABOL_DEFINITION; i++)
	{
		m_ParabolPoints[i].ZeroComponents();
	}
	m_CachedLineTest.GetAllProbeResults();
}
//-------------------------------------------------------------------------------------------------------------------
void audBreakableGlassEvent::SetParabolPoint(u32 index, Vec3V_Ref pPoint)
{
	naAssertf(index < GLASS_PARABOL_DEFINITION,"Bad parabol point index.");
	m_ParabolPoints[index] = pPoint;
}
//-------------------------------------------------------------------------------------------------------------------
Vec3V_Out audBreakableGlassEvent::GetParabolPoint (u32 index)
{
	naAssertf(index < GLASS_PARABOL_DEFINITION,"Bad parabol point index.");
	return m_ParabolPoints[index];
}
//-------------------------------------------------------------------------------------------------------------------
void audBreakableGlassEvent::BrokenGlassGroundTest()
{
	PF_START(BrokenGlassGroundTest);
	//Before submitting the test, clear them in case there is some.
	m_CachedLineTest.ClearAll();
	//Add the test.
	m_ParabolPoints[0] = m_HitPosition;
	Vec3V normHitMag = m_HitMagnitude;
	normHitMag = Normalize(normHitMag);
	bgBreakable &breakable = bgGlassManager::GetGlassBreakable(m_GlassHandle);
	m_FallenRatio = breakable.GetFallenRatio();
	f32 testDuration = BANK_ONLY(audGlassAudioEntity::sm_OverrideTestDuration ? audGlassAudioEntity::sm_OverridenTestDuration :) CalculateTestDuration(); 
	Vec3V nextPoint,magComp,gravityComp;
	for (s32 i = 1; i < GLASS_PARABOL_DEFINITION ; ++i)
	{
		//Modulate using the FALLEN RATIO
		magComp = ( ScalarV(m_FallenRatio) * normHitMag *   ScalarV(testDuration * float(i)));
		gravityComp = (ScalarV(V_HALF) * (Vec3V(0.f,0.f,GRAVITY) * ScalarV(testDuration * float(i) * testDuration * float(i))) );
		nextPoint =  m_ParabolPoints[0] + magComp + gravityComp; 
		m_ParabolPoints[i] = nextPoint;
		m_CachedLineTest.m_MainLineTest[i-1].AddProbe(VEC3V_TO_VECTOR3(m_ParabolPoints[i-1]),VEC3V_TO_VECTOR3(m_ParabolPoints[i]),NULL,ArchetypeFlags::GTA_ALL_MAP_TYPES,false);
	}
	BANK_ONLY(LogBrokenGlassGroundTest(testDuration););
	PF_STOP(BrokenGlassGroundTest);
}
//-------------------------------------------------------------------------------------------------------------------
f32 audBreakableGlassEvent::CalculateTestDuration()
{
	PF_START(CalculateTestDuration);
	// The idea is to, based on the glass position, calculate the ideal test duration. 
	CPed *pPlayer = CGameWorld::FindLocalPlayer();
	if(pPlayer)
	{
		Vector3 groundPos = pPlayer->GetGroundPos();
		f32 upDistanceSqr;
		upDistanceSqr = (m_HitPosition.GetZf() - groundPos.GetZ());
		upDistanceSqr *= upDistanceSqr ;
		f32 testDuration = upDistanceSqr / (2.f* GLASS_PARABOL_DEFINITION);
		testDuration = Max(testDuration,audGlassAudioEntity::sm_MinTestDist);
		testDuration = Min(testDuration,audGlassAudioEntity::sm_MaxTestDist);
#if __BANK
		if(audGlassAudioEntity::sm_ShowTestInfo)
		{
			char txt[256];
			formatf(txt,"[Number of test %d] [length %f] [distToGroundSqr %f] [fallenRatio %f]"
				,GLASS_PARABOL_DEFINITION,testDuration,upDistanceSqr,m_FallenRatio);
			grcDebugDraw::Text(m_HitPosition,Color_white,txt,true,1000);
		}
		if(audGlassAudioEntity::sm_ShowGlassInfo)
		{
			bgBreakable &breakable = bgGlassManager::GetGlassBreakable(m_GlassHandle);
			char txt[256];
			formatf(txt,"[Pane size :%f] [Cracked ratio %f] [fallenRatio :%f]",breakable.GetPaneSize()
				,(breakable.GetCrackedRatio() - breakable.GetLastCrackedRatio()) * 100.f,(breakable.GetFallenRatio() - breakable.GetLastFallenRatio()) * 100.f);
			grcDebugDraw::Text(m_HitPosition,Color_white,txt,true,250);
			naDisplayf("[CR %f] [LCR %f] [FR %f] [LFR %f]",
				breakable.GetCrackedRatio() * 100.f, breakable.GetLastCrackedRatio() * 100.f
				,breakable.GetFallenRatio() * 100.f, breakable.GetLastFallenRatio() * 100.f);
		}
#endif
		return testDuration;
	}
	PF_STOP(CalculateTestDuration);
	return 0.f;
}
//-------------------------------------------------------------------------------------------------------------------
void audBreakableGlassEvent::UpdateBrokenGlassGroundTest()
{
	PF_START(UpdateBrokenGlassGroundTest);
	BANK_ONLY(LogUpdateBrokenGlassGroundTest(););
	//Collect all the results we can from the CWorldProbeAsync
	m_CachedLineTest.GetAllProbeResults();

	//Check if we complete a new test and process it.
	//Otherwise wait another frame until a new one is complete.
	if(m_CachedLineTest.NewTestComplete() && !m_CachedLineTest.m_HasFoundTheSolution)
	{
		// Process it
		m_CachedLineTest.m_HasFoundTheSolution = ProcessBrokenGlassGroundTest();
	}
	else if (m_CachedLineTest.m_HasFoundTheSolution)
	{
		m_HasFoundSolution = true;
		//Store the result position to approximate the next hits.
		Vec3V resultPos = VECTOR3_TO_VEC3V(m_CachedLineTest.m_ResultPosition);
		//Add the area
		g_GlassAudioEntity.AddBrokenGlassArea(m_GlassHandle,m_HitPosition,resultPos,m_FallenRatio);
		//Clean up the event.
		m_CachedLineTest.ClearAll();
		m_GlassHandle = bgGlassHandle_Invalid;
		m_FallenRatio = 0.f;
		g_GlassAudioEntity.DecreaseNumberOfGlassEvents();
	}
	//else if(m_CachedLineTest.AreAllComplete()&& !m_CachedLineTest.m_HasFoundTheSolution)
	//{
	//	IssueAsyncLineTestProbes();
	//}
	PF_STOP(UpdateBrokenGlassGroundTest);
}
//-------------------------------------------------------------------------------------------------------------------
bool audBreakableGlassEvent::ProcessBrokenGlassGroundTest()
{
	s32 currentIndex = m_CachedLineTest.m_CurrentIndex;

	audCachedLos & lineTest = m_CachedLineTest.m_MainLineTest[currentIndex];

	if(lineTest.m_bContainsProbe)
	{
		if(lineTest.m_bHitSomething==true)
		{
			m_CachedLineTest.m_ResultPosition = lineTest.m_vIntersectPos;
			m_CachedLineTest.m_ResultNormal = lineTest.m_vIntersectNormal;
			return true;
		}
	}
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
void audBreakableGlassEvent::IssueAsyncLineTestProbes()
{
	/*m_ParabolPoints[0] = m_ParabolPoints[GLASS_PARABOL_DEFINITION];
	Vec3V normHitMag = m_HitMagnitude;
	normHitMag = Normalize(normHitMag);
	f32 testDuration = BANK_ONLY(audGlassAudioEntity::sm_OverrideTestDuration ? audGlassAudioEntity::sm_OverridenTestDuration :) CalculateTestDuration(); 
	for (s32 i = 1; i < GLASS_PARABOL_DEFINITION ; ++i)
	{
		Vec3V nextPoint,magComp,gravityComp;

		magComp = ( normHitMag *   ScalarV(testDuration * float(i + GLASS_PARABOL_DEFINITION)));
		gravityComp = (ScalarV(V_HALF) * (Vec3V(0.f,0.f,GRAVITY) * ScalarV(testDuration * float(i + GLASS_PARABOL_DEFINITION) * testDuration * float(i + GLASS_PARABOL_DEFINITION))) );
		nextPoint =  m_ParabolPoints[0] + magComp + gravityComp; 
		m_ParabolPoints[i] = nextPoint;
		m_CachedLineTest.m_MainLineTest[i-1].AddProbe(VEC3V_TO_VECTOR3(m_ParabolPoints[i-1]),VEC3V_TO_VECTOR3(m_ParabolPoints[i]),NULL,ArchetypeFlags::GTA_ALL_MAP_TYPES,false);
	}*/
}

//-------------------------------------------------------------------------------------------------------------------
#if __BANK
void audBreakableGlassEvent::LogBrokenGlassGroundTest(f32 testDuration)
{
	if(audGlassAudioEntity::sm_OutputLogInfo)
	{
		naDisplayf(" %d Sent ground test: ", fwTimer::GetFrameCount());
		naDisplayf(" %d [handle %d] [Number of test :%d ][testDuration %f]", fwTimer::GetFrameCount(), m_GlassHandle, GLASS_PARABOL_DEFINITION, testDuration);
	}
}
void audBreakableGlassEvent::LogUpdateBrokenGlassGroundTest()
{
	if(audGlassAudioEntity::sm_OutputLogInfo)
	{
		naDisplayf(" %d Updating ground test: ", fwTimer::GetFrameCount());
		naDisplayf(" %d [handle %d]", fwTimer::GetFrameCount(), m_GlassHandle);
	}
}
#endif
//===================================================================================================================
//
//
//===================================================================================================================
//-------------------------------------------------------------------------------------------------------------------
audSmashableGlassEvent::audSmashableGlassEvent() : m_HasHitGround(false)
{
}
//-------------------------------------------------------------------------------------------------------------------
audSmashableGlassEvent::~audSmashableGlassEvent()
{
}
//-------------------------------------------------------------------------------------------------------------------
void audSmashableGlassEvent::Init()
{
	m_HasHitGround = false;
}
//-------------------------------------------------------------------------------------------------------------------
void audSmashableGlassEvent::InitClass()
{
	sm_SmashableGlassSoundset.Init(ATSTRINGHASH("VEHICLE_BREAKABLE_GLASS_SOUNSET", 0x52E4CA08));
}
//-------------------------------------------------------------------------------------------------------------------
void audSmashableGlassEvent::ReportPolyHitGround(const Vector3 &position,const CVehicle* veh)
{
	// only trigger sound on first poly
	if(!m_HasHitGround && audSmashableGlassEvent::sm_NumSoundsThisFrame < audSmashableGlassEvent::sm_MaxSoundsThisFrame)
	{
		audSoundInitParams initParams;	
		m_HasHitGround = true;
		initParams.Position = position;
		if (veh)
		{
			g_GlassAudioEntity.AddVehBrokenGlassArea(veh,VECTOR3_TO_VEC3V(position),2.f);

			if(!veh->IsUpsideDown())
			{
				g_GlassAudioEntity.CreateDeferredSound(sm_SmashableGlassSoundset.Find(ATSTRINGHASH("GLASS_DEBRIS", 0xCF04822B)), veh, &initParams, true);
			}
			else
			{
				g_GlassAudioEntity.CreateDeferredSound(sm_SmashableGlassSoundset.Find(ATSTRINGHASH("GLASS_DEBRIS_UPSIDEDWON", 0xA40E5018)), veh, &initParams, true);
			}

			PF_INCREMENT(audSmashableGlassEvent);
			sm_NumSoundsThisFrame++;
		}		
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audSmashableGlassEvent::ReportSmash(const CEntity* entity, const Vector3& position, const s32 numInstantPieces, bool forced)
{
	if(CReplayMgr::IsEditorActive() && !CReplayMgr::IsJustPlaying()) // if the editor is active only process sounds if playing B*2659476
	{
		return;
	}

	if(entity && entity->GetType() == ENTITY_TYPE_VEHICLE)
	{
		CVehicle *veh = (CVehicle*)entity;
		if (!veh->GetDriver())		// If this car has a driver triggering the alarm would be silly (sometimes happened in network games)
		{
			if(veh->GetVehicleAudioEntity())
			{
				veh->GetVehicleAudioEntity()->TriggerAlarm();
			}
		}
	}
	if(audSmashableGlassEvent::sm_NumSoundsThisFrame < audSmashableGlassEvent::sm_MaxSoundsThisFrame)
	{
		audSoundInitParams initParams;
		initParams.Position = position;

		if(entity && entity->GetIsTypeVehicle())
		{
			CVehicle* veh = (CVehicle*)entity;
			veh->GetVehicleAudioEntity()->SmashWindow();

			if (veh->ContainsLocalPlayer())
			{
				CPed* localPlayerPed = CGameWorld::FindLocalPlayer();
				CMiniMap::CreateSonarBlipAndReportStealthNoise(localPlayerPed, localPlayerPed->GetTransform().GetPosition(), CMiniMap::sm_Tunables.Sonar.fSoundRange_GlassBreak, HUD_COLOUR_BLUEDARK);
			}
		}

		if (forced)
		{
			g_GlassAudioEntity.CreateDeferredSound(sm_SmashableGlassSoundset.Find(ATSTRINGHASH("GLASS_BREAK_ALL", 0x527A52F)), entity, &initParams, true);
		}
		else if (numInstantPieces < sm_NumPiecesThreshold)
		{
			g_GlassAudioEntity.CreateDeferredSound(sm_SmashableGlassSoundset.Find(ATSTRINGHASH("GLASS_BREAK_SMALL", 0x80D415F)), entity, &initParams, true);
		}
		else
		{
			g_GlassAudioEntity.CreateDeferredSound(sm_SmashableGlassSoundset.Find(ATSTRINGHASH("GLASS_BREAK_LARGE", 0xC142FBDD)), entity, &initParams, true);
		}

		PF_INCREMENT(audSmashableGlassEvent);
		sm_NumSoundsThisFrame++;			
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audSmashableGlassEvent::ReportGlassCrack(const CEntity* entity,const Vector3 &pos)
{
	if(audSmashableGlassEvent::sm_NumSoundsThisFrame < audSmashableGlassEvent::sm_MaxSoundsThisFrame)
	{
		PF_INCREMENT(audSmashableGlassEvent);
		audSoundInitParams initParams;
		initParams.Position = pos;

		g_GlassAudioEntity.CreateDeferredSound(sm_SmashableGlassSoundset.Find(ATSTRINGHASH("GLASS_CRACK", 0xB3C2E362)), entity, &initParams, true);

		PF_INCREMENT(audSmashableGlassEvent);
		sm_NumSoundsThisFrame++;		
	}
}
//===================================================================================================================
//
//
//===================================================================================================================
audGlassAudioEntity::audGlassAudioEntity():
m_NumCurrentAreas (0),
m_NumCurrentVehAreas (0),
m_NumberOfGlassEvents (0)
{
}
//-------------------------------------------------------------------------------------------------------------------
audGlassAudioEntity::~audGlassAudioEntity()
{
}
//-------------------------------------------------------------------------------------------------------------------
void audGlassAudioEntity::Init()
{
	naAudioEntity::Init();
	audEntity::SetName("audGlassAudioEntity");
	for (u32 i = 0; i< MAX_NUM_GLASS_AREAS; i++)
	{
		m_BrokenGlassAreas[i].area.Set(Vec3V(V_ZERO),ScalarV(V_ZERO));
		m_BrokenGlassAreas[i].glassHandle = bgGlassHandle_Invalid;

		m_BrokenVehGlassAreas[i].area.Set(Vec3V(V_ZERO),ScalarV(V_ZERO));
		m_BrokenVehGlassAreas[i].vehicle = NULL;	
	}
	for (u32 i = 0; i< MAX_NUMBER_GLASS_EVENTS; i++)
	{
		m_GlassEvents[i].Init();
	}
	for (u32 i = 0; i < MAX_NUM_GLASS_SHARDS; i++)
	{
		m_BrokenGlassShards[i].ground = NULL;
		m_BrokenGlassShards[i].impactVel = 0.f;
		m_BrokenGlassShards[i].lastUpdateTime = 0;
		m_BrokenGlassShards[i].alreadyPlayedVHit = false;
		m_BrokenGlassShards[i].alreadyPlayedHHit = false;
	}
	m_NumCurrentAreas = 0;
	m_NumCurrentVehAreas = 0;
	m_NumberOfGlassEvents = 0;

	m_FallenRatioToAreaRadius.Init(ATSTRINGHASH("FALLEN_RATIO_TO_AREA_RADIUS", 0x9835DB9B));
	m_DistanceToGlassiness.Init(ATSTRINGHASH("DISTANCE_TO_GLASSINESS", 0x19DAE2FC));
	m_GroundDistSqdToPreDelay.Init(ATSTRINGHASH("GROUND_DIST_SQD_TO_PREDELAY", 0x8C537A94));
	m_GlassSoundSet.Init(ATSTRINGHASH("BREAKABLE_GLASS_SOUNDSET", 0x4FB27367));

}
//-------------------------------------------------------------------------------------------------------------------
void audGlassAudioEntity::InitClass()
{
	bgContactFunctor breakableGlassFunctor;
	breakableGlassFunctor.Bind(&HandleBreakableGlassEvent);
	bgGlassManager::SetContactFunctor(breakableGlassFunctor);
	bgGlassImpactFunc cleanUpGlassFunctor;
	cleanUpGlassFunctor.Bind(&HandleCleanUpGlassEvent);
	bgGlassManager::SetCleanUpGlassAudioFunctor(cleanUpGlassFunctor);
}
//-------------------------------------------------------------------------------------------------------------------
void audGlassAudioEntity::Update()
{
	PF_START(UpdateGlass);
	for (u32 i = 0; i< MAX_NUMBER_GLASS_EVENTS; i++)
	{
		if(m_GlassEvents[i].GetGlassHandle() != bgGlassHandle_Invalid && !m_GlassEvents[i].HasFoundASolution())
		{
			m_GlassEvents[i].UpdateBrokenGlassGroundTest();
		}
	}
	for (u32 i = 0; i < MAX_NUM_GLASS_SHARDS; i++)
	{
		phInst* pInst = PHLEVEL->GetInstance(m_BrokenGlassShards[i].shard);

		if(!pInst || !m_BrokenGlassShards[i].ground)
		{
			// enough time without updating, clean it.
			m_BrokenGlassShards[i].ground = NULL;
			m_BrokenGlassShards[i].impactVel = 0.f;
			m_BrokenGlassShards[i].lastUpdateTime = 0;
			m_BrokenGlassShards[i].alreadyPlayedVHit = false;
			m_BrokenGlassShards[i].alreadyPlayedHHit = false;
			if(m_NumShardsUpdating > 0)
			{
				m_NumShardsUpdating --;
			}
			continue;
		}
		if(m_BrokenGlassShards[i].alreadyPlayedHHit)
		{
			u32 deltaTime = fwTimer::GetTimeInMilliseconds() - m_BrokenGlassShards[i].lastUpdateTime;
			if( deltaTime > sm_TimeThreshToCleanUp )
			{
				// enough time without updating, clean it.
				m_BrokenGlassShards[i].ground = NULL;
				m_BrokenGlassShards[i].impactVel = 0.f;
				m_BrokenGlassShards[i].lastUpdateTime = 0;
				m_BrokenGlassShards[i].alreadyPlayedVHit = false;
				m_BrokenGlassShards[i].alreadyPlayedHHit = false;
				if(m_NumShardsUpdating > 0)
				{
					m_NumShardsUpdating --;
				}
				continue;
			}
		}
		else  
		{
			Matrix34 shardMatrix = MAT34V_TO_MATRIX34(pInst->GetMatrix());
			Matrix34 groundMatrix = MAT34V_TO_MATRIX34(m_BrokenGlassShards[i].ground->GetTransform().GetMatrix());
			f32 dot = shardMatrix.c.Dot(groundMatrix.c);
			dot = fabs(dot);
#if __BANK
			if(sm_ShowGlassShardInfo)
			{
				char txt[256];
				formatf(txt,"[M %u] [Dot %f] [Imp %f] [pInst %u] [time %u]",&m_BrokenGlassShards[i].shard,dot,m_BrokenGlassShards[i].impactVel,m_BrokenGlassShards[i].lastUpdateTime);
				grcDebugDraw::Text(shardMatrix.d,Color_white,txt);
			}
#endif
			if(!m_BrokenGlassShards[i].alreadyPlayedVHit && m_BrokenGlassShards[i].impactVel > sm_VerticalVelThresh 
				&& dot <= sm_VerticalDotThresh)
			{
				audSoundInitParams initParams;
				initParams.Position = shardMatrix.d;
				CreateAndPlaySound(m_GlassSoundSet.Find(ATSTRINGHASH("VERTICAL_SHARD_HIT", 0xDFC3A25C)),&initParams);
				m_BrokenGlassShards[i].alreadyPlayedVHit = true;
#if __BANK
				if(sm_ShowGlassShardInfo)
				{
					grcDebugDraw::Sphere(shardMatrix.d,2.f,Color32(0,255,0,128));
				}
#endif
			}
			if(!m_BrokenGlassShards[i].alreadyPlayedHHit && m_BrokenGlassShards[i].impactVel > sm_HorizontalVelThresh 
				&& dot >= sm_HorizontalDotThresh)
			{
				audSoundInitParams initParams;
				initParams.Position = shardMatrix.d;
				CreateAndPlaySound(m_GlassSoundSet.Find(ATSTRINGHASH("HORIZONTAL_SHARD_HIT", 0xD3A0B9A2)),&initParams);
				m_BrokenGlassShards[i].alreadyPlayedHHit = true;
#if __BANK
				if(sm_ShowGlassShardInfo)
				{
					grcDebugDraw::Sphere(shardMatrix.d,2.f,Color32(0,255,0,128));
				}
#endif
			}
		}
	}
	sm_TimeSinceLastContact += fwTimer::GetTimeStepInMilliseconds();
	PF_STOP(UpdateGlass);
	BANK_ONLY(GlassDebugInfo(););
}
//-------------------------------------------------------------------------------------------------------------------
void audGlassAudioEntity::HandleBreakableGlassEvent(bgContactReport* pReport)
{
	PF_START(HandleBreakableGlassEvent);
	// this function is automatically called with each glass impact.
	Assert(pReport);
	//Get the hit position and magnitude. 
	Vec3V VhitPos,VhitMagV;
	fwEntity* pBrokenEntity = static_cast<fwEntity*>(pReport->pFragInst->GetUserData());
	bgGlassManager::GetLastImpactPositionAndMagnitude(pReport->handle,VhitPos,VhitMagV);
	VhitMagV = Normalize(VhitMagV);
	if(IsFalseAll(IsFinite(VhitMagV * VhitPos)))
	{
		naDebugf1("Got a NAN in GetLastImpactPositionAndMagnitude, don't create the event. ");
		return;
	}
	bool shouldPlay = false;
	if(pReport->pFragInst)
	{
		shouldPlay = audNorthAudioEngine::GetMicrophones().IsVisibleBySniper(pReport->pFragInst->GetXFormSphere());
	}
	if(!shouldPlay)
	{
		f32 distanceToListener = g_AudioEngine.GetEnvironment().ComputeSqdDistanceToPanningListener(VhitPos);
		shouldPlay = distanceToListener <= sm_DistanceThreshold;
	}
	if (shouldPlay)
	{
		// Add log output
		BANK_ONLY(g_GlassAudioEntity.LogHandleBreakableGlassEvent(VhitPos,VhitMagV,pReport->handle,pReport->firstContact););
		//If first contact, add a new glass event
		if(pReport->firstContact)
		{
			g_GlassAudioEntity.AddBrokenGlassEvent(pBrokenEntity,VhitPos,VhitMagV,pReport->handle);
		}
		// otherwise approximate the area.
		else
		{
			g_GlassAudioEntity.ApproximateArea(VhitPos,VhitMagV,pReport->handle);
		}
		if(sm_TimeSinceLastContact > sm_TimeThresholdToTriggerNextBreakSound)
		{
			g_GlassAudioEntity.PlayGlassBreak(VhitPos,pReport->handle);
		}
	}
	PF_STOP(HandleBreakableGlassEvent);
}
//-------------------------------------------------------------------------------------------------------------------
void audGlassAudioEntity::PlayGlassBreak(Vec3V_Ref VhitPos,	bgGlassHandle handle)
{
	bgBreakable &breakable = bgGlassManager::GetGlassBreakable(handle);
	f32 crackedRatio = (breakable.GetCrackedRatio() - breakable.GetLastCrackedRatio()) * 100.f;
	f32 fallenRatio = (breakable.GetFallenRatio() - breakable.GetLastFallenRatio()) * 100.f;
	audSoundInitParams initParams; 
	initParams.Position = VEC3V_TO_VECTOR3(VhitPos);
	if(breakable.GetPaneSize() < sm_PaneSizeThreshold)
	{
		// Small panes
		if (fallenRatio < sm_FallenRatioCrackThreshold && crackedRatio > sm_CrackRatioThreshold)
		{
			CreateAndPlaySound(m_GlassSoundSet.Find(ATSTRINGHASH("SMALL_PANE_CRACK", 0x97B56EC4)),&initParams);
			sm_TimeSinceLastContact = 0;
		}
		else if ( fallenRatio >= sm_FallenRatioCrackThreshold && fallenRatio < sm_FallenRatioSmallBreakThreshold )
		{
			CreateAndPlaySound(m_GlassSoundSet.Find(ATSTRINGHASH("SMALL_PANE_SMALL_BREAK", 0x1C768100)),&initParams);
			sm_TimeSinceLastContact = 0;
		}
		else if ( fallenRatio >= sm_FallenRatioCrackThreshold)
		{
			CreateAndPlaySound(m_GlassSoundSet.Find(ATSTRINGHASH("SMALL_PANE_BIG_BREAK", 0x329EF8E)),&initParams);
			sm_TimeSinceLastContact = 0;
		}
	}
	else 
	{
		//Large panes
		if (fallenRatio < sm_FallenRatioCrackThreshold && crackedRatio > sm_CrackRatioThreshold)
		{
			CreateAndPlaySound(m_GlassSoundSet.Find(ATSTRINGHASH("LARGE_PANE_CRACK", 0x6A5C4FC1)),&initParams);
			sm_TimeSinceLastContact = 0;
		}
		else if ( fallenRatio >= sm_FallenRatioCrackThreshold && fallenRatio < sm_FallenRatioSmallBreakThreshold )
		{
			CreateAndPlaySound(m_GlassSoundSet.Find(ATSTRINGHASH("LARGE_PANE_SMALL_BREAK", 0x2B3AA4C0)),&initParams);
			sm_TimeSinceLastContact = 0;
		}
		else if ( fallenRatio >= sm_FallenRatioCrackThreshold)
		{
			CreateAndPlaySound(m_GlassSoundSet.Find(ATSTRINGHASH("LARGE_PANE_BIG_BREAK", 0xCA4A0783)),&initParams);
			sm_TimeSinceLastContact = 0;
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGlassAudioEntity::ScriptHandleBreakableGlassEvent(Vec3V_Ref centrePosition, f32 areaRadius)
{
	PF_START(ScriptHandleBreakableGlassEvent);
	// Add log output
	g_GlassAudioEntity.AddBrokenGlassArea(audScriptedGlassHandle,centrePosition,centrePosition,areaRadius,true);
	PF_STOP(ScriptHandleBreakableGlassEvent);
}
//-------------------------------------------------------------------------------------------------------------------
void audGlassAudioEntity::HandleCleanUpGlassEvent(bgGlassHandle in_handle)
{
	// this function is automatically called each time we free a glass
	naAssertf(in_handle != bgGlassHandle_Invalid,"Got invalid handle to clean up.");
	g_GlassAudioEntity.CleanUpGlass(in_handle);
}
//-------------------------------------------------------------------------------------------------------------------
void audGlassAudioEntity::CleanUpGlass(bgGlassHandle in_handle)
{
	if(m_NumCurrentAreas > 0)
	{
		PF_START(CleanUpGlass);
		// Vec3V pedPos = CGameWorld::FindLocalPlayer()->GetTransform().GetPosition();
		for(u32 i = 0; i < MAX_NUM_GLASS_AREAS; i++)
		{
			if(m_BrokenGlassAreas[i].glassHandle == in_handle)
			{
				m_BrokenGlassAreas[i].glassHandle = bgGlassHandle_Invalid;
				m_BrokenGlassAreas[i].area.Set(Vec3V(V_ZERO),ScalarV(V_ZERO));
				m_NumCurrentAreas --;
				m_NumCurrentAreas = Max(m_NumCurrentAreas, 0u);
				break;
			}
		}
		PF_STOP(CleanUpGlass);
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGlassAudioEntity::DecreaseNumberOfGlassEvents()
{
	naAssertf(m_NumberOfGlassEvents > 0, "Try to decrease the number of events but the number is 0");
	m_NumberOfGlassEvents --;
#if __BANK 
	if(audGlassAudioEntity::sm_OutputLogInfo)
	{
		naDisplayf(" %d Solution found and processed, decreasing number of glass events: ", fwTimer::GetFrameCount());
		naDisplayf(" %d [num of events %d] ", fwTimer::GetFrameCount(), m_NumberOfGlassEvents);
	}
#endif
}
//-------------------------------------------------------------------------------------------------------------------
void audGlassAudioEntity::AddBrokenGlassEvent(fwEntity* UNUSED_PARAM(brokenEntity),Vec3V_Ref hitPosition,Vec3V_Ref hitMagnitude, bgGlassHandle handle)
{
	PF_START(AddBrokenGlassEvent);
	if(m_NumberOfGlassEvents < MAX_NUMBER_GLASS_EVENTS)
	{
		for (u32 i = 0; i< MAX_NUMBER_GLASS_EVENTS; i++)
		{
			if(m_GlassEvents[i].GetGlassHandle() == bgGlassHandle_Invalid)
			{
				m_GlassEvents[i].SetHitMagnitude( hitMagnitude );
				m_GlassEvents[i].SetHitPosition( hitPosition );
				m_GlassEvents[i].SetGlassHandle( handle );
				m_GlassEvents[i].SetHasFoundSolution( false );
				BANK_ONLY(LogAddBrokenGlassEvent(handle););
				//m_GlassEvents[m_NumberOfGlassEvents].SetGlassEntity( brokenEntity );
				m_GlassEvents[i].BrokenGlassGroundTest();
				m_NumberOfGlassEvents ++;
				break;
			}
		}
	}
	else 
	{
		naWarningf("Glass audio events out of bounds.");
	}
	PF_STOP(AddBrokenGlassEvent);
}
//-------------------------------------------------------------------------------------------------------------------
void audGlassAudioEntity::ApproximateArea(Vec3V_Ref hitPosition,Vec3V_Ref hitMagnitude, bgGlassHandle handle)
{
	//Calculate the position via approximation
	for (u32 i = 0; i< MAX_NUM_GLASS_AREAS; i++)
	{
		if(m_BrokenGlassAreas[i].glassHandle == handle)
		{
			bgBreakable &breakable = bgGlassManager::GetGlassBreakable(m_BrokenGlassAreas[i].glassHandle);
			Vec3V currentHitPosition = hitPosition;
			//Translate the current hit position an amount based on the hit magnitude and the 'parabol scale' (from the first hit) along the hit direction.
			currentHitPosition += hitMagnitude * ScalarV(breakable.GetFallenRatio());
			//Translate the hit position to the result plane.
			currentHitPosition.SetZ(m_BrokenGlassAreas[i].area.GetCenter().GetZ());
			AddBrokenGlassArea(m_BrokenGlassAreas[i].glassHandle,hitPosition,currentHitPosition,breakable.GetFallenRatio());
			CPed *pPlayer = CGameWorld::FindLocalPlayer();
			Vector3 groundPos = pPlayer->GetGroundPos();
			f32 upDistanceSqr;
			upDistanceSqr = (hitPosition.GetZf() - groundPos.GetZ());
			upDistanceSqr *= upDistanceSqr ;
#if __BANK
			if(audGlassAudioEntity::sm_ShowTestInfo)
			{
				char txt[256];
				formatf(txt,"[distToGroundSqr %f] [fallenRatio %f]"	,upDistanceSqr,breakable.GetFallenRatio());

				grcDebugDraw::Text(currentHitPosition,Color_white,txt,true,500);
				grcDebugDraw::Line(currentHitPosition,currentHitPosition + Vec3V(0.5f,0.f,0.f),Color_blue,Color_blue,500);
				grcDebugDraw::Line(currentHitPosition,currentHitPosition + Vec3V(0.f,0.5f,0.f),Color_green,Color_green,500);
				grcDebugDraw::Line(currentHitPosition,currentHitPosition + Vec3V(0.f,0.f,0.5f),Color_blue,Color_blue,500);
			}
			if(audGlassAudioEntity::sm_ShowGlassInfo)
			{
				char txt[256];
				formatf(txt,"[Pane size :%f] [Cracked ratio %f] [fallenRatio :%f]",breakable.GetPaneSize()
					,(breakable.GetCrackedRatio() - breakable.GetLastCrackedRatio()) * 100.f,(breakable.GetFallenRatio() - breakable.GetLastFallenRatio()) * 100.f);
				grcDebugDraw::Text(currentHitPosition,Color_white,txt,true,250);
				naDisplayf("[CR %f] [LCR %f] [FR %f] [LFR %f]",
					breakable.GetCrackedRatio() * 100.f, breakable.GetLastCrackedRatio() * 100.f
					,breakable.GetFallenRatio() * 100.f, breakable.GetLastFallenRatio() * 100.f);
			}
#endif
			break;
		}
	}
#if 0
	//Use asyncprobes for the new impacts in the same glass
	for (u32 i = 0; i< m_NumberOfGlassEvents; i++)
	{
		if(m_GlassEvents[i].GetGlassHandle() == handle)
		{
			m_GlassEvents[i].SetHitMagnitude( hitMagnitude);
			m_GlassEvents[i].SetHitPosition( hitPosition );
			m_GlassEvents[i].SetHasFoundSolution( false );
			m_GlassEvents[i].GetCachedLineTest().ClearAll();
			Vec3V zero = Vec3V(V_ZERO);
			for(u32 j=0; j< GLASS_PARABOL_DEFINITION; j++)
			{
				m_GlassEvents[i].SetParabolPoint(j, zero);
			}
			m_GlassEvents[i].BrokenGlassGroundTest();
		}
	}
#endif
}
//-------------------------------------------------------------------------------------------------------------------
f32 audGlassAudioEntity::GetGlassiness(Vec3V_In pedPos,bool isMainPlayer,f32 glassinessReduction)
{
	PF_START(GetGlassiness);
	// If we are getting the glassiness for the main player, check all the possible results, otherwise stop the search after the first hit.
	f32 glassiness = 0.f;
	for(u32 i = 0; i < MAX_NUM_GLASS_AREAS; i++)
	{
		if(m_BrokenGlassAreas[i].area.ContainsPoint(pedPos))
		{
			const Vec3V diff = m_BrokenGlassAreas[i].area.GetCenter() - pedPos;
			const ScalarV distance = Mag(diff);
			if(isMainPlayer)
			{
				glassiness += m_DistanceToGlassiness.CalculateRescaledValue(0.f,1.f,0.f,m_BrokenGlassAreas[i].area.GetRadiusf(),distance.Getf()) * m_BrokenGlassAreas[i].remainingGlass;
				m_BrokenGlassAreas[i].remainingGlass = Clamp(m_BrokenGlassAreas[i].remainingGlass - glassinessReduction, 0.0f, 1.0f);
			}
			else
			{
				glassiness = m_DistanceToGlassiness.CalculateRescaledValue(0.f,1.f,0.f,m_BrokenGlassAreas[i].area.GetRadiusf(),distance.Getf()) * m_BrokenGlassAreas[i].remainingGlass;
				m_BrokenGlassAreas[i].remainingGlass = Clamp(m_BrokenGlassAreas[i].remainingGlass - glassinessReduction, 0.0f, 1.0f);
				return glassiness;
			}
		}
		if(m_BrokenVehGlassAreas[i].area.ContainsPoint(pedPos))
		{
			const Vec3V diff = m_BrokenVehGlassAreas[i].area.GetCenter() - pedPos;
			const ScalarV distance = Mag(diff);
			if(isMainPlayer)
			{
				glassiness += m_DistanceToGlassiness.CalculateRescaledValue(0.f,1.f,0.f,m_BrokenVehGlassAreas[i].area.GetRadiusf(),distance.Getf()) * m_BrokenVehGlassAreas[i].remainingGlass;
				m_BrokenVehGlassAreas[i].remainingGlass = Clamp(m_BrokenVehGlassAreas[i].remainingGlass - glassinessReduction, 0.0f, 1.0f);
			}
			else
			{
				glassiness = m_DistanceToGlassiness.CalculateRescaledValue(0.f,1.f,0.f,m_BrokenVehGlassAreas[i].area.GetRadiusf(),distance.Getf()) * m_BrokenVehGlassAreas[i].remainingGlass;
				m_BrokenVehGlassAreas[i].remainingGlass = Clamp(m_BrokenVehGlassAreas[i].remainingGlass - glassinessReduction, 0.0f, 1.0f);
				return glassiness;
			}
		}
	}
	PF_STOP(GetGlassiness);
	return glassiness;
}
//-------------------------------------------------------------------------------------------------------------------
void audGlassAudioEntity::PlayShardCollision(CEntity* ground,f32 impactVel,phInst *shard)
{
	bool shouldPlay = naMicrophones::IsSniping();
	if(!shouldPlay)
	{
		Matrix34 shardMatrix = MAT34V_TO_MATRIX34(shard->GetMatrix());
		f32 distanceToListener = g_AudioEngine.GetEnvironment().ComputeSqdDistanceToPanningListener(shardMatrix.d);
		shouldPlay = distanceToListener <= sm_DistanceThreshold;
	}
	if (shouldPlay)
	{
		s32 freeIndex = -1;
		phHandle handle = PHLEVEL->GetHandle(shard); 
		// Add the current manifold if it's not already added.
		for (s32 i = 0; i < MAX_NUM_GLASS_SHARDS; i++)
		{
			if(m_BrokenGlassShards[i].shard == handle)
			{
				m_BrokenGlassShards[i].ground = ground;
				m_BrokenGlassShards[i].shard = handle;
				m_BrokenGlassShards[i].impactVel = impactVel;
				m_BrokenGlassShards[i].lastUpdateTime = fwTimer::GetTimeInMilliseconds();
				return;
			}
			else if (!m_BrokenGlassShards[i].ground)
			{
				freeIndex = i;
			}
		}
		if(freeIndex != -1 && m_NumShardsUpdating < MAX_NUM_GLASS_SHARDS)
		{
			m_BrokenGlassShards[freeIndex].ground = ground;
			m_BrokenGlassShards[freeIndex].shard = handle;
			m_BrokenGlassShards[freeIndex].impactVel = impactVel;
			m_BrokenGlassShards[freeIndex].alreadyPlayedVHit = false;
			m_BrokenGlassShards[freeIndex].alreadyPlayedHHit = false;
			m_BrokenGlassShards[freeIndex].lastUpdateTime = fwTimer::GetTimeInMilliseconds();
			if(m_NumShardsUpdating < MAX_NUM_GLASS_SHARDS -1)	
			{
				m_NumShardsUpdating++;
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGlassAudioEntity::AddBrokenGlassArea(bgGlassHandle handle,Vec3V_Ref hitPosition,Vec3V_Ref centre,f32 fallenRatio,bool isScriptedArea)
{
	PF_START(AddBrokenGlassArea);
	f32 radius = m_FallenRatioToAreaRadius.CalculateValue(fallenRatio);
	if( isScriptedArea )
	{
		radius = fallenRatio;
	}
	else if (sm_TimeSinceLastContact > sm_TimeThresholdToTriggerNextBreakSound)
	{
		f32 upDistanceSqr;
		upDistanceSqr = (hitPosition.GetZf() - centre.GetZf());
		upDistanceSqr *= upDistanceSqr ;
		// play glass debris sound
		bgBreakable &breakable = bgGlassManager::GetGlassBreakable(handle);
		if(breakable.IsValid())
		{
			f32 debrisAmmount = (breakable.GetFallenRatio() - breakable.GetLastFallenRatio()) * 100.f;
			audSoundInitParams initParams; 
			initParams.Position = VEC3V_TO_VECTOR3(centre);
			initParams.Predelay = (s32)m_GroundDistSqdToPreDelay.CalculateValue(upDistanceSqr);
			if(breakable.GetPaneSize() < sm_PaneSizeThreshold)
			{
				if ( debrisAmmount >= sm_FallenRatioCrackThreshold && debrisAmmount < sm_FallenRatioSmallBreakThreshold )
				{
					CreateAndPlaySound(m_GlassSoundSet.Find(ATSTRINGHASH("SMALL_PANE_SMALL_DEBRIS", 0x36DE953B)),&initParams);
				}
				else if ( debrisAmmount >= sm_FallenRatioCrackThreshold)
				{
					CreateAndPlaySound(m_GlassSoundSet.Find(ATSTRINGHASH("SMALL_PANE_BIG_DEBRIS", 0xB04C1FA9)),&initParams);
				}
			}
			else
			{
				if ( debrisAmmount >= sm_FallenRatioCrackThreshold && debrisAmmount < sm_FallenRatioSmallBreakThreshold )
				{
					CreateAndPlaySound(m_GlassSoundSet.Find(ATSTRINGHASH("LARGE_PANE_SMALL_DEBRIS", 0x4AFA0EAE)),&initParams);
				}
				else if ( debrisAmmount >= sm_FallenRatioCrackThreshold)
				{
					CreateAndPlaySound(m_GlassSoundSet.Find(ATSTRINGHASH("LARGE_PANE_BIG_DEBRIS", 0xE7E9A386)),&initParams);
				}
			}
		}
	}
	spdSphere newArea = spdSphere(centre,ScalarV(radius));
	if(!UpdateExistingAreas(handle,newArea))
	{
		if(m_NumCurrentAreas < MAX_NUM_GLASS_AREAS)
		{
			for(u32 i = 0; i < MAX_NUM_GLASS_AREAS; i++)
			{
				if(m_BrokenGlassAreas[i].glassHandle == bgGlassHandle_Invalid)
				{
					m_BrokenGlassAreas[i].remainingGlass = 1.0f;
					m_BrokenGlassAreas[i].glassHandle = handle;
					m_BrokenGlassAreas[i].area = newArea;
					m_NumCurrentAreas ++;
					break;
				}
			}
		}
		else 
		{
			//Replace the furthest away one. 
			u32 furthestAwayIndex = 0;
			f32 dist = 0;
			Vec3V pedPos = CGameWorld::FindLocalPlayer()->GetTransform().GetPosition();
			for(u32 i = 0; i < MAX_NUM_GLASS_AREAS; i++)
			{
				// No glass left? Good to re-use
				if(m_BrokenGlassAreas[i].remainingGlass == 0.0f)
				{
					furthestAwayIndex = i;
					break;
				}

				Vec3V area = m_BrokenGlassAreas[i].area.GetCenter();
				const ScalarV  distance = Mag(area - pedPos);
				f32 distanceSqr = distance.Getf();
				distanceSqr *= distanceSqr;
				if(distanceSqr >= dist)
				{
					furthestAwayIndex = i;
					dist = distanceSqr;
				}
			}
			naAssertf (furthestAwayIndex < MAX_NUM_GLASS_AREAS, "Wrong area index.");
			m_BrokenGlassAreas[furthestAwayIndex].glassHandle = handle;
			m_BrokenGlassAreas[furthestAwayIndex].area = newArea;
		}
	}
	PF_STOP(AddBrokenGlassArea);
}
//-------------------------------------------------------------------------------------------------------------------
void audGlassAudioEntity::AddVehBrokenGlassArea(const CVehicle *vehicle,Vec3V_ConstRef centre,f32 radius)
{
	spdSphere newArea = spdSphere(centre,ScalarV(radius));
	if(!UpdateVehExistingAreas(vehicle,newArea))
	{
		if(m_NumCurrentVehAreas < MAX_NUM_GLASS_AREAS)
		{
			for(u32 i = 0; i < MAX_NUM_GLASS_AREAS; i++)
			{
				if(m_BrokenVehGlassAreas[i].vehicle == NULL)
				{
					m_BrokenVehGlassAreas[i].remainingGlass = 1.0f;
					m_BrokenVehGlassAreas[i].vehicle = vehicle;
					m_BrokenVehGlassAreas[i].area = newArea;
					m_NumCurrentVehAreas ++;
					break;
				}
			}
		}
		else 
		{
			//Replace the furthest away one. 
			u32 furthestAwayIndex = 0;
			f32 dist = 0;
			Vec3V pedPos = CGameWorld::FindLocalPlayer()->GetTransform().GetPosition();
			for(u32 i = 0; i < MAX_NUM_GLASS_AREAS; i++)
			{
				// No glass left? Good to re-use
				if(m_BrokenVehGlassAreas[i].remainingGlass == 0.0f)
				{
					furthestAwayIndex = i;
					break;
				}

				Vec3V area = m_BrokenVehGlassAreas[i].area.GetCenter();
				const ScalarV  distance = Mag(area - pedPos);
				f32 distanceSqr = distance.Getf();
				distanceSqr *= distanceSqr;
				if(distanceSqr >= dist)
				{
					furthestAwayIndex = i;
					dist = distanceSqr;
				}
			}
			naAssertf (furthestAwayIndex < MAX_NUM_GLASS_AREAS, "Wrong area index.");
			m_BrokenVehGlassAreas[furthestAwayIndex].remainingGlass = 1.0f;
			m_BrokenVehGlassAreas[furthestAwayIndex].vehicle = vehicle;
			m_BrokenVehGlassAreas[furthestAwayIndex].area = newArea;
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
bool audGlassAudioEntity::UpdateExistingAreas(bgGlassHandle handle,const spdSphere &newArea)
{
	PF_START(UpdateExistingAreas);
	if(m_NumCurrentAreas == 0)
		return false;
	for(u32 i = 0; i < MAX_NUM_GLASS_AREAS; i++)
	{
		if(m_BrokenGlassAreas[i].glassHandle == handle)
		{
			if(!m_BrokenGlassAreas[i].area.IntersectsSphere(newArea) && !m_BrokenGlassAreas[i].area.ContainsSphere(newArea))
			{
				continue;
			}
			else
			{
				// Calculate new area.
				m_BrokenGlassAreas[i].area.GrowSphere(newArea);
				m_BrokenGlassAreas[i].remainingGlass = 1.0f;
				return true;
			}
		}
	}
	PF_STOP(UpdateExistingAreas);
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
bool audGlassAudioEntity::UpdateVehExistingAreas(const CVehicle *vehicle,const spdSphere &newArea)
{
	if(m_NumCurrentVehAreas == 0)
		return false;
	for(u32 i = 0; i < MAX_NUM_GLASS_AREAS; i++)
	{
		if(m_BrokenVehGlassAreas[i].vehicle == vehicle)
		{
			if(!m_BrokenVehGlassAreas[i].area.IntersectsSphere(newArea) && !m_BrokenVehGlassAreas[i].area.ContainsSphere(newArea))
			{
				continue;
			}
			else
			{
				// Calculate new area.
				m_BrokenVehGlassAreas[i].remainingGlass = 1.0f;
				m_BrokenVehGlassAreas[i].area.GrowSphere(newArea);
				return true;
			}
		}
	}
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
void audGlassAudioEntity::ClearAll(bool clearScriptedOnly)
{
	//Clear glass areas
	for(u32 i = 0; i < MAX_NUM_GLASS_AREAS; i++)
	{
		if(clearScriptedOnly && m_BrokenGlassAreas[i].glassHandle == audScriptedGlassHandle)
		{
			// if we want to clear the scripted areas only, just clear the ones with the invalid glass handle.
			m_BrokenGlassAreas[i].glassHandle = bgGlassHandle_Invalid;
			m_BrokenGlassAreas[i].area.Set(Vec3V(V_ZERO),ScalarV(V_ZERO));
			m_NumCurrentAreas --;
			m_NumCurrentAreas = (u32)Max((f32)m_NumCurrentAreas, 0.f);
		}
		else if (!clearScriptedOnly)
		{
			m_BrokenGlassAreas[i].glassHandle = bgGlassHandle_Invalid;
			m_BrokenGlassAreas[i].area.Set(Vec3V(V_ZERO),ScalarV(V_ZERO));
			m_BrokenVehGlassAreas[i].vehicle = NULL;
			m_BrokenVehGlassAreas[i].area.Set(Vec3V(V_ZERO),ScalarV(V_ZERO));
		}
	}
	//Clear glass events
	for (u32 i = 0; i< MAX_NUMBER_GLASS_EVENTS; i++)
	{
		if(clearScriptedOnly && m_BrokenGlassAreas[i].glassHandle == audScriptedGlassHandle)
		{
			m_GlassEvents[i].Clear();
			m_NumberOfGlassEvents --;
			m_NumberOfGlassEvents = (u32)Max((f32)m_NumberOfGlassEvents, 0.f);
		}
		else if (!clearScriptedOnly)
		{
			m_GlassEvents[i].Clear();
		}
	}
	if(!clearScriptedOnly)
	{
		m_NumCurrentAreas = 0;
		m_NumCurrentVehAreas = 0;
		m_NumberOfGlassEvents = 0;
	}

}
//-------------------------------------------------------------------------------------------------------------------
#if __BANK
void audGlassAudioEntity::LogHandleBreakableGlassEvent(Vec3V_Ref hitPosition,Vec3V_Ref hitMagnitude, bgGlassHandle handle, bool firstContact)
{
	if(sm_OutputLogInfo)
	{
		naDisplayf(" %d Handling glass event: ", fwTimer::GetFrameCount());
		naDisplayf(" %d [handle %d] %s [hit pos <%f,%f,%f>] [hit magnitude <%f,%f,%f>] "
			, fwTimer::GetFrameCount(),handle,(firstContact ? "[First contact]" : "")
			, hitPosition.GetXf(),hitPosition.GetYf(),hitPosition.GetZf(), hitMagnitude.GetXf(),hitMagnitude.GetYf(),hitMagnitude.GetZf());
	}
}
void audGlassAudioEntity::LogAddBrokenGlassEvent(bgGlassHandle handle)
{
	if(sm_OutputLogInfo)
	{
		naDisplayf(" %d Added glass event: ", fwTimer::GetFrameCount());
		naDisplayf(" %d [handle %d] [numEvents %d]", fwTimer::GetFrameCount(), handle, (m_NumberOfGlassEvents +1));
	}
}
void audGlassAudioEntity::GlassDebugInfo()
{
	if(sm_DrawAreaClaculationInfo)
	{
		char txt[64];
		formatf(txt, "Number of glass areas :%d", m_NumCurrentAreas);
		grcDebugDraw::AddDebugOutput(Color_white,txt);
		formatf(txt, "Number of glass events :%d", m_NumberOfGlassEvents);
		grcDebugDraw::AddDebugOutput(Color_white,txt);
		formatf(txt, "Number of vehicle glass areas :%d", m_NumCurrentVehAreas);
		grcDebugDraw::AddDebugOutput(Color_white,txt);
		for (u32 i = 0; i< MAX_NUMBER_GLASS_EVENTS; i++)
		{
			if(m_GlassEvents[i].GetGlassHandle() != bgGlassHandle_Invalid)
			{
				for (s32 j = 1; j < GLASS_PARABOL_DEFINITION ; j++)
				{
					grcDebugDraw::Line(m_GlassEvents[i].GetParabolPoint(j-1),m_GlassEvents[i].GetParabolPoint(j), Color_red,1);
				}
			}
			grcDebugDraw::Line(m_GlassEvents[i].GetHitPosition(),m_GlassEvents[i].GetHitPosition() + m_GlassEvents[i].GetHitMagnitude()  ,Color_orange,Color_orange,1);
			grcDebugDraw::Line(m_GlassEvents[i].GetHitPosition(),m_GlassEvents[i].GetHitPosition() - m_GlassEvents[i].GetHitMagnitude() ,Color_red,Color_red,1);

		}
		for(u32 i = 0; i < MAX_NUM_GLASS_AREAS; i++)
		{
			grcDebugDraw::Line(m_BrokenGlassAreas[i].area.GetCenter(),m_BrokenGlassAreas[i].area.GetCenter() + Vec3V(0.5f,0.f,0.f),Color_red,Color_red,1);
			grcDebugDraw::Line(m_BrokenGlassAreas[i].area.GetCenter(),m_BrokenGlassAreas[i].area.GetCenter() + Vec3V(0.f,0.5f,0.f),Color_green,Color_green,1);
			grcDebugDraw::Line(m_BrokenGlassAreas[i].area.GetCenter(),m_BrokenGlassAreas[i].area.GetCenter() + Vec3V(0.f,0.f,0.5f),Color_blue,Color_blue,1);
			grcDebugDraw::Circle(m_BrokenGlassAreas[i].area.GetCenter(),m_BrokenGlassAreas[i].area.GetRadiusf(),Color_red,Vec3V(1.f,0.f,0.f),Vec3V(0.f,1.f,0.f),false,false,1);
			grcDebugDraw::Line(m_BrokenVehGlassAreas[i].area.GetCenter(),m_BrokenVehGlassAreas[i].area.GetCenter() + Vec3V(0.5f,0.f,0.f),Color_red,Color_red,1);
			grcDebugDraw::Line(m_BrokenVehGlassAreas[i].area.GetCenter(),m_BrokenVehGlassAreas[i].area.GetCenter() + Vec3V(0.f,0.5f,0.f),Color_green,Color_green,1);
			grcDebugDraw::Line(m_BrokenVehGlassAreas[i].area.GetCenter(),m_BrokenVehGlassAreas[i].area.GetCenter() + Vec3V(0.f,0.f,0.5f),Color_blue,Color_blue,1);
			grcDebugDraw::Circle(m_BrokenVehGlassAreas[i].area.GetCenter(),m_BrokenVehGlassAreas[i].area.GetRadiusf(),Color_red,Vec3V(1.f,0.f,0.f),Vec3V(0.f,1.f,0.f),false,false,1);
			
			if(m_BrokenGlassAreas[i].remainingGlass > 0)
			{
				formatf(txt, "Glass: %.02f", m_BrokenGlassAreas[i].remainingGlass);
				grcDebugDraw::Text(m_BrokenGlassAreas[i].area.GetCenter(), Color_white,0,0,txt);
			}			

			if(m_BrokenVehGlassAreas[i].remainingGlass > 0)
			{
				formatf(txt, "Glass: %.02f", m_BrokenVehGlassAreas[i].remainingGlass);
				grcDebugDraw::Text(m_BrokenVehGlassAreas[i].area.GetCenter(), Color_white,0,0,txt);
			}
		}
	}
	if(sm_ShowGlassShardInfo)
	{
		char txt [64];
		formatf(txt,"Shards updating : %d",m_NumShardsUpdating);
		grcDebugDraw::AddDebugOutput(Color_red,txt);
	}
	if(sm_ForceClear)
	{
		ClearAll();
		sm_ForceClear = false;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGlassAudioEntity::AddWidgets(bkBank &bank)
{
	bank.PushGroup("audGlassAudioEntity",false);
		bank.AddSlider("Distance threshold to trigger glass events",&sm_DistanceThreshold,0.f,10000.f,1.f);
		bank.AddToggle("Show glass hit Info", &sm_ShowGlassInfo);
		bank.AddToggle("Show glass shard Info", &sm_ShowGlassShardInfo);
		bank.AddSlider("Time Threshold To Trigger Next Break Sound",&sm_TimeThresholdToTriggerNextBreakSound,0,10000,100);
		bank.AddSlider("sm_PaneSizeThreshold",&sm_PaneSizeThreshold,0.f,100.f,0.1f);
		bank.AddSlider("sm_CrackRatioThreshold",&sm_CrackRatioThreshold,0.f,100.f,0.1f);
		bank.AddSlider("sm_FallenRatioCrackThreshold",&sm_FallenRatioCrackThreshold,0.f,100.f,0.1f);
		bank.AddSlider("sm_FallenRatioSmallBreakThreshold",&sm_FallenRatioSmallBreakThreshold,0.f,100.f,0.1f);
		bank.PushGroup("Shards of glass");
		bank.AddSlider("sm_VerticalVelThresh",&sm_VerticalVelThresh,0.f,10000.f,1.f);
		bank.AddSlider("sm_HorizontalVelThresh",&sm_HorizontalVelThresh,0.f,10000.f,1.f);
		bank.AddSlider("sm_VerticalDotThresh",&sm_VerticalDotThresh,0.f,1.f,0.01f);
		bank.AddSlider("sm_HorizontalDotThresh",&sm_HorizontalDotThresh,0.f,1.f,0.01f);
		bank.AddSlider("sm_TimeThreshToCleanUp",&sm_TimeThreshToCleanUp,0,10000,100);
	bank.PopGroup();
	bank.PushGroup("Broken areas calculation",false);
		bank.AddToggle("Draw broken glass area calculation info", &sm_DrawAreaClaculationInfo);
		bank.AddSlider("Test minimum dist",&sm_MinTestDist,0.f,100.f,0.01f);
		bank.AddSlider("Test maximum dist",&sm_MaxTestDist,0.f,100.f,0.01f);
		bank.AddToggle("Test Info", &sm_ShowTestInfo);
		bank.AddToggle("Override Test Duration", &sm_OverrideTestDuration);
		bank.AddSlider("Overridden Test Duration",&sm_OverridenTestDuration,0.f,100.f,0.01f);
		bank.AddToggle("Output log Info", &sm_OutputLogInfo);
		bank.AddToggle("Clear all results", &sm_ForceClear);
	bank.PopGroup();
	bank.PopGroup();
}
#endif
