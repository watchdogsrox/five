/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutsSceneParticleEffectsEntity.cpp
// PURPOSE : The enitiy class is the interface between rage and the game side
//			 cut scene objects. For each object a entity is created, this 
//			 entity recieves events from the manager invokes the correct function
//			 on the game object.  
//			 Particle effects entity: Passes the anim to the object
//			 Calls the update function. 
//			 Removes the animation.
// AUTHOR  : Thomas French
// STARTED : 
//
/////////////////////////////////////////////////////////////////////////////////

//Rage Headers
#include "cutscene/cutsevent.h"
#include "cutscene/cutseventargs.h"
#include "fwanimation/animmanager.h"


//Game Headers
#include "cutscene/CutSceneManagerNew.h"
#include "cutscene/CutSceneParticleEffectEntity.h"
#include "cutscene/AnimatedModelEntity.h"
#include "Cutscene/cutscene_channel.h"
#include "Vfx/Systems/VfxScript.h"
#include "Vfx/Decals/DecalManager.h"


/////////////////////////////////////////////////////////////////////////////////

ANIM_OPTIMISATIONS()

/////////////////////////////////////////////////////////////////////////////////
CCutSceneParticleEffectsEntity::CCutSceneParticleEffectsEntity (const cutfObject* pObject)
:cutsUniqueEntity ( pObject )
,m_bIsLoaded(false)
{
	m_pParticleEffect = NULL; 
	if(pObject->GetType() == CUTSCENE_PARTICLE_EFFECT_OBJECT_TYPE)
	{
		const cutfParticleEffectObject* particleEffectObject = static_cast<const cutfParticleEffectObject*>(pObject); 	
		m_pParticleEffect = rage_new CCutSceneTriggeredParticleEffect(particleEffectObject);
	}
	else if(pObject->GetType() == CUTSCENE_ANIMATED_PARTICLE_EFFECT_OBJECT_TYPE)
	{
		const cutfAnimatedParticleEffectObject* pAnimatedParticleEffect = static_cast<const cutfAnimatedParticleEffectObject*>(pObject); 
		m_pParticleEffect = rage_new CCutSceneAnimatedParticleEffect(pAnimatedParticleEffect);
	}
}

/////////////////////////////////////////////////////////////////////////////////

CCutSceneParticleEffectsEntity::~CCutSceneParticleEffectsEntity ()
{	
	if(m_pParticleEffect)
	{
		delete m_pParticleEffect;
		m_pParticleEffect = NULL;
	}
}

/////////////////////////////////////////////////////////////////////////////////

bool CCutSceneParticleEffectsEntity::HasParticleEffectLoaded(cutsManager* pManager)
{
	if(!m_bIsLoaded)
	{
		CutSceneManager* pCutSceneManager = static_cast<CutSceneManager*>(pManager); 

		CCutSceneAssetMgrEntity* pAssetManager = pCutSceneManager->GetAssetManager(); 

		const char* pCutsceneName = pManager->GetCutsceneName();
		atHashWithStringNotFinal ptFxAssetHashName = g_vfxScript.GetCutscenePtFxAssetName(pCutsceneName);

			// check that there is a particle asset set up for this cutscene
		if (Verifyf(ptFxAssetHashName.GetHash(), "cutscene %s has particle effects but no particle asset set up - can't request the asset", pCutsceneName))
		{
			if(pAssetManager->HasPTFXListLoaded(ptFxAssetHashName))
			{
				m_bIsLoaded = true; 
			}
		}
	}

	return m_bIsLoaded; 
}

void CCutSceneParticleEffectsEntity::DispatchEvent( cutsManager* pManager, const cutfObject* BANK_ONLY(pObject), s32 iEventId, const cutfEventArgs* pEventArgs, const float UNUSED_PARAM(fTime), const u32 UNUSED_PARAM(StickyId))
{
	switch ( iEventId )
	{
	case CUTSCENE_START_OF_SCENE_EVENT:
		{
			cutsceneParticleEffectEntityDebugf("CUTSCENE_START_OF_SCENE_EVENT"); 
		}
		break;

	case CUTSCENE_UPDATE_EVENT:
	case CUTSCENE_PAUSE_EVENT:
		{
			if(HasParticleEffectLoaded(pManager))
			{
				if(m_pParticleEffect && m_pParticleEffect->GetType() == CUTSCENE_ANIMATED_PARTICLE_EFFECT)
				{
					CCutSceneAnimatedParticleEffect* pAnimatedParticleEffect = static_cast<CCutSceneAnimatedParticleEffect*>(m_pParticleEffect); 
					//only used for continuous particle effects
					if(pAnimatedParticleEffect->IsActive())
					{
						//Some entities should only get updated in the post scene, after the animation update, check if this should be updated here 
						CutSceneManager* pCutManager = static_cast<CutSceneManager*>(pManager); 

						if (pCutManager->GetPostSceneUpdate())
						{				
							m_pParticleEffect->Update(pManager);
						}
					}
				}
			}
		}
		break;

	case CUTSCENE_PLAY_PARTICLE_EFFECT_EVENT:
		{
			if(HasParticleEffectLoaded(pManager))
			{
				if (m_pParticleEffect)
				{
					switch(m_pParticleEffect->GetType())
					{
					case CUTSCENE_TRIGGERED_PARTICLE_EFFECT:
						{
							cutsceneParticleEffectEntityDebugf("CUTSCENE_PLAY_PARTICLE_EFFECT_EVENT: CUTSCENE_TRIGGERED_PARTICLE_EFFECT"); 
							if(pEventArgs && pEventArgs->GetType() == CUTSCENE_PLAY_PARTICLE_EFFECT_EVENT_ARGS_TYPE)
							{
								const cutfPlayParticleEffectEventArgs* pParticleEventArgs = static_cast<const cutfPlayParticleEffectEventArgs*>(pEventArgs); 
								CCutSceneTriggeredParticleEffect* pTriggeredEffect = static_cast<CCutSceneTriggeredParticleEffect*>(m_pParticleEffect); 

								pTriggeredEffect->SetAttachParentAndBoneAttach(pParticleEventArgs->GetAttachParentId(), pParticleEventArgs->GetAttachBoneHash()); 
								
								Quaternion rotation(pParticleEventArgs->GetInitialRotation()); 	
								
								pTriggeredEffect->SetInitialRotation(rotation); 
								pTriggeredEffect->SetInitialPosition(pParticleEventArgs->GetInitialOffset()); 

								pTriggeredEffect->Update(pManager);
							}
						}
						break; 

					case CUTSCENE_ANIMATED_PARTICLE_EFFECT:
						{
							cutsceneParticleEffectEntityDebugf("CUTSCENE_PLAY_PARTICLE_EFFECT_EVENT: CUTSCENE_ANIMATED_PARTICLE_EFFECT"); 
							if(pEventArgs && m_pParticleEffect)
							{
								CCutSceneAnimatedParticleEffect* pAnimatedParticleEffect = static_cast<CCutSceneAnimatedParticleEffect*>(m_pParticleEffect);
								
								const cutfPlayParticleEffectEventArgs* pParticleEventArgs = static_cast<const cutfPlayParticleEffectEventArgs*>(pEventArgs); 
								m_pParticleEffect->SetAttachParentAndBoneAttach(pParticleEventArgs->GetAttachParentId(), pParticleEventArgs->GetAttachBoneHash()); 
								
								pAnimatedParticleEffect->SetIsActive(true); 
							}
						}
						break; 
					default:
						cutsceneDebugf1("CUTSCENE_PLAY_PARTICLE_EFFECT_EVENT: Unknown particle effect type"); 
						break;
					}
				}
			}
		}
		break; 

	case CUTSCENE_STOP_PARTICLE_EFFECT_EVENT:
		{
			if (m_pParticleEffect && m_pParticleEffect->GetType() == CUTSCENE_ANIMATED_PARTICLE_EFFECT)
			{
				cutsceneParticleEffectEntityDebugf("CUTSCENE_STOP_PARTICLE_EFFECT_EVENT: CUTSCENE_ANIMATED_PARTICLE_EFFECT"); 
				CCutSceneAnimatedParticleEffect* pAnimatedParticleEffect = static_cast<CCutSceneAnimatedParticleEffect*>(m_pParticleEffect); 
				pAnimatedParticleEffect->StopParticleEffect(pManager);
				pAnimatedParticleEffect->SetIsActive(false);
			}
		}
		break; 

	case CUTSCENE_SET_CLIP_EVENT:
		{
			if(m_pParticleEffect && m_pParticleEffect->GetType() == CUTSCENE_ANIMATED_PARTICLE_EFFECT)
			{
				cutsceneParticleEffectEntityDebugf("CUTSCENE_SET_CLIP_EVENT: CUTSCENE_ANIMATED_PARTICLE_EFFECT"); 
				if (pEventArgs && pEventArgs->GetType() == CUTSCENE_CLIP_EVENT_ARGS_TYPE)
				{
					CutSceneManager* pCutManager = static_cast<CutSceneManager*>(pManager); 					
					const cutsClipEventArgs *pClipEventArgs = static_cast<const cutsClipEventArgs *>( pEventArgs );					
					CCutSceneAnimatedParticleEffect* pAnimatedParticleEffect = static_cast<CCutSceneAnimatedParticleEffect*>(m_pParticleEffect); 
					pAnimatedParticleEffect->GetCutSceneAnimation().Init(pClipEventArgs->GetClip(), pCutManager->GetSectionStartTime(pCutManager->GetCurrentSection()) );
				}
			}
		}
		break;

	case CUTSCENE_CLEAR_ANIM_EVENT:
		{
			if (m_pParticleEffect && m_pParticleEffect->GetType() == CUTSCENE_ANIMATED_PARTICLE_EFFECT)
			{
				cutsceneParticleEffectEntityDebugf("CUTSCENE_CLEAR_ANIM_EVENT: CUTSCENE_ANIMATED_PARTICLE_EFFECT"); 
				CCutSceneAnimatedParticleEffect* pAnimatedParticleEffect = static_cast<CCutSceneAnimatedParticleEffect*>(m_pParticleEffect); 
				
				pAnimatedParticleEffect->StopParticleEffect(pManager);				
				pAnimatedParticleEffect->GetCutSceneAnimation().Remove();
				pAnimatedParticleEffect->SetIsActive(false); 
			}
		}
		break;

	case CUTSCENE_SHOW_DEBUG_LINES_EVENT:
	{
		
	}


	case CUTSCENE_CAMERA_CUT_EVENT:
		{
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

/////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CCutSceneParticleEffectsEntity::DebugDraw() const
{
	if(m_pParticleEffect)
	{
		m_pParticleEffect->DisplayDebugInfo(GetObject()->GetDisplayName().c_str());
	}
}


void CCutSceneParticleEffectsEntity::CommonDebugStr(const cutfObject* pObject, char * debugStr)
{
	if (!pObject)
	{
		return;
	}

	sprintf(debugStr, "%s - CT(%6.2f) - CF(%u) - FC(%u) - Particle Effects Entity(%d, %s) - ", 
		CutSceneManager::GetInstance()->GetStateName(CutSceneManager::GetInstance()->GetState()),
		CutSceneManager::GetInstance()->GetCutSceneCurrentTime(),
		CutSceneManager::GetInstance()->GetCutSceneCurrentFrame(),
		fwTimer::GetFrameCount(), 
		pObject->GetObjectId(), 
		pObject->GetDisplayName().c_str());
}


#endif

bool CCutSceneDecalEntity::sm_DecalAddedThisFrame = false; 

CCutSceneDecalEntity::CCutSceneDecalEntity (const cutfObject* pObject)//, s32 iAttachedEntityId)
:cutsUniqueEntity ( pObject )
,m_DecalId(0)
,m_LifeTime(0.0f)
{
}

CCutSceneDecalEntity::~CCutSceneDecalEntity ()
{
	sm_DecalAddedThisFrame = false;
	m_DecalId = 0; 
	m_LifeTime = 0.0f; 
}

void CCutSceneDecalEntity::DispatchEvent(cutsManager *pManager, const cutfObject* BANK_ONLY(pObject), s32 iEventId, const cutfEventArgs* pEventArgs, const float UNUSED_PARAM(fTime), const u32 UNUSED_PARAM(StickyId))
{
	switch ( iEventId )
	{
	case CUTSCENE_START_OF_SCENE_EVENT:
		{
		
		}
		break;

	case CUTSCENE_UPDATE_EVENT:
	case CUTSCENE_PAUSE_EVENT:
		{
			CutSceneManager* pCutManager = static_cast<CutSceneManager*>(pManager); 
			
			//reset this static here so decals can be added next frame
			if (pCutManager->GetPostSceneUpdate())
			{	
				sm_DecalAddedThisFrame = false; 
			}
		}
		break;

	case CUTSCENE_TRIGGER_DECAL_EVENT:
		{
			CutSceneManager* pCutManager = static_cast<CutSceneManager*>(pManager); 

#if __ASSERT
			if(sm_DecalAddedThisFrame && !pCutManager->WasSkipped())
			{
				cutsceneWarningf("One decal request has already been made this frame (the vfx system can only cope with one request per frame) so subsequent decal requests will be ignored - %s", GetObject()->GetDisplayName().c_str() ); 
			}
#endif //__ASSERT

			if(!sm_DecalAddedThisFrame || pCutManager->WasSkipped())
			{
				cutsceneDecalEntityDebugf("CUTSCENE_TRIGGER_DECAL_EVENT"); 
				if(GetObject() && pEventArgs && pEventArgs->GetType() == CUTSCENE_DECAL_EVENT_ARGS)
				{
					const cutfDecalEventArgs* pDecalEventArgs = static_cast<const cutfDecalEventArgs*>(pEventArgs); 

					s32 decalRenderSettingIndex = 0;
					s32 decalRenderSettingCount = 0;

					//convert our decal into scene space
					Matrix34 SceneMat(M34_IDENTITY);
					pManager->GetSceneOrientationMatrix(SceneMat);

					//construct the decal projection matrix
					Matrix34 DecalMat(M34_IDENTITY); 
					Quaternion DecalRot(pDecalEventArgs->GetRotationQuaternion()); 

					DecalMat.FromQuaternion(DecalRot); 
					DecalMat.d = pDecalEventArgs->GetPosition(); 
					
					//Vector3 test(-15.0f, -1.0f * (float)GetObject()->GetObjectId(), 0.0f); 
					//DecalMat.RotateX(-PI/2.0f); 

					//DecalMat.d =  test; 

					//put it into scene space
					DecalMat.Dot(SceneMat); 
					
#if __BANK
					m_DebugDrawDecalMatrix = DecalMat;
					m_DebugBoxSize.Set(pDecalEventArgs->GetWidth()/2.0f, 0.3f, pDecalEventArgs->GetHeight() /2.0f); 
#endif

					u32 DecalTextureId = GetObject()->GetRenderId(); 
					
					Color32 Decal_colour = pDecalEventArgs->GetColor(); 
					//Decal_colour.SetAlpha(255); 

					cutsceneAssertf(DecalTextureId > 0, "Invalid render setting id. Requested decal %s likely doesn't exist", GetObject()->GetDisplayName().c_str()); 
	
					g_decalMan.FindRenderSettingInfo(DecalTextureId, decalRenderSettingIndex, decalRenderSettingCount);	

					m_DecalId = g_decalMan.AddGeneric(DECALTYPE_GENERIC_COMPLEX_COLN, decalRenderSettingIndex, decalRenderSettingCount, VECTOR3_TO_VEC3V(DecalMat.d), VECTOR3_TO_VEC3V(DecalMat.b), VECTOR3_TO_VEC3V(DecalMat.c), pDecalEventArgs->GetWidth(), pDecalEventArgs->GetHeight(), 0.0f, -1, 0.0f, 1.0f, 1.0f, 0.0f, Decal_colour, false, false, 0.0f, false, false REPLAY_ONLY(, 0.0f));
					m_LifeTime = pDecalEventArgs->GetLifeTime();

					sm_DecalAddedThisFrame= true; 
				}
			}
		}
		break; 

	case CUTSCENE_REMOVE_DECAL_EVENT:
		{
			if(m_LifeTime > -1.0f && m_DecalId > 0)
			{
				g_decalMan.Remove(m_DecalId);
			}

			m_DecalId = 0; 
			m_LifeTime = 0.0f; 
		}
		break;

	}

#if __BANK
	cutsEntity::DispatchEvent(pManager, pObject, iEventId, pEventArgs); 
#endif
}

const cutfDecalObject* CCutSceneDecalEntity::GetObject() const
{
	if(cutsUniqueEntity::GetObject() && cutsUniqueEntity::GetObject()->GetType() == CUTSCENE_DECAL_OBJECT_TYPE)
	{
		return static_cast<const cutfDecalObject*>(cutsUniqueEntity::GetObject()); 
	}

	return NULL; 
}

#if __BANK

void CCutSceneDecalEntity::CommonDebugStr(const cutfObject* pObject, char * debugStr)
{
	if (!pObject)
	{
		return;
	}

	sprintf(debugStr, "%s - CT(%6.2f) - CF(%u) - FC(%u) - Decal Entity(%d, %s) - ", 
		CutSceneManager::GetInstance()->GetStateName(CutSceneManager::GetInstance()->GetState()),
		CutSceneManager::GetInstance()->GetCutSceneCurrentTime(),
		CutSceneManager::GetInstance()->GetCutSceneCurrentFrame(),
		fwTimer::GetFrameCount(), 
		pObject->GetObjectId(), 
		pObject->GetDisplayName().c_str());
}

void CCutSceneDecalEntity::DebugDraw() const
{
	if(GetObject())
	{
		grcDebugDraw::Axis(m_DebugDrawDecalMatrix, 0.3f, true);
		grcDebugDraw::Text(m_DebugDrawDecalMatrix.d, CRGBA(0,0,0,255),0, grcDebugDraw::GetScreenSpaceTextHeight(), GetObject()->GetDisplayName().c_str());
		
		Vector3 Min = m_DebugBoxSize; 
		Min.x = m_DebugBoxSize.x * -1.0f; 	
		Min.y = 0.0f; 
		Min.z = m_DebugBoxSize.z * -1.0f; 	
		grcDebugDraw::BoxOriented(VECTOR3_TO_VEC3V(Min), VECTOR3_TO_VEC3V(m_DebugBoxSize),MATRIX34_TO_MAT34V(m_DebugDrawDecalMatrix), CRGBA(255,0,0,255), false);
	}
}


#endif
