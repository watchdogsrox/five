//
// camera/scripted/RoundedSplineCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/scripted/RoundedSplineCamera.h"
#include "camera/system/CameraMetadata.h"

INSTANTIATE_RTTI_CLASS(camRoundedSplineCamera,0x63B7AB86)

camRoundedSplineCamera::camRoundedSplineCamera(const camBaseObjectMetadata& metadata)
: camBaseSplineCamera(metadata)
, m_Metadata(static_cast<const camRoundedSplineCameraMetadata&>(metadata))
{

}

void camRoundedSplineCamera::BuildTranslationSpline()
{
	u32 numNodes = m_Nodes.GetCount();
	if(numNodes == 2)
	{
		m_ShouldLerp = true; //We can only LERP with 2 nodes!
		return;
	}

	m_ShouldLerp = false;
	const u32 lastNodeIndex = numNodes - 1;

	Vector3 velocity;
	for(u32 i=1; i<lastNodeIndex; i++)
	{
		camSplineNode* previousNode	= m_Nodes[i - 1];
		camSplineNode* currentNode	= m_Nodes[i];
		camSplineNode* nextNode		= m_Nodes[i + 1];

	#if SUPPORT_ZERO_DURATION_NODES
		// The following allows zero duration nodes to be handled properly.
		if (currentNode->GetTransitionDelta() == 0.0f)
		{
			velocity.Zero();
			currentNode->SetTranslationVelocity(velocity);
			continue;
		}
	#endif // SUPPORT_ZERO_DURATION_NODES

		//Split the angle.
		Vector3 velocityOut = nextNode->GetPosition() - currentNode->GetPosition();
		velocityOut.Normalize();
		Vector3 velocityIn = previousNode->GetPosition() - currentNode->GetPosition();
		velocityIn.Normalize();
		velocity = velocityOut - velocityIn;
		velocity.Normalize();
		currentNode->SetTranslationVelocity(velocity);
	}

	//Calculate start and end velocities.
	velocity = ComputeStartTranslationVelocity(0);
	m_Nodes[0]->SetTranslationVelocity(velocity);
	velocity = ComputeEndTranslationVelocity(lastNodeIndex);
	m_Nodes[lastNodeIndex]->SetTranslationVelocity(velocity);
}

void camRoundedSplineCamera::ValidateSplineDuration()
{
	cameraAssertf(m_Duration != 0, "The camera (name: %s, hash: %u) requires a spline duration, make sure that it is set. Defaulting the spline duration to 3 secs"
		,GetName(), GetNameHash());

	if(m_Duration == 0)
	{
		m_Duration = 3000; 		
	}
}