#if __BANK

#ifndef _ANIM_SCENE_EVENT_INITIALISERS_H_
#define _ANIM_SCENE_EVENT_INITIALISERS_H_

#include "atl/hashstring.h"
#include "parser/macros.h"

#include "animation/debug/AnimDebug.h"

class CAnimSceneEditor;
class CanimSceneEntity;
class CAnimSceneSection;
class CAnimSceneEvent;

class CAnimSceneEventInitialiser
{
public:
	CAnimSceneEventInitialiser()
		: m_pEditor(NULL)
		, m_pEntity(NULL)
		, m_bRequiresWidgetCleanup(false)
	{

	}

	virtual ~CAnimSceneEventInitialiser() {}

	virtual void AddWidgets(bkGroup*, CAnimSceneEditor*) {};
	virtual void RemoveWidgets(bkGroup*, CAnimSceneEditor*) {};
	virtual atHashString GetEventTypeName() { return (u32)0; }
	virtual atHashString GetFriendlyEventTypeName() { return (u32)0; }
	virtual bool SupportsEntityType( eAnimSceneEntityType ) { return false; } // which entity types is this event valid for?

	CAnimSceneEditor* m_pEditor;
	CAnimSceneEntity* m_pEntity;
	CAnimSceneSection* m_pSection;

	bool m_bRequiresWidgetCleanup;

#if __ASSERT
	static const char * GetStructureName(const CAnimSceneEvent* pEvt);
#endif //_ASSERT

	template<class Return_T>
	static const Return_T* Cast(const CAnimSceneEventInitialiser* pInit, const CAnimSceneEvent* ASSERT_ONLY(pEvt))
	{
		animAssertf(pInit, "NULL event initialiser passed to '%s'!", GetStructureName(pEvt));
		if (pInit)
		{
			const Return_T* pCastInit = dynamic_cast<const Return_T*>(pInit);
			if (animVerifyf(pCastInit, "Wrong initialiser type '%s' passed to anim scene event '%s'!", pInit->parser_GetStructure()->GetName(), GetStructureName(pEvt)))
			{
				return pCastInit;
			}
		}
		return NULL;
	}

	PAR_PARSABLE;
};


// PURPOSE: Simplifies declaring initialisers for basic types (no special widgets, etc required)
#define DECLARE_SIMPLE_ANIM_SCENE_EVENT_INITIALISER(className, eventClassName, friendlyName, allowedEntityType)	\
public:																								\
	className()																						\
	:CAnimSceneEventInitialiser()																	\
{																									\
}																									\
	virtual ~className(){};																			\
	virtual atHashString GetEventTypeName() { return atHashString(eventClassName); }				\
	virtual atHashString GetFriendlyEventTypeName() { return atHashString(friendlyName); }			\
	virtual bool SupportsEntityType( eAnimSceneEntityType type) { return type==allowedEntityType; }	\
	PAR_PARSABLE;																					\
public:																								\
	//END

class CAnimSceneEquipWeaponEventInitialiser : public CAnimSceneEventInitialiser
{
public:
	CAnimSceneEquipWeaponEventInitialiser()
		:CAnimSceneEventInitialiser()
		, m_entitySelector()
	{

	}

	virtual ~CAnimSceneEquipWeaponEventInitialiser(){};

	virtual void AddWidgets(bkGroup* pGroup, CAnimSceneEditor* pEditor);
	virtual atHashString GetEventTypeName() { return atHashString("CAnimSceneEquipWeaponEvent"); }
	virtual atHashString GetFriendlyEventTypeName() { return atHashString("Equip weapon"); }
	virtual bool SupportsEntityType( eAnimSceneEntityType type) { return type==ANIM_SCENE_ENTITY_PED; }// which entity types is this event valid for?

	CDebugAnimSceneEntitySelector m_entitySelector;

	PAR_PARSABLE;
};

class CAnimScenePlayAnimEventInitialiser : public CAnimSceneEventInitialiser
{
public:
	CAnimScenePlayAnimEventInitialiser()
		:CAnimSceneEventInitialiser()
		, m_clipSelector(true, false, false)
	{

	}

	virtual ~CAnimScenePlayAnimEventInitialiser(){};

	virtual void AddWidgets(bkGroup* pGroup, CAnimSceneEditor*) { m_clipSelector.AddWidgets(pGroup); };
	virtual void RemoveWidgets(bkGroup* pGroup, CAnimSceneEditor*) { m_clipSelector.RemoveWidgets(CDebugClipSelector::FindBank(pGroup)); };
	virtual atHashString GetEventTypeName() { return atHashString("CAnimScenePlayAnimEvent"); }
	virtual atHashString GetFriendlyEventTypeName() { return atHashString("Play anim"); }
	virtual bool SupportsEntityType( eAnimSceneEntityType type) // which entity types is this event valid for?
	{ 
		switch (type)
		{
		case ANIM_SCENE_ENTITY_PED:
		case ANIM_SCENE_ENTITY_OBJECT:
		case ANIM_SCENE_ENTITY_VEHICLE:
			return true;
		default:
			return false;
		}
	} 

	CDebugClipSelector m_clipSelector;

	PAR_PARSABLE;
};

class CAnimScenePlayCameraAnimEventInitialiser : public CAnimSceneEventInitialiser
{
public:
	CAnimScenePlayCameraAnimEventInitialiser()
		:CAnimSceneEventInitialiser()
		, m_clipSelector(true, false, false)
	{

	}

	virtual ~CAnimScenePlayCameraAnimEventInitialiser(){};

	virtual void AddWidgets(bkGroup* pGroup, CAnimSceneEditor*) { m_clipSelector.AddWidgets(pGroup); };
	virtual void RemoveWidgets(bkGroup* pGroup, CAnimSceneEditor*) { m_clipSelector.RemoveWidgets(CDebugClipSelector::FindBank(pGroup)); };
	virtual atHashString GetEventTypeName() { return atHashString("CAnimScenePlayCameraAnimEvent"); }
	virtual atHashString GetFriendlyEventTypeName() { return atHashString("Play camera anim"); }
	virtual bool SupportsEntityType( eAnimSceneEntityType type) // which entity types is this event valid for?
	{ 
		switch (type)
		{
		case ANIM_SCENE_ENTITY_CAMERA:
			return true;
		default:
			return false;
		}
	} 

	CDebugClipSelector m_clipSelector;

	PAR_PARSABLE;
};

class CAnimSceneForceMotionStateEventInitialiser: public CAnimSceneEventInitialiser
{
	DECLARE_SIMPLE_ANIM_SCENE_EVENT_INITIALISER(CAnimSceneForceMotionStateEventInitialiser,"CAnimSceneForceMotionStateEvent", "Force motion state", ANIM_SCENE_ENTITY_PED);
};

class CAnimSceneCreatePedEventInitialiser: public CAnimSceneEventInitialiser
{
	DECLARE_SIMPLE_ANIM_SCENE_EVENT_INITIALISER(CAnimSceneCreatePedEventInitialiser,"CAnimSceneCreatePedEvent", "Create ped", ANIM_SCENE_ENTITY_PED);
	virtual void AddWidgets(bkGroup*, CAnimSceneEditor*);
	CDebugPedModelSelector m_pedSelector;
};

class CAnimSceneCreateObjectEventInitialiser: public CAnimSceneEventInitialiser
{
	DECLARE_SIMPLE_ANIM_SCENE_EVENT_INITIALISER(CAnimSceneCreateObjectEventInitialiser,"CAnimSceneCreateObjectEvent", "Create object", ANIM_SCENE_ENTITY_OBJECT);
	virtual void AddWidgets(bkGroup*, CAnimSceneEditor*);
	virtual void RemoveWidgets(bkGroup*, CAnimSceneEditor*);
	CDebugObjectSelector m_objectSelector;
};


class CAnimSceneCreateVehicleEventInitialiser: public CAnimSceneEventInitialiser
{
	DECLARE_SIMPLE_ANIM_SCENE_EVENT_INITIALISER(CAnimSceneCreateVehicleEventInitialiser,"CAnimSceneCreateVehicleEvent", "Create vehicle", ANIM_SCENE_ENTITY_VEHICLE);
	virtual void AddWidgets(bkGroup*, CAnimSceneEditor*);
	virtual void RemoveWidgets(bkGroup*, CAnimSceneEditor*);
	CDebugVehicleSelector m_vehicleSelector;
};

class CAnimScenePlaySceneEventInitialiser: public CAnimSceneEventInitialiser
{
	DECLARE_SIMPLE_ANIM_SCENE_EVENT_INITIALISER(CAnimScenePlaySceneEventInitialiser,"CAnimScenePlaySceneEvent", "Play scene", ANIM_SCENE_ENTITY_NONE);
	virtual void AddWidgets(bkGroup*, CAnimSceneEditor*);
	virtual void RemoveWidgets(bkGroup*, CAnimSceneEditor*);
	CDebugAnimSceneSelector m_sceneSelector;
};

class CAnimSceneInternalLoopEventInitialiser: public CAnimSceneEventInitialiser
{
	DECLARE_SIMPLE_ANIM_SCENE_EVENT_INITIALISER(CAnimSceneInternalLoopEventInitialiser,"CAnimSceneInternalLoopEvent", "Internal loop", ANIM_SCENE_BOOLEAN);
	virtual void AddWidgets(bkGroup*, CAnimSceneEditor*);
	bool m_bBreakOutImmediately;
};

class CAnimScenePlayVfxEventInitialiser: public CAnimSceneEventInitialiser
{
	DECLARE_SIMPLE_ANIM_SCENE_EVENT_INITIALISER(CAnimScenePlayVfxEventInitialiser,"CAnimScenePlayVfxEvent", "Play Vfx", ANIM_SCENE_ENTITY_PED);
};

#endif //_ANIM_SCENE_EVENT_INITIALISERS_H_

#endif //__BANK
