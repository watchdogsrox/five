
//rage
#include "audio/northaudioengine.h"
#include "bank/bank.h"
#include "fwanimation/directorcomponentfacialrig.h"
#include "fwanimation/directorcomponentmotiontree.h"
#include "fwanimation/facialclipsetgroups.h"
#include "math/amath.h"
#include "grcore/viewport.h"
#include "vectormath/mat34v.h"

//game
#include "UIMenuPed.h"

#include "Camera/CamInterface.h"
#include "Frontend/ui_channel.h"
#include "Frontend/WarningScreen.h"
#include "Frontend/ReportMenu.h"
#include "Network/Voice/NetworkVoice.h"
#include "Peds/ped.h"
#include "Peds/PedFactory.h"
#include "Peds/PedIntelligence.h"
#include "Renderer/Lights/lights.h"
#include "Renderer/PostProcessFX.h"
#include "Renderer/UI3DDrawManager.h"
#include "Scene/world/GameWorld.h"
#include "Script/script.h"
#include "Task/General/TaskBasic.h"

ANIM_OPTIMISATIONS()

#define INVALID_CLONE_PED_INDEX MAX_NUM_ACTIVE_PLAYERS

#define MENU_PED_LEFT_SLOT (ATSTRINGHASH("PAUSE_SINGLE_LEFT",0xc81b3278))
#define MENU_PED_MIDDLE_SLOT (ATSTRINGHASH("PAUSE_SINGLE_MIDDLE",0xad197067))
#define MENU_PED_RIGHT_SLOT (ATSTRINGHASH("PAUSE_SINGLE_RIGHT",0x5adfafd0))

#define ANIM_DATA_PREVIOUS (0)
#define ANIM_DATA_CURRENT (1)

// Male clips
const fwMvClipId SLEEP_CLIP_IDLE_MALE("IDLE",0x71C21326);
const fwMvClipId SLEEP_CLIP_INTRO_MALE("Sleep_Intro",0x7747D370);
const fwMvClipId SLEEP_CLIP_LOOP_MALE("Sleep_Loop",0xBCB979D6);
const fwMvClipId SLEEP_CLIP_OUTRO_MALE("Sleep_Outro",0x91F82BC7);

// Female clips
const fwMvClipId SLEEP_CLIP_IDLE_FEMALE("IdleFemale",0x4EE9DB1C);
const fwMvClipId SLEEP_CLIP_INTRO_FEMALE("Sleep_IntroFemale",0x5A5C4F66);
const fwMvClipId SLEEP_CLIP_LOOP_FEMALE("Sleep_LoopFemale",0xB02DA731);
const fwMvClipId SLEEP_CLIP_OUTRO_FEMALE("Sleep_OutroFemale",0x87C4D5E1);

#define ANIM_STATE_BLEND_DELTA (SLOW_BLEND_IN_DELTA)
#define FACIAL_VOICE_DRIVEN_MOUTH_MOVEMENT_BLEND_DELTA (NORMAL_BLEND_IN_DELTA)

bank_float CUIMenuPed::sm_singleFrontOffset = 0.6f;
bank_float CUIMenuPed::sm_singleRightOffset = 0.0f;
bank_float CUIMenuPed::sm_singleUpOffset = 0.07f;
bank_float CUIMenuPed::sm_doubleLocalFrontOffset = 0.7f;
bank_float CUIMenuPed::sm_doubleLocalRightOffset = 0.37f;
bank_float CUIMenuPed::sm_doubleLocalUpOffset = 0.07f;
bank_float CUIMenuPed::sm_doubleOtherFrontOffset = 0.7f;
bank_float CUIMenuPed::sm_doubleOtherRightOffset = -0.37f;
bank_float CUIMenuPed::sm_doubleOtherUpOffset = 0.07f;
bank_float CUIMenuPed::sm_headingOffset = 180.0f;
bank_float CUIMenuPed::sm_pitchOffset = 0.0f;
bank_float CUIMenuPed::sm_dimIntensity = 0.0f;
bank_float CUIMenuPed::sm_litIntensity = 0.8f;

#if __BANK
bool sForcePedSleepState = false;
bool sPedIsAsleep = false;
#endif

const bool cResetTaskOnFlush = true;

CUIMenuPed::CUIMenuPed():
m_pScriptPed(nullptr),
m_pDisplayPed(nullptr),
m_pOldDisplayPed(nullptr),
m_pClonePed(nullptr),
m_isVisible(true),
m_deletePending(false),
m_isMale(true),
m_bCloneLocalPlayer(false),
m_iClonePlayerIndex(INVALID_CLONE_PED_INDEX),
m_fFacialTimer(0.0f),
m_fFacialVoiceDrivenMouthMovementTimer(0.0f),
m_fAnimStateBlendWeight(1.0f),
m_fFacialVoiceDrivenMouthMovementBlendWeight(0.0f),
m_currentDisplayColumn(PM_COLUMN_MIDDLE),
m_nextDisplayColumn(PM_COLUMN_MIDDLE),
m_fCachedAnimPhaseForNextPed(-1.0f),
m_slot(0),
m_uFadeInStartTimeMS(0),
m_uFadeDurationMS(0),
m_controlsBG(true),
m_clearBGonPedDelete(true),
m_bWaitingForNewPed(false)
{

}

CUIMenuPed::~CUIMenuPed()
{
	Shutdown();
	Clear(m_pScriptPed.Get());
	Clear(m_pDisplayPed.Get());
	Clear(m_pOldDisplayPed.Get());
}

void CUIMenuPed::Init()
{
	fwMvClipSetId clipSetId("mp_sleep",0xECD664BF);
	m_sleepAnimRequest.Request(clipSetId);

	m_animStateDataBuffer[ANIM_DATA_PREVIOUS].Reset();
	m_animStateDataBuffer[ANIM_DATA_CURRENT].Reset();

	m_isVisible = true;
	m_deletePending = true;

	m_fCachedAnimPhaseForNextPed = -1.0f;

	m_bWaitingForNewPed = false;
}

void CUIMenuPed::Shutdown()
{
	SetVisible(false);
	m_deletePending = true;
	m_bCloneLocalPlayer = false;
	m_isLit = false;
	m_iClonePlayerIndex = INVALID_CLONE_PED_INDEX;
	m_sleepAnimRequest.Release();
	m_fCachedAnimPhaseForNextPed = -1.0f;
}

void CUIMenuPed::SetIsLit(bool isLit)
{
	m_isLit = isLit;
}

void CUIMenuPed::Update(float fAlphaFader)
{
#if __BANK
	if(sForcePedSleepState)
	{
		SetIsAsleep(sPedIsAsleep);
	}
#endif

	Clear(m_pOldDisplayPed.Get());

	// Delete Pending
	//-------------------

	if(m_deletePending)
	{
		const bool originalControlsBG = m_controlsBG;
		SetControlsBG(m_clearBGonPedDelete);
		SetVisible(false);
		SetControlsBG(originalControlsBG);

		Clear(m_pScriptPed.Get());
		Clear(m_pDisplayPed.Get());

		m_pScriptPed 	= nullptr;
		m_pDisplayPed 	= nullptr;
		m_pClonePed 	= nullptr;
		
		m_bCloneLocalPlayer = false;
		m_iClonePlayerIndex = INVALID_CLONE_PED_INDEX;

		m_deletePending = false;
	}

	// Ped Cloning / Script Ped
	// ------------------------

	CPed* pPedToClone = nullptr;
	if(m_bCloneLocalPlayer)
	{
		pPedToClone = CGameWorld::FindLocalPlayer();
		m_bCloneLocalPlayer = false;
	}

	if(m_iClonePlayerIndex != INVALID_CLONE_PED_INDEX)
	{
		if(CPauseMenu::IsMP())
		{
			// This data doesn't exist in the lobby yet.
			CNetGamePlayer* pNetPed = NetworkInterface::GetActivePlayerFromIndex(m_iClonePlayerIndex);
			pPedToClone = AssertVerify(pNetPed) ? pNetPed->GetPlayerPed() : nullptr;
		}
		m_iClonePlayerIndex = INVALID_CLONE_PED_INDEX;
	}

	if(pPedToClone || m_pScriptPed.Get())
	{
		m_pOldDisplayPed 	= m_pDisplayPed.Get();
		m_pDisplayPed 		= nullptr;
		m_pClonePed 		= nullptr;

		if(m_pScriptPed.Get())
		{
			m_pDisplayPed = m_pScriptPed.Get();
			m_pScriptPed = nullptr;
			m_pClonePed = m_pScriptPed;
		}
		else
		{
			CPedFactory* pFactory = CPedFactory::GetFactory();
			if(uiVerifyf(pFactory, "CUIMenuPed::Update - Trying to create a ped when the Ped Factory doesn't exist yet."))
			{
				if(uiVerifyf(pPedToClone->GetPedModelInfo(), "CUIMenuPed::Update - Cannot clone ped because ped does not have a valid model info!"))
				{
					m_pDisplayPed = pFactory->ClonePed(pPedToClone, false, false, true);
					m_pClonePed = pPedToClone;
					m_pDisplayPed->FinalizeHeadBlend();
				}
			}
		}

		if(m_pDisplayPed.Get())
		{
			InitPed(m_pDisplayPed);
		}
	}

	float fCurrentPedAlpha = 1.0f;
	if(m_uFadeInStartTimeMS > 0)
	{
		u32 currentTimeMS = fwTimer::GetTimeInMilliseconds();
		if(currentTimeMS >= m_uFadeInStartTimeMS)
		{
			if(!IsVisible())
			{
				SetVisible(true);
			}

			fCurrentPedAlpha = (float)(currentTimeMS - m_uFadeInStartTimeMS) / (float)m_uFadeDurationMS;

			if(fCurrentPedAlpha >= 1.0f)
			{
				// End Fade
				m_uFadeDurationMS = 0;
				m_uFadeInStartTimeMS = 0;
				fCurrentPedAlpha = 1.0f;
			}
			else
			{
				// Menu fade isn't linear so let's try to match it
				fCurrentPedAlpha = fCurrentPedAlpha * fCurrentPedAlpha;
			}
		}
	}


	// Main Ped Update
	//----------------

	if(m_pDisplayPed && m_isVisible)
	{	
		if (CPauseMenu::GetDynamicPauseMenu())
		{
			CPauseMenu::GetDynamicPauseMenu()->SetAvatarBGAlpha(fCurrentPedAlpha);
		}

		m_pDisplayPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
		m_pDisplayPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate | CPedAILod::AL_LodTimesliceAnimUpdate);
		m_pDisplayPed->SetDesiredHeading(m_pDisplayPed->GetTransform().GetHeading());

		UpdateAnimState();

		bool hideForWarning = CPauseMenu::GetDynamicPauseMenu() ? CPauseMenu::GetDynamicPauseMenu()->GetErrorMessage().IsActive() : false;
		if(SReportMenu::GetInstance().IsActive())
		{
			hideForWarning = true;
		}

		if(!hideForWarning &&
			m_animState != ANIM_INVALID)
		{
			if(m_pDisplayPed->HasHeadBlendFinished())
			{
				// Dirty Dirty HACK, to stop the ped from trying to turn.
				m_pDisplayPed->SetHeading(m_pDisplayPed->GetTransform().GetHeading());
				m_pDisplayPed->SetDesiredHeading(m_pDisplayPed->GetTransform().GetHeading());

				if(m_pDisplayPed->GetMotionData())
				{
					m_pDisplayPed->GetMotionData()->SetCurrentHeading(m_pDisplayPed->GetTransform().GetHeading());
					m_pDisplayPed->GetMotionData()->SetDesiredHeading(m_pDisplayPed->GetTransform().GetHeading());
				}

				Vector3 offset;

				if(m_currentDisplayColumn == PM_COLUMN_EXTRA4)
				{
					offset = Vector3(-sm_doubleLocalRightOffset, -sm_doubleLocalFrontOffset, sm_doubleLocalUpOffset); 
				}
				else if(m_currentDisplayColumn == PM_COLUMN_EXTRA3)
				{
					offset = Vector3(-sm_doubleOtherRightOffset, -sm_doubleOtherFrontOffset, sm_doubleOtherUpOffset); 
				}
				else
				{
					offset = Vector3(-sm_singleRightOffset, -sm_singleFrontOffset, sm_singleUpOffset); 
				}

				if(UI3DDRAWMANAGER.IsAvailable())
				{
					UI3DDRAWMANAGER.PushCurrentPreset(GetMenuPedSlot());
				}
				UI3DDRAWMANAGER.AssignPedToSlot(GetMenuPedSlot(), m_slot, m_pDisplayPed, offset, fCurrentPedAlpha);
				UI3DDRAWMANAGER.AssignGlobalLightIntensityToSlot(GetMenuPedSlot(), m_slot, m_isLit ? sm_litIntensity : sm_dimIntensity * fAlphaFader);
			}
		}
	}
}

void CUIMenuPed::UpdatePedPose()
{
	if (m_pDisplayPed && m_pDisplayPed->GetSkeleton() && m_pDisplayPed->GetPedModelInfo())
	{
		if (CPedType::IsAnimalType(m_pDisplayPed->GetPedType()))
		{
			UpdateAnimalPedPose();
			return;
		}

		const crClip* pBodyClipCurrent = fwAnimManager::GetClipIfExistsBySetId(m_animStateDataBuffer[ANIM_DATA_CURRENT].m_ClipSetId, m_animStateDataBuffer[ANIM_DATA_CURRENT].m_ClipId);
		if (pBodyClipCurrent)
		{
			// Get delta
			const float deltaTime = fwTimer::GetTimeStep();

			fwAnimDirector* director = m_pDisplayPed->GetAnimDirector();

			fwAnimDirectorComponentFacialRig* pFacialRigComp = director->GetComponentByPhase<fwAnimDirectorComponentFacialRig>(fwAnimDirectorComponent::kPhaseAll);
			if (pFacialRigComp)
			{
				fwAnimDirectorComponentMotionTree::SetLockedGlobal(true, fwAnimDirectorComponent::kPhasePrePhysics);
				director->StartUpdatePrePhysics(deltaTime, true);
				director->WaitForMotionTreeByPhase(fwAnimDirectorComponent::kPhasePrePhysics);
				fwAnimDirectorComponentMotionTree::SetLockedGlobal(false, fwAnimDirectorComponent::kPhaseNone);

				// Make a frame for the body animation 
				const crFrameData& pedFrameData = m_pDisplayPed->GetCreature()->GetFrameData();
				crFrame frameBody;	
				frameBody.Init(pedFrameData);

				// Update current state
				m_animStateDataBuffer[ANIM_DATA_CURRENT].Update(deltaTime);

				// Update blend weight and clamp it
				m_fAnimStateBlendWeight += deltaTime * ANIM_STATE_BLEND_DELTA;
				m_fAnimStateBlendWeight = Clamp(m_fAnimStateBlendWeight, 0.0f, 1.0f);

				// Blend needed between current and previous state?
				const crClip* pBodyClipPrevious = fwAnimManager::GetClipIfExistsBySetId(m_animStateDataBuffer[ANIM_DATA_PREVIOUS].m_ClipSetId, m_animStateDataBuffer[ANIM_DATA_PREVIOUS].m_ClipId);
				if (m_fAnimStateBlendWeight < 1.0f && pBodyClipPrevious)
				{
					// Update the previous state too
					m_animStateDataBuffer[ANIM_DATA_PREVIOUS].Update(deltaTime);

					// Create another frame for the current pose, the previous pose we'll put into frameBody
					crFrame frameBodyCurrent;	
					frameBodyCurrent.Init(pedFrameData);

					// Compose the previous animation into a frame
					pBodyClipPrevious->Composite(frameBody, m_animStateDataBuffer[ANIM_DATA_PREVIOUS].m_fAnimTimer);

					// Compose the current animation into a frame
					pBodyClipCurrent->Composite(frameBodyCurrent, m_animStateDataBuffer[ANIM_DATA_CURRENT].m_fAnimTimer);

					// Blend them together
					frameBody.Blend(m_fAnimStateBlendWeight, frameBodyCurrent);
				}
				else
				{
					// Compose the currrent animation into a frame
					pBodyClipCurrent->Composite(frameBody, m_animStateDataBuffer[ANIM_DATA_CURRENT].m_fAnimTimer);
				}

				// Play a facial animation if one is available
				fwFacialClipSetGroup* pFacialClipSetGroup = fwFacialClipSetGroupManager::GetFacialClipSetGroup(m_pDisplayPed->GetPedModelInfo()->GetFacialClipSetGroupId());
				if (pFacialClipSetGroup)
				{
					fwMvClipSetId facialBaseClipSetId = fwClipSetManager::GetClipSetId(pFacialClipSetGroup->GetBaseClipSetName());
					Assertf(facialBaseClipSetId != CLIP_SET_ID_INVALID, "%s:Unrecognised facial base clipset", pFacialClipSetGroup->GetBaseClipSetName().GetCStr());

					const crClip* pFacialClip = fwAnimManager::GetClipIfExistsBySetId(facialBaseClipSetId, CLIP_FACIAL_MOOD_NORMAL);
					if (pFacialClip)
					{
						// Make a frame for the facial animation
						crFrame frameFacial;
						frameFacial.Init(pedFrameData);

						// Increment the phase of the anim loop automatically
						m_fFacialTimer += fwTimer::GetTimeStep();
						m_fFacialTimer = fmodf(m_fFacialTimer, pFacialClip->GetDuration());

						// Compose the animation into a frame
						pFacialClip->Composite(frameFacial, m_fFacialTimer);

						// Merge the facial frame into the body frame
						frameBody.Merge(frameFacial, nullptr);
					}
				}

				bool bIsPlayerTalking = false;
				strLocalIndex clipDictionarySlot = strLocalIndex(g_ClipDictionaryStore.FindSlot("mp_facial"));
				if(Verifyf(g_ClipDictionaryStore.IsValidSlot(clipDictionarySlot), "Could not find clip dictionary mp_facial in the clip dictionary store!"))
				{
					if(Verifyf(g_ClipDictionaryStore.HasObjectLoaded(clipDictionarySlot), "Clip dictionary mp_facial has not been loaded!"))
					{
						const crClip* pFacialVoiceDrivenMouthMovementClip = fwAnimManager::GetClipIfExistsByDictIndex(clipDictionarySlot.Get(), CLIP_FACIAL_VOICE_DRIVEN_MOUTH_MOVEMENT);
						if (pFacialVoiceDrivenMouthMovementClip)
						{
							if(m_pClonePed.Get())
							{
								bIsPlayerTalking = pFacialRigComp->IsVoiceActive();
							}

							if(bIsPlayerTalking)
							{
								// Increment the phase of the anim loop automatically
								m_fFacialVoiceDrivenMouthMovementTimer += fwTimer::GetTimeStep();
								m_fFacialVoiceDrivenMouthMovementTimer = fmodf(m_fFacialVoiceDrivenMouthMovementTimer, pFacialVoiceDrivenMouthMovementClip->GetDuration());

								m_fFacialVoiceDrivenMouthMovementBlendWeight += (deltaTime * FACIAL_VOICE_DRIVEN_MOUTH_MOVEMENT_BLEND_DELTA);
							}
							else
							{
								m_fFacialVoiceDrivenMouthMovementBlendWeight -= (deltaTime * FACIAL_VOICE_DRIVEN_MOUTH_MOVEMENT_BLEND_DELTA);
							}
							m_fFacialVoiceDrivenMouthMovementBlendWeight = Clamp(m_fFacialVoiceDrivenMouthMovementBlendWeight, 0.0f, 1.0f);

							if(m_fFacialVoiceDrivenMouthMovementBlendWeight > 0)
							{
								// Make a frame for the facial animation
								crFrame frameFacialVoiceDrivenMouthMovement;
								frameFacialVoiceDrivenMouthMovement.Init(pedFrameData);

								// Compose the animation into a frame
								pFacialVoiceDrivenMouthMovementClip->Composite(frameFacialVoiceDrivenMouthMovement, m_fFacialVoiceDrivenMouthMovementTimer);

								// Merge the facial frame into the body frame
								frameBody.Blend(m_fFacialVoiceDrivenMouthMovementBlendWeight, frameFacialVoiceDrivenMouthMovement);
							}
						}
					}
				}

				// Reset skeleton to not accumulate the floating-point imprecisions
				m_pDisplayPed->GetSkeleton()->Reset();

				//Pose the peds skeleton using the combined body and facial frame
				frameBody.Pose(*m_pDisplayPed->GetSkeleton(),false, nullptr);						
				m_pDisplayPed->GetSkeleton()->Update();

				fwAnimDirectorComponentFacialRig* pFacialRigComp = director->GetComponentByPhase<fwAnimDirectorComponentFacialRig>(fwAnimDirectorComponent::kPhaseAll);
				if(pFacialRigComp)
				{
					pFacialRigComp->GetFrame().Set(frameBody);
				}

				fwAnimDirectorComponentMotionTree::SetLockedGlobal(true, fwAnimDirectorComponent::kPhaseMidPhysics);
				director->StartUpdateMidPhysics(deltaTime);
				director->WaitForMotionTreeByPhase(fwAnimDirectorComponent::kPhaseMidPhysics);
				fwAnimDirectorComponentMotionTree::SetLockedGlobal(false, fwAnimDirectorComponent::kPhaseNone);

				// Do the animation pre-render update (to update the expressions)
				fwAnimDirectorComponentMotionTree::SetLockedGlobal(true, fwAnimDirectorComponent::kPhasePreRender);
				director->StartUpdatePreRender(deltaTime);
				director->WaitForMotionTreeByPhase(fwAnimDirectorComponent::kPhasePreRender);
				fwAnimDirectorComponentMotionTree::SetLockedGlobal(false, fwAnimDirectorComponent::kPhaseNone);
			}
		}
	}
}

void CUIMenuPed::UpdateAnimalPedPose()
{
	if (m_pDisplayPed)
	{
		m_pDisplayPed->SetPedConfigFlag(CPED_CONFIG_FLAG_BlockNonTemporaryEvents, true);
	}
}

void CUIMenuPed::ClearPed(bool clearBG)
{
	// NOTE: This function will currently invoke 1-frame of no ped rendering.  Only call this if you really have to or are closing the menu down.
	// To just swap the ped use SetPedModel() only.
	if(!UI3DDRAWMANAGER.IsAvailable())
	{
		UI3DDRAWMANAGER.AssignPedToSlot(GetMenuPedSlot(), m_slot, nullptr, 0.0f);
	}

	m_fCachedAnimPhaseForNextPed = -1.0f;

	m_clearBGonPedDelete = clearBG;
	m_deletePending = true;
}

bool CUIMenuPed::HasPed() const
{
	return m_pDisplayPed != nullptr ||
		m_pScriptPed.Get() != nullptr ||
		m_bCloneLocalPlayer ||
		m_iClonePlayerIndex != INVALID_CLONE_PED_INDEX;
}

void CUIMenuPed::SetVisible(bool isVisible)
{
	if(!isVisible && !UI3DDRAWMANAGER.IsAvailable() &&
		(UI3DDRAWMANAGER.IsUsingPreset(MENU_PED_LEFT_SLOT) || UI3DDRAWMANAGER.IsUsingPreset(MENU_PED_MIDDLE_SLOT) || UI3DDRAWMANAGER.IsUsingPreset(MENU_PED_RIGHT_SLOT)))
	{
		UI3DDRAWMANAGER.AssignPedToSlot(GetMenuPedSlot(), m_slot, nullptr, 0.0f);
	}
	m_isVisible = isVisible;

	if(m_controlsBG)
	{
		CPauseMenu::GetDynamicPauseMenu()->SetAvatarBGOn(isVisible);
	}

	if(!isVisible)
	{
		// Reset Fade
		m_uFadeInStartTimeMS = 0;
		m_uFadeDurationMS = 0;
	}
}

void CUIMenuPed::SetFadeIn(u32 fadeDurationMS, u32 fadeStartDelayMS, bool bForceOverrideExistingFade)
{
	if(!IsVisible())
	{
		if(!bForceOverrideExistingFade && (m_uFadeInStartTimeMS != 0))
		{
			return;
		}

		m_uFadeInStartTimeMS = fwTimer::GetTimeInMilliseconds() + fadeStartDelayMS;
		m_uFadeDurationMS = fadeDurationMS;
	}
}

void CUIMenuPed::SetPedModel(ActivePlayerIndex playerIndex, PM_COLUMNS column /*= PM_COLUMN_MIDDLE*/)
{
	UI3DDRAWMANAGER.AssignPedToSlot(GetMenuPedSlot(), 0, nullptr, 0.0f);

	m_iClonePlayerIndex = playerIndex;
	m_nextDisplayColumn = column;

	m_bWaitingForNewPed = false;
}

void CUIMenuPed::SetPedModel(CPed* pPed, PM_COLUMNS column /*= PM_COLUMN_MIDDLE*/)
{
	UI3DDRAWMANAGER.AssignPedToSlot(GetMenuPedSlot(), m_slot, nullptr, 0.0f);

	m_pScriptPed = pPed;
	m_nextDisplayColumn = column;

	m_bWaitingForNewPed = false;
}

void CUIMenuPed::InitPed(CPed* pPed)
{
	if(uiVerifyf(pPed, "CUIMenuPed::InitPed - Invalid pointer, how did this happen?"))
	{
		pPed->SetHealth(pPed->GetMaxHealth());
		pPed->SetBaseFlag(fwEntity::IS_FIXED);
		pPed->DisableCollision();
		pPed->DisablePhysicsIfFixed();
		pPed->SetClothForcePin(1);
		pPed->SetHairScaleLerp(false);

		pPed->SetRenderPhaseVisibilityMask(VIS_PHASE_MASK_OTHER);
		pPed->SetUseAmbientScale(false);
		pPed->SetUseDynamicAmbientScale(false);
		pPed->SetNaturalAmbientScale(255);
		pPed->SetArtificialAmbientScale(255);
		
		pPed->GetPedIntelligence()->FlushImmediately(cResetTaskOnFlush);

		m_animStateDataBuffer[ANIM_DATA_PREVIOUS].Reset();
		m_animStateDataBuffer[ANIM_DATA_CURRENT].Reset();
		m_fAnimStateBlendWeight = 1.0f;
		m_fFacialVoiceDrivenMouthMovementBlendWeight = 0.0f;
		m_animState = ANIM_INVALID;

		// Dirty Dirty HACK, to stop the ped from trying to turn.
		pPed->SetHeading(pPed->GetTransform().GetHeading());
		pPed->SetDesiredHeading(pPed->GetTransform().GetHeading());

		if(pPed->GetMotionData())
		{
			pPed->GetMotionData()->SetCurrentHeading(pPed->GetTransform().GetHeading());
			pPed->GetMotionData()->SetDesiredHeading(pPed->GetTransform().GetHeading());
		}

		if(pPed->GetSpeechAudioEntity())
		{
			pPed->GetSpeechAudioEntity()->DisablePainAudio(true);
			pPed->GetSpeechAudioEntity()->DisableSpeaking(true);
		}

		// Set ped gender
		m_isMale = pPed->IsMale();
	}

	m_currentDisplayColumn = m_nextDisplayColumn;
}

void CUIMenuPed::Clear(CPed* pOldPed)
{
	if(pOldPed)
	{
		CPedFactory* pFactory = CPedFactory::GetFactory();
		if(uiVerifyf(pFactory, "CUIMenuPed::Clear - Trying to delete a ped when the Ped Factory doesn't exist yet."))
		{
			while( audNorthAudioEngine::IsAudioUpdateCurrentlyRunning() )
				sysIpcSleep(1);

			pFactory->DestroyPed(pOldPed);
		}
	}
}

void CUIMenuPed::UpdateAnimState()
{
	switch(m_animState)
	{
	case ANIM_INVALID:
		if(m_isAsleep)
		{
			if(PlayAnim(m_isMale ? SLEEP_CLIP_LOOP_MALE : SLEEP_CLIP_LOOP_FEMALE, true, false, m_fCachedAnimPhaseForNextPed))
			{
				m_animState = ANIM_STATE_SLEEP_LOOP;
			}
		}
		else
		{
			if(PlayAnim(m_isMale ? SLEEP_CLIP_IDLE_MALE : SLEEP_CLIP_IDLE_FEMALE, true, false, m_fCachedAnimPhaseForNextPed))
			{
				m_animState = ANIM_STATE_IDLE;
			}
		}
		break;
	case ANIM_STATE_IDLE:
		m_fCachedAnimPhaseForNextPed = m_animStateDataBuffer[ANIM_DATA_CURRENT].m_fCurrentPhase;
		if(m_isAsleep)
		{
			GoToSleepIntroAnim();
		}
		break;
	case ANIM_STATE_SLEEP_INTRO:
		if(m_animStateDataBuffer[ANIM_DATA_CURRENT].m_fCurrentPhase + (m_animStateDataBuffer[ANIM_DATA_CURRENT].m_fCurrentPhaseDelta * 2.0f) >= 1.0f)
		{
			GoToSleepLoopAnim();
		}
		break;
	case ANIM_STATE_SLEEP_LOOP:
		m_fCachedAnimPhaseForNextPed = m_animStateDataBuffer[ANIM_DATA_CURRENT].m_fCurrentPhase;
		if(!m_isAsleep)
		{
			GoToSleepOutroAnim();
		}
		break;
	case ANIM_STATE_SLEEP_OUTRO:
		if(m_animStateDataBuffer[ANIM_DATA_CURRENT].m_fCurrentPhase + (m_animStateDataBuffer[ANIM_DATA_CURRENT].m_fCurrentPhaseDelta * 2.0f) >= 1.0f)
		{
			GoToIdleAnim();
		}
		break;
	}
}

void CUIMenuPed::GoToIdleAnim()
{
	if(PlayAnim(m_isMale ? SLEEP_CLIP_IDLE_MALE : SLEEP_CLIP_IDLE_FEMALE, true, true))
	{
		m_animState = ANIM_STATE_IDLE;
	}
}

void CUIMenuPed::GoToSleepIntroAnim()
{
	if(PlayAnim(m_isMale ? SLEEP_CLIP_INTRO_MALE : SLEEP_CLIP_INTRO_FEMALE, false, true))
	{
		m_animState = ANIM_STATE_SLEEP_INTRO;
	}
}

void CUIMenuPed::GoToSleepLoopAnim()
{
	if(PlayAnim(m_isMale ? SLEEP_CLIP_LOOP_MALE : SLEEP_CLIP_LOOP_FEMALE, true, true))
	{
		m_animState = ANIM_STATE_SLEEP_LOOP;
	}
}

void CUIMenuPed::GoToSleepOutroAnim()
{
	if(PlayAnim(m_isMale ? SLEEP_CLIP_OUTRO_MALE : SLEEP_CLIP_OUTRO_FEMALE, false, true))
	{
		m_animState = ANIM_STATE_SLEEP_OUTRO;
	}
}

bool CUIMenuPed::PlayAnim(const fwMvClipId& animClipId, bool bLoop, bool bBlend, float fInitialPhase)
{
	if(m_sleepAnimRequest.Request() && m_sleepAnimRequest.IsLoaded())
	{
		// Get the clip
		const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(m_sleepAnimRequest.GetClipSetId(), animClipId);
		if (pClip)
		{
			if (bBlend)
			{
				// Copy the old
				m_animStateDataBuffer[ANIM_DATA_PREVIOUS] = m_animStateDataBuffer[ANIM_DATA_CURRENT];

				// Start blending
				m_fAnimStateBlendWeight = 0.0f;
			}
			else
			{
				m_animStateDataBuffer[ANIM_DATA_PREVIOUS].Reset();
				m_fAnimStateBlendWeight = 1.0f;
			}

			// Set the current
			m_animStateDataBuffer[ANIM_DATA_CURRENT].m_ClipSetId = m_sleepAnimRequest.GetClipSetId();
			m_animStateDataBuffer[ANIM_DATA_CURRENT].m_ClipId = animClipId;
			m_animStateDataBuffer[ANIM_DATA_CURRENT].m_bLooped = bLoop;
			m_animStateDataBuffer[ANIM_DATA_CURRENT].m_fAnimTimer = 0.0f;
			m_animStateDataBuffer[ANIM_DATA_CURRENT].m_fCurrentPhase = 0.0f;
			m_animStateDataBuffer[ANIM_DATA_CURRENT].m_fCurrentPhaseDelta = 0.0f;

			// If we've been given a specific phase to start with, use that.
			if (fInitialPhase >= 0.0f)
			{
				float fNewPhase = Clamp(fInitialPhase, 0.0f, 1.0f);
				m_animStateDataBuffer[ANIM_DATA_CURRENT].m_fCurrentPhase = fNewPhase;
				m_animStateDataBuffer[ANIM_DATA_CURRENT].m_fAnimTimer = pClip->ConvertPhaseToTime(fNewPhase);
			}

			return true;
		}
	}

	return false;
}

atHashString CUIMenuPed::GetMenuPedSlot() const
{
	if(m_currentDisplayColumn == PM_COLUMN_LEFT)
	{
		return MENU_PED_LEFT_SLOT;
	}
	else if(m_currentDisplayColumn == PM_COLUMN_RIGHT)
	{
		return MENU_PED_RIGHT_SLOT;
	}

	return MENU_PED_MIDDLE_SLOT;
}

void CUIMenuPed::AnimStateData::Reset()
{
	m_ClipSetId = CLIP_SET_ID_INVALID;
	m_ClipId = CLIP_ID_INVALID;
	m_fCurrentPhase = 0.0f;
	m_fCurrentPhaseDelta = 0.0f;
	m_fAnimTimer = 0.0f;
	m_bLooped = true;
}

CUIMenuPed::AnimStateData& CUIMenuPed::AnimStateData::operator=(const CUIMenuPed::AnimStateData& other)
{
	m_ClipSetId = other.m_ClipSetId;
	m_ClipId = other.m_ClipId;
	m_fCurrentPhase = other.m_fCurrentPhase;
	m_fCurrentPhaseDelta = other.m_fCurrentPhaseDelta;
	m_fAnimTimer = other.m_fAnimTimer;
	m_bLooped = other.m_bLooped;

	return *this;
}

void CUIMenuPed::AnimStateData::Update(float fDelta)
{
	const crClip* pBodyClip = fwAnimManager::GetClipIfExistsBySetId(m_ClipSetId, m_ClipId);
	if (pBodyClip)
	{
		// Remember previous phase
		const float fPrevPhase = pBodyClip->ConvertTimeToPhase(m_fAnimTimer);

		// Increment the phase of the anim
		m_fAnimTimer += fDelta;

		// Loop?
		if (m_bLooped)
		{
			m_fAnimTimer = fmodf(m_fAnimTimer, pBodyClip->GetDuration());
		}
		else
		{
			if (m_fAnimTimer > pBodyClip->GetDuration())
			{
				m_fAnimTimer = pBodyClip->GetDuration();
			}
		}

		// Update phase information
		m_fCurrentPhase = pBodyClip->ConvertTimeToPhase(m_fAnimTimer);
		if (m_bLooped && (m_fCurrentPhase < fPrevPhase))
		{
			// Phase delta from (prev phase to end) and (start to current phase)
			m_fCurrentPhaseDelta = (1.0f - fPrevPhase) + m_fCurrentPhase;
		}
		else
		{
			m_fCurrentPhaseDelta = m_fCurrentPhase - fPrevPhase;
		}
	}
}

#if __BANK
void CUIMenuPed::CreateBankWidgets(bkBank *bank)
{
	if (bank)
	{
		bank->PushGroup("Menu Ped");

		bank->AddSlider("Single Front Dist", &sm_singleFrontOffset, -10.0, 100.0, 0.01f);
		bank->AddSlider("Single Right Dist", &sm_singleRightOffset, -10.0, 10.0, 0.01f);
		bank->AddSlider("Single Up Dist", &sm_singleUpOffset, -10.0f, 10.0f, 0.01f);

		bank->AddSlider("Double Local Front Dist", &sm_doubleLocalFrontOffset, -10.0, 100.0, 0.01f);
		bank->AddSlider("Double Local Right Dist", &sm_doubleLocalRightOffset, -10.0, 10.0, 0.01f);
		bank->AddSlider("Double Local Up Dist", &sm_doubleLocalUpOffset, -10.0f, 10.0f, 0.01f);

		bank->AddSlider("Double Other Front Dist", &sm_doubleOtherFrontOffset, -10.0, 100.0, 0.01f);
		bank->AddSlider("Double Other Right Dist", &sm_doubleOtherRightOffset, -10.0, 10.0, 0.01f);
		bank->AddSlider("Double Other Up Dist", &sm_doubleOtherUpOffset, -10.0f, 10.0f, 0.01f);

		bank->AddSlider("Heading Degrees", &sm_headingOffset, -360.0, 360.0, 1.0f);
		bank->AddSlider("Pitch Degrees", &sm_pitchOffset, -360.0, 360.0, 1.0f);
		bank->AddSlider("Dim Intensity", &sm_dimIntensity, -1000.0, 1000.0, 0.1f);
		bank->AddSlider("Lit Intensity", &sm_litIntensity, -1000.0, 1000.0, 0.1f);

		bank->AddSeparator();
		bank->AddToggle("Force Ped Sleep State", &sForcePedSleepState);
		bank->AddToggle("Ped Is Asleep", &sPedIsAsleep, datCallback(CFA(DebugSetSleepState)));

		bank->PopGroup();
	}
}

void CUIMenuPed::DebugSetSleepState()
{
	sForcePedSleepState = true;
}
#endif

//eof
