//////////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutsceneParticleEffectObject.cpp
// PURPOSE : Describes an in game particle effect. The effects updated via their 
//			 respective entity types. Entitys call the objects update function when 
//			 required. The particles are updated via the cutscene animation class;
//			 When initated the time of anim start time is stored an used to calculate
//			 its phase based on its duration. 
// AUTHOR  : Thomas French
// STARTED : 01/10 /2009
//
/////////////////////////////////////////////////////////////////////////////////////

//Rage includes
#include "grcore/debugdraw.h"
#include "rmptfx/ptxeffectrule.h"
#include "vfx/ptfx/ptfxwrapper.h"
#include "vfx/ptfx/ptfxscript.h"
#include "vfx/systems/Vfxscript.h"

//Game includes
#include "cutscene/cutscenemanagernew.h"
#include "cutscene/animatedModelEntity.h"
#include "cutscene/cutscene_channel.h"
#include "renderer/color.h"
#include "string/stringhash.h"
#include "CutSceneParticleEffectObject.h"
#include "cutfile/cutfobject.h"
#include "control/replay/effects/ParticleMiscFxPacket.h"
/////////////////////////////////////////////////////////////////////////////////////

ANIM_OPTIMISATIONS ()


/////////////////////////////////////////////////////////////////////////////////////

CCutSceneParticleEffect::CCutSceneParticleEffect()//, s32 iAttachedEntityId = -1)
{	
	m_mParticleEffect.Identity(); 
	
	m_iEffectId = -1; 
	m_IsActive = false; 
	m_AttachParent = 0;
	m_BoneHash = 0;
#if __BANK
	m_bCanOverrideEffect = false; 
	m_vOverridenParticleEffectPos = VEC3_ZERO;
	m_vOverridenParticleEffectRot = VEC3_ZERO; 
#endif

}
/////////////////////////////////////////////////////////////////////////////////////

CCutSceneParticleEffect::~CCutSceneParticleEffect()
{
}

CEntity* CCutSceneParticleEffect::GetAttachParent(cutsManager* pManager, u32 AttachParentId)
{
	cutsEntity* pCutsEntity = pManager->GetEntityByObjectId( AttachParentId); 
	
	CEntity* pGameEntity = NULL; 

	if(pCutsEntity && (pCutsEntity->GetType() == CUTSCENE_PROP_GAME_ENITITY ||
		pCutsEntity->GetType() == CUTSCENE_WEAPON_GAME_ENTITY ||
		pCutsEntity->GetType() == CUTSCENE_ACTOR_GAME_ENITITY ||
		pCutsEntity->GetType() == CUTSCENE_VEHICLE_GAME_ENITITY))
	{
		CCutsceneAnimatedModelEntity* pModelEntity = static_cast<CCutsceneAnimatedModelEntity*>(pCutsEntity); 
		pGameEntity =  pModelEntity->GetGameEntity(); 
	}
	return pGameEntity; 
}

s32 CCutSceneParticleEffect::GetAttachParentsBoneIndex(cutsManager* pManager, u16 BoneHash, u32 AttachParentId)
{
	s32 boneIndex = -1;

	CEntity* pGameEntity = GetAttachParent( pManager, AttachParentId); 

	if(pGameEntity && pGameEntity->GetIsDynamic())
	{
		CDynamicEntity* pDynamicEntity = static_cast<CDynamicEntity*>(pGameEntity); 

		if(pDynamicEntity->GetSkeleton())
		{	
			pDynamicEntity->GetSkeletonData().ConvertBoneIdToIndex(BoneHash, boneIndex); 

		}
	}	
	return boneIndex; 
}


/////////////////////////////////////////////////////////////////////////////////////
//Sets if the effect is triggered or continuous, checks an instance of the effect and checks their duration


CCutSceneAnimatedParticleEffect::CCutSceneAnimatedParticleEffect(const cutfAnimatedParticleEffectObject* pObject)
:CCutSceneParticleEffect()
,m_pCutfObject(pObject)
{
	m_EffectRuleHashName = m_pCutfObject->GetStreamingName(); 
}

CCutSceneAnimatedParticleEffect::~CCutSceneAnimatedParticleEffect()
{

}

void CCutSceneAnimatedParticleEffect::StopParticleEffect(const cutsManager* pManager)
{
	if (m_iEffectId != -1)
	{
		m_AttachParent = -1; 
		m_BoneHash = 0; 
		
		const CutSceneManager* pCutManager = static_cast<const CutSceneManager*>(pManager);

		if (pCutManager->WasSkipped())
		{
			ptfxScript::Remove(m_iEffectId);
			m_iEffectId = -1;
		}
		else
		{
			ptfxScript::Stop(m_iEffectId);

#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				CReplayMgr::RecordPersistantFx<CPacketStopScriptPtFx>(CPacketStopScriptPtFx(), CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_iEffectId), NULL, false);
				CReplayMgr::StopTrackingFx(CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_iEffectId));
			}
#endif

			m_iEffectId = -1;
		}
	}
}

void CCutSceneAnimatedParticleEffect::Update( cutsManager* pManager)
{
	if (m_Animation.GetClip() && IsActive() && m_pCutfObject)
	{	
		if(m_EffectListHashName.GetHash() == 0)
		{
			m_EffectListHashName = g_vfxScript.GetCutscenePtFxAssetName(pManager->GetCutsceneName());
		}

		CutSceneManager* pCutManager = static_cast<CutSceneManager*>(pManager); 
		
		float fPhase = 0.0f; 
		const crClip* pClip = m_Animation.GetClip(); 
		s32 boneIndex = -1; 
		CEntity* pAttachEntity = NULL; 
		if (pClip)
		{
			if(m_AttachParent> 0)
			{
				boneIndex = GetAttachParentsBoneIndex(pManager, m_BoneHash, m_AttachParent); 
				
				cutsceneAssertf( boneIndex != -1, "Bone hash %d for attach parent id %d does not return a valid bone index. Particle: %s",m_BoneHash , m_AttachParent, m_pCutfObject->GetDisplayName().c_str());
				
				pAttachEntity = GetAttachParent(pManager, m_AttachParent); 

				cutsceneAssertf( pAttachEntity, "Attach parent id %d does not return a valid attach entity, check that the parent id matches the entity you want to attach particle %s to.", m_AttachParent, m_pCutfObject->GetDisplayName().c_str());
			}

			fPhase =  pCutManager->GetPhaseUpdateAmount( pClip, m_Animation.GetStartTime()); 

			Vector3 vEffectPos (VEC3_ZERO);
			
			m_Animation.GetVector3Value(m_Animation.GetClip(),kTrackBoneTranslation,fPhase, vEffectPos);
			
			Quaternion qValue (Quaternion::sm_I);
			m_Animation.GetQuaternionValue(m_Animation.GetClip(),kTrackBoneRotation, fPhase,qValue); 

			m_mParticleEffect.FromQuaternion(qValue); 
			m_mParticleEffect.d = vEffectPos; 
			
			//not attaching to a bone so make the effect scene space relative
			if(boneIndex == -1)
			{
				Matrix34 SceneMat(M34_IDENTITY);
				pCutManager->GetSceneOrientationMatrix(SceneMat);
				SceneMat.d  = pCutManager->GetSceneOffset(); 

				m_mParticleEffect.Dot(SceneMat); 
#if __BANK
				if(m_bCanOverrideEffect)
				{
					m_mParticleEffect.Identity3x3();
					m_mParticleEffect.d = m_vOverridenParticleEffectPos; 

					m_mParticleEffect.FromEulersYXZ(m_vOverridenParticleEffectRot); 
				}
#endif
			}
	
			if (m_iEffectId == -1)
			{
				Vector3 vZero = VEC3_ZERO;
				
				if(boneIndex != -1 && pAttachEntity)
				{
					
					m_iEffectId = ptfxScript::Start(m_EffectRuleHashName, m_EffectListHashName, pAttachEntity, boneIndex, RC_VEC3V(m_mParticleEffect.d), RC_VEC3V(vZero));

#if GTA_REPLAY
					if(CReplayMgr::ShouldRecord())
					{
						CReplayMgr::RecordPersistantFx<CPacketStartScriptPtFx>(
							CPacketStartScriptPtFx(atHashWithStringNotFinal(m_EffectRuleHashName), m_EffectListHashName, boneIndex, m_mParticleEffect.d, vZero, 1.0f, 0),
							CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_iEffectId),
							pAttachEntity, 
							true);
					}
#endif

					ptfxScript::UpdateOffset(m_iEffectId, MATRIX34_TO_MAT34V(m_mParticleEffect));
#if __BANK		
					Matrix34 BoneGlobal; 
					pAttachEntity->GetSkeleton()->GetGlobalMtx(boneIndex, RC_MAT34V(BoneGlobal)); 
					
					m_mParticleEffectWorld = m_mParticleEffect; 

					m_mParticleEffectWorld.Dot(BoneGlobal);
#endif
				}
				else
				{
					ptxEffectRule* pEffectRule = ptfxWrapper::GetEffectRule(m_EffectRuleHashName, m_EffectListHashName);
					if (pEffectRule)
					{
						// duration is -1 for looped 
						if (pEffectRule->GetIsInfinitelyLooped())
						{
							m_iEffectId = ptfxScript::Start(m_EffectRuleHashName, m_EffectListHashName, NULL, 0, RC_VEC3V(m_mParticleEffect.d), RC_VEC3V(vZero));

#if GTA_REPLAY
							if(CReplayMgr::ShouldRecord())
							{
								CReplayMgr::RecordPersistantFx<CPacketStartScriptPtFx>(
									CPacketStartScriptPtFx(atHashWithStringNotFinal(m_EffectRuleHashName), m_EffectListHashName, 0, m_mParticleEffect.d, vZero, 1.0f, 0),
									CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_iEffectId),
									NULL, 
									true);
							}
#endif

							ptfxScript::UpdateOffset(m_iEffectId, MATRIX34_TO_MAT34V(m_mParticleEffect));
						}
						else
						{
							//legacy support - Need to convert to non animated particle effects
							ptfxScript::Trigger(m_EffectRuleHashName, m_EffectListHashName, NULL, 0, MATRIX34_TO_MAT34V(m_mParticleEffect));
							SetIsActive(false);
						}
					}
				
#if __BANK		
					m_mParticleEffectWorld = m_mParticleEffect; 
#endif
				}
				Assertf( m_iEffectId != 0, "Effect: %s could not be started", m_pCutfObject->GetDisplayName().c_str() );
			}
			else
			{
				//the start effect returns 0 if the effect is unknown 
				if (m_iEffectId != 0)
				{ 
					ptfxScript::UpdateOffset(m_iEffectId, MATRIX34_TO_MAT34V(m_mParticleEffect));
#if GTA_REPLAY
					if(CReplayMgr::ShouldRecord())
					{
						CReplayMgr::RecordPersistantFx<CPacketUpdateOffsetScriptPtFx>(
							CPacketUpdateOffsetScriptPtFx(m_mParticleEffect),
							CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)m_iEffectId),
							NULL, 
							true);
					}
#endif
					//keep the particle up todate for debugging
#if __BANK					
					if(boneIndex != -1 && pAttachEntity)
					{
						Matrix34 BoneGlobal; 
						pAttachEntity->GetSkeleton()->GetGlobalMtx(boneIndex, RC_MAT34V(BoneGlobal)); 

						m_mParticleEffectWorld = m_mParticleEffect; 

						m_mParticleEffectWorld.Dot(BoneGlobal);
					}
					else
					{
				
						m_mParticleEffectWorld = m_mParticleEffect; 

					}
#endif				
					if(m_pCutfObject->GetAttributeList().GetAttributeArray().GetCount() > 0)
					{
						for(int i = 0; i < m_pCutfObject->GetAttributeList().GetAttributeArray().GetCount(); i++)
						{
							const crClip* pClip = m_Animation.GetClip(); 
							
							float EvoEffect = 0.0f; 
							
							u16 EffectId = (u16) m_pCutfObject->GetAttributeList().GetAttributeArray()[i].GetIntValue(); 

							if(m_Animation.GetFloatValue(pClip,kTrackParticleData, fPhase, EvoEffect, EffectId))
							{
								ptfxScript::UpdateEvo(m_iEffectId, EffectId, EvoEffect);
							}
							else
							{
								cutsceneAssertf(0, "Not a valid evo track %d for particle %s", EffectId, m_pCutfObject->GetDisplayName().c_str()); 
							}
							
						}
					}
				}	
			}
		}
	}
}

CCutSceneTriggeredParticleEffect::CCutSceneTriggeredParticleEffect(const cutfParticleEffectObject* pObject)
:CCutSceneParticleEffect()
,m_pCutfObject(pObject)
{ 
	m_EffectRuleHashName = m_pCutfObject->GetStreamingName(); 
}


CCutSceneTriggeredParticleEffect::~CCutSceneTriggeredParticleEffect()
{
}

void CCutSceneTriggeredParticleEffect::Update(cutsManager* pManager)
{
	CutSceneManager* pCutManager = static_cast<CutSceneManager*>(pManager);

	if (pCutManager->WasSkipped()==false && m_pCutfObject)
	{		
		if(m_EffectListHashName.GetHash() == 0)
		{
			m_EffectListHashName = g_vfxScript.GetCutscenePtFxAssetName(pManager->GetCutsceneName());
		}
		s32 boneIndex = -1; 
		CEntity* pGameEntity = NULL; 

		if(m_AttachParent > 0)
		{
			boneIndex = GetAttachParentsBoneIndex(pManager, m_BoneHash, m_AttachParent); 

			cutsceneAssertf( boneIndex != -1, "Bone hash %d for attach parent id %d does not return a valid bone index. Particle: %s",m_BoneHash , m_AttachParent, m_pCutfObject->GetDisplayName().c_str());

			pGameEntity = GetAttachParent(pManager, m_AttachParent); 

			cutsceneAssertf( pGameEntity, "Attach parent id %d does not return a valid attach entity, check that the parent id matches the entity you want to attach particle %s to.", m_AttachParent, m_pCutfObject->GetDisplayName().c_str());
		}
	
		Quaternion Rot(m_InitialRotation);
		m_mParticleEffect.FromQuaternion(Rot); 
		m_mParticleEffect.d = m_Position; 

		if( boneIndex == -1)
		{
			//there is not attach bone so we assume that the positions are scene relative
			Matrix34 SceneMat(M34_IDENTITY);
			pCutManager->GetSceneOrientationMatrix(SceneMat);
			m_mParticleEffect.Dot(SceneMat); 

			REPLAY_ONLY(bool success = )ptfxScript::Trigger(m_EffectRuleHashName, m_EffectListHashName, NULL, 0, MATRIX34_TO_MAT34V(m_mParticleEffect));

#if GTA_REPLAY
			if(success && CReplayMgr::ShouldRecord())
			{
				CReplayMgr::RecordFx<CPacketTriggeredScriptPtFx>(
					CPacketTriggeredScriptPtFx(atHashWithStringNotFinal(m_EffectRuleHashName), m_EffectListHashName, 0, m_mParticleEffect),
					NULL,
					true);
			}
#endif

#if __BANK
			m_mParticleEffectWorld = m_mParticleEffect; 
#endif		
		}
		else if(boneIndex != -1 && pGameEntity)
		{
			REPLAY_ONLY(bool success =) ptfxScript::Trigger(m_EffectRuleHashName, m_EffectListHashName, pGameEntity, boneIndex, MATRIX34_TO_MAT34V(m_mParticleEffect));

#if GTA_REPLAY
			if(success && CReplayMgr::ShouldRecord())
			{
				CReplayMgr::RecordFx<CPacketTriggeredScriptPtFx>(
					CPacketTriggeredScriptPtFx(atHashWithStringNotFinal(m_EffectRuleHashName), m_EffectListHashName, boneIndex, m_mParticleEffect),
					pGameEntity,
					true);
			}
#endif

#if __BANK
			if(boneIndex != -1 && pGameEntity)
			{
				Matrix34 BoneGlobal; 
				pGameEntity->GetSkeleton()->GetGlobalMtx(boneIndex, RC_MAT34V(BoneGlobal)); 

				m_mParticleEffectWorld = m_mParticleEffect; 

				m_mParticleEffectWorld.Dot(BoneGlobal);
			}
#endif
		}
		
	}
}

#if __BANK

Vector3 CCutSceneParticleEffect::GetEffectRotation() const
{
	Vector3 rot; 
	 
	m_mParticleEffect.ToEulersFastYXZ(rot);

	return rot;
}


void  CCutSceneParticleEffect::DisplayDebugInfo(const char* pName) const
{
	grcDebugDraw::Axis(m_mParticleEffectWorld, 0.3f, true);
	grcDebugDraw::Text(m_mParticleEffectWorld.d, CRGBA(0,0,0,255),0, grcDebugDraw::GetScreenSpaceTextHeight(), pName );

}
#endif

/////////////////////////////////////////////////////////////////////////////////////