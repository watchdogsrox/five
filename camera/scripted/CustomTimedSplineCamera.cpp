//
// camera/scripted/CustomTimedSplineCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/scripted/CustomTimedSplineCamera.h"
#include "camera/system/CameraMetadata.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/ThirdPersonCamera.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camCustomTimedSplineCamera,0xD7235332)

#if __BANK
////bool g_bSmoothSpline = false;

bool camCustomTimedSplineCamera::ms_bConstantVelocity = false;
bool camCustomTimedSplineCamera::ms_bOverrideAcceleration = false;
bool camCustomTimedSplineCamera::ms_bFixupSplineNodeDuration = false;

extern s32	g_LastNodeStoppedFor;
extern bool	g_AllowPause;
#endif // __BANK

const u32 INVALID_NODE_INDEX		= 0xFFFFFFFF;

camCustomTimedSplineCamera::camCustomTimedSplineCamera(const camBaseObjectMetadata& metadata)
: camTimedSplineCamera(metadata)
, m_Metadata(static_cast<const camCustomTimedSplineCameraMetadata&>(metadata))
, m_ElapsedDistance(0.0f)
, m_TotalDistance(0.0f)
, m_SplineSpeed(0.0f)
, m_TotalPhase(0.0f)
, m_PreviousNodeIndex(INVALID_NODE_INDEX)
, m_PreviousDistCalcNode(0)
, m_PreviousSpeedCalcNode(0)
, m_TimeBalanceCalcNode(INVALID_NODE_INDEX)
, m_SplineTimesAdjusted(false)
, m_bConstantVelocity(false)
, m_bOverrideAcceleration(false)
, m_bFixupZeroNodeTangent(false)
, m_bFixupSplineNodeDuration(false)
, m_CurrentVelChangeIndex(0)
, m_CurrentBlurChangeIndex(0)
{
	for (int i = 0; i < MAX_VEL_CHANGES; i++)
	{
		m_aoVelControl[i].start = (float)(MAX_NODES-1);
		m_aoVelControl[i].speedAtStart = -1.0f;			// must be negative
	}

	for (int i = 0; i < MAX_BLUR_CHANGES; i++)
	{
		m_aoBlurControl[i].start = (float)(MAX_NODES-1);
		m_aoBlurControl[i].motionBlurStrength = -1.0f;	// must be negative
	}
	
}

void camCustomTimedSplineCamera::ValidateSplineDuration()
{
	if(m_Duration == 0)
	{
		m_Duration = (u32)m_TotalDelta; //Automatically set the duration to the sum of the transition times.
	}
}

void camCustomTimedSplineCamera::BuildTranslationSpline()
{
	camTimedSplineCamera::BuildTranslationSpline();

	if (m_bFixupZeroNodeTangent)
	{
		u32 numNodes = m_Nodes.GetCount();
		if(numNodes > 2)
		{
			Vector3 velocity;
			const u32 lastNodeIndex = numNodes - 1;
			for(u32 i=1; i<lastNodeIndex; i++)
			{
				// The following allows zero duration nodes to be handled properly.
				camSplineNode* previousNode = m_Nodes[i - 1];
				camSplineNode* currentNode  = m_Nodes[i];
				camSplineNode* nextNode     = m_Nodes[i + 1];
				if ((IsNearZero(previousNode->GetTransitionDelta(), SMALL_FLOAT) || currentNode->IsForceLinear()) &&
					!IsNearZero(currentNode->GetTransitionDelta(), SMALL_FLOAT))
				{
					velocity = nextNode->GetPosition() - currentNode->GetPosition();
					velocity.Normalize();
					velocity.Scale(1.0f/currentNode->GetTransitionDelta());
					currentNode->SetTranslationVelocity(velocity);
					continue;
				}
			}
		}
	}

#if __BANK && 0
	if (g_bSmoothSpline)
	{
		FixTangentsForSmoothAcceleration();
	}
	else
#endif // __BANK
	if ( m_bConstantVelocity &&
		 (m_PreviousNodeIndex == INVALID_NODE_INDEX ||
		  (m_PreviousDistCalcNode != m_NodeIndex &&
		  (IsNearZero(m_Nodes[m_PreviousNodeIndex]->GetTransitionDelta(), SMALL_FLOAT) ||
		   (m_NodeIndex > 0 && IsNearZero(m_Nodes[m_NodeIndex-1]->GetTransitionDelta(), SMALL_FLOAT))) &&
		  !IsNearZero(m_Nodes[m_NodeIndex]->GetTransitionDelta(), SMALL_FLOAT))) )
	{
		BANK_ONLY(float previousTotalDistance = m_TotalDistance;)
		BANK_ONLY(bool bSavedPauseAllowed = g_AllowPause;)
		BANK_ONLY(g_AllowPause = false;)
		m_TotalDistance = 0.0f;
		u32 numNodes = m_Nodes.GetCount();
		for(u32 i=0; i<numNodes; i++)
		{
			if (i < MAX_NODES)
			{
				m_NodeDistance[i] = 0.0f;
			}
		}

		// Calculate total distance of spline.
		Vector3 previousPosition = ComputePosition(0, 0.0f);
		const u32 numSplinePathIterations = 1000;
		u32 previousNodeIndex = 0;
		for(u32 i=1; i<numSplinePathIterations; i++)
		{
			const float phase = (float)i / (float)(numSplinePathIterations - 1); 

			u32 nodeIndex;
			float nodePhase;
			ComputeNodeIndexAndNodePhase(phase, nodeIndex, nodePhase);

			const Vector3 position = ComputePosition(nodeIndex, nodePhase);

			float fIncrementalDistance = (position - previousPosition).Mag();
			if (nodeIndex < MAX_NODES)
			{
				if (nodeIndex == previousNodeIndex)
				{
					m_NodeDistance[nodeIndex] += fIncrementalDistance;
				}
				else
				{
					// Divide the distance between the last node and current node.
					Vector3 endOfPreviousNode = ComputePosition(previousNodeIndex, 1.0f);
					float fDistanceForPreviousNode = (endOfPreviousNode - previousPosition).Mag();

					m_NodeDistance[previousNodeIndex] += fDistanceForPreviousNode;

					if (IsNearZero(m_Nodes[nodeIndex-1]->GetTransitionDelta(), SMALL_FLOAT))
					{
						Vector3 startOfCurrentNode = ComputePosition(nodeIndex, 0.0f);
						fIncrementalDistance = (position - startOfCurrentNode).Mag();
						m_NodeDistance[nodeIndex] += fIncrementalDistance;
					}
					else
					{
						m_NodeDistance[nodeIndex] += (fIncrementalDistance - fDistanceForPreviousNode);
					}
				}
			}

			// Calculate below after all the node distances are determined.
			////m_TotalDistance += fIncrementalDistance;
			previousPosition = position;
			previousNodeIndex = nodeIndex;
			m_PreviousDistCalcNode = m_NodeIndex;
		}

		// Sum the node distances to get the total spline distance.
		for(u32 i=0; i<numNodes; i++)
		{
			if (i < MAX_NODES)
			{
				m_TotalDistance += m_NodeDistance[i];
			}
		}

#if __BANK
		if (previousTotalDistance != m_TotalDistance)
		{
			Displayf("New total distance %f, previous %f", m_TotalDistance, previousTotalDistance);
		}

		g_AllowPause = bSavedPauseAllowed;
#endif // __BANK

		FixupSplineTimes();
	}
}

bool camCustomTimedSplineCamera::Update()
{
	bool bReturn = false;
	if (/*BANK_ONLY(!g_bSmoothSpline &&)*/ m_bConstantVelocity)
	{
		const s32 time = GetCurrentTimeMSec();
		const u32 numNodes = m_Nodes.GetCount();

	#if GTA_REPLAY
		if((!m_CreatedForReplay && m_StartTime == 0) ||
			(m_CreatedForReplay && m_NumUpdatesPerformed == 0))
	#else
		if(m_StartTime == 0)
	#endif // GTA_REPLAY
		{
			m_StartTime = time;
		}

		if(!cameraVerifyf(numNodes >= 2, "Cannot run a spline camera with less than 2 nodes"))
		{
			return false;
		}

		ValidateSplineDuration(); 

		UpdateSplines();
		m_ShouldBuildSpline |= m_SplineTimesAdjusted;
		m_SplineTimesAdjusted = false;

		m_PreviousNodeIndex = m_NodeIndex;
		u32   previousNodeIndex = m_NodeIndex;
		float previousNodePhase = m_NodePhase;
		m_TotalPhase = ComputePhaseNodeIndexAndNodePhase(m_NodeIndex, m_NodePhase);
		TestForPause(previousNodeIndex, previousNodePhase);

		Vector3 position = ComputePosition(m_NodeIndex, m_NodePhase);
		m_Frame.SetPosition(position);

		if(m_LookAtMode != LOOK_AT_NOTHING)
		{
			//Update the camera orientation using the look-at target.
			camScriptedCamera::Update();
		}
		else
		{
			Quaternion orientation = ComputeOrientation(m_NodeIndex, m_NodePhase);
			Matrix34 worldMatrix;
			worldMatrix.FromQuaternion(orientation);
			m_Frame.SetWorldMatrix(worldMatrix);
		}

		LerpLensParameters(m_NodeIndex, m_NodePhase);
		UpdateOverridenMotionBlur();

	#if __BANK
		camSplineNode* currentNode = (m_NodeIndex < numNodes) ? m_Nodes[m_NodeIndex] : NULL;
		if (currentNode)
		{
			camScriptedCamera* pCurrentCamera = (camScriptedCamera*)currentNode->GetCamera();
			m_WasScriptedCamInterpolating = pCurrentCamera && pCurrentCamera->IsInterpolating();
		}
	#endif // __BANK

		if(m_ShouldSelfDestruct && (m_TotalPhase >= 1.0f))
		{
			delete this;
		}

		return true;
	}
	else
	{
		bReturn = camBaseSplineCamera::Update();
		UpdateOverridenMotionBlur();
	}

	return bReturn;
}

float camCustomTimedSplineCamera::ComputePhaseNodeIndexAndNodePhase(u32& nodeIndex, float& nodePhase)
{
	const u32 numNodes = m_Nodes.GetCount();
	if ( m_SplineSpeed == 0.0f || 
		(m_PreviousSpeedCalcNode != nodeIndex &&
		 IsNearZero(m_Nodes[nodeIndex-1]->GetTransitionDelta(), SMALL_FLOAT) &&
		 !IsNearZero(m_Nodes[nodeIndex]->GetTransitionDelta(), SMALL_FLOAT)) )
	{
		m_SplineSpeed = (m_TotalDelta != 0.0f) ? 1000.0f * m_TotalDistance / m_TotalDelta : 0.0f;	// m_TotalDelta is in milliseconds, so convert to seconds.
	}

	float fCurrentSplineSpeed = m_SplineSpeed;
	if (m_bOverrideAcceleration && fCurrentSplineSpeed > 0.0f && m_aoVelControl[0].speedAtStart >= 0.0f)
	{
		// Find the current SplineVelChange entry.
		float fCurrentPosition = (float)nodeIndex + nodePhase;
		for (u8 entry = m_CurrentVelChangeIndex; entry < MAX_VEL_CHANGES-1; entry++)
		{
			if ( m_aoVelControl[entry].start < fCurrentPosition &&
				((entry == MAX_VEL_CHANGES-2) ||
				 (m_aoVelControl[entry+1].start > fCurrentPosition &&
				  m_aoVelControl[entry+1].speedAtStart >= 0.0f) ||
				 (m_aoVelControl[entry+1].speedAtStart < 0.0f)) )
			{
				m_CurrentVelChangeIndex = entry;
				break;
			}
		}

		fCurrentSplineSpeed = m_aoVelControl[m_CurrentVelChangeIndex].speedAtStart;

		if (m_CurrentVelChangeIndex < MAX_VEL_CHANGES-1 &&
			m_aoVelControl[m_CurrentVelChangeIndex+1].speedAtStart >= 0.0f)
		{
			// Determine total distance and duration of current velocity change.
			u32 startNode = (u32)m_aoVelControl[m_CurrentVelChangeIndex].start;
			float startPhase = m_aoVelControl[m_CurrentVelChangeIndex].start - (float)startNode;
			u32 endNode = (u32)m_aoVelControl[m_CurrentVelChangeIndex+1].start;
			float endPhase = m_aoVelControl[m_CurrentVelChangeIndex+1].start - (float)endNode;
			float changeDistance = m_NodeDistance[endNode] * endPhase + m_NodeDistance[startNode] * (1.0f - startPhase);
			float changeDuration = (m_Nodes[endNode]->GetTransitionDelta() * endPhase +
									m_Nodes[startNode]->GetTransitionDelta() * (1.0f - startPhase));
			for (int i = startNode+1; i < endNode; i++)
			{
				changeDistance += m_NodeDistance[i];
				changeDuration += m_Nodes[i]->GetTransitionDelta();
			}
			changeDuration *= 1.0f/1000.0f;															// convert to seconds

			// Remove the distance from m_ElapsedDistance before the vel change.
			float elapsedNodeDistance = m_ElapsedDistance;
			for (int index = 0; index < startNode; index++)
			{
				elapsedNodeDistance -= m_NodeDistance[index];
			}
			elapsedNodeDistance -= m_NodeDistance[startNode] * startPhase;

			float distancePhase = (changeDistance != 0 && changeDuration != 0.0f) ? elapsedNodeDistance / changeDistance : 0.0f;

			// From http://www.inmechasol.com/fullpanel/uploads/files/motionplanning-01.pdf
			if (distancePhase >= 0.0f &&
				m_CurrentVelChangeIndex < MAX_VEL_CHANGES-1)
			{
				float startAcceleration = 0.0f;
				float targetAcceleration = 0.0f;
				float blendPhaseTotalTime = changeDuration;			// in seconds
				float deltaVel = (m_aoVelControl[m_CurrentVelChangeIndex+1].speedAtStart - m_aoVelControl[m_CurrentVelChangeIndex].speedAtStart);
				float maxAcceleration = 2.0f * deltaVel / blendPhaseTotalTime;

				if ( m_CurrentVelChangeIndex > 0 &&
					(startNode == 0 || !IsNearZero(m_Nodes[startNode-1]->GetTransitionDelta(), SMALL_FLOAT)) && 
					m_aoVelControl[m_CurrentVelChangeIndex].speedAtStart >= 0.0f &&
					m_aoVelControl[m_CurrentVelChangeIndex+1].speedAtStart >= 0.0f)
				{
					// If we were accelerating/decelerating and the current change is doing the same thing, use the current change to calculate the starting acceleration.
					bool bStillAccelerating = (m_aoVelControl[m_CurrentVelChangeIndex-1].speedAtStart < m_aoVelControl[m_CurrentVelChangeIndex].speedAtStart &&
												m_aoVelControl[m_CurrentVelChangeIndex].speedAtStart < m_aoVelControl[m_CurrentVelChangeIndex+1].speedAtStart);
					bool bStillDecelerating = (m_aoVelControl[m_CurrentVelChangeIndex-1].speedAtStart > m_aoVelControl[m_CurrentVelChangeIndex].speedAtStart &&
												m_aoVelControl[m_CurrentVelChangeIndex].speedAtStart > m_aoVelControl[m_CurrentVelChangeIndex+1].speedAtStart);
					if (bStillAccelerating || bStillDecelerating)
					{
						startAcceleration = (m_aoVelControl[m_CurrentVelChangeIndex].speedAtStart + m_aoVelControl[m_CurrentVelChangeIndex+1].speedAtStart) / changeDuration;
					}
				}

				if (m_CurrentVelChangeIndex < MAX_VEL_CHANGES-2 && 
					m_aoVelControl[m_CurrentVelChangeIndex+1].speedAtStart >= 0.0f &&
					m_aoVelControl[m_CurrentVelChangeIndex+2].speedAtStart >= 0.0f)
				{
					// If we are accelerating/decelerating and the next change is doing the same thing, use the next change to calculate the target acceleration.
					bool bStillAccelerating = (m_aoVelControl[m_CurrentVelChangeIndex].speedAtStart < m_aoVelControl[m_CurrentVelChangeIndex+1].speedAtStart &&
												m_aoVelControl[m_CurrentVelChangeIndex+1].speedAtStart < m_aoVelControl[m_CurrentVelChangeIndex+2].speedAtStart);
					bool bStillDecelerating = (m_aoVelControl[m_CurrentVelChangeIndex].speedAtStart > m_aoVelControl[m_CurrentVelChangeIndex+1].speedAtStart &&
												m_aoVelControl[m_CurrentVelChangeIndex+1].speedAtStart > m_aoVelControl[m_CurrentVelChangeIndex+2].speedAtStart);
					if (bStillAccelerating || bStillDecelerating)
					{
						u32 startNode = (u32)m_aoVelControl[m_CurrentVelChangeIndex+1].start;
						float startPhase = m_aoVelControl[m_CurrentVelChangeIndex+1].start - (float)startNode;
						u32 endNode = (u32)m_aoVelControl[m_CurrentVelChangeIndex+2].start;
						float endPhase = m_aoVelControl[m_CurrentVelChangeIndex+2].start - (float)endNode;
						float nextChangeDuration = (m_Nodes[endNode]->GetTransitionDelta() * endPhase +
													m_Nodes[startNode]->GetTransitionDelta() * (1.0f - startPhase));
						for (int i = startNode+1; i <= endNode-1; i++)
						{
							nextChangeDuration += m_Nodes[i]->GetTransitionDelta();
						}
						nextChangeDuration *= 1.0f/1000.0f;											// convert to seconds

						targetAcceleration = (m_aoVelControl[m_CurrentVelChangeIndex+1].speedAtStart + m_aoVelControl[m_CurrentVelChangeIndex+2].speedAtStart) / nextChangeDuration;
					}
				}

				if (distancePhase <= 0.50f)
				{
					// Do concave part of s-curve
					float maxJerk = 2.0f * (maxAcceleration - startAcceleration) / blendPhaseTotalTime;
					float time = distancePhase*changeDuration;			// in seconds
					fCurrentSplineSpeed = m_aoVelControl[m_CurrentVelChangeIndex].speedAtStart + maxJerk*0.50f*time*time;		// start acceleration is zero.
				}
				else
				{
					// Do convex part of s-curve
					float maxJerk = 2.0f * (targetAcceleration - maxAcceleration) / blendPhaseTotalTime;
					float halfwayVel = (m_aoVelControl[m_CurrentVelChangeIndex+1].speedAtStart + m_aoVelControl[m_CurrentVelChangeIndex].speedAtStart) * 0.50f;
					float time = (distancePhase - 0.50f)*changeDuration;	// in seconds
					fCurrentSplineSpeed = halfwayVel + maxAcceleration*time + maxJerk*0.50f*time*time;							// start acceleration is maxAcceleration.
				}
			}
		}
	}

	float fTimeStep = m_Metadata.m_CanBePaused ? fwTimer::GetCamTimeStep() : fwTimer::GetNonPausableCamTimeStep();
#if __BANK
	if (ms_PauseGameForNewNode && g_AllowPause && fwTimer::IsGamePaused() &&
		GetIsClassId(camCustomTimedSplineCamera::GetStaticClassId()))
	{
		fTimeStep = 0.0f;
	}
#endif // __BANK

	m_ElapsedDistance = Min(m_ElapsedDistance + fCurrentSplineSpeed * fTimeStep, m_TotalDistance);

	// Convert distance to a phase.
	float phase = (m_TotalDistance != 0.0f) ? m_ElapsedDistance / m_TotalDistance : 0.0f;

	{
	#if __BANK
		u32 oldNodeIndex = nodeIndex;
	#endif // __BANK

		float requiredTransition = phase * m_TotalDistance;

		float transitionDelta = 0.0f;
		float totalTransitionDelta = 0.0f;
	
		if (requiredTransition < m_TotalDistance)
		{
			for (nodeIndex=0; nodeIndex<numNodes; nodeIndex++)
			{
				if (nodeIndex < MAX_NODES)
				{
					transitionDelta = m_NodeDistance[nodeIndex];
					if((totalTransitionDelta + transitionDelta) >= (requiredTransition - SMALL_FLOAT))
					{
						break;
					}

					totalTransitionDelta += transitionDelta;
				}
			}

			if(cameraVerifyf(nodeIndex < (numNodes - 1), "Unable to compute a camera spline position - overran the spline nodes"))
			{
				nodePhase = RampValueSafe(requiredTransition - totalTransitionDelta, 0.0f, transitionDelta, 0.0f, 1.0f); //Scale to 0 - 1.
			}
			else
			{
				//Fall-back to the end of the spline.
				nodeIndex = numNodes - 2;
				nodePhase = 1.0f;
			}

			if (m_Nodes[nodeIndex]->IsForceCut() && nodeIndex < (numNodes - 2))
			{
				// This node is a cut, so skip to the next one.
				nodeIndex++;
			}
		}
		else
		{
			nodeIndex = numNodes-2;
			nodePhase = 1.0f;
		}

	#if __BANK
		if (ms_PauseGameForNewNode && g_AllowPause &&
			GetIsClassId(camCustomTimedSplineCamera::GetStaticClassId()))
		{
			float fThreshold = 1.0f;
			camSplineNode* currentNode	= m_Nodes[nodeIndex];
			camSplineNode* nextNode		= (nodeIndex < numNodes-1) ? m_Nodes[nodeIndex + 1] : NULL;

			bool bIsInterpolating = false;
			if (currentNode)
			{
				float currentDelta = currentNode->GetTransitionDelta();
				fThreshold = (currentDelta != 0.0f) ? (currentDelta - 1000.0f/9.0f) / currentDelta : 1.0f;		// 1000.0f/20.0f converts 20 fps to milliseconds.

				camScriptedCamera* pCurrentCamera = (camScriptedCamera*)currentNode->GetCamera();
				bIsInterpolating = pCurrentCamera && pCurrentCamera->IsInterpolating();
			}

			if ((g_LastNodeStoppedFor < (s32)nodeIndex+1 && oldNodeIndex < nodeIndex) ||
				( nodeIndex == 0 && nodePhase == 0.0f && (GetNumUpdatesPerformed() == 0 || (!bIsInterpolating && m_WasScriptedCamInterpolating)) ))
			{
				fwTimer::StartUserPause(false);
				g_LastNodeStoppedFor = (s32)nodeIndex;
				nodePhase = 0.0f;
			}
			else if (g_LastNodeStoppedFor < nodeIndex+1 && 
					 nextNode && nextNode->GetTransitionDelta() == 0.0f &&
					 nextNode->GetDirector() == NULL &&
					 nodePhase > fThreshold && nodePhase < 1.0f)
			{
				fwTimer::StartUserPause(false);
				g_LastNodeStoppedFor = (s32)nodeIndex+1;
				if (g_LastNodeStoppedFor < (s32)numNodes-1)
				{
					// Set node index and phase to this zero duration node, so camera position will be correct while paused.
					nodeIndex = (u32)g_LastNodeStoppedFor;
					nodePhase = 0.0f;
				}
			}
		}
	#endif // __BANK
	}

	return phase;
}

Quaternion camCustomTimedSplineCamera::ComputeOrientation(u32 nodeIndex, float nodeTime)
{
	bool bIsOrientationSet = false;
	Quaternion currentOrientation;

	if (m_bOverrideAcceleration)
	{
		// To be safe, only do if overriding spline speed, so we don't affect existing splines.
		// TODO: enable for all splines?
		camSplineNode* currentNode	= m_Nodes[nodeIndex];
		camSplineNode* nextNode		= m_Nodes[nodeIndex + 1];

		if (currentNode &&
			currentNode->GetCamera() &&
			currentNode->GetCamera()->GetIsClassId(camScriptedCamera::GetStaticClassId()) &&
			nextNode &&
			( (nextNode->GetCamera() && nextNode->GetCamera()->GetIsClassId(camScriptedCamera::GetStaticClassId())) ||
			  (nextNode->GetDirector() && nextNode->GetDirector()->GetIsClassId(camGameplayDirector::GetStaticClassId())) ))
		{
			if(nextNode->ShouldSmoothOrientation())
			{
				//Apply a smoothing function to the node time in order to reduce the hard corners that can appear when SLERPing orientation between nodes.
				nodeTime = SlowInOut(nodeTime);
			}

			camScriptedCamera* pCurrentCamera = (camScriptedCamera*)currentNode->GetCamera();
			if (pCurrentCamera->GetLookAtMode() == camScriptedCamera::LOOK_AT_ENTITY)
			{
				Vector3 vLookAtOffset = pCurrentCamera->GetLookAtOffset();
				Vector3 lookAtPosition = pCurrentCamera->GetCachedLookAtPosition();

				if (nextNode->GetCamera())
				{
					camScriptedCamera* pNextCamera = (camScriptedCamera*)nextNode->GetCamera();
					if (pNextCamera->IsLookingAtSomething())
					{
						Vector3 nextLookAtPosition = pNextCamera->GetCachedLookAtPosition();

						float fStartRoll = camFrame::ComputeRollFromMatrix(pCurrentCamera->GetFrame().GetWorldMatrix());
						float fEndRoll = camFrame::ComputeRollFromMatrix(pNextCamera->GetFrame().GetWorldMatrix());
						float fCurrentRoll = Lerp(nodeTime, fStartRoll, fEndRoll);

						lookAtPosition.Lerp(nodeTime, nextLookAtPosition);

						Vector3 front = lookAtPosition - m_Frame.GetPosition();
						front.NormalizeSafe(rage::ZAXIS);
						if (Abs(front.z) < (1.0f - 0.05f))
						{
							Matrix34 matOrientation;
							camFrame::ComputeWorldMatrixFromFront(front, matOrientation);

							matOrientation.RotateUnitAxis(front, fCurrentRoll);
							matOrientation.ToQuaternion( currentOrientation );
							bIsOrientationSet = true;
						}
					}
				}
				else if (nextNode->GetDirector())
				{
					if (camInterface::GetGameplayDirector().GetThirdPersonCamera())
					{
						float fStartRoll = camFrame::ComputeRollFromMatrix(pCurrentCamera->GetFrame().GetWorldMatrix());

						Matrix34 matGameplayCamera = camInterface::GetGameplayDirector().GetFrame().GetWorldMatrix();
						float fEndRoll = camFrame::ComputeRollFromMatrix(matGameplayCamera);

						float fCurrentRoll = Lerp(nodeTime, fStartRoll, fEndRoll);

						lookAtPosition.Lerp(nodeTime, camInterface::GetGameplayDirector().GetThirdPersonCamera()->GetLookAtPosition());

						Vector3 front = lookAtPosition - m_Frame.GetPosition();
						front.NormalizeSafe(rage::ZAXIS);
						if (Abs(front.z) < (1.0f - 0.05f))
						{
							Matrix34 matOrientation;
							camFrame::ComputeWorldMatrixFromFront(front, matOrientation);

							matOrientation.RotateUnitAxis(front, fCurrentRoll);
							matOrientation.ToQuaternion( currentOrientation );
							bIsOrientationSet = true;
						}
					}
				}
			}
		}
	}

	if (!bIsOrientationSet)
	{
		currentOrientation = camBaseSplineCamera::ComputeOrientation(nodeIndex, nodeTime);
	}

#if __DEV
	// Debug only
	Vector3 vAngles;
	currentOrientation.ToEulers(vAngles);
#endif

	return currentOrientation;
}

void camCustomTimedSplineCamera::OverrideVelocity(u32 Entry, float SpeedStart, float Speed)
{
	if (Entry < MAX_VEL_CHANGES)
	{
		// Speed has to be set for first node.
		m_aoVelControl[Entry].start        = (Entry > 0) ? SpeedStart : 0.0f;
		m_aoVelControl[Entry].speedAtStart = Speed;

		m_bConstantVelocity = true;
		m_bOverrideAcceleration = true;
		m_bFixupZeroNodeTangent = true;
		m_bFixupSplineNodeDuration = true;

	#if __BANK
		ms_bConstantVelocity = m_bConstantVelocity;
		ms_bOverrideAcceleration = m_bOverrideAcceleration;
		ms_bFixupSplineNodeDuration = m_bFixupSplineNodeDuration;
	#endif // __BANK
	}
}

void camCustomTimedSplineCamera::OverrideMotionBlur(u32 Entry, float BlurStart, float MotionBlur)
{
	if (Entry < MAX_BLUR_CHANGES)
	{
		// Speed has to be set for first node.
		m_aoBlurControl[Entry].start       = BlurStart;
		m_aoBlurControl[Entry].motionBlurStrength = MotionBlur;
	}
}

void  camCustomTimedSplineCamera::UpdateOverridenMotionBlur()
{
	////if (m_bOverrideAcceleration)
	{
		float fCurrentPosition = (float)m_NodeIndex + m_NodePhase;
		float finalMotionBlur = -1.0f;

		for (u8 entry = 0; entry < MAX_BLUR_CHANGES-1; entry++)
		{
			if ( m_aoBlurControl[entry].start < fCurrentPosition &&
				((entry == MAX_BLUR_CHANGES-2) ||
				 (m_aoBlurControl[entry+1].start > fCurrentPosition &&
				  m_aoBlurControl[entry+1].motionBlurStrength >= 0.0f) ||
				 (m_aoBlurControl[entry+1].motionBlurStrength < 0.0f)) )
			{
				m_CurrentBlurChangeIndex = entry;
				break;
			}
		}

		if (m_CurrentBlurChangeIndex < MAX_BLUR_CHANGES &&
			m_aoBlurControl[m_CurrentBlurChangeIndex].motionBlurStrength >= 0.0f)
		{
			if (m_CurrentBlurChangeIndex == 0 && fCurrentPosition < m_aoBlurControl[m_CurrentBlurChangeIndex].start)
			{
				// If first node is not set at time zero, assume zero motion blur until we reach the start time.
				// If interpolation is required, first node should set at time zero.
				finalMotionBlur = 0.0f;
			}
			else
			{
				// Current blur change "node" is valid.
				float fCurrentMotionBlur = m_aoBlurControl[m_CurrentBlurChangeIndex].motionBlurStrength;
				finalMotionBlur = fCurrentMotionBlur;
				if (m_CurrentBlurChangeIndex+1 < MAX_BLUR_CHANGES &&
					m_aoBlurControl[m_CurrentBlurChangeIndex+1].motionBlurStrength >= 0.0f &&
					!IsNearZero(m_aoBlurControl[m_CurrentBlurChangeIndex+1].start - m_aoBlurControl[m_CurrentBlurChangeIndex].start, SMALL_FLOAT))
				{
					float fInterp = (fCurrentPosition - m_aoBlurControl[m_CurrentBlurChangeIndex].start) /
									(m_aoBlurControl[m_CurrentBlurChangeIndex+1].start - m_aoBlurControl[m_CurrentBlurChangeIndex].start);
					fInterp = Clamp(fInterp, 0.0f, 1.0f);

					// Next blur change "node" is valid, so interpolate.
					float fNextMotionBlur = m_aoBlurControl[m_CurrentBlurChangeIndex+1].motionBlurStrength;
					finalMotionBlur = Lerp(fInterp, fCurrentMotionBlur, fNextMotionBlur);
				}
			}
		}

		if (finalMotionBlur >= 0.0f)
		{
			m_Frame.SetMotionBlurStrength(finalMotionBlur);
		}
	}
}

void  camCustomTimedSplineCamera::SetPhase(float progress)
{
	if (m_bOverrideAcceleration)
		m_ElapsedDistance = progress*m_TotalDistance;
	else
		camBaseSplineCamera::SetPhase(progress);
}

float camCustomTimedSplineCamera::GetPhase(s32* time) const
{
	if (m_bOverrideAcceleration)
		return m_TotalPhase;
	else
		return camBaseSplineCamera::GetPhase(time);
}


void camCustomTimedSplineCamera::FixupSplineTimes()
{
	if (/*BANK_ONLY(!g_bSmoothSpline &&)*/ m_bFixupSplineNodeDuration)
	{
		// Find segment with largest time.
		u32 numNodes = m_Nodes.GetCount();
		u32 indexOfLargestDist = 0;
		float largestDistance = 0.0f;
		u32 indexOfSecondLargestDist = 0;
		float secondLargestDistance = 0.0f;
		if (m_TimeBalanceCalcNode == INVALID_NODE_INDEX)
		{
			for(u32 i=0; i<numNodes; i++)
			{
				if (i < MAX_NODES &&
					m_NodeDistance[i] > largestDistance &&
					!IsNearZero(m_Nodes[i]->GetTransitionDelta(), SMALL_FLOAT))
				{
					if (largestDistance > 0.0f)
					{
						indexOfSecondLargestDist = indexOfLargestDist;
						secondLargestDistance = largestDistance;
					}
					indexOfLargestDist = i;
					largestDistance = m_NodeDistance[i];
				}
			}
		}
		else
		{
			indexOfLargestDist = m_TimeBalanceCalcNode;
			largestDistance = m_NodeDistance[m_TimeBalanceCalcNode];
			indexOfSecondLargestDist = indexOfLargestDist;
			secondLargestDistance = largestDistance;
		}

		float fNewTotal = 0.0f;
		const float c_fVelocityThreshold = 75.0f;
		if (m_Nodes[indexOfLargestDist]->GetTransitionDelta() > SMALL_FLOAT)
		{
			float fTargetVelocity = 1000.0f * largestDistance / m_Nodes[indexOfLargestDist]->GetTransitionDelta();		// m/s
			m_TimeBalanceCalcNode = indexOfLargestDist;
			if ( m_Nodes[indexOfSecondLargestDist]->GetTransitionDelta() > SMALL_FLOAT &&
				(fTargetVelocity > c_fVelocityThreshold || m_Nodes[indexOfLargestDist]->GetTransitionDelta() > 15000.0f) )
			{
				float fSecondTargetVelocity = 1000.0f * secondLargestDistance / m_Nodes[indexOfSecondLargestDist]->GetTransitionDelta();		// m/s
				if (fSecondTargetVelocity > 0.0f && fSecondTargetVelocity < c_fVelocityThreshold)
				{
					fTargetVelocity = fSecondTargetVelocity;
					m_TimeBalanceCalcNode = indexOfSecondLargestDist;
				}
			}
			if (fTargetVelocity > 0.0f)
			{
				for(u32 i=0; i<numNodes; i++)
				{
					if (i < MAX_NODES && m_NodeDistance[i] >= SMALL_FLOAT)
					{
						float fNewTime = 1000.0f * m_NodeDistance[i] / fTargetVelocity;	// milliseconds
						m_Nodes[i]->SetTransitionDelta( fNewTime );
						fNewTotal += fNewTime;
					}
				}
			}
		}

		m_TotalDelta = fNewTotal;
		m_SplineTimesAdjusted = true;
	}
}

#if __BANK && 0
void camCustomTimedSplineCamera::FixTangentsForSmoothAcceleration()
{
	const u32 numNodes = m_Nodes.GetCount();
	const u32 c_MaxNodes = 16;
	Assert(numNodes <= c_MaxNodes);
	if (numNodes > 2 && numNodes <= c_MaxNodes)
	{
		// from http://en.wikipedia.org/wiki/Tridiagonal_matrix_algorithm

		// solves Ax = v where A is a tridiagonal matrix consisting of vectors a, b, c
		// note that contents of input vector c will be modified, making this a one-time-use function
		// result[] - initially contains the input vector v, and returns the solution x. indexed from [0, ..., N - 1]
		// numNodes - number of equations
		// a[] - subdiagonal (means it is the diagonal below the main diagonal) -- indexed from [1, ..., N - 1]
		// b[] - the main diagonal, indexed from [0, ..., N - 1]
		// c[] - superdiagonal (means it is the diagonal above the main diagonal) -- indexed from [0, ..., N - 2]

		const float a_all = 2.0f;
		const float c_all = 2.0f;
		const float b_zero = 4.0f;
		const float b_last = 4.0f;
		const float b_rest = 8.0f;
		float c[c_MaxNodes];
		Vector3 result[c_MaxNodes];

		s32 in = 0;
		c[in] = c_all / b_zero;
		
		////result[in] = 6.0f * (m_Nodes[in+1]->GetPosition() - m_Nodes[in]->GetPosition()) / b_zero;
		result[in] = m_Nodes[in]->GetTranslationVelocity() / b_zero;

		/* loop from 1 to N - 2 inclusive */
		for (in = in+1; in < (s32)numNodes-1; in++)
		{
			float m = 1.0f / (b_rest - a_all * c[in - 1]);
			c[in] = c_all * m;
			if (!IsNearZero(m_Nodes[in]->GetTransitionDelta(), SMALL_FLOAT))
			{
				result[in] = (6.0f * (m_Nodes[in+1]->GetPosition() - m_Nodes[in-1]->GetPosition()) - (a_all * result[in-1])) * m;
			}
			else
			{
				break;
			}
		}

		// Handle N - 1
		s32 stoppedAt = in;
		float m = 1.0f / (b_last - a_all * c[in - 1]);
		c[in] = c_all * m;
		result[in] = (6.0f * (m_Nodes[in]->GetPosition() - m_Nodes[in-1]->GetPosition()) - (a_all * result[in-1])) * m;
		result[in].NormalizeSafe();
		result[in].Scale( m_Nodes[in]->GetTranslationVelocity().Mag() );
		m_Nodes[in]->SetTranslationVelocity( result[in] );
		if (m_Nodes[in]->GetTangentInterp() > 0.0f)
		{
			result[in].Lerp(m_Nodes[in]->GetTangentInterp(), m_Nodes[in]->GetTranslationVelocity());
		}

		// loop from N - 2 to 0 inclusive, safely testing loop end condition
		s32 lastIndex = in - 1;
		for (in = lastIndex; in >= 0; --in)
		{
			result[in] = result[in] - c[in] * result[in + 1];
			result[in].NormalizeSafe();
			result[in].Scale( m_Nodes[in]->GetTranslationVelocity().Mag() );
			if (m_Nodes[in]->GetTangentInterp() > 0.0f)
			{
				result[in].Lerp(m_Nodes[in]->GetTangentInterp(), m_Nodes[in]->GetTranslationVelocity());
			}
			m_Nodes[in]->SetTranslationVelocity( result[in] );
		}

		if (stoppedAt < (numNodes - 1))
		{
			in = stoppedAt+1;
			if (IsNearZero(m_Nodes[in]->GetTransitionDelta(), SMALL_FLOAT))
			{
				result[in].Zero();
				m_Nodes[in]->SetTranslationVelocity( result[in] );
				in++;
			}
			c[in] = c_all / b_zero;
			////result[in] = 6.0f * (m_Nodes[in+1]->GetPosition() - m_Nodes[in]->GetPosition()) / b_zero;
			result[in] = m_Nodes[in]->GetTranslationVelocity() / b_zero;

			// loop from 1 to N - 2 inclusive
			for (in = in+1; in < (s32)numNodes-1; in++)
			{
				float m = 1.0f / (b_rest - a_all * c[in - 1]);
				c[in] = c_all * m;
				if (!IsNearZero(m_Nodes[in-1]->GetTransitionDelta(), SMALL_FLOAT))
				{
					result[in] = (6.0f * (m_Nodes[in+1]->GetPosition() - m_Nodes[in-1]->GetPosition()) - (a_all * result[in-1])) * m;
				}
				else
				{
					Assert(false);
					break;
				}
			}

			// Handle N - 1
			float m = 1.0f / (b_last - a_all * c[in - 1]);
			c[in] = c_all * m;
			result[in] = (6.0f * (m_Nodes[in]->GetPosition() - m_Nodes[in-1]->GetPosition()) - (a_all * result[in-1])) * m;
			result[in].NormalizeSafe();
			result[in].Scale( m_Nodes[in]->GetTranslationVelocity().Mag() );
			m_Nodes[in]->SetTranslationVelocity( result[in] );
			if (m_Nodes[in]->GetTangentInterp() > 0.0f)
			{
				result[in].Lerp(m_Nodes[in]->GetTangentInterp(), m_Nodes[in]->GetTranslationVelocity());
			}

			// loop from N - 2 to 0 inclusive, safely testing loop end condition
			lastIndex = in - 1;
			for (in = lastIndex; in >= stoppedAt; --in)
			{
				result[in] = result[in] - c[in] * result[in + 1];
				result[in].NormalizeSafe();
				result[in].Scale( m_Nodes[in]->GetTranslationVelocity().Mag() );
				if (m_Nodes[in]->GetTangentInterp() > 0.0f)
				{
					result[in].Lerp(m_Nodes[in]->GetTangentInterp(), m_Nodes[in]->GetTranslationVelocity());
				}
				m_Nodes[in]->SetTranslationVelocity( result[in] );
			}
		}
	}
}
#endif // __BANK

void camCustomTimedSplineCamera::ConstrainTranslationVelocities()
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
		if((previousTransitionDelta == 0.0f) && (currentTransitionDelta == 0.0f))
		{
			velocity = VEC3_ZERO; //Avoid divide by zero.
		}
		else
		{
			float r0 = (previousTransitionDelta != 0.0f) ? (currentNode->GetPosition() - previousNode->GetPosition()).Mag() / previousTransitionDelta : 0.0f;
			float r1 = (currentTransitionDelta != 0.0f) ? (nextNode->GetPosition() - currentNode->GetPosition()).Mag() / currentTransitionDelta : 0.0f;
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

void camCustomTimedSplineCamera::AddNodeInternal(camSplineNode* node, u32 transitionTime)
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


#if __BANK
void camCustomTimedSplineCamera::AddWidgets(bkBank &bank)
{
	////bank.AddToggle("Smooth acceleration", &g_bSmoothSpline);
	bank.AddToggle("Constant Velocity [READ ONLY]", &ms_bConstantVelocity);
	bank.AddToggle("Override acceleration [READ ONLY]", &ms_bOverrideAcceleration);
	bank.AddToggle("Generate spline durations [READ ONLY]", &ms_bFixupSplineNodeDuration);
}
#endif // __BANK
