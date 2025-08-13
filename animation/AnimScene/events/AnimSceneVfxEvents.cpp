#include "AnimSceneVfxEvents.h"

#include "AnimSceneVfxEvents_parser.h"

// Rage includes
#include "move/move_parameterbuffer.h"

// Framework includes
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxmanager.h"

// game includes
#include "ai\ambient\ConditionalAnimManager.h"
#include "animation/AnimScene/AnimScene.h"
#include "animation/AnimScene/entities/AnimSceneEntities.h"
#include "animation/AnimScene/events/AnimSceneEventInitialisers.h"
#include "animation/EventTags.h"
#include "Peds\Ped.h"
#include "Peds\PedIntelligence.h"
#include "streaming/streaming.h"
#include "task/Animation/TaskScriptedAnimation.h"
#include "vehicleAi\VehicleIntelligence.h"
#include "vfx/systems/VfxEntity.h"

//////////////////////////////////////////////////////////////////////////
// CAnimScenePlayCameraAnimEvent
void CAnimScenePlayVfxEvent::OnInit(CAnimScene* pScene)
{
	m_flags.SetFlag( kRequiresPreloadEnter | kRequiresPreloadUpdate | kRequiresEnter | kRequiresUpdate | kRequiresExit );
	static s32 s_staticUniqueId = 0;
	m_data->m_effectId = s_staticUniqueId;
	s_staticUniqueId++;

	// Initialise Helpers
	m_entity.OnInit(pScene);
}
void CAnimScenePlayVfxEvent::OnShutdown(CAnimScene* UNUSED_PARAM(pScene))
{
	// any cleanup needed here?
}

bool CAnimScenePlayVfxEvent::OnEnterPreload(CAnimScene* UNUSED_PARAM(pScene))
{
	atHashWithStringNotFinal ptFxAssetHashName = g_vfxEntity.GetPtFxAssetName(PT_ANIM_FX, m_vfxName);
	const int slot = CConditionalAnimStreamingVfxManager::GetInstance().RequestVfxAssetSlot(ptFxAssetHashName.GetHash());
	ptfxAssertf(slot>-1, "cannot find particle asset of type (%d) named (%s) with slot name (%s) in any loaded rpf", PT_ANIM_FX, m_vfxName.GetCStr(), ptFxAssetHashName.GetCStr());
	Assert(slot <= 0x7fff);
	m_data->m_streamingSlot = (s16)slot;

	if (m_data->m_streamingSlot>=0)
	{
		return ptfxManager::GetAssetStore().HasObjectLoaded(strLocalIndex(m_data->m_streamingSlot));
	}
	else
	{
		// don't hold up the scene indefinitely because an effect doesn't exist
		return true;
	} 
}

bool CAnimScenePlayVfxEvent::OnUpdatePreload(CAnimScene* UNUSED_PARAM(pScene))
{
	if (m_data->m_streamingSlot>=0)
	{
		return ptfxManager::GetAssetStore().HasObjectLoaded(strLocalIndex(m_data->m_streamingSlot));
	}
	else
	{
		// don't hold up the scene indefinitely because an effect doesn't exist
		return true;
	}	
}

void CAnimScenePlayVfxEvent::OnEnter(CAnimScene* UNUSED_PARAM(pScene), AnimSceneUpdatePhase phase)
{
	if (!m_continuous && !m_useEventTags && phase==kAnimSceneUpdatePreAnim)
	{
		if (m_data->m_streamingSlot<0)
		{
			return;
		}

		// get the entity from the scene
		CPhysical* pPhys = m_entity.GetPhysicalEntity();

		if (!pPhys)
		{
			return;
		}

		ptfxAssertf(ptfxManager::GetAssetStore().HasObjectLoaded(strLocalIndex(m_data->m_streamingSlot)), "Vfx asset %s (slot %d) didn't load in time!", m_vfxName.GetCStr(), m_data->m_streamingSlot);

		CParticleAttr temp;
		InitParticleData(temp);
		vfxAssertf(!g_vfxEntity.GetIsInfinitelyLooped(temp), "VFX {%s} cant be used as a triggered effect.", m_vfxName.GetCStr());
		g_vfxEntity.TriggerPtFxEntityAnim(pPhys, &temp);
	}
}

void CAnimScenePlayVfxEvent::OnUpdate(CAnimScene* UNUSED_PARAM(pScene), AnimSceneUpdatePhase phase)
{
	if ((m_continuous || m_useEventTags) && phase==kAnimSceneUpdatePreAnim)
	{
		if (m_data->m_streamingSlot<0)
		{
			return;
		}

		// get the entity from the scene
		CPhysical* pPhys = m_entity.GetPhysicalEntity();

		if (!pPhys)
		{
			return;
		}

		ptfxAssertf(ptfxManager::GetAssetStore().HasObjectLoaded(strLocalIndex(m_data->m_streamingSlot)), "Vfx asset %s (slot %d) didn't load in time!", m_vfxName.GetCStr(), m_data->m_streamingSlot);

		if (m_useEventTags)
		{
			// find the scripted animation task to search for vfx tags
			CTaskScriptedAnimation* pAnimTask = FindScriptedAnimTask(*pPhys);

			if (pAnimTask && pAnimTask->GetMoveNetworkHelper() && pAnimTask->GetMoveNetworkHelper()->GetNetworkPlayer())
			{
				mvParameterBuffer::Iterator it;
				for(mvKeyedParameter* kp = it.Begin(pAnimTask->GetMoveNetworkHelper()->GetNetworkPlayer()->GetOutputBuffer(), mvParameter::kParameterProperty, CClipEventTags::ObjectVfx); kp != NULL; kp = it.Next())
				{
					const crProperty* prop = kp->Parameter.GetProperty();
					const crPropertyAttribute* attrib = prop->GetAttribute(CClipEventTags::VFXName);
					if (attrib && aiVerifyf(attrib->GetType() == crPropertyAttribute::kTypeHashString, "tag attribute %s that is not of the expected type [HashString].", CClipEventTags::VFXName.GetCStr()))
					{
						const crPropertyAttributeHashString* hashAttrib = (const crPropertyAttributeHashString*)attrib;
						if (hashAttrib->GetValue().GetHash()==m_vfxName.GetHash())
						{
							const crPropertyAttribute* aTri = prop->GetAttribute(CClipEventTags::Trigger);
							bool isTriggered = (
								aTri && 
								aiVerifyf(aTri->GetType() == crPropertyAttribute::kTypeBool, "tag attribute %s that is not of the expected type [BOOL].", CClipEventTags::Trigger.GetCStr()) && 
								((const crPropertyAttributeBool*)aTri)->GetBool()
								) ? true : false;

							CParticleAttr temp;
							InitParticleData(temp);

							if (isTriggered)
							{
								vfxAssertf(!g_vfxEntity.GetIsInfinitelyLooped(temp), "VFX {%s} cant be used as a triggered effect.", m_vfxName.GetCStr());
								g_vfxEntity.TriggerPtFxEntityAnim(pPhys, &temp);					
							}
							else
							{
								vfxAssertf(g_vfxEntity.GetIsInfinitelyLooped(temp), "VFX {%s} cant be used as a registered effect.", m_vfxName.GetCStr());
								g_vfxEntity.UpdatePtFxEntityAnim(pPhys, &temp, m_data->m_effectId);
							}
						}
					}
					
				}
			}
		}
		else
		{
			// update the registered effect
			CParticleAttr temp;
			InitParticleData(temp);
			vfxAssertf(g_vfxEntity.GetIsInfinitelyLooped(temp), "VFX {%s} cant be used as a registered effect.", m_vfxName.GetCStr());
			g_vfxEntity.UpdatePtFxEntityAnim(pPhys, &temp, m_data->m_effectId);
		}
	}
}

void CAnimScenePlayVfxEvent::InitParticleData(CParticleAttr& data)
{
	Quaternion tquat;
	data.m_fxType = PT_ANIM_FX;
	data.m_allowRubberBulletShotFx = false;
	data.m_allowElectricBulletShotFx = false;
	data.m_ignoreDamageModel = true;
	data.m_playOnParent = false;
	data.m_onlyOnDamageModel = false;
	data.m_tagHashName = m_vfxName;
	data.SetPos(VEC3V_TO_VECTOR3(m_offsetPosition));
	data.m_scale = m_scale;
	data.m_probability = m_probability;
	data.m_boneTag = m_boneId;
	data.m_hasTint = false;

	if (m_color.GetColor() != 0)
	{
		data.m_hasTint = true;
		data.m_tintR = m_color.GetRed();
		data.m_tintG = m_color.GetGreen();
		data.m_tintB = m_color.GetBlue();
		data.m_tintA = m_color.GetAlpha();
	}

	tquat.FromEulers(VEC3V_TO_VECTOR3(m_offsetEulerRotation));
	data.qX = tquat.x;
	data.qY = tquat.y;
	data.qZ = tquat.z;
	data.qW = tquat.w;
}

void CAnimScenePlayVfxEvent::OnExit(CAnimScene* UNUSED_PARAM(pScene), AnimSceneUpdatePhase phase)
{
	if (phase==kAnimSceneUpdatePreAnim || phase==kAnimSceneShuttingDown)
	{
		ReleaseVfxRequest();
	}
}

CAnimScenePlayVfxEvent::~CAnimScenePlayVfxEvent()
{
	ReleaseVfxRequest();
}

void CAnimScenePlayVfxEvent::ReleaseVfxRequest()
{
	if(m_data->m_streamingSlot >= 0)
	{
		CConditionalAnimStreamingVfxManager::GetInstance().UnrequestVfxAssetSlot(m_data->m_streamingSlot);
		m_data->m_streamingSlot = -1;
	}
}

CTaskScriptedAnimation* CAnimScenePlayVfxEvent::FindScriptedAnimTask(const CPhysical& phys) const
{
	CTaskScriptedAnimation* pTask = NULL;

	if(phys.GetIsTypePed())
	{
		const CPed& ped = static_cast<const CPed&>(phys);
		const CPedIntelligence* pPedIntelligence = ped.GetPedIntelligence();	
		if (pPedIntelligence)
		{
			pTask = static_cast<CTaskScriptedAnimation*>(pPedIntelligence->FindTaskActiveByType(CTaskTypes::TASK_SCRIPTED_ANIMATION));
				
			if (pTask==NULL)
			{
				pTask = static_cast<CTaskScriptedAnimation*>(pPedIntelligence->GetTaskSecondaryPartialAnim());
				if (pTask && pTask->GetTaskType()!=CTaskTypes::TASK_SCRIPTED_ANIMATION)
					pTask = NULL;
			}
		}
	}
	else if(phys.GetIsTypeObject())
	{
		const CObject& object = static_cast<const CObject&>(phys);
		const CObjectIntelligence* pIntelligence = object.GetObjectIntelligence();
		if (pIntelligence)
		{
			pTask = static_cast< CTaskScriptedAnimation* >(pIntelligence->FindTaskByType(CTaskTypes::TASK_SCRIPTED_ANIMATION));

			if (pTask==NULL)
			{
				pTask = static_cast< CTaskScriptedAnimation* >(pIntelligence->FindTaskSecondaryByType(CTaskTypes::TASK_SCRIPTED_ANIMATION));
			}
		}
	}
	else if(phys.GetIsTypeVehicle())
	{
		const CVehicle& veh = static_cast<const CVehicle&>(phys);
		const CVehicleIntelligence* pIntelligence = veh.GetIntelligence();
		if (pIntelligence)
		{
			pTask = static_cast< CTaskScriptedAnimation* >(pIntelligence->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_SECONDARY, CTaskTypes::TASK_SCRIPTED_ANIMATION));
		}
	}

	return pTask;
}

#if __BANK
bool CAnimScenePlayVfxEvent::InitForEditor(const CAnimSceneEventInitialiser* pInitialiser)
{
	CAnimSceneEvent::InitForEditor(pInitialiser);
	m_entity.SetEntity(pInitialiser->m_pEntity);
	SetPreloadDuration(4.0f);
	return true;
}
#endif //__BANK