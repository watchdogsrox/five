//
// audio/environment/occlusion/occlusionportalinfo.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef NAOCCLUSIONPORTALINFO_H
#define NAOCCLUSIONPORTALINFO_H

// Game
#include "audio/environment/occlusion/occlusioninteriormetadata.h"
#include "audio/environment/occlusion/occlusionportalentity.h"
#include "scene/RegdRefTypes.h"
#include "control/replay/ReplaySettings.h" 

// Framework
#include "fwmaths/PortalCorners.h"
#include "fwscene/world/InteriorLocation.h"

// Rage
#include "atl/referencecounter.h"

class CInteriorInst;
class CInteriorProxy;
namespace rage
{
	struct InteriorSettings;
	struct InteriorRoom;
	struct PortalSettings;
}

struct naOcclusionPerPortalSettingsOverride
{
	s32 roomIndex;
	s32 portalIndex;
	u32 overridenSettingsHash;
};

typedef atArray<naOcclusionPerPortalSettingsOverride> naOcclusionPerPortalSettingsOverrideList;

// Contains all the information necessary to get a portal
class naOcclusionPortalInfo : public rage::atReferenceCounter
{

public:

	FW_REGISTER_CLASS_POOL(naOcclusionPortalInfo);

	naOcclusionPortalInfo();
	~naOcclusionPortalInfo();

	static void InitClass();  

	static naOcclusionPortalInfo* Allocate();

	void InitFromMetadata(naOcclusionPortalInfoMetadata* portalInfoMetadata);
	
	// Updates the linked entities and glass blockage metrics
	void Update();

	// Blockage is based on InteriorSettings values, door openness and occlusion values, and amount of broken glass.
	f32 GetEntityBlockage(const InteriorSettings* intSettings, const InteriorRoom* intRoom, const bool useInteriorSettingsMetrics) const;

	const CInteriorInst* GetInteriorInst() const;		// CInteriorInst's are streamed in and out, so if it's not loaded this will return NULL
	const CInteriorProxy* GetInteriorProxy() const;		// CInteriorProxy's are always loaded, so this only returns NULL if we haven't initialized properly
	u32 GetUniqueInteriorProxyHashkey() const;
	s32 GetRoomIdx() const;
	s32 GetPortalIdx() const;
	u32 GetDestInteriorHash() const;
	s32 GetDestRoomIdx() const;

	static const atMap<u32, u32>& GetPortalSettingsOverrideMap() { return sm_PortalSettingsScriptOverrideMap; }
	static const atMap<u32, naOcclusionPerPortalSettingsOverrideList>& GetPerPortalSettingsOverrideMap() { return sm_PerPortalSettingsScriptOverrideMap; }
	void SetInteriorProxy(const CInteriorProxy* intProxy) { m_IntProxy = intProxy; }

	naOcclusionPortalEntity* GetPortalEntity(const s32 idx) { return m_LinkedEntities[idx]; }
	void SetPortalEntity(const s32 idx, naOcclusionPortalEntity* portalEntity) { m_LinkedEntities[idx] = portalEntity; }
	s32 GetNumLinkedEntities() const { return m_LinkedEntities.GetCount(); }
	void ResizeLinkedEntityArray(const s32 size) { m_LinkedEntities.Resize(size); }

	void SetPortalCorners(const fwPortalCorners* portalCorners) { m_PortalCorners = portalCorners; }
	const fwPortalCorners* GetPortalCorners() const { return m_PortalCorners; }

	void SetPortalSettings(const PortalSettings* portalSettings) { m_PortalSettings = portalSettings; }
	const PortalSettings* GetPortalSettings() const { return m_PortalSettings; }

	static void SetPortalSettingsScriptOverride(const char* oldPortalSettingsName, const char* newPortalSettingsName);
	static void SetPortalSettingsScriptOverride(u32 interiorNameHash, s32 roomIndex, s32 portalIndex, const char* newPortalSettingsName);
	static void RemovePortalSettingsScriptOverride(const char* portalSettingsName);
	static void RemovePortalSettingsScriptOverride(u32 interiorNameHash, s32 roomIndex, s32 portalIndex);

#if GTA_REPLAY
	static void SetReplayPortalSettingsOverride(u32 oldHash, u32 newHash);
	static void SetReplayPerPortalSettingsOverride(u32 interiorNameHash, s32 roomIndex, s32 portalIndex, u32 portalSettingsHash);
	static void ClearReplayPortalSettingsOverrides();
#endif

#if __DEV
	const char* GetInteriorName();
	const char* GetRoomName();
	const char* GetDestinationInteriorName();
	const char* GetDestinationRoomName();
#endif

#if __BANK
	void SetInteriorProxyHash(const u32 intProxyHash);
	void SetRoomIdx(const s32 roomIdx);
	void SetPortalIdx(const u32 portalIdx);
	void SetDestInteriorHash(const u32 destInteriorHash);
	void SetDestInteriorRoomIdx(const s32 destRoomIdx);

	void BuildPortalEntityArray();
#endif

private:

	// Grab the corners and PortalSettings from the CInteriorInst
	void UpdatePortalInfoSettings();

	// Based on the entity link type tries to grab RegdEnt* to the entities that are attached to the portal
	void FindPortalEntities();

	// Looks at the CInteriorInst::GetPortalObjInstancePtr to find attached entities.
	void GetPortalLinkedEntities(const CInteriorInst* intInst);

	// Looks at the CInteriorInst::GetPortalObjInstancePtr to find attached entities on the corresponding matched portal (same portal different interior)
	void GetMatchingPortalLinkedEntities(const CInteriorInst* intInst);

	// Does a box probe defined by portal corners and uses the FindDoorsCB() to add them to the m_aLinkedEntity
	void GetNonLinkedDoorEntities();

	// Creates a naOcclusionPortalEntity and populates all the data needed
	// Store a RedgEnt, this must be done on the game thread.
	void AddEntity(const CEntity* entity, audOcclusionPortalEntityLinkType linkType);

	// Checks if the entity is already in the m_aLinkedEntity array
	bool IsEntityInArray(const CEntity* entity);

	// Looks through the array to find the slot that matches the entity's hashkey
	naOcclusionPortalEntity* GetPortalEntitySlot(const CEntity* entity, audOcclusionPortalEntityLinkType linkType);

	// Calls an update on all the naOcclusionPortalEntity's that are attached
	void UpdatePortalEntities();

	// When doors are open or windows are broken, we resize the portal to leak sounds specifically from those locations
	void CombinePortalEntityBounds();

	// Computes a modifier to dampen occlusion based on the total area of all the broken glass on the attached entities
	void ComputeGlassBlockageModifier();

#if __BANK
	void DrawDebug();
#endif

	// Callback from gWorldMgr.ForAllEntitiesIntersecting.  Checks to see if there are any entities with door physics or DoorSounds game objects
	static bool FindDoorsCB(CEntity* entity, void* data);


public:

#if __BANK
	static const s32 sm_OcclusionBuildPortalInfoPoolSize = 2000;
#endif

private:

	atArray<naOcclusionPortalEntity*> m_LinkedEntities;

	const fwPortalCorners* m_PortalCorners;
	const CInteriorProxy* m_IntProxy;
	const PortalSettings* m_PortalSettings;

	u32 m_InteriorProxyHash;
	s32 m_PortalIdx;
	s32 m_RoomIdx;
	u32 m_DestInteriorHash;
	s32 m_DestRoomIdx;
	f32 m_GlassBlockageModifier;

	static atMap<u32, u32> sm_PortalSettingsScriptOverrideMap;
	static atMap<u32, naOcclusionPerPortalSettingsOverrideList> sm_PerPortalSettingsScriptOverrideMap;
	REPLAY_ONLY(static atMap<u32, u32> sm_PortalSettingsReplayOverrideMap;)
	REPLAY_ONLY(static atMap<u32, naOcclusionPerPortalSettingsOverrideList> sm_PerPortalSettingsReplayOverrideMap;)

	static naOcclusionPortalInfo* sm_CurrentPortalToAddNonLinkedDoors;

	static audCurve sm_GlassBrokenAreaToBlockage;

	static u32 sm_LastFrameProbeUsedToDetectDoor;
	static f32 sm_MaxDistanceForDoorProbe2;
};

#endif // NAOCCLUSIONPORTALINFO_H
