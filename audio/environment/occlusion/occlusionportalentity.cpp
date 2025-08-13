// 
// audio/environment/occlusion/occlusionportalentity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#include "audio/audio_channel.h"
#include "audio/environment/occlusion/occlusionportalentity.h"
#include "audio/northaudioengine.h"

#include "breakableglass/glassmanager.h"
#include "breakableglass/breakable.h"
#include "fragment/typegroup.h"

#include "objects/door.h"

AUDIO_ENVIRONMENT_OPTIMISATIONS()

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(naOcclusionPortalEntity, CONFIGURED_FROM_FILE, 0.61f, atHashString("OcclusionPortalEntity",0xc757e6cc));	

audCurve naOcclusionPortalEntity::sm_DoorAngleToPortalOcclusionCurve;

#if __BANK
extern bool g_DrawBrokenGlass;
extern bool g_DebugDoorOcclusion;
#endif

naOcclusionPortalEntity::naOcclusionPortalEntity() : 
m_Entity(NULL),
m_LinkType(AUD_OCC_LINK_TYPE_NONE),
m_MaxOcclusion(1.0f),
m_EntityModelHashkey(0),
m_DoorOcclusion(1.0f), 
m_BrokenGlassArea(0.0f),
m_IsDoor(false),
m_IsGlass(false)
{
#if __BANK
	m_DebugColor.SetRed(audEngineUtil::GetRandomNumberInRange(100, 255));
	m_DebugColor.SetGreen(audEngineUtil::GetRandomNumberInRange(100, 255));
	m_DebugColor.SetBlue(audEngineUtil::GetRandomNumberInRange(100, 255));
#endif
}

naOcclusionPortalEntity::~naOcclusionPortalEntity()
{
}

void naOcclusionPortalEntity::InitClass()
{
	sm_DoorAngleToPortalOcclusionCurve.Init(ATSTRINGHASH("DOOR_ANGLE_TO_PORTAL_OCCLUSION", 0x09c99495d));
}

naOcclusionPortalEntity* naOcclusionPortalEntity::Allocate()
{
	if(naVerifyf(naOcclusionPortalEntity::GetPool()->GetNoOfFreeSpaces() > 0, "naOcclusionPortalEntity pool is full (size %d), need to increase size", naOcclusionPortalEntity::GetPool()->GetSize()))
	{
		return rage_new naOcclusionPortalEntity();
	}
	return NULL;
}

void naOcclusionPortalEntity::InitFromMetadata(naOcclusionPortalEntityMetadata* portalEntityMetadata)
{
	m_LinkType = (audOcclusionPortalEntityLinkType)portalEntityMetadata->m_LinkType;
	m_MaxOcclusion = portalEntityMetadata->m_MaxOcclusion;
	m_EntityModelHashkey = portalEntityMetadata->m_EntityModelHashkey;
	m_IsDoor = portalEntityMetadata->m_IsDoor;
	m_IsGlass = portalEntityMetadata->m_IsGlass;
}

void naOcclusionPortalEntity::Update()
{
	if(GetIsGlass())
	{
		UpdateGlass();
	}
	if(GetIsDoor())
	{
		UpdateDoor();
	}
}

void naOcclusionPortalEntity::UpdateGlass()
{
	m_BrokenGlassArea = 0.0f;	// Reset this so if the entity streams out and back in and is whole again, we're not using a broken value
	
	if(m_Entity)
	{
		const fragInst* frag = m_Entity->GetFragInst();
		if(frag)
		{
			const fragCacheEntry* cacheEntry = frag->GetCacheEntry();
			const fragType* type = frag->GetType();
			if(cacheEntry && type)
			{
				const fragHierarchyInst* hierarchyInst = cacheEntry->GetHierInst();
				if(hierarchyInst)
				{
					const u8 numGroups = frag->GetTypePhysics()->GetNumChildGroups();
					for(u8 groupIdx = 0; groupIdx < numGroups; groupIdx++)
					{
						const fragTypeGroup* typeGroup = frag->GetTypePhysics()->GetGroup(groupIdx);
						if(typeGroup && typeGroup->GetMadeOfGlass() && hierarchyInst->groupBroken->IsSet(groupIdx))
						{
							spdAABB paneBounds;
							paneBounds.Invalidate();
							const bgPaneModelInfoBase* paneModelInfo = type->GetAllGlassPaneModelInfos()[typeGroup->GetGlassPaneModelInfoIndex()];
							if(paneModelInfo)
							{
								// Determine the bounds of the pane of glass and transform that into world space
								Vec3V topLeft = VECTOR3_TO_VEC3V(paneModelInfo->m_posBase);																
								Vec3V bottomRight = VECTOR3_TO_VEC3V(paneModelInfo->m_posBase + paneModelInfo->m_posWidth + paneModelInfo->m_posHeight);

								// spdAABB needs the m_min to actually be the 'min' corner or else it'll be marked as invalid.
								Vec3V boxMin = Min(topLeft, bottomRight);
								Vec3V boxMax = Max(topLeft, bottomRight);

								paneBounds.Set(boxMin, boxMax);
							}

							if(paneBounds.IsValid())
							{
								m_BrokenGlassArea += GetAreaOfBounds(paneBounds);

#if __BANK
								if(g_DrawBrokenGlass)
								{
									paneBounds.Transform(m_Entity->GetMatrix());
									grcDebugDraw::BoxAxisAligned(paneBounds.GetMin(), paneBounds.GetMax(), Color_aquamarine1, false);
								}
#endif
							}
						}
					}
				}
			}
		}
	}
}

f32 naOcclusionPortalEntity::GetAreaOfBounds(spdAABB& bounds)
{
	f32 area = 0.0f;

	// bounds isn't always set up correctly with our min and max, so we need to do this
	Vec3V vMin = Min(bounds.GetMin(), bounds.GetMax());
	Vec3V vMax = Max(bounds.GetMin(), bounds.GetMax());

	f32 length = vMax.GetXf() - vMin.GetXf();
	f32 width = vMax.GetYf() - vMin.GetYf();
	f32 height = vMax.GetZf() - vMin.GetZf();
	if(length < width && length < height)
		area = width * height;
	else if(width < length && width < height)
		area = length * height;
	else
		area = length * width;

	return area;
}

void naOcclusionPortalEntity::UpdateDoor()
{
	// Use the pre-baked max occlusion value to use if the door is streamed out because we can assume if it's streamed out it's closed.
	m_DoorOcclusion = GetMaxOcclusion();

	// Use the actual door openness if we have the entity
	if(m_Entity && m_Entity->GetIsTypeObject())
	{
		const CObject* object = static_cast<const CObject*>(m_Entity.Get());
		if(object && object->IsADoor())
		{
			const CDoor* door = static_cast<const CDoor*>(object);
			if(door)
			{
				f32 doorAngle = abs(door->GetDoorOpenRatio());
				if(!FPIsFinite(doorAngle))
				{
					doorAngle = 0.0f;
				}
				f32 doorOcclusionRatio = sm_DoorAngleToPortalOcclusionCurve.CalculateValue(doorAngle);
				m_DoorOcclusion = Lerp(doorOcclusionRatio, 0.0f, GetMaxOcclusion());

#if __BANK
				if(g_DebugDoorOcclusion && door->GetCurrentPhysicsInst() && door->GetDoorAudioEntity())
				{
					SetMaxOcclusion(audNorthAudioEngine::GetOcclusionManager()->ComputeMaxDoorOcclusion(m_Entity));
				}
#endif
			}
		}
	}
}

u32 naOcclusionPortalEntity::GetEntityModelHashkey() const 
{ 
	return m_EntityModelHashkey;
}

audOcclusionPortalEntityLinkType naOcclusionPortalEntity::GetLinkType() const 
{ 
	return m_LinkType;
}

f32 naOcclusionPortalEntity::GetMaxOcclusion() const 
{ 
	return m_MaxOcclusion;
}

bool naOcclusionPortalEntity::GetIsDoor() const 
{ 
	return m_IsDoor;
}

bool naOcclusionPortalEntity::GetIsGlass() const 
{ 
	return m_IsGlass;
}

#if __BANK
void naOcclusionPortalEntity::SetMaxOcclusion(const f32 maxOcclusion)
{ 
	m_MaxOcclusion = maxOcclusion;
}

// Occlusion Builder Functions
void naOcclusionPortalEntity::SetEntityModelHashkey(const u32 modelHashkey)
{ 
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	naAssertf(audNorthAudioEngine::GetOcclusionManager()->GetIsBuildingOcclusionPaths(), "We shouldn't be setting modelHashkey values unless we're doing an occlusion build");
	m_EntityModelHashkey = modelHashkey;
}

void naOcclusionPortalEntity::SetLinkType(const audOcclusionPortalEntityLinkType linkType)
{ 
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	naAssertf(audNorthAudioEngine::GetOcclusionManager()->GetIsBuildingOcclusionPaths(), "We shouldn't be setting linkType values unless we're doing an occlusion build");
	m_LinkType = linkType;
}

void naOcclusionPortalEntity::SetIsDoor(const bool isDoor)
{ 
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	naAssertf(audNorthAudioEngine::GetOcclusionManager()->GetIsBuildingOcclusionPaths(), "We shouldn't be setting isDoor values unless we're doing an occlusion build");
	m_IsDoor = isDoor;
}

void naOcclusionPortalEntity::SetIsGlass(const bool isGlass)
{ 
	naAssertf(PARAM_audOcclusionBuildEnabled.Get(), "Calling occlusion build function when occlusion build is not enabled.");
	naAssertf(audNorthAudioEngine::GetOcclusionManager()->GetIsBuildingOcclusionPaths(), "We shouldn't be setting isGlass values unless we're doing an occlusion build");
	m_IsGlass = isGlass;
}
#endif






