/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutSceneFadeEntity.cpp
// PURPOSE : 
// AUTHOR  : Thomas French
// STARTED : 12/8/2009
//
/////////////////////////////////////////////////////////////////////////////////

//Rage Headers
#include "cutfile/cutfevent.h"
#include "cutscene/cutsevent.h"
#include "vector/colors.h"

//Game Headers
#include "Cutscene\CutSceneManagerNew.h"
#include "cutscene/CutSceneFadeEntity.h"
#include "camera/CamInterface.h"
#include "text/TextFile.h"
#include "text/messages.h"

/////////////////////////////////////////////////////////////////////////////////

ANIM_OPTIMISATIONS ()

CCutSceneFadeEntity::CCutSceneFadeEntity()
:cutsUniqueEntity()
{
}

/////////////////////////////////////////////////////////////////////////////////

CCutSceneFadeEntity::CCutSceneFadeEntity(s32 iObjectid, const cutfObject* pObject)
:cutsUniqueEntity( pObject )
{
	m_iObjectId = iObjectid;
}

/////////////////////////////////////////////////////////////////////////////////

CCutSceneFadeEntity::~CCutSceneFadeEntity()
{

}

/////////////////////////////////////////////////////////////////////////////////
//Handles events coming into the fade entity, interface to game camera fades

void CCutSceneFadeEntity::DispatchEvent(cutsManager* pManager, const cutfObject* UNUSED_PARAM(pObject), s32 iEventId, const cutfEventArgs* pEventArgs, const float UNUSED_PARAM(fTime), const u32 UNUSED_PARAM(StickyId))
{
	switch ( iEventId )
	{
	case CUTSCENE_FADE_OUT_EVENT:
		{	
			if(pManager->IsSkippingPlayback() && pManager->HasFadeOutAtEndBeenCalled())
			{
				return; 
			}
			
			if (pEventArgs && pEventArgs->GetType() == CUTSCENE_SCREEN_FADE_EVENT_ARGS_TYPE)
			{
				const cutfScreenFadeEventArgs* pScreenFadeEventArgs = static_cast<const cutfScreenFadeEventArgs*>(pEventArgs);
				//convert the float from the data file in seconds to milliseconds, that the fade expects
				s32 iFadeTime = (s32) (pScreenFadeEventArgs->GetFloat1() * 1000.0f);
				
				Color32 FadeColor = pScreenFadeEventArgs->GetColor(); 
				
				//this fixes a problem in the fade system where is does not deal with colours that have alphas less than 200
				FadeColor.SetAlpha(255); 
				camInterface::FadeOut(iFadeTime, true, FadeColor);
			}
			
		}
		break;
	case CUTSCENE_FADE_IN_EVENT:
		{
			//dont dispatch mid scene fades cause we are skipping to the end, only dispatch if we are fading back to game play
			if(pManager->IsSkippingPlayback() && !pManager->HasFadeInAtEndBeenCalled())
			{
				return; 
			}

			//@@: location CCUTSCENEFADEENTITY_DISPATCHEVENT_FADE_IN
			if (pEventArgs && pEventArgs->GetType() == CUTSCENE_SCREEN_FADE_EVENT_ARGS_TYPE)
			{
				const cutfScreenFadeEventArgs* pScreenFadeEventArgs = static_cast<const cutfScreenFadeEventArgs*>(pEventArgs);
				
				s32 iFadeTime = (s32) (pScreenFadeEventArgs->GetFloat1() * 1000.0f);

				Color32 FadeColor = pScreenFadeEventArgs->GetColor(); 

				//this fixes a problem in the fade system where is does not deal with colours that have alphas less tahn 200
				FadeColor.SetAlpha(255); 

				camInterface::FadeIn(iFadeTime, FadeColor);
			}
		}
		break;

	case CUTSCENE_RESTART_EVENT:
		{

		}
		break;

	case CUTSCENE_PAUSE_EVENT:
		{
			//TF add some way to pause a fade 
		}
		break;

	case CUTSCENE_UPDATE_FADING_OUT_AT_BEGINNING_EVENT:
	case CUTSCENE_UPDATE_LOADING_EVENT:
		{

		}
		break;

	case CUTSCENE_UPDATE_UNLOADING_EVENT:
	case CUTSCENE_UNLOADED_EVENT:
	case CUTSCENE_START_OF_SCENE_EVENT:
	case CUTSCENE_END_OF_SCENE_EVENT:
	case CUTSCENE_PLAY_EVENT:
	case CUTSCENE_STOP_EVENT:
	case CUTSCENE_CANCEL_LOAD_EVENT:
	case CUTSCENE_SCENE_ORIENTATION_CHANGED_EVENT:
	case CUTSCENE_STEP_BACKWARD_EVENT:
	case CUTSCENE_STEP_FORWARD_EVENT:
	case CUTSCENE_REWIND_EVENT:
	case CUTSCENE_FAST_FORWARD_EVENT:
	case CUTSCENE_SHOW_DEBUG_LINES_EVENT:
		{
			// handle silently
		}
		break;

/*	default:
		{
			Warningf("%s event '%s' (ID=%d) not supported on entity type '%s'",
				pManager->GetDisplayTime(), cutfEvent::GetDisplayName(iEventId), iEventId, pObject->GetTypeName());
		}
		break;
*/
	}
}
/////////////////////////////////////////////////////////////////////////////////
//	SUBTITLES
/////////////////////////////////////////////////////////////////////////////////

CCutSceneSubtitleEntity::CCutSceneSubtitleEntity()
:cutsUniqueEntity()
{
}

/////////////////////////////////////////////////////////////////////////////////

CCutSceneSubtitleEntity::CCutSceneSubtitleEntity(const cutfObject* pObject)
:cutsUniqueEntity( pObject )
{
}

/////////////////////////////////////////////////////////////////////////////////

CCutSceneSubtitleEntity::~CCutSceneSubtitleEntity()
{

}

/////////////////////////////////////////////////////////////////////////////////

void CCutSceneSubtitleEntity::DispatchEvent(cutsManager* pManager, const cutfObject* UNUSED_PARAM(pObject), s32 iEventId, const cutfEventArgs* pEventArgs, const float UNUSED_PARAM(fTime), const u32 UNUSED_PARAM(StickyId))
{
	switch ( iEventId )
	{
		case CUTSCENE_SHOW_SUBTITLE_EVENT:
			{	
				if (pEventArgs && pEventArgs->GetType() == CUTSCENE_SUBTITLE_EVENT_ARGS_TYPE)
				{
					// don't process subtitles after a skip
					if (pManager && static_cast<CutSceneManager*>(pManager)->WasSkipped())
						return;

					const cutfSubtitleEventArgs* pSubtitleArgs = static_cast<const cutfSubtitleEventArgs*>(pEventArgs); 
					
					atHashString LabelHash = pSubtitleArgs->GetName(); 
					
					char *pString = NULL; 
#if !__FINAL
					pString = TheText.Get(LabelHash.GetHash(),LabelHash.GetCStr());
#else
					if(TheText.DoesTextLabelExist(LabelHash.GetHash()))
					{
						pString = TheText.Get(LabelHash.GetHash(),"");
					}
					else
					{
						return; 
					}
#endif

					u32 uSubtitleDuration =  (u32)(pSubtitleArgs->GetSubtitleDuration() * 1000.0f); 
				
					CMessages::AddMessage( pString, TheText.GetBlockContainingLastReturnedString(), 
						uSubtitleDuration, false, false, PREVIOUS_BRIEF_NO_OVERRIDE, NULL, 0, NULL, 0, false);
				}

			}
			break;
	}

}