//
//camera/cinematic/CinematicVehicleTrackingCamera.cpp
//
//Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "CinematicVehicleTrackingCamera.h"

#include "camera/helpers/Collision.h"
#include "camera/system/CameraMetadata.h"
#include "crskeleton/skeleton.h"
#include "vehicles/vehicle.h"
#include "camera/CamInterface.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camCinematicVehicleTrackingCamera,0xCE74C8C3)

camCinematicVehicleTrackingCamera::camCinematicVehicleTrackingCamera(const camBaseObjectMetadata& metadata)
: camBaseCamera(metadata)
, m_Metadata(static_cast<const camCinematicVehicleTrackingCameraMetadata&>(metadata))
, m_startPos(VEC3_ZERO)
, m_endPos(VEC3_ZERO)
, m_startLookAtPosition(VEC3_ZERO)
, m_endLookAtPosition(VEC3_ZERO)
, m_phase(0.0f)
, m_IntitalState(-1)
, m_HoldAtEndTime(0)
, m_canUpdate(false)
{

}

void camCinematicVehicleTrackingCamera::Init()
{
	m_canUpdate = false;  

	if(m_AttachParent == NULL)
	{
		return;  
	}

	camCinematicDirector::eSideOfTheActionLine sideOfThelineOfAction = camInterface::GetCinematicDirector().GetSideOfActionLine(m_LookAtTarget); 

	if(sideOfThelineOfAction == camCinematicDirector::ACTION_LINE_ANY)
	{
		sideOfThelineOfAction = ComputeSideOfAction(); 
	}

	if(m_AttachParent->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(m_AttachParent.Get()); 

		if(pVehicle->DoesVehicleHaveAConvertibleRoofAnimation())
		{
			m_IntitalState = pVehicle->GetConvertibleRoofState(); 	
		}
		else
		{
			return; 
		}
	}
	else
	{
		return; 
	}

	if( ComputeStartAndEndPositions(sideOfThelineOfAction)) 
	{
		if(!UpdateCollision(m_startPos, m_endPos))
		{
			//swap sides we failed due to collision
			if(sideOfThelineOfAction == camCinematicDirector::ACTION_LINE_LEFT)
			{
				sideOfThelineOfAction = camCinematicDirector::ACTION_LINE_RIGHT; 
			}
			else if(sideOfThelineOfAction == camCinematicDirector::ACTION_LINE_RIGHT)
			{
				sideOfThelineOfAction = camCinematicDirector::ACTION_LINE_LEFT; 
			}

			if(ComputeStartAndEndPositions(sideOfThelineOfAction)) 
			{
				if(!UpdateCollision(m_startPos, m_endPos))
				{
					return; 
				}
			}
			else
			{
				return; 
			}
		}
		//change to return a valid position
		if(ComputeLookAtPosition(sideOfThelineOfAction))
		{
			m_canUpdate = true; 
		}
	}
}

bool camCinematicVehicleTrackingCamera::HasRoofChangedState() const
{
	if(m_AttachParent && m_AttachParent->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(m_AttachParent.Get()); 

		if(pVehicle->DoesVehicleHaveAConvertibleRoofAnimation())
		{
			s32 roofState = pVehicle->GetConvertibleRoofState(); 

			if(roofState == CTaskVehicleConvertibleRoof::STATE_LOWERING && m_IntitalState == CTaskVehicleConvertibleRoof::STATE_RAISING)
			{
				return true; 	
			}

			if(roofState == CTaskVehicleConvertibleRoof::STATE_RAISING&& m_IntitalState == CTaskVehicleConvertibleRoof::STATE_LOWERING)
			{
				return true; 	
			}
		}
	}
	return false; 
};

bool camCinematicVehicleTrackingCamera::Update()
{
	if(GetNumUpdatesPerformed() == 0)
	{
		if(!m_canUpdate)
		{
			return false; 
		}
	}
	
	if(m_AttachParent == NULL)
	{
		m_canUpdate = false; 
		return false; 
	}
	
	if(!HasRoofChangedState())
	{
		UpdatePhase(); 
	}
	else
	{
		m_canUpdate = false; 
	}

	if(IsHoldAtEndTimeUp())
	{
		m_canUpdate = false; 
	}
	
	Vector3 camPosition = Lerp(m_phase, m_startPos, m_endPos); 
	Vector3 LookAtPosition = Lerp(m_phase, m_startLookAtPosition, m_endLookAtPosition); 

	if(IsClipping(camPosition, (ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE ))
	{
		m_canUpdate = false; 
		return false; 
	}

	Vector3 front = VEC3_ZERO;
	front = LookAtPosition - camPosition; 
	front.NormalizeSafe(YAXIS); 

	m_Frame.SetWorldMatrixFromFront(front);

	m_Frame.SetPosition(camPosition); 
	m_Frame.SetFov(m_Metadata.m_Fov); 

	return m_canUpdate; 
}

bool camCinematicVehicleTrackingCamera::IsClipping(const Vector3& position, const u32 flags)
{
	float  waterHeight; 

	if(camCollision::ComputeWaterHeightAtPosition(position, m_Metadata.m_MaxDistanceForWaterClippingTest, waterHeight, m_Metadata.m_MaxDistanceForRiverWaterClippingTest))
	{
		if(position.z <  waterHeight + m_Metadata.m_MinHeightAboveWater)
		{
			return true; 
		}
	}

	//Change the camera if it is clipping. 
	WorldProbe::CShapeTestSphereDesc sphereTest;
	//dont need to test against both the animated instance and ragdoll, so just test against ragdoll
	sphereTest.SetIncludeFlags(flags); 
	//sphereTest.SetIncludeFlags((ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE );
	sphereTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	sphereTest.SetContext(WorldProbe::LOS_Camera);
	sphereTest.SetSphere(position, m_Metadata.m_CollisionRadius * m_Metadata.m_RadiusScalingForClippingTest);
	sphereTest.SetTreatPolyhedralBoundsAsPrimitives(true);
	if(m_AttachParent)
	{
		sphereTest.SetExcludeEntity(m_AttachParent, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS); 
	}
	
	bool results = WorldProbe::GetShapeTestManager()->SubmitTest(sphereTest); 

	return results;
}

bool camCinematicVehicleTrackingCamera::UpdateCollision(Vector3& cameraDesiredPosition, const Vector3& lookAtPosition)
{
	bool shouldUpdateThisFrame = true; 

	//Compute collision against all entites first time around only update if the camera is not constrained in any way.
	if(m_NumUpdatesPerformed == 0)
	{
		if(ComputeCollision(cameraDesiredPosition, lookAtPosition, (ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE))
		{
			shouldUpdateThisFrame = false; 
		}
	}

	return shouldUpdateThisFrame; 
}

bool camCinematicVehicleTrackingCamera::ComputeCollision(Vector3& cameraPosition, const Vector3&  lookAtPosition, const u32 flags)
{

	Vector3 desiredCameraPosition; 
	bool hasHit = false;


	// Constrain the camera against the world
	WorldProbe::CShapeTestCapsuleDesc constrainCapsuleTest;
	WorldProbe::CShapeTestFixedResults<> constrainShapeTestResults;

	constrainCapsuleTest.SetResultsStructure(&constrainShapeTestResults);
	constrainCapsuleTest.SetIncludeFlags(flags);
	constrainCapsuleTest.SetIsDirected(false);
	constrainCapsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST); 
	constrainCapsuleTest.SetContext(WorldProbe::LOS_Camera);

	if(m_AttachParent)
	{
		constrainCapsuleTest.SetExcludeEntity(m_AttachParent, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS); 
	}

	constrainCapsuleTest.SetCapsule(lookAtPosition, cameraPosition, m_Metadata.m_CollisionRadius * m_Metadata.m_RadiusScalingForCapsule);

	hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(constrainCapsuleTest);

	return hasHit; 
}


bool camCinematicVehicleTrackingCamera::IsValid()
{
	return m_canUpdate; 
}

void camCinematicVehicleTrackingCamera::UpdatePhase()
{
	if(m_AttachParent && m_AttachParent->GetIsTypeVehicle() && m_phase < 1.0f - SMALL_FLOAT)
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(m_AttachParent.Get()); 
		if(pVehicle->DoesVehicleHaveAConvertibleRoofAnimation())
		{
			m_phase = pVehicle->GetConvertibleRoofProgress(); 

		//1 is open 0 is closed
		
			if(pVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_RAISING)
			{
				m_phase = 1.0f - m_phase; 
			}

			m_phase = SlowInOut(m_phase); 
		}
	}
}

bool camCinematicVehicleTrackingCamera::IsHoldAtEndTimeUp()
{
	if(m_phase >= 1.0f - SMALL_FLOAT)
	{
		m_HoldAtEndTime += fwTimer::GetCamTimeStepInMilliseconds(); 
	
		if(m_HoldAtEndTime > m_Metadata.m_HoldAtEndTime)
		{
			return true; 
		}
	}

	return false; 
}

void camCinematicVehicleTrackingCamera::ComputeWorldSpacePositionRelativeToVehicleBoneWithOffset(const CVehicle* pVehicle, s32 BoneIndex, const Vector3& LocalOffset, Vector3& WorldPos)
{
	if(pVehicle && BoneIndex > -1)
	{
		CVehicleModelInfo* pModelInfo = pVehicle->GetVehicleModelInfo(); 

		if(pModelInfo)
		{
			crSkeleton* pSkeleton = pVehicle->GetSkeleton();
			
			if(pSkeleton)
			{
				Matrix34 VehicleMat = MAT34V_TO_MATRIX34(pVehicle->GetTransform().GetMatrix()); 

				Mat34V boneMat;

				pSkeleton->GetGlobalMtx(pModelInfo->GetBoneIndex(BoneIndex), boneMat);

				Vector3 boneWorldPosition = VEC3V_TO_VECTOR3(boneMat.d()); 

				VehicleMat.d = boneWorldPosition; 

				VehicleMat.Transform(LocalOffset, WorldPos); 
			}
		}
	}
}

bool camCinematicVehicleTrackingCamera::ComputeWorldSpaceOrientationRelativeToVehicleBone(const CVehicle* pVehicle, s32 BoneIndex, Quaternion& WorldOrientation, bool UseXAxis, bool UseYAxis, bool UseZAxis)
{
	if(pVehicle && BoneIndex > -1)
	{
		CVehicleModelInfo* pModelInfo = pVehicle->GetVehicleModelInfo(); 

		if(pModelInfo)
		{
			crSkeleton* pSkeleton = pVehicle->GetSkeleton();

			if(pSkeleton)
			{
				Matrix34 VehicleMat = MAT34V_TO_MATRIX34(pVehicle->GetTransform().GetMatrix()); 
				Mat34V boneMat;

				pSkeleton->GetGlobalMtx(pModelInfo->GetBoneIndex(BoneIndex), boneMat);

				Vector3 vBoneEulers = MAT34V_TO_MATRIX34(boneMat).GetEulers();
				Vector3 vWorldEulers =  VehicleMat.GetEulers();

				if (UseXAxis)
				{
					vWorldEulers.x = vBoneEulers.x;
				}

				if (UseYAxis)
				{
					vWorldEulers.y = vBoneEulers.y;
				}
			
				if (UseZAxis)
				{
					vWorldEulers.z = vBoneEulers.z;
				}

				WorldOrientation.FromEulers(vWorldEulers);
				return true;
			}
		}
	}
	return false;
}

void camCinematicVehicleTrackingCamera::ComputeRelativePostionToVehicleBoneFromWorldSpacePosition(const CVehicle* pVehicle, s32 BoneIndex, const Vector3& WorldPos, Vector3& LocalOffset)
{
	if(pVehicle)
	{
		CVehicleModelInfo* pModelInfo = pVehicle->GetVehicleModelInfo(); 

		if(pModelInfo)
		{
			crSkeleton* pSkeleton = pVehicle->GetSkeleton();

			if(pSkeleton)
			{
				Matrix34 VehicleMat = MAT34V_TO_MATRIX34(pVehicle->GetTransform().GetMatrix()); 

				Mat34V boneMat;

				pSkeleton->GetGlobalMtx(pModelInfo->GetBoneIndex(BoneIndex), boneMat);

				Vector3 boneWorldPosition = VEC3V_TO_VECTOR3(boneMat.d()); 

				VehicleMat.d = boneWorldPosition; 

				VehicleMat.UnTransform(WorldPos, LocalOffset); 
			}
		}
	}
}



bool camCinematicVehicleTrackingCamera::ComputeStartAndEndPositions(camCinematicDirector::eSideOfTheActionLine SideOfTheAction)
{
	if(m_AttachParent && m_AttachParent->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(m_AttachParent.Get()); 

		s32 StartBoneIndex = -1; 
		s32 EndBoneIndex = -1; 
		
		Vector3 vOffset = m_Metadata.m_PositionOffset; 

		float initialInterpValue = 0.0f; 
		float finalInterpValue = 1.0f; 

		if(pVehicle->DoesVehicleHaveAConvertibleRoofAnimation())
		{
			if(pVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_LOWERING)
			{
				initialInterpValue = m_Metadata.m_RoofLowerPositionBlends.x; 
				finalInterpValue = m_Metadata.m_RoofLowerPositionBlends.y; 
				
				if(SideOfTheAction == camCinematicDirector::ACTION_LINE_LEFT)	
				{
					//change the sign of the offset as we need to mirror the offsets relative to the vehicle
					StartBoneIndex = VEH_WHEEL_LF; 
					EndBoneIndex = VEH_WHEEL_LR; 
					vOffset.x = vOffset.x * -1.0f; 
				}
				else if(SideOfTheAction == camCinematicDirector::ACTION_LINE_RIGHT)
				{
					StartBoneIndex = VEH_WHEEL_RF; 
					EndBoneIndex = VEH_WHEEL_RR; 
				}
			}
			else if(pVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_RAISING)
			{
				initialInterpValue = m_Metadata.m_RoofRaisePositionBlends.x; 
				finalInterpValue = m_Metadata.m_RoofRaisePositionBlends.y; 
				
				if(SideOfTheAction == camCinematicDirector::ACTION_LINE_LEFT)	
				{
					StartBoneIndex = VEH_WHEEL_LR; 
					EndBoneIndex = VEH_WHEEL_LF; 
					vOffset.x = vOffset.x * -1.0f; 
				}
			
				else if(SideOfTheAction == camCinematicDirector::ACTION_LINE_RIGHT)
				{
					StartBoneIndex = VEH_WHEEL_RR; 
					EndBoneIndex = VEH_WHEEL_RF; 
				}
			}

			if(StartBoneIndex > -1 && EndBoneIndex > -1)
			{
				CVehicleModelInfo* pModelInfo = pVehicle->GetVehicleModelInfo(); 

				if(pModelInfo && pModelInfo->GetBoneIndex(StartBoneIndex) > -1 && pModelInfo->GetBoneIndex(EndBoneIndex)  > -1 )
				{
					Vector3 startPos; 
					Vector3 endPos; 

					ComputeWorldSpacePositionRelativeToVehicleBoneWithOffset(pVehicle, StartBoneIndex, vOffset, m_startPos);

					ComputeWorldSpacePositionRelativeToVehicleBoneWithOffset(pVehicle, EndBoneIndex, vOffset, m_endPos);

					startPos = Lerp(initialInterpValue, m_startPos, m_endPos); 
					endPos = Lerp(finalInterpValue, m_startPos, m_endPos); 

					m_startPos = startPos; 
					m_endPos = endPos;
					return true;
				}
			}
		}
	}
	return false; 
}

bool camCinematicVehicleTrackingCamera::ComputeLookAtPosition(camCinematicDirector::eSideOfTheActionLine SideOfTheAction)
{
	if(m_AttachParent && m_AttachParent->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(m_AttachParent.Get());
		
		if(pVehicle)
		{
			s32 StartBoneIndex = -1; 
			s32 EndBoneIndex = -1; 
			
			Vector3 vOffset = m_Metadata.m_LookAtOffset; 

			CVehicleModelInfo* pModelInfo = pVehicle->GetVehicleModelInfo(); 

			crSkeleton* pSkeleton = pVehicle->GetSkeleton();

			float initialInterpValue = 0.0f; 
			float finalInterpValue = 0.0f; 

			if(pSkeleton && pModelInfo)
			{
				if(pVehicle->DoesVehicleHaveAConvertibleRoofAnimation())
				{
					if(pVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_LOWERING)
					{
						initialInterpValue = m_Metadata.m_RoofLowerLookAtPositionBlends.x; 
						finalInterpValue = m_Metadata.m_RoofLowerLookAtPositionBlends.y; 

						if(SideOfTheAction == camCinematicDirector::ACTION_LINE_LEFT)	
						{
							StartBoneIndex = VEH_WHEEL_RF; 
							EndBoneIndex = VEH_WHEEL_RR; 
						}
						else if(SideOfTheAction == camCinematicDirector::ACTION_LINE_RIGHT)
						{
							vOffset.x = vOffset.x * -1.0f; 
							StartBoneIndex = VEH_WHEEL_LF; 
							EndBoneIndex = VEH_WHEEL_LR; 
						}
					}
					else if(pVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_RAISING)
					{
						initialInterpValue = m_Metadata.m_RoofRaiseLookAtPositionBlends.x; 
						finalInterpValue = m_Metadata.m_RoofRaiseLookAtPositionBlends.y; 

						if(SideOfTheAction == camCinematicDirector::ACTION_LINE_LEFT )	
						{
							StartBoneIndex = VEH_WHEEL_RR; 
							EndBoneIndex = VEH_WHEEL_RF; 
						}
						else if(SideOfTheAction == camCinematicDirector::ACTION_LINE_RIGHT)
						{
							vOffset.x = vOffset.x * -1.0f; 
							StartBoneIndex = VEH_WHEEL_LR; 
							EndBoneIndex = VEH_WHEEL_LF; 
						}
					}
				}

				if(pModelInfo->GetBoneIndex(StartBoneIndex) > -1 && pModelInfo->GetBoneIndex(EndBoneIndex) > -1)
				{				
					Vector3 startPos; 
					Vector3 endPos; 

					ComputeWorldSpacePositionRelativeToVehicleBoneWithOffset(pVehicle, StartBoneIndex, vOffset, m_startLookAtPosition);

					ComputeWorldSpacePositionRelativeToVehicleBoneWithOffset(pVehicle, EndBoneIndex, vOffset, m_endLookAtPosition);

					startPos = Lerp(initialInterpValue, m_startLookAtPosition, m_endLookAtPosition); 
					endPos = Lerp(finalInterpValue, m_startLookAtPosition, m_endLookAtPosition); 

					m_startLookAtPosition = startPos; 
					m_endLookAtPosition = endPos;
					return true; 
				}
			}
		}
	}
	return false; 
}

camCinematicDirector::eSideOfTheActionLine camCinematicVehicleTrackingCamera::ComputeSideOfAction()
{
	camCinematicDirector::eSideOfTheActionLine sideOfThelineOfAction = camCinematicDirector::ACTION_LINE_ANY; 
	
	if(m_AttachParent)
	{
		const float lineOfActionHeading						= m_AttachParent->GetTransform().GetHeading(); //camFrame::ComputeHeadingFromFront();
		const float gameplayCameraHeading					= camInterface::GetHeading();
		const float lineOfActionToGameplayHeadingDelta		= fwAngle::LimitRadianAngle(gameplayCameraHeading - lineOfActionHeading);

		//currently this is inverted as the anims have been exported with reference to the victim
		sideOfThelineOfAction	= (lineOfActionToGameplayHeadingDelta >= 0.0f) ?  camCinematicDirector::ACTION_LINE_RIGHT : camCinematicDirector::ACTION_LINE_LEFT;
	}

	return sideOfThelineOfAction; 
};
