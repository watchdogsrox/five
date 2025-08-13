///////////////////////////////////////////////////////////////////////////////
//  
//	FILE:	Breakable.cpp
//	BY	: 	
//	FOR	:	Rockstar North
//	ON	:	14 June 2005
// 
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////


// rage
#include "crskeleton/skeleton.h"
#include "fragment/drawable.h"
#include "fragment/manager.h"
#include "fragment/tune.h"
#include "fragment/type.h"
#include "fragment/typegroup.h"
#include "phbound/boundcomposite.h"
#include "phglass/glassinstance.h"
#include "system/memory.h"

// framework
#include "fwrenderer/renderlistbuilder.h"
#include "fwrenderer/renderlistgroup.h"
#include "fwdrawlist/drawlistmgr.h"
#include "vfx/vfxutil.h"

// game 
#include "camera/CamInterface.h"
#include "debug/vectormap.h"
#include "objects/object.h"
#include "peds/Ped.h"	// temp for cheat init()
#include "physics/gtaArchetype.h"
#include "physics/gtaInst.h"
#include "physics/physics.h"
#include "renderer/renderListGroup.h"
#include "renderer/DrawLists/DrawableDataStructs.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "shaders/ShaderLib.h"
#include "scene/world/gameWorld.h"
#include "scene/lod/LodDrawable.h"
#include "scene/lod/LodScale.h"
#include "vehicles/vehicle.h"

#include "Breakable.h"

PHYSICS_OPTIMISATIONS()


//  GLOBAL VARIABLES
// CBreakable g_breakable;
__THREAD CBreakable *g_pBreakable = NULL;
int	CBreakable::m_nFragCacheObjectCount = 0;
bool CBreakable::m_bInsertDirectToRenderList = false;


CBreakable::CBreakable()
{
	m_drawBucketMask = 0x0;
	m_renderMode = rmNIL;
	m_modelType = -1;
	m_bIsVehicleMod = false;
	m_bIsWreckedVehicle = false;
	m_clampedLod = 0xff;
	m_lowLodMult = 1.f;
	m_bIsHd = false;
	m_phaseLODs = LODDrawable::LODFLAG_NONE_ALL;
	m_lastLODIdx = 0;
	m_entityAlpha = 255;
#if RSG_PC || __ASSERT
	m_modelInfo = NULL;
#endif // RSG_PC || __ASSERT
}

													
void CBreakable::InitFragInst(fragInst* pFragInst)
{
	pFragInst->SetUserData(NULL);
}


dev_bool sbVehicleComponentNoCollide = false;
//
void CBreakable::Insert(fragInst* pInst)
{
	// we got the NewMovable function to set the inst's movableId to point to itself
	physicsAssert(!pInst->IsInLevel());

	if(pInst && (pInst->GetUserData()==NULL || pInst->GetInstFlag(phInstGta::FLAG_USERDATA_PARENT_INST)))
	{
		sysMemStartTemp();
		// increment count of active frag cache objects
		m_nFragCacheObjectCount++;

		Assert(pInst->GetClassType()==PH_INST_FRAG_CACHE_OBJECT);
		fragInst* pFragInst = (fragInst* )pInst;

		fragInst* pParentFragInst = (fragInst*)pInst->GetUserData();
		pInst->SetUserData(NULL);
		pInst->SetInstFlag(phInstGta::FLAG_USERDATA_PARENT_INST, false);
		CEntity* pParentEntity = NULL;
		if(pParentFragInst)
			pParentEntity = CPhysics::GetEntityFromInst(pParentFragInst);

		// now create an object for this fragment
		if (!Verifyf(pParentEntity, "Can't do parentless insertion of breakable entity"))
		{
			pInst->SetInstFlag(phfwInst::FLAG_USERDATA_ENTITY, false);
			m_nFragCacheObjectCount--;
			sysMemEndTemp();
			return;
		}

		CObject* pObj = CObjectPopulation::CreateObject(pParentEntity->GetModelId(), ENTITY_OWNEDBY_FRAGMENT_CACHE, false, false, false);
		if (!pObj)
		{
			Warningf("CBreakable::Insert - Unable to allocate a CObject");
			pInst->SetInstFlag(phfwInst::FLAG_USERDATA_ENTITY, false);
			m_nFragCacheObjectCount--;
			sysMemEndTemp();
			return;
		}

	#if GTA_REPLAY
		// Broken object created so tell the replay it should record it
		CReplayMgr::RecordObject(pObj);

		// Inherit the mapID from the parent.
		if(pParentEntity->GetIsTypeObject())
			pObj->SetHash(static_cast< CObject  *>(pParentEntity)->GetHash());
	#endif // GTA_REPLAY

		// set bRenderDamaged flag for now, lets other code know this thing has been damaged (eg to turn off lights)
		pObj->m_nFlags.bRenderDamaged = true;

		pObj->GetLodData().SetResetDisabled(true);
		pObj->m_nFlags.bIsFrag = true;
		pObj->m_nEndOfLifeTimer = fwTimer::GetTimeInMilliseconds();

		pObj->SetPhysicsInst(pInst, false);		// false means the physics ref counter won't be increased.		

		// increment ref count due to physInst being set
		CBaseModelInfo *pModelInfo = pObj->GetBaseModelInfo();
		pModelInfo->AddRef();

		if (pParentEntity)
		{
			u32 tintIndex = pParentEntity->GetTintIndex();
			pObj->SetTintIndex(tintIndex);
		}

		pObj->SetIsVisibleForModule(SETISVISIBLE_MODULE_PHYSICS, true);
		pObj->CreateDrawable();

		if( !pObj->GetSkeleton() )
		{
			pObj->CreateSkeleton();
		}		

		pObj->SetBaseFlag(fwEntity::DONT_STREAM);

		// set flags in archetype to say what type of physics object this is
		pInst->GetArchetype()->SetTypeFlags(ArchetypeFlags::GTA_OBJECT_TYPE);

		// set flags in archetype to say what type of physics object we wish to collide with
		pInst->GetArchetype()->SetIncludeFlags(ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES);


		// created by fragment manager - set its position from the fragment instance
		const Matrix34& mat = RCC_MATRIX34(pInst->GetMatrix());
		// try and catch bad matrix coming from breaking apart
		Assertf(mat.a==mat.a && mat.b==mat.b && mat.c==mat.c && mat.d==mat.d, "%s:CBreakable::Insert() new fragInst has a bad matrix", pModelInfo->GetModelName());
		pObj->SetMatrix(mat, false, false, false);

		pObj->SetIsVisibleInSomeViewportThisFrame(true);	// assume visible until process control has a chance to update this

		if(pParentEntity)
		{
			CEntity* pTopParent = pParentEntity;
			if(pTopParent->GetIsTypeObject() && ((CObject*)pTopParent)->GetFragParent())
				pTopParent = ((CObject*)pTopParent)->GetFragParent();
			// check - top parent shouldn't have a parent itself
			Assert(!pTopParent->GetIsTypeObject() || ((CObject*)pTopParent)->GetFragParent()==NULL);
			
			// Set the parent ambient scales, until next frame where it'll get updated.
			u32 natAmb = pParentEntity->GetNaturalAmbientScale();
			u32 artAmb = pParentEntity->GetArtificialAmbientScale();

			pObj->SetNaturalAmbientScale(natAmb);
			pObj->SetArtificialAmbientScale(artAmb);
			
			// want the frag parent to be the original fragment that everything broke off from
			pObj->SetFragParent(pTopParent);

			// stop child colliding with parent until there are no impacts between them

			if(sbVehicleComponentNoCollide && pParentEntity->GetIsTypeVehicle())
				pObj->SetNoCollision(pParentEntity, NO_COLLISION_RESET_WHEN_NO_IMPACTS);

			// let the parent know it's been damaged as well
			if(!pTopParent->GetIsTypeVehicle())
				pTopParent->m_nFlags.bRenderDamaged = true;

			// report the broken frag so we can use this info to update particles, explosion and projected textures
			vfxUtil::ReportBrokenFrag(pParentEntity, pObj);

			if(pParentEntity->GetIsTypeObject())
			{
				CObject* pParentObject = static_cast<CObject*>(pParentEntity);
				// We need to notify the pathfinder that the navigation bounds for the parent now needs recalculating
				// Only do for objects for now, as vehicles retain roughly their same shape despite damage.
				if(pParentObject->IsInPathServer())
					pParentObject->m_nObjectFlags.bBoundsNeedRecalculatingForNavigation = true;

				// need to delete water samples, so they will get re-allocated using new bounding box
			}
			// add a bit to the lifetimer of things that break from mission objects
			if(pTopParent->GetIsTypeObject() && ((CObject*)pTopParent)->GetOwnedBy()==ENTITY_OWNEDBY_SCRIPT)
			{
				pObj->m_nEndOfLifeTimer += 100000;
			}

			pObj->m_nFlags.bHasExploded = pParentEntity->m_nFlags.bHasExploded;

			if(pParentEntity->GetIsTypePed() || pObj->GetBaseModelInfo()->GetModelType()==MI_TYPE_PED)
			{
				Assertf(false, "%s:Woah something broke off a ped ragdoll!", pObj->GetModelName());
				// stop this object from rendering, it's only going to crash because of the ped's shaders
				pObj->SetIsVisibleForModule( SETISVISIBLE_MODULE_PHYSICS, false );
			}

			
			if(pParentEntity->GetIsTypeVehicle())
			{
				CVehicle::RegisterVehicleBreak();

				CVehicle* pParentVehicle = static_cast<CVehicle*>(pParentEntity);

				if(CVehicleDamage::ms_bDisableVehiclePartCollisionOnBreak)
				{
					// During explosion related breaks we don't want to do collision between the vehicle part and the vehicle
					pObj->SetNoCollision(pParentVehicle,NO_COLLISION_RESET_WHEN_NO_BBOX);
				}

				// child indices for car are already stored in model info
				phBoundComposite* newComposite = pFragInst->GetCacheEntry()->GetBound();

				// go through wheel children and see if any are intact in this pFragInst, if so this must be a wheel
				for(int nWheel=0; nWheel<pParentVehicle->GetNumWheels(); nWheel++)
				{
					CWheel* pWheel = pParentVehicle->GetWheel(nWheel);
					int nWheelChildIndex = pWheel->GetFragChild();
					if(nWheelChildIndex > -1 && !pFragInst->GetChildBroken(nWheelChildIndex))
					{
						pObj->m_nObjectFlags.bCarWheel = true;
						
						// Update the bounds to match the skeleton since we don't pose the bounds when the wheel turns
						pFragInst->PoseBoundsFromSkeleton(true,false,false);
						Mat34V_ConstRef wheelComponentMatrix = newComposite->GetCurrentMatrix(nWheelChildIndex);

						// Undo any wheel sweeping that was happening
						newComposite->SetLastMatrix(nWheelChildIndex, wheelComponentMatrix);

						for(int extraWheelBoundIndex = 1; extraWheelBoundIndex < MAX_WHEEL_BOUNDS_PER_WHEEL; ++extraWheelBoundIndex)
						{
							int extraWheelBoundComponentIndex = pWheel->GetFragChild(extraWheelBoundIndex);
							if(extraWheelBoundComponentIndex > -1)
							{
								if(pWheel->HasInnerWheel())
								{
									phBound* pMainWheelBound = newComposite->GetBound(nWheelChildIndex);
									phBound* pExtraWheelBound = newComposite->GetBound(extraWheelBoundComponentIndex);
									if(Verifyf(pMainWheelBound && pExtraWheelBound && pMainWheelBound->GetType() == pExtraWheelBound->GetType(), "Extra wheel bound on '%s' is different type than main wheel.",pParentVehicle->GetDebugName()))
									{
										sysMemEndTemp();
										fragMemStartCacheHeapFunc(pFragInst->GetCacheEntry());
										phLevelNew::CompositeBoundSetBoundThreadSafe(*newComposite,extraWheelBoundComponentIndex,pMainWheelBound);
										fragMemEndCacheHeap();
										sysMemStartTemp();
									}

									// Shift the extra wheel bound over to cover the extra wheel
									// NOTES: -It looks like the right vector of the wheel is already pointing inwards, regardless of if the wheel is on the left or right side of the vehicle
									const ScalarV wheelOffset = ScalarVFromF32(pWheel->GetWheelWidth()*1.1f);
									Mat34V innerWheelComponentMatrix(wheelComponentMatrix.GetMat33ConstRef(), AddScaled(wheelComponentMatrix.GetCol3(),wheelComponentMatrix.GetCol0(),wheelOffset));
									newComposite->SetCurrentMatrix(extraWheelBoundComponentIndex,innerWheelComponentMatrix);
									newComposite->SetLastMatrix(extraWheelBoundComponentIndex,innerWheelComponentMatrix);
								}
								else
								{
									// Just move the extra wheel bound into the center of the wheel so it's out of the way
									newComposite->SetCurrentMatrix(extraWheelBoundComponentIndex,wheelComponentMatrix);
									newComposite->SetLastMatrix(extraWheelBoundComponentIndex,wheelComponentMatrix);
								}
							}
						}

						newComposite->CalculateCompositeExtents();
						if(pFragInst->IsInLevel())
						{
							PHLEVEL->RebuildCompositeBvh(pFragInst->GetLevelIndex());
						}
						else
						{
							newComposite->UpdateBvh(true);
						}
						pFragInst->GetCacheEntry()->CalcCenterOfGravity();
					}
				}
				pParentVehicle->ClearLastBrokenOffPart();
			}

			CGameWorld::Add(pObj, pParentEntity->GetInteriorLocation());
		}
		else
		{
			Assertf(false, "Can't do parentless insertion of breakable entity");
		}

		pObj->UpdateSkeleton();

		// force this object direct onto the main render list if it was broken in weapon processing stage
		if(m_bInsertDirectToRenderList)
		{
			const float camDist = (VEC3V_TO_VECTOR3(pObj->GetTransform().GetPosition()) - camInterface::GetPos()).Mag();
			CGtaRenderListGroup::DeferredAddEntity(31-_CountLeadingZeros(CRenderer::GetSceneToGBufferListBits()), pObj, 0, camDist, RPASS_VISIBLE, 0);
			int shadowPhases = CRenderer::GetShadowPhases();
			int list = 0;
			int subphaseVisFlags = SUBPHASE_CASCADE_MASK;
			while(shadowPhases)
			{
				if(shadowPhases & 0x1)
				{
					CGtaRenderListGroup::DeferredAddEntity(list, pObj, subphaseVisFlags, camDist, RPASS_VISIBLE, 0);
				}
				shadowPhases >>= 1;
				list++;
			}
		}

		// play any required sound effects
		//g_CollisionAudioEntity.ReportFragmentBreak(pObj);

		sysMemEndTemp();
	}
}



void CBreakable::Remove(fragInst* pInst)
{
	// we got the NewMovable function to set the inst's movableId to point to itself
	if(pInst && pInst->GetClassType() == PH_INST_FRAG_CACHE_OBJECT
	&& pInst->GetUserData() && ((CEntity*)pInst->GetUserData())->GetIsTypeObject())
	{
		// decrement count of active frag cache objects
		Assert(m_nFragCacheObjectCount > 0);
		m_nFragCacheObjectCount--;

		// this is a fragment object that was created by CBreakable::Insert() so we're ready  to delete it now.
		CObject *pObj = (CObject*)pInst->GetUserData();
		CGameWorld::Remove(pObj);
		pInst->SetUserData(NULL);
		pInst->SetInstFlag(phfwInst::FLAG_USERDATA_ENTITY, false);
		pInst->SetInstFlag(phInstGta::FLAG_USERDATA_PARENT_INST, false);

		// need to delete drawable and inst pointers so they don't get used before this object gets deleted
		pObj->DeleteDrawable();
		pObj->DeleteSkeleton();
		pObj->DeleteInst();

		// can't delete CObject directly, we might be inside the render update
		// so just make it a temp object that will get deleted next update instead
		//CObjectPopulation::DestroyObject(pObj);
		Assertf(pObj->GetRelatedDummy() == NULL, "Planning to remove a Breakable but it has a dummy");
		Assertf(pObj->GetIsAnyManagedProcFlagSet()==false, "Trying to turn a procedural object into a ENTITY_OWNEDBY_TEMP");
		pObj->SetOwnedBy( ENTITY_OWNEDBY_TEMP );
		pObj->m_nEndOfLifeTimer = 1;
	}
/*
	CObject* pObj = (CObject*)movableId;
	CGameWorld::Remove(pObj);
	pObj->m_nModelIndex = -1;
*/
}



void CBreakable::InitGlassInst(phGlassInst& pGlassInst)
{
	Assert(!pGlassInst.IsInLevel());
	pGlassInst.GetArchetype()->SetTypeFlags(ArchetypeFlags::GTA_GLASS_TYPE | ArchetypeFlags::GTA_UNSMASHED_TYPE);
	pGlassInst.GetArchetype()->SetIncludeFlags(ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES);

	// Glass inst's are owned by a parent frag inst, so make sure the user data points back to the same CEntity
	if(pGlassInst.GetIgnoreInst() && pGlassInst.GetIgnoreInst()->GetInstFlag(phfwInst::FLAG_USERDATA_ENTITY))
	{
		pGlassInst.SetUserData(pGlassInst.GetIgnoreInst()->GetUserData());
		pGlassInst.SetInstFlag(phfwInst::FLAG_USERDATA_ENTITY,true);
	}
}

void CBreakable::InitGlassBound(phBound& pGlassBound)
{
	// only set type bound level type flags if this glass is composite
	if(pGlassBound.IsTypeComposite(pGlassBound.GetType()))
	{
		phBoundComposite& pGlassBoundComposite = *static_cast<phBoundComposite*>(&pGlassBound);

		if(!pGlassBoundComposite.GetTypeAndIncludeFlags())
		{
			pGlassBoundComposite.AllocateTypeAndIncludeFlags();
		}
		
#if BREAKABLE_GLASS_USE_BVH
		pGlassBoundComposite.SetTypeFlags(phGlass::BoundIndex_BVH, ArchetypeFlags::GTA_GLASS_TYPE);
		pGlassBoundComposite.SetIncludeFlags(phGlass::BoundIndex_BVH, ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES);

		pGlassBoundComposite.SetTypeFlags(phGlass::BoundIndex_Box, ArchetypeFlags::GTA_UNSMASHED_TYPE);	
		pGlassBoundComposite.SetIncludeFlags(phGlass::BoundIndex_Box, ArchetypeFlags::GTA_ALL_SHAPETEST_TYPES);
#else
		pGlassBoundComposite.SetTypeFlags(0, ArchetypeFlags::GTA_GLASS_TYPE);	
		pGlassBoundComposite.SetIncludeFlags(0, ArchetypeFlags::GTA_ALL_SHAPETEST_TYPES | ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES);
#endif // BREAKABLE_GLASS_USE_BVH
	}
}

void CBreakable::InitShardInst(phInst& pShardInst)
{
	Assert(!pShardInst.IsInLevel());
	pShardInst.GetArchetype()->SetTypeFlags(ArchetypeFlags::GTA_GLASS_TYPE);
	pShardInst.GetArchetype()->SetIncludeFlags(ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES);
	
	// To reduce glass breaking spikes, don't allow shards to collide other shards or the pane. 
	// Shard vs. Shard collision on the ground doesn't look that great anyways since they're small and thin. 
	pShardInst.GetArchetype()->RemoveIncludeFlags(ArchetypeFlags::GTA_GLASS_TYPE);

	// Also don't allow camera shapetests to collide with shards (in particular, in first person mode, we perform a capsule test 
	// including glass to compute a fallback camera position, if the player is in front of a window and the shards get flung towards the player
	// this causes the camera to hitch). If for some reason we do want the camera shapetests to include shards, this may need to be removed and
	// culled on a per case basis
	pShardInst.GetArchetype()->RemoveIncludeFlags(ArchetypeFlags::GTA_CAMERA_TEST);

	static phMaterialMgr::Id s_GlassMaterial = phMaterialMgr::DEFAULT_MATERIAL_ID;

	if (s_GlassMaterial == phMaterialMgr::DEFAULT_MATERIAL_ID)
	{
		s_GlassMaterial = PGTAMATERIALMGR->g_idGlassShootThrough;
	}

	pShardInst.GetArchetype()->GetBound()->SetMaterial(s_GlassMaterial);
}

bool CBreakable::AllowBreaking()
{
	return CObject::GetPool()->GetNoOfFreeSpaces() > 0;
}

// if lods available, then setup appropriate values based on distance to camera
u32 CBreakable::SelectVehicleLodLevel(const rmcDrawable& toDraw, float dist, float lodMult, bool forceHiLOD){

	u32 retLod = 0;

	// no lods to do anything with
	if (!toDraw.GetLodGroup().ContainsLod(1))
		return retLod;

	float unscaledLowLod = toDraw.GetLodGroup().GetLodThresh(1);
    float unscaledLowerLod = toDraw.GetLodGroup().GetLodThresh(2);
	if (m_bIsWreckedVehicle)
	{
		if (unscaledLowLod < 70.f)
			unscaledLowLod = 70.f;
		if (unscaledLowerLod < 70.f)
			unscaledLowerLod = 70.f;
	}

	float scaledHighLODDistance = toDraw.GetLodGroup().GetLodThresh(0)*g_LodScale.GetGlobalScaleForRenderThread()*lodMult;
	float scaledLowLODDistance = unscaledLowLod*g_LodScale.GetGlobalScaleForRenderThread()*lodMult*m_lowLodMult;
    float scaledLowerLODDistance = unscaledLowerLod * g_LodScale.GetGlobalScaleForRenderThread() * lodMult * m_lowLodMult;

	retLod = 2;			// low LOD
	if (forceHiLOD)
	{
		retLod = 0;
	}
	else if (dist < scaledHighLODDistance)
	{
		retLod = 0;		// high LOD
	}
	else if (dist < scaledLowLODDistance)
	{
		retLod = 1;		// med LOD
	}
	else if (dist > scaledLowerLODDistance)
	{
        retLod = 3;     // lower lod
	}

	if (LODDrawable::IsRenderingLowestCascade() && m_modelType == MI_TYPE_VEHICLE)
	{
		// set to lowest lod		
		retLod = 3;
	}

	// for mods we bump the lod index by one, unless we render the HD assets.
	// this is because we want the first lod for the mods to behave like the HD assets but without
	// being streamed in separately.
	if (m_bIsVehicleMod && retLod < (LOD_COUNT-1))
	{
		retLod++;
		if (m_bIsHd)
		{
			bool bAllowHD = true;
			gDrawListMgr->AdjustVehicleHD(true, bAllowHD);
			if (bAllowHD)
			{
				retLod--;
			}
		}
	}

	// adjust LOD based on current renderphase settings etc.
	Assert(m_modelType != -1);
	gDrawListMgr->AdjustFragmentLOD(true, retLod, m_modelType == MI_TYPE_VEHICLE);

	// if a clamped lod is set make sure we don't render a higher detailed version
	while ((s8)m_clampedLod > -1 && retLod < m_clampedLod && toDraw.GetLodGroup().ContainsLod(retLod + 1))
	{
		retLod++;
	}

	// validate LOD and select last one if possible, but not for mods when L3 is used.
    // we don't care about mods not rendering at that distance
	while ((!m_bIsVehicleMod || retLod <= 3) && retLod && !toDraw.GetLodGroup().ContainsLod(retLod))
	{
		retLod--;
	}

#if RSG_PC
	// force LOD3 for Kosatka's water reflection: BS#6811365 - PC: Kosatka - Flashing lights coming from the middle of Kosatka
	const bool isWaterReflectionDrawList = DRAWLISTMGR->IsExecutingWaterReflectionDrawList();
	if(isWaterReflectionDrawList)
	{
		if(m_modelInfo)
		{
			const u32 modelNameHash = m_modelInfo->GetModelNameHash();
			if((modelNameHash == MID_KOSATKA) || (modelNameHash == MID_LONGFIN))
			{
				retLod = 3;
			}
		}
	}
#endif //RSG_PC...

	Assert(retLod < LOD_COUNT);
	return(retLod);
}

//
// name:		CBreakable::AddToDrawBucket
// description:	AddToDrawBucket function used by fragment code
bool CBreakable::AddToDrawBucketFragCallback(const rmcDrawable& toDraw, const Matrix34& matrix, const crSkeleton* UNUSED_PARAM(skeleton),	grmMatrixSet* sharedMatrixset, float dist)
{
	Assert(g_pBreakable);
	return g_pBreakable->AddToDrawBucket(toDraw, matrix, (grmMatrixSet*)NULL, sharedMatrixset, dist, 0, 1.f, false);
}

#if __BANK
extern DECLARE_MTR_THREAD bool gIsDrawingVehicle;
extern DECLARE_MTR_THREAD bool gIsDrawingHDVehicle;

void DrawVehicleLODLevel(const Matrix34& matrix, s32 lodLevel)
{
    if (!DRAWLISTMGR->IsExecutingGBufDrawList())
        return;

	if (gIsDrawingVehicle){
		char LODLevel[4] = "X";
		if (gIsDrawingHDVehicle){
			LODLevel[0] = '0';
		} else if (lodLevel == 0){
			LODLevel[0] = '1';
		} else if (lodLevel == 1){
			LODLevel[0] = '2';
		} else if (lodLevel == 2){
			LODLevel[0] = '3';
		} else if (lodLevel == 3){
			LODLevel[0] = '4';
		}
		grcDebugDraw::Text(matrix.d + Vector3(0.f,0.f,2.5f), CRGBA(255, 255, 128, 255), LODLevel);

		gIsDrawingVehicle = false;
		gIsDrawingHDVehicle = false;
	}	
}
#else //__BANK
void DrawVehicleLODLevel(const Matrix34&, s32) {}
#endif //__BANK

bool CBreakable::AddToDrawBucket(const rmcDrawable& toDraw, const Matrix34& matrix, const grmMatrixSet* matrixset,	grmMatrixSet* sharedMatrixset, float dist, u16 drawableStats, float lodMult, bool forceHiLOD)
{
	const eRenderMode	renderMode = m_renderMode;
	Assert(renderMode != rmNIL);

#if HAS_RENDER_MODE_SHADOWS
	const bool forceShader = (grmModel::GetForceShader() != NULL);
	const bool renderShadows = forceShader && IsRenderingShadows(renderMode);
	const bool renderShadowsSkinned = forceShader && renderShadows && IsRenderingShadowsSkinned(renderMode);
#endif // HAS_RENDER_MODE_SHADOWS

	u32 LODLevel = 0;
	s32 crossFade = -1;

	const bool isShadowDrawList = DRAWLISTMGR->IsExecutingShadowDrawList();
	const bool isGBufferDrawList = DRAWLISTMGR->IsExecutingGBufDrawList();
	const bool isHeightmapDrawList = DRAWLISTMGR->IsExecutingRainCollisionMapDrawList();

	const bool bDontWriteDepth = false;
	const bool setAlpha = IsNotRenderingModes(renderMode, rmSimpleNoFade|rmSimple) && !isGBufferDrawList;
	const bool setStipple = isGBufferDrawList;

	if (m_modelType == MI_TYPE_VEHICLE)
	{
		LODLevel = SelectVehicleLodLevel(toDraw, dist, lodMult, forceHiLOD);
	}
	else if( !forceHiLOD )
	{
		// it's a damaged drawable, so there's a chance that the last LOD idx will not match, so we need to fix it up first
		const rmcLodGroup &lodGroup = toDraw.GetLodGroup();
		if(!lodGroup.ContainsLod(m_lastLODIdx))
		{ 
			for (int i = LOD_COUNT - 1; i > 0; i--)
			{
				if (lodGroup.ContainsLod(i))
				{
					m_lastLODIdx = i;
					break;
				}
			}
		}
		
		u32 LODFlag = (m_phaseLODs >> DRAWLISTMGR->GetDrawListLODFlagShift()) & LODDrawable::LODFLAG_NONE;

		if (LODFlag != LODDrawable::LODFLAG_NONE)
		{
			if (LODFlag & LODDrawable::LODFLAG_FORCE_LOD_LEVEL)
			{
				LODLevel = Min(LODFlag & ~LODDrawable::LODFLAG_FORCE_LOD_LEVEL,m_lastLODIdx);
			}
			else
			{
				LODDrawable::CalcLODLevelAndCrossfadeForProxyLod(&toDraw, dist, LODFlag, LODLevel, crossFade, LODDrawable::CFDIST_MAIN, m_lastLODIdx);
			}
		}
		else
		{
			LODDrawable::CalcLODLevelAndCrossfade(&toDraw, dist, LODLevel, crossFade, LODDrawable::CFDIST_MAIN, m_lastLODIdx ASSERT_ONLY(, m_modelInfo));
		}

		if ((LODFlag != LODDrawable::LODFLAG_NONE || isShadowDrawList || isHeightmapDrawList) && crossFade > -1)
		{
			if (crossFade < 128 && LODLevel+1 < LOD_COUNT && toDraw.GetLodGroup().ContainsLod(LODLevel+1))
			{
				LODLevel = LODLevel+1;
			}

			crossFade = -1;
		}
		else if (m_entityAlpha < 255) // Entity + cross fade fade: we can't deal with this, nuke crossFade
		{
			crossFade = -1;
		}
	}
	
	// lods may not exist
	if(!toDraw.GetLodGroup().ContainsLod(LODLevel))
		return false;

	s32 bucketMask = m_drawBucketMask;
	if( forceHiLOD )
	{ // We're forcing HiLOD, so the bucket mask might not match anymore (for proxyed drawables.), so we fix it. LoL.
		u32 baseBucket = CRenderer::GetBaseBucket(bucketMask);
		bucketMask = CRenderer::PackBucketMask(baseBucket,0xff00);
	}
		
	const grmMatrixSet* pMatrixSetParam = NULL;

	if(matrixset)
	{
		pMatrixSetParam = matrixset;
	}
	else if(sharedMatrixset)
	{
		pMatrixSetParam = sharedMatrixset;
	}

	if (pMatrixSetParam)
	{
#if HAS_RENDER_MODE_SHADOWS
		if (renderShadows)
		{
			if( renderShadowsSkinned )
			{
			#if !RAGE_SUPPORT_TESSELLATION_TECHNIQUES
				toDraw.DrawSkinnedNoShaders(matrix, *pMatrixSetParam,bucketMask,LODLevel,rmcDrawable::RENDER_SKINNED);
			#else
				toDraw.DrawSkinnedNoShadersTessellationControlled(matrix, *pMatrixSetParam,bucketMask,LODLevel,rmcDrawable::RENDER_SKINNED);
			#endif
			}
			else
			{
			#if !RAGE_SUPPORT_TESSELLATION_TECHNIQUES
				toDraw.DrawSkinnedNoShaders(matrix, *pMatrixSetParam,bucketMask,LODLevel,rmcDrawable::RENDER_NONSKINNED);
			#else
				toDraw.DrawSkinnedNoShadersTessellationControlled(matrix, *pMatrixSetParam,bucketMask,LODLevel,rmcDrawable::RENDER_NONSKINNED);
			#endif
			}
		}
		else
#endif // HAS_RENDER_MODE_SHADOWS
		{
			// check if crossfading - need to render twice if so.
			if (m_modelType != MI_TYPE_VEHICLE)
			{
				if (IsRenderingModes(renderMode, rmStandard|rmGBuffer) && crossFade > -1)
				{
					Assert(LODLevel+1 < LOD_COUNT && toDraw.GetLodGroup().ContainsLod(LODLevel+1));
					Assert(m_entityAlpha == 255);

					u32 fadeAlphaN1 = crossFade;
					u32 fadeAlphaN2 = 255 - crossFade;

#if !RSG_ORBIS && !RSG_DURANGO
					s32 previousTechnique = -1;
					bool techOverride = CShaderLib::UsesStippleFades() && DRAWLISTMGR->IsExecutingGBufDrawList();
					if (techOverride)
					{
						// On DX11 we can't use the D3DRS_MULTISAMPLEMASK for fade withough MSAA (as on 360) so for the fade pass.
						// It needs to be manually inserted into the shader itself. For DX11, as we don't want to add it to 
						// all shaders, it's been added to the alphaclip passes and we force the technique here.
						previousTechnique = grmShaderFx::GetForcedTechniqueGroupId();
						if (previousTechnique == DeferredLighting::GetSSATechniqueGroup())
						{
							grmShaderFx::SetForcedTechniqueGroupId(DeferredLighting::GetSSAAlphaClipTechniqueGroup());
						}
						else
						{
							grmShaderFx::SetForcedTechniqueGroupId(DeferredLighting::GetAlphaClipTechniqueGroup());
						}
					}
#endif // !RSG_ORBIS

					if (fadeAlphaN1 > 0)
					{
						CShaderLib::SetScreenDoorAlpha(setAlpha, fadeAlphaN1, setStipple, false, fadeAlphaN1, bDontWriteDepth);
						toDraw.DrawSkinned(matrix, *pMatrixSetParam, bucketMask, LODLevel, drawableStats);
					}

					if (fadeAlphaN2 > 0)
					{
						CShaderLib::SetScreenDoorAlpha(setAlpha, fadeAlphaN2, setStipple, true, fadeAlphaN1, bDontWriteDepth);
						toDraw.DrawSkinned(matrix, *pMatrixSetParam, bucketMask, LODLevel+1, drawableStats);
					}	

#if !RSG_ORBIS && !RSG_DURANGO
					if (techOverride)
					{
						grmShaderFx::SetForcedTechniqueGroupId(previousTechnique);
					}
#endif // !RSG_ORBIS
					
					CShaderLib::ResetAlpha(setAlpha,setStipple);
					
				} else {
					toDraw.DrawSkinned(matrix, *pMatrixSetParam, bucketMask, LODLevel, drawableStats);
				}
			}
			else {
				DrawVehicleLODLevel(matrix, LODLevel);
				toDraw.DrawSkinned(matrix, *pMatrixSetParam, bucketMask, LODLevel, drawableStats);
			}
		}
	}
	else
	{
#if HAS_RENDER_MODE_SHADOWS
		if (renderShadows)
		{
			if (false == renderShadowsSkinned)
			{
			#if !RAGE_SUPPORT_TESSELLATION_TECHNIQUES
				toDraw.DrawNoShaders(matrix, bucketMask, LODLevel);
			#else
				toDraw.DrawNoShadersTessellationControlled(matrix, bucketMask, LODLevel);
			#endif
			}
		}
		else
#endif // HAS_RENDER_MODE_SHADOWS
		{
			DrawVehicleLODLevel(matrix, LODLevel);
			toDraw.Draw(matrix, bucketMask, LODLevel, drawableStats);
		}
	}

	return true;
}




