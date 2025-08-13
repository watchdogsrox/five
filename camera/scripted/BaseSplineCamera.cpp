//
// camera/scripted/SplineCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/scripted/BaseSplineCamera.h"

#include "camera/system/CameraFactory.h"
#include "camera/system/CameraManager.h"
#include "camera/system/CameraMetadata.h"

#include "camera/scripted/CustomTimedSplineCamera.h"

#include "camera/CamInterface.h"
#include "camera/scripted/ScriptDirector.h"

#include "Peds/Ped.h"

#include "fwsys/timer.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camBaseSplineCamera,0x7D4F8809)

const Matrix44 g_HermiteInterpolation //The Hermite interpolation matrix used by the splines.
(
	2.0f,	-2.0f,	1.0f,	1.0f,
	-3.0f,	3.0f,	-2.0f,	-1.0f,
	0.0f,	0.0f,	1.0f,	0.0f,
	1.0f,	0.0f,	0.0f,	0.0f
);

float	camBaseSplineCamera::ms_SpeedGraphMarkers[g_MaxSplineSpeedGraphMarkers];
u32		camBaseSplineCamera::ms_NumSpeedGraphMarkers = 0;

#if __BANK
bool	camBaseSplineCamera::ms_ShouldDebugRenderSplines = false;
bool	camBaseSplineCamera::ms_ShouldDebugRenderTangents = false;
bool	camBaseSplineCamera::ms_PauseGameForNewNode = false;
s32		g_LastNodeStoppedFor = -1;
bool	g_AllowPause    = true;
#endif // __BANK


camBaseSplineCamera::camBaseSplineCamera(const camBaseObjectMetadata& metadata)
: camScriptedCamera(metadata)
, m_Metadata(static_cast<const camBaseSplineCameraMetadata&>(metadata))
, m_SmoothingStyle(SLOW_IN_OUT)
, m_NodeIndex(0)
, m_Duration(0)
, m_StartTime(0)
, m_TotalDelta(0.0f)
, m_NodePhase(0.0f)
, m_ShouldSelfDestruct(false)
, m_ShouldBuildSpline(false)
, m_ShouldLerp(false)
, m_Paused(false)
#if GTA_REPLAY
, m_CreatedForReplay(false)
#endif // GTA_REPLAY
#if __BANK
, m_WasScriptedCamInterpolating(false)
#endif // __BANK
{
#if __BANK
	g_LastNodeStoppedFor = -1;
#endif // __BANK
}

camBaseSplineCamera::~camBaseSplineCamera()
{
	ClearAllNodes();
}

void camBaseSplineCamera::ClearAllNodes()
{
	const u32 numNodes = m_Nodes.GetCount();
	for(u32 i=0; i<numNodes; i++)
	{
		camSplineNode* node = m_Nodes[i];
		if(node)
		{
			delete node;
		}
	}

	m_Nodes.Reset();

	m_Duration	= 0;
	m_TotalDelta = 0.0f; 
}

s32 camBaseSplineCamera::GetCurrentTimeMSec() const
{
#if GTA_REPLAY
	if (m_CreatedForReplay)
	{
		return (s32)CReplayMgr::GetCurrentTimeRelativeMs();
	}
#endif // GTA_REPLAY

	const s32 time = m_Metadata.m_CanBePaused ? fwTimer::GetCamTimeInMilliseconds() : fwTimer::GetNonPausableCamTimeInMilliseconds();
	return time;
}

bool camBaseSplineCamera::Update()
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

	float totalPhase;
	//float nodePhase;

	
	u32   previousNodeIndex = m_NodeIndex;
	float previousNodePhase = m_NodePhase;
	ComputePhaseNodeIndexAndNodePhase(time, totalPhase, m_NodeIndex, m_NodePhase);
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

#if __BANK
	camSplineNode* currentNode = (m_NodeIndex < numNodes) ? m_Nodes[m_NodeIndex] : NULL;
	if (currentNode)
	{
		camScriptedCamera* pCurrentCamera = (camScriptedCamera*)currentNode->GetCamera();
		m_WasScriptedCamInterpolating = pCurrentCamera && pCurrentCamera->IsInterpolating();
	}
#endif // __BANK

	if(m_ShouldSelfDestruct && (totalPhase >= 1.0f))
	{
		delete this;
	}

	return true;
}

void camBaseSplineCamera::UpdateSplines()
{
	const u32 numNodes = m_Nodes.GetCount();

	//Update translation spline:

	bool haveNodesChanged = false;
	for(u32 i=0; i<numNodes; i++)
	{
		if(m_Nodes[i]->UpdatePosition())
		{
			haveNodesChanged = true;
		}
	}

	if(m_ShouldBuildSpline || haveNodesChanged)
	{
		UpdateTransitionDeltas();
		BuildTranslationSpline();
		AdjustTangents();
	}

	//Update orientation spline:

	haveNodesChanged = false;
	for(u32 i=0; i<numNodes; i++)
	{
		if(m_Nodes[i]->UpdateOrientation())
		{
			haveNodesChanged = true;
		}
	}

	m_ShouldBuildSpline = false;
}

void camBaseSplineCamera::UpdateTransitionDeltas()
{
	m_TotalDelta = 0.0f;

	const u32 numNodes = m_Nodes.GetCount();
	for(u32 i=1; i<numNodes; i++)
	{
		camSplineNode* previousNode	= m_Nodes[i - 1];
		camSplineNode* node			= m_Nodes[i];
		if(previousNode && node)
		{
			float transitionDelta = previousNode->GetPosition().Dist(node->GetPosition());
		#if SUPPORT_ZERO_DURATION_NODES
			// The following allows zero duration nodes to be handled properly.
			if (!IsNearZero(previousNode->GetTransitionDelta(), SMALL_FLOAT))
		#endif // SUPPORT_ZERO_DURATION_NODES
			{
				previousNode->SetTransitionDelta(transitionDelta);
			}
		#if SUPPORT_ZERO_DURATION_NODES
			else
			{
				transitionDelta = 0.0f;
			}
		#endif // SUPPORT_ZERO_DURATION_NODES
			m_TotalDelta += transitionDelta;
		}
	}
}

void camBaseSplineCamera::AdjustTangents()
{
	const u32 numNodes = m_Nodes.GetCount();
	Vector3 velocity;
	for(u32 i=0; i<numNodes; i++)
	{
		camSplineNode* currentNode  = m_Nodes[i];
		if (!IsNearZero(currentNode->GetTangentInterp(), SMALL_FLOAT))
		{
			camSplineNode* previousNode = (i > 0) ? m_Nodes[i - 1] : NULL;
			camSplineNode* nextNode     = (i < numNodes-1) ? m_Nodes[i + 1] : NULL;
			if (previousNode && currentNode->GetTangentInterp() < 0.0f)
			{
				velocity.Lerp(-currentNode->GetTangentInterp(), currentNode->GetTranslationVelocity(), previousNode->GetTranslationVelocity());
				currentNode->SetTranslationVelocity(velocity);
			}
			else if (nextNode && currentNode->GetTangentInterp() > 0.0f)
			{
				velocity.Lerp(currentNode->GetTangentInterp(), currentNode->GetTranslationVelocity(), nextNode->GetTranslationVelocity());
				currentNode->SetTranslationVelocity(velocity);
			}
		}
	}
}

Vector3 camBaseSplineCamera::ComputeStartTranslationVelocity(int nodeIndex)
{
	camSplineNode* currentNode	= m_Nodes[nodeIndex];
	camSplineNode* nextNode		= m_Nodes[nodeIndex + 1];

	float transitionDelta = currentNode->GetTransitionDelta();
	if(transitionDelta == 0.0f)
	{
		return VEC3_ZERO; //Avoid divide by zero.
	}

	Vector3 temp = 3.0f * (nextNode->GetPosition() - currentNode->GetPosition()) / transitionDelta;
	return (temp - nextNode->GetTranslationVelocity()) * 0.5f;
}

Vector3 camBaseSplineCamera::ComputeEndTranslationVelocity(int nodeIndex)
{
	camSplineNode* previousNode = m_Nodes[nodeIndex - 1];

	float transitionDelta = previousNode->GetTransitionDelta();
	if(transitionDelta == 0.0f)
	{
		return VEC3_ZERO; //Avoid divide by zero.
	}

	Vector3 temp = 3.0f * (m_Nodes[nodeIndex]->GetPosition() - previousNode->GetPosition()) / transitionDelta;
	return (temp - previousNode->GetTranslationVelocity()) * 0.5f;
}


float camBaseSplineCamera::EaseIn(float phase, float scale)
{
	//return SlowIn(phase * scale);

	// Bezier curve
	scale = Clamp(scale, 0.0f, 1.0f);
	float fOneMinus = 1.0f - phase;
	float fPoint2X  = scale;
	Vector2 vPoint3(1.0f, 1.0f);
	Vector2 vPoint2(fPoint2X, 1.0f);
	Vector2 vPoint1(vPoint2);
	Vector2 vPoint0(0.0f, 0.0f);
	Vector2 result = vPoint0;
	result.AddScaled(vPoint1, 3.0f*fOneMinus*fOneMinus*phase);
	result.AddScaled(vPoint2, 3.0f*fOneMinus*phase*phase);
	result.AddScaled(vPoint3, phase*phase*phase);
	return (result.y);
}

float camBaseSplineCamera::EaseOut(float phase, float scale)
{
	//return SlowOut(phase * scale);

	// Bezier curve
	scale = Clamp(scale, 0.0f, 1.0f);
	float fOneMinus = 1.0f - phase;
	float fPoint2X  = scale;
	Vector2 vPoint3(1.0f, 1.0f);
	Vector2 vPoint2(fPoint2X, 0.0f);
	Vector2 vPoint1(vPoint2);
	Vector2 vPoint0(0.0f, 0.0f);
	Vector2 result = vPoint0;
	result.AddScaled(vPoint1, 3.0f*fOneMinus*fOneMinus*phase);
	result.AddScaled(vPoint2, 3.0f*fOneMinus*phase*phase);
	result.AddScaled(vPoint3, phase*phase*phase);
	return (result.y);
}

float camBaseSplineCamera::EaseInOut(float phase, float scale)
{
	//return SlowInOut(phase * scale);

	// Bezier curve
	// Note: negative scale flips the curve, so instead of being slow at start/end and fast in middle
	//		 it becomes fast at start/end and slow in middle. (weird? yes, yes it is)
	scale = Clamp(scale, -1.0f, 1.0f);
	float fOneMinus = 1.0f - phase;
	Vector2 vPoint3;	// (1.0f, 1.0f) set later
	Vector2 vPoint2;
	Vector2 vPoint1;
	Vector2 vPoint0(0.0f, 0.0f);

	bool bFlip = false;
	if (scale < 0.0f)
	{
		scale *= -1.0f;
		bFlip  = true;
	}

	if (scale < 0.5f)
	{
		vPoint2.Set(0.50f, 0.50f + 1.00f*scale);
		vPoint1.Set(0.50f, 0.50f - 1.00f*scale);
	}
	else
	{
		vPoint2.Set(0.50f - 1.00f*(scale-0.50f), 1.0f);
		vPoint1.Set(0.50f + 1.00f*(scale-0.50f), 0.0f);
	}

	if (bFlip)
	{
		// Swap vPoint1 and vPoint2. (use vPoint3 as temp, will be set later)
		vPoint3 = vPoint2;
		vPoint2 = vPoint1;
		vPoint1 = vPoint3;
	}
	vPoint3.Set(1.0f, 1.0f);

	Vector2 result = vPoint0;
	result.AddScaled(vPoint1, 3.0f*fOneMinus*fOneMinus*phase);
	result.AddScaled(vPoint2, 3.0f*fOneMinus*phase*phase);
	result.AddScaled(vPoint3, phase*phase*phase);
	return (result.y);
}

void camBaseSplineCamera::ComputePhaseNodeIndexAndNodePhase(s32 time, float& phase, u32& nodeIndex, float& nodePhase) const
{
	phase = GetPhase(&time); 
	ComputeNodeIndexAndNodePhase(phase, nodeIndex, nodePhase);

	////if (m_SmoothingStyle == NO_SMOOTHING)
	{
		const camSplineNode* currentNode = m_Nodes[nodeIndex];
		if (currentNode->ShouldEaseIn())
		{
			nodePhase = EaseIn(nodePhase, m_Nodes[nodeIndex]->GetEaseScale());
		}
		else if (currentNode->ShouldEaseOut())
		{
			nodePhase = EaseOut(nodePhase, m_Nodes[nodeIndex]->GetEaseScale());
		}
		else if (currentNode->ShouldEaseInOut())
		{
			nodePhase = EaseInOut(nodePhase, m_Nodes[nodeIndex]->GetEaseScale());
		}

		nodePhase = Clamp(nodePhase, 0.0f, 1.0f);
	}
}

//Maps a 0-1 input to a 0-1 output using a custom piecewise-linear speed graph.
//TODO: Validate this works.
float camBaseSplineCamera::ComputeCustomSpeedGraphFunction(float x) const
{
	x = Clamp(x, 0.0f, 1.0f);

	if(ms_NumSpeedGraphMarkers == 0)
	{
		return x;
	}

	u32 lastMarkerIndex = (ms_NumSpeedGraphMarkers - 1);
	float fractionalMarker = x * lastMarkerIndex;
	u32 startMarker = (u32)floorf(fractionalMarker);
	u32 endMarker = Min(startMarker + 1, lastMarkerIndex);
	float remainder = fractionalMarker - (float)startMarker;

	float result = Lerp(remainder, ms_SpeedGraphMarkers[startMarker], ms_SpeedGraphMarkers[endMarker]);

	return result;
}

float camBaseSplineCamera::ApplySpeedGraph(float x) const
{
	float result;

	x = Clamp(x, 0.0f, 1.0f);

	switch(m_SmoothingStyle)
	{
	case SLOW_IN:
		result = SlowIn(x);
		break;

	case SLOW_OUT:
		result = SlowOut(x);
		break;

	case SLOW_IN_OUT:
		result = SlowInOut(x);
		break;

	case VERY_SLOW_IN:
		{
			result = SlowIn(SlowIn(x));
		}
		break;

	case VERY_SLOW_OUT:
		{
			result = SlowOut(SlowOut(x));
		}
		break;

	case VERY_SLOW_IN_SLOW_OUT:
		{
			result = SlowIn(SlowInOut(x));
		}
		break;

	case SLOW_IN_VERY_SLOW_OUT:
		{
			result = SlowOut(SlowInOut(x));
		}
		break;

	case VERY_SLOW_IN_VERY_SLOW_OUT:
		{
			result = SlowInOut(SlowInOut(x));
		}
		break;

	case EASE_IN:
		{
			result = camBaseSplineCamera::EaseOut(x);
		}
		break;

	case EASE_OUT:
		{
			result = camBaseSplineCamera::EaseIn(x);
		}
		break;

	case QUADRATIC_EASE_OUT:
		{
			result = QuadraticEaseOut(x);
		}
		break;

	case QUADRATIC_EASE_IN_OUT:
		{		
			result = QuadraticEaseInOut(x);
		}
		break;

	case CUBIC_EASE_IN:
		{
			result = CubicEaseIn(x);
		}
		break;

	case CUBIC_EASE_OUT:
		{
			result = CubicEaseOut(x);
		}
		break;

	case CUBIC_EASE_IN_OUT:
		{		
			result = CubicEaseInOut(x);
		}
		break;

	case QUARTIC_EASE_IN:
		{
			result = QuarticEaseIn(x);
		}
		break;

	case QUARTIC_EASE_OUT:
		{
			result = QuarticEaseOut(x);
		}
		break;

	case QUARTIC_EASE_IN_OUT:
		{
			result = QuarticEaseInOut(x);
		}
		break;

	case QUINTIC_EASE_IN:
		{
			result = QuinticEaseIn(x);
		}
		break;

	case QUINTIC_EASE_OUT:
		{
			result = QuinticEaseOut(x);
		}
		break;

	case QUINTIC_EASE_IN_OUT:
		{
			result = QuinticEaseInOut(x);
		}
		break;

	case CIRCULAR_EASE_IN:
		{
			result = CircularEaseIn(x);
		}
		break;

	case CIRCULAR_EASE_OUT:
		{
			result =CircularEaseOut(x);
		}
		break;

	case CIRCULAR_EASE_IN_OUT:
		{
			result = CircularEaseInOut(x);
		}
		break;

	//case NO_SMOOTHING:
	default:
		result = x;
		break;
	}

	return result;
}

void camBaseSplineCamera::ComputeNodeIndexAndNodePhase(float totalPhase, u32& nodeIndex, float& nodePhase) const
{
	const u32 numNodes = m_Nodes.GetCount();

#if __BANK
	u32 oldNodeIndex = nodeIndex;
#endif // __BANK

	float requiredTransition = totalPhase * m_TotalDelta;

	float transitionDelta = 0.0f;
	float totalTransitionDelta = 0.0f;
	
	for(nodeIndex=0; nodeIndex<numNodes; nodeIndex++)
	{
		transitionDelta = m_Nodes[nodeIndex]->GetTransitionDelta();
		if((totalTransitionDelta + transitionDelta) >= (requiredTransition - SMALL_FLOAT))
		{
			break;
		}

		totalTransitionDelta += transitionDelta;
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

#if __BANK
	if (ms_PauseGameForNewNode && g_AllowPause &&
		GetIsClassId(camTimedSplineCamera::GetStaticClassId()))
	{
		float fThreshold = 1.0f;
		camSplineNode* currentNode	= m_Nodes[nodeIndex];
		camSplineNode* nextNode		= m_Nodes[nodeIndex + 1];
		bool bIsInterpolating = false;
		if (currentNode)
		{
			float currentDelta = currentNode->GetTransitionDelta();
			fThreshold = (currentDelta - 1000.0f/9.0f) / currentDelta;		// 1000.0f/20.0f converts 20 fps to milliseconds.

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

void camBaseSplineCamera::TestForPause(u32 previousNodeIndex, float previousNodePhase)
{
	m_Paused = false;
	if (m_Nodes[m_NodeIndex]->IsForcePause())
	{
		m_NodeIndex = previousNodeIndex;
		m_NodePhase = previousNodePhase;
		m_Paused = true;
	}
}

Vector3 camBaseSplineCamera::ComputePosition(u32 nodeIndex, float nodeTime)
{
	camSplineNode* currentNode	= m_Nodes[nodeIndex];
	camSplineNode* nextNode		= m_Nodes[nodeIndex + 1];

	if(m_ShouldLerp || (currentNode && currentNode->IsForceLinear() && nodeIndex < m_Nodes.GetCount()-1))
	{
		//Perform a simple LERP, bypassing the spline.
		Vector3 position = Lerp(nodeTime, currentNode->GetPosition(), nextNode->GetPosition());
		return position;
	}

	float transitionDelta = currentNode->GetTransitionDelta();

	//Pad-out to Vector4.
	Vector4 startPosition;
	startPosition.SetVector3ClearW(currentNode->GetPosition());
	Vector4 startVelocity;
	startVelocity.SetVector3ClearW(currentNode->GetTranslationVelocity() * transitionDelta);
	Vector4 endPosition;
	endPosition.SetVector3ClearW(nextNode->GetPosition());
	Vector4 endVelocity;
	endVelocity.SetVector3ClearW(nextNode->GetTranslationVelocity() * transitionDelta);

	Vector4 position = ComputePositionOnCubic(startPosition, startVelocity, endPosition, endVelocity, nodeTime);

	return position.GetVector3();
}

Quaternion camBaseSplineCamera::ComputeOrientation(u32 nodeIndex, float nodeTime)
{
	camSplineNode* currentNode	= m_Nodes[nodeIndex];
	camSplineNode* nextNode		= m_Nodes[nodeIndex + 1];

	const bool shouldSmoothOrientation = nextNode->ShouldSmoothOrientation();
	if(shouldSmoothOrientation)
	{
		//Apply a smoothing function to the node time in order to reduce the hard corners that can appear when SLERPing orientation between nodes.
		nodeTime = SlowInOut(nodeTime);
	}

	Quaternion currentOrientation = currentNode->GetOrientation();
	currentOrientation.SlerpNear(nodeTime, nextNode->GetOrientation());

	if (currentNode && currentNode->IsForceLinear() && currentNode->GetCamera() &&
		nextNode    && nextNode->GetCamera())
	{
		camScriptedCamera* pCurrentCamera = (camScriptedCamera*)currentNode->GetCamera();
		camScriptedCamera* pNextCamera    = (camScriptedCamera*)nextNode->GetCamera();
		if (pCurrentCamera->GetLookAtMode() == camScriptedCamera::LOOK_AT_ENTITY &&
			pCurrentCamera->GetLookAtMode() == pNextCamera->GetLookAtMode() &&
			pCurrentCamera->IsLookAtOffsetRelativeToMatrix() == pNextCamera->IsLookAtOffsetRelativeToMatrix() &&
			pCurrentCamera->GetLookAtBoneTag() == pNextCamera->GetLookAtBoneTag() && 
			pCurrentCamera->GetLookAtTarget() == pNextCamera->GetLookAtTarget())
		{
			Vector3 vLookAtOffset = pCurrentCamera->GetLookAtOffset();
			vLookAtOffset.Lerp(nodeTime, pNextCamera->GetLookAtOffset());

			Vector3 lookAtPosition;
			if((pCurrentCamera->GetLookAtBoneTag() >= 0) && pCurrentCamera->GetLookAtTarget()->GetIsTypePed())
			{
				const CPed* lookAtPed = static_cast<const CPed*>(pCurrentCamera->GetLookAtTarget());
				lookAtPed->GetBonePosition(lookAtPosition, (eAnimBoneTag)pCurrentCamera->GetLookAtBoneTag());
			}
			else
			{
				lookAtPosition = VEC3V_TO_VECTOR3(pCurrentCamera->GetLookAtTarget()->GetTransform().GetPosition());
			}

			if(pCurrentCamera->IsLookAtOffsetRelativeToMatrix())
			{
				if(vLookAtOffset.IsNonZero())
				{
					lookAtPosition += VEC3V_TO_VECTOR3(pCurrentCamera->GetLookAtTarget()->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vLookAtOffset)));
				}
			}
			else
			{
				lookAtPosition += vLookAtOffset;
			}

			Vector3 front = lookAtPosition - m_Frame.GetPosition();
			if (currentNode->IsForceLevel())
			{
				front.z = 0.0f;
			}
			if(front.IsNonZero())
			{
				Matrix34 worldMatrix;
				camFrame::ComputeWorldMatrixFromFront(front, worldMatrix);
				worldMatrix.ToQuaternion(currentOrientation);
			}
		}
		else
		if (pCurrentCamera->GetLookAtMode() == camScriptedCamera::LOOK_AT_POSITION &&
			pCurrentCamera->GetLookAtMode() == pNextCamera->GetLookAtMode())
		{
			// Interpolate the look at positions, if they are the same,
			// then the spline will look at the same point while the camera moves.
			Vector3 lookAtPosition = pCurrentCamera->GetLookAtPosition() + pCurrentCamera->GetLookAtOffset();
			lookAtPosition.Lerp(nodeTime, lookAtPosition, pNextCamera->GetLookAtPosition() + pNextCamera->GetLookAtOffset());

			Vector3 front = lookAtPosition - m_Frame.GetPosition();
			if (currentNode->IsForceLevel())
			{
				front.z = 0.0f;
			}
			if(front.IsNonZero())
			{
				Matrix34 worldMatrix;
				camFrame::ComputeWorldMatrixFromFront(front, worldMatrix);
				worldMatrix.ToQuaternion(currentOrientation);
			}
		}
	}

	return currentOrientation;
}

Vector4 camBaseSplineCamera::ComputePositionOnCubic(const Vector4 &startPos, const Vector4 &startVel, const Vector4 &endPos, const Vector4 &endVel, float time)
{
	Matrix44 matrix;
	matrix.a = startPos; 
	matrix.b = endPos;
	matrix.c = startVel; 
	matrix.d = endVel;

	Matrix44 matrix2;
	matrix2.Dot(matrix, g_HermiteInterpolation);

	Vector4 t(time * time * time, time * time, time, 1.0f);
	Vector4 position;
	matrix2.Transform(t, position);

	return position;
}

//Performs a linear interpolation of the FOV, near/far clip, near/far DOF and motion blur between the camera frames at consecutive nodes.
void camBaseSplineCamera::LerpLensParameters(u32 nodeIndex, float nodeTime)
{
	//NOTE: We can only Lerp if the both nodes have a valid camera frame.
	const camSplineNode* sourceNode			= m_Nodes[nodeIndex];
	const camSplineNode* destinationNode	= m_Nodes[nodeIndex + 1];
	if(sourceNode && sourceNode->IsFrameValid() && destinationNode && destinationNode->IsFrameValid())
	{
		const bool shouldSmoothLensParams = destinationNode->ShouldSmoothLensParams();
		if(shouldSmoothLensParams)
		{
			//Apply a smoothing function to the node time in order to reduce the appearance of discontinuities around the nodes.
			nodeTime = SlowInOut(nodeTime);
		}

		const camFrame& sourceFrame			= sourceNode->GetFrame();
		const camFrame& destinationFrame	= destinationNode->GetFrame();

		float a;
		float b;
		float result;

		a = sourceFrame.GetFov();
		b = destinationFrame.GetFov();
		result = Lerp(nodeTime, a, b);
		m_Frame.SetFov(result);

		a = sourceFrame.GetNearClip();
		b = destinationFrame.GetNearClip();
		result = Lerp(nodeTime, a, b);
		m_Frame.SetNearClip(result);

		a = sourceFrame.GetFarClip();
		b = destinationFrame.GetFarClip();
		result = Lerp(nodeTime, a, b);
		m_Frame.SetFarClip(result);

		m_Frame.InterpolateDofSettings(nodeTime, sourceFrame, destinationFrame);

		a = sourceFrame.GetMotionBlurStrength();
		b = destinationFrame.GetMotionBlurStrength();
		result = Lerp(nodeTime, a, b);
		m_Frame.SetMotionBlurStrength(result);
	}
}

bool camBaseSplineCamera::HasReachedEnd() const
{
	bool hasReachedEnd = false;
	if(m_StartTime > 0) //NOTE: Returns false by default prior to the first update.
	{
		s32 time = GetCurrentTimeMSec();
		u32 timeSinceStart = time - (u32)m_StartTime;
		hasReachedEnd = (timeSinceStart >= m_Duration);
	}

	return hasReachedEnd;
}

//Sets the spline to have progressed to the requested ratio, between 0 and 1.
void camBaseSplineCamera::SetPhase(float phase)
{
#if __BANK
	// To allow spline to be recreated and its phase set properly. (debug feature)
	ValidateSplineDuration();
#endif // __BANK

	phase = Clamp(phase, 0.0f, 1.0f);

	s32 phaseTime = (s32)floorf(phase * (float)m_Duration);	
	s32 time = GetCurrentTimeMSec();
	m_StartTime = time - phaseTime;

	//This interface could be used to implement a cut, so we must report one.
	m_Frame.GetFlags().SetFlag(camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation);

#if __BANK
	if (ms_PauseGameForNewNode && fwTimer::IsUserPaused())
	{
		// Update when paused so debug rendering is correct.
		// Have to update script director so all scripted cameras are updated as they may be spline nodes.
		////Update();
		camFrame dummyFrame;
		fwTimer::SetSingleStepThisFrame();
		camInterface::GetScriptDirector().BaseUpdate(dummyFrame);
	}
#endif // __BANK
}

float camBaseSplineCamera::GetPhase(s32* time) const
{
	s32 NewTime = 0; 
	
	if(time ==NULL )	
	{
		 NewTime = GetCurrentTimeMSec();
	}
	else
	{	
		NewTime = *time; 	
	}

	if(m_Duration > 0)	
	{
		u32 timeSinceStart = NewTime - (u32)m_StartTime;
		timeSinceStart = Min(timeSinceStart, m_Duration);
		float phase = (float)timeSinceStart / (float)m_Duration;

		phase = ComputeCustomSpeedGraphFunction(phase);
		phase = ApplySpeedGraph(phase);

		return phase; 
	}
	return 0.0f;
}

void camBaseSplineCamera::AddNode(camBaseCamera* camera, u32 transitionTime, camSplineNode::eFlags flags)
{
	if(cameraVerifyf(camera, "Cannot add a NULL camera as a spline node"))
	{
		camSplineNode* node = camera_verified_pool_new(camSplineNode) (camera, flags);
		if(node)
		{
			AddNodeInternal(node, transitionTime); 
		}
	}
}

void camBaseSplineCamera::AddNode(const camBaseDirector& director, u32 transitionTime, camSplineNode::eFlags flags)
{
	camSplineNode* node = camera_verified_pool_new(camSplineNode) (director, flags);
	if(node)
	{
		AddNodeInternal(node, transitionTime); 
	}
}

void camBaseSplineCamera::AddNode(const camFrame& frame, u32 transitionTime, camSplineNode::eFlags flags)
{
	camSplineNode* node = camera_verified_pool_new(camSplineNode) (frame, flags);
	if(node)
	{
		AddNodeInternal(node, transitionTime); 
	}
}

void camBaseSplineCamera::AddNode(const Vector3& position, const Vector3& lookAtPosition, u32 transitionTime, camSplineNode::eFlags flags)
{
	camSplineNode* node = camera_verified_pool_new(camSplineNode) (position, flags);
	if(node)
	{
		node->SetLookAtPosition(lookAtPosition);

		AddNodeInternal(node, transitionTime); 
	}
}

void camBaseSplineCamera::AddNodeWithOrientation(const Vector3& position, const Vector3& orientationEulers, u32 transitionTime, camSplineNode::eFlags flags,
												 int rotationOrder)
{
	camSplineNode* node = camera_verified_pool_new(camSplineNode) (position, flags);
	if(node)
	{
		node->SetOrientationEulers(orientationEulers, rotationOrder);
	
		AddNodeInternal(node, transitionTime); 
	}
}

const float c_fFwdDownAngleTolerance = -0.9999f;
const float c_fUpDownAngleTolerance  =  0.0001f;
void camBaseSplineCamera::SetOrientationBasedOnStartEndNodes(u32 nodeIndex, float percent)
{
	const u32 numNodes = m_Nodes.GetCount();
	Matrix34 matOrient;
	Quaternion qNewOrient;
	Vector3 vStartDesiredDirection;
	Vector3 vEndDesiredDirection;

	if (numNodes < 3)
	{
		return;
	}

	if (nodeIndex == 0 || nodeIndex >= numNodes-1)
	{
		return;
	}

	// Validate that the last node is not pointing straight down.
	matOrient.FromQuaternion(m_Nodes[numNodes-1]->GetOrientation());
	if (matOrient.b.z <= c_fFwdDownAngleTolerance && Abs(matOrient.c.z) <= c_fUpDownAngleTolerance)
	{
		// Last node is pointing downward, so align previous downward nodes with its up vector.
		vEndDesiredDirection = matOrient.c;
		vEndDesiredDirection.z = 0.0f;
		vEndDesiredDirection.Normalize();
	}
	else
	{
		// Last node is not pointing downward, so align previous downward nodes with its forward vector.
		vEndDesiredDirection = matOrient.b;
		vEndDesiredDirection.z = 0.0f;
		vEndDesiredDirection.Normalize();
	}

	matOrient.FromQuaternion(m_Nodes[0]->GetOrientation());
	if (matOrient.b.z <= c_fFwdDownAngleTolerance && Abs(matOrient.c.z) <= c_fUpDownAngleTolerance)
	{
		// Last node is pointing downward, so align previous downward nodes with its up vector.
		vStartDesiredDirection = matOrient.c;
		vStartDesiredDirection.z = 0.0f;
		vStartDesiredDirection.Normalize();
	}
	else
	{
		// Last node is not pointing downward, so align previous downward nodes with its forward vector.
		vStartDesiredDirection = matOrient.b;
		vStartDesiredDirection.z = 0.0f;
		vStartDesiredDirection.Normalize();
	}

	Vector3 vDesiredDirection = vEndDesiredDirection;
	if (percent == 0.0f)
	{
		vDesiredDirection = vStartDesiredDirection;
	}
	else if (percent < 1.0f)
	{
		Vector3 vRotationAxis;
		vRotationAxis.Cross(vStartDesiredDirection, vEndDesiredDirection);

		float fDot = vStartDesiredDirection.Dot(vEndDesiredDirection);
		float fAngleToRotate = Acosf(fDot);
		fAngleToRotate *= percent;

		// Rotate start vector about axis by the angle to get new forward.
		{
			Quaternion qDir(vStartDesiredDirection.x, vStartDesiredDirection.y, vStartDesiredDirection.z, 0.0f);
			Quaternion qRotation;
			qRotation.FromRotation(vRotationAxis, fAngleToRotate);

			Quaternion qResult = qRotation;
			qResult.Multiply(qDir);
			qResult.MultiplyInverse(qRotation);

			vDesiredDirection.Set(qResult.x, qResult.y, qResult.z);
			vDesiredDirection.Normalize();
		}
	}

	// Adjust the specified node's orientation.
	matOrient.FromQuaternion(m_Nodes[nodeIndex]->GetOrientation());
	if (matOrient.b.z <= c_fFwdDownAngleTolerance && Abs(matOrient.c.z) <= c_fUpDownAngleTolerance)
	{
		// Make sure up vector lines up with last valid node (forward) orientation.
		// Eliminates twist when switching from looking down to final camera.
		matOrient.c = vDesiredDirection;

		// Recalculate right vector to match.
		matOrient.a.Cross(matOrient.b, matOrient.c);
		matOrient.Normalize(); //Ensure this is an orthonormal matrix.

		matOrient.ToQuaternion(qNewOrient);
	}
	else
	{
		Vector3 vCurrentHeading = matOrient.b;
		vCurrentHeading.z = 0.0f;
		vCurrentHeading.Normalize();

		Vector3 vUpAxis;
		vUpAxis.Cross(vCurrentHeading, vDesiredDirection);

		float fDot = vCurrentHeading.Dot(vDesiredDirection);
		float fAngleToRotate = Acosf(fDot);
		if (vUpAxis.z < 0.0f)
		{
			fAngleToRotate *= -1.0f;
		}

		// Rotate start vector about axis by the angle to get new forward.
		const Vector3 vUp(0.0f, 0.0f, 1.0f);
		qNewOrient = m_Nodes[nodeIndex]->GetOrientation();
		Quaternion qRotation;
		qRotation.FromRotation(vUp, fAngleToRotate);
		qNewOrient.Multiply(qRotation);
	}

	// Save result.
	{
		m_Nodes[nodeIndex]->SetOrientation(qNewOrient);
		matOrient.FromQuaternion(qNewOrient);

		// Because the node orientation is updated based on the values it was set with,
		// have to alter those as well or the old orientation will be used instead.
		// NOTE: assuming that no node is set using script director's camera frame
		//       because I don't want to alter that one.
		if(m_Nodes[nodeIndex]->IsFrameValid())
		{
			// Orientation set using camera frame, so update its world matrix.
			m_Nodes[nodeIndex]->SetFrameMatrix(matOrient, true);
		}
		else //Use the specified orientation Eulers.
		{
			// Orientation set using Euler angle, so recalculate them for the new orientation.
			Vector3 eulers;
			matOrient.ToEulersYXZ(eulers);
			m_Nodes[nodeIndex]->SetOrientationEulers(eulers, EULER_YXZ);
		}
	}
}


void camBaseSplineCamera::AddCustomSpeedGraphMarker(float marker)
{
	if(cameraVerifyf(ms_NumSpeedGraphMarkers <= g_MaxSplineSpeedGraphMarkers, "Out of custom speed graph markers for a spline camera"))
	{
		marker = Clamp(marker, 0.0f, 1.0f);
		ms_SpeedGraphMarkers[ms_NumSpeedGraphMarkers++] = marker;
	}
}

void camBaseSplineCamera::AddNodeInternal(camSplineNode* node, u32 SUPPORT_ZERO_DURATION_NODES_ONLY(transitionTime))
{
	const u32 numNodes = m_Nodes.GetCount();
	if(numNodes > 0)
	{
		camSplineNode* previousNode = m_Nodes[numNodes - 1];
		float NodeLength = (previousNode->GetPosition() - node->GetPosition()).Mag(); 
	#if SUPPORT_ZERO_DURATION_NODES
		// The following allows zero duration nodes to be handled properly.
		if (!IsNearZero(transitionTime, SMALL_FLOAT))
	#endif // SUPPORT_ZERO_DURATION_NODES
		{
			previousNode->SetTransitionDelta(NodeLength);
		}
	#if SUPPORT_ZERO_DURATION_NODES
		else
		{
			previousNode->SetTransitionDelta(0.0f);
			NodeLength = 0.0f;
		}
	#endif // SUPPORT_ZERO_DURATION_NODES
		m_TotalDelta += NodeLength;
	}

	m_Nodes.Grow() = node;

	m_ShouldBuildSpline = true; //Ensure that the spline is (re)built on the next update.
}

void camBaseSplineCamera::ForceLinearBlend(u32 nodeIndex, bool enable)
{
	if (nodeIndex < m_Nodes.GetCount())
	{
		m_Nodes[nodeIndex]->ForceLinearBlend(enable);
	}
}

void camBaseSplineCamera::ForceCut(u32 nodeIndex, bool enable)
{
	if (nodeIndex < m_Nodes.GetCount())
	{
		m_Nodes[nodeIndex]->ForceCut(enable);
	}
}

void camBaseSplineCamera::ForcePause(u32 nodeIndex, bool enable)
{
	if (nodeIndex < m_Nodes.GetCount())
	{
		m_Nodes[nodeIndex]->ForcePause(enable);
	}
}

void camBaseSplineCamera::ForceLevel(u32 nodeIndex, bool enable)
{
	if (nodeIndex < m_Nodes.GetCount())
	{
		m_Nodes[nodeIndex]->ForceLevel(enable);
	}
}

void camBaseSplineCamera::SetEase(u32 nodeIndex, u32 easeFlag, float easeScale)
{
	if (nodeIndex < m_Nodes.GetCount())
	{
		m_Nodes[nodeIndex]->SetEaseFlag((camSplineNode::eFlags)easeFlag);
		m_Nodes[nodeIndex]->SetEaseScale(easeScale);
	}
}

void camBaseSplineCamera::SetVelocityScale(u32 nodeIndex, float scale)
{
	if (nodeIndex < m_Nodes.GetCount())
	{
		Assert(scale >= -1.0f);
		Assert(scale <=  1.0f);
		if (scale >= -1.0f && scale <= 1.0f)
		{
			m_Nodes[nodeIndex]->SetTangentInterp(scale);
		}
		else
		{
			m_Nodes[nodeIndex]->SetTangentInterp(0.0f);
		}
	}
}

u32 camBaseSplineCamera::GetCurrentNode() const
{
#if __BANK
	if (g_LastNodeStoppedFor >= 0 && g_LastNodeStoppedFor < m_Nodes.GetCount() && (u32)g_LastNodeStoppedFor > m_NodeIndex)
	{
		return g_LastNodeStoppedFor;
	}
#endif // __BANK

	return (m_NodeIndex);
}


#if __BANK
void camBaseSplineCamera::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Spline cam", false);
	{
		bank.AddToggle("RenderSpline", &ms_ShouldDebugRenderSplines, datCallback(CFA(OnRenderSplineChange)));
		bank.AddToggle("RenderTangents", &ms_ShouldDebugRenderTangents);
		bank.AddToggle("Enable pause on new node", &ms_PauseGameForNewNode, datCallback(CFA(OnSplineTogglePause)));

		bank.AddSeparator();
		bank.AddButton("Advance to next node", datCallback(CFA(OnSplineAdvance)), "Only valid if pausing is enabled");

		camCustomTimedSplineCamera::AddWidgets(bank);
	}
	bank.PopGroup();
}

void camBaseSplineCamera::OnRenderSplineChange()
{
	//Also, toggle the debug rendering of cameras, as this is required for the spline to render.
	camManager::DebugSetShouldRenderCameras(ms_ShouldDebugRenderSplines);
}

void camBaseSplineCamera::OnSplineTogglePause()
{
	if (!ms_PauseGameForNewNode)
	{
		fwTimer::EndUserPause();
	}
}

void camBaseSplineCamera::OnSplineAdvance()
{
	if (ms_PauseGameForNewNode)
	{
		fwTimer::EndUserPause();
	}
}

//Render the camera so we can see it.
void camBaseSplineCamera::DebugRender()
{
	g_AllowPause = false;
	camScriptedCamera::DebugRender();

	//NOTE: These elements are drawn even if this is the active/rendering camera. This is necessary in order to debug the camera when active.

	const u32 numNodes = m_Nodes.GetCount();
	if(ms_ShouldDebugRenderSplines && (numNodes >= 2))
	{
		//Render the node positions.
		for(u32 i=0; i<numNodes; i++)
		{
			const camSplineNode* node = m_Nodes[i];
			if(node)
			{
				const Vector3& nodePosition = node->GetPosition();
				if (!ms_ShouldDebugRenderTangents)
				{
					grcDebugDraw::Sphere(nodePosition, 0.12f, Color_HotPink, true);
				}

				Matrix34 m_OrientationMatrix;
				m_OrientationMatrix.FromQuaternion(node->GetOrientation());
				grcDebugDraw::Axis(m_OrientationMatrix, 1.80f);

				if (node->IsForceLinear() && i > 0)
				{
					const Vector3 position1 = ComputePosition(i-1, 0.0f);
					const Vector3 position2 = (i < numNodes-1) ? ComputePosition(i,   0.0f) : m_Nodes[i]->GetPosition();
					grcDebugDraw::Line(position1, position2, Color_green);
				}

				if (ms_ShouldDebugRenderTangents)
				{
					Vector3 nodeVelocity = node->GetTranslationVelocity();
					if (nodeVelocity.Mag2() > VERY_SMALL_FLOAT)
					{
						nodeVelocity.NormalizeSafe();
						grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(nodePosition), VECTOR3_TO_VEC3V(nodePosition + (nodeVelocity * 0.75f)), 0.07f, Color_yellow2);
					}
				}
			}
		}

		//Render the spline path.
		Vector3 previousPosition = ComputePosition(0, 0.0f);
		const u32 numSplinePathIterations = 100;
		bool bSaveForceLinear = false;
		for(u32 i=1; i<numSplinePathIterations; i++)
		{
			const float phase = (float)i / (float)(numSplinePathIterations - 1); 

			u32 nodeIndex;
			float nodePhase;
			ComputeNodeIndexAndNodePhase(phase, nodeIndex, nodePhase);

			camSplineNode* currentNode = m_Nodes[nodeIndex];
			currentNode->SaveAndClearForceLinear(bSaveForceLinear);
			const Vector3 position = ComputePosition(nodeIndex, nodePhase);
			currentNode->RestoreForceLinear(bSaveForceLinear);

			grcDebugDraw::Sphere(position, 0.05f, Color_red , true);
			grcDebugDraw::Line(previousPosition, position, Color_blue);

			previousPosition = position;
		}
	}
	g_AllowPause = true;
}

void camBaseSplineCamera::DebugAppendDetailedText(char* string, u32 maxLength) const
{
	camScriptedCamera::DebugAppendDetailedText(string, maxLength);

	//Append the number of spline nodes, as this can aid debugging.
	const s32 numSplineNodes = GetNumNodes();
	safecatf(string, maxLength, ", NumSplineNodes=%d", numSplineNodes);
}
#endif // __BANK
