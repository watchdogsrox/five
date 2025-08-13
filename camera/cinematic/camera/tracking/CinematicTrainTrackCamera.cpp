//
// camera/cinematic/CinematicCamManCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/cinematic/camera/tracking/CinematicTrainTrackCamera.h"

#include "camera/CamInterface.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Collision.h"
#include "camera/system/CameraMetadata.h"
#include "game/ModelIndices.h"
#include "peds/ped.h"
#include "vehicles/train.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camCinematicTrainTrackCamera,0xFE895475)

camCinematicTrainTrackCamera::camCinematicTrainTrackCamera(const camBaseObjectMetadata& metadata)
: camCinematicCamManCamera(metadata)
, m_Metadata(static_cast<const camCinematicTrainTrackingCameraMetadata&>(metadata))
, m_CameraPosition(VEC3_ZERO)
, m_LookAtPosition(VEC3_ZERO)
, m_BaseCamMatrix(M34_IDENTITY)
{
}

bool camCinematicTrainTrackCamera::IsValid()
{
	return m_HasAcquiredTarget; 
}

bool camCinematicTrainTrackCamera::ComputeCameraTrackPosition(CTrain* engine, Vector3& FinalCamPos)
{	
		
	//get a shot distance
	float shotDist = fwRandom::GetRandomNumberInRange(m_Metadata.m_ShotDistanceAheadOfTrain.x,  m_Metadata.m_ShotDistanceAheadOfTrain.y);
	
	//store the node and track index
	s32 nodeIndex = engine->m_currentNode; // FindClosestTrackNode(MAT34V_TO_MATRIX34(engine->GetMatrix()).d, trackIndex); 
	s8 trackIndex = engine->m_trackIndex; 

	CTrainTrack* pTrack = engine->GetTrainTrack(trackIndex); 

	float distance = 0.0f;  
	s32 previousNodeIndex = nodeIndex; 
	s32 NummberOfNodes = 0;	//makes sure we have at least 2 nodes in  front of the train to calculate direction.
	while ((distance <  shotDist * shotDist) || NummberOfNodes < 2)
	{
		Vector3 targetCamPos = VEC3_ZERO; 
	
		if(nodeIndex ==  pTrack->m_numNodes)
		{
			nodeIndex = 0; 
		}

		engine->GetWorldPosFromTrackNode(targetCamPos,trackIndex,nodeIndex);

		distance = targetCamPos.Dist2(VEC3V_TO_VECTOR3(engine->GetTransform().GetPosition())); 

		previousNodeIndex = nodeIndex;
		nodeIndex++; 
		NummberOfNodes++; 
	}

	Vector3 previousNodePos = VEC3_ZERO; 
	Vector3 targetNodePos = VEC3_ZERO;	

	engine->GetWorldPosFromTrackNode(previousNodePos,trackIndex,previousNodeIndex);
	engine->GetWorldPosFromTrackNode(targetNodePos,trackIndex,nodeIndex);

	//calculate the direction of travel between two nodes
	Vector3 direction = targetNodePos - previousNodePos; 

	direction.Normalize(); 

	m_BaseCamMatrix.d = targetNodePos; 

	camFrame::ComputeWorldMatrixFromFront(direction, m_BaseCamMatrix); 
	
	float SideOffsetMin; 
	float SideOffsetMax;
	float VerticalOffsetMin; 
	float VerticalOffsetMax; 

	switch(m_RequestedShotMode)
	{
	case T_FIXED:
		{
			SideOffsetMin = m_Metadata.m_FixedShotSideOffset.x; 
			SideOffsetMax =  m_Metadata.m_FixedShotSideOffset.y;
			VerticalOffsetMin = m_Metadata.m_FixedShotVerticalOffset.x;
			VerticalOffsetMax = m_Metadata.m_FixedShotVerticalOffset.y;
		}
		break;

	default:
		{
			SideOffsetMin = m_Metadata.m_TrackingShotSideOffset.x; 
			SideOffsetMax =  m_Metadata.m_TrackingShotSideOffset.y;
			VerticalOffsetMin = m_Metadata.m_TrackingShotVerticalOffset.x;
			VerticalOffsetMax = m_Metadata.m_TrackingShotVerticalOffset.y;
		}
		break;
	}


	//Now calculate a position in front of this for the potential new camera pos 
	 float SideOffset = fwRandom::GetRandomNumberInRange(SideOffsetMin, SideOffsetMax);
	 float SideOffsetSign = fwRandom::GetRandomNumberInRange(-1.0f, 1.0f);	
	 SideOffset*= Sign(SideOffsetSign);

	//calculate vertical offset
	const float VerticalOffset = fwRandom::GetRandomNumberInRange( VerticalOffsetMin, VerticalOffsetMax);

	Vector3 camOffset(SideOffset, 0.0f, VerticalOffset);  
	Vector3 finalCamPos = VEC3_ZERO;  
	
	m_BaseCamMatrix.Transform(camOffset, finalCamPos); 

	//conduct the shape test from slightly above the node pos
	Vector3 ShapeTestPos = m_BaseCamMatrix.d; 
	ShapeTestPos.z += 0.5f; 

	// Constrain the camera against the world
	WorldProbe::CShapeTestCapsuleDesc constrainCapsuleTest;
	WorldProbe::CShapeTestFixedResults<> constrainShapeTestResults;

	constrainCapsuleTest.SetResultsStructure(&constrainShapeTestResults);
	constrainCapsuleTest.SetIncludeFlags(camCollision::GetCollisionIncludeFlags());
	constrainCapsuleTest.SetIsDirected(true);
	constrainCapsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST); 
	constrainCapsuleTest.SetContext(WorldProbe::LOS_Camera);
	constrainCapsuleTest.SetCapsule(ShapeTestPos, finalCamPos, 0.3f);

	bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(constrainCapsuleTest);
	const float hitTValue	= hasHit ? constrainShapeTestResults[0].GetHitTValue() : 1.0f;

	finalCamPos = Lerp(hitTValue, ShapeTestPos, finalCamPos);

	//respect the minimum distance
	Vector3 offsetVector; 

	//get the local offset vector, if the z component is less than 3 
	m_BaseCamMatrix.UnTransform(finalCamPos, offsetVector); 

	if(Abs(offsetVector.x) < SideOffsetMin )
	{
		return false; 
	}

	FinalCamPos = finalCamPos; 
	
	return true; 
}

bool camCinematicTrainTrackCamera::UpdateShotOfType()
{
	bool bShouldUpdate = false; 

	switch(m_RequestedShotMode)
	{
		case T_TRACKING:
			{			
				Vector3 BackOfTrainOffset(0.0f, 0.0f, 1.0f); 

				//default offset vector looking at train
				Vector3 front = m_LookAtPosition - m_CameraPosition;
		
				front.Normalize();
				m_Frame.SetWorldMatrixFromFront(front);
				
				CVehicle* targetVehicle	= (CVehicle*)m_LookAtTarget.Get();
				CTrain* engine	= CTrain::FindCaboose(static_cast<CTrain*>(targetVehicle));

				//If the back of the train has passed the camera say we are done.  
				Matrix34 TrainMat = MAT34V_TO_MATRIX34(engine->GetTransform().GetMatrix());

				Vector3 TrainPos = VEC3_ZERO; 

				TrainMat.Transform(BackOfTrainOffset, TrainPos); 

				Vector3 TrainToNode = m_BaseCamMatrix.d - TrainPos; 

				float dotTrainForwardToNodeForward = m_BaseCamMatrix.b.Dot(TrainToNode); 
				
				//also calculate the distance beyond the camera 
				TrainPos.z = 0.0f;
				Vector3 camPos = m_BaseCamMatrix.d; 
				camPos.z = 0.0f;
				
				//select a distance to terminate the shot
				Vector2 DistanceBeyondCameraRange (10.0f, 25.0f); 
				
				float DistanceBeyondCamera = fwRandom::GetRandomNumberInRange(DistanceBeyondCameraRange.x, DistanceBeyondCameraRange.y); 

				if(dotTrainForwardToNodeForward < 0.0f && TrainPos.Dist2(camPos) > DistanceBeyondCamera* DistanceBeyondCamera)
				{
					bShouldUpdate = false; 
				}
				else
				{
					bShouldUpdate = true; 
				}
			}
			break;

		case T_FIXED:
			{
				Vector3 BackOfTrainOffset(0.0f, 0.0f, 1.0f); 
				
				if(m_NumUpdatesPerformed == 0)
				{
					Vector3 front = m_LookAtPosition - m_CameraPosition;
					front.Normalize();
					m_Frame.SetWorldMatrixFromFront(front);
				}

				//m_Frame.SetPosition(m_CameraPos);
				CVehicle* targetVehicle	= (CVehicle*)m_LookAtTarget.Get();
				CTrain* caboose	= CTrain::FindCaboose(static_cast<CTrain*>(targetVehicle));

				//If the back of the train has passed the camera say we are done.  
				Matrix34 TrainMat = MAT34V_TO_MATRIX34(caboose->GetTransform().GetMatrix());

				Vector3 TrainPos = VEC3_ZERO; 
					
				TrainMat.Transform(BackOfTrainOffset, TrainPos); 
				
				Vector3 TrainToNode = m_BaseCamMatrix.d - TrainPos; 

				float dotTrainForwardToNodeForward = m_BaseCamMatrix.b.Dot(TrainToNode); 

				if(dotTrainForwardToNodeForward < 0.0f)
				{
					return false; 
				}
				else
				{
					bShouldUpdate = true; 
				}
				
			}
			break;
	};

	return bShouldUpdate; 
}

//Create a look at position at the front of the train 
void camCinematicTrainTrackCamera::ComputeLookAtPos(CTrain* pEngine, Vector3 &NewLookAt, const Vector3& vOffsetVector)
{
	if(pEngine)	
	{
		Matrix34 trainMat = MAT34V_TO_MATRIX34(pEngine->GetMatrix()); 

		//transform the offset into train space
		trainMat.Transform(vOffsetVector, NewLookAt); 
	}
}

bool camCinematicTrainTrackCamera::Update()
{

	if((m_LookAtTarget.Get() == NULL) || !m_LookAtTarget->GetIsTypeVehicle()) //Safety check.
	{
		m_HasAcquiredTarget = false;
		return false;
	}

	CVehicle* targetVehicle	= (CVehicle*)m_LookAtTarget.Get();
	if(!cameraVerifyf(targetVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN, "A cinematic train-tracking camera has targetted a non-train vehicle"))
	{
		m_HasAcquiredTarget = false;
		return false;
	}

	CTrain* engine	= CTrain::FindEngine(static_cast<CTrain*>(targetVehicle));
	CTrain* caboose	= CTrain::FindCaboose(static_cast<CTrain*>(targetVehicle));

	if(!engine || !caboose)
	{
		m_HasAcquiredTarget = false;
		return false;
	}

	if(m_NumUpdatesPerformed == 0)
	{
		if(!ComputeCameraTrackPosition(engine, m_CameraPosition))
		{
			m_HasAcquiredTarget = false;
			return false;
		}
		m_Frame.SetPosition(m_CameraPosition);
	}

	ComputeLookAtPos(engine, m_LookAtPosition, m_Metadata.m_EngineRelativeLookAtOffset); 
	
	if (!UpdateShotOfType())
	{
		m_HasAcquiredTarget = false;
		return false;
	}

	if(!ComputeIsPositionValid())
	{
		m_HasAcquiredTarget = false;
		return false;
	}

	return m_HasAcquiredTarget;
}

bool camCinematicTrainTrackCamera::ComputeIsPositionValid()
{
	// const Vector3 targetPosition	= VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition());
	const Vector3& cameraPosition	= m_Frame.GetPosition();
	//Vector3 delta					= targetPosition - cameraPosition;
	//float distanceToTarget			= delta.Mag();

	CVehicle* targetVehicle			= (CVehicle*)m_LookAtTarget.Get();
	bool isTargetInside				= targetVehicle->m_nFlags.bInMloRoom;

	//Change the camera if it is clipping. 
	WorldProbe::CShapeTestSphereDesc sphereTest;
	//dont need to test against both the animated instance and ragdoll, so just test against ragdoll
	sphereTest.SetIncludeFlags((ArchetypeFlags::GTA_ALL_TYPES_WEAPON & ~ArchetypeFlags::GTA_PED_TYPE) | ArchetypeFlags::GTA_RAGDOLL_TYPE );
	sphereTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	sphereTest.SetContext(WorldProbe::LOS_Camera);
	sphereTest.SetSphere(m_Frame.GetPosition(), m_Metadata.m_CollisionRadius);
	sphereTest.SetTreatPolyhedralBoundsAsPrimitives(true);

	if(WorldProbe::GetShapeTestManager()->SubmitTest(sphereTest))
	{
		return false;
	}

	//We don't want the camera outside looking inside or vice versa.
	s32 dummyRoomIdx;
	CInteriorInst* pIntInst = NULL;
	CPortalTracker::ProbeForInterior(cameraPosition, pIntInst, dummyRoomIdx, NULL, 10.0f);
	bool isCameraInside = (pIntInst != NULL);

	if(isTargetInside != isCameraInside)
	{
		return false;
	}
	
	//m_PreviousTimeHadLosToTarget = fwTimer::GetCamTimeInMilliseconds(); //Always report good LOS for trains, as the camera nodes are custom.

	return true;
}

//float camCinematicTrainTrackCamera::GetAngleToTrain(const Vector3& trainLookAtOffset )
//{
//	//calulate angle to back of train 
//
//	CVehicle* targetVehicle	= (CVehicle*)m_LookAtTarget.Get();
//	CTrain* engine	= CTrain::FindEngine(static_cast<CTrain*>(targetVehicle));
//
//	Matrix34 trainMat = MAT34V_TO_MATRIX34(engine->GetMatrix()); 
//
//	Vector3 TrainLookAt; 
//
//	trainMat.Transform(trainLookAtOffset, TrainLookAt); 
//
//	//calculate the angle to the back of train
//
//	Vector3 CameraToBackOfTrain = TrainLookAt - m_CameraPos; 
//
//	CameraToBackOfTrain.Normalize(); 
//
//	Vector3 engineForward = VEC3V_TO_VECTOR3(engine->GetTransform().GetB()); 
//
//	engineForward.Normalize(); 
//
//	float DotCameraEngine = CameraToBackOfTrain.Dot(engineForward); 
//
//	//Displayf("GetAngleToTrain::DotCameraEngine: %f, trainLookAtOffset: %f, %f, %f", DotCameraEngine, trainLookAtOffset.x, trainLookAtOffset.y, trainLookAtOffset.z); 
//
//	return DotCameraEngine; 
//}

//bool camCinematicTrainTrackCamera::ComputeHasShotReachedTargetAngle(float Desired)
//{
//	CVehicle* targetVehicle	= (CVehicle*)m_LookAtTarget.Get();
//	CTrain* engine	= CTrain::FindEngine(static_cast<CTrain*>(targetVehicle));
//	
//	Vector3 CameraToEngine = m_LookAtPos - m_CameraPos; 
//
//	CameraToEngine.Normalize(); 
//
//	Vector3 engineForward = VEC3V_TO_VECTOR3(engine->GetTransform().GetB()); 
//
//	engineForward.Normalize(); 
//
//	float DotCameraEngine = CameraToEngine.Dot(engineForward); 
//
//	//Displayf("ComputeHasShotReachedTargetAngle::DotCameraEngine: %f", DotCameraEngine); 
//	
//	if(-DotCameraEngine < Desired)
//	{
//		return true;
//	}
//
//	return false; 
//}

//Vector3 camCinematicTrainTrackCamera::ComputeUpdateOffsetVector()
//{
//	TUNE_GROUP_FLOAT(CAMERA, AngleLerpScalar, 1.0f, 0.0f, 10.0f, 0.1f); 
//	
//	Vector3 OffsetVectorToBackOfTrain(0.0f, -11.1f, 1.0f); 
//	Vector3 OffsetVectorToFrontOfTrain(0.0f, 11.1f, 1.0f); 
//	
//	Vector3 offsetLookAtPos = VEC3_ZERO; 
//
//	float AngleToBackOfTrain = GetAngleToTrain(OffsetVectorToBackOfTrain); 
//
//	float AngleToFrontOfTrain = GetAngleToTrain(OffsetVectorToFrontOfTrain);
//
//	if(!m_HaveComputedTotalAngle)
//	{	
//		m_TotalAngle = abs (AngleToBackOfTrain) + abs(AngleToFrontOfTrain); 
//		m_HaveComputedTotalAngle = true; 
//	}
//
//	//need to avoid a divide by zero here
//	//cameraAssertf(m_TotalAngle != 0.0f, "m_TotalAngle must be non zero to avoid a divide by zero"); 
//
//	float LerpValue = 1.0f - (abs(AngleToBackOfTrain) / abs(m_TotalAngle)); 
//	
//	LerpValue = Clamp(LerpValue, 0.0f, 1.0f);
//
//	if(LerpValue > m_LerpMax)
//	{
//		m_LerpMax = LerpValue;
//	}
//	else
//	{
//		LerpValue = m_LerpMax; 
//	}	
//
//	Displayf("LerpValue: %f", LerpValue); 
//
//	LerpValue *= AngleLerpScalar; 
//
//	offsetLookAtPos = Lerp(SlowInOut(LerpValue), OffsetVectorToFrontOfTrain, OffsetVectorToBackOfTrain); 
//
//	return offsetLookAtPos; 
//}

//Angle approach: The angular velocity relative to camera is non constant, meaning the camera sweeps in a non-uniform way.
//Vector3 camCinematicTrainTrackCamera::ComputeUpdateOffsetVector()
//{
//	TUNE_GROUP_FLOAT(CAMERA, AngleOfEffect, 45.0f, -500.0f, 500.0f, 1.0f); 
//	
//	TUNE_GROUP_FLOAT(CAMERA, MaxAngleRange, 45.0f, -500.0f, 500.0f, 1.0f); 
//	TUNE_GROUP_FLOAT(CAMERA, MInRange, 275.0f, -500.0f, 500.0f, 1.0f); 
//	
//
//	Vector3 OffsetVectorToBackOfTrain(0.0f, -11.1f, 1.0f); 
//	Vector3 OffsetVectorToFrontOfTrain(0.0f, 11.1f, 1.0f); 
//
//	CVehicle* targetVehicle	= (CVehicle*)m_LookAtTarget.Get();
//	CTrain* engine	= CTrain::FindEngine(static_cast<CTrain*>(targetVehicle));
//	
//	Vector3 offsetLookAtPos = VEC3_ZERO; 
//
//	Vector3 baseCamPos = m_BaseCamPos.d; 
//	baseCamPos.z = 0.0f; 
//	
//	Vector3 camPos = m_CameraPos; 
//	camPos.z = 0.0f;
//
//	grcDebugDraw:: 
//
//	float DistanceToCenter = baseCamPos.Dist(camPos); 
//
//	float AngleRange = 45.0f; 
//
//	float LerpAngleRange = DistanceToCenter / 25.0f; //max x distance
//
//	LerpAngleRange = Lerp(LerpAngleRange, MInRange , MaxAngleRange); 
//
//	Vector3 ToTrackCentre = m_BaseCamPos.d - m_CameraPos; 
//
//	ToTrackCentre.Normalize();
//
//	Vector3 ToTrainCentre = VEC3V_TO_VECTOR3(engine->GetTransform().GetPosition()) - m_CameraPos; 
//
//	ToTrainCentre.Normalize(); 
//
//	float AngleDelta =	camFrame::ComputeHeadingDeltaBetweenFronts(ToTrackCentre, ToTrainCentre);
//
//	AngleDelta *= RtoD; 
//
//	Vector3 TrainRight = VEC3V_TO_VECTOR3(engine->GetTransform().GetA());
//	TrainRight.Normalize(); 
//	
//	float DotLeft = ToTrackCentre.Dot(TrainRight); 
//
//	float LerpValue = RampValueSafe(AngleDelta, LerpAngleRange * Sign(DotLeft) * -1.0f, LerpAngleRange * Sign(DotLeft), 0.0f, 1.0f);
//
//	offsetLookAtPos = Lerp(LerpValue, OffsetVectorToFrontOfTrain, OffsetVectorToBackOfTrain); 
//	
//	Displayf("LerpValue: %f offsetLookAtPos: %f, %f, %f, angle:%f", LerpValue, offsetLookAtPos.x, offsetLookAtPos.y, offsetLookAtPos.z, AngleDelta); 
//
//	return offsetLookAtPos;
//}

//Distance to node approach: Problem there was a pop often when the camera transitioned between the front and back carriage
//Vector3 camCinematicTrainTrackCamera::ComputeUpdateOffsetVector()
//{
//	TUNE_GROUP_FLOAT(CAMERA, MinRange, -11.1f, -500.0f, 500.0f, 1.0f); 
//	TUNE_GROUP_FLOAT(CAMERA, MaxRange, 11.1f, -500.0f, 500.0f, 1.0f); 
//
//	Vector3 OffsetVectorToBackOfTrain(0.0f, -11.1f, 1.0f); 
//	Vector3 OffsetVectorToFrontOfTrain(0.0f, 11.1f, 1.0f); 
//	Vector3 offsetLookAtPos = VEC3_ZERO; 
//
//	//**
//	//test the dif in target and  engine pos
//	CVehicle* targetVehicle	= (CVehicle*)m_LookAtTarget.Get();
//	CTrain* engine	= CTrain::FindEngine(static_cast<CTrain*>(targetVehicle));
//	
//	Vector3 TartgetPos = VEC3V_TO_VECTOR3(m_LookAtTarget->GetTransform().GetPosition()); 
//	Vector3 EnginePos =  VEC3V_TO_VECTOR3(engine->GetTransform().GetPosition()); 	
//	
//	Vector3 EnginePosUp(EnginePos.x, EnginePos.y, EnginePos.z+5.0f); 
//	Vector3 TartgetPosUp(TartgetPos.x, TartgetPos.y, TartgetPos.z+5.0f);
//
//	grcDebugDraw::Line(EnginePos, EnginePosUp, Color_green); 
//	grcDebugDraw::Line(TartgetPos, TartgetPosUp, Color_cyan); 
//
//	Displayf("m_LookAtTarget Pos: %f, %f, %f", TartgetPos.x, TartgetPos.y, TartgetPos.z); 
//	Displayf("Engine Pos: %f, %f, %f", EnginePos.x, EnginePos.y, EnginePos.z); 
//	//**
//
//	grcDebugDraw::Axis(m_BaseCamPos, 1.0, true);
//
//	grcDebugDraw::Line(m_CameraPos, m_BaseCamPos.d, Color_blue); 
//	
//	grcDebugDraw::Sphere(m_CameraPos, 0.5f, Color_blue); 
//
//	Vector3 nodePos = m_BaseCamPos.d; 
//	
//	nodePos.z = 0.0f; 
//
//	Vector3 EnginePosZeroZ = EnginePos; 
//	EnginePosZeroZ.z = 0.0f; 
//
//	//calculate distance to train node	
//	float distanceTrainCentreToNode = EnginePosZeroZ.Dist(nodePos); 
//	
//	//get the sign of the direction of travel
//	Vector3 TrainToNode = m_BaseCamPos.d - EnginePos; 
//
//	float dotTrainForwardToNodeForawrd = m_BaseCamPos.b.Dot(TrainToNode); 
//
//	//Set the sign of the distance
//	distanceTrainCentreToNode*= Sign(dotTrainForwardToNodeForawrd); 
//
//	float LerpValue = RampValueSafe(distanceTrainCentreToNode, MinRange, MaxRange, 0.0f, 1.0f);
//
//	offsetLookAtPos = Lerp(SlowInOut(LerpValue), OffsetVectorToBackOfTrain, OffsetVectorToFrontOfTrain); 
//
//	Displayf("LerpValue: %f offsetLookAtPos: %f, %f, %f, distance:%f", LerpValue, offsetLookAtPos.x, offsetLookAtPos.y, offsetLookAtPos.z, distanceTrainCentreToNode); 
//
//	return offsetLookAtPos;
//}
