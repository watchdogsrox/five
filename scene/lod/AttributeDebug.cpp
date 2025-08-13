
#if __BANK

#include "grcore/debugdraw.h"
#include "fwdebug/picker.h"

#include "atl/string.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "objects/object.h"
#include "objects/DummyObject.h"
#include "scene/building.h"
#include "scene/AnimatedBuilding.h"
#include "scene/lod/LodDebug.h"
#include "debug/Editing/MapEditRESTInterface.h"

#include "AttributeDebug.h"

//DON'T COMMIT
//OPTIMISATIONS_OFF()

bool						CAttributeTweaker::bTweakAttributes = false;

// These need to reflect what the picked entity have set
TweakerAttributeContainer	CAttributeTweaker::m_Attributes;
TweakerAttributeContainer	CAttributeTweaker::m_OldAttributes;
							bool CAttributeTweaker::bStreamingPriorityLow;
int							CAttributeTweaker::iPriorityLevel;
CEntity*					CAttributeTweaker::pLastSelectedEntity = NULL;

void CAttributeTweaker::AddWidgets(bkBank* pBank)
{
	pBank->PushGroup("Attributes", false);
	{
		pBank->AddToggle("Enable attribute tweaking", &bTweakAttributes);
		pBank->AddSeparator("");
		pBank->AddToggle("Don't cast shadows", &m_Attributes.bDontCastShadows);								// Archetype (Special case)
		pBank->AddToggle("Don't render in shadows", &m_Attributes.bDontRenderInShadows);					// Entity

		pBank->AddToggle("Don't render in reflections", &m_Attributes.bDontRenderInReflections);
		pBank->AddToggle("Only render in reflections", &m_Attributes.bOnlyRenderInReflections);

		pBank->AddToggle("Don't render in water reflections", &m_Attributes.bDontRenderInWaterReflections);
		pBank->AddToggle("Only render in water reflections", &m_Attributes.bOnlyRenderInWaterReflections);

		pBank->AddToggle("Don't render in mirror reflections", &m_Attributes.bDontRenderInMirrorReflections);
		pBank->AddToggle("Only render in mirror reflections", &m_Attributes.bOnlyRenderInMirrorReflections);

		pBank->AddToggle("Low priority (for streaming)", &bStreamingPriorityLow);
		pBank->AddSlider("Priority Level", &iPriorityLevel, (int)fwEntityDef::PRI_REQUIRED, (int)fwEntityDef::PRI_OPTIONAL_LOW, 1 );
	}
	pBank->PopGroup();
}

void CAttributeTweaker::Update()
{
	if( bTweakAttributes )	// Master switch
	{
		if (g_PickerManager.GetSelectedEntity() != pLastSelectedEntity)
		{
			pLastSelectedEntity = (CEntity*)g_PickerManager.GetSelectedEntity();
			if (pLastSelectedEntity)
			{
				// Look at where this data comes from to determine how to put it back into the source, vis flags are just reflected in the entity from elsewhere
				// Shadows are special case because they can be set in both the entity and the archetype
				GetShadowAttribsFromEntity(pLastSelectedEntity);
				GetReflectionAttribsFromEntity(pLastSelectedEntity);

				bStreamingPriorityLow = pLastSelectedEntity->IsBaseFlagSet(fwEntity::LOW_PRIORITY_STREAM);	// fwEntityDef
				iPriorityLevel = pLastSelectedEntity->GetDebugPriority();									// fwEntityDef
			}
		}
		else if (pLastSelectedEntity)
		{
			bool	hasChanged = false;

			// Reflect any changes
			if( m_Attributes.bDontCastShadows != GetDontCastShadows(pLastSelectedEntity) || m_OldAttributes.HaveAttribsChanged(m_Attributes) )
			{
				hasChanged = true;
				
				fwFlags16 &flags = pLastSelectedEntity->GetRenderPhaseVisibilityMask();
				flags.SetAllFlags(	VIS_PHASE_MASK_ALL );

				SetDontCastShadows(pLastSelectedEntity, m_Attributes.bDontCastShadows);
				SetDontRenderInShadows(pLastSelectedEntity, m_Attributes.bDontRenderInShadows);
				SetReflectionAttribsInEntity(pLastSelectedEntity);
			}
			m_OldAttributes = m_Attributes;

			if(	bStreamingPriorityLow != pLastSelectedEntity->IsBaseFlagSet(fwEntity::LOW_PRIORITY_STREAM) )
			{
				hasChanged = true;
				if( bStreamingPriorityLow )
				{
					pLastSelectedEntity->SetBaseFlag(fwEntity::LOW_PRIORITY_STREAM);
				}
				else
				{
					pLastSelectedEntity->ClearBaseFlag(fwEntity::LOW_PRIORITY_STREAM);
				}
			}
			
			if( iPriorityLevel != pLastSelectedEntity->GetDebugPriority() )
			{
				hasChanged = true;
				pLastSelectedEntity->SetDebugPriority(iPriorityLevel, pLastSelectedEntity->GetIsFixedFlagSet());
			}

			if(	hasChanged )	// Only write out values if they've been changed.
			{
				// Update any changes in the REST list
				UpdateRestAttributeList(pLastSelectedEntity, m_Attributes, bStreamingPriorityLow, iPriorityLevel);
			}
		}
	}
}

void	CAttributeTweaker::GetShadowAttribsFromEntity(CEntity *pEntity)
{
	m_OldAttributes.bDontCastShadows = m_Attributes.bDontCastShadows = GetDontCastShadows(pEntity);
	m_OldAttributes.bDontRenderInShadows = m_Attributes.bDontRenderInShadows = GuessDontRenderInShadows(pEntity);
}

// See Entity.cpp (InitEntityFromDefinition)
void	CAttributeTweaker::GetReflectionAttribsFromEntity(CEntity *pEntity)
{
	fwFlags16 &flags = pEntity->GetRenderPhaseVisibilityMask();

	m_OldAttributes.bDontRenderInReflections = m_Attributes.bDontRenderInReflections = (flags & VIS_PHASE_MASK_REFLECTIONS) == 0;
	m_OldAttributes.bOnlyRenderInReflections = m_Attributes.bOnlyRenderInReflections = ( (flags & ~(VIS_PHASE_MASK_REFLECTIONS|VIS_PHASE_MASK_STREAMING)) == 0) && 
																						(flags & VIS_PHASE_MASK_REFLECTIONS) != 0;

	m_OldAttributes.bDontRenderInWaterReflections = m_Attributes.bDontRenderInWaterReflections = (flags & VIS_PHASE_MASK_WATER_REFLECTION) == 0;
	m_OldAttributes.bOnlyRenderInWaterReflections = m_Attributes.bOnlyRenderInWaterReflections = ( (flags & ~(VIS_PHASE_MASK_WATER_REFLECTION|VIS_PHASE_MASK_STREAMING)) == 0) &&
																								  (flags & VIS_PHASE_MASK_WATER_REFLECTION) != 0;

	m_OldAttributes.bDontRenderInMirrorReflections = m_Attributes.bDontRenderInMirrorReflections = (flags & VIS_PHASE_MASK_MIRROR_REFLECTION) == 0;
	m_OldAttributes.bOnlyRenderInMirrorReflections = m_Attributes.bOnlyRenderInMirrorReflections = ( (flags & ~(VIS_PHASE_MASK_MIRROR_REFLECTION|VIS_PHASE_MASK_STREAMING)) == 0) &&
																									(flags & VIS_PHASE_MASK_MIRROR_REFLECTION) != 0;
}

// Set the attributes based on the current settings.
// fwEntity::fwEntity() sets all flags, we jist set the 
void	CAttributeTweaker::SetReflectionAttribsInEntity(CEntity *pEntity)
{
	fwFlags16 &flags = pEntity->GetRenderPhaseVisibilityMask();

	// set flags for reflection
	if( m_Attributes.bDontRenderInReflections )
		flags.ClearFlag( VIS_PHASE_MASK_REFLECTIONS );
	else if( m_Attributes.bOnlyRenderInReflections)
		flags.ClearFlag( (u16)(~(VIS_PHASE_MASK_REFLECTIONS|VIS_PHASE_MASK_STREAMING)) );

	// set flags for water reflection
	if(m_Attributes.bDontRenderInWaterReflections)
		flags.ClearFlag( VIS_PHASE_MASK_WATER_REFLECTION );
	else if(m_Attributes.bOnlyRenderInWaterReflections)
		flags.ClearFlag( (u16)(~(VIS_PHASE_MASK_WATER_REFLECTION|VIS_PHASE_MASK_STREAMING)));

	// set flags for mirror reflection
	if(m_Attributes.bDontRenderInMirrorReflections)
		flags.ClearFlag( VIS_PHASE_MASK_MIRROR_REFLECTION );
	else if(m_Attributes.bOnlyRenderInMirrorReflections)
		flags.ClearFlag( (u16)(~(VIS_PHASE_MASK_MIRROR_REFLECTION|VIS_PHASE_MASK_STREAMING)));

	pEntity->RequestUpdateInWorld();
}

// DONT CAST SHADOWS
// Comes from "archetype"
bool CAttributeTweaker::GetDontCastShadows( CEntity *pEntity )
{
	CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();
	if( pModelInfo )
	{
		return pModelInfo->GetDontCastShadows();
	}
	return false;
}


void CAttributeTweaker::SetEntityShadowFlags(CEntity *pEntity, bool entityShadowFlag)
{
	CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();

	// Set the flags on this entity
	fwFlags16 &flags = pEntity->GetRenderPhaseVisibilityMask();
	bool dontRenderInShadows = pModelInfo->GetDontCastShadows() || entityShadowFlag;
	if(dontRenderInShadows)
	{
		flags.ClearFlag( VIS_PHASE_MASK_SHADOWS );
	}
	else
	{
		flags.SetFlag( VIS_PHASE_MASK_SHADOWS );
	}
	pEntity->RequestUpdateInWorld();

}


void CAttributeTweaker::SetDontCastShadows( CEntity *pEntity, bool val )
{
	CBaseModelInfo *pModelInfo = pEntity->GetBaseModelInfo();
	if( pModelInfo )
	{
		pModelInfo->SetDontCastShadows(val);

		SetEntityShadowFlags(pEntity, m_Attributes.bDontRenderInShadows);

		// Go Through all the pools looking for entities that have the same modelinfo and update accordingly.
		{	// DUMMY OBJECTS
			CDummyObject::Pool* pPool = CDummyObject::GetPool();
			for(int i = 0;i<pPool->GetSize(); i++)
			{
				CEntity *pOtherEntity = pPool->GetSlot(i);
				if(pOtherEntity)
				{
					CBaseModelInfo *pOtherModelInfo = pOtherEntity->GetBaseModelInfo();
					if(pOtherModelInfo)
					{
						if( pModelInfo == pOtherModelInfo )
						{
							// Got one
							SetEntityShadowFlags(pOtherEntity, false);	// We can't know the other flag, assume false
						}
					}
				}
			}
		}
		
		{	// OBJECTS
			CObject::Pool* pPool = CObject::GetPool();
			for(int i = 0;i<pPool->GetSize(); i++)
			{
				CEntity *pOtherEntity = pPool->GetSlot(i);
				if(pOtherEntity)
				{
					CBaseModelInfo *pOtherModelInfo = pOtherEntity->GetBaseModelInfo();
					if(pOtherModelInfo)
					{
						if( pModelInfo == pOtherModelInfo )
						{
							// Got one
							SetEntityShadowFlags(pOtherEntity, false);	// We can't know the other flag, assume false
						}
					}
				}
			}
		}

		{	// BUILDINGS
			CBuilding::Pool* pPool = CBuilding::GetPool();
			for(int i = 0;i<pPool->GetSize(); i++)
			{
				CEntity *pOtherEntity = pPool->GetSlot(i);
				if(pOtherEntity)
				{
					CBaseModelInfo *pOtherModelInfo = pOtherEntity->GetBaseModelInfo();
					if(pOtherModelInfo)
					{
						if( pModelInfo == pOtherModelInfo )
						{
							// Got one
							SetEntityShadowFlags(pOtherEntity, false);	// We can't know the other flag, assume false
						}
					}
				}
			}
		}
		
		{	// ANIMATED BUILDINGS
			CAnimatedBuilding::Pool* pPool = CAnimatedBuilding::GetPool();
			for(int i = 0;i<pPool->GetSize(); i++)
			{
				CEntity *pOtherEntity = pPool->GetSlot(i);
				if(pOtherEntity)
				{
					CBaseModelInfo *pOtherModelInfo = pOtherEntity->GetBaseModelInfo();
					if(pOtherModelInfo)
					{
						if( pModelInfo == pOtherModelInfo )
						{
							// Got one
							SetEntityShadowFlags(pOtherEntity, false);	// We can't know the other flag, assume false
						}
					}
				}
			}
		}
	}
}



// DONT RENDER IN SHADOWS
// This comes from "entity"
// Since this is data is thrown away, and all I have are vis flags in the entity I have no idea if this flag is set.
// However, if the DONT shadow flags aren't set in the Archetype, and they are in the entity, then it must have been set in the fwEntityDef.
// If it is set in the archetype, then it may or may not have been set in the entity. In this case, assume it wasn't set.
bool CAttributeTweaker::GuessDontRenderInShadows( CEntity *pEntity )
{
	if(GetDontCastShadows(pEntity))
	{
		// Don't cast shadows is true, can't determine the entity shadow flag, so assume it's unset
		return false;
	}
	else
	{
		// Don't cast shadows is false, if entity doesn't cast shadows, then DontRenderInShadows must be true
		fwFlags16 &flags = pEntity->GetRenderPhaseVisibilityMask();
		if( !flags.IsFlagSet(VIS_PHASE_MASK_SHADOWS) )
		{
			// Shadows aren't drawn
			return true;
		}
	}
	return false;
}


// Try to also include the Archetype info so it looks correct on screen
void CAttributeTweaker::SetDontRenderInShadows( CEntity *pEntity, bool val )
{
	SetEntityShadowFlags(pEntity, val);
}


void CAttributeTweaker::UpdateRestAttributeList(CEntity* pEntity, TweakerAttributeContainer &tweakValues,
												bool bStreamingPriorityLow, int priority)
{
	if (!pEntity && !CLodDistTweaker::IsEntityFromMapData(pEntity))
		return;

	atArray<CAttributeDebugAttribOverride>& list = g_MapEditRESTInterface.m_attributeOverride;
	CAttributeDebugAttribOverride newEntry(pEntity, tweakValues, bStreamingPriorityLow, priority );

	for (s32 i=0; i<list.GetCount(); i++)
	{
		CAttributeDebugAttribOverride& listEntry = list[i];
		if (listEntry == newEntry)
		{
			listEntry.m_bDontCastShadows = tweakValues.bDontCastShadows;
			listEntry.m_bDontRenderInShadows = tweakValues.bDontRenderInShadows;
			listEntry.m_bDontRenderInReflections = tweakValues.bDontRenderInReflections;
			listEntry.m_bOnlyRenderInReflections = tweakValues.bOnlyRenderInReflections;
			listEntry.m_bDontRenderInWaterReflections = tweakValues.bDontRenderInWaterReflections;
			listEntry.m_bOnlyRenderInWaterReflections = tweakValues.bOnlyRenderInWaterReflections;
			listEntry.m_bDontRenderInMirrorReflections = tweakValues.bDontRenderInMirrorReflections;
			listEntry.m_bOnlyRenderInMirrorReflections = tweakValues.bOnlyRenderInMirrorReflections;
			listEntry.m_bStreamingPriorityLow = bStreamingPriorityLow;
			listEntry.m_iPriority = priority;
			return;
		}
	}
	list.Grow() = newEntry;
}

#endif	//__BANK