//
// camera/cinematic/CinematicAnimatedCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/CamInterface.h"
#include "camera/cinematic/camera/animated/CinematicAnimatedCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/system/CameraMetadata.h"
#include "peds/Ped.h"
#include "peds/PedIntelligence.h"
#include "task/Combat/TaskCombatMelee.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camCinematicAnimatedCamera,0xAED1523F)


camCinematicAnimatedCamera::camCinematicAnimatedCamera(const camBaseObjectMetadata& metadata)
: camAnimatedCamera(metadata)
, m_Metadata(static_cast<const camCinematicAnimatedCameraMetadata&>(metadata))
, m_IsClipping(false)
{
}

bool camCinematicAnimatedCamera::IsValid()
{
	return !m_IsClipping; 
}

bool camCinematicAnimatedCamera::Update()
{
	camAnimatedCamera::Update(); 
	
	m_IsClipping = ComputeIsCameraClipping(GetFrame().GetPosition());

	return !m_IsClipping;
}

u32 camCinematicAnimatedCamera::ComputeValidCameraAnimId(CEntity* pTargetEntity, const CActionResult* pActionResult)
{
	//get the vector between the player and the target to work out if its on the left or the right
	const CPed* pPlayer = camInterface::FindFollowPed();

	if(!pPlayer || !pTargetEntity)
	{
		return 0; 
	}
	cameraAssertf(pTargetEntity->GetIsTypePed(), "camCinematicAnimatedCamera::GetValidCameraId The target entity must be a ped");

	if(!pTargetEntity->GetIsTypePed())
	{
		return 0; 
	}

	Vector3 playerPos= VEC3_ZERO; 
	Vector3 targetPos = VEC3_ZERO; 
	Vector3 lineOfAction = VEC3_ZERO; 

	playerPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
	targetPos = VEC3V_TO_VECTOR3(pTargetEntity->GetTransform().GetPosition()); 

	lineOfAction = targetPos - playerPos;
	lineOfAction.NormalizeSafe(YAXIS);

	const float lineOfActionHeading						= camFrame::ComputeHeadingFromFront(lineOfAction);
	const float gameplayCameraHeading					= camInterface::GetGameplayDirector().GetFrame().ComputeHeading();
	const float lineOfActionToGameplayHeadingDelta		= fwAngle::LimitRadianAngle(gameplayCameraHeading - lineOfActionHeading);
	
	//currently this is inverted as the anims have been exported with reference to the victim
	eSideOfTheActionLine sideOfThelineOfAction	= (lineOfActionToGameplayHeadingDelta >= 0.0f) ?  RIGHT_HAND_SIDE : LEFT_HAND_SIDE;
	
	CPed* pPed = static_cast<CPed*>(pTargetEntity); 

	return ComputeValidCameraId(pPed, sideOfThelineOfAction, pActionResult); 
}

void camCinematicAnimatedCamera::ComputeLookAtPosition(const CPed* pTarget, Vector3& lookAtPosition)
{	
	if(pTarget && pTarget->GetIsTypePed())
	{
		const CPed* lookAtPed = static_cast<const CPed*>(pTarget);

		eAnimBoneTag lookAtBoneTag = BONETAG_SPINE2; 

		Matrix34 boneMatrix;
		lookAtPed->GetBoneMatrix(boneMatrix, lookAtBoneTag);

		lookAtPosition = boneMatrix.d;
	}
}

void camCinematicAnimatedCamera::AddEntityToExceptionList(const CPed* pPed, atArray<const phInst*> &excludeInstances)
{
	//Exclude all of the ped's immediate child attachments.
	const fwEntity* childAttachment = pPed->GetChildAttachment();
	while(childAttachment)
	{
		const phInst* attachmentInstance = childAttachment->GetCurrentPhysicsInst();
		if(attachmentInstance)
		{
			excludeInstances.Grow() = attachmentInstance;
		}

		childAttachment = childAttachment->GetSiblingAttachment();
	}

		//Exclude the follow ped capsule and ragdoll bounds.

		const phInst* animatedInstance = pPed->GetAnimatedInst();
		if(animatedInstance)
		{
			excludeInstances.Grow() = animatedInstance;
		}

		const phInst* ragdollInstance = (const phInst*)pPed->GetRagdollInst();
		if(ragdollInstance)
		{
			excludeInstances.Grow() = ragdollInstance;
		}
}


bool camCinematicAnimatedCamera::ComputeIsCameraPositionValid(const CPed* pTargetEntity, const Vector3& lookAtPosition, const Vector3& cameraPosition)
{
	const CPed* followPed = camInterface::FindFollowPed();
	const CPed* targetPed = static_cast<const CPed*>(pTargetEntity);
	
	if(!followPed)
	{
		return false;
	}

	WorldProbe::CShapeTestCapsuleDesc capsuleTest;
	WorldProbe::CShapeTestFixedResults<> shapeTestResults;
	capsuleTest.SetResultsStructure(&shapeTestResults);
	capsuleTest.SetIncludeFlags((ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE );
	capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	capsuleTest.SetContext(WorldProbe::LOS_Camera);
	
	atArray<const phInst*> excludeInstances;

	AddEntityToExceptionList(followPed, excludeInstances); 
	AddEntityToExceptionList(targetPed, excludeInstances); 

	const int numExcludeInstances = excludeInstances.GetCount();
	if(numExcludeInstances > 0)
	{
		capsuleTest.SetExcludeInstances(excludeInstances.GetElements(), numExcludeInstances);
	}

	//Test line of sight.
	capsuleTest.SetCapsule(lookAtPosition, cameraPosition, g_CinematicAnimatedCamCollisionRadius);
	const bool hasHit		= WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);

	return !hasHit; 
}

bool camCinematicAnimatedCamera::ComputeIsCameraClipping(const Vector3& cameraPosition)
{
	WorldProbe::CShapeTestSphereDesc sphereTest;
	sphereTest.SetIncludeFlags((ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE );
	sphereTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	sphereTest.SetContext(WorldProbe::LOS_Camera);
	sphereTest.SetSphere(cameraPosition, g_CinematicAnimatedCamCollisionRadius);
	sphereTest.SetTreatPolyhedralBoundsAsPrimitives(true);
	const bool isClipping = WorldProbe::GetShapeTestManager()->SubmitTest(sphereTest);

	return isClipping;
}

Vector3 camCinematicAnimatedCamera::ComputeCameraPositionFromAnim(fwClipSet* pClipSet, fwMvClipId ClipId, CTaskMelee* pMeleeTask)
{
	crClip* pClip = pClipSet->GetClip(ClipId);

	//Get the first frame of the animation
	Matrix34 mMat(M34_IDENTITY);
	Quaternion qRot (Quaternion::sm_I);
	Vector3 vAnimPos = VEC3_ZERO; 

	CCutsceneAnimation::GetQuaternionValue(pClip,kTrackCameraRotation, 0.0f, qRot );
	CCutsceneAnimation::GetVector3Value(pClip,kTrackCameraTranslation, 0.0f, vAnimPos ); 
	mMat.FromQuaternion(qRot);
	mMat.d = vAnimPos;

	//calculate where the ped will be at the start of the animation
	//pMeleeTask->CalculateStealthKillWorldOffset(); 

	Matrix34 targetMat(M34_IDENTITY);
	targetMat.d = VEC3V_TO_VECTOR3( pMeleeTask->GetStealthKillWorldOffset() ); 
	targetMat.MakeRotateZ(pMeleeTask->GetStealthKillHeading()); 

	//put the camera anim into world space. 
	mMat.Dot(targetMat);

	return mMat.d; 
}

u32 camCinematicAnimatedCamera::ComputeValidCameraId(const CPed* pTargetEntity, eSideOfTheActionLine SideOfTheActionLine, const CActionResult* pMeleeActionResult)
{
	const CPed* pPlayer = camInterface::FindFollowPed();

	if(!pPlayer)
	{	
		return 0; 
	}
	
	u32 CameraHash = 0; 

	CTaskMelee*	pMeleeTask = pPlayer->GetPedIntelligence()->GetTaskMelee(); 

	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(pMeleeActionResult->GetClipSet());

	switch(SideOfTheActionLine)
	{
		case RIGHT_HAND_SIDE:
			{			
				for(int i = 0; i < pMeleeActionResult->GetRightCameraAnims().GetCount(); i++)
				{
					fwMvClipId ClipId(pMeleeActionResult->GetRightCameraAnims()[i].GetHash()); 

					//see if this position is valid
					Vector3 lookAtPosition = VEC3_ZERO; 
					Vector3 camPosition = VEC3_ZERO; 
					
					ComputeLookAtPosition(pTargetEntity, lookAtPosition); 
					
					camPosition = ComputeCameraPositionFromAnim(pClipSet, ClipId, pMeleeTask );

					if(!ComputeIsCameraClipping(camPosition))
					{
						if(ComputeIsCameraPositionValid(pTargetEntity, lookAtPosition, camPosition) )
						{
							CameraHash = pMeleeActionResult->GetRightCameraAnims()[i].GetHash(); 
							break; 
						}
					}
				}
			}
			break; 

		case LEFT_HAND_SIDE:
			{
				for(int i = 0; i < pMeleeActionResult->GetLeftCameraAnims().GetCount(); i++)
				{
					fwMvClipId ClipId(pMeleeActionResult->GetLeftCameraAnims()[i].GetHash()); 

					//see if this position is valid
					Vector3 lookAtPosition = VEC3_ZERO; 
					Vector3 camPosition = VEC3_ZERO; 

					ComputeLookAtPosition(pTargetEntity, lookAtPosition); 

					camPosition = ComputeCameraPositionFromAnim(pClipSet, ClipId, pMeleeTask);

					if(!ComputeIsCameraClipping(camPosition))
					{
						if(ComputeIsCameraPositionValid(pTargetEntity, lookAtPosition, camPosition))
						{
							CameraHash = pMeleeActionResult->GetLeftCameraAnims()[i].GetHash(); 
							break; 
						}
					}
				}
			}
			break;
	}

	return CameraHash; 
}