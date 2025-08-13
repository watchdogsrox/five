//
// camera/marketing/MarketingAToBCamera.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#if __BANK //Marketing tools are only available in BANK builds.

#include "camera/marketing/MarketingAToBCamera.h"

#include "input/pad.h"

#include "camera/scripted/TimedSplineCamera.h"
#include "camera/system/CameraFactory.h"
#include "camera/system/CameraMetadata.h"
#include "system/pad.h"
#include "text/messages.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camMarketingAToBCamera,0xF83B144B)

static const char* g_SmoothingStyleNames[camBaseSplineCamera::NUM_SMOOTHING_STYLES] =
{
	"No smoothing",	// camSplineCamera::NO_SMOOTHING
	"Slow in",		// camSplineCamera::SLOW_IN
	"Slow out",		// camSplineCamera::SLOW_OUT
	"Slow in & out"	// camSplineCamera::SLOW_IN_OUT
};


camMarketingAToBCamera::camMarketingAToBCamera(const camBaseObjectMetadata& metadata)
: camMarketingFreeCamera(metadata)
, m_Metadata(static_cast<const camMarketingAToBCameraMetadata&>(metadata))
, m_SplineNodeWidgetGroup(NULL)
, m_SplineTransitionTimes(NULL)
, m_MaxSplineNodes(m_Metadata.m_MaxSplineNodes)
, m_OverallSmoothingStyle(camBaseSplineCamera::SLOW_IN_OUT)
, m_IsSplineActive(false)
, m_ShouldUseFreeLookAroundMode(false)
, m_ShouldSmoothRotation(true)
, m_ShouldSmoothLensParameters(true)
{
	m_SplineCamera = camFactory::CreateObject<camTimedSplineCamera>(m_Metadata.m_SplineCameraRef.GetHash());
	cameraAssertf(m_SplineCamera, "A marketing A-B camera (name: %s, hash: %u) failed to create a spline camera (name: %s, hash: %u)",
		GetName(), GetNameHash(), SAFE_CSTRING(m_Metadata.m_SplineCameraRef.GetCStr()), m_Metadata.m_SplineCameraRef.GetHash());

	m_SplineTransitionTimes = rage_new float[m_Metadata.m_MaxSplineNodes];
	if(m_SplineTransitionTimes)
	{
		for(u32 i=0; i<m_Metadata.m_MaxSplineNodes; i++)
		{
			m_SplineTransitionTimes[i] = static_cast<float>(m_Metadata.m_DefaultNodeTransitionTime) / 1000.0f;
		}
	}
}

camMarketingAToBCamera::~camMarketingAToBCamera()
{
	if(m_SplineTransitionTimes)
	{
		delete[] m_SplineTransitionTimes;
	}

	if(m_SplineCamera)
	{
		delete m_SplineCamera;
	}
}

bool camMarketingAToBCamera::Update()
{
	bool hasSucceeded = camMarketingFreeCamera::Update(); //Fall-back to the base class behavior.

	if(m_IsSplineActive && m_SplineCamera && m_SplineCamera->GetIsClassId(camTimedSplineCamera::GetStaticClassId()))
	{
		camTimedSplineCamera& splineCamera = static_cast<camTimedSplineCamera&>(*m_SplineCamera);

		Matrix34 worldMatrix = m_Frame.GetWorldMatrix();

		//Overwrite our frame with the result of the spline camera update.
		hasSucceeded = splineCamera.BaseUpdate(m_Frame);

		//NOTE: We must explicitly overwrite our local FOV setting in order to prevent a zoom glitch at the end of playback.
		m_Fov = m_Frame.GetFov();

		if(m_ShouldUseFreeLookAroundMode)
		{
			//Restore the free camera orientation that was computed prior to the spline update.
			m_Frame.SetWorldMatrix(worldMatrix, true);
		}

		//Automatically deactivate playback once the spline is finished.
		if(splineCamera.HasReachedEnd())
		{
			m_IsSplineActive = false;
		}
	}

	return hasSucceeded;
}

void camMarketingAToBCamera::UpdateMiscControl(CPad& pad)
{
	camMarketingFreeCamera::UpdateMiscControl(pad);

	if(m_SplineCamera && m_SplineCamera->GetIsClassId(camTimedSplineCamera::GetStaticClassId()))
	{
		camTimedSplineCamera& splineCamera		= static_cast<camTimedSplineCamera&>(*m_SplineCamera);
		const u32 numSplineNodes	= static_cast<u32>(splineCamera.GetNumNodes());

		if(pad.GetPressedDebugButtons() & ioPad::L2)
		{
			if(numSplineNodes < m_MaxSplineNodes)
			{
				//Add a new spline node.

				u32 transitionTimeMs;
				if(m_SplineTransitionTimes && (numSplineNodes > 0))
				{
					//NOTE: The transition time specified when adding a spline node describes the time taken to transition from the previous node,
					//so we must offset the array of times exposed to RAG.
					//NOTE: The widgets display the transition times in seconds, so we need to convert to milliseconds for the nodes.
					transitionTimeMs	= static_cast<u32>(Floorf(m_SplineTransitionTimes[numSplineNodes - 1] * 1000.0f));
				}
				else
				{
					transitionTimeMs	= m_Metadata.m_DefaultNodeTransitionTime;
				}

				int nodeFlags = 0;
				if(m_ShouldSmoothRotation)
				{
					nodeFlags |= camSplineNode::SHOULD_SMOOTH_ROTATION;
				}

				if(m_ShouldSmoothLensParameters)
				{
					nodeFlags |= camSplineNode::SHOULD_SMOOTH_LENS_PARAMS;
				}

				splineCamera.AddNode(m_Frame, transitionTimeMs, camSplineNode::eFlags(nodeFlags));

				//Reset the duration to ensure that it will be recomputed based upon the node transition times.
				splineCamera.SetDuration(0);

				//Display confirmation debug text (if the HUD is enabled.)
				char *string = TheText.Get("MKT_AB_ADD");

				CNumberWithinMessage NumberArray[1];
				NumberArray[0].Set(numSplineNodes + 1);
				CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, string, NumberArray, 1);
			}
		}
		else if(pad.GetPressedDebugButtons() & ioPad::R2)
		{
			if(numSplineNodes >= 2) //We need a minimum of two spline nodes in order to activate playback.
			{
				//Toggle playback of the spline.
				m_IsSplineActive = !m_IsSplineActive;

				//Reset/rewind the spline progress.
				splineCamera.SetPhase(0.0f);
			}
		}
	}
}

void camMarketingAToBCamera::ClearAllSplineNodes()
{
	//NOTE: We do not allow the spline nodes to be cleared when playback is active.
	if(!m_IsSplineActive && m_SplineCamera && m_SplineCamera->GetIsClassId(camTimedSplineCamera::GetStaticClassId()))
	{
		camTimedSplineCamera& splineCamera = static_cast<camTimedSplineCamera&>(*m_SplineCamera);
		splineCamera.ClearAllNodes();
	}
}

void camMarketingAToBCamera::OnOverallSmoothingChange()
{
	//Update all the spline camera smoothing style to reflect the latest values in RAG.
	if(m_SplineCamera && m_SplineCamera->GetIsClassId(camTimedSplineCamera::GetStaticClassId()))
	{
		camTimedSplineCamera& splineCamera = static_cast<camTimedSplineCamera&>(*m_SplineCamera);
		splineCamera.SetSmoothingStyle(camBaseSplineCamera::eSmoothingStyle(m_OverallSmoothingStyle));
	}
}

void camMarketingAToBCamera::OnSmoothingChange()
{
	//Update all the spline node smoothing settings to reflect the latest values in RAG.
	if(m_SplineCamera && m_SplineCamera->GetIsClassId(camTimedSplineCamera::GetStaticClassId()))
	{
		camTimedSplineCamera& splineCamera		= static_cast<camTimedSplineCamera&>(*m_SplineCamera);
		const u32 numSplineNodes	= static_cast<u32>(splineCamera.GetNumNodes());

		for(u32 i=0; i<numSplineNodes - 1; i++)
		{
			camSplineNode* splineNode = splineCamera.DebugGetNode(i);
			if(splineNode)
			{
				int nodeFlags = 0;
				if(m_ShouldSmoothRotation)
				{
					nodeFlags |= camSplineNode::SHOULD_SMOOTH_ROTATION;
				}

				if(m_ShouldSmoothLensParameters)
				{
					nodeFlags |= camSplineNode::SHOULD_SMOOTH_LENS_PARAMS;
				}

				splineNode->SetFlags(camSplineNode::eFlags(nodeFlags));
			}
		}
	}
}

void camMarketingAToBCamera::OnTransitionTimeChange()
{
	//Update all the spline node transition times to reflect the latest values in RAG.
	if(m_SplineCamera && m_SplineCamera->GetIsClassId(camTimedSplineCamera::GetStaticClassId()) && m_SplineTransitionTimes)
	{
		camTimedSplineCamera& splineCamera		= static_cast<camTimedSplineCamera&>(*m_SplineCamera);
		const u32 numSplineNodes	= static_cast<u32>(splineCamera.GetNumNodes());
		if(numSplineNodes > 0)
		{
			float totalNodeTimeMs = 0.0f;

			for(u32 i=0; i<numSplineNodes - 1; i++)
			{
				camSplineNode* splineNode = splineCamera.DebugGetNode(i);
				if(splineNode)
				{
					//NOTE: The widgets display the transition times in seconds, so we need to convert to milliseconds for the nodes.
					const float transitionTimeMs = m_SplineTransitionTimes[i] * 1000.0f;
					splineNode->SetTransitionDelta(transitionTimeMs);

					totalNodeTimeMs += transitionTimeMs;
				}
			}

			//Update the total time and duration of the spline to reflect the new node transition times.
			splineCamera.DebugSetTotalDelta(totalNodeTimeMs);
			splineCamera.SetDuration(static_cast<u32>(Floorf(totalNodeTimeMs)));

			//NOTE: We must force a rebuild of the spline, as we have altered the internal nodes.
			splineCamera.DebugForceSplineRebuild();
		}
	}
}

void camMarketingAToBCamera::AddWidgetsForInstance()
{
	camMarketingFreeCamera::AddWidgetsForInstance();

	if(m_WidgetGroup)
	{
		//Extend the widget group created by the base implementation.
		bkBank* bank = BANKMGR.FindBank("Marketing Tools");
		if(bank)
		{
			bank->SetCurrentGroup(*m_WidgetGroup);
			{
				bank->AddSeparator();
				bank->AddTitle("A-B key frames");

				bank->AddButton("Clear all key frames", datCallback(MFA(camMarketingAToBCamera::ClearAllSplineNodes), this));
				bank->AddSlider("Max number of key frames", &m_MaxSplineNodes, 2, m_Metadata.m_MaxSplineNodes, 1);
				bank->AddCombo("Overall smoothing style", &m_OverallSmoothingStyle, camBaseSplineCamera::NUM_SMOOTHING_STYLES, g_SmoothingStyleNames,
					datCallback(MFA(camMarketingAToBCamera::OnOverallSmoothingChange), this), "Select a smoothing style for overall playback");
				bank->AddToggle("Use free aim mode", &m_ShouldUseFreeLookAroundMode);

				bank->AddSeparator();
				bank->AddTitle("Key frame smoothing");

				bank->AddToggle("Smooth rotation", &m_ShouldSmoothRotation, datCallback(MFA(camMarketingAToBCamera::OnSmoothingChange), this));
				bank->AddToggle("Smooth lens parameters", &m_ShouldSmoothLensParameters, datCallback(MFA(camMarketingAToBCamera::OnSmoothingChange), this));

				bank->AddSeparator();
				bank->AddTitle("Transition times (secs)");

				const int maxTitleLength = 10;
				char title[maxTitleLength];
				for(u32 i=0; i<m_Metadata.m_MaxSplineNodes; i++)
				{
					formatf(title, maxTitleLength, "%u->%u", i + 1, i + 2);
					bank->AddSlider(title, m_SplineTransitionTimes + i, 0.0f, 600.0f, 0.1f, datCallback(MFA(camMarketingAToBCamera::OnTransitionTimeChange),
						this));
				}
			}
			bank->UnSetCurrentGroup(*m_WidgetGroup);
		}
	}
}

#endif // __BANK
