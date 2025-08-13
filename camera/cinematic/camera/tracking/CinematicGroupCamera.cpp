//
// camera/cinematic/CinematicGroupCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "CinematicGroupCamera.h"
#include "camera/cinematic/cinematicdirector.h"
#include "camera/CamInterface.h"
#include "camera/helpers/Collision.h"
#include "peds/ped.h"
#include "physics/gtaArchetype.h"
#include "scene/Entity.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camCinematicGroupCamera,0x60152CDA)

camCinematicGroupCamera::camCinematicGroupCamera(const camBaseObjectMetadata& metadata)
: camBaseCinematicTrackingCamera(metadata)
, m_Metadata(static_cast<const camCinematicGroupCameraMetadata&>(metadata))
, m_RelativeOffsetFromTarget(VEC3_ZERO)
, m_GroupCentrePosition(VEC3_ZERO)
, m_GroupAverageFront(VEC3_ZERO)
, m_InitialRelativeCameraFront(VEC3_ZERO)
, m_CameraPos(VEC3_ZERO)
, m_DistanceToFurthestEntityFromGroupCentre(0.0f)
, m_Fov(50.0f)
, m_CameraOrbitDistanceScalar(m_Metadata.m_OrbitDistanceScalar)
, m_ShotTimeSpentOccluded(0)
, m_CanUpdate(true)
{
}

bool camCinematicGroupCamera::IsValid()
{
	return m_CanUpdate; 
}

bool camCinematicGroupCamera::Update()
{
	if(m_Entities.GetCount() < 2)
	{
		m_CanUpdate = false; 
		return false; 
	}

	ComputeGroupCentre(); 	

	ComputeGroupAverageFront(); 
	
	if(GetNumUpdatesPerformed() == 0)
	{
		ComputeInitalCameraFront(); 
	}

	ComputeDistanceToFurthestEntityFromGroupCentre(); 

	ComputeFov(); 

	Vector3 PreviousCameraPosition = m_CameraPos; 

	ComputeCameraPosition(); 

	if(m_NumUpdatesPerformed == 0)
	{	
		if(!camInterface::GetCinematicDirector().IsCameraPositionValidWithRespectToTheLine(m_LookAtTarget, m_CameraPos))
		{
			m_CanUpdate = false; 
			return m_CanUpdate;
		}
	}

	if(m_NumUpdatesPerformed > 0)
	{
		if(HasCameraCollidedBetweenUpdates(PreviousCameraPosition, m_CameraPos))
		{
			m_CanUpdate = false; 
			return false; 
		}
	}

	if (IsClipping(m_CameraPos, (ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE ))
	{
		m_CanUpdate = false; 
		return false; 
	}
	
	if(!HaveLineOfSightToEnoughEntites())
	{
		m_CanUpdate = false; 
		return false; 
	}
	
	m_Frame.SetFov(m_Fov * RtoD); 

	m_Frame.SetPosition(m_CameraPos); 

	Vector3 Look = m_GroupCentrePosition - m_Frame.GetPosition();

	m_Frame.SetWorldMatrixFromFront(Look); 
		
	return m_CanUpdate; 
}

void camCinematicGroupCamera::Init(const atVector<RegdConstEnt>& Entities)
{
	SetEntityList(Entities); 
}

void camCinematicGroupCamera::SetEntityList(const atVector<RegdConstEnt>& Entities)
{
	m_Entities.Reset(); 
	m_Entities.Reserve(Entities.GetCount()); 

	for(int i = 0; i < Entities.GetCount(); i++)
	{
		m_Entities.Append() = Entities[i]; 
	}
}

void camCinematicGroupCamera::ComputeGroupCentre()
{
	Vector3 AveragePos = VEC3_ZERO; 
	u32 NumberOfEntities = 0; 

	for(int i = 0; i < m_Entities.GetCount(); i++)
	{
		if(m_Entities[i])
		{
			AveragePos += VEC3V_TO_VECTOR3(m_Entities[i]->GetTransform().GetPosition());
			NumberOfEntities++; 
		}
		
	}

	if(NumberOfEntities > 0)
	{
		AveragePos.InvScale((float)NumberOfEntities); 
	}

	m_GroupCentrePosition = AveragePos; 

	DampGroupCentrePositionChange(); 
}

void camCinematicGroupCamera::DampGroupCentrePositionChange()
{
	const CPed* pPed = camInterface::FindFollowPed(); 	

	if(pPed)
	{
		Matrix34 playerMatMinusRoll; 
		Vector3 PlayerFront = MAT34V_TO_MATRIX34(pPed->GetTransform().GetMatrix()).b; 
			
		PlayerFront.Normalize(); 

		camFrame::ComputeWorldMatrixFromFront(PlayerFront, playerMatMinusRoll);
		playerMatMinusRoll.d = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()); 
		
		playerMatMinusRoll.UnTransform(m_GroupCentrePosition); 

		if(GetNumUpdatesPerformed() == 0)
		{
			m_PositionSpringX.Reset(m_GroupCentrePosition.x); 
			m_PositionSpringY.Reset(m_GroupCentrePosition.y); 
			m_PositionSpringZ.Reset(m_GroupCentrePosition.z); 
		}
		else
		{
			m_PositionSpringX.Update(m_GroupCentrePosition.x, m_Metadata.m_GroupCentreSpringConstant, 1.0f, true); 
			m_PositionSpringY.Update(m_GroupCentrePosition.y, m_Metadata.m_GroupCentreSpringConstant, 1.0f, true); 
			m_PositionSpringZ.Update(m_GroupCentrePosition.z, m_Metadata.m_GroupCentreSpringConstant, 1.0f, true); 

			m_GroupCentrePosition.x = m_PositionSpringX.GetResult(); 
			m_GroupCentrePosition.y = m_PositionSpringY.GetResult(); 
			m_GroupCentrePosition.z = m_PositionSpringZ.GetResult(); 
		}
		
		playerMatMinusRoll.Transform(m_GroupCentrePosition); 
	}
}

void camCinematicGroupCamera::ComputeGroupAverageFront()
{
	Vector3 Forward = VEC3_ZERO; 
	Vector3 SafeFallBack(0.0f, 1.0f, 0.0f); 
	s32 NumOfEntities =0; 

	for(int i= 0; i < m_Entities.GetCount(); i++)
	{
		if(m_Entities[i])
		{
			Forward += VEC3V_TO_VECTOR3(m_Entities[i]->GetTransform().GetForward()); 
			NumOfEntities++;
		}
	}

	if(NumOfEntities)
	{
		Forward.NormalizeSafe(SafeFallBack); 
		m_GroupAverageFront = Forward; 

		DampGroupAverageFrontVector(); 
	}
}

void camCinematicGroupCamera::DampGroupAverageFrontVector()
{
	if(GetNumUpdatesPerformed() == 0)
	{
		m_AverageFrontDampX.Reset(m_GroupAverageFront.x); 
		m_AverageFrontDampY.Reset(m_GroupAverageFront.y); 
		m_AverageFrontDampZ.Reset(m_GroupAverageFront.z); 
	}
	else
	{
		m_AverageFrontDampX.Update(m_GroupAverageFront.x, m_Metadata.m_AverageFrontSpringConstant, 1.0f, true); 
		m_AverageFrontDampY.Update(m_GroupAverageFront.y, m_Metadata.m_AverageFrontSpringConstant, 1.0f, true); 
		m_AverageFrontDampZ.Update(m_GroupAverageFront.z, m_Metadata.m_AverageFrontSpringConstant, 1.0f, true); 

		m_GroupAverageFront.x = m_AverageFrontDampX.GetResult(); 
		m_GroupAverageFront.y = m_AverageFrontDampY.GetResult(); 
		m_GroupAverageFront.z = m_AverageFrontDampZ.GetResult(); 
	}
}

void camCinematicGroupCamera::ComputeDistanceToFurthestEntityFromGroupCentre()
{
	float radiusSquared = 0.0f; 

	for(int i = 0; i <m_Entities.GetCount(); i++)
	{
		if(m_Entities[i])	
		{
			float DistanceSquaredToGroupCentre = VEC3V_TO_VECTOR3(m_Entities[i]->GetTransform().GetPosition()).Dist2(m_GroupCentrePosition); 
			if ( DistanceSquaredToGroupCentre > radiusSquared)
			{
				radiusSquared = DistanceSquaredToGroupCentre; 
			}
		}
	}	

	m_DistanceToFurthestEntityFromGroupCentre = Sqrtf(radiusSquared); 
}

void camCinematicGroupCamera::ComputeInitalCameraFront()
{
	Matrix34 m_TrackingMat(M34_IDENTITY); 	
	camFrame::ComputeWorldMatrixFromFront(m_GroupAverageFront, m_TrackingMat); 

	m_TrackingMat.d = m_GroupCentrePosition; 

	float desiredHeading = 0.0f; 
	
	//set to meta data
	const float desiredPitch = fwRandom::GetRandomNumberInRange( 10.0f , 20.0f );
	//const float Heading = fwRandom::GetRandomNumberInRange( 0.0f, TWO_PI );

	const float currentHeading = m_Frame.ComputeHeading();
	
	//metadata min angle
	const float minHeadingBetweenShots = DtoR * 45.0f; 
	const float HeadingRange = 2.0f * ( PI - minHeadingBetweenShots );

	const float HeadingDeltaWithinRange = fwRandom::GetRandomNumberInRange( 0.0f, HeadingRange );
	desiredHeading = currentHeading + minHeadingBetweenShots + HeadingDeltaWithinRange;
	desiredHeading = fwAngle::LimitRadianAngle( desiredHeading );

	Vector3 Front = VEC3_ZERO; 
	camFrame::ComputeFrontFromHeadingAndPitch(desiredHeading, desiredPitch * DtoR, Front); 
	
	m_TrackingMat.UnTransform3x3(Front, m_InitialRelativeCameraFront); 
}

void camCinematicGroupCamera::ComputeFov()
{	
	m_CameraOrbitDistanceScalar = m_Metadata.m_OrbitDistanceScalar; 

	m_Fov = rage::Atan2f( m_DistanceToFurthestEntityFromGroupCentre , m_CameraOrbitDistanceScalar) * 2.0f; 

	if(m_Fov < m_Metadata.m_FovLimits.x *DtoR)
	{
		m_Fov = Clamp(m_Fov, m_Metadata.m_FovLimits.x *DtoR, m_Metadata.m_FovLimits.y*DtoR); 
	}
	else if(m_Fov >= m_Metadata.m_FovLimits.y)
	{
		m_Fov = Clamp(m_Fov, m_Metadata.m_FovLimits.x *DtoR, m_Metadata.m_FovLimits.y*DtoR); 

		float newFov = m_Fov / 2.0f; 

		m_CameraOrbitDistanceScalar = m_DistanceToFurthestEntityFromGroupCentre / rage::Tanf(newFov); 
	}

	DampFov(); 
	DampOrbitDistanceScalar(); 
}


void camCinematicGroupCamera::DampFov()
{	
	if(GetNumUpdatesPerformed() == 0)
	{
		m_FovSpring.Reset(m_Fov); 
	}
	else
	{
		m_FovSpring.Update(m_Fov, m_Metadata.m_FovSpringConstant, 1.0f, true); 

		m_Fov = m_FovSpring.GetResult(); 
	}
}

void camCinematicGroupCamera::DampOrbitDistanceScalar()
{
	if(GetNumUpdatesPerformed()  == 0)
	{
		m_DistanceScalarSpring.Reset(m_CameraOrbitDistanceScalar); 
	}
	else
	{
		m_DistanceScalarSpring.Update(m_CameraOrbitDistanceScalar, m_Metadata.m_OrbitDistanceScalarSpringConstant, 1.0f, true); 

		m_CameraOrbitDistanceScalar = m_DistanceScalarSpring.GetResult(); 
	}

}

void camCinematicGroupCamera::ComputeCameraPosition()
{	
	Matrix34 m_TrackingMat(M34_IDENTITY); 	
	camFrame::ComputeWorldMatrixFromFront(m_GroupAverageFront, m_TrackingMat); 

	m_TrackingMat.d = m_GroupCentrePosition; 

	Vector3 Front = m_InitialRelativeCameraFront * m_CameraOrbitDistanceScalar;

	m_TrackingMat.Transform(Front); 
	
	m_CameraPos = Front; 

}

bool camCinematicGroupCamera::HasCameraCollidedBetweenUpdates(const Vector3& PreviousPosition, const Vector3& currentPosition)
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

bool camCinematicGroupCamera::IsClipping(const Vector3& position, const u32 flags)
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

	return WorldProbe::GetShapeTestManager()->SubmitTest(sphereTest);
}

bool camCinematicGroupCamera::HaveLineOfSightToEnoughEntites()
{
	s32 NumberOfEntitiesHaveLineOfSight =0;
	for(int i=0; i < m_Entities.GetCount(); i++)
	{
		if(m_Entities[i])	
		{
			Vector3 EntityPos = VEC3V_TO_VECTOR3(m_Entities[i]->GetTransform().GetPosition()); 
			
			if (camCollision::ComputeIsLineOfSightClear(EntityPos, m_CameraPos, m_Entities[i],
				camCollision::GetCollisionIncludeFlags() & ~(ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE)))
			{
				NumberOfEntitiesHaveLineOfSight++;
			}
		}
	}
	//min num for in line of sight meta data
	if(NumberOfEntitiesHaveLineOfSight > 1)
	{
		m_ShotTimeSpentOccluded = 0; 
		return true;
	}
	else
	{
		if(GetNumUpdatesPerformed() == 0)
		{
			return false; 
		}

		m_ShotTimeSpentOccluded += fwTimer::GetCamTimeStepInMilliseconds();  
	}

	if(m_ShotTimeSpentOccluded > m_Metadata.m_MaxTimeToSpendOccluded)
	{
		return false;
	}
	else
	{
		return true;  
	}
}
