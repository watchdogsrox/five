/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutsSceneLightEntity.cpp
// PURPOSE : 
// AUTHOR  : Thomas French
// STARTED : 
//
/////////////////////////////////////////////////////////////////////////////////

//Rage Headers
#include "cutscene/cutsevent.h"
#include "cutscene/cutseventargs.h"
#include "fwanimation/animmanager.h"

//Game Headers
#include "cutscene/CutSceneLightEntity.h"
#include "cutscene/CutSceneManagerNew.h"
#include "Cutscene/cutscene_channel.h"
#include "camera/CamInterface.h"


/////////////////////////////////////////////////////////////////////////////////

ANIM_OPTIMISATIONS()

/////////////////////////////////////////////////////////////////////////////////

CCutSceneLightEntity::CCutSceneLightEntity (const cutfObject* pObject)
:cutsUniqueEntity ( pObject )
{
	m_pLight = rage_new CCutSceneLight(static_cast<const cutfLightObject*> (pObject));
}

/////////////////////////////////////////////////////////////////////////////////

CCutSceneLightEntity::~CCutSceneLightEntity ()
{
	delete m_pLight;
	m_pLight = NULL;
}

/////////////////////////////////////////////////////////////////////////////////
//	PURPOSE: Dispatch events to game light objects. 
void CCutSceneLightEntity::DispatchEvent(cutsManager* pManager, const cutfObject* BANK_ONLY(pObject), s32 iEventId, const cutfEventArgs* pEventArgs, const float fTime,  const u32 UNUSED_PARAM(StickyId))
{
	switch ( iEventId )
	{
		case CUTSCENE_START_OF_SCENE_EVENT:
			{
#if __BANK
				if (m_pLight)
				{
					m_pLight->SetLightInfo(pManager->GetEditLightInfo( pObject->GetObjectId() ));
				}
#endif
			}
			break;
		
		case CUTSCENE_UPDATE_EVENT:
		{
			//Some entities should only get updated in the post scene, after the animation update, check if this should be updated here 
			CutSceneManager* pCutManager = static_cast<CutSceneManager*>(pManager); 
			
			bool pushLightEarly = false; 

			if(m_pLight->CanUpdateStaticLight() && pCutManager->IsFlagSet(m_pLight->GetLightCutObject()->GetLightFlags(), CUTSCENE_LIGHTFLAG_IS_CHARACTER_LIGHT))
			{
				pushLightEarly = true; 
			//	cutsceneLightEntityDebugf("CUTSCENE_SET_LIGHT_EVENT: Pushing ped light"); 
			}

			// if there is a camera cut this frame (or we're on the first frame) and we're shadowed, lets add the light to the previous light list (so shadows know about it) as well as the normal post scene add
			if (m_pLight)
			{

				// during a camera cut, we add the light to the previous light list before the scene, and then to the real list again after. this is so shadows are not a frame behind
				if (m_pLight->GetLightCutObject())
				{
					float cutsceneTime = pCutManager->GetCutSceneCurrentTime();
					bool isShadowLight =  pCutManager->IsFlagSet(m_pLight->GetLightCutObject()->GetLightFlags(), (CUTSCENE_LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS | CUTSCENE_LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS));
					bool isCoronaOnly = pCutManager->IsFlagSet(m_pLight->GetLightCutObject()->GetLightFlags(), CUTSCENE_LIGHTFLAG_CORONA_ONLY);
					bool staticLight = (m_pLight->GetLightCutObject()->GetType() == CUTSCENE_LIGHT_OBJECT_TYPE) && m_pLight->CanUpdateStaticLight();
					bool animatedLight = (m_pLight->GetLightCutObject()->GetType() == CUTSCENE_ANIMATED_LIGHT_OBJECT_TYPE) && (m_pLight->GetCutSceneAnimation().GetClip()!=NULL);
					if ((pCutManager->GetHasCutThisFrame() || cutsceneTime==0.0f) && (staticLight || animatedLight) && isShadowLight && !isCoronaOnly)
					{
						pushLightEarly = true; 
					}
				}

				if (pCutManager->GetPostSceneUpdate() || pushLightEarly)
				{				
					m_pLight->UpdateLight(pManager);
				}
			}
		}
		break;
		
		case CUTSCENE_SET_LIGHT_EVENT:
			{
#if __BANK
				if (m_pLight && m_pLight->GetLightInfo() == NULL)
				{
					m_pLight->SetLightInfo(pManager->GetEditLightInfo( pObject->GetObjectId()));
				}
#endif
				
				cutsceneLightEntityDebugf("CUTSCENE_SET_LIGHT_EVENT"); 
				
				if(m_pLight->GetLightCutObject()->GetType() ==  CUTSCENE_LIGHT_OBJECT_TYPE)
				{
					m_pLight->SetCanUpdateStaticLight(true); 
				}
				
				if(pEventArgs && pEventArgs->GetType() == CUTSCENE_TRIGGER_LIGHT_EVENT_ARGS_TYPE)
				{
					const cutfTriggerLightEffectEventArgs* pLightArgs = static_cast<const cutfTriggerLightEffectEventArgs*>(pEventArgs); 

					if (pLightArgs && m_pLight)
					{
						m_pLight->SetAttachParentId(pLightArgs->GetAttachParentId());
						m_pLight->SetAttachBoneHash(pLightArgs->GetAttachBoneHash());
					}
				}
			}
			break; 

		case CUTSCENE_CLEAR_LIGHT_EVENT:
			{
				CutSceneManager* pCutManager = static_cast<CutSceneManager*>(pManager); 
				if(fTime < pCutManager->GetEndTime())
				{
					cutsceneLightEntityDebugf("CUTSCENE_CLEAR_LIGHT_EVENT"); 
					if (m_pLight)
					{
						m_pLight->SetAttachParentId(-1);
						m_pLight->SetAttachBoneHash(0);
						
						if(m_pLight->GetLightCutObject()->GetType() ==  CUTSCENE_LIGHT_OBJECT_TYPE)
						{
							m_pLight->SetCanUpdateStaticLight(false); 	
						}
					}
				}
			}
		break; 

		case CUTSCENE_SET_CLIP_EVENT:
		{
			if (pEventArgs && pEventArgs->GetType() == CUTSCENE_CLIP_EVENT_ARGS_TYPE)
			{
				CutSceneManager* pCutManager = static_cast<CutSceneManager*>(pManager); 
				
				const cutsClipEventArgs *pClipEventArgs = static_cast<const cutsClipEventArgs *>( pEventArgs );
				if (m_pLight)
				{
					if (Verifyf((fwAnimManager::FindSlot(pClipEventArgs->GetAnimDict()) != -1  && pClipEventArgs->GetClip()), "Invalid anim or dictionary playin on a light"))
					{
						//Check we are not already playing this anim a fix for a debug issue when playing backwards
						if (m_pLight->GetCutSceneAnimation().GetClip() !=  pClipEventArgs->GetClip())
						{
							m_pLight->GetCutSceneAnimation().Init(pClipEventArgs->GetClip(), pCutManager->GetSectionStartTime(pCutManager->GetCurrentSection()));
#if __BANK							
							cutsceneDebugf1 ("Light Entity id: %d (%s) : CUTSCENE_SET_CLIP_EVENT", pObject->GetObjectId(), pObject->GetDisplayName().c_str());
#endif		
						}
					}
				}
			}
		}
		break;

		case CUTSCENE_SET_ANIM_EVENT:
			{
				cutsceneAssertf(0, "Pass to Default Cutscene Animation: The light file has been merged badly with the cutfile %s, CUTSCENE_SET_ANIM_EVENT events should be dispatched to the anim manager no the light object itself", pManager->GetCutsceneName());

#if CUTSCENE_LIGHT_EVENT_LOGGING
					const cutfLightObject *pLightObject = m_pLight->GetLightCutObject();
					if(pLightObject->GetLightFlags() & (CUTSCENE_LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS | CUTSCENE_LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS))
					{
						cutsDisplayf("\t%u\t%.2f\t%s\tACTIVATE\t%.2f", fwTimer::GetFrameCount(), pManager->GetTime(), pObject->GetDisplayName().c_str(), pLightObject->GetActivateTime());
					}
#endif
			}
			break; 
 

		case CUTSCENE_CLEAR_ANIM_EVENT:
		{
			if(m_pLight && m_pLight->GetLightCutObject()->GetType() ==  CUTSCENE_ANIMATED_LIGHT_OBJECT_TYPE)
			{
				m_pLight->GetCutSceneAnimation().Remove();	
			}
				
		}
		break;

		case CUTSCENE_PLAY_EVENT:
		case CUTSCENE_UPDATE_LOADING_EVENT:
		case CUTSCENE_STOP_EVENT:
		case CUTSCENE_STEP_FORWARD_EVENT:
		case CUTSCENE_STEP_BACKWARD_EVENT:
		case CUTSCENE_FAST_FORWARD_EVENT:
		case CUTSCENE_REWIND_EVENT:
		case CUTSCENE_RESTART_EVENT:
			break;

		default:
			{

			}
			break;
	}


//this is here to dispatch debug draw event for this entity.
#if __BANK
	cutsEntity::DispatchEvent(pManager, pObject, iEventId, pEventArgs); 
#endif
}

#if __DEV
void CCutSceneLightEntity::DebugDraw() const 
{
	if(m_pLight && m_pLight->GetLightCutObject())
	{
		Matrix34 cutsceneAnimMat;
		CutSceneManager::GetInstance()->GetSceneOrientationMatrix(cutsceneAnimMat);
		Vector3 lightWorldPos = cutsceneAnimMat * m_pLight->GetLightCutObject()->GetLightPosition();
		float mag = (camInterface::GetPos() - lightWorldPos).Mag();

		if(m_pLight->GetLightCutObject()->GetType() ==  CUTSCENE_LIGHT_OBJECT_TYPE)
		{
			m_pLight->DisplayDebugLightinfo(m_pLight->GetLightCutObject()->GetDisplayName().c_str(), false, mag); 
		}
		else if(m_pLight->GetCutSceneAnimation().GetClip())
		{
			m_pLight->DisplayDebugLightinfo(m_pLight->GetLightCutObject()->GetDisplayName().c_str(), true , mag); 
		}
	}
}
#endif


#if !__NO_OUTPUT

void CCutSceneLightEntity::CommonDebugStr(const cutfObject* pObject, char * debugStr)
{
	if (!pObject)
	{
		return;
	}

	sprintf(debugStr, "%s - CT(%6.2f) - CF(%u) - FC(%u) - Light Entity(%p, %d, %s)", 
		CutSceneManager::GetInstance()->GetStateName(CutSceneManager::GetInstance()->GetState()),
		CutSceneManager::GetInstance()->GetCutSceneCurrentTime(),
		CutSceneManager::GetInstance()->GetCutSceneCurrentFrame(),
		fwTimer::GetFrameCount(), 
		pObject, 
		pObject->GetObjectId(), 
		pObject->GetDisplayName().c_str());
}

#endif // !__NO_OUTPUT

/////////////////////////////////////////////////////////////////////////////////