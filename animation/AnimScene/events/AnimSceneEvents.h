#ifndef _ANIM_SCENE_EVENTS_H_
#define _ANIM_SCENE_EVENTS_H_

// framework includes
#include "entity/archetypemanager.h"
#include "streaming/streamingrequest.h"

// game includes
#include "animation\AnimScene\AnimScene.h"
#include "animation/AnimScene/AnimSceneEvent.h"
#include "animation/AnimScene/AnimSceneHelpers.h"
#include "peds/PedMotionState.h"
//////////////////////////////////////////////////////////////////////////
// Ped control
//////////////////////////////////////////////////////////////////////////

class CAnimSceneForceMotionStateEvent : public CAnimSceneEvent
{
public:

	CAnimSceneForceMotionStateEvent()
	{

	}

	CAnimSceneForceMotionStateEvent(const CAnimSceneForceMotionStateEvent& other)
		: CAnimSceneEvent(other)
		, m_motionState(other.m_motionState)
		, m_restartState(other.m_restartState)
	{
		m_ped = other.m_ped;
	}

	virtual ~CAnimSceneForceMotionStateEvent()
	{
	}


	virtual void OnEnter(CAnimScene* pScene, AnimSceneUpdatePhase phase);

	virtual void OnInit(CAnimScene* pScene);
	virtual void OnShutdown(CAnimScene* pScene);

	virtual CAnimSceneEvent* Copy() const { return rage_new CAnimSceneForceMotionStateEvent(*this); }

	virtual u32 GetType() { return ForceMotionStateEvent; }

	virtual CAnimSceneEntity::Id GetPrimaryEntityId() { return m_ped.GetId(); }

#if __BANK
	// PURPOSE: Called automatically after the pargen widgets for the scene are added - used to add editing functionality 
	void OnPostAddWidgets(bkBank& pBank);
	// PURPOSE: Callback for the drop down list
	void OnWidgetChanged();
	virtual bool InitForEditor(const CAnimSceneEventInitialiser* pInitialiser);
#endif //__BANK

private:
	CAnimSceneEntityHandle m_ped;
	CPedMotionStates::eMotionState m_motionState;
	bool m_restartState;

#if __BANK
	struct Widgets{
		Widgets(){}
		s32 m_selection;
	};
#endif //__BANK

	DECLARE_ANIM_SCENE_BANK_DATA(Widgets, m_widgets);

	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
// Entity creation events
//////////////////////////////////////////////////////////////////////////

class CAnimSceneCreatePedEvent : public CAnimSceneEvent
{
public:

	CAnimSceneCreatePedEvent()
	{

	}

	CAnimSceneCreatePedEvent(const CAnimSceneCreatePedEvent& other)
		: CAnimSceneEvent(other)
	{
		m_entity = other.m_entity;
		m_modelName = other.m_modelName;
	}

	virtual ~CAnimSceneCreatePedEvent()
	{
	}

	virtual CAnimSceneEvent* Copy() const { return rage_new CAnimSceneCreatePedEvent(*this); }

	virtual bool OnUpdatePreload(CAnimScene* pScene);
	virtual void OnEnter(CAnimScene* pScene, AnimSceneUpdatePhase phase);
	virtual void OnExit(CAnimScene* pScene, AnimSceneUpdatePhase phase);

	virtual void OnInit(CAnimScene* pScene);

	virtual u32 GetType() { return CreatePedEvent; }

	virtual CAnimSceneEntity::Id GetPrimaryEntityId() { return m_entity.GetId(); }

#if __BANK
	virtual bool InitForEditor(const CAnimSceneEventInitialiser* pInitialiser);
#endif // __BANK

private:
	CAnimSceneEntityHandle m_entity;
	atFinalHashString m_modelName;

	struct RuntimeData{
		fwModelId m_modelId;
		strRequest m_modelRequest;
		RegdPed m_pPed; // the ped we created
	};

	DECLARE_ANIM_SCENE_RUNTIME_DATA(RuntimeData, m_data);
	
	PAR_PARSABLE;
};

class CAnimSceneCreateObjectEvent : public CAnimSceneEvent
{
public:

	CAnimSceneCreateObjectEvent()
	{

	}

	CAnimSceneCreateObjectEvent(const CAnimSceneCreateObjectEvent& other)
		: CAnimSceneEvent(other)
	{
		m_entity = other.m_entity;
		m_modelName = other.m_modelName;
	}

	virtual ~CAnimSceneCreateObjectEvent()
	{
	}

	virtual CAnimSceneEvent* Copy() const { return rage_new CAnimSceneCreateObjectEvent(*this); }

	virtual bool OnUpdatePreload(CAnimScene* pScene);
	virtual void OnEnter(CAnimScene* pScene, AnimSceneUpdatePhase phase);
	virtual void OnExit(CAnimScene* pScene, AnimSceneUpdatePhase phase);

	virtual void OnInit(CAnimScene* pScene);
	virtual u32 GetType() { return CreateObjectEvent; }

	virtual CAnimSceneEntity::Id GetPrimaryEntityId() { return m_entity.GetId(); }

#if __BANK
	virtual bool InitForEditor(const CAnimSceneEventInitialiser* pInitialiser);
#endif // __BANK

private:
	CAnimSceneEntityHandle m_entity;
	atFinalHashString m_modelName;

	struct RuntimeData{
		fwModelId m_modelId;
		strRequest m_modelRequest;
		RegdObj m_pObject; // the entity we created
	};

	DECLARE_ANIM_SCENE_RUNTIME_DATA(RuntimeData, m_data);

	PAR_PARSABLE;
};

class CAnimSceneCreateVehicleEvent : public CAnimSceneEvent
{
public:

	CAnimSceneCreateVehicleEvent()
	{

	}

	CAnimSceneCreateVehicleEvent(const CAnimSceneCreateVehicleEvent& other)
		: CAnimSceneEvent(other)
	{
		m_entity = other.m_entity;
		m_modelName = other.m_modelName;
	}

	virtual ~CAnimSceneCreateVehicleEvent()
	{
	}

	virtual CAnimSceneEvent* Copy() const { return rage_new CAnimSceneCreateVehicleEvent(*this); }

	virtual bool OnUpdatePreload(CAnimScene* pScene);
	virtual void OnEnter(CAnimScene* pScene, AnimSceneUpdatePhase phase);
	virtual void OnExit(CAnimScene* pScene, AnimSceneUpdatePhase phase);

	virtual void OnInit(CAnimScene* pScene);
	virtual u32 GetType() { return CreateVehicleEvent; }
	virtual CAnimSceneEntity::Id GetPrimaryEntityId() { return m_entity.GetId(); }

#if __BANK
	virtual bool InitForEditor(const CAnimSceneEventInitialiser* pInitialiser);
#endif // __BANK

private:
	CAnimSceneEntityHandle m_entity;
	atFinalHashString m_modelName;

	struct RuntimeData{
		fwModelId m_modelId;
		strRequest m_modelRequest;
		RegdVeh m_pVehicle; // the entity we created
	};

	DECLARE_ANIM_SCENE_RUNTIME_DATA(RuntimeData, m_data);
	
	PAR_PARSABLE;
};

class CAnimScenePlaySceneEvent : public CAnimSceneEvent
{
public:

	CAnimScenePlaySceneEvent()
		: m_bLoopScene(false)
		, m_bBreakOutImmediately(false)
	{

	}

	CAnimScenePlaySceneEvent(const CAnimScenePlaySceneEvent& other)
		: CAnimSceneEvent(other)
	{
		m_trigger = other.m_trigger;
		m_sceneName = other.m_sceneName;
		m_bLoopScene = other.m_bLoopScene;
		m_bBreakOutImmediately = other.m_bBreakOutImmediately;
		m_breakOutTrigger = other.m_breakOutTrigger;
	}

	virtual ~CAnimScenePlaySceneEvent()
	{

	}

	virtual CAnimSceneEvent* Copy() const { return rage_new CAnimScenePlaySceneEvent(*this); }

	virtual bool OnUpdatePreload(CAnimScene* pScene);

	virtual void OnUpdate(CAnimScene* pScene, AnimSceneUpdatePhase phase);
	virtual void OnExit(CAnimScene* pScene, AnimSceneUpdatePhase phase);

	virtual void OnInit(CAnimScene* pScene);

	virtual u32 GetType() { return PlaySceneEvent; }

#if __BANK
	bool InitForEditor(const CAnimSceneEventInitialiser* pInitialiser);
#endif // __BANK

private:

	// helper function to handle loading the new anim scene
	bool ProcessLoadScene(CAnimScene* pScene);

	atHashString m_sceneName;
	CAnimSceneEntityHandle m_trigger;
	
	// PURPOSE: If true, loops this scene (and internally loops the
	//			master scene) until the breakout condition is met.
	bool m_bLoopScene;
	// PURPOSE: The condition to break out of the loop (must be set if m_bLoopScene is specified)
	CAnimSceneEntityHandle m_breakOutTrigger;
	// PURPOSE: If set, jump the parent scene time immediately to the end of this event. If false,
	//			the scene will complete its loop before moving on.
	bool m_bBreakOutImmediately;

	struct RuntimeData{
		RuntimeData()
			: m_pScene(NULL)
			, m_bSceneTriggered(false)
		{

		}
		CAnimScene* m_pScene;
		bool m_bSceneTriggered;
	};

	DECLARE_ANIM_SCENE_RUNTIME_DATA(RuntimeData, m_data);

	PAR_PARSABLE;
};

class CAnimSceneInternalLoopEvent  : public CAnimSceneEvent
{
public:

	CAnimSceneInternalLoopEvent()
		: m_bBreakOutImmediately(false)
		, m_bDisable(false)
	{

	}

	CAnimSceneInternalLoopEvent(const CAnimSceneInternalLoopEvent& other)
		: CAnimSceneEvent(other)
	{
		m_bBreakOutImmediately = other.m_bBreakOutImmediately;
		m_breakOutTrigger = other.m_breakOutTrigger;
		m_bDisable = other.m_bDisable;
	}

	virtual ~CAnimSceneInternalLoopEvent()
	{

	}

	virtual CAnimSceneEvent* Copy() const { return rage_new CAnimSceneInternalLoopEvent(*this); }


	virtual void OnEnter(CAnimScene* pScene, AnimSceneUpdatePhase phase);
	virtual void OnUpdate(CAnimScene* pScene, AnimSceneUpdatePhase phase);

	virtual void OnInit(CAnimScene* pScene);

	virtual u32 GetType() { return InternalLoopEvent; }

	virtual CAnimSceneEntity::Id GetPrimaryEntityId() { return m_breakOutTrigger.GetId(); }

#if __BANK
	virtual bool InitForEditor(const CAnimSceneEventInitialiser* pInitialiser);
#endif //__BANK

private:
	// PURPOSE: The condition to break out of the loop
	CAnimSceneEntityHandle m_breakOutTrigger;
	// PURPOSE: If set, jump the parent scene time immediately to the end of this event. If false,
	//			the scene will complete its loop before moving on.
	bool m_bBreakOutImmediately;

	// PURPOSE: Disable the loop if the breakout event doesn't exist in the scene metadata.
	bool m_bDisable;

	PAR_PARSABLE;
};

#endif //_ANIM_SCENE_EVENTS_H_
