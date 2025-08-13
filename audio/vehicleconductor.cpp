#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "debugaudio.h"
#include "grcore/debugdraw.h"

#include "audio/environment/environmentgroup.h"
#include "ai/EntityScanner.h"
#include "control/record.h"
#include "northaudioengine.h"
#include "network/NetworkInterface.h"
#include "frontendaudioentity.h"
#include "Peds/PedIntelligence.h"
#include "physics/WorldProbe/worldprobe.h"
#include "profile/profiler.h"
#include "scene/world/GameWorld.h"
#include "script/script_hud.h"
#include "scriptaudioentity.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "vehicleconductor.h"
#include "vehicleAi/task/TaskVehicleMissionBase.h"
#include "vehicles/vehicle.h"
#include "vehicleAi/VehicleIntelligence.h"

AUDIO_OPTIMISATIONS()
//-------------------------------------------------------------------------------------------------------------------
//														PROBES
//-------------------------------------------------------------------------------------------------------------------
bool audCachedCarLandingTestResults::NewTestComplete()
{
	for( s32 i = 0; i < NUMBER_OF_TEST; i++)
	{
		if((!m_MainLineTest[i].m_hShapeTest.GetResultsWaitingOrReady() || m_MainLineTest[i].m_bHasProbeResults) && !m_MainLineTest[i].m_bHasBeenProcessed) 
		{
			m_MainLineTest[i].m_bHasBeenProcessed = true;
			SUPERCONDUCTOR.GetVehicleConductor().GetCachedLineTest()->m_CurrentIndex = i;
			return true;
		}
	}
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
void audCachedCarLandingTestResults::ClearAll()
{
	m_HasFoundTheSolution = false;
	m_ResultPosition = Vector3(0.f,0.f,0.f);
	m_ResultNormal = Vector3(0.f,0.f,0.f);
	for( s32 i = 0; i < NUMBER_OF_TEST; i++)
	{
		m_MainLineTest[i].Clear();
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audCachedCarLandingTestResults::GetAllProbeResults()
{
	for( u32 i = 0; i < NUMBER_OF_TEST; i++)
	{
		m_MainLineTest[i].GetProbeResult();
	}
}
//-------------------------------------------------------------------------------------------------------------------
bool audCachedCarLandingTestResults::AreAllComplete()
{
	for( u32 i = 0; i < NUMBER_OF_TEST; i++)
	{
		if(m_MainLineTest[i].m_hShapeTest.GetResultsWaitingOrReady() && !m_MainLineTest[i].m_bHasProbeResults)
		{
			return false;
		}
	}
	return true;
}
//-------------------------------------------------------------------------------------------------------------------
//PF_PAGE(CarConductorAudioTimers,	"carConductor");
//PF_GROUP(carjumps); 
//PF_LINK(CarConductorAudioTimers, carjumps);
//PF_TIMER(LandingTestTimer,carjumps);

//-------------------------------------------------------------------------------------------------------------------
//														INIT
//-------------------------------------------------------------------------------------------------------------------
#if __BANK 
bool audVehicleConductor::sm_ShowFSMInfo = false; 
bool audVehicleConductor::sm_ShowSirensInfo = false; 
bool audVehicleConductor::sm_PreImpactSweeteners = false; 
bool audVehicleConductor::sm_DrawJumpResults = false; 
bool audVehicleConductor::sm_ShowJumpInfo = false; 
bool audVehicleConductor::sm_ShowTargets = false;
bool audVehicleConductor::sm_ShowNewTargets = false;
bool audVehicleConductor::sm_ShowVehSirenState = false; 
bool audVehicleConductor::sm_DrawTrajectory = false; 
bool audVehicleConductor::sm_DrawVelocity = false; 
bool audVehicleConductor::sm_ShowLandingInfo = false; 
bool audVehicleConductor::sm_PauseOnLandEvent = false; 
bool audVehicleConductor::sm_HasToPause = false; 
bool audVehicleConductor::sm_DrawMap = false;
#endif

bool audVehicleConductor::sm_PlaySirenWithNoNetworkPlayerDriver = false; 
bool audVehicleConductor::sm_PlaySirenWithNoDriver = false; 
bool audVehicleConductor::sm_EnableSirens = true; 
f32 audVehicleConductor::sm_DurationOfTest = 0.5f;
f32 audVehicleConductor::sm_LandTimeError = 0.f;
f32	audVehicleConductor::sm_DistanceThreshold = 10.f;
f32	audVehicleConductor::sm_VehMinFitnessThreshold = 0.4f;
f32	audVehicleConductor::sm_VehMaxFitnessThreshold = 0.6f;
f32	audVehicleConductor::sm_ThresholdTimeToLand = 0.5f;
f32	audVehicleConductor::sm_ThresholdFitnessToLand = 0.35f;
f32	audVehicleConductor::sm_ThresholdTimeToBadLand = 0.5f;
f32	audVehicleConductor::sm_ThresholdFitnessToBadLand = 0.05f;
f32	audVehicleConductor::sm_JumpConductorImpactThreshold = 5.f;
f32	audVehicleConductor::sm_DistanceTraveledThreshold = 0.8f;

audSoundSet audVehicleConductor::sm_MPStuntJumpSoundSet;

f32	audVehicleConductor::sm_TimeToMuteNonTargets = 2.f;
f32	audVehicleConductor::sm_TimeToUnMuteNonTargets = 0.1f;
f32	audVehicleConductor::sm_TimeToMuteVehMidZone = 2.f;
f32	audVehicleConductor::sm_TimeToUnMuteVehMidZone = 1.f;
f32	audVehicleConductor::sm_TimeToMuteVehFarZone = 7.f;

u32	audVehicleConductor::sm_TimeToUpdateTargets = 1000;
u32	audVehicleConductor::sm_TimeSinceLastUpdate = 0;
u32	audVehicleConductor::sm_TimeSinceFakeSirensWasPlayed = 0;
u32	audVehicleConductor::sm_TimeToStopFakeSirens = 5000;

u32 g_TimeInAirThreshold = 1500;
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductor::Init()
{
	m_NumberOfVehWithSirenOn = 0;
	for ( u32 i = 0; i < AUD_MAX_NUM_VEH_WITH_SIREN; i++ )
	{
		m_VehiclesWithSirenOn[i].pVeh = NULL;
		m_VehiclesWithSirenOn[i].area = audConductorMap::FurtherAwayArea;
		m_VehiclesWithSirenOn[i].fitness = -1.f;
	}

	m_NumberOfTargets = 0;
	m_NumberOfNewTargets = 0;
	m_LastNumVehInFarArea = 0;
	for ( s32 i = 0; i < MaxNumVehConductorsTargets; i++ )
	{
		m_VehicleTargets[i].pVeh = NULL;
		m_VehicleTargets[i].area = audConductorMap::FurtherAwayArea;
		m_VehicleTargets[i].fitness = -1.f;
		m_VehicleNewTargets[i].pVeh = NULL;
		m_VehicleNewTargets[i].area = audConductorMap::FurtherAwayArea;
		m_VehicleNewTargets[i].fitness = -1.f;
	}

	m_InitialVehVelocity =  Vector3(0.f,0.f,0.f);
	m_AvgDirectionToDistantSirens = Vector3(0.f,0.f,0.f);
	m_elapsedTime = 0;
	m_StuntJumpStarted = false;
	m_ShouldUpdateJump = false;
	m_ForceStuntJumpTimeToLandToZero = false;
	m_lastAngVelocity = 0.f;
	m_YawDiffToFitness.Init(ATSTRINGHASH("YAW_DIFF_TO_FITNESS", 0x22115EFF));
	m_PitchDiffToFitness.Init(ATSTRINGHASH("PITCH_DIFF_TO_FITNESS", 0x20850FA3));
	m_UpRightToFitness.Init(ATSTRINGHASH("UP_RIGHT_TO_FITNESS", 0x549FA418));
	m_ScaledVelocity.Init(ATSTRINGHASH("ANG_VEL_TO_FITNESS", 0x77DDDA0D));
	m_DistanceToFitness.Init(ATSTRINGHASH("VEH_DISTANCE_TO_FITNESS", 0xF7A99DB3));
	m_DistanceVehVehToFitness.Init(ATSTRINGHASH("DISTANCE_VEH_VEH_TO_FITNESS", 0x5B182A6));
	m_NumberOfChasingVehToConfidence.Init(ATSTRINGHASH("CHASING_VEH_TO_FITNESS", 0x13F75785));

	m_CarsToNumSirensToPlay.Init(ATSTRINGHASH("CARS_TO_NUM_SIRENS", 0x2CB0DDB5));

	m_StuntJumpBedLoop = NULL;
	m_StuntJumpGoodLandingLoop = NULL;
	m_StuntJumpBadLandingLoop = NULL;

	m_StuntJumpSoundSet.Init(ATSTRINGHASH("STUNT_JUMPS_SOUNDSET", 0xE15250BF));
	sm_MPStuntJumpSoundSet.Init(ATSTRINGHASH("MP_STUNT_JUMPS_SOUNDSET", 0xC9ACA58A));
	
	m_NumberOfSirensToPlay = 0;
	BANK_ONLY(m_SirensBeingPlayed = 0;);

	for(s32 i=0; i<NUMBER_OF_TEST; i++)
	{
		m_ParabolPoints[i] = Vector3(0.f,0.f,0.f);
	}
	m_LandingResults.fitness = -1.f;
	m_LandingResults.timeToLand = -1.f;

	for ( s32 i = 0; i < audConductorMap::MaxNumMapAreas; i++ )
	{
		m_InterestingAreas[i] = 0;
	}
	m_LastOrder = Order_Invalid;
	m_VehicleConductorDM.Init();
	m_VehicleConductorFS.Init();
	SetState(ConductorState_Idle);
	for (u32 i = 0 ; i < NumSides ; i++)
	{
		m_SpatializationFitness[i] = 1.f;
	}
	m_WheelAlreadyLanded.Reset();
	m_LastCheckPoint = Vector3(0.f,0.f,0.f);

	m_VehTimeInAir = 0;
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductor::Reset()
{
	m_VehicleConductorDM.Reset();
	m_VehicleConductorFS.Reset();

	m_NumberOfVehWithSirenOn = 0;
	for ( u32 i = 0; i < AUD_MAX_NUM_VEH_WITH_SIREN; i++ )
	{
		m_VehiclesWithSirenOn[i].pVeh = NULL;
		m_VehiclesWithSirenOn[i].area = audConductorMap::FurtherAwayArea;
		m_VehiclesWithSirenOn[i].fitness = -1.f;
	}

	m_NumberOfTargets = 0;
	m_NumberOfNewTargets = 0;
	m_LastNumVehInFarArea = 0;
	for ( s32 i = 0; i < MaxNumVehConductorsTargets; i++ )
	{
		m_VehicleTargets[i].pVeh = NULL;
		m_VehicleTargets[i].area = audConductorMap::FurtherAwayArea;
		m_VehicleTargets[i].fitness = -1.f;
		m_VehicleNewTargets[i].pVeh = NULL;
		m_VehicleNewTargets[i].area = audConductorMap::FurtherAwayArea;
		m_VehicleNewTargets[i].fitness = -1.f;
	}

	m_InitialVehVelocity =  Vector3(0.f,0.f,0.f);
	m_elapsedTime = 0;
	m_StuntJumpStarted = false;
	m_ShouldUpdateJump = false;
	m_lastAngVelocity = 0.f;
	m_NumberOfSirensToPlay = 0;
	BANK_ONLY(m_SirensBeingPlayed = 0;);

	for(s32 i=0; i<NUMBER_OF_TEST; i++)
	{
		m_ParabolPoints[i] = Vector3(0.f,0.f,0.f);
	}

	for ( s32 i = 0; i < audConductorMap::MaxNumMapAreas; i++ )
	{
		m_InterestingAreas[i] = 0;
	}
	for (u32 i = 0 ; i < NumSides ; i++)
	{
		m_SpatializationFitness[i] = 1.f;
	}
	m_LastOrder = Order_Invalid;
	SetState(ConductorState_Idle);
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductor::InitForDLC()
{
	sm_MPStuntJumpSoundSet.Init(ATSTRINGHASH("MP_STUNT_JUMPS_SOUNDSET", 0xC9ACA58A));
}
//-------------------------------------------------------------------------------------------------------------------
//													DECISION MAKING
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductor::ProcessUpdate()
{
	BANK_ONLY(VehicleConductorDebugInfo(););

	if(sm_EnableSirens)
	{
		sm_TimeSinceLastUpdate += fwTimer::GetTimeStepInMilliseconds();
		UpdateSirens();
	}
	
	Update();
	m_VehicleConductorDM.ProcessUpdate();
	m_VehicleConductorFS.ProcessUpdate();
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audVehicleConductor::UpdateState(const s32 state, const s32, const fwFsm::Event event)
{
	fwFsmBegin
		fwFsmState(ConductorState_Idle)
		fwFsmOnUpdate
		return Idle();	
	fwFsmState(ConductorState_EmphasizeLowIntensity)
		fwFsmOnUpdate
		return EmphasizeLowIntensity();			
	fwFsmState(ConductorState_EmphasizeMediumIntensity)
		fwFsmOnUpdate
		return EmphasizeMediumIntensity();			
	fwFsmState(ConductorState_EmphasizeHighIntensity)
		fwFsmOnUpdate
		return EmphasizeHighIntensity();			
	fwFsmEnd
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audVehicleConductor::Idle()
{
	return fwFsm::Continue;
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audVehicleConductor::EmphasizeLowIntensity()
{
	return fwFsm::Continue;
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audVehicleConductor::EmphasizeMediumIntensity()
{
	return fwFsm::Continue;
}
//-------------------------------------------------------------------------------------------------------------------
fwFsm::Return audVehicleConductor::EmphasizeHighIntensity()
{
	return fwFsm::Continue;
}
//-------------------------------------------------------------------------------------------------------------------
//														HELPERS	
//-------------------------------------------------------------------------------------------------------------------
ConductorsInfo audVehicleConductor::UpdateInfo()
{
	ConductorsInfo info;
	info.info = Info_NothingToDo;
	CPed *player = CGameWorld::FindLocalPlayer();
	if(!player || (player && player->IsDead()))
	{
		StopStuntJump();
	}
	if(m_ShouldUpdateJump)
	{
		info.info = UpdateInfoJump();
	}
	else
	{
		StopStuntJump();
	}
	info.confidence = 0.f; 
	info.emphasizeIntensity = Intensity_Invalid; 
	if(m_NumberOfVehWithSirenOn > 0) 
	{
		info.confidence = m_NumberOfChasingVehToConfidence.CalculateValue((f32)m_NumberOfVehWithSirenOn); 
		if(info.confidence >= 0.8f)
		{
			info.emphasizeIntensity = Intensity_High;
		}
		else if(info.confidence >= 0.5f)
		{
			info.emphasizeIntensity = Intensity_Medium;
		}
		else if(info.confidence >= 0.3f)
		{
			info.emphasizeIntensity = Intensity_Low;
		}
		else 
		{
			info.emphasizeIntensity = Intensity_Invalid;
		}
		return info;
	}
	return info;
}
//-------------------------------------------------------------------------------------------------------------------
bool audVehicleConductor::IsTargetValid(CVehicle* vehicle,audConductorMap::MapAreas area)
{
	if(vehicle && vehicle->GetVehicleAudioEntity() 
		&& vehicle->GetVehicleAudioEntity()->GetSirenControlledByConductors() && vehicle->UsesSiren()
		&& vehicle->GetVehicleAudioEntity()->IsPlayingSiren() && vehicle->GetStatus() != STATUS_WRECKED)
	{
		//Check if the target has become the player's vehicle
		CVehicle* playerVeh = CGameWorld::FindLocalPlayerVehicle();
		if(!playerVeh || (playerVeh && (vehicle != playerVeh)))
		{
			// Check that the target still in a valid zone. 
			bool notValid = (area > audConductorMap::ClosestArea && m_InterestingAreas[audConductorMap::ClosestArea] >= 2)
				|| (area > audConductorMap::MediumDistanceArea);
			if(!notValid)
			{
				return true;
			}
		}
	}
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
bool audVehicleConductor::IsCandidateValid(CVehicle* vehicle,f32 &distance,Vector3 &dirToVeh)
{
	if(vehicle && vehicle->GetVehicleAudioEntity() 
		&& vehicle->GetVehicleAudioEntity()->GetSirenControlledByConductors() && vehicle->UsesSiren()
		&& vehicle->GetVehicleAudioEntity()->IsPlayingSiren() && vehicle->GetHealth() > 0.f)
	{
		//Check if the target has become the player's vehicle
		CVehicle* playerVeh = CGameWorld::FindLocalPlayerVehicle();
		if(!playerVeh || (playerVeh && (vehicle != playerVeh)))
		{
			CPed* player = CGameWorld::FindLocalPlayer();
			if (player)
			{
				//Compute area based on the distance from the veh to the player.
				Vector3 vehiclePso2D = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition()); 
				vehiclePso2D.SetZ(0.f);
				Vector3 playerPosition = VEC3V_TO_VECTOR3(player->GetTransform().GetPosition());
				playerPosition.SetZ(0.f);

				dirToVeh = vehiclePso2D - playerPosition;
				distance = dirToVeh.Mag();
				return distance <= audConductorMap::MaxGridDistance;
			}
		}
		else if( playerVeh && (vehicle == playerVeh))
		{
			vehicle->GetVehicleAudioEntity()->SetMustPlaySiren(true);
			vehicle->GetVehicleAudioEntity()->SmoothSirenVolume(1.f, 1.f);
		}
	}
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductor::CleanUpSirensInfo()
{
	// Clean up the list of vehicles candidates.
	for (u32 i = 0 ; i < AUD_MAX_NUM_VEH_WITH_SIREN; i++)
	{
		m_VehiclesWithSirenOn[i].area = audConductorMap::FurtherAwayArea;
		m_VehiclesWithSirenOn[i].pVeh = NULL;
		m_VehiclesWithSirenOn[i].fitness = -1.f;
	}
	m_NumberOfVehWithSirenOn = 0;
	//Clean interesting areas info.
	for ( u32 i = 0; i < audConductorMap::MaxNumMapAreas; i++ )
	{
		m_InterestingAreas[i] = 0;
	}
	// Clean the list of possible new targets.
	for ( u32 i = 0; i < MaxNumVehConductorsTargets; i++ )
	{
		m_VehicleNewTargets[i].pVeh = NULL;
		m_VehicleNewTargets[i].area = audConductorMap::FurtherAwayArea;
		m_VehicleNewTargets[i].fitness = -1.f;
	}
	m_NumberOfNewTargets = 0;
	//Check the current targets to see if they still valid, otherwise clean them up.
	Targets	cachedVehicleTargets;  
	for ( u32 i = 0; i < MaxNumVehConductorsTargets; i++ )
	{
		if(IsTargetValid(m_VehicleTargets[i].pVeh,m_VehicleTargets[i].area))
		{
			cachedVehicleTargets[i].pVeh = m_VehicleTargets[i].pVeh;
			cachedVehicleTargets[i].area =  m_VehicleTargets[i].area;
			cachedVehicleTargets[i].fitness =  m_VehicleTargets[i].fitness;
		}
	}
	// Copy into the targets array the valid targets after the clean up process.
	m_NumberOfTargets = 0;
	for (u32 i = 0; i < MaxNumVehConductorsTargets; i++ )
	{
		m_VehicleTargets[i].pVeh = cachedVehicleTargets[i].pVeh;
		m_VehicleTargets[i].area =  cachedVehicleTargets[i].area;
		m_VehicleTargets[i].fitness =  cachedVehicleTargets[i].fitness;

		if(cachedVehicleTargets[i].pVeh)
		{
			m_NumberOfTargets ++;
		}
	}
	// Reset the spatialization fitness
	for (u32 i = 0 ; i < NumSides ; i++)
	{
		m_SpatializationFitness[i] = 1.f;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductor::UpdateSirensInfo(CVehicle *vehicle)
{
	if(fwTimer::IsGamePaused())
	{
		return;
	}
	f32 distance = 0.f;
	Vector3 dirToVeh;
	dirToVeh.Zero();
	if(IsCandidateValid(vehicle,distance,dirToVeh))
	{
		// Add the vehicle to the list, update the areas and compute its fitness.
		if(naVerifyf(m_NumberOfVehWithSirenOn < AUD_MAX_NUM_VEH_WITH_SIREN,"Siren conductor run out of veh slots."))
		{
			audSides side;
			m_VehiclesWithSirenOn[m_NumberOfVehWithSirenOn].area = CoumputeAndUpdateAreas(vehicle,distance);
			m_VehiclesWithSirenOn[m_NumberOfVehWithSirenOn].pVeh = vehicle;
			m_VehiclesWithSirenOn[m_NumberOfVehWithSirenOn].fitness = ComputeVehicleFitnessAndSide(distance,dirToVeh,side);
			m_VehiclesWithSirenOn[m_NumberOfVehWithSirenOn].side = side;

			// if the vehicle is in the closest zone, update the list of possible targets.
			if(m_VehiclesWithSirenOn[m_NumberOfVehWithSirenOn].area <= audConductorMap::ClosestArea)
			{
				if((m_NumberOfNewTargets < MaxNumVehConductorsTargets) && !IsNewTarget(m_VehiclesWithSirenOn[m_NumberOfVehWithSirenOn].pVeh))
				{						
					m_VehicleNewTargets[m_NumberOfNewTargets].pVeh = m_VehiclesWithSirenOn[m_NumberOfVehWithSirenOn].pVeh;
					m_VehicleNewTargets[m_NumberOfNewTargets].fitness = m_VehiclesWithSirenOn[m_NumberOfVehWithSirenOn].fitness;
					m_VehicleNewTargets[m_NumberOfNewTargets].area = m_VehiclesWithSirenOn[m_NumberOfVehWithSirenOn].area;
					m_VehicleNewTargets[m_NumberOfNewTargets].side = m_VehiclesWithSirenOn[m_NumberOfVehWithSirenOn].side;
					m_SpatializationFitness[m_VehicleNewTargets[m_NumberOfNewTargets].side] *= 0.5f;
					m_NumberOfNewTargets ++;
				}
				else
				{
					for (u32 i = 0; i < MaxNumVehConductorsTargets; i ++)
					{
						if((!m_VehicleNewTargets[i].pVeh && !IsNewTarget(m_VehiclesWithSirenOn[m_NumberOfVehWithSirenOn].pVeh))|| (m_VehicleNewTargets[i].pVeh && m_VehicleNewTargets[i].fitness <= m_VehiclesWithSirenOn[m_NumberOfVehWithSirenOn].fitness))
						{
							if(!m_VehicleNewTargets[i].pVeh)
							{
								m_NumberOfNewTargets ++;
							}
							else 
							{
								m_SpatializationFitness[m_VehicleNewTargets[i].side] = 1.f;
								m_VehicleNewTargets[i].side = m_VehiclesWithSirenOn[m_NumberOfVehWithSirenOn].side;
								m_SpatializationFitness[m_VehicleNewTargets[i].side] *= 0.5f;
							}
							m_VehicleNewTargets[i].pVeh = m_VehiclesWithSirenOn[m_NumberOfVehWithSirenOn].pVeh;
							m_VehicleNewTargets[i].fitness = m_VehiclesWithSirenOn[m_NumberOfVehWithSirenOn].fitness;
							m_VehicleNewTargets[i].area = m_VehiclesWithSirenOn[m_NumberOfVehWithSirenOn].area;
						}
					}
				}
			}
			// if that vehicle is already a target, update its fitness
			s32 targetIndex = -1;
			if(IsTarget(m_VehiclesWithSirenOn[m_NumberOfVehWithSirenOn].pVeh,targetIndex))
			{
				m_VehicleTargets[targetIndex].fitness = m_VehiclesWithSirenOn[m_NumberOfVehWithSirenOn].fitness;
				m_VehicleTargets[targetIndex].area = m_VehiclesWithSirenOn[m_NumberOfVehWithSirenOn].area;
			}
			m_NumberOfVehWithSirenOn ++;
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
audConductorMap::MapAreas audVehicleConductor::CoumputeAndUpdateAreas(CVehicle* vehicle,f32 distance)
{
	audConductorMap::MapAreas area = audConductorMap::FurtherAwayArea;
	if(distance <= audConductorMap::NumZonesInClosestArea * audConductorMap::WidthOfZones)
	{
		area = audConductorMap::ClosestArea;
		m_InterestingAreas[audConductorMap::ClosestArea]++;
		// Make sure if the veh is in the closest area it has this flag set to true
		vehicle->GetVehicleAudioEntity()->SetMustPlaySiren(true);
	}
	else if (distance <= (audConductorMap::NumZonesInClosestArea + audConductorMap::NumZonesInMediumArea) * audConductorMap::WidthOfZones)
	{
		area = audConductorMap::MediumDistanceArea;
		m_InterestingAreas[audConductorMap::MediumDistanceArea]++;
	}
	else if ( vehicle->GetDriver() )
	{
		m_InterestingAreas[audConductorMap::FurtherAwayArea]++;
	}
	return area;
}
//-------------------------------------------------------------------------------------------------------------------
f32  audVehicleConductor::ComputeVehicleFitnessAndSide(f32 distance,Vector3 &dirToVeh,audSides &side)
{
	f32 fitness = 1.f;
	fitness *= m_DistanceToFitness.CalculateRescaledValue(0.f,1.f,0.f,audConductorMap::NumZonesInClosestArea * audConductorMap::WidthOfZones,distance);
	dirToVeh.Normalize();
	f32 fDot = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix()).b.Dot(dirToVeh);
	f32 rDot = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetVolumeListenerMatrix()).a.Dot(dirToVeh);
	//  Calculate in which side the vehicle is to get the fitness. 
	if((fDot >= 0.f && rDot >= 0.f) || (fDot < 0.f && rDot >= 0.f))
	{
		side = right;
		fitness *= m_SpatializationFitness[side];
		//m_SpatializationFitness[right] *= 0.5f;
	}
	else
	{
		side = left;
		fitness *= m_SpatializationFitness[side];
		//m_SpatializationFitness[left] *= 0.5f;
	}
	return fitness;
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductor::UpdateSirens()
{
	// First of all check if it's time to update the targets
	UpdateTargets();
	UpdateClosestArea();
	UpdateMediumDistanceArea();
	UpdateFarArea();
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductor::UpdateClosestArea()
{
	for ( u32 j = 0; j < m_NumberOfVehWithSirenOn; j++ )
	{
		if ( m_VehiclesWithSirenOn[j].area <= audConductorMap::ClosestArea )
		{
			s32 targetIndex = -1;
			if( m_VehiclesWithSirenOn[j].pVeh)
			{
				if(!IsTarget(m_VehiclesWithSirenOn[j].pVeh,targetIndex))
				{
					// if it's no target, flag the vehicle to mute the siren.
					m_VehiclesWithSirenOn[j].pVeh->GetVehicleAudioEntity()->SmoothSirenVolume(1.f/(sm_TimeToMuteNonTargets * 1000.f), 0.f);
				}
				else
				{
					m_VehiclesWithSirenOn[j].pVeh->GetVehicleAudioEntity()->SmoothSirenVolume(1.f/(sm_TimeToUnMuteNonTargets * 1000.f), 1.f);
				}
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductor::UpdateTargets()
{
	// If we don't have current targets, update them with the ones just calculated
	if(m_NumberOfTargets == 0)
	{
		for ( s32 i = 0; i < MaxNumVehConductorsTargets; i++ )
		{
			if(m_VehicleNewTargets[i].pVeh)
			{
				m_VehicleTargets[i].pVeh = m_VehicleNewTargets[i].pVeh;
				m_VehicleTargets[i].fitness = m_VehicleNewTargets[i].fitness;
				m_VehicleTargets[i].area = m_VehicleNewTargets[i].area;
				m_NumberOfTargets ++;
			}
		}
	}
	// if we don't have enough, update the list
	else if( m_NumberOfTargets < m_NumberOfNewTargets)
	{
		for ( s32 i = 0; i < MaxNumVehConductorsTargets; i++ )
		{
			s32 targetIndex = -1;
			if(m_VehicleNewTargets[i].pVeh && !IsTarget(m_VehicleNewTargets[i].pVeh,targetIndex))
			{
				for ( s32 j = 0; j < MaxNumVehConductorsTargets; j++ )
				{
					if(!m_VehicleTargets[j].pVeh)
					{
						m_VehicleTargets[j].pVeh = m_VehicleNewTargets[i].pVeh;
						m_VehicleTargets[j].fitness = m_VehicleNewTargets[i].fitness;
						m_VehicleTargets[j].area = m_VehicleNewTargets[i].area;
						m_NumberOfTargets ++;
						break;
					}
				}
				break;
			}
		}
	}
	else
	{
		// if we already have targets, check if we have a better option, if not update the current ones.
		for ( s32 i = 0; i < MaxNumVehConductorsTargets; i++ )
		{
			for (s32 j = 0; j< MaxNumVehConductorsTargets; j++)
			{
				s32 targetIndex = -1;
				if (m_VehicleNewTargets[j].pVeh)
				{
					bool isTarget = IsTarget(m_VehicleNewTargets[j].pVeh,targetIndex);
					if(!isTarget)
					{
						if( (m_VehicleNewTargets[j].pVeh->GetVehicleAudioEntity()->IsLastPlayerVeh())
							|| ((m_VehicleTargets[i].fitness < sm_VehMinFitnessThreshold )
							&& (m_VehicleNewTargets[j].fitness > sm_VehMaxFitnessThreshold)
							&& sm_TimeSinceLastUpdate > sm_TimeToUpdateTargets))

						{
							// if the veh is the last player veh, force it to be a target.
							m_VehicleTargets[i].pVeh    = m_VehicleNewTargets[j].pVeh;
							m_VehicleTargets[i].fitness = m_VehicleNewTargets[j].fitness;
							m_VehicleTargets[i].area  = m_VehicleNewTargets[j].area;
						}
					}
					else if (m_VehicleTargets[i].pVeh == m_VehicleNewTargets[j].pVeh)
					{
						m_VehicleTargets[i].fitness = m_VehicleNewTargets[j].fitness;
						m_VehicleTargets[i].area  = m_VehicleNewTargets[j].area;
					}
				}
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductor::UpdateMediumDistanceArea()
{
	// Get the number of vehicles to play
	m_NumberOfSirensToPlay = (s32)m_CarsToNumSirensToPlay.CalculateValue((f32)m_InterestingAreas[audConductorMap::MediumDistanceArea]);
	BANK_ONLY(m_SirensBeingPlayed  = 0;);
	// If we have vehicles in the middle area
	if(m_InterestingAreas[audConductorMap::ClosestArea] == 0 && m_InterestingAreas[audConductorMap::MediumDistanceArea] > 0)
	{
		//First of all keep playing the transition targets.
		for ( u32 j = 0; j < MaxNumVehConductorsTargets; j++ )
		{
			if( m_VehicleTargets[j].pVeh && m_VehicleTargets[j].area >= audConductorMap::MediumDistanceArea && m_VehicleTargets[j].area < audConductorMap::FurtherAwayArea)
			{
				if( m_NumberOfSirensToPlay > 0 )
				{
					//Double check that this is set up to true
					m_VehicleTargets[j].pVeh->GetVehicleAudioEntity()->SetMustPlaySiren(true);
					m_VehicleTargets[j].pVeh->GetVehicleAudioEntity()->SmoothSirenVolume(1.f/(sm_TimeToUnMuteVehMidZone * 1000.f), 1.f);
					m_NumberOfSirensToPlay --;
					BANK_ONLY(m_SirensBeingPlayed ++;);
				}
				else
				{
					m_VehicleTargets[j].pVeh->GetVehicleAudioEntity()->SmoothSirenVolume(1.f/(sm_TimeToMuteVehMidZone * 1000.f), 0.f,true);
				}
			}
		}
		// Now if we still have sirens to play
		for ( u32 j = 0; j < m_NumberOfVehWithSirenOn; j++ )
		{
			s32 targetIndex = -1;
			if(m_VehiclesWithSirenOn[j].pVeh && m_VehiclesWithSirenOn[j].area >= audConductorMap::MediumDistanceArea && m_VehiclesWithSirenOn[j].area < audConductorMap::FurtherAwayArea
				&& !IsTarget(m_VehiclesWithSirenOn[j].pVeh,targetIndex))
			{
				if( m_NumberOfSirensToPlay > 0)
				{
					if( m_VehiclesWithSirenOn[j].pVeh)
					{
						m_VehiclesWithSirenOn[j].pVeh->GetVehicleAudioEntity()->SetMustPlaySiren(true);
						m_VehiclesWithSirenOn[j].pVeh->GetVehicleAudioEntity()->SmoothSirenVolume(1.f/(sm_TimeToUnMuteVehMidZone * 1000.f), 1.f);
						m_NumberOfSirensToPlay --;
						BANK_ONLY(m_SirensBeingPlayed ++;);
					}
				}
				else
				{
					m_VehiclesWithSirenOn[j].pVeh->GetVehicleAudioEntity()->SmoothSirenVolume(1.f/(sm_TimeToMuteVehMidZone * 1000.f), 0.f,true);
				}

			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductor::UpdateFarArea()
{
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if (naVerifyf(pPlayer,"Can't find local player."))
	{	
		// If there is any vehicle in the further away area, play a fake siren sound. 
		if( m_InterestingAreas[audConductorMap::FurtherAwayArea] > 0 && !g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::ScriptPlayingDistantSiren) && !g_PlayerSwitch.IsActive() REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))
		{
			m_VehicleConductorFS.PlayFakeDistanceSirens(/*Send number of veh, to play one sound or another*/);
			sm_TimeSinceFakeSirensWasPlayed = 0;
		}
		else if(!g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::ScriptPlayingDistantSiren))
		{
			if(!g_PlayerSwitch.IsActive() && (pPlayer->GetPlayerWanted()->GetWantedLevel()!= WANTED_CLEAN || CScriptHud::iFakeWantedLevel > 0))
			{
				sm_TimeSinceFakeSirensWasPlayed += fwTimer::GetTimeStepInMilliseconds();
				if(sm_TimeSinceFakeSirensWasPlayed > sm_TimeToStopFakeSirens)
				{
					sm_TimeSinceFakeSirensWasPlayed = 0;
					// Stop whatever was playing. 
					m_VehicleConductorFS.StopFakeDistanceSirens();
				}
			}else  
			{
				// Stop whatever was playing. 
				m_VehicleConductorFS.StopFakeDistanceSirens();
			}
		}
		else if(g_PlayerSwitch.IsActive())
		{
			m_VehicleConductorFS.StopFakeDistanceSirens();
		}
		Vector3 avgDirection = Vector3(0.f,0.f,0.f);
		for ( u32 j = 0; j < m_NumberOfVehWithSirenOn; j++ )
		{
			// Mute all vehicles in the further away area.
			if( m_VehiclesWithSirenOn[j].pVeh )
			{
				if ( m_VehiclesWithSirenOn[j].area >= audConductorMap::FurtherAwayArea ) 
				{
					m_VehiclesWithSirenOn[j].pVeh->GetVehicleAudioEntity()->SmoothSirenVolume(1.f/(sm_TimeToMuteVehFarZone * 1000.f), 0.f,true);
					if(m_InterestingAreas[audConductorMap::FurtherAwayArea] > 0)
					{
						Vector3 dirToVeh = VEC3V_TO_VECTOR3(m_VehiclesWithSirenOn[j].pVeh->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
						// Keep the avg direction
						avgDirection = avgDirection + dirToVeh;
					}
				}
			}
		}
		if(m_InterestingAreas[audConductorMap::FurtherAwayArea] > 0)
		{
			if(m_LastNumVehInFarArea != m_InterestingAreas[audConductorMap::FurtherAwayArea])
			{
				m_VehicleConductorFS.ResetFakeSirenPosApply();
			}
			m_LastNumVehInFarArea = m_InterestingAreas[audConductorMap::FurtherAwayArea]; 
			avgDirection *= 1.f/ m_InterestingAreas[audConductorMap::FurtherAwayArea];
			avgDirection.Normalize();
			m_AvgDirectionToDistantSirens = avgDirection;
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductor::GetAvgDirForFakeSiren(Vector3 &sirenPosition)
{
	sirenPosition = m_AvgDirectionToDistantSirens;
}
//-------------------------------------------------------------------------------------------------------------------
bool audVehicleConductor::PlayStuntJumpCollision(f32 impactMag)
{
	if(m_ShouldUpdateJump && impactMag >= sm_JumpConductorImpactThreshold)
	{
		CVehicle* playerVeh = CGameWorld::FindLocalPlayerVehicle();
		if(playerVeh)
		{
			m_StuntJumpStarted = false;
			m_CachedLineTest.ClearAll();
			m_lastAngVelocity = 0.f;
#if __BANK
			if(sm_HasToPause)
			{
				naDisplayf("Impact Mag %f",impactMag);
				fwTimer::StartUserPause();
			}
#endif
			// Force the time to land to be 0, to correc the ~0.005 error we get.
			m_LandingResults.timeToLand = 0.f;
			m_ForceStuntJumpTimeToLandToZero = true;
			if(m_StuntJumpBedLoop)
			{
				m_StuntJumpBedLoop->FindAndSetVariableValue(ATSTRINGHASH("TimeToLand", 0x3AE72077),m_LandingResults.timeToLand);
				m_StuntJumpBedLoop->FindAndSetVariableValue(ATSTRINGHASH("Confidence", 0x5C254F1D),m_LandingResults.fitness);
			}
			if(m_StuntJumpGoodLandingLoop)
			{
				m_StuntJumpGoodLandingLoop->FindAndSetVariableValue(ATSTRINGHASH("TimeToLand", 0x3AE72077),m_LandingResults.timeToLand);
				m_StuntJumpGoodLandingLoop->FindAndSetVariableValue(ATSTRINGHASH("Confidence", 0x5C254F1D),m_LandingResults.fitness);
			}
			if(m_StuntJumpBadLandingLoop)
			{
				m_StuntJumpBadLandingLoop->FindAndSetVariableValue(ATSTRINGHASH("TimeToLand", 0x3AE72077),m_LandingResults.timeToLand);
				m_StuntJumpBadLandingLoop->FindAndSetVariableValue(ATSTRINGHASH("Confidence", 0x5C254F1D),m_LandingResults.fitness);
			}
			//bool shouldStop = true;
			audSoundInitParams initParams;	
			// allow orphaned so that the select sound doesn't get killed when we shutdown session when starting a new game
			initParams.Pan = 0;
			initParams.SetVariableValue(ATSTRINGHASH("timeInAir", 0x894DA9CE),(f32)m_VehTimeInAir);
			//initParams.Volume = g_SilenceVolume;
			audMetadataRef soundRef = g_NullSoundRef;
			if(playerVeh->GetVehicleType() == VEHICLE_TYPE_CAR || playerVeh->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR)
			{
				if(NetworkInterface::IsGameInProgress())
				{
					soundRef = sm_MPStuntJumpSoundSet.Find(ATSTRINGHASH("BAD_LAND_CAR", 0xFDFF5192));
				}
				else
				{
					soundRef = m_StuntJumpSoundSet.Find(ATSTRINGHASH("BAD_LAND_CAR", 0xFDFF5192));
				}
			}
			else if(playerVeh->GetVehicleType() == VEHICLE_TYPE_BIKE || playerVeh->GetVehicleType() == VEHICLE_TYPE_QUADBIKE || playerVeh->GetVehicleType() == VEHICLE_TYPE_BICYCLE || playerVeh->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)
			{
				if(NetworkInterface::IsGameInProgress())
				{
					soundRef = sm_MPStuntJumpSoundSet.Find(ATSTRINGHASH("BAD_LAND_BIKE", 0x3449F666));
				}
				else
				{
					soundRef = m_StuntJumpSoundSet.Find(ATSTRINGHASH("BAD_LAND_BIKE", 0x3449F666));
				}
			}
			g_FrontendAudioEntity.CreateAndPlaySound(soundRef,&initParams);
			return true;
		}
	}
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductor::StopStuntJump()
{
	if(m_StuntJumpBedLoop)
	{
		m_StuntJumpBedLoop->StopAndForget();
		m_StuntJumpBedLoop = NULL;
	}
	if(m_StuntJumpGoodLandingLoop)
	{
		m_StuntJumpGoodLandingLoop->StopAndForget();
		m_StuntJumpGoodLandingLoop = NULL;
	}
	if(m_StuntJumpBadLandingLoop)
	{
		m_StuntJumpBadLandingLoop->StopAndForget();
		m_StuntJumpBadLandingLoop = NULL;
	}
	m_VehicleConductorDM.StopStuntJumpScene();
	m_VehicleConductorDM.StopGoodLandingScene();
	m_VehicleConductorDM.StopBadLandingScene();
	m_StuntJumpStarted = false;
	m_ShouldUpdateJump = false;
	m_ForceStuntJumpTimeToLandToZero = false;
	m_VehTimeInAir = 0;
	m_LastOrder = Order_Invalid;
	SetState(ConductorState_Idle);
}
//-------------------------------------------------------------------------------------------------------------------
ConductorsInfoType audVehicleConductor::UpdateInfoJump()
{
	//PF_FUNC(LandingTestTimer);
	ConductorsInfoType info; 
	info = Info_NothingToDo;
	CVehicle* playerVeh = CGameWorld::FindLocalPlayerVehicle();

	if(playerVeh && (!playerVeh->GetSpecialFlightModeAllowed() || playerVeh->GetSpecialFlightModeRatio() == 0.f) && (playerVeh->GetVehicleType() == VEHICLE_TYPE_CAR || playerVeh->GetVehicleType() == VEHICLE_TYPE_BIKE || playerVeh->GetVehicleType() == VEHICLE_TYPE_QUADBIKE || playerVeh->GetVehicleType() == VEHICLE_TYPE_BICYCLE || playerVeh->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR || playerVeh->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE))
	{
		if (playerVeh->IsInAir())
		{
			// update time in air
			m_VehTimeInAir += audNorthAudioEngine::GetTimeStepInMs();
		}
		if(playerVeh->IsInAir() && !CVehicleRecordingMgr::IsPlaybackGoingOnForCar(playerVeh) && !m_StuntJumpStarted)
		{
			m_WheelAlreadyLanded.Reset();
			m_StuntJumpStarted = true;
#if __BANK
			if(sm_PauseOnLandEvent)
			{
				sm_HasToPause = true;
			}
#endif
			m_ParabolPoints[0] = VEC3V_TO_VECTOR3(playerVeh->GetTransform().GetPosition());
			m_InitialVehVelocity = playerVeh->GetVelocity();
			m_elapsedTime = fwTimer::GetTimeInMilliseconds();
			m_LastCheckPoint = m_ParabolPoints[0];
			for (s32 i = 1; i < NUMBER_OF_TEST ; ++i)
			{
				Vector3 nextPoint;
				nextPoint =  m_ParabolPoints[0] + (playerVeh->GetVelocity() *   sm_DurationOfTest * float(i) )+ (0.5*(Vector3(0.f,0.f,GRAVITY) * sm_DurationOfTest * float(i) *sm_DurationOfTest * float(i)) ); 
				m_ParabolPoints[i] = nextPoint;
				m_CachedLineTest.m_MainLineTest[i-1].AddProbe(m_ParabolPoints[i-1],m_ParabolPoints[i],NULL,ArchetypeFlags::GTA_ALL_MAP_TYPES,false);
			}
#if __BANK
			if(sm_DrawTrajectory)
			{
				for (s32 i = 1; i < NUMBER_OF_TEST ; ++i)
				{
					grcDebugDraw::Line(m_ParabolPoints[i-1],m_ParabolPoints[i], Color_red,1000);
				}
			}
#endif
			if(!m_StuntJumpBedLoop)
			{
				audSoundInitParams initParams;	
				initParams.Pan = 0;
				audMetadataRef soundRef = g_NullSoundRef;
				GetStuntJumpBedLoop(soundRef,playerVeh);													
				g_FrontendAudioEntity.CreateAndPlaySound_Persistent(soundRef,&m_StuntJumpBedLoop,&initParams);
			}		
			m_VehicleConductorDM.StartStuntJumpScene();

		}
		else if (!playerVeh->IsInAir())
		{
			m_CachedLineTest.ClearAll();
			m_lastAngVelocity = 0.f;
#if __BANK
			if(sm_HasToPause)
			{
				fwTimer::StartUserPause();
			}
#endif
			// In network games we only trigger stuff if the vehicle has been in the air for a while
			if(NetworkInterface::IsGameInProgress() && m_VehTimeInAir < g_TimeInAirThreshold)
			{
				StopStuntJump();
				return info;
			}

			// Force the time to land to be 0, to correct the ~0.005 error we get.
			m_LandingResults.timeToLand = 0.f;
			m_ForceStuntJumpTimeToLandToZero = true;

			if(m_StuntJumpBedLoop)
			{
				m_StuntJumpBedLoop->FindAndSetVariableValue(ATSTRINGHASH("TimeToLand", 0x3AE72077),m_LandingResults.timeToLand);
				m_StuntJumpBedLoop->FindAndSetVariableValue(ATSTRINGHASH("Confidence", 0x5C254F1D),m_LandingResults.fitness);
			}
			if(m_StuntJumpGoodLandingLoop)
			{
				m_StuntJumpGoodLandingLoop->FindAndSetVariableValue(ATSTRINGHASH("TimeToLand", 0x3AE72077),m_LandingResults.timeToLand);
				m_StuntJumpGoodLandingLoop->FindAndSetVariableValue(ATSTRINGHASH("Confidence", 0x5C254F1D),m_LandingResults.fitness);
			}
			if(m_StuntJumpBadLandingLoop)
			{
				m_StuntJumpBadLandingLoop->FindAndSetVariableValue(ATSTRINGHASH("TimeToLand", 0x3AE72077),m_LandingResults.timeToLand);
				m_StuntJumpBadLandingLoop->FindAndSetVariableValue(ATSTRINGHASH("Confidence", 0x5C254F1D),m_LandingResults.fitness);
			}
			//bool shouldStop = true;
			if(m_StuntJumpStarted)
			{
				if(m_LandingResults.fitness > sm_ThresholdFitnessToLand)
				{
					// suspension sounds
					for(s32 i = 0; i < playerVeh->GetNumWheels(); i++)
					{
						// suspension bottoming out
						CWheel* wheel = playerVeh->GetWheel(i);

						if(wheel)
						{
							u32 hierarchyID = wheel->GetHierarchyId();
							s8 wheelAudId = -1;			
							if(hierarchyID == VEH_WHEEL_LF || hierarchyID == VEH_WHEEL_RF)
							{
								wheelAudId = 0;
							}
							else if(hierarchyID == VEH_WHEEL_LR || hierarchyID == VEH_WHEEL_RR)
							{
								wheelAudId = 1;
							} 
							if (wheelAudId != -1)
							{
								if(m_WheelAlreadyLanded.IsClear(wheelAudId) && wheel->GetDynamicFlags().IsFlagSet(WF_HIT))
								{
									m_WheelAlreadyLanded.Set(wheelAudId);
									audSoundInitParams initParams;	
									// allow orphaned so that the select sound doesn't get killed when we shutdown session when starting a new game
									initParams.Pan = 0;
									initParams.SetVariableValue(ATSTRINGHASH("timeInAir", 0x894DA9CE),(f32)m_VehTimeInAir);
									//initParams.Volume = g_SilenceVolume;
									audMetadataRef soundRef = g_NullSoundRef;
									if(playerVeh->GetVehicleType() == VEHICLE_TYPE_CAR || playerVeh->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR)
									{
										if(NetworkInterface::IsGameInProgress())
										{
											soundRef = sm_MPStuntJumpSoundSet.Find(ATSTRINGHASH("GOOD_LAND_CAR", 0x3A1E031C));
										}
										else
										{
											soundRef = m_StuntJumpSoundSet.Find(ATSTRINGHASH("GOOD_LAND_CAR", 0x3A1E031C));
										}
									}
									else if(playerVeh->GetVehicleType() == VEHICLE_TYPE_BIKE || playerVeh->GetVehicleType() == VEHICLE_TYPE_QUADBIKE || playerVeh->GetVehicleType() == VEHICLE_TYPE_BICYCLE || playerVeh->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)
									{
										if(NetworkInterface::IsGameInProgress())
										{
											soundRef = sm_MPStuntJumpSoundSet.Find(ATSTRINGHASH("GOOD_LAND_BIKE", 0x5B2DBC73));
										}
										else
										{
											soundRef = m_StuntJumpSoundSet.Find(ATSTRINGHASH("GOOD_LAND_BIKE", 0x5B2DBC73));
										}
									}
									g_FrontendAudioEntity.CreateAndPlaySound(soundRef,&initParams);
								}
							}
						}
					}
				}
				else 
				{
					PlayStuntJumpCollision(10.f);
				}
			}
			// In MP we don't rely on the stunt jump logic to reset the logic so if we crash, stop the jump for the next one
			if(NetworkInterface::IsGameInProgress())
			{
				StopStuntJump();
			}
		}
		else
		{
			UpdateLineTestAsync(info);
		}
	}
	else
	{
		StopStuntJump();
	}
	
	return info;
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductor::GetStuntJumpBedLoop(audMetadataRef &soundRef, const CVehicle* playerVeh)
{
	CPed* player = CGameWorld::FindLocalPlayer();
	if(!player || !playerVeh )
	{
		return;
	}
	if(NetworkInterface::IsGameInProgress() && playerVeh)
	{
		if(playerVeh->GetVehicleType() == VEHICLE_TYPE_CAR || playerVeh->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR)
		{
			soundRef = sm_MPStuntJumpSoundSet.Find(ATSTRINGHASH("BED_LOOP_CAR", 0x13D5ECAE));
		}
		else if(playerVeh->GetVehicleType() == VEHICLE_TYPE_BIKE || playerVeh->GetVehicleType() == VEHICLE_TYPE_QUADBIKE || playerVeh->GetVehicleType() == VEHICLE_TYPE_BICYCLE || playerVeh->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)
		{
			soundRef = sm_MPStuntJumpSoundSet.Find(ATSTRINGHASH("BED_LOOP_BIKE", 0xA5B570CE));
		}
	}
	else
	{
		if( player && player->GetPedModelInfo())
		{
			switch(player->GetPedModelInfo()->GetHashKey())
			{
			case 0xD7114C9: //ATSTRINGHASH("Player_Zero", 0xD7114C9)
				{
					if(playerVeh->GetVehicleType() == VEHICLE_TYPE_CAR || playerVeh->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR)
					{
						soundRef = m_StuntJumpSoundSet.Find(ATSTRINGHASH("BED_LOOP_CAR_MICHAEL", 0x4E7AF8EA));
					}
					else if(playerVeh->GetVehicleType() == VEHICLE_TYPE_BIKE || playerVeh->GetVehicleType() == VEHICLE_TYPE_QUADBIKE || playerVeh->GetVehicleType() == VEHICLE_TYPE_BICYCLE || playerVeh->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)
					{
						soundRef = m_StuntJumpSoundSet.Find(ATSTRINGHASH("BED_LOOP_BIKE_MICHAEL", 0xD6274975));
					}
					break;
				}
			case 0x9B22DBAF://ATSTRINGHASH("Player_One", 0x9B22DBAF)
				{
					if(playerVeh->GetVehicleType() == VEHICLE_TYPE_CAR || playerVeh->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR)
					{
						soundRef = m_StuntJumpSoundSet.Find(ATSTRINGHASH("BED_LOOP_CAR_FRANKLIN", 0xCEEDA271));
					}
					else if(playerVeh->GetVehicleType() == VEHICLE_TYPE_BIKE || playerVeh->GetVehicleType() == VEHICLE_TYPE_QUADBIKE || playerVeh->GetVehicleType() == VEHICLE_TYPE_BICYCLE || playerVeh->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)
					{
						soundRef = m_StuntJumpSoundSet.Find(ATSTRINGHASH("BED_LOOP_BIKE_FRANKLIN", 0x4190ED16));
					}
					break;
				}
			case 0x9B810FA2://ATSTRINGHASH("Player_Two", 0x9B810FA2)
				{
					if(playerVeh->GetVehicleType() == VEHICLE_TYPE_CAR || playerVeh->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR)
					{
						soundRef = m_StuntJumpSoundSet.Find(ATSTRINGHASH("BED_LOOP_CAR_TREVOR", 0xDBCDE6AE));
					}
					else if(playerVeh->GetVehicleType() == VEHICLE_TYPE_BIKE || playerVeh->GetVehicleType() == VEHICLE_TYPE_QUADBIKE || playerVeh->GetVehicleType() == VEHICLE_TYPE_BICYCLE || playerVeh->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)
					{
						soundRef = m_StuntJumpSoundSet.Find(ATSTRINGHASH("BED_LOOP_BIKE_TREVOR", 0x871E822D));
					}
					break;
				}
			default:
				break;
			}
		}
	}
}	
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductor::IssueAsyncLineTestProbes()
{
	CVehicle* playerVeh = CGameWorld::FindLocalPlayerVehicle();
	m_StuntJumpStarted = true;
	m_ParabolPoints[0] = VEC3V_TO_VECTOR3(playerVeh->GetTransform().GetPosition());
	m_LastCheckPoint = m_ParabolPoints[0];
	m_InitialVehVelocity = playerVeh->GetVelocity();
	m_elapsedTime = fwTimer::GetTimeInMilliseconds();
	m_lastAngVelocity = 0.f;
	m_DistanceTraveled = 0.f;
	for(s32 i=1; i<NUMBER_OF_TEST; i++)
	{
		Vector3 nextPoint;
		nextPoint =  m_ParabolPoints[0] +(playerVeh->GetVelocity() *   sm_DurationOfTest * float(i) )+ (0.5f*(Vector3(0.f,0.f,GRAVITY) * sm_DurationOfTest * float(i) *sm_DurationOfTest * float(i)) ); 
		m_ParabolPoints[i] = nextPoint;
		m_CachedLineTest.m_MainLineTest[i-1].AddProbe(m_ParabolPoints[i-1],m_ParabolPoints[i],NULL,ArchetypeFlags::GTA_ALL_MAP_TYPES,false);
	}
#if __BANK
	if(sm_DrawTrajectory){
		for(u32 i = 1; i < NUMBER_OF_TEST; ++i)
		{
			grcDebugDraw::Line(m_ParabolPoints[i-1],m_ParabolPoints[i], Color_red,1000);
		}
	}
#endif
}
//-------------------------------------------------------------------------------------------------------------------
bool audVehicleConductor::ProcessAsyncLineTestProbes()
{
	s32 currentIndex = m_CachedLineTest.m_CurrentIndex;

	audCachedLos & lineTest = m_CachedLineTest.m_MainLineTest[currentIndex];

	if(lineTest.m_bContainsProbe)
	{
		if(lineTest.m_bHitSomething==true)
		{

#if __BANK
			if(sm_DrawTrajectory)
				grcDebugDraw::Line(lineTest.m_vIntersectPos,lineTest.m_vIntersectPos + 100.f * lineTest.m_vIntersectNormal, Color_red,1000);
#endif
			m_CachedLineTest.m_ResultPosition = lineTest.m_vIntersectPos;
			m_CachedLineTest.m_ResultNormal = lineTest.m_vIntersectNormal;
			return true;
		}
	}
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductor::UpdateLineTestAsync(ConductorsInfoType &info)
{
	CVehicle* playerVeh = CGameWorld::FindLocalPlayerVehicle();
	//Collect all the results we can from the CWorldProbeAsync
	m_CachedLineTest.GetAllProbeResults();


	m_DistanceTraveled = (VEC3V_TO_VECTOR3(playerVeh->GetTransform().GetPosition()) - m_LastCheckPoint).Mag2();
	//Check if we complete a new test and process it.
	//Otherwise wait another frame until a new one is complete.
	if(m_CachedLineTest.NewTestComplete() && !m_CachedLineTest.m_HasFoundTheSolution)
	{
		// Process it
		m_CachedLineTest.m_HasFoundTheSolution = ProcessAsyncLineTestProbes();
	}
	else if (m_CachedLineTest.m_HasFoundTheSolution)
	{
		// Calculate the time to land.
		f32 timeToLand = 0.f;
		Vector3 vehCurrentPos2D = VEC3V_TO_VECTOR3(playerVeh->GetTransform().GetPosition());
		vehCurrentPos2D.SetZ(0.f);
		Vector3 intersectionPos2D = m_CachedLineTest.m_ResultPosition;
		intersectionPos2D.SetZ(0.f);
		timeToLand = fabs((intersectionPos2D - vehCurrentPos2D).Mag()) / (playerVeh->GetVelocity().Mag() + 0.0001f); // Double check is not zero.
		timeToLand -= sm_LandTimeError;
		timeToLand = Selectf(timeToLand,timeToLand,0.0f);
		timeToLand = m_ForceStuntJumpTimeToLandToZero ? 0.f : timeToLand;
		// Calculate confidence of landing. 
		// Get the yaw difference between the front of the car and the velocity.
		f32 fitness = 1.f;  
		Vector3 carFront =  VEC3V_TO_VECTOR3(playerVeh->GetTransform().GetB());
		carFront.SetZ(0.f);
		carFront.NormalizeSafe();
		Vector3 vel = playerVeh->GetVelocity(); 
		vel.SetZ(0.f);
		vel.NormalizeSafe();
		f32 dotHAngle = carFront.Dot(vel);
		// Get the pitch difference between the up of the car and the surface normal.
		Vector3 carUp =  VEC3V_TO_VECTOR3(playerVeh->GetTransform().GetC());
		carUp.NormalizeSafe();
		Vector3 surfaceNormal = m_CachedLineTest.m_ResultNormal;
		surfaceNormal.NormalizeSafe();
		f32 carPitch = RtoD * atan2(carUp.GetZ(),carUp.GetX());
		f32 surfacePitch = RtoD * atan2(surfaceNormal.GetZ(),surfaceNormal.GetX());
		f32 deltaPitch  = surfacePitch - carPitch;
		if( deltaPitch > 180.0f )
		{
			deltaPitch -= 360.0f ;
		}
		else
		{
			if( deltaPitch < -180.0f )
			{
				deltaPitch += 360.0f;
			}
		}
		Assertf( deltaPitch <= 180 , "Wrong angle when calculation the car land." );
		Assertf( deltaPitch >= -180, "Wrong angle when calculation the car land." );
		//Work out the fitness based on the deltas above.
		fitness *= m_YawDiffToFitness.CalculateValue(dotHAngle);
		Assertf((fitness >= 0.f && fitness <= 1.f), "Wrong fitness value"); 
		//naDisplayf("dotHAngle %f, fitness %f", dotHAngle,fitness);
		deltaPitch = fabs(deltaPitch);
		fitness *= m_PitchDiffToFitness.CalculateValue(deltaPitch);
		Assertf((fitness >= 0.f && fitness <= 1.f), "Wrong fitness value"); 
		//naDisplayf("deltaPitch %f, fitness %f", deltaPitch,fitness);

		
		// Update info since we've find a solution.
		//if(fitness >= sm_ThresholdFitnessToLand)
		//{
			info = Info_DoSpecialStuffs;
		//}
		//else 
		//{
			//info = Info_DoBasicJob;
		//}
		m_LandingResults.fitness = fitness;
		m_LandingResults.timeToLand = timeToLand;
		if(m_StuntJumpBedLoop)
		{
			m_StuntJumpBedLoop->FindAndSetVariableValue(ATSTRINGHASH("TimeToLand", 0x3AE72077),m_LandingResults.timeToLand);
			m_StuntJumpBedLoop->FindAndSetVariableValue(ATSTRINGHASH("Confidence", 0x5C254F1D),m_LandingResults.fitness);
		}
		if(m_StuntJumpGoodLandingLoop)
		{
			m_StuntJumpGoodLandingLoop->FindAndSetVariableValue(ATSTRINGHASH("TimeToLand", 0x3AE72077),m_LandingResults.timeToLand);
			m_StuntJumpGoodLandingLoop->FindAndSetVariableValue(ATSTRINGHASH("Confidence", 0x5C254F1D),m_LandingResults.fitness);
		}
		if(m_StuntJumpBadLandingLoop)
		{
			m_StuntJumpBadLandingLoop->FindAndSetVariableValue(ATSTRINGHASH("TimeToLand", 0x3AE72077),m_LandingResults.timeToLand);
			m_StuntJumpBadLandingLoop->FindAndSetVariableValue(ATSTRINGHASH("Confidence", 0x5C254F1D),m_LandingResults.fitness);
		}
#if __BANK 
		if(sm_ShowLandingInfo)
		{
			char text[256];
			formatf(text, sizeof(text),"TIME TO LAND : %f ", timeToLand);
			grcDebugDraw::Text(m_CachedLineTest.m_ResultPosition + Vector3(0.0f,0.0f,0.8f), Color32(255, 255, 255, 255),text,true,1000);
			formatf(text, sizeof(text),"dotHAngle: %f ", dotHAngle);
			grcDebugDraw::Text(m_CachedLineTest.m_ResultPosition + Vector3(0.0f,0.0f,1.f), Color32(255, 255, 255, 255),text,true,1000);
			formatf(text, sizeof(text),"Fitness : %f LandTime %f", fitness,timeToLand);
			grcDebugDraw::Text(m_CachedLineTest.m_ResultPosition + Vector3(0.0f,0.0f,1.6f), Color32(255, 255, 255, 255),text,true,1000);
			grcDebugDraw::Text(playerVeh->GetTransform().GetPosition() + Vec3V(0.0f,0.0f,1.6f), Color32(255, 255, 255, 255),text,true,1);
		}
#endif
	}
	else if(m_CachedLineTest.AreAllComplete()&& !m_CachedLineTest.m_HasFoundTheSolution)
	{
		// Send a new set of probes if the vehicle still on the air.
		if(CGameWorld::FindLocalPlayerVehicle()->IsInAir() && m_DistanceTraveled > sm_DistanceTraveledThreshold * (m_ParabolPoints[1] - m_ParabolPoints[0]).Mag2())
		{
			IssueAsyncLineTestProbes();
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
f32 audVehicleConductor::GetPlayerVehVelocity()
{
	CVehicle* playerVeh = CGameWorld::FindLocalPlayerVehicle();
	if(playerVeh)
	{
		return playerVeh->GetVelocity().Mag();
	}
	return 0.f;
}

//-------------------------------------------------------------------------------------------------------------------
f32 audVehicleConductor::GetPlayerVehAirtime()
{
	CVehicle* playerVeh = CGameWorld::FindLocalPlayerVehicle();
	if(playerVeh)
	{
		audVehicleAudioEntity * vehAudio = playerVeh->GetVehicleAudioEntity();
		if(vehAudio)
		{
			return (f32)(fwTimer::GetTimeInMilliseconds() - vehAudio->GetLastTimeOnGround());
		}
	}
	return 0.f;
}

//-------------------------------------------------------------------------------------------------------------------
f32 audVehicleConductor::GetPlayerVehUpsideDown()
{
	CVehicle* playerVeh = CGameWorld::FindLocalPlayerVehicle();
	if(playerVeh)
	{
		if((playerVeh->GetVehicleType() == VEHICLE_TYPE_CAR || playerVeh->GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR) &&
			(playerVeh->IsUpsideDown()))
		{
			return 1.f;
		}
	}
	return 0.f;
}

//-------------------------------------------------------------------------------------------------------------------
CVehicle* audVehicleConductor::GetTarget(ConductorsTargets target) const
{
	naAssertf(target >= Near_Primary && target <= Far_Secondary, "Bad target index.");
	return m_VehicleTargets[target].pVeh;
}
//-------------------------------------------------------------------------------------------------------------------
CVehicle* audVehicleConductor::GetVehWithSirenOn(s32 vehIndex) const
{
	naAssertf(vehIndex >= 0 && vehIndex < m_NumberOfVehWithSirenOn, "Bad veh index.");
	return m_VehiclesWithSirenOn[vehIndex].pVeh;
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductor::ProcessSuperConductorOrder(ConductorsOrders order)
{
	naAssertf((order >= Order_Invalid ) && (order <= Order_StuntJump), "Wrong vehicleConductor order.");
	switch(order)
	{
	case Order_LowIntensity:
		m_LastOrder = Order_LowIntensity;
		//m_VehicleConductorDM.SetState(ConductorState_EmphasizeLowIntensity);
		//SetState(ConductorState_EmphasizeLowIntensity);
		break;
	case Order_MediumIntensity:
		m_LastOrder = Order_MediumIntensity;
		//m_VehicleConductorDM.SetState(ConductorState_EmphasizeMediumIntensity);
		//SetState(ConductorState_EmphasizeMediumIntensity);
		break;
	case Order_HighIntensity:
		m_LastOrder = Order_HighIntensity;
		//m_VehicleConductorDM.SetState(ConductorState_EmphasizeHighIntensity);
		//SetState(ConductorState_EmphasizeHighIntensity);
		break;
	case Order_DoNothing:
		m_LastOrder = Order_DoNothing;
		break;
	case Order_LowSpecialIntensity:
		if(m_LastOrder != Order_LowSpecialIntensity && m_LandingResults.timeToLand > sm_ThresholdTimeToLand)
		{
			m_LastOrder = Order_LowSpecialIntensity;
		}
		break;
	case Order_MediumSpecialIntensity:
		if(m_LastOrder != Order_MediumSpecialIntensity && m_LandingResults.timeToLand > sm_ThresholdTimeToLand)
		{
			m_LastOrder = Order_MediumSpecialIntensity;
		}
		break;
	case Order_HighSpecialIntensity:
		if(m_LastOrder != Order_HighSpecialIntensity && m_LandingResults.timeToLand > sm_ThresholdTimeToLand)
		{
			m_LastOrder = Order_HighSpecialIntensity;
		}
		break;
	case Order_StuntJump:
		if(m_LastOrder != Order_StuntJump)
		{
			if(m_LandingResults.timeToLand <= sm_ThresholdTimeToLand)
			{
				BANK_ONLY(bool goodLand = true;);
				m_LastOrder = Order_StuntJump;
				if(m_LandingResults.fitness > sm_ThresholdFitnessToLand)
				{
					m_VehicleConductorDM.StartGoodLandingScene();
					if(!m_StuntJumpGoodLandingLoop)
					{
						audSoundInitParams initParams;	
						// allow orphaned so that the select sound doesn't get killed when we shutdown session when starting a new game
						initParams.Pan = 0;
						//initParams.Volume = g_SilenceVolume;
						if(NetworkInterface::IsGameInProgress())
						{
							g_FrontendAudioEntity.CreateAndPlaySound_Persistent(sm_MPStuntJumpSoundSet.Find(ATSTRINGHASH("GOOD_LANDING_LOOP", 0xFDA3774C)),&m_StuntJumpGoodLandingLoop,&initParams);
						}
						else
						{
							g_FrontendAudioEntity.CreateAndPlaySound_Persistent(m_StuntJumpSoundSet.Find(ATSTRINGHASH("GOOD_LANDING_LOOP", 0xFDA3774C)),&m_StuntJumpGoodLandingLoop,&initParams);
						}
					}				
				}
				else
				{
					m_VehicleConductorDM.StartBadLandingScene();
					BANK_ONLY(goodLand = false;);
					if(!m_StuntJumpBadLandingLoop)
					{
						audSoundInitParams initParams;	
						// allow orphaned so that the select sound doesn't get killed when we shutdown session when starting a new game
						initParams.Pan = 0;
						//initParams.Volume = g_SilenceVolume;
						if(NetworkInterface::IsGameInProgress())
						{
							g_FrontendAudioEntity.CreateAndPlaySound_Persistent(sm_MPStuntJumpSoundSet.Find(ATSTRINGHASH("BAD_LANDING_LOOP", 0x46AB3C4E)),&m_StuntJumpBadLandingLoop,&initParams);
						}
						else
						{
							g_FrontendAudioEntity.CreateAndPlaySound_Persistent(m_StuntJumpSoundSet.Find(ATSTRINGHASH("BAD_LANDING_LOOP", 0x46AB3C4E)),&m_StuntJumpBadLandingLoop,&initParams);
						}
					}
				}
#if __BANK 
				if(sm_DrawJumpResults)
				{
					CVehicle* vehicle = CGameWorld::FindLocalPlayerVehicle();
					if(vehicle)
					{
						grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition()), 2.f,goodLand ? Color_green : Color_red,true,20);
					}
				}
#endif
			}
		}
		break;
	default:
		break;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductor::ProcessMessage(const ConductorMessageData &messageData)
{
	CVehicle* pVeh = NULL;
	switch(messageData.message)
	{
	case SirenTurnedOn:
		m_VehicleTargets[Near_Primary].pVeh = pVeh;
		break;
	case ScriptFakeSirens:
		// extraInfo -> tells us if we want to play (true) or stop (false) the fake distant sirens
		if(sm_EnableSirens)
		{
			if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::ScriptPlayingDistantSiren))
			{
				m_VehicleConductorFS.PlayFakeDistanceSirens();
			}
			else
			{
				m_VehicleConductorFS.StopFakeDistanceSirens();
			}
		}
		break;
	case StuntJump:
		m_ShouldUpdateJump = messageData.bExtraInfo;
		break;
	default: break;
	}
}
//-------------------------------------------------------------------------------------------------------------------
bool audVehicleConductor::IsTarget(const CVehicle *pVeh,s32 &targetIndex)
{
	for (s32 i = 0; i < MaxNumVehConductorsTargets; i++)
	{
		if(m_VehicleTargets[i].pVeh == pVeh)
		{
			targetIndex = i;
			return true;
		}
	}
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
bool audVehicleConductor::IsTarget(const CVehicle *pVeh)
{
	for (s32 i = 0; i < MaxNumVehConductorsTargets; i++)
	{
		if(m_VehicleTargets[i].pVeh == pVeh)
		{
			return true;
		}
	}
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
bool audVehicleConductor::IsNewTarget(const CVehicle *pVeh)
{
	for (s32 i = 0; i < MaxNumVehConductorsTargets; i++)
	{
		if(m_VehicleNewTargets[i].pVeh == pVeh)
		{
			return true;
		}
	}
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
#if __BANK
const char* audVehicleConductor::GetStateName(s32 iState) const
{
	taskAssert(iState >= ConductorState_Idle && iState <= ConductorState_EmphasizeHighIntensity);
	switch (iState)
	{
	case ConductorState_Idle:						return "ConductorState_Idle";
	case ConductorState_EmphasizeLowIntensity:		return "ConductorState_EmphasizeLowIntensity";
	case ConductorState_EmphasizeMediumIntensity:	return "ConductorState_EmphasizeMediumIntensity";
	case ConductorState_EmphasizeHighIntensity:		return "ConductorState_EmphasizeHighIntensity";
	default: taskAssert(0); 	
	}
	return "ConductorState_Invalid";
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductor::VehicleConductorDebugInfo() 
{

	if(sm_ShowFSMInfo)
	{
		char text[64];
		formatf(text,sizeof(text),"VehicleConductor state -> %s", GetStateName(GetState()));
		grcDebugDraw::AddDebugOutput(text);
		formatf(text,sizeof(text),"Time in state -> %f", GetTimeInState());
		grcDebugDraw::AddDebugOutput(text);
	}
	if(sm_ShowSirensInfo)
	{
		char text[64];
		formatf(text,sizeof(text),"Targets -> %d", m_NumberOfTargets);
		grcDebugDraw::AddDebugOutput(text);
		formatf(text,sizeof(text),"Vehicles with siren ON -> %d", m_NumberOfVehWithSirenOn);
		grcDebugDraw::AddDebugOutput(text);
		for ( s32 i = 0; i < audConductorMap::MaxNumMapAreas; i++ )
		{
				formatf(text,sizeof(text),"Zone -> %d, Hits : %d", i,m_InterestingAreas[i]);
				grcDebugDraw::AddDebugOutput(text);
		}
		formatf(text,sizeof(text),"Medium area sirens to play-> %d", m_NumberOfSirensToPlay);
		grcDebugDraw::AddDebugOutput(text);
		formatf(text,sizeof(text),"Medium area sirens being played -> %d", m_SirensBeingPlayed);
		grcDebugDraw::AddDebugOutput(text);
		for(u32 i = 0 ; i < m_NumberOfVehWithSirenOn ; i++)
		{
			if(m_VehiclesWithSirenOn[i].pVeh)
			{
				// Vehicle is playing siren and is not in the non-targets mixgroup, must be playing the siren.
				if(m_VehiclesWithSirenOn[i].pVeh->GetVehicleAudioEntity()->GetMustPlaySiren() 
				&& m_VehiclesWithSirenOn[i].pVeh->GetVehicleAudioEntity()->GetDesireSirenVolume() == 1.f)
				{
					grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_VehiclesWithSirenOn[i].pVeh->GetTransform().GetPosition()), 2.f,Color32(0.f,255.f,0.f,0.5f));
				}
				else if(m_VehiclesWithSirenOn[i].pVeh->GetVehicleAudioEntity()->GetMustPlaySiren() )
				{
					grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_VehiclesWithSirenOn[i].pVeh->GetTransform().GetPosition()), 2.f,Color32(0.f,0.f,255.f,0.5f));
				}
				else 
				{
					grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(m_VehiclesWithSirenOn[i].pVeh->GetTransform().GetPosition()), 2.f,Color32(255.f,0.f,0.f,0.5f));
				}
				if(m_InterestingAreas[audConductorMap::FurtherAwayArea] > 0)
				{
					grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition()), 50.f,Color32(0.f,255.f,0.f,0.5f));
				}
			}

		}

	}
	if(sm_ShowJumpInfo)
	{
		char text[64];
		formatf(text,sizeof(text),"Last land fitness -> %f", m_LandingResults.fitness);
		grcDebugDraw::AddDebugOutput(text);
		formatf(text,sizeof(text),"Last time to land -> %f", m_LandingResults.timeToLand);
		grcDebugDraw::AddDebugOutput(text);
	}

	if(sm_ShowTargets)
	{
		for(u32 i = 0 ; i < MaxNumVehConductorsTargets ; i++)
		{
			if(m_VehicleTargets[i].pVeh)
			{
				char text[64];
				Vector3 vecPos(0.0f,0.0f,0.0f);
				vecPos = VEC3V_TO_VECTOR3(m_VehicleTargets[i].pVeh->GetTransform().GetPosition());
				formatf(text,sizeof(text),"Target fitness :%f",m_VehicleTargets[i].fitness);
				grcDebugDraw::Text(vecPos + Vector3(0.0f,0.0f,0.5f), Color32(255, 255, 255, 255),text,true,1);
			}
		}
	}
	if(sm_ShowNewTargets)
	{	
		for(u32 i = 0 ; i < m_NumberOfNewTargets ; i++)
		{
			if(m_VehicleNewTargets[i].pVeh)
			{
				char text[64];
				Vector3 vecPos(0.0f,0.0f,0.0f);
				vecPos = VEC3V_TO_VECTOR3(m_VehicleNewTargets[i].pVeh->GetTransform().GetPosition());
				formatf(text,sizeof(text),"New Target fitness :%f",m_VehicleNewTargets[i].fitness);
				grcDebugDraw::Text(vecPos + Vector3(0.0f,0.0f,1.f), Color32(255, 255, 255, 255),text,true,1);
			}
		}
	}
	if(sm_ShowVehSirenState)
	{
		for(u32 i = 0 ; i < m_NumberOfVehWithSirenOn ; i++)
		{
			if(m_VehiclesWithSirenOn[i].pVeh)
			{
				char text[64];
				Vector3 vecPos(0.0f,0.0f,0.0f);
				vecPos = VEC3V_TO_VECTOR3(m_VehiclesWithSirenOn[i].pVeh->GetTransform().GetPosition());
				formatf(text, "Veh with siren On fitness: %f",m_VehiclesWithSirenOn[i].fitness);
				grcDebugDraw::Text(vecPos + Vector3(0.0f,0.0f,2.f), Color32(255, 255, 255, 255),text,true,1);
			}

		}
	}
	
	if(sm_DrawVelocity)
	{
		CVehicle* playerVeh = CGameWorld::FindLocalPlayerVehicle();
		if(playerVeh)
		{
			Vector3 vel = playerVeh->GetVelocity();
			vel.NormalizeSafe();
			Vector3 front = VEC3V_TO_VECTOR3(playerVeh->GetTransform().GetB());
			front.NormalizeSafe();
			Vector3 left = VEC3V_TO_VECTOR3(playerVeh->GetTransform().GetA());
			left.NormalizeSafe();
			Vector3 up = VEC3V_TO_VECTOR3(playerVeh->GetTransform().GetC());
			up.NormalizeSafe();

			Vector3 pos =  VEC3V_TO_VECTOR3(playerVeh->GetTransform().GetPosition());
			grcDebugDraw::Line(pos,pos + 10.f * vel, Color_black,1);
			grcDebugDraw::Line(pos,pos + 3.f * front, Color_red,1);
			grcDebugDraw::Line(pos,pos + 3.f * left, Color_green,1);
			grcDebugDraw::Line(pos,pos + 3.f * up, Color_blue,1);
		}
	}
	if(sm_DrawMap)
	{
		audConductorMap::DrawMap();
	}
}
//-------------------------------------------------------------------------------------------------------------------
void UnPause(void)
{
	fwTimer::EndUserPause();
	audVehicleConductor::sm_HasToPause = false;
}
//-------------------------------------------------------------------------------------------------------------------
void audVehicleConductor::AddWidgets(bkBank& bank) 
{
	bank.PushGroup("Vehicle Conductor", false);
		bank.AddToggle("Show FSM info", &sm_ShowFSMInfo);
		bank.PushGroup("Sirens",false);
		bank.AddToggle("Enable", &sm_EnableSirens);
			bank.AddToggle("Draw map", &sm_DrawMap);
			bank.AddToggle("Show sirens info", &sm_ShowSirensInfo);
			bank.AddToggle("Show vehicles with siren on", &sm_ShowVehSirenState);
			bank.AddToggle("Show targets vehicles", &sm_ShowTargets);
			bank.AddToggle("Show new targets vehicles", &sm_ShowNewTargets);
			bank.AddToggle("Play siren with no driver", &sm_PlaySirenWithNoDriver);
			bank.AddToggle("Keep playing the siren for the network player vehicles with no driver", &sm_PlaySirenWithNoNetworkPlayerDriver);
			bank.AddSlider("Time to update targets", &sm_TimeToUpdateTargets, 0, 60000,1000 );
			bank.AddSlider("Time to stop fake sirens", &sm_TimeToStopFakeSirens, 0, 60000,1000 );
			bank.AddSlider("Time to mute non-targets closest zone (s)", &sm_TimeToMuteNonTargets, 0.f, 5.f,0.1f );
			bank.AddSlider("Time to unmute non-targets closest zone (s)", &sm_TimeToUnMuteNonTargets, 0.f, 5.f,0.1f );
			bank.AddSlider("Time to mute veh mid zone (s)", &sm_TimeToMuteVehMidZone, 0.f, 5.f,0.1f );
			bank.AddSlider("Time to unmute veh mid zone (s))", &sm_TimeToUnMuteVehMidZone, 0.f, 5.f,0.1f );
			bank.AddSlider("Time to mute veh far zone (s)", &sm_TimeToMuteVehFarZone, 0.f, 15.f,0.1f );
			bank.AddSlider("Min fitness threshold to force a target change.", &sm_VehMinFitnessThreshold, 0.f, 1.f,0.01f );
			bank.AddSlider("Max fitness threshold to force a target change.", &sm_VehMaxFitnessThreshold, 0.f, 1.f,0.01f );
		bank.PopGroup();
		bank.PushGroup("Jump",true);
			bank.AddToggle("Draw results", &sm_DrawJumpResults);
			bank.AddToggle("Show jump info", &sm_ShowJumpInfo);
			bank.AddSlider("sm_DistanceTraveledThreshold", &sm_DistanceTraveledThreshold, 0.f, 100.f, 0.01f);
			bank.AddSlider("Duration of test", &sm_DurationOfTest, 0.f, 1.f, 0.01f);
			bank.AddSlider("Threshold time to trigger jump scene", &sm_ThresholdTimeToLand, 0.f, 1.f, 0.01f);
			bank.AddSlider("Threshold fitness to trigger jump scene", &sm_ThresholdFitnessToLand, 0.f, 1.f, 0.01f);
			bank.AddSlider("Land time error", &sm_LandTimeError, 0.f, 1.f, 0.01f);
			bank.AddSlider("sm_JumpConductorImpactThreshold", &sm_JumpConductorImpactThreshold, 0.f, 1000.f, 0.01f);
				bank.AddToggle("Draw trajectory", &sm_DrawTrajectory);
			bank.AddToggle("Draw velocity", &sm_DrawVelocity);
			bank.AddToggle("Show landing info", &sm_ShowLandingInfo);
			bank.AddToggle("Pause on land event", &sm_PauseOnLandEvent);
			bank.AddButton("Toggle Pause", datCallback(CFA(UnPause)));
			bank.AddSlider("Time in air threshold", &g_TimeInAirThreshold, 0, 10000, 1);
			/*	bank.PushGroup(("Pre-impact sweetneres test"));
			bank.AddToggle("Enable PreImpactSweeteners", &sm_PreImpactSweeteners);
			bank.AddSlider("Threshold time to trigger pre-impact sweetners", &sm_ThresholdTimeToBadLand, 0.f, 1.f, 0.01f);
			bank.AddSlider("Threshold fitness to trigger preImpact sweetners", &sm_ThresholdFitnessToBadLand, 0.f, 1.f, 0.01f);
			bank.PopGroup();*/
		bank.PopGroup();
		audVehicleConductorDynamicMixing::AddWidgets(bank);
		audVehicleConductorFakeScenes::AddWidgets(bank);
	bank.PopGroup();
}
#endif
