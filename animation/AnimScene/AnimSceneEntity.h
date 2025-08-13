#ifndef _ANIM_SCENE_ENTITY_H_
#define _ANIM_SCENE_ENTITY_H_

// rage includes
#include "atl/binmap.h"
#include "atl/hashstring.h"
#include "atl/string.h"
#include "data/base.h"
#include "physics/debugplayback.h"
#include "parser/macros.h"
#include "script/value.h"
#include "script/thread.h"
#include "vectormath/quatv.h"
#include "vectormath/vec3v.h"

// framework includes
#include "fwtl/regdrefs.h"

// game includes
#include "animation/AnimScene/AnimSceneEntityTypes.h"

class CAnimScene;
class CAnimSceneEntityInitialiser;

struct AnimSceneEntityLocationData
{
	AnimSceneEntityLocationData()
		: m_startPos(V_ZERO)
		, m_startRot(V_IDENTITY)
		, m_startTime(0.0f)
		, m_endPos(V_ZERO)
		, m_endRot(V_IDENTITY)
		, m_endTime(0.0f)
	{

	}
	
	Vec3V m_startPos;
	QuatV m_startRot;
	float m_startTime;
	Vec3V m_endPos;
	QuatV m_endRot;
	float m_endTime;

	PAR_PARSABLE;
};

// keep in sync with ANIM_SCENE_ENTITY_LOCATION_DATA in commands_anim.sch
struct scrAnimSceneEntityLocationData
{
	scrVector m_startPos;
	scrVector m_startRot;
	scrVector m_endPos;
	scrVector m_endRot;
};

//
// animation/AnimScene/AnimSceneEntity.h
//
// CAnimSceneEntity
// represents a single entity or data value referenced in an anim scene
//
class CAnimSceneEntity : public /*fwRefAwareBase*/ datBase
{
public:

	typedef atHashString Id;

	CAnimSceneEntity()
		: m_pScene(NULL)
	{

	}

	CAnimSceneEntity(const CAnimSceneEntity& other)
		: m_Id(other.m_Id)
		, m_pScene(NULL)
	{
		for (s32 i=0; i<other.m_locationData.GetCount(); i++)
		{
			SetLocationData(*other.m_locationData.GetKey(i), **other.m_locationData.GetItem(i));
		}
	}

	virtual ~CAnimSceneEntity()
	{

	}

	virtual eAnimSceneEntityType GetType() const { return ANIM_SCENE_ENTITY_NONE; }

	// returns the unique id for this entity.
	Id GetId() const { return m_Id; }
	// PURPOSE: Set the id for the event
	void SetId(Id id) { m_Id = id; }

	// shutdown the entity at the end of the scene
	virtual void Init(CAnimScene* pScene) { m_pScene = pScene; }

	// shutdown the entity at the end of the scene
	virtual void Shutdown(CAnimScene* UNUSED_PARAM(pScene)) {}

	// PURPOSE: Make and return a copy of this event.
	virtual CAnimSceneEntity* Copy()
	{
		return rage_new CAnimSceneEntity(*this);
	}

	// PURPOSE: Return a cached pointer to the anim scene this entity belongs to
	CAnimScene* GetScene() { return m_pScene; }

	// PURPOSE: Does this entity support loading and saving situations for each playback list
	virtual bool SupportsLocationData() { return false; }
	// PURPOSE: Get the start and end location for the entity in this playback list
	const AnimSceneEntityLocationData* GetLocationData(atHashString playbackListId) const;
	// PURPOSE: Set this entities start and end location for the given playback list
	void SetLocationData(atHashString playbackListId, const AnimSceneEntityLocationData& mat);
	// PURPOSE: Get the start and end location of the entity in the given playback list
	AnimSceneEntityLocationData* GetLocationData(atHashString playbackListId);
	// PURPOSE: Return true from this if the entity should be removed from playback on this instance of the scene.
	//			All events that return this entity as the primary event will also be disabled.
	virtual bool IsDisabled() const { return false; }
	// PURPOSE: Return true if the entity is not required
	virtual bool IsOptional() const { return false; }

	// PURPOSE: Called if the entity is removed from the scene early
	virtual void OnRemovedFromSceneEarly() {};

	// PURPOSE: Called if the entity is removed from the scene early
	virtual void OnOtherEntityRemovedFromSceneEarly(CAnimSceneEntity::Id ) {};

	// PURPOSE: Called once at the start of the scene
	void StartOfScene() { OnStartOfScene(); }

	// PURPOSE: Called once at the start of the scene. Override this to add any custom functionality
	virtual void OnStartOfScene() {};

	// PURPOSE: Check whether the id is valid for this entity.
	static bool IsValidId(const CAnimSceneEntity::Id& id);

#if __BANK
	// PURPOSE: Called automatically before the pargen widgets for the scene are added - used to add editing functionality 
	void OnPreAddWidgets(bkBank& pBank);

	// PURPOSE: Called automatically after the pargen widgets for the scene are added - used to add editing functionality 
	void OnPostAddWidgets(bkBank& pBank);

	// PURPOSE: Callback function to rename the entity (change it's Id)
	void Rename();

	// PURPOSE: Called when a new event is added through the anim scene editor.
	virtual void InitForEditor(CAnimSceneEntityInitialiser*) { }

#if __DEV
	// PURPOSE: Handle debug rendering for this entity
	virtual void DebugRender(CAnimScene* pScene, debugPlayback::OnScreenOutput& mainOutput);
#endif //__DEV

	// PURPOSE: override this to change the rendered debug text in the editor
	virtual atString GetDebugString();

	// PURPOSE: override this to add custom click functionality
	virtual void OnDebugTextSelected() {};

	// PURPOSE: Return true only if this entity is owned by a scene currently being used in
	// an anim scene editor.
	bool IsInEditor();
#endif //__BANK

#if !__NO_OUTPUT
	static const char * GetEntityTypeName(eAnimSceneEntityType type);
#endif //__NO_OUTPUT

	PAR_PARSABLE;

protected:

	Id m_Id;

	CAnimScene* m_pScene;

	atBinaryMap<AnimSceneEntityLocationData*, atHashString> m_locationData; // caches the start and end locations for each playback list
};

//typedef	fwRegdRef<class CAnimSceneEntity> RegdAnimSceneEnt;
//typedef	fwRegdRef<const class CAnimSceneEntity>	RegdConstAnimSceneEnt;

extern const CAnimSceneEntity::Id ANIM_SCENE_ENTITY_ID_INVALID;

#endif //_ANIM_SCENE_ENTITY_H_
