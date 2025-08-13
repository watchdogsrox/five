#include "AnimScenePlayCameraAnimEvent.h"

#include "AnimScenePlayCameraAnimEvent_parser.h"

// game includes
#include "animation/AnimScene/AnimScene.h"
#include "animation/AnimScene/AnimSceneManager.h"
#include "animation/AnimScene/entities/AnimSceneEntities.h"
#include "animation/AnimScene/events/AnimSceneEventInitialisers.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/syncedscene/SyncedSceneDirector.h"
#include "camera/animscene/AnimSceneDirector.h"
#include "camera/cutscene/AnimatedCamera.h"
#include "streaming/streaming.h"

#if __BANK
#include "animation/AnimScene/events/AnimScenePlayAnimEvent.h" // For editor settings
#endif // __BANK

ANIM_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CAnimScenePlaySyncedAnimEvent
void CAnimScenePlayCameraAnimEvent::OnInit(CAnimScene* pScene)
{
	m_flags.SetFlag( kRequiresPreloadUpdate | kRequiresEnter | kRequiresUpdate | kRequiresExit );

	//add accessor to parent
	if(pScene)
	{ 
		m_data->m_pSceneParentMat = &pScene->GetSceneMatrix(); 
	}

	// Initialise Helpers
	m_entity.OnInit(pScene);

	//Todo: Temporarily set the camera settings flag to use the scene origin
	//if the old flag was set. To be removed after a week so that
	//we can remove m_useSceneOrigin (it will be a duplicate of a flag)
	if(m_useSceneOrigin)
	{
		m_cameraSettings.BitSet().Set(ASCF_USE_SCENE_ORIGIN);
	}

	m_data->m_blendOutStart = GetEnd();
}
void CAnimScenePlayCameraAnimEvent::OnShutdown(CAnimScene* UNUSED_PARAM(pScene))
{
#if __BANK
	if(m_pSceneOffset)
	{
		m_pSceneOffset->SetParentMatrix(NULL); 
	}
	//Added so that the camera stops on shutdown. This stops all cameras, so
	//could have strange effect if multiple cameras are set at the same time
	camInterface::GetAnimSceneDirector().StopAnimatingCamera(); 
#endif
}

bool CAnimScenePlayCameraAnimEvent::OnUpdatePreload(CAnimScene* UNUSED_PARAM(pScene))
{
	s32 dictIndex = fwAnimManager::FindSlotFromHashKey(m_clip.GetClipDictionaryName()).Get();
	animAssertf(dictIndex!=-1, "dictionary %s does not exist!", m_clip.GetClipDictionaryName().GetCStr());

	if (dictIndex<0)
	{
		// asset doesn't exist, return true so we don't hold up the entire scene
		return true;
	}

	return m_data->m_requestHelper.RequestClipsByIndex(dictIndex);
}

void CAnimScenePlayCameraAnimEvent::OnEnter(CAnimScene* pScene, AnimSceneUpdatePhase phase)
{
	if (phase==kAnimSceneUpdatePreAi)
	{
		// grab the entity and assert it exists
		CAnimSceneCamera* pEnt = m_entity.GetEntity< CAnimSceneCamera >(ANIM_SCENE_ENTITY_CAMERA);
		animAssertf(pEnt, "CAnimScenePlayCameraAnimEvent : entity not found! scene: %s, entity: %s(%p)  (clip '%s' '%s')", CAnimSceneManager::GetSceneDescription(pScene).c_str(), m_entity.GetId().GetCStr(), m_entity.GetEntity(), m_clip.GetClipDictionaryName().GetCStr(), m_clip.GetClipName().GetCStr());

		if (!pEnt)
		{
			// no need to keep the dictionary around, the entity is missing
			m_data->m_requestHelper.ReleaseClips();
			return;
		}

		// assert the dictionary is loaded
		s32 dictIndex = fwAnimManager::FindSlotFromHashKey(m_clip.GetClipDictionaryName()).Get();
		animAssertf(dictIndex!=-1, "dictionary %s does not exist! From updating anim scene entity %s", m_clip.GetClipDictionaryName().GetCStr(), m_entity.GetId().GetCStr());

		if (dictIndex<0)
		{
			m_data->m_requestHelper.ReleaseClips();
			return;
		}

		// verify the clip exists
		if (!fwAnimManager::GetClipIfExistsByDictIndex(dictIndex, m_clip.GetClipName()))
		{
			animAssertf(0, "clip %s does not exist in the dictionary %s ! From updating anim scene entity %s", m_clip.GetClipName().GetCStr(), m_clip.GetClipDictionaryName().GetCStr(), m_entity.GetId().GetCStr());
			m_data->m_requestHelper.ReleaseClips();
			return;
		}

		bool bUseSceneOrigin = m_cameraSettings.BitSet().IsSet(ASCF_USE_SCENE_ORIGIN);
#if __BANK
		// If we are previewing in the editor
		if (pScene == g_AnimSceneManager->GetSceneEditor().GetScene())
		{
			bUseSceneOrigin = true;
		}
#endif //__BANK

		u32 BlendInDuration = (u32)(m_blendInDuration*1000.0f);

		if (bUseSceneOrigin || m_pSceneOffset)
		{
			Mat34V origin = pScene->GetSceneOrigin();

			if (m_pSceneOffset)
			{
#if __BANK
				if(m_data->m_pSceneParentMat)
				{
					m_pSceneOffset->SetParentMatrix(m_data->m_pSceneParentMat); 
				}
#endif
				Transform(origin, origin, m_pSceneOffset->GetMatrix());
			}			

			camInterface::GetAnimSceneDirector().AnimateCamera( m_clip.GetClipDictionaryName(), 
				*fwAnimManager::GetClipIfExistsByDictIndex(dictIndex, m_clip.GetClipName()), 
				MAT34V_TO_MATRIX34(origin), GetStart(), pScene->GetTime(), 0, BlendInDuration); 
		}
		else
		{
			Matrix34 origin = camInterface::GetDominantRenderedCamera()->GetFrame().GetWorldMatrix(); 


			camInterface::GetAnimSceneDirector().AnimateCamera( m_clip.GetClipDictionaryName(), 
				*fwAnimManager::GetClipIfExistsByDictIndex(dictIndex, m_clip.GetClipName()), 
				origin, GetStart(), pScene->GetTime(), 0, BlendInDuration); 
		}

		// we can release our request helper now. The task will take care of referencing the dictionary
		m_data->m_requestHelper.ReleaseClips();
	}	
}

void CAnimScenePlayCameraAnimEvent::OnUpdate(CAnimScene* pScene, AnimSceneUpdatePhase phase)
{
	//if (phase==kAnimSceneUpdatePreAi)
	if (phase==kAnimSceneUpdatePostMovement)
	{
		//!!TODO!! if (m_data->m_pTask)
		{
			bool bUseSceneOrigin = m_cameraSettings.BitSet().IsSet(ASCF_USE_SCENE_ORIGIN);

#if __BANK
			// If we are previewing in the editor
			if (pScene == g_AnimSceneManager->GetSceneEditor().GetScene())
			{
				bUseSceneOrigin = true;
			}
#endif //__BANK

			// if necessary, update the anim origin
			if ((bUseSceneOrigin || m_pSceneOffset))
			{
				Mat34V origin = pScene->GetSceneOrigin();

				if (m_pSceneOffset)
				{
#if __BANK
					if(m_data->m_pSceneParentMat)
					{
						m_pSceneOffset->SetParentMatrix(m_data->m_pSceneParentMat); 
					}
#endif
					Transform(origin, origin, m_pSceneOffset->GetMatrix());
				}

				animAssertf( GetStart() <= GetEnd(), "PlayCameraAnimEvent: The blend out duration (%f) must be shorter than the camera anim clip duration (start of event was after end, which is the start of the blend out). ",m_blendOutDuration); 

				camAnimatedCamera* cam = camInterface::GetAnimSceneDirector().GetCamera();

				if(cam)
				{
					cam->SetSceneMatrix(MAT34V_TO_MATRIX34(origin));
					cam->SetCurrentTime(pScene->GetTime()); 
				}
			}
		}
	}
}

void CAnimScenePlayCameraAnimEvent::OnExit(CAnimScene* UNUSED_PARAM(pScene), AnimSceneUpdatePhase phase)
{
	if (phase==kAnimSceneUpdatePreAi || phase==kAnimSceneShuttingDown)
	{
		// stop the scripted anim task on the entity
		CAnimSceneCamera* pEnt = m_entity.GetEntity< CAnimSceneCamera >(ANIM_SCENE_ENTITY_CAMERA);

		if (!pEnt)
		{
			return;
		}

		//Work like play anim events and exit at the start of a blendout, passing control over to the AnimSceneDirector
		if(m_blendOutDuration > 0.0f && !camInterface::GetAnimSceneDirector().IsInterpolating())
		{
			//Trigger the blend out now
			u32 blendOutDuration = (u32)((m_blendOutDuration) * 1000.0f);
			camInterface::GetAnimSceneDirector().BlendOutCameraAnimation(blendOutDuration); 
			//This ensures the AnimSceneDirector is aware it must update the camera's timestep and clean up when it's finished.
			camInterface::GetAnimSceneDirector().SetCleanUpCameraAfterBlendOut(true);
			return;
		}


		if(m_cameraSettings.BitSet().IsSet(ASCF_CATCHUP_CAM))
		{
			const camFrame& frame = camInterface::GetAnimSceneDirector().GetFrame();
			camInterface::GetGameplayDirector().CatchUpFromFrame(frame);
		}
		camInterface::GetAnimSceneDirector().StopAnimatingCamera(); 
	}
}

bool CAnimScenePlayCameraAnimEvent::Validate()
{
	float duration = GetEnd() - GetStart();
	float clipDuration = -1.0f;

	CAnimSceneClip clip = GetClip();
	s32 dictIndex = fwAnimManager::FindSlotFromHashKey(clip.GetClipDictionaryName()).Get();
	animAssertf(dictIndex!=-1, "dictionary %s does not exist! From validating camera anim event.", clip.GetClipDictionaryName().GetCStr());

	// grab the clip and force load it
	strRequest req;
	strLocalIndex slot = fwAnimManager::FindSlotFromHashKey(clip.GetClipDictionaryName().GetHash());
	req.Request(slot, fwAnimManager::GetStreamingModuleId(), 0);
	CStreaming::LoadAllRequestedObjects();

	const crClip* clipData = fwAnimManager::GetClipIfExistsByDictIndex(dictIndex, clip.GetClipName());
	if (!clipData)
	{
		animAssertf(0, "clip %s does not exist in the dictionary %s ! From updating anim scene entity %s", 
			clip.GetClipName().TryGetCStr(), clip.GetClipDictionaryName().GetCStr(), m_entity.GetId().GetCStr());
		return false;
	}
	clipDuration = clipData->GetDuration();
	if( clipDuration < duration - 1e-4f)
	{
		animWarningf("Validation of PlayCameraAnimEvent time failed (clip dictionary: %s, clip name: %s, clip duration: %f, event duration: %f).",
			clip.GetClipDictionaryName().TryGetCStr(),clip.GetClipName().TryGetCStr(),clipDuration, duration);
		return false;
	}
	else
	{
		return true;
	}
}

#if __BANK
//////////////////////////////////////////////////////////////////////////

void CAnimScenePlayCameraAnimEvent::OnPostAddWidgets(bkBank& bank)
{
	// set the callback for the clip selector
	m_clip.SetClipSelectedCallback(MakeFunctor(*this, &CAnimScenePlayCameraAnimEvent::OnClipChanged));

	bank.PushGroup("Scene offset");
	if (m_pSceneOffset)
	{
		// add a button to remove the scene offset
		PARSER.AddWidgets(bank, m_pSceneOffset);
		m_widgets->m_pSceneOffsetButton = bank.AddButton("Remove scene offset", datCallback(MFA1(CAnimScenePlayCameraAnimEvent::RemoveSceneOffset), this, (datBase*)&bank));
	}
	else
	{
		m_widgets->m_pSceneOffsetButton = bank.AddButton("Add scene offset", datCallback(MFA1(CAnimScenePlayCameraAnimEvent::AddSceneOffset), this, (datBase*)&bank));
	}
	bank.PopGroup();
}
//////////////////////////////////////////////////////////////////////////
void CAnimScenePlayCameraAnimEvent::OnClipChanged(atHashString clipDict, atHashString clip)
{
	// grab the clip from the anim manager
	const crClip* pClip = fwAnimManager::GetClipIfExistsByName(clipDict.GetCStr(), clip.GetCStr());

	if (pClip)
	{
		// update the duration appropriately
		SetEnd(GetStart() + GetRoundedTime(pClip->GetDuration()));
		NotifyRangeUpdated();
	}
}




//////////////////////////////////////////////////////////////////////////
void CAnimScenePlayCameraAnimEvent::OnRangeUpdated()
{
	// by default, ensure the event isn't longer than the anim length
	// minus the blend out duration.
	if (CAnimScenePlayAnimEvent::ms_editorSettings.m_clampEventLengthToAnim)
	{
		strLocalIndex slot = fwAnimManager::FindSlotFromHashKey(m_clip.GetClipDictionaryName().GetHash());

		if (slot.IsValid())
		{
			// grab the clip and force load it
			strRequest req;
			req.Request(slot, fwAnimManager::GetStreamingModuleId(), 0);
			CStreaming::LoadAllRequestedObjects();

			const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(slot.Get(), m_clip.GetClipName().GetHash());

			if (!pClip)
			{
				return;
			}

			// set the time which the blend out starts in the clip
			float clipEndTime = GetRoundedTime(pClip->ConvertPhaseToTime(1.0f - m_startPhase));
			float clipDuration = clipEndTime - GetStart();
			clipDuration -= m_blendOutDuration;
			SetBlendOutStart(clipDuration);//This is the offset from the start of the clip
			
			// Act like play anim events (new method)
			// don't allow the event to be longer than the clip
			clipEndTime -= m_blendOutDuration;
			SetEnd(GetStart() + clipEndTime);

			//get info on blended starts
			float clipStartTime = GetRoundedTime(pClip->ConvertPhaseToTime(m_startPhase));
			clipStartTime += m_blendInDuration;
			m_blendInEnd = clipStartTime;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimScenePlayCameraAnimEvent::AddSceneOffset(bkBank* pBank)
{
	if (!m_pSceneOffset && pBank)
	{
		bkGroup* pGroup = NULL;
		if (m_widgets->m_pSceneOffsetButton)
		{
			pGroup = (bkGroup*)m_widgets->m_pSceneOffsetButton->GetParent();
			m_widgets->m_pSceneOffsetButton->Destroy();
			m_widgets->m_pSceneOffsetButton = NULL;			
		}

		if(m_pSceneOffset == NULL)
		{
			m_pSceneOffset = rage_new CAnimSceneMatrix();
		}

		CAnimScene::SetCurrentAddWidgetsScene(GetParentList()->GetOwningScene());

		if(pGroup)
		{
			pBank->SetCurrentGroup(*pGroup);
		}

		PARSER.AddWidgets(*pBank, m_pSceneOffset);
		m_widgets->m_pSceneOffsetButton = pBank->AddButton("Remove scene offset", datCallback(MFA1(CAnimScenePlayCameraAnimEvent::RemoveSceneOffset), this, (datBase*)pBank));

		if(pGroup)
		{
			pBank->UnSetCurrentGroup(*pGroup);
		}

		CAnimScene::SetCurrentAddWidgetsScene(NULL);
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimScenePlayCameraAnimEvent::RemoveSceneOffset(bkBank* pBank)
{
	if (m_pSceneOffset && pBank)
	{
		bkGroup* pGroup = NULL;
		if (m_widgets->m_pSceneOffsetButton)
		{
			pGroup = (bkGroup*)m_widgets->m_pSceneOffsetButton->GetParent();
			m_widgets->m_pSceneOffsetButton->Destroy();
			m_widgets->m_pSceneOffsetButton = NULL;
		}

		delete m_pSceneOffset;
		m_pSceneOffset = NULL;

		if(pGroup)
		{
			while (pGroup->GetChild())
			{
				// remove all the scene offset group widgets
				pGroup->GetChild()->Destroy();
			}
		}

		if(pGroup)
		{
			pBank->SetCurrentGroup(*pGroup);
		}

		m_widgets->m_pSceneOffsetButton = pBank->AddButton("Add scene offset", datCallback(MFA1(CAnimScenePlayCameraAnimEvent::AddSceneOffset), this, (datBase*)pBank));

		if(pGroup)
		{
			pBank->UnSetCurrentGroup(*pGroup);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
#if __DEV
void CAnimScenePlayCameraAnimEvent::DebugRender(CAnimScene* pScene, debugPlayback::OnScreenOutput& mainOutput)
{
	// TODO - add more info
	CAnimSceneEvent::DebugRender(pScene, mainOutput);
}
#endif //__DEV

bool CAnimScenePlayCameraAnimEvent::InitForEditor(const CAnimSceneEventInitialiser* pInitialiser)
{
	const CAnimScenePlayCameraAnimEventInitialiser* pInit = CAnimSceneEventInitialiser::Cast<CAnimScenePlayCameraAnimEventInitialiser>(pInitialiser, this);
	if (pInit)
	{
		CAnimSceneEvent::InitForEditor(pInitialiser);
		m_entity.SetEntity(pInitialiser->m_pEntity);
		m_clip.SetClipName(pInit->m_clipSelector.GetSelectedClipName());
		m_clip.SetClipDictionaryName(pInit->m_clipSelector.GetSelectedClipDictionary());
		
		OnClipChanged(pInit->m_clipSelector.GetSelectedClipDictionary(), pInit->m_clipSelector.GetSelectedClipName());
		SetPreloadDuration(4.0f);
	}
	return true;
}

void CAnimScenePlayCameraAnimEvent::SetBlendOutStart(float time)
{
	m_data->m_blendOutStart = time;
}

float CAnimScenePlayCameraAnimEvent::GetBlendOutStart()
{
	return m_data->m_blendOutStart;
}

bool CAnimScenePlayCameraAnimEvent::GetEditorEventText(atString& textToDisplay, CAnimSceneEditor& editor)
{
	atHashString name = GetClip().GetClipName(); 

	if( name.GetHash()!=0)
	{
		textToDisplay = name.GetCStr();
		return true;
	}
	else
	{
		return CAnimSceneEvent::GetEditorEventText(textToDisplay, editor);
	}	
}

bool CAnimScenePlayCameraAnimEvent::GetEditorHoverText(atString& textToDisplay, CAnimSceneEditor& editor)
{
	atHashString name = GetClip().GetClipName();

	bool ret = CAnimSceneEvent::GetEditorHoverText(textToDisplay, editor);

	if( name.GetHash()!=0)
	{
		textToDisplay += atVarString(": %s/%s", GetClip().GetClipDictionaryName().GetCStr(), name.GetCStr()).c_str();
		return true;
	}
	else
	{
		return ret;
	}
}

#endif //__BANK
