//
// audio/environment/occlusion/occlusionportalentity.h
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef NAOCCLUSIONPORTALENTITY_H
#define NAOCCLUSIONPORTALENTITY_H

// Game
#include "audio/environment/occlusion/occlusionpathnode.h"
#include "audio/environment/occlusion/occlusioninteriormetadata.h"
#include "audio/debugaudio.h"
#include "scene/RegdRefTypes.h"

// Rage
#include "spatialdata/aabb.h"
#include "vector/color32.h"

enum audOcclusionPortalEntityLinkType
{
	AUD_OCC_LINK_TYPE_NONE,
	AUD_OCC_LINK_TYPE_PORTAL,			// The entity is linked to the portal
	AUD_OCC_LINK_TYPE_MATCHING_PORTAL,	// The entity is linked to the matching portal in the adjacent MILO
	AUD_OCC_LINK_TYPE_EXTERIOR			// The entity is in an exterior scene (ipl) and needs a probe to be found
};

namespace rage
{
	struct fragHierarchyInst;
	class fragTypeChild;
	class bgPaneModelInfoBase;
}

// Contains all the information about an entity that provides some sort of occlusion metrics, so a door or a window
// The entity also provides all the information needed for how to find itself, based on the audOcclusionEntityLinkType
class naOcclusionPortalEntity
{

public:

	FW_REGISTER_CLASS_POOL(naOcclusionPortalEntity);

	naOcclusionPortalEntity();
	~naOcclusionPortalEntity();

	static void InitClass();

	static naOcclusionPortalEntity* Allocate();

	void InitFromMetadata(naOcclusionPortalEntityMetadata* portalEntityMetadata);

	void Update();

	const CEntity* GetEntity() const { return m_Entity; }
	u32 GetEntityModelHashkey() const;
	audOcclusionPortalEntityLinkType GetLinkType() const;
	f32 GetMaxOcclusion() const;
	f32 GetDoorOcclusion() const { return m_DoorOcclusion; }
	f32 GetBrokenGlassArea() const { return m_BrokenGlassArea; }
	bool GetIsDoor() const;
	bool GetIsGlass() const;

	void SetEntity(const CEntity* entity) { m_Entity = entity; }

	// These should only be used during the offline build process with the exception of set max occlusion which makes it easier for sound designers
#if __BANK
	void SetMaxOcclusion(const f32 maxOcclusion);
	void SetEntityModelHashkey(const u32 modelHashkey);
	void SetLinkType(audOcclusionPortalEntityLinkType linkType);
	void SetIsDoor(const bool isDoor); 
	void SetIsGlass(const bool isGlass);
#endif


private:

	void UpdateGlass();
	f32 GetAreaOfBounds(spdAABB& pBounds);

	void UpdateDoor();

public:

#if __BANK
	Color32 m_DebugColor;
	static const s32 sm_OcclusionBuildPortalEntityPoolSize = 500;
#endif

private:

	RegdConstEnt m_Entity;
	
	audOcclusionPortalEntityLinkType m_LinkType;
	f32 m_MaxOcclusion;
	u32 m_EntityModelHashkey;
	f32 m_DoorOcclusion;
	f32 m_BrokenGlassArea;
	bool m_IsDoor;
	bool m_IsGlass;

	static audCurve sm_DoorAngleToPortalOcclusionCurve;
};

#endif // NAOCCLUSIONPORTALENTITY_H
