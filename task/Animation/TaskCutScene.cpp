// Class header
#include "Task/Animation/TaskCutScene.h"

// Rage headers
#include "audiohardware/device.h"
#include "audiohardware/driver.h"
#include "creature/componentextradofs.h"

// Framework headers
#include "fwscene/stores/framefilterdictionarystore.h"

// Game headers
#include "animation/MoveObject.h"
#include "animation/MovePed.h"
#include "animation/MoveVehicle.h"
#include "animation/EventTags.h"
#include "Cutscene/cutscene_channel.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "Cutscene/AnimatedModelEntity.h"
#include "Objects/object.h"
#include "peds/ped.h"
#include "vehicles/vehicle.h"
#include "Task/System/TaskHelpers.h"
#include "scene/Physical.h"
#include "script/script.h"
#include "script/script_channel.h"
#include "vehicleAI/VehicleAILodManager.h"
#include "vehicles/Metadata/VehicleSeatInfo.h"

AI_OPTIMISATIONS()

float CTaskCutScene::ms_CutsceneScopeDistMultiplierOverride = 1.0f;

CTaskCutScene::CTaskCutScene(const crClip* pClip,  const Matrix34& SceneOrigin, float phase, float cutsceneTimeAtStart)
: m_bExitNextUpdate(false)
, m_bIsWarpFrame(true)
, m_fVehicleLightingScalar(0.0f)
, m_fOriginalHairScale(0.0f)
, m_bPreRender(false)
, m_bPreserveHairScale(false)
, m_bInstantHairScaleUpdate(false)
{
	if(pClip != NULL)
	{
		pClip->AddRef(); 

		m_bPreRender = pClip->HasDof(kTrackFacialTinting, 0);
	}

	m_pClip = pClip; 
	SetAnimOrigin(SceneOrigin); 
	m_fPhase = phase; 
	m_fLastPhase = m_fPhase;
	m_BlendOutPhase = 1.0f; 
	m_BlendOutDuration = 0.0f; 
	m_HaveEventTag = false;
	m_fCutsceneTimeAtStart = cutsceneTimeAtStart;

#if __DEV
	m_bApplyMoverTrackFixup = true; 
#endif
	SetInternalTaskType(CTaskTypes::TASK_CUTSCENE);
}

CTaskCutScene::~CTaskCutScene()
{
	if(m_pClip != NULL)
	{
		m_pClip->Release(); 
		m_pClip = NULL; 
	}
}

void CTaskCutScene::SetPhase(float fPhase)
{
	CutSceneManager* pManager = CutSceneManager::GetInstance();
	// Only clamp the phase on skipped
	if (pManager && pManager->WasSkipped())
	{
		fPhase = Min(fPhase, m_BlendOutPhase);
	}

	if(m_networkHelper.IsNetworkActive() && !GetPhysical()->m_nDEflags.bFrozen && !fwTimer::IsGamePaused())
	{
		m_networkHelper.SetFloat(ms_Phase, fPhase); 
	}

	if (m_pClip)
	{
		const float time					= m_pClip->ConvertPhaseToTime(fPhase);
		const float timeOnPreviousUpdate	= m_pClip->ConvertPhaseToTime(m_fPhase);
		m_bIsWarpFrame = m_bIsWarpFrame || m_pClip->CalcBlockPassed(timeOnPreviousUpdate, time);

		// make sure our local origin is up to date
		if (pManager)
		{
			s32 concatSection = pManager->GetConcatSectionForTime(m_fCutsceneTimeAtStart + time);
			if (pManager->IsConcatSectionValidForPlayBack(concatSection))
				pManager->GetSceneOrientationMatrix(concatSection, m_AnimOrigin);
		}
	}

	m_fLastPhase = m_bIsWarpFrame ? m_fPhase : fPhase;
	m_fPhase = fPhase; 
}

void CTaskCutScene::SetClip(const crClip* pClip, const Matrix34& SceneMat, float cutsceneTimeAtStart)
{
	if(m_bPreRender)
	{
		PEDDAMAGEMANAGER.AddPedDamageDecal(GetPed(), kDamageZoneHead, Vector2(0.5f, 0.5f), 0.0f, 1.0f, atHashWithStringBank("blushing"), false, 0, 0.0f, 0.0f);

		m_bPreRender = false;
	}

	SetAnimOrigin(SceneMat);
	m_fCutsceneTimeAtStart = cutsceneTimeAtStart;

	if(m_networkHelper.IsNetworkActive() && !GetPhysical()->m_nDEflags.bFrozen)
	{
		m_networkHelper.SetClip(pClip,ms_Clip);
	}
	
	if(pClip != NULL)
	{
		pClip->AddRef(); 

		m_bPreRender = pClip->HasDof(kTrackFacialTinting, 0);
	}
	
	if(m_pClip != NULL)
	{
		m_pClip->Release(); 
	}

	m_pClip = pClip; 

	SetBlendOutTag(m_pClip);

	m_bIsWarpFrame = true;
}



bool CTaskCutScene::ProcessPostPreRender()
{
	TUNE_GROUP_BOOL(CUTSCENE_BLUSH, bOverride, false);
	TUNE_GROUP_FLOAT(CUTSCENE_BLUSH, fOverrideValue, 0.0f, 0.0f, 1.0f, 0.01f);

	CPed *pPed = GetPed();
	if(pPed)
	{
		crCreature *pCreature = pPed->GetCreature();
		if(taskVerifyf(pCreature, "Ped %p %s has no creature!", pPed, pPed->GetModelName()))
		{
			crCreatureComponentExtraDofs *pCreatureComponentExtraDofs = pCreature->FindComponent< crCreatureComponentExtraDofs >();
			if(taskVerifyf(pCreatureComponentExtraDofs, "Ped %p %s has no creature component extra dofs!", pPed, pPed->GetModelName()))
			{
				crFrame &poseFrame = pCreatureComponentExtraDofs->GetPoseFrame();
				if(poseFrame.HasDof(kTrackFacialTinting, 0))
				{
					float fValue = 0.0f;
					if(poseFrame.GetFloat(kTrackFacialTinting, 0, fValue))
					{
						float alpha = bOverride ? fOverrideValue : fValue;
						Assertf(alpha <= 1.0f, "kTrackFacialTinting Value '%f' is greater than 1.0)", alpha);

						if(alpha < 0.0f || alpha > 0.001f) // don't request invisible blits (-1 is a valid request for a random alpha based on tuning values)
						{
							PEDDAMAGEMANAGER.AddPedDamageDecal(pPed, kDamageZoneHead, Vector2(0.5f, 0.56f), 0.0f, 1.0f, atHashWithStringBank("cs_flush_anger"), false, 0, alpha, 0.0f);
						}
					}
				}
			}
		}
	}

	return true;
}

CTask::FSM_Return CTaskCutScene::ProcessPreFSM()
{
	if(m_bPreRender)
	{
		GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostPreRender, true);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskCutScene::UpdateFSM( const s32 iState, const FSM_Event iEvent)
{

	DIAG_CONTEXT_MESSAGE("Cutscene task update: %s, entity:%s", CutSceneManager::GetInstance() ?  CutSceneManager::GetInstance()->GetCutsceneName() : "Unknown cutscene", GetEntity() ? GetEntity()->GetModelName() : "No Entity!");

	FSM_Begin
		// Entry point
		FSM_State(State_Start) 
		FSM_OnUpdate
		return Start_OnUpdate();

	// Play an animation
	FSM_State(State_RunningScene)
		FSM_OnUpdate
		return RunningScene_OnUpdate();

	FSM_State(State_Exit)
		FSM_OnUpdate
		return FSM_Quit; 
	FSM_End
}

void CTaskCutScene::SetFilter(const atHashString& filter)
{
	if (m_networkHelper.IsNetworkActive())
	{
		m_FilterHash = filter;
		crFrameFilter *frameFilter = g_FrameFilterDictionaryStore.FindFrameFilter(fwMvFilterId(m_FilterHash));
		if(frameFilter)
		{
			m_networkHelper.SetFilter(frameFilter, ms_Filter);
		}
	}
}

#if __BANK
void CutsceneDebugf3(size_t addr, const char* sym, size_t displacement)
{
	cutsceneDebugf3("%8" SIZETFMT "x - %s+%" SIZETFMT "x", addr, sym, displacement);
}
#endif // __BANK

bool CTaskCutScene::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
#if __BANK
	if(iPriority == ABORT_PRIORITY_IMMEDIATE && !m_bExitNextUpdate)
	{
		sysStack::PrintStackTrace(CutsceneDebugf3);
		cutsceneDebugf3("Aborting for event: %s", pEvent ? ((CEvent*)pEvent)->GetName() : "none");
	}
#endif // __BANK

	if(iPriority == ABORT_PRIORITY_IMMEDIATE || m_bExitNextUpdate)
	{
		return CTask::ShouldAbort(iPriority, pEvent);
	}

	return false; 
}

bool CTaskCutScene::WillFinishThisPhase(float Phase)
{
	if(m_HaveEventTag)
	{
		if(Phase > 0.0f && Phase >= m_BlendOutPhase)
		{
			return true; 
		}
	}
	return false; 
}

#if !__FINAL

void CTaskCutScene::Debug() const
{
#if DEBUG_DRAW
	//Grab the ped.
	const CPed* pPed = GetPed();

	//Create the buffer.
	char buf[128];

	//Keep track of the line.
	int iLine = 0;

	//Calculate the text position.
	Vector3 vTextPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + (ZAXIS * 0.5f);

	if(pPed)
	{
		crCreature *pCreature = pPed->GetCreature();
		if(taskVerifyf(pCreature, "Ped %p %s has no creature!", pPed, pPed->GetModelName()))
		{
			crCreatureComponentExtraDofs *pCreatureComponentExtraDofs = pCreature->FindComponent< crCreatureComponentExtraDofs >();
			if(taskVerifyf(pCreatureComponentExtraDofs, "Ped %p %s has no creature component extra dofs!", pPed, pPed->GetModelName()))
			{
				crFrame &poseFrame = pCreatureComponentExtraDofs->GetPoseFrame();
				if(poseFrame.HasDof(kTrackFacialTinting, 0))
				{
					float fValue = 0.0f;
					if(poseFrame.GetFloat(kTrackFacialTinting, 0, fValue))
					{
						formatf(buf, "Facial Blush: %0.3f", fValue);
						grcDebugDraw::Text(vTextPosition, Color_blue, 0, iLine++ * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
					}
				}
			}
		}
	}
#endif

	//Call the base class version.
	CTask::Debug();
}

const char * CTaskCutScene::GetStaticStateName( s32 iState )
{
	Assert(iState>=0&&iState<=State_Exit);
	static const char* aPreviewMoveNetworkStateNames[] = 
	{
		"State_Start",
		"State_RunningScene",
		"State_Exit"
	};

	return aPreviewMoveNetworkStateNames[iState];
}
#endif

void CTaskCutScene::CleanUp()
{
	DIAG_CONTEXT_MESSAGE("Cutscene task cleanup: %s, entity:%s", CutSceneManager::GetInstance() ?  CutSceneManager::GetInstance()->GetCutsceneName() : "Unknown cutscene", GetEntity() ? GetEntity()->GetModelName() : "No Entity");

	RemoveMoveNetworkPlayerForEntityType(GetPhysical()); 
	// Displayf("CTaskCutScene::CleanUp frame - %d", fwTimer::GetFrameCount()); 
	if(m_pClip != NULL)
	{
		m_pClip->Release(); 
		m_pClip = NULL; 
	}
	
	if(GetPhysical() && GetPhysical()->GetIsTypeVehicle())
	{
		GetVehicle()->m_nVehicleFlags.bUseCutsceneWheelCompression = false;
	}

	if(m_bPreRender)
	{
		PEDDAMAGEMANAGER.AddPedDamageDecal(GetPed(), kDamageZoneHead, Vector2(0.5f, 0.5f), 0.0f, 1.0f, atHashWithStringBank("blushing"), false, 0, 0.0f, 0.0f);

		m_bPreRender = false;
	}
}

void CTaskCutScene::SetBlendOutTag(const crClip* pClip)
{
	if(pClip)		
	{
		const crTag* pTag = CClipEventTags::FindFirstEventTag(pClip, CClipEventTags::TagSyncBlendOut); 

		if(pTag)
		{
			m_BlendOutPhase = pTag->GetStart(); 

			m_BlendOutDuration = pClip->ConvertPhaseToTime(pTag->GetEnd()) - pClip->ConvertPhaseToTime(pTag->GetStart()); 

			m_HaveEventTag = true; 
		}
	}
}


CTask::FSM_Return CTaskCutScene::Start_OnUpdate()
{
	// Note Start_OnUpdate gets called once on cutscene start 
	// RunningScene_OnUpdate gets called the same frame and every frame there after.
	CPhysical* pEnt = GetPhysical(); 
	if(pEnt && pEnt->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pEnt); 
		if(pPed)
		{
			pPed->ResetStandData();
			pPed->GetIkManager().ResetAllSolvers();
			if(m_bInstantHairScaleUpdate)
			{
				pPed->SetHairScaleLerp(false);
			}
		}
	}

	if((!m_networkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskCutScene)) ||  (!CreateMoveNetworkPlayerForEntityType(GetPhysical())) )
	{
		return FSM_Quit; 
	}

	// clip should be valid unless the cutscene was skipped or exited (e.g paused then a save game loaded)
	animAssertf(m_pClip || CutSceneManager::GetInstance()->WasSkipped() || m_bExitNextUpdate, "Model %s has no clip during scene %s",  pEnt->GetModelName(), CutSceneManager::GetInstance()->GetCutsceneName() );

	if (m_pClip)
	{
		m_networkHelper.SetFloat(ms_Phase, m_fPhase); 
		m_networkHelper.SetFloat(ms_Rate,0.0f); 
		m_networkHelper.SetClip(m_pClip,ms_Clip); 
		
		SetBlendOutTag(m_pClip); 

		GetPhysical()->GetPortalTracker()->RequestRescanNextUpdate();

		GetPhysical()->GetAnimDirector()->RequestUpdatePostCamera();
		
		SetState(State_RunningScene);
	}
	else
	{
		//animAssertf(0, "Task cutsceme scene: Couldn't find clip - Dict: %s, clip: %s", m_dictionary.GetCStr(), m_clip.GetCStr());
		return FSM_Quit;
	}


	return FSM_Continue;
}

CTask::FSM_Return CTaskCutScene::RunningScene_OnUpdate()
{
	// Note Start_OnUpdate gets called once on cutscene start 
	// RunningScene_OnUpdate gets called the same frame and every frame there after.

	CPhysical* pEnt = GetPhysical(); 
	if(pEnt && pEnt->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pEnt); 		
		if(pPed)
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_UsingMoverExtraction, true);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableGroundAttachment, true);
			pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_OFF);
			pPed->SetDisableArmSolver(true);
			pPed->SetDisableHeadSolver(true);
			pPed->SetDisableLegSolver(true);
			pPed->SetDisableTorsoSolver(true);
			pPed->SetDisableTorsoReactSolver(true);

			if(m_bPreserveHairScale)
			{
				pPed->SetPedResetFlag(CPED_RESET_FLAG_OverrideHairScale, true);
				pPed->SetPedResetFlag(CPED_RESET_FLAG_OverrideHairScaleLarger, true);
				pPed->SetTargetHairScale(m_fOriginalHairScale);
			}
		}
	}
	
	if(!m_pClip)
	{
		SetState(State_Exit);
		return FSM_Continue;
	}

	if (m_bExitNextUpdate)
	{
		SetState(State_Exit);
		return FSM_Continue;
	}
	else
	{	
		ApplyMoverTrackFixup(GetPhysical()); 
		UpdatePhysics(GetPhysical()); 
	}

	if(pEnt && pEnt->GetIsTypePed())
	{
		CPed *pPed = static_cast<CPed*>(pEnt);

		const crTag *pTag = CClipEventTags::FindFirstEventTag(m_pClip, CClipEventTags::CutsceneEnterExitVehicle, m_fLastPhase, m_fPhase);
		if(pTag)
		{
			const CClipEventTags::CCutsceneEnterExitVehicleAnimEventTag *pEnterExitVehicleTag = static_cast< const CClipEventTags::CCutsceneEnterExitVehicleAnimEventTag * >(&pTag->GetProperty());
			if(pEnterExitVehicleTag)
			{
				atHashString VehicleSceneHandle = pEnterExitVehicleTag->GetVehicleSceneHandle();

				CutSceneManager *pCutsManager = CutSceneManager::GetInstance();

				atHashString modelName = atHashString::Null();
				CCutsceneAnimatedModelEntity *pModelEntity = pCutsManager->GetAnimatedModelEntityFromSceneHandle(VehicleSceneHandle, modelName);
				if(taskVerifyf(pModelEntity, "Could not find target vehicle %u %s for CutsceneEnterExitVehicle event!",
					VehicleSceneHandle.GetHash(), VehicleSceneHandle.GetCStr()))
				{
					if(taskVerifyf(pModelEntity->GetType() == CUTSCENE_VEHICLE_GAME_ENITITY, "Found a model entity %p %u %s for CutsceneEnterExitVehicle event but it's not a CUTSCENE_VEHICLE_MODEL_TYPE!",
						pModelEntity, VehicleSceneHandle.GetHash(), VehicleSceneHandle.GetCStr()))
					{
						CCutsceneAnimatedVehicleEntity *pVehicleEntity = static_cast< CCutsceneAnimatedVehicleEntity * >(pModelEntity);

						CVehicle *pVehicle = pVehicleEntity->GetGameEntity();
						if(taskVerifyf(pVehicle, "Could not get game vehicle entity for cutscene vehicle entity %p %u %s!",
							pVehicleEntity, VehicleSceneHandle.GetHash(), VehicleSceneHandle.GetCStr()))
						{
							if(pVehicle->HasRaisedRoof())
							{
								int iSeatIndex = pEnterExitVehicleTag->GetSeatIndex();
								if(taskVerifyf(pVehicle->IsSeatIndexValid(iSeatIndex), "Seat index %i is invalid for vehicle entity %p %u %s!",
									iSeatIndex, pVehicleEntity, VehicleSceneHandle.GetHash(), VehicleSceneHandle.GetCStr()))
								{
									const CVehicleSeatInfo *pSeatInfo = pVehicle->GetSeatInfo(iSeatIndex);
									if(taskVerifyf(pSeatInfo, "Could not find seat info for seat index %i, vehicle entity %p %u %s",
										iSeatIndex, pVehicleEntity, VehicleSceneHandle.GetHash(), VehicleSceneHandle.GetCStr()))
									{
										bool bDisableHairScale = false;
										const CVehicleModelInfo *pVehicleModelInfo = pVehicle->GetVehicleModelInfo();
										if(pVehicleModelInfo)
										{
											float fMinSeatHeight = pVehicleModelInfo->GetMinSeatHeight();
											float fHairHeight = pPed->GetHairHeight();

											if(fHairHeight >= 0.0f && fMinSeatHeight >= 0.0f)
											{
												const float fSeatHeadHeight = 0.59f; /* This is a conservative approximate measurement */
												if((fSeatHeadHeight + fHairHeight + CVehicle::sm_HairScaleDisableThreshold) < fMinSeatHeight)
												{
													bDisableHairScale = true;
												}
											}
										}

										float fTime = m_pClip->ConvertPhaseToTime(m_fPhase);
										float fTagStartTime = m_pClip->ConvertPhaseToTime(pTag->GetStart());
										float fTagEndTime = m_pClip->ConvertPhaseToTime(pTag->GetEnd());
										float fBlendInTime = fTagStartTime + pEnterExitVehicleTag->GetBlendIn();
										float fBlendOutTime = fTagEndTime - pEnterExitVehicleTag->GetBlendOut();

										float fHairScaleValue = 0.0f;

										if(fTime >= fTagStartTime && fTime < fBlendInTime)
										{
											/* Blend in */

											fHairScaleValue = (fTime - fTagStartTime) / (fBlendInTime - fTagStartTime);
										}
										else if(fTime >= fBlendOutTime && fTime < fTagEndTime)
										{
											/* Blend out */

											fHairScaleValue = (fTime - fBlendOutTime) / (fTagEndTime - fBlendOutTime);

											fHairScaleValue = 1.0f - fHairScaleValue;
										}
										else if(fTime > fBlendInTime && fTime < fBlendOutTime)
										{
											/* Fully blended in */

											fHairScaleValue = 1.0f;
										}

										float fHairScale = pVehicle->GetIsHeli() ? 0.0f : pSeatInfo->GetHairScale() * fHairScaleValue;
										if(fHairScale < 0.0f && !bDisableHairScale)
										{
											pPed->SetPedResetFlag(CPED_RESET_FLAG_OverrideHairScale, true);
											pPed->SetHairScaleLerp(false);
											pPed->SetTargetHairScale(fHairScale);
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if(!CutSceneManager::GetInstance()->IsRunning())
	{
		if(!AreNearlyEqual(m_fPhase, 1.0f, 0.01f))
		{
#if __BANK
			/* To display the clip dictionary name we need to find the clip dictionary index in the clip dictionary store */
			const char *clipDictionaryName = NULL;

			if (m_pClip->GetDictionary())
			{
				/* Iterate through the clip dictionaries in the clip dictionary store */
				const crClipDictionary *clipDictionary = NULL;
				int clipDictionaryIndex = 0, clipDictionaryCount = g_ClipDictionaryStore.GetSize();
				for(; clipDictionaryIndex < clipDictionaryCount && clipDictionaryName==NULL; clipDictionaryIndex ++)
				{
					if(g_ClipDictionaryStore.IsValidSlot(strLocalIndex(clipDictionaryIndex)))
					{
						clipDictionary = g_ClipDictionaryStore.Get(strLocalIndex(clipDictionaryIndex));
						if(clipDictionary == m_pClip->GetDictionary())
						{
							clipDictionaryName = g_ClipDictionaryStore.GetName(strLocalIndex(clipDictionaryIndex));
						}
					}
				}
			}
			taskWarningf("Scene: %s cutscene task playing %s (%f) at phase %f is still active but the scene has finished", clipDictionaryName , m_pClip->GetName(), m_pClip->GetDuration(), m_fPhase); 
#endif // __BANK
			
		}

		SetState(State_Exit);
		return FSM_Continue;
	}

		//only force the motion if not exiting the task.
	if(pEnt->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pEnt); 

		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_DoNothing);
		// don't allow ai timeslice lodding during cutscenes.
		pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
		pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate | CPedAILod::AL_LodTimesliceAnimUpdate);
	}
	else if(pEnt->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(pEnt);
		CVehicleAILodManager::DisableTimeslicingImmediately(*pVehicle);
	}

	return FSM_Continue; 
}


const fwMvFloatId CTaskCutScene::ms_Phase("Phase",0xA27F482B);
const fwMvFloatId CTaskCutScene::ms_Rate("Rate",0x7E68C088);
const fwMvClipId CTaskCutScene::ms_Clip("Clip",0xE71BD32A);
const fwMvFilterId CTaskCutScene::ms_Filter("Filter",0x49DC73B3); 

bool CTaskCutScene::CreateMoveNetworkPlayerForEntityType(const CPhysical* pEnt)
{
	if(pEnt)	
	{
		m_networkHelper.CreateNetworkPlayer(pEnt, CClipNetworkMoveInfo::ms_NetworkTaskCutScene);

		if(pEnt->GetIsTypePed())
		{
			// Attach it to an empty precedence slot in fwMove
			Assert(GetPed()->GetAnimDirector());
			CMovePed& move = GetPed()->GetMovePed();
			move.SetTaskNetwork(m_networkHelper, 0.0f);
			return true; 
		}
		
		if(pEnt->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = GetVehicle();
			ENABLE_FRAG_OPTIMIZATION_ONLY(pVehicle->GiveFragCacheEntry(true);)

			CMoveVehicle& move = pVehicle->GetMoveVehicle();
			move.SetNetwork(m_networkHelper.GetNetworkPlayer(), 0.0f);
			return true; 
		}

		if(pEnt->GetIsTypeObject())
		{
			CMoveObject& move = GetObject()->GetMoveObject();
			move.SetSecondaryNetwork(m_networkHelper.GetNetworkPlayer(), 0.0f);
			return true; 
		}


	}
	return false; 
}

void CTaskCutScene::RemoveMoveNetworkPlayerForEntityType(const CPhysical* pEnt)
{
	if(pEnt && pEnt->GetAnimDirector())	
	{
		if(pEnt->GetIsTypePed())
		{
			// Attach it to an empty precedence slot in fwMove
			Assert(GetPed()->GetAnimDirector());
			CMovePed& move = GetPed()->GetMovePed();
			
			if(m_HaveEventTag)
			{
				if (m_networkHelper.IsNetworkActive())
				{
					m_networkHelper.SetFloat(ms_Rate, 1.0f); 
				}				
				move.ClearTaskNetwork(m_networkHelper, m_BlendOutDuration, CMovePed::Task_TagSyncTransition|CMovePed::Task_DontDisableParamUpdate);
			}
			else
			{
			move.ClearTaskNetwork(m_networkHelper, 0.0f);
			}
			return; 
		}

		if(pEnt->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = GetVehicle();
			ENABLE_FRAG_OPTIMIZATION_ONLY(pVehicle->GiveFragCacheEntry(true);)

			CMoveVehicle& move = pVehicle->GetMoveVehicle();
			move.ClearNetwork(m_networkHelper, 0.0f);
			return; 
		}

		if(pEnt->GetIsTypeObject())
		{
			CMoveObject& move = GetObject()->GetMoveObject();
			move.ClearSecondaryNetwork(m_networkHelper, 0.0f);
			return; 
		}
	}
}


void CTaskCutScene::ApplyMoverTrackFixup(CPhysical * pPhysical)
{
	CPed * pPed = pPhysical->GetIsTypePed() ? static_cast<CPed*>(pPhysical) : NULL;
	CVehicle * pVehicle = pPhysical->GetIsTypeVehicle() ? static_cast<CVehicle*>(pPhysical) : NULL;

#if __DEV
	if (m_bApplyMoverTrackFixup)
#endif	
	{
		if(m_pClip)
		{
			Matrix34 deltaMatrix(M34_IDENTITY);
			Matrix34 startMatrix(m_AnimOrigin);
			
			if(!startMatrix.IsOrthonormal())
			{
				startMatrix.Normalize(); 
				taskAssertf(0, "TaskCutscene: The cut scene %s matrix is not orthonormal, has been normalised but will look wrong", CutSceneManager::GetInstance()->GetCutsceneName()); 
			}	

			if (pPed)
			{
				pPed->SetPedResetFlag( CPED_RESET_FLAG_DontChangeMbrInSimpleMoveDoNothing, true ); 
			}

			//Apply the fix up as absolute and not a delta
			deltaMatrix.d = fwAnimHelpers::GetMoverTrackTranslation(*m_pClip, m_fPhase);

			Quaternion qRot(Quaternion::sm_I); 
			qRot = fwAnimHelpers::GetMoverTrackRotation(*m_pClip, m_fPhase); 

			deltaMatrix.FromQuaternion(qRot); 
					
			if(!deltaMatrix.IsOrthonormal())
			{
				deltaMatrix.Normalize(); 
				taskAssertf(0, "TaskCutscene: The mover track data from animation %s does not result in a orthonormal matrix, normalising but will look wrong", m_pClip->GetName()); 
			}

			// Transform original transform by mover offset
			startMatrix.DotFromLeft(deltaMatrix);
	
			// Apply to entity. Check that script haven't left a lingering attachment at this point.
#if __ASSERT		
			bool bAssertion = !pPhysical->GetIsAttached()
				|| pPhysical->GetAttachmentExtension()->GetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE)
				|| pPhysical->GetAttachmentExtension()->GetAttachFlag(ATTACH_FLAG_IN_DETACH_FUNCTION)
				|| !(pPhysical->GetAttachmentExtension()->GetAttachState()==ATTACH_STATE_BASIC
				|| pPhysical->GetAttachmentExtension()->GetAttachState()==ATTACH_STATE_WORLD);
			scriptAssertf(bAssertion, "(%s) Object %s is still attached and cutscene wants to move it.", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPhysical->GetModelName());
#endif // __ASSERT
#if !__NO_OUTPUT
			bool ReactivateLogging = false; 
			if(pPhysical->m_LogSetMatrixCalls)
			{
				pPhysical->m_LogSetMatrixCalls = false; 
				ReactivateLogging = true; 
			}
#endif //!__NO_OUTPUT
			pPhysical->SetMatrix(startMatrix,true, true, m_bIsWarpFrame);

#if !__NO_OUTPUT			
			if(ReactivateLogging)
			{
				pPhysical->m_LogSetMatrixCalls = true;
			}
#endif //!__NO_OUTPUT			
			m_bIsWarpFrame = false;

			if(pPed)
			{
				pPed->SetDesiredHeading(pPed->GetTransform().GetHeading());
			}
			else if(pVehicle)
			{
				pVehicle->UpdateAfterCutsceneMovement();
			}
		}
	}
};

void CTaskCutScene::UpdatePhysics(CPhysical * pPhysical)
{
	CPed * pPed = pPhysical->GetIsTypePed() ? static_cast<CPed*>(pPhysical) : NULL;

	if(pPed)
	{
		pPed->SetPedResetFlag(CPED_RESET_FLAG_OverridePhysics, true);
		pPed->SetPedResetFlag( CPED_RESET_FLAG_AllowUpdateIfNoCollisionLoaded, true ); 
	}
}
