//
// camera/cinematic/CinematicMountedPartCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/camera/mounted/CinematicMountedPartCamera.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/follow/FollowVehicleCamera.h"
#include "camera/helpers/BaseFrameShaker.h"
#include "camera/helpers/Collision.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/helpers/Envelope.h"
#include "camera/helpers/SpringMount.h"
#include "camera/system/CameraFactory.h"
#include "system/control.h"

#include "frontend/MobilePhone.h"
#include "peds/ped.h"
#include "vehicles/vehicle.h"

#include "fwmaths/random.h"
//#include "camera/cinematic/CinematicDirector.h"
#include "vehicleAi/task/TaskVehicleAnimation.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camCinematicMountedPartCamera,0x4F1DAEB)


camCinematicMountedPartCamera::camCinematicMountedPartCamera(const camBaseObjectMetadata& metadata)
: camBaseCamera(metadata)
, m_Metadata(static_cast<const camCinematicMountedPartCameraMetadata&>(metadata))
, m_SpringMount(NULL)
, m_canUpdate(true)
{
	const camSpringMountMetadata* springMountMetadata = camFactory::FindObjectMetadata<camSpringMountMetadata>(m_Metadata.m_SpringMountRef.GetHash());
	if(springMountMetadata)
	{
		m_SpringMount = camFactory::CreateObject<camSpringMount>(*springMountMetadata);
		cameraAssertf(m_SpringMount, "A cinematic mounted camera (name: %s, hash: %u) failed to create a spring mount (name: %s, hash: %u)",
			GetName(), GetNameHash(), SAFE_CSTRING(springMountMetadata->m_Name.GetCStr()), springMountMetadata->m_Name.GetHash());
	}
}

camCinematicMountedPartCamera::~camCinematicMountedPartCamera()
{
	if(m_SpringMount)
	{
		delete m_SpringMount;
	}
}

bool camCinematicMountedPartCamera::IsValid()
{
	return m_canUpdate; 
}

bool camCinematicMountedPartCamera::IsCameraInsideVehicle() const
{
	return m_Metadata.m_IsBehindVehicleGlass;
}

bool camCinematicMountedPartCamera::IsCameraForcingMotionBlur() const
{
	return m_Metadata.m_IsForcingMotionBlur;
}

bool camCinematicMountedPartCamera::ShouldDisplayReticule() const
{
	//NOTE: The reticule is explicitly disabled when the mobile phone camera is rendering.
	const bool isRenderingMobilePhoneCamera	= CPhoneMgr::CamGetState();
	const bool shouldDisplayReticule		= !isRenderingMobilePhoneCamera && m_Metadata.m_ShouldDisplayReticule;

	return shouldDisplayReticule;
}

bool camCinematicMountedPartCamera::ShouldMakeFollowPedHeadInvisible() const
{
	return m_Metadata.m_ShouldMakeFollowPedHeadInvisible;
}

bool camCinematicMountedPartCamera::IsValidForReversing() const
{
	return m_Metadata.m_RelativeLookAtPosition.y < 0.0f; 
}

bool camCinematicMountedPartCamera::Update()
{

	if(!m_AttachParent.Get() || !m_AttachParent->GetIsPhysical() || !m_AttachParent->GetIsTypeVehicle() || m_canUpdate == false) //Safety check.
	{
		m_canUpdate = false;
		return m_canUpdate;
	}

	bool bFailCamera = false;
	const CVehicle& attachVehicle = static_cast<const CVehicle&>(*m_AttachParent.Get());
	const bool bDrivable =	 attachVehicle.GetStatus() != STATUS_WRECKED &&
							!attachVehicle.m_nVehicleFlags.bIsDrowning && 
							 attachVehicle.m_nVehicleFlags.bIsThisADriveableCar;

	if (!bDrivable)
	{
		bFailCamera = true;
	}

	eHierarchyId eAttachPart = ComputeVehicleHieracahyId(m_Metadata.m_AttachPart); 
	
	if(!attachVehicle.InheritsFromBike())
	{
		if(ShouldTerminateForSwingingDoors(const_cast<CVehicle*>(&attachVehicle), eAttachPart))
		{
			m_canUpdate = false;
			return m_canUpdate; 
		}
	}
	
	const CPhysical& attachParentPhysical = static_cast<const CPhysical&>(*m_AttachParent.Get());

	Matrix34 vehicleMatrix;
	Matrix34 attachMatrix;
	Vector3  offsetFromVehicleToPart;
	ComputeAttachMatrix(vehicleMatrix, attachMatrix, offsetFromVehicleToPart);

	// If look at is looking forward and vehicle is reversing or
	// look at is behind and vehicle is moving forward, then fail camera.
	if(GetNumUpdatesPerformed() == 0 && (attachVehicle.GetVehicleType() == VEHICLE_TYPE_CAR || (!attachVehicle.GetIsInWater() && (attachVehicle.GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || attachVehicle.GetVehicleType() == VEHICLE_TYPE_SUBMARINECAR))))
	{
		bool ShouldTerminateDueToFacingTheWrongDirection = false; 
		float fSpeed = attachVehicle.GetVelocity().Dot(vehicleMatrix.b);
		
		CControl* control = camInterface::GetGameplayDirector().GetActiveControl();
		if(control)
		{
			if(fSpeed < 0.0f && !IsValidForReversing() && control->GetVehicleBrake().IsDown() )
			{
				ShouldTerminateDueToFacingTheWrongDirection = true;
			}

			//traveling forwards but the current camera is looking backwards
			if(fSpeed > 0.0f && IsValidForReversing() && control->GetVehicleAccelerate().IsDown())
			{
				ShouldTerminateDueToFacingTheWrongDirection = true; 
			}
		}

		if ( ShouldTerminateDueToFacingTheWrongDirection )
		{
			bFailCamera = true;
		}
	}

	// Transform to world space, using vehicle orientation instead of part orientation,
	// then offset to make relative to part position.
	Vector3 cameraPosition;

	vehicleMatrix.Transform(m_Metadata.m_RelativeAttachOffset, cameraPosition);

	if (!m_canUpdate)
	{
		return m_canUpdate;
	}
	cameraPosition += offsetFromVehicleToPart;

	const bool isAttachedToFormulaCar = attachVehicle.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_FORMULA_VEHICLE); 
	const bool useWorldUpForOrientation = attachVehicle.InheritsFromBike() || isAttachedToFormulaCar;

	Vector3 lookAtPosition;
	UpdateOrientation(attachParentPhysical, vehicleMatrix, attachMatrix, cameraPosition, lookAtPosition, offsetFromVehicleToPart, useWorldUpForOrientation);

	if(m_Metadata.m_ShouldTestForClipping && IsClipping(cameraPosition, lookAtPosition, (ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE ))
	{
		bFailCamera = true;
	}

	m_Frame.SetPosition(cameraPosition);

	UpdateFov();

	m_Frame.SetNearClip(m_Metadata.m_BaseNearClip);

	if(m_NumUpdatesPerformed == 0)
	{
		const Vector3 vZero(Vector3::ZeroType);
		Vector3 vDirection = m_Metadata.m_RelativeLookAtPosition - m_Metadata.m_RelativeAttachOffset;
		if (vDirection.Mag2() < SMALL_FLOAT)
		{
			m_canUpdate = false;
			return false; 
		}

		
	}

	UpdateHighSpeedShake(attachVehicle);

	const camGameplayDirector& gameplayDirector	= camInterface::GetGameplayDirector();
	const camBaseCamera* renderedGameplayCamera	= gameplayDirector.GetRenderedCamera();

	if ( m_Metadata.m_ShouldCopyVehicleCameraMotionBlur &&
		 renderedGameplayCamera &&
		 renderedGameplayCamera->GetIsClassId(camFollowVehicleCamera::GetStaticClassId()) )
	{
		// Inherit motion blur settings from follow vehicle camera.
		m_Frame.SetMotionBlurStrength( renderedGameplayCamera->GetFrame().GetMotionBlurStrength() );
	}

	if (bFailCamera)
	{
		// One more update, then fail camera.
		m_canUpdate = false;
	}
	return m_canUpdate;
}

eHierarchyId camCinematicMountedPartCamera::ComputeVehicleHieracahyId(eCinematicVehicleAttach CinematicAttachPart)
{
	eHierarchyId eAttachPart = VEH_INVALID_ID;
	switch (CinematicAttachPart)
	{
	case CVA_WHEEL_FRONT_LEFT:
		eAttachPart = VEH_WHEEL_LF;
		break;
	case CVA_WHEEL_FRONT_RIGHT:
		eAttachPart = VEH_WHEEL_RF;
		break;
	case CVA_WHEEL_REAR_LEFT:
		eAttachPart = VEH_WHEEL_LR;
		break;
	case CVA_WHEEL_REAR_RIGHT:
		eAttachPart = VEH_WHEEL_RR;
		break;

	case CVA_WINDSCREEN:
		eAttachPart = VEH_WINDSCREEN;
		break;
	case CVA_REARWINDOW:
		eAttachPart = VEH_WINDSCREEN_R;
		break;
	case CVA_WINDOW_FRONT_LEFT:
		eAttachPart = VEH_WINDOW_LF;
		break;
	case CVA_WINDOW_FRONT_RIGHT:
		eAttachPart = VEH_WINDOW_RF;
		break;

	case CVA_BONNET:
		eAttachPart = VEH_BONNET;
		break;
	case CVA_TRUNK:
		eAttachPart = VEH_BOOT;
		break;

	case CVA_PLANE_WINGTIP_LEFT:
		eAttachPart = PLANE_WINGTIP_1;
		break;
	case CVA_PLANE_WINGTIP_RIGHT:
		eAttachPart = PLANE_WINGTIP_2;
		break;
	case CVA_PLANE_WINGLIGHT_LEFT:
		eAttachPart = VEH_SIREN_2;
		break;
	case CVA_PLANE_WINGLIGHT_RIGHT:
		eAttachPart = VEH_SIREN_1;
		break;
	case CVA_PLANE_RUDDER:
		eAttachPart = PLANE_RUDDER;
		break;
	case CVA_PLANE_RUDDER_2:
		eAttachPart = PLANE_RUDDER_2;
		break;

	case CVA_HELI_ROTOR_MAIN:
		eAttachPart = HELI_ROTOR_MAIN;
		break;
	case CVA_HELI_ROTOR_REAR:
		eAttachPart = HELI_ROTOR_REAR;
		break;

	case CVA_HANDLEBAR_LEFT:
		eAttachPart = VEH_HBGRIP_L;
		break;
	case CVA_HANDLEBAR_RIGHT:
		eAttachPart = VEH_HBGRIP_R;
		break;

	case CVA_SUB_ELEVATOR_LEFT:
		eAttachPart = SUB_ELEVATOR_L;
		break;
	case CVA_SUB_ELEVATOR_RIGHT:
		eAttachPart = SUB_ELEVATOR_R;
		break;
	case CVA_SUB_RUDDER:
		eAttachPart = SUB_RUDDER;
		break;

	case CVA_SIREN_1:
			eAttachPart = VEH_SIREN_1;
		break;
	case CVA_HANDLEBAR_MAIN:
			eAttachPart = BIKE_HANDLEBARS;
		break;

	case CVA_TURRET_1_BASE:
		eAttachPart = VEH_TURRET_1_BASE;
		break;
	case CVA_TURRET_2_BASE:
		eAttachPart = VEH_TURRET_2_BASE;
		break;
	case CVA_TURRET_3_BASE:
		eAttachPart = VEH_TURRET_3_BASE;
		break;
	case CVA_TURRET_4_BASE:
		eAttachPart = VEH_TURRET_4_BASE;
		break;

	default:
		eAttachPart = VEH_INVALID_ID;
		break;
	}

	return eAttachPart; 
}

bool camCinematicMountedPartCamera::ShouldTerminateForSwingingDoors(CVehicle* pVehicle, eHierarchyId eAttachPart)
{
	if(pVehicle)
	{
		if(eAttachPart == VEH_WHEEL_LF || eAttachPart == VEH_WHEEL_LR)
		{
			CCarDoor* Door = pVehicle->GetDoorFromId(VEH_DOOR_DSIDE_F); 

			if(Door && !Door->GetIsLatched(pVehicle))
			{
				return true;
			}

			Door = pVehicle->GetDoorFromId(VEH_DOOR_DSIDE_R);

			if(Door && !Door->GetIsLatched(pVehicle))
			{
				return true;
			}
		}
		else if(eAttachPart == VEH_WHEEL_RF || eAttachPart == VEH_WHEEL_RR)
		{
			CCarDoor* Door = pVehicle->GetDoorFromId(VEH_DOOR_PSIDE_F); 

			if(Door && !Door->GetIsLatched(pVehicle))
			{
				return true;
			}

			Door = pVehicle->GetDoorFromId(VEH_DOOR_PSIDE_R);

			if(Door && !Door->GetIsLatched(pVehicle))
			{
				return true;

			}
		}
	}
	else
	{
		return true; 
	}
	return false; 
}

void camCinematicMountedPartCamera::ComputeAttachMatrix(Matrix34& vehicleMatrix, Matrix34& attachMatrix, Vector3& offsetFromVehicleToPart)
{
	//Attach the camera the vehicle offset position that corresponds to the requested shot.

	m_AttachParent.Get()->GetMatrixCopy(vehicleMatrix);
	attachMatrix = vehicleMatrix;
	offsetFromVehicleToPart.Zero();

	const CVehicle* attachVehicle = m_AttachParent->GetIsTypeVehicle() ? static_cast<const CVehicle*>(m_AttachParent.Get()) : NULL;
	if(!attachVehicle)
	{
		return;
	}
	
	eHierarchyId eAttachPart = ComputeVehicleHieracahyId(m_Metadata.m_AttachPart); 

	if (eAttachPart != VEH_INVALID_ID)
	{
		int boneIndex = attachVehicle->GetBoneIndex( eAttachPart );
		if (boneIndex != -1)
		{
			attachVehicle->GetGlobalMtx(boneIndex, attachMatrix);
			offsetFromVehicleToPart = attachMatrix.d - vehicleMatrix.d;
		}
		else
		{
			// This vehicle does not have the specified bone, fail the camera.
			m_canUpdate = false;
			return;
		}
	}

	if (eAttachPart == VEH_WINDSCREEN_R &&
		attachVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_LOWERED)
	{
		// Trunk camera does not work when roof is down.
		m_canUpdate = false;
		return;
	}
}

void camCinematicMountedPartCamera::UpdateOrientation(const CPhysical& attachPhysical, const Matrix34& vehicleMatrix, const Matrix34& attachMatrix,
													  const Vector3& cameraPosition, Vector3& lookAtPosition, const Vector3& UNUSED_PARAM(offsetFromVehicleToPart),
													  bool useWorldUp)
{
	// Vehicle part may not be facing up, so always apply z offset relative to vehicle up.
	Vector3 vLookAtOffset = m_Metadata.m_RelativeLookAtPosition;

#if 0
	vehicleMatrix.Transform(vLookAtOffset, lookAtPosition); //Transform to world space.

	lookAtPosition += offsetFromVehicleToPart;
#else
	Vector3 offsetForward, offsetRight, offsetUp;
	offsetForward.Scale(vehicleMatrix.b, vLookAtOffset.y);
	offsetRight.Scale(  vehicleMatrix.a, vLookAtOffset.x);
	offsetUp.Scale(     vehicleMatrix.c, vLookAtOffset.z);
	lookAtPosition = cameraPosition + offsetForward + offsetRight + offsetUp;
#endif

	Vector3 vForward = lookAtPosition - cameraPosition;
	vForward.Normalize();

	// Camera up is vehicle up, so camera rolls with the vehicle.
	Matrix34 lookAtMatrix;
	if (!useWorldUp)
	{
		camFrame::ComputeWorldMatrixFromFrontAndUp(vForward, vehicleMatrix.c, lookAtMatrix);
	}
	else
	{
		camFrame::ComputeWorldMatrixFromFront(vForward, lookAtMatrix);
	}
	lookAtMatrix.d = attachMatrix.d;

	if(m_SpringMount)
	{
		//Apply the spring mount.
		m_SpringMount->Update(lookAtMatrix, attachPhysical);
	}

	m_Frame.SetWorldMatrix(lookAtMatrix);
}

void camCinematicMountedPartCamera::UpdateFov()
{
	float fovToApply = m_Metadata.m_BaseFov;

	m_Frame.SetFov(fovToApply);
}

bool camCinematicMountedPartCamera::IsClipping(const Vector3& position, const Vector3& lookAt, const u32 flags)
{
	float waterHeight; 
	if(camCollision::ComputeWaterHeightAtPosition(position, m_Metadata.m_MaxDistanceForWaterClippingTest, waterHeight,  m_Metadata.m_MaxDistanceForRiverWaterClippingTest))
	{
		if(position.z <  waterHeight + m_Metadata.m_MinHeightAboveWater)
		{
			return true; 
		}
	}
	
	//First perform an undirected line test between the root position of the attach parent and the camera position, to ensure that the camera
	//is not completely penetrating a map collision mesh.
	//NOTE: We must explicitly ignore river and 'deep surface' bounds for this test, as they can clip inside the collision bounds of the attach parent. Likewise we consider mover bounds,
	//rather than weapon bounds, as the attach parent can clip into deep surfaces that are also flagged as weapon-type collision.
	WorldProbe::CShapeTestProbeDesc lineTest;
	lineTest.SetIsDirected(false);
	lineTest.SetIncludeFlags((flags | ArchetypeFlags::GTA_MAP_TYPE_MOVER) & ~(ArchetypeFlags::GTA_RIVER_TYPE | ArchetypeFlags::GTA_DEEP_SURFACE_TYPE | ArchetypeFlags::GTA_MAP_TYPE_WEAPON));
	lineTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	lineTest.SetContext(WorldProbe::LOS_Camera);
	lineTest.SetExcludeEntity(m_AttachParent, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
	lineTest.SetTreatPolyhedralBoundsAsPrimitives(true);
	
	Vector3 attachParentPosition = VEC3V_TO_VECTOR3(m_AttachParent->GetTransform().GetPosition());
	lineTest.SetStartAndEnd(attachParentPosition, position);

	bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(lineTest);
	
#if __BANK
	if(hasHit)
	{
		cameraDebugf3("coliding between root and atatch parent"); 
	}

#endif
	if(!hasHit)
	{
		//Now perform an undirected capsule test between the camera and look-at positions.
		WorldProbe::CShapeTestCapsuleDesc capsuleTest;
		capsuleTest.SetIncludeFlags(flags);
		capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
		capsuleTest.SetIsDirected(false);
		capsuleTest.SetContext(WorldProbe::LOS_Camera);
		capsuleTest.SetCapsule(lookAt, position, m_Metadata.m_CollisionRadius * m_Metadata.m_RadiusScalingForClippingTest);
	

		hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);

#if __BANK
		if(hasHit)
		{
			cameraDebugf3("coliding attach parent to look ahead"); 
		}
#endif
	}

	return hasHit; 
}

void camCinematicMountedPartCamera::UpdateHighSpeedShake(const CVehicle& attachVehicle)
{
	const camSpeedRelativeShakeSettingsMetadata& settings = m_Metadata.m_HighSpeedShakeSettings;

	const u32 shakeHash = settings.m_ShakeRef.GetHash();
	if(shakeHash == 0)
	{
		//No high-speed shake was specified in metadata.
		return;
	}

	camBaseFrameShaker* frameShaker = FindFrameShaker(shakeHash);
	if(!frameShaker)
	{
		//Try to kick-off the specified high-speed shake.
		frameShaker = Shake(shakeHash);
		if(!frameShaker)
		{
			return;
		}
	}

	//Consider the forward speed only.
	const Vector3 vehicleFront		= VEC3V_TO_VECTOR3(attachVehicle.GetTransform().GetB());
	const Vector3& vehicleVelocity	= attachVehicle.GetVelocity();
	const float forwardSpeed		= DotProduct(vehicleVelocity, vehicleFront);
	const float desiredSpeedFactor	= SlowInOut(RampValueSafe(forwardSpeed, settings.m_MinForwardSpeed, settings.m_MaxForwardSpeed, 0.0f, 1.0f));

	float speedFactorToApply		= m_HighSpeedShakeSpring.Update(desiredSpeedFactor, settings.m_SpringConstant);
	speedFactorToApply				= Clamp(speedFactorToApply, 0.0f, 1.0f); //For safety.

	frameShaker->SetAmplitude(speedFactorToApply);
}
