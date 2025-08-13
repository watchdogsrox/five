//
// filename:	MloModelInfo.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "string/stringutil.h"

#include "fwscene/mapdata/mapdata.h"
#include "fwscene/stores/maptypesstore.h"

// Game headers
#include "audio/northaudioengine.h"
#include "modelinfo/mlomodelinfo.h"
#include "scene/loader/mapdata_entities.h"
#include "scene/loader/mapdata_interiors.h"
#include "scene/portals/interiorinst.h"
#include "scene/world/gameworld.h"
#include "streaming/streaming.h"

#include "diag/art_channel.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------


CMloModelInfo::CMloModelInfo() 
{
	m_type = MI_TYPE_MLO;
}

CMloModelInfo::~CMloModelInfo()
{
	Assertf(!audNorthAudioEngine::IsAudioUpdateCurrentlyRunning(), "Deleting MloModelInfo instance while audio is updating");
}

//
// name:		CMloModelInfo::Init
// description:	Initialise Mlo
void CMloModelInfo::Init()
{
	CBaseModelInfo::Init(); 
	m_MLOInstanceBlockOffset = 0; 
	m_numMLOInstances = 0;
	m_mapFileDefIndex = fwModelId::TF_INVALID;
	m_mloFlags = 0;
}

void CMloModelInfo::InitArchetypeFromDefinition(strLocalIndex mapTypeDefIndex, fwArchetypeDef* definition, bool UNUSED_PARAM(bHasAssetReferences))
{
	SetIsStreamType(false);

	// new style archetype loading
	fwMapTypesDef* pDef = g_MapTypesStore.GetSlot(mapTypeDefIndex);
	if (pDef && !pDef->GetIsPermanent())
	{
		SetIsStreamType(true);

		XPARAM(NoStaticIntCols);
		if (!PARAM_NoStaticIntCols.Get() )
		{
			definition->m_physicsDictionary = atHashValue(0u);		// for static interior collisions : neutralize phys dict loading
		}

		// old style archetype loading
		CBaseModelInfo::InitArchetypeFromDefinition(mapTypeDefIndex, definition, false);		// force no asset references
		CMloArchetypeDef& def = *smart_cast<CMloArchetypeDef*>(definition);

		u32 numPortals = def.m_portals.GetCount();
		u32 numRooms = def.m_rooms.GetCount();

		if (numPortals ==0){
			numRooms = 1;			// if exterior MLO, then will only contain 1 room to stores refs to instanced objects
		}

		m_mloFlags = def.m_mloFlags;

		SetHasLoaded(true);

		SetMapTypeDefIndex(mapTypeDefIndex.Get());
		SetNumMLOInstances(0);

		m_pEntities = &def.m_entities;
		m_pRooms = &def.m_rooms;
		m_pPortals = &def.m_portals;
		m_pEntitySets = &def.m_entitySets;
		m_pTimeCycleModifiers = &def.m_timeCycleModifiers;

		Assertf(def.m_rooms[0].m_attachedObjects.GetCount() < INTLOC_MAX_ENTITIES_IN_ROOM_0, "Interior %s has more than %d entities in room 0 (limbo)",GetModelName(), INTLOC_MAX_ENTITIES_IN_ROOM_0);

		m_cachedAudioSettings.Reset();
		m_cachedAudioSettings.Reserve(m_pRooms->GetCount());
		m_cachedAudioSettings.Resize(m_pRooms->GetCount());

		spdAABB bbox;
		bbox.Invalidate();

		float furthestPortalSqrd = 0.0f;
		for (u32 i = 0; i < numPortals; ++i)
		{
			CMloPortalDef& portal = (*m_pPortals)[i];
			if ((portal.m_roomFrom == 0) || (portal.m_roomTo == 0))
			{
				for (u32 j=0; j<4; j++)
				{
					float dist = portal.m_corners[j].Mag2();
					bbox.GrowPoint(VECTOR3_TO_VEC3V(portal.m_corners[j]));
					furthestPortalSqrd = Max(dist, furthestPortalSqrd);
				}
			}
		}

		m_boundingPortalRadius = Sqrtf(furthestPortalSqrd);

		for (u32 i=0; i<m_pEntities->GetCount(); i++)
		{
			const fwEntityDef& entity = *(*m_pEntities)[i];
			bbox.GrowPoint(VECTOR3_TO_VEC3V(entity.m_position));
		}

		SetBoundingBox(bbox);

		SetLodDistance(def.m_lodDist);

		// data massaging required:
		for (u32 i = 0; i < numPortals; ++i)
		{
			CMloPortalDef& portal = (*m_pPortals)[i];

#if __ASSERT
			const u32 validPortalFlags =
				INTINFO_PORTAL_ONE_WAY                          |
				INTINFO_PORTAL_LINK                             |
				INTINFO_PORTAL_MIRROR                           |
				INTINFO_PORTAL_IGNORE_MODIFIER                  |
				INTINFO_PORTAL_MIRROR_USING_EXPENSIVE_SHADERS   | // ignored
				INTINFO_PORTAL_LOWLODONLY                       |
				INTINFO_PORTAL_ALLOWCLOSING                     |
				INTINFO_PORTAL_MIRROR_CAN_SEE_DIRECTIONAL_LIGHT |
				INTINFO_PORTAL_MIRROR_CAN_SEE_EXTERIOR_VIEW		|
				INTINFO_PORTAL_MIRROR_USING_PORTAL_TRAVERSAL    |
				INTINFO_PORTAL_MIRROR_FLOOR                     |
				INTINFO_PORTAL_WATER_SURFACE                    |
				INTINFO_PORTAL_USE_LIGHT_BLEED					|
				0;
			//	INTINFO_PORTAL_WATER_SURFACE_EXTEND_TO_HORIZON  ;

			const u32 mirrorPortalFlags =
				INTINFO_PORTAL_MIRROR                           |
				INTINFO_PORTAL_MIRROR_USING_EXPENSIVE_SHADERS   | // ignored
				INTINFO_PORTAL_MIRROR_CAN_SEE_DIRECTIONAL_LIGHT |
				INTINFO_PORTAL_MIRROR_CAN_SEE_EXTERIOR_VIEW		|
				INTINFO_PORTAL_MIRROR_USING_PORTAL_TRAVERSAL    |
				INTINFO_PORTAL_MIRROR_FLOOR                     |
				INTINFO_PORTAL_WATER_SURFACE                    |
				0;
			//	INTINFO_PORTAL_WATER_SURFACE_EXTEND_TO_HORIZON  ;

			const u32 nonMirrorPortalFlags =
			//	INTINFO_PORTAL_ONE_WAY         | // all mirrors are technically "one-way", we can ignore this flag (it gets forced on in the code below)
			//	INTINFO_PORTAL_ALLOWCLOSING    | // we have a lot of mirrors with "allow closing" flag set for some reason .. we should be able to clean this up eventually
				INTINFO_PORTAL_LINK            |
				INTINFO_PORTAL_IGNORE_MODIFIER |
				INTINFO_PORTAL_LOWLODONLY      ;

			Assertf((portal.m_flags & ~validPortalFlags) == 0, "CMloModelInfo::InitArchetypeFromDefinition: invalid portal flags! %s (0x%08x)", GetModelName(), portal.m_flags);

			if ((portal.m_flags & INTINFO_PORTAL_MIRROR_FLOOR) != 0 &&
				(portal.m_flags & INTINFO_PORTAL_MIRROR) == 0)
			{
				Assertf(0, "CMloModelInfo::InitArchetypeFromDefinition: MIRROR_FLOOR requires MIRROR flag! %s (0x%08x)", GetModelName(), portal.m_flags);
			}

			if ((portal.m_flags & INTINFO_PORTAL_WATER_SURFACE) != 0 &&
				(portal.m_flags & INTINFO_PORTAL_MIRROR_FLOOR) == 0)
			{
				Assertf(0, "CMloModelInfo::InitArchetypeFromDefinition: WATER_SURFACE requires MIRROR_FLOOR flag! %s (0x%08x)", GetModelName(), portal.m_flags);
			}

			if ((portal.m_flags & INTINFO_PORTAL_MIRROR) != 0 &&
				(portal.m_flags & nonMirrorPortalFlags) != 0)
			{
				Assertf(0, "CMloModelInfo::InitArchetypeFromDefinition: invalid portal flags for mirror! %s (0x%08x)", GetModelName(), portal.m_flags);
			}

			if ((portal.m_flags & INTINFO_PORTAL_MIRROR) == 0 && // not a mirror
				(portal.m_flags & mirrorPortalFlags) != 0)
			{
				Assertf(0, "CMloModelInfo::InitArchetypeFromDefinition: invalid portal flags for non-mirror! %s (0x%08x)", GetModelName(), portal.m_flags);
			}
#endif // __ASSERT

			if ((portal.m_flags & INTINFO_PORTAL_MIRROR) != 0)
			{
				// force any horizontal mirror to be a mirror floor (hack for BS#1527956, mirror floor in v_franklins)
				if (MaxElement(Abs(Cross(VECTOR3_TO_VEC3V(portal.m_corners[2] - portal.m_corners[0]), VECTOR3_TO_VEC3V(portal.m_corners[1] - portal.m_corners[0]))).GetXY()).Getf() < 0.1f)
				{
					portal.m_flags |= INTINFO_PORTAL_MIRROR_FLOOR;
				}

				portal.m_flags |= INTINFO_PORTAL_ONE_WAY;
			}
		}

		return;
	}

	Assertf(false,"%s can't create MLO archetypes from non streamed .ityp files", g_MapTypesStore.GetName(strLocalIndex(mapTypeDefIndex)));
	return;
}

fwEntity* CMloModelInfo::CreateEntity()
{
	CInteriorInst* mloEntity = rage_new CInteriorInst( ENTITY_OWNEDBY_IPL );
	Assert(mloEntity);
	Assert(mloEntity->GetCurrentPhysicsInst()==NULL);

	return mloEntity;
}

//
// name:		CMloModelInfo::Shutdown
// description:	Shutdown Mlo, currently just calls basemodelinfo shutdown
void CMloModelInfo::Shutdown()
{
	m_cachedAudioSettings.Reset();

	CBaseModelInfo::Shutdown(); 
}


// request that all the objects in the given room are loaded
void CMloModelInfo::RequestObjectsInRoom(const char* /*pRoomName*/, u32 /*flags*/){

	modelinfoAssertf(false, "RequestObjectsInRoom() is a dead command");

// 	if (m_pInterior){
// 		s32 key = atStringHash(pRoomName);
// 		s32 roomIdx = m_pInterior->FindRoomByHashcode(key);
// 
// 		CRoomInfo_DEPRECATED& requestedRoom = m_pInterior->GetRoom(roomIdx);
// 		for(s32 i=0; i<requestedRoom.GetNumObjects(); i++){
// 			s32 objIdx = requestedRoom.GetObjectIdx(i);
// 			modelinfoAssert(objIdx < GetNumInstances());
// 
// 			const CMloInstance& mloInst = GetInstance(objIdx);
// 			u32 modelIdx = mloInst.m_modelIndex;
// 			fwModelId modelId(modelIdx);
// 			CModelInfo::RequestAssets(modelId, flags);
// 		}
// 	}

}

// check status of all models in given room and determine if they have all loaded
bool CMloModelInfo::HaveObjectsInRoomLoaded(const char* /*pRoomName*/){

	modelinfoAssertf(false, "HaveObjectsInRoomLoaded() is a dead command");

// 	modelinfoAssert(m_pInterior);
// 
// 	// check the objects for the desired room
// 	if (m_pInterior){
// 		s32 key = atStringHash(pRoomName);
// 		s32 roomIdx = m_pInterior->FindRoomByHashcode(key);
// 
// 		CRoomInfo_DEPRECATED& requestedRoom = m_pInterior->GetRoom(roomIdx);
// 		for(s32 i=0; i<requestedRoom.GetNumObjects(); i++){
// 			s32 objIdx = requestedRoom.GetObjectIdx(i);
// 			modelinfoAssert(objIdx < GetNumInstances());
// 
// 			const CMloInstance& mloInst = GetInstance(objIdx);
// 
// 			u32 modelIdx = mloInst.m_modelIndex;
// 			fwModelId modelId(modelIdx);
// 			if (!CModelInfo::HaveAssetsLoaded(modelId)){
// 				return(false);
// 			}
// 		}
// 	}

	return(true);
}


//
// name:		CMloModelInfo::SetMissionDoesntRequireRoom
// description:	Clear mission required flag on objects in room
void CMloModelInfo::SetMissionDoesntRequireRoom(const char* /*pRoomName*/)
{
	Assert(false);			// needed?

// 	modelinfoAssert(m_pInterior);
// 
// 	if (m_pInterior){
// 		s32 key = atStringHash(pRoomName);
// 		s32 roomIdx = m_pInterior->FindRoomByHashcode(key);
// 
// 		CRoomInfo_DEPRECATED& requestedRoom = m_pInterior->GetRoom(roomIdx);
// 		for(s32 i=0; i<requestedRoom.GetNumObjects(); i++){
// 			s32 objIdx = requestedRoom.GetObjectIdx(i);
// 			modelinfoAssert(objIdx < GetNumInstances());
// 
// 			const CMloInstance& mloInst = GetInstance(objIdx);
// 
// 			s32 modelIdx = mloInst.m_modelIndex;
// 			fwModelId modelId(modelIdx);
// 			CStreaming::SetMissionDoesntRequireObject(modelIdx, CModelInfo::GetStreamingModuleId());
// 		}
// 	}
}

void CMloModelInfo::GetAudioSettings(u32 roomIdx, const InteriorSettings*& audioSettings, const InteriorRoom*& audioRoom) const
{
	modelinfoAssert(GetIsStreamType());

	if(modelinfoVerifyf(m_pRooms, "Requesting audio settings but atArray< CMloRoomDef>* is NULL") 
		&& modelinfoVerifyf(roomIdx < m_pRooms->GetCount(), "Requesting invalid roomIdx: %u in interior: %s", roomIdx, GetModelName()))
	{
		audioSettings = m_cachedAudioSettings[roomIdx].m_audioSettings;
		audioRoom = m_cachedAudioSettings[roomIdx].m_audioRoom;
	}
}

void CMloModelInfo::SetAudioSettings(u32 roomIdx, const InteriorSettings* audioSettings, const InteriorRoom* audioRoom)
{
	modelinfoAssert(GetIsStreamType());

	if(modelinfoVerifyf(m_pRooms, "Requesting audio settings but atArray< CMloRoomDef>* is NULL") &&
		modelinfoVerifyf(roomIdx < m_pRooms->GetCount(), "Setting invalid roomIdx: %u in interior: %s", roomIdx, GetModelName()))
	{
		m_cachedAudioSettings[roomIdx].m_audioSettings = audioSettings;
		m_cachedAudioSettings[roomIdx].m_audioRoom = audioRoom;
	}
}

u32	CMloModelInfo::GetRoomFlags(u32 roomIdx) const
{ 
	FastAssert(m_pRooms); 
	return(((*m_pRooms)[roomIdx]).m_flags); 
}

CPortalFlags	CMloModelInfo::GetPortalFlags(u32 portalIdx) const
{ 
	FastAssert(m_pRooms); 
	return(CPortalFlags(((*m_pPortals)[portalIdx]).m_flags)); 
}

void	CMloModelInfo::GetRoomData(u32 roomIdx, atHashWithStringNotFinal& timeCycleName, atHashWithStringNotFinal& secondaryTimeCycleName, float& blend) const
{ 
	modelinfoAssert(m_pRooms); 
	modelinfoAssert(roomIdx < m_pRooms->GetCount());

	const CMloRoomDef& room = ((*m_pRooms)[roomIdx]);
	timeCycleName = room.m_timecycleName;
	secondaryTimeCycleName = room.m_secondaryTimecycleName;
	blend = room.m_blend;
}

void	CMloModelInfo::GetRoomTimeCycleNames(u32 roomIdx, atHashWithStringNotFinal& timeCycleName, atHashWithStringNotFinal& secondaryTimeCycleName) const
{
	modelinfoAssert(m_pRooms); 
	modelinfoAssert(roomIdx < m_pRooms->GetCount());

	timeCycleName = ((*m_pRooms)[roomIdx]).m_timecycleName;
	secondaryTimeCycleName = ((*m_pRooms)[roomIdx]).m_secondaryTimecycleName;
}


void	CMloModelInfo::GetPortalData(u32 portalIdx, u32& roomIdx1, u32& roomIdx2, CPortalFlags& flags) const
{ 
	modelinfoAssert(m_pPortals); 
	modelinfoAssert(portalIdx < m_pPortals->GetCount());

	const CMloPortalDef& portal = ((*m_pPortals)[portalIdx]);
	flags = CPortalFlags(portal.m_flags); 
	roomIdx1 = portal.m_roomFrom;
	roomIdx2 = portal.m_roomTo;
}

#if __BANK
const char* CMloModelInfo::GetRoomName(fwInteriorLocation location) const
{
	if (location.IsValid())
	{
		return m_pRooms->operator[](location.GetRoomIndex()).m_name.c_str();
	}

	return "";
}
#endif // __BANK

s32	CMloModelInfo::GetInteriorPortalIdx(u32 roomIdx, u32 roomPortalIdx) const {

	modelinfoAssert(m_pPortals);
	modelinfoAssert(GetIsStreamType());

	{
		s32 interiorPortalIdx = -1;

		for(u32 i=0; i<m_pPortals->GetCount(); i++)
		{
			const CMloPortalDef& portal = (*m_pPortals)[i];
			if (portal.m_roomFrom == roomIdx || portal.m_roomTo == roomIdx)
			{
				interiorPortalIdx++;
				if (roomPortalIdx == 0)
				{
					return(interiorPortalIdx);
				} else
				{
					roomPortalIdx--;
				}
			}
		}

		return(-1);
	}
}




