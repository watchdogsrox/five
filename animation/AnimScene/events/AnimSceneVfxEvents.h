#ifndef _ANIM_SCENE_VFX_EVENTS_H_
#define _ANIM_SCENE_VFX_EVENTS_H_

// rage includes
#include "fwanimation/boneids.h"

// game includes
#include "animation/AnimScene/AnimSceneEvent.h"
#include "animation/AnimScene/AnimSceneHelpers.h"

struct CParticleAttr;
class CTaskScriptedAnimation;

//////////////////////////////////////////////////////////////////////////
// Loads and triggers playback of a visual effect
//////////////////////////////////////////////////////////////////////////

class CAnimScenePlayVfxEvent : public CAnimSceneEvent
{
public:

	CAnimScenePlayVfxEvent()
		: m_vfxName(atHashString::Null())
		//, m_entity()
		, m_offsetPosition(V_ZERO)
		, m_offsetEulerRotation(V_ZERO)
		, m_scale(1.0f)
		, m_probability(100)
		, m_boneId(BONETAG_INVALID)
		//, m_color()
		, m_continuous(false)
		, m_useEventTags(false)
	{

	}

	CAnimScenePlayVfxEvent(const CAnimScenePlayVfxEvent& other)
		: CAnimSceneEvent(other)
		, m_vfxName(other.m_vfxName)
		, m_entity(other.m_entity)
		, m_offsetPosition(other.m_offsetPosition)
		, m_offsetEulerRotation(other.m_offsetEulerRotation)
		, m_scale(other.m_scale)
		, m_probability(other.m_probability)
		, m_boneId(other.m_boneId)
		, m_color(other.m_color)
		, m_continuous(other.m_continuous)
		, m_useEventTags(other.m_useEventTags)
	{
	}

	virtual CAnimSceneEvent* Copy() const { return rage_new CAnimScenePlayVfxEvent(*this); }

	virtual ~CAnimScenePlayVfxEvent();

	virtual bool OnEnterPreload(CAnimScene* pScene);
	virtual bool OnUpdatePreload(CAnimScene* pScene);

	virtual void OnEnter(CAnimScene* pScene, AnimSceneUpdatePhase phase);
	virtual void OnUpdate(CAnimScene* pScene, AnimSceneUpdatePhase phase);
	virtual void OnExit(CAnimScene* pScene, AnimSceneUpdatePhase phase);

	virtual void OnInit(CAnimScene* pScene);
	virtual void OnShutdown(CAnimScene* pScene);

	virtual u32 GetType() { return PlayVfxEvent; }

	virtual CAnimSceneEntity::Id GetPrimaryEntityId() { return m_entity.GetId(); }

#if __BANK
	virtual bool InitForEditor(const CAnimSceneEventInitialiser* pInitialiser);
#endif // __BANK

private:
	
	void InitParticleData(CParticleAttr& data);
	void ReleaseVfxRequest();
	// PURPOSE: Search for a running animation task on the passed in entity.
	CTaskScriptedAnimation* FindScriptedAnimTask(const CPhysical& phys) const;

	// PURPOSE: the vfx name
	atHashString m_vfxName;
	// PURPOSE: Optional entity to play back the particle effect on.
	CAnimSceneEntityHandle m_entity;
	// PURPOSE: Position / rotation offset for the effect
	Vec3V m_offsetPosition;
	Vec3V m_offsetEulerRotation;
	// PURPOSE: Effect scale
	float m_scale;
	// PURPOSE: Probability of firing
	s32 m_probability;
	// PURPOSE: bone tag on the scene entity to attach the effect
	eAnimBoneTag m_boneId;
	// PURPOSE: The color of the effect
	Color32 m_color;
	// PURPOSE: If true, the effect is a one off and should be triggered on the enter of the event
	//			If false, continuously update the effect whilst the event is active
	bool m_continuous;
	// PURPOSE: If true, only load the effect by default, and trigger / update it based on event tags from the entities animations.
	bool m_useEventTags; 

	// PURPOSE: Helper variables for streaming purposes
	struct RuntimeData{
		RuntimeData()
			: m_streamingSlot(-1)
			, m_effectId(-1)
		{}
		s16 m_streamingSlot;
		s32 m_effectId;
	};
		
	DECLARE_ANIM_SCENE_RUNTIME_DATA(RuntimeData, m_data);
			
	PAR_PARSABLE;
};

#endif //_ANIM_SCENE_VFX_EVENTS_H_
