//
// name:		DynamicEntity.cpp
// description:	Class describing an entity that is not static
//

// Rage headers
#include "breakableglass/breakable.h"
#include "creature/creature.h"
#include "creature/componentskeleton.h"
#include "creature/componentblendshapes.h"
#include "creature/componentextradofs.h"
#include "creature/componentshadervars.h"
#include "cranimation/frame.h"
#include "cranimation/animtrack.h"
#include "crskeleton/skeleton.h"
#include "crskeleton/skeletondata.h"
#include "grblendshapes/manager.h"
#include "grmodel/matrixset.h"
#include "fragment/cache.h"
#include "cloth/clothcontroller.h"
#include "cloth/charactercloth.h"
#include "profile/cputrace.h"
#include "profile/element.h"
#include "profile/page.h"
#include "profile/group.h"
#include "spatialdata/sphere.h"

// Framework headers
#include "fwanimation/animdirector.h"
#include "fwanimation/directorcomponentcreature.h"
#include "fwanimation\expressionsets.h"
#include "fwutil/PtrList.h"
#include "fwscene/stores/blendshapestore.h"
#include "fwsys/metadatastore.h"
#include "fwscene/world/WorldLimits.h"
#include "fwscene/world/WorldRepMulti.h"
#include "fwsys/metadatastore.h"

// Game headers
#include "animation/CreatureMetadata.h"
#include "animation/Move.h"
#include "animation/MovePed.h"
#include "animation/debug/AnimViewer.h"
#include "camera/CamInterface.h"
#include "cutscene/CutSceneManagerNew.h"
#include "debug/debugscene.h"
#include "game/config.h"
#include "pathserver/PathServer.h"
#include "physics/breakable.h"
#include "physics/GtaInst.h"
#include "physics/physics.h"
#include "renderer/Entities/DynamicEntityDrawHandler.h"
#include "renderer/renderer.h"
#include "scene/DynamicEntity.h"
#include "scene/Physical.h"
#include "scene/portals/portalTracker.h"
#include "scene/world/gameWorld.h"
#include "script/script.h"
#include "script/gta_thread.h"
#include "script/script_channel.h"
#include "script/Handlers/GameScriptEntity.h"
#include "shaders/ShaderLib.h"
#include "shaders/CustomShaderEffectPed.h"
#include "streaming/streaming.h"
#include "system/TaskScheduler.h"
#include "system/ThreadPriorities.h"
#include "TimeCycle/TimeCycleAsyncQueryMgr.h"
#include "vehicles/vehicle.h"
// TEMP
#include "peds/ped.h"
#include "modelinfo/PedModelInfo.h"
#include "Peds/rendering/pedVariationPack.h"
#include "peds/rendering/PedVariationStream.h"

// #include "system/findsize.h"
//FindSize(CPortalTracker); // 16
#if __DEV
#if RSG_PC
CompileTimeAssertSize(CPortalTracker, 160, 160); // Larger due to the obfuscated position
#else //RSG_PC
CompileTimeAssertSize(CPortalTracker, 112, 112);
#endif //RSG_PC
#endif // __DEV

ANIM_OPTIMISATIONS()
SCENE_OPTIMISATIONS()

#define FORCE_DRAW_DYNAMICBOUNDS 0

#define OBJECT_PROXIMITY_FOR_ANIM_UPDATE (30.0f) // 30 metres.

u8 CDynamicEntity::sm_FrameCountForVisibility = 0;
bool CDynamicEntity::sm_CanUseCachedVisibilityThisFrame = true;

//
// name:		CDynamicEntity::CDynamicEntity
// description:	constructor
CDynamicEntity::CDynamicEntity(const eEntityOwnedBy ownedBy)
: CEntity( ownedBy )
{
	fwEntity::SetBaseFlag( fwEntity::IS_DYNAMIC );


	// flags init
	m_nDEflags.bForcePrePhysicsAnimUpdate = false;
	m_nDEflags.bIsBreakableGlass = false;
	m_nDEflags.bCheckedForDead = false;
	m_nDEflags.bIsGolfBall = false;
	m_nDEflags.bFrozenByInterior = false;
	m_nDEflags.bFrozen = false;
	m_nDEflags.nStatus = STATUS_ABANDONED;
	m_nDEflags.nPopType = POPTYPE_UNKNOWN;
	m_nDEflags.bIsOutOfMap = false;
	m_nDEflags.bOverridePhysicsBounds = false;
	m_nDEflags.bHasMovedSinceLastPreRender = true;
	m_nDEflags.bUseExtendedBoundingBox = false;
	m_nDEflags.bIsStraddlingPortal = false;
	REPLAY_ONLY(m_nDEflags.bReplayWarpedThisFrame = false;)

	m_portalTracker.SetOwner(this);
	m_portalTracker.SetLoadsCollisions(false);

	SetIsVisibleInSomeViewportThisFrame(false);
	
	m_randomSeed = (u16)(fwRandom::GetRandomNumber());		// middle bits tend to be more random when using crappy C rand function
	m_portalStraddlingContainerIndex = 0xFF;
}

//
// name:		CDynamicEntity::~CDynamicEntity
// description:	destructor
CDynamicEntity::~CDynamicEntity()
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnDestroyOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfDestroyCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	ASSERT_ONLY(CScriptEntityExtension* pExtension = GetExtension<CScriptEntityExtension>());

	aiFatalAssertf(!(pExtension && pExtension->GetScriptHandler()), "A script entity is being destroyed without the script handler being informed");

	if (IsStraddlingPortal())
	{
		RemoveFromStraddlingPortalsContainer();
	}

	DeleteDrawable();

	// better not be in an interior when we trash the portal tracker - otherwise the room
	// will contain a ref to the deleted portal tracker
	Assert(!m_nFlags.bInMloRoom);

	// Since the scene update system owns the scene udpate extension, we need
	// to give it a chance to reclaim it.
	fwSceneUpdate::RemoveFromSceneUpdate(*this, ~0U, true);
}

//
// name:		CDynamicEntity::Add
// description:	Add to world, while storing pointers to all the ptrnodes generated
void CDynamicEntity::Add()
{
	gWorldMgr.AddToWorld(this,GetBoundRect());
}

//
// name:		CDynamicEntity::Remove
// description:	Remove entity from world
void CDynamicEntity::Remove()
{
	gWorldMgr.RemoveFromWorld(this);
}

// name:		CDynamicEntity::AddToInterior
// description:	add entity to the active object list for an interior (which is set in entity, in advance of this call)
void CDynamicEntity::AddToInterior(fwInteriorLocation interiorLocation){

	Assert(!GetIsInExterior());		// make sure not in exterior already

	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfAddToInteriorCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	CInteriorInst* pIntInst = CInteriorInst::GetInteriorForLocation( interiorLocation );

	if (pIntInst){
		pIntInst->AddObjectInstance(this, interiorLocation);
		Assert(m_nFlags.bInMloRoom == true);
		return;		// success
	}

	// failure
	Assertf(false,"Can't add dynamicEntity to interior : there is no CInteriorInst created for it. Dumping entity...");
	CDebug::DumpEntity(this);
}

// name:		CDynamicEntity::RemoveFromInterior
// description:	remove entity from the interior which it is in
void CDynamicEntity::RemoveFromInterior(){

	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfRemoveFromInteriorCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );
	Assert(m_nFlags.bInMloRoom);

	CInteriorInst* pIntInst = CInteriorInst::GetInteriorForLocation( this->GetInteriorLocation() );
	Assertf(pIntInst, "Cannot remove object from interior - interior isn't valid");
	if (pIntInst){
		pIntInst->RemoveObjectInstance(this);
	}
	else
	{
		Assertf(false,"Trying to remove entity from interior with invalid interior id. Dumping entity...");
		CDebug::DumpEntity(this);	
	}

	m_portalTracker.m_roomIdx = 0;
	m_portalTracker.m_pInteriorProxy = NULL;
}

fwExpressionSet* CDynamicEntity::GetExpressionSet()
{
	CBaseModelInfo* pModelInfo = GetBaseModelInfo();

	if (pModelInfo && (pModelInfo->GetExpressionSet() != EXPRESSION_SET_ID_INVALID))
	{
		return fwExpressionSetManager::GetExpressionSet(pModelInfo->GetExpressionSet());
	}

	return NULL;
}

//
// name:		GetBoundRadius
// description:	Return the radius of the bounding sphere
float CDynamicEntity::GetBoundRadius() const
{
	phInst* pInst = GetFragInst();
	if(pInst==NULL)
		pInst = GetCurrentPhysicsInst();

	if(pInst && !m_nDEflags.bOverridePhysicsBounds)
		return pInst->GetArchetype()->GetBound()->GetRadiusAroundCentroid() + GetForceAddToBoundRadius();

	return GetBaseModelInfo()->GetBoundingSphereRadius() + GetForceAddToBoundRadius();
}

//
// name:		CDynamicEntity::GetBoundCentre
// description:	Return bound centre in world space
Vector3 CDynamicEntity::GetBoundCentre() const
{
	phInst* pInst = GetFragInst();
	if(pInst==NULL)
		pInst = GetCurrentPhysicsInst();

	if(pInst && !m_nDEflags.bOverridePhysicsBounds)
	{
		Vector3 output;
		RCC_MATRIX34(pInst->GetMatrix()).Transform(VEC3V_TO_VECTOR3(pInst->GetArchetype()->GetBound()->GetCentroidOffset()), output);
		return output;
	}
	else
	{
		return CEntity::GetBoundCentre();
	}
}

//
// name:		CDynamicEntity::GetBoundCentre
// description:	Return bound centre in world space
void CDynamicEntity::GetBoundCentre(Vector3& centre) const
{
	phInst* pInst = GetFragInst();
	if(pInst==NULL)
		pInst = GetCurrentPhysicsInst();

	if(pInst && !m_nDEflags.bOverridePhysicsBounds)
	{
		RCC_MATRIX34(pInst->GetMatrix()).Transform(VEC3V_TO_VECTOR3(pInst->GetArchetype()->GetBound()->GetCentroidOffset()), centre);
	}
	else
	{
		CEntity::GetBoundCentre(centre);
	}
}

float CDynamicEntity::GetBoundCentreAndRadius(Vector3& centre) const
{
	phInst* pInst = GetFragInst();
	phBound* pBound = NULL;
	if(pInst==NULL)
		pInst = GetCurrentPhysicsInst();

	if(pInst && !m_nDEflags.bOverridePhysicsBounds)// && m_nFlags.bLightObject==false)
	{
		pBound = pInst->GetArchetype()->GetBound();

		RCC_MATRIX34(pInst->GetMatrix()).Transform(VEC3V_TO_VECTOR3(pInst->GetArchetype()->GetBound()->GetCentroidOffset()), centre);
		
		return pBound->GetRadiusAroundCentroid();
	}
	else
	{	
		return CEntity::GetBoundCentreAndRadius(centre);
	}
}

float CDynamicEntity::GetExtendedBoundCentreAndRadius(Vector3& centre) const
{
	phInst* pInst = GetFragInst();
	if(pInst==NULL)
		pInst = GetCurrentPhysicsInst();

	spdSphere	sphere;

	if(pInst && !m_nDEflags.bOverridePhysicsBounds)
	{
		// [SPHERE-OPTIMISE]
		phBound* pBound = pInst->GetArchetype()->GetBound();
		sphere.Set(pBound->GetCentroidOffset(), pBound->GetRadiusAroundCentroidV());

		if( m_nDEflags.bIsBreakableGlass )
		{
			fragInst *pFragInst = (fragInst*)pInst;

			if(fragCacheEntry* pCacheEntry = pFragInst->GetCacheEntry())
			{
				if(fragHierarchyInst* pHierInst = pCacheEntry->GetHierInst())
				{
					for(int glassIndex = 0; glassIndex != pHierInst->numGlassInfos; ++ glassIndex)
					{
						bgGlassHandle handle = pHierInst->glassInfos[glassIndex].handle;
						if(handle != bgGlassHandle_Invalid)						
						{
							ScalarV inflatedRadius = sphere.InflateSphere(spdSphere(VECTOR4_TO_VEC4V(bgGlassManager::GetGlassBreakable(handle).GetBoundingSphere())));
							sphere.SetRadius(inflatedRadius);
						}
					}
				}
			}
		}

		const Vec3V		transformedCenter = Transform( pInst->GetMatrix(), sphere.GetCenter() );
		sphere.Set( transformedCenter, sphere.GetRadius() );
	}
	else
	{
		const float		radius = CEntity::GetBoundCentreAndRadius(centre);
		sphere.Set( VECTOR3_TO_VEC3V(centre), ScalarV(radius) );
	}

	// GtaV B*128062: Some animations cause the entity bounds to be way off the entity position. In these cases,
	// even though the entity (position) is clearly in view, it's culled in the visibility pipeline and its
	// skeleton is not updated. So we need to enlarge the bounds to include the entity position. - AP
	centre = VEC3V_TO_VECTOR3( sphere.GetCenter() );
	return (sphere.GetRadius() + Mag(sphere.GetCenter() - this->GetTransform().GetPosition())).Getf();
}

//
// name:		CDynamicEntity::GetBoundingBoxMin
// description:	Return bounding box min
const Vector3& CDynamicEntity::GetBoundingBoxMin() const
{
	phInst* pInst = GetFragInst();
	if(pInst==NULL)
	{
		pInst = GetCurrentPhysicsInst();
	}

	if(pInst && !m_nDEflags.bOverridePhysicsBounds)
	{
		const phArchetype* pArchetype = pInst->GetArchetype();
		if( AssertVerify(pArchetype) )
		{
			const phBound* pBound = pArchetype->GetBound();
			if( AssertVerify(pBound) )
			{
				return RCC_VECTOR3(pBound->GetBoundingBoxMin());
			}
		}
	}
	
	return CEntity::GetBoundingBoxMin();
}

//
// name:		CDynamicEntity::GetBoundingBoxMax
// description:	Return bounding box max
const Vector3& CDynamicEntity::GetBoundingBoxMax() const
{
	phInst* pInst = GetFragInst();
	if(pInst==NULL)
	{
		pInst = GetCurrentPhysicsInst();
	}

	if(pInst && !m_nDEflags.bOverridePhysicsBounds)
	{
		const phArchetype* pArchetype = pInst->GetArchetype();
		if( AssertVerify(pArchetype) )
		{
			const phBound* pBound = pArchetype->GetBound();
			if( AssertVerify(pBound) )
			{
				return RCC_VECTOR3(pBound->GetBoundingBoxMax());
			}
		}
	}

	return CEntity::GetBoundingBoxMax();
}

__forceinline phInst* CDynamicEntity::GetBoundBoxInline(spdAABB& box) const // private non-virtual inline implementation
{
	Vec3V bbmin, bbmax;
	bool bAddClothBox = false;

	phInst* pInst = GetFragInst();
	if(pInst==NULL)
	{
		pInst = GetCurrentPhysicsInst();
	}
#if !USE_CLOTH_PHINST
	else
	{
		const fragCacheEntry* pCacheEntry = ((fragInst*)pInst)->GetCacheEntry();
		if( pCacheEntry  )
		{
			const fragHierarchyInst* pHierInst = pCacheEntry->GetHierInst();
			const bool hasCloth = ( pHierInst && (0 != pHierInst->envCloth)) ? true: false;
			if( hasCloth )
			{
				environmentCloth* pEnvCloth = pHierInst->envCloth;
				Assert( pEnvCloth );
				Assert( pEnvCloth->GetBehavior() );
				if( !pEnvCloth->GetBehavior()->IsMotionSeparated())
				{
					bbmin = pEnvCloth->GetBBMinWorldSpace();
					bbmax = pEnvCloth->GetBBMaxWorldSpace();
					bAddClothBox = true;
				}
			}
		}
	}
#endif

	if (pInst && !m_nDEflags.bOverridePhysicsBounds)// && m_nFlags.bLightObject==false)
	{
		box.Set(
			pInst->GetArchetype()->GetBound()->GetBoundingBoxMin(),
			pInst->GetArchetype()->GetBound()->GetBoundingBoxMax());
	}
	else
	{
		box = GetBaseModelInfo()->GetBoundingBox();
	}

	box.Transform(GetMatrix());

	if( bAddClothBox )
	{
		Vec3V minV = box.GetMin();
		Vec3V maxV = box.GetMax();
		VecBoolV nonZeroIfNewMin = IsLessThan( bbmin, minV );
		VecBoolV nonZeroIfNewMax = IsGreaterThan( bbmax, maxV );
		
		box.SetMin( SelectFT( nonZeroIfNewMin, minV, bbmin ) );
		box.SetMax( SelectFT( nonZeroIfNewMax, maxV, bbmax ) );
	}

	return pInst;
}

FASTRETURNCHECK(const spdAABB &) CDynamicEntity::GetBoundBox(spdAABB& box) const
{
	GetBoundBoxInline(box);
	return box;
}

FASTRETURNCHECK(const spdAABB &) CDynamicEntity::GetLocalSpaceBoundBox(spdAABB& box) const
{
	phInst* pInst = GetFragInst();
	if(pInst==NULL)
		pInst = GetCurrentPhysicsInst();

	if (pInst && !m_nDEflags.bOverridePhysicsBounds)
	{
		box.Set(
			pInst->GetArchetype()->GetBound()->GetBoundingBoxMin(),
			pInst->GetArchetype()->GetBound()->GetBoundingBoxMax());
	}
	else
	{
		box = GetBaseModelInfo()->GetBoundingBox();
	}

	return box;
}

void CDynamicEntity::GetExtendedBoundBox(spdAABB& box) const
{
	phInst* pInst = GetBoundBoxInline(box);

#if FORCE_DRAW_DYNAMICBOUNDS
	grcDebugDraw::BoxAxisAligned(box.GetMin(),box.GetMax(),Color32(0x00,0xff,0),false);
#endif // FORCE_DRAW_DYNAMICBOUNDS

	if( m_nDEflags.bIsBreakableGlass && pInst!=NULL)
	{
		fragInst *pFragInst = (fragInst*)pInst;
	  			
		if(fragCacheEntry* pCacheEntry = pFragInst->GetCacheEntry())
		{
			if(fragHierarchyInst* pHierInst = pCacheEntry->GetHierInst())
			{
				for(int glassIndex = 0; glassIndex != pHierInst->numGlassInfos; ++ glassIndex)
				{
					bgGlassHandle handle = pHierInst->glassInfos[glassIndex].handle;
					if(handle != bgGlassHandle_Invalid)						
					{
						// The alpha update will happen on the centroid of the box, even if the box is huge
						// To counter this, we double extend the box so that its centre stays where it was
						// while it still contains all the glass shard.
		
						spdAABB glassBoxA;
						spdAABB glassBoxB;
						Vec3V bbMin, bbMax;
						const bgBreakable &breakable = bgGlassManager::GetGlassBreakable(handle);
						
						breakable.GetBoundingBox(RC_VECTOR3(bbMin), RC_VECTOR3(bbMax));

						Assertf((bbMin.GetXf() > WORLDLIMITS_XMIN) && (bbMin.GetYf() > WORLDLIMITS_YMIN) && (bbMin.GetZf() > WORLDLIMITS_ZMIN), "[0] Entity: %s %s - X= %5.3f : Y= %5.3f : Z= %5.3f", GetModelName(), PopTypeIsMission()? "(mission owned)":"", bbMin.GetXf(), bbMin.GetYf(), bbMin.GetZf());
						Assertf((bbMax.GetXf() < WORLDLIMITS_XMAX) && (bbMax.GetYf() < WORLDLIMITS_YMAX) && (bbMax.GetZf() < WORLDLIMITS_ZMAX), "[1] Entity: %s %s - X= %5.3f : Y= %5.3f : Z= %5.3f", GetModelName(), PopTypeIsMission()? "(mission owned)":"", bbMax.GetXf(), bbMax.GetYf(), bbMax.GetZf());

						glassBoxA.Set(bbMin, bbMax);
						glassBoxB.Set(-bbMax, -bbMin);

						Matrix34 breakableMatrix = breakable.GetTransform();
						Assertf((breakableMatrix.d.x < WORLDLIMITS_XMAX) && (breakableMatrix.d.y < WORLDLIMITS_YMAX) && (breakableMatrix.d.z < WORLDLIMITS_ZMAX), "[2] Entity: %s %s - Brekable matrix pos X= %5.3f : Y= %5.3f : Z= %5.3f", GetModelName(), PopTypeIsMission()? "(mission owned)":"", breakableMatrix.d.x, breakableMatrix.d.y, breakableMatrix.d.z);

						glassBoxA.Transform(RCC_MAT34V(breakableMatrix));
						glassBoxB.Transform(RCC_MAT34V(breakableMatrix));
						
#if FORCE_DRAW_DYNAMICBOUNDS
						grcDebugDraw::BoxAxisAligned(glassBoxA.GetMin(),glassBoxA.GetMax(),Color32(0xff,0,0),false);
						grcDebugDraw::BoxAxisAligned(glassBoxB.GetMin(),glassBoxB.GetMax(),Color32(0,0,0xff),false);
#endif // FORCE_DRAW_DYNAMICBOUNDS

						box.GrowAABB(glassBoxA);
						box.GrowAABB(glassBoxB);

						Assertf((box.GetMin().GetXf() > WORLDLIMITS_XMIN) && (box.GetMin().GetYf() > WORLDLIMITS_YMIN) && (box.GetMin().GetZf() > WORLDLIMITS_ZMIN), "[5] Entity: %s %s - X= %5.3f : Y= %5.3f : Z= %5.3f", GetModelName(), PopTypeIsMission()? "(mission owned)":"", box.GetMin().GetXf(), box.GetMin().GetYf(), box.GetMin().GetZf());
						Assertf((box.GetMax().GetXf() < WORLDLIMITS_XMAX) && (box.GetMax().GetYf() < WORLDLIMITS_YMAX) && (box.GetMax().GetZf() < WORLDLIMITS_ZMAX), "[6] Entity: %s %s - X= %5.3f : Y= %5.3f : Z= %5.3f", GetModelName(), PopTypeIsMission()? "(mission owned)":"", box.GetMax().GetXf(), box.GetMax().GetYf(), box.GetMax().GetZf());
					}
				}
			}
		}
	}
	
	Assertf((box.GetMin().GetXf() > WORLDLIMITS_XMIN) && (box.GetMin().GetYf() > WORLDLIMITS_YMIN) && (box.GetMin().GetZf() > WORLDLIMITS_ZMIN), "[7] Entity: %s %s - X= %5.3f : Y= %5.3f : Z= %5.3f", GetModelName(), PopTypeIsMission()? "(mission owned)":"", box.GetMin().GetXf(), box.GetMin().GetYf(), box.GetMin().GetZf());
	Assertf((box.GetMax().GetXf() < WORLDLIMITS_XMAX) && (box.GetMax().GetYf() < WORLDLIMITS_YMAX) && (box.GetMax().GetZf() < WORLDLIMITS_ZMAX), "[8] Entity: %s %s - X= %5.3f : Y= %5.3f : Z= %5.3f", GetModelName(), PopTypeIsMission()? "(mission owned)":"", box.GetMax().GetXf(), box.GetMax().GetYf(), box.GetMax().GetZf());

	// GtaV B*128062: Some animations cause the entity bounds to be way off the entity position. In these cases,
	// even though the entity (position) is clearly in view, it's culled in the visibility pipeline and its
	// skeleton is not updated. So we need to enlarge the bounds to include the entity position. - AP
	if ( GetAnimDirector() != NULL )
	{
		const Vec3V		entityPosition = this->GetTransform().GetPosition();
		const Vec3V		boxExtent = box.GetExtent();
		box.GrowPoint( entityPosition - boxExtent );
		box.GrowPoint( entityPosition + boxExtent );
	}

	// When running IK, the weapon may not always render since visibility scanning uses the pre-IK position for the weapon.
	// To workaround this, the bounding box for the weapon is stretched so that it never gets culled.
	if ( Unlikely(m_nDEflags.bUseExtendedBoundingBox) )
	{
		const Vec3V		entityPosition = this->GetTransform().GetPosition();
		Vec3V			boxExtent = box.GetExtent();
		TUNE_GROUP_FLOAT(EXTENDED_BOUND_BOX, EXTENT_HEIGHT_OFFSET, 1.0f, -2.0f, 2.0f, 0.01f);
		TUNE_GROUP_FLOAT(EXTENDED_BOUND_BOX, EXTENT_XY_OFFSET, 1.0f, -2.0f, 2.0f, 0.01f);
		const float fBoxHeight = boxExtent.GetZf();
		boxExtent.SetXf(fBoxHeight + EXTENT_XY_OFFSET);
		boxExtent.SetYf(fBoxHeight + EXTENT_XY_OFFSET);
		boxExtent.SetZf(fBoxHeight + EXTENT_HEIGHT_OFFSET);
		box.GrowPoint( entityPosition - boxExtent );
		box.GrowPoint( entityPosition + boxExtent );
	}

#if FORCE_DRAW_DYNAMICBOUNDS
	grcDebugDraw::BoxAxisAligned(box.GetMin(),box.GetMax(),Color32(0xffffffff),false);
#endif // FORCE_DRAW_DYNAMICBOUNDS	
}


//
// name:		CDynamicEntity::AddToMovingList
// description:	Add to list of objects to be processed
void CDynamicEntity::AddToSceneUpdate()
{
	Assertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE), "CDynamicEntity::AddToSceneUpdate call being made from outside the main thread!");

	// If we are updating through the animation queue, we don't want to set SU_UPDATE or SU_UPDATE_VEHICLE.
	u32 flagsToAdd = CGameWorld::SU_RESET_VISIBILITY;
	
	if(!GetUpdatingThroughAnimQueue())
	{
		flagsToAdd |= GetMainSceneUpdateFlag();
	}

	if(GetAnimDirector())
	{
		flagsToAdd |= GetStartAnimSceneUpdateFlag();
	}

	bool addPreSimPhysicsUpdate = true;
	if (GetType() == ENTITY_TYPE_PED)
	{
		flagsToAdd |= CGameWorld::SU_ADD_SHOCKING_EVENTS
					| CGameWorld::SU_UPDATE_PERCEPTION
					| CGameWorld::SU_UPDATE_PAUSED;

		CPed* pPed = static_cast<CPed*>(this);

		// Normally, we manage SU_PRESIM_PHYSICS_UPDATE for peds in CPed::ProcessPrePhysics(),
		// but when we insert in the scene it's probably best to not set that flag for low LOD
		// peds from the start.
		const bool bIsUsingLowLodPhysics = pPed->IsUsingLowLodPhysics();
		if(bIsUsingLowLodPhysics || pPed->GetPedResetFlag(CPED_RESET_FLAG_OverridePhysics))
		{
			addPreSimPhysicsUpdate = false;
		}
	}

	if (GetIsDynamic())
	{
		flagsToAdd |= CGameWorld::SU_START_ANIM_UPDATE_POST_CAMERA;
	}

	if (GetIsTypeVehicle() && ((CVehicle*)this)->GetVehicleType()==VEHICLE_TYPE_TRAIN)
	{
		flagsToAdd |= CGameWorld::SU_TRAIN;
	}

	if (GetIsPhysical())
	{
		if(addPreSimPhysicsUpdate)
		{
			flagsToAdd |= CGameWorld::SU_PRESIM_PHYSICS_UPDATE;
		}
		flagsToAdd	|=  CGameWorld::SU_PHYSICS_PRE_UPDATE
					|  CGameWorld::SU_PHYSICS_POST_UPDATE;
	}

	fwSceneUpdate::AddToSceneUpdate(*this, flagsToAdd);
}

//
// name:		CDynamicEntity::RemoveFromMovingList
// description:	Remove from list of objects to be processed
void CDynamicEntity::RemoveFromSceneUpdate()
{
	Assertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE), "CDynamicEntity::RemoveFromSceneUpdate call being made from outside the main thread!");

	fwSceneUpdate::RemoveFromSceneUpdate(*this, 0xffffffff);
}

//
// name:		CDynamicEntity::AddSceneUpdateFlags
// description:	Adds individual flags to the scene update flag list
void CDynamicEntity::AddSceneUpdateFlags(u32 flags)
{
	fwSceneUpdate::AddToSceneUpdate(*this, flags);
}

//
// name:		CDynamicEntity::RemoveSceneUpdateFlags
// description:	Removes individual flags from the scene update flag list
void CDynamicEntity::RemoveSceneUpdateFlags(u32 flags)
{
	fwSceneUpdate::RemoveFromSceneUpdate(*this, flags);
}

//
// name:		CDynamicEntity::GetIsMainSceneUpdateFlagSet
// description:	Returns whether the main scene update flag is set
bool CDynamicEntity::GetIsMainSceneUpdateFlagSet() const
{
    const fwSceneUpdateExtension *extension = GetExtension<fwSceneUpdateExtension>();

    if(extension && (extension->m_sceneUpdateFlags & GetMainSceneUpdateFlag()) != 0)
    {
        return true;
    }

    return false;
}

//
// name:		UpdateSkeleton
// description:	Update the skeleton for an entity
RAGETRACE_DECL(CDynamicEntity_UpdateSkeleton);
void CDynamicEntity::UpdateSkeleton()
{
	RAGETRACE_SCOPE(CDynamicEntity_UpdateSkeleton);

	WaitForAnyActiveAnimUpdateToComplete();

	if(GetSkeleton())
	{
		GetSkeleton()->Update();

		fragInst* pFragInst = GetFragInst();
		if (pFragInst && pFragInst->GetCached() && (!GetIsTypePed() || CPhysics::GetLevel()->IsActive(pFragInst->GetLevelIndex())))
		{
			fragHierarchyInst* hierInst = pFragInst->GetCacheEntry()->GetHierInst();

			// if fragment is articulated then update skeleton using special function
			if(hierInst->body && pFragInst->IsInLevel() && CPhysics::GetLevel()->IsActive(pFragInst->GetLevelIndex()))
			{
				GetFragInst()->SyncSkeletonToArticulatedBody();
			}
			// otherwise do the matrix zero'ing ourselves
			else
			{
				pFragInst->ZeroSkeletonMatricesByGroup(GetSkeleton(), hierInst->damagedSkeleton);
			}
		}
	}
}


//
// name:		InverseUpdateSkeleton
// description:	Inverse update the skeleton for an entity
void CDynamicEntity::InverseUpdateSkeleton()
{
	WaitForAnyActiveAnimUpdateToComplete();

	if(GetSkeleton())
	{
		GetSkeleton()->InverseUpdate();
	}
}

//
// name:		ProcessPaused
// description:	Do this when the dynamic entity is paused
// Should be called even when paused
void CDynamicEntity::UpdatePaused()
{
	fwDynamicEntityComponent *dynComp = GetDynamicComponent();

	if (dynComp)
	{
		// Reset the extracted velocity
		//dynComp->SetAnimatedVelocity(VEC3_ZERO);

		// Reset the extracted angular velocity
		//dynComp->SetAnimatedAngularVelocity(0.0f);
	}
}

//
// name:		StartAnimUpdate
// description:	
void CDynamicEntity::StartAnimUpdate(float fTimeStep)
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnUpdateAnimOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfUpdateAnimCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	fwDynamicEntityComponent *dynComp = CreateDynamicComponentIfMissing();

	// Reset the animated velocity
	dynComp->SetAnimatedVelocity(VEC3_ZERO);

	// Reset the animated angular velocity
	dynComp->SetAnimatedAngularVelocity(0.0f);

	// Pre update the anim director - must always be called
	if(GetSkeleton() && GetAnimDirector())
	{
		GetAnimDirector()->PreUpdate(fTimeStep);

		bool bDoAnimUpdate = GetIsVisibleInSomeViewportThisFrame() || m_nDEflags.bForcePrePhysicsAnimUpdate;

		// Objects should also check their proximity to the camera for anim updates.
		// Required for audio events to be heard for nearby, off-screen objects.
		if (!bDoAnimUpdate && GetIsTypeObject())
		{
			const float fDist = GetBoundRadius() + OBJECT_PROXIMITY_FOR_ANIM_UPDATE;

			Vector3 tempDir(VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()));
			tempDir -= camInterface::GetPos();

			if (tempDir.Mag2() <= fDist*fDist)
			{
				bDoAnimUpdate = true;
			}
		}

		if (GetIsTypeVehicle() && 
			(static_cast<CVehicle*>(this)->m_nVehicleFlags.bForcePostCameraAnimUpdate ||
			static_cast<CVehicle*>(this)->m_nVehicleFlags.bForcePostCameraAnimUpdateUseZeroTimeStep))
		{
			// Intentionally falls through to the StopUpdatingThroughAnimQueue() call below.
		}
		else if(bDoAnimUpdate)
		{
			fwAnimDirector* pParentAnimDirector = NULL;

			// For the driver to update before its vehicle, if this is the vehicle, we find the
			// anim director of the parent, and pass that in to StartUpdatePrePhysics().
			CDynamicEntity* pParentEntity = GetProcessControlOrderParent();
			if(pParentEntity)
			{
				pParentAnimDirector = pParentEntity->GetAnimDirector();

#if __ASSERT
				// Verify that GetStartAnimSceneUpdateFlag() returns the right value in order to
				// ensure that the dependency indicated by GetProcessControlOrderParent() can be achieved.
				// Note: we could also check the actual flags in fwSceneUpdateExtension to make sure they
				// are set accordingly.
				if(pParentAnimDirector)
				{
					Assert(pParentEntity->GetStartAnimSceneUpdateFlag() == CGameWorld::SU_START_ANIM_UPDATE_PRE_PHYSICS);
					Assert(GetStartAnimSceneUpdateFlag() == CGameWorld::SU_START_ANIM_UPDATE_PRE_PHYSICS_PASS2);
				}
#endif	// __ASSERT

			}

			GetAnimDirector()->StartUpdatePrePhysics(fTimeStep, ( GetIsTypePed() && static_cast<CPed*>(this)->GetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate) ), pParentAnimDirector );

			// If we were not already updating through the animation queue, and we should update through
			// the animation queue now, set the internal flag to remember that we are in this mode now,
			// and clear out the regular update flags.
			if(!GetUpdatingThroughAnimQueue() && CGameWorld::ShouldUpdateThroughAnimQueue(*this))
			{
				SetUpdatingThroughAnimQueue(true);
				RemoveSceneUpdateFlags(CGameWorld::SU_UPDATE | CGameWorld::SU_UPDATE_VEHICLE);
			}

			// Return to avoid calling StopUpdatingThroughAnimQueue().
			return;
		}
	}

	// If we did not call fwAnimDirector::StartUpdatePrePhysics() above, this object
	// won't end up in the update queue, so if we are currently trying to update using
	// that, switch back to regular updates.
	StopUpdatingThroughAnimQueue();
}

//
// name:		UpdateVelocityAndAngularVelocity
// description:	
void CDynamicEntity::UpdateVelocityAndAngularVelocity(float fTimeStep)
{
	if(GetSkeleton() && GetAnimDirector())
	{
		GetAnimDirector()->UpdateVelocityAndAngularVelocity(fTimeStep);
	}
}

//
// name:		UpdateAnimAfterCameraUpdate
// description:	Update the animation director for an entity
void CDynamicEntity::StartAnimUpdateAfterCamera(float fTimeStep)
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnUpdateAnimAfterCameraUpdateOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfUpdateAnimAfterCameraUpdateCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	// First, do a quick rejection based on visibility, unless we are doing a camera cut.
	if(sm_CanUseCachedVisibilityThisFrame && !GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	fwAnimDirector* pAnimDirector = GetAnimDirector();
	if(pAnimDirector && pAnimDirector->RequiresUpdatePostCamera() && GetSkeleton())
	{
		// If we are doing a camera cut, do a test against the new viewport after the cut,
		// instead of the GetIsVisibleInSomeViewportThisFrame() call.
		if(!sm_CanUseCachedVisibilityThisFrame)
		{
			Vec3V boundCentre;
			float boundRadius = GetBoundCentreAndRadius(RC_VECTOR3(boundCentre));
			bool visible = camInterface::IsSphereVisibleInGameViewport(RCC_VECTOR3(boundCentre), boundRadius);
			if (!visible)
			{
				return;
			}
		}

		if(!pAnimDirector->IsUpdated())
		{
			OnEnterViewport();
		}

		pAnimDirector->StartUpdatePostCamera(fTimeStep);
	}
}

void CDynamicEntity::EndAnimUpdate(float fTimeStep)
{
	fwAnimDirector* pDirector = GetAnimDirector();
	if(pDirector)
	{
		pDirector->EndUpdatePrePhysics(fTimeStep);
	}
}

//
// name:		UpdatePortalTracker
// description:	update the portal tracker if needed
void CDynamicEntity::UpdatePortalTracker()
{
	Assert(GetIsTypeObject() || GetIsTypePed() || GetIsTypeVehicle());

	m_portalTracker.Update(VEC3V_TO_VECTOR3(GetTransform().GetPosition()));
}

//
// name:		PreRender
// description:	Render a dynamic entity (checks if entity has a skeleton)
ePrerenderStatus CDynamicEntity::PreRender(const bool bIsVisibleInMainViewport)
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnPreRenderOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfPreRenderCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	DEV_ONLY(ApplyBoneOverrides());

	bool applyDynamicAmbientScale = (bIsVisibleInMainViewport && GetUseAmbientScale() && GetUseDynamicAmbientScale());
	applyDynamicAmbientScale &= (m_nDEflags.bHasMovedSinceLastPreRender);
	
	if( applyDynamicAmbientScale )
	{
		CTimeCycleAsyncQueryMgr::AddNonPedEntity(this);
		m_nDEflags.bHasMovedSinceLastPreRender = false;
	}

	return CEntity::PreRender(bIsVisibleInMainViewport);
}

void CDynamicEntity::PreRender2(const bool UNUSED_PARAM(bIsVisibleInMainViewport))
{
}

#if __DEV

void CDynamicEntity::ApplyBoneOverrides()
{
	if ( CAnimViewer::m_pDynamicEntity && CAnimViewer::m_pDynamicEntity == this )
	{
		if ( GetSkeleton() )
		{
			if (CAnimViewer::ms_bOverrideBoneRotation || CAnimViewer::ms_bOverrideBoneTranslation || CAnimViewer::ms_bOverrideBoneScale)
			{
			GetSkeleton()->InverseUpdate();

			// Debug functionality to override a bones rotation
			//NOTE: This is slightly different from pre refactoring since
			// ApplyExpressions() is no longer called after this.
			if (CAnimViewer::ms_bOverrideBoneRotation)
			{
				if (CAnimViewer::ms_bApplyOverrideRelatively)
				{
					Matrix34 bind = MAT34V_TO_MATRIX34(GetSkeletonData().GetDefaultTransform(CAnimViewer::m_boneIndex));
					Matrix34 source = RC_MATRIX34(GetSkeleton()->GetLocalMtx(CAnimViewer::m_boneIndex));
					Matrix34 goal;
					goal.Identity();
					goal.FromEulersXYZ(CAnimViewer::m_rotation);
					goal.Dot(bind);

					// Set the bone local rotation to the bind + override
					RC_MATRIX34(GetSkeleton()->GetLocalMtx(CAnimViewer::m_boneIndex)).Interpolate(
						source,
						goal,
						CAnimViewer::ms_fOverrideBoneBlend);
				}
				else
				{
					Matrix34 source = RC_MATRIX34(GetSkeleton()->GetLocalMtx(CAnimViewer::m_boneIndex));
					Matrix34 goal = RC_MATRIX34(GetSkeleton()->GetLocalMtx(CAnimViewer::m_boneIndex));
					goal.FromEulersXYZ(CAnimViewer::m_rotation);

					// Set the bone local rotation to the override
					RC_MATRIX34(GetSkeleton()->GetLocalMtx(CAnimViewer::m_boneIndex)).Interpolate(
						source,
						goal,
						CAnimViewer::ms_fOverrideBoneBlend);
				}
			}

			if (CAnimViewer::ms_bOverrideBoneTranslation)
			{
				Matrix34 localMatrix = MAT34V_TO_MATRIX34(GetSkeleton()->GetLocalMtx(CAnimViewer::m_boneIndex));

				if (CAnimViewer::ms_bApplyOverrideRelatively)
				{
					Vector3 translation = CAnimViewer::m_translation;
					translation.Dot3x3(localMatrix);

					Vector3 bindTranslation = VEC3V_TO_VECTOR3(GetSkeletonData().GetDefaultTransform(CAnimViewer::m_boneIndex).d());
					translation = bindTranslation + translation;

					Matrix34 source = RC_MATRIX34(GetSkeleton()->GetLocalMtx(CAnimViewer::m_boneIndex));
					Matrix34 goal = RC_MATRIX34(GetSkeleton()->GetLocalMtx(CAnimViewer::m_boneIndex));
					goal.d.Set(translation);

					// Set the bone local rotation to the override
					RC_MATRIX34(GetSkeleton()->GetLocalMtx(CAnimViewer::m_boneIndex)).Interpolate(
						source,
						goal,
						CAnimViewer::ms_fOverrideBoneBlend);
				}
				else
				{
					Vector3 translation = CAnimViewer::m_translation;
					translation.Dot3x3(localMatrix);

					Matrix34 source = RC_MATRIX34(GetSkeleton()->GetLocalMtx(CAnimViewer::m_boneIndex));
					Matrix34 goal = RC_MATRIX34(GetSkeleton()->GetLocalMtx(CAnimViewer::m_boneIndex));
					goal.d.Set(translation);

					// Set the bone local rotation to the override
					RC_MATRIX34(GetSkeleton()->GetLocalMtx(CAnimViewer::m_boneIndex)).Interpolate(
						source,
						goal,
						CAnimViewer::ms_fOverrideBoneBlend);
				}
			}

			if(CAnimViewer::ms_bOverrideBoneScale)
			{
				Vec3V scale = VECTOR3_TO_VEC3V(CAnimViewer::m_scale);
				if(CAnimViewer::ms_bApplyOverrideRelatively)
				{
					scale *= GetSkeletonData().GetBoneData(CAnimViewer::m_boneIndex)->GetDefaultScale();
				}

				Mat34V& localMtx = GetSkeleton()->GetLocalMtx(CAnimViewer::m_boneIndex);
				Scale3x3(localMtx, scale, localMtx);
			}

				GetSkeleton()->Update();
			}
		}
	}
}

#endif //_DEV


//
// name:		CDynamicEntity::CreateSkeleton
// description:	Create a skeleton for this entity
void CDynamicEntity::CreateSkeleton()
{
	Assertf(GetEntitySkeleton() == NULL, "Skeleton already created");

	// fragInst's use the skeleton provided from the fragment cache
	fragInst* pEntityFragInst = GetFragInst();
	if (pEntityFragInst && !pEntityFragInst->GetCached())
	{
		pEntityFragInst->PutIntoCache();
		Assertf(GetSkeleton(), "Skeleton not created!");
		return;
	}

	if (GetIsTypePed() || GetIsTypeVehicle())
	{
		Assertf(GetSkeleton(), "Skeleton not created for %s", GetModelName());
		return;
	}

	// Create a skeleton
	if (GetDrawable())
	{
		crSkeletonData *pSkeletonData = GetDrawable()->GetSkeletonData();
		if (pSkeletonData)
		{
			crSkeleton *skeleton = rage_new crSkeleton();
			skeleton->Init(*pSkeletonData, NULL);
			SetSkeleton(skeleton);

			// ensure matrix is in memory all the time

			// KS - VMATH - oh fuck - only skeletons still needs a matrix ptr!!!!
			Assertf(GetTransformPtr()->IsMatrixTransform(), "Entity %s needs to have a matrix transform, but has type %d", GetModelName(), GetTransformPtr()->GetTypeIdentifier());
			const fwMatrixTransform* m = static_cast<const fwMatrixTransform*>(GetTransformPtr());
			skeleton->SetParentMtx(m->GetMatrixPtr());
		}
		else
		{
			Assertf(0, "%s : has invalid skeleton data", GetBaseModelInfo() ?  GetModelName() : "Unknown dynamic entity");
		}

	}
	else
	{
		Assertf(0, "%s : has invalid drawable", GetBaseModelInfo() ?  GetModelName() : "Unknown dynamic entity");
	}

	Assertf(GetEntitySkeleton(), "%s : failed to create a skeleton", GetBaseModelInfo() ?  GetModelName() : "Unknown dynamic entity");
}

fwDrawData* CDynamicEntity::AllocateDrawHandler(rmcDrawable* pDrawable)
{
	if (m_nFlags.bIsFrag)
	{
		return rage_new CDynamicEntityFragmentDrawHandler(this, pDrawable);
	}
	else if (pDrawable->GetSkeletonData())
	{
		return rage_new CDynamicEntitySkinnedDrawHandler(this, pDrawable);
	}
	else
	{
		if(pDrawable->HasInstancedGeometry())
		{
			return rage_new CEntityInstancedBasicDrawHandler(this, pDrawable);
		}
		return rage_new CEntityBasicDrawHandler(this, pDrawable);
	}
}

void CDynamicEntity::DeleteDrawable()
{
	DeleteAnimDirector();
	CEntity::DeleteDrawable();
	DeleteSkeleton();
}

void CDynamicEntity::SetHeading(float new_heading)
{
	m_nDEflags.bHasMovedSinceLastPreRender = true;
	CEntity::SetHeading(new_heading);
}

void CDynamicEntity::SetPosition(const Vector3& vec, bool bUpdateGameWorld, bool bUpdatePhysics, bool bWarp)
{
	m_nDEflags.bHasMovedSinceLastPreRender = true;

	if (bWarp)
	{
		// Reset secondary motions
		crCreature* creature = GetCreature();
		if(creature)
		{
			crCreatureComponentPhysical* physicalComp = creature->FindComponent<crCreatureComponentPhysical>();
			if(physicalComp)
			{
				physicalComp->ResetMotions();
			}
		}
#if GTA_REPLAY
			CReplayMgr::RecordWarpedEntity(this);
#endif
	}

	CEntity::SetPosition(vec, bUpdateGameWorld, bUpdatePhysics, bWarp);
}

void CDynamicEntity::SetMatrix(const Matrix34& mat, bool bUpdateGameWorld, bool bUpdatePhysics, bool bWarp)
{
	if (bWarp)
	{
		// Reset secondary motions
		crCreature* creature = GetCreature();
		if(creature)
		{
			crCreatureComponentPhysical* physicalComp = creature->FindComponent<crCreatureComponentPhysical>();
			if(physicalComp && !IsCloseAll(RCC_MAT34V(mat), GetMatrix(), ScalarV(V_FLT_SMALL_3)))
			{
				physicalComp->ResetMotions();
			}
		}
#if GTA_REPLAY
		CReplayMgr::RecordWarpedEntity(this);
#endif
	}

	m_nDEflags.bHasMovedSinceLastPreRender = true;
	CEntity::SetMatrix(mat, bUpdateGameWorld, bUpdatePhysics, bWarp);
}

void CDynamicEntity::AddShaderVarCreatureComponent(CCustomShaderEffectBase* pShaderEffect)
{
	if (GetIsTypePed())
	{
		Assert(GetCreature());
		crCreatureComponentShaderVars* shaderVarComponent = GetCreature()->FindComponent<crCreatureComponentShaderVars>();

		CPedVariationData* pVarData = NULL; 
		CPed* pPed = static_cast<CPed*>(this);
		if(pPed)
		{
			pVarData = &pPed->GetPedDrawHandler().GetVarData(); 
		}

		CPedModelInfo* pPedModelInfo = static_cast<CPedModelInfo*>(GetBaseModelInfo());
		Assert(pPedModelInfo);

		CCustomShaderEffectPed* pPedShaderEffect=static_cast<CCustomShaderEffectPed*>(pShaderEffect);
		if (pVarData && pPedModelInfo && pPedShaderEffect)
		{
			// Is a creature metadata file defined in peds.meta for this ped?
			if ( pPedModelInfo->GetCreatureMetadataFileIndex()!=-1)
			{
				atFixedArray<CShaderVariableComponent*, 256> shaderVarMetaDatas;

				// Create the shader variable components using the creature metadata file
				// The creature metadata file should already be streamed in as it is a dependency of the ped
#if __ASSERT
				bool bHasCreatureMetaLoaded = g_fwMetaDataStore.HasObjectLoaded(strLocalIndex(pPedModelInfo->GetCreatureMetadataFileIndex()));
				Assert(bHasCreatureMetaLoaded);
#endif
				CCreatureMetaData *creatureMetaData = g_fwMetaDataStore.Get(strLocalIndex(pPedModelInfo->GetCreatureMetadataFileIndex()))->GetObject<CCreatureMetaData>();
				for (int i=0; i<creatureMetaData->m_shaderVariableComponents.GetCount(); i++)
				{
					CShaderVariableComponent& shaderVarMetaData = creatureMetaData->m_shaderVariableComponents[i];
					animAssert((s32)shaderVarMetaData.m_pedcompID > PV_COMP_INVALID && shaderVarMetaData.m_pedcompID < PV_MAX_COMP);
				
					rmcDrawable* pComponent = NULL;
					if (pPedModelInfo->GetIsStreamedGfx())
					{
						pComponent = CPedVariationStream::GetComponent((ePedVarComp)shaderVarMetaData.m_pedcompID, this, *pVarData);
					
					} 
					else 
					{
						pComponent = CPedVariationPack::GetComponent((ePedVarComp)shaderVarMetaData.m_pedcompID, this, *pVarData);
					}

					if (pComponent)
					{
						shaderVarMetaDatas.Append() = &shaderVarMetaData;
					}
				}
	
				u32 shaderMetaDatasHash = atDataHash((u32*)shaderVarMetaDatas.GetElements(), sizeof(CShaderVariableComponent*)*shaderVarMetaDatas.GetCount());

				if(shaderVarComponent)
				{
					if(shaderVarComponent->GetProjectData() == shaderMetaDatasHash)
					{
						return;
					}
					else
					{
						GetCreature()->RemoveComponentType(crCreatureComponent::kCreatureComponentTypeShaderVars);
						shaderVarComponent = NULL;
					}
				}
				if(shaderVarMetaDatas.GetCount() > 0)
				{
					if(!shaderVarComponent)
					{
						shaderVarComponent = static_cast<crCreatureComponentShaderVars*>(GetCreature()->AddComponentType(crCreatureComponent::kCreatureComponentTypeShaderVars));
						shaderVarComponent->SetProjectData(shaderMetaDatasHash);
					}

					int numNewPairs = 0;
					for(int i=0; i<shaderVarMetaDatas.GetCount(); ++i)
					{
						CShaderVariableComponent& shaderVarMetaData = *shaderVarMetaDatas[i];
						int numTracks = shaderVarMetaData.m_tracks.GetCount();
						numNewPairs += numTracks;
					}

					crCreatureComponentShaderVars::FastAddShaderVarDofPairs addPairs(*shaderVarComponent, numNewPairs);

					for(int i=0; i<shaderVarMetaDatas.GetCount(); ++i)
					{
						CShaderVariableComponent& shaderVarMetaData = *shaderVarMetaDatas[i];
						int numTracks = shaderVarMetaData.m_tracks.GetCount();
#if __ASSERT
						int numIds = shaderVarMetaData.m_ids.GetCount();
						int numComponents = shaderVarMetaData.m_components.GetCount();
						animAssertf(numIds == numTracks, "Found %d ids while expecting %d",numIds,numTracks);
						animAssertf(numComponents == numTracks, "Found %d components while expecting %d",numComponents,numTracks);

						numTracks = Min(numTracks,Min(numIds,numComponents));
#endif // __ASSERT
						fwShaderVarComponentData data;
						data.unpack.m_Hash = shaderVarMetaData.m_shaderVariableHashString.GetHash();
						data.unpack.m_PedCompId = u8(shaderVarMetaData.m_pedcompID);
						data.unpack.m_MaskId = u8(shaderVarMetaData.m_maskID);

						for(int n=0; n<numTracks; ++n)
						{
							data.unpack.m_Component = shaderVarMetaData.m_components[n];
							addPairs.FastAddPair(shaderVarMetaData.m_tracks[n], shaderVarMetaData.m_ids[n], data.m_Packed);
						}
					}
					return;
				}
			}
		}

		GetCreature()->RemoveComponentType(crCreatureComponent::kCreatureComponentTypeShaderVars);
	}
}

//
// name:		CDynamicEntity::RemoveShaderVarCreatureComponent
// description:	Removes the shadervars component from the creature
void CDynamicEntity::RemoveShaderVarCreatureComponent()
{
	crCreature *creature = GetCreature();
	if (creature)
	{
		creature->RemoveComponentType(crCreatureComponent::kCreatureComponentTypeShaderVars);
	}
}

#if ENABLE_BLENDSHAPES
//
// name:		CDynamicEntity::AddBlendshapesCreatureComponent
// description:	Add a blendshape component to the creature
// add blendshape dofs to the animation frame
// add blendshape dofs to the static animation frames in the animblender
void CDynamicEntity::AddBlendshapesCreatureComponent(rmcDrawable& drawable)
{
	if ( GetCreature() && GetCreature()->FindComponent(crCreatureComponent::kCreatureComponentTypeShaderVars))
	{
		Assertf(0, "Creature already has a CreatureComponentTypeBlendShapes");
		return;
	}

	fwDynamicEntityComponent *dynComp = GetDynamicComponent();
	if (dynComp)
	{
		crCreature *creature = dynComp->GetCreature();
		if (creature)
		{
			// Add a blendshape component to the creature
			grbTargetManager *targetManager = dynComp->GetTargetManager();
			if(targetManager)
			{
				crCreatureComponentBlendShapes* component = static_cast<crCreatureComponentBlendShapes*>(GetCreature()->AllocateComponent(crCreatureComponent::kCreatureComponentTypeBlendShapes));
				component->Init(*creature, drawable, *targetManager);
				creature->AddComponent(*component);
			}
		}
	}
}

//
// name:		CDynamicEntity::RemoveBlendshapesCreatureComponent
// description:	Removes the blendshape component from the creature
void CDynamicEntity::RemoveBlendshapesCreatureComponent()
{
	crCreature *creature = GetCreature();
	if (creature)
	{
		creature->RemoveComponentType(crCreatureComponent::kCreatureComponentTypeBlendShapes);
	}
}
#endif // ENABLE_BLENDSHAPES

void CDynamicEntity::CreateAnimDirector(rmcDrawable& ENABLE_BLENDSHAPES_ONLY(drawable), bool addExtraDofsComponent, bool withRagDollComponent, bool withFacialRigComponent, CCustomShaderEffectBase* pCustomShaderEffectBase, grbTargetManager * ENABLE_BLENDSHAPES_ONLY(pgrbTargetManager))
{
#if ENABLE_BLENDSHAPES
	InitTargetManager();
#endif // ENABLE_BLENDSHAPES

	Assert(!GetCreature());
	crCreature* creature = rage_new crCreature;
	creature->SetComponentFactory(fwAnimDirectorComponentCreature::GetComponentFactory());
	creature->Init(fwAnimDirectorComponentCreature::GetFrameDataFactory(), fwAnimDirectorComponentCreature::GetAccelerator(), GetSkeleton());
	SetCreature(creature);

	if(addExtraDofsComponent)
	{
		const u16 kNumMoverDofs = 2;
		const u16 kNumExtraDofs = 7;
		static crFrameDataFixedDofs<kNumMoverDofs + kNumExtraDofs> frameData;

		if(!frameData.GetNumDofs())
		{
			crFrameDataInitializerMover moverInitializer;
			moverInitializer.InitializeFrameData(frameData);

			u8 tracks[kNumExtraDofs] =
			{
				kTrackFirstPersonCameraWeight,
				kTrackConstraintHelperLeftHandWeight,
				kTrackConstraintHelperRightHandWeight,
				kTrackBoneRotation,
				kTrackBoneTranslation,
				kTrackBoneRotation,
				kTrackBoneTranslation
			};
			u16 ids[kNumExtraDofs] =
			{
				0,
				0,
				0,
				BONETAG_CH_L_HAND,
				BONETAG_CH_L_HAND,
				BONETAG_CH_R_HAND,
				BONETAG_CH_R_HAND
			};
			u8 types[kNumExtraDofs] =
			{
				kFormatTypeFloat,
				kFormatTypeFloat,
				kFormatTypeFloat,
				kFormatTypeQuaternion,
				kFormatTypeVector3,
				kFormatTypeQuaternion,
				kFormatTypeVector3
			};
			crFrameDataInitializerDofs dofsInitializer(kNumExtraDofs, tracks, ids, types, false);
			dofsInitializer.InitializeFrameData(frameData);
		}

		AddExtraDofsCreatureComponent(frameData);
	}

	if(pCustomShaderEffectBase)
	{
		AddShaderVarCreatureComponent(pCustomShaderEffectBase);
	}

#if ENABLE_BLENDSHAPES
	if(pgrbTargetManager)
	{
		AddBlendshapesCreatureComponent(drawable);
	}
#endif // ENABLE_BLENDSHAPES

	fwDynamicEntityComponent* dynamic = CreateDynamicComponentIfMissing();
	dynamic->CreateAnimDirector(*this, withRagDollComponent, withFacialRigComponent);

	AddSceneUpdateFlags(GetStartAnimSceneUpdateFlag());
}

void CDynamicEntity::DeleteAnimDirector()
{
	fwDynamicEntityComponent* dynamic = GetDynamicComponent();
	if (dynamic)
	{
		dynamic->DeleteAnimDirector();

		delete dynamic->GetCreature();
		dynamic->SetCreature(NULL);
	}

#if ENABLE_BLENDSHAPES
	ShutdownTargetManager();
#endif // ENABLE_BLENDSHAPES
}

const GtaThread* CDynamicEntity::GetThreadThatCreatedMe() const
{
	const CScriptEntityExtension* pExtension = GetExtension<CScriptEntityExtension>();

	if (pExtension && pExtension->GetScriptHandler())
	{
		return static_cast<const GtaThread*>(pExtension->GetScriptHandler()->GetThread());
	}

	return NULL;
}

const CGameScriptId* CDynamicEntity::GetScriptThatCreatedMe() const
{
	const CScriptEntityExtension* pExtension = GetExtension<CScriptEntityExtension>();

	if (pExtension && pExtension->GetScriptInfo())
	{
		return static_cast<const CGameScriptId*>(&pExtension->GetScriptInfo()->GetScriptId());
	}

	return NULL;
}

void CDynamicEntity::SetupMissionState()
{
	PopTypeSet(POPTYPE_MISSION);	

	m_nDEflags.bCheckedForDead = TRUE;

	if (!GetIsTypeObject() && AssertVerify(GetPortalTracker()))
	{
		GetPortalTracker()->SetLoadsCollisions(true);	//force loading of interior collisions
		CPortalTracker::AddToActivatingTrackerList(GetPortalTracker());		// causes interiors to activate & stay active
	}

	if (GetNetworkObject() && !IsNetworkClone())
	{
		NetworkInterface::GetObjectManager().ConvertRandomObjectToScriptObject(GetNetworkObject());
	}
}

void CDynamicEntity::CleanupMissionState()
{
	scriptDisplayf("CDynamicEntity::CleanupMissionState called for script entity with model %s (Entity = 0x%p)", GetModelName(), this);

	PopTypeSet(POPTYPE_RANDOM_AMBIENT);

	if (AssertVerify(GetPortalTracker()))
	{
		GetPortalTracker()->SetLoadsCollisions(false);	
		CPortalTracker::RemoveFromActivatingTrackerList(GetPortalTracker());
	}

	if (GetNetworkObject())
	{
		GetNetworkObject()->OnConversionToAmbientObject();
	}
}

void CDynamicEntity::DestroyScriptExtension()
{
	CScriptEntityExtension* pExtension = GetExtension<CScriptEntityExtension>();

	if (pExtension)
	{
		Assert(!pExtension->GetScriptHandler());
		GetExtensionList().Destroy(pExtension);
	}
}

void CDynamicEntity::InitClass()
{
	//CCreatureMetaData::InitClass();

	fwAnimDirector::InitClass();

	CBaseIkManager::InitClass();

	CMovePed::InitClass();
}

void CDynamicEntity::ShutdownClass()
{
	CMovePed::ShutdownClass();

	fwAnimDirector::ShutdownClass();
}

eSkelMatrixMode CDynamicEntity::GetSkelMode()
{
	eSkelMatrixMode mode = SKEL_NORMAL;
	CBaseModelInfo* pModelInfo = GetBaseModelInfo();
	const rmcLodGroup& lodgroup = pModelInfo->GetDrawable()->GetLodGroup();
	if (lodgroup.ContainsLod(LOD_HIGH) && lodgroup.GetLodModel0(LOD_HIGH).IsModelRelative())
		mode = SKEL_MODEL_RELATIVE;

	return(mode);
}

void CDynamicEntity::AddToStraddlingPortalsContainer()
{
	Assertf(m_nDEflags.bIsStraddlingPortal == 0, "Entity is already straddling a portal");
	fwStraddlingPortalsContainer *pContainer = static_cast<fwExteriorSceneGraphNode*>(fwWorldRepMulti::GetSceneGraphRoot())->GeStradlingPortalsContainer();

	if (Verifyf(pContainer->GetEntityCount() < pContainer->GetStorageSize(), "Container for Entities straddling portals is full"))
	{
		fwExteriorSceneGraphNode *pExteriorSceneGraphNode = static_cast<fwExteriorSceneGraphNode*>(fwWorldRepMulti::GetSceneGraphRoot());
		m_portalStraddlingContainerIndex = pExteriorSceneGraphNode->AppendToStraddlingPortalContainer(this);
		// This flag must be set here after the call to AppendToStraddlingPortalContainer or we will end up in a loop recursively calling CGameWorld::UpdateEntityDesc
		m_nDEflags.bIsStraddlingPortal = 1;
		RequestUpdateInWorld();
	}

}
void CDynamicEntity::RemoveFromStraddlingPortalsContainer()
{
	Assertf(m_nDEflags.bIsStraddlingPortal == 1 && m_portalStraddlingContainerIndex != 0xFF, "Entity is not straddling a portal");

	m_nDEflags.bIsStraddlingPortal = 0;
	fwExteriorSceneGraphNode *pExteriorSceneGraphNode = static_cast<fwExteriorSceneGraphNode*>(fwWorldRepMulti::GetSceneGraphRoot());
	pExteriorSceneGraphNode->RemoveFromStraddlingPortalContainer(m_portalStraddlingContainerIndex);
	m_portalStraddlingContainerIndex = 0xFF;

	RequestUpdateInWorld();
}

void CDynamicEntity::UpdateStraddledPortalEntityDesc()
{
	fwStraddlingPortalsContainer *pContainer = static_cast<fwExteriorSceneGraphNode*>(fwWorldRepMulti::GetSceneGraphRoot())->GeStradlingPortalsContainer();
	u8 bIsStraddlingPortalSave = m_nDEflags.bIsStraddlingPortal;

	// In order to prevent recursively calling this function we must temporarily turn this flag off
	m_nDEflags.bIsStraddlingPortal = 0;
	CGameWorld::UpdateEntityDesc(this, pContainer->GetEntityDesc(m_portalStraddlingContainerIndex));
	m_nDEflags.bIsStraddlingPortal = bIsStraddlingPortalSave;
}



#if __BANK
void CDynamicEntity::LogBaseFlagChange(s32 nFlag, bool bIsProtected, bool bNewVal, bool bOldVal)
{
	TUNE_BOOL(DISABLE_COLLISION_FLAG_LOGGING, false);

	if(DISABLE_COLLISION_FLAG_LOGGING)
		return;

	if(bNewVal == bOldVal)
		return;

	if(GetIsDynamic() && PopTypeIsMission())
	{
		if(CAILogManager::IsLogInitialised())
		{
			const char *pFlagNames[3] = {
				"IS_FIXED",
				"IS_FIXED_BY_NETWORK",
				"IS_FIXED_UNTIL_COLLISION",
			};

			int nNameIndex = -1;

			if(!bIsProtected)
			{
				switch(nFlag)
				{
				case fwEntity::IS_FIXED:
					nNameIndex = 0;
					break;
				case fwEntity::IS_FIXED_BY_NETWORK:
					nNameIndex = 1;
					break;				
				}
			}
			else
			{
				switch(nFlag)
				{				
				case fwEntity::IS_FIXED_UNTIL_COLLISION:
					nNameIndex = 2;
					break;
				}
			}

			if(nNameIndex > -1)
			{
				const char *pEntName = AILogging::GetDynamicEntityNameSafe(reinterpret_cast<CDynamicEntity *>(this));
				if(pEntName[0] == 0)
					pEntName = GetModelName();

				CAILogManager::GetLog().Log(bNewVal ? "[FixedPhysicsFlags] Entity %s had %s flag set.\n" : "[FixedPhysicsFlags] Entity %s had %s flag unset.\n", pEntName, pFlagNames[nNameIndex]);
				
				if(CTheScripts::GetCurrentScriptNameAndProgramCounter()[0] != ' ')
					CAILogManager::GetLog().Log("%s\n", CTheScripts::GetCurrentScriptNameAndProgramCounter());

				AILogging::StackTraceLog();	
				CAILogManager::GetLog().Log("[FixedPhysicsFlags] END BASE FLAG CHANGE OUTPUT\n");
			}
		}
	}
}
#endif