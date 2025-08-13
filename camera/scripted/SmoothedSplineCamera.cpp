//
// camera/scripted/SmoothedSplineCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/scripted/SmoothedSplineCamera.h"
#include "camera/system/CameraMetadata.h"

INSTANTIATE_RTTI_CLASS(camSmoothedSplineCamera,0x5358ACCD)

camSmoothedSplineCamera::camSmoothedSplineCamera(const camBaseObjectMetadata& metadata)
: camRoundedSplineCamera(metadata)
, m_Metadata(static_cast<const camSmoothedSplineCameraMetadata&>(metadata))
{

}

void camSmoothedSplineCamera::BuildTranslationSpline()
{
	camRoundedSplineCamera::BuildTranslationSpline();

	for(u32 i=0; i<m_Metadata.m_NumTnSplineSmoothingPasses; i++)
	{
		SmoothTranslationVelocities();
	}
}

void camSmoothedSplineCamera::SmoothTranslationVelocities()
{
	const u32 numNodes = m_Nodes.GetCount();
	const u32 lastNodeIndex = numNodes - 1;

	Vector3 newVelocity;
	Vector3 oldVelocity = ComputeStartTranslationVelocity(0);
	for(u32 i=1; i<lastNodeIndex; i++)
	{
		camSplineNode* previousNode = m_Nodes[i - 1];
		camSplineNode* currentNode = m_Nodes[i];

		float combinedTransitionDelta = previousNode->GetTransitionDelta() + currentNode->GetTransitionDelta();
		if(combinedTransitionDelta > 0.0f)
		{
			newVelocity = (ComputeEndTranslationVelocity(i) * currentNode->GetTransitionDelta()) +
				(ComputeStartTranslationVelocity(i) * previousNode->GetTransitionDelta());
			newVelocity /= combinedTransitionDelta;
		}
		else
		{
			newVelocity = VEC3_ZERO; //Avoid divide by zero.
		}

		previousNode->SetTranslationVelocity(oldVelocity);
		oldVelocity = newVelocity;
	}

	Vector3 endVelocity = ComputeEndTranslationVelocity(lastNodeIndex);
	m_Nodes[lastNodeIndex]->SetTranslationVelocity(endVelocity);
	m_Nodes[lastNodeIndex - 1]->SetTranslationVelocity(oldVelocity);
}
