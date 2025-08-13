
//Rage headers
#include "cutfile/cutfobject.h"
#include "cutscene/cutsevent.h"
#include "cutscene/cutseventargs.h"
#include "cutscene/cutsmanager.h"
#include "crclip/clipdictionary.h"
#include "cranimation/animation.h"
#include "fwanimation/animmanager.h"
#include "fwscene/stores/clipdictionarystore.h"
#include "fwsys/timer.h"

//Game headers
#include "cutscene/CutSceneManagerNew.h"
#include "cutscene/CutSceneAnimManager.h"
#include "Cutscene/cutscene_channel.h"
#include "streaming/streaming.h "

ANIM_OPTIMISATIONS()

///////////////////////////////////////////////////////////////////////////////////////////////////

CCutSceneAnimMgrEntity::CCutSceneAnimMgrEntity(const cutfObject* pObject )
:cutsAnimationManagerEntity(pObject)
{
	
}

///////////////////////////////////////////////////////////////////////////////////////////////////

CCutSceneAnimMgrEntity::~CCutSceneAnimMgrEntity()
{
	UnloadAllDictionaries(); 

#if __DEV
	cutsceneDebugf1("~CCutSceneAnimMgrEntity() ");
#endif
}

void CCutSceneAnimMgrEntity::UnloadAllDictionaries()
{
	for(s32 iBuffer =0; iBuffer < m_RequestedDicts.GetCount(); iBuffer++ )
	{
		UnloadDictionary(NULL, iBuffer);
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
//Purpose: Requests the clip dictionary

bool CCutSceneAnimMgrEntity::LoadDictionary( cutsManager *pManager, s32 iObjectId, const char* pName, int iBuffer )
{
	strLocalIndex iAnimDictIndex = fwAnimManager::FindSlot(pName);  // get the index
	
	cutsceneAnimMgrDebugf("Loading Dictionary:%s-%d", pName, iBuffer);

	if (cutsceneVerifyf(iAnimDictIndex != -1 , "Anim dict %s has returned an invalid slot, will not attempt to load this dictionary" , pName)) 
	{
		if (cutsceneVerifyf(CStreaming::IsObjectInImage(iAnimDictIndex, fwAnimManager::GetStreamingModuleId()), "Anim dictionary %s is not in the image" , pName)) 
		{
			if (m_RequestedDicts.GetCount() == 0 )
			{
				//create enough requests for the number of sections
				for(int i = 0; i < pManager->GetSectionStartTimeList().GetCount(); i++ )
				{
					DictionaryRequests NewRequest; 
					m_RequestedDicts.PushAndGrow(NewRequest);
				}
			}
			
			// Try and stream in the animation block but check we dont have that id already if so say its streamed.
			if(m_RequestedDicts[iBuffer].RequestId == iBuffer )
			{
				return true; 
			}
			
			m_RequestedDicts[iBuffer].RequestId = iBuffer; 
			m_RequestedDicts[iBuffer].m_Request.Request(iAnimDictIndex,fwAnimManager::GetStreamingModuleId(), STRFLAG_CUTSCENE_REQUIRED | STRFLAG_PRIORITY_LOAD);

			//store this on the rage side so it can dispatch the correct anims
			safecpy (m_Dictionaries[iBuffer].cName, pName, sizeof(m_Dictionaries[iBuffer].cName));

			//we have requested a anim let tell the manager to wait
			pManager->SetIsLoading( iObjectId, true );

			return true;
		}
	}		
	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// checks the streaming status of the clip dictionary and when loaded fills out the base class data

bool CCutSceneAnimMgrEntity::HasDictionaryLoaded()
{
	//check that all the requests we have made have loaded
	for( int index = 0; index < m_RequestedDicts.GetCount(); index++)
	{
		//check that its a valid request id
		if(m_RequestedDicts[index].RequestId != -1)
		{
			if (!m_RequestedDicts[index].m_Request.HasLoaded())
			{
				return false;  
			}
			//fill out the relevant rage info if we don't have it
			if(!(m_Dictionaries[index].bAnimDictIsResource ||m_Dictionaries[index].bClipDictIsResource ))
			{
				crClipDictionary* pClipDict  = fwAnimManager::Get(strLocalIndex(m_RequestedDicts[index].m_Request.GetRequestId()));
				
				if (cutsceneVerifyf(pClipDict, "Didn't find dictionary for given anim"))
				{
					crAnimDictionary* pAnimDict = pClipDict->GetAnimationDictionary();
					//cuts anim manager data used to play anims on model objects 
					m_Dictionaries[index].pAnimDictionary = pAnimDict;
					m_Dictionaries[index].pClipDictionary = pClipDict; 
					m_Dictionaries[index].bClipDictIsResource = true;
					m_Dictionaries[index].bAnimDictIsResource = true;

					CutSceneManager::GetInstance()->DictionaryLoadedCB(m_Dictionaries[index].pClipDictionary, index);

					
#if __ASSERT
					ValidateClipDictionary(*pClipDict, "CCutSceneAnimMgrEntity");
#endif // __ASSERT
				}
			}
		}
	}
	
	return true;
}


bool CCutSceneAnimMgrEntity::IsDictionaryLoaded(int Buffer)
{
	
	if(Buffer < m_RequestedDicts.GetCount() && Buffer > -1)
	{
		if(m_RequestedDicts[Buffer].RequestId != -1)
		{
			if (m_RequestedDicts[Buffer].m_Request.HasLoaded())
			{
				return true;  
			}
		}
	}
	return false; 
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//do a pointer fix up on our data so that it does not get lost when defragged

bool CCutSceneAnimMgrEntity::UnloadDictionary( cutsManager *BANK_ONLY(pManager), int iBuffer )
{
	if (m_RequestedDicts.GetCount()> 0)
	{
		//only unstream an object that is valid
		if(m_RequestedDicts[iBuffer].RequestId != -1)
		{
			// remove the anim dict:
			m_RequestedDicts[iBuffer].RequestId = -1; 
			
			cutsceneAnimMgrDebugf("Unloading Dictionary:%s-%d", m_Dictionaries[iBuffer].cName, iBuffer);

			//Reset our rage data slot for more anims
			m_Dictionaries[iBuffer].cName[0] = 0;
			m_Dictionaries[iBuffer].bAnimDictIsResource = false;
			m_Dictionaries[iBuffer].bClipDictIsResource = false;
			m_Dictionaries[iBuffer].pClipDictionary = NULL;
			m_Dictionaries[iBuffer].pAnimDictionary = NULL;
			
#if __BANK
			CutSceneManager* CutManager = static_cast<CutSceneManager*>(pManager);
			CutManager->OutputMoveNetworkForEntities(); 
			m_HasUnloadDictionaryBeenCalledThisFrame = true; 
#endif
			m_RequestedDicts[iBuffer].m_Request.ClearRequiredFlags(STRFLAG_CUTSCENE_REQUIRED);

			m_RequestedDicts[iBuffer].m_Request.Release();	//told to be cached - when scrubbing and loading dictionaries- saves dictionary tracking issues
			
		/*	if (pManager)
			{
				pManager->SetIsUnloaded(GetObject()->GetObjectId(), true);
			}*/
		}
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//possibly implement an unloading of the assets update

void CCutSceneAnimMgrEntity::DispatchEvent( cutsManager* pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs, const float UNUSED_PARAM(fTime), const u32 UNUSED_PARAM(StickyId) )
{
	switch ( iEventId )
	{
		case CUTSCENE_UPDATE_LOADING_EVENT:
		case CUTSCENE_UPDATE_EVENT:
		{
			if ( HasDictionaryLoaded() )
			{
				pManager->SetIsLoaded( pObject->GetObjectId(), true );	//tell the manager we have loaded our dictionaries
			}
			else
			{
				pManager->SetIsLoading(pObject->GetObjectId(), true ); 
			}
		
#if __BANK		
			m_HasUnloadDictionaryBeenCalledThisFrame = false;
 #endif

		}
		break;

		case CUTSCENE_UPDATE_UNLOADING_EVENT:
		{
			if ( pManager->IsUnloading( pObject->GetObjectId() ) )
			{
				pManager->SetIsUnloaded( pObject->GetObjectId(), true );
			}
		}
		break;
		
		case CUTSCENE_CANCEL_LOAD_EVENT:
			{
				UnloadAllDictionaries(); 
				//pManager->SetIsLoaded( pObject->GetObjectId(), true );
			}
		break;

		case CUTSCENE_UNLOADED_EVENT:
			{		
				UnloadAllDictionaries(); 

				cutsceneDebugf1("CUTSCENE_UNLOADED_EVENT"); 
			}
			break;
			
		//case CUTSCENE_SET_ANIM_EVENT:
		//	{
		//		//For static lights, which have no anim data, create an event so it knows it should update.
		//		//Static lights will get switched off when a CUTSCENE_CLEAR_ANIM_EVENT is dispatched.

		//		if(pEventArgs && pEventArgs->GetType() == CUTSCENE_OBJECT_ID_NAME_EVENT_ARGS_TYPE)
		//		{
		//			const cutfObjectIdNameEventArgs *pObjectIdNameEventArgs = static_cast<const cutfObjectIdNameEventArgs *>( pEventArgs );
		//			
		//			cutsEntity *pEntity = pManager->GetEntityByObjectId( pObjectIdNameEventArgs->GetObjectId());
		//			const cutfObject *pTargetObject = pManager->GetObjectById( pObjectIdNameEventArgs->GetObjectId() );
		//			
		//			if (pEntity && pTargetObject)
		//			{
		//				if(pTargetObject->GetType() == CUTSCENE_LIGHT_OBJECT_TYPE)
		//				{
		//					const cutfLightObject* pLight = static_cast<const cutfLightObject*>(pTargetObject); 
		//					
		//					if(pLight->IsLightStatic())
		//					{
		//						cutsUpdateEventArgs cutsUpdateEventArgs(0.0f, 0.0f, 0.0f, 0.0f); 
		//						pEntity->DispatchEvent( pManager, pTargetObject, CUTSCENE_SET_CLIP_EVENT, &cutsUpdateEventArgs);
		//					}
		//				}
		//			}
		//		}
		//	}
		//	break;

		case CUTSCENE_UNLOAD_ANIM_DICT_EVENT:
		case CUTSCENE_CLEAR_ANIM_EVENT:
		case CUTSCENE_START_OF_SCENE_EVENT:
		case CUTSCENE_END_OF_SCENE_EVENT:
		case CUTSCENE_PLAY_EVENT:	
		case CUTSCENE_PAUSE_EVENT:
		case CUTSCENE_STOP_EVENT:
		case CUTSCENE_STEP_FORWARD_EVENT:
		case CUTSCENE_STEP_BACKWARD_EVENT:
		case CUTSCENE_FAST_FORWARD_EVENT:
		case CUTSCENE_REWIND_EVENT:
		case CUTSCENE_RESTART_EVENT:
		case CUTSCENE_SCENE_ORIENTATION_CHANGED_EVENT:
			break;

		default:
		{
			//rdr2CutSceneGlobal::DebugOutput( DEBUG_LEVEL_0, DBGMSG_WARNING, "%s event '%s' (%d) not supported on entity type '%s'",
			//	pManager->GetDisplayTime(), cutfEvent::GetDisplayName(iEventId), iEventId, pObject->GetTypeName().c_str());
		}
		break;
	}

	cutsAnimationManagerEntity::DispatchEvent(pManager, pObject, iEventId, pEventArgs);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#if __BANK

void CCutSceneAnimMgrEntity::PrintStreamingRequests()
{
	for(int i = 0; i < m_RequestedDicts.GetCount(); i ++)
	{
		if(m_RequestedDicts[i].RequestId != -1)
		{
			if(!m_RequestedDicts[i].m_Request.HasLoaded())
			{
				u32 uSize = 0;
				strIndex index = m_RequestedDicts[i].m_Request.GetIndex();
				strStreamingInfo *pStreamingInfo = strStreamingEngine::GetInfo().GetStreamingInfo(index);
				if(pStreamingInfo)
				{
					uSize += pStreamingInfo->ComputeVirtualSize(index, true);
				}
				cutsceneAnimMgrDebugf("Loading: Dictionary:%s Size:%u", m_Dictionaries[i].cName, uSize);
			}
		}
	}
}

#endif // __BANK

///////////////////////////////////////////////////////////////////////////////////////////////////

//const char * CCutSceneAnimMgrEntity::GetCurrentAnimDict()
//{
//	
//	return m_dictionaryData[].cName;
//
//}

///////////////////////////////////////////////////////////////////////////////////////////////////

#if __BANK

void CCutSceneAnimMgrEntity::CommonDebugStr( char * debugStr)
{
	sprintf(debugStr, "%s - CT(%6.2f) - CF(%u) - FC(%u) - Anim Manager - ", 
		CutSceneManager::GetInstance()->GetStateName(CutSceneManager::GetInstance()->GetState()),
		CutSceneManager::GetInstance()->GetCutSceneCurrentTime(),
		CutSceneManager::GetInstance()->GetCutSceneCurrentFrame(),
		fwTimer::GetFrameCount());
}


#endif //__BANK
