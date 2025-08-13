
/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CCutSceneSoundEntity.cpp
// PURPOSE : 
// AUTHOR  : Thomas French
// STARTED : 27/10/2009
//
/////////////////////////////////////////////////////////////////////////////////

//Rage Headers
#include "audio/cutsceneaudioentity.h"
#include "camera/caminterface.h"
#include "cutscene/CutSceneSoundEntity.h"
#include "cutfile/cutfobject.h"
#include "cutscene/cutsevent.h"
#include "cutscene/cutsmanager.h"
#include "cutfile/cutfeventargs.h"
#include "cutscene/cutseventargs.h"
#include "cutscene/cutscene_channel.h"
#include "cutscene/CutSceneManagerNew.h"
#include "fwsys/timer.h"

/////////////////////////////////////////////////////////////////////////////////

ANIM_OPTIMISATIONS()

/////////////////////////////////////////////////////////////////////////////////

bool CCutSceneAudioEntity::ms_bIsPlaying = false; 
bool CCutSceneAudioEntity::ms_bIsLoaded = false; 

CCutSceneAudioEntity::CCutSceneAudioEntity (const cutfObject* pObject)
:cutsUniqueEntity (pObject)
{
#if !__FINAL
	m_bWasJogged = false;
#endif // !__FINAL
}

CCutSceneAudioEntity::~CCutSceneAudioEntity ()
{
	ms_bIsPlaying = false; 
	ms_bIsLoaded = false; 
}

/////////////////////////////////////////////////////////////////////////////////

void CCutSceneAudioEntity::Play()
{
	g_CutsceneAudioEntity.TriggerCutscene();	
}

/////////////////////////////////////////////////////////////////////////////////

void CCutSceneAudioEntity::Stop(bool stopAllStreams)
{
	g_CutsceneAudioEntity.Stop(stopAllStreams, -1 BANK_ONLY(, m_bWasJogged ? 0:500) );
	ms_bIsLoaded = false;
}

void CCutSceneAudioEntity::StopAllCutSceneAudioStreams(bool bWasSkipped)
{
	g_CutsceneAudioEntity.StopCutscene(bWasSkipped);
	ms_bIsLoaded = false;
	
}

void CCutSceneAudioEntity::Pause()
{
	g_CutsceneAudioEntity.Pause();
}

void CCutSceneAudioEntity::UnPause()
{
	g_CutsceneAudioEntity.UnPause();
}
#if !__NO_OUTPUT
void CCutSceneAudioEntity::CommonDebugStr(const cutfObject* pObject, char * debugStr)
{
	if (!pObject)
	{
		return;
	}
	const cutfAudioObject* pAudioObj = static_cast<const cutfAudioObject*>(pObject); 

	sprintf(debugStr, "%s - CT(%6.2f) - CF(%u) - FC(%u) - Audio Entity(%d, %s) - ", 
		CutSceneManager::GetInstance()->GetStateName(CutSceneManager::GetInstance()->GetState()),
		CutSceneManager::GetInstance()->GetCutSceneCurrentTime(),
		CutSceneManager::GetInstance()->GetCutSceneCurrentFrame(),
		fwTimer::GetFrameCount(), 
		pAudioObj->GetObjectId(), 
		pAudioObj->GetDisplayName().c_str()
		);
}
#endif //!__NO_OUTPUT

//s
//void CCutSceneAudioEntity::AddAudioPlayBackRef()
//{
//	ms_AudioPlayBackRef+= 1;
//	
//}
//
//void CCutSceneAudioEntity::RemoveAudioPlayBackRef()
//{
//	ms_AudioPlayBackRef-= 2;
//}

//scrubbing for multiple audio needs more work to  be able to queue the audio correctly, from the last audio playback time
//void CCutSceneAudioEntity::GetCurrentAudioEntityPlayingBack(const cutsManager* pManager)
//{
//	const atArray<cutfEvent*>& pEventList = pManager->GetCutfFile()->GetEventList();
//
//	for(int i=0; i < pEventList.GetCount(); i++)
//	{
//		if(pEventList[i]->GetEventId() == CUTSCENE_PLAY_AUDIO_EVENT)
//		{
//			const cutfObjectIdEvent* pEvent = static_cast<const cutfObjectIdEvent*>(pEventList[i]); 
//
//			if(pEvent)
//			{
//				if(pManager->GetSeconds() > pEvent->GetTime())
//				{
//					ms_ObjectIdOfCurrentAudioObject = pEvent->GetObjectId(); 
//					ms_LastAudioplayBackTime = pEvent->GetTime(); 
//				}
//			}
//		}
//	}
//}


void CCutSceneAudioEntity::DispatchEvent(cutsManager* pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* UNUSED_PARAM(pEventArgs)/* =NULL */, const float UNUSED_PARAM(fTime), const u32 UNUSED_PARAM(StickyId))
{
	if (pObject == NULL)
	{
		Warningf("Audio entity has no object, audio will not be updated");
		return;
	}
	
	//if skipping dont dispatch any audio events
	const CutSceneManager* pCutSceneManager = static_cast<const CutSceneManager*>(pManager); 

	if(pCutSceneManager->WasSkipped() && camInterface::IsFadedOut())
	{
		if(ms_bIsPlaying)
		{
			cutsceneSoundEntityDebugf(pObject, "Cutscene skipped and faded out. Stopping audio");
			StopAllCutSceneAudioStreams(pCutSceneManager->WasSkipped()); 
			ms_bIsPlaying = false; 
		}
		
	}

	switch (iEventId)
	{
#if !__FINAL
		case CUTSCENE_STEP_BACKWARD_EVENT:
		case CUTSCENE_STEP_FORWARD_EVENT:
		case CUTSCENE_REWIND_EVENT:
		case CUTSCENE_FAST_FORWARD_EVENT:
		case CUTSCENE_PLAY_BACKWARDS_EVENT:
		case CUTSCENE_PAUSE_EVENT:
			{
				cutsceneSoundEntityDebugf(pObject, "DEBUG_PLAYBACK_EVENT - event: %d", iEventId);
				if(pCutSceneManager->GetCutfFile() && pCutSceneManager->GetScrubbingState() != cutsManager::SCRUBBING_NONE)
				{
					if(!m_bWasJogged)
					{
						cutsceneSoundEntityDebugf(pObject, "Scrubbing and not jogged, Stopping audio for load.");
						StopAllCutSceneAudioStreams(true); // "Skipped"
						m_bWasJogged = true;
						ms_bIsPlaying = false; 
						pManager->NeedToLoadBeforeResuming(); 
						pManager->SetIsLoaded(pObject->GetObjectId(), false);
					}			
				}
			}
			break;
		case CUTSCENE_RESTART_EVENT:
			{
				cutsceneSoundEntityDebugf(pObject, "CUTSCENE_RESTART_EVENT");
				if(pCutSceneManager->GetCutfFile())
				{
					
					m_bWasJogged = true;
					ms_bIsPlaying = false; 
					cutsceneSoundEntityDebugf(pObject, "Stopping audio.");
					StopAllCutSceneAudioStreams(true); // "Skipped"
				}
			}
			// intentional fall through
#endif
		case CUTSCENE_LOAD_AUDIO_EVENT:
			{
				//cutsceneSoundEntityDebugf(pObject, "CUTSCENE_LOAD_AUDIO_EVENT");
#if !__FINAL
				if(m_bWasJogged)
				{
					u32 TimeInMiliSeconds = (u32)(pCutSceneManager->GetTime() * 1000.0f); 

					cutsceneSoundEntityDebugf(pObject, "Skipping audio to time %u (from jog).", TimeInMiliSeconds);
					g_CutsceneAudioEntity.SkipCutsceneToTime(TimeInMiliSeconds);
					ms_bIsLoaded = false; 

					pManager->SetIsLoading(pObject->GetObjectId(), true);
				}
				else
#endif
				{
					if(pManager->IsSkippingPlayback() || pCutSceneManager->WasSkipped())
					{
						cutsceneSoundEntityDebugf(pObject, "CUTSCENE_LOAD_AUDIO_EVENT: Cutscene audio is skipping");
						pManager->SetIsLoaded(pObject->GetObjectId(), true);
						return;
					}

					pManager->SetIsLoading(pObject->GetObjectId(), true);

					if (g_CutsceneAudioEntity.IsCutsceneTrackPrepared())
					{
						cutsceneSoundEntityDebugf(pObject, "CUTSCENE_LOAD_AUDIO_EVENT: Cutscene audio is prepared");
						pManager->SetIsLoaded(pObject->GetObjectId(), true);
					}
					else
					{
						cutsceneSoundEntityDebugf(pObject, "CUTSCENE_LOAD_AUDIO_EVENT - Cutscene audio is not prepared");
					}
				}


			}
			break;
		


		case CUTSCENE_STOP_AUDIO_EVENT:
			{
		/*		cutsceneSoundEntityDebugf(pObject, "CUTSCENE_STOP_AUDIO_EVENT");		
				if(ms_bIsPlaying)
				{
					StopAllCutSceneAudioStreams(); 
					ms_bIsPlaying = false; 
				}*/
			}
			break;

		case CUTSCENE_UPDATE_EVENT:
			{
				if(pManager->IsSkippingPlayback() || pCutSceneManager->WasSkipped())
				{
					pManager->SetIsLoaded(pObject->GetObjectId(), true);
					return;
				}
				
			}
			break; 

		case CUTSCENE_UPDATE_LOADING_EVENT:
			{
				cutsceneSoundEntityDebugf(pObject, "CUTSCENE_UPDATE_LOADING_EVENT");
				if(pManager->IsSkippingPlayback() || pCutSceneManager->WasSkipped())
				{
					cutsceneSoundEntityDebugf(pObject, "CUTSCENE_UPDATE_LOADING_EVENT: Cutscene audio is skipping ");
					pManager->SetIsLoaded(pObject->GetObjectId(), true); 
					return; 
				}
				
				if(!ms_bIsLoaded)
				{
					cutsceneSoundEntityDebugf(pObject, "CUTSCENE_UPDATE_LOADING_EVENT: Cutscene audio is loading");
#if __BANK
					if(pCutSceneManager->GetDebugManager().m_bPrintModelLoadingToDebug)
					{
						cutsceneDisplayf("Waiting to load Audio: %s", pObject->GetDisplayName().c_str()); 
					}
#endif						
				
					if(g_CutsceneAudioEntity.IsCutsceneTrackPrepared())
					{
						cutsceneSoundEntityDebugf(pObject, "CUTSCENE_UPDATE_LOADING_EVENT: Cutscene audio is prepared");
						pManager->SetIsLoaded(pObject->GetObjectId(), true);
						ms_bIsLoaded = true; 
					}
				}
				else
				{
					cutsceneSoundEntityDebugf(pObject, "CUTSCENE_UPDATE_LOADING_EVENT: Cutscene audio is already loaded");
					pManager->SetIsLoaded(pObject->GetObjectId(), true);
				}
			
			}
			break;
		
		case CUTSCENE_START_OF_SCENE_EVENT:
		case CUTSCENE_PLAY_AUDIO_EVENT: 
			{
				cutsceneSoundEntityDebugf(pObject, "CUTSCENE_PLAY_AUDIO_EVENT");
				
				if(pManager->IsSkippingPlayback())
				{
					pManager->SetIsLoaded(pObject->GetObjectId(), true);
					return;
				}
				
				if(!ms_bIsPlaying)
				{
					if(g_CutsceneAudioEntity.IsCutsceneTrackPrepared())
					{
						pManager->SetIsLoaded(pObject->GetObjectId(), true);
						ms_bIsLoaded = true; 

						//cutsceneDisplayf("Play Audio Track: %s with offset: %f at time %f ",pObject->GetDisplayName().c_str(), ((float) m_iAudioTime) / 1000.0f , pCutSceneManager->GetCutSceneCurrentTime());

						Play();
						cutsceneSoundEntityDebugf(pObject, "Triggering audio");
						ms_bIsPlaying = true; 
						ms_bIsLoaded = false;
#if !__FINAL
						m_bWasJogged = false;
#endif	
					}
					else
					{
						cutsceneSoundEntityDebugf(pObject, "Not triggering audio - cutscene track is not prepared");
					}
				}
				else
				{
					cutsceneSoundEntityDebugf(pObject, "Not triggering audio - audio already playing");
				}
			}
			break;

#if !__FINAL
		//is time stepped from pause break need to deal with this 
		case CUTSCENE_PLAY_EVENT:
			{
				cutsceneSoundEntityDebugf(pObject, "CUTSCENE_PLAY_EVENT");
				if(pManager->IsResumingPlaybackFromBeingPaused() && !ms_bIsPlaying && pCutSceneManager->GetCutfFile())
				{
					//// this should hit if we jogged forward or backwards
					if(g_CutsceneAudioEntity.IsCutsceneTrackPrepared())
					{
						cutsceneSoundEntityDebugf(pObject, "Triggering audio after being paused");
						Play();						
						ms_bIsPlaying = true; 
						ms_bIsLoaded = false;
						m_bWasJogged = false;
					}
					else
					{
						pManager->SetIsLoading(pObject->GetObjectId(), true);
						pManager->NeedToLoadBeforeResuming(); 
					}
				}
			}
			break;
#endif
		case CUTSCENE_CANCEL_LOAD_EVENT:
			{
				cutsceneSoundEntityDebugf(pObject, "CUTSCENE_CANCEL_LOAD_EVENT - stopping audio");
				StopAllCutSceneAudioStreams(true); // "Skipped"
				ms_bIsPlaying = false; 
		
				pManager->SetIsLoaded(pObject->GetObjectId(), true); 

			}
			break;

		case CUTSCENE_STOP_EVENT:
			{
				cutsceneSoundEntityDebugf(pObject, "CUTSCENE_STOP_EVENT");
				if(ms_bIsPlaying)
				{
					cutsceneSoundEntityDebugf(pObject, "Stopping audio");
					StopAllCutSceneAudioStreams(pCutSceneManager->WasSkipped() || pCutSceneManager->GetShouldStopNow()); 
					ms_bIsPlaying = false; 
				}
			}
			break;

		case CUTSCENE_END_OF_SCENE_EVENT:
			{
				cutsceneSoundEntityDebugf(pObject, "CUTSCENE_END_OF_SCENE_EVENT");
				if(ms_bIsPlaying)
				{
					cutsceneSoundEntityDebugf(pObject, "Stopping audio");
					StopAllCutSceneAudioStreams(pCutSceneManager->WasSkipped()); 
					ms_bIsPlaying = false; 
				}
			}
			break;

	}
}

