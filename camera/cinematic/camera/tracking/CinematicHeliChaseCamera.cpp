//
// camera/cinematic/CinematicHeliChaseCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/camera/tracking/CinematicHeliChaseCamera.h"

#include "profile/group.h"
#include "profile/page.h"

#include "camera/CamInterface.h"
#include "camera/cinematic/camera/tracking/CinematicTrainTrackCamera.h"
#include "camera/cinematic/cinematicdirector.h"
#include "camera/helpers/BaseFrameShaker.h"
#include "camera/helpers/Collision.h"
#include "camera/helpers/InconsistentBehaviourZoomHelper.h"
#include "camera/helpers/LookAheadHelper.h"
#include "camera/helpers/LookAtDampingHelper.h"
#include "camera/system/CameraMetadata.h"
#include "math/angmath.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "vehicles/vehicle.h"
#include "vehicles/Automobile.h"
#include "scene/world/GameWorldHeightMap.h"
#include "task/Movement/TaskParachute.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camCinematicHeliChaseCamera,0x77DCF630)

PF_PAGE(camCinematicHeliChaseCameraPage, "Camera: Cinematic Heli Chase Camera");

PF_GROUP(camCinematicHeliChaseCameraMetrics);
PF_LINK(camCinematicHeliChaseCameraPage, camCinematicHeliChaseCameraMetrics);

PF_VALUE_FLOAT(fullAmp,			camCinematicHeliChaseCameraMetrics);
PF_VALUE_FLOAT(desiredAmp,		camCinematicHeliChaseCameraMetrics);
PF_VALUE_FLOAT(dampedAmp,		camCinematicHeliChaseCameraMetrics);
PF_VALUE_FLOAT(headingRatio,	camCinematicHeliChaseCameraMetrics);
PF_VALUE_FLOAT(headingAmp,		camCinematicHeliChaseCameraMetrics);
PF_VALUE_FLOAT(pitchAmp,		camCinematicHeliChaseCameraMetrics);
PF_VALUE_FLOAT(rollAmp,			camCinematicHeliChaseCameraMetrics);
PF_VALUE_FLOAT(fovAmp,			camCinematicHeliChaseCameraMetrics);
PF_VALUE_FLOAT(desiredTurbulenceAmp,	camCinematicHeliChaseCameraMetrics);
PF_VALUE_FLOAT(dampedTurbulenceAmp,		camCinematicHeliChaseCameraMetrics);

#if __BANK
float camCinematicHeliChaseCamera::ms_DebugUncertaintyShakeAmplitude	= 1.0f;
float camCinematicHeliChaseCamera::ms_DebugHeadingRatio					= 0.5f;
float camCinematicHeliChaseCamera::ms_DebugTurbulenceVehicleScale		= 1.0f;
float camCinematicHeliChaseCamera::ms_DebugTurbulenceDistanceScale		= 0.5f;
bool camCinematicHeliChaseCamera::ms_DebugSetUncertaintyShakeAmplitude	= false;
bool camCinematicHeliChaseCamera::ms_DebugSetTurbulenceShakeAmplitude	= false;
bool camCinematicHeliChaseCamera::ms_DebugSetShakeRatio					= false;
#endif // __BANK

camCinematicHeliChaseCamera::camCinematicHeliChaseCamera(const camBaseObjectMetadata& metadata)
: camBaseCamera(metadata)
, m_Metadata(static_cast<const camCinematicHeliChaseCameraMetadata&>(metadata))
, m_RouteStartPosition(VEC3_ZERO)
, m_RouteEndPosition(VEC3_ZERO)
, m_TargetLockPosition(VEC3_ZERO)
, m_LookAtPosition(g_InvalidPosition)
, m_StartGameTime(fwTimer::GetTimeInMilliseconds())
, m_PreviousTimeHadLosToTarget(0)
, m_TargetLockStartTime(0)
, m_ZoomDuration(0)
, m_CurrentZoomTime(0)
, m_Mode(0)
, m_StartDistanceBehindTarget(m_Metadata.m_StartDistanceBehindTarget)
, m_EndDistanceBeyondTarget(m_Metadata.m_EndDistanceBeyondTarget)
, m_HeightAboveTarget(m_Metadata.m_HeightAboveTarget)
, m_HeightAboveTargetForTrain(m_Metadata.m_HeightAboveTargetForTrain)
, m_HeightAboveTargetForHeli(m_Metadata.m_HeightAboveTargetForHeli)
, m_HeightAboveTargetForJetpack(m_Metadata.m_HeightAboveTargetForJetpack)
, m_HeightAboveTargetForBike(m_Metadata.m_HeightAboveTargetForBike)
, m_PathOffsetFromSideOfVehicle(m_Metadata.m_PathOffsetFromSideOfVehicle)
, m_ZoomOutStartFov(m_Metadata.m_MinZoomFov)
, m_PathHeading(0.0f)
, m_MaxFov(m_Metadata.m_MaxZoomFov)
, m_MinFov(m_Metadata.m_MinZoomFov)
, m_InitialHeading(0.0f)
, m_CurrentFov(0.0f)
, m_TargetFov(0.0f)
, m_TrackingAccuracy(1.0f)
, m_HeadingOnPreviousUpdate(0.0f)
, m_PitchOnPreviousUpdate(0.0f)
, m_RollOnPreviousUpdate(0.0f)
, m_FovOnPreviousUpdate(0.0f)
, m_InVehicleLookAtDamper(NULL)
, m_OnFootLookAtDamper(NULL)
, m_InVehicleLookAheadHelper(NULL)
, m_InconsistentBehaviourZoomHelper(NULL)
, m_HasAcquiredTarget(false)
, m_ShouldLockTarget(false)
, m_CanUseDampedLookAtHelpers(false)
, m_ShouldZoom(false)
, m_ShouldIgnoreLineOfAction(false)
, m_ShouldTrackParachuteInMP(false)
, m_CanUseLookAheadHelper(false)
, m_ReactedToInconsistentBehaviourLastUpdate(false)
, m_ZoomTriggeredFromInconsistency(false)
, m_HasLosToTarget(false)
{
	//Randomise some of the default settings.
	m_StartDistanceBehindTarget				*=  fwRandom::GetRandomNumberInRange(0.5f,1.0f);
	m_EndDistanceBeyondTarget				*=  fwRandom::GetRandomNumberInRange(0.1f,1.5f);
	m_HeightAboveTarget						*=  fwRandom::GetRandomNumberInRange(0.5f,1.0f);
	m_HeightAboveTargetForTrain				*=  fwRandom::GetRandomNumberInRange(0.5f,1.0f);
	m_HeightAboveTargetForBike				*=	fwRandom::GetRandomNumberInRange(0.75f,1.0f);
	m_HeightAboveTargetForHeli				*=  fwRandom::GetRandomNumberInRange(0.5f,1.0f);
	m_HeightAboveTargetForJetpack			*=	fwRandom::GetRandomNumberInRange(0.5f,1.0f);
	m_PathOffsetFromSideOfVehicle			*=	fwRandom::GetRandomNumberInRange(0.5f,1.0f);

	const camLookAtDampingHelperMetadata* pLookAtHelperMetaData = camFactory::FindObjectMetadata<camLookAtDampingHelperMetadata>(m_Metadata.m_InVehicleLookAtDampingRef.GetHash());

	if(pLookAtHelperMetaData)
	{
		m_InVehicleLookAtDamper = camFactory::CreateObject<camLookAtDampingHelper>(*pLookAtHelperMetaData);
		cameraAssertf(m_InVehicleLookAtDamper, "Heli Chase Camera (name: %s, hash: %u) cannot create a look at damping helper (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(pLookAtHelperMetaData->m_Name.GetCStr()), pLookAtHelperMetaData->m_Name.GetHash());
	}


	pLookAtHelperMetaData = camFactory::FindObjectMetadata<camLookAtDampingHelperMetadata>(m_Metadata.m_OnFootLookAtDampingRef.GetHash());

	if(pLookAtHelperMetaData)
	{
		m_OnFootLookAtDamper = camFactory::CreateObject<camLookAtDampingHelper>(*pLookAtHelperMetaData);
		cameraAssertf(m_OnFootLookAtDamper, "Heli Chase Camera (name: %s, hash: %u) cannot create a look at damping helper (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(pLookAtHelperMetaData->m_Name.GetCStr()), pLookAtHelperMetaData->m_Name.GetHash());
	}

	const camLookAheadHelperMetadata* pLookAheadHelperMetaData = camFactory::FindObjectMetadata<camLookAheadHelperMetadata>(m_Metadata.m_InVehicleLookAheadRef.GetHash());

	if(pLookAheadHelperMetaData)
	{
		m_InVehicleLookAheadHelper = camFactory::CreateObject<camLookAheadHelper>(*pLookAheadHelperMetaData);
		cameraAssertf(m_InVehicleLookAheadHelper, "Heli Chase Camera (name: %s, hash: %u) cannot create a look ahead helper (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(pLookAheadHelperMetaData->m_Name.GetCStr()), pLookAheadHelperMetaData->m_Name.GetHash());
	}

	const camInconsistentBehaviourZoomHelperMetadata* inconsistentBehaviourZoomHelper = camFactory::FindObjectMetadata<camInconsistentBehaviourZoomHelperMetadata>(m_Metadata.m_InconsistentBehaviourZoomHelperRef.GetHash());

	if(inconsistentBehaviourZoomHelper)
	{
		m_InconsistentBehaviourZoomHelper = camFactory::CreateObject<camInconsistentBehaviourZoomHelper>(*inconsistentBehaviourZoomHelper);
		cameraAssertf(m_InconsistentBehaviourZoomHelper, "Heli Chase Camera (name: %s, hash: %u) cannot create an inconsistent behaviour zoom helper (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(inconsistentBehaviourZoomHelper->m_Name.GetCStr()), inconsistentBehaviourZoomHelper->m_Name.GetHash());
	}
}

camCinematicHeliChaseCamera::~camCinematicHeliChaseCamera()
{
	if(m_InVehicleLookAtDamper)
	{
		delete m_InVehicleLookAtDamper; 
	}

	if(m_OnFootLookAtDamper)
	{
		delete m_OnFootLookAtDamper; 
	}

	if(m_InVehicleLookAheadHelper)
	{
		delete m_InVehicleLookAheadHelper; 
	}

	if (m_InconsistentBehaviourZoomHelper)
	{
		delete m_InconsistentBehaviourZoomHelper;
	}
}

void camCinematicHeliChaseCamera::SetTrackingAccuracyMode(u32 Mode)
{
	switch(Mode)
	{
	case HIGH_TRACKING_ACCURACY:
		{
			m_TrackingAccuracy = m_Metadata.m_HighAccuracyTrackingScalar; 
		}
		break;

	case MEDIUM_TRACKING_ACCURACY:
		{
			m_TrackingAccuracy = m_Metadata.m_MediumAccuracyTrackingScalar; 
		}
		break;

	case LOW_TRACKING_ACCURACY:
		{
			m_TrackingAccuracy = m_Metadata.m_LowAccuracyTrackingScalar; 
		}
		break;

	default:
		{
			RandomiseTrackingAccuracy();
		}
		break; 
	};

	cameraDebugf3("tracking accuracy %f", m_TrackingAccuracy); 
}

void camCinematicHeliChaseCamera::RandomiseTrackingAccuracy()
{
	u32 accuracyRandom = fwRandom::GetRandomNumberInRange(0, NUM_OF_ACCURACY_MODES);

	if(accuracyRandom == HIGH_TRACKING_ACCURACY)
	{
		m_TrackingAccuracy = m_Metadata.m_HighAccuracyTrackingScalar; 
	}

	if(accuracyRandom == MEDIUM_TRACKING_ACCURACY)
	{
		m_TrackingAccuracy = m_Metadata.m_MediumAccuracyTrackingScalar; 
	}

	if(accuracyRandom == LOW_TRACKING_ACCURACY)
	{
		m_TrackingAccuracy = m_Metadata.m_LowAccuracyTrackingScalar; 
	}

	cameraDebugf3("camCinematicHeliChaseCamera: tracking accuracy %f", m_TrackingAccuracy); 
}

bool camCinematicHeliChaseCamera::Update()
{
	if(!m_HasAcquiredTarget) //Ensure we acquired the target and planned a valid route.
	{
		ComputeValidRoute();
		
		if(m_Metadata.m_UseLowOrbitMode && GetNumUpdatesPerformed() == 0)
		{
			Vector2 vMin; 
			Vector2 vMax;

			vMin.x = Min(m_RouteStartPosition.x, m_RouteEndPosition.x); 
			vMin.y = Min(m_RouteStartPosition.y, m_RouteEndPosition.y);
			vMax.x = Max(m_RouteStartPosition.x, m_RouteEndPosition.x); 
			vMax.y = Max(m_RouteStartPosition.y, m_RouteEndPosition.y);

			vMin.x -= m_Metadata.m_HeightMapCheckBoundsExtension; 
			vMin.y -= m_Metadata.m_HeightMapCheckBoundsExtension; 
			vMax.y += m_Metadata.m_HeightMapCheckBoundsExtension;
			vMax.x += m_Metadata.m_HeightMapCheckBoundsExtension; 

			float MinHeigtAboveMap = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(vMin.x, vMin.y, vMax.x, vMax.y) + m_Metadata.m_MinHeightAboveHeightMap; 

			if(VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition()).z < MinHeigtAboveMap 
				|| m_RouteStartPosition.z < MinHeigtAboveMap 
				|| m_RouteEndPosition.z < MinHeigtAboveMap)
			{
				m_HasAcquiredTarget = false;
			}
		}


		if(!m_HasAcquiredTarget)
		{
			return false;
		}
	}

	if(m_LookAtTarget.Get() == NULL) //Safety check.
	{
		m_HasAcquiredTarget = false;
		return false;
	}

	if(NetworkInterface::IsGameInProgress() && m_LookAtTarget->GetIsTypePed() && static_cast<const CPed*>(m_LookAtTarget.Get())->IsNetworkClone())
	{
		if(NetworkInterface::IsEntityHiddenNetworkedPlayer(m_LookAtTarget))
		{
			m_HasAcquiredTarget = false;
			return false; //Spectating a player that is hidden under the map
		}
	}

	u32 gameTime			= fwTimer::GetTimeInMilliseconds();
	u32 gameTimeSinceStart	= gameTime - m_StartGameTime;
	float t					= (float)gameTimeSinceStart / (float)m_Metadata.m_MaxDuration;

	//Update camera position.
	Vector3 newPosition;
	
	//Allow the camera to track the vehicle, though only use the initial heading 
	if(m_Metadata.m_ShouldTrackVehicle ||  m_ShouldTrackParachuteInMP )
	{
		Matrix34 LookAtTargetMat = MAT34V_TO_MATRIX34(m_LookAtTarget->GetTransform().GetMatrix()); 
		
		if(GetNumUpdatesPerformed() == 0)
		{			
			m_InitialHeading = camFrame::ComputeHeadingFromMatrix(LookAtTargetMat);; 

			m_InitialHeading = fwAngle::LimitRadianAngle(m_InitialHeading); 
			
			m_InitialLookAtTargetPosition = LookAtTargetMat.d;
			
		}	

		Matrix34 NewMat(M34_IDENTITY); 

		camFrame::ComputeWorldMatrixFromHeadingPitchAndRoll(m_InitialHeading, 0.0f, 0.0f, NewMat); 
		
		Vector3 NewPos = Lerp(m_TrackingAccuracy, m_InitialLookAtTargetPosition, LookAtTargetMat.d); 
		
		NewMat.d = NewPos; 

		NewMat.Transform(m_LocalStart, m_RouteStartPosition); 
		NewMat.Transform(m_LocalEnd, m_RouteEndPosition); 
	}
	
	newPosition.Lerp(t, m_RouteStartPosition, m_RouteEndPosition);

	const CVehicle* targetVehicle	= m_LookAtTarget->GetIsTypeVehicle() ? static_cast<const CVehicle*>(m_LookAtTarget.Get()) : NULL;
	bool isTargetAnAircraft			= targetVehicle && targetVehicle->GetIsAircraft();
	bool IsTargetInTheAir			= targetVehicle && const_cast<CVehicle*>(targetVehicle)->IsInAir(); 

	//Prevent 180degree flip when going right over the car.
	if((isTargetAnAircraft && !IsTargetInTheAir) || !isTargetAnAircraft)
	{
		Vector3 targetPosition			= VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition());
		Vector3 deltaToVehicle			= targetPosition - newPosition;
		float horizDistanceToVehicle	= deltaToVehicle.XYMag();
		if((horizDistanceToVehicle > 0.0f) && (horizDistanceToVehicle < m_Metadata.m_MaxHorizDistanceToForceCameraOut))
		{
			deltaToVehicle	/= horizDistanceToVehicle;
			newPosition.x	= targetPosition.x - (deltaToVehicle.x * m_Metadata.m_MaxHorizDistanceToForceCameraOut);
			newPosition.y	= targetPosition.y - (deltaToVehicle.y * m_Metadata.m_MaxHorizDistanceToForceCameraOut);
		}
	}

	if((m_Metadata.m_ShouldTrackVehicle ||  m_ShouldTrackParachuteInMP) && m_NumUpdatesPerformed > 0)
	{
		if(HasCameraCollidedBetweenUpdates(m_Frame.GetPosition(), newPosition))
		{
			m_HasAcquiredTarget = false;
			return false; 
		}
	}

	if(m_NumUpdatesPerformed == 0 && !m_ShouldIgnoreLineOfAction)
	{	
		if(!camInterface::GetCinematicDirector().IsCameraPositionValidWithRespectToTheLine(m_LookAtTarget, newPosition))
		{
			cameraDebugf3("camCinematicHeliChaseCamera: failed due to line of the action"); 
			m_HasAcquiredTarget = false; 
			return m_HasAcquiredTarget;
		}
	}

	//Check the new camera position is valid.
	Vector3 targetPosition = VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition());
	if(!ComputeIsPositionValid(newPosition, targetPosition))
	{
		//Simply flag that we lost the target, so that this camera will report that it's not happy on the next update.
		//NOTE: We still allow this update to proceed as it is better to render an imperfect frame from this camera than have the discontinuity of no
		//cinematic camera frame.
		m_HasAcquiredTarget = false;
	}

	if(m_Metadata.m_ShouldTerminateForPitch)
	{
		if(camInterface::GetPitch() < m_Metadata.m_PitchLimits.x * DtoR || camInterface::GetPitch() > m_Metadata.m_PitchLimits.y * DtoR)
		{
			cameraDebugf3("camCinematicHeliChaseCamera: failed due Pitch"); 
			m_HasAcquiredTarget = false;
		}
	}

	m_Frame.SetPosition(newPosition);

	m_LookAtPosition = targetPosition;

	//push the look at position dependent on the direction of travel
	if (m_LookAtTarget->GetIsTypeVehicle() && m_CanUseLookAheadHelper && m_InVehicleLookAheadHelper)
	{
		Vector3 lookAheadPositionOffset = VEC3_ZERO;
		const CPhysical* lookAtTargetPhysical = static_cast<const CPhysical*>(m_LookAtTarget.Get());
		m_InVehicleLookAheadHelper->UpdateConsideringLookAtTarget(lookAtTargetPhysical, m_NumUpdatesPerformed == 0, lookAheadPositionOffset);
		m_LookAtPosition += lookAheadPositionOffset;
	}

	Vector3 front = m_LookAtPosition - newPosition;
	front.Normalize();
	m_Frame.SetWorldMatrixFromFront(front);

	UpdateZoom(newPosition, targetPosition);

	m_Frame.SetNearClip(m_Metadata.m_NearClip);
	
	if(m_CanUseDampedLookAtHelpers)
	{
		if(m_LookAtTarget->GetIsTypeVehicle())
		{
			if(m_InVehicleLookAtDamper)
			{
				Matrix34 WorldMat(m_Frame.GetWorldMatrix()); 

				m_InVehicleLookAtDamper->Update(WorldMat, m_Frame.GetFov(), m_NumUpdatesPerformed == 0); 

				m_Frame.SetWorldMatrix(WorldMat);
			}
		}
		else
		{
			if(m_OnFootLookAtDamper)
			{
				Matrix34 WorldMat(m_Frame.GetWorldMatrix()); 

				m_OnFootLookAtDamper->Update(WorldMat, m_Frame.GetFov(), m_NumUpdatesPerformed == 0); 

				m_Frame.SetWorldMatrix(WorldMat);
			}
		}
	}

	UpdateShake();

	return m_HasAcquiredTarget;
}
#if __BANK
void	camCinematicHeliChaseCamera::DebugRender()
{
	camBaseCamera::DebugRender(); 
	
	grcDebugDraw::Sphere(m_RouteStartPosition, 1.0f, Color_green);
	grcDebugDraw::Sphere(m_RouteEndPosition, 1.0f, Color_red);
	grcDebugDraw::Line(m_RouteStartPosition, m_RouteEndPosition, Color_green, Color_red);

	if(m_LookAtTarget)
	{
		Vector3 lookAtTargetPos = VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition()); 

		grcDebugDraw::Line(m_RouteStartPosition, lookAtTargetPos, Color_green, Color_blue, 1);
		grcDebugDraw::Line(m_RouteEndPosition, lookAtTargetPos, Color_red, Color_blue, 1);
	}
}
#endif
bool camCinematicHeliChaseCamera::IsClipping(const Vector3& position, const u32 flags)
{
	//Change the camera if it is clipping. 
	WorldProbe::CShapeTestSphereDesc sphereTest;
	sphereTest.SetIncludeFlags(flags); 
	sphereTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	sphereTest.SetContext(WorldProbe::LOS_Camera);
	sphereTest.SetSphere(position, m_Metadata.m_ClippingRadiusCheck * m_Metadata.m_ClippingRadiusScalar);
	sphereTest.SetTreatPolyhedralBoundsAsPrimitives(true);
	if(m_AttachParent)
	{
		sphereTest.SetExcludeEntity(m_AttachParent, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS); 
	}

	bool results = WorldProbe::GetShapeTestManager()->SubmitTest(sphereTest); 

	return results;
}

void camCinematicHeliChaseCamera::ComputeValidRoute()
{
	const CVehicle* targetVehicle	= m_LookAtTarget->GetIsTypeVehicle() ? static_cast<const CVehicle*>(m_LookAtTarget.Get()) : NULL;
	bool isTargetATrain				= targetVehicle && (targetVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN);
	bool isTargetJetpack			= targetVehicle && MI_JETPACK_THRUSTER.IsValid() && targetVehicle->GetModelIndex() == MI_JETPACK_THRUSTER;
	bool isTargetAnAircraft			= targetVehicle && targetVehicle->GetIsAircraft();
	bool isTargetABike				= targetVehicle && targetVehicle->InheritsFromBike(); 
	float SpeedDistanceScalar		= 1.0f; 
	
	Matrix34 PathMat = MAT34V_TO_MATRIX34(m_LookAtTarget->GetTransform().GetMatrix());
	float boundRadius = m_LookAtTarget->GetBoundRadius();  
	float boundsScaling = 1.0f; 

	//override the start and end distance using the jetpack specific metadata values
	if(isTargetJetpack)
	{
		m_StartDistanceBehindTarget  = m_Metadata.m_StartDistanceBehindTargetForJetpack;
		m_EndDistanceBeyondTarget    = m_Metadata.m_EndDistanceBeyondTargetForJetpack;
		m_StartDistanceBehindTarget	*=  fwRandom::GetRandomNumberInRange(0.5f,1.0f);
		m_EndDistanceBeyondTarget	*=  fwRandom::GetRandomNumberInRange(0.1f,1.5f);

		if(boundRadius > m_Metadata.m_StartDistanceBehindTargetForJetpack)
		{
			m_StartDistanceBehindTarget = m_Metadata.m_StartDistanceBehindTargetForJetpack; 
			m_EndDistanceBeyondTarget = m_Metadata.m_EndDistanceBeyondTargetForJetpack; 
			boundsScaling = (boundRadius / m_Metadata.m_StartDistanceBehindTargetForJetpack) * m_Metadata.m_BoundBoxScaleValue; 
		}
	}
	else
	{
		if(boundRadius > m_Metadata.m_StartDistanceBehindTarget)
		{
			m_StartDistanceBehindTarget = m_Metadata.m_StartDistanceBehindTarget; 
			m_EndDistanceBeyondTarget = m_Metadata.m_EndDistanceBeyondTarget; 
			boundsScaling = (boundRadius / m_Metadata.m_StartDistanceBehindTarget) * m_Metadata.m_BoundBoxScaleValue; 
		}
	}
	
	if(targetVehicle)
	{
		float fSpeed = DotProduct(targetVehicle->GetVelocity(), VEC3V_TO_VECTOR3(targetVehicle->GetTransform().GetB()));
		SpeedDistanceScalar	= RampValue (Abs(fSpeed), m_Metadata.m_MinVelocityDistanceScalar, m_Metadata.m_MaxVelocityDistanceScalar, 1.0f, m_Metadata.m_InitialPositionExtensionVelocityScalar);
	}

	if(NetworkInterface::IsGameInProgress())
	{
		const CObject* pParachuteObject = GetParachuteObject();
		if(pParachuteObject)
		{
			const Vector3 ParachuteVelocity = pParachuteObject->GetVelocity();

			const float currentAttachParentPitch = camFrame::ComputePitchFromMatrix(MAT34V_TO_MATRIX34(m_LookAtTarget->GetMatrix()));

			float fSpeed = DotProduct(ParachuteVelocity, VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetB()));
			SpeedDistanceScalar = RampValue (Abs(fSpeed), m_Metadata.m_MinVelocityDistanceScalar, m_Metadata.m_MaxVelocityDistanceScalar, 1.0f, m_Metadata.m_MaxDistanceFailureVelocityScalar);

			if(fSpeed > 30.0f && currentAttachParentPitch * RtoD < -40.0f)
			{
				m_ShouldTrackParachuteInMP = true; 
			}
		}
	}

	camCinematicDirector::eSideOfTheActionLine sideOfTheActionLine = camInterface::GetCinematicDirector().GetSideOfActionLine(m_LookAtTarget); 

	if(m_Metadata.m_ShouldApplyDesiredPathHeading)
	{
		if(sideOfTheActionLine == camCinematicDirector::ACTION_LINE_ANY)
		{
			Matrix34 LookAtTargetMat = MAT34V_TO_MATRIX34(m_LookAtTarget->GetTransform().GetMatrix()); 

			float vehicleHeading = camFrame::ComputeHeadingFromMatrix(LookAtTargetMat); 

			float RandomHeading = RandomiseInitialHeading(PI/2.0f ,0.0f,0.0f, 0.0f); 

			if((m_ShouldTrackParachuteInMP || isTargetAnAircraft) && RandomHeading > m_Metadata.m_InitialHeadingWithRespectToLookAtToExtendStartPoistion * DtoR)
			{
				m_StartDistanceBehindTarget = (isTargetJetpack ? m_Metadata.m_StartDistanceBehindTargetForJetpack : m_Metadata.m_StartDistanceBehindTarget) * SpeedDistanceScalar; 
			}

			//flip the sign randomly

			if(fwRandom::GetRandomNumberInRange(0, 10) > 5)
			{
				RandomHeading*=-1.0f; 
			}

			float DesiredHeading = fwAngle::LimitRadianAngle(vehicleHeading + RandomHeading);
			PathMat.MakeRotateZ(DesiredHeading); 

		}
		else
		{
			Matrix34 LookAtTargetMat = MAT34V_TO_MATRIX34(m_LookAtTarget->GetTransform().GetMatrix()); 

			Vector3	LookAtForward = LookAtTargetMat.b;

			//compute the vector from the camera pos to parent
			Vector3 vLastToLookAt = LookAtTargetMat.d - camInterface::GetCinematicDirector().GetLastValidCinematicCameraPosition(); 

			vLastToLookAt.NormalizeSafe(); 

			//compute the relative angle between this and vehicle front
			float heading = fabsf(LookAtForward.AngleZ(vLastToLookAt)); 

			//heading with respect target

			float MinHeadingForMinRange = m_Metadata.m_MinAngleFromMinRange * DtoR; 
			float MinHeadingForMaxRange = m_Metadata.m_MinAngleFromMaxRange * DtoR; 

			if(isTargetAnAircraft)
			{
				MinHeadingForMinRange*=2.0f; 
			}

			float RandomHeading = RandomiseInitialHeading(heading, m_Metadata.m_MinAngleBetweenShots* DtoR,MinHeadingForMinRange,  MinHeadingForMaxRange); 

			if((m_ShouldTrackParachuteInMP || isTargetAnAircraft) && RandomHeading > m_Metadata.m_InitialHeadingWithRespectToLookAtToExtendStartPoistion * DtoR)
			{
				m_StartDistanceBehindTarget = (isTargetJetpack ? m_Metadata.m_StartDistanceBehindTargetForJetpack : m_Metadata.m_StartDistanceBehindTarget) * SpeedDistanceScalar; 
			}

			//if the heading between the previous camera and the look at target is less than 25 degrees allow the line to be crossed
			if(m_ShouldTrackParachuteInMP &&  (Abs(heading * RtoD) < 25.0f))		
			{
				m_ShouldIgnoreLineOfAction = true; 
				bool leftSide = fwRandom::GetRandomTrueFalse(); 

				if(leftSide)
				{
					sideOfTheActionLine = camCinematicDirector::ACTION_LINE_LEFT; 
				}
				else
				{
					sideOfTheActionLine = camCinematicDirector::ACTION_LINE_RIGHT; 
				}
			}

			//local
			float vehicleHeading = camFrame::ComputeHeadingFromMatrix(LookAtTargetMat); 

			float DesiredHeading = 0.0f; 

			if(sideOfTheActionLine == camCinematicDirector::ACTION_LINE_LEFT)
			{
				DesiredHeading = fwAngle::LimitRadianAngle(vehicleHeading - RandomHeading);
			}
			else if(sideOfTheActionLine == camCinematicDirector::ACTION_LINE_RIGHT)
			{
				DesiredHeading = fwAngle::LimitRadianAngle(vehicleHeading + RandomHeading);
			} 
			PathMat.MakeRotateZ(DesiredHeading); 
		}
	}
	
	const Vector3 targetRight		= PathMat.a;
	const Vector3 targetForward		= PathMat.b;
	Vector3 targetPosition 	= VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition());

	//offset the target path by a factor 
	float heightDelta;
	if(isTargetATrain)
	{
		heightDelta = m_HeightAboveTargetForTrain;
	}
	else if(isTargetABike)
	{
		heightDelta = m_HeightAboveTargetForBike;
	}
	else if(isTargetJetpack)
	{
		heightDelta = m_HeightAboveTargetForJetpack;
	}
	else if(isTargetAnAircraft)
	{
		//TODO: Rename m_HeightAboveTargetForHeli, which is now a misnomer.
		heightDelta = m_HeightAboveTargetForHeli;
	}
	else
	{
		heightDelta = m_HeightAboveTarget;
		
		//if tracking the parachute make sure we are above it. 
		if(m_ShouldTrackParachuteInMP)
		{
			heightDelta*= 3.0f; 
		}
	}

	//for(u32 i=0; i<m_Metadata.m_MaxAttemptsToFindValidRoute; i++)
	{
		float Heading = camFrame::ComputeHeadingFromMatrix(MAT34V_TO_MATRIX34(m_LookAtTarget->GetTransform().GetMatrix()));
		Matrix34 VehicleNoRollOrPitchMat; 
		camFrame::ComputeWorldMatrixFromHeadingPitchAndRoll(Heading, 0.0f, 0.0f, VehicleNoRollOrPitchMat);
		VehicleNoRollOrPitchMat.d = VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition()); 

		m_RouteStartPosition	= targetPosition - (targetForward * m_StartDistanceBehindTarget * boundsScaling);
		m_RouteStartPosition.z	+=  heightDelta;
		
		//create a matrix without pitch and roll we want a flat path 
		
		Matrix34 RouteStartMat(VehicleNoRollOrPitchMat); 

		RouteStartMat.d = m_RouteStartPosition; 
		
		//the camera is facing the front of the vehicle so travel in the opposite direction to the vehicle
		float dotOfVehilceFrontToCameraFront= targetForward.Dot(RouteStartMat.b) ; 
		if(dotOfVehilceFrontToCameraFront< 0.0f)
		{
			RouteStartMat.RotateLocalZ(PI); 
		}
		
		if(Abs(dotOfVehilceFrontToCameraFront) > 0.9f)
		{
			float rotate = PI /4.0f; 
			
			if(fwRandom::GetRandomNumberInRange(0, 10) > 5)
			{
				rotate*=-1.0f; 
			}

			RouteStartMat.RotateLocalZ(rotate); 
		}
		

		Vector3 RouteEnd(0.0f, (m_EndDistanceBeyondTarget +  m_StartDistanceBehindTarget) * boundsScaling, 0.0f); 

		RouteStartMat.Transform(RouteEnd, m_RouteEndPosition);
		
		VehicleNoRollOrPitchMat.UnTransform(m_RouteStartPosition, m_LocalStart); //m_RouteStartPosition - VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition()); 
		VehicleNoRollOrPitchMat.UnTransform(m_RouteEndPosition, m_LocalEnd); 

		//Check the start of the path is clear.
		if(!camCollision::ComputeDoesSphereCollide(m_RouteStartPosition, m_Metadata.m_CollisionCheckRadius))
		{

			const CEntity* pExcludeEntity = nullptr; 

			if(m_LookAtTarget->GetIsTypePed())
			{
				const CPed* pPed = static_cast<const CPed*>(m_LookAtTarget.Get()); 
				pExcludeEntity = camCinematicCamManCamera::GetVehiclePedIsExitingEnteringOrIn(pPed); 
			}

			const CObject* pParachute = GetParachuteObject();
			if(pParachute)
			{
				pExcludeEntity = (const CEntity*)pParachute;
			}

			if(camCollision::ComputeIsLineOfSightClear(targetPosition, m_RouteStartPosition, m_LookAtTarget, camCollision::GetCollisionIncludeFlags(), pExcludeEntity))
			{
				m_MaxFov = ComputeFov(m_RouteStartPosition, m_Metadata.m_BoundsRadiusScaleForMaxFov);
				m_PreviousTimeHadLosToTarget = fwTimer::GetCamTimeInMilliseconds();
				m_HasAcquiredTarget = true;
				
				return;
			}
			else
			{
				cameraDebugf3("camCinematicHeliChaseCamera: failed due to line of sight"); 
			}
		}
		else
		{
			cameraDebugf3("camCinematicHeliChaseCamera: failed due to collision"); 
		}
		 
	}
}

float camCinematicHeliChaseCamera::RandomiseInitialHeading(float ForbiddenAngle, float MinAngleDelta, float MinHeadingFromMinRange, float MaxHeadingFromMaxRange)
{
	float ForbiddenMinAngle = ForbiddenAngle - MinAngleDelta; 
	float ForbiddenMaxAngle = ForbiddenAngle + MinAngleDelta; 

	ForbiddenMinAngle = Clamp(ForbiddenMinAngle, 0.0f, PI); 
	ForbiddenMaxAngle = Clamp(ForbiddenMaxAngle, 0.0f, PI); 

	float MaxRange =  PI - ForbiddenMaxAngle; 
	float MinRange = ForbiddenMinAngle; 

	bool IsMaxRangeValid = MaxRange > MaxHeadingFromMaxRange; 
	bool IsMinRangeValid = MinRange > MinHeadingFromMinRange; 

	//compute the total range
	float TotalRange; 

	if(IsMaxRangeValid && IsMinRangeValid)	
	{
		TotalRange = MaxRange + MinRange; 
	}
	else if(IsMaxRangeValid)
	{
		TotalRange = MaxRange; 
	}
	else
	{
		TotalRange = MinRange; 
	}

	//compute the angle step size

	float AngleStepSize = TotalRange / GetNumOfModes(); 

	float DesiredAngle = AngleStepSize * m_Mode; 

	if(IsMaxRangeValid && IsMinRangeValid)	
	{
		//if the desired is greater than the 
		if(DesiredAngle > MinRange)
		{
			DesiredAngle = (DesiredAngle - MinRange) + ForbiddenMaxAngle; 
		}
	}
	else if(IsMaxRangeValid)
	{
		DesiredAngle = ForbiddenMaxAngle + DesiredAngle; 
	}

	return  DesiredAngle; 
}


void camCinematicHeliChaseCamera::DampFov(float& Fov)
{	
	if(GetNumUpdatesPerformed() == 0)
	{
		m_FovSpring.Reset(Fov); 
	}
	else
	{
		m_FovSpring.Update(Fov, m_Metadata.m_FovSpringConstant, 1.0f, true); 

		Fov = m_FovSpring.GetResult(); 
	}
}

void camCinematicHeliChaseCamera::ComputeMaxFovBasedOnTargetVelocity(float &MaxFov)
{
	if(m_LookAtTarget && m_LookAtTarget->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(m_LookAtTarget.Get()); 

		if(pVehicle)
		{
			float fSpeed = pVehicle->GetVelocity().Mag();
			
			float TValue = RampValue (Abs(fSpeed), m_Metadata.m_MinSpeedForHighFovScalar, m_Metadata.m_MaxSpeedForHighFovScalar, 0.0f, 1.0f);

			MaxFov = Lerp(TValue, m_MaxFov, m_Metadata.m_HighSpeedMaxZoomFov); 

			DampFov(MaxFov); 
		}
	}
}

float camCinematicHeliChaseCamera::ComputeFov(const Vector3& position, const float boundRadiusScale) const
{
	float fov = m_Metadata.m_MaxZoomFov;

	if(m_LookAtTarget)
	{
		float boundRadius = m_LookAtTarget->GetBoundRadius() * boundRadiusScale;

		const CObject* pParachute = GetParachuteObject();
		if(pParachute)
		{
			boundRadius += pParachute->GetBoundRadius();
		}

		const Vector3 targetPosition = VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition());

		const float distanceToTarget = targetPosition.Dist(position);

		float desiredFov = rage::Atan2f(boundRadius, distanceToTarget) * 2.0f;
		desiredFov = desiredFov * RtoD;

		fov = Clamp(desiredFov, m_Metadata.m_MaxZoomFov, m_Metadata.m_MinZoomFov);
	}

	return fov;
}

bool camCinematicHeliChaseCamera::ComputeIsPositionValid(const Vector3& position, Vector3& targetPosition)
{
	const CVehicle* targetVehicle	= m_LookAtTarget->GetIsTypeVehicle() ? static_cast<const CVehicle*>(m_LookAtTarget.Get()) : NULL;
	bool isTargetAnAircraft			= targetVehicle && targetVehicle->GetIsAircraft();
	bool isTargetJetpack			= targetVehicle && MI_JETPACK_THRUSTER.IsValid() && targetVehicle->GetModelIndex() == MI_JETPACK_THRUSTER;
	bool IsTargetInTheAir			= targetVehicle && const_cast<CVehicle*>(targetVehicle)->IsInAir(); 
	bool IsTargetBikeABike			= targetVehicle && targetVehicle->InheritsFromBike(); 
	float SpeedDistanceScalar		= 1.0f; 

	Vector3 deltaToVehicle			= targetPosition - position;

	if(m_ShouldTrackParachuteInMP)
	{
		const CObject* pParachuteObject = GetParachuteObject();

		if(pParachuteObject)
		{
			Vector3 ParachuteVelocity = pParachuteObject->GetVelocity(); 

			float fSpeed = DotProduct(ParachuteVelocity, VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetB()));
			SpeedDistanceScalar = RampValue (Abs(fSpeed), m_Metadata.m_MinVelocityDistanceScalar, m_Metadata.m_MaxVelocityDistanceScalar, 1.0f, 1.5f);

			//push out the distance scalar based of the the tracking accuracy. This is because when the tracking is not so accurate the target will get further 
			//from the camera faster. 
			SpeedDistanceScalar += (0.8f - m_TrackingAccuracy); 
		}
	}

	float boundsScaling = 1.0f; 
	
	if(m_LookAtTarget)
	{
		if(m_LookAtTarget->GetBoundRadius() > (isTargetJetpack ? m_Metadata.m_StartDistanceBehindTargetForJetpack : m_Metadata.m_StartDistanceBehindTarget))
		{
			boundsScaling = m_Metadata.m_BoundBoxScaleValue; 
		}
	}
	
	float DistanceToVehicle; 

	if(m_Metadata.m_ShouldConsiderZDistanceToTarget)	
	{
		DistanceToVehicle = deltaToVehicle.Mag();
	}
	else
	{
		DistanceToVehicle = deltaToVehicle.XYMag();
	}
	
	if(targetVehicle)
	{
		float fSpeed = DotProduct(targetVehicle->GetVelocity(), VEC3V_TO_VECTOR3(targetVehicle->GetTransform().GetB()));

		if(IsTargetBikeABike)
		{
			SpeedDistanceScalar = RampValue (Abs(fSpeed), m_Metadata.m_MinVelocityDistanceScalar, m_Metadata.m_MaxVelocityDistanceScalar, 1.0f, m_Metadata.m_MaxDistanceFailureVelocityScalarForBike);
		}
		else
		{
			SpeedDistanceScalar = RampValue (Abs(fSpeed), m_Metadata.m_MinVelocityDistanceScalar, m_Metadata.m_MaxVelocityDistanceScalar, 1.0f, m_Metadata.m_MaxDistanceFailureVelocityScalar);
		}
	}

	//Check that the target is within our (horizontal) range.
	if(isTargetAnAircraft && IsTargetInTheAir)
	{
		//TODO: Rename MaxHorizDistanceToHeliTarget, which is now a misnomer.
		
		//scale the distance value based on the speed of the look at target
		if(DistanceToVehicle > m_Metadata.m_MaxHorizDistanceToHeliTarget * boundsScaling * SpeedDistanceScalar)
		{
			cameraDebugf3("camCinematicHeliChaseCamera: failed due horizontal distance to target which is a plane in the air %f", DistanceToVehicle); 
			return false;
		}
	}
	else if(DistanceToVehicle > m_Metadata.m_MaxHorizDistanceToTarget * boundsScaling * SpeedDistanceScalar)
	{
		cameraDebugf3("camCinematicHeliChaseCamera: failed due horizontal distance to target %f", DistanceToVehicle); 
		return false;	
	}


	if(IsClipping(position, (ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE ))
	{
		cameraDebugf3("camCinematicHeliChaseCamera: failed due clippping"); 
		return false; 
	}

	//if(camCollision::ComputeDoesSphereCollide(position, m_Metadata.m_CollisionCheckRadius))
	//{
	//	//We hit a building or something - stop moving:
	//	//TODO: Implement proper deceleration later.
	//	return false;			
	//}

	u32 time = fwTimer::GetCamTimeInMilliseconds();

	if(!m_ShouldLockTarget)
	{
		const CEntity* pSecondIgnoreEntity = NULL;
		if (targetVehicle)
		{
			if(camInterface::GetGameplayDirector().GetAttachedTrailer())
			{
				pSecondIgnoreEntity = camInterface::GetGameplayDirector().GetAttachedTrailer(); 
			}
		}

		const CObject* pParachuteObject = GetParachuteObject();
		if(pParachuteObject)
		{
			//it is fine to override this here...is not likeley we will have trailer vehicles WITH parachutes...
			pSecondIgnoreEntity = pParachuteObject;
		}

		m_HasLosToTarget = camCollision::ComputeIsLineOfSightClear(targetPosition, position, m_LookAtTarget,
												camCollision::GetCollisionIncludeFlags(), pSecondIgnoreEntity);

		if(m_HasLosToTarget)
		{
			m_PreviousTimeHadLosToTarget = time;
		}
		else
		{
			//zoom behaviour is handled by InconsistentBehaviourZoomHelper now 
			u32 timeOutOfLos = time - m_PreviousTimeHadLosToTarget;
			if(timeOutOfLos > m_Metadata.m_MaxTimeOutOfLosBeforeLock)
			{
				//Lock the target position - 'Damn, we lost him!'
				m_ShouldLockTarget		= true;
				m_TargetLockPosition	= targetPosition;
				m_TargetLockStartTime	= time;
			}
		}
	}
	else
	{
		//Stayed locked-on to the last known target position.
		u32 lockDuration = time - m_TargetLockStartTime;
		if(lockDuration > m_Metadata.m_MaxTargetLockDuration)
		{
			cameraDebugf3("camCinematicHeliChaseCamera: failed due LOS"); 
			return false;
		}

		targetPosition = m_TargetLockPosition;
	}

	return true;
}

void camCinematicHeliChaseCamera::TriggerZoom(float InitialFov, float TargetFov, u32 ZoomDuration)
{
	m_CurrentFov = InitialFov; 
	m_TargetFov = TargetFov; 
	m_ZoomDuration = ZoomDuration; 
	m_CurrentZoomTime = 0; 
	m_ShouldZoom = true; 
}

void camCinematicHeliChaseCamera::UpdateZoom(const Vector3& position, const Vector3& UNUSED_PARAM(targetPosition))
{
	if(m_NumUpdatesPerformed == 0)
	{
		m_MinFov = ComputeFov(position, m_Metadata.m_BoundsRadiusScaleForMinFov);
		m_MaxFov = ComputeFov(position, m_Metadata.m_BoundsRadiusScaleForMaxFov);
	}
	
	float DesiredFov = m_Frame.GetFov(); 
	
	if(m_NumUpdatesPerformed == 0)
	{
		bool startZoomedIn = fwRandom::GetRandomTrueFalse(); 
		bool shouldZoomInAtStart = fwRandom::GetRandomTrueFalse();
		
		if(startZoomedIn)
		{
			DesiredFov = m_MaxFov; 
		}
		else
		{
			DesiredFov = m_MinFov;

			if(shouldZoomInAtStart)
			{
				u32 ZoomDuration = (u32)fwRandom::GetRandomNumberInRange((s32)m_Metadata.m_MinInitialZoomDuration, (s32)m_Metadata.m_MaxInitialZoomDuration);
				TriggerZoom(m_MinFov, m_MaxFov, ZoomDuration); 
			}
		}
	}

	UpdateZoomForInconsistencies(position);

	if(m_ShouldZoom)
	{
		float zoomT = (float)(m_CurrentZoomTime) / (float)m_ZoomDuration;
		zoomT = Clamp(zoomT, 0.0f, 1.0f);
		const float zoomTimeToConsider = m_ZoomTriggeredFromInconsistency ? SlowOut(SlowOut(zoomT)) : SlowInOut(zoomT);
		DesiredFov = Lerp(zoomTimeToConsider, m_CurrentFov, m_TargetFov);

		m_CurrentZoomTime += fwTimer::GetCamTimeStepInMilliseconds(); 

		if(m_CurrentZoomTime > m_ZoomDuration)
		{
			m_ShouldZoom = false; 
			m_CurrentZoomTime = 0; 
		}
	}

	//zooming from inconsistency ignores these restrictions
	if (!m_ZoomTriggeredFromInconsistency)
	{
		float MaxFov = m_MaxFov; 
		float MinFov = m_MinFov;

		ComputeMaxFovBasedOnTargetVelocity(MaxFov); 

		if(m_MinFov < MaxFov)
		{
			MinFov = MaxFov; 
		}

		DesiredFov = Clamp(DesiredFov, MaxFov , MinFov); 
	}

	m_Frame.SetFov(DesiredFov); 
}

void camCinematicHeliChaseCamera::UpdateZoomForInconsistencies(const Vector3& position)
{
	if (!m_InconsistentBehaviourZoomHelper)
	{
		return;
	}

	const bool shouldReactToInconsistentBehaviour = m_InconsistentBehaviourZoomHelper->UpdateShouldReactToInconsistentBehaviour(m_LookAtTarget, m_Frame, m_HasLosToTarget);

	if (shouldReactToInconsistentBehaviour && !m_ReactedToInconsistentBehaviourLastUpdate)
	{
		const float initialFov			= m_Frame.GetFov();

		const float boundRadiusScale	= m_InconsistentBehaviourZoomHelper->GetBoundsRadiusScale();
		const float maxInconsistentFov	= m_InconsistentBehaviourZoomHelper->GetMaxFov();

		float fovToUse					= ComputeFov(position, boundRadiusScale);
		fovToUse						= Clamp(fovToUse, m_Metadata.m_MaxZoomFov, maxInconsistentFov);

		const float minFovDifference	= m_InconsistentBehaviourZoomHelper->GetMinFovDifference();
		if (initialFov < (fovToUse - minFovDifference))
		{
			const float targetFov		= fovToUse;
			const u32 zoomDuration		= m_InconsistentBehaviourZoomHelper->GetZoomDuration();
			TriggerZoom(initialFov, targetFov, zoomDuration);
			m_ZoomTriggeredFromInconsistency = true;
		}
		else
		{
			//disable any currently running zoom
			m_ShouldZoom = false; 
			m_CurrentZoomTime = 0; 
		}
	}

	m_ReactedToInconsistentBehaviourLastUpdate = shouldReactToInconsistentBehaviour;
}

//There is duplicate UpdateShake and UpdateCameraOperatorShakeAmplitudes functions in CinematicCamManCamera.
//If you change anything here, change it there too. This ideally needs to be refactored into a helper when there's time.
void camCinematicHeliChaseCamera::UpdateShake()
{
	const camCinematicCameraOperatorShakeUncertaintySettings& uncertaintySettings = m_Metadata.m_CameraOperatorShakeSettings.m_UncertaintySettings;
	const camCinematicCameraOperatorShakeTurbulenceSettings& turbulenceSettings = m_Metadata.m_CameraOperatorShakeSettings.m_TurbulenceSettings;

	if (!uncertaintySettings.m_XShakeRef && !uncertaintySettings.m_ZShakeRef && !turbulenceSettings.m_ShakeRef)
	{
		return;
	}

	float xUncertaintyAmplitude;
	float zUncertaintyAmplitude;
	float turbulenceAmplitude;
	UpdateCameraOperatorShakeAmplitudes(xUncertaintyAmplitude, zUncertaintyAmplitude, turbulenceAmplitude);
	
	if (uncertaintySettings.m_XShakeRef)
	{
		if (camBaseFrameShaker* shake = FindFrameShaker(uncertaintySettings.m_XShakeRef))
		{
			shake->SetAmplitude(xUncertaintyAmplitude);
		}
		else
		{
			Shake(uncertaintySettings.m_XShakeRef, xUncertaintyAmplitude);
		}
	}

	if (uncertaintySettings.m_ZShakeRef)
	{
		if (camBaseFrameShaker* shake = FindFrameShaker(uncertaintySettings.m_ZShakeRef))
		{
			shake->SetAmplitude(zUncertaintyAmplitude);
		}
		else
		{
			Shake(uncertaintySettings.m_ZShakeRef, zUncertaintyAmplitude);
		}
	}

	if (turbulenceSettings.m_ShakeRef)
	{
		if (camBaseFrameShaker* shake = FindFrameShaker(turbulenceSettings.m_ShakeRef))
		{
			shake->SetAmplitude(turbulenceAmplitude);
		}
		else
		{
			Shake(turbulenceSettings.m_ShakeRef, turbulenceAmplitude);
		}
	}
}

void camCinematicHeliChaseCamera::UpdateCameraOperatorShakeAmplitudes(float& xUncertaintyAmplitude, float& zUncertaintyAmplitude, float& turbulenceAmplitude)
{
	xUncertaintyAmplitude = 0.0f;
	zUncertaintyAmplitude = 0.0f;
	turbulenceAmplitude = 0.0f;

	float heading;
	float pitch;
	float roll;
	camFrame::ComputeHeadingPitchAndRollFromMatrix(m_Frame.GetWorldMatrix(), heading, pitch, roll);
	float fov = m_Frame.GetFov();
	if (m_NumUpdatesPerformed == 0)
	{
		m_HeadingOnPreviousUpdate	= heading;
		m_PitchOnPreviousUpdate		= pitch;
		m_RollOnPreviousUpdate		= roll;
		m_FovOnPreviousUpdate		= fov;
	}

	//Uncertainty shake
	const camCinematicCameraOperatorShakeUncertaintySettings& uncertaintySettings = m_Metadata.m_CameraOperatorShakeSettings.m_UncertaintySettings;

	const float invTimeStep			= fwTimer::GetInvTimeStep();

	const float headingSpeed		= fwAngle::LimitRadianAngle(heading - m_HeadingOnPreviousUpdate) * invTimeStep;
	const float headingAmplitude	= RampValueSafe(Abs(headingSpeed), uncertaintySettings.m_AbsHeadingSpeedRange.x, uncertaintySettings.m_AbsHeadingSpeedRange.y, 0.0f, 1.0f);
	PF_SET(headingAmp, headingAmplitude);

	const float pitchSpeed			= fwAngle::LimitRadianAngle(pitch - m_PitchOnPreviousUpdate) * invTimeStep;
	const float pitchAmplitude		= RampValueSafe(Abs(pitchSpeed), uncertaintySettings.m_AbsPitchSpeedRange.x, uncertaintySettings.m_AbsPitchSpeedRange.y, 0.0f, 1.0f);
	PF_SET(pitchAmp, pitchAmplitude);

	const float rollSpeed			= fwAngle::LimitRadianAngle(roll - m_RollOnPreviousUpdate) * invTimeStep;
	const float rollAmplitude		= RampValueSafe(Abs(rollSpeed), uncertaintySettings.m_AbsRollSpeedRange.x, uncertaintySettings.m_AbsRollSpeedRange.y, 0.0f, 1.0f);
	PF_SET(rollAmp, rollAmplitude);

	const float invCamTimeStep		= fwTimer::GetCamInvTimeStep();
	const float fovSpeed			= (fov - m_FovOnPreviousUpdate) * invCamTimeStep;
	const float fovAmplitude		= RampValueSafe(Abs(fovSpeed), uncertaintySettings.m_AbsFovSpeedRange.x, uncertaintySettings.m_AbsFovSpeedRange.y, 0.0f, 1.0f);
	PF_SET(fovAmp, fovAmplitude);

	m_HeadingOnPreviousUpdate		= heading;
	m_PitchOnPreviousUpdate			= pitch;
	m_RollOnPreviousUpdate			= roll;
	m_FovOnPreviousUpdate			= fov;

	//add together to get overall amplitude
	float desiredUncertaintyAmplitude = Clamp(headingAmplitude + pitchAmplitude + rollAmplitude + fovAmplitude, 0.0f, 1.0f);
	desiredUncertaintyAmplitude		*= uncertaintySettings.m_ScalingFactor;

#if __BANK
	if (ms_DebugSetUncertaintyShakeAmplitude)
	{
		desiredUncertaintyAmplitude	= ms_DebugUncertaintyShakeAmplitude * uncertaintySettings.m_ScalingFactor;
	}
#endif // __BANK

	const bool shouldCutSpring					= (m_NumUpdatesPerformed == 0)
													|| m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition)
													|| m_Frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);
	const float uncertaintyAmplitudeLastUpdate	= m_CameraOperatorUncertaintySpring.GetResult();
	const float uncertaintySpringConstant		= shouldCutSpring ? 0.0f :
													(desiredUncertaintyAmplitude > uncertaintyAmplitudeLastUpdate) ?
													uncertaintySettings.m_IncreasingSpringConstant : uncertaintySettings.m_DecreasingSpringConstant;
	const float uncertaintyAmplitude			= m_CameraOperatorUncertaintySpring.Update(desiredUncertaintyAmplitude, uncertaintySpringConstant, uncertaintySettings.m_SpringDampingRatio, true);
	PF_SET(desiredAmp, desiredUncertaintyAmplitude);
	PF_SET(dampedAmp, uncertaintyAmplitude);
	PF_SET(fullAmp, 1.0f);

	//get ratio between heading and pitch
	const float totalSpeed						= Abs(headingSpeed) + Abs(pitchSpeed);

	const float headingRatioLastUpdate			= m_CameraOperatorHeadingRatioSpring.GetResult();
	float desiredHeadingRatio					= (m_NumUpdatesPerformed == 0) ? 0.5f :
													(totalSpeed > uncertaintySettings.m_HeadingAndPitchThreshold) ? (Abs(headingSpeed) / totalSpeed) : headingRatioLastUpdate;
	desiredHeadingRatio							= Clamp(desiredHeadingRatio, 0.0f, 1.0f);

	const float pitchRatioLastUpdate			= m_CameraOperatorPitchRatioSpring.GetResult();
	float desiredPitchRatio						= (m_NumUpdatesPerformed == 0) ? 0.5f :
													(totalSpeed > uncertaintySettings.m_HeadingAndPitchThreshold) ? (Abs(pitchSpeed) / totalSpeed) : pitchRatioLastUpdate;
	desiredPitchRatio							= Clamp(desiredPitchRatio, 0.0f, 1.0f);

	const float ratioSpringConstant				= shouldCutSpring ? 0.0f : uncertaintySettings.m_RatioSpringConstant;
	
	const float headingRatio					= m_CameraOperatorHeadingRatioSpring.Update(desiredHeadingRatio, ratioSpringConstant, uncertaintySettings.m_RatioSpringDampingRatio, true);
	const float pitchRatio						= m_CameraOperatorPitchRatioSpring.Update(desiredPitchRatio, ratioSpringConstant, uncertaintySettings.m_RatioSpringDampingRatio, true);
	PF_SET(headingRatio, headingRatio);

	//apply ratio to each axis's amplitude
	xUncertaintyAmplitude			= uncertaintyAmplitude * pitchRatio;
	zUncertaintyAmplitude			= uncertaintyAmplitude * headingRatio;

#if __BANK
	if (ms_DebugSetShakeRatio)
	{
		xUncertaintyAmplitude		= uncertaintyAmplitude * (1.0f - ms_DebugHeadingRatio);
		zUncertaintyAmplitude		= uncertaintyAmplitude * ms_DebugHeadingRatio;
	}
#endif // __BANK

	//Turbulence shake
	const camCinematicCameraOperatorShakeTurbulenceSettings& turbulenceSettings = m_Metadata.m_CameraOperatorShakeSettings.m_TurbulenceSettings;

	const Vector3 cameraPosition	= m_Frame.GetPosition();
	const Vector3 lookAtPosition	= VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition());
	const float distanceToTarget	= cameraPosition.Dist(lookAtPosition);
	float distanceScale				= RampValueSafe(distanceToTarget, turbulenceSettings.m_DistanceRange.x, turbulenceSettings.m_DistanceRange.y, 1.0f, 0.0f);
	distanceScale					= SlowOut(distanceScale);

	const CVehicle* targetVehicle	= m_LookAtTarget->GetIsTypeVehicle() ? static_cast<const CVehicle*>(m_LookAtTarget.Get()) : NULL;
	const Vector3 vehicleVelocity	= targetVehicle ? targetVehicle->GetVelocity() : VEC3_ZERO;
	const float vehicleSpeed		= vehicleVelocity.Mag();
	const float speedScale			= RampValueSafe(vehicleSpeed, turbulenceSettings.m_AbsVehicleSpeedRange.x, turbulenceSettings.m_AbsVehicleSpeedRange.y, 0.0f, 1.0f);

	float desiredTurbulenceAmplitude = distanceScale * speedScale;

#if __BANK
	if (ms_DebugSetTurbulenceShakeAmplitude)
	{
		desiredTurbulenceAmplitude	= ms_DebugTurbulenceVehicleScale * ms_DebugTurbulenceDistanceScale;
	}
#endif // __BANK

	const float turbulenceAmplitudeLastUpdate	= m_CameraOperatorTurbulenceSpring.GetResult();
	const float turbulenceSpringConstant		= shouldCutSpring ? 0.0f :
													(desiredTurbulenceAmplitude > turbulenceAmplitudeLastUpdate) ?
													turbulenceSettings.m_IncreasingSpringConstant : turbulenceSettings.m_DecreasingSpringConstant;
	turbulenceAmplitude							= m_CameraOperatorTurbulenceSpring.Update(desiredTurbulenceAmplitude, turbulenceSpringConstant, turbulenceSettings.m_SpringDampingRatio, true);
	PF_SET(desiredTurbulenceAmp, desiredTurbulenceAmplitude);
	PF_SET(dampedTurbulenceAmp, turbulenceAmplitude);
}

bool camCinematicHeliChaseCamera::IsValid()
{
	return m_HasAcquiredTarget;
}

bool camCinematicHeliChaseCamera::HasCameraCollidedBetweenUpdates(const Vector3& PreviousPosition, const Vector3& currentPosition)
{
	WorldProbe::CShapeTestProbeDesc lineTest;
	lineTest.SetIsDirected(false);
	lineTest.SetIncludeFlags(camCollision::GetCollisionIncludeFlags());
	lineTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	lineTest.SetContext(WorldProbe::LOS_Camera);
	lineTest.SetOptions(WorldProbe::LOS_IGNORE_NO_CAM_COLLISION);

	lineTest.SetStartAndEnd(PreviousPosition, currentPosition);

	bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(lineTest);

	return hasHit; 
}

const CObject* camCinematicHeliChaseCamera::GetParachuteObject() const
{
	if(!m_LookAtTarget)
	{
		return nullptr;
	}

	const CVehicle* targetVehicle	= m_LookAtTarget->GetIsTypeVehicle() ? static_cast<const CVehicle*>(m_LookAtTarget.Get()) : NULL;
	const CPed*	targetPed			= m_LookAtTarget->GetIsTypePed() ? static_cast<const CPed*>(m_LookAtTarget.Get()) : NULL;

	const CObject* pParachuteObject = nullptr;

	if (targetPed && targetPed->GetPedIntelligence())
	{
		const CTask* pTask = targetPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_PARACHUTE); 
		if(pTask)
		{
			const CTaskParachute* pParachuteTask = static_cast<const CTaskParachute*>(pTask); 
			pParachuteObject = pParachuteTask->GetParachute();
		}
	}

	if(targetVehicle && targetVehicle->InheritsFromAutomobile())
	{
		const CAutomobile* pAutomobile = static_cast<const CAutomobile*>(targetVehicle);
		pParachuteObject = pAutomobile->GetParachuteObject();
	}

	return pParachuteObject;
}

#if __BANK
void camCinematicHeliChaseCamera::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Cinematic Heli Chase camera");
	{
		bank.AddToggle("Force uncertainty shake amplitude", &ms_DebugSetUncertaintyShakeAmplitude);
		bank.AddSlider("Uncertainty shake amplitude", &ms_DebugUncertaintyShakeAmplitude, 0.0f, 1.0f, 0.01f);
		bank.AddToggle("Force shake ratio", &ms_DebugSetShakeRatio);
		bank.AddSlider("Shake ratio", &ms_DebugHeadingRatio, 0.0f, 1.0f, 0.01f);
		bank.AddToggle("Force turbulence shake amplitude", &ms_DebugSetTurbulenceShakeAmplitude);
		bank.AddSlider("Turbulence shake vehicle scale", &ms_DebugTurbulenceVehicleScale, 0.0f, 1.0f, 0.01f);
		bank.AddSlider("Turbulence distance scale", &ms_DebugTurbulenceDistanceScale, 0.0f, 1.0f, 0.01f);
	}
	bank.PopGroup(); //Cinematic Heli Chase camera
}
#endif // __BANK
