//
// camera/helpers/AnimatedFrameShaker.cpp
// 
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/AnimatedFrameShaker.h"

#include "bank/bank.h"
#include "fwmaths/angle.h"
#include "fwsys/timer.h"
#include "grcore/debugdraw.h"
#include "profile/element.h"
#include "profile/group.h"
#include "profile/page.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/Envelope.h"
#include "camera/helpers/Oscillator.h"
#include "camera/system/CameraMetadata.h"
#include "camera/system/CameraFactory.h"

#include "animation/debug/AnimDebug.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camAnimatedFrameShaker,0x9E2778BD)

#if __BANK
CDebugClipSelector camAnimatedFrameShaker::ms_cameraAnimSelector(true, false, false);
bkBank*		camAnimatedFrameShaker::ms_WidgetBank = NULL;
bkGroup*	camAnimatedFrameShaker::ms_WidgetGroup = NULL;
char		camAnimatedFrameShaker::ms_DebugAnimDictionary[100];
float		camAnimatedFrameShaker::ms_fAmplitude = 1.0f;
#endif // __BANK


camAnimatedFrameShaker::camAnimatedFrameShaker(const camBaseObjectMetadata& metadata)
: camBaseFrameShaker(metadata)
, m_AnimDictIndex(-1)
, m_AnimPhase(0.0f)
, m_bStartAnimNextFrame(false)
{
}

camAnimatedFrameShaker::~camAnimatedFrameShaker()
{
	if (m_AnimDictIndex != -1)
	{
		fwAnimManager::RemoveRefWithoutDelete(strLocalIndex(m_AnimDictIndex));
	}
}

bool camAnimatedFrameShaker::SetAnimation(const char* pszAnimDictionary, const char* pszAnimClipName)
{
	m_AnimDictionary.SetFromString(pszAnimDictionary);
	m_AnimClip.SetFromString(pszAnimClipName);

	if (m_AnimDictIndex != -1)
	{
		//Ensure that we clean up our previous animation (and referencing) before replacing it.
		m_Animation.Remove();

		fwAnimManager::RemoveRefWithoutDelete(strLocalIndex(m_AnimDictIndex));
		m_AnimDictIndex = -1;
	}

	strLocalIndex animDictIndex = fwAnimManager::FindSlot(m_AnimDictionary);
	if (cameraVerifyf(animDictIndex.IsValid(), "Failed to find a slot for anim dictionary %s", SAFE_CSTRING(m_AnimDictionary.TryGetCStr())))
	{
		if (cameraVerifyf(fwAnimManager::Get(animDictIndex) != NULL, "Anim dictionary %s is not loaded", SAFE_CSTRING(m_AnimDictionary.TryGetCStr())))
		{
			u32 clipHash = m_AnimClip.GetHash();
			const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(animDictIndex.Get(), clipHash);

			if (cameraVerifyf(pClip != NULL, "Failed to find a clip, %s, for anim dictionary %s", SAFE_CSTRING(m_AnimClip.TryGetCStr()), SAFE_CSTRING(m_AnimDictionary.TryGetCStr())))
			{
				m_AnimDictIndex = animDictIndex.Get();
				fwAnimManager::AddRef(strLocalIndex(m_AnimDictIndex));

				u32 currentTimeInMs	= fwTimer::GetTimeInMilliseconds();
				float fCurrentTime	= (float)currentTimeInMs * 0.001f;
				m_Animation.Init(pClip, fCurrentTime);
			}
		}
	}

	return (m_AnimDictIndex != -1);
}

bool camAnimatedFrameShaker::UpdateFrame(float overallEnvelopeLevel, camFrame& frameToShake)
{
	const crClip* animationClip = (m_AnimDictIndex != -1) ? m_Animation.GetClip() : NULL;
	if (animationClip)
	{
		UpdateAnimationPhase();

		const float finalScale = m_Amplitude * overallEnvelopeLevel;
		Matrix34 matWorld = frameToShake.GetWorldMatrix();

		// Extract rotation and translation from animation and apply to camera frame.
		Quaternion qRotation(Quaternion::sm_I);
		Vector3 vOffset(VEC3_ZERO);

		m_Animation.GetQuaternionValue(animationClip, kTrackCameraRotation, m_AnimPhase, qRotation);
		m_Animation.GetVector3Value(animationClip, kTrackCameraTranslation, m_AnimPhase, vOffset);

		qRotation.ScaleAngle(finalScale);

		Matrix34 matAnim(M34_IDENTITY);
		matAnim.FromQuaternion(qRotation);

		matWorld.Dot3x3(matAnim);
		matWorld.d.AddScaled(vOffset, finalScale);

		frameToShake.SetWorldMatrix(matWorld, false);
	}

	return (animationClip != NULL);
}

bool camAnimatedFrameShaker::ShakeFrame(float overallEnvelopeLevel, camFrame& frameToShake) const
{
	const crClip* animationClip = (m_AnimDictIndex != -1) ? m_Animation.GetClip() : NULL;
	if (animationClip)
	{
		const float finalScale = m_Amplitude * overallEnvelopeLevel;
		Matrix34 matWorld = frameToShake.GetWorldMatrix();

		// Extract rotation and translation from animation and apply to camera frame.
		Quaternion qRotation(Quaternion::sm_I);
		Vector3 vOffset(VEC3_ZERO);

		m_Animation.GetQuaternionValue(animationClip, kTrackCameraRotation, m_AnimPhase, qRotation);
		m_Animation.GetVector3Value(animationClip, kTrackCameraTranslation, m_AnimPhase, vOffset);

		qRotation.ScaleAngle(finalScale);

		Matrix34 matAnim(M34_IDENTITY);
		matAnim.FromQuaternion(qRotation);

		matWorld.Dot3x3(matAnim);
		matWorld.d.AddScaled(vOffset, finalScale);

		frameToShake.SetWorldMatrix(matWorld, false);
	}

	return (animationClip != NULL);
}

void camAnimatedFrameShaker::UpdateAnimationPhase()
{
	u32 currentTimeInMs	= fwTimer::GetTimeInMilliseconds();
	float fCurrentTime	= (float)currentTimeInMs * 0.001f;

	m_AnimPhase = m_Animation.GetPhase(fCurrentTime); 

	////if (IsFlagSet(APF_ISLOOPED))
	{
		//There is a frame delay here so that the anim hits phase 1.0 so the script can check it gets to the final phase
		//Can rework to allow for the time step at the loop transition. 
		if (m_bStartAnimNextFrame)
		{
			m_Animation.SetStartTime(fCurrentTime); 
			m_AnimPhase = 0.0f; 
			m_bStartAnimNextFrame = false; 
		}

		if (m_AnimPhase >= 1.0f )
		{
			m_bStartAnimNextFrame = true; 
		}
	}
}

#if __BANK
#include "streaming/streaming.h"			// For CStreaming::LoadAllRequestedObjects(), etc.
#include "system/ipc.h"

void camAnimatedFrameShaker::AddWidgets(bkBank& bank)
{
	ms_DebugAnimDictionary[0] = '\0';

	ms_WidgetBank = &bank;

	ms_WidgetGroup = bank.PushGroup("Animated frame shaker", false);
	{
		bank.AddSlider("Shake Amplitude", &ms_fAmplitude, 0.10f, 10.0f, 0.50f);
		bank.AddButton("Shake gameplay director", datCallback(CFA(ShakeGameplayDirector)));
		bank.AddButton("Stop shaking gameplay director", datCallback(CFA(StopShakingGameplayDirector)));
		bank.AddButton("Create animation selector widget", datCallback(CFA(AddAnimationSelectorWidget)));
	}
	bank.PopGroup(); //Frame shaker.
}

void camAnimatedFrameShaker::AddAnimationSelectorWidget()
{
	if(!ms_WidgetBank || !ms_WidgetGroup)
	{
		return;
	}

	ms_WidgetBank->SetCurrentGroup(*ms_WidgetGroup);
	{
		ms_cameraAnimSelector.AddWidgets(ms_WidgetBank);
	}
	ms_WidgetBank->UnSetCurrentGroup(*ms_WidgetGroup);

	//Inhibit any further widgets being added.
	ms_WidgetBank	= NULL;
	ms_WidgetGroup	= NULL;
}

static bool LoadAnimation(const char* pName)
{
	const strStreamingObjectName dictionary(pName);
	strLocalIndex animDictIndex = strLocalIndex(fwAnimManager::FindSlot(dictionary));
	if (animDictIndex.Get() >= 0)
	{
		if (fwAnimManager::IsValidSlot(animDictIndex))
		{
			if ( CStreaming::IsObjectInImage(animDictIndex, fwAnimManager::GetStreamingModuleId()) )
			{
				if ( !CStreaming::HasObjectLoaded(animDictIndex, fwAnimManager::GetStreamingModuleId()) )
				{
					fwAnimManager::StreamingRequest(animDictIndex, STRFLAG_FORCE_LOAD);
					CStreaming::LoadAllRequestedObjects();
				}

				return true;
			}
		}
	}
	return false;
}

static bool IsAnimLoaded(const char* pName)
{
	const strStreamingObjectName dictionary(pName);
	strLocalIndex animDictIndex = strLocalIndex(fwAnimManager::FindSlot(dictionary));
	if (animDictIndex.Get() >= 0)
	{
		if (fwAnimManager::IsValidSlot(animDictIndex))
		{
			if ( CStreaming::IsObjectInImage(animDictIndex, fwAnimManager::GetStreamingModuleId()) )
			{
				if ( CStreaming::HasObjectLoaded(animDictIndex, fwAnimManager::GetStreamingModuleId()) )
				{
					return true;
				}
			}
		}
	}

	return false;
}

static void UnloadAnimation(const char* UNUSED_PARAM(pName))
{
	// On second thought, let streaming system discard when it wants to.

	////const strStreamingObjectName dictionary(pName);
	////s32 animDictIndex = fwAnimManager::FindSlot(dictionary);
	////if (animDictIndex >= 0)
	////{
	////	if (fwAnimManager::IsValidSlot(animDictIndex))
	////	{
	////		if ( CStreaming::IsObjectInImage(animDictIndex, fwAnimManager::GetStreamingModuleId()) )
	////		{
	////			if ( CStreaming::HasObjectLoaded(animDictIndex, fwAnimManager::GetStreamingModuleId()) )
	////			{
	////				CStreaming::RemoveObject(animDictIndex, fwAnimManager::GetStreamingModuleId());
	////			}
	////		}
	////	}
	////}
}

bool WasAnimDictLoaded = true;
void camAnimatedFrameShaker::ShakeGameplayDirector()
{
	if (!camInterface::GetGameplayDirector().IsShaking())
	{
		if (ms_cameraAnimSelector.GetSelectedClipDictionary() &&
			ms_cameraAnimSelector.GetSelectedClipDictionary()[0] != 0 &&
			ms_cameraAnimSelector.GetSelectedClipName() &&
			ms_cameraAnimSelector.GetSelectedClipName()[0] != 0)
		{
			WasAnimDictLoaded = true;
			if (!IsAnimLoaded(ms_cameraAnimSelector.GetSelectedClipDictionary()))
			{
				if (LoadAnimation(ms_cameraAnimSelector.GetSelectedClipDictionary()))
				{
					WasAnimDictLoaded = false;
					u32 uStartTime = fwTimer::GetSystemTimeInMilliseconds();
					while ( fwTimer::GetSystemTimeInMilliseconds() < (uStartTime + 5000) &&
							!IsAnimLoaded(ms_cameraAnimSelector.GetSelectedClipDictionary()) )
					{
						// busy wait for anim to load.
						rage::sysIpcSleep(33);
					}
				}
			}

			safecpy(ms_DebugAnimDictionary, ms_cameraAnimSelector.GetSelectedClipDictionary());
		}

		camInterface::GetGameplayDirector().Shake(ms_cameraAnimSelector.GetSelectedClipDictionary(), ms_cameraAnimSelector.GetSelectedClipName(), "blah", ms_fAmplitude);
	}
}

void camAnimatedFrameShaker::StopShakingGameplayDirector()
{
	camInterface::GetGameplayDirector().StopShaking(false);

	if (!WasAnimDictLoaded)
	{
		// If we loaded it, unload it.
		UnloadAnimation(ms_DebugAnimDictionary);
		WasAnimDictLoaded = true;
	}
	ms_DebugAnimDictionary[0] = '\0';
}
#endif // __BANK
