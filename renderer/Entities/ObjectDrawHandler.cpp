/////////////////////////////////////////////////////////////////////////////////
// Title	:	ObjectDrawHandler.cpp
// Author	:	Russ Schaaf
// Started	:	18/11/2010
//
/////////////////////////////////////////////////////////////////////////////////


#include "renderer/Entities/ObjectDrawHandler.h"

// Rage Headers

// Framework Headers
//#include "fwdrawlist/drawlistmgr.h"

// Game Headers
#include "control/replay/replay.h"
#include "debug/DebugScene.h"
#include "objects/object.h"
#include "objects/ObjectPopulation.h"
#include "scene/Entity.h"
#include "peds/Ped.h"
#include "pickups/Pickup.h"
#include "pickups/Data/PickupData.h"
#include "renderer/DrawLists/DrawListProfileStats.h"
#include "renderer/OcclusionQueries.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "fwscene/stores/drawablestore.h"
#include "Weapons/Weapon.h"
#include "Weapons/components/WeaponComponent.h"

RENDER_OPTIMISATIONS()

dlCmdBase* CObjectDrawHandler::AddToDrawList(fwEntity* pBaseEntity, fwDrawDataAddParams* pBaseParams)
{
	DL_PF_FUNC( TotalAddToDrawList );
	DL_PF_FUNC( TotalObjectAddToDrawList );
	DL_PF_FUNC( ObjectAddToDrawList );

	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnAddToDrawListOfFocusEntity(), pBaseEntity );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfAddToDrawListCallingEntity(), VEC3V_TO_VECTOR3(pBaseEntity->GetTransform().GetPosition()) );

	CDrawListPrototypeManager::Flush();

	CObject* pObject = static_cast<CObject*>(pBaseEntity);

#if __BANK
	if (ShouldSkipEntity(pObject))
		return NULL;
#endif // __BANK

	// If attached to an entity and that entity is frozen by an interior then don't render it
	fwEntity *attachParent = pObject->GetAttachParent();
	if (attachParent && ((CPhysical *) attachParent)->m_nDEflags.bFrozenByInterior)
		return NULL;


	if(CObjectPopulation::IsCObjectsAlbedoSupportEnabled())
	{
		if(	(pObject->GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT)	&&
			(pObject->GetIsAlbedoAlpha())						)
		{
			// BS#6576933: Arcade Games - Exploder 3D -  New shader for props in 3D maze game:
			// skip rendering if the object is marked as albedo and albedo rendering mode is not enabled (so object is rendered in a different renderphase, etc.):
			if(!CObjectPopulation::IsCObjectsAlbedoRenderingActive())
			{
				return NULL;
			}
		}
	}


	// don't render object if attached to a ped and ped is invisible for some reason
	if (pObject->GetIsTypeObject() && pObject->GetWeapon())
	{
		const CWeapon* weapon = pObject->GetWeapon();
		if (weapon->GetOwner())
		{
			const CPed* ped = weapon->GetOwner();
			// force motion blur mask if this is a player ped
			if (ped && ped->GetPedModelInfo() && ped->IsLocalPlayer())
			{
				pObject->m_nFlags.bAddtoMotionBlurMask = true;
			}

			const bool bNotBuildingHUDDrawList = !DRAWLISTMGR->IsBuildingHudDrawList();
			if (ped->GetPedResetFlag(CPED_RESET_FLAG_DontRenderThisFrame) && bNotBuildingHUDDrawList)
				return NULL;
		}
	}

	if (pObject->GetIsTypeObject() && pObject->GetWeaponComponent())
	{
		const CWeapon* weapon = pObject->GetWeaponComponent()->GetWeapon();
		const CPed* ped = weapon->GetOwner();
		// force motion blur mask if this is a player ped
		if (ped && ped->GetPedModelInfo() && ped->IsLocalPlayer())
		{
			pObject->m_nFlags.bAddtoMotionBlurMask = true;
		}
	}



#if NORTH_CLOTHS
	if(pObject->GetIsTypeObject())
	{
		CBaseModelInfo* pModel = pObject->GetBaseModelInfo();
		if(pModel->GetAttributes()==MODEL_ATTRIBUTE_IS_BENDABLE_PLANT)
		{
			if(!pObject->GetSkeleton())	// CObject: no skeleton on bendable plant?
			{
				Assertf(pObject->GetSkeleton(), "Bendable plant '%s' doesn't have skeleton!", pModel->GetModelName());
				return(NULL);
			}
		}
	}
#endif //NORTH_CLOTHS...

	// detect vehicle's falloff parts (doors, etc.) here:
	bool bIsVehicleFallOffPart = FALSE;
	CVehicle *pParentVehicle = NULL;
	fwCustomShaderEffect* pOriginalShaderEffect = NULL;

	CBaseModelInfo* pMI = pObject->GetBaseModelInfo();
	Assert(pMI);

	if(pObject->m_nObjectFlags.bVehiclePart || pObject->GetFragParent())
	{
		CEntity* pParentEntity = pObject->GetFragParent();
		if(pParentEntity == NULL || (pParentEntity->GetIsTypeVehicle() && ((CVehicle*)pParentEntity)->GetHasBeenRepainted()))
		{
			REPLAY_ONLY(if(CReplayMgr::IsReplayInControlOfWorld() == false || pObject->GetOwnedBy() != ENTITY_OWNEDBY_REPLAY))
			{
				pObject->SetBaseFlag(fwEntity::REMOVE_FROM_WORLD);
			}
			return NULL;
		}

		// modelinfo of the part is vehicle's mi:
		if(pMI->GetModelType() == MI_TYPE_VEHICLE)
		{
			bIsVehicleFallOffPart = TRUE;
			pParentVehicle = static_cast<CVehicle*>(pObject->GetFragParent());
			Assert(pParentVehicle);
			pOriginalShaderEffect = GetShaderEffect();

			fwCustomShaderEffect *se = pParentVehicle->GetDrawHandler().GetShaderEffect();
			// temporarily set VehicleShaderEffect from fragParent onto this child:
			this->SetShaderEffect(se);

			Assert(se->GetType() == CSE_VEHICLE);
			CCustomShaderEffectVehicle* pSev = (CCustomShaderEffectVehicle*)se;
			pSev->SetIsBrokenOffPart(true);
		}
	}

	if (pObject->m_nObjectFlags.bPedProp && !pObject->GetFragParent())
	{
		REPLAY_ONLY(if(CReplayMgr::IsReplayInControlOfWorld() == false || pObject->GetOwnedBy() != ENTITY_OWNEDBY_REPLAY))
		{
			pObject->SetBaseFlag(fwEntity::REMOVE_FROM_WORLD);
		}
		return NULL;
	}

	if(pObject->GetUseOcclusionQuery())
	{
		unsigned int query = GetOcclusionQueryId();
			
		if(query == 0)
		{
			query = OcclusionQueries::OQAllocate();
			SetOcclusionQueryId(query);
		}
			
		if( query )
		{
			Vector3 bbMin,bbMax;
			GetDrawable()->GetLodGroup().GetBoundingBox(bbMin, bbMax);
			Vec3V min = VECTOR3_TO_VEC3V(bbMin);
			Vec3V max = VECTOR3_TO_VEC3V(bbMax);
			if( pObject->m_nObjectFlags.bIsPickUp )
			{
				min *= ScalarV(V_TWO); // YAKOI ?
				max *= ScalarV(V_TWO);
			}
			OcclusionQueries::OQSetBoundingBox(query, min, max, pObject->GetMatrix());
		}
	}

	dlCmdBase *pReturnDC = NULL;

	// don't draw this objects model info - draw the one for the original prop
	if (pObject->m_nObjectFlags.bDetachedPedProp){
		//DLC ( 	CDrawPedPropDC, (uPropData.m_parentModelIdx,  GetMatrix(), uPropData.m_propIdxHash, 
		//	uPropData.m_texIdxHash, AlphaFade, GetAmbientOcclusionScale(), m_nFlags.bAddtoMotionBlurMask));
		DLC ( CDrawDetachedPedPropDC, (pObject));
	}
	else if( true == pObject->m_nFlags.bAddtoMotionBlurMask )
	{
		DLC_Add(CShaderLib::BeginMotionBlurMask);
		pReturnDC = CDynamicEntityDrawHandler::AddToDrawList(pBaseEntity, pBaseParams); //rsTODO: Can I be more specific about which AddToDrawList function to call?
		DLC_Add(CShaderLib::EndMotionBlurMask);
	}
	else
	{
		pReturnDC = CDynamicEntityDrawHandler::AddToDrawList(pBaseEntity, pBaseParams); // rsTODO: Can I be more specific about which AddToDrawList function to call?
	}

	if (pReturnDC && pMI->GetModelType() == MI_TYPE_WEAPON)
	{
		Assert(pReturnDC->GetInstructionIdStatic() == DC_DrawSkinnedEntity);
		CDrawSkinnedEntityDC* pSkinnedDC = static_cast<CDrawSkinnedEntityDC*>(pReturnDC);
		CWeapon *pWeapon = pObject->GetWeapon();
		// Not all weapon objects have CWeapon structures attached.  e.g. Ammo doesn't 
		bool currentlyHD = false;
		if (pWeapon && pWeapon->GetIsCurrentlyHD())
		{
			currentlyHD = true;
		}
		else
		{
			CWeaponComponent *pComponent = pObject->GetWeaponComponent();
			if (pComponent && pComponent->GetIsCurrentlyHD())
			{
				currentlyHD = true;
			}
		}

		if (currentlyHD)
		{
			gDrawListMgr->AddArchetype_HD_Reference(pObject->GetModelIndex());
			pSkinnedDC->GetDrawData().SetIsHD(true);
		}

		pSkinnedDC->GetDrawData().SetIsWeaponModelInfo(true);
	}

	if(bIsVehicleFallOffPart)
	{
		fwCustomShaderEffect* se = this->GetShaderEffect();
		Assert(se->GetType() == CSE_VEHICLE);
		CCustomShaderEffectVehicle* pSev = (CCustomShaderEffectVehicle*)se;
		pSev->SetIsBrokenOffPart(false);

		// reset ShaderEffect of vehicle's child (so only main fragParent keeps it):
		this->SetShaderEffect(pOriginalShaderEffect);

		if(pReturnDC)
		{
			Assert(pReturnDC->GetInstructionIdStatic() == DC_DrawFrag);
			CDrawFragDC* pFragDC = static_cast<CDrawFragDC*>(pReturnDC);

			if(pObject->m_nObjectFlags.bCarWheel && pObject->m_nFlags.bIsFrag)
			{
				// set flag in draw command to draw wheels along with car
				pFragDC->SetFlag(CDrawFragDC::CAR_WHEEL);
			}

			bool bAllowHD = true;

			// NOTE: this doesn't work here, we have to intercept this in FragDraw and FragDrawCarWheels
			// adjust HD based on current renderphase settings etc.
			//gDrawListMgr->AdjustVehicleHD(false, bAllowHD);

			Assert(pParentVehicle);
			if(pParentVehicle->GetIsCurrentlyHD() && bAllowHD)
			{
				pFragDC->SetFlag(CDrawFragDC::HD_REQUIRED);
				
				// fallen off vehicle part is HD, so make sure HD frag stays in memory during renderthread execution:
				gDrawListMgr->AddArchetype_HD_Reference(pObject->GetModelIndex());
			}
		}
	}

	// nothing needs the return value for this type atm
	return NULL;
}


dlCmdBase* CObjectFragmentDrawHandler::AddToDrawList(fwEntity* pBaseEntity, fwDrawDataAddParams* pBaseParams)
{
	DL_PF_FUNC( TotalAddToDrawList );
	DL_PF_FUNC( TotalObjectAddToDrawList );
	DL_PF_FUNC( ObjectFragmentAddToDrawList );

	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnAddToDrawListOfFocusEntity(), pBaseEntity );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfAddToDrawListCallingEntity(), VEC3V_TO_VECTOR3(pBaseEntity->GetTransform().GetPosition()) );

	CDrawListPrototypeManager::Flush();

	Assert(pBaseEntity->GetType() == ENTITY_TYPE_OBJECT);
	CObject* pObject = static_cast<CObject*>(pBaseEntity);

#if __BANK
	if (ShouldSkipEntity(pObject))
		return NULL;
#endif // __BANK

	// If attached to an entity and that entity is frozen by an interior then don't render it
	fwEntity *attachParent = pObject->GetAttachParent();
	if (attachParent && ((CPhysical *) attachParent)->m_nDEflags.bFrozenByInterior)
		return NULL;


#if NORTH_CLOTHS
	if(pObject->GetIsTypeObject())
	{
		CBaseModelInfo* pModel = pObject->GetBaseModelInfo();
		if(pModel->GetAttributes()==MODEL_ATTRIBUTE_IS_BENDABLE_PLANT)
		{
			if(!pObject->GetSkeleton())	// CObject: no skeleton on bendable plant?
			{
				Assertf(pObject->GetSkeleton(), "Bendable plant '%s' doesn't have skeleton!", pModel->GetModelName());
				return(NULL);
			}
		}
	}
#endif //NORTH_CLOTHS...

	// prepare this object for rendering if it has been smashed
	Assert(pObject->m_nFlags.bIsFrag);

	// detect vehicle's falloff parts (doors, etc.) here:
	bool bIsVehicleFallOffPart = FALSE;
	CVehicle *pParentVehicle = NULL;
	fwCustomShaderEffect* pOriginalShaderEffect = NULL;
	if(pObject->m_nObjectFlags.bVehiclePart || pObject->GetFragParent())
	{
		CEntity* pParentEntity = pObject->GetFragParent();
		if(pParentEntity == NULL || (pParentEntity->GetIsTypeVehicle() && ((CVehicle*)pParentEntity)->GetHasBeenRepainted()))
		{
			REPLAY_ONLY(if(CReplayMgr::IsReplayInControlOfWorld() == false || pObject->GetOwnedBy() != ENTITY_OWNEDBY_REPLAY))
			{
				pObject->SetBaseFlag(fwEntity::REMOVE_FROM_WORLD);
			}
			return NULL;
		}

		// modelinfo of the part is vehicle's mi:
		CBaseModelInfo* pMI = pObject->GetBaseModelInfo();
		Assert(pMI);
		if(pMI->GetModelType() == MI_TYPE_VEHICLE)
		{
			bIsVehicleFallOffPart = TRUE;
			pParentVehicle = static_cast<CVehicle*>(pObject->GetFragParent());
			Assert(pParentVehicle);
			pOriginalShaderEffect = this->GetShaderEffect();

			fwCustomShaderEffect *se = pParentVehicle->GetDrawHandler().GetShaderEffect();
			// temporarily set VehicleShaderEffect from fragParent onto this child:
			this->SetShaderEffect(se);

			Assert(se->GetType() == CSE_VEHICLE);
			CCustomShaderEffectVehicle* pSev = (CCustomShaderEffectVehicle*)se;
			pSev->SetIsBrokenOffPart(true);
		} 
	}

	if (pObject->m_nObjectFlags.bPedProp && !pObject->GetFragParent())
	{
		REPLAY_ONLY(if(CReplayMgr::IsReplayInControlOfWorld() == false || pObject->GetOwnedBy() != ENTITY_OWNEDBY_REPLAY))
		{
			pObject->SetBaseFlag(fwEntity::REMOVE_FROM_WORLD);
		}
		return NULL;
	}

	if(pObject->GetUseOcclusionQuery())
	{
		unsigned int query = GetOcclusionQueryId();
			
		if(query == 0)
		{
			query = OcclusionQueries::OQAllocate();
			SetOcclusionQueryId(query);
		}
			
		if( query )
		{
			Vector3 bbMin,bbMax;
			GetDrawable()->GetLodGroup().GetBoundingBox(bbMin, bbMax);
			Vec3V min = VECTOR3_TO_VEC3V(bbMin);
			Vec3V max = VECTOR3_TO_VEC3V(bbMax);

			OcclusionQueries::OQSetBoundingBox(query, min, max, pObject->GetMatrix());
		}
	}
	
	dlCmdBase *pReturnDC = NULL;

	// don't draw this objects model info - draw the one for the original prop
	if (pObject->m_nObjectFlags.bDetachedPedProp){
		//DLC ( 	CDrawPedPropDC, (uPropData.m_parentModelIdx,  GetMatrix(), uPropData.m_propIdxHash, 
		//	uPropData.m_texIdxHash, AlphaFade, GetAmbientOcclusionScale(), m_nFlags.bAddtoMotionBlurMask));
		DLC ( CDrawDetachedPedPropDC, (pObject));
	}
	else if( true == pObject->m_nFlags.bAddtoMotionBlurMask )
	{
		DLC_Add(CShaderLib::BeginMotionBlurMask);
		pReturnDC = CDynamicEntityDrawHandler::AddFragmentToDrawList(this, pBaseEntity, pBaseParams);
		DLC_Add(CShaderLib::EndMotionBlurMask);

	}
	else
	{
		pReturnDC = CDynamicEntityDrawHandler::AddFragmentToDrawList(this, pBaseEntity, pBaseParams);
	}

	// Use high LOD for articulated objects that have been moved from their initial pose
// 	TUNE_BOOL(bForceArticulatedHighLOD, true);
// 	if(bForceArticulatedHighLOD)
	if (pReturnDC)
	{
		Assert(pReturnDC->GetInstructionIdStatic() == DC_DrawFrag);
		CDrawFragDC* pFragDC = static_cast<CDrawFragDC*>(pReturnDC);
		if (fragInst* pFragInst = pObject->GetFragInst())
		{
			if (fragCacheEntry* pCacheEntry = pFragInst->GetCacheEntry())
			{
				if (pCacheEntry->GetHierInst()->body)
				{
					//grcDebugDraw::Line(CGameWorld::FindLocalPlayerCoors(), RCC_VECTOR3(pFragInst->GetPosition()), Color32(255.0f, 0.0f, 0.0f));
					pFragDC->SetFlag(CDrawFragDC::FORCE_HI_LOD);
				}
			}
		}
	}

	if(bIsVehicleFallOffPart)
	{
		fwCustomShaderEffect* se = this->GetShaderEffect();
		Assert(se->GetType() == CSE_VEHICLE);
		CCustomShaderEffectVehicle* pSev = (CCustomShaderEffectVehicle*)se;
		pSev->SetIsBrokenOffPart(false);

		// reset ShaderEffect of vehicle's child (so only main fragParent keeps it):
		this->SetShaderEffect(pOriginalShaderEffect);

		if(pReturnDC)
		{
			Assert(pReturnDC->GetInstructionIdStatic() == DC_DrawFrag);
			CDrawFragDC* pFragDC = static_cast<CDrawFragDC*>(pReturnDC);

			if(pObject->m_nObjectFlags.bCarWheel)
			{
				// set flag in draw command to draw wheels along with car
				pFragDC->SetFlag(CDrawFragDC::CAR_PLUS_WHEELS);

				// copy burst ratios into draw command if any tyres are burst
				CCustomShaderEffectVehicle* pShaderEffect = static_cast<CCustomShaderEffectVehicle*>(pOriginalShaderEffect);
				if(pShaderEffect->GetIsBurstWheel())
				{
					pFragDC->SetupBurstWheels(pShaderEffect->GetBurstAndSideRatios(0));
				}
			}

			Assert(pParentVehicle);
			if(pParentVehicle->GetIsCurrentlyHD())
			{
				pFragDC->SetFlag(CDrawFragDC::HD_REQUIRED);

				// fallen off vehicle part is HD, so make sure HD frag stays in memory during renderthread execution:
				gDrawListMgr->AddArchetype_HD_Reference(pObject->GetModelIndex());
			}
		}
	}

	// nothing needs the return value for this type atm
	return NULL;
}
