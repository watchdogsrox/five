//
// scene/ContinuityMgr.cpp
//
// Copyright (C) 1999-2012 Rockstar Games. All Rights Reserved.
//

#include "scene/ContinuityMgr.h"

#include "camera/camInterface.h"
#include "camera/base/basecamera.h"
#include "camera/viewports/ViewportManager.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/debug/DebugDirector.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "objects/object.h"
#include "peds/Ped.h"
#include "peds/PedFlagsMeta.h"
#include "peds/PedIntelligence.h"
#include "peds/PlayerInfo.h"
#include "peds/QueriableInterface.h"
#include "scene/lod/LodScale.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/streamer/SceneStreamerBase.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/world/GameWorld.h"
#include "scene/world/GameWorldHeightMap.h"
#include "streaming/streamingengine.h"
#include "streaming/streaminginfo.h"
#include "task/Movement/TaskFall.h"
#include "task/System/Task.h"
#include "task/System/TaskTypes.h"
#include "task/Movement/TaskParachute.h"
#include "vehicles/vehicle.h"
#include "vehicles/AmphibiousAutomobile.h"
#include "world/GameWorld.h"

CContinuityMgr g_ContinuityMgr;
static spdAABB s_approxCityBounds;

#include "data/aes_init.h"
AES_INIT_A;

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Reset
// PURPOSE:		clears state to defaults
//////////////////////////////////////////////////////////////////////////
void CContinuityState::Reset()
{
	m_vPos = Vec3V(V_ZERO);
	m_vDir = Vec3V(V_ZERO);
	m_fVelocity = 0.0f;
	m_fLodScale = 1.0f;
	m_bAimingFirstPerson = false;
	m_bInCarView = false;
	m_bUsingNonGameCamera = false;
	m_bIsPlayerFlyingAircraft = false;
	m_bIsPlayerDrivingTrain = false;
	m_bAboveHeightMap = false;
	m_bIsPlayerDrivingGroundVehicle = false;
	m_bIsPlayerInCityArea = false;
	m_bIsPlayerSkyDiving = false;
	m_bLookingBackwards = false;
	m_bLookingAwayFromDirectionOfMovement = false;
	m_bIsPlayerOnFoot = false;
	m_bIsPlayerAiming = false;
	m_bIsAnyFirstPersonCamera = false;
	m_bSceneStreamingUnderVeryHeavyPressure = false;

	m_fHighestRequiredSceneScore = 0.0f;
	m_eStreamingPressure = SCENE_STREAMING_PRESSURE_LIGHT;	
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		retrieve latest state from camera, renderer etc
//////////////////////////////////////////////////////////////////////////
void CContinuityState::Update()
{
	const Vector3 vPlayerVelocity = CGameWorld::FindFollowPlayerSpeed();

	m_vPos = VECTOR3_TO_VEC3V(camInterface::GetPos());
	m_vDir = VECTOR3_TO_VEC3V(camInterface::GetFront());
	m_fLodScale = g_LodScale.GetGlobalScale();
	m_fVelocity = fabsf( vPlayerVelocity.Mag() );

	{
		// detect when camera is looking away from direction of movement
		// for example when a passenger in a car and looking sideways
		Vector3 vVelocityDir = vPlayerVelocity;
		Vector3 vCamDir = camInterface::GetFront();

		vCamDir.NormalizeSafe();
		vVelocityDir.NormalizeSafe();

		const float fThresholdAngle = 0.3f;
		m_bLookingAwayFromDirectionOfMovement = vCamDir.Dot(vVelocityDir) <= fThresholdAngle;
	}

	camGameplayDirector& gameplayDirector = camInterface::GetGameplayDirector();
	m_bAimingFirstPerson = gameplayDirector.IsFirstPersonAiming();

	bool isRenderingPovCamera = false;
	const camBaseCamera* dominantCamera = camInterface::GetDominantRenderedCamera();
	if (dominantCamera && dominantCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
	{
		const camCinematicMountedCamera* povCamera = static_cast<const camCinematicMountedCamera*>(dominantCamera);
		isRenderingPovCamera = povCamera->IsFirstPersonCamera();
	}
	m_bInCarView = isRenderingPovCamera;

	const camBaseCamera* renderedGameplayCamera = gameplayDirector.GetRenderedCamera();
	m_bUsingNonGameCamera = !(renderedGameplayCamera && camInterface::IsDominantRenderedCamera(*renderedGameplayCamera)) && !isRenderingPovCamera;

	m_bSwitching = g_PlayerSwitch.IsActive();

	m_bIsPlayerFlyingAircraft = false;
	m_bIsPlayerDrivingTrain = false;
	m_bIsPlayerDrivingGroundVehicle = false;
	m_bIsPlayerOnFoot = true;
	m_bIsPlayerAiming = false;
	m_bIsPlayerSkyDiving = false;

	CPed* pPlayerPed = CGameWorld::FindFollowPlayer();

	if (pPlayerPed)
	{
		if (pPlayerPed->GetPlayerInfo())
		{
			m_bIsPlayerAiming = (pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_IsAiming) && pPlayerPed->GetPlayerInfo()->IsHardAiming());
		}

		if (pPlayerPed->GetIsInVehicle())
		{
			m_bIsPlayerOnFoot = false;

			CVehicle* pVehicle = pPlayerPed->GetVehiclePedInside();
			if (pVehicle)
			{
				VehicleType vehType = pVehicle->GetVehicleType();
				if (vehType == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)
				{
					vehType = VEHICLE_TYPE_QUADBIKE;
				}

				switch (vehType)
				{
				case VEHICLE_TYPE_CAR:
				case VEHICLE_TYPE_QUADBIKE:
				case VEHICLE_TYPE_BIKE:
				case VEHICLE_TYPE_BICYCLE:
					m_bIsPlayerDrivingGroundVehicle = true;
					break;

				case VEHICLE_TYPE_TRAIN:
					m_bIsPlayerDrivingTrain = true;
					break;

				case VEHICLE_TYPE_HELI:
				case VEHICLE_TYPE_AUTOGYRO:
				case VEHICLE_TYPE_BLIMP:
				case VEHICLE_TYPE_PLANE:
					if (pVehicle->IsInAir())
					{
						m_bIsPlayerFlyingAircraft = true;
					}
					break;

				default:
					break;
				}
			}
		}
		else
		{
			if ( pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_IsParachuting) )
			{
				CQueriableInterface* pQueriableInterface = pPlayerPed->GetPedIntelligence()->GetQueriableInterface();
				if (pQueriableInterface->IsTaskCurrentlyRunning(CTaskTypes::TASK_PARACHUTE))
				{
					switch (pQueriableInterface->GetStateForTaskType(CTaskTypes::TASK_PARACHUTE))
					{
					case CTaskParachute::State_Skydiving:
						m_bIsPlayerSkyDiving = true;
						break;
					case CTaskParachute::State_Parachuting:
						{
							m_bIsPlayerSkyDiving = true;

							const CTask* pTask = pPlayerPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_PARACHUTE);
							if (pTask)
							{
								const CTaskParachute* pParachuteTask = static_cast<const CTaskParachute*>(pTask);
								if (pParachuteTask->GetParachute())
								{
									const CObject* pParachuteObj = pParachuteTask->GetParachute();
									m_fVelocity = fabsf( pParachuteObj->GetVelocity().Mag() );
								}
							}
						}
						
						break;
					default:
						break;
					}
				}
				
			}
		}
	}


	m_bAboveHeightMap = ( m_vPos.GetZf() >= CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(m_vPos.GetXf(), m_vPos.GetYf()) );

	m_bIsPlayerInCityArea = (pPlayerPed && s_approxCityBounds.IsValid() && s_approxCityBounds.ContainsPoint( pPlayerPed->GetTransform().GetPosition() ) );

	m_bLookingBackwards = ( gameplayDirector.IsLookingBehind() && camInterface::IsRenderingDirector(gameplayDirector) );

	m_bIsAnyFirstPersonCamera = isRenderingPovCamera || camInterface::IsRenderingFirstPersonShooterCamera();

	//////////////////////////////////////////////////////////////////////////
	// check for scene streaming performance
	m_fHighestRequiredSceneScore = CSceneStreamerMgr::GetCachedNeededScore();	

	if ( m_fHighestRequiredSceneScore >= (float)STREAMBUCKET_VISIBLE )
	{
		m_eStreamingPressure = SCENE_STREAMING_PRESSURE_VERY_HEAVY;
	}
	else if ( m_fHighestRequiredSceneScore >= (float)STREAMBUCKET_VISIBLE_FAR )
	{
		m_eStreamingPressure = SCENE_STREAMING_PRESSURE_HEAVY;
	}
	else if ( m_fHighestRequiredSceneScore >= (float)STREAMBUCKET_INVISIBLE_FAR )
	{
		m_eStreamingPressure = SCENE_STREAMING_PRESSURE_MODERATE;
	}
	else
	{
		m_eStreamingPressure = SCENE_STREAMING_PRESSURE_LIGHT;
	}
	m_bSceneStreamingUnderVeryHeavyPressure = ((m_fHighestRequiredSceneScore>=(float)STREAMBUCKET_VISIBLE) && (strStreamingEngine::GetInfo().GetNumberPriorityObjectsRequested()>0));
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DiffersSignificantly
// PURPOSE:		returns true of the new state is very different from exis ting state
//////////////////////////////////////////////////////////////////////////
bool CContinuityState::DiffersSignificantly(CContinuityState& newState)
{
	if ( m_bAimingFirstPerson != newState.m_bAimingFirstPerson )
		return true;

	if ( m_bInCarView != newState.m_bInCarView )
		return true;

	if ( m_bUsingNonGameCamera != newState.m_bUsingNonGameCamera )
		return true;

	if (m_bSwitching != newState.m_bSwitching)
		return true;

	if (m_bLookingBackwards != newState.m_bLookingBackwards)
		return true;

	if (m_bIsAnyFirstPersonCamera != newState.m_bIsAnyFirstPersonCamera)
		return true;

	// check for significant changes in lod multiplier. don't check for equality because there are time cycle fp precision issues
	if (fabsf(m_fLodScale - newState.m_fLodScale) >= 0.001f)
		return true;

	// TODO: insert more cases here based on position, FOV etc
	const ScalarV camMovementThresholdSqd(5.0f * 5.0f);
	const ScalarV distSqd = DistSquared(m_vPos, newState.m_vPos);
	if ( IsGreaterThanOrEqualAll(distSqd, camMovementThresholdSqd) )
	{
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Init
// PURPOSE:		reset state to defaults
//////////////////////////////////////////////////////////////////////////
void CContinuityMgr::Init()
{
	m_bMajorChangeOccurred = false;
	m_bLodScaleChangeOccurred = false;

	// extremely rough bounds for GTAV. sometimes useful
	s_approxCityBounds.Invalidate();
	s_approxCityBounds.Set(
		Vec3V(-2211.0f, -3730.0f, -100.0f),
		Vec3V(1900.0f, 365.0f, 400.0f)
	);

	m_currentState.Reset();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		refresh state, check for major changes
//////////////////////////////////////////////////////////////////////////
void CContinuityMgr::Update()
{
	CContinuityState newState;
	newState.Update();
	m_bMajorChangeOccurred = m_currentState.DiffersSignificantly(newState);
	m_bMajorChangeOccurred = m_bMajorChangeOccurred || ( fabsf(CRenderer::GetPrevCameraVelocity()) > LODTYPES_MAX_SPEED_TIMEFADE );
	m_bLodScaleChangeOccurred = m_bMajorChangeOccurred && (fabsf(m_currentState.m_fLodScale - newState.m_fLodScale) >= 0.001f);
	m_currentState = newState;
}

