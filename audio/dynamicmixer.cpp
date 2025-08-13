// 
// audio/dynamicmixer.cpp
//
// Copyright (C) 1999-2010 Rockstar Games. All Rights Reserved
//

#include "audioentity.h"
#include "dynamicmixer.h"
#include "northaudioengine.h"
#include "scriptaudioentity.h"

#include "audiohardware/driverutil.h"
#include "audioengine/categorycontrollermgr.h"
#include "audioengine/engine.h"
#include "audioengine/environment.h"
#include "audiosoundtypes/soundcontrol.h"
#include "audiosoundtypes/sound.h"
#include "audiosoundtypes/simplesound.h"

#include "grcore/font.h"
#include "input/keyboard.h"
#include "grcore/quads.h"

#include "script/wrapper.h"
#include "system/param.h"
#include "system/timemgr.h"
#include "peds/ped.h"

#include "debugaudio.h"

#include "control/replay/Audio/DynamicMixerPacket.h"

AUDIO_DYNAMICMIXING_OPTIMISATIONS()

#if __BANK
audDynamicMixerObjectManager audDynamicMixer::sm_DynamicMixerObjectManager;  
AuditionSceneDrawMode audDynamicMixer::sm_SceneDrawMode = kScenesOnly;

bool audDynamicMixer::sm_MixgroupDebugToolIsActive = false;
bool audDynamicMixer::sm_AlwaysUpdateDebugApplyVariable = false;
bool audDynamicMixer::sm_DisableMixgroupSoundProcess = false;

extern bool g_DebugDrawMixGroups;
extern u32 g_DebugKillMixGroupHash;
extern bool g_DrawMixgroupBoundingBoxes;

float audDynamicMixer::sm_AuditionSceneDrawOffsetY = 0.f;

bool g_DisplayScenes = false;
bool g_HideCodeScenes = false;
bool g_HideScriptScenes = false;
bool g_HidePlayerMixGroup = false;
bool g_DisableDynamicMixing = false;
bool g_DynamixObjectModifiedThisFrame = false;
bool g_DynamixObjectModifiedLastFrame = false;
u32 s_DebugSceneToStart = 0;

bool audDynamicMixer::sm_PrintAllControllers = false;
bool audDynamicMixer::sm_DisableGlobalControllers = false;
bool audDynamicMixer::sm_DisableDynamicControllers = false;
bool audDynamicMixer::sm_PrintDebug = false;

#endif

bool audDynamicMixer::sm_PlayerFrontend = true;
bool audDynamicMixer::sm_StealthScene = false;
bool audDynamicMixer::sm_FrontendSceneOverrided = false;

namespace rage
{
	XPARAM(noaudio);
	XPARAM(noaudiothread);
	XPARAM(audiowidgets);
};      

#define INVLOG2 (1.4426950408889634073599246810019f)

#define LOGBASE(base, target) (log(target) / log(base) )
#define LOG2(target) ( log(target) * INVLOG2 )


using namespace rage;

audDynamicMixer* audDynamicMixer::sm_Instance = NULL;
static atPool<audDynMixPatchNode> *s_MixPatchPool;
static atPool<audSceneNode> *s_ScenePool;

const u32 g_DefaultAudSceneVar = ATSTRINGHASH("apply", 0x0e865cde8);

#if __BANK

void AddDebugVariableCB()
{
	DYNAMICMIXER.AddDebugVariable();
}

#endif


////////////////////////////////////////////////////////////////////////////
// audDynMixPatch
////////////////////////////////////////////////////////////////////////////

audDynMixPatch::audDynMixPatch()
{
	m_PatchObject	= NULL;
	m_NumCategoryControllers = 0;
	m_PatchActiveTime = 0;
	m_ObjectHash = 0;
	m_ApplyFactor = 1.f;
	m_ApproachApplyFactor = 1.f;
	m_FadeOutApplyFactor = 1.f;
	m_TimeToDelete = ~0U;
	m_MixPatchRef = NULL;
	m_IsApplied = false;
	m_IsRemoving = false;
	m_IsInitialized = false;
	m_StartTime = 0;
	m_StopTime = 0;
	m_MixGroup = NULL;
	m_NoTransitions = false;
}

audDynMixPatch::~audDynMixPatch()
{
	Shutdown();
}
 
void audDynMixPatch::SetPatchObject(MixerPatch* patchObject)
{
	naAssert(patchObject);
	m_PatchObject = patchObject;
}

void audDynMixPatch::Initialize(audMixGroup * mixGroup)
{
	if (m_IsInitialized)
	{
		return;
	}

	if(mixGroup && !m_MixGroup)
	{
		m_MixGroup = mixGroup;
		BANK_ONLY(dmDebugf1("Adding mixgroup ref for patch %s", AUD_DM_HASH_NAME(m_ObjectHash));)
		m_MixGroup->AddReference(audMixGroup::PATCH_REF);
	}

	if(m_PatchObject->PreDelay > m_PatchActiveTime)
	{
		return;
	}


	naAssertf(m_PatchObject->numMixCategories <= (u32) sm_MaxCategoryControllers, "audDynMixPatch object \"%s\" %i is too many category controllers, max is %i", DYNAMICMIXMGR.GetMetadataManager().GetObjectNameFromNameTableOffset(m_PatchObject->NameTableOffset), m_PatchObject->numMixCategories, sm_MaxCategoryControllers);
	
	m_NumCategoryControllers = (u16)Min((u32)m_PatchObject->numMixCategories, sm_MaxCategoryControllers);
	sysMemSet(m_CategoryControllers, 0, sizeof (m_CategoryControllers));

#if __BANK
	if(!g_DisableDynamicMixing)
#endif
	{

		if(m_MixGroup)
		{
			if(!AUDCATEGORYCONTROLLERMANAGER.CreateMultipleDynamicControllers(mixGroup->GetCategoryIndex(), m_CategoryControllers, &m_PatchObject->MixCategories[0].Name, m_NumCategoryControllers, sizeof(MixerPatch::tMixCategories), true))
			{
				m_NumCategoryControllers = 0;
				sysMemSet(m_CategoryControllers, 0, sizeof(m_CategoryControllers));
			}

		}
		else
		{
			if(!AUDCATEGORYCONTROLLERMANAGER.CreateMultipleControllers(m_CategoryControllers, &m_PatchObject->MixCategories[0].Name, m_NumCategoryControllers, sizeof(MixerPatch::tMixCategories), true))
			{
				m_NumCategoryControllers = 0;
				sysMemSet(m_CategoryControllers, 0, sizeof(m_CategoryControllers));
			}
		}
	}
#if __BANK
	else
	{
		m_NumCategoryControllers = 0;
		sysMemSet(m_CategoryControllers, 0, sizeof(m_CategoryControllers));
	}
#endif

	m_ApplyFactorCurve.Init(m_PatchObject->ApplyFactorCurve != 0 ? m_PatchObject->ApplyFactorCurve : ATSTRINGHASH("LINEAR_RISE", 0x00d0e6f19));
	m_ApplySmoother.Init(m_PatchObject->ApplySmoothRate/1000.f);
	m_ApplySmoother.CalculateValue(1.f, m_StartTime); 

	BANK_ONLY(dmDebugf1("Initialised patch %s, stop time %u, start time %u, is applied %s, time to delete %u", AUD_DM_NAME(m_PatchObject), m_StopTime, m_StartTime, AUD_BOOL2STR(m_IsApplied), m_TimeToDelete);) 

	m_IsInitialized = true;
}

u32 audDynMixPatch::GetTime() const
{
	if(naVerifyf(m_PatchObject, "Trying to get time on a dynamic mix patch with no MixerPatch"))
	{
		if(AUD_GET_TRISTATE_VALUE(m_PatchObject->Flags, FLAG_ID_MIXERPATCH_DOESPAUSEDTRANSITIONS) == AUD_TRISTATE_TRUE)
		{
			return g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(1);
		}
		else
		{
			return g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
		}
	}
	else
	{
		return 0;
	}
}

//Constuctor won't be called on new audDynMixPatches, so 
void audDynMixPatch::Initialize(MixerPatch* patchObject, audMixGroup * mixGroup, u32 startOffset)
{
	m_PatchObject	= NULL;
	m_NumCategoryControllers = 0;
	m_PatchActiveTime = 0;
	m_ApplyFactor = 1.f;
	m_ApproachApplyFactor = 1.f;
	m_FadeOutApplyFactor = 1.f;
	m_TimeToDelete = ~0U;
	m_MixPatchRef = NULL;
	m_IsApplied = false;
	m_IsRemoving = false;
	m_IsInitialized = false;
	m_ApplyVariable = NULL;
	m_MixGroup = NULL;
	m_NoTransitions = false;


	SetPatchObject(patchObject);

	m_StartTime = GetTime() > startOffset ? GetTime() - startOffset : 0;

	Initialize(mixGroup);
}

void audDynMixPatch::Shutdown()
{
	BANK_ONLY(dmDisplayf("ShutDown patch %s at time %d, frame %u", AUD_DM_NAME(m_PatchObject), fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());)

	if(m_CategoryControllers)
	{
		AUDCATEGORYCONTROLLERMANAGER.DestroyMultipleControllers(m_CategoryControllers, m_NumCategoryControllers);
		sysMemSet(m_CategoryControllers, 0, sizeof(m_CategoryControllers));
		m_NumCategoryControllers = 0;
	}
	if(m_MixPatchRef)
	{
		*m_MixPatchRef = NULL;
		m_MixPatchRef = NULL;
	}

	if(m_MixGroup)
	{
		m_MixGroup->RemoveReference(audMixGroup::PATCH_REF);
		m_MixGroup = NULL;
	}

	m_IsRemoving = false;
	m_IsInitialized = false;
}  

bool audDynMixPatch::DoTransition()
{
	if(m_NoTransitions)
	{
		return false;
	}
	if(m_PatchObject)
	{
		return m_PatchObject->FadeIn !=0 || m_PatchObject->FadeOut !=0;
	}
	else
	{
		BANK_ONLY(naErrorf("Patch object on %s (%u) is invalid", g_AudioEngine.GetDynamicMixManager().GetMetadataManager().GetObjectName(m_ObjectHash), m_ObjectHash);)
	}
	return false;
}

bool audDynMixPatch::Apply(float applyFactor, audScene* scene)
{
	m_Scene = scene;

	//This will be the next time this patch exits
	m_PatchActiveTime = 0;

	m_IsApplied = true;

	{
		m_ApplyFactor = applyFactor;
		m_ApproachApplyFactor = 0; 

		m_IsRemoving = false;
	}


	return true;
}

void audDynMixPatch::Remove()
{
	if(!m_IsApplied)
	{
		NOTFINAL_ONLY(naErrorf("[audDynMixPatch] Why are trying to detrigger patch %s when it's already inactive?",
			m_PatchObject ? DYNAMICMIXMGR.GetMetadataManager().GetNameFromNTO_Debug(m_PatchObject->NameTableOffset) : "Unknown");)
		return;
	}
	
	if(m_PatchObject) 
	{
		BANK_ONLY(dmDebugf3("Removing patch object %s, at time %u, frame %u", AUD_DM_NAME(m_PatchObject), fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());)

		
		{
			m_FadeOutApplyFactor = m_ApproachApplyFactor;
			// Adjust out game object's registered category volumes if we don't use a scalar value
			if(!DoTransition())
			{
				for (u32 i =0; i < m_NumCategoryControllers; ++i)
				{
					audCategoryController* cat = m_CategoryControllers[i];

					cat->SetVolumeLinear(1.f);
					cat->SetDistanceRolloffScale(1.f);
					cat->SetPitchCents(0.f);
					cat->SetLPFCutoffHz(kVoiceFilterLPFMaxCutoff);
					cat->SetHPFCutoffHz(kVoiceFilterHPFMinCutoff);
				}
			}
			m_IsRemoving = true;

			m_StopTime = GetTime();

			//Clear the applied flag
			m_IsApplied = false;
			m_PatchActiveTime = 0;

			// we know we need to be out no later than 1/4 seconds after our transition time
			m_TimeToDelete = GetTime() + 250 + m_PatchObject->FadeOut;

			// Nullify the external reference if necessary
			if(m_MixPatchRef)
			{
				*m_MixPatchRef = NULL;
				m_MixPatchRef = NULL;
			}
		}
	}
}

bool audDynMixPatch::IsReadyForDeletion() const
{

	if((!m_IsInitialized && !m_IsRemoving) || m_IsApplied)
	{
		return false;
	}
	
	//This will likely be true if something has gone wrong, like a controller getting stuck, in which case just delete it!
	if(GetTime() >= m_TimeToDelete)
	{
		return true;
	}

	for(u32 i=0; i < m_NumCategoryControllers; ++i)
	{
		if(!m_CategoryControllers[i]->IsAtUnity())
		{
			return false;
		}
	}

	return true;
} 

f32 audDynMixPatch::GetApplyFactor()
{
#if __BANK
	if(g_DisableDynamicMixing)
	{
		return 0.0f;
	}
#endif

	//If we haven't found an apply variable yet or are using the default apply when we have
	//a specified variable, look up to the scenes for the variable
	if(!m_ApplyVariable || (m_PatchObject->ApplyVariable != m_ApplyVariable->hash))
	{
		if(m_Scene)
		{
			 m_ApplyVariable = m_Scene->GetVariableAddress(m_PatchObject->ApplyVariable);
		}
	}


	float applyFactor = 1.f;

	//There will almost always be an apply variable, unless the patch is not created from a scene
	if(m_ApplyVariable)
	{
		applyFactor = m_ApplyVariable->value;
	}

	m_ApplyFactor = Clamp<f32>(m_ApplyFactorCurve.CalculateValue(applyFactor), 0.f, 1.f);

	if(AUD_GET_TRISTATE_VALUE(m_PatchObject->Flags, FLAG_ID_MIXERPATCH_DISABLEDINPAUSEMENU) != AUD_TRISTATE_FALSE && audNorthAudioEngine::IsAudioPaused() && CPauseMenu::IsActive())
	{
		m_ApplyFactor = 0.f;
	}

	return m_ApplyFactor;
}
	
void audDynMixPatch::UpdateFromRave()
{
#if __BANK
		audCategoryController* oldControllers[sm_MaxCategoryControllers] = {NULL};
		u32 oldNumControllers = m_NumCategoryControllers;
		for(int i =0; i< oldNumControllers; i++)
		{
			oldControllers[i] = m_CategoryControllers[i];
		}
		m_PatchObject = DYNAMICMIXMGR.GetObject<MixerPatch>(m_ObjectHash);
		m_NumCategoryControllers = m_PatchObject->numMixCategories;
		for(int i=0; i < m_NumCategoryControllers; i++)
		{
			m_CategoryControllers[i] = NULL;
			for(int j=0; j < oldNumControllers; j++)
			{
				if(oldControllers[j] && atStringHash(oldControllers[j]->GetCategory()->GetNameString()) == m_PatchObject->MixCategories[i].Name)
				{
					m_CategoryControllers[i] = oldControllers[j];
					oldControllers[j] = NULL;
					break;
				}
			}
		}
		m_ApplySmoother.SetRate(m_PatchObject->ApplySmoothRate/1000.f);
#endif
}

void audDynMixPatch::Update()
{
	if(m_PatchObject->Duration > 0.f && m_PatchObject->Duration < (m_PatchActiveTime - m_PatchObject->PreDelay))
	{
		Remove();
	}

	u32 now = GetTime();


	if(m_IsApplied)
	{
		m_ApplyFactor = GetApplyFactor();
		float applyFactor = Max(m_ApplyFactor, 0.0001f);
		m_PatchActiveTime = now - m_StartTime;

		BANK_ONLY(bool forceupdate = g_DynamixObjectModifiedLastFrame;)

		if(!m_IsInitialized) 
		{
			if(m_PatchObject->PreDelay <= m_PatchActiveTime)
			{
				Initialize(m_MixGroup);
			}
			else
			{
				return;
			}
		}

		naAssertf(m_IsInitialized, "Updating a patch %s in scene %s that isn't initialized is not waiting for predelay", g_AudioEngine.GetDynamicMixManager().GetMetadataManager().GetObjectName(m_ObjectHash), DYNAMICMIXMGR.GetMetadataManager().GetObjectNameFromNameTableOffset(m_Scene->GetSceneSettings()->NameTableOffset));

		f32 smoothedApplyFactor;

		if(m_PatchObject->ApplySmoothRate > 0.f BANK_ONLY(&& !g_DisableDynamicMixing))
		{
			smoothedApplyFactor = m_ApplySmoother.CalculateValue(applyFactor, now);
		}
		else
		{
			smoothedApplyFactor = applyFactor;
		}


		// Adjust all the category levels according to our apply value
		if(!DoTransition())
		{
			if(m_ApproachApplyFactor != applyFactor BANK_ONLY(|| forceupdate))
			{
				for (u32 i = 0; i < m_NumCategoryControllers; ++i)
				{
					audCategoryController* cat = m_CategoryControllers[i];
					const MixerPatch::tMixCategories& params = m_PatchObject->MixCategories[i];


					f32 dbVol = params.Volume * 0.01f;
					if(params.Volume > 0.f)
					{
						dbVol *= -1.f;
					}
					f32 gain = Lerp(smoothedApplyFactor, 1.f, audDriverUtil::ComputeLinearVolumeFromDb(dbVol));
					if(params.Volume > 0.f)
					{
						gain = 1.f/(gain + 0.0001f);
					}
					f32 freq = params.Pitch != 0 ? audDriverUtil::ConvertPitchToRatio(params.Pitch) : params.Frequency;

					f32 linFreq = Lerp(smoothedApplyFactor, 1.f, freq);
					if(params.PitchInvert == INPUT_INVERT && linFreq != 0.f)
					{
						linFreq = 1.f/linFreq;
					}
					const f32 LPFCutoffOctaves = Lerp(smoothedApplyFactor, 0.f, LOG2(Max(static_cast<f32>(kVoiceFilterLPFMinCutoff),(float)params.LPFCutoff) * g_InvSourceLPFilterCutoff));
					const f32 HPFCutoffOctaves = Lerp(smoothedApplyFactor, g_OctavesBelowFilterMaxAt1Hz, LOG2(Max(1.f, (float)params.HPFCutoff) * g_InvSourceHPFilterCutoff));
					const f32 rolloff = Lerp(smoothedApplyFactor, 1.f, params.Rolloff);

					cat->SetVolumeLinear(gain);
					cat->SetFrequencyRatio(linFreq);
					cat->SetLPFCutoffOctaves(LPFCutoffOctaves);
					cat->SetHPFCutoffOctaves(HPFCutoffOctaves);
					cat->SetDistanceRolloffScale(rolloff);
				}

				m_ApproachApplyFactor = smoothedApplyFactor;
			}
		}
		else 
		{
			if(m_PatchObject->FadeIn > (now-m_StartTime - m_PatchObject->PreDelay))
			{
				m_ApproachApplyFactor = smoothedApplyFactor * (((f32)(now-m_StartTime - m_PatchObject->PreDelay))/((f32)(m_PatchObject->FadeIn)));
			}
			else
			{
				m_ApproachApplyFactor = smoothedApplyFactor;
			}

			RecalculateCategoriesUsingApply(m_ApproachApplyFactor);
		}
	}
	else 
	{
		if(!m_IsRemoving) 
		{
			if(DoTransition())
			{
				if(m_PatchObject->FadeOut > (now-m_StopTime))
				{
					m_FadeOutApplyFactor = m_ApproachApplyFactor * (1.f - (((f32)(now-m_StopTime))/((f32)(m_PatchObject->FadeOut))));
				}

				RecalculateCategoriesUsingApply(m_FadeOutApplyFactor);
			}
			else
			{

				for (u32 i =0; i < m_NumCategoryControllers; ++i)
				{
					audCategoryController* cat = m_CategoryControllers[i];

					cat->SetVolumeLinear(1.f);
					cat->SetDistanceRolloffScale(1.f);
					cat->SetPitchCents(0.f);
					cat->SetLPFCutoffHz(kVoiceFilterLPFMaxCutoff);
					cat->SetHPFCutoffHz(kVoiceFilterHPFMinCutoff);
				}
			}
			m_IsRemoving = true;
		}
		else 
		{
			if(DoTransition())
			{
				if(m_PatchObject->FadeOut > (now-m_StopTime))
				{
					m_FadeOutApplyFactor = m_ApproachApplyFactor * (1.f - (((f32)(now-m_StopTime))/((f32)(m_PatchObject->FadeOut))));
				}
				else
				{
					m_FadeOutApplyFactor = 0.f;
				}
				RecalculateCategoriesUsingApply(m_FadeOutApplyFactor);
			}
		}
	}
}

bool audDynMixPatch::HasFinishedFade()
{
	return !DoTransition() || m_ApplyFactor == m_ApproachApplyFactor;
}

void audDynMixPatch::RecalculateCategoriesUsingApply(float applyFactor)
{
	m_ApplyFactor = applyFactor; 

	// Adjust our game object's registered category settings if we don't use a constantly adjusting scalar value
	if(DoTransition()) 
	{
		for(u32 i=0; i < m_NumCategoryControllers; ++i)
		{
			audCategoryController* cat = m_CategoryControllers[i]; 
			const MixerPatch::tMixCategories& params = m_PatchObject->MixCategories[i];
			f32 dbVol = params.Volume * 0.01f;
			if(params.Volume > 0.f)
			{
				dbVol *= -1.f;
			}
			f32 gain = Lerp(applyFactor, 1.f, audDriverUtil::ComputeLinearVolumeFromDb(dbVol));
			if(params.Volume > 0.f)
			{
				gain = 1.f/(gain + 0.0001f);
			}

			f32 freq = params.Pitch != 0 ? audDriverUtil::ConvertPitchToRatio(params.Pitch) : params.Frequency;

			f32 linFreq = Lerp(applyFactor, 1.f, freq);
			if(params.PitchInvert == INPUT_INVERT && linFreq != 0.f)
			{
				linFreq = 1.f/linFreq;
			}

			const f32 LPFCutoffOctaves = Lerp(applyFactor, 0.f, LOG2(Max(static_cast<f32>(kVoiceFilterLPFMinCutoff),(float)params.LPFCutoff) * g_InvSourceLPFilterCutoff));
			const f32 HPFCutoffOctaves = Lerp(applyFactor, g_OctavesBelowFilterMaxAt1Hz, LOG2(Max(1.f, (float)params.HPFCutoff)*g_InvSourceHPFilterCutoff));
			const f32 rolloff = Lerp(applyFactor, 1.f, params.Rolloff);

			cat->SetVolumeLinear(gain);
			cat->SetFrequencyRatio(linFreq);
			cat->SetLPFCutoffOctaves(LPFCutoffOctaves);
			cat->SetHPFCutoffOctaves(HPFCutoffOctaves);
			cat->SetDistanceRolloffScale(rolloff);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////
//audScene
//////////////////////////////////////////////////////////////////////////////////

//Should never be called as we use an atStaticPool 
audScene::audScene()
{
#if __BANK
	m_IsDebugScene = false;
#endif
	m_IncrementedMobileRadioCount = false;
	m_SceneHash = 0;
	m_IsReplay = false;
}

//NB: if we're using the pools this won't get called
audScene::~audScene()
{
	Shutdown();
}

void audScene::Shutdown()
{

#if GTA_REPLAY
	//Stop recording script audScene (hash only set for script) 
	if(CReplayMgr::ShouldRecord() && GetHash() != 0)
	{
		CReplayMgr::RecordPersistantFx<CPacketDynamicMixerScene>(
			CPacketDynamicMixerScene(GetHash(), true, 0),
			CTrackedEventInfo<tTrackedSceneType>(this), NULL, false);
	
		//Need to make sure that the tracking of this packet is cleaned up.
		//Fixes going into out of control sections and not cleaning up the tracking state.
		CReplayMgr::StopTrackingFx(CTrackedEventInfo<tTrackedSceneType>(this));	
		SetHash(0);
	}
#endif //GTA_REPLAY

	if(m_SceneRef)
	{
		*m_SceneRef = NULL;
		m_SceneRef = NULL;
	}

	BANK_ONLY(dmDebugf2("Shutting down scene %s at time %u", AUD_DM_NAME(m_SceneSettings), fwTimer::GetTimeInMilliseconds());)

	if(m_SceneSettings)
	{
		for(s32 i=0; i < m_SceneSettings->numPatchGroups; i++)
		{
			naAssertf(!m_Patches[i].patch, "Scene %s is shutting down but it still has patch references!", g_AudioEngine.GetDynamicMixManager().GetMetadataManager().GetObjectNameFromNameTableOffset(m_SceneSettings->NameTableOffset));
			
			if(m_Patches[i].mixGroup)
			{
				DYNAMICMIXMGR.ReleaseMixGroupReference(m_Patches[i].mixGroup->GetObjectHash(), audMixGroup::SCENE_REF);
				m_Patches[i].mixGroup = NULL;
			}
		}

		if(AUD_GET_TRISTATE_VALUE(m_SceneSettings->Flags, FLAG_ID_MIXERSCENE_MUTEUSERMUSIC) == AUD_TRISTATE_TRUE)
		{
			g_AudioEngine.RestoreUserMusic();
		}
		if(AUD_GET_TRISTATE_VALUE(m_SceneSettings->Flags, FLAG_ID_MIXERSCENE_OVERRIDEFRONTENDSCENE) == AUD_TRISTATE_TRUE)
		{
			if(naVerifyf(audDynamicMixer::sm_FrontendSceneOverrided, "Scene %s trying to remove a 'Frontend Scene Disabled' ref but count is already 0", DYNAMICMIXMGR.GetMetadataManager().GetObjectNameFromNameTableOffset(m_SceneSettings->NameTableOffset)))
			{
				audNorthAudioEngine::OverrideFrontendScene(NULL);
				audDynamicMixer::sm_FrontendSceneOverrided = false;
			}
		}
	}

	if(m_IncrementedMobileRadioCount)
	{
		DYNAMICMIXER.DecrementMobileRadioCount();
		m_IncrementedMobileRadioCount = false;
	}

	m_SceneSettings = NULL;

#if __BANK
	m_IsDebugScene = false;
#endif
}

void audScene::Init(MixerScene *sceneSettings)
{
	SetSceneObject(sceneSettings);

	sysMemSet(m_Variables, 0, sizeof(variableNameValuePair)*g_MaxAudSceneVariables);  
	
	//The first variable of every sound scene is the default 'apply' variable
	//which we fall back to if scene specified variables have not been set

	SetVariableValue(g_DefaultAudSceneVar, 1.f);

	sysMemSet(m_Patches, 0, sizeof(audPatchGroup)*MixerScene::MAX_PATCHGROUPS);

	m_SceneRef = NULL;
	m_Script = NULL;
	m_CurrentTransition = g_NullSoundHash;
	m_IncrementedMobileRadioCount = false;
	m_SceneHash = 0;
	m_IsReplay = false;
#if __BANK
	m_IsDebugScene = false;
#endif
}

void audScene::SetSceneObject(MixerScene* sceneSettings)
{
	naAssertf(sceneSettings, "Invalid MixerScene object passed into audScene::SetSceneObject");
	m_SceneSettings = sceneSettings;
}

//Unlike audio variables, mix variables can report a default so we return the variableNameValuePair
//in order that the mix patch can check which variable it actually recieved. If we want to use these variables for 
//sounds too, add an extra function to return just the value
const variableNameValuePair * audScene::GetVariableAddress(u32 nameHash)
{
	//Loop through our own variable block to see if it's one of ours
	for(s32 i=0; i < g_MaxAudSceneVariables; i++)
	{
		if(m_Variables[i].hash == nameHash)
		{
			//We own the variable it's asking about, so return its address
			return &(m_Variables[i]);
		}
		else if(m_Variables[i].hash == 0)
		{
			break;
		}
	}

	//No scene scope variable, so look up to the script
	if(m_Script)
	{
		const variableNameValuePair* vPair = m_Script->GetVariableAddress(nameHash);
		if(vPair)
		{
			return vPair;
		}
	}
	
	//No scene or script variables matching this name so we'll use the default 'apply' for the scene
	//which is always the first variable
	return &(m_Variables[0]);
}

const variableNameValuePair* audScene::GetVariableAddress(const char * name)
{
	return GetVariableAddress(atStringHash(name));
}

void audScene::SetVariableValue(u32 nameHash, f32 value)
{
#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordPersistantFx<CPacketSceneSetVariable>(
			CPacketSceneSetVariable(nameHash, value),
			CTrackedEventInfo<tTrackedSceneType>(this));
	}
#endif // GTA_REPLAY

	//Look through our variable block to see if it exists already
	for(s32 i=0; i < g_MaxAudSceneVariables; i++)
	{
		if(m_Variables[i].hash == nameHash)
		{
			//the variable exists, so set it
			m_Variables[i].value = value;
			return;
		}
		else if (m_Variables[i].hash == 0)
		{
			//we've got the first empty one, so set this one to be the new variable
			//and bail out
			m_Variables[i].hash = nameHash;
			m_Variables[i]. value = value;
			return;
		}
	}
	naAssertf(0, "audScene (%s) has run out of variables; consider using less variables or upping the max number of variables", DYNAMICMIXMGR.GetMetadataManager().GetObjectNameFromNameTableOffset(m_SceneSettings->NameTableOffset));
}

#if __BANK
void audScene::SetVariableValueDebug(u32 nameHash, f32 value)
{
	//Look through our variable block to see if it exists already
	for(s32 i=0; i < g_MaxAudSceneVariables; i++)
	{
		if(m_Variables[i].hash == nameHash)
		{
			//the variable exists, so set it
			m_Variables[i].value = value;
			return;
		}
	}
}
#endif

void audScene::SetVariableValue(const char * name, float value)
{
	SetVariableValue(atStringHash(name), value);
}

void audScene::Start(const audScript* script, u32 startOffset)
{
	Assert(m_SceneSettings);

	m_Script = script;

	NOTFINAL_ONLY(dmDisplayf("numPatchgroups %d, object name %s", m_SceneSettings->numPatchGroups, DYNAMICMIXMGR.GetMetadataManager().GetNameFromNTO_Debug(m_SceneSettings->NameTableOffset));)

	for(s32 i=0; i < m_SceneSettings->numPatchGroups; i++)
	{
		//If the scene has no mix group, global categories will be used
		if(m_SceneSettings->PatchGroup[i].MixGroup)
		{
			DYNAMICMIXMGR.GetMixGroupReference(m_SceneSettings->PatchGroup[i].MixGroup, audMixGroup::SCENE_REF);
			m_Patches[i].mixGroup = DYNAMICMIXMGR.GetMixGroup(m_SceneSettings->PatchGroup[i].MixGroup);
#if __ASSERT
			MixGroup *mixGroupSettings = DYNAMICMIXMGR.GetObject<MixGroup>(m_SceneSettings->PatchGroup[i].MixGroup);

			if(naVerifyf(mixGroupSettings, "Failed to find mix group settings for %u", m_SceneSettings->PatchGroup[i].MixGroup))
			{
				naAssertf(m_Patches[i].mixGroup, "Attempting to apply a patch with mix group %s but the group has not been created by either script or code", DYNAMICMIXMGR.GetMetadataManager().GetObjectNameFromNameTableOffset(mixGroupSettings->NameTableOffset));
			}
#endif
		}

		audMetadataObjectInfo info;
		if(naVerifyf(DYNAMICMIXMGR.GetMetadataManager().GetObjectInfo(m_SceneSettings->PatchGroup[i].Patch, info), "Patch group has no valid MixerPatch when starting scene"))
		{
			if(info.GetType() == MixerPatch::TYPE_ID)
			{
				DYNAMICMIXER.ApplyPatch(m_SceneSettings->PatchGroup[i].Patch, m_Patches[i].mixGroup, &m_Patches[i].patch, this, 1.f, startOffset);
			}
		}
	}

	m_IncrementedMobileRadioCount = AUD_GET_TRISTATE_VALUE(m_SceneSettings->Flags, FLAG_ID_MIXERSCENE_KEEPMOBILERADIOACTIVE) == AUD_TRISTATE_TRUE;
	if(m_IncrementedMobileRadioCount)
	{
		DYNAMICMIXER.IncrementMobileRadioCount();
	}
}

audDynMixPatch * audScene::GetPatch(u32 nameHash)
{
	for(s32 i=0; i < m_SceneSettings->numPatchGroups; i++)
	{
		if(m_Patches[i].patch && m_Patches[i].patch->GetHash()==nameHash)
		{
			return m_Patches[i].patch;
		}
	}
	return NULL;
}

bool audScene::IsFullyApplied()
{
	for(int i=0; i< m_SceneSettings->numPatchGroups; i++)
	{
		if(m_Patches[i].patch && m_Patches[i].patch->GetApproachApplyFactor() < 1.f)
		{
			return false;
		}
	}
	return true;
}

bool audScene::IsFullyAppliedForPause()
{
	for(int i=0; i< m_SceneSettings->numPatchGroups; i++)
	{
		if(m_Patches[i].patch && m_Patches[i].patch->GetGameObject() && AUD_GET_TRISTATE_VALUE(m_Patches[i].patch->GetGameObject()->Flags, FLAG_ID_MIXERPATCH_DOESPAUSEDTRANSITIONS) != AUD_TRISTATE_TRUE && m_Patches[i].patch->GetApproachApplyFactor() < 1.f)
		{
			return false;
		}
	}
	return true;
}

bool audScene::HasFinishedFade()
{

	for(int i=0; i<m_SceneSettings->numPatchGroups; i++)
	{
		if(!m_Patches[i].patch->HasFinishedFade())
		{
			return false;
		}
	}

	return true;
}

void audScene::Stop()
{
#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord() && GetHash() != 0)
	{
		CReplayMgr::RecordPersistantFx<CPacketDynamicMixerScene>(
			CPacketDynamicMixerScene(GetHash(), true, 0),
			CTrackedEventInfo<tTrackedSceneType>(this), NULL);
		
		//Need to make sure that the tracking of this packet is cleaned up.
		//Fixes going into out of control sections and not cleaning up the tracking state.
		CReplayMgr::StopTrackingFx(CTrackedEventInfo<tTrackedSceneType>(this));
		SetHash(0);
	}
#endif //GTA_REPLAY

	NOTFINAL_ONLY(dmDisplayf("Stopping audio scene with settings %s at time %u", DYNAMICMIXMGR.GetMetadataManager().GetNameFromNTO_Debug(m_SceneSettings->NameTableOffset), fwTimer::GetTimeInMilliseconds());)
	for(s32 i=0; i < m_SceneSettings->numPatchGroups; i++)
	{
		if(m_Patches[i].patch)
		{
			m_Patches[i].patch->Remove();
			m_Patches[i].patch = NULL;
		}
		if(m_Patches[i].mixGroup)
		{
			DYNAMICMIXMGR.ReleaseMixGroupReference(m_Patches[i].mixGroup->GetObjectHash(), audMixGroup::SCENE_REF);
			m_Patches[i].mixGroup = NULL;
		}
	}
	
	//Remove the script's reference to the scene and the scene's reference to the script.
	if(m_SceneRef)
	{
		*m_SceneRef = NULL;
		m_SceneRef = NULL;
	}

	m_Script = NULL;

	if(m_IncrementedMobileRadioCount)
	{
		DYNAMICMIXER.DecrementMobileRadioCount();
		m_IncrementedMobileRadioCount = false;
	}
}

bool audScene::IsReadyForDeletion()
{
	for(s32 i=0; i < m_SceneSettings->numPatchGroups; i++)
	{
		if(m_Patches[i].patch)
		{
			return false;
		}
	}
	return true;
}

bool audScene::IsInstanceOf(u32 sceneNameHash)
{
	return m_SceneSettings && m_SceneSettings==DYNAMICMIXMGR.GetObject<MixerScene>(sceneNameHash);
}

///////////////////////////////////////////////////////////////////////////////////
// audDynamicMixer
///////////////////////////////////////////////////////////////////////////////////


audDynamicMixer::audDynamicMixer()
{
	m_IsInitialized = false;
}

audDynamicMixer::~audDynamicMixer()
{
	Shutdown();
}

void audDynamicMixer::ShutdownSession()
{
	audDynMixPatchNode * patchNode = m_ListMixPatches.GetHead();

	while(patchNode)
	{
		audDynMixPatch * patch = &patchNode->Data;
		
		audDynMixPatchNode * next = patchNode->GetNext();
		patch->Shutdown();
		m_ListMixPatches.Remove(*patchNode);
		s_MixPatchPool->Delete(patchNode);
		patchNode = next;
	}

	audSceneNode * sceneNode = m_ListScenes.GetHead();

	while(sceneNode)
	{
		audScene* scene = &sceneNode->Data;
		{
			scene->Shutdown();

			audSceneNode* next = sceneNode->GetNext();
			m_ListScenes.Remove(*sceneNode);
			s_ScenePool->Delete(sceneNode);
			sceneNode = next;
		}
	}
}
void audDynamicMixer::Shutdown()
{
	ShutdownSession();

	if(s_MixPatchPool)
	{
		delete s_MixPatchPool;
		s_MixPatchPool = NULL;
	}

	if(s_ScenePool)
	{
		delete s_ScenePool;
		s_ScenePool = NULL;
	}

	m_IsInitialized = false;
}

void audDynamicMixer::IncrementMobileRadioCount()
{
	sysInterlockedIncrement(&m_MobileRadioActiveCount);
}

void audDynamicMixer::DecrementMobileRadioCount()
{
	sysInterlockedDecrement(&m_MobileRadioActiveCount);
}

void audDynamicMixer::Init()
{
	naAssert(!s_ScenePool && !s_MixPatchPool);
	s_ScenePool = rage_new atPool<audSceneNode>(audScene::sm_MaxScenes, 0.47f);
	s_MixPatchPool = rage_new atPool<audDynMixPatchNode>(audDynMixPatch::sm_MaxMixPatches);
	m_MobileRadioActiveCount = 0;
	BANK_ONLY(DYNAMICMIXMGR.GetMetadataManager().SetObjectModifiedCallback(&sm_DynamicMixerObjectManager););

	m_IsInitialized = true;
}

void audDynamicMixer::AddEntityToMixGroup(CEntity * entity, u32 mixGroupHash,float fadeTime)  
{
	naAssert(entity);


	naAudioEntity* audio = entity->GetAudioEntity();

	if(!audio && entity->GetIsTypePed())
	{
		audio = ((CPed*)entity)->GetPedAudioEntity();
	}
	else if(!audio && entity->GetIsTypeVehicle())
	{
		audio = ((CVehicle*)entity)->GetVehicleAudioEntity();
	}

	if(audio)
	{
		audio->GetEnvironmentGroup(true);

		if(naVerifyf(DYNAMICMIXMGR.GetMetadataManager().GetObject<MixGroup>(mixGroupHash), "Trying to add an entity to mix group (hash: %d) but mixgroup doesn't exist in data", mixGroupHash))
		{
			audio->CreateEnvironmentGroup("MixGroup",naAudioEntity::MIXGROUP);

			if(!audio->GetEnvironmentGroup()) //If we don't successfully create one let's cancel the request
			{
				audio->RemoveEnvironmentGroup(naAudioEntity::MIXGROUP);
				return;
			}

			if(audio->GetEnvironmentGroup()->GetMixGroupIndex() != -1)
			{
				u32 oldMixGroupHash = g_AudioEngine.GetDynamicMixManager().GetMixGroupHash(entity->GetAudioEnvironmentGroup()->GetMixGroupIndex());
				if(oldMixGroupHash == mixGroupHash)
				{
					return;
				}
			}

			DEV_ONLY(dmDisplayf("Adding entity %s to mixgroup %s (%u) at time %d", entity->GetDebugName(), DYNAMICMIXMGR.GetMetadataManager().GetObjectName(mixGroupHash), mixGroupHash, fwTimer::GetTimeInMilliseconds());)
		
			float fadeTimeToUse = fadeTime;
			u32 applyValue = 255;
			if(fadeTimeToUse < 0.f)
			{ 
				fadeTimeToUse = ((MixGroup*)(g_AudioEngine.GetDynamicMixManager().GetMetadataManager().GetObject<MixGroup>(mixGroupHash)))->FadeTime;
			}

			if(fadeTimeToUse > 0.f)
			{
				applyValue = 0;

				if(audio->GetEnvironmentGroup()->GetMixGroupIndex() != -1)
				{
					MoveEntityToMixGroup(entity, mixGroupHash, fadeTimeToUse);
					return;
				}
			}

			audMixGroup * mixGroup = DYNAMICMIXMGR.CreateMixGroup(mixGroupHash);

			if(naVerifyf(mixGroup, "Trying to add an entity to mix group %s but the group has not been created", DYNAMICMIXMGR.GetMetadataManager().GetObjectNameFromNameTableOffset(DYNAMICMIXMGR.GetMetadataManager().GetObject<MixGroup>(mixGroupHash)->NameTableOffset)))
			{
				entity->AddToMixGroup((s16)mixGroup->GetIndex());
				((naEnvironmentGroup *)audio->GetEnvironmentGroup())->SetApplyValue((u8)applyValue);
				((naEnvironmentGroup *)audio->GetEnvironmentGroup())->SetMixGroupFadeTime(fadeTimeToUse);
				((naEnvironmentGroup *)audio->GetEnvironmentGroup())->SetMixGroupState(naEnvironmentGroup::State_Adding);
			}
			
		}
	}
}

void audDynamicMixer::RemoveEntityFromMixGroup(CEntity *entity, f32 fadeTime)
{
	naAssert(entity);
	if(!entity->GetAudioEnvironmentGroup() || entity->GetAudioEnvironmentGroup()->GetMixGroupIndex() < 0)
	{
		return;
	}

	u32 mixGroupHash = g_AudioEngine.GetDynamicMixManager().GetMixGroupHash(entity->GetAudioEnvironmentGroup()->GetMixGroupIndex());

	DEV_ONLY(dmDisplayf("Removing entity %s from mixgroup %s at time %d", entity->GetModelName(), AUD_DM_HASH_NAME(mixGroupHash), fwTimer::GetTimeInMilliseconds());)
	
	naAudioEntity* audio = entity->GetAudioEntity();

	if(!audio && entity->GetIsTypePed())
	{
		audio = ((CPed*)entity)->GetPedAudioEntity();
	}
	else if(!audio && entity->GetIsTypeVehicle())
	{
		audio = ((CVehicle*)entity)->GetVehicleAudioEntity();
	}

	float fadeTimeToUse = fadeTime;
	u32 applyValue = 255;
	if(fadeTimeToUse < 0.f)
	{
		fadeTimeToUse = g_AudioEngine.GetDynamicMixManager().GetMixGroup(mixGroupHash)->GetFadeTime();
	}

	if(fadeTimeToUse <= 0.f)
	{
		applyValue = 0;
		fadeTimeToUse = 0.f;
	}

	((naEnvironmentGroup *)entity->GetAudioEnvironmentGroup())->SetApplyValue((u8)applyValue);
	((naEnvironmentGroup *)entity->GetAudioEnvironmentGroup())->SetMixGroupFadeTime(fadeTimeToUse);
	((naEnvironmentGroup *)entity->GetAudioEnvironmentGroup())->SetMixGroupState(naEnvironmentGroup::State_Removing);

	if(fadeTimeToUse == 0.f)
	{
		DEV_ONLY(dmDebugf1("Removing %s reference from mixgroup %s (%u) in dynamicmixer", entity->GetDebugName(), g_AudioEngine.GetDynamicMixManager().GetMixGroupNameFromIndex(entity->GetMixGroupIndex()), g_AudioEngine.GetDynamicMixManager().GetMixGroupHash(entity->GetMixGroupIndex()));)
		entity->GetAudioEnvironmentGroup()->UnsetMixGroup();
	}
	
}

void audDynamicMixer::MoveEntityToMixGroup(CEntity* entity, u32 mixGroupHash,f32 transitionTime)
{
	naAssert(entity);

	if(naVerifyf(DYNAMICMIXMGR.GetMetadataManager().GetObject<MixGroup>(mixGroupHash), "Trying to move an entity to a mix group (hash; %d) mixgroup does not exist in data", mixGroupHash))
	{
		naEnvironmentGroup* envGroup = (naEnvironmentGroup *)entity->GetAudioEnvironmentGroup();
		if(!envGroup)
		{
			return;
		}

		naAssertf(envGroup->GetMixGroupState()== naEnvironmentGroup::State_Added,	"Trying to move an entity to a mixgrop before it's being completely added to its current mixgroup.");

		//Make sure there's a mix group for us
		audMixGroup * mixGroup = DYNAMICMIXMGR.CreateMixGroup(mixGroupHash);

		if(naVerifyf(mixGroup, "Trying to add an entity to mix group %s but the group has not been created", DYNAMICMIXMGR.GetMetadataManager().GetObjectNameFromNameTableOffset(DYNAMICMIXMGR.GetMetadataManager().GetObject<MixGroup>(mixGroupHash)->NameTableOffset)))
		{
			//once we are sure the entity is completely added to a mix group we start the transition.
			envGroup->SetPrevMixGroup(envGroup->GetMixGroupIndex());
			envGroup->SetApplyValue(0);
			entity->AddToMixGroup((s16)mixGroup->GetIndex());
			envGroup->SetMixGroupFadeTime(transitionTime);
			envGroup->SetMixGroupState(naEnvironmentGroup::State_Transitioning);
		}
	}
}

bool audDynamicMixer::StartScene(const char* sceneName, audScene** sceneRef, const audScript* script, u32 startOffset)
{
	bool success = StartScene(DYNAMICMIXMGR.GetObject<MixerScene>(atStringHash(sceneName)), sceneRef, script, startOffset);
#if __BANK
	if(!success)
	{
		dmDisplayf("Unable to start scene %s", sceneName);
	}
#endif
	return success;
}

bool audDynamicMixer::StartScene(u32 sceneHash, audScene** sceneRef, const audScript* script, u32 startOffset)
{
	bool success = StartScene(DYNAMICMIXMGR.GetObject<MixerScene>(sceneHash), sceneRef, script, startOffset);
#if __BANK
	if(!success)
	{
		dmDisplayf("Unable to start scene %d", sceneHash);
	}
#endif
	return success;
}

bool audDynamicMixer::StartScene(audMetadataRef objectRef, audScene** sceneRef, const audScript* script, u32 startOffset)
{
	return StartScene(DYNAMICMIXMGR.GetMetadataManager().GetObject<MixerScene>(objectRef), sceneRef, script, startOffset);
}

bool audDynamicMixer::StartScene(const MixerScene* sceneSettings, audScene** sceneRef, const audScript* script, u32 startOffset)
{
	BANK_ONLY(SYS_CS_SYNC(sm_DebugDrawLock));

	if(!sceneSettings)
	{
		naAssertf(0, "Tried to start a scene but the settings were null");
		return false;
	}

	//N.B Using the pool circumvents the constructor so setting up members
	//happens in the Init function
	if(!naVerifyf(!s_ScenePool->IsFull(), "Run out of audio dynamic mixer scenes!"))
	{
		return false;
	}
	audSceneNode *sceneNode = static_cast<audSceneNode *>(s_ScenePool->New());
	naAssert(sceneNode);

	if(!sceneNode)
	{
		return false;
	}

	sceneNode->SetNext(NULL);

	audScene * scene = &sceneNode->Data;

	//const cast because we need to adjust the reference count on the settings
	scene->Init(const_cast<MixerScene *>(sceneSettings));

	//Set up the scene list
	m_ListScenes.Append(*sceneNode);

	if(sceneRef)
	{
		naAssert(*sceneRef == NULL);
		*sceneRef = scene;
		scene->m_SceneRef = sceneRef;
	}

	scene->Start(script, startOffset);

	if(AUD_GET_TRISTATE_VALUE(sceneSettings->Flags, FLAG_ID_MIXERSCENE_MUTEUSERMUSIC) == AUD_TRISTATE_TRUE)
	{
		g_AudioEngine.OverrideUserMusic();
	}

	if(AUD_GET_TRISTATE_VALUE(sceneSettings->Flags, FLAG_ID_MIXERSCENE_OVERRIDEFRONTENDSCENE) == AUD_TRISTATE_TRUE)
	{
		if(naVerifyf(sm_FrontendSceneOverrided==false, "Started a frontend scene override (%s) but there's already one active", DYNAMICMIXMGR.GetMetadataManager().GetObjectNameFromNameTableOffset(scene->GetSceneSettings()->NameTableOffset)))
		{
			sm_FrontendSceneOverrided = true;
			audNorthAudioEngine::OverrideFrontendScene(scene);
		}
	}

	NOTFINAL_ONLY(dmDisplayf("Starting audio scene with settings %s at time %u", DYNAMICMIXMGR.GetMetadataManager().GetNameFromNTO_Debug(sceneSettings->NameTableOffset), fwTimer::GetTimeInMilliseconds());)

	return true;
}

bool audDynamicMixer::IsMetadataInChunk(void * metadataPtr, const audMetadataChunk &chunk)
{
	if(chunk.data <= metadataPtr && metadataPtr < (chunk.data+chunk.dataSize))
	{
		return true;
	}

	return false;
}

void audDynamicMixer::StopOnlineScenes()
{
	StopScenesFromChunk("GTAOnline");
}

void audDynamicMixer::StopScenesFromChunk(const char * chunkName)
{
	dmDebugf2("Stopping Online Scenes at time %u, frame %u", fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());

	const audMetadataManager &metadataMgr = g_AudioEngine.GetDynamicMixManager().GetMetadataManager();
	int chunkId = metadataMgr.FindChunkId(chunkName);
	if(chunkId > -1)
	{
		const audMetadataChunk &chunk = metadataMgr.GetChunk(chunkId);

		audDynMixPatchNode * patchNode = m_ListMixPatches.GetHead();
		while(patchNode)
		{
			audDynMixPatch* patch = &patchNode->Data;
			if(IsMetadataInChunk((void*)(patch->GetGameObject()), chunk))
			{
				BANK_ONLY(naDebugf3("Setting no transations in StopOnlineScenes on %s at time %u, frame %u", AUD_DM_HASH_NAME(patch->m_ObjectHash), fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());)
					patch->m_NoTransitions = true;
				patch->m_TimeToDelete = patch->GetTime();
			}
			patchNode = patchNode->GetNext();
		}

		audSceneNode * sceneNode = m_ListScenes.GetHead();
		while(sceneNode)
		{
			audScene* scene = &sceneNode->Data;
			if(IsMetadataInChunk((void*)(scene->GetSceneSettings()), chunk))
			{
				scene->Stop();
			}
			sceneNode = sceneNode->GetNext();
		}
	}
}


void audScene::PreparePatchesForKill()
{
	if(m_SceneSettings)
	{
		for(int i=0; i<MixerScene::MAX_PATCHGROUPS; i++)
		{
			if(m_Patches[i].patch)
			{
				BANK_ONLY(dmDebugf3("Setting no transations in PreparePatchesToKill on %s at time %u, frame %u", AUD_DM_HASH_NAME(m_Patches[i].patch->m_ObjectHash), fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());)
				m_Patches[i].patch->m_NoTransitions = true;
				m_Patches[i].patch->m_TimeToDelete = m_Patches[i].patch->GetTime();
			}
		}
	}
}


void audDynamicMixer::KillReplayScenes()
{
	dmDebugf2("Killing replay scenes at time %u, frame %u", fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());

	audSceneNode * sceneNode = m_ListScenes.GetHead();
	while(sceneNode)
	{
		audScene* scene = &sceneNode->Data;
		if(scene->GetIsReplay())
		{
			scene->PreparePatchesForKill();
			scene->Stop();
		}
		sceneNode = sceneNode->GetNext();
	}
}



audScene* audDynamicMixer::GetActiveSceneFromHash(const u32 hash)
{
	return GetActiveSceneFromMixerScene(DYNAMICMIXMGR.GetObject<MixerScene>(hash));
}

audScene* audDynamicMixer::GetActiveSceneFromMixerScene(const MixerScene *desiredScene)
{
	audSceneNode * sceneNode = m_ListScenes.GetHead();

	if(desiredScene)
	{
		while(sceneNode)
		{
			audScene* scene = &sceneNode->Data;
			if(scene && scene->GetSceneSettings() == desiredScene)
			{
				return scene;
			}	
			sceneNode = sceneNode->GetNext();
		}
	}

	return NULL;
}


void audDynamicMixer::ApplyPatch(const u32 patchObjectHash, audMixGroup * mixGroup, audDynMixPatch** mixPatchRef, audScene* scene, float applyFactor, u32 startOffset)
{
	BANK_ONLY(SYS_CS_SYNC(sm_DebugDrawLock));

	MixerPatch * patchObject = DYNAMICMIXMGR.GetObject<MixerPatch>(patchObjectHash);

	naAssert(patchObject);
	if ( patchObject == NULL )
	{
		return;
	}

	BANK_ONLY(dmDebugf1("Applying patch %s at time %u", AUD_DM_NAME(patchObject), fwTimer::GetTimeInMilliseconds());)

	//N.B. The pool circumvents the constructor so setting up members
	//happens in the Initialize function
	if(s_MixPatchPool->IsFull())
	{
		naAssert(!s_MixPatchPool->IsFull());
		return;
	}

	audDynMixPatchNode *patchNode = static_cast<audDynMixPatchNode*>(s_MixPatchPool->Construct());
	naAssert(patchNode);

	if(!patchNode)
	{
		return;
	}

	patchNode->SetNext(NULL);

	audDynMixPatch *mixPatch = &patchNode->Data;

	mixPatch->m_ObjectHash = patchObjectHash;

	//const cast because we need to adjust the reference count on the settings
	mixPatch->Initialize(const_cast<MixerPatch*>(patchObject), mixGroup, startOffset);

	// Set up the mixer patch list
	m_ListMixPatches.Append(*patchNode);

	if(mixPatchRef)
	{
		naAssert(*mixPatchRef == NULL);
		*mixPatchRef = mixPatch;
		mixPatch->m_MixPatchRef = mixPatchRef;
	}

	mixPatch->Apply(applyFactor, scene);
}
   
void audDynamicMixer::Update()
{
	BANK_ONLY(SYS_CS_SYNC(sm_DebugDrawLock));
	if(!IsInitialized())
	{
		return;
	}

#if __BANK
	if(sm_MixgroupDebugToolIsActive)
	{
		if(g_pFocusEntity && (g_pFocusEntity->GetIsTypePed() || g_pFocusEntity->GetIsTypeVehicle()))
		{
			bool checkRightMouseClickOnSelectEntity = (ioMouse::GetPressedButtons()&ioMouse::MOUSE_RIGHT) == ioMouse::MOUSE_RIGHT;
			bool checkLeftMouseClickOnSelectEntity = (bool)(ioMouse::GetPressedButtons()&ioMouse::MOUSE_LEFT);
			if(checkLeftMouseClickOnSelectEntity)
			{
				if(strlen(sm_DynamicMixerObjectManager.m_RaveMixGroupName))
				{
					u32 mixGroupHash = atStringHash(sm_DynamicMixerObjectManager.m_RaveMixGroupName);
					if(DYNAMICMIXMGR.GetMetadataManager().GetObject<MixGroup>(mixGroupHash))
					{
						AddEntityToMixGroup(g_pFocusEntity, mixGroupHash);
					}
				}
			}
			else if(checkRightMouseClickOnSelectEntity)
			{
				RemoveEntityFromMixGroup(g_pFocusEntity);
			}
		}
	}

	
#endif

	audDynMixPatchNode * patchNode = m_ListMixPatches.GetHead();

	while(patchNode)
	{
		audDynMixPatch * patch = &patchNode->Data;
		if(patch->m_IsApplied)
		{
			patch->Update();

			patchNode = patchNode->GetNext();
		}
		else
		{
			if(patch->IsReadyForDeletion())
			{
				audDynMixPatchNode * next = patchNode->GetNext();
				patch->Shutdown();
				m_ListMixPatches.Remove(*patchNode);
				s_MixPatchPool->Delete(patchNode);
				patchNode = next;
			}
			else 
			{
				patch->Update();
				patchNode = patchNode->GetNext();
			}
		}   
	}

	audSceneNode * sceneNode = m_ListScenes.GetHead();

	bool playerFrontend = true;
	bool stealthScene = false;
	while(sceneNode)
	{
		audScene* scene = &sceneNode->Data;
		if(scene->IsReadyForDeletion())
		{
			scene->Shutdown();

			audSceneNode* next = sceneNode->GetNext();
			m_ListScenes.Remove(*sceneNode);
			s_ScenePool->Delete(sceneNode);
			sceneNode = next;
		}
		else
		{
#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord() && scene->m_Script && scene->GetSceneSettings())
			{
				audDynMixPatch *patch = NULL;
				for(int patchIndex=0; patchIndex < scene->GetSceneSettings()->numPatchGroups; patchIndex++)
				{
					if(scene->m_Patches[patchIndex].patch)
					{
						patch = scene->m_Patches[patchIndex].patch;
						break;
					}
				}

				if(patch)
				{
					dmAssertf(scene->GetHash(), "Null hash on audio dynamic mix scene %s when recording for replay", 
						scene->GetSceneSettings() ?
						g_AudioEngine.GetDynamicMixManager().GetMetadataManager().GetObjectNameFromNameTableOffset(scene->GetSceneSettings()->NameTableOffset) :
						"(no settings on scene)");
					if(scene->GetHash())
					{
						CReplayMgr::RecordPersistantFx<CPacketDynamicMixerScene>(
							CPacketDynamicMixerScene(scene->GetHash(), false, patch->GetTime() - patch->m_StartTime),
							CTrackedEventInfo<tTrackedSceneType>(scene), NULL, true);
					}
					else
					{
						dmErrorf("Null hash on audio dynamic mix scene %s when recording for replay", 
							scene->GetSceneSettings() ?
							g_AudioEngine.GetDynamicMixManager().GetMetadataManager().GetObjectNameFromNameTableOffset(scene->GetSceneSettings()->NameTableOffset) :
							"(no settings on scene)");
					}
				}
			}
#endif

#if __BANK
			if(g_DynamixObjectModifiedThisFrame || sm_AlwaysUpdateDebugApplyVariable)
			{
				scene->SetVariableValueDebug(atStringHash(sm_DynamicMixerObjectManager.m_ApplyVariableName),sm_DynamicMixerObjectManager.m_ApplyVariableValue);
			}
#endif
			//NB: if audScene had an update, we'd call it here
			if(scene->GetSceneSettings() 
			&& AUD_GET_TRISTATE_VALUE(scene->GetSceneSettings()->Flags,FLAG_ID_MIXERSCENE_PLAYERCARDIALOGUEFRONTEND) == AUD_TRISTATE_FALSE)
			{
				playerFrontend = false;
			}
			if(scene->GetSceneSettings()
				&& AUD_GET_TRISTATE_VALUE(scene->GetSceneSettings()->Flags,FLAG_ID_MIXERSCENE_STEALTHSCENE) == AUD_TRISTATE_TRUE)
			{
				stealthScene = true;
			}

			if(scene->HasIncrementedMobileRadioCount() && scene->IsFullyApplied())
			{
				scene->DecrementMobileRadioCount();
			}

			sceneNode = sceneNode->GetNext();
		}
	}

	sm_PlayerFrontend = playerFrontend;
	sm_StealthScene = stealthScene;


}

void audDynamicMixer::FinishUpdate()
{
#if __BANK


	audDynMixPatchNode * patchNode = m_ListMixPatches.GetHead();
	if(g_DynamixObjectModifiedThisFrame)
	{
		while(patchNode)
		{
			audDynMixPatch * patch = &patchNode->Data;
			if(patch->m_IsApplied)
			{
				patch->UpdateFromRave();

				patchNode = patchNode->GetNext();
			}
			else
			{
				if(!patch->IsReadyForDeletion()) 
				{
					patch->UpdateFromRave();
				}
				patchNode = patchNode->GetNext();
			}   
		}
	}
	g_DynamixObjectModifiedLastFrame = g_DynamixObjectModifiedThisFrame;
	g_DynamixObjectModifiedThisFrame = false;

	audEnvironmentSound::SetDisableMixgroupSoundProcess(sm_DisableMixgroupSoundProcess);
	audCategoryController::SetDisableSceneControllers(sm_DisableGlobalControllers);
	audCategoryController::SetDisableDynamicControllers(sm_DisableDynamicControllers);
	if(sm_PrintAllControllers)
	{
		sm_PrintAllControllers = false;
		audCategoryControllerManager::PrintControllers();
	}
	if(s_DebugSceneToStart)
	{
		DYNAMICMIXER.StartDebugScene(s_DebugSceneToStart, true);
		s_DebugSceneToStart = 0;
	}
	if(sm_PrintDebug)
	{
		PrintDebug();
		sm_PrintDebug = false;
	}
#endif
}


#if __BANK

char audDynamicMixer::sm_DebugSceneName[audDynamicMixerObjectManager::k_NameLength] = {0};

void audDynamicMixer::StartRagDebugScene()
{
	DYNAMICMIXER.GetObjectManager().OnObjectAuditionStart(atStringHash(sm_DebugSceneName));
}
void audDynamicMixer::StopRagDebugScene()
{
	DYNAMICMIXER.GetObjectManager().OnObjectAuditionStop(atStringHash(sm_DebugSceneName));
}

f32 g_DebugTextScaling = 1.2f;

void audDynamicMixer::AddWidgets(bkBank& bank)
{ 
	bank.PushGroup("Dynamic Mixer", false);  
		bank.PushGroup("Dynamic Mixer Debug Tool", false);
			bank.AddText("Debug scene name", sm_DebugSceneName, sm_DynamicMixerObjectManager.k_NameLength);
			bank.AddButton("Start debug scene", CFA(StartRagDebugScene));
			bank.AddButton("Stop debug scene", CFA(StopRagDebugScene));
			bank.AddToggle("Activate mixgroup debug", &sm_MixgroupDebugToolIsActive);
			bank.AddText("Current debug mixgroup", sm_DynamicMixerObjectManager.m_RaveMixGroupName, sm_DynamicMixerObjectManager.k_NameLength);
			bank.AddText("Apply Variable", sm_DynamicMixerObjectManager.m_ApplyVariableName, sm_DynamicMixerObjectManager.k_NameLength);
			bank.AddToggle("Continuously update apply variable", &sm_AlwaysUpdateDebugApplyVariable);
			bank.AddSlider("Apply Variable Value", &sm_DynamicMixerObjectManager.m_ApplyVariableValue, 0.f, 1.f, 0.01f);
			bank.AddButton("Create new Apply variable", CFA(AddDebugVariableCB));
		bank.PopGroup();
		bank.AddToggle("Render Mixgroups", &g_DebugDrawMixGroups);
		bank.AddToggle("Display Scenes", &g_DisplayScenes);
		const char *nameList[] =
		{
			"Nothing",
			"Everything",
			"Scenes",
			"Variables",
			"Patches",
			"Categories"
		};
		bank.AddCombo("Scene Debug Info Type", (s32*)&sm_SceneDrawMode, kNumDrawModes, nameList, 0);
		bank.AddToggle("Hide Code Scenes", &g_HideCodeScenes);
		bank.AddToggle("Hide Script Scenes", &g_HideScriptScenes);
		bank.AddToggle("Hide PlayerMixGroup", &g_HidePlayerMixGroup);
		bank.AddToggle("Draw mixgroup bounding boxes", &g_DrawMixgroupBoundingBoxes);
		bank.AddToggle("Disable Dynamic Mixing", &g_DisableDynamicMixing);
		bank.AddSlider("Debug text scaling", &g_DebugTextScaling, 0.f, 4.f, 0.01f);
		bank.AddToggle("Print all controllers", &sm_PrintAllControllers);
		bank.AddToggle("Disable mixgroup sound processing", &sm_DisableMixgroupSoundProcess);
		bank.AddToggle("Disable global controllers", &sm_DisableGlobalControllers);
		bank.AddToggle("Disable mixgroup controllers", &sm_DisableDynamicControllers);
		bank.AddToggle("Print debug", &sm_PrintDebug);
		bank.AddButton("Stop all scenes", CFA(DebugStopAllScenes));
		bank.AddButton("Stop named scenes", CFA(DebugStopNamedScenes));

	bank.PopGroup();
}


audScene * audDynamicMixer::GetDebugScene(u32 sceneHash)
{
	audSceneNode* sceneNode = m_ListScenes.GetHead();

	while(sceneNode)
	{
		audScene * scene = &sceneNode->Data;
		if(scene->IsDebugScene())
		{
			if(scene->IsInstanceOf(sceneHash))
			{
				return scene;
			}
		}
		sceneNode = sceneNode->GetNext();
	}
	return NULL;
}

bool audDynamicMixer::StartDebugScene(u32 sceneHash, bool isTopLevel)
{
	MixerScene * sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(sceneHash);

	audScene * existingScene = GetDebugScene(sceneHash);
	if(existingScene)
	{
		return true;
	}

	//Tell the dynamic mixer to kick off this scene and pass through m_scene 
	//to pick up the reference

	audScene *newScene = NULL;

	bool ready = true;
	if(sceneSettings)
	{
		DYNAMICMIXER.StartScene(sceneSettings, &newScene, NULL);

		if(newScene)
		{
			newScene->SetIsDebugScene();
			newScene->SetIsTopLevel(isTopLevel);
		}
		else
		{
			Assert(newScene);
			return false;
		}
	}
	return ready;
}

void audDynamicMixer::StopDebugScene(u32 sceneHash)
{
	audScene* scene = GetDebugScene(sceneHash);

	if(scene)
	{
		scene->Stop();
	}
}

void audDynamicMixer::AddDebugVariable()
{
	u32 nameHash = atStringHash(sm_DynamicMixerObjectManager.m_ApplyVariableName);
	audSceneNode * sceneNode = m_ListScenes.GetHead();

	if(!nameHash)
	{
		return;
	}

	while(sceneNode)
	{
		audScene* scene = &sceneNode->Data;
		if(scene->IsDebugScene())
		{
			scene->SetVariableValue(nameHash, sm_DynamicMixerObjectManager.m_ApplyVariableValue);
		}
		sceneNode = sceneNode->GetNext();
	}
}

#endif

void audDynamicMixer::RemoveAll()
{
	for(audDynMixPatchNode * node = m_ListMixPatches.GetHead(); node; node = node->GetNext())
	{
		audDynMixPatch * patch = &node->Data;
		if(patch->m_IsApplied)
		{
			patch->Remove();
		}
	}
}



void audDynamicMixerObjectManager::OnObjectAuditionStart(const u32 BANK_ONLY(nameHash))
{
#if __BANK 
	audMetadataObjectInfo info;
	if(naVerifyf(DYNAMICMIXMGR.GetMetadataManager().GetObjectInfo(nameHash, info), "Rave Object has no valid dynamic mixer info for hash %u", nameHash))
	{
		if(info.GetType() == MixerScene::TYPE_ID)
		{
			s_DebugSceneToStart = nameHash;
		}
		else if(info.GetType() == MixGroup::TYPE_ID)
		{
			DYNAMICMIXER.SetMixGroupDebugToolIsActive(true);
			g_DebugDrawMixGroups = true;
			formatf(m_RaveMixGroupName, "%s", DYNAMICMIXMGR.GetMetadataManager().GetObjectName(nameHash));
		}
	}
#endif
}
void audDynamicMixerObjectManager::OnObjectAuditionStop(const u32 BANK_ONLY(nameHash))
{ 
#if __BANK 
	audMetadataObjectInfo info;
	if(naVerifyf(DYNAMICMIXMGR.GetMetadataManager().GetObjectInfo(nameHash, info), "Rave Object has no valid dynamic mixer info"))
	{
		if(info.GetType() == MixerScene::TYPE_ID)
		{
			DYNAMICMIXER.StopDebugScene(nameHash);
		}
		else if(info.GetType() == MixGroup::TYPE_ID)
		{
			g_DebugKillMixGroupHash = nameHash;
		}
	}
#endif
}

void audDynamicMixerObjectManager::OnObjectModified(const u32 UNUSED_PARAM(nameHash))
{
#if __BANK
	g_DynamixObjectModifiedThisFrame = true;
#endif
}

void audDynamicMixerObjectManager::OnObjectViewed(const u32 BANK_ONLY(nameHash))
{
#if __BANK
	audMetadataObjectInfo info;
	if(naVerifyf(DYNAMICMIXMGR.GetMetadataManager().GetObjectInfo(nameHash, info), "Rave Object has no valid dynamic mixer info"))
	{
		if(info.GetType() == MixGroup::TYPE_ID)
		{
			formatf(m_RaveMixGroupName, "%s", DYNAMICMIXMGR.GetMetadataManager().GetObjectName(nameHash));
		}
	}
#endif
}

void audScene::DecrementMobileRadioCount()
{
	if(audVerifyf(m_IncrementedMobileRadioCount, "Can't decrement mobile radio count as we never incremented!"))
	{
		DYNAMICMIXER.DecrementMobileRadioCount();
		m_IncrementedMobileRadioCount = false;
	}	
}

#if __BANK


int audScene::DebugDraw(audNorthDebugDrawManager & drawMgr, AuditionSceneDrawMode mode) const
{
	if(!m_SceneSettings)
	{
		return 0;
	}
	f32 prevScaling = grcFont::GetCurrent().GetInternalScale();

	bool bOldLighting = grcLighting( false );

	if(m_IsDebugScene)
	{
		drawMgr.SetColour(Color32( 250, 100, 250 ));
	}
	else if(m_Script)
	{
		drawMgr.SetColour(Color32(250, 250, 100));
	}
	else
	{
		drawMgr.SetColour(Color32( 100, 250, 250 ));
	}
	grcFont::GetCurrent().SetInternalScale(g_DebugTextScaling);

	int linesDrawn = 0;

	char sectionHeader[128];
	formatf(sectionHeader, "%s", DYNAMICMIXMGR.GetMetadataManager().GetObjectNameFromNameTableOffset(m_SceneSettings->NameTableOffset));
	drawMgr.PushSection(sectionHeader);
	linesDrawn++;

	drawMgr.ResetColour();

	if(mode == kEverything || mode == kVariables)
	{
		if(m_Script)
		{
			if(m_Script->m_Variables[0].hash != 0)
			{
				formatf(sectionHeader, "Script Variables");
				drawMgr.PushSection(sectionHeader);
				linesDrawn++;

				for(int i=0; i<g_MaxAudScriptVariables; i++)
				{
					if(m_Script->m_Variables[i].hash ==0)
					{
						break;
					}
					grcFont::GetCurrent().SetInternalScale(g_DebugTextScaling);

					formatf(sectionHeader, "hash: %u val; %f", m_Script->m_Variables[i].hash, m_Script->m_Variables[i].value);
					drawMgr.DrawLine(sectionHeader);
					linesDrawn++;
				}

				drawMgr.PopSection();
			}
		}

		if(m_Variables[0].hash !=0)
		{
			formatf(sectionHeader, "Scene Variables");
			drawMgr.PushSection(sectionHeader);
			linesDrawn++;

			for(int i=0; i < g_MaxAudSceneVariables; i++)
			{
				if(m_Variables[i].hash == 0)
				{
					break;
				}
				grcFont::GetCurrent().SetInternalScale(g_DebugTextScaling);

				formatf(sectionHeader, "hash: %u val; %f", m_Variables[i].hash, m_Variables[i].value);
				drawMgr.DrawLine(sectionHeader);
				linesDrawn++;
			}

			drawMgr.PopSection();
		}
	}

	if(mode == kEverything || mode == kPatches)
	{
		formatf(sectionHeader, "Patches");
		drawMgr.PushSection(sectionHeader);
		linesDrawn++;

		g_DebugDrawMixGroups = true;

		for(int i=0; i<MixerScene::MAX_PATCHGROUPS; i++)
		{

			if(m_Patches[i].patch)
			{
				linesDrawn += m_Patches[i].patch->DebugDraw(drawMgr);
			}
		}

		drawMgr.PopSection(); 
	}

	if(mode == kEverything || mode == kCategories)
	{
		formatf(sectionHeader, "Categories");
		drawMgr.PushSection(sectionHeader);
		linesDrawn++;

		g_DebugDrawMixGroups = true;

		for(int i=0; i< m_SceneSettings->numPatchGroups; i++)
		{
			if(m_Patches[i].patch && m_Patches[i].patch->GetGameObject())
			{
				formatf(sectionHeader, "PATCH: %s ", DYNAMICMIXMGR.GetMetadataManager().GetObjectNameFromNameTableOffset(m_Patches[i].patch->GetGameObject()->NameTableOffset));
				drawMgr.PushSection(sectionHeader);
				linesDrawn++;
				for(int j=0; j < m_Patches[i].patch->GetGameObject()->numMixCategories; j++)
				{
					audCategory* category = g_AudioEngine.GetCategoryManager().GetCategoryPtr(m_Patches[i].patch->GetGameObject()->MixCategories[j].Name);
					if( category )
					{
						char sectionHeader[128];
						formatf(sectionHeader, "%s  [Patch Vol : %d]  [Category combined volume : %f]"
							, g_AudioEngine.GetCategoryManager().GetMetadataManager().GetObjectName(m_Patches[i].patch->GetGameObject()->MixCategories[j].Name)
							, m_Patches[i].patch->GetGameObject()->MixCategories[j].Volume
							, category->GetVolume());
						grcFont::GetCurrent().SetInternalScale(g_DebugTextScaling);
						drawMgr.DrawLine(sectionHeader); 
						linesDrawn++;
					}
				}
				drawMgr.PopSection();
			}
		}
		drawMgr.PopSection(); 
	}

	drawMgr.PopSection();

	grcFont::GetCurrent().SetInternalScale(prevScaling);
	grcLighting(bOldLighting);
	return linesDrawn;
}

int audDynMixPatch::DebugDraw(audNorthDebugDrawManager & drawMgr) 
{
	char sectionHeader[128];
	formatf(sectionHeader, "%s Var: %u Apply %f Transition %f", DYNAMICMIXMGR.GetMetadataManager().GetObjectNameFromNameTableOffset(m_PatchObject->NameTableOffset), m_ApplyVariable ? m_ApplyVariable->hash : 0, m_IsRemoving? 0.f : GetApplyFactor(), m_IsRemoving? m_FadeOutApplyFactor : GetApproachApplyFactor());

	drawMgr.ResetColour();
	grcFont::GetCurrent().SetInternalScale(g_DebugTextScaling);

	if(m_MixGroup)
	{
		drawMgr.SetColour( Color32(m_MixGroup->GetObjectHash()|0xFF000000) );
		grcFont::GetCurrent().SetInternalScale(g_DebugTextScaling);
	}

	drawMgr.DrawLine(sectionHeader); 

	drawMgr.ResetColour();

	return 1; //number of lines drawn
}

const char* audScene::GetName() const
{
	return g_AudioEngine.GetDynamicMixManager().GetMetadataManager().GetObjectNameFromNameTableOffset(m_SceneSettings->NameTableOffset);
}

const char* audDynMixPatch::GetName() const
{
	return g_AudioEngine.GetDynamicMixManager().GetMetadataManager().GetObjectName(m_ObjectHash);
}

const audDynMixPatch* audScene::GetPatchFromIndex(u32 patchIndex) const
{
	return m_Patches[patchIndex].patch;
}


void audDynamicMixer::PrintScenes() const
{
	const audSceneNode * sceneNode = m_ListScenes.GetHead();
	dmDisplayf("Active Scenes (%u):", m_ListScenes.GetNumItems());

	while(sceneNode)
	{
		const audScene* scene = &sceneNode->Data;

		if(scene)
		{
			const MixerScene* settings = scene->GetSceneSettings();
			if(settings)
			{
				dmDisplayf("  %s, (%u patches) ", scene->GetName(), settings->numPatchGroups);
				for(int patchIndex = 0; patchIndex < settings->numPatchGroups; patchIndex++)
				{
					const audDynMixPatch* patch = scene->GetPatchFromIndex(patchIndex);
					if(patch)
					{
						dmDisplayf("    %s, Apply: %f", patch->GetName(), patch->GetApproachApplyFactor());
					}
				}
			}
		}

		sceneNode = sceneNode->GetNext();
	}

	dmDisplayf("");
}

void audDynamicMixer::PrintPatches() const
{
	const audDynMixPatchNode * patchNode = m_ListMixPatches.GetHead();

	dmDisplayf("Active Patches (%u):", m_ListMixPatches.GetNumItems());
	while(patchNode)
	{
		const audDynMixPatch* patch = &patchNode->Data;

		if(patch)
		{
			dmDisplayf("  %s, Apply: %f", patch->GetName(), patch->GetApproachApplyFactor());
		}

		patchNode = patchNode->GetNext();
	}
	dmDisplayf("");
}

void audDynamicMixer::PrintDebug() const
{
	PrintScenes();
	PrintPatches();
}

void audDynamicMixer::DebugStopAllScenes()
{
	audSceneNode * sceneNode = audNorthAudioEngine::GetDynamicMixer().m_ListScenes.GetHead();

	while(sceneNode)
	{
		audScene* scene = &sceneNode->Data;
		
		if(scene)
		{
			scene->Stop();
		}

		sceneNode = sceneNode->GetNext();
	}
}

void audDynamicMixer::DebugStopNamedScenes()
{
	audSceneNode * sceneNode = audNorthAudioEngine::GetDynamicMixer().m_ListScenes.GetHead();

	while(sceneNode)
	{
		audScene* scene = &sceneNode->Data;

		if(scene && scene->GetHash() == atStringHash(sm_DebugSceneName))
		{
			scene->Stop();
		}

		sceneNode = sceneNode->GetNext();
	}
}

void audDynamicMixer::DrawDebugScenes()
{
	SYS_CS_SYNC(sm_DebugDrawLock);

	if(!g_DisplayScenes)
	{
		return;
	}

	PUSH_DEFAULT_SCREEN();
	//grcFont::GetCurrent().DrawCharBegin();		// set the font texture once in the outer code

	int linesDrawn = 0;

	AuditionSceneDrawMode drawMode = (AuditionSceneDrawMode)sm_SceneDrawMode;

	audNorthDebugDrawManager drawMgr(100.f, 50.f, 50.f, 720.f);
	drawMgr.SetOffsetY(sm_AuditionSceneDrawOffsetY);
	
	const audSceneNode * sceneNode = m_ListScenes.GetHead();

	while(sceneNode)
	{
		const audScene* scene = &sceneNode->Data;
	
		if(!(g_HideCodeScenes && !scene->m_Script) && !(g_HideScriptScenes && scene->m_Script))
		{
			linesDrawn += scene->DebugDraw(drawMgr, drawMode);
		}

		sceneNode = sceneNode->GetNext();
	}

	if(drawMode == kEverything || drawMode == kPatches)
	{
		char sectionHeader[128] = "Orphaned Patches";
		drawMgr.PushSection(sectionHeader);


		const audDynMixPatchNode * patchNode = m_ListMixPatches.GetHead();
		while(patchNode)
		{
			audDynMixPatch * patch = const_cast<audDynMixPatch*>(&patchNode->Data);
			if(patch->m_IsRemoving)
			{
				linesDrawn += patch->DebugDraw(drawMgr);
			}
			patchNode = patchNode->GetNext();
		}

		drawMgr.PopSection();
	}

	if(KEYBOARD.KeyDown(KEY_DOWN)) 
	{
		sm_AuditionSceneDrawOffsetY += 10.f;
	}
	if(KEYBOARD.KeyDown(KEY_UP)) 
	{
		sm_AuditionSceneDrawOffsetY -= 10.f;
	}
	if(KEYBOARD.KeyDown(KEY_ESCAPE))
	{
		sm_AuditionSceneDrawOffsetY = 0.f;
	}

	sm_AuditionSceneDrawOffsetY = Max(Min(sm_AuditionSceneDrawOffsetY, (linesDrawn*10.f - 50.f)), -570.f);
	
	//grcFont::GetCurrent().DrawCharEnd();		// and finally unset the font texture
	POP_DEFAULT_SCREEN();
}

#endif


