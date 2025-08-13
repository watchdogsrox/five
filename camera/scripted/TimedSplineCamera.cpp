//
// camera/scripted/TimedSplineCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/scripted/TimedSplineCamera.h"
#include "camera/system/CameraMetadata.h"

INSTANTIATE_RTTI_CLASS(camTimedSplineCamera,0x83F24791)

camTimedSplineCamera::camTimedSplineCamera(const camBaseObjectMetadata& metadata)
: camSmoothedSplineCamera(metadata)
, m_Metadata(static_cast<const camTimedSplineCameraMetadata&>(metadata))
{

}

void camTimedSplineCamera::ValidateSplineDuration()
{
	if(m_Duration == 0)
	{
		m_Duration = (u32)m_TotalDelta; //Automatically set the duration to the sum of the transition times.
	}
}

void camTimedSplineCamera::BuildTranslationSpline()
{
	camRoundedSplineCamera::BuildTranslationSpline();

	for(u32 i=0; i<m_Metadata.m_NumTnSplineSmoothingPasses; i++)
	{
		SmoothTranslationVelocities();
		ConstrainTranslationVelocities();
	}
}

void camTimedSplineCamera::ConstrainTranslationVelocities()
{
	const u32 numNodes = m_Nodes.GetCount();
	for(u32 i=1; i<numNodes-1; i++)
	{
		camSplineNode* previousNode	= m_Nodes[i - 1];
		camSplineNode* currentNode	= m_Nodes[i];
		camSplineNode* nextNode		= m_Nodes[i + 1];

		float previousTransitionDelta	= previousNode->GetTransitionDelta();
		float currentTransitionDelta = currentNode->GetTransitionDelta();

		Vector3 velocity;
		if((previousTransitionDelta == 0.0f) || (currentTransitionDelta == 0.0f))
		{
			velocity = VEC3_ZERO; //Avoid divide by zero.
		}
		else
		{
			float r0 = (currentNode->GetPosition() - previousNode->GetPosition()).Mag() / previousTransitionDelta;
			float r1 = (nextNode->GetPosition() - currentNode->GetPosition()).Mag() / currentTransitionDelta;
			float denominator = (r0 + r1) * (r0 + r1);
			if(denominator > 0.0f)
			{
				velocity = currentNode->GetTranslationVelocity() * 4.0f * r0 * r1 / denominator;
			}
			else
			{
				velocity = VEC3_ZERO; //Avoid divide by zero.
			}
		}

		currentNode->SetTranslationVelocity(velocity);
	}
}

void camTimedSplineCamera::AddNodeInternal(camSplineNode* node, u32 transitionTime)
{
	const u32 numNodes = m_Nodes.GetCount();
	if(numNodes > 0)
	{
		camSplineNode* previousNode = m_Nodes[numNodes - 1];
		previousNode->SetTransitionDelta((float)transitionTime);
		m_TotalDelta += (float)transitionTime;
	}

	m_Nodes.Grow() = node;

	m_ShouldBuildSpline = true; //Ensure that the spline is (re)built on the next update.
}