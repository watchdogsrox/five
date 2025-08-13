// 
// audio/environment/occlusion/occlusionportalinfo.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#include "audio/audio_channel.h"
#include "audio/dooraudioentity.h"
#include "audio/environment/occlusion/occlusionportalinfo.h"
#include "audio/northaudioengine.h"

#include "camera/CamInterface.h"
#include "modelinfo/MloModelInfo.h"
#include "Objects/door.h"

#include "fwscene/stores/mapdatastore.h"
#include "control/replay/Audio/AmbientAudioPacket.h"

AUDIO_ENVIRONMENT_OPTIMISATIONS()

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(naOcclusionPortalInfo, CONFIGURED_FROM_FILE, 0.41f, atHashString("OcclusionPortalInfo",0xea0ec0e8));	

// Recommended LOD in 3DMax for a standard door is 45, so this should give us a little breathing room.
f32 naOcclusionPortalInfo::sm_MaxDistanceForDoorProbe2 = 20.0f * 20.0f;	

u32 naOcclusionPortalInfo::sm_LastFrameProbeUsedToDetectDoor = 0;

naOcclusionPortalInfo* naOcclusionPortalInfo::sm_CurrentPortalToAddNonLinkedDoors = NULL;

audCurve naOcclusionPortalInfo::sm_GlassBrokenAreaToBlockage;

atMap<u32, u32> naOcclusionPortalInfo::sm_PortalSettingsScriptOverrideMap;
atMap<u32, naOcclusionPerPortalSettingsOverrideList> naOcclusionPortalInfo::sm_PerPortalSettingsScriptOverrideMap;
REPLAY_ONLY(atMap<u32, u32> naOcclusionPortalInfo::sm_PortalSettingsReplayOverrideMap;)
REPLAY_ONLY(atMap<u32, naOcclusionPerPortalSettingsOverrideList> naOcclusionPortalInfo::sm_PerPortalSettingsReplayOverrideMap;)

#if __BANK
extern bool g_DebugPortalInfo;
extern f32 g_DebugPortalInfoRange;
#endif

naOcclusionPortalInfo::naOcclusionPortalInfo() : 
m_IntProxy(NULL), 
m_PortalCorners(NULL), 
m_PortalSettings(NULL), 
m_InteriorProxyHash(0),
m_PortalIdx(INTLOC_INVALID_INDEX),
m_RoomIdx(INTLOC_INVALID_INDEX),
m_DestInteriorHash(0),
m_DestRoomIdx(INTLOC_INVALID_INDEX),
m_GlassBlockageModifier(1.0f)
{
}

naOcclusionPortalInfo::~naOcclusionPortalInfo()
{
	const s32 numLinkedEntities = GetNumLinkedEntities();
	for(s32 entityIdx = 0; entityIdx < numLinkedEntities; entityIdx++)
	{
		naOcclusionPortalEntity* portalEntity = GetPortalEntity(entityIdx);
		if(portalEntity)
		{
			delete portalEntity;
		}			
	}
}

void naOcclusionPortalInfo::InitClass()
{
	sm_GlassBrokenAreaToBlockage.Init(ATSTRINGHASH("GLASS_BROKEN_AREA_TO_BLOCKAGE", 0x074a7adfd));
	sm_CurrentPortalToAddNonLinkedDoors = NULL;
	sm_LastFrameProbeUsedToDetectDoor = 0;
	sm_PortalSettingsScriptOverrideMap.Reset();
	sm_PerPortalSettingsScriptOverrideMap.Reset();
	REPLAY_ONLY(sm_PortalSettingsReplayOverrideMap.Reset();)
	REPLAY_ONLY(sm_PerPortalSettingsReplayOverrideMap.Reset();)
}

naOcclusionPortalInfo* naOcclusionPortalInfo::Allocate()
{
	if(naVerifyf(naOcclusionPortalInfo::GetPool()->GetNoOfFreeSpaces() > 0, "naOcclusionPortalInfo pool is full, need to increase size"))
	{
		return rage_new naOcclusionPortalInfo();
	}
	return NULL;
}

void naOcclusionPortalInfo::InitFromMetadata(naOcclusionPortalInfoMetadata* portalInfoMetadata)
{
	m_InteriorProxyHash = portalInfoMetadata->m_InteriorProxyHash;
	m_PortalIdx = portalInfoMetadata->m_PortalIdx;
	m_RoomIdx = portalInfoMetadata->m_RoomIdx;
	m_DestInteriorHash = portalInfoMetadata->m_DestInteriorHash;
	m_DestRoomIdx = portalInfoMetadata->m_DestRoomIdx;

	m_IntProxy = audNorthAudioEngine::GetOcclusionManager()->GetInteriorProxyFromUniqueHashkey(GetUniqueInteriorProxyHashkey());
	naCWarningf(m_IntProxy, "Couldn't find CInteriorProxy from audio occlusion metadata, rebuild paths");

	s32 numPortalEntities = portalInfoMetadata->m_PortalEntityList.GetCount();
		s32 numSpacesNeeded = naOcclusionPortalEntity::GetPool()->GetNoOfFreeSpaces() - numPortalEntities;
	
	// make sure there is enough room in the pool before resizing the LinkedEntityArray
	if (naVerifyf(numSpacesNeeded >= 0, "naOcclusionPortalEntity pool is full (size %d), need to increase pool size by %d", naOcclusionPortalEntity::GetPool()->GetSize(), ABS(numSpacesNeeded)))
	{
		ResizeLinkedEntityArray(numPortalEntities);
		for(s32 entityIdx = 0; entityIdx < numPortalEntities; entityIdx++)
		{
			naOcclusionPortalEntity* portalEntity = naOcclusionPortalEntity::Allocate();
			if(portalEntity)
			{
				portalEntity->InitFromMetadata(&portalInfoMetadata->m_PortalEntityList[entityIdx]);
				SetPortalEntity(entityIdx, portalEntity);
			}					
		}
	}
}

void naOcclusionPortalInfo::Update()
{
	UpdatePortalInfoSettings();

	FindPortalEntities();

	UpdatePortalEntities();

	ComputeGlassBlockageModifier();

#if __BANK
	if(g_DebugPortalInfo)
	{
		DrawDebug();
	}
#endif
}

void naOcclusionPortalInfo::UpdatePortalInfoSettings()
{
	const CInteriorInst* intInst = GetInteriorInst();
	if(intInst && intInst->IsPopulated())
	{
		// Update this if we haven't already gotten the portal corners, or if we have portal settings as we need to check for an overridden value
		if((!m_PortalCorners || m_PortalSettings) && GetRoomIdx() < (s32)intInst->GetNumRooms() && GetPortalIdx() < intInst->GetNumPortalsInRoom(GetRoomIdx()))
		{
			const s32 globalPortalIdx = intInst->GetPortalIdxInRoom(GetRoomIdx(), GetPortalIdx());
			m_PortalCorners = &intInst->GetPortal(globalPortalIdx);

			const CMloPortalDef& portalDef = intInst->GetMloModelInfo()->GetPortals()[globalPortalIdx];

			if(portalDef.m_audioOcclusion)
			{
				// Check if we've overridden this by script (or a script override stored by the replay)
				const u32* overridenPortalSettings = NULL;								

#if GTA_REPLAY
				if(CReplayMgr::IsEditModeActive())
				{
					overridenPortalSettings = sm_PortalSettingsReplayOverrideMap.Access(portalDef.m_audioOcclusion);
				}
				else
#endif
				{
					overridenPortalSettings = sm_PortalSettingsScriptOverrideMap.Access(portalDef.m_audioOcclusion);
				}		

				if(overridenPortalSettings)
				{
					m_PortalSettings = audNorthAudioEngine::GetObject<PortalSettings>(*overridenPortalSettings);
				}
				else
				{
					m_PortalSettings = audNorthAudioEngine::GetObject<PortalSettings>(portalDef.m_audioOcclusion);
				}

				naOcclusionPerPortalSettingsOverrideList* naOcclusionPerPortalSettingsOverrides = nullptr;

#if GTA_REPLAY
				if(CReplayMgr::IsEditModeActive())
				{
					naOcclusionPerPortalSettingsOverrides = sm_PerPortalSettingsReplayOverrideMap.Access(intInst->GetModelNameHash());
				}
				else
#endif
				{
					naOcclusionPerPortalSettingsOverrides = sm_PerPortalSettingsScriptOverrideMap.Access(intInst->GetModelNameHash());
				}

				if (naOcclusionPerPortalSettingsOverrides)
				{
					for (naOcclusionPerPortalSettingsOverride& interiorPortalOverrides : *naOcclusionPerPortalSettingsOverrides)
					{
						if (interiorPortalOverrides.roomIndex == GetRoomIdx() && interiorPortalOverrides.portalIndex == GetPortalIdx())
						{
							m_PortalSettings = audNorthAudioEngine::GetObject<PortalSettings>(interiorPortalOverrides.overridenSettingsHash);
						}
					}
				}
			}
		}
	}
	else
	{
		// If this is a portal on another CinteriorInst, and it streams out, we want to make sure we're not pointing at those corners anymore 
		m_PortalCorners = NULL;
	}
}

void naOcclusionPortalInfo::FindPortalEntities()
{
	const CInteriorInst* intInst = GetInteriorInst();
	if(intInst && intInst->IsPopulated() && GetRoomIdx() < (s32)intInst->GetNumRooms() && GetPortalIdx() < intInst->GetNumPortalsInRoom(GetRoomIdx()))
	{
		const s32 numLinkedEntities = m_LinkedEntities.GetCount();

		// Do we need to update linked entities?  We do if we have an AUD_OCC_LINK_TYPE_PORTAL type but we don't have an entity
		for(s32 i = 0; i < numLinkedEntities; i++)
		{
			if(!m_LinkedEntities[i]->GetEntity() && m_LinkedEntities[i]->GetLinkType() == AUD_OCC_LINK_TYPE_PORTAL)
			{
				GetPortalLinkedEntities(intInst);
				break;
			}
		}

		// Do we need to get matching portal linked entities?
		for(s32 i = 0; i < numLinkedEntities; i++)
		{
			if(!m_LinkedEntities[i]->GetEntity() && m_LinkedEntities[i]->GetLinkType() == AUD_OCC_LINK_TYPE_MATCHING_PORTAL)
			{
				GetMatchingPortalLinkedEntities(intInst);
				break;
			}
		}

		// Do we need to get an entity that's in an exterior scene?  See if all our constraints are met and then try the probe if so.
		const u32 frameCount = fwTimer::GetFrameCount();
		for(s32 i = 0; i < numLinkedEntities; i++)
		{
			if(!m_LinkedEntities[i]->GetEntity() && m_LinkedEntities[i]->GetLinkType() == AUD_OCC_LINK_TYPE_EXTERIOR && m_PortalCorners)
			{
				// Only probe for 1 door per 31 fames so we don't cause spikes since GetNonLinkedDoorEntitiesForPortal is using a probe.
				if(frameCount > sm_LastFrameProbeUsedToDetectDoor + 31)
				{
					// Do a distance check to make sure that we're not probing when the only the LOD door is there
					Vector3 portalCenter;
					portalCenter = m_PortalCorners->GetCorner(0) + m_PortalCorners->GetCorner(1) + m_PortalCorners->GetCorner(2) + m_PortalCorners->GetCorner(3);
					portalCenter.Scale(0.25f);
					const Vector3 listenerPosition = audNorthAudioEngine::GetMicrophones().GetLocalEnvironmentScanPosition();
					const f32 dist2 = (portalCenter - listenerPosition).Mag2();

					// Also do a check to make sure the door is on an interior you're in, or that you're outside.  We don't need to worry about doors
					// in different interiors than us, and they're also most likely not streamed in anyway.
					const CInteriorInst* playerIntInst = audNorthAudioEngine::GetGtaEnvironment()->GetListenerInteriorInstance();

					// Help avoid probes by checking if the door is in the frustum.  If it's not then there's a decent chance it's streamed out.
					const bool isVisible = camInterface::IsSphereVisibleInGameViewport(portalCenter, 1.0f);

					if(dist2 < sm_MaxDistanceForDoorProbe2 &&  (!playerIntInst || playerIntInst == intInst) && isVisible)
					{
						GetNonLinkedDoorEntities();
						sm_LastFrameProbeUsedToDetectDoor = frameCount;
					}
				}
		
				break;
			}
		}
	}
}

void naOcclusionPortalInfo::GetPortalLinkedEntities(const CInteriorInst* intInst)
{
	if(naVerifyf(intInst, "naOcclusionPortalInfo::GetPortalLinkedEntities Passed NULL CInteriorInst* when checking if entities are attached"))
	{
		const s32 globalPortalIdx = intInst->GetPortalIdxInRoom(GetRoomIdx(), GetPortalIdx());
		const u32 numPortalObjects = intInst->GetNumObjInPortal(globalPortalIdx);
		// Need to go through all of them since we're not guaranteed that if the 1st slot is NULL the 2nd is NULL as well
		for(u32 i = 0; i < numPortalObjects; i++)
		{
			const CEntity* entity = intInst->GetPortalObjInstancePtr(GetRoomIdx(), GetPortalIdx(), i);
			if(entity)
			{
				AddEntity(entity, AUD_OCC_LINK_TYPE_PORTAL);
			}
		}
	}
}

void naOcclusionPortalInfo::GetMatchingPortalLinkedEntities(const CInteriorInst* intInst)
{
	if(naVerifyf(intInst, "naOcclusionPortalInfo::GetMatchingPortalLinkedEntities Passed NULL CInteriorInst* when checking if entities are attached"))
	{
		CInteriorInst* destIntInst;
		s32 destRoomIdx;
		s32 linkedIntRoomLimboPortalIdx;	// Portal Index of room 0 used to reach this destination
		audNorthAudioEngine::GetOcclusionManager()->GetPortalDestinationInfo(intInst, GetRoomIdx(), GetPortalIdx(), &destIntInst, &destRoomIdx, &linkedIntRoomLimboPortalIdx);

		//Check to make sure the data is valid
		if(destIntInst && destIntInst->m_bIsPopulated && destRoomIdx != INTLOC_INVALID_INDEX && linkedIntRoomLimboPortalIdx != INTLOC_INVALID_INDEX)
		{
			const s32 globalPortalIdx = destIntInst->GetPortalIdxInRoom(INTLOC_ROOMINDEX_LIMBO, (u32)linkedIntRoomLimboPortalIdx);
			const s32 numPortalObjects = destIntInst->GetNumObjInPortal(globalPortalIdx);

			// Need to go through all of them since we're not guaranteed that if the 1st slot is NULL the 2nd is NULL as well
			for(s32 i = 0; i < numPortalObjects; i++)
			{
				// Use INTLOC_ROOMINDEX_LIMBO instead of destRoomIdx because the linkedIntRoomLimboPortalIdx is based off room 0 or limbo
				const CEntity* entity = destIntInst->GetPortalObjInstancePtr(INTLOC_ROOMINDEX_LIMBO, (u32)linkedIntRoomLimboPortalIdx, i);
				if(entity)
				{
					AddEntity(entity, AUD_OCC_LINK_TYPE_MATCHING_PORTAL);
				}
			}
		}
	}
}

void naOcclusionPortalInfo::GetNonLinkedDoorEntities()
{
	// We need to use a static callback function, so we need to store a global variable for which naOcclusionPortalInfo object we're looking at
	sm_CurrentPortalToAddNonLinkedDoors = this;

	// Use a box intersection test, since that's guaranteed to find all the doors in a portal, and hopefully shouldn't find anything nearby we don't want.
	const Vec3V vMin(Min(VECTOR3_TO_VEC3V(m_PortalCorners->GetCorner(0)), VECTOR3_TO_VEC3V(m_PortalCorners->GetCorner(2))));
	const Vec3V vMax(Max(VECTOR3_TO_VEC3V(m_PortalCorners->GetCorner(0)), VECTOR3_TO_VEC3V(m_PortalCorners->GetCorner(2))));

	spdAABB portalBox(Vec3V(vMin - Vec3V(V_QUARTER)), Vec3V(vMax + Vec3V(V_QUARTER)));
	fwIsBoxInside intersectionTest(portalBox); 

	CGameWorld::ForAllEntitiesIntersecting(&intersectionTest, FindDoorsCB, NULL, ENTITY_TYPE_MASK_OBJECT, SEARCH_LOCATION_EXTERIORS);
}

bool naOcclusionPortalInfo::FindDoorsCB(CEntity* entity, void* )
{
	// See if the doors have door physics or if they have a DoorSounds game object.  Check for the game object in case the physics isn't initialized.
	naAssertf(entity, "naOcclusionPortalInfo::FindDoorsCB Null ptr entity (CEntity*) passed");
	naCErrorf(entity->GetType() == ENTITY_TYPE_OBJECT, "entity has type %d which is invalid", entity->GetType());

	// Because we're loading doors across the map, doors with fragments will not load their physics unless you're close enough.
	// So we only need to check to make sure it's a supported door type, and whether or not we've hooked up door sounds to the model
	if(entity->GetIsTypeObject() && (((CObject*)entity)->IsADoor()))
	{
		// Make sure it's not in a MILO
		if(!entity->GetAudioInteriorLocation().IsValid() && sm_CurrentPortalToAddNonLinkedDoors)
		{
			sm_CurrentPortalToAddNonLinkedDoors->AddEntity(entity, AUD_OCC_LINK_TYPE_EXTERIOR);
		}
	} 

	return true;
}

// If we're computing occlusion paths, than add the door to the array and store the occlusion, otherwise we just want to store the pointer
void naOcclusionPortalInfo::AddEntity(const CEntity* entity, audOcclusionPortalEntityLinkType linkType)
{
	if(naVerifyf(entity, "naOcclusionPortalInfo::AddEntity Passed NULL CEntity*") && !entity->GetIsTypeDummyObject() && !entity->GetIsTypeLight())
	{
		// Make sure we don't already have this entity and that it's a valid entity to add (door or window)
		const bool isEntityInArray = IsEntityInArray(entity);
		const bool isDoor = audNorthAudioEngine::GetOcclusionManager()->ComputeIsEntityADoor(entity);
		const bool isGlass = audNorthAudioEngine::GetOcclusionManager()->ComputeIsEntityGlass(entity);

		if(!isEntityInArray && (isDoor || isGlass))
		{
			naOcclusionPortalEntity* portalEntity = NULL;
			bool portalEntityAllocated = false;
#if __BANK
			if (PARAM_audOcclusionBuildEnabled.Get())
			{
				if(audNorthAudioEngine::GetOcclusionManager()->GetIsBuildingOcclusionPaths())
				{
					portalEntity = naOcclusionPortalEntity::Allocate();
					portalEntityAllocated = true;
					if(portalEntity)
					{
						portalEntity->SetLinkType(linkType);
						portalEntity->SetMaxOcclusion(audNorthAudioEngine::GetOcclusionManager()->ComputeMaxDoorOcclusion(entity));
						portalEntity->SetEntityModelHashkey(entity->GetBaseModelInfo()->GetHashKey());
						portalEntity->SetIsDoor(isDoor);
						portalEntity->SetIsGlass(isGlass);

						naDisplayf("Found an entity: %s", entity->GetBaseModelInfo()->GetModelName());

						// We need to allocate memory for this new naOcclusionPortalEntity* pointer, so grow it rather than set it.
						m_LinkedEntities.PushAndGrow(portalEntity);
					}
				}
			}
#endif

			if (!portalEntityAllocated)
			{
				portalEntity = GetPortalEntitySlot(entity, linkType);
			}

			if(portalEntity)
			{
				portalEntity->SetEntity(entity);
			}
		}
	}
}

bool naOcclusionPortalInfo::IsEntityInArray(const CEntity* entity)
{
	if(entity)
	{
		for(s32 i = 0; i < m_LinkedEntities.GetCount(); i++)
		{
			if(m_LinkedEntities[i]->GetEntity() == entity)
			{
				return true;
			}
		}
	}
	return false;
}

naOcclusionPortalEntity* naOcclusionPortalInfo::GetPortalEntitySlot(const CEntity* entity, audOcclusionPortalEntityLinkType linkType)
{
	if(naVerifyf(entity, "naOcclusionPortalInfo::GetPortalEntitySlot Passed NULL CEntity*"))
	{
		// Find the slot that's expecting this model and this link type
		for(s32 i = 0; i < m_LinkedEntities.GetCount(); i++)
		{
			if(!m_LinkedEntities[i]->GetEntity() 
				&& m_LinkedEntities[i]->GetLinkType() == linkType 
				&& m_LinkedEntities[i]->GetEntityModelHashkey() == entity->GetBaseModelInfo()->GetHashKey())
			{
				return m_LinkedEntities[i];
			}
		}
	}
	
	return NULL;
}

void naOcclusionPortalInfo::UpdatePortalEntities()
{
	for(s32 i = 0; i < m_LinkedEntities.GetCount(); i++)
	{
		if(m_LinkedEntities[i]->GetEntity())
		{
			m_LinkedEntities[i]->Update();
		}
	}
}

void naOcclusionPortalInfo::ComputeGlassBlockageModifier()
{
	// Get the total area of broken glass, and get a blockage value off of that number
	f32 brokenGlassArea = 0.0f;
	for(s32 i = 0; i < m_LinkedEntities.GetCount(); i++)
	{
		if(m_LinkedEntities[i]->GetEntity() && m_LinkedEntities[i]->GetIsGlass())
		{
			brokenGlassArea += m_LinkedEntities[i]->GetBrokenGlassArea();
		}
	}
	m_GlassBlockageModifier = sm_GlassBrokenAreaToBlockage.CalculateValue(brokenGlassArea);
}

f32 naOcclusionPortalInfo::GetEntityBlockage(const InteriorSettings* interiorSettings, const InteriorRoom* intRoom, const bool useInteriorSettingsMetrics) const
{
	f32 portalBlockage = 0.0f;

	// If we have a door, then use that
	bool usingDoor = false;
	f32 doorBlockage = 1.0f;	// Set max so we can come down for double door cases
	f32 portalSettingsBlockage = 1.0f;
	for(s32 i = 0; i < m_LinkedEntities.GetCount(); i++)
	{
		if(m_LinkedEntities[i]->GetIsDoor())
		{
			usingDoor = true;
			if(m_LinkedEntities[i]->GetEntity())
			{
				doorBlockage = Min(doorBlockage, m_LinkedEntities[i]->GetDoorOcclusion());
			}
			else
			{
				doorBlockage = Min(doorBlockage, m_LinkedEntities[i]->GetMaxOcclusion());
			}
		}
	}
	if(m_PortalSettings)
	{
		portalSettingsBlockage = m_PortalSettings->MaxOcclusion;
	}

	// Grab the min, so that we can have both a door and custom portal settings for a portal
	// Needed for situations where doors are removed by script, and we want to override the value instead of using the cached door max occlusion value.
	if(usingDoor || m_PortalSettings)
	{
		portalBlockage = Min(doorBlockage, portalSettingsBlockage);
	}
	// Only use the non-marked if we specify that we want to use it
	else if(useInteriorSettingsMetrics && interiorSettings && intRoom)
	{
		portalBlockage = intRoom->NonMarkedPortalOcclusion;
	}

	// Modify this by the amount of broken glass
	portalBlockage *= m_GlassBlockageModifier;

	naAssertf(FPIsFinite(portalBlockage), "portalBlockage !FPIsFinite");
	naAssertf(portalBlockage >= 0.0f, "portalBlockage <= 0.0f");

	return portalBlockage;
}

const CInteriorInst* naOcclusionPortalInfo::GetInteriorInst() const
{ 
	if(m_IntProxy && m_IntProxy->GetInteriorInst() && m_IntProxy->GetInteriorInst()->IsPopulated())
	{
		return m_IntProxy->GetInteriorInst();
	}
	return NULL;
}

const CInteriorProxy* naOcclusionPortalInfo::GetInteriorProxy() const
{ 
	if(m_IntProxy)
	{
		return m_IntProxy;
	}
	return NULL;
}

u32 naOcclusionPortalInfo::GetUniqueInteriorProxyHashkey() const
{
	return m_InteriorProxyHash;
}

s32 naOcclusionPortalInfo::GetRoomIdx() const
{ 
	return m_RoomIdx;
}

s32 naOcclusionPortalInfo::GetPortalIdx() const
{ 
	return m_PortalIdx;
}

u32 naOcclusionPortalInfo::GetDestInteriorHash() const
{ 
	return m_DestInteriorHash;
}

s32 naOcclusionPortalInfo::GetDestRoomIdx() const
{ 
	return m_DestRoomIdx;
}

void naOcclusionPortalInfo::SetPortalSettingsScriptOverride(const char* oldPortalSettingsName, const char* newPortalSettingsName)
{
	naAssertf(audNorthAudioEngine::GetObject<PortalSettings>(oldPortalSettingsName), "SetPortalSettingsScriptOverride - Don't have old PortalSettings GO with name %s", oldPortalSettingsName);
	naAssertf(audNorthAudioEngine::GetObject<PortalSettings>(newPortalSettingsName), "SetPortalSettingsScriptOverride - Don't have new PortalSettings GO with name %s", newPortalSettingsName);

	const u32 oldHash = atStringHash(oldPortalSettingsName);
	const u32* currentOverride = sm_PortalSettingsScriptOverrideMap.Access(oldHash);
	if(currentOverride)
	{
		sm_PortalSettingsScriptOverrideMap.Delete(oldHash);
	}
	const u32 newHash = atStringHash(newPortalSettingsName);
	sm_PortalSettingsScriptOverrideMap.Insert(oldHash, newHash);
}

void naOcclusionPortalInfo::SetPortalSettingsScriptOverride(u32 interiorNameHash, s32 roomIndex, s32 portalIndex, const char* newPortalSettingsName)
{
	naAssertf(audNorthAudioEngine::GetObject<PortalSettings>(newPortalSettingsName), "SetPortalSettingsScriptOverride - Don't have new PortalSettings GO with name %s", newPortalSettingsName);

	naOcclusionPerPortalSettingsOverrideList* interiorPortalOverrides = sm_PerPortalSettingsScriptOverrideMap.Access(interiorNameHash);
	
	if (!interiorPortalOverrides)
	{
		interiorPortalOverrides = &sm_PerPortalSettingsScriptOverrideMap.Insert(interiorNameHash).data;
	}

	if (interiorPortalOverrides)
	{
		for (naOcclusionPerPortalSettingsOverride& portalOverride : *interiorPortalOverrides)
		{
			if (portalOverride.roomIndex == roomIndex && portalOverride.portalIndex == portalIndex)
			{
				portalOverride.overridenSettingsHash = atStringHash(newPortalSettingsName);
				return;
			}
		}

		naOcclusionPerPortalSettingsOverride newOverride;
		newOverride.roomIndex = roomIndex;
		newOverride.portalIndex = portalIndex;
		newOverride.overridenSettingsHash = atStringHash(newPortalSettingsName);
		interiorPortalOverrides->Push(newOverride);
	}
}

void naOcclusionPortalInfo::RemovePortalSettingsScriptOverride(const char* portalSettingsName)
{
	naAssertf(audNorthAudioEngine::GetObject<PortalSettings>(portalSettingsName), "RemovePortalSettingsScriptOverride - Don't have old PortalSettings GO with name %s", portalSettingsName);

	const u32 hash = atStringHash(portalSettingsName);
	sm_PortalSettingsScriptOverrideMap.Delete(hash);
}

void naOcclusionPortalInfo::RemovePortalSettingsScriptOverride(u32 interiorNameHash, s32 roomIndex, s32 portalIndex)
{
	if (naOcclusionPerPortalSettingsOverrideList* interiorPortalOverrides = sm_PerPortalSettingsScriptOverrideMap.Access(interiorNameHash))
	{
		for (u32 i = 0; i < interiorPortalOverrides->GetCount(); i++)
		{
			if ((*interiorPortalOverrides)[i].roomIndex == roomIndex && (*interiorPortalOverrides)[i].portalIndex == portalIndex)
			{
				interiorPortalOverrides->Delete(i);
				return;
			}
		}
	}
}

#if GTA_REPLAY
void naOcclusionPortalInfo::SetReplayPortalSettingsOverride(u32 oldHash, u32 newHash)
{
	naAssertf(audNorthAudioEngine::GetObject<PortalSettings>(oldHash), "SetReplayPortalSettingsOverride - Don't have old PortalSettings GO with hash %u", oldHash);
	naAssertf(audNorthAudioEngine::GetObject<PortalSettings>(newHash), "SetReplayPortalSettingsOverride - Don't have new PortalSettings GO with hash %u", newHash);
	const u32* currentOverride = sm_PortalSettingsReplayOverrideMap.Access(oldHash);
	if(currentOverride)
	{
		sm_PortalSettingsReplayOverrideMap.Delete(oldHash);
	}	
	sm_PortalSettingsReplayOverrideMap.Insert(oldHash, newHash);
}

void naOcclusionPortalInfo::SetReplayPerPortalSettingsOverride(u32 interiorNameHash, s32 roomIndex, s32 portalIndex, u32 portalSettingsHash)
{
	naAssertf(audNorthAudioEngine::GetObject<PortalSettings>(portalSettingsHash), "SetReplayPerPortalSettingsOverride - Don't have PortalSettings GO with hash %u", portalSettingsHash);

	naOcclusionPerPortalSettingsOverrideList* interiorPortalOverrides = sm_PerPortalSettingsReplayOverrideMap.Access(interiorNameHash);

	if (!interiorPortalOverrides)
	{
		interiorPortalOverrides = &sm_PerPortalSettingsReplayOverrideMap.Insert(interiorNameHash).data;
	}

	if (interiorPortalOverrides)
	{
		for (naOcclusionPerPortalSettingsOverride& portalOverride : *interiorPortalOverrides)
		{
			if (portalOverride.roomIndex == roomIndex && portalOverride.portalIndex == portalIndex)
			{
				portalOverride.overridenSettingsHash = portalSettingsHash;
				return;
			}
		}

		naOcclusionPerPortalSettingsOverride newOverride;
		newOverride.roomIndex = roomIndex;
		newOverride.portalIndex = portalIndex;
		newOverride.overridenSettingsHash = portalSettingsHash;
		interiorPortalOverrides->Push(newOverride);
	}
}

void naOcclusionPortalInfo::ClearReplayPortalSettingsOverrides()
{
	sm_PortalSettingsReplayOverrideMap.Reset();
	sm_PerPortalSettingsReplayOverrideMap.Reset();
}
#endif
	
#if __BANK

void naOcclusionPortalInfo::DrawDebug()
{
	// Draw portals that are in the same room as the listener, or outside portals if the listener is outside (!playerIntInst and a room index of 0 means it's outside)
	const CInteriorInst* portalIntInst = GetInteriorInst();
	if(portalIntInst && portalIntInst->IsPopulated() && GetRoomIdx() < (s32)portalIntInst->GetNumRooms() && GetPortalIdx() < portalIntInst->GetNumPortalsInRoom(GetRoomIdx()))
	{
		CInteriorInst* playerIntInst = audNorthAudioEngine::GetGtaEnvironment()->GetListenerInteriorInstance();
		if(portalIntInst && m_PortalCorners &&
			((portalIntInst == playerIntInst && GetRoomIdx() != INTLOC_ROOMINDEX_LIMBO) || (!playerIntInst && GetRoomIdx() == INTLOC_ROOMINDEX_LIMBO)))
		{
			char buffer[255];
			s32 line = 0;
			bool usingDoor = false;
			bool useInteriorSettingsMetrics = false;
			const InteriorSettings* interiorSettings = NULL;
			const InteriorRoom* interiorRoom = NULL;

			CInteriorInst* destIntInst;
			s32 destRoomIdx;
			audNorthAudioEngine::GetOcclusionManager()->GetPortalDestinationInfo(portalIntInst, GetRoomIdx(), GetPortalIdx(), &destIntInst, &destRoomIdx);

			if(destIntInst && GetRoomIdx() == INTLOC_ROOMINDEX_LIMBO)
			{
				audNorthAudioEngine::GetGtaEnvironment()->GetInteriorSettingsForInterior(destIntInst, destRoomIdx, interiorSettings, interiorRoom);
				useInteriorSettingsMetrics = true;
			}
			else if(destRoomIdx == INTLOC_ROOMINDEX_LIMBO && !portalIntInst->IsLinkPortal(GetRoomIdx(), GetPortalIdx()))
			{
				audNorthAudioEngine::GetGtaEnvironment()->GetInteriorSettingsForInterior(portalIntInst, GetRoomIdx(), interiorSettings, interiorRoom);
				useInteriorSettingsMetrics = true;
			}

			Vector3 portalCenter = m_PortalCorners->GetCorner(0) + m_PortalCorners->GetCorner(1) + m_PortalCorners->GetCorner(2) + m_PortalCorners->GetCorner(3);
			portalCenter.Scale(0.25f);

			const Vector3 listenerPos = audNorthAudioEngine::GetMicrophones().GetLocalEnvironmentScanPosition();

			if(g_DebugPortalInfoRange > 0.f && listenerPos.Dist2(portalCenter) > g_DebugPortalInfoRange * g_DebugPortalInfoRange)
			{
				return;
			}

			for(s32 i = 0; i < m_LinkedEntities.GetCount(); i++)
			{
				if(m_LinkedEntities[i]->GetIsDoor())
				{
					usingDoor = true;
				}

				if(m_LinkedEntities[i]->GetEntity())
				{
					formatf(buffer, sizeof(buffer), "ENTITY %d: %s", i, m_LinkedEntities[i]->GetEntity()->GetBaseModelInfo()->GetModelName());
					grcDebugDraw::Text(portalCenter, m_LinkedEntities[i]->m_DebugColor, 0, line, buffer);
					line += 10;

					switch(m_LinkedEntities[i]->GetLinkType())
					{
					case AUD_OCC_LINK_TYPE_PORTAL:
						formatf(buffer, sizeof(buffer), "LINKTYPE: AUD_OCC_LINK_TYPE_PORTAL");
						break;
					case AUD_OCC_LINK_TYPE_MATCHING_PORTAL:
						formatf(buffer, sizeof(buffer), "LINKTYPE: AUD_OCC_LINK_TYPE_MATCHING_PORTAL");
						break;
					case AUD_OCC_LINK_TYPE_EXTERIOR:
						formatf(buffer, sizeof(buffer), "LINKTYPE: AUD_OCC_LINK_TYPE_EXTERIOR");
						break;
					default:
						formatf(buffer, sizeof(buffer), "LINKTYPE: UNKNOWN");
						break;
					}

					grcDebugDraw::Text(portalCenter, m_LinkedEntities[i]->m_DebugColor, 0, line, buffer);
					line += 10;

					if(m_LinkedEntities[i]->GetIsDoor() && m_LinkedEntities[i]->GetEntity()->GetIsTypeObject())
					{
						atString doorSoundsName("DOOR WITH NO SETTINGS");

						const CObject* object = static_cast<const CObject*>(m_LinkedEntities[i]->GetEntity());
						if(object && object->IsADoor())
						{
							const CDoor* door = static_cast<const CDoor*>(object);
							if(door && door->GetCurrentPhysicsInst())
							{
								const audDoorAudioEntity* doorAudioEntity = door->GetDoorAudioEntity();
								if(doorAudioEntity)
								{
									const DoorAudioSettings* doorSettings = doorAudioEntity->GetDoorAudioSettings();
									if(doorSettings)
									{
										doorSoundsName = audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(doorSettings->NameTableOffset);
									}
								}
							}
						}

						formatf(buffer, sizeof(buffer), "DOORSOUNDS: %s BLOCKAGE: %0.2f / %0.2f", 
							doorSoundsName.c_str(), m_LinkedEntities[i]->GetDoorOcclusion(), m_LinkedEntities[i]->GetMaxOcclusion());
						grcDebugDraw::Text(portalCenter, m_LinkedEntities[i]->m_DebugColor, 0, line, buffer);
						line +=10;
					}

					if(m_LinkedEntities[i]->GetIsGlass())
					{
						formatf(buffer, sizeof(buffer), "GLASS BROKEN AREA: %0.2f", m_LinkedEntities[i]->GetBrokenGlassArea());
						grcDebugDraw::Text(portalCenter, m_LinkedEntities[i]->m_DebugColor, 0, line, buffer);
						line += 10;
					}

					fwPortalCorners entityCorners;
					portalIntInst->GetPortalInRoom(GetRoomIdx(), GetPortalIdx(), entityCorners);
					Color32 entityColor(m_LinkedEntities[i]->m_DebugColor);
					entityColor.SetAlpha(100);
					Vector3 entityCenter = entityCorners.GetCorner(1) + entityCorners.GetCorner(2);
					entityCenter.Scale(0.5f);
					formatf(buffer, sizeof(buffer), "ENTITY: %d", i);
					grcDebugDraw::Text(entityCenter, m_LinkedEntities[i]->m_DebugColor, buffer);
				}
				else
				{
					if(m_LinkedEntities[i]->GetIsDoor())
					{
						formatf(buffer, sizeof(buffer), "USED - DOOR: %d = STREAMED OUT. CACHED BLOCKAGE: %0.2f", i, m_LinkedEntities[i]->GetMaxOcclusion());
						grcDebugDraw::Text(portalCenter, m_LinkedEntities[i]->m_DebugColor, 0, line, buffer);
						line += 10;
					}
					else
					{
						formatf(buffer, sizeof(buffer), "UNUSED - WINDOW: %d STREAMED OUT", i);
						grcDebugDraw::Text(portalCenter, m_LinkedEntities[i]->m_DebugColor, 0, line, buffer);
						line += 10;
					}

				}
			}

			if(m_PortalSettings)
			{
				formatf(buffer, sizeof(buffer), "USED - PORTALSETTINGS: %s BLOCKAGE: %0.2f", 
					audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_PortalSettings->NameTableOffset),
					m_PortalSettings->MaxOcclusion);
				grcDebugDraw::Text(portalCenter, Color_green, 0, line, buffer);
				line += 10;
			}
			else
			{
				formatf(buffer, sizeof(buffer), "UNUSED - PORTALSETTINGS: NULL");
				grcDebugDraw::Text(portalCenter, Color_red, 0, line, buffer);
				line += 10;

				// If this is a portal that's leading inside, get the destination info and then grab the InteriorSettings* from that
				if(portalIntInst)
				{
					f32 nonMarkedBlockage = 0.0f;
					if(interiorSettings)
					{
						if(interiorRoom)
						{
							nonMarkedBlockage = interiorRoom->NonMarkedPortalOcclusion;
						}

						if(usingDoor)
						{
							formatf(buffer, sizeof(buffer), "UNUSED - NonMarkedPortalOcclusion: %0.2f", nonMarkedBlockage);
						}
						else
						{
							formatf(buffer, sizeof(buffer), "USED - NonMarkedPortalOcclusion: %0.2f", nonMarkedBlockage);
						}

						grcDebugDraw::Text(portalCenter, usingDoor ? Color_red : Color_green, 0, line, buffer);
						line += 10;
					}
				}
			}

			formatf(buffer, sizeof(buffer), "Int: %u, Room: %d, Portal: %d", GetUniqueInteriorProxyHashkey(), GetRoomIdx(), GetPortalIdx());
			grcDebugDraw::Text(portalCenter, Color_white, 0, line, buffer);
			line += 10;

			f32 exteriorBlockage = 0.0f;
			if(destRoomIdx == 0 && !portalIntInst->IsLinkPortal(GetRoomIdx(), GetPortalIdx()) && interiorSettings)
			{
				const Vector3 listenerPos = audNorthAudioEngine::GetMicrophones().GetLocalEnvironmentScanPosition();
				exteriorBlockage = audNorthAudioEngine::GetOcclusionManager()->CalculateExteriorBlockageForPortal(listenerPos, interiorSettings, interiorRoom, m_PortalCorners);
				formatf(buffer, sizeof(buffer), "EXTERIOR DISTANCE BLOCKAGE: %0.2f", exteriorBlockage);
				grcDebugDraw::Text(portalCenter, Color_green, 0, line, buffer);
				line += 10;
			}

			f32 finalBlockage = GetEntityBlockage(interiorSettings, interiorRoom, useInteriorSettingsMetrics);
			finalBlockage = Lerp(finalBlockage, exteriorBlockage, 1.0f);

			formatf(buffer, sizeof(buffer), "FINAL BLOCKAGE: %0.2f", finalBlockage);
			grcDebugDraw::Text(portalCenter, Color_white, 0, line, buffer);

			grcDebugDraw::Line(m_PortalCorners->GetCorner(0), m_PortalCorners->GetCorner(1), Color_orange);
			grcDebugDraw::Line(m_PortalCorners->GetCorner(1), m_PortalCorners->GetCorner(2), Color_orange);
			grcDebugDraw::Line(m_PortalCorners->GetCorner(2), m_PortalCorners->GetCorner(3), Color_orange);
			grcDebugDraw::Line(m_PortalCorners->GetCorner(3), m_PortalCorners->GetCorner(0), Color_orange);

			grcDebugDraw::Line(m_PortalCorners->GetCorner(0), m_PortalCorners->GetCorner(2), Color_orange);
			grcDebugDraw::Line(m_PortalCorners->GetCorner(1), m_PortalCorners->GetCorner(3), Color_orange);
		}
	}
}

#endif

#if __DEV
const char* naOcclusionPortalInfo::GetInteriorName()
{
	const CInteriorProxy* intProxy = GetInteriorProxy();
	if(intProxy)
	{
		return intProxy->GetModelName();
	}

	return NULL;
}

const char* naOcclusionPortalInfo::GetRoomName()
{
	const CInteriorInst* intInst = GetInteriorInst();
	if(intInst)
	{
		return intInst->GetRoomName(GetRoomIdx());
	}

	return NULL;
}

const char* naOcclusionPortalInfo::GetDestinationInteriorName()
{
	const CInteriorInst* nodeIntInst = GetInteriorInst();
	if(nodeIntInst)
	{
		CInteriorInst* destIntInst;
		s32 destRoomIdx;
		audNorthAudioEngine::GetOcclusionManager()->GetPortalDestinationInfo(nodeIntInst, GetRoomIdx(), GetPortalIdx(), &destIntInst, &destRoomIdx);
		if(naVerifyf(destIntInst && destRoomIdx != INTLOC_INVALID_INDEX, "Got bad portal destination info"))
		{
			return destIntInst->GetProxy()->GetModelName();
		}
	}

	return NULL;
}

const char* naOcclusionPortalInfo::GetDestinationRoomName()
{
	const CInteriorInst* nodeIntInst = GetInteriorInst();
	if(nodeIntInst)
	{
		CInteriorInst* destIntInst;
		s32 destRoomIdx;
		audNorthAudioEngine::GetOcclusionManager()->GetPortalDestinationInfo(nodeIntInst, GetRoomIdx(), GetPortalIdx(), &destIntInst, &destRoomIdx);
		if(naVerifyf(destIntInst && destRoomIdx != INTLOC_INVALID_INDEX, "Got bad portal destination info"))
		{
			return destIntInst->GetRoomName(destRoomIdx);
		}
	}

	return NULL;
}
#endif // __DEV

#if __BANK

void naOcclusionPortalInfo::SetInteriorProxyHash(const u32 intProxyHash)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	naAssertf(audNorthAudioEngine::GetOcclusionManager()->GetIsBuildingOcclusionPaths(), "Shouldn't be trying to set metadata values if we're not building occlusion paths");
	m_InteriorProxyHash = intProxyHash;
}

void naOcclusionPortalInfo::SetRoomIdx(const s32 roomIdx)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	naAssertf(audNorthAudioEngine::GetOcclusionManager()->GetIsBuildingOcclusionPaths(), "Shouldn't be trying to set metadata values if we're not building occlusion paths");
	m_RoomIdx = roomIdx;
}

void naOcclusionPortalInfo::SetPortalIdx(const u32 portalIdx)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	naAssertf(audNorthAudioEngine::GetOcclusionManager()->GetIsBuildingOcclusionPaths(), "Shouldn't be trying to set metadata values if we're not building occlusion paths");
	m_PortalIdx = portalIdx;
}

void naOcclusionPortalInfo::SetDestInteriorHash(const u32 destInteriorHash)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	naAssertf(audNorthAudioEngine::GetOcclusionManager()->GetIsBuildingOcclusionPaths(), "Shouldn't be trying to set metadata values if we're not building occlusion paths");
	const u32 swappedDestIntHash = audNorthAudioEngine::GetOcclusionManager()->GetSwappedHashForInterior(destInteriorHash);
	m_DestInteriorHash = swappedDestIntHash;
}

void naOcclusionPortalInfo::SetDestInteriorRoomIdx(const s32 destRoomIdx)
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	naAssertf(audNorthAudioEngine::GetOcclusionManager()->GetIsBuildingOcclusionPaths(), "Shouldn't be trying to set metadata values if we're not building occlusion paths");
	m_DestRoomIdx = destRoomIdx;
}

void naOcclusionPortalInfo::BuildPortalEntityArray()
{
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	const CInteriorInst* intInst = GetInteriorInst();
	if(intInst)
	{
		// Check to see if there are any doors or glass linked to the portal
		GetPortalLinkedEntities(intInst);

		// Get any doors that are on the same portal but on the matching MILO
		GetMatchingPortalLinkedEntities(intInst);

		// And finally get the doors that are in the exterior scene
		GetNonLinkedDoorEntities();
	}
}
#endif // __BANK

