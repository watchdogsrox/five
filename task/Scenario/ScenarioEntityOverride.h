//
// task/Scenario/ScenarioEntityOverride.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_SCENARIO_SCENARIOENTITYOVERRIDE_H
#define TASK_SCENARIO_SCENARIOENTITYOVERRIDE_H

// Rage headers
#include "atl/hashstring.h"
#include "parser/macros.h"
#include "vectormath/vec3v.h"

// Framework headers

// Game headers
#include "scene/loader/MapData_Extensions.h"
#include "task/Scenario/ScenarioPoint.h"

// Forwards
class CEntity;
class CScenarioPointRegion;

#define SCENARIO_VERIFY_ENTITY_OVERRIDES (SCENARIO_DEBUG)
#define SCENARIO_ENTITY_OVERRIDE_PERF_TRACK (SCENARIO_DEBUG)

/*
PURPOSE
	Data about scenario point overrides for a specific entity.
*/
class CScenarioEntityOverride
{
public:

#if SCENARIO_DEBUG
	// PURPOSE:	Initialize this object as an empty override for the given entity.
	void Init(CEntity& entity);

	void PrepSaveObject(const CScenarioEntityOverride& prepFrom);

	static bool GetDefaultMayNotAlwaysExist(const CEntity& entity);
#endif // SCENARIO_DEBUG

	static void PostPsoPlace(void* data);

	// PURPOSE:	Position of entity we are overriding.
	Vec3V								m_EntityPosition;

	// PURPOSE:	Type of entity we are overriding.
	atHashString						m_EntityType;

	// PURPOSE:	Array of persistent data for the scenarios that should be attached to this entity.
	atArray<CExtensionDefSpawnPoint>	m_ScenarioPoints;

	// PURPOSE:	Points currently attached to the entity, parallel to m_ScenarioPoints[].
	// NOTES:	If we are careful, we may be able to not have these be registered objects.
	atArray<RegdScenarioPnt>			m_ScenarioPointsInUse; //NON-parser data

	// PURPOSE:	Pointer to the entity we are currently overriding, if any.
	// NOTES:	Here too, if we are careful, we may be able to get away without using a registered object.
	RegdEnt								m_EntityCurrentlyAttachedTo; //NON-parser data

	// PURPOSE:	Normally, we expect that if we are close to the override position, the
	//			associated entity should be there, and if it's not found, we consider it
	//			an error. In some cases, we have IPLs that can be turned on or off,
	//			and in that case, we don't want to consider this an error.
	bool								m_EntityMayNotAlwaysExist;

	// PURPOSE:	Normally, a CScenarioEntityOverride replaces any art-attached points on a prop with
	//			other points, but if those points are removed, the art-attached points will come back.
	//			If m_SpecificallyPreventArtPoints is set, the CScenarioEntityOverride is specifically there
	//			to disable the art-attached points, and it's valid for it to be empty.
	bool								m_SpecificallyPreventArtPoints;

#if SCENARIO_VERIFY_ENTITY_OVERRIDES
	enum
	{
		kVerificationStatusUnknown,
		kVerificationStatusBroken,
		kVerificationStatusBrokenProp,
		kVerificationStatusStruggling,
		kVerificationStatusWorking,
		kVerificationStatusInteriorNotStreamedIn
	};
#endif	// SCENARIO_VERIFY_ENTITY_OVERRIDES
	u8		m_VerificationStatus; //NON-parser data, but padded out we use this space anyway

	PAR_SIMPLE_PARSABLE;
};

//-----------------------------------------------------------------------------

class CScenarioEntityOverridePostLoadUpdateQueue
{
public:
	static CScenarioEntityOverridePostLoadUpdateQueue& GetInstance() { Assert(sm_Instance); return *sm_Instance; }
	static void InitClass() { Assert(!sm_Instance); sm_Instance = rage_new CScenarioEntityOverridePostLoadUpdateQueue(); }
	static void ShutdownClass() { Assert(sm_Instance); delete sm_Instance; sm_Instance = NULL; }

	CScenarioEntityOverridePostLoadUpdateQueue();

	void Update();

	void AddToQueue(CScenarioPointRegion* region);
	void RemoveFromQueue(CScenarioPointRegion* region);

	// Check if this entity is pending for the given region
	bool IsEntityPending(CScenarioPointRegion& region, CEntity& entity);

private:

	u32 m_LastObject;
	u32 m_LastBuilding;
#if SCENARIO_ENTITY_OVERRIDE_PERF_TRACK
	float m_AccumulatedTime;
	float m_MaxTime;
	u32 m_TotalUpdates;
#endif

	atArray<CScenarioPointRegion*> m_QueuedRegions;
	static CScenarioEntityOverridePostLoadUpdateQueue* sm_Instance;
};

//-----------------------------------------------------------------------------

#if SCENARIO_VERIFY_ENTITY_OVERRIDES

/*
PURPOSE
	This class handles verification of scenario entity overrides, basically checking
	to make sure that the overrides are not misplaced for example due to the map
	data changing (props moving around, changing type, etc). The results may get
	communicated to the user by failing an assert, or may just affect debug drawing.
*/
class CScenarioEntityOverrideVerification
{
public:
	static CScenarioEntityOverrideVerification* GetInstancePtr() { return sm_Instance; }
	static CScenarioEntityOverrideVerification& GetInstance() { Assert(sm_Instance); return *sm_Instance; }
	static void InitClass() { Assert(!sm_Instance); sm_Instance = rage_new CScenarioEntityOverrideVerification; }
	static void ShutdownClass() { Assert(sm_Instance); delete sm_Instance; sm_Instance = NULL; }

	CScenarioEntityOverrideVerification();

	void Update();

	void ClearInfoForRegion(CScenarioPointRegion& reg);
	void ClearAllStruggling();

protected:
	struct Tunables
	{
		Tunables();

		float	m_BrokenPropSearchRange;
		float	m_ExpectStreamedInDistance;
		float	m_StruggleTime;
		int		m_NumToCheckPerFrame;
	};

	struct StrugglingOverride
	{
		u16		m_RegionIndex;
		u16		m_OverrideIndex;
		float	m_TimeFirstStruggling;
	};

	int FindStruggleIndex(int regionIndex, int overrideIndex) const;

	void UpdateEntityOverride(int regionIndex, int overrideIndex);

	struct CheckNearbyBrokenObjectCallbackData
	{
		CheckNearbyBrokenObjectCallbackData() : m_DesiredModelInfo(NULL), m_ObjectFound(false) {}

		CBaseModelInfo*	m_DesiredModelInfo;
		bool			m_ObjectFound;
	};
	static bool CheckBrokenObjectOfTypeCallback(CEntity* pEntity, void* pData);
	static bool IsOverrideStruggling(const CScenarioEntityOverride& override);

	atArray<StrugglingOverride>	m_ScenarioOverridesStruggling;

	int							m_CurrentRegionIndex;
	int							m_CurrentOverrideIndex;

	static Tunables				sm_Tunables;

	static CScenarioEntityOverrideVerification* sm_Instance;
};

#endif	// SCENARIO_VERIFY_ENTITY_OVERRIDES

#endif // TASK_SCENARIO_SCENARIOENTITYOVERRIDE_H