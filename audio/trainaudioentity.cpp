// 
// audio/trainaudioentity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#include "northaudioengine.h"
#include "vehicleaudioentity.h"
#include "policescanner.h"
#include "radioaudioentity.h"
#include "trainaudioentity.h"

#include "animation/animbones.h"
#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "audiohardware/driverdefs.h"
#include "audiohardware/driverutil.h"
#include "audiosoundtypes/sound.h"
#include "audiosoundtypes/soundcontrol.h"
#include "game/zones.h"
#include "crskeleton/skeleton.h"
#include "debug/debugglobals.h"
#include "grcore/debugdraw.h"
#include "modelinfo/vehiclemodelinfo.h"
#include "phbound/boundcomposite.h"
#include "physics/gtaMaterialManager.h"
#include "fwsys/timer.h"
#include "vehicles/automobile.h"
#include "vehicles/heli.h"
#include "vehicles/train.h"
#include "vehicles/vehicle.h"
#include "math/simplemath.h"
#include "debugaudio.h"

AUDIO_VEHICLES_OPTIMISATIONS()

#define AUD_TRAINANNOUNCER_ENABLED (NA_POLICESCANNER_ENABLED&&0)

const f32 g_audTrainMinDoubleWheelSpeed = 1.0f;
f32 g_CableCarClunkSpeed = 0.015f;
f32 g_MinTautWireSpeed = 0.4f;
f32 g_MaxTautWireSpeed = 0.9f;
u32 g_MinTautWireTime = 1500;
u32 g_MaxTautWireTime = 2000;
s32 g_NextTrainStartOffset = 0;
f32 g_AngleSmootherRate = 0.1f;
f32 g_BrakeVolSmootherIncreaseRate = 0.1f;
f32 g_BrakeVolSmootherDecreaseRate = 0.05f;
f32 g_TrainActivationRangeTimeScale = 2.0f;
u32 g_MaxRealTrains = 5;
f32 g_TrainActivationRange2 = 400.0f * 400.0f;
f32 g_TrainDistanceFactorMaxDist = 300.0f;

audCurve audTrainAudioEntity::sm_TrainDistanceToAttackTime;
u32 audTrainAudioEntity::sm_NumTrainsInActivationRange = 0u;
f32 audTrainAudioEntity::sm_TrainActivationRangeScale = 1.0f;
u8 audTrainAudioEntity::sm_TrackUpdateRumbleFrameIndex = 0;

extern RegdEnt g_AudioDebugEntity;
extern CTrainTrack gTrainTracks[MAX_TRAIN_TRACKS_PER_LEVEL];
extern TrainStation *g_audTrainStations[MAX_TRAIN_TRACKS_PER_LEVEL][MAX_NUM_STATIONS];
extern u32 g_SuperDummyRadiusSq;

BANK_ONLY(bool g_DebugTrain = false;)
BANK_ONLY(bool g_DebugActiveCarriages = false;)
BANK_ONLY(bool g_DebugBrakeSounds = false;)
BANK_ONLY(bool g_ForceCableCar = false;)
BANK_ONLY(f32 g_DrawCarriageInfoScroll = 0.0f;)
BANK_ONLY(f32 g_RumbleVolumeTrim = 0.0f;)
BANK_ONLY(f32 g_GrindVolumeTrim = 0.0f;)
BANK_ONLY(f32 g_SquealVolumeTrim = 0.0f;)
BANK_ONLY(f32 g_CarriageVolumeTrim = 0.0f;)
BANK_ONLY(f32 g_BrakeVolumeTrim = 0.0f;)

// ----------------------------------------------------------------
// audTrainAudioEntity constructor
// ----------------------------------------------------------------
audTrainAudioEntity::audTrainAudioEntity() : audVehicleAudioEntity()
{
	m_VehicleType = AUD_VEHICLE_TRAIN;
	m_TrainCarriage = m_TrainDriveTone = m_TrainDriveToneSynth = m_TrainGrind = m_TrainSqueal = NULL;
	m_TrainIdle = m_TrainRumble = m_TrainBrakes = m_ApproachingTrainRumble = NULL;
	m_AngleSmoother.Init(g_AngleSmootherRate);
	m_TrainDistanceFactorSmoother.Init(0.05f);
	m_BrakeVolumeSmoother.Init(g_BrakeVolSmootherIncreaseRate, g_BrakeVolSmootherDecreaseRate);
	m_TrackRumbleUpdateFrame = 0;
	m_ShockwaveId = kInvalidShockwaveId;
	m_VehicleLOD = AUD_VEHICLE_LOD_DISABLED;
}
audTrainAudioEntity::~audTrainAudioEntity() 
{
	if(m_ShockwaveId != kInvalidShockwaveId)
	{
		audNorthAudioEngine::GetGtaEnvironment()->FreeShockwave(m_ShockwaveId);
		m_ShockwaveId = kInvalidShockwaveId;
	}
}
// ----------------------------------------------------------------
// audTrainAudioEntity reset
// ----------------------------------------------------------------
void audTrainAudioEntity::Reset()
{
	audVehicleAudioEntity::Reset();
	m_TrainSpeed = 0.0f;
	m_TrainDistance = 0.0f;
	m_TrainAcceleration = 0.0f;
	m_TrainAudioSettings = NULL;
	m_TrainBrakeReleaseNextTriggerTime = 0;
	m_NumTrackNodesSearched = 0;
	m_TrainAirHornNextTriggerTime = 0;
	m_TimeElapsedTrainTurn = 0.0f;
	m_TrainLastWheelTriggerTime = 0;
	m_LastTrainAnnouncement = 0;
	m_FramesAtDesiredLOD = 0;
	m_DesiredLODLastFrame = AUD_VEHICLE_LOD_DISABLED;
	m_LastStationAnnounced = ~0U;
	m_AnnouncedCurrentStation = m_AnnouncedNextStation = -1;
	m_TrainEngine = false;
	m_LinkedBackwards = false;
	m_LinkedTrainValid = false;
	m_LinkedTrainMatrixCol0 = Vec3V(V_ZERO);
	m_LinkedTrainMatrixCol1 = Vec3V(V_ZERO);
	m_ClosestCarriageToListenerPos = Vec3V(V_ZERO);
	m_ClosestPointOnTrainToListener.Zero();
	m_RumblePositioningLineValid = false;
	m_HasPlayedBigBrakeRelease = true;
	m_TimeAtCurrentLOD = 1.0f;
	m_TrainDistanceFactorSmoother.Reset();
	m_AngleSmoother.Reset();
	m_BrakeVolumeSmoother.Reset();
	m_VehicleLOD = AUD_VEHICLE_LOD_DISABLED;
	if(m_ShockwaveId != kInvalidShockwaveId)
	{
		audNorthAudioEngine::GetGtaEnvironment()->FreeShockwave(m_ShockwaveId);
		m_ShockwaveId = kInvalidShockwaveId;
	}
}

// ----------------------------------------------------------------
// Initialise the train class
// ----------------------------------------------------------------
void audTrainAudioEntity::InitClass()
{
	StaticConditionalWarning(sm_TrainDistanceToAttackTime.Init(ATSTRINGHASH("TRAIN_DISTANCE_TO_ATTACK_TIME", 0x169D5852)), "Invalid TRAIN_DISTANCE_TO_ATTACK_TIME");
}

// ----------------------------------------------------------------
// Initialise our train settings
// ----------------------------------------------------------------
void audTrainAudioEntity::InitTrainSettings()
{
	// Init train audio settings
	m_TrainAudioSettings = GetTrainAudioSettings();

	if(m_TrainAudioSettings)
	{
		// Init curves for train audio entity
		ConditionalWarning(m_CarriagePitchCurve.Init(m_TrainAudioSettings->CarriagePitchCurve), "Invalid m_CarriagePitchCurve");
		ConditionalWarning(m_CarriageVolumeCurve.Init(m_TrainAudioSettings->CarriageVolumeCurve), "Invalid m_CarriagePitchCurve");
		ConditionalWarning(m_DriveTonePitchCurve.Init(m_TrainAudioSettings->DriveTonePitchCurve), "Invalid m_DriveTonePitchCurve");
		ConditionalWarning(m_DriveToneVolumeCurve.Init(m_TrainAudioSettings->DriveToneVolumeCurve), "Invalid m_DriveToneVolumeCurve");
		ConditionalWarning(m_DriveToneSynthPitchCurve.Init(m_TrainAudioSettings->DriveToneSynthPitchCurve), "Invalid m_DriveToneSynthPitchCurve");
		ConditionalWarning(m_DriveToneSynthVolumeCurve.Init(m_TrainAudioSettings->DriveToneSynthVolumeCurve), "Invalid m_DriveToneSynthVolumeCurve");
		ConditionalWarning(m_GrindPitchCurve.Init(m_TrainAudioSettings->GrindPitchCurve), "Invalid m_GrindPitchCurve");
		ConditionalWarning(m_GrindVolumeCurve.Init(m_TrainAudioSettings->GrindVolumeCurve), "Invalid m_GrindVolumeCurve");
		ConditionalWarning(m_TrainIdlePitchCurve.Init(m_TrainAudioSettings->TrainIdlePitchCurve), "Invalid m_IdlePitchCurve");
		ConditionalWarning(m_TrainIdleVolumeCurve.Init(m_TrainAudioSettings->TrainIdleVolumeCurve), "Invalid m_IdleVolumeCurve");
		ConditionalWarning(m_SquealPitchCurve.Init(m_TrainAudioSettings->SquealPitchCurve), "Invalid m_SquealPitchCurve");
		ConditionalWarning(m_SquealVolumeCurve.Init(m_TrainAudioSettings->SquealVolumeCurve), "Invalid m_SquealVolumeCurve");
		ConditionalWarning(m_ScrapeSpeedVolumeCurve.Init(m_TrainAudioSettings->ScrapeSpeedVolumeCurve), "Invalid m_ScrapeSpeedVolumeCurve");
		ConditionalWarning(m_WheelVolumeCurve.Init(m_TrainAudioSettings->WheelVolumeCurve), "Invalid m_WheelVolumeCurve");
		ConditionalWarning(m_WheelDelayCurve.Init(m_TrainAudioSettings->WheelVolumeCurve), "Invalid m_WheelDelayCurve");
		ConditionalWarning(m_RumbleVolumeCurve.Init(m_TrainAudioSettings->RumbleVolumeCurve), "Invalid m_RumbleVolumeCurve");
		ConditionalWarning(m_BrakeVelocityPitchCurve.Init(m_TrainAudioSettings->BrakeVelocityPitchCurve), "Invalid sm_BrakeVelocityPitchCurve");
		ConditionalWarning(m_BrakeVelocityVolumeCurve.Init(m_TrainAudioSettings->BrakeVelocityVolumeCurve), "Invalid sm_BrakeVelocityVolumeCurve");
		ConditionalWarning(m_BrakeAccelerationPitchCurve.Init(m_TrainAudioSettings->BrakeAccelerationPitchCurve), "Invalid sm_BrakeAccelerationPitchCurve");
		ConditionalWarning(m_BrakeAccelerationVolumeCurve.Init(m_TrainAudioSettings->BrakeAccelerationVolumeCurve), "Invalid sm_BrakeAccelerationVolumeCurve");
		ConditionalWarning(m_ApproachingRumbleDistanceToIntensity.Init(m_TrainAudioSettings->TrackRumbleDistanceToIntensity), "Invalid TrackRumbleDistanceToIntensityCurve");
		ConditionalWarning(m_TrainDistanceToRollOff.Init(m_TrainAudioSettings->TrainDistanceToRollOffScale), "Invalid TrainDistanceToRollOffScale");
		m_VehicleHornHash = m_TrainAudioSettings->AirHornOneShot;
	}
}

// ----------------------------------------------------------------
// Init anything train specific
// ----------------------------------------------------------------
bool audTrainAudioEntity::InitVehicleSpecific()
{
	CTrain *train = (CTrain*)m_Vehicle;

	InitTrainSettings();

	if(!m_TrainAudioSettings)
	{
		return false;
	}

	// no radio for trains
	NA_RADIO_ENABLED_ONLY(m_RadioType = RADIO_TYPE_NONE);

	// See if we're the front one
	m_TrainEngine = train->IsEngine();

	if(m_TrainEngine)
	{
		m_TrackRumbleUpdateFrame = sm_TrackUpdateRumbleFrameIndex;
		sm_TrackUpdateRumbleFrameIndex++;

		if(sm_TrackUpdateRumbleFrameIndex > 31)
		{
			sm_TrackUpdateRumbleFrameIndex = 0;
		}
	}

	SetEnvironmentGroupSettings(false, audNorthAudioEngine::GetOcclusionManager()->GetMaxOcclusionPathDepth());

#if 0
	f32 innerMap[5][3] = {	{ 0.0f,  0.0f, 0.0f},
	{ 1.8f,  8.0f, 0.0f},
	{ 1.8f, -8.0f, 0.0f},
	{-1.8f,  8.0f, 0.0f},
	{-1.8f, -8.0f, 0.0f}};

	f32 outerMap[8][3] = {	{ 2.5f,  10.0f,  2.0f},
	{ 2.5f, -10.0f,  2.0f},
	{-2.5f,  10.0f,  2.0f},
	{-2.5f, -10.0f,  2.0f},
	{ 2.5f,  10.0f, -2.0f},
	{ 2.5f, -10.0f, -2.0f},
	{-2.5f,  10.0f, -2.0f},
	{-2.5f, -10.0f, -2.0f}};

	if (m_OcclusionGroup)
	{
		m_OcclusionGroup->SetBounds(innerMap, outerMap);
	}	
#endif

	m_TrainStartOffset = g_NextTrainStartOffset;
	g_NextTrainStartOffset += 25;
	if(g_NextTrainStartOffset >= 100)
	{
		g_NextTrainStartOffset = 0;
	}

	return true;
}

// ----------------------------------------------------------------
// Post update
// ----------------------------------------------------------------
void audTrainAudioEntity::PostUpdate()
{
	audVehicleAudioEntity::PostUpdate();

	if(m_Vehicle)
	{
		CTrain *train = (CTrain*)m_Vehicle;
		CTrain* linkedTrain = NULL;

		m_LinkedBackwards = true;
		m_LinkedTrainValid = false;

		if (train->GetLinkedToBackward())
		{
			linkedTrain = train->GetLinkedToBackward();
		}
		else if (train->GetLinkedToForward())
		{
			linkedTrain = train->GetLinkedToForward();
			m_LinkedBackwards = false;
		}

		if(linkedTrain)
		{
			m_LinkedTrainValid = true;
			m_LinkedTrainMatrixCol0 = linkedTrain->GetTransform().GetA();
			m_LinkedTrainMatrixCol1 = linkedTrain->GetTransform().GetB();
		}

		if(m_TrainEngine && train->GetTrackIndex() >= 0)
		{
			CTrainTrack* trainTrack = CTrain::GetTrainTrack(train->GetTrackIndex());
			Vector3 listenerPos = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
			CTrain* closestTrain = FindClosestLinkedCarriage(listenerPos);

			if(closestTrain)
			{
				m_ClosestCarriageToListenerPos = closestTrain->GetTransform().GetPosition();

				Vector3 closestPoint;
				Vector3 forward = VEC3V_TO_VECTOR3(closestTrain->GetMatrix().b());
				Vector3 vehiclePosition = VEC3V_TO_VECTOR3(closestTrain->GetVehiclePosition());
				f32 vehicleLength = closestTrain->GetBoundSphere().GetRadiusf();

				// Carriages can be different heights (eg. engine vs. freight) so ensure that we move smoothly between them
				f32 trainHeightEnd = closestTrain->GetVehiclePosition().GetZf();
				f32 trainHeightStart = closestTrain->GetLinkedToBackward()? closestTrain->GetLinkedToBackward()->GetVehiclePosition().GetZf() : trainHeightEnd;
				fwGeom::fwLine positioningLine = fwGeom::fwLine(vehiclePosition - (forward * vehicleLength), vehiclePosition + (forward * vehicleLength));
				positioningLine.m_start.SetZ(trainHeightStart);
				positioningLine.m_end.SetZ(trainHeightEnd);
				positioningLine.FindClosestPointOnLine(listenerPos, m_ClosestPointOnTrainToListener);

				// Train tracks can be pretty long, and no need to be super accurate here so no point searching through them every frame
				if((fwTimer::GetSystemFrameCount() & 31) == m_TrackRumbleUpdateFrame)
				{
					if(trainTrack)
					{
						s32 closestNode = CTrain::FindClosestNodeOnTrack(listenerPos, CTrainTrack::GetTrackIndex(trainTrack), closestTrain->GetCurrentNode(), g_TrainDistanceFactorMaxDist, m_NumTrackNodesSearched);

						if(closestNode >= 0)
						{
							s32 prevNode = closestNode - 1;
							s32 nextNode = closestNode + 1;

							if(prevNode < 0)
							{
								prevNode = trainTrack->m_numNodes - 1;
							}

							if(nextNode >= trainTrack->m_numNodes)
							{
								nextNode = 0;
							}

							// We know the closest node, work out the 2nd closest, and the line between the two as our positioning line
							Vector3 trackCoords = trainTrack->GetNode(closestNode)->GetCoors();
							Vector3 prevCoords = trainTrack->GetNode(prevNode)->GetCoors();
							Vector3 nextCoords = trainTrack->GetNode(nextNode)->GetCoors();
							m_TrackRumblePositioningLine.Set(trackCoords, prevCoords.Dist2(listenerPos) < nextCoords.Dist2(listenerPos) ? prevCoords : nextCoords);

							// Push the start and end points back a bit. We're not updating the positioning line every frame so need it to have a bit of slack
							Vector3 normalisedVector = m_TrackRumblePositioningLine.m_end - m_TrackRumblePositioningLine.m_start;
							normalisedVector.NormalizeFast();
							normalisedVector *= 20.0f;

							m_TrackRumblePositioningLine.m_end += normalisedVector;
							m_TrackRumblePositioningLine.m_start -= normalisedVector;
							m_RumblePositioningLineValid = true;
						}
						else
						{
							m_RumblePositioningLineValid = false;
						}
					}
				}
			}
		}
	}
}

// ----------------------------------------------------------------
// Update anything train specific
// ----------------------------------------------------------------
void audTrainAudioEntity::UpdateVehicleSpecific(audVehicleVariables& vehicleVariables, audVehicleDSPSettings& UNUSED_PARAM(variables))
{
	if (!g_AudioEngine.IsAudioEnabled())
	{
		return;
	}

#if __BANK
	m_AngleSmoother.SetRate(g_AngleSmootherRate);
	m_BrakeVolumeSmoother.SetRates(g_BrakeVolSmootherIncreaseRate, g_BrakeVolSmootherDecreaseRate);
#endif

	CTrain *train = (CTrain*)m_Vehicle;
	m_TimeAtCurrentLOD += fwTimer::GetTimeStep();
	audVehicleLOD desiredLod = AUD_VEHICLE_LOD_DISABLED;

	u32 vehicleLODDistance = vehicleVariables.visibleBySniper? Min(vehicleVariables.distFromListenerSq, GetDistance2FromSniperAimVector()) : vehicleVariables.distFromListenerSq;

	if(vehicleLODDistance < g_TrainActivationRange2 * sm_TrainActivationRangeScale || m_TrainEngine)
	{
		if(!m_TrainEngine)
		{
			sm_NumTrainsInActivationRange++;
		}
		
		desiredLod = AUD_VEHICLE_LOD_REAL;
	}

	if(desiredLod == m_DesiredLODLastFrame)
	{
		m_FramesAtDesiredLOD++;
	}
	else
	{
		m_FramesAtDesiredLOD = 0;
	}

	if(desiredLod != m_VehicleLOD && 
	   m_FramesAtDesiredLOD > 3 &&
	   (m_TimeAtCurrentLOD > 1.0f || desiredLod > m_VehicleLOD))
	{
		SetLOD(desiredLod);
		m_TimeAtCurrentLOD = 0.0f;
	}

	m_DesiredLODLastFrame = desiredLod;

	if(!IsDisabled())
	{
		m_TrainDistance = Dist(m_Vehicle->GetVehiclePosition(), g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()).Getf();
		f32 rollOffScalar = 1.0f;

		if(m_TrainDistanceToRollOff.IsValid())
		{	
			rollOffScalar = m_TrainDistanceToRollOff.CalculateValue(m_TrainDistance);
		}

		if (train->CarriagesAreSuspended() BANK_ONLY(|| g_ForceCableCar))
		{
			UpdateCableCarSounds();
		}
		else
		{
			UpdateTrainAmbientSounds(rollOffScalar);
		}

		if (!train->CarriagesAreSuspended() && m_TrainEngine)
		{
			UpdateTrainEngineSounds(rollOffScalar);
		}
	}
	else
	{
		StopTrainAmbientSounds(2000);
		StopTrainEngineSounds(2000);
	}

#if __BANK
	UpdateDebug();
#endif

	bool shouldTriggerShockwave = false;

	// trigger shockwave loops for surrounding environment
	if(m_TrainEngine && train->GetTrackIndex() >= 0)
	{
		Vector3 listenerPos = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
		
		if(m_RumblePositioningLineValid || m_ApproachingTrainRumble)
		{
			Vector3 closestPoint;
			m_TrackRumblePositioningLine.FindClosestPointOnLine(listenerPos, closestPoint);
			f32 speedFactor = Clamp(Abs(train->m_trackForwardSpeed- 1.0f)/10.0f, 0.0f, 1.0f);
			f32 trainDistance = closestPoint.Dist(VEC3V_TO_VECTOR3(m_ClosestCarriageToListenerPos));
			f32 distanceFactor = m_TrainDistanceFactorSmoother.CalculateValue(m_ApproachingRumbleDistanceToIntensity.CalculateValue(trainDistance));

			shouldTriggerShockwave = !m_Vehicle->GetIsInInterior();
			CPed* pPlayer = CGameWorld::FindLocalPlayer();
			shouldTriggerShockwave = shouldTriggerShockwave || (m_Vehicle->GetIsInInterior() && pPlayer && pPlayer->GetIsInInterior() && m_Vehicle->GetAudioInteriorLocation().IsSameLocation(pPlayer->GetAudioInteriorLocation()));
			if(shouldTriggerShockwave)
			{
				// Shockwave is positioned at the closest point on the tracks, and scales up in intensity as the train approaches
				f32 distanceIntensityFactor = trainDistance/ naEnvironment::sm_TrainDistanceThreshold; 
				distanceIntensityFactor = 1.f - Min (distanceIntensityFactor, 1.f);
				audShockwave shockwave;
				shockwave.radius = naEnvironment::sm_TrainShockwaveRadius;
				shockwave.centre = closestPoint;
				shockwave.intensity = Clamp((train->m_trackForwardSpeed/10.f) * distanceIntensityFactor, 0.0f, 1.0f);
				shockwave.centered = true;
				audNorthAudioEngine::GetGtaEnvironment()->UpdateShockwave(m_ShockwaveId, shockwave);
			}

			if(!m_ApproachingTrainRumble && m_RumblePositioningLineValid)
			{
				CreateAndPlaySound_Persistent(m_TrainAudioSettings->TrainApproachTrackRumble, &m_ApproachingTrainRumble);
			}

			f32 volumeLin = speedFactor * distanceFactor;

			if(m_ApproachingTrainRumble)
			{
				if(volumeLin > 0.0f)
				{
					m_ApproachingTrainRumble->SetReleaseTime(2000);
					m_ApproachingTrainRumble->SetRequestedPosition(closestPoint);
					m_ApproachingTrainRumble->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(volumeLin));
					m_ApproachingTrainRumble->SetRequestedDopplerFactor(0.0f);
				}
				else
				{
					StopAndForgetSounds(m_ApproachingTrainRumble);
				}
			}
		}
	}
	
	if ( !shouldTriggerShockwave && m_ShockwaveId != kInvalidShockwaveId)
	{
		audNorthAudioEngine::GetGtaEnvironment()->FreeShockwave(m_ShockwaveId);
		m_ShockwaveId = kInvalidShockwaveId;
	}

	if(train->m_nTrainFlags.bEngine && train->AnnouncesStations())
	{
		if(!train->m_nTrainFlags.bAtStation && ((m_Vehicle->GetRandomSeed() + fwTimer::GetSystemFrameCount()) & 15) && train->m_nextStation > 0 && (train->m_trackIndex >= 0) && train->m_nextStation < gTrainTracks[train->m_trackIndex].m_numStations)
		{
			bool shouldAnnounce = (train->m_trackIndex >= 0) && (train->IsLocalPlayerSeatedInAnyCarriage() || ((VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()) - gTrainTracks[train->m_trackIndex].m_aStationCoors[train->m_nextStation]).Mag2() < (50.f * 50.f)));
			if(shouldAnnounce)
			{
				s32 nextStation = train->m_nextStation;
				f32 stationPosOnTrack = gTrainTracks[train->m_trackIndex].m_aStationDistFromStart[nextStation];

				//grcDebugDraw::AddDebugOutput("%d %f", train->m_nextStation, (stationPosOnTrack - train->m_trackPosFront) / train->m_linearSpeed);
				if(m_LastStationAnnounced != nextStation)
				{
					// roughly how long till we get there?
					f32 time = (stationPosOnTrack - train->m_trackPosFront) / train->m_trackForwardSpeed;

					// announce 12s before arrival
					if(time > 4.f && time < 12.f)
					{
						m_LastStationAnnounced = nextStation;
						AnnounceNextStation(nextStation);
					}
				}
			}
		}
	}

//	if(train->IsLocalPlayerSeatedInThisCarriage())
//	{
//#if DEBUG_DRAW
//		if(train->m_trackIndex >= 0)
//		{
//			grcDebugDraw::AddDebugOutput("current station: %s [%d] next station: %s [%d] Time til next station: %d", train->GetStationName(train->GetCurrentStation()), train->GetCurrentStation(), train->GetStationName(train->GetNextStation()), train->GetNextStation(), train->GetTimeTilNextStation());
//		}
//#endif // DEBUG_DRAW
//	}

	SetEnvironmentGroupSettings(false, audNorthAudioEngine::GetOcclusionManager()->GetMaxOcclusionPathDepth());
}

// ----------------------------------------------------------------
// Find the closest linked carriage to the given position
// ----------------------------------------------------------------
CTrain* audTrainAudioEntity::FindClosestLinkedCarriage(Vector3::Param position)
{
	CTrain* thisTrain = (CTrain*) m_Vehicle;
	CTrain* closestTrain = thisTrain;
	f32 closestDistSq = VEC3V_TO_VECTOR3(thisTrain->GetTransform().GetPosition()).Dist2(position);
	u32 numCarriages = thisTrain->GetNumCarriages();
	u32 iterations = 0;

	while(thisTrain->GetLinkedToForward() && iterations < numCarriages)
	{
		thisTrain = thisTrain->GetLinkedToForward();
		f32 distSq = VEC3V_TO_VECTOR3(thisTrain->GetTransform().GetPosition()).Dist2(position);

		if(distSq < closestDistSq)
		{
			closestDistSq = distSq;
			closestTrain = thisTrain;
		}

		iterations++;
	}

	thisTrain = (CTrain*) m_Vehicle;
	iterations = 0;

	while(thisTrain->GetLinkedToBackward() && iterations < numCarriages)
	{
		thisTrain = thisTrain->GetLinkedToBackward();
		f32 distSq = VEC3V_TO_VECTOR3(thisTrain->GetTransform().GetPosition()).Dist2(position);

		if(distSq < closestDistSq)
		{
			closestDistSq = distSq;
			closestTrain = thisTrain;
		}

		iterations++;
	}

	return closestTrain;
}

// ----------------------------------------------------------------
// Update activation ranges for trains
// ----------------------------------------------------------------
void audTrainAudioEntity::UpdateActivationRanges()
{
	if(sm_NumTrainsInActivationRange > g_MaxRealTrains)
	{
		sm_TrainActivationRangeScale -= fwTimer::GetTimeStep() * g_TrainActivationRangeTimeScale;
	}
	else if(sm_NumTrainsInActivationRange < g_MaxRealTrains)
	{
		sm_TrainActivationRangeScale += fwTimer::GetTimeStep() * g_TrainActivationRangeTimeScale;
	}

	sm_TrainActivationRangeScale = Clamp(sm_TrainActivationRangeScale, 0.01f, 1.0f);
	sm_NumTrainsInActivationRange = 0;
}

// ----------------------------------------------------------------
// Trigger a door open sound
// ----------------------------------------------------------------
void audTrainAudioEntity::TriggerDoorOpenSound(eHierarchyId doorId)
{
	TriggerDoorSound(ATSTRINGHASH("VEHICLES_TRAIN_SUBWAY_DOORS_OPEN", 0x11302684), doorId);
}

// ----------------------------------------------------------------
// Trigger a door shut sound
// ----------------------------------------------------------------
void audTrainAudioEntity::TriggerDoorCloseSound(eHierarchyId doorId, const bool UNUSED_PARAM(isBroken))
{
	TriggerDoorSound(ATSTRINGHASH("VEHICLES_TRAIN_SUBWAY_DOORS_CLOSE", 0x8C0B7A92), doorId);
}

// ----------------------------------------------------------------
// Generic start sequence for train sounds
// ----------------------------------------------------------------
void audTrainAudioEntity::StartTrainSound(u32 soundID, audSound** sound, audSoundInitParams* initParams, u32 releaseTime)
{
	if (!(*sound))
	{
		CreateSound_PersistentReference(soundID, sound, initParams);

		if(*sound)
		{
			(*sound)->SetReleaseTime(releaseTime);
			(*sound)->PrepareAndPlay();
		}
	}
}

// ----------------------------------------------------------------
// Update train sounds
// ----------------------------------------------------------------
void audTrainAudioEntity::UpdateTrainEngineSounds(f32 rolloffScalar)
{
	audSoundInitParams initParams;
	initParams.EnvironmentGroup = m_EnvironmentGroup;
	initParams.Tracker = m_Vehicle->GetPlaceableTracker();
	initParams.UpdateEntity = true;
	initParams.AttackTime = (u32)(sm_TrainDistanceToAttackTime.CalculateValue(m_TrainDistance) * 1000.0f);

	if(!m_TrainDriveTone)
	{
		StartTrainSound(m_TrainAudioSettings->DriveTone, &m_TrainDriveTone, &initParams, 2000);
	}

	if(!m_TrainDriveToneSynth)
	{
		StartTrainSound(m_TrainAudioSettings->DriveToneSynth, &m_TrainDriveToneSynth, &initParams, 2000);
	}

	if(!m_TrainIdle)
	{
		StartTrainSound(m_TrainAudioSettings->IdleLoop, &m_TrainIdle, &initParams, 2000);
	}

	if(!m_TrainRumble)
	{
		initParams.Tracker = NULL;
		StartTrainSound(m_TrainAudioSettings->AmbientRumble, &m_TrainRumble, &initParams, 2000);
	}
	
	if(!m_TrainBrakes)
	{
		initParams.Tracker = NULL;
		StartTrainSound(m_TrainAudioSettings->Brakes, &m_TrainBrakes, &initParams, 2000);
	}
	
	// Work out our speed
	CTrain* train = (CTrain*)m_Vehicle;
	f32 trainSpeed = 0.0f;

	trainSpeed = train->m_trackForwardSpeed;

	// m_linearSpeed is the speed along the positive direction of the trackIndex. We are interested
	// in the speed in the direction of the train however.
	if (!(train->m_nTrainFlags.bDirectionTrackForward))
	{
		trainSpeed = -trainSpeed;
	}
	
	if(m_TrainDriveTone)
	{
		f32 driveToneVolume = m_DriveToneVolumeCurve.CalculateValue(trainSpeed);
		f32 driveTonePitch = m_DriveTonePitchCurve.CalculateValue(trainSpeed);
		m_TrainDriveTone->SetRequestedVolume(driveToneVolume);
		m_TrainDriveTone->SetRequestedPitch((s32)driveTonePitch);
		m_TrainDriveTone->SetRequestedVolumeCurveScale(rolloffScalar);
	}

	if(m_TrainDriveToneSynth)
	{
		f32 driveToneSynthVolume = m_DriveToneSynthVolumeCurve.CalculateValue(trainSpeed);
		f32 driveToneSynthPitch = m_DriveToneSynthPitchCurve.CalculateValue(trainSpeed);
		m_TrainDriveToneSynth->SetRequestedVolume(driveToneSynthVolume);
		m_TrainDriveToneSynth->SetRequestedPitch((s32)driveToneSynthPitch);
		m_TrainDriveToneSynth->SetRequestedVolumeCurveScale(rolloffScalar);
	}

	if(m_TrainIdle)
	{
		f32 idleVolume = m_TrainIdleVolumeCurve.CalculateValue(trainSpeed);
		f32 idlePitch = m_TrainIdlePitchCurve.CalculateValue(trainSpeed);
		m_TrainIdle->SetRequestedVolume(idleVolume);
		m_TrainIdle->SetRequestedPitch((s32)idlePitch);
		m_TrainIdle->SetRequestedVolumeCurveScale(rolloffScalar);
	}

	if(m_TrainBrakes)
	{
		f32 brakeVolume = m_BrakeAccelerationVolumeCurve.CalculateValue(m_TrainAcceleration) + m_BrakeVelocityVolumeCurve.CalculateValue(m_TrainSpeed);
		f32 brakePitch = m_BrakeAccelerationPitchCurve.CalculateValue(m_TrainAcceleration) + m_BrakeVelocityPitchCurve.CalculateValue(m_TrainSpeed);
		f32 brakeVolumeLin = m_BrakeVolumeSmoother.CalculateValue(audDriverUtil::ComputeLinearVolumeFromDb(brakeVolume));
		m_TrainBrakes->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(brakeVolumeLin) BANK_ONLY(+ g_BrakeVolumeTrim));
		m_TrainBrakes->SetRequestedPitch((s32)brakePitch);
		m_TrainBrakes->SetRequestedPosition(m_ClosestPointOnTrainToListener);

#if __BANK
		if(g_DebugBrakeSounds)
		{
			grcDebugDraw::Sphere(m_ClosestPointOnTrainToListener, 3.f, Color32(255,255,0,255));
		}
#endif
	}

	if(m_TrainRumble)
	{
		f32 rumbleVolume = m_RumbleVolumeCurve.CalculateValue(trainSpeed);

#if __BANK
		rumbleVolume += g_RumbleVolumeTrim;
#endif

		m_TrainRumble->SetRequestedVolume(rumbleVolume);
		m_TrainRumble->SetRequestedVolumeCurveScale(rolloffScalar);
		m_TrainRumble->SetRequestedPosition(m_ClosestPointOnTrainToListener);
	}
	
	// See if we should play a one-shot big brake release - do it as soon as we come to a big halt
	if (Abs(trainSpeed) < 0.01f && !m_HasPlayedBigBrakeRelease)
	{
		m_HasPlayedBigBrakeRelease = true;
		audSoundInitParams initParams;
		initParams.Tracker = m_Vehicle->GetPlaceableTracker();
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		CreateAndPlaySound(m_TrainAudioSettings->BigBrakeRelease, &initParams);

		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(m_TrainAudioSettings->BigBrakeRelease, &initParams, m_Vehicle, m_Vehicle));
	}

	if (Abs(trainSpeed) > 1.0f)
	{
		m_HasPlayedBigBrakeRelease = false;
	}
	else
	{
		// If we're stationary, ensure that we don't play the airhorn as soon as we start moving
		m_TrainAirHornNextTriggerTime = 90000;
	}

	// Play the air horn sound, if we've not played it in a while and the train is moving.
	if (!m_IsPlayerDrivingVehicle && m_IsHornEnabled && Abs(trainSpeed) > 1.0f && m_TrainAirHornNextTriggerTime < fwTimer::GetTimeInMilliseconds() REPLAY_ONLY(&& !CReplayMgr::IsEditorActive()))
	{
		// Time to play it again
		m_TrainAirHornNextTriggerTime = fwTimer::GetTimeInMilliseconds() +
			audEngineUtil::GetRandomNumberInRange(30000, 90000);
		audSoundInitParams initParams;		
		initParams.UpdateEntity = true;
		initParams.u32ClientVar = AUD_VEHICLE_SOUND_HORN;		
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		CreateAndPlaySound(m_TrainAudioSettings->AirHornOneShot, &initParams);

		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(m_TrainAudioSettings->AirHornOneShot, &initParams, m_Vehicle));

	}
}

// ----------------------------------------------------------------
// Update train ambient sounds
// ----------------------------------------------------------------
void audTrainAudioEntity::UpdateTrainAmbientSounds(f32 rolloffScalar)
{
	// Work out our speed
	CTrain* train = (CTrain*)m_Vehicle;
	f32 trainSpeed = 0.0f;

	trainSpeed = train->m_trackForwardSpeed;

	// m_linearSpeed is the speed along the positive direction of the trackIndex. We are interested
	// in the speed in the direction of the train however.
	if (!(train->m_nTrainFlags.bDirectionTrackForward))
	{
		trainSpeed = -trainSpeed;
	}

	// See how much we're braking
	f32 trainAcceleration = (trainSpeed - m_TrainSpeed) / fwTimer::GetTimeStep();
	m_TrainSpeed = trainSpeed;
	m_TrainAcceleration = trainAcceleration;

	f32 carriageVolume = m_CarriageVolumeCurve.CalculateValue(Abs(trainSpeed));
	f32 carriagePitch = m_CarriagePitchCurve.CalculateValue(Abs(trainSpeed));

	// Work out how much we're turning - see the angle to the next carriage. This obviously won't work
	// if we only have one carriage - will change it if that ever actually happens. Looks forwards, then
	// back, so the final one isn't quieter - does mean we'll have two identical squeal params.
	f32 angle = 0.0f;
	
	if (m_LinkedTrainValid)
	{
		Vector3 ourDirection = VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetA());
		ourDirection.NormalizeFast(ourDirection);
		Vector3 nextDirection = VEC3V_TO_VECTOR3(m_LinkedTrainMatrixCol0);
		nextDirection.NormalizeFast(nextDirection);
		f32 dotAngle = ourDirection.Dot(nextDirection);
		angle = RtoD * AcosfSafe(dotAngle);
		
		// Spot any stupid angles and reverse them. This is caused by a carriage being attached backwards (eg. the driver carriages at either 
		// end of a subway train)
		if(angle >= 120)
		{
			angle = Abs(angle - 180);
		}

		Vector3 linkedLeftDirection = VEC3V_TO_VECTOR3(m_LinkedTrainMatrixCol1);
		linkedLeftDirection.NormalizeFast(linkedLeftDirection);
		f32 dotLeft = ourDirection.Dot(linkedLeftDirection);
		bool turningRight = dotLeft<0.0f;
		if (!m_LinkedBackwards)
		{
			turningRight = !turningRight;
		}
		train->m_nTrainFlags.bIsCarriageTurningRight = turningRight;
	}

	angle = m_AngleSmoother.CalculateValue(angle);
	f32 grindVolume = m_GrindVolumeCurve.CalculateValue(angle);
	f32 squealVolume = m_SquealVolumeCurve.CalculateValue(angle);
	f32 grindPitch = m_GrindPitchCurve.CalculateValue(angle);
	f32 squealPitch = m_SquealPitchCurve.CalculateValue(angle);
	f32 scrapeVolume = m_ScrapeSpeedVolumeCurve.CalculateValue(Abs(trainSpeed));

#if __BANK
	carriageVolume += g_CarriageVolumeTrim;
	grindVolume += g_GrindVolumeTrim;
	squealVolume += g_SquealVolumeTrim;
#endif

	audSoundInitParams initParams;
	initParams.EnvironmentGroup = m_EnvironmentGroup;
	initParams.Tracker = m_Vehicle->GetPlaceableTracker();
	initParams.StartOffset = m_TrainStartOffset;
	initParams.IsStartOffsetPercentage = true;
	initParams.UpdateEntity = true;
	initParams.AttackTime = (u32)(sm_TrainDistanceToAttackTime.CalculateValue(m_TrainDistance) * 1000.0f);

	if(!m_TrainCarriage)
	{
		StartTrainSound(m_TrainAudioSettings->AmbientCarriage, &m_TrainCarriage, &initParams, 2000);
	}

	if(m_TrainCarriage)
	{
		m_TrainCarriage->SetRequestedVolume(carriageVolume);
		m_TrainCarriage->SetRequestedPitch((s32)carriagePitch);
		m_TrainCarriage->SetRequestedVolumeCurveScale(rolloffScalar);
	}

	if(grindVolume + scrapeVolume > g_SilenceVolume)
	{
		if(!m_TrainGrind)
		{
			initParams.AttackTime = 100;
			CreateAndPlaySound_Persistent(m_TrainAudioSettings->AmbientGrind, &m_TrainGrind, &initParams);
		}

		if(m_TrainGrind)
		{
			m_TrainGrind->SetRequestedVolume(grindVolume+scrapeVolume);
			m_TrainGrind->SetRequestedPitch((s32)grindPitch);
			m_TrainGrind->SetRequestedVolumeCurveScale(rolloffScalar);
		}
	}
	else if(m_TrainGrind)
	{
		m_TrainGrind->StopAndForget();
	}

	if(squealVolume + scrapeVolume > g_SilenceVolume)
	{
		if(!m_TrainSqueal)
		{
			initParams.AttackTime = 100;
			CreateAndPlaySound_Persistent(m_TrainAudioSettings->AmbientSqueal, &m_TrainSqueal, &initParams);
		}

		if(m_TrainSqueal)
		{
			m_TrainSqueal->SetRequestedVolume(squealVolume+scrapeVolume);
			m_TrainSqueal->SetRequestedPitch((s32)squealPitch);
			m_TrainSqueal->SetRequestedVolumeCurveScale(rolloffScalar);
		}
	}
	else if(m_TrainSqueal)
	{
		m_TrainSqueal->StopAndForget();
	}

	// Set the squeal intensity for the particle effects
	train->m_wheelSquealIntensity = audDriverUtil::ComputeLinearVolumeFromDb(grindVolume+scrapeVolume);

	// Also play the brake release sound, if we've not played it in a while
	if (m_TrainBrakeReleaseNextTriggerTime < fwTimer::GetTimeInMilliseconds())
	{
		// Time to play it again
		m_TrainBrakeReleaseNextTriggerTime = fwTimer::GetTimeInMilliseconds() + audEngineUtil::GetRandomNumberInRange(12000, 24000);

		if(Abs(trainSpeed) > 1.0f)
		{
			audSoundInitParams initParams;
			initParams.Tracker = m_Vehicle->GetPlaceableTracker();
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			CreateAndPlaySound(m_TrainAudioSettings->BrakeRelease, &initParams);

			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(m_TrainAudioSettings->BrakeRelease, &initParams, m_Vehicle, m_Vehicle));
		}
	}
}

#if __BANK
// ----------------------------------------------------------------
// Update debug info
// ----------------------------------------------------------------
void audTrainAudioEntity::UpdateDebug()
{
	CTrain* train = (CTrain*)m_Vehicle;
	const Vector3 vTrainPosition = VEC3V_TO_VECTOR3(train->GetTransform().GetPosition());

	if(g_DebugActiveCarriages)
	{
		Color32 colour;

		if(IsDummy())
		{
			colour = Color32(0,0,255,128);
		}
		else if(IsDisabled())
		{
			colour = Color32(255,0,0,128);
		}
		else
		{
			colour = Color32(0,255,0,128);
		}

		grcDebugDraw::Sphere(vTrainPosition, 6.f, colour);
	}

	if (g_DebugTrain)
	{
		const f32 listenerDist2 = (VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition())-vTrainPosition).Mag2();

		// only do it for nearby trains, or there might be more than one
		if(listenerDist2 < (500.0f*500.0f))
		{
			char trainDebug[256] = "";

			if(m_TrainEngine)
			{
				formatf(trainDebug, "%d nodes found", m_NumTrackNodesSearched);
				grcDebugDraw::Text(Vector2(0.5f, 0.15f), Color32(255,255,255), trainDebug);
			}
			
			f32 right = 0.0f;
			if (train->m_nTrainFlags.bIsCarriageTurningRight)
			{
				right = 1.0f;
			}

			u32 carriageIndex = 0;
			const f32 carriageSpacing = 0.26f;
			f32 carriageScroll = -1.0f * g_DrawCarriageInfoScroll;
			CTrain* linkedTrain = train->GetLinkedToForward();

			while(linkedTrain)
			{
				carriageIndex++;
				linkedTrain = linkedTrain->GetLinkedToForward();
			}

			sprintf(trainDebug, "Train Info:");
			grcDebugDraw::Text(Vector2(0.1f, 0.1f + carriageScroll), Color32(255,255,255), trainDebug);

			sprintf(trainDebug, "    Carriage: %d", carriageIndex + 1);
			grcDebugDraw::Text(Vector2(0.1f, 0.12f + carriageScroll + (carriageSpacing * carriageIndex)), Color32(255,255,255), trainDebug);

			sprintf(trainDebug, "        Speed: %f", m_TrainSpeed);
			grcDebugDraw::Text(Vector2(0.1f, 0.14f + carriageScroll + (carriageSpacing * carriageIndex)), Color32(255,255,255), trainDebug);

			sprintf(trainDebug, "        Angle: %f", m_AngleSmoother.GetLastValue());
			grcDebugDraw::Text(Vector2(0.1f, 0.16f + carriageScroll + (carriageSpacing * carriageIndex)), Color32(255,255,255), trainDebug);

			sprintf(trainDebug, "        Intensity: %f", train->m_wheelSquealIntensity);
			grcDebugDraw::Text(Vector2(0.1f, 0.18f + carriageScroll + (carriageSpacing * carriageIndex)), Color32(255,255,255), trainDebug);

			sprintf(trainDebug, "        Right: %02f", right);
			grcDebugDraw::Text(Vector2(0.1f, 0.2f + carriageScroll + (carriageSpacing * carriageIndex)), Color32(255,255,255), trainDebug);

			sprintf(trainDebug, "        Acc: %f", m_TrainAcceleration);
			grcDebugDraw::Text(Vector2(0.1f, 0.22f + carriageScroll + (carriageSpacing * carriageIndex)), Color32(255,255,255), trainDebug);

			if(m_TrainGrind)
			{
				sprintf(trainDebug, "        Grind Sound - Vol: %02f Pitch: %d", m_TrainGrind->GetRequestedVolume(), m_TrainGrind->GetRequestedPitch());
				grcDebugDraw::Text(Vector2(0.1f, 0.24f + carriageScroll + (carriageSpacing * carriageIndex)), Color32(255,255,255), trainDebug);
			}
			
			if(m_TrainSqueal)
			{
				sprintf(trainDebug, "        Squeal Sound - Vol: %02f Pitch: %d", m_TrainSqueal->GetRequestedVolume(), m_TrainSqueal->GetRequestedPitch());
				grcDebugDraw::Text(Vector2(0.1f, 0.26f + carriageScroll + (carriageSpacing * carriageIndex)), Color32(255,255,255), trainDebug);
			}
			
			if(m_TrainRumble)
			{
				sprintf(trainDebug, "        Rumble Sound - Vol: %02f", m_TrainRumble->GetRequestedVolume());
				grcDebugDraw::Text(Vector2(0.1f, 0.28f + carriageScroll + (carriageSpacing * carriageIndex)), Color32(255,255,255), trainDebug);
			}

			if(m_TrainCarriage)
			{
				sprintf(trainDebug, "        Carriage Sound - Vol: %02f Pitch: %d", m_TrainCarriage->GetRequestedVolume(), m_TrainCarriage->GetRequestedPitch());
				grcDebugDraw::Text(Vector2(0.1f, 0.30f + carriageScroll + (carriageSpacing * carriageIndex)), Color32(255,255,255), trainDebug);
			}

			if(m_TrainBrakes)
			{
				sprintf(trainDebug, "        Brake Sound - Vol: %02f Pitch: %d", m_TrainBrakes->GetRequestedVolume(), m_TrainBrakes->GetRequestedPitch());
				grcDebugDraw::Text(Vector2(0.1f, 0.32f + carriageScroll + (carriageSpacing * carriageIndex)), Color32(255,255,255), trainDebug);
			}
		}
	}
}
#endif

// ----------------------------------------------------------------
// Update cable car sounds
// ----------------------------------------------------------------
void audTrainAudioEntity::UpdateCableCarSounds()
{
	// Work out our speed
	CTrain* train = (CTrain*)m_Vehicle;
	f32 trainSpeed = Abs(train->m_trackForwardSpeed);

	// See how much we're braking
	f32 trainAcceleration = (trainSpeed - m_TrainSpeed) / fwTimer::GetTimeStep();
	m_TrainSpeed = trainSpeed;
	m_TrainAcceleration = trainAcceleration;

	// should we play taut-wire stuff - do it with velocity
	if (m_TrainSpeed > g_MinTautWireSpeed && m_TrainSpeed < g_MaxTautWireSpeed)
	{
		// Play the taught-wire sound, if we've not played it in a while - use brake release vars
		if (m_TrainBrakeReleaseNextTriggerTime < fwTimer::GetTimeInMilliseconds())
		{
			// Time to play it again
			m_TrainBrakeReleaseNextTriggerTime = fwTimer::GetTimeInMilliseconds() + 
			audEngineUtil::GetRandomNumberInRange((s32)g_MinTautWireTime, (s32)g_MaxTautWireTime);
			audSoundInitParams initParams;
			initParams.Tracker = m_Vehicle->GetPlaceableTracker();
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			CreateAndPlaySound(ATSTRINGHASH("CABLE_CAR_TAUT_CABLE", 0xE3C41DE7), &initParams);

			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(ATSTRINGHASH("CABLE_CAR_TAUT_CABLE", 0xE3C41DE7), &initParams, m_Vehicle, m_Vehicle));
		}
	}

	// See if we should play a one-shot bang - do it as soon as we come to a big halt, and use same logic/vars as big train brake release
	if (Abs(m_TrainSpeed) < g_CableCarClunkSpeed && !m_HasPlayedBigBrakeRelease)
	{
		m_HasPlayedBigBrakeRelease = true;
		audSoundInitParams initParams;
		initParams.Tracker = m_Vehicle->GetPlaceableTracker();
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		CreateAndPlaySound(ATSTRINGHASH("CABLE_CAR_CLUNK", 0xF35B9BC8), &initParams);

		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(ATSTRINGHASH("CABLE_CAR_CLUNK", 0xF35B9BC8), &initParams, m_Vehicle, m_Vehicle));
	}
	if (Abs(trainSpeed) > 1.0f)
	{
		m_HasPlayedBigBrakeRelease = false;
	}
	// DEBUG!
#if __BANK
	if (g_DebugTrain && m_Vehicle == g_AudioDebugEntity)
	{
		char trainDebug[256] = "";
		sprintf(trainDebug, "speed: %f;", trainSpeed);
		grcDebugDraw::PrintToScreenCoors(trainDebug, 5,30);
	}
#endif
}

// ----------------------------------------------------------------
// Stop train engine sounds
// ----------------------------------------------------------------
void audTrainAudioEntity::StopTrainEngineSounds(u32 releaseTime)
{
	if(m_TrainDriveTone)
	{
		m_TrainDriveTone->SetReleaseTime(releaseTime);
	}

	if(m_TrainDriveToneSynth)
	{
		m_TrainDriveToneSynth->SetReleaseTime(releaseTime);
	}

	if(m_TrainIdle)
	{
		m_TrainIdle->SetReleaseTime(releaseTime);
	}

	if(m_TrainRumble)
	{
		m_TrainRumble->SetReleaseTime(releaseTime);
	}

	if(m_TrainBrakes)
	{
		m_TrainBrakes->SetReleaseTime(releaseTime);
	}

	StopAndForgetSounds(m_TrainDriveTone, m_TrainDriveToneSynth, m_TrainRumble, m_TrainIdle, m_TrainBrakes);
}

// ----------------------------------------------------------------
// Stop train ambient sounds
// ----------------------------------------------------------------
void audTrainAudioEntity::StopTrainAmbientSounds(u32 releaseTime)
{
	if(m_TrainCarriage)
	{
		m_TrainCarriage->SetReleaseTime(releaseTime);
	}

	if(m_TrainGrind)
	{
		m_TrainGrind->SetReleaseTime(releaseTime);
	}

	if(m_TrainSqueal)
	{
		m_TrainSqueal->SetReleaseTime(releaseTime);
	}

	StopAndForgetSounds(m_TrainCarriage, m_TrainGrind, m_TrainSqueal);
}

// ----------------------------------------------------------------
// Called when moving track nodes
// ----------------------------------------------------------------
void audTrainAudioEntity::TrainHasMovedTrackNode()
{
	CTrain* train = (CTrain*)m_Vehicle; 

	if(!m_TrainAudioSettings)
	{
		return;
	}

	// No clacks for dummy carriages
	if(GetVehicleLOD() != AUD_VEHICLE_LOD_REAL)
	{
		return;
	}

	// We're maybe triggering these way too much (like every frame) - presumably due to a bug somewhere else. So check when we last triggered.
	u32 timeInMs = fwTimer::GetTimeInMilliseconds();
	if (timeInMs < (m_TrainLastWheelTriggerTime + 2000))
	{
		return;
	}
	m_TrainLastWheelTriggerTime = timeInMs;

	// Work out our speed
	f32 trainSpeed = 0.0f;

	trainSpeed = train->m_trackForwardSpeed;

	// m_linearSpeed is the speed along the positive direction of the trackIndex. We are interested
	// in the speed in the direction of the train however.
	if (!(train->m_nTrainFlags.bDirectionTrackForward))
	{
		trainSpeed = -trainSpeed;
	}
	f32 wheelVolume = m_WheelVolumeCurve.CalculateValue(trainSpeed);
	audSoundInitParams initParams;
	initParams.Tracker = m_Vehicle->GetPlaceableTracker();
	initParams.EnvironmentGroup = m_EnvironmentGroup;
	initParams.Volume = wheelVolume;
	CreateAndPlaySound(m_TrainAudioSettings->WheelDry, &initParams);
	
	REPLAY_ONLY(CReplayMgr::ReplayRecordSound(m_TrainAudioSettings->WheelDry, &initParams, m_Vehicle, m_Vehicle));

	// We want to play that characteristic two-wheels close together thing, with the gap decreasing with speed;
	// But don't do it if we're traveling TOO slowly, as we might have stopped altogether by the time of the second one.
	if (trainSpeed>g_audTrainMinDoubleWheelSpeed)
	{
		f32 wheelDelay = m_WheelDelayCurve.CalculateValue(trainSpeed);
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		initParams.Tracker = m_Vehicle->GetPlaceableTracker();
		initParams.Volume = wheelVolume;
		initParams.Predelay = (s32)wheelDelay;
		CreateAndPlaySound(m_TrainAudioSettings->WheelDry, &initParams);

		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(m_TrainAudioSettings->WheelDry, &initParams, m_Vehicle, m_Vehicle));

		train->SignalTrainPadShake((u32)wheelDelay);
	}
	else
	{
		train->SignalTrainPadShake();
	}
}

// ----------------------------------------------------------------
// Set the train rolloff
// ----------------------------------------------------------------
void audTrainAudioEntity::SetTrainRolloff(f32 rolloff)
{
	if (m_EnvironmentGroup)
	{
		m_EnvironmentGroup->GetEnvironmentGameMetric().SetRolloffFactor(rolloff);
	}
}

// ----------------------------------------------------------------
// Check if the train should play station announcements
// ----------------------------------------------------------------
bool audTrainAudioEntity::ShouldPlayStationAnnouncements()
{
	bool isInteriorASubway;
	f32 playerInteriorRatio;
	bool isInInterior = audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior(&isInteriorASubway, &playerInteriorRatio);
	bool isInMahattan = false; // CMapAreas::IsPositionInNamedArea(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()), atStringhash("Manhattan"));

	const Vector3 northwoodCoors(-368.f, 1414.f, 28.f);
	return ((isInMahattan && isInInterior && isInteriorASubway) || 
		(!isInMahattan && !isInInterior) || 
		(isInMahattan && !isInInterior && (VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition())-northwoodCoors).Mag2() < (70.f*70.f)));
}

u32 audTrainAudioEntity::GetVehicleHornSoundHash(bool UNUSED_PARAM(ignoreMods)) 
{
	return m_VehicleHornHash;
};

// ----------------------------------------------------------------
// Announce the train arriving at a station
// ----------------------------------------------------------------
void audTrainAudioEntity::AnnounceTrainAtStation()
{
#if AUD_TRAINANNOUNCER_ENABLED
	Assert(((CTrain*)m_Vehicle)->m_trackIndex >= 0);

	CTrain *train = (CTrain*)m_Vehicle;
	naCErrorf(train->m_nTrainFlags.bEngine, "Carrige other than the engine is attempting to announce the train is at the station");
	// update last station announced regardless of player
	m_LastStationAnnounced = train->m_nextStation;
	if(m_AnnouncedCurrentStation != train->m_nextStation)
	{
		m_AnnouncedCurrentStation = train->m_nextStation;
		if(train->AnnouncesStations() && train->IsLocalPlayerSeatedInAnyCarriage())
		{
			s32 stationId = train->m_nextStation;
			if(stationId != -1)
			{
				TrainStation *station = g_audTrainStations[train->m_trackIndex][stationId];
				if(station)
				{
					audScannerSentence *sentence = g_AudioScannerManager.AllocateSentence();
					InitScannerSentenceForTrain(sentence);

					u32 idx = 0;

					sentence->sentenceSubType = 1;
					sentence->phrases[idx].postDelay = 0;
					sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_3;
					sentence->phrases[idx].sound = NULL;
					sentence->phrases[idx++].soundHash = ATSTRINGHASH("ANNOUNCE_ARRIVED", 0xF3ED0B0E);

					sentence->phrases[idx].postDelay = 0;
					sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_2;
					sentence->phrases[idx].sound = NULL;
					sentence->phrases[idx++].soundHash = station->StationNameSound;

					sentence->numPhrases = idx;
					sentence->isPositioned = false;	
					sentence->timeRequested = fwTimer::GetTimeInMilliseconds() + 1000;
				}
			}
		}
	}
#endif // NA_POLICECSANNER_ENABLED
}

// ----------------------------------------------------------------
// Announce the train leaving a station
// ----------------------------------------------------------------
void audTrainAudioEntity::AnnounceTrainLeavingStation()
{
#if AUD_TRAINANNOUNCER_ENABLED

	Assert(((CTrain*)m_Vehicle)->m_trackIndex >= 0);

	// TRAIN_LEAVING_STATION
	CTrain *engineTrain = (CTrain*)m_Vehicle;
	if(!engineTrain->m_nTrainFlags.bDisableNextStationAnnouncements && engineTrain->AnnouncesStations() && fwTimer::GetTimeInMilliseconds() > m_LastTrainAnnouncement + 10000)
	{
		audScannerSentence *sentence = g_AudioScannerManager.AllocateSentence();
		InitScannerSentenceForTrain(sentence);

		u32 idx = 0;

		sentence->phrases[idx].postDelay = 0;
		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_1;
		sentence->phrases[idx].sound = NULL;
		sentence->phrases[idx++].soundHash = ATSTRINGHASH("TRAIN_LEAVING_STATION", 0x44C98442);

		sentence->timeRequested = fwTimer::GetTimeInMilliseconds() + 2000;
		sentence->numPhrases = idx;
		// position at the train
		sentence->tracker = m_Vehicle->GetPlaceableTracker();
		sentence->occlusionGroup = m_EnvironmentGroup;
		sentence->isPositioned = true;	

		m_LastTrainAnnouncement = fwTimer::GetTimeInMilliseconds();


		if(engineTrain->IsLocalPlayerSeatedInAnyCarriage())
		{
			f32 posOnTrack;
			s32 stationId;
			engineTrain->FindNextStationPositionInDirection(engineTrain->m_nTrainFlags.bDirectionTrackForward, engineTrain->m_trackPosFront, &posOnTrack, &stationId);

			if(stationId != -1)
			{	
				TrainStation *station = g_audTrainStations[engineTrain->m_trackIndex][stationId];

				if(station)
				{
					m_AnnouncedNextStation = stationId;

					audScannerSentence *sentence = g_AudioScannerManager.AllocateSentence();
					InitScannerSentenceForTrain(sentence);

					u32 idx = 0;


					if(station->Letter != g_NullSoundHash)
					{
						sentence->phrases[idx].postDelay = 750;
						sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_1;
						sentence->phrases[idx].sound = NULL;
						sentence->phrases[idx++].soundHash = station->Letter;
					}

					sentence->sentenceSubType = 1;
					sentence->phrases[idx].postDelay = 0;
					sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_3;
					sentence->phrases[idx].sound = NULL;
					sentence->phrases[idx++].soundHash = ATSTRINGHASH("ANNOUNCE_NEXT_STOP", 0x5FE7B7DE);

					sentence->phrases[idx].postDelay = 0;
					sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_2;
					sentence->phrases[idx].sound = NULL;
					sentence->phrases[idx++].soundHash = station->StationNameSound;

					sentence->numPhrases = idx;
					sentence->isPositioned = false;	

					sentence->timeRequested = fwTimer::GetTimeInMilliseconds();
				}
			}
		}
	}
#endif // AUD_TRAINANNOUNCER_ENABLED
}

// ----------------------------------------------------------------
// Announce an upcoming station
// ----------------------------------------------------------------
void audTrainAudioEntity::AnnounceNextStation(s32 UNUSED_PARAM(stationId))
{
#if AUD_TRAINANNOUNCER_ENABLED
	Assert(((CTrain*)m_Vehicle)->m_trackIndex >= 0);

	CTrain *train = (CTrain*)m_Vehicle;
	TrainStation *station = g_audTrainStations[train->m_trackIndex][stationId];

	if(train->AnnouncesStations() && station)
	{
		if(train->IsLocalPlayerSeatedInAnyCarriage())
		{
			audScannerSentence *sentence = g_AudioScannerManager.AllocateSentence();
			InitScannerSentenceForTrain(sentence);

			u32 idx = 0;

			sentence->sentenceSubType = 1;
			sentence->phrases[idx].postDelay = 0;
			sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_3;
			sentence->phrases[idx].sound = NULL;
			sentence->phrases[idx++].soundHash = ATSTRINGHASH("ANNOUNCE_APPROACHING", 0x1C60D164);

			sentence->phrases[idx].postDelay = 0;
			sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_2;
			sentence->phrases[idx].sound = NULL;
			sentence->phrases[idx++].soundHash = station->StationNameSound;

			if(station->TransferSound != g_NullSoundHash)
			{
				sentence->phrases[idx-1].postDelay = 800;

				sentence->phrases[idx].postDelay = 0;
				sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_1;
				sentence->phrases[idx].sound = NULL;
				sentence->phrases[idx++].soundHash = station->TransferSound;
			}
			sentence->numPhrases = idx;
			sentence->isPositioned = false;	
			sentence->timeRequested = fwTimer::GetTimeInMilliseconds();
		}
		else
		{
			if(ShouldPlayStationAnnouncements())
			{
				s32 prevStationId = stationId + (train->m_nTrainFlags.bDirectionTrackForward?-1:1);
				s32 nextStationId = stationId + (train->m_nTrainFlags.bDirectionTrackForward?1:-1);
				if(prevStationId == -1)
				{
					prevStationId = gTrainTracks[train->m_trackIndex].m_numStations-1;
				}
				if(prevStationId >= gTrainTracks[train->m_trackIndex].m_numStations)
				{
					prevStationId = 0;
				}
				if(nextStationId == -1)
				{
					nextStationId = gTrainTracks[train->m_trackIndex].m_numStations-1;
				}
				if(nextStationId >= gTrainTracks[train->m_trackIndex].m_numStations)
				{
					nextStationId = 0;
				}
				naCErrorf(stationId != prevStationId, "While announcing the next station, current and previous stations are the same");
				naCErrorf(stationId != nextStationId, "While announcing the next station, current and next stations are the same");

				TrainStation *prevStation = g_audTrainStations[train->m_trackIndex][prevStationId];
				TrainStation *nextStation = g_audTrainStations[train->m_trackIndex][nextStationId];
				if(prevStation && nextStation)
				{
					audScannerSentence *sentence = g_AudioScannerManager.AllocateSentence();
					InitScannerSentenceForTrain(sentence);
					u32 idx = 0;

					sentence->phrases[idx].postDelay = 0;
					sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_3;
					sentence->phrases[idx].sound = NULL;
					sentence->phrases[idx++].soundHash = station->ApproachingStation;

					sentence->phrases[idx].postDelay = 0;
					sentence->phrases[idx].slot = g_AudioScannerManager.GetNextSmallScannerSlot(*sentence); //SCANNER_SMALL_SLOT_1;
					sentence->phrases[idx].sound = NULL;
					sentence->phrases[idx++].soundHash = ATSTRINGHASH("ANNOUNCE_BOUND_FOR", 0x49E7982);

					sentence->phrases[idx].postDelay = 0;
					sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_2;
					sentence->phrases[idx].sound = NULL;
					sentence->phrases[idx++].soundHash = nextStation->StationNameSound;

					sentence->numPhrases = idx;
					sentence->isPositioned = true;
					sentence->fakeEchoTime = 100;
					sentence->position = gTrainTracks[train->m_trackIndex].m_aStationCoors[stationId];
					sentence->position.z += 1.5f;
					sentence->occlusionGroup = NULL;

					sentence->timeRequested = fwTimer::GetTimeInMilliseconds();
				}
			}
		}
	}
#endif // AUD_TRAINANNOUNCER_ENABLED
}

#if AUD_TRAINANNOUNCER_ENABLED
// ----------------------------------------------------------------
// audTrainAudioEntity::InitScannerSentenceForTrain
// ----------------------------------------------------------------
void audTrainAudioEntity::InitScannerSentenceForTrain(audScannerSentence *sentence)
{
	sentence->areaHash = 0;
	sentence->backgroundSFXHash = g_NullSoundHash;
	sentence->currentPhrase = 0;
	sentence->shouldDuckRadio = false;
	sentence->predelay = 0;
	sentence->inUse = true;
	sentence->isPositioned = false;
	sentence->sentenceType = AUD_SCANNER_TRAIN;
	sentence->sentenceSubType = 0;
	sentence->priority = AUD_SCANNER_PRIO_CRITICAL;
	sentence->tracker = NULL;
	sentence->category = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("GENERAL_TRAIN_ANNOUNCER", 0x354F997A));
	sentence->fakeEchoTime = 0;
	sentence->useRadioVolOffset = false;
	for(u32 i = 0; i < g_MaxPhrasesPerSentence; i++)
	{
		sentence->phrases[i].echoSound = sentence->phrases[i].sound = NULL;
	}
}

// ----------------------------------------------------------------
// audTrainAudioEntity::UpdateAmbientTrainStations
// ----------------------------------------------------------------
void audTrainAudioEntity::UpdateAmbientTrainStations()
{
	u32 dist2[MAX_TRAIN_TRACKS_PER_LEVEL][MAX_NUM_STATIONS];

	if(!ShouldPlayStationAnnouncements())
	{
		return;
	}

	Vector3 listenerPos = g_AudioEngine.GetEnvironment().GetVolumeListenerPosition();
	// only search bronx and queens tracks
	for(s8 trackIndex = 0; trackIndex < MAX_TRAIN_TRACKS_PER_LEVEL; trackIndex+=2)
	{
		for(s32 station = 0; station < gTrainTracks[trackIndex].m_numStations; station++)
		{
			dist2[trackIndex][station] = static_cast<u32>((listenerPos - gTrainTracks[trackIndex].m_aStationCoors[station]).Mag2());
		}
	}

	u32 minDist2 = ~0U;
	s32 nearestStation = -1;
	s32 nearestTrackIndex = -1;
	for(s8 trackIndex = 0; trackIndex < MAX_TRAIN_TRACKS_PER_LEVEL; trackIndex+=2)
	{
		for(s32 station = 0; station < gTrainTracks[trackIndex].m_numStations; station++)
		{
			if(dist2[trackIndex][station] < minDist2)
			{
				minDist2 = dist2[trackIndex][station];
				nearestStation = station;
				nearestTrackIndex = trackIndex;
			}
		}
	}

	if(minDist2 < (100*100))
	{
		audScannerSentence *sentence = g_AudioScannerManager.AllocateSentence();
		InitScannerSentenceForTrain(sentence);
		u32 idx = 0;

		if(!g_audTrainStations[nearestTrackIndex][nearestStation] || audEngineUtil::ResolveProbability(0.7f))
		{
			sentence->phrases[idx].postDelay = 0;
			sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_1;
			sentence->phrases[idx].sound = NULL;
			s32 var = audEngineUtil::GetRandomNumberInRange(1, 15);
			char soundName[96];
			formatf(soundName, sizeof(soundName), "POLICE_SCANNER_WARNINGS_SUBWAY_WARNING_%02d", var);
			sentence->phrases[idx++].soundHash = atStringHash(soundName);
		}
		else
		{
			sentence->phrases[idx].postDelay = 0;
			sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_3;
			sentence->phrases[idx].sound = NULL;
			sentence->phrases[idx++].soundHash = ATSTRINGHASH("ANNOUNCE_ARRIVED", 0xF3ED0B0E);


			sentence->phrases[idx].postDelay = 0;
			sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_2;
			sentence->phrases[idx].sound = NULL;
			sentence->phrases[idx++].soundHash = g_audTrainStations[nearestTrackIndex][nearestStation]->StationNameSound;

		}

		sentence->numPhrases = idx;
		sentence->isPositioned = true;
		sentence->fakeEchoTime = 100;
		sentence->position = gTrainTracks[nearestTrackIndex].m_aStationCoors[nearestStation];
		sentence->position.z += 1.5f;
		sentence->occlusionGroup = NULL;
		//sentence->priority = AUD_SCANNER_PRIO_NORMAL;

		sentence->timeRequested = fwTimer::GetTimeInMilliseconds();
	}
}
#endif // AUD_TRAINANNOUNCER_ENABLED

// ----------------------------------------------------------------
// audTrainAudioEntity::GetTrainAudioSettings
// ----------------------------------------------------------------
TrainAudioSettings * audTrainAudioEntity::GetTrainAudioSettings()
{
	if(!g_AudioEngine.IsAudioEnabled())
	{
		return NULL;
	}

	naAssertf(m_Vehicle->InheritsFromTrain(), "Trying to get a train settings from a vehicle that doesn't have a train");
	TrainAudioSettings *settings = NULL;

	if(m_ForcedGameObject != 0u)
	{
		settings = audNorthAudioEngine::GetObject<TrainAudioSettings>(m_ForcedGameObject);
		m_ForcedGameObject = 0u;
	}

	if(!settings)
	{
		if(m_Vehicle->GetVehicleModelInfo()->GetAudioNameHash() != 0)
		{
			settings = audNorthAudioEngine::GetObject<TrainAudioSettings>(m_Vehicle->GetVehicleModelInfo()->GetAudioNameHash());
		}

		if(!settings)
		{
			settings = audNorthAudioEngine::GetObject<TrainAudioSettings>(GetVehicleModelNameHash());
		}
	}

	if(!settings)
	{
		if(!audVerifyf(settings, "Couldn't find train settings for train: %s", GetVehicleModelName()))
		{
			settings = audNorthAudioEngine::GetObject<TrainAudioSettings>(ATSTRINGHASH("SUBWAY_TRAIN", 0xE6C118FD));
		}
	}

	return settings;
}

VehicleCollisionSettings* audTrainAudioEntity::GetVehicleCollisionSettings() const
{
	if(m_TrainAudioSettings)
	{
		return audNorthAudioEngine::GetObject<VehicleCollisionSettings>(m_TrainAudioSettings->VehicleCollisions);
	}

	return audVehicleAudioEntity::GetVehicleCollisionSettings();
}

#if __BANK
// ----------------------------------------------------------------
// Add widgets to RAG tool
// ----------------------------------------------------------------
void audTrainAudioEntity::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Trains",false);
	bank.AddToggle("Debug Train", &g_DebugTrain);
	bank.AddToggle("Show Active Carriages", &g_DebugActiveCarriages);
	bank.AddToggle("Show Brake Sounds", &g_DebugBrakeSounds);
	bank.AddToggle("Force Cable Car", &g_ForceCableCar);
	bank.AddSlider("Max Real Trains", &g_MaxRealTrains, 0, 20, 1);
	bank.AddSlider("Activation Range Time Scaler", &g_TrainActivationRangeTimeScale, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("Debug Scroll", &g_DrawCarriageInfoScroll, 0.0f, 1.0f, 0.1f, NullCB, "Use this to scroll the carriage list" );
	bank.AddSlider("Grind Volume Trim", &g_GrindVolumeTrim, -100.0f, 100.f, 0.1f);
	bank.AddSlider("Squeal Volume Trim", &g_SquealVolumeTrim, -100.0f, 100.f, 0.1f);
	bank.AddSlider("Rumble Volume Trim", &g_RumbleVolumeTrim, -100.0f, 100.f, 0.1f);
	bank.AddSlider("Carriage Volume Trim", &g_CarriageVolumeTrim, -100.0f, 100.f, 0.1f);
	bank.AddSlider("Brake Volume Trim", &g_BrakeVolumeTrim, -100.0f, 100.f, 0.1f);
	bank.AddSlider("Angle Smoothing Rate", &g_AngleSmootherRate, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("Brake Volume Smoothing Increase Rate", &g_BrakeVolSmootherIncreaseRate, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("Brake Volume Smoothing Decrease Rate", &g_BrakeVolSmootherDecreaseRate, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("g_CableCarClunkSpeed", &g_CableCarClunkSpeed, 0.0f, 1.0f, 0.001f);
	bank.AddSlider("g_MinTautWireSpeed", &g_MinTautWireSpeed, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("g_MaxTautWireSpeed", &g_MaxTautWireSpeed, 0.0f, 10.0f, 0.01f);
	bank.AddSlider("g_MinTautWireTime", &g_MinTautWireTime, 0, 10000, 1);
	bank.AddSlider("g_MaxTautWireTime", &g_MaxTautWireTime, 0, 10000, 1);
	bank.PopGroup();
}
#endif
