//
// filename:	AnimatedBuilding.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "crclip/clip.h"
#include "crmetadata/properties.h"
#include "crmetadata/property.h"
#include "crmetadata/propertyattributes.h"
#include "fwanimation/animdirector.h"
#include "fwanimation/move.h"
#include "fwsys/timer.h"

// Game headers
#include "AnimatedBuilding.h"
#include "animation/EventTags.h"
#include "animation/Move.h"
#include "Cloth/ClothMgr.h"
#include "scene/lod/LodMgr.h"

ANIM_OPTIMISATIONS()

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CAnimatedBuilding, CONFIGURED_FROM_FILE, 0.55f, atHashString("AnimatedBuilding",0x2e968919));

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

//
// name:		CBuilding::CBuilding
// description:	Constructor for a building
CAnimatedBuilding::CAnimatedBuilding(const eEntityOwnedBy ownedBy)
	: CDynamicEntity( ownedBy )
{
	SetTypeAnimatedBuilding();
	SetVisibilityType( VIS_ENTITY_ANIMATED_BUILDING );

	EnableCollision();
	SetBaseFlag(fwEntity::IS_FIXED);
}

CAnimatedBuilding::~CAnimatedBuilding()
{
	TellAudioAboutAudioEffectsRemove();
}

void CAnimatedBuilding::Add()
{
	CEntity::Add();
}

void CAnimatedBuilding::Remove()
{
	CEntity::Remove();
}

#if !__NO_OUTPUT
void CAnimatedBuilding::PrintSkeletonData()
{
	size_t bytes = 0;
	Displayf("Name, Num_Bones, Bytes");

	for (s32 i = 0; i < CAnimatedBuilding::GetPool()->GetSize(); ++i)
	{
		CAnimatedBuilding* pEntity = CAnimatedBuilding::GetPool()->GetSlot(i);
		if (pEntity)
		{
			pEntity->PrintSkeletonSummary();
			bytes += pEntity->GetSkeletonSize();
		}
	}
	Displayf("Total CAnimatedBuilding Skeletons: %" SIZETFMT "d KB\n", bytes >> 10);
}
#endif

// name:		CDynamicEntity::AddToInterior
// description:	add entity to the active object list for an interior (which is set in entity, in advance of this call)
void CAnimatedBuilding::AddToInterior(fwInteriorLocation interiorLocation)
{
	CEntity::AddToInterior( interiorLocation );
}

void CAnimatedBuilding::RemoveFromInterior()
{
	CEntity::RemoveFromInterior();
}

void CAnimatedBuilding::InitEntityFromDefinition(fwEntityDef* definition, fwArchetype* archetype, s32 mapDataDefIndex)
{
	CDynamicEntity::InitEntityFromDefinition(definition, archetype, mapDataDefIndex);
	TellAudioAboutAudioEffectsAdd();
}

//
// name:		CAnimatedBuilding::CreateDrawable
// description:	Create animated building drawable and setup animation
bool CAnimatedBuilding::CreateDrawable()
{
	// give building physics if required
	if ( GetLodData().IsHighDetail() && IsBaseFlagSet(fwEntity::HAS_PHYSICS_DICT) && !IsBaseFlagSet(fwEntity::NO_INSTANCED_PHYS) && !GetCurrentPhysicsInst() )
	{
		InitPhys();
		if (GetCurrentPhysicsInst())
		{
			AddPhysics();
		}
	}

	// If dynamic entity create drawable is successful
	if(CDynamicEntity::CreateDrawable())
	{
		// setup skeleton and anim structures
		CreateSkeleton(); 
		if (GetSkeleton())
		{
			CreateAnimDirector(*GetDrawable(), false);
			if (GetAnimDirector())
			{
				// apply anim to drawable
				CBaseModelInfo* pModelInfo = GetBaseModelInfo();
				s32 clipDictionaryIndex = pModelInfo->GetClipDictionaryIndex().Get();
				Assert(clipDictionaryIndex != -1);

				// get the clip and load
				const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(clipDictionaryIndex, pModelInfo->GetHashKey());
				int flags = 0;

				if (pClip && pClip->HasProperties())
				{
					const crProperty* pProperty = pClip->GetProperties()->FindProperty("AnimPlayerFlags");

					if (pProperty)
					{
						const crPropertyAttribute* pAttrib = pProperty->GetAttribute("AnimPlayerFlags");
						if(pAttrib && pAttrib->GetType() == crPropertyAttribute::kTypeInt)
						{
							const crPropertyAttributeInt* pAttribInt = static_cast<const crPropertyAttributeInt*>(pAttrib);
							flags = pAttribInt->GetInt();
						}
						else 
						{
							flags = APF_ISLOOPED;
						}
					}
					else
					{
						flags = APF_ISLOOPED;
					}
				}
				else
				{
					flags = APF_ISLOOPED;
				}

				CMoveAnimatedBuilding& move = GetMoveAnimatedBuilding();

				move.SetPlaybackAnim((s16) clipDictionaryIndex, pModelInfo->GetHashKey());

				UpdateRateAndPhase();

				AddToSceneUpdate();
				return true;
			}
		}
	}
	return false;
}

void CAnimatedBuilding::UpdateRateAndPhase()
{
	if(CAnimatedBuilding::GetAnimDirector())
	{
		CEntity *pLod = (CEntity *)GetRootLod();
		if(pLod)
		{
			CBaseModelInfo *pModelInfo = GetBaseModelInfo();
			s32 clipDictionaryIndex = pModelInfo->GetClipDictionaryIndex().Get();
			Assert(clipDictionaryIndex != -1);
			const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(clipDictionaryIndex, pModelInfo->GetHashKey());
			if(pClip)
			{
				u32 randomSeed = GetLodRandomSeed(pLod);

				float rate = 1.0f;
				const CClipEventTags::CRandomizeRateProperty *pRateProp = CClipEventTags::FindProperty< CClipEventTags::CRandomizeRateProperty >(pClip, CClipEventTags::RandomizeRate);
				if(pRateProp)
				{
					rate = pRateProp->PickRandomRate(randomSeed);
				}

				float phase = 0.0f;
				const CClipEventTags::CRandomizeStartPhaseProperty* pPhaseProp = CClipEventTags::FindProperty< CClipEventTags::CRandomizeStartPhaseProperty >(pClip, CClipEventTags::RandomizeStartPhase);
				if(pPhaseProp)
				{
					phase = pPhaseProp->PickRandomStartPhase(randomSeed);
				}

				float time = pClip->ConvertPhaseToTime(phase);
				u32 basetime = NetworkInterface::IsNetworkOpen() ? (u32) NetworkInterface::GetSyncedTimeInMilliseconds() & 0x000FFFFF : fwTimer::GetTimeInMilliseconds(); //use the synchronized network time in MP to keep the animation movement aligned - only use lower bits of network time as it is too large
				time += ((static_cast< float >(basetime) / 1000.0f) * rate);
				phase = pClip->ConvertTimeToPhase(time);
				phase = phase - Floorf(phase);

				CMoveAnimatedBuilding &move = GetMoveAnimatedBuilding();
				move.SetRate(rate);
				move.SetPhase(phase);
			}
		}
	}
}

//
// name:		CAnimatedBuilding::DeleteDrawable
// description:	Delete the drawable and remove from the process control list
void CAnimatedBuilding::DeleteDrawable()
{
	CDynamicEntity::DeleteDrawable();
	RemoveFromSceneUpdate();
}

bool CAnimatedBuilding::ProcessControl()
{
	UpdateRateAndPhase();

	return true;
}

#if !__SPU
fwMove *CAnimatedBuilding::CreateMoveObject()
{
	return rage_new CMoveAnimatedBuildingPooledObject(*this); 
}

const fwMvNetworkDefId &CAnimatedBuilding::GetAnimNetworkMoveInfo() const
{
	return CClipNetworkMoveInfo::ms_NetworkAnimatedBuilding;
}
#endif // !__SPU
