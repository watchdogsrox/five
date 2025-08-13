#if __BANK

#ifndef _ANIM_SCENE_ENTITY_INITIALISERS_H_
#define _ANIM_SCENE_ENTITY_INITIALISERS_H_

#include "animation/AnimScene/events/AnimSceneEventInitialisers.h"

class CAnimSceneEditor;

class CAnimSceneEntityInitialiser
{
public:
	CAnimSceneEntityInitialiser()
		: m_pEditor(NULL)
		, m_bRequiresWidgetCleanup(false)
	{

	}

	virtual ~CAnimSceneEntityInitialiser() {}

	virtual void AddWidgets(bkGroup*, CAnimSceneEditor*) {};
	virtual void RemoveWidgets(bkGroup*, CAnimSceneEditor*) {};
	virtual atHashString GetEntityTypeName() { return (u32)0; }
	virtual atHashString GetFriendlyEntityTypeName() { return (u32)0; }

	CAnimSceneEditor* m_pEditor;

	bool m_bRequiresWidgetCleanup;

	template<class Return_T>
	static Return_T* Cast(CAnimSceneEntityInitialiser* pInit, const CAnimSceneEntity* ASSERT_ONLY(pEnt))
	{
		animAssertf(pInit, "NULL event initialiser passed to '%s'!", pEnt->parser_GetStructure()->GetName());
		if (pInit)
		{
			Return_T* pCastInit = dynamic_cast<Return_T*>(pInit);
			if (animVerifyf(pCastInit, "Wrong initialiser type '%s' passed to anim scene entity '%s'!", pInit->parser_GetStructure()->GetName(), pEnt->parser_GetStructure()->GetName()))
			{
				return pCastInit;
			}
		}
		return NULL;
	}

	PAR_PARSABLE;
};

// PURPOSE: Simplifies declaring initialisers for basic types (no special widgets, etc required)
#define DECLARE_SIMPLE_ANIM_SCENE_ENTITY_INITIALISER(className, entityClassName, friendlyName)		\
public:																								\
	className()																						\
	:CAnimSceneEntityInitialiser()																	\
	{																								\
	}																								\
	virtual ~className(){};																			\
	virtual atHashString GetEntityTypeName() { return atHashString(entityClassName); }				\
	virtual atHashString GetFriendlyEntityTypeName() { return atHashString(friendlyName); }			\
	PAR_PARSABLE;																					\
public:																								\
	//END


class CAnimScenePedInitialiser : public CAnimSceneEntityInitialiser
{
	DECLARE_SIMPLE_ANIM_SCENE_ENTITY_INITIALISER(CAnimScenePedInitialiser, "CAnimScenePed", "Ped");
	virtual void AddWidgets(bkGroup*, CAnimSceneEditor*);
	virtual void RemoveWidgets(bkGroup*, CAnimSceneEditor*);

	CDebugPedModelSelector m_pedSelector;
	CAnimScenePlayAnimEventInitialiser m_animEventInitialiser;
};

class CAnimSceneObjectInitialiser : public CAnimSceneEntityInitialiser
{
	DECLARE_SIMPLE_ANIM_SCENE_ENTITY_INITIALISER(CAnimSceneObjectInitialiser, "CAnimSceneObject", "Object");
	virtual void AddWidgets(bkGroup*, CAnimSceneEditor*);
	virtual void RemoveWidgets(bkGroup*, CAnimSceneEditor*);

	CDebugObjectSelector m_objectSelector;
	CAnimScenePlayAnimEventInitialiser m_animEventInitialiser;
};

class CAnimSceneVehicleInitialiser : public CAnimSceneEntityInitialiser
{
	DECLARE_SIMPLE_ANIM_SCENE_ENTITY_INITIALISER(CAnimSceneVehicleInitialiser, "CAnimSceneVehicle", "Vehicle");
	virtual void AddWidgets(bkGroup*, CAnimSceneEditor*);
	virtual void RemoveWidgets(bkGroup*, CAnimSceneEditor*);

	CDebugVehicleSelector m_vehicleSelector;
	CAnimScenePlayAnimEventInitialiser m_animEventInitialiser;
};

class CAnimSceneBooleanInitialiser : public CAnimSceneEntityInitialiser
{
	DECLARE_SIMPLE_ANIM_SCENE_ENTITY_INITIALISER(CAnimSceneBooleanInitialiser, "CAnimSceneBoolean", "Bool");
	virtual void AddWidgets(bkGroup*, CAnimSceneEditor*);

	bool m_bDefault;
};

class CAnimSceneCameraInitialiser : public CAnimSceneEntityInitialiser
{
	DECLARE_SIMPLE_ANIM_SCENE_ENTITY_INITIALISER(CAnimSceneCameraInitialiser, "CAnimSceneCamera", "Camera");
	virtual void AddWidgets(bkGroup*, CAnimSceneEditor*);
	virtual void RemoveWidgets(bkGroup*, CAnimSceneEditor*);

	CAnimScenePlayCameraAnimEventInitialiser m_animEventInitialiser;
};

#endif //_ANIM_SCENE_ENTITY_INITIALISERS_H_

#endif //__BANK