#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "debugaudio.h"
#include "grcore/debugdraw.h"


#include "audio/collisionaudioentity.h"
#include "audio/scriptaudioentity.h"
#include "ai/EntityScanner.h"
#include "camera/CamInterface.h"
#include "fragment/instance.h"
#include "fragment/type.h"
#include "gunfightconductorfakescenes.h"
#include "northaudioengine.h"
#include "Objects/Door.h"
#include "Peds/PedIntelligence.h"
#include "physics/physics.h"
#include "physics/gtaInst.h"
#include "scene/world/GameWorld.h"
#include "superconductor.h"
#include "vehicles/vehicle.h"
#include "vehicles/vehicleDamage.h"
#include "vfx/Systems/VfxVehicle.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "system/controlMgr.h"

AUDIO_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

#if __BANK
bool audGunFightConductorFakeScenes::sm_DrawMaterialBehindTest = false;
bool audGunFightConductorFakeScenes::sm_FakeBulletImpacts = false;
bool audGunFightConductorFakeScenes::sm_DrawNearByDetectionSphere = false;
bool audGunFightConductorFakeScenes::sm_DontCheckOffScreen = false;
bool audGunFightConductorFakeScenes::sm_DisplayDynamicSoundsPlayed = false;
bool audGunFightConductorFakeScenes::sm_ForceOpenSpaceScene = false;
bool audGunFightConductorFakeScenes::sm_DrawResultPosition = false;
bool audGunFightConductorFakeScenes::sm_ShowFakeRiccoPos = false;
bool audGunFightConductorFakeScenes::sm_ShowPostShootOutPos = false;
s32  audGunFightConductorFakeScenes::sm_FakeBulletImpactType = audGunFightConductorFakeScenes::Single_Impact;
const char*  audGunFightConductorFakeScenes::sm_FakeBulletImpactTypes[Max_BulletImpactTypes] = {"Not selected","Single","Burst","Hard","Bunch of impacts",};
#endif // __BANK

f32 audGunFightConductorFakeScenes::sm_BuildDistThresholdToPlayPostShootOut = 20.f;
f32 audGunFightConductorFakeScenes::sm_VehSqdDistanceToPlayFarDebris = 100.f;
f32 audGunFightConductorFakeScenes::sm_BuildingSqdDistanceToPlayFarDebris = 100.f;
f32 audGunFightConductorFakeScenes::sm_MinDistanceToFakeRicochets = 700.f; //actually min dist squared
f32 audGunFightConductorFakeScenes::sm_OpenSpaceConfidenceThreshold = 0.7f;
f32 audGunFightConductorFakeScenes::sm_LifeThresholdToFakeRunAwayScene = 0.6f;
f32	audGunFightConductorFakeScenes::sm_DtToPlayFakedBulletImp = -1.f;
f32	audGunFightConductorFakeScenes::sm_DtToPlayFakedBulletImpRunningAway = -1.f;
f32	audGunFightConductorFakeScenes::sm_DtToPlayFakedBulletImpOpenSpace = -1.f;
f32	audGunFightConductorFakeScenes::sm_RSphereNearByEntities = 10.f;
f32	audGunFightConductorFakeScenes::sm_MaterialTestLenght = 2.f;
f32 audGunFightConductorFakeScenes::sm_TimeSinceLastFakedBulletImpRunningAway = 0.f; 
u32 audGunFightConductorFakeScenes::sm_TimeSinceLastSirenSmashed = 0; // ms
u32 audGunFightConductorFakeScenes::sm_TimeToSmashSirens = 20000; // ms
u32 audGunFightConductorFakeScenes::sm_MaxOffScreenTimeOut = 5000; // ms
u32 audGunFightConductorFakeScenes::sm_MaxTimeToFakeRicochets = 100; // ms
u32 audGunFightConductorFakeScenes::sm_MinTimeFiringToFakeRicochets = 500; // ms
u32 audGunFightConductorFakeScenes::sm_MinTimeNotFiringToFakeNearLightVehDebris = 350; // ms
u32 audGunFightConductorFakeScenes::sm_MinTimeNotFiringToFakeNearVehDebris = 350; // ms
u32 audGunFightConductorFakeScenes::sm_MinTimeNotFiringToFakeFarVehDebris = 700; // ms

u32 audGunFightConductorFakeScenes::sm_MinTimeNotFiringToFakeNearBuildingDebris = 2000; // ms
u32 audGunFightConductorFakeScenes::sm_MinTimeNotFiringToFakeFarBuildingDebris = 2000; // ms
u32 audGunFightConductorFakeScenes::sm_TimeToResetBulletCount = 4000;
u32 audGunFightConductorFakeScenes::sm_BulletHitsToTriggerVehicleDebris = 25;
u32 audGunFightConductorFakeScenes::sm_BulletHitsToTriggerBuildingDebris = 10;

s32 audGunFightConductorFakeScenes::sm_FakedEntityToPlayProbThreshold = 70; 
s32 audGunFightConductorFakeScenes::sm_CarPriorityOverObjects = 2; 
bool audGunFightConductorFakeScenes::sm_AllowRunningAwayScene = true;
bool audGunFightConductorFakeScenes::sm_ForceRunAwayScene = true;
bool audGunFightConductorFakeScenes::sm_AllowOpenSpaceWhenRunningAway = true;
//-------------------------------------------------------------------------------------------------------------------
//														INIT
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::Init()
{
	naAudioEntity::Init();
	audEntity::SetName("audGunFightConductorFakeScenes");

	m_OffScreenTimeOut = 0;

	m_TimeSinceLastCandidateHit = 0;
	m_CandidateHitPosition.Zero();
	m_PostShootOutVehCandidateHitPosition.Zero();
	m_PostShootOutBuildingCandidateHitPosition.Zero();
	m_Candidate = NULL;
	m_PostShootOutVehCandidate = NULL;
	m_CurrentFakedCollisionMaterial = NULL;
	m_ObjectToFake = NULL;
	m_VehicleToFake = NULL;
	m_FakeBulletImpactSound = NULL;
	m_FakeRicochetsSound = NULL;
	m_PostShootOutVehDebrisSound = NULL;
	m_PostShootOutBuildingDebrisSound = NULL;

	m_typeOfFakedBulletImpacts = Invalid_Impact;
	m_PedsInCombatToFakedBulletImpactsProbability.Init(ATSTRINGHASH("NUMBER_OF_PEDS_TO_FAKED_BULLET_IMPACTS_PROB", 0x56FD90B5));
	m_DistanceToFakeRicochetsPredelay.Init(ATSTRINGHASH("SQD_DIST_TO_PLAYER_FAKE_RICCO_PREDELAY", 0xCDEF0DF7));
	m_HasToTriggerGoingIntoCover = false;
	m_FakeObjectWhenOffScreen = false;
	m_HasToPlayFakeRicochets = false;
	m_HasToPlayPostShootOutVehDebris = false;
	m_HasToPlayPostShootOutBuildingDebris = false;
	m_WasSprinting = false;
	m_StateProcessed = false;
	SetState(State_Idle);
	m_LastState = State_Idle;
	m_PostShootOutSounds.Init(ATSTRINGHASH("POST_SHOOTOUT_DEBRIS_SOUNDSET", 0x967CE077));
	sysMemSet(&m_PostShootOutBuilding,0,sizeof(m_PostShootOutBuilding));
	m_PostShootOutBuildingCandidate.building = NULL;
	m_PostShootOutBuildingCandidate.bulletHitCount = 0;
	m_PostShootOutBuildingCandidate.timeSinceLastBulletHit = 0;
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::Reset()
{
	m_CurrentFakedCollisionMaterial = NULL;
	m_ObjectToFake = NULL;
	m_VehicleToFake = NULL;
	m_TimeSinceLastCandidateHit = 0;
	m_CandidateHitPosition.Zero();
	m_PostShootOutVehCandidateHitPosition.Zero();
	m_PostShootOutBuildingCandidateHitPosition.Zero();
	m_Candidate = NULL;
	m_PostShootOutVehCandidate = NULL;
	m_typeOfFakedBulletImpacts = Invalid_Impact;
	m_PostShootOutVehDebrisSound = NULL;
	m_PostShootOutBuildingDebrisSound = NULL;
	m_StateProcessed = false;
	SetState(State_Idle);
	m_LastState = State_Idle;
	sysMemSet(&m_PostShootOutBuilding,0,sizeof(m_PostShootOutBuilding));
	m_PostShootOutBuildingCandidate.building = NULL;
	m_PostShootOutBuildingCandidate.bulletHitCount = 0;
	m_PostShootOutBuildingCandidate.timeSinceLastBulletHit = 0;
}
//-------------------------------------------------------------------------------------------------------------------
//									GUNFIGHT CONDUCTOR FAKED SCENES DECISION MAKING
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::PreUpdateService(u32 UNUSED_PARAM(timeInMs))
{
	if(SUPERCONDUCTOR.GetGunFightConductor().GetIntensity() > Intensity_Low)
	{
		if(m_FakeObjectWhenOffScreen)
		{
			if((m_OffScreenTimeOut > sm_MaxOffScreenTimeOut) || !m_ObjectToFake )
			{
				m_OffScreenTimeOut = 0;
				m_FakeObjectWhenOffScreen = false;
				//m_ObjectToFake = NULL;
			}
			else 
			{
				// Check if the object to fake has gone off-screen, if so fake impacts
				spdSphere boundSphere;
				m_ObjectToFake->GetBoundSphere(boundSphere);
				bool visible = camInterface::IsSphereVisibleInGameViewport(boundSphere.GetV4());
				if(!visible)
				{
					PlayObjectsFakedImpacts(true);
					m_FakeObjectWhenOffScreen = false;
					m_FakedImpactsHistory[AUD_OBJECT]++;
					m_OffScreenTimeOut = 0;
				}
				else 
				{
					m_OffScreenTimeOut += fwTimer::GetTimeStepInMilliseconds() ;
				}
			}
		}
	}
	sm_TimeSinceLastFakedBulletImpRunningAway += (fwTimer::GetTimeStepInMilliseconds() / 1000.f);
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::UpdatePostShootOutAndRicochets()
{
	for (u32 i = 0; i < AUD_MAX_POST_SHOOT_OUT_BUILDINGS; i ++)
	{
		m_PostShootOutBuilding[i].timeSinceLastBulletHit += fwTimer::GetTimeStepInMilliseconds();
		if(m_PostShootOutBuilding[i].timeSinceLastBulletHit >= sm_TimeToResetBulletCount)
		{
			m_PostShootOutBuilding[i].building = NULL;
			m_PostShootOutBuilding[i].bulletHitCount = 0;
		}
	}
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Can't find local player."))
	{
		if(!pPlayer->IsFiring())
		{
			m_TimeNotFiring += fwTimer::GetTimeStepInMilliseconds();
			// Ricochets
			UpdateFakeRicochets();
			//Vehicle debris
			UpdatePostShootOutVehDebris();
			//Building debris
			UpdatePostShootOutBuildingDebris();
			m_TimeFiring = 0;
		}
		else
		{
			m_TimeFiring += fwTimer::GetTimeStepInMilliseconds();
			m_TimeNotFiring = 0;
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::UpdateFakeRicochets()
{
	if(m_HasToPlayFakeRicochets && !m_FakeRicochetsSound)
	{
		if(m_TimeFiring >= sm_MinTimeFiringToFakeRicochets  && m_TimeSinceLastCandidateHit <= sm_MaxTimeToFakeRicochets)
		{
			m_HasToPlayFakeRicochets = false;
			audSoundInitParams initParams;
			initParams.Position = m_CandidateHitPosition;
			f32 distanceToListener = g_AudioEngine.GetEnvironment().ComputeSqdDistanceToPanningListener(m_CandidateHitPosition);
			f32 finalPredelay = m_DistanceToFakeRicochetsPredelay.CalculateValue(distanceToListener) - (f32)m_TimeSinceLastCandidateHit;
			initParams.Predelay = (u16)Max(0.f,finalPredelay);
			naAssert(audCollisionAudioEntity::sm_PlayerRicochetsRollOffScaleFactor != 0.f);
			initParams.VolumeCurveScale = audCollisionAudioEntity::sm_PlayerRicochetsRollOffScaleFactor;
			CreateAndPlaySound_Persistent(ATSTRINGHASH("CONDUCTOR_RICCO_RAND", 0x9288EA3D),&m_FakeRicochetsSound,&initParams);
#if __BANK
			if(sm_ShowFakeRiccoPos)
			{
				grcDebugDraw::Line(m_CandidateHitPosition,m_CandidateHitPosition + Vector3(1.f,0.f,0.f),Color_red,1000);
				grcDebugDraw::Line(m_CandidateHitPosition,m_CandidateHitPosition + Vector3(0.f,1.f,0.f),Color_green,1000);
				grcDebugDraw::Line(m_CandidateHitPosition,m_CandidateHitPosition + Vector3(0.f,0.f,1.f),Color_blue,1000);
			}
#endif
		}
	}
	m_TimeSinceLastCandidateHit += fwTimer::GetTimeStepInMilliseconds();
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::UpdatePostShootOutVehDebris()
{
	if(m_HasToPlayPostShootOutVehDebris && !m_PostShootOutVehDebrisSound)
	{
		if(m_PostShootOutVehCandidate)
		{
			bool nearEvent = true;
			bool nearLightEvent = SUPERCONDUCTOR.GetGunFightConductor().GetIntensity() < Intensity_Medium;
			u32 timeThreshold = sm_MinTimeNotFiringToFakeNearVehDebris;
			if(g_AudioEngine.GetEnvironment().ComputeSqdDistanceToPanningListener(VEC3V_TO_VECTOR3(m_PostShootOutVehCandidate->GetTransform().GetPosition())) > sm_VehSqdDistanceToPlayFarDebris)
			{
				timeThreshold = sm_MinTimeNotFiringToFakeFarVehDebris;
				nearEvent = false;
			}
			else if( nearLightEvent )
			{
				timeThreshold = sm_MinTimeNotFiringToFakeNearLightVehDebris;
			}
			
			if(g_CollisionAudioEntity.GetTimeSinceLastBulletHit() >= timeThreshold)
			{
				if(m_PostShootOutVehCandidate->GetVehicleAudioEntity() && m_PostShootOutVehCandidate->GetVehicleAudioEntity()->GetBulletHitCount() >= sm_BulletHitsToTriggerVehicleDebris)
				{
					m_HasToPlayPostShootOutVehDebris = false;
					audSoundInitParams initParams;
					//initParams.Position = m_PostShootOutVehCandidateHitPosition;
					initParams.Tracker = m_PostShootOutVehCandidate->GetPlaceableTracker();
					initParams.EnvironmentGroup = m_PostShootOutVehCandidate->GetVehicleAudioEntity()->GetEnvironmentGroup();
					if(nearEvent)
					{
						if (!nearLightEvent)
						{
							CreateAndPlaySound_Persistent(m_PostShootOutSounds.Find(ATSTRINGHASH("VEHICLE_DEBRIS_NEAR", 0x9D0A93AA)),&m_PostShootOutVehDebrisSound,&initParams);
						}
						else
						{
							CreateAndPlaySound_Persistent(m_PostShootOutSounds.Find(ATSTRINGHASH("VEHICLE_DEBRIS_NEAR_LIGHT", 0xDB4AD363)),&m_PostShootOutVehDebrisSound,&initParams);
						}
					}
					else if (!nearLightEvent)
					{
						CreateAndPlaySound_Persistent(m_PostShootOutSounds.Find(ATSTRINGHASH("VEHICLE_DEBRIS_FAR", 0xA88A1963)),&m_PostShootOutVehDebrisSound,&initParams);
					}
#if __BANK
					if(sm_ShowPostShootOutPos)
					{
						grcDebugDraw::Line(m_PostShootOutVehCandidateHitPosition,m_PostShootOutVehCandidateHitPosition + Vector3(1.f,0.f,0.f),Color_red,1000);
						grcDebugDraw::Line(m_PostShootOutVehCandidateHitPosition,m_PostShootOutVehCandidateHitPosition + Vector3(0.f,1.f,0.f),Color_green,1000);
						grcDebugDraw::Line(m_PostShootOutVehCandidateHitPosition,m_PostShootOutVehCandidateHitPosition + Vector3(0.f,0.f,1.f),Color_blue,1000);
					}
#endif
				}
				m_HasToPlayPostShootOutVehDebris = false;
				m_PostShootOutVehCandidate = NULL;
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::UpdatePostShootOutBuildingDebris()
{
	if(m_HasToPlayPostShootOutBuildingDebris && !m_PostShootOutBuildingDebrisSound)
	{
		bool nearEvent = true;
		u32 timeThreshold = sm_MinTimeNotFiringToFakeNearBuildingDebris;
		if(g_AudioEngine.GetEnvironment().ComputeSqdDistanceToPanningListener(m_PostShootOutBuildingCandidateHitPosition) > sm_BuildingSqdDistanceToPlayFarDebris)
		{
			timeThreshold = sm_MinTimeNotFiringToFakeFarBuildingDebris;
			nearEvent = false;
		}
			
		if(g_CollisionAudioEntity.GetTimeSinceLastBulletHit() >= timeThreshold)
		{
			if(m_PostShootOutBuildingCandidate.building && m_PostShootOutBuildingCandidate.bulletHitCount >= sm_BulletHitsToTriggerBuildingDebris)
			{
				m_HasToPlayPostShootOutBuildingDebris = false;
				audSoundInitParams initParams;
				initParams.Position = m_PostShootOutBuildingCandidateHitPosition;
				if(nearEvent)
				{
					CreateAndPlaySound_Persistent(m_PostShootOutSounds.Find(ATSTRINGHASH("CONCRETE_DEBRIS_NEAR", 0xDA17134C)),&m_PostShootOutBuildingDebrisSound,&initParams);
				}
				else 
				{
					CreateAndPlaySound_Persistent(m_PostShootOutSounds.Find(ATSTRINGHASH("CONCRETE_DEBRIS_FAR", 0x93F32379)),&m_PostShootOutBuildingDebrisSound,&initParams);
				}
#if __BANK
				if(sm_ShowPostShootOutPos)
				{
					grcDebugDraw::Line(m_PostShootOutBuildingCandidateHitPosition,m_PostShootOutBuildingCandidateHitPosition + Vector3(1.f,0.f,0.f),Color_red,1000);
					grcDebugDraw::Line(m_PostShootOutBuildingCandidateHitPosition,m_PostShootOutBuildingCandidateHitPosition + Vector3(0.f,1.f,0.f),Color_green,1000);
					grcDebugDraw::Line(m_PostShootOutBuildingCandidateHitPosition,m_PostShootOutBuildingCandidateHitPosition + Vector3(0.f,0.f,1.f),Color_blue,1000);
				}
#endif
			}
			m_HasToPlayPostShootOutBuildingDebris = false;
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::ProcessUpdate()
{
	UpdatePostShootOutAndRicochets();
	if(SUPERCONDUCTOR.GetGunFightConductor().GetIntensity() >= Intensity_Medium)
	{
		m_StateProcessed = true;
		sm_TimeSinceLastSirenSmashed += fwTimer::GetTimeStepInMilliseconds();
		Update();
		BANK_ONLY(GunfightConductorFSDebugInfo();)
	}
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audGunFightConductorFakeScenes::UpdateState(const s32 state, const s32, const fwFsm::Event event)
{
	fwFsmBegin
		fwFsmState(State_Idle)
			fwFsmOnUpdate
				return Idle();
		fwFsmState(State_EmphasizePlayerIntoCoverScene)
			fwFsmOnUpdate
				return EmphasizePlayerIntoCoverScene();
		fwFsmState(State_EmphasizeRunAwayScene)
			fwFsmOnUpdate
				return EmphasizeRunAwayScene();
		fwFsmState(State_EmphasizeOpenSpaceScene)
			fwFsmOnUpdate
			return EmphasizeOpenSpaceScene();
	fwFsmEnd
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audGunFightConductorFakeScenes::Idle()
{
	// First of all check if we have to trigger run away scene
	if(m_HasToTriggerGoingIntoCover)
	{
		m_HasToTriggerGoingIntoCover = false;
		m_LastState = GetState();
		SetState(State_EmphasizePlayerIntoCoverScene);
		m_StateProcessed = false;
		return fwFsm::Continue;
	}
	//The open space scene also cares about the running away feature, but in case we are not in an open space, let it work as usual.
	if(!HandleOpenSpaceScene())
	{
		HandleRunAwayScene();
	}
	return fwFsm::Continue;
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audGunFightConductorFakeScenes::EmphasizePlayerIntoCoverScene()
{
	if (!m_FakeBulletImpactSound && ((sm_DtToPlayFakedBulletImp >= 0.f) && (GetTimeInState() >= sm_DtToPlayFakedBulletImp)))
	{
		GunfightConductorIntensitySettings *intesitySettings = GetGunfightConductroIntensitySettings();
		// Check if it's safe to trigger the fake bi and compute its type
		if(ComputeLastShotTimeAndFakedBulletImpactType(intesitySettings->Common.MaxTimeAfterLastShot))
		{	
			CPed* pPlayer = CGameWorld::FindLocalPlayer();
			if (naVerifyf(pPlayer,"Can't find local player."))
			{
				SetTimeInState(0.f);
				sm_DtToPlayFakedBulletImp = fwRandom::GetRandomNumberInRange(intesitySettings->GoingIntoCover.MinTimeToReTriggerFakeBI
					, intesitySettings->GoingIntoCover.MaxTimeToReTriggerFakeBI);

				CEntity* pEnt = CPlayerInfo::ms_DynamicCoverHelper.GetCoverEntryEntity();
				if(pEnt)
				{
					if(pEnt->GetType() == ENTITY_TYPE_OBJECT)
					{
						CObject* pObject = (CObject*) pEnt; 
						if(pObject)
						{
							m_ObjectToFake = pObject;
							PlayObjectsFakedImpacts();
						}
					}else if ( pEnt->GetType() == ENTITY_TYPE_VEHICLE)
					{
						CVehicle* pVeh = (CVehicle*) pEnt;
						if(pVeh)
						{
							m_VehicleToFake = pVeh;
							PlayVehiclesFakedImpacts();
							UpdateFakedVehicleVfx();
						}
					}
					else
					{
						naWarningf("Wrong entity type.");
					}
				}
			}
		}
	}
	return fwFsm::Continue;
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audGunFightConductorFakeScenes::EmphasizeRunAwayScene()
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Can't find local player."))
	{
		if(/*healthPercentage > sm_LifeThresholdToFakeRunAwayScene || */!pPlayer->GetMotionData()->GetIsSprinting() 
			&& !sm_ForceRunAwayScene)
		{
			DeEmphasizeRunAwayScene();
		}
		FakeRunningAway();
	}
	return fwFsm::Continue;
}
//-------------------------------------------------------------------------------------------------------------------
bool audGunFightConductorFakeScenes::FakeRunningAway()
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Can't find local player."))
	{
		f32 time = (GetState() == State_EmphasizeRunAwayScene ? GetTimeInState() : sm_TimeSinceLastFakedBulletImpRunningAway);
		if(!m_FakeBulletImpactSound && (sm_DtToPlayFakedBulletImpRunningAway >= 0.f) && (time >= sm_DtToPlayFakedBulletImpRunningAway))
		{  
			CPedIntelligence *pPedIntelligence = pPlayer->GetPedIntelligence();
			if(pPedIntelligence)
			{		
				GunfightConductorIntensitySettings *intesitySettings = GetGunfightConductroIntensitySettings();
				// Check if it's safe to trigger the fake bi and compute its type
				if(ComputeLastShotTimeAndFakedBulletImpactType(intesitySettings->Common.MaxTimeAfterLastShot))
				{	
					sm_DtToPlayFakedBulletImpRunningAway = fwRandom::GetRandomNumberInRange(intesitySettings->RunningAway.MinTimeToFakeBulletImpacts, intesitySettings->RunningAway.MaxTimeToFakeBulletImpacts);
					if(GetState() == State_EmphasizeRunAwayScene)
					{
						SetTimeInState(0.f);
					}
					sm_TimeSinceLastFakedBulletImpRunningAway = 0.f;
					audFakedImpactsEntity entityToPlay = AnalizeFakedImpactsHistory();
					Vector3 dirToPlayer;
					f32 distanceToPlayer;
					CObject* pObject = NULL;
					CVehicle* pVehicle = NULL;
					switch (entityToPlay)
					{
					case AUD_OBJECT:
						//we want to play and object, get the closest one and check it against the distance test
						pObject = GetClosestValidObject();
						if(pObject)
						{
							dirToPlayer = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition() - pObject->GetTransform().GetPosition());
							distanceToPlayer = dirToPlayer.Mag2();
							if(distanceToPlayer <= (sm_RSphereNearByEntities * sm_RSphereNearByEntities))
							{
								m_ObjectToFake = pObject;
								if(SUPERCONDUCTOR.GetGunFightConductor().GetIntensity() >= Intensity_High BANK_ONLY(|| sm_DontCheckOffScreen))
								{
									PlayObjectsFakedImpacts(true);
									m_FakedImpactsHistory[AUD_OBJECT]++;
									return true;
								}
								else
								{
									//Flag to play when the object goes offScreen.
									m_FakeObjectWhenOffScreen = true;
									m_OffScreenTimeOut = 0;
									return true;
								}
							}
						}
						//In case we don't have an object or it hasn't pass the test try to play a veh.
						pVehicle = pPedIntelligence->GetClosestVehicleInRange();
						if(pVehicle)
						{
							dirToPlayer = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition() - pVehicle->GetTransform().GetPosition());
							distanceToPlayer = dirToPlayer.Mag2();
							if(distanceToPlayer <= (sm_RSphereNearByEntities * sm_RSphereNearByEntities))
							{
								m_VehicleToFake = pVehicle;
								m_FakedImpactsHistory[AUD_VEHICLE]++;
								PlayVehiclesFakedImpacts();
								UpdateFakedVehicleVfx(true);
								return true;
							}
						}
						break;
					case AUD_VEHICLE:
						//we want to play and veh, get the closest one and check it against the distance test
						pVehicle = pPedIntelligence->GetClosestVehicleInRange();
						if(pVehicle)
						{
							dirToPlayer = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition() - pVehicle->GetTransform().GetPosition());
							distanceToPlayer = dirToPlayer.Mag2();
							if(distanceToPlayer <= (sm_RSphereNearByEntities * sm_RSphereNearByEntities))
							{
								m_VehicleToFake = pVehicle;
								m_FakedImpactsHistory[AUD_VEHICLE]++;
								PlayVehiclesFakedImpacts();
								UpdateFakedVehicleVfx(true);
								return true;
							}
						}
						//In case we don't have an veh or it hasn't pass the test try to play a object.
						pObject = GetClosestValidObject();
						if(pObject)
						{
							dirToPlayer = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition() - pObject->GetTransform().GetPosition());
							distanceToPlayer = dirToPlayer.Mag2();
							if(distanceToPlayer <= (sm_RSphereNearByEntities * sm_RSphereNearByEntities))
							{
								m_ObjectToFake = pObject;
								if(SUPERCONDUCTOR.GetGunFightConductor().GetIntensity() >= Intensity_High BANK_ONLY(|| sm_DontCheckOffScreen))
								{
									PlayObjectsFakedImpacts(true);
									m_FakedImpactsHistory[AUD_OBJECT]++;
									return true;
								}
								else
								{
									//Flag to play when the object goes offScreen.
									m_FakeObjectWhenOffScreen = true;
									m_OffScreenTimeOut = 0;
									return true;
								}
							}
						}
						break;
					default: break;
					}
				}
			}
		}
	}
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audGunFightConductorFakeScenes::EmphasizeOpenSpaceScene()
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Can't find local player."))
	{
		//Check if we want to play the running away stuff 
		if((pPlayer->GetMotionData()->GetIsSprinting() || sm_ForceRunAwayScene))
		{
			if(!m_WasSprinting)
			{
				GunfightConductorIntensitySettings *intesitySettings = GetGunfightConductroIntensitySettings();
				// Always emphasize bullet impacts to make the situation feel more dangerous.
				if(sm_AllowRunningAwayScene)
				{	
					m_WasSprinting = true;
					sm_DtToPlayFakedBulletImpRunningAway = fwRandom::GetRandomNumberInRange(0.f, intesitySettings->RunningAway.MaxTimeToFakeBulletImpacts);
				}
			}
		} 
		else 
		{
			m_VehicleToFake = NULL;
			m_ObjectToFake = NULL;
			m_FakedImpactsHistory[AUD_OBJECT] = 0;
			m_FakedImpactsHistory[AUD_VEHICLE] = 0;
			sm_DtToPlayFakedBulletImpRunningAway = -1.f;
			m_WasSprinting = false;
		}
		if(FakeRunningAway() || m_FakeObjectWhenOffScreen)
		{
			if(!sm_AllowOpenSpaceWhenRunningAway)
			{
				return fwFsm::Continue;
			}
		}
		if((CalculateOpenSpaceConfidence() < sm_OpenSpaceConfidenceThreshold || sm_DtToPlayFakedBulletImpOpenSpace < 0)	BANK_ONLY(&& !sm_ForceOpenSpaceScene ))
		{
			DeEmphasizeOpenSpaceScene();
		}
#if __BANK 
		if(sm_DtToPlayFakedBulletImpOpenSpace == -1.f && sm_ForceOpenSpaceScene)
		{
			TriggerOpenSpaceScene();
		}
#endif
		if(!m_FakeBulletImpactSound && (sm_DtToPlayFakedBulletImpOpenSpace >= 0.f) && (GetTimeInState() >= sm_DtToPlayFakedBulletImpOpenSpace))
		{  
			GunfightConductorIntensitySettings *intesitySettings = GetGunfightConductroIntensitySettings();
			// Check if it's safe to trigger the fake bi and compute its type
			if(ComputeLastShotTimeAndFakedBulletImpactType(intesitySettings->Common.MaxTimeAfterLastShot))
			{	
 				sm_DtToPlayFakedBulletImpOpenSpace = fwRandom::GetRandomNumberInRange(intesitySettings->OpenSpace.MinTimeToFakeBulletImpacts, intesitySettings->OpenSpace.MaxTimeToFakeBulletImpacts);
 				SetTimeInState(0.f);
				PlayGroundFakedImpacts();
			}
		}
	}
	return fwFsm::Continue;
}
//-------------------------------------------------------------------------------------------------------------------
//														HELPERS	
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::HandleRunAwayScene()
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Can't find local player."))
	{	
		// TODO: At the moment the health check is disable.
		//float healthPercentage = pPlayer->GetHealth() / pPlayer->GetMaxHealth();
		if((GetState() != State_EmphasizeRunAwayScene)
			/*&& (healthPercentage <= sm_LifeThresholdToFakeRunAwayScene */&& (pPlayer->GetMotionData()->GetIsSprinting()
			|| sm_ForceRunAwayScene))/*)*/
		{
			EmphasizePlayerRunningAway();
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
bool audGunFightConductorFakeScenes::HandleOpenSpaceScene()
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Can't find local player."))
	{	
		f32 openSpaceConfidence = CalculateOpenSpaceConfidence();
		if((GetState() != State_EmphasizeOpenSpaceScene)
			&& (openSpaceConfidence >= sm_OpenSpaceConfidenceThreshold	BANK_ONLY(|| sm_ForceOpenSpaceScene)))
		{
			TriggerOpenSpaceScene();
			return true;
		}
	}
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
f32 audGunFightConductorFakeScenes::CalculateOpenSpaceConfidence()
{
	////This function will return the confidence to check how open the space is 
	//for(u32 i = AUD_AMB_DIR_NORTH ; i < AUD_AMB_DIR_LOCAL; i++)
	//{
	//	f32 externalOclussion = 0.f;
	//	char txt[256];
	//	formatf(txt, "[%d %f %f]",i
	//		,audNorthAudioEngine::GetGtaEnvironment()->GetBlockedFactorForDirection((audAmbienceDirection)i,externalOclussion)
	//		,externalOclussion);
	//	grcDebugDraw::AddDebugOutput(Color_white,txt);
	//}
	//CPed* pPlayer = CGameWorld::FindLocalPlayer();
	//f32 exteriorOclusion = 0.f;
	//Vector3 pos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
	//f32 blockedFactorStart = audNorthAudioEngine::GetGtaEnvironment()->GetBlockedFactorFor3DPosition(pos, exteriorOclusion);
	//char txt[256];
	//formatf(txt, "[3d pos %f %f]"	,blockedFactorStart	,exteriorOclusion);
	//grcDebugDraw::AddDebugOutput(Color_white,txt);

	// CHECK BLOCKED FACTOR -> Do it as rest of the ambience factors.
	return (audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior() ? 0.f : 1.f) ;
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::TriggerOpenSpaceScene()
{
	// Emphasize bullet impacts
	SUPERCONDUCTOR.GetGunFightConductor().GetDynamicMixingConductor().EmphasizeBulletImpacts();
	GunfightConductorIntensitySettings *intesitySettings = GetGunfightConductroIntensitySettings();
	// Always emphasize bullet impacts to make the situation feel more dangerous.
	sm_DtToPlayFakedBulletImpOpenSpace = fwRandom::GetRandomNumberInRange(0.f, intesitySettings->OpenSpace.MaxTimeToFakeBulletImpacts);
	m_LastState = GetState();
	SetState(State_EmphasizeOpenSpaceScene);
	m_StateProcessed = false;
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::EmphasizePlayerIntoCover()
{
	GunfightConductorIntensitySettings *intesitySettings = GetGunfightConductroIntensitySettings();
	// Always emphasize bullet impacts to make the situation feel more dangerous.
	sm_DtToPlayFakedBulletImp = fwRandom::GetRandomNumberInRange(0.f, intesitySettings->GoingIntoCover.TimeToFakeBulletImpacts);

	if(m_LastState == State_EmphasizeRunAwayScene && GetState() == State_EmphasizeRunAwayScene )
	{
		m_HasToTriggerGoingIntoCover = true;
		return;
	}
	m_LastState = GetState();
	SetState(State_EmphasizePlayerIntoCoverScene);
	m_StateProcessed = false;
}
//-------------------------------------------------------------------------------------------------------------------
GunfightConductorIntensitySettings * audGunFightConductorFakeScenes::GetGunfightConductroIntensitySettings()
{
	GunfightConductorIntensitySettings *intensitySettings = NULL;
	switch(SUPERCONDUCTOR.GetGunFightConductor().GetIntensity())
	{
	case Intensity_Medium:
		intensitySettings =  audNorthAudioEngine::GetObject<GunfightConductorIntensitySettings>(ATSTRINGHASH("MEDIUM_INTENSITY", 0x49EEFB4F));
		break;
	case Intensity_High:
		intensitySettings =  audNorthAudioEngine::GetObject<GunfightConductorIntensitySettings>(ATSTRINGHASH("HIGH_INTENSITY", 0xEE401A36));
		break;
	default:
		break;
	}
	naAssertf(intensitySettings,"Failed getting gunfight conductor intesnity settings.");
	return intensitySettings;
}
//--------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::HandlePlayerExitCover()
{
	m_ObjectToFake = NULL;
	m_LastState = GetState();
	SetState(State_Idle);
	m_StateProcessed = false;
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::EmphasizePlayerRunningAway()
{
	// Emphasize bullet impacts
	SUPERCONDUCTOR.GetGunFightConductor().GetDynamicMixingConductor().EmphasizeBulletImpacts();
	GunfightConductorIntensitySettings *intesitySettings = GetGunfightConductroIntensitySettings();
	// Always emphasize bullet impacts to make the situation feel more dangerous.
	if(sm_AllowRunningAwayScene )
	{	
		sm_DtToPlayFakedBulletImpRunningAway = fwRandom::GetRandomNumberInRange(0.f, intesitySettings->RunningAway.MaxTimeToFakeBulletImpacts);
	}
	m_LastState = GetState();
	SetState(State_EmphasizeRunAwayScene);
	m_StateProcessed = false;
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::DeEmphasizeRunAwayScene()
{
	SUPERCONDUCTOR.GetGunFightConductor().GetDynamicMixingConductor().DeemphasizeBulletImpacts();
	m_VehicleToFake = NULL;
	m_ObjectToFake = NULL;
	m_FakedImpactsHistory[AUD_OBJECT] = 0;
	m_FakedImpactsHistory[AUD_VEHICLE] = 0;
	sm_DtToPlayFakedBulletImpRunningAway = -1.f;
	m_LastState = GetState();
	SetState(State_Idle);
	m_StateProcessed = false;
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::DeEmphasizeOpenSpaceScene()
{
	SUPERCONDUCTOR.GetGunFightConductor().GetDynamicMixingConductor().DeemphasizeBulletImpacts();
	sm_DtToPlayFakedBulletImpOpenSpace = -1.f;
	m_LastState = GetState();
	SetState(State_Idle);
	m_StateProcessed = false;
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::PlayObjectsFakedImpacts(bool useMACSmaterial)
{
#if __BANK
	if(sm_FakeBulletImpacts)
	{
		m_typeOfFakedBulletImpacts = (typeOfFakedBulletImpacts)(sm_FakeBulletImpactType -1);
	}
#endif

	if(g_ScriptAudioEntity.ShouldDuckForScriptedConversation() && audCollisionAudioEntity::ShouldDialogueSuppressBI())
	{
		return;
	}

	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Can't find local player."))
	{
		if (naVerifyf(m_ObjectToFake,"NULL object when trying to play faked impacts.") && !m_FakeBulletImpactSound)
		{
			//Check if they have a custom sound for faked bullet impacts or we have to play the dynamic sound or nothing.
			audSoundSet objectFakedImpact;
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_ObjectToFake->GetAudioEnvironmentGroup();
			initParams.Tracker = m_ObjectToFake->GetPlaceableTracker();
			initParams.Position = VEC3V_TO_VECTOR3(m_ObjectToFake->GetTransform().GetPosition());
			const ModelAudioCollisionSettings * objectMatSet = m_ObjectToFake->GetBaseModelInfo()->GetAudioCollisionSettings();
			EmphasizeIntensity currentIntensity = SUPERCONDUCTOR.GetGunFightConductor().GetIntensity();
			if(objectMatSet)
			{
				if(currentIntensity == Intensity_Medium)
				{
					objectFakedImpact.Init(objectMatSet->MediumIntensity);
				}
				else if (currentIntensity == Intensity_High)
				{
					objectFakedImpact.Init(objectMatSet->HighIntensity);
				}
				//If fakeBulletImpactSound is null, play the dynamic sound.
				if(!objectFakedImpact.IsInitialised())
				{
					if(!useMACSmaterial)
					{
						m_CurrentFakedCollisionMaterial = CPlayerInfo::ms_DynamicCoverHelper.GetCoverEntryMaterial();
					}
					else if (objectMatSet->numMaterialOverrides != 0)
					{
						m_CurrentFakedCollisionMaterial = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(objectMatSet->MaterialOverride[0].Override);
					}

					if(m_CurrentFakedCollisionMaterial)
					{
						if(currentIntensity == Intensity_Medium)
						{
							objectFakedImpact.Init(m_CurrentFakedCollisionMaterial->MediumIntensity);
						}
						else if (currentIntensity == Intensity_High)
						{
							objectFakedImpact.Init(m_CurrentFakedCollisionMaterial->HighIntensity);
						}
						if(objectFakedImpact.IsInitialised())
						{
							switch(m_typeOfFakedBulletImpacts)
							{
							case Single_Impact: 
								CreateSound_PersistentReference(objectFakedImpact.Find(ATSTRINGHASH("SINGLE_IMPACT", 0xADF58ED0)),&m_FakeBulletImpactSound, &initParams);
								break;			
							case Burst_Of_Impacts:
								CreateSound_PersistentReference(objectFakedImpact.Find(ATSTRINGHASH("BURST_OF_IMPACTS", 0xF54F48E9)),&m_FakeBulletImpactSound, &initParams);
								break;			
							case Hard_Impact:	
								CreateSound_PersistentReference(objectFakedImpact.Find(ATSTRINGHASH("HARD_IMPACT", 0xB80E7AB6)),&m_FakeBulletImpactSound, &initParams);
								break;			
							case Bunch_Of_Impacts:
								CreateSound_PersistentReference(objectFakedImpact.Find(ATSTRINGHASH("BUNCH_OF_IMPACTS", 0x9C7A322E)),&m_FakeBulletImpactSound, &initParams);
								break;
							default: 
								break;
							}
						}
					}
				}
				else
				{
					switch(m_typeOfFakedBulletImpacts)
					{
					case Single_Impact: 
						CreateSound_PersistentReference(objectFakedImpact.Find(ATSTRINGHASH("SINGLE_IMPACT", 0xADF58ED0)),&m_FakeBulletImpactSound, &initParams);
						break;			
					case Burst_Of_Impacts:
						CreateSound_PersistentReference(objectFakedImpact.Find(ATSTRINGHASH("BURST_OF_IMPACTS", 0xF54F48E9)),&m_FakeBulletImpactSound, &initParams);
						break;			
					case Hard_Impact:	
						CreateSound_PersistentReference(objectFakedImpact.Find(ATSTRINGHASH("HARD_IMPACT", 0xB80E7AB6)),&m_FakeBulletImpactSound, &initParams);
						break;			
					case Bunch_Of_Impacts:
						CreateSound_PersistentReference(objectFakedImpact.Find(ATSTRINGHASH("BUNCH_OF_IMPACTS", 0x9C7A322E)),&m_FakeBulletImpactSound, &initParams);
						break;
					default: 
						break;
					}

				}
			}
			else 
			{
				m_CurrentFakedCollisionMaterial = CPlayerInfo::ms_DynamicCoverHelper.GetCoverEntryMaterial();
				if(m_CurrentFakedCollisionMaterial)
				{
					if(currentIntensity == Intensity_Medium)
					{
						objectFakedImpact.Init(m_CurrentFakedCollisionMaterial->MediumIntensity);
					}
					else if (currentIntensity == Intensity_High)
					{
						objectFakedImpact.Init(m_CurrentFakedCollisionMaterial->HighIntensity);
					}
					if(objectFakedImpact.IsInitialised())
					{
						switch(m_typeOfFakedBulletImpacts)
						{
						case Single_Impact: 
							CreateSound_PersistentReference(objectFakedImpact.Find(ATSTRINGHASH("SINGLE_IMPACT", 0xADF58ED0)),&m_FakeBulletImpactSound, &initParams);
							break;												  
						case Burst_Of_Impacts:									  
							CreateSound_PersistentReference(objectFakedImpact.Find(ATSTRINGHASH("BURST_OF_IMPACTS", 0xF54F48E9)),&m_FakeBulletImpactSound, &initParams);
							break;												  
						case Hard_Impact:										  
							CreateSound_PersistentReference(objectFakedImpact.Find(ATSTRINGHASH("HARD_IMPACT", 0xB80E7AB6)),&m_FakeBulletImpactSound, &initParams);
							break;												  
						case Bunch_Of_Impacts:									  
							CreateSound_PersistentReference(objectFakedImpact.Find(ATSTRINGHASH("BUNCH_OF_IMPACTS", 0x9C7A322E)),&m_FakeBulletImpactSound, &initParams);
							break;
						default: 
							break;
						}
					}
				}
			}
			//if at this point we still don't have a soundset, use the dynamic one.
			if(!objectFakedImpact.IsInitialised())
			{
				audSoundSet dynamicObjectSoundSet;
				GunfightConductorIntensitySettings *intesitySettings = GetGunfightConductroIntensitySettings();
				if(currentIntensity == Intensity_Medium)
				{
					dynamicObjectSoundSet.Init(intesitySettings->ObjectsMediumIntensity);
				}
				else if (currentIntensity == Intensity_High)
				{
					dynamicObjectSoundSet.Init(intesitySettings->ObjectsHighIntensity);
				}
				if(dynamicObjectSoundSet.IsInitialised())
				{
					switch(m_typeOfFakedBulletImpacts)
					{
					case Single_Impact: 
						CreateSound_PersistentReference(dynamicObjectSoundSet.Find(ATSTRINGHASH("SINGLE_IMPACT", 0xADF58ED0)),&m_FakeBulletImpactSound, &initParams);
						break;												  
					case Burst_Of_Impacts:									  
						CreateSound_PersistentReference(dynamicObjectSoundSet.Find(ATSTRINGHASH("BURST_OF_IMPACTS", 0xF54F48E9)),&m_FakeBulletImpactSound, &initParams);
						break;												  
					case Hard_Impact:										  
						CreateSound_PersistentReference(dynamicObjectSoundSet.Find(ATSTRINGHASH("HARD_IMPACT", 0xB80E7AB6)),&m_FakeBulletImpactSound, &initParams);
						break;												  
					case Bunch_Of_Impacts:									  
						CreateSound_PersistentReference(dynamicObjectSoundSet.Find(ATSTRINGHASH("BUNCH_OF_IMPACTS", 0x9C7A322E)),&m_FakeBulletImpactSound, &initParams);
						break;
					default: 
						break;
					}				
				}
			}
			BANK_ONLY(Color32 color = Color32(255,0,0,128););
			if(m_FakeBulletImpactSound )
			{
				BANK_ONLY( color = Color32(0,255,0,128););
				m_FakeBulletImpactSound->PrepareAndPlay();
			}
#if __BANK
			if(sm_DisplayDynamicSoundsPlayed)
			{
				grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_ObjectToFake->GetTransform().GetPosition()), 2.f,color);
			}
#endif
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::PlayVehiclesFakedImpacts()
{
#if __BANK
	if(sm_FakeBulletImpacts)
	{
		m_typeOfFakedBulletImpacts = (typeOfFakedBulletImpacts)(sm_FakeBulletImpactType -1);
	}
#endif
	if(g_ScriptAudioEntity.ShouldDuckForScriptedConversation() && audCollisionAudioEntity::ShouldDialogueSuppressBI())
	{
		return;
	}
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Can't find local player."))
	{
		if (naVerifyf(m_VehicleToFake,"NULL Vehicle when trying to play faked impacts.") && !m_FakeBulletImpactSound)
		{
			if(m_VehicleToFake->GetVehicleType() != VEHICLE_TYPE_BICYCLE)
			{
				EmphasizeIntensity currentIntensity = SUPERCONDUCTOR.GetGunFightConductor().GetIntensity();
				//Check if they have a custom sound for faked bullet impacts or we have to play the dynamic sound or nothing.
				audVehicleAudioEntity *vehicleAudioEntity = m_VehicleToFake->GetVehicleAudioEntity(); 
				if(vehicleAudioEntity)
				{
					VehicleCollisionSettings * vehicleCollisionSet = vehicleAudioEntity->GetCollisionAudio().GetVehicleCollisionSettings(); 
					if(vehicleCollisionSet)
					{					
						audSoundInitParams initParams;
						initParams.EnvironmentGroup = m_VehicleToFake->GetAudioEnvironmentGroup();
						initParams.Tracker = m_VehicleToFake->GetPlaceableTracker();
						initParams.Position = VEC3V_TO_VECTOR3(m_VehicleToFake->GetTransform().GetPosition());

						audSoundSet vehicleFakedImpact;
						if(currentIntensity == Intensity_Medium)
						{
							vehicleFakedImpact.Init(vehicleCollisionSet->MediumIntensity);
						}
						else if (currentIntensity == Intensity_High)
						{
							vehicleFakedImpact.Init(vehicleCollisionSet->HighIntensity);
						}
						if(vehicleFakedImpact.IsInitialised())
						{
							switch(m_typeOfFakedBulletImpacts)
							{
							case Single_Impact: 
								CreateSound_PersistentReference(vehicleFakedImpact.Find(ATSTRINGHASH("SINGLE_IMPACT", 0xADF58ED0)),&m_FakeBulletImpactSound, &initParams);
								break;											   
							case Burst_Of_Impacts:								   
								CreateSound_PersistentReference(vehicleFakedImpact.Find(ATSTRINGHASH("BURST_OF_IMPACTS", 0xF54F48E9)),&m_FakeBulletImpactSound, &initParams);
								break;											   
							case Hard_Impact:									   
								CreateSound_PersistentReference(vehicleFakedImpact.Find(ATSTRINGHASH("HARD_IMPACT", 0xB80E7AB6)),&m_FakeBulletImpactSound, &initParams);
								break;											   
							case Bunch_Of_Impacts:								   
								CreateSound_PersistentReference(vehicleFakedImpact.Find(ATSTRINGHASH("BUNCH_OF_IMPACTS", 0x9C7A322E)),&m_FakeBulletImpactSound, &initParams);
								break;
							default: 
								break;
							}
						}
						else
						{
							audSoundSet dynamicVehSoundSet;
							GunfightConductorIntensitySettings *intesitySettings = GetGunfightConductroIntensitySettings();
							if(currentIntensity == Intensity_Medium)
							{
								dynamicVehSoundSet.Init(intesitySettings->VehiclesMediumIntensity);
							}
							else if (currentIntensity == Intensity_High)
							{
								dynamicVehSoundSet.Init(intesitySettings->VehiclesHighIntensity);
							}
							if(dynamicVehSoundSet.IsInitialised())
							{
								m_CurrentFakedCollisionMaterial = vehicleAudioEntity->GetCollisionAudio().GetVehicleCollisionMaterialSettings();

								switch(m_typeOfFakedBulletImpacts)
								{
								case Single_Impact: 
									CreateSound_PersistentReference(dynamicVehSoundSet.Find(ATSTRINGHASH("SINGLE_IMPACT", 0xADF58ED0)),&m_FakeBulletImpactSound, &initParams);
									break;											   
								case Burst_Of_Impacts:								   
									CreateSound_PersistentReference(dynamicVehSoundSet.Find(ATSTRINGHASH("BURST_OF_IMPACTS", 0xF54F48E9)),&m_FakeBulletImpactSound, &initParams);
									break;											   
								case Hard_Impact:									   
									CreateSound_PersistentReference(dynamicVehSoundSet.Find(ATSTRINGHASH("HARD_IMPACT", 0xB80E7AB6)),&m_FakeBulletImpactSound, &initParams);
									break;											   
								case Bunch_Of_Impacts:								   
									CreateSound_PersistentReference(dynamicVehSoundSet.Find(ATSTRINGHASH("BUNCH_OF_IMPACTS", 0x9C7A322E)),&m_FakeBulletImpactSound, &initParams);
									break;
								default: 
									break;
								}
							}
						}
						BANK_ONLY(Color32 color = Color32(255,0,0,128););
						if(m_FakeBulletImpactSound)
						{
							BANK_ONLY(color = Color32(0,255,0,128););
							m_FakeBulletImpactSound->PrepareAndPlay();
						}
#if __BANK
						if(sm_DisplayDynamicSoundsPlayed)
						{
							grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_VehicleToFake->GetTransform().GetPosition()), 2.f,color);
						}
#endif
					}
				}
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::PlayGroundFakedImpacts()
{
#if __BANK
	if(sm_FakeBulletImpacts)
	{
		m_typeOfFakedBulletImpacts = (typeOfFakedBulletImpacts)(sm_FakeBulletImpactType -1);
	}
#endif
	if(g_ScriptAudioEntity.ShouldDuckForScriptedConversation() && audCollisionAudioEntity::ShouldDialogueSuppressBI())
	{
		return;
	}
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Can't find local player."))
	{
		if(pPlayer->GetIsInWater())
		{
			return;
		}
		CVehicle *playerVeh = CGameWorld::FindLocalPlayerVehicle();
		if(playerVeh && playerVeh->GetIsInWater())
		{
			return;
		}
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = pPlayer->GetAudioEnvironmentGroup();
		initParams.Position = CalculateFakeGroundBIPosition();
		//If fakeBulletImpactSound is null, play the dynamic sound.
		m_CurrentFakedCollisionMaterial = pPlayer->GetPedAudioEntity()->GetFootStepAudio().GetCurrentMaterialSettings();
		if(m_CurrentFakedCollisionMaterial)
		{
			audSoundSet groundFakedImpact;
			EmphasizeIntensity currentIntensity = SUPERCONDUCTOR.GetGunFightConductor().GetIntensity();
			if(currentIntensity == Intensity_Medium)
			{
				groundFakedImpact.Init(m_CurrentFakedCollisionMaterial->MediumIntensity);
			}
			else if (currentIntensity == Intensity_High)
			{
				groundFakedImpact.Init(m_CurrentFakedCollisionMaterial->HighIntensity);
			}
			if(groundFakedImpact.IsInitialised())
			{

				switch(m_typeOfFakedBulletImpacts)
				{
				case Single_Impact: 
					CreateSound_PersistentReference(groundFakedImpact.Find(ATSTRINGHASH("SINGLE_IMPACT", 0xADF58ED0)),&m_FakeBulletImpactSound, &initParams);
					break;											  
				case Burst_Of_Impacts:								  
					CreateSound_PersistentReference(groundFakedImpact.Find(ATSTRINGHASH("BURST_OF_IMPACTS", 0xF54F48E9)), &m_FakeBulletImpactSound,&initParams);
					break;											  
				case Hard_Impact:									  
					CreateSound_PersistentReference(groundFakedImpact.Find(ATSTRINGHASH("HARD_IMPACT", 0xB80E7AB6)),&m_FakeBulletImpactSound, &initParams);
					break;											  
				case Bunch_Of_Impacts:								  
					CreateSound_PersistentReference(groundFakedImpact.Find(ATSTRINGHASH("BUNCH_OF_IMPACTS", 0x9C7A322E)),&m_FakeBulletImpactSound, &initParams);
					break;
				default: 
					break;
				}
			} 
			else
			{
				audSoundSet dynamicGroundSoundSet;
				GunfightConductorIntensitySettings *intesitySettings = GetGunfightConductroIntensitySettings();
				if(currentIntensity == Intensity_Medium)
				{
					dynamicGroundSoundSet.Init(intesitySettings->GroundMediumIntensity);
				}
				else if (currentIntensity == Intensity_High)
				{
					dynamicGroundSoundSet.Init(intesitySettings->GroundHighIntensity);
				}
				if(dynamicGroundSoundSet.IsInitialised())
				{
					switch(m_typeOfFakedBulletImpacts)
					{
					case Single_Impact: 
						CreateSound_PersistentReference(dynamicGroundSoundSet.Find(ATSTRINGHASH("SINGLE_IMPACT", 0xADF58ED0)),&m_FakeBulletImpactSound, &initParams);
						break;
					case Burst_Of_Impacts:
						CreateSound_PersistentReference(dynamicGroundSoundSet.Find(ATSTRINGHASH("BURST_OF_IMPACTS", 0xF54F48E9)),&m_FakeBulletImpactSound, &initParams);
						break;
					case Hard_Impact:
						CreateSound_PersistentReference(dynamicGroundSoundSet.Find(ATSTRINGHASH("HARD_IMPACT", 0xB80E7AB6)),&m_FakeBulletImpactSound, &initParams);
						break;
					case Bunch_Of_Impacts:
						CreateSound_PersistentReference(dynamicGroundSoundSet.Find(ATSTRINGHASH("BUNCH_OF_IMPACTS", 0x9C7A322E)),&m_FakeBulletImpactSound, &initParams);
						break;
					default: 
						break;
					}
				}
			}
		}
		if(m_FakeBulletImpactSound)
		{
			m_FakeBulletImpactSound->PrepareAndPlay();
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
Vector3 audGunFightConductorFakeScenes::CalculateFakeGroundBIPosition()
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Can't find local player."))
	{
		//Calculate a point in the ground a few meters (random number) behind the player in that direction. 
		f32 rndDist = fwRandom::GetRandomNumberInRange(0.5f,3.f);
		Vector3 rndDir = Vector3( fwRandom::GetRandomNumberInRange(0.f,1.f),fwRandom::GetRandomNumberInRange(0.f,1.f),0.f);
		Vector3 groundHitPosition = pPlayer->GetGroundPos() + rndDir * rndDist;
#if __BANK
		if(sm_DrawResultPosition)
		{
			grcDebugDraw::Line(groundHitPosition,groundHitPosition + Vector3(1.f,0.f,0.f),Color_red,1000);
			grcDebugDraw::Line(groundHitPosition,groundHitPosition + Vector3(0.f,1.f,0.f),Color_green,1000);
			grcDebugDraw::Line(groundHitPosition,groundHitPosition + Vector3(0.f,0.f,1.f),Color_blue,1000);
		}
#endif
		return groundHitPosition;
	}
	return Vector3(0.f,0.f,0.f);
}
//-------------------------------------------------------------------------------------------------------------------
s32 audGunFightConductorFakeScenes::GetLightIdx(eHierarchyId nLightId)
{
	s32 lightIdx = -1;
	switch (nLightId)
	{
	case VEH_HEADLIGHT_L: 
		lightIdx = 0;
		break;
	case VEH_HEADLIGHT_R: 
		lightIdx = 1;
		break;
	case VEH_TAILLIGHT_L: 
		lightIdx = 2;
		break;
	case VEH_TAILLIGHT_R: 
		lightIdx = 3;
		break;
	case VEH_INDICATOR_LF: 
		lightIdx = 4;
		break;
	case VEH_INDICATOR_RF: 
		lightIdx = 5;
		break;
	case VEH_INDICATOR_LR: 
		lightIdx = 6;
		break;
	case VEH_INDICATOR_RR: 
		lightIdx = 7;
		break;
	case VEH_BRAKELIGHT_L: 
		lightIdx = 8;
		break;
	case VEH_BRAKELIGHT_R: 
		lightIdx = 9;
		break;
	case VEH_BRAKELIGHT_M: 
		lightIdx = 10;
		break;
	case VEH_REVERSINGLIGHT_L: 
		lightIdx = 11;
		break;
	case VEH_REVERSINGLIGHT_R: 
		lightIdx = 12;
		break;
	default: 
		break;
	}
	return lightIdx;
}
//-------------------------------------------------------------------------------------------------------------------
audGunFightConductorFakeScenes::audFakedImpactsEntity audGunFightConductorFakeScenes::AnalizeFakedImpactsHistory()
{
	s32 objectTimesPlayed  = m_FakedImpactsHistory[AUD_OBJECT];
	s32 vehicleTimesPlayed = m_FakedImpactsHistory[AUD_VEHICLE];
	// Check we haven't been playing vehicles for so long.
	if( vehicleTimesPlayed >= (sm_CarPriorityOverObjects * objectTimesPlayed ))
	{
		return AUD_OBJECT;
	}
	// If it's been a while without playing an object, try to do it but prioritize the vehicles.
	if( vehicleTimesPlayed >= (objectTimesPlayed + sm_CarPriorityOverObjects ))
	{
		s32 r = fwRandom::GetRandomNumberInRange(0,100);
		if(r > sm_FakedEntityToPlayProbThreshold)
		{
			return AUD_OBJECT;
		}
		else
		{
			return AUD_VEHICLE;
		}
	}
	// Otherwise fake vehicle.
	return AUD_VEHICLE;
}
//-------------------------------------------------------------------------------------------------------------------
CObject* audGunFightConductorFakeScenes::GetClosestValidObject()
{ 
	// TODO: Need a second pass to check which objects we really want.  At the moment we don't count trees but we do count a small bottle.
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Can't find local player."))
	{
		CPedIntelligence *pPedIntelligence = pPlayer->GetPedIntelligence();
		if(pPedIntelligence)
		{
			const CEntityScannerIterator objectList = pPedIntelligence->GetNearbyObjects();
			if(objectList.GetCount() > 0)
			{
				for(const CEntity* pEnt = objectList.GetFirst(); pEnt; pEnt = objectList.GetNext())
				{
					if(pEnt)
					{
						CObject *pObj = (CObject *)pEnt;
						if(pObj->GetIsAttached())
						{
							continue;
						}
						return pObj;
					}
				}
			}

		}
	}
	return NULL;
}
//-------------------------------------------------------------------------------------------------------------------
bool audGunFightConductorFakeScenes::ComputeLastShotTimeAndFakedBulletImpactType(u32 timeSinceLastShot)
{
#if __BANK
	if(sm_FakeBulletImpacts)
	{
		m_typeOfFakedBulletImpacts = (typeOfFakedBulletImpacts)(sm_FakeBulletImpactType -1);
		return true;
	}
#endif
	// Analyze the last fire event played to fake bullet impacts.
	CEntity *parentEntity = g_WeaponAudioEntity.GetLastWeaponFireEventPlayedAgainstPlayer().fireInfo.parentEntity;
	if(parentEntity && parentEntity->GetIsTypePed())
	{
		const CPedWeaponManager* weaponManager = ((CPed*)parentEntity)->GetWeaponManager();
		if (weaponManager) 
		{
			const CWeapon* weapon = weaponManager->GetEquippedWeapon();
			if (weapon) 
			{
				const CWeaponInfo* weaponInfo = weapon->GetWeaponInfo();
				if (weaponInfo) 
				{
					eWeaponEffectGroup effectGroup = weaponInfo->GetEffectGroup();
					// Pistols, just play one bullet impact.
					if(effectGroup >= WEAPON_EFFECT_GROUP_PISTOL_SMALL && effectGroup <= WEAPON_EFFECT_GROUP_PISTOL_SILENCED)
					{
						m_typeOfFakedBulletImpacts = Single_Impact;
					}
					// Automatic guns, play a burst of bullet impacts.
					else if (effectGroup == WEAPON_EFFECT_GROUP_SMG || effectGroup == WEAPON_EFFECT_GROUP_RIFLE_ASSAULT)
					{
						m_typeOfFakedBulletImpacts = Burst_Of_Impacts;
					}
					// Sniper rifle, Added if we want to play a different type of bullet impact.
					else if (effectGroup == WEAPON_EFFECT_GROUP_RIFLE_SNIPER)
					{
						m_typeOfFakedBulletImpacts = Hard_Impact;
					}
					// Shotgun, I think is a good idea to play like 5 or 6 bullet impacts at the same time.
					else if (effectGroup == WEAPON_EFFECT_GROUP_SHOTGUN)
					{
						m_typeOfFakedBulletImpacts = Bunch_Of_Impacts;
					}
					// Otherwise, don't fake bullet impacts.
					else 
					{
						m_typeOfFakedBulletImpacts = Invalid_Impact;
						return false; 
					}
					u32 deltaTime = fwTimer::GetTimeInMilliseconds() - g_WeaponAudioEntity.GetLastTimeEnemyNearbyFiredAGun();
					if(deltaTime <= timeSinceLastShot)
					{
						return true;
					}
				}
			}
		}
	}
	m_typeOfFakedBulletImpacts = Invalid_Impact;
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
CollisionMaterialSettings* audGunFightConductorFakeScenes::GetGroundMaterial() const
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Can't find local player."))
	{ 
		audPedAudioEntity* pedAudEntity = pPlayer->GetPedAudioEntity();
		if (pedAudEntity)
		{
			return pedAudEntity->GetFootStepAudio().GetCurrentMaterialSettings();
		}
	}
	return NULL;
}
//-------------------------------------------------------------------------------------------------------------------
CollisionMaterialSettings* audGunFightConductorFakeScenes::GetMaterialBehind() const
{
	if(!m_ObjectToFake)
	{
		naAssertf(0,"Null object to fake when getting the material behind.");
		return NULL;
	}
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Can't find local player."))
	{
		phIntersection testIntersections[AUD_NUM_INTERSECTIONS_FOR_PED_PROBES];
		phSegment testSegment;
		const Matrix34 &rootMatrix = MAT34V_TO_MATRIX34(pPlayer->GetTransform().GetMatrix()); 
		Vector3 vecStart = rootMatrix.d;
		Vector3 vecEnd = VEC3V_TO_VECTOR3(m_ObjectToFake->GetTransform().GetPosition());
		vecEnd.SetZ(vecStart.GetZ());
		Vector3 dir = vecStart - vecEnd;
		dir.NormalizeSafe(dir);
		vecEnd = vecStart + (dir * sm_MaterialTestLenght);

		testSegment.Set(vecStart, vecEnd);
		u32 nFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER |	ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_PED_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE;
		int nNumResults = CPhysics::GetLevel()->TestProbe(testSegment, testIntersections, pPlayer->GetCurrentPhysicsInst(), nFlags, TYPE_FLAGS_ALL, phLevelBase::STATE_FLAGS_ALL, AUD_NUM_INTERSECTIONS_FOR_PED_PROBES);

		int nUseIntersection = -1;
		for(int i=0; i<nNumResults; i++)
		{

			if(testIntersections[i].IsAHit())
			{
				CEntity* pHitEntity = CPhysics::GetEntityFromInst(testIntersections[i].GetInstance());
				if(pHitEntity && pHitEntity->IsCollisionEnabled())
				{
					// check we've got the first intersection along the probe
					if(nUseIntersection < 0 || VEC3V_TO_VECTOR3(testIntersections[i].GetPosition()).GetZ() > VEC3V_TO_VECTOR3(testIntersections[nUseIntersection].GetPosition()).GetZ())
					{
						nUseIntersection = i;
					}
				} 
			}
			else
				break;
		}
		if(nUseIntersection > -1)
		{
			if(testIntersections[nUseIntersection].GetInstance() && CPhysics::GetEntityFromInst(testIntersections[nUseIntersection].GetInstance())
				&& CPhysics::GetEntityFromInst(testIntersections[nUseIntersection].GetInstance())->GetIsPhysical()
				&& ((CPhysical*)CPhysics::GetEntityFromInst(testIntersections[nUseIntersection].GetInstance()))->GetAttachParent()==pPlayer)
			{
				//Assertf(false, "Ped is trying to stand on something he's attached to");
				return NULL;
			}
			phMaterialMgr::Id	ProbeHitMaterial=0;
			ProbeHitMaterial = testIntersections[nUseIntersection].GetMaterialId();
			CollisionMaterialSettings *material = NULL;
			CEntity *entity = CPhysics::GetEntityFromInst(testIntersections[nUseIntersection].GetInstance());
			//This includes a call to GetMaterialOverride so no need to worry our pretty little heads about calling it seperately
			material = g_CollisionAudioEntity.GetMaterialSettingsFromMatId(entity, ProbeHitMaterial, testIntersections[nUseIntersection].GetComponent());
			// fall back to default if we couldnt find anything...
			return material;
		}	
	}
	return NULL;
}
//-------------------------------------------------------------------------------------------------------------------
//PURPOSE
//Supports audDynamicEntitySound queries
u32 audGunFightConductorFakeScenes::QuerySoundNameFromObjectAndField(const u32 *objectRefs, u32 numRefs)
{
	naAssertf(objectRefs, "A null object ref array ptr has been passed into audBoatAudioEntity::QuerySoundNameFromObjectAndField");
	naAssertf(numRefs >= 2,"The object ref list should always have at least two entries when entering QuerySoundNameFromObjectAndField");
	if(numRefs ==2)
	{
		//We're at the end of the object ref chain, see if we have a valid object to query
		if(*objectRefs == ATSTRINGHASH("BULLET_IMPACT_ENTITY_MATERIAL", 0x9D923DF4))
		{
			if(m_CurrentFakedCollisionMaterial)
			{
				++objectRefs;
				//TODO: Once Col check the collision audio entity enable it to play the resonance sound.
				//if(collisionMaterial->BulletCollisionScaling > 0)
				//{
				//	g_CollisionAudioEntity.TriggerFakedBulletImpact(static_cast<CObject*>(object),collisionMaterial->BulletCollisionScaling,collisionMaterial);
				//}		
				u32 *ret ((u32 *)m_CurrentFakedCollisionMaterial->GetFieldPtr(*objectRefs));
#if __BANK
				if(sm_DisplayDynamicSoundsPlayed)
				{
					naDisplayf("ENTITY MATERIAL HASH: %d,NAME : %s ",(ret) ? *ret : 0,SOUNDFACTORY.GetMetadataManager().GetObjectName((ret) ? *ret : 0));
				}
#endif
				return (ret) ? *ret : 0;
			}
		}
		else if(*objectRefs == ATSTRINGHASH("BULLET_IMPACT_ENTITY_BEHIND_MATERIAL", 0x8F1D59C5))
		{
			CollisionMaterialSettings* materialBehind = GetMaterialBehind();
			if(materialBehind)
			{
				++objectRefs;
				u32 *ret ((u32 *)materialBehind->GetFieldPtr(*objectRefs));
#if __BANK
				if(sm_DisplayDynamicSoundsPlayed)
				{
					naDisplayf("MATERIAL BEHIND HASH: %d,NAME : %s ",(ret) ? *ret : 0,SOUNDFACTORY.GetMetadataManager().GetObjectName((ret) ? *ret : 0));
				}
#endif
				return (ret) ? *ret : 0;
			}else if(m_CurrentFakedCollisionMaterial)
			{
				u32 *ret ((u32 *)m_CurrentFakedCollisionMaterial->GetFieldPtr(*objectRefs));
#if __BANK
				if(sm_DisplayDynamicSoundsPlayed)
				{
					naDisplayf("Didn't find a collision behind, defaulting to ENTITY MATERIAL HASH: %d,NAME : %s ",(ret) ? *ret : 0,SOUNDFACTORY.GetMetadataManager().GetObjectName((ret) ? *ret : 0));
				}
#endif
				return (ret) ? *ret : 0;
			}
		}
		else if(*objectRefs == ATSTRINGHASH("BULLET_IMPACT_GROUND_MATERIAL", 0x32590C6A))
		{
			CollisionMaterialSettings* groundMaterial = GetGroundMaterial();
			if(groundMaterial)
			{
				++objectRefs;
				u32 *ret ((u32 *)groundMaterial->GetFieldPtr(*objectRefs));
#if __BANK
				if(sm_DisplayDynamicSoundsPlayed)
				{
					naDisplayf("GROUND MATERIAL HASH: %d,NAME : %s ",(ret) ? *ret : 0,SOUNDFACTORY.GetMetadataManager().GetObjectName((ret) ? *ret : 0));
				}
#endif
				return (ret) ? *ret : 0;
			}
		}
		else
		{
			naWarningf("Attempting to query an object field in gunfightConductor but object is not a supported type.  Object Ref %p",objectRefs);
		}
	}
	return 0; 
}
//-------------------------------------------------------------------------------------------------------------------
//													VFX
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::UpdateFakedVehicleVfx(bool runningAwayScene )
{
	naAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE),"fake conductor calls executing not in the main thread");
	// If we have a vehicle near
	if(naVerifyf(m_VehicleToFake,"NULL fake vehicle pointer on conductors."))
	{
		if(m_VehicleToFake->m_nPhysicalFlags.bNotDamagedByAnything || m_VehicleToFake->m_nPhysicalFlags.bNotDamagedByBullets)
		{
			return;
		}
		CPed* pPlayer = CGameWorld::FindLocalPlayer();
		if (naVerifyf(pPlayer,"Can't find local player."))
		{
			// First check if the car got a siren and if it's time to play the siren vfx do it.
			if(!FakeVehicleSmashSirens())
			{
				// If not go for the rest of vfx.
				FakeVehicleVfx(runningAwayScene);
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::FakeVehicleVfx(bool runningAwayScene )
{
	naAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE),"fake conductor calls executing not in the main thread");
	// If we have a vehicle near
	if(naVerifyf(m_VehicleToFake,"NULL fake vehicle pointer on conductors."))
	{
		CPed* pPlayer = CGameWorld::FindLocalPlayer();
		if (naVerifyf(pPlayer,"Can't find local player."))
		{
			// Based on the player front and car front, choose which element to smash.
			// Vector3 carPos = VEC3V_TO_VECTOR3(m_VehicleToFake->GetTransform().GetPosition());
			Vector3 carFront = VEC3V_TO_VECTOR3(m_VehicleToFake->GetTransform().GetB());
			Vector3 carRight = VEC3V_TO_VECTOR3(m_VehicleToFake->GetTransform().GetA());
			Vector3 dirToPlayer = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition() - m_VehicleToFake->GetTransform().GetPosition());
			carFront.NormalizeSafe();
			carRight.NormalizeSafe();
			dirToPlayer.NormalizeSafe();
			f32 frontAngle = carFront.Dot(dirToPlayer);
			f32 rightAngle = carRight.Dot(dirToPlayer);

			GunfightConductorIntensitySettings *intesitySettings = GetGunfightConductroIntensitySettings();
			if(audEngineUtil::ResolveProbability(intesitySettings->RunningAway.VfxProbability))
			{
				// Right side of the car.
				if(rightAngle >= 0.f)
				{
					if(frontAngle >= 0.9f)
					{
						//Front lights.
						FakeVehicleSmashLights(VEH_HEADLIGHT_R);
					}else if (frontAngle >= 0.5f && !runningAwayScene)
					{
						//Front wheels.
						if(!m_VehicleToFake->m_nVehicleFlags.bTyresDontBurst)
							FakeVehicleTyreBurst(VEH_WHEEL_RF);
					}else if (frontAngle >= 0.f && !runningAwayScene)
					{
						//Front windows.
						FakeVehicleSmashWindow(VEH_WINDOW_RF);
					}else if(frontAngle <= -0.9f)
					{
						//Rear lights.
						FakeVehicleSmashLights(VEH_TAILLIGHT_R);
					}else if (frontAngle <= -0.5f)
					{
						//Rear wheels.
						if(!m_VehicleToFake->m_nVehicleFlags.bTyresDontBurst  && !runningAwayScene)
							FakeVehicleTyreBurst(VEH_WHEEL_RR);
					}else if (!runningAwayScene)
					{
						//Rear windows.
						FakeVehicleSmashWindow(VEH_WINDOW_RR);
					}
				}else // Left side of the car
				{
					if(frontAngle >= 0.9f  )
					{
						//Front lights.
						FakeVehicleSmashLights(VEH_HEADLIGHT_L);
					}else if (frontAngle >= 0.5f  && !runningAwayScene)
					{
						//Front wheels.
						if(!m_VehicleToFake->m_nVehicleFlags.bTyresDontBurst)
							FakeVehicleTyreBurst(VEH_WHEEL_LF);
					}else if (frontAngle >= 0.f  && !runningAwayScene)
					{
						//Front windows.
						FakeVehicleSmashWindow(VEH_WINDOW_LF);
					}else if(frontAngle <= -0.9f )
					{
						//Rear lights.
						FakeVehicleSmashLights(VEH_TAILLIGHT_L);
					}else if (frontAngle <= -0.5f  && !runningAwayScene)
					{
						//Rear wheels.
						if(!m_VehicleToFake->m_nVehicleFlags.bTyresDontBurst)
							FakeVehicleTyreBurst(VEH_WHEEL_LR);
					}else if (!runningAwayScene)
					{
						//Rear windows.
						FakeVehicleSmashWindow(VEH_WINDOW_LR);
					}
				}
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::FakeVehicleSmashWindow(eHierarchyId nWindowId)
{
	naAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE),"fake conductor calls executing not in the main thread");
	if(naVerifyf(m_VehicleToFake,"NULL fake vehicle pointer on conductors."))
	{
		s32 boneIndex = m_VehicleToFake->GetBoneIndex(nWindowId);
		if (boneIndex>-1)
		{
			s32 iWindowComponent = m_VehicleToFake->GetFragInst()->GetComponentFromBoneIndex(boneIndex);
			if(iWindowComponent != -1)
			{
				const Vec3V vSmashNormal = m_VehicleToFake->GetWindowNormalForSmash(nWindowId);
				g_vehicleGlassMan.SmashCollision(m_VehicleToFake, iWindowComponent, VEHICLEGLASSFORCE_WEAPON_IMPACT, vSmashNormal);
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::FakeVehicleSmashLights(eHierarchyId nLightId)
{
	naAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE),"fake conductor calls executing not in the main thread");
	if(naVerifyf(m_VehicleToFake,"NULL fake vehicle pointer on conductors."))
	{
		if( !m_VehicleToFake->m_nVehicleFlags.bUnbreakableLights )
		{ // don't break unbreakable lights...
			s32 lightIdx = GetLightIdx(nLightId);
			if(nLightId != VEH_INVALID_ID)
			{
				const int boneIdx = m_VehicleToFake->GetBoneIndex(nLightId);
				const bool lightState = m_VehicleToFake->GetVehicleDamage()->GetLightStateImmediate(lightIdx);
				if( boneIdx != -1 && (false == lightState))
				{
					// get the light position in world space
					Matrix34 worldLightMat;
					m_VehicleToFake->GetGlobalMtx(boneIdx, worldLightMat);
					// it is - smash the light
					Vector3 localLightPos = VEC3V_TO_VECTOR3(m_VehicleToFake->GetTransform().UnTransform(VECTOR3_TO_VEC3V(worldLightMat.d)));

					// find a suitable normal
					Vector3 localNormal(0.0f, 0.0f, 1.0f);
					localNormal = localLightPos;
					localNormal.Normalize();
					// check that the normal is valid
					Assertf(localLightPos.x!=0.0f && localLightPos.y!=0.0f && localLightPos.z!=0.0f, "Light position is not valid on %s %.4f,%.4f,%.4f for light %d,%d", m_VehicleToFake->GetModelName(), localLightPos.x, localLightPos.y, localLightPos.z,boneIdx,lightIdx);
					Assertf(localNormal.Mag()>0.997f && localNormal.Mag()<1.003f, "Normal is not valid on %s %.4f,%.4f,%.4f for light %d,%d", m_VehicleToFake->GetModelName(), localNormal.x, localNormal.y, localNormal.z,boneIdx,lightIdx);

					g_vfxVehicle.TriggerPtFxLightSmash(m_VehicleToFake, nLightId, RCC_VEC3V(localLightPos), RCC_VEC3V(localNormal));
					m_VehicleToFake->GetVehicleAudioEntity()->TriggerHeadlightSmash(nLightId,localLightPos);
					m_VehicleToFake->GetVehicleDamage()->SetLightStateImmediate(lightIdx,true);
					m_VehicleToFake->GetVehicleDamage()->SetUpdateLightBones(true); 
				}
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
bool audGunFightConductorFakeScenes::FakeVehicleSmashSirens()
{
	naAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE),"fake conductor calls executing not in the main thread");
	if(sm_TimeSinceLastSirenSmashed >= sm_TimeToSmashSirens)
	{
		CPed* pPlayer = CGameWorld::FindLocalPlayer();
		if (naVerifyf(pPlayer,"Can't find local player."))
		{
			CPedIntelligence *pPedIntelligence = pPlayer->GetPedIntelligence();
			if(pPedIntelligence) 
			{
				//Get closest object.
				CVehicle* pVehicle = pPedIntelligence->GetClosestVehicleInRange();
				if(pVehicle)
				{
					if( pVehicle->UsesSiren() && !pVehicle->m_nVehicleFlags.bUnbreakableLights)
					{
						// Siren
						for(int i=VEH_SIREN_1; i<VEH_SIREN_3+1; i++)
						{
							if(audEngineUtil::ResolveProbability(fwRandom::GetRandomNumberInRange(0.f,1.f)))
							{
								const bool sirenState = pVehicle->GetVehicleDamage()->GetSirenState((eHierarchyId)i);
								const int boneIdx = pVehicle->GetBoneIndex((eHierarchyId)i);

								if( (boneIdx != -1) && (false == sirenState) ) 
								{
									Vector3 vertPos;
									pVehicle->GetDefaultBonePositionSimple(boneIdx,vertPos);
									if( (vertPos.Mag2() > 0.0f))
									{
										pVehicle->GetVehicleDamage()->SetUpdateSirenBones(true);
										pVehicle->GetVehicleDamage()->SetSirenState((eHierarchyId)i,true);

										// No Fxs
										// pop/break the glass casing
										const int boneGlassID = pVehicle->GetBoneIndex((eHierarchyId)(i + (VEH_SIREN_GLASS_1-VEH_SIREN_1)));
										const int component = pVehicle->GetVehicleFragInst()->GetComponentFromBoneIndex(boneGlassID);
										if( component != -1 )
										{
											Vec3V vSmashNormal(V_Z_AXIS_WZERO);
											g_vehicleGlassMan.SmashCollision(pVehicle,component, VEHICLEGLASSFORCE_SIREN_SMASH, vSmashNormal);
											pVehicle->GetVehicleAudioEntity()->SmashSiren();
											sm_TimeSinceLastSirenSmashed = 0;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::FakeVehicleTyreBurst(eHierarchyId nWheelId)
{ 
	naAssertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE),"fake conductor calls executing not in the main thread");
	if(naVerifyf(m_VehicleToFake,"NULL fake vehicle pointer on conductors."))
	{
		if( m_VehicleToFake->GetWheelFromId(nWheelId))
		{
			m_VehicleToFake->GetWheelFromId(nWheelId)->ApplyTyreDamage(NULL, 25.0, Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), DAMAGE_TYPE_BULLET, 0);
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::PlayFakeRicochets(CEntity *hitEntity,const Vector3 &hitPosition)
{ 
	naAssertf(FPIsFinite(hitPosition.Mag()), "Invalid hit position: [%f,%f,%f]", hitPosition.x, hitPosition.y, hitPosition.z);
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Can't find local player.") && hitEntity)
	{
		// Check player's distance to the impact. 
		Vector3 distanceToImpact = hitPosition - VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
		if(distanceToImpact.Mag2() < sm_MinDistanceToFakeRicochets)
		{
			return;
		}
		// Check if the player is shooting an automatic weapon.
		const CPedWeaponManager* weaponManager = pPlayer->GetWeaponManager();
		if (weaponManager) 
		{
			const CWeapon* weapon = weaponManager->GetEquippedWeapon();
			if (weapon) 
			{
				const CWeaponInfo* weaponInfo = weapon->GetWeaponInfo();
				if (weaponInfo) 
				{
					eWeaponEffectGroup effectGroup = weaponInfo->GetEffectGroup();
					// Automatic guns.
					if (effectGroup != WEAPON_EFFECT_GROUP_SMG && effectGroup != WEAPON_EFFECT_GROUP_RIFLE_ASSAULT)
					{
						return;
					}
				}
			}
		}
		//Flag to start updating.
		if(!m_HasToPlayFakeRicochets)
		{
			m_HasToPlayFakeRicochets = true;
			m_Candidate = hitEntity;
			m_CandidateHitPosition = hitPosition;
			m_TimeSinceLastCandidateHit = 0;
			//m_TimeFiring = 0;
		}
		else
		{
			// Check if the entity hit is the candidate
			if(hitEntity == m_Candidate)
			{
				m_TimeSinceLastCandidateHit = 0;
				m_CandidateHitPosition = hitPosition;
			}
			else if (m_TimeSinceLastCandidateHit > sm_MaxTimeToFakeRicochets) 
			{
				m_Candidate = hitEntity;
				m_CandidateHitPosition = hitPosition;
				m_TimeSinceLastCandidateHit = 0;
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::PlayPostShootOutVehDebris(CEntity *hitEntity,const Vector3 &hitPosition)
{ 
	naAssertf(FPIsFinite(hitPosition.Mag()), "Invalid hit position: [%f,%f,%f]", hitPosition.x, hitPosition.y, hitPosition.z);
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Can't find local player.") && hitEntity)
	{
		if(m_HasToPlayPostShootOutVehDebris && m_PostShootOutVehCandidate && m_PostShootOutVehCandidate->GetVehicleAudioEntity())
		{
			const CVehicle *hitVehicle = static_cast<const CVehicle*>(hitEntity);
			// Check if the entity hit is the candidate
			if(hitVehicle->GetVehicleAudioEntity() &&( hitVehicle->GetVehicleAudioEntity()->GetBulletHitCount() >= m_PostShootOutVehCandidate->GetVehicleAudioEntity()->GetBulletHitCount()))
			{
				m_PostShootOutVehCandidate = static_cast<CVehicle*>(hitEntity);
				m_PostShootOutVehCandidateHitPosition = hitPosition;
			}
		}
		else
		{
			//Flag to start updating.
			m_HasToPlayPostShootOutVehDebris = true;
			m_PostShootOutVehCandidate = static_cast<CVehicle*>(hitEntity);
			m_PostShootOutVehCandidateHitPosition = hitPosition;
			//m_TimeSinceLastCandidateHit = 0;
			//m_TimeFiring = 0;
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::PlayPostShootOutBuildingDebris(CEntity *hitEntity,const Vector3 &hitPosition)
{ 
	naAssertf(FPIsFinite(hitPosition.Mag()), "Invalid hit position: [%f,%f,%f]", hitPosition.x, hitPosition.y, hitPosition.z);
	s32 freeIndex = -1;
	s32 currentIndex = -1;
	// First of all update the hit count and get the candidate
	for(u32 i = 0; i<AUD_MAX_POST_SHOOT_OUT_BUILDINGS; i++)
	{
		if(hitEntity == m_PostShootOutBuilding[i].building)
		{
			m_PostShootOutBuilding[i].bulletHitCount ++;
			m_PostShootOutBuilding[i].timeSinceLastBulletHit = 0;
			currentIndex = i;
			break;
		}
		else if( freeIndex == -1 && !m_PostShootOutBuilding[i].building )
		{
			freeIndex = i;
		}
	}
	if(currentIndex == -1 && freeIndex != -1)
	{
		naAssertf(freeIndex < AUD_MAX_POST_SHOOT_OUT_BUILDINGS, "Out of bounds");
		m_PostShootOutBuilding[freeIndex].building = hitEntity;
		m_PostShootOutBuilding[freeIndex].bulletHitCount = 1;
		m_PostShootOutBuilding[freeIndex].timeSinceLastBulletHit = 0;
		currentIndex = freeIndex;
	}
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Can't find local player.") && hitEntity)
	{
		//Flag to start updating.
		if(!m_HasToPlayPostShootOutBuildingDebris)
		{
			m_HasToPlayPostShootOutBuildingDebris = true;
			m_PostShootOutBuildingCandidate.building = hitEntity;
			m_PostShootOutBuildingCandidate.bulletHitCount = 1;
			m_PostShootOutBuildingCandidate.timeSinceLastBulletHit = 0;
			m_PostShootOutBuildingCandidateHitPosition = hitPosition;
		}
		else
		{
			naAssertf(currentIndex < AUD_MAX_POST_SHOOT_OUT_BUILDINGS, "Out of bounds");
			if(currentIndex != -1)
			{
				// Check if the entity hit is the candidate
				if(m_PostShootOutBuilding[currentIndex].bulletHitCount >= m_PostShootOutBuildingCandidate.bulletHitCount)
				{
					m_PostShootOutBuildingCandidate.building = m_PostShootOutBuilding[currentIndex].building;
					m_PostShootOutBuildingCandidate.bulletHitCount = m_PostShootOutBuilding[currentIndex].bulletHitCount;
					m_PostShootOutBuildingCandidate.timeSinceLastBulletHit = m_PostShootOutBuilding[currentIndex].timeSinceLastBulletHit;
					m_PostShootOutBuildingCandidateHitPosition = hitPosition;
				}
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
#if __BANK
const char* audGunFightConductorFakeScenes::GetStateName(s32 iState) const
{
	taskAssert(iState >= State_Idle && iState <= State_EmphasizeOpenSpaceScene);
	switch (iState)
	{
	case State_Idle:							return "State_Idle";
	case State_EmphasizePlayerIntoCoverScene:	return "State_EmphasizePlayerIntoCoverScene";
	case State_EmphasizeRunAwayScene:			return "State_EmphasizeRunAwayScene";
	case State_EmphasizeOpenSpaceScene:			return "State_EmphasizeOpenSpaceScene";
	default: taskAssert(0); 	
	}
	return "State_Invalid";
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::GunfightConductorFSDebugInfo() 
{
	if(audGunFightConductor::GetShowInfo())
	{
		char text[64];
		formatf(text,"GunfightConductor Faked Scenes -> %s", GetStateName(GetState()));
		grcDebugDraw::AddDebugOutput(text);
		formatf(text,"Time in state -> %f", GetTimeInState());
		grcDebugDraw::AddDebugOutput(text);
		u32 deltaTime = fwTimer::GetTimeInMilliseconds() - g_WeaponAudioEntity.GetLastTimeEnemyNearbyFiredAGun();
		formatf(text,"Time since last shot-> %d", deltaTime);
		grcDebugDraw::AddDebugOutput(text);
		formatf(text,"Last Shot-> %d", m_typeOfFakedBulletImpacts);
		grcDebugDraw::AddDebugOutput(text);
	}
	if(sm_DrawNearByDetectionSphere)
	{
		CPed* pPlayer = CGameWorld::FindLocalPlayer();
		if (naVerifyf(pPlayer,"Can't find local player."))
		{
			CPedIntelligence *pPedIntelligence = pPlayer->GetPedIntelligence();
			if(pPedIntelligence)
			{
				//grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()), sm_RSphereNearByEntities,Color32(1.f,1.f,0.f,0.5f));
				if(m_VehicleToFake)
					grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_VehicleToFake->GetTransform().GetPosition()), 2.f,Color32(0.f,0.f,1.f,0.5f));
				if(m_ObjectToFake)
					grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_ObjectToFake->GetTransform().GetPosition()), 2.f,Color32(1.f,0.f,0.f,0.5f));
				CDoor* pDoorsList = pPedIntelligence->GetClosestDoorInRange();
				if(pDoorsList)
					grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pDoorsList->GetTransform().GetPosition()), 2.f,Color32(0.f,1.f,0.f,0.5f));
			}
		}
		s32 objectTimesPlayed  = m_FakedImpactsHistory[AUD_OBJECT];
		s32 vehicleTimesPlayed = m_FakedImpactsHistory[AUD_VEHICLE];
		char txt[128];
		formatf(txt,"Fake impact history [VEH %d] [OBJ %d] ",vehicleTimesPlayed,objectTimesPlayed);
		grcDebugDraw::AddDebugOutput(Color_white,txt);
	}
	if(sm_DrawMaterialBehindTest)
	{
		CPed* pPlayer = CGameWorld::FindLocalPlayer();
		if (naVerifyf(pPlayer,"Can't find local player."))
		{ 
			if(m_ObjectToFake)
			{
				const Matrix34 &rootMatrix = MAT34V_TO_MATRIX34(pPlayer->GetTransform().GetMatrix()); 
				Vector3 vecStart = rootMatrix.d;
				Vector3 vecEnd = VEC3V_TO_VECTOR3(m_ObjectToFake->GetTransform().GetPosition());
				vecEnd.SetZ(vecStart.GetZ());
				Vector3 dir = vecStart - vecEnd;
				dir.NormalizeSafe(dir);
				vecEnd = vecStart + (dir * sm_MaterialTestLenght);
				grcDebugDraw::Line(vecStart,vecEnd, Color_red,1);
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audGunFightConductorFakeScenes::AddWidgets(bkBank &bank){
	bank.PushGroup("GunFight Conductor Fake Scenes",false);
		bank.AddToggle("Fake bulletImpacts", &sm_FakeBulletImpacts);
		bank.AddCombo("Impact debug type",&sm_FakeBulletImpactType,5,&sm_FakeBulletImpactTypes[0],0,NullCB,"Select the Fake type of bullet impact");
		bank.PushGroup("Going into cover",false);
			bank.AddToggle("Show Dynamic sounds played", &sm_DisplayDynamicSoundsPlayed);
			bank.AddToggle("Draw Material test", &sm_DrawMaterialBehindTest);
			bank.AddSlider("Material test length", &sm_MaterialTestLenght, 0.0f, 10.0f, 0.5f);
		bank.PopGroup();
		bank.PushGroup("Running away",false);
			bank.AddToggle("Script allows it", &sm_AllowRunningAwayScene);
			bank.AddToggle("Force scene", &sm_ForceRunAwayScene);
			bank.AddToggle("Don't check object offscreen ", &sm_DontCheckOffScreen);
			bank.AddToggle("Draw debug sphere", &sm_DrawNearByDetectionSphere);
			bank.AddSlider("Sphere radius to detect nearby entities", &sm_RSphereNearByEntities, 0.0f, 50.0f, 0.5f);
			bank.AddSlider("Life percentage threshold to fake running away scene", &sm_LifeThresholdToFakeRunAwayScene, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Delta time to smash sirens", &sm_TimeToSmashSirens, 0, 50000, 5000);
		bank.PopGroup();
		bank.PushGroup("Open space",false);
			bank.AddToggle("Force scene", &sm_ForceOpenSpaceScene);
			bank.AddToggle("Allow open scene when running away ", &sm_AllowOpenSpaceWhenRunningAway);
			bank.AddToggle("Draw fake impact position", &sm_DrawResultPosition);
		bank.PopGroup();
		bank.PushGroup("Ricochets",false);
			bank.AddToggle("Draw Riccochetes position", &sm_ShowFakeRiccoPos);
			bank.AddSlider("Min distance to fake ricochets", &sm_MinDistanceToFakeRicochets, 0.f, 50000.f, 100.f);
			bank.AddSlider("Max time to fake ricochets", &sm_MaxTimeToFakeRicochets, 0, 500, 10);
			bank.AddSlider("Min time firing to fake ricochets", &sm_MinTimeFiringToFakeRicochets, 0, 5000, 10);
		bank.PopGroup();
		bank.PushGroup("Post-Shoot-Out Debris",false);
			bank.AddToggle("Draw Post-shootout debris position", &sm_ShowPostShootOutPos);
			bank.AddSlider("Time to reset bullet count (ms)", &sm_TimeToResetBulletCount, 0, 10000, 100);
			bank.PushGroup("Vehicles",false);
			bank.AddSlider("Min time not firing to trigger near light debris (ms)", &sm_MinTimeNotFiringToFakeNearLightVehDebris, 0, 10000, 100);
			bank.AddSlider("Min time not firing to trigger near debris (ms)", &sm_MinTimeNotFiringToFakeNearVehDebris, 0, 10000, 100);
			bank.AddSlider("Min time not firing to trigger far debris (ms)", &sm_MinTimeNotFiringToFakeFarVehDebris, 0, 10000, 100);
				bank.AddSlider("Num bullet hits to trigger debris", &sm_BulletHitsToTriggerVehicleDebris, 0, 10000, 1);
				bank.AddSlider("Sqd dist to trigger far version ", &sm_VehSqdDistanceToPlayFarDebris, 0.f, 10000.f, 1.f);
			bank.PopGroup();
			bank.PushGroup("Buildings");
			bank.AddSlider("Min time not firing to trigger near debris (ms)", &sm_MinTimeNotFiringToFakeNearBuildingDebris, 0, 10000, 100);
			bank.AddSlider("Min time not firing to trigger far debris  (ms)", &sm_MinTimeNotFiringToFakeFarBuildingDebris, 0, 10000, 100);
				bank.AddSlider("Dist threshold to check NPCs bullet hits", &sm_BuildDistThresholdToPlayPostShootOut, 0.f, 10000.f, 1.f);
					bank.AddSlider("Num bullet hits to trigger debris", &sm_BulletHitsToTriggerBuildingDebris, 0, 10000, 1);
				bank.AddSlider("Sqd dist to trigger far version ", &sm_BuildingSqdDistanceToPlayFarDebris, 0.f, 10000.f, 1.f);
				bank.PopGroup();
		bank.PopGroup();
		
	bank.PopGroup();
}
#endif


