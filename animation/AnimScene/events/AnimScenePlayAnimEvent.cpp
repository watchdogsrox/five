#include "AnimScenePlayAnimEvent.h"

#include "AnimScenePlayAnimEvent_Parser.h"

// game includes
#include "animation/AnimScene/entities/AnimSceneEntities.h"
#include "animation/AnimScene/events/AnimSceneEventInitialisers.h"
#include "Peds/PedIntelligence.h"
#include "streaming/streaming.h"
#include "task/Animation/TaskAnims.h"
#include "vehicles/train.h"
#include "vehicleAi/VehicleIntelligence.h"

ANIM_OPTIMISATIONS()

#if __BANK

CAnimScenePlayAnimEvent::EditorSettings CAnimScenePlayAnimEvent::ms_editorSettings;

#endif // __BANK


//////////////////////////////////////////////////////////////////////////
// CAnimScenePlaySyncedAnimEvent
void CAnimScenePlayAnimEvent::OnInit(CAnimScene* pScene)
{
	m_flags.SetFlag( kRequiresPreloadUpdate | kRequiresEnter | kRequiresUpdate | kRequiresExit );

	//add accessor to parent
	if(pScene)
	{ 
		m_data->m_pSceneParentMat = &pScene->GetSceneMatrix(); 
	}

	// Initialise Helpers
	m_entity.OnInit(pScene);

	m_startFullyIn = m_blendInDuration;
}
void CAnimScenePlayAnimEvent::OnShutdown(CAnimScene* UNUSED_PARAM(pScene))
{
#if __BANK
	if(m_pSceneOffset)
	{
		m_pSceneOffset->SetParentMatrix(NULL); 
	}
#endif
}

bool CAnimScenePlayAnimEvent::OnUpdatePreload(CAnimScene* UNUSED_PARAM(pScene))
{
	strLocalIndex dictIndex = fwAnimManager::FindSlotFromHashKey(m_clip.GetClipDictionaryName());
	animAssertf(dictIndex!=-1, "dictionary %s does not exist!", m_clip.GetClipDictionaryName().GetCStr());

	if (dictIndex.Get()<0)
	{
		// asset doesn't exist, return true so we don't hold up the entire scene
		return true;
	}

	return m_data->m_requestHelper.RequestClipsByIndex(dictIndex.Get());
}

void CAnimScenePlayAnimEvent::OnEnter(CAnimScene* pScene, AnimSceneUpdatePhase phase)
{
	if (phase==kAnimSceneUpdatePreAi)
	{
		// grab the entity and assert it exists
		CPhysical* pPhys = m_entity.GetPhysicalEntity();
		animAssertf(pPhys, "CAnimScenePlayAnimEvent : entity not found! scene: %s, entity: %s(%p)  (clip '%s' '%s')", CAnimSceneManager::GetSceneDescription(pScene).c_str(), m_entity.GetId().GetCStr(), m_entity.GetEntity(), m_clip.GetClipDictionaryName().GetCStr(), m_clip.GetClipName().GetCStr());

		if (!pPhys)
		{
			// no need to keep the dictionary around, the entity is missing
			m_data->m_requestHelper.ReleaseClips();
			return;
		}

		// assert the dictionary is loaded
		strLocalIndex dictIndex = fwAnimManager::FindSlotFromHashKey(m_clip.GetClipDictionaryName());
		animAssertf(dictIndex!=-1, "dictionary %s does not exist! From updating anim scene entity %s", m_clip.GetClipDictionaryName().GetCStr(), m_entity.GetId().GetCStr());

		if (dictIndex.Get()<0)
		{
			m_data->m_requestHelper.ReleaseClips();
			return;
		}

		// verify the clip exists
		if (!fwAnimManager::GetClipIfExistsByDictIndex(dictIndex.Get(), m_clip.GetClipName()))
		{
			animAssertf(0, "clip %s does not exist in the dictionary %s ! From updating anim scene entity %s", m_clip.GetClipName().GetCStr(), m_clip.GetClipDictionaryName().GetCStr(), m_entity.GetId().GetCStr());
			m_data->m_requestHelper.ReleaseClips();
			return;
		}

		eScriptedAnimFlagsBitSet animFlags = m_animFlags;
		// The anim should always be ended by the event exit, rather than ending on its own.
		animFlags.BitSet().Set(AF_HOLD_LAST_FRAME);

		bool bUseSceneOrigin = m_useSceneOrigin;

#if __BANK
		bool bForceInstantMoverBlend = false;
		// If we are previewing in the editor
		if (pScene == g_AnimSceneManager->GetSceneEditor().GetScene())
		{
			animFlags.BitSet().Set(AF_USE_MOVER_EXTRACTION);
			animFlags.BitSet().Set(AF_EXTRACT_INITIAL_OFFSET);
			bUseSceneOrigin = true;

			// HACK - if the entity is close to the world origin, force an instant blend on the mover
			// This stops debug entities from lerping from the world origin to the scene location.
			if (MagSquared(pPhys->GetMatrix().d()).Getf()<0.01f)
			{
				bForceInstantMoverBlend = true;
			}
		}
#endif //__BANK

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

			Vector3 m_vInitialPosition(VEC3V_TO_VECTOR3(origin.d()));
			Quaternion m_qInitialOrientation = QUATV_TO_QUATERNION(QuatVFromMat34V(origin));

			m_data->m_pTask = rage_new CTaskScriptedAnimation( m_clip.GetClipDictionaryName(), m_clip.GetClipName(), CTaskScriptedAnimation::kPriorityMid, m_filter, m_blendInDuration, m_blendOutDuration, -1, animFlags, m_startPhase, m_vInitialPosition, m_qInitialOrientation, false, false, m_ikFlags, false, false);
		}
		else
		{
			m_data->m_pTask = rage_new CTaskScriptedAnimation( m_clip.GetClipDictionaryName(), m_clip.GetClipName(), CTaskScriptedAnimation::kPriorityMid, m_filter, m_blendInDuration, m_blendOutDuration, -1, animFlags, m_startPhase, false, false, m_ikFlags, false, false);
		}

		static_cast<CTaskScriptedAnimation*>(m_data->m_pTask.Get())->SetOwningAnimScene(pScene->GetInstId(), m_entity.GetId());

		// if everythings good, start the scripted anim task on the entity
		if(pPhys->GetIsTypePed())
		{
			CPed * pPed = static_cast<CPed*>(pPhys);


			pPed->ApplyRagdollBlockingFlags(m_ragdollBlockingFlags); // need to apply ragdoll blocking flags manually for now, since the scripted anim task doesn't support them yet
			
			// Make sure any current anim task from this anim scene knows that it's getting aborted
			CTaskScriptedAnimation* pCurrentTask = static_cast<CTaskScriptedAnimation*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SCRIPTED_ANIMATION));
			if (pCurrentTask && pCurrentTask->GetOwningAnimScene() == pScene->GetInstId())
			{
				pCurrentTask->ExitNextUpdate();
			}

			pPed->GetPedIntelligence()->AddTaskAtPriority(m_data->m_pTask, PED_TASK_PRIORITY_PRIMARY, TRUE);

			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);
		}
		else if (pPhys->GetIsTypeObject())
		{
			CObject* pObj = static_cast<CObject*>(pPhys);

			// Make sure any current anim task from this anim scene knows that it's getting aborted
			CTaskScriptedAnimation* pCurrentTask = CTaskScriptedAnimation::FindObjectTask(pObj);
			if (pCurrentTask && pCurrentTask->GetOwningAnimScene() == pScene->GetInstId())
			{
				pCurrentTask->ExitNextUpdate();
			}

			CTaskScriptedAnimation::GiveTaskToObject(pObj, static_cast<CTaskScriptedAnimation*>(m_data->m_pTask.Get()));

			pObj->ForcePostCameraAnimUpdate(true, true);
		}
		else if (pPhys->GetIsTypeVehicle())
		{
			CVehicle* pVeh = static_cast<CVehicle*>(pPhys);

			// Make sure any current anim task from this anim scene knows that it's getting aborted
			CTaskScriptedAnimation* pCurrentTask = static_cast<CTaskScriptedAnimation*>(pVeh->GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_SECONDARY, CTaskTypes::TASK_SCRIPTED_ANIMATION));
			if (pCurrentTask && pCurrentTask->GetOwningAnimScene() == pScene->GetInstId())
			{
				pCurrentTask->ExitNextUpdate();
			}

			pVeh->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->SetTask(m_data->m_pTask, VEHICLE_TASK_SECONDARY_ANIM);
			pVeh->ForcePostCameraAnimUpdate(true, true);
		}

		// we can release our request helper now. The task will take care of referencing the dictionary
		m_data->m_requestHelper.ReleaseClips();
	}	
}

void CAnimScenePlayAnimEvent::OnUpdate(CAnimScene* pScene, AnimSceneUpdatePhase phase)
{
	if (phase==kAnimSceneUpdatePostMovement)
	{
		if (m_data->m_pTask)
		{
			animAssert(m_data->m_pTask->GetTaskType()==CTaskTypes::TASK_SCRIPTED_ANIMATION);
			CTaskScriptedAnimation* pAnimTask = static_cast<CTaskScriptedAnimation*>(m_data->m_pTask.Get());

			bool bUseSceneOrigin = m_useSceneOrigin;

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
#if __BANK
				if(m_pSceneOffset && m_data->m_pSceneParentMat)
				{
					m_pSceneOffset->SetParentMatrix(m_data->m_pSceneParentMat); 
				}
#endif
				Mat34V origin;
				CalcAnimOrigin(pScene, origin);
				Vector3 vInitialPosition(VEC3V_TO_VECTOR3(origin.d()));
				Quaternion qInitialOrientation = QUATV_TO_QUATERNION(QuatVFromMat34V(origin));

				pAnimTask->SetOrigin(qInitialOrientation, vInitialPosition);
			}
		}
	}
	else if (phase == kAnimSceneUpdatePreAnim )
	{
		CTaskScriptedAnimation* pAnimTask = static_cast<CTaskScriptedAnimation*>(m_data->m_pTask.Get());

		if (pAnimTask && pAnimTask->GetState()>CTaskScriptedAnimation::State_Start && !pAnimTask->GetIsFlagSet(aiTaskFlags::IsAborted))
		{
			// update the time on the clip
			// the rate will take care of getting us to the correct time
			float rate = pScene->GetRate();
			if (pScene->GetPaused())
			{
				rate = 0.0f;
			}
			pAnimTask->SetTime(Max((pScene->GetTime() - GetStart()) - (fwTimer::GetTimeStep()*rate), 0.0f), CTaskScriptedAnimation::kPriorityMid);
			pAnimTask->SetRate(rate, CTaskScriptedAnimation::kPriorityMid);
		}
	}
}

void CAnimScenePlayAnimEvent::OnExit(CAnimScene* pScene, AnimSceneUpdatePhase phase)
{
	if (phase==kAnimSceneUpdatePreAi || phase==kAnimSceneShuttingDown)
	{
		// stop the scripted anim task on the entity
		CPhysical* pPhys = m_entity.GetPhysicalEntity();

		if (!pPhys)
		{
			return;
		}

		CTaskScriptedAnimation* pTask = NULL;

		if (pPhys->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(pPhys);

			pPed->ClearRagdollBlockingFlags(m_ragdollBlockingFlags);
			pTask = static_cast<CTaskScriptedAnimation*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SCRIPTED_ANIMATION));
			if (pTask && pTask == m_data->m_pTask)
			{
				if (m_blendOutDuration>0.0f && pTask->IsActive(CTaskScriptedAnimation::kPriorityMid))
					pTask->SetRate(pScene->GetRate(), CTaskScriptedAnimation::kPriorityMid);

				pTask->ExitNextUpdate();
			}
		}
		else if (pPhys->GetIsTypeVehicle())
		{
			CVehicle* pVeh = static_cast<CVehicle*>(pPhys);

			if (pVeh->GetIntelligence()->GetTaskManager())
			{
				pTask = static_cast<CTaskScriptedAnimation*>(pVeh->GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_SECONDARY, CTaskTypes::TASK_SCRIPTED_ANIMATION));

				if (pTask && pTask == m_data->m_pTask)
				{
					pTask->ExitNextUpdate();
					if (m_blendOutDuration>0.0f && pTask->IsActive(CTaskScriptedAnimation::kPriorityMid))
						pTask->SetRate(pScene->GetRate(), CTaskScriptedAnimation::kPriorityMid);
					pVeh->GetIntelligence()->GetTaskManager()->ClearTask(VEHICLE_TASK_TREE_SECONDARY, VEHICLE_TASK_SECONDARY_ANIM);
				}
			}

			pVeh->PlaceOnRoadAdjust();
		}
		else if (pPhys->GetIsTypeObject())
		{
			CObject* pObj = static_cast<CObject*>(pPhys);
			pTask = static_cast<CTaskScriptedAnimation*>(pObj->GetObjectIntelligence() ? pObj->GetObjectIntelligence()->FindTaskSecondaryByType(CTaskTypes::TASK_SCRIPTED_ANIMATION) : NULL);
			if (pTask && pTask == m_data->m_pTask)
			{
				pTask->ExitNextUpdate();
				if (m_blendOutDuration>0.0f && pTask->IsActive(CTaskScriptedAnimation::kPriorityMid))
					pTask->SetRate(pScene->GetRate(), CTaskScriptedAnimation::kPriorityMid);
				pObj->SetTask(NULL, CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY, CObjectIntelligence::OBJECT_TASK_SECONDARY_ANIM);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// helper methods

void CAnimScenePlayAnimEvent::CalcAnimOrigin(CAnimScene* pScene, Mat34V_InOut origin)
{
	//If attached to an object, should be called post movement update!
	origin = pScene->GetSceneOrigin();

	if (m_pSceneOffset)
	{
		Transform(origin, origin, m_pSceneOffset->GetMatrix());
	}
}

//////////////////////////////////////////////////////////////////////////

bool CAnimScenePlayAnimEvent::IsEntityOptional() const
{
	const CAnimSceneEntity* pEnt = m_entity.GetEntity();

	if (pEnt)
	{
		switch (pEnt->GetType())
		{
		case ANIM_SCENE_ENTITY_PED:			return static_cast<const CAnimScenePed*>(pEnt)->IsOptional();
		case ANIM_SCENE_ENTITY_OBJECT:		return static_cast<const CAnimSceneObject*>(pEnt)->IsOptional();
		case ANIM_SCENE_ENTITY_VEHICLE:		return static_cast<const CAnimSceneVehicle*>(pEnt)->IsOptional();
		default:							return false;
		}
	}
	return false;
}

bool CAnimScenePlayAnimEvent::Validate()
{
	float duration = GetEnd() - GetStart();
	float clipDuration = -1.0f;

	CAnimSceneClip clip = GetClip();
	s32 dictIndex = fwAnimManager::FindSlotFromHashKey(clip.GetClipDictionaryName()).Get();
	animAssertf(dictIndex!=-1, "dictionary %s does not exist! From validating anim event.", clip.GetClipDictionaryName().GetCStr());

	// grab the clip and force load it
	strRequest req;
	strLocalIndex slot = fwAnimManager::FindSlotFromHashKey(m_clip.GetClipDictionaryName().GetHash());
	if (!req.Request(slot, fwAnimManager::GetStreamingModuleId(), 0))
	{
		CStreaming::LoadAllRequestedObjects();
	}

	const crClip* clipData = fwAnimManager::GetClipIfExistsByDictIndex(dictIndex, clip.GetClipName());
	if(!clipData)
	{
		animWarningf("Couldn't load clip data (clip dict %s, clip %s).",
			clip.GetClipDictionaryName().GetCStr(),clip.GetClipName().TryGetCStr());
		return false;
	}

	clipDuration = clipData->GetDuration();
	if( clipDuration < duration  - 1e-4f)
	{
		animWarningf("Validation of PlayCameraAnimEvent time failed (clip dictionary: %s, clip name: %s, clip duration: %f, event duration: %f).",
			clip.GetClipDictionaryName().GetCStr(),clip.GetClipName().TryGetCStr(),clipDuration, duration);
		return false;
	}
	else
	{
		return true;
	}
}

//////////////////////////////////////////////////////////////////////////
///BANK
//////////////////////////////////////////////////////////////////////////
#if __BANK

void CAnimScenePlayAnimEvent::CalcMoverOffset(CAnimScene* ASSERT_ONLY(pScene), Mat34V_InOut mat, float eventRelativeTime)
{
	// force load the dictionary if necessary

	strLocalIndex animStreamingIndex = fwAnimManager::FindSlotFromHashKey(m_clip.GetClipDictionaryName().GetHash());
	//first check if it's been loaded already
	if (animStreamingIndex.IsValid() && !CStreaming::HasObjectLoaded(animStreamingIndex,fwAnimManager::GetStreamingModuleId()))
	{
		// Is the associated animation dictionary in the image?
		if (CStreaming::IsObjectInImage(animStreamingIndex,fwAnimManager::GetStreamingModuleId()))
		{
			// Try and stream in the animation dictionary
			CStreaming::RequestObject(animStreamingIndex, fwAnimManager::GetStreamingModuleId(), STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
			CStreaming::LoadAllRequestedObjects();
		}

		// if we still haven't loaded the dictionary, bail.
		if (!CStreaming::HasObjectLoaded(animStreamingIndex,fwAnimManager::GetStreamingModuleId()))
		{
			animAssertf(0, "Unable to stream anim scene dictionary '%s' (whilst attempting to calculate entity metadata for entity '%s' in anim scene '%s')", m_clip.GetClipDictionaryName().GetCStr(), m_entity.GetId().GetCStr(), pScene->GetName().GetCStr());
			return;
		}
	}

	const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(animStreamingIndex.Get(), m_clip.GetClipName().GetHash());

	if (!pClip)
	{
		return;
	}

	Mat34V origin(V_IDENTITY);
	if (m_pSceneOffset)
	{
		Transform(origin, origin, m_pSceneOffset->GetMatrix());
	}

	if (m_animFlags.BitSet().IsSet(AF_EXTRACT_INITIAL_OFFSET))
	{
		// apply the initial offset from the clip
		fwAnimHelpers::ApplyInitialOffsetFromClip(*pClip, RC_MATRIX34(origin));
	}

	// Get the calculated mover track offset from phase 0.0 to the new phase
	float clipPhase = m_startPhase + pClip->ConvertTimeToPhase(eventRelativeTime);

	Mat34V deltaMatrix(V_IDENTITY);

	if (clipPhase>0.0f)
	{
		fwAnimHelpers::GetMoverTrackMatrixDelta(*pClip, 0.f, clipPhase, RC_MATRIX34(deltaMatrix));
	}

	g_ClipDictionaryStore.ClearRequiredFlag(animStreamingIndex.Get(), STRFLAG_DONTDELETE);

	// Transform by the mover offset
	Transform(mat, origin, deltaMatrix);
}

//////////////////////////////////////////////////////////////////////////

void CAnimScenePlayAnimEvent::OnPostAddWidgets(bkBank& bank)
{
	// set the callback for the clip selector
	m_clip.SetClipSelectedCallback(MakeFunctor(*this, &CAnimScenePlayAnimEvent::OnClipChanged));

	bank.PushGroup("Scene offset");
	if (m_pSceneOffset)
	{
		// add a button to remove the scene offset
		PARSER.AddWidgets(bank, m_pSceneOffset);
		m_widgets->m_pSceneOffsetButton = bank.AddButton("Remove scene offset", datCallback(MFA1(CAnimScenePlayAnimEvent::RemoveSceneOffset), this, (datBase*)&bank));
	}
	else
	{
		m_widgets->m_pSceneOffsetButton = bank.AddButton("Add scene offset", datCallback(MFA1(CAnimScenePlayAnimEvent::AddSceneOffset), this, (datBase*)&bank));
	}
	bank.PopGroup();
}
//////////////////////////////////////////////////////////////////////////
void CAnimScenePlayAnimEvent::OnClipChanged(atHashString clipDict, atHashString clip)
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
void CAnimScenePlayAnimEvent::OnRangeUpdated()
{
	// by default, ensure the event isn't longer than the anim length
	// minus the blend out duration.
	if (ms_editorSettings.m_clampEventLengthToAnim)
	{
		strLocalIndex slot = fwAnimManager::FindSlotFromHashKey(m_clip.GetClipDictionaryName().GetHash());

		if (slot.IsValid())
		{

			// grab the clip and force load it
			strRequest req;

			if (!req.Request(slot, fwAnimManager::GetStreamingModuleId(), 0))
			{
				CStreaming::LoadAllRequestedObjects();
			}		

			const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(slot.Get(), m_clip.GetClipName().GetHash());

			if (!pClip)
			{
				return;
			}

			// don't allow the event to be longer than the clip
			float clipEndTime = GetRoundedTime(pClip->ConvertPhaseToTime(1.0f - m_startPhase));
			
			// if the anim isn't set to loop, we also need to make sure we finish blending out before we hit the end
			if (!m_animFlags.BitSet().IsSet(AF_LOOPING))
			{
				//Why? Because although it keeps playing the time is set shorter so another anim can play and blend in
				clipEndTime -= m_blendOutDuration;
			}
			SetEnd(GetStart() + clipEndTime);

			//get info on blended starts
			float clipStartTime = GetRoundedTime(pClip->ConvertPhaseToTime(m_startPhase));
			clipStartTime += m_blendInDuration;
			m_startFullyIn = clipStartTime;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimScenePlayAnimEvent::AddSceneOffset(bkBank* pBank)
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
		m_widgets->m_pSceneOffsetButton = pBank->AddButton("Remove scene offset", datCallback(MFA1(CAnimScenePlayAnimEvent::RemoveSceneOffset), this, (datBase*)pBank));
		
		if(pGroup)
		{
			pBank->UnSetCurrentGroup(*pGroup);
		}
		
		CAnimScene::SetCurrentAddWidgetsScene(NULL);
	}
}

//////////////////////////////////////////////////////////////////////////

void CAnimScenePlayAnimEvent::RemoveSceneOffset(bkBank* pBank)
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
			
		m_widgets->m_pSceneOffsetButton = pBank->AddButton("Add scene offset", datCallback(MFA1(CAnimScenePlayAnimEvent::AddSceneOffset), this, (datBase*)pBank));
		
		if(pGroup)
		{
			pBank->UnSetCurrentGroup(*pGroup);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
#if __DEV
void CAnimScenePlayAnimEvent::DebugRender(CAnimScene* pScene, debugPlayback::OnScreenOutput& mainOutput)
{
	// TODO - add more info
	CAnimSceneEvent::DebugRender(pScene, mainOutput);
}
#endif //__DEV
bool CAnimScenePlayAnimEvent::InitForEditor(const CAnimSceneEventInitialiser* pInitialiser)
{	
	if (pInitialiser)
	{
		CAnimSceneEvent::InitForEditor(pInitialiser);

		const CAnimScenePlayAnimEventInitialiser* pAnimEventInitialiser = dynamic_cast<const CAnimScenePlayAnimEventInitialiser*>(pInitialiser);
		
		const CDebugClipSelector& selector = pAnimEventInitialiser->m_clipSelector;
		if(!(selector.GetClipContext() == kClipContextClipDictionary))
		{
			animAssertf(0,"Clip context must be set to Clip Dictionary.");
			return false;
		}		
		const char * clipName = selector.GetSelectedClipName();
		const char * clipDict = selector.GetSelectedClipDictionary();
		if(!clipName || !clipDict)
		{
			return false;
		}
		m_entity.SetEntity(pAnimEventInitialiser->m_pEntity);
		m_clip.SetClipName(clipName);
		m_clip.SetClipDictionaryName(clipDict);
		m_animFlags.BitSet().Set(AF_NOT_INTERRUPTABLE);
		m_animFlags.BitSet().Set(AF_USE_MOVER_EXTRACTION);
		m_animFlags.BitSet().Set(AF_EXTRACT_INITIAL_OFFSET);

		OnClipChanged(clipDict, clipName);
		SetPreloadDuration(4.0f);
	}	
	return true;
}



// display the name of the clip
bool CAnimScenePlayAnimEvent::GetEditorEventText(atString& textToDisplay, CAnimSceneEditor& editor)
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

// display the dictionary / clip name after the frame range.
bool CAnimScenePlayAnimEvent::GetEditorHoverText(atString& textToDisplay, CAnimSceneEditor& editor)
{
	atHashString name = GetClip().GetClipName();

	bool ret = CAnimSceneEvent::GetEditorHoverText(textToDisplay, editor);

	if( name.GetHash()!=0)
	{
		if (ms_editorSettings.m_displayDictionaryNames)
		{
			textToDisplay += atVarString(": %s/%s", GetClip().GetClipDictionaryName().GetCStr(), name.GetCStr()).c_str();
		}
		else
		{
			textToDisplay += atVarString(": %s",  name.GetCStr()).c_str();
		}
		
		return true;
	}
	else
	{
		return ret;
	}
}

void CAnimScenePlayAnimEvent::GetEditorColor(Color32& color, CAnimSceneEditor& editor, int colourId)
{
	
	if(colourId % 2 == 0)
	{
		color = editor.GetColor(CAnimSceneEditor::kPlayAnimEventColor);
	}
	else
	{
		color = editor.GetColor(CAnimSceneEditor::kPlayAnimEventColorAlt);
	}
}

float CAnimScenePlayAnimEvent::GetStartFullBlendInTime()
{
	return GetStart() + m_startFullyIn;
}



#endif //__BANK
