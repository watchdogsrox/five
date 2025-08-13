/////////////////////////////////////////////////////////////////////////////////
// Title	:	DynamicEntityDrawHandler.cpp
// Author	:	Russ Schaaf
// Started	:	18/11/2010
//
/////////////////////////////////////////////////////////////////////////////////

#include "renderer/Entities/DynamicEntityDrawHandler.h"

// Rage Headers
#include "breakableglass/bgdrawable.h"
#include "breakableglass/breakable.h"
#include "grcore/matrix43.h"
#include "profile/cputrace.h"

// Framework Headers
#if __BANK
#include "fwdebug/picker.h"
#endif // __BANK

// Game Headers
#include "cloth/charactercloth.h"
#include "physics/gtaInst.h"
#include "physics/physics.h"
#include "scene/DynamicEntity.h"
#include "renderer/Renderer.h"
#include "renderer/DrawLists/drawListMgr.h"
#include "renderer/DrawLists/DrawListProfileStats.h"
#include "renderer/Entities/VehicleDrawHandler.h"
#include "shaders/CustomShaderEffectVehicle.h"
#include "shaders/ShaderLib.h"

#if __BANK
#include "vehicles/VehicleFactory.h"
#endif // __BANK

RENDER_OPTIMISATIONS()

RAGETRACE_DECL(CDynamicEntity_AddToDrawList);


dlCmdBase* CDynamicEntityDrawHandler::AddToDrawList(fwEntity* pBaseEntity, fwDrawDataAddParams* pBaseParams)
{
	DL_PF_FUNC( TotalAddToDrawList );
	DL_PF_FUNC( TotalDynamicEntityAddToDrawList );
	DL_PF_FUNC( DynamicEntityAddToDrawList );

	RAGETRACE_SCOPE(CDynamicEntity_AddToDrawList);

	CDynamicEntity* pEntity = static_cast<CDynamicEntity*>(pBaseEntity);

#if __BANK
	if (ShouldSkipEntity(pEntity))
		return NULL;
#endif // __BANK

	// fragments need a fragInst to render
	if(pEntity->m_nFlags.bIsFrag && pEntity->GetFragInst() == NULL)
		return NULL;

	// want to return the draw command that was created here
	dlCmdBase *pReturnDC = NULL;

	// if we're not a fragment and don't have a skeleton then use basic render
	if(!pEntity->m_nFlags.bIsFrag && pEntity->GetSkeleton() == NULL)
	{
		pReturnDC = CEntityDrawHandler::AddBasicToDrawList(this, pBaseEntity, pBaseParams);
	}
	else
	{
		if (pEntity->m_nFlags.bIsFrag)
		{
			CDrawListPrototypeManager::Flush();
			pReturnDC = CDynamicEntityDrawHandler::AddFragmentToDrawList(this, pBaseEntity, pBaseParams);
		}
		else
		{
			CDrawListPrototypeManager::Flush();
			pReturnDC = CDynamicEntityDrawHandler::AddSkinnedToDrawList(this, pBaseEntity, pBaseParams);
		}
	}

	return pReturnDC;
}

dlCmdBase* CDynamicEntityDrawHandler::AddFragmentToDrawList(CEntityDrawHandler* pDrawHandler, fwEntity* pBaseEntity, fwDrawDataAddParams* pParams)
{
	DL_PF_FUNC( TotalAddToDrawList );
	DL_PF_FUNC( TotalDynamicEntityAddToDrawList );
	DL_PF_FUNC( DynamicEntityFragmentAddToDrawList );

	RAGETRACE_SCOPE(CDynamicEntity_AddToDrawList);

	CDynamicEntity* pEntity = static_cast<CDynamicEntity*>(pBaseEntity);

#if __BANK
	if (ShouldSkipEntity(pEntity))
		return NULL;
#endif // __BANK

	// fragments need a fragInst to render
	Assertf(pEntity->m_nFlags.bIsFrag, "Only fragments are allowed here!");


	fragInst* pFragInst = pEntity->GetFragInst();
	if(!pEntity->m_nFlags.bIsFrag || pFragInst == NULL)
		return NULL;

	// Make sure its really really a fragment 
#if __ASSERT
	Assertf(IsFragInst(pFragInst), "Object (%s) claimed to be a fragment, but the physics isn't a fraginst", pEntity->GetBaseModelInfo()->GetModelName());
#endif

	// want to return the draw command that was created here
	dlCmdBase *pReturnDC = NULL;

	eRenderMode renderMode = DRAWLISTMGR->GetUpdateRenderMode();
	u32 bucket = DRAWLISTMGR->GetUpdateBucket();
	u32 bucketMask = DRAWLISTMGR->GetUpdateBucketMask();

	pDrawHandler->BeforeAddToDrawList(pEntity, pEntity->GetModelIndex(), renderMode, bucket, bucketMask, pParams, true);

	CBaseModelInfo* pMI = pEntity->GetBaseModelInfo();
	Assert(pMI);
	// Search for a cloth drawable
	fragCacheEntry *cacheEntry = pFragInst->GetCacheEntry();
	rmcDrawable *pClothDrawable = NULL;

	bool captureStats = false;
	BANK_ONLY(captureStats = pBaseEntity == g_PickerManager.GetSelectedEntity();)

	bool onlyCloth = true;
	bool isClothReady = true;
	environmentCloth* pEnvCloth = NULL;
	if( cacheEntry )
	{
		fragHierarchyInst* hierInst = cacheEntry->GetHierInst();
		Assertf( hierInst,"No HierInst on a cached frag" );
		pEnvCloth = hierInst->envCloth;
		if( pEnvCloth REPLAY_ONLY(&& !pEnvCloth->GetFlag(environmentCloth::CLOTH_ForceInactiveForReplay)))
		{
			pClothDrawable = pEnvCloth->GetDrawable();
			pEnvCloth->SetFlag(environmentCloth::CLOTH_IsVisible,true);
			Assert( pFragInst->GetType() );
			onlyCloth = (NULL == pFragInst->GetType()->GetClothDrawable()) ;

			Assert( pEnvCloth->GetClothController() );
			Assert( pEnvCloth->GetClothController()->GetClothInstance() );
			isClothReady = ((clothInstance*) pEnvCloth->GetClothController()->GetClothInstance())->IsReady();
		}
	}

	bool bIsVehicleCloth = false;

	// Draw a cloth ?
	if( pClothDrawable && onlyCloth && isClothReady )
	{
#if __BANK
		if(g_DrawCloth)
#endif // __BANK
		{		
			Assert( pEnvCloth );
			gDrawListMgr->AddClothReference(pEnvCloth);
			Assert( pClothDrawable );
			// Set material to trees to get back lighting to work
			u8 matid = pEntity->GetDeferredMaterial();
			if ((matid&DEFERRED_MATERIAL_CLEAR)==0){
				matid = (matid&~DEFERRED_MATERIAL_CLEAR) |DEFERRED_MATERIAL_TREE;
			}

			Vector3 v = VEC3V_TO_VECTOR3(pEnvCloth->GetOffset());

			grcDepthStencilStateHandle DSS = static_cast<CDrawListMgr*>(gDrawListMgr)->IsBuildingMirrorReflectionDrawList() ? CPhysics::GetClothManager()->GetInvertPassDSS() : CPhysics::GetClothManager()->GetPassDSS();
			DLC( dlCmdDrawTwoSidedDrawable, (pClothDrawable, v, pEntity->GetModelIndex(), pParams->AlphaFade, 
					pEntity->GetNaturalAmbientScale(), pEntity->GetArtificialAmbientScale(), matid, pEntity->GetInteriorLocation().IsValid(), CPhysics::GetClothManager()->GetPassRS(), DSS, captureStats, true, bIsVehicleCloth, pEntity));
		}
	}
	else if( pMI->GetModelType() != MI_TYPE_PED )
	{ 
		// No Peds
		if (pFragInst->GetSkeleton())
		{
			GET_DLC(pReturnDC, CDrawFragDC, (pEntity, false));

			// render a second time the damaged part
			// if we have a skeleton, then we must have a frag cache entry (that's where the skeleton comes from)
			// also must have a hierachy instance, since that is contained in the frag cache entry
			fragCacheEntry* entry = pFragInst->GetCacheEntry();
			Assert(entry);
			Assert(entry->GetHierInst());
			if (entry->GetHierInst()->anyGroupDamaged)
			{
				DLC(CDrawFragDC, (pEntity, true));
			}
		} 
#if ENABLE_FRAG_OPTIMIZATION
		else if(pMI->GetModelType() == MI_TYPE_VEHICLE)
		{
			DLC(CDrawFragTypeVehicleDC, (pEntity));
		}
#endif // ENABLE_FRAG_OPTIMIZATION
		else 
		{
			DLC(CDrawFragTypeDC, (pEntity));
		}

		// NOTE: If there is cloth attached to the vehicle, need to be rendered separately
		if( !onlyCloth && isClothReady )
		{
			Assert( pClothDrawable );
#if __BANK
			if(g_DrawCloth)
#endif // __BANK
			{
				if(pMI->GetModelType()==MI_TYPE_VEHICLE)
				{
					bIsVehicleCloth = true;
				}
				else
				{
					Assert( pEnvCloth );
					gDrawListMgr->AddClothReference(pEnvCloth);
					Vector3 v = VEC3V_TO_VECTOR3(pEnvCloth->GetOffset());
					grcDepthStencilStateHandle DSS = static_cast<CDrawListMgr*>(gDrawListMgr)->IsBuildingMirrorReflectionDrawList() ? CPhysics::GetClothManager()->GetInvertPassDSS() : CPhysics::GetClothManager()->GetPassDSS();
					DLC( dlCmdDrawTwoSidedDrawable, (pClothDrawable, v, pEntity->GetModelIndex(), pParams->AlphaFade, pEntity->GetNaturalAmbientScale(), pEntity->GetArtificialAmbientScale(), pEntity->GetDeferredMaterial(), pEntity->GetInteriorLocation().IsValid(), CPhysics::GetClothManager()->GetPassRS(), DSS, captureStats, true, bIsVehicleCloth, pEntity));
				}
			}
		}
	}

	if(!DRAWLISTMGR->IsBuildingShadowDrawList() && !DRAWLISTMGR->IsBuildingWaterReflectionDrawList() && !DRAWLISTMGR->IsBuildingGlobalReflectionDrawList() && (bucket == CRenderer::RB_ALPHA || bucket == CRenderer::RB_CUTOUT)) // Skip unsupported modes/buckets
	{
		if(fragCacheEntry* pCacheEntry = pFragInst->GetCacheEntry())
		{
			if(fragHierarchyInst* pHierInst = pCacheEntry->GetHierInst())
			{
				for(int glassIndex = 0; glassIndex != pHierInst->numGlassInfos; ++ glassIndex)
				{
					bgGlassHandle handle = pHierInst->glassInfos[glassIndex].handle;
					if(handle != bgGlassHandle_Invalid)
					{
						bgDrawable& glassDrawable = bgGlassManager::GetGlassDrawable(handle);
						
						if( glassDrawable.IsReadyToDraw() )
						{
							bgBreakable& glassBreakable = bgGlassManager::GetGlassBreakable(handle);

							// Copy the transform matrices on a fire&forget DC buffer
							atArray<Matrix43> &brTransforms = glassBreakable.GetCurrentMatrices();
							float *transforms = NULL;

							const int numTransforms = brTransforms.GetCount();
							if(numTransforms)
							{
								// Allocate the buffer
								u32 dataSize = numTransforms * sizeof(Matrix43);
								dataSize += 0xf;
								dataSize &= 0xfffffff0;
								transforms = (float*)gDCBuffer->AllocatePagedMemory(DPT_LIFETIME_RENDER_THREAD, dataSize, false, 16);
								Assert16(transforms);
								sysMemCpy(transforms, brTransforms.GetElements(), dataSize);

								//Setup Draw Data 
								bgDrawableDrawData bgDrawData;
								bgDrawData.SetupDrawData(glassBreakable, glassDrawable, transforms, numTransforms);
							
								//Pass draw data into the breakable Glass DC
								DLC(CDrawBreakableGlassDC, (glassDrawable, bgDrawData, pEntity)); 
							}
						}
					}
				}
			}
		}
	}

	// VehicleMods: Andrzej's note to self: this must be called as last item as it directly modifies current vehicle's CSE instance:
	if (pMI->GetModelType() == MI_TYPE_VEHICLE && pEntity->GetIsTypeVehicle())
	{
		CVehicleDrawHandler* vdh = (CVehicleDrawHandler*)pDrawHandler;
		if (vdh->GetVariationInstance().GetNumMods() > 0 && vdh->GetVariationInstance().GetVehicleRenderGfx())
		{
#if __BANK
			if (CVehicleFactory::ms_variationRenderMods)
#endif // __BANK
			{
				dlCmdBase* varDlc = NULL;
				GET_DLC(varDlc, CDrawVehicleVariationDC, (pEntity));
				Assert(varDlc);

				CCustomShaderEffectVehicle* pShaderEffect = static_cast<CCustomShaderEffectVehicle*>(pEntity->GetDrawHandler().GetShaderEffect());
				Assert(pShaderEffect);

				if(pShaderEffect->GetIsBurstWheel() && (vdh->GetVariationInstance().HasCustomWheels() || vdh->GetVariationInstance().HasCustomRearWheels()))
				{
					((CDrawVehicleVariationDC*)varDlc)->SetupBurstWheels(pShaderEffect->GetBurstAndSideRatios(0));
				}
			}
		}
	}

	pDrawHandler->AfterAddToDrawList(pEntity, renderMode, pParams);

	if(bIsVehicleCloth && isClothReady)
	{
		CVehicleDrawHandler *pVehDrawHandler = (CVehicleDrawHandler*)pDrawHandler;
		Assert(pVehDrawHandler->GetShaderEffect()->GetType()==CSE_VEHICLE);
		CCustomShaderEffectVehicle *pOrigVehCSE = (CCustomShaderEffectVehicle*)pVehDrawHandler->GetShaderEffect();
		Assert(pOrigVehCSE);

		if(pVehDrawHandler->GetShaderEffectSD())
		{
			CCustomShaderEffectVehicle *pVehCseHD = pOrigVehCSE;
			CCustomShaderEffectVehicle *pVehCseSD = (CCustomShaderEffectVehicle*)pVehDrawHandler->GetShaderEffectSD();
			// copy CSE HD->std settings:
			pVehCseSD->CopySettings(pVehCseHD, pEntity);
			
			pVehDrawHandler->SetShaderEffect(pVehCseSD);	// hack: replace CSE for SD
		}

		// disable veh damage for cloth:
		CCustomShaderEffectVehicle *pVehClothCSE = (CCustomShaderEffectVehicle*)pVehDrawHandler->GetShaderEffect();
		Assert(pVehClothCSE);
		pVehClothCSE->UpdateVehicleColors(pEntity);
		const bool bOrigVehDamageEnabled = pVehClothCSE->GetEnableDamage();
		pVehClothCSE->SetEnableDamage(false);

		pDrawHandler->BeforeAddToDrawList(pEntity, pEntity->GetModelIndex(), renderMode, bucket, bucketMask, pParams, true);

		Assertf(pClothDrawable->GetShaderGroup().GetShaderGroupVarCount()>0, "CSEVehicle: Uninitialized cloth drawable in model %s!", pEntity->GetModelName());

		Assert( pEnvCloth );
		gDrawListMgr->AddClothReference(pEnvCloth);
		Vector3 v = VEC3V_TO_VECTOR3(pEnvCloth->GetOffset());
		grcDepthStencilStateHandle DSS = static_cast<CDrawListMgr*>(gDrawListMgr)->IsBuildingMirrorReflectionDrawList() ? CPhysics::GetClothManager()->GetInvertPassDSS() : CPhysics::GetClothManager()->GetPassDSS();
		DLC( dlCmdDrawTwoSidedDrawable, (pClothDrawable, v, pEntity->GetModelIndex(), pParams->AlphaFade, pEntity->GetNaturalAmbientScale(), pEntity->GetArtificialAmbientScale(), pEntity->GetDeferredMaterial(), pEntity->GetInteriorLocation().IsValid(), CPhysics::GetClothManager()->GetPassRS(), DSS, captureStats, true, bIsVehicleCloth, pEntity));

		pDrawHandler->AfterAddToDrawList(pEntity, renderMode, pParams);

		pVehClothCSE->SetEnableDamage(bOrigVehDamageEnabled);	// restore vehicle damage
		pVehDrawHandler->SetShaderEffect(pOrigVehCSE);			// hack: set original CSE back
	}

	return pReturnDC;
}


dlCmdBase* CDynamicEntityDrawHandler::AddSkinnedToDrawList(CEntityDrawHandler* pDrawHandler, fwEntity* pBaseEntity, fwDrawDataAddParams* pBaseParams)
{
	DL_PF_FUNC( TotalAddToDrawList );
	DL_PF_FUNC( TotalDynamicEntityAddToDrawList );
	DL_PF_FUNC( DynamicEntitySkinnedAddToDrawList );

	RAGETRACE_SCOPE(CDynamicEntity_AddToDrawList);

	CDynamicEntity* pEntity = static_cast<CDynamicEntity*>(pBaseEntity);

#if __BANK
	if (ShouldSkipEntity(pEntity))
		return NULL;
#endif // __BANK

	// want to return the draw command that was created here
	dlCmdBase *pReturnDC = NULL;

	// if we're not a fragment and don't have a skeleton then use basic render
	Assert(!pEntity->m_nFlags.bIsFrag && pEntity->GetSkeleton() != NULL);

	eRenderMode renderMode = DRAWLISTMGR->GetUpdateRenderMode();
	u32 bucket = DRAWLISTMGR->GetUpdateBucket();
	u32 bucketMask = DRAWLISTMGR->GetUpdateBucketMask();

	pDrawHandler->BeforeAddToDrawList(pEntity, pEntity->GetModelIndex(), renderMode, bucket, bucketMask, pBaseParams);

	// copy off skeleton data into the drawlist	
	GET_DLC(pReturnDC, CDrawSkinnedEntityDC, (pEntity));

	pDrawHandler->AfterAddToDrawList(pEntity, renderMode, pBaseParams);

	return pReturnDC;
}
