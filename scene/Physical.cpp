// Title	:	Physical.cpp
// Author	:	Richard Jobling
// Started	:	17/11/98
#include "Physical.h"

// Rage headers
#include "control/replay/ReplayExtensions.h"
#include "crskeleton/skeleton.h"
#include "debug/DebugEntityState.h"
#include "fragment/cache.h"
#include "grcore/debugdraw.h"
#include "entity/altskeletonextension.h"
#include "fwanimation/directorcomponentsyncedscene.h"
#include "fwscene/stores/staticboundsstore.h"
#include "math/simplemath.h"
#include "phbound/boundbvh.h"
#include "phbound/boundcomposite.h"
#include "phcore/phmath.h"
#include "phsolver/contactmgr.h"
#include "physics/archetype.h"
#include "physics/sleep.h"
#include "fwmaths/Random.h"
#include "fwmaths/Vector.h"
#include "fwscene/world/WorldLimits.h"
#include "physics/constraintspherical.h"
#include "physics/constraintfixed.h"
#include "physics/constraintattachment.h"
#include "vehicles/metadata/vehicleseatinfo.h"
#include "phbound/support.h"

#if __BANK
#include "renderer/DrawLists/drawListNY.h"
#include "scene/EntityIterator.h"
#endif

#if __ASSERT
// Used for traking physics creation backtraces
#include "system/stack.h"
#endif

// Game headers
#include "modelinfo/ModelInfo.h"
#include "Cloth/Cloth.h"
#include "Cloth/ClothMgr.h"
#include "control/gamelogic.h"
#include "Debug/debugscene.h"
#include "event/EventGroup.h"
#include "event/EventScript.h"
#include "Frontend/MiniMapCommon.h"
#include "game/Cheat.h"
#include "game/modelindices.h"
#include "modelinfo/Vehiclemodelinfo.h"
#include "Network/NetworkInterface.h"
#include "network/Debug/NetworkDebug.h"
#include "network/general/NetworkUtil.h"
#include "objects/object.h"
#include "objects/Door.h"
#include "pathserver/ExportCollision.h"
#include "peds/AssistedMovement.h"
#include "peds/Ped.h"
#include "peds/PedIntelligence.h"
#include "peds/PlayerSpecialAbility.h"
#include "physics/Breakable.h"
#include "physics/GtaInst.h"
#include "physics/GtaMaterialManager.h"
#include "physics/physics_channel.h"
#include "physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Physics/RagdollConstraints.h"
#include "pickups/Pickup.h"
#include "scene/Physical.h"
#include "scene/scene.h"
#include "scene/world/gameWorld.h"
#include "scene/world/gameWorldWaterHeight.h"
#include "script/Handlers/GameScriptEntity.h"
#include "script/script.h"
#include "security/plugins/linkdatareporterplugin.h"
#include "security/obfuscatedtypes.h"
#include "system/TamperActions.h"
#include "system/Timer.h"
#include "Task/Motion/TaskMotionBase.h"
#include "vehicles/Automobile.h"
#include "vehicles/Bike.h"
#include "vehicles/Boat.h"
#include "vehicles/Heli.h"
#include "vehicles/Vehicle.h"
#include "vehicles/train.h"
#include "renderer/water.h"
#include "renderer/RenderPhases/RenderPhaseWaterReflection.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "Vfx/VfxHelper.h"
#include "Task/Physics/TaskNM.h"

#if __BANK
#include "fwnet/netobject.h"
#include "system/stack.h"
#endif // __BANK

// defines to calc how many collision steps are required
#define MIN_RAD_VEHICLE		(0.4f)
#define MIN_RAD_VEHICLE_SQR	(MIN_RAD_VEHICLE*MIN_RAD_VEHICLE)
#define MIN_RAD_PED			(0.3f)
#define MIN_RAD_PED_SQR		(MIN_RAD_PED*MIN_RAD_PED)
#define MIN_RAD_OBJECT		(0.3f)
#define MIN_RAD_OBJECT_SQR	(MIN_RAD_OBJECT*MIN_RAD_OBJECT)
#define MIN_RAD_LAMPPOST	(0.3f)
#define MIN_RAD_LAMPPOST_SQR (MIN_RAD_LAMPPOST*MIN_RAD_LAMPPOST)
#define HIGHSPEED_ELASTICITY_MULT_PED	(2.0f)

// defines to shape collision responses
#define PHYSICAL_VEHICLEVERTICALCOLLISIONDAMPING 0.3f
#define PHYSICAL_VEHICLECOLLISION_FORCEMULT		1.0f	//1.4f
#define PHYSICAL_VEHICLECOLLISION_TORQUEDAMP	0.8f
#define PHYSICAL_VEHICLEBUILDING_FORCEMULT		1.2f
#define PHYSICAL_VEHICLEBULIDING_TORQUEDAMP		0.8f
#define PHYSICAL_VEHICLEFRICTION_FORCEDAMP		(-0.3f)

#define PHYSICAL_DEFAULT_HEALTH					(100.0f)

PARAM(debugFixedByNetwork, "Log local player fixed by network changes");

ENTITY_OPTIMISATIONS()
SCENE_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(CPhysical,0xA00A9444);

AUTOID_IMPL(CObjectCoverExtension);
AUTOID_IMPL(CBrokenAndHiddenComponentFlagsExtension);

namespace entity_commands
{
	void SetEntityAsNoLongerNeeded(CPhysical *pEntity, int &EntityIndex);
};

#if DR_ENABLED
bool CPhysical::smb_RecordAttachments=false;
CPhysical::RecordEntityAttachmentFunc CPhysical::sm_RecordAttachmentsFunc=0;
#endif

#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
CPhysical::CSecondSurfaceConfig CPhysical::sm_defaultSecondSurfaceConfig(
	0.2f,		// Sink lift factor
	1.0f,		// TangV2 lift factor
	1.5f,		// Max TangV2 lift factor
	0.5f		// Drag factor
	);

CPhysical::CSecondSurfaceConfig::CSecondSurfaceConfig() : 
m_fSinkLiftFactor(0.0f),
m_fTangV2LiftFactor(0.0f),
m_fMaxTangV2LiftFactor(0.0f),
m_fDragFactor(0.0f)
{
}

CPhysical::CSecondSurfaceConfig::CSecondSurfaceConfig(float fSinkLiftFactor,
													  float fTangV2LiftFactor,
													  float fMaxTangV2LiftFactor,
													  float fDragFactor) :
m_fSinkLiftFactor(fSinkLiftFactor),
m_fTangV2LiftFactor(fTangV2LiftFactor),
m_fMaxTangV2LiftFactor(fMaxTangV2LiftFactor),
m_fDragFactor(fDragFactor)
{
}

#if __BANK
void CPhysical::CSecondSurfaceConfig::AddWidgetsToBank(bkBank& bank)
{
	bank.AddSlider("Sink lift factor", &m_fSinkLiftFactor, 0.0f, 100.0f, 0.1f);
	bank.AddSlider("Tang2V lift factor", &m_fTangV2LiftFactor, 0.0f, 100.0f, 0.1f);
	bank.AddSlider("Max Tank2V lift factor", &m_fMaxTangV2LiftFactor, 0.0f, 100.0f, 0.1f);
	bank.AddSlider("Drag coefficient", &m_fDragFactor, 0.0f, 100.0f, 0.1f);
}
#endif  // BANK
#endif	// HACK_GTA4_BOUND_GEOM_SECOND_SURFACE

BANK_ONLY(CDebugObjectIDQueue<CPhysical::DebugObjectID, CPhysical::sMaxObjectIDs> CPhysical::ms_aDebugObjectIDs);

#if __ASSERT
bool CPhysical::sm_enableNoCollisionPlaneFixDebug = false;
#endif

CPhysical::CPhysical(const eEntityOwnedBy ownedBy) :
CDynamicEntity(ownedBy),
m_fHealth(PHYSICAL_DEFAULT_HEALTH),
m_fMaxHealth(PHYSICAL_DEFAULT_HEALTH),
m_MatrixTransformPtr(NULL),
m_bDontResetDamageFlagsOnCleanupMissionState(false),
m_pWeaponDamageInfos(NULL)
#if __BANK
	,m_iDebugObjectId(sInvalidObjectID)
#endif
{
#if ENABLE_MATRIX_MEMBER
	Mat34V m(V_IDENTITY);	
	SetTransform(m);
	SetTransformScale(1.0f, 1.0f);
	m_MatrixTransformPtr = GetTransform().GetMatrixPtr();
#else
	Mat34V m(V_IDENTITY);
	fwMatrixTransform* trans = rage_new fwMatrixTransform(m);
	SetTransform(trans);
	m_MatrixTransformPtr = trans->GetMatrixPtr();
#endif
	EnableCollision();

	// RA - unused
	//m_fPrevDistFromCam = PHYSICAL_MAX_DIST_FROM_CAMERA;
	
	m_nPhysicalFlags.Clear();

	m_specificRelGroupHash = 0;

	CRenderPhaseWaterReflectionInterface::UpdateInWater(this, m_nPhysicalFlags.bIsInWater);

	m_canOnlyBeDamagedByEntity = NULL;
	m_cantBeDamagedByEntity = NULL;

#if !__NO_OUTPUT
	m_LogSetMatrixCalls = false; 
	m_LogVisibiltyCalls = false; 
	m_LogDeletions = false; 
	m_LogVariationCalls = false;
#endif
#if __ASSERT
	m_physicsBacktrace = 0;
#endif

	BANK_ONLY(sprintf(m_DebugLogName, ""));
}

CPhysical::~CPhysical()
{
#if GTA_REPLAY
	replayAssertf(!CReplayExtensionManager::HasAnyExtension(this), "Replay Extensions not removed before entity deletion.  Pls bug to Andrew Collinson if this happens");
#endif
/*RAGE	if (m_pRealTimeShadow)
	{	
		Assert(m_pRealTimeShadow->m_pPhysical==this);
		g_realTimeShadowMan.ReturnRealTimeShadow(m_pRealTimeShadow);
	}*/

	// not allowed to delete objects if they are registered with the network object manager
	Assert(!GetNetworkObject());

	if(GetType() != ENTITY_TYPE_VEHICLE)
	{
		// Vehicles can be destroyed from threads other than the update thread, and fwAltSkeletonExtension::RemoveExtension
		// is only safe to call from the update thread. However, vehicles don't have an fwAltSkeletonExtension anyway...
		fwAltSkeletonExtension::RemoveExtension(*this);
	}

	if(m_pWeaponDamageInfos)
	{
		delete m_pWeaponDamageInfos;
		m_pWeaponDamageInfos = NULL;
	}

	DeleteAllChildren(true);

	// The DeleteAllChildren call will entail child entities being destructed before parent entities 
	// and therefore child attachment extensions before parent attachment extensions
	DetachFromParentAndChildren(
#if __ASSERT
		DETACH_FLAG_DONT_ASSERT_IF_IN_VEHICLE | DETACH_FLAG_IN_PHYSICAL_DESTRUCTOR
#else
		DETACH_FLAG_IN_PHYSICAL_DESTRUCTOR
#endif
		);

	Assertf(m_MatrixTransformPtr == static_cast<const fwMatrixTransform&>(GetTransform()).GetMatrixPtr(), "The matrix for this entity has been changed - so the m_MatrixTransformPtr was out of date");
	// GTA_RAGE_PHYSICS
//	DeleteInst(); // called in CEntity::~CEntity anyway

	BANK_ONLY(ResetDebugObjectID());
}


void CPhysical::SetIsInWater(const bool isInWater)
{
	if ( static_cast< bool >( m_nPhysicalFlags.bIsInWater ) == isInWater )
		return;

	CRenderPhaseWaterReflectionInterface::UpdateInWater(this, isInWater);

	m_nPhysicalFlags.bIsInWater = isInWater;
	UpdateWorldFromEntity();
}

// add and remove for physics world
void CPhysical::AddPhysics()
{
	//----------------------------------------------------------------------------------

	if(GetCurrentPhysicsInst())
	{
		Assert(GetTransform().IsOrthonormal());

		// update physics position
		UpdatePhysicsFromEntity(true);
	
		if(GetFragInst())
		{
		}
		else if(GetCurrentPhysicsInst()->GetArchetype() && !GetCurrentPhysicsInst()->IsInLevel())
		{
			// update physics instance's matrix to match physics entity matrix
			GetCurrentPhysicsInst()->SetMatrix(GetMatrix());

			// hack - bug 877346
			if (GetOwnedBy() == ENTITY_OWNEDBY_COMPENTITY){
				CPhysics::GetSimulator()->AddFixedObject(GetCurrentPhysicsInst());		// to get stairs working when part of a rayfire start / end object	
			} else
			// end of hack
			if(IsBaseFlagSet(fwEntity::IS_FIXED))
			{
				GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, true);
				CPhysics::GetSimulator()->AddInactiveObject(GetCurrentPhysicsInst());
				if(GetIsAttached() && GetAttachParent() != NULL && static_cast<CEntity*>(GetAttachParent())->GetIsPhysical())
				{
					CPhysical *pParent = static_cast<CPhysical*>(GetAttachParent());
					if(pParent->GetCurrentPhysicsInst() && pParent->GetCurrentPhysicsInst()->GetInstFlag(phInst::FLAG_QUERY_EXTERN_VEL))
					{
						GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_QUERY_EXTERN_VEL,true);
					}
				}
			}
			else if(!IsBaseFlagSet(fwEntity::SPAWN_PHYS_ACTIVE) || GetIsAnyFixedFlagSet() || !IsCollisionEnabled())
			{
				CPhysics::GetSimulator()->AddInactiveObject(GetCurrentPhysicsInst());
			}
			else
			{
				CPhysics::GetSimulator()->AddActiveObject(GetCurrentPhysicsInst(), false);
			}
		}

		if(GetCurrentPhysicsInst()->IsInLevel())
		{
			fwAttachmentEntityExtension *extension = GetAttachmentExtension();
			if(extension && extension->IsAttachStateBasicDerived())
			{
				extension->SetAttachFlag(ATTACH_FLAG_INST_ADDED_LATE, true);

				if( !extension->GetAttachFlag(ATTACH_FLAG_COL_ON) )
				{
					CPhysics::GetLevel()->SetInstanceIncludeFlags(GetCurrentPhysicsInst()->GetLevelIndex(), u32(ArchetypeFlags::GTA_BASIC_ATTACHMENT_INCLUDE_TYPES));
				}
				
				GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE,true);

				if(CPhysics::GetLevel()->IsActive(GetCurrentPhysicsInst()->GetLevelIndex()))
				{
					PHSIM->DeactivateObject(GetCurrentPhysicsInst());
				}
			}
		}

#if __DEV
		if(GetIsTypeVehicle() && GetCollider())
			((CVehicle *)this)->m_pDebugColliderDO_NOT_USE = GetCollider();
#endif

		if(!IsInPathServer())
		{
			if(CPathServerGta::MaybeAddDynamicObject(this))
				CAssistedMovementRouteStore::MaybeAddRouteForDoor(this);
		}

		// If game code / script has requested that this boat has collision disabled, we set the appropriate include flags here.
		if(!IsCollisionEnabled())
		{
			bool completely = false;
			if(NetworkInterface::IsGameInProgress() && GetNetworkObject())
			{
				CNetObjEntity* netEnt = SafeCast(CNetObjEntity, GetNetworkObject());
				if(netEnt)
				{
					completely = netEnt->GetDisableCollisionCompletely();
				}
			}

			DisableCollision(NULL, completely);
		}
	}

#if __ASSERT
	if(GetIsTypePed() || GetIsTypeVehicle())
	{
		// Track this AddPhysics() call for peds and vehicles
		m_physicsBacktrace = sysStack::RegisterBacktrace();
	}
#endif

	const CBaseModelInfo* pModelInfo = GetBaseModelInfo();

	if (!pModelInfo->GetHasDrawableProxyForWaterReflections())
	{
		if (GetIsTypeVehicle())
		{
			const CVehicleModelInfo* pVehicleModelInfo = static_cast<const CVehicleModelInfo*>(pModelInfo);

			if (pVehicleModelInfo->GetVehicleType() != VEHICLE_TYPE_BOAT &&
				pVehicleModelInfo->GetVehicleType() != VEHICLE_TYPE_PLANE &&
				pVehicleModelInfo->GetVehicleType() != VEHICLE_TYPE_SUBMARINE)
			{
				m_nFlags.bUseMaxDistanceForWaterReflection = true;
			}
		}
		else if (GetIsTypePed())
		{
			m_nFlags.bUseMaxDistanceForWaterReflection = true;
		}
		else if (GetIsTypeObject())
		{
			bool bHasBuoyancy = false;

			GET_2D_EFFECTS_NOLIGHTS(const_cast<CBaseModelInfo*>(pModelInfo));
			for (int i = 0; i < iNumEffects; i++)
			{
				const C2dEffect* pEffect = (*pa2dEffects)[i];

				if (pEffect->GetType() == ET_BUOYANCY)
				{
					bHasBuoyancy = true;
					break;
				}
			}

			m_nFlags.bUseMaxDistanceForWaterReflection = !bHasBuoyancy;
		}
	}
}

void CPhysical::RemovePhysics()
{
	phInst *pPhysInst = GetCurrentPhysicsInst();

	//Necessary to re-instate inst include and activation flags
	if( GetIsAttached() )
		DetachFromParent(0);

	if(pPhysInst)
	{
		if (IsUsingKinematicPhysics())
		{
			DisableKinematicPhysics();
		}

		DetachFromParentAndChildren(DETACH_FLAG_DONT_REMOVE_BASIC_ATTACHMENTS);

		if(IsFragInst(pPhysInst))
		{
			// don't do anything for fragInst's
			// the fragManager should take care of removing from physics world
		}
		else if(pPhysInst->IsInLevel())
		{
			CPhysics::GetSimulator()->DeleteObject(pPhysInst->GetLevelIndex());
		}
	}

	if(IsInPathServer())
	{
		CAssistedMovementRouteStore::MaybeRemoveRouteForDoor(this);
		CPathServerGta::MaybeRemoveDynamicObject(this);
	}
}

void CPhysical::RemoveBlip(u32 blipType)
{
	Assert((eBLIP_TYPE)blipType < BLIP_TYPE_MAX);
	if(!GetCreatedBlip())
		return;
	
	CMiniMap::RemoveBlipAttachedToEntity(this, (eBLIP_TYPE)blipType);
	SetCreatedBlip(false);
	
#if __ASSERT
	CMiniMapBlip* pBlip = CMiniMap::GetBlipAttachedToEntity(this, (eBLIP_TYPE)blipType, 0);
	if(pBlip)
	{
		Assert(!CMiniMap::IsBlipIdInUse(pBlip->m_iUniqueId));
	}
#endif
}


void CPhysical::RemoveConstraints(phInst* pInst)
{
	DetachFromParentAndChildren(DETACH_FLAG_DONT_REMOVE_BASIC_ATTACHMENTS);
	
	// Reset cuff pointers for Peds
	if (GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(this);
		if (static_cast<phInst*>(pPed->GetRagdollInst()) == pInst
			&& pPed->GetRagdollConstraintData())
		{
			pPed->GetRagdollConstraintData()->RemoveRagdollConstraints();
		}
	}

	PHCONSTRAINT->RemoveActiveConstraints(pInst);
}

void CPhysical::UpdatePhysicsFromEntity(bool bWarp)
{
	phInst* pCurrentPhysInst = GetCurrentPhysicsInst();
	if(pCurrentPhysInst)
	{
		if(bWarp)
		{
			// delete this collider from the manifold manager?
			CPhysics::ClearManifoldsForInst(pCurrentPhysInst);
			GetFrameCollisionHistory()->Reset();
		}
		const Mat34V& matrix = GetMatrixRef();

		SetPreviousPosition(VEC3V_TO_VECTOR3(matrix.GetCol3()));

		pCurrentPhysInst->SetMatrix(matrix);

		if(phCollider* pCollider = GetCollider())
		{
			pCollider->SetColliderMatrixFromInstance();

			// With new update order we need this to be set correctly even when not warping
			pCollider->SetLastInstanceMatrix(matrix);
            
			if( bWarp )
			{				
				pCollider->ClearNonPenetratingAfterTeleport();
			}

			Assertf(bWarp || !IsCollisionEnabled()|| (VEC3V_TO_VECTOR3(pCurrentPhysInst->GetMatrix().GetCol3() - pCollider->GetLastInstanceMatrix().GetCol3())).Mag2() < 100.0f, "%s:UpdatePhysicsFromEntity - moved too far without using warp", GetDebugName());

#if __DEV
			if(GetIsTypeVehicle())
				((CVehicle *)this)->m_pDebugColliderDO_NOT_USE = GetCollider();
#endif
		}

		if(pCurrentPhysInst->IsInLevel())
		{
			CPhysics::GetLevel()->UpdateObjectLocation(pCurrentPhysInst->GetLevelIndex());

			// if we're using a fragInst need to let it know we've moved as well
			if(pCurrentPhysInst->GetClassType() >= PH_INST_FRAG_GTA)
				((fragInst *)pCurrentPhysInst)->ReportMovedBySim();
		}

		// Update the entity's bounds in the pathserver system.
		if(IsInPathServer())
		{
			CPathServerGta::UpdateDynamicObject(this);
		}
	}
}

void CPhysical::ActivatePhysics()
{
	/*
#if NAVMESH_EXPORT
	// Avoid crashes in non __FINAL collision exporter tools
	if(CNavMeshDataExporter::IsExportingCollision())
		return;
#endif
	*/

#if GTA_REPLAY
	//we need a physics object for replay in some cases (e.g. to attach rope) but we don't want to ever activate it
	if(CReplayMgr::IsReplayInControlOfWorld())
	{
		return;
	}
#endif

	// might want to override this in CPed, in case we need to do stuff for ragdoll
	if(GetCurrentPhysicsInst() && GetCurrentPhysicsInst()->IsInLevel())
	{
#if __DEV
		if(GetIsAttached() && !GetAttachmentExtension()->GetAttachFlag(ATTACH_FLAG_IN_DETACH_FUNCTION)
			&& (GetAttachmentExtension()->GetAttachState()==ATTACH_STATE_BASIC||GetAttachmentExtension()->GetAttachState()==ATTACH_STATE_WORLD))
		{
			Displayf("Trying to activate object(%s) which has a BASIC attachment.", GetModelName());
			sysStack::PrintStackTrace();
		}
#endif // __DEV

		if(CPhysics::GetLevel()->IsInactive(GetCurrentPhysicsInst()->GetLevelIndex()))
			CPhysics::GetSimulator()->ActivateObject(GetCurrentPhysicsInst()->GetLevelIndex(), GetCollider());

#if __DEV
		if(GetIsTypeVehicle() && GetCollider())
			((CVehicle *)this)->m_pDebugColliderDO_NOT_USE = GetCollider();
#endif
	}
//	else
//	{
//		Assertf(false, "Trying to activate physical not in the physics level");
//		AddPhysics();
//	}
}

void CPhysical::DeActivatePhysics()
{
	// might want to override this in CPed, in case we need to do stuff for ragdoll
	if(GetCurrentPhysicsInst())
	{
		if(GetCurrentPhysicsInst()->IsInLevel())
		{
			if(CPhysics::GetLevel()->IsActive(GetCurrentPhysicsInst()->GetLevelIndex()))
				CPhysics::GetSimulator()->DeactivateObject(GetCurrentPhysicsInst()->GetLevelIndex());
		}
	}
}

void CPhysical::SetFixedPhysics(bool bFixed, bool bNetwork)
{
#if DR_ENABLED
	debugPlayback::RecordFixedStateChange(*this, bFixed, bNetwork);
#endif // DR_ENABLED

	if(GetCurrentPhysicsInst())
	{
		if(bFixed)
		{
			if (bNetwork)
			{
				if (PARAM_debugFixedByNetwork.Get())
				{
					Displayf("%s[0x%p] SETTING FIXED BY NETWORK", GetLogName(), this);
					sysStack::PrintStackTrace();
				}

				SetBaseFlag(fwEntity::IS_FIXED_BY_NETWORK);
			}
			else
				SetBaseFlag(fwEntity::IS_FIXED);

			DisablePhysicsIfFixed();

			if(GetCurrentPhysicsInst())
			{
				GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, true);
			}
		}
		else
		{
			if (bNetwork)
			{
				if (PARAM_debugFixedByNetwork.Get())
				{
					Displayf("%s[0x%p] UNSETTING FIXED BY NETWORK", GetLogName(), this);
					sysStack::PrintStackTrace();
				}

				ClearBaseFlag(fwEntity::IS_FIXED_BY_NETWORK);
			}
			else
			{
				ClearBaseFlag(fwEntity::IS_FIXED);

#if __BANK
				if(GetIsAnyFixedFlagSet())
				{
					sysStack::PrintStackTrace();
					Warningf("Unfixing something but it is still fixed: 0x%p '%s'. Fixed: %s Fixed by Network: %s", this, GetModelName(),
						IsBaseFlagSet( fwEntity::IS_FIXED ) ? "TRUE" : "FALSE", 
						IsBaseFlagSet( fwEntity::IS_FIXED_BY_NETWORK ) ? "TRUE" : "FALSE" );
				}
#endif // #if __BANK
			}
#if __ASSERT
			if(!GetIsAnyFixedFlagSet() && GetIsTypeVehicle() && static_cast<const CVehicle*>(this)->InheritsFromTrain()
				&& !static_cast<const CTrain*>(this)->AllowedToActivate())
			{
				sysStack::PrintStackTrace();
				Warningf("Someone may be trying to activate a train: 0x%p '%s'.",this,GetModelName());
			}

			if(GetIsTypeVehicle() && !IsCollisionEnabled())
			{
				sysStack::PrintStackTrace();
				Warningf("Unfixing vehicle with collision disabled 0x%p '%s'. It will probably fall through the world.",this,GetModelName());
			}
#endif // __ASSERT
			// might have been originally added as fixed, so need to 
			if(GetCurrentPhysicsInst())
			{
				// Don't reactivate the ped capsule if the disable ped capsule flag is set.
				CPed* pPed = GetIsTypePed() ? ((CPed*)this) : NULL;
				bool bReactivatePhysics = !pPed || !pPed->GetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsule) || (pPed->GetCurrentPhysicsInst()!=pPed->GetAnimatedInst());
				
				if (bReactivatePhysics)
				{
					bool bAttachedToGround = false;
					if (GetAttachmentExtension() && GetAttachmentExtension()->GetAttachState()==ATTACH_STATE_PED_ON_GROUND)
					{
						bAttachedToGround = true;
					}

					bool bBoatInLowLodMode = false;
					if(GetIsTypeVehicle() && ((CVehicle*)this)->InheritsFromBoat() && ((CBoat*)this)->GetAnchorHelper().UsingLowLodMode())
					{
						bBoatInLowLodMode = true;
					}

					//The IS_FIXED state was toggled so this object can be activated

					// If this object is attached to another object (using basic attach mode), it will be flagged such that it should
					// never activate. Don't touch this flag in that case, let the detach call reset it.
					if((bAttachedToGround || (!(GetIsAttached() &&
											(GetAttachmentExtension()->IsAttachStateBasicDerived() || GetAttachmentExtension()->GetAttachState()==ATTACH_STATE_WORLD ))))
											&& !bBoatInLowLodMode)
					{
						GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, false);
					}

					// ped's need physics to be active all the time (for now - sandy)
					// sconde: I'm not sure we need to activate peds at all any longer (shouldn't that happen as needed as part of CPed::ProcessIsAsleep?) but we certainly
					// don't want to activate dead peds as their mover collision has been setup to only collide with the map so if they are on top of a physical object/vehicle
					// they will fall right through on activation
					if(GetIsTypePed() && !GetIsAnyFixedFlagSet() && GetCurrentPhysicsInst()->IsInLevel() && (static_cast<CPed*>(this)->GetDeathState() != DeathState_Dead || !static_cast<CPed*>(this)->ShouldBeDead()))
					{
						if(!GetIsAttached() || !(GetAttachmentExtension()->IsAttachStateBasicDerived()
							|| GetAttachmentExtension()->GetAttachState()==ATTACH_STATE_WORLD) )
						{
							ActivatePhysics();
						}
					}
				}
			}
		}
	}
	else
	{
		if (bNetwork)
			AssignBaseFlag(fwEntity::IS_FIXED_BY_NETWORK, bFixed);
		else
			AssignBaseFlag(fwEntity::IS_FIXED, bFixed);
	}
}

void CPhysical::DisablePhysicsIfFixed()
{
	if(GetIsAnyFixedFlagSet())
	{
		if(GetCurrentPhysicsInst() && GetCurrentPhysicsInst()->IsInLevel() && CPhysics::GetLevel()->IsActive(GetCurrentPhysicsInst()->GetLevelIndex()))
		{
			if(GetIsTypePed())
				static_cast<CPed*>(this)->DeactivatePhysicsAndDealWithRagdoll();
			else
				DeActivatePhysics();
			Assertf(GetCollider() == NULL,"Was unable to deactivate object '%s' even though it should be fixed.",GetDebugName());
		}
	}
}



void CPhysical::SetVelocity(const Vector3& vecMoveSpeed)
{
	if(GetCurrentPhysicsInst()==NULL || !GetCurrentPhysicsInst()->IsInLevel())
		return;

	physicsAssertf( GetIgnoreVelocityCheck() || GetCurrentPhysicsInst()->GetArchetype() && vecMoveSpeed.Mag2() <=
		(square(GetCurrentPhysicsInst()->GetArchetype()->GetMaxSpeed())+0.01f),
		"vecMoveSpeed (%f) is above archetype's maximum allowed speed of %f (%s)",
		vecMoveSpeed.Mag(), GetCurrentPhysicsInst()->GetArchetype()->GetMaxSpeed(), GetModelName());

	Assert(vecMoveSpeed.x==vecMoveSpeed.x && vecMoveSpeed.y==vecMoveSpeed.y && vecMoveSpeed.z==vecMoveSpeed.z);

	// wake up if it's not active
	if(CPhysics::GetLevel()->IsInactive(GetCurrentPhysicsInst()->GetLevelIndex())
	&& (vecMoveSpeed.x!=0.0f || vecMoveSpeed.y!=0.0f || vecMoveSpeed.z!=0.0f))
		ActivatePhysics();	

	if(GetCollider() && GetCurrentPhysicsInst()->IsInLevel())
	{				
		Assertf(!GetIsTypePed() || IsLessThanAll(MagSquared(RCC_VEC3V(vecMoveSpeed)), ScalarV(DEFAULT_VELOCITY_LIMIT * DEFAULT_VELOCITY_LIMIT)), "Setting invalid collider velocity:  (%f, %f, %f).", 
			VEC3V_ARGS(RCC_VEC3V(vecMoveSpeed)));

		GetCollider()->SetVelocity(vecMoveSpeed);
		GetCollider()->ClampVelocity();
	}
}

const Vector3& CPhysical::GetVelocity() const
{
	const phCollider* pCollider = GetCollider();
	if (pCollider)
	{
		Assertf(!GetIsTypePed() || IsLessThanAll(MagSquared(pCollider->GetVelocity()), ScalarV(DEFAULT_VELOCITY_LIMIT * DEFAULT_VELOCITY_LIMIT)), "Collider velocity was invalid:  (%f, %f, %f). Articulated(%s)", 
			VEC3V_ARGS(pCollider->GetVelocity()), (pCollider->IsArticulated() ? "true" : "false"));
		Assertf(IsFiniteAll(pCollider->GetVelocity()), "Collider velocity was invalid: (%f, %f, %f). Articulated(%s)", VEC3V_ARGS(pCollider->GetVelocity()), pCollider->IsArticulated() ? "true" : "false");
		return RCC_VECTOR3(pCollider->GetVelocity());
	}
	else if(m_Buoyancy.m_buoyancyFlags.bLowLodBuoyancyMode)
	{
		Assertf(IsFiniteAll(RCC_VEC3V(m_Buoyancy.GetVelocityWhenInLowLodMode())), "Low LOD buoyancy velocity was invalid: (%f, %f, %f).",
			VEC3V_ARGS(RCC_VEC3V(m_Buoyancy.GetVelocityWhenInLowLodMode())));

		return m_Buoyancy.GetVelocityWhenInLowLodMode();
	}

	return VEC3_ZERO;
}

const Vector3& CPhysical::GetReferenceFrameVelocity() const
{
	const phCollider* pCollider = GetCollider();
	if (pCollider)
	{
		return RCC_VECTOR3(pCollider->GetReferenceFrameVelocity());
	}
	if(GetIsAttached() && GetAttachParent() != NULL && static_cast<CEntity*>(GetAttachParent())->GetIsTypeVehicle() && static_cast<CVehicle *>(GetAttachParent())->InheritsFromTrain())
	{
		CPhysical* pAttachParent = static_cast<CPhysical*>(GetAttachParent());
		return pAttachParent->GetVelocity();
	}
	return VEC3_ZERO;
}

const Vector3& CPhysical::GetReferenceFrameAngularVelocity() const
{
	const phCollider* pCollider = GetCollider();
	if (pCollider)
	{
		return RCC_VECTOR3(pCollider->GetReferenceFrameAngularVelocity());
	}
	if(GetIsAttached() && GetAttachParent() != NULL && static_cast<CEntity*>(GetAttachParent())->GetIsTypeVehicle() && static_cast<CVehicle *>(GetAttachParent())->InheritsFromTrain())
	{
		CPhysical* pAttachParent = static_cast<CPhysical*>(GetAttachParent());
		return pAttachParent->GetAngVelocity();
	}
	return VEC3_ZERO;
}

Vec3V_Out CPhysical::CalculateVelocityFromMatrices(Mat34V_In mOld, Mat34V_In mNew, ScalarV_In scInvTimeStep) const
{
	//Calculate the velocity.
	Vec3V vVelocity = (mNew.GetCol3() - mOld.GetCol3()) * scInvTimeStep;
	
	return vVelocity;
}

void CPhysical::SetVelocityFromMatrices(Mat34V_In mOld, Mat34V_In mNew, ScalarV_In scInvTimeStep)
{
	//Set the velocity.
	SetVelocity(VEC3V_TO_VECTOR3(CalculateVelocityFromMatrices(mOld, mNew, scInvTimeStep)));
}

void CPhysical::SetAngVelocity(const Vector3& vecTurnSpeed)
{
	if(GetCurrentPhysicsInst()==NULL || !GetCurrentPhysicsInst()->IsInLevel())
		return;

	Assertf(vecTurnSpeed.Mag() < DEFAULT_MAX_ANG_SPEED * 15.0f,"CPhysical::SetAngVelocity vecTurnSpeed[%f %f %f][%f] !< (DEFAULT_MAX_ANG_SPEED[%f] * 15.0f)[%f]",vecTurnSpeed.x,vecTurnSpeed.y,vecTurnSpeed.z,vecTurnSpeed.Mag(),DEFAULT_MAX_ANG_SPEED,(DEFAULT_MAX_ANG_SPEED * 15.0f));
	Assert(vecTurnSpeed.x==vecTurnSpeed.x && vecTurnSpeed.y==vecTurnSpeed.y && vecTurnSpeed.z==vecTurnSpeed.z);


	// wake up if it's not active
	if(CPhysics::GetLevel()->IsInactive(GetCurrentPhysicsInst()->GetLevelIndex())
	&& (vecTurnSpeed.x!=0.0f || vecTurnSpeed.y!=0.0f || vecTurnSpeed.z!=0.0f))
		ActivatePhysics();

	if(GetCollider() && GetCurrentPhysicsInst()->IsInLevel())
		GetCollider()->SetAngVelocity(vecTurnSpeed);
}

const Vector3& CPhysical::GetAngVelocity() const
{
	const phCollider* pCollider = GetCollider();
	if (pCollider)
		return RCC_VECTOR3(pCollider->GetAngVelocity());

	return VEC3_ZERO;
}

Vec3V_Out CPhysical::CalculateAngVelocityFromQuaternions(QuatV_In qOrientationOld, QuatV_In qOrientationNew, ScalarV_In scInvTimeStep) const
{
	//Calculate the angular velocity from the orientations.
	Vec3V vAngularVelocity;
	if(!IsCloseAll(qOrientationOld, qOrientationNew, ScalarV(V_FLT_SMALL_6)))
	{
		//Calculate the change in orientation.
		QuatV qOrientationChange = Multiply(qOrientationNew, Invert(qOrientationOld));

		//Calculate the angular velocity.
		vAngularVelocity = Scale(ComputeAxisAnglePrecise(qOrientationChange),scInvTimeStep);
	}
	else
	{
		//Clear the angular velocity.
		vAngularVelocity = Vec3V(V_ZERO);
	}

	return vAngularVelocity;
}

Vec3V_Out CPhysical::CalculateAngVelocityFromMatrices(Mat34V_In mOld, Mat34V_In mNew, ScalarV_In scInvTimeStep) const
{
	//Grab the old orientation.
	QuatV qOrientationOld = QuatVFromMat33V(mOld.GetMat33());

	//Grab the new orientation.
	QuatV qOrientationNew = QuatVFromMat33V(mNew.GetMat33());

	return CalculateAngVelocityFromQuaternions(qOrientationOld,qOrientationNew,scInvTimeStep);
}

void CPhysical::SetAngVelocityFromMatrices(Mat34V_In mOld, Mat34V_In mNew, ScalarV_In scInvTimeStep)
{
	//Set the angular velocity.
	SetAngVelocity(VEC3V_TO_VECTOR3(CalculateAngVelocityFromMatrices(mOld, mNew, scInvTimeStep)));
}

#if ENABLE_MATRIX_MEMBER
void CPhysical::SetTransform(Mat34V_In transform, bool bUpdateGameWorld /* = true */, bool bUpdatePhysics /* = true */, bool bWarp /* = false */)
#else
void CPhysical::SetTransform(fwTransform* transform, bool bUpdateGameWorld /* = true */, bool bUpdatePhysics /* = true */, bool bWarp /* = false */)
#endif
{	
#if ENABLE_MATRIX_MEMBER	
	CDynamicEntity::SetTransform(transform, bUpdateGameWorld, bUpdatePhysics, bWarp);
	m_MatrixTransformPtr = CDynamicEntity::GetTransform().GetMatrixPtr();
#else
	Assertf(transform->IsMatrixTransform(), "All CPhysicals are expected to have matrix transforms!");
	CDynamicEntity::SetTransform(transform, bUpdateGameWorld, bUpdatePhysics, bWarp);
	m_MatrixTransformPtr = static_cast<fwMatrixTransform*>(transform)->GetMatrixPtr();
#endif
	
}

Mat34V_Out CPhysical::GetMatrixPrePhysicsUpdate() const
{
	//Grab the physics instance.
	phInst* pInst = GetCurrentPhysicsInst();
	if(pInst)
	{
		return pInst->GetMatrix();
	}
	else
	{
		return GetMatrix();
	}
}

void CPhysical::SetMass(float fNewMass)
{
	Assert(fNewMass > 0.0f);
	
	phArchetypeDamp* pGtaArchetype = GetPhysArch();
	if(pGtaArchetype)
	{
		CBaseModelInfo* pModelInfo = GetBaseModelInfo();

		// check if object still has original physics archetype
		if(pModelInfo && pModelInfo->GetPhysics() == pGtaArchetype)
		{
			// if so then we need to clone it so that we can modify values
			pGtaArchetype = static_cast<phArchetypeDamp*>(pGtaArchetype->Clone());

			// pass new archetype to the objects physinst -> should have 1 ref only
			GetCurrentPhysicsInst()->SetArchetype(pGtaArchetype);
		}

		pGtaArchetype->SetMass(fNewMass);

		Vector3 angInertia = VEC3V_TO_VECTOR3(pGtaArchetype->GetBound()->GetComputeAngularInertia(fNewMass));
		pGtaArchetype->SetAngInertia(angInertia);

		if(GetCollider())
		{
			// If object is active then collider needs updating with new mass and ang inertia
			GetCollider()->InitInertia();
		}
	}
}

void CPhysical::SetHealth(const float fNewHealth, bool ASSERT_ONLY(bNetCall), bool ASSERT_ONLY(bAssertMissingDamageEvents))
{
	// can't change health on objects controlled by another machine
#if __ASSERT
	if (!bNetCall && GetNetworkObject())
	{
		Assertf(!GetNetworkObject()->IsClone(), "Clone %s is having health altered illegally", GetNetworkObject()->GetLogName());			
		Assertf(!GetNetworkObject()->IsPendingOwnerChange() || m_fHealth==fNewHealth, "%s is having health altered illegally while migrating", GetNetworkObject()->GetLogName());			
	}
#endif

	// trigger a damaged event
	if (fNewHealth < m_fHealth)
	{
		GetEventGlobalGroup()->Add(CEventEntityDamaged(this));
	}

	// trigger a destroyed event
	if (m_fHealth > 0.0001f && fNewHealth < 0.0001f)
	{
		GetEventGlobalGroup()->Add(CEventEntityDestroyed(this));
	}

	if (GetIsTypePed())
	{
		CPlayerSpecialAbilityManager::ChargeEvent(ACET_RESET_LOW_HEALTH, static_cast<CPed*>(this));

		// set or remove the injured status if appropriate
		CPed* pPed= static_cast<CPed*>(this);

#if __ASSERT && __BANK
		//Debug missing damage events due to ped health being set to 0 without calling UpdateDamageTracker()
		if (pPed->GetNetworkObject() && NetworkDebug::DebugPedMissingDamageEvents() && bAssertMissingDamageEvents)
		{
			const bool isTheFatalBlow = (m_fHealth > pPed->GetDyingHealthThreshold() && fNewHealth < pPed->GetDyingHealthThreshold());
			if (isTheFatalBlow)
			{
				static_cast<CNetObjPed*>(pPed->GetNetworkObject())->CheckDamageEventOnDie();
			}
		}
#endif // __ASSERT && __BANK

		if (pPed->ShouldUseInjuredMovement())
		{
			if (!pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInjured))
			{
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsInjured, true);
				// don't reset the on foot clip set immediately, it will be applied once its streamed in
				// (See CPed::ProcessControl_Animation)
			}
		}
		else
		{
			if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInjured))
			{
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_IsInjured, false);
				// if we're using the injured movement set, get rid of it.
				CTaskMotionBase* pTask = pPed->GetPrimaryMotionTaskIfExists();
				if ( pTask)
				{
					pTask->ResetOnFootClipSet(true);
				}
			}
		}
	}

#if __BANK
	if (IsNetworkClone() && m_fHealth > 0.0f && fNewHealth == 0.0f && GetIsTypePed())
	{
		const CPed* pPed = static_cast<const CPed*>(this);
		AI_LOG("[Health]\n");
		AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED(pPed, "Clone ped %s is setting health to zero, previous health %.2f\n", AILogging::GetDynamicEntityNameSafe(pPed), GetHealth());
		AI_LOG_STACK_TRACE(1);
		AI_LOG("[/Health]\n");
	}
#endif // __BANK

	m_fHealth=fNewHealth;
}

void  CPhysical::ChangeHealth( float fHealthChange )  
{
	// can't change health on objects controlled by another machine
	Assert(!IsNetworkClone() || fHealthChange == 0.0f);

	// trigger a damaged event
	if (fHealthChange < 0.0f)
	{
		GetEventGlobalGroup()->Add(CEventEntityDamaged(this));
	}

	// trigger a destroyed event
	if (m_fHealth > 0.0001f && -fHealthChange > m_fHealth)
	{
		GetEventGlobalGroup()->Add(CEventEntityDestroyed(this));
	}

	m_fHealth += fHealthChange; 

	if (GetIsTypePed())
	{
		CPlayerSpecialAbilityManager::ChargeEvent(ACET_CHANGED_HEALTH, static_cast<CPed*>(this), m_fHealth);
	}
}

float CPhysical::GetMaxSpeed() const
{
	CBaseModelInfo* pBaseModelInfo		= GetBaseModelInfo();
	phInst*			pCurrentPhysicsInst	= GetCurrentPhysicsInst();
	phArchetype*	pArchetype			= NULL;

	if( pBaseModelInfo && pCurrentPhysicsInst )
	{
		pArchetype = pCurrentPhysicsInst->GetArchetype();
	}

	if( pArchetype )
	{
		phArchetypeDamp* pPhysics = pBaseModelInfo->GetPhysics();

		if( pPhysics )
		{
			return pPhysics->GetMaxSpeed();
		}
	}

	return DEFAULT_MAX_SPEED;
}

void CPhysical::SetWeaponDamageInfo(CEntity* pEntity, u32 hash, u32 time, bool meleeHit)
{
	WeaponDamageInfo weaponDamageInfo;
	weaponDamageInfo.pWeaponDamageEntity = pEntity;
	weaponDamageInfo.weaponDamageHash = hash;
	weaponDamageInfo.weaponDamageTime = time;
	weaponDamageInfo.bMeleeHit = meleeHit;

	SetWeaponDamageInfo(weaponDamageInfo);
}

void CPhysical::SetWeaponDamageInfo(WeaponDamageInfo weaponDamageInfo)
{
	if(!m_pWeaponDamageInfos)
	{
		m_pWeaponDamageInfos = rage_new WeaponDamageInfos;
		Assert(m_pWeaponDamageInfos);
	}

	int iIndex = m_pWeaponDamageInfos->Find(weaponDamageInfo);
	if (iIndex != -1)
	{
		// Keep it at the top most of the list.
		if(iIndex != m_pWeaponDamageInfos->GetCount() - 1)
		{
			m_pWeaponDamageInfos->Delete(iIndex);
			m_pWeaponDamageInfos->Push(weaponDamageInfo);
		}
		else
		{
			// Update damage time.
			(*m_pWeaponDamageInfos)[iIndex].weaponDamageTime = weaponDamageInfo.weaponDamageTime;
			(*m_pWeaponDamageInfos)[iIndex].bMeleeHit = weaponDamageInfo.bMeleeHit;
		}

		return;
	}

	if(m_pWeaponDamageInfos->IsFull())
	{
		m_pWeaponDamageInfos->Delete(0);
	}
	m_pWeaponDamageInfos->Push(weaponDamageInfo); 

	CEntity* pEntity = weaponDamageInfo.pWeaponDamageEntity;
	if (pEntity)
	{
		CPed *pInflictorPed = NULL;
		if (pEntity->GetIsTypeVehicle())
		{
			CVehicle *pInflictorVehicle = (CVehicle *)pEntity;
			pInflictorPed = pInflictorVehicle->GetDriver();
		}
		else if (pEntity->GetIsTypePed())
		{
			pInflictorPed = (CPed *)pEntity;
		}

		if (pInflictorPed && pInflictorPed->IsPlayer())
		{
			CPlayerInfo *pPlayerInfo = pInflictorPed->GetPlayerInfo();
			Assertf(pPlayerInfo, "CPhysical::SetWeaponDamageInfo - player ped doesn't have valid PlayerInfo");
			if (pPlayerInfo)
			{
#if __BANK
				if (NetworkInterface::IsGameInProgress() && NetworkDebug::DebugWeaponDamageEvent() && GetIsTypeVehicle())
				{
					const CVehicle* vec = static_cast<const CVehicle*>(this);
					if (vec)
					{
						netObject* obj = vec->GetNetworkObject();
						netObject* objinflictor = pInflictorPed->GetNetworkObject();
						if (obj && objinflictor)
						{
							gnetWarning("[DebugWeaponDamageEvent] SetWeaponDamageInfo: Vehicle:%s, Inflictor:%s", obj->GetLogName(), objinflictor->GetLogName());
							sysStack::PrintStackTrace();
						}
					}
				}
#endif // __BANK

				//It's possible that the ped is punching a wall and doing damage to them self, don't allow this.
				if (this->GetIsTypePed() && pInflictorPed != this)
				{
					pPlayerInfo->bHasDamagedAtLeastOnePed = true;
					if(!CPedType::IsAnimalType(((CPed*)this)->GetPedType()))
					{
						pPlayerInfo->bHasDamagedAtLeastOneNonAnimalPed = true;
					}
				}
				else if (this->GetIsTypeVehicle())
				{
					pPlayerInfo->bHasDamagedAtLeastOneVehicle = true;
				}
			}
		}
	}
}

u32 CPhysical::GetWeaponDamagedTimeByEntity(CEntity* pEntity) const
{
	if(m_pWeaponDamageInfos)
	{
		for(int i = 0; i < m_pWeaponDamageInfos->GetCount(); i++)
		{
			if((*m_pWeaponDamageInfos)[i].pWeaponDamageEntity == pEntity)
			{
				return (*m_pWeaponDamageInfos)[i].weaponDamageTime;
			}
		}
	}

	return 0;
}

u32 CPhysical::GetWeaponDamagedTimeByHash(u32 uWeaponHash) const
{
	if(m_pWeaponDamageInfos)
	{
		for(int i = 0; i < m_pWeaponDamageInfos->GetCount(); i++)
		{
			if((*m_pWeaponDamageInfos)[i].weaponDamageHash == uWeaponHash)
			{
				return (*m_pWeaponDamageInfos)[i].weaponDamageTime;
			}
		}
	}

	return 0;
}

void CPhysical::MakeAnonymousDamageByEntity(CEntity* pEntity) 
{
	if(m_pWeaponDamageInfos)
	{
		for(int i = 0; i < m_pWeaponDamageInfos->GetCount(); i++)
		{
			if((*m_pWeaponDamageInfos)[i].pWeaponDamageEntity == pEntity)
			{
				(*m_pWeaponDamageInfos)[i].pWeaponDamageEntity = NULL;
			}
		}
	}
}

//void CPhysical::SetWeaponDamageEntity(CEntity* pEntity)
//{
//	if(!m_pWeaponDamageEntities)
//	{
//		m_pWeaponDamageEntities = rage_new DamageEntities;
//		Assert(m_pWeaponDamageEntities);
//	}
//
//	RegdEnt pRegedEnt(pEntity);
//	int iIndex = m_pWeaponDamageEntities->Find(pRegedEnt);
//	if (iIndex != -1)
//	{
//		// Keep it at the top most of the list.
//		if(iIndex != m_pWeaponDamageEntities->GetCount() - 1)
//		{
//			m_pWeaponDamageEntities->Delete(iIndex);
//			m_pWeaponDamageEntities->Push(pRegedEnt);
//		}
//		return;
//	}
//
//	if(m_pWeaponDamageEntities->IsFull())
//	{
//		m_pWeaponDamageEntities->Delete(0);
//	}
//	m_pWeaponDamageEntities->Push(pRegedEnt); 
//
//	if (pEntity)
//	{
//		CPed *pInflictorPed = NULL;
//		if (pEntity->GetIsTypeVehicle())
//		{
//			CVehicle *pInflictorVehicle = (CVehicle *)pEntity;
//			pInflictorPed = pInflictorVehicle->GetDriver();
//		}
//		else if (pEntity->GetIsTypePed())
//		{
//			pInflictorPed = (CPed *)pEntity;
//		}
//
//		if (pInflictorPed && pInflictorPed->IsPlayer())
//		{
//			CPlayerInfo *pPlayerInfo = pInflictorPed->GetPlayerInfo();
//			Assertf(pPlayerInfo, "CPhysical::SetWeaponDamageEntity - player ped doesn't have valid PlayerInfo");
//			if (pPlayerInfo)
//			{
//#if __BANK
//				if (NetworkInterface::IsGameInProgress() && NetworkDebug::DebugWeaponDamageEvent() && GetIsTypeVehicle())
//				{
//					const CVehicle* vec = static_cast<const CVehicle*>(this);
//					if (vec)
//					{
//						netObject* obj = vec->GetNetworkObject();
//						netObject* objinflictor = pInflictorPed->GetNetworkObject();
//						if (obj && objinflictor)
//						{
//							gnetWarning("[DebugWeaponDamageEvent] SetWeaponDamageEntity: Vehicle:%s, Inflictor:%s", obj->GetLogName(), objinflictor->GetLogName());
//							sysStack::PrintStackTrace();
//						}
//					}
//				}
//#endif // __BANK
//				
//				//It's possible that the ped is punching a wall and doing damage to them self, don't allow this.
//				if (this->GetIsTypePed() && pInflictorPed != this)
//				{
//					pPlayerInfo->bHasDamagedAtLeastOnePed = true;
//				}
//				else if (this->GetIsTypeVehicle())
//				{
//					pPlayerInfo->bHasDamagedAtLeastOneVehicle = true;
//				}
//			}
//		}
//	}
//}

CEntity* CPhysical::GetWeaponDamageEntityPedSafe() const 
{ 
	CEntity* pEntity = GetWeaponDamageEntity();

	// To be super safe now at the end of the project, this check could probably be removed after GTA V ships
	if (NetworkInterface::IsGameInProgress())
	{
		// Last resort digging out last ped damager
		if (m_pWeaponDamageInfos && (!pEntity || !pEntity->GetIsTypePed()))
		{
			for(int i = m_pWeaponDamageInfos->GetCount() - 1; i >= 0; --i)
			{
				if((*m_pWeaponDamageInfos)[i].pWeaponDamageEntity && (*m_pWeaponDamageInfos)[i].pWeaponDamageEntity->GetIsTypePed())
				{
					pEntity = (*m_pWeaponDamageInfos)[i].pWeaponDamageEntity;
					break;
				}
			}
		}
	}

	return pEntity;
}

bool CPhysical::HasBeenDamagedByEntity(CEntity* pEntity) const
{
	if(!m_pWeaponDamageInfos)
	{
		return false;
	}

	for(int i = 0; i < m_pWeaponDamageInfos->GetCount(); i++)
	{
		if((*m_pWeaponDamageInfos)[i].pWeaponDamageEntity == pEntity)
		{
			return true;
		}
	}

	return false;
}

bool CPhysical::HasBeenDamagedByHash(u32 hash) const
{
	if(!m_pWeaponDamageInfos)
	{
		return false;
	}

	for(int i = 0; i < m_pWeaponDamageInfos->GetCount(); i++)
	{
		if((*m_pWeaponDamageInfos)[i].weaponDamageHash == hash)
		{
			return true;
		}
	}

	return false;
}

bool CPhysical::HasBeenDamagedByGeneralWeaponType(int generalWeaponType) const
{
	if(!m_pWeaponDamageInfos)
	{
		return false;
	}

	for(int i = 0; i < m_pWeaponDamageInfos->GetCount(); i++)
	{
		if(CWeaponInfoManager::CheckDamagedGeneralWeaponType((*m_pWeaponDamageInfos)[i].weaponDamageHash, generalWeaponType))
		{
			return true;
		}
	}

	return false;
}

bool CPhysical::HasBeenDamagedByAnyPed() const
{
	if(!m_pWeaponDamageInfos)
	{
		return false;
	}

	for(int i = 0; i < m_pWeaponDamageInfos->GetCount(); i++)
	{
		if((*m_pWeaponDamageInfos)[i].pWeaponDamageEntity && (*m_pWeaponDamageInfos)[i].pWeaponDamageEntity->GetIsTypePed())
		{
			return true;
		}
	}

	return false;
}

bool CPhysical::HasBeenDamagedByAnyObject() const
{
	if(!m_pWeaponDamageInfos)
	{
		return false;
	}

	for(int i = 0; i < m_pWeaponDamageInfos->GetCount(); i++)
	{
		if((*m_pWeaponDamageInfos)[i].pWeaponDamageEntity  && (*m_pWeaponDamageInfos)[i].pWeaponDamageEntity->GetIsTypeObject())
		{
			return true;
		}
	}

	return false;
}

bool CPhysical::HasBeenDamagedByAnyVehicle() const
{
	if(!m_pWeaponDamageInfos)
	{
		return false;
	}
	
	// We store the driver rather than the vehicle now (in CPedDamageCalculator::ApplyDamageAndComputeResponse). So we need to check the weapon hash for vehicle damage.
	for(int i = 0; i < m_pWeaponDamageInfos->GetCount(); i++)
	{
		if((*m_pWeaponDamageInfos)[i].pWeaponDamageEntity && (*m_pWeaponDamageInfos)[i].pWeaponDamageEntity->GetIsTypeVehicle())
		{
			return true;
		}

		if((*m_pWeaponDamageInfos)[i].weaponDamageHash == WEAPONTYPE_RUNOVERBYVEHICLE || (*m_pWeaponDamageInfos)[i].weaponDamageHash == WEAPONTYPE_RAMMEDBYVEHICLE)
		{
			return true;
		}
	}

	return false;
}

#if __ASSERT
void  CPhysical::SpewDamageHistory( ) const
{
	if (!m_pWeaponDamageInfos)
		return;

	pedDebugf3("[PedDamageCalculator] Damage History for: %s", GetDebugName());

	const u32 currTime = sysTimer::GetSystemMsTime();

	for (int i=0; i<m_pWeaponDamageInfos->GetCount(); i++)
	{
		const WeaponDamageInfo& damageInfo = (*m_pWeaponDamageInfos)[i];

		const char* dname = "NULL"; 
		CEntity* entity = damageInfo.pWeaponDamageEntity;
		if (entity)
		{
			dname = entity->GetDebugName();
			if (entity->GetIsPhysical())
			{
				CPhysical* physical = SafeCast(CPhysical, entity);
				dname = physical->GetDebugName();
			}
		}

		const CWeaponInfo* wi = CWeaponInfoManager::GetInfo< CWeaponInfo >(damageInfo.weaponDamageHash);
		const char* wname = (wi && wi->GetName()) ? wi->GetName() : "unknown";

		pedDebugf3("[PedDamageCalculator] .... Damage info for index: %d", i);
		pedDebugf3("[PedDamageCalculator] .......... Inflictor: %s", dname);
		pedDebugf3("[PedDamageCalculator] ............. Weapon: %s, %u", wname, damageInfo.weaponDamageHash);
		pedDebugf3("[PedDamageCalculator] ............... Time: %u (%u)", currTime - damageInfo.weaponDamageTime, currTime);
	}
}
#endif

void CPhysical::SetupMissionState()
{
	CDynamicEntity::SetupMissionState();

	BANK_ONLY(InitDebugName());
}

void CPhysical::ClearAllCachedObjectCoverObstructed()
{
	// Get a handle to the extension, if any
	CObjectCoverExtension* pExtension = GetExtension<CObjectCoverExtension>();
	if( pExtension )
	{
		pExtension->ClearAllCachedObjectCoverObstructed();
	}
}

void CPhysical::SetCachedObjectCoverObstructed(int cachedObjectCoverIndex)
{
	// Get a handle to the extension, if any
	CObjectCoverExtension* pExtension = GetExtension<CObjectCoverExtension>();
	if( !pExtension )
	{
		// need to create the extension first
		pExtension = rage_new CObjectCoverExtension;
		GetExtensionList().Add( *pExtension );
	}

	// mark the given index bit
	if( AssertVerify(pExtension) )
	{
		pExtension->SetCachedObjectCoverObstructed(cachedObjectCoverIndex);	
	}
}

bool CPhysical::HasExpiredObjectCoverObstructed() const
{
	// Get a handle to the extension, if any
	const CObjectCoverExtension* pExtension = GetExtension<CObjectCoverExtension>();
	if( pExtension )
	{
		// report whether obstruction was marked and has expired
		return pExtension->IsCoverObstructedTimeExpired();
	}

	// by default no obstruction marked
	return false;
}

bool CPhysical::IsCachedObjectCoverObstructed(int cachedObjectCoverIndex) const
{
	// Get a handle to the extension, if any
	const CObjectCoverExtension* pExtension = GetExtension<CObjectCoverExtension>();
	if( pExtension )
	{
		// report the status for the requested index
		return pExtension->IsCachedObjectCoverObstructed(cachedObjectCoverIndex);
	}

	// by default no obstruction marked
	return false;
}

void CPhysical::UpdateEntityFromPhysics(phInst *pInst, int nLoop)
{
	bool bInvalidResult = false;

/*	VecBoolV finite = (	IsFinite(pInst->GetMatrix().GetCol0()) & 
						IsFinite(pInst->GetMatrix().GetCol1()) &
						IsFinite(pInst->GetMatrix().GetCol2()) &
						IsFinite(pInst->GetMatrix().GetCol3())		);

	// don't care about the 4th column
	finite |= VecBoolV(V_F_F_F_T);*/
	bool isFinite = (	FPIsFinite(pInst->GetMatrix().GetCol0().GetXf()) & FPIsFinite(pInst->GetMatrix().GetCol0().GetYf()) & FPIsFinite(pInst->GetMatrix().GetCol0().GetZf()) &
						FPIsFinite(pInst->GetMatrix().GetCol1().GetXf()) & FPIsFinite(pInst->GetMatrix().GetCol1().GetYf()) & FPIsFinite(pInst->GetMatrix().GetCol1().GetZf()) &
						FPIsFinite(pInst->GetMatrix().GetCol2().GetXf()) & FPIsFinite(pInst->GetMatrix().GetCol2().GetYf()) & FPIsFinite(pInst->GetMatrix().GetCol2().GetZf()) &
						FPIsFinite(pInst->GetMatrix().GetCol3().GetXf()) & FPIsFinite(pInst->GetMatrix().GetCol3().GetYf()) & FPIsFinite(pInst->GetMatrix().GetCol3().GetZf()) );


	if (!isFinite)//IsEqualIntAll(finite,VecBoolV(V_T_T_T_T)))
	{
		Assertf(false, "%s:Physics returned an invalid matrix for object:\n[%5.3f\t %5.3f\t %5.3f\t %5.3f\t]\n[%5.3f\t %5.3f\t %5.3f\t %5.3f\t]\n[%5.3f\t %5.3f\t %5.3f\t %5.3f\t]\n",
			GetDebugName(),
			pInst->GetMatrix().GetCol0().GetXf(), pInst->GetMatrix().GetCol1().GetXf(), pInst->GetMatrix().GetCol2().GetXf(), pInst->GetMatrix().GetCol3().GetXf(),
			pInst->GetMatrix().GetCol0().GetYf(), pInst->GetMatrix().GetCol1().GetYf(), pInst->GetMatrix().GetCol2().GetYf(), pInst->GetMatrix().GetCol3().GetYf(),
			pInst->GetMatrix().GetCol0().GetZf(), pInst->GetMatrix().GetCol1().GetZf(), pInst->GetMatrix().GetCol2().GetZf(), pInst->GetMatrix().GetCol3().GetZf());
		bInvalidResult = true;
	}
	else if(!IsEqualIntAll(pInst->GetMatrix().IsOrthonormal3x3V(ScalarVFromF32(REJUVENATE_ERROR_NEW_VEC)), VecBoolV(V_T_T_T_T)))
	{
		Assertf(false, "%s:Physics returned a non-ortho matrix:\n[%5.3f\t %5.3f\t %5.3f\t %5.3f\t]\n[%5.3f\t %5.3f\t %5.3f\t %5.3f\t]\n[%5.3f\t %5.3f\t %5.3f\t %5.3f\t]\n",
			GetDebugName(),
			pInst->GetMatrix().GetCol0().GetXf(), pInst->GetMatrix().GetCol1().GetXf(), pInst->GetMatrix().GetCol2().GetXf(), pInst->GetMatrix().GetCol3().GetXf(),
			pInst->GetMatrix().GetCol0().GetYf(), pInst->GetMatrix().GetCol1().GetYf(), pInst->GetMatrix().GetCol2().GetYf(), pInst->GetMatrix().GetCol3().GetYf(),
			pInst->GetMatrix().GetCol0().GetZf(), pInst->GetMatrix().GetCol1().GetZf(), pInst->GetMatrix().GetCol2().GetZf(), pInst->GetMatrix().GetCol3().GetZf());
		bInvalidResult = true;
	}
	else if(MagSquared(pInst->GetMatrix().GetCol3()).Getf() > 1.0e12f)	// 1,000km squared
	{
		ASSERT_ONLY( const Vec3V currentPos = CEntity::GetTransform().GetPosition(); )
		Assertf(false, "%s:Physics returned a position outside world (%5.3f, %5.3f, %5.3f). Entity is currently at (%5.3f, %5.3f, %5.3f).", GetDebugName(),
			pInst->GetMatrix().GetCol3().GetXf(), pInst->GetMatrix().GetCol3().GetYf(), pInst->GetMatrix().GetCol3().GetZf(),
			currentPos.GetXf(), currentPos.GetYf(), currentPos.GetZf() );
		bInvalidResult = true;
	}
	else if( (pInst->GetArchetype()->GetBound()->GetRadiusAroundCentroid()!=pInst->GetArchetype()->GetBound()->GetRadiusAroundCentroid()) || 
		(!m_nPhysicalFlags.bModifiedBounds && pInst->GetArchetype()->GetBound()->GetRadiusAroundCentroid() > 5.0f * GetBaseModelInfo()->GetBoundingSphereRadius()))
	{
		// For vehicles we can sometimes validly sweep wheels a long way away from the centroid so increase the radius tolerance with speed.
		if(!GetIsTypeVehicle() || 
			(!m_nPhysicalFlags.bModifiedBounds && pInst->GetArchetype()->GetBound()->GetRadiusAroundCentroid() > (4.0f + (fwTimer::GetTimeStep() * GetVelocity().Mag())) * GetBaseModelInfo()->GetBoundingSphereRadius()) )
		{
			Displayf( "%s:Physics returned an invalid bounding sphere. Centroid radius=%5.3f (model radius=%5.3f)", GetDebugName(),
				pInst->GetArchetype()->GetBound()->GetRadiusAroundCentroid(),
				GetBaseModelInfo()->GetBoundingSphereRadius());
			bInvalidResult = true;
		}
	}

	if(bInvalidResult)
	{
		Warningf("CPhysical::UpdateEntityFromPhysics - invalid result detected - will attempt to switch back to animation");
#if __ASSERT
		static bool dumpedInvalidResultManifolds = false;
		if(!dumpedInvalidResultManifolds)
		{
			dumpedInvalidResultManifolds = true;
			CPhysics::GetSimulator()->DumpManifoldsWithInstance(pInst);
		}

		if (GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(this);
			static bool bAssertedAlready = false;
			if (!bAssertedAlready)
			{
				phBoundComposite* pRagdollBound = (phBoundComposite*)pPed->GetRagdollInst()->GetArchetype()->GetBound();
				for(int iBound=0; iBound<pRagdollBound->GetNumBounds(); iBound++)
				{
					Vec3V boundPos = pRagdollBound->GetCurrentMatrix(iBound).GetCol3();
					Vector3 boundOffset = RCC_VECTOR3(boundPos) - GetObjectMtx(0).d;
					if(boundOffset.Mag2() > 20.0f*20.0f)
					{
						bAssertedAlready = true;

						Printf("\n****************** Begin debug spew for CPhysical::UpdateEntityFromPhysics() ******************\n");
						Printf("Animated Inst Matrix:");
						Printf("	%f %f %f %f", pPed->GetAnimatedInst()->GetMatrix().GetCol0().GetXf(), pPed->GetAnimatedInst()->GetMatrix().GetCol1().GetXf(), pPed->GetAnimatedInst()->GetMatrix().GetCol2().GetXf(), pPed->GetAnimatedInst()->GetMatrix().GetCol3().GetXf());
						Printf("	%f %f %f %f", pPed->GetAnimatedInst()->GetMatrix().GetCol0().GetYf(), pPed->GetAnimatedInst()->GetMatrix().GetCol1().GetYf(), pPed->GetAnimatedInst()->GetMatrix().GetCol2().GetYf(), pPed->GetAnimatedInst()->GetMatrix().GetCol3().GetYf());
						Printf("	%f %f %f %f", pPed->GetAnimatedInst()->GetMatrix().GetCol0().GetZf(), pPed->GetAnimatedInst()->GetMatrix().GetCol1().GetZf(), pPed->GetAnimatedInst()->GetMatrix().GetCol2().GetZf(), pPed->GetAnimatedInst()->GetMatrix().GetCol3().GetZf());
						Printf("Ragdoll Inst Matrix:");
						Printf("	%f %f %f %f", pPed->GetRagdollInst()->GetMatrix().GetCol0().GetXf(), pPed->GetRagdollInst()->GetMatrix().GetCol1().GetXf(), pPed->GetRagdollInst()->GetMatrix().GetCol2().GetXf(), pPed->GetRagdollInst()->GetMatrix().GetCol3().GetXf());
						Printf("	%f %f %f %f", pPed->GetRagdollInst()->GetMatrix().GetCol0().GetYf(), pPed->GetRagdollInst()->GetMatrix().GetCol1().GetYf(), pPed->GetRagdollInst()->GetMatrix().GetCol2().GetYf(), pPed->GetRagdollInst()->GetMatrix().GetCol3().GetYf());
						Printf("	%f %f %f %f", pPed->GetRagdollInst()->GetMatrix().GetCol0().GetZf(), pPed->GetRagdollInst()->GetMatrix().GetCol1().GetZf(), pPed->GetRagdollInst()->GetMatrix().GetCol2().GetZf(), pPed->GetRagdollInst()->GetMatrix().GetCol3().GetZf());
						Printf("Entity Transform:");
						Printf("	%f %f %f %f", pPed->GetTransform().GetMatrix().GetCol0().GetXf(), pPed->GetTransform().GetMatrix().GetCol1().GetXf(), pPed->GetTransform().GetMatrix().GetCol2().GetXf(), pPed->GetTransform().GetMatrix().GetCol3().GetXf());
						Printf("	%f %f %f %f", pPed->GetTransform().GetMatrix().GetCol0().GetYf(), pPed->GetTransform().GetMatrix().GetCol1().GetYf(), pPed->GetTransform().GetMatrix().GetCol2().GetYf(), pPed->GetTransform().GetMatrix().GetCol3().GetYf());
						Printf("	%f %f %f %f", pPed->GetTransform().GetMatrix().GetCol0().GetZf(), pPed->GetTransform().GetMatrix().GetCol1().GetZf(), pPed->GetTransform().GetMatrix().GetCol2().GetZf(), pPed->GetTransform().GetMatrix().GetCol3().GetZf());
						for (int ibone = 0; ibone < GetSkeleton()->GetBoneCount(); ibone++)
						{
							Vector3 vObjectOffset = pPed->GetObjectMtx(ibone).d - pPed->GetObjectMtx(0).d;
							Vector3 vLocalOffset = pPed->GetLocalMtx(ibone).d;
							Printf("Offset between root bone position and %s bone = %f,%f,%f (locals %f,%f,%f)\n", GetSkeleton()->GetSkeletonData().GetBoneData(ibone)->GetName(), vObjectOffset.x, vObjectOffset.y, vObjectOffset.z, vLocalOffset.x, vLocalOffset.y, vLocalOffset.z);
						}	
						Printf("\n****************** SpewRagdollTaskInfo ******************\n");
						pPed->SpewRagdollTaskInfo();
						Assertf(false, "%s ragdoll component %d offset too big (%f,%f,%f)", GetModelName(), iBound, pRagdollBound->GetCurrentMatrix(iBound).GetCol3().GetXf(), pRagdollBound->GetCurrentMatrix(iBound).GetCol3().GetYf(), pRagdollBound->GetCurrentMatrix(iBound).GetCol3().GetZf());
						Printf("\n****************** End debug spew for CPed::UpdateRagdollBoundsFromAnimatedSkel() ******************\n");
						break;
					}
				}
			}
		}
#endif

		// get rid of orientation, just keep position if possible
		Matrix34 lastEntityMatrix(Matrix34::IdentityType);
		lastEntityMatrix.d = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
		if (lastEntityMatrix.d.x != lastEntityMatrix.d.x || lastEntityMatrix.d.x*0.0f != 0.0f ||
			lastEntityMatrix.d.y != lastEntityMatrix.d.y || lastEntityMatrix.d.y*0.0f != 0.0f ||
			lastEntityMatrix.d.z != lastEntityMatrix.d.z || lastEntityMatrix.d.z*0.0f != 0.0f)
		{
			Warningf("CPhysical::UpdateEntityFromPhysics() - lastEntityMatrix.d isn't finite - setting to 0,0,100");
			lastEntityMatrix.d = Vector3(0.0f,0.0f,100.0f);
		}

		phCollider* pCollider = CPhysics::GetSimulator()->GetCollider(pInst);
		if(pCollider)
		{
			// Set the inst matrix and collider matrix from the last entity matrix
			pInst->SetMatrix(RCC_MAT34V(lastEntityMatrix));
			pCollider->SetColliderMatrixFromInstanceRigid();

			Warningf("CPhysical::UpdateEntityFromPhysics - Collider being reset");
			pCollider->Reset();
			
			fragInst* pFragInst = pInst->GetClassType() >= PH_INST_FRAG_GTA ? static_cast<fragInst*>(pInst) : NULL;
			phBoundComposite* pCompositeBound = pInst->GetArchetype()->GetBound()->GetType() == phBound::COMPOSITE ? static_cast<phBoundComposite*>(pInst->GetArchetype()->GetBound()) : NULL;
			if(pFragInst && pCompositeBound)
			{
				if (GetIsTypePed())
				{
					Warningf("CPhysical::UpdateEntityFromPhysics() - Will try to give the bounds and skeleton reasonable values");
					pFragInst->PoseBoundsFromArticulatedBody();
					pFragInst->PoseSkeletonFromArticulatedBody();
				}

				float oldRadius = pCompositeBound->GetRadiusAroundCentroid();

				pCompositeBound->CalculateCompositeExtents();
				if(pCompositeBound->HasBVHStructure())
				{
					CPhysics::GetLevel()->RebuildCompositeBvh(pInst->GetLevelIndex());
				}

				if(pFragInst->GetClassType()==PH_INST_FRAG_PED || !pFragInst->GetManualSkeletonUpdate())
					pFragInst->ReportMovedBySim();
				else
					pFragInst->SyncSkeletonToArticulatedBody();

				if (oldRadius != pCompositeBound->GetRadiusAroundCentroid())
					CPhysics::GetLevel()->UpdateObjectLocationAndRadius(pInst->GetLevelIndex(),(Mat34V_Ptr)(NULL));
			}
		}

		if(GetIsTypePed())
		{
			static_cast<CPed*>(this)->DeactivatePhysicsAndDealWithRagdoll();
		}
		else
			DeActivatePhysics();

		// force matrix back to old matrix that was stored in the entity before physics update
		SetMatrix(lastEntityMatrix, true, true, true);
	}
	else
	{
		// only update previous position on the first physics update loop
		if(CPhysics::GetIsFirstTimeSlice(nLoop))
			SetPreviousPosition(VEC3V_TO_VECTOR3(GetTransform().GetPosition()));

		Assertf(pInst->GetMatrix().IsOrthonormal3x3(ScalarVFromF32(REJUVENATE_ERROR_NEW_VEC)), "%s:Physics returned a non ortho matrix for object", GetDebugName());
		// the player's allowed to drive outside the world now, so only check things going too high now
#if __ASSERT
		{
			static int s_NumOutOfWorldDumps = 0;
			if(s_NumOutOfWorldDumps++ < 10)
			{
				const Vec3V currentPos = CEntity::GetTransform().GetPosition();
				if(pInst->GetMatrix().GetM23f() >= WORLDLIMITS_ZMAX)
				{
					Displayf("===== object (%s) outside world limits (ZMAZ: z=%f) =====", GetDebugName(), pInst->GetMatrix().GetM23f());
					DumpOutOfWorldInfo();
					Assertf(false, "%s:Physics returned a position outside top of world (z=%f)."
						" Entity is currently at (%5.3f, %5.3f, %5.3f). See log for more info.",
						GetDebugName(), pInst->GetMatrix().GetM23f(), currentPos.GetXf(), currentPos.GetYf(), currentPos.GetZf());
				}

				if(pInst->GetMatrix().GetM23f() <= WORLDLIMITS_ZMIN)
				{
					Displayf("===== object (%s) outside world limits (ZMIN: z=%f) =====", GetDebugName(), pInst->GetMatrix().GetM23f());
					DumpOutOfWorldInfo();
					Assertf(false,
						"%s:Physics returned a position below bottom of world (z=%f)."
						" Entity is currently at (%5.3f, %5.3f, %5.3f). See log for more info.",
						GetDebugName(), pInst->GetMatrix().GetM23f(), currentPos.GetXf(), currentPos.GetYf(), currentPos.GetZf());
				}
			}
		}
#endif // __ASSERT

		CDynamicEntity::SetMatrix(RCC_MATRIX34(pInst->GetMatrix()));

		if( CPhysics::GetIsLastTimeSlice(nLoop) )
		{
			// Update the entity's bounds in the path server system.
			// Only necessary in the final timeslice.
			if(IsInPathServer())
			{
				CPathServerGta::UpdateDynamicObject(this);
			}
			// Attempt to add the object, if we have failed to before
			else if(WasUnableToAddToPathServer())
			{
				if(CPathServerGta::MaybeAddDynamicObject(this))
				{
					CAssistedMovementRouteStore::MaybeAddRouteForDoor(this);
				}
			}
		}
	}
#if __ASSERT
	if( !m_nPhysicalFlags.bModifiedBounds )
	{
		// bound radius is already checked above, as 4*std, so test for 4 and a bit* here
		Vec3V instBBmax = pInst->GetArchetype()->GetBound()->GetBoundingBoxMax();
		Vec3V instBBmin = pInst->GetArchetype()->GetBound()->GetBoundingBoxMin();
		Vec3V modelBBmax = VECTOR3_TO_VEC3V( GetBaseModelInfo()->GetBoundingBoxMax() );
		Vec3V modelBBmin = VECTOR3_TO_VEC3V( GetBaseModelInfo()->GetBoundingBoxMin() );

		bool boxOk = VEC3V_TO_VECTOR3(instBBmax - instBBmin).Mag2() < 17.0f * VEC3V_TO_VECTOR3( modelBBmax - modelBBmin).Mag2() ? true: false;
		if( !boxOk )
		{
			Displayf( "\n instBBmax %f %f %f \n instBBmin %f %f %f ", VEC3V_ARGS(instBBmax), VEC3V_ARGS(instBBmin) );
			Displayf( "\n modelBBmax %f %f %f \n modelBBmin %f %f %f ", VEC3V_ARGS(modelBBmax), VEC3V_ARGS(modelBBmin) );
			Assertf( !boxOk, "%s:Physics returned an bounding box bigger than expected", GetDebugName() );
		}
	}
#endif
}

//
// This function ignores any X or Y rotation which was previously
//	applied to the Placeable
void CPhysical::SetHeading(float new_heading)
{
	CDynamicEntity::SetHeading(new_heading);
	Assert(GetTransform().IsOrthonormal());

	// update the representations in the physics world
	UpdatePhysicsFromEntity();
}

void CPhysical::SetPosition(const Vector3& vec, bool bUpdateGameWorld, bool bUpdatePhysics, bool bWarp)
{

#if __ASSERT
	const char*	networkName = GetNetworkObject() ? GetNetworkObject()->GetLogName() : "none";
	Assertf( g_WorldLimits_AABB.ContainsPoint(RCC_VEC3V(vec)),
		"Setting position of entity '%s' (debug name '%s' network name '%s') outside world limits (%5.2f, %5.2f, %5.2f). Old position (%5.2f, %5.2f, %5.2f)", 
		GetModelName(), GetDebugName(), networkName, VEC3V_ARGS(RCC_VEC3V(vec)), VEC3V_ARGS(GetTransform().GetPosition()));
#endif
		
	CDynamicEntity::SetPosition(vec, bUpdateGameWorld, bUpdatePhysics, bWarp);
	
	ResetWaterStatus();

	if(bUpdatePhysics) {
		UpdatePhysicsFromEntity(bWarp);
	}
}

void CPhysical::SetMatrix(const Matrix34& mat, bool bUpdateGameWorld, bool bUpdatePhysics, bool bWarp)
{
#if __ASSERT
	bool bOrthonormal = mat.IsOrthonormal();
	if( !bOrthonormal )
	{
		Matrix34 matTrans(mat);
				 matTrans.Transpose3x4();
		Matrix34 I;
				 I.Dot(matTrans, mat);

		Assertf(bOrthonormal, "Matrix wasn't orthonormal: \n a (%.4f, %.4f, %.4f) \n b (%.4f, %.4f, %.4f) \n c (%.4f, %.4f, %.4f) \n d (%.4f, %.4f, %.4f)\n"
							  "Matrix times matrix-transpose - I: \n a (%.5f, %.5f, %.5f) \n b (%.5f, %.5f, %.5f) \n c (%.5f, %.5f, %.5f)",
								mat.a.x, mat.a.y, mat.a.z, mat.b.x, mat.b.y, mat.b.z, mat.c.x, mat.c.y, mat.c.z, mat.d.x, mat.d.y, mat.d.z,
								I.a.x - 1.0f, I.a.y, I.a.z, I.b.x, I.b.y - 1.0f, I.b.z, I.c.x, I.c.y, I.c.z - 1.0f);
	}
#endif

#if !__NO_OUTPUT

	if(m_LogSetMatrixCalls)
	{
		EntityDebugfWithCallStack(this, "SetMatrix"); 
	}
#endif

	CDynamicEntity::SetMatrix(mat, false, bUpdatePhysics, bWarp);
	ResetWaterStatus();

	if(bUpdatePhysics){
		UpdatePhysicsFromEntity(bWarp);
	}
	
	if (bUpdateGameWorld){
		UpdateWorldFromEntity();
	}
}

void CPhysical::ProcessCollision(phInst const * myInst, CEntity* pHitEnt, phInst const * hitInst, const Vector3& vMyHitPos,
								 const Vector3& vOtherHitPos, float fImpulseMag, const Vector3& vMyNormal, int iMyComponent,
								 int iOtherComponent, phMaterialMgr::Id iOtherMaterial, bool bIsPositiveDepth, bool bIsNewContact)
{
	if( pHitEnt )
	{
		if( pHitEnt == m_cantBeDamagedByEntity )
		{
			if( pHitEnt->GetIsPhysical() &&
				pHitEnt->GetIsDynamic() )
			{
				Displayf("[CPhysical::ProcessCollision] [Physical: %s %s] is ignoring collision with [Physical: %s %s]", GetModelName(), GetNetworkObject() ? GetNetworkObject()->GetLogName() : "none",
																													static_cast<CPhysical*>(pHitEnt)->GetModelName(), static_cast<CPhysical*>(pHitEnt)->GetNetworkObject() ? static_cast<CPhysical*>(pHitEnt)->GetNetworkObject()->GetLogName() : "none" );
			}
			return;
		}

		// If we just broke a piece of breakable glass, it is possible that the hit inst has been swapped out and so may not
		// still be in the level. If that's the case, there is no point continuing; we will likely get a collision with the
		// new inst.
		if(pHitEnt->GetIsDynamic())
		{
			if(static_cast<CDynamicEntity*>(pHitEnt)->m_nDEflags.bIsBreakableGlass && !hitInst->IsInLevel())
				return;

			if( pHitEnt->GetIsPhysical() &&
				this == static_cast<CPhysical*>(pHitEnt)->GetCantBeDamagedByCollisionEntity() )
			{
				Displayf("[CPhysical::ProcessCollision] [Physical: %s %s] is ignoring collision with [Physical: %s %s]", static_cast<CPhysical*>(pHitEnt)->GetModelName(), static_cast<CPhysical*>(pHitEnt)->GetNetworkObject() ? static_cast<CPhysical*>(pHitEnt)->GetNetworkObject()->GetLogName() : "none", 
																														GetModelName(), GetNetworkObject() ? GetNetworkObject()->GetLogName() : "none" );
				return;
			}

			if (NetworkInterface::IsGameInProgress())
			{
				//Update the last damage entity...
				// NOTE: I'm changing the previous behavior due to Gas tanks where the last damage would screw 
				//         the last inflictor causing people to come with statistics saying that they committed suicide and other ode behavior.

				if(GetIsTypeObject() && GetFragInst() && pHitEnt->GetIsTypeVehicle())
				{
					CEntity* lastInflictor = GetWeaponDamageEntity();
					const u32 timeSinceLastDamage = fwTimer::GetTimeInMilliseconds() - GetWeaponDamagedTime();
					const u32 TIME_INTERVAL = 10000;

					if (!lastInflictor || timeSinceLastDamage > TIME_INTERVAL)
					{
						SetWeaponDamageInfo(pHitEnt, 0, fwTimer::GetTimeInMilliseconds());
					}
				}
			}
			else
			{
				//Update the last damage entity...
				// NOTE: I'm not sure GetFragInst is necessary but I didn't want to change the previous behavior
				if(GetIsTypeObject() && GetFragInst() && pHitEnt->GetIsTypeVehicle())
				{
					SetWeaponDamageInfo(pHitEnt, 0, fwTimer::GetTimeInMilliseconds());
				}
			}
		}
	}

	// Looking up if a group is damageable is expensive so don't do it for things we know can't be damaged (peds, vehicles)
	if(GetIsTypeObject() && !static_cast<CObject*>(this)->m_nObjectFlags.bVehiclePart)
	{
		if( m_canOnlyBeDamagedByEntity == NULL ||
			pHitEnt == m_canOnlyBeDamagedByEntity )
		{
			if (fragInst* pFragInst = GetFragInst())
			{
				// Early out here in the common case where the object isn't damageable
				if(pFragInst->GetTypePhysics()->IsChildsGroupDamageable(iMyComponent))
				{
					// ModifyImpulseMag will sometimes want to distribute the force between contacts regardless of the incoming impulse. During rage breaking
					//   it is important that ModifyImpulseMag knows how many impulses there are between the two components because we're accumulating impulse
					//   per-component and we want the exact impulse sum. 
					// Here though, we aren't accumulating impulses between contacts. Instead we're calling NotifyImpulse per-contact. Distributing the force here
					//   would dilute the impulse based on the number of contacts. We might want to consider accumulating impulses here, but for now I'm keeping
					//   the current behavior. 
					int numComponentImpulses = 1;

					ScalarV vImpulseMag = pFragInst->ModifyImpulseMag(iMyComponent, iOtherComponent, numComponentImpulses, ScalarVFromF32(fImpulseMag*fImpulseMag), pHitEnt ? pHitEnt->GetCurrentPhysicsInst() : NULL);
					Vector3 vImpulse;
					if(IsTrue(vImpulseMag > ScalarV(V_NEGONE)))
					{
						vImpulse = VEC3V_TO_VECTOR3(vImpulseMag * Vec3V(V_X_AXIS_WZERO));
					}
					else
					{
						// If instance returns -1.0 then just use contact impulse
						vImpulse = fImpulseMag*vMyNormal;
					}

					pFragInst->NotifyImpulse(vImpulse, vMyNormal, iMyComponent, 0);
				}
			}
		}
	}
#if __ASSERT
	else if(fragInst* pFragInst = GetFragInst())
	{
		if(iMyComponent < pFragInst->GetTypePhysics()->GetNumChildren())
		{
			Assert(pFragInst->GetTypePhysics() && pFragInst->GetTypePhysics()->IsChildsGroupDamageable(iMyComponent) == false);
		}
	}
#endif

	if(pHitEnt)
	{
		if(IsRecordingCollisionHistory())
		{
			GetFrameCollisionHistory()->AddCollisionRecord(myInst, pHitEnt, vMyNormal*fImpulseMag, vMyNormal, vMyHitPos, vOtherHitPos, iMyComponent,
			iOtherComponent, iOtherMaterial, bIsPositiveDepth, bIsNewContact);
		}

        NetworkInterface::RegisterCollision(*this, pHitEnt, fImpulseMag);
	}
}

void CPhysical::AddCollisionRecordBeforeDisablingContact(phInst* pThisInst, CEntity* pOtherEntity, phContactIterator impacts)
{
	if(pOtherEntity && IsRecordingCollisionHistory())
	{
		// Extract info from the impact and add info about this event to the collision recording system because
		// it won't be added in IterateOverManifolds() once we disable the impact.
		Vector3 vMyImpulse, vMyNormal;
		impacts.GetMyImpulse(vMyImpulse);
		impacts.GetMyNormal(vMyNormal);
		GetFrameCollisionHistory()->AddCollisionRecord(pThisInst, pOtherEntity, vMyImpulse, vMyNormal,
			VEC3V_TO_VECTOR3(impacts.GetMyPosition()), VEC3V_TO_VECTOR3(impacts.GetOtherPosition()), impacts.GetMyComponent(),
			impacts.GetOtherComponent(), impacts.GetOtherMaterialId(), impacts.GetContact().IsPositiveDepth(), impacts.GetContact().GetLifetime() == 1);
	}
}

void CPhysical::AddCollisionRecordBeforeDisablingContact(phInst* pThisInst, CEntity* pOtherEntity, phCachedContactIteratorElement* pIterator)
{
	if(pOtherEntity && IsRecordingCollisionHistory())
	{
		// Extract info from the impact and add info about this event to the collision recording system because
		// it won't be added in IterateOverManifolds() once we disable the impact.
		Vector3 vMyImpulse = VEC3V_TO_VECTOR3(pIterator->m_Contact->GetAccumImpulse()*pIterator->m_MyNormal);
		GetFrameCollisionHistory()->AddCollisionRecord(pThisInst, pOtherEntity, vMyImpulse, VEC3V_TO_VECTOR3(pIterator->m_MyNormal),
			VEC3V_TO_VECTOR3(pIterator->m_MyPosition), VEC3V_TO_VECTOR3(pIterator->m_OtherPosition), pIterator->m_MyComponent,
			pIterator->m_OtherComponent, pIterator->m_Contact->GetMaterialIdB(), pIterator->m_Contact->IsPositiveDepth(), pIterator->m_Contact->GetLifetime() == 1);
	}
}

#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE	// Don't need to do any of this if we don't have second surface.
void CPhysical::ProcessPreComputeImpacts(phContactIterator impacts) 
{
	ProcessSecondSurfaceImpacts(impacts);
}

void CPhysical::ProcessSecondSurfaceImpacts(phContactIterator impacts, const CSecondSurfaceConfig& secondSurfaceConfig ) 
{

	//Simulate the effect of the soft material between the top surface 
	//and the second surface of a bvh.  We want the dynamic object hitting the bvh to slow down.
	
	// For objects hitting a second surface BVH, impacts will be generated for both the top surface and the interpolated surface
	// These top surface impacts can be identified because they have a dist to top surface of -1
	// Dist to top surface:
	// -1.0f : The top surface of a BVH poly intersection that has a 2nd surface
	//  0.0f : This contact has no second surface
	//	> 0.0f : A contact with the interpolated surface


	int iImpactCount=0;
	bool bSecondSurfaceHit = false;
	impacts.Reset();
	while(!impacts.AtEnd())
	{
		//Test that the physical is active in the simulator
		if(impacts.GetMyInstance() && PHLEVEL->IsActive(impacts.GetMyInstance()->GetLevelIndex()))
		{
			Vector3 vOffset = impacts.GetMyPosition() - VEC3V_TO_VECTOR3(GetTransform().GetPosition());

			//Test if we've got a contact with a bvh that has a second surface.
#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
			float fDistToTopSurface=impacts.GetDistToTopSurface();
#else
			float fDistToTopSurface=impacts.GetUserData();
#endif
			if(fDistToTopSurface < 0.0f)
			{
				// A 'fake' top surface contact
				// Disable it but use its depth as the top surface


				// We don't disable the contact in the neo second surface implementation, because we just store the depth in the regular contact instead of making a new one
#if !HACK_GTA4_BOUND_GEOM_SECOND_SURFACE_DATA || HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
				fDistToTopSurface = impacts.GetDepth();		
				impacts.DisableImpact();
#endif

				// Support force varies with tang speed and depth
				// There is also a constant support force
				float fSpringForce = secondSurfaceConfig.m_fSinkLiftFactor * fDistToTopSurface;	// Can never do more than support physical's weight

				Vector3 vNormal = ZAXIS;
				//impacts.GetMyNormal(vNormal);
				Vector3 vVel = GetVelocity();
				float fSpeedIntoSecondSurface = vVel.Dot(vNormal);
				
				// Tang velocity gives lift force
				// Make a squared dependence to avoid sqrt

				vVel -= fSpeedIntoSecondSurface * vNormal;

				fSpringForce += rage::Min(vVel.Mag2() * secondSurfaceConfig.m_fTangV2LiftFactor, secondSurfaceConfig.m_fMaxTangV2LiftFactor);

				Vector3 vForce = ZAXIS * fSpringForce * GetMass();
				impacts.GetMyInstance()->ApplyForce(vForce,vOffset,impacts.GetMyComponent(),0,0.0f);
			}

			// Note this code is hit if the code above has been hit too
			if(fDistToTopSurface>0.0f)
			{
				bSecondSurfaceHit = true;

				/// Apply forces at contact:
				// Apply drag at contact point proportional to dist to top surface and vel

				Vector3 vDragForce = impacts.GetRelVelocity();
				vDragForce.Scale(GetMass() * -secondSurfaceConfig.m_fDragFactor * fDistToTopSurface);

				impacts.GetMyInstance()->ApplyForce(vDragForce,vOffset,impacts.GetMyComponent(),0,0.0f);
			}
			
			
		}
		iImpactCount++;
		impacts++;
	}

	m_nPhysicalFlags.bSecondSurfaceImpact=false;

	if(bSecondSurfaceHit)
	{
		m_nPhysicalFlags.bSecondSurfaceImpact = true;
	}
}
#endif	// HACK_GTA4_BOUND_GEOM_SECOND_SURFACE

void CPhysical::DoCollisionEffects()
{
	// check if we've hit anything this frame
	const CCollisionHistory* pCollisionHistory = GetFrameCollisionHistory();
	const CCollisionRecord* pColRecord = pCollisionHistory->GetMostSignificantCollisionRecord();

	if (pColRecord != NULL && pColRecord->m_pRegdCollisionEntity.Get() != NULL)
	{
		// check that this material can smash glass
		bool shouldProcess = false;
		phInst* pPhysInst = GetCurrentPhysicsInst();
		if (pPhysInst)
		{
			phArchetype* pArchetype = pPhysInst->GetArchetype();
			if (pArchetype)
			{
				phBound* pBound = pArchetype->GetBound();
				if (pBound)
				{
					phMaterialMgr::Id mtlId = phMaterialMgr::DEFAULT_MATERIAL_ID;
					
					if (pBound->GetType() == phBound::COMPOSITE)
					{
						phBoundComposite* pCompositeBound = static_cast<phBoundComposite*>(pBound);
						for (int partIndex = 0; partIndex < pCompositeBound->GetNumBounds(); ++partIndex)
						{
							if (phBound* pPartBound = pCompositeBound->GetBound(partIndex))
							{
								Assertf(pPartBound->GetType()!=phBound::COMPOSITE, "%s is a composite bound with an child bound that is also composite", GetDebugName());
								mtlId = PGTAMATERIALMGR->UnpackMtlId(pPartBound->GetMaterialId(0));
								break;
							}
						}
					}
					else
					{
						mtlId = PGTAMATERIALMGR->UnpackMtlId(pBound->GetMaterialId(0));
					}

					if (PGTAMATERIALMGR->GetMtlVfxGroup(mtlId)!=VFXGROUP_PAPER && PGTAMATERIALMGR->GetMtlVfxGroup(mtlId)!=VFXGROUP_CARDBOARD)
					{
						shouldProcess = true;
					}
				}
			}
		}

		if (!shouldProcess)
		{
			return;
		}

//		float impulseMag = GetLatestCollisionImpulseMag();
//		Printf("ColnFx: ImpulseMag %.2f\n", impulseMag);
//		float speedMag = GetLatestCollisionImpulseMag()/GetMass();
//		Printf("ColnFx: SpeedMag %.2f\n", speedMag);
		if (
			pCollisionHistory->GetMaxCollisionImpulseMagLastFrame()>VEHICLEGLASSTHRESH_PHYSICAL_COLN_MIN_IMPULSE
			|| pCollisionHistory->GetMaxCollisionImpulseMagLastFrame()/GetMass()>5.0f)
		{
			// check if the collision material was smashable
			if (/*PGTAMATERIALMGR->GetIsSmashableGlass(GetOtherCollisionMaterialId()) && */IsEntitySmashable(pColRecord->m_pRegdCollisionEntity.Get()))
			{
				// set up the collision info structure
				VfxCollInfo_s vfxCollInfo;
				vfxCollInfo.regdEnt = pColRecord->m_pRegdCollisionEntity.Get();
				vfxCollInfo.vPositionWld = RCC_VEC3V(pColRecord->m_MyCollisionPos);						
				vfxCollInfo.vNormalWld = -RCC_VEC3V(pColRecord->m_MyCollisionNormal);
				vfxCollInfo.vDirectionWld = -vfxCollInfo.vNormalWld;
				vfxCollInfo.materialId = pColRecord->m_OtherCollisionMaterialId;
				vfxCollInfo.roomId = 0;
				vfxCollInfo.componentId = pColRecord->m_OtherCollisionComponent;

				Assert(vfxCollInfo.vNormalWld.GetXf()!=0.0f || vfxCollInfo.vNormalWld.GetYf()!=0.0f || vfxCollInfo.vNormalWld.GetZf()!=0.0f);
																		
				vfxCollInfo.weaponGroup = WEAPON_EFFECT_GROUP_PUNCH_KICK;		// TODO: pass in correct weapon group?
				vfxCollInfo.force = (pCollisionHistory->GetMaxCollisionImpulseMagLastFrame()-VEHICLEGLASSTHRESH_PHYSICAL_COLN_MIN_IMPULSE)/(VEHICLEGLASSTHRESH_PHYSICAL_COLN_MAX_IMPULSE-VEHICLEGLASSTHRESH_PHYSICAL_COLN_MIN_IMPULSE);
				vfxCollInfo.force = MAX(vfxCollInfo.force, 0.1f);
				vfxCollInfo.force = MIN(vfxCollInfo.force, 1.0f);
				vfxCollInfo.isBloody = false;
				vfxCollInfo.isWater = false;
				vfxCollInfo.isExitFx = false;
				vfxCollInfo.noPtFx = false;
				vfxCollInfo.noPedDamage = false;
				vfxCollInfo.noDecal = false;
				vfxCollInfo.isSnowball = false;
				vfxCollInfo.isFMJAmmo = false;

				// try to smash the object
				g_vehicleGlassMan.StoreCollision(vfxCollInfo);
			}
		}
	}
}


/*
void CPhysical::ApplyMoveForce(Vector3 vecForce)
{
	if(m_nPhysicalFlags.bDoorPhysics || m_nPhysicalFlags.bHangingPhysics)
		return;
	else if(m_nPhysicalFlags.bPoolBallPhysics)
		vecForce.z = 0.0f;
	
	vecForce /= m_fMass;
	GetVelocity() += vecForce;

	Assert( rage::Abs(GetVelocity().x) < 10.0f );
	Assert( rage::Abs(GetVelocity().y) < 10.0f );
	Assert( rage::Abs(GetVelocity().z) < 10.0f );
}

void CPhysical::ApplyTurnForce(Vector3 vecForce, Vector3 vecOffset)
{	// might want to change this so only need do COM stuff if set non-zero
	if(m_nPhysicalFlags.bPedPhysics)
		return;

	Vector3 vecCOM(0.0f,0.0f,0.0f);
	float fTurnMass = m_fTurnMass;
	// parallel axis theorm since we're not rotating about the Centre of Mass
	if(m_nPhysicalFlags.bHangingPhysics)
		fTurnMass += 0.5f*m_fMass*m_vecCOM.z*m_vecCOM.z;
	else
		GetMatrix().Transform3x3(m_vecCOM, vecCOM);
	
	if(m_nPhysicalFlags.bDoorPhysics)
	{
		vecForce.z = 0.0f;
		vecOffset.z = 0.0f;
	}
	
	vecForce = CrossProduct(vecOffset - vecCOM, vecForce) / m_fTurnMass;
	AddToTurnSpeed(vecForce);

	Assert( rage::Abs(GetAngVelocity().x) < 10.0f );
	Assert( rage::Abs(GetAngVelocity().y) < 10.0f );
	Assert( rage::Abs(GetAngVelocity().z) < 10.0f );
}
*/

#if __ASSERT
float CPhysical::GetRadiusAroundLocalOrigin()
{
	if(GetCurrentPhysicsInst() && GetCurrentPhysicsInst()->GetArchetype())
		return GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetRadiusAroundCentroid() + Mag(GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetCentroidOffset()).Getf();
	else
		return GetBoundRadius();
}

bool CPhysical::CheckForceInRange(const Vector3 &vecForce, const float maxAccel) const
{
	return vecForce.Mag2() <= square(maxAccel * rage::Max(1.0f, GetMass()));
}
#endif // __ASSERT

void CPhysical::NotifyWaterImpact(const Vector3& UNUSED_PARAM(vecForce), const Vector3& UNUSED_PARAM(vecTorque), int UNUSED_PARAM(nComponent), float UNUSED_PARAM(fTimeStep))
{
}

// apply both move and turn forces at same time so we can use different physics models
void CPhysical::ApplyForce(const Vector3& vecForce, const Vector3& vecOffset, int nComponent, bool ASSERT_ONLY(bCheckForces), float timeStepOverride)
{
	phInst *curPhysInst = GetCurrentPhysicsInst();

	Assert(curPhysInst);
	if(curPhysInst==NULL)
		return;

	ScalarV vTimeStep = ScalarV(Selectf(timeStepOverride, timeStepOverride, fwTimer::GetTimeStep() / CPhysics::GetNumTimeSlices()));

	if(Verifyf(curPhysInst->IsInLevel(),"Should never apply force when phInst is not in level"))
	{
		// Ignore forces being applied to components that do not exist exist on the object
		// LOD changes can reduce the number of bounds while the user is holding onto the component
		const phBound *curPhysBound = curPhysInst->GetArchetype()->GetBound();
		if(curPhysBound->GetType() == phBound::COMPOSITE)
		{
			const phBoundComposite* compBound = static_cast<const phBoundComposite*>(curPhysBound);
			if(nComponent >= compBound->GetNumBounds() || compBound->GetBound(nComponent) == NULL)
			{
				return;
			}
		}

#if __ASSERT
		if (bCheckForces)
		{
			FORCE_CHECK("[CPhysical::ApplyForce()]", vecForce, DEFAULT_ACCEL_LIMIT);
			OFFSET_FROM_CENTROID_CHECK("[CPhysical::ApplyForce()]", vecOffset, 1.2f);
		}
#endif
	#if __DEV
		if(fragInstNMGta::ms_bLogRagdollForces && GetCurrentPhysicsInst()->GetClassType()==PH_INST_FRAG_PED)
		{
			Displayf("Ragdoll Force %g Offset %g  (Inst %p Time %d)\n", vecForce.Mag(), vecOffset.Mag(), GetCurrentPhysicsInst(), fwTimer::GetTimeInMilliseconds());
			Vector3 vecDirn(vecForce);
			vecDirn.NormalizeFast();
			const Vector3 vStartOfLine = vecOffset + VEC3V_TO_VECTOR3(GetTransform().GetPosition());
			grcDebugDraw::Line(vStartOfLine, vStartOfLine - vecDirn, Color_white);
		}
	#endif

		// wake up if it's not active
		if(CPhysics::GetLevel()->IsInactive(curPhysInst->GetLevelIndex()))
			ActivatePhysics();

		phCollider *pCollider = GetCollider();
		if(pCollider)
		{
			pCollider->ApplyForce(vecForce, vecOffset + VEC3V_TO_VECTOR3(GetTransform().GetPosition()), vTimeStep.GetIntrin128(), nComponent);
		}
	}
}
void CPhysical::ApplyForceCg(const Vector3& vecForce, float timeStepOverride)
{
	Assert(GetCurrentPhysicsInst());
	if(GetCurrentPhysicsInst()==NULL)
		return;

	ScalarV vTimeStep = ScalarV(Selectf(timeStepOverride, timeStepOverride, fwTimer::GetTimeStep() / CPhysics::GetNumTimeSlices()));

	if(Verifyf(GetCurrentPhysicsInst()->IsInLevel(),"Should never apply force when phInst is not in level"))
	{
		FORCE_CHECK("[CPhysical::ApplyForceCg()]", vecForce, DEFAULT_ACCEL_LIMIT);
		Assert(vecForce.x==vecForce.x && vecForce.y==vecForce.y && vecForce.z==vecForce.z);
		//Assert(GetCurrentPhysicsInst()->GetClassType()!=PH_INST_FRAG_PED);

		// wake up if it's not active
		if(CPhysics::GetLevel()->IsInactive(GetCurrentPhysicsInst()->GetLevelIndex()))
			ActivatePhysics();

		phCollider * pCollider = GetCollider();
		if(pCollider)
		{
			pCollider->ApplyForceCenterOfMass(vecForce, vTimeStep.GetIntrin128());

#if DR_ENABLED && __ASSERT
			static bool sbOnceOnly;
			if (!sbOnceOnly)
			{
				if (!CheckForceInRange(vecForce, DEFAULT_ACCEL_LIMIT))
				{
					debugPlayback::RecordTaggedVectorValue(*GetCurrentPhysicsInst(), RCC_VEC3V(vecForce), "FlaggedForce", debugPlayback::eVectorTypeVelocity);
					debugPlayback::SaveRecordingForInst(GetCurrentPhysicsInst());
					sbOnceOnly = true;
				}
			}
#endif
		}
	}
}

void CPhysical::ApplyTorque(const Vector3& vecForce, const Vector3& vecOffset, float timeStepOverride)
{
	Assert(GetCurrentPhysicsInst());
	if(GetCurrentPhysicsInst()==NULL)
		return;

	ScalarV vTimeStep = ScalarV(Selectf(timeStepOverride, timeStepOverride, fwTimer::GetTimeStep() / CPhysics::GetNumTimeSlices()));

	if(Verifyf(GetCurrentPhysicsInst()->IsInLevel(),"Should never apply torque when phInst is not in level"))
	{
		FORCE_CHECK("[CPhysical::ApplyTorque()]", vecForce, DEFAULT_ACCEL_LIMIT);
		OFFSET_FROM_CENTROID_CHECK("[CPhysical::ApplyTorque()]", vecOffset, 1.2f);
		Assert(vecForce.x==vecForce.x && vecForce.y==vecForce.y && vecForce.z==vecForce.z);
		Assert(vecOffset.x==vecOffset.x && vecOffset.y==vecOffset.y && vecOffset.z==vecOffset.z);
		//Assert(GetCurrentPhysicsInst()->GetClassType()!=PH_INST_FRAG_PED);

		// wake up if it's not active
		if(CPhysics::GetLevel()->IsInactive(GetCurrentPhysicsInst()->GetLevelIndex()))
			ActivatePhysics();

		if(GetCollider())
		{
			Vector3 vecTorque(vecOffset);
			vecTorque.Cross(vecForce);
			GetCollider()->ApplyTorque(vecTorque, vTimeStep.GetIntrin128());
		}
	}
}

void CPhysical::ApplyTorque(const Vector3& vecTorque, float timeStepOverride)
{
	Assert(GetCurrentPhysicsInst());
	if(GetCurrentPhysicsInst()==NULL)
		return;

	ScalarV vTimeStep = ScalarV(Selectf(timeStepOverride, timeStepOverride, fwTimer::GetTimeStep() / CPhysics::GetNumTimeSlices()));

	Assert(GetCurrentPhysicsInst()->IsInLevel());
	// JP: Removed this because we use this command to apply angular impulses too, which can be v large
// #if __ASSERT
// 	// Check torque isn't going to cause a massive change in ang velocity
// 	Vector3 vecAngAccel;
// 	GetMatrix().UnTransform3x3(vecTorque,vecAngAccel);
// 	const float fAngAccel = vecAngAccel.Dot(GetCurrentPhysicsInst()->GetArchetype()->GetInvAngInertia());
// 	Assertf(Abs(fAngAccel) < 15.0f,"fAngAccel: %f",fAngAccel);
// #endif
	Assert(vecTorque.x==vecTorque.x && vecTorque.y==vecTorque.y && vecTorque.z==vecTorque.z);
	//Assert(GetCurrentPhysicsInst()->GetClassType()!=PH_INST_FRAG_PED);

	// wake up if it's not active
	if(CPhysics::GetLevel()->IsInactive(GetCurrentPhysicsInst()->GetLevelIndex()))
		ActivatePhysics();

	if(GetCollider())
		GetCollider()->ApplyTorque(vecTorque, vTimeStep.GetIntrin128());
}

void CPhysical::ApplyTorqueAndForce(const Vector3& vecTorque, const Vector3& vecWorldPosition, float timeStepOverride)
{
	Assert(GetCurrentPhysicsInst());
	if(GetCurrentPhysicsInst()==NULL)
		return;

	ScalarV vTimeStep = ScalarV(Selectf(timeStepOverride, timeStepOverride, fwTimer::GetTimeStep() / CPhysics::GetNumTimeSlices()));

	Assert(GetCurrentPhysicsInst()->IsInLevel());
#if __ASSERT
	// Check torque isn't going to cause a massive change in ang velocity
	Vector3 vecAngAccel = VEC3V_TO_VECTOR3(GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vecTorque)));
	const float fAngAccel = vecAngAccel.Dot(GetCurrentPhysicsInst()->GetArchetype()->GetInvAngInertia());
	Assertf(fAngAccel < 15.0f,"fAngAccel: %f",fAngAccel);
#endif
	Assert(vecTorque.x==vecTorque.x && vecTorque.y==vecTorque.y && vecTorque.z==vecTorque.z);
	//Assert(GetCurrentPhysicsInst()->GetClassType()!=PH_INST_FRAG_PED);

	// wake up if it's not active
	if(CPhysics::GetLevel()->IsInactive(GetCurrentPhysicsInst()->GetLevelIndex()))
		ActivatePhysics();

	if(GetCollider())
		GetCollider()->ApplyTorqueAndForce(vecTorque,vecWorldPosition, vTimeStep.GetIntrin128());
}


void CPhysical::SetDamping( int dampingType, float dampingValue )
{	
	phInst* inst = GetCurrentPhysicsInst();
	if(Verifyf(inst, "CPhysical::SetDamping - No Instance on this CPhysical"))
	{
		phArchetypeDamp* pGtaArchetype = (phArchetypeDamp*)inst->GetArchetype();
		if(pGtaArchetype)
		{
			Vec3V dampingConst = Vec3VFromF32( dampingValue );
			pGtaArchetype->ActivateDamping( (phArchetypeDamp::MotionDampingType) dampingType, RC_VECTOR3(dampingConst) );
		}
	}
}

Vector3 CPhysical::GetDamping( int dampingType)
{	
	Vector3 vDamping = VEC3_ZERO;
	phInst* inst = GetCurrentPhysicsInst();
	if(Verifyf(inst, "CPhysical::GetDamping - No Instance on this CPhysical"))
	{
		phArchetypeDamp* pGtaArchetype = (phArchetypeDamp*)inst->GetArchetype();
		if(pGtaArchetype)
		{
			vDamping = const_cast<Vector3&>(pGtaArchetype->GetDampingConstant(dampingType));
		}
	}
	return vDamping;
}

void CPhysical::ApplyImpulse(const Vector3& vecImpulse, const Vector3& vecOffset, int nComponent, bool bLocalOffset)
{
	Assert(GetCurrentPhysicsInst());
	if(GetCurrentPhysicsInst()==NULL)
		return;

	Assert(GetCurrentPhysicsInst()->IsInLevel());
	IMPULSE_CHECK("[CPhysical::ApplyImpulse()]", vecImpulse, DEFAULT_IMPULSE_LIMIT);
	Assertf(vecOffset.Mag2() < square(2.0f*GetRadiusAroundLocalOrigin()),
		"Radius around local origin: %5.3f, force being applied at offset: %5.3f", GetRadiusAroundLocalOrigin(), vecOffset.Mag());
	Assert(vecImpulse.x==vecImpulse.x && vecImpulse.y==vecImpulse.y && vecImpulse.z==vecImpulse.z);
	Assert(vecOffset.x==vecOffset.x && vecOffset.y==vecOffset.y && vecOffset.z==vecOffset.z);

	// wake up if it's not active
	if(CPhysics::GetLevel()->IsInactive(GetCurrentPhysicsInst()->GetLevelIndex()))
		ActivatePhysics();

	if(GetCollider())
	{
		bool bAppliedViaNM = false;
		// Send the impulse via NM message if it's an active agent
		if (GetCurrentPhysicsInst()->GetClassType()==PH_INST_FRAG_PED)
		{
			fragInstNMGta *pedInst = static_cast<fragInstNMGta*>(GetCurrentPhysicsInst());
			if (pedInst->GetType()->GetARTAssetID() >= 0 && pedInst->GetNMAgentID() >= 0)
			{
#if __DEV
				if(fragInstNMGta::ms_bLogRagdollForces)
				{
					Displayf("Ragdoll Impulse %g Offset %g Local %d (Inst %p Time %d)\n", vecImpulse.Mag(), vecOffset.Mag(), bLocalOffset, GetCurrentPhysicsInst(), fwTimer::GetTimeInMilliseconds());
					Vector3 vecDirn(vecImpulse);
					vecDirn.NormalizeFast();
					Vector3 vStartOfLine;
					if (bLocalOffset)
					{
						const phBoundComposite* pRagdollBound = static_cast<const phBoundComposite*>(pedInst->GetArchetype()->GetBound());
						Assertf(nComponent >= 0 && nComponent < pRagdollBound->GetNumActiveBounds(), "CPhysical::ApplyImpulse - bad component.  input component is %d.  Num active bounds is %d.", nComponent, pRagdollBound->GetNumActiveBounds());
						Mat34V matResult;
						Transform(matResult, pedInst->GetMatrix(), pRagdollBound->GetCurrentMatrix(nComponent));
						RCC_MATRIX34(matResult).Transform(vecOffset, vStartOfLine);
					}
					else
					{
						vStartOfLine = vecOffset + VEC3V_TO_VECTOR3(GetTransform().GetPosition());
					}
					grcDebugDraw::Line(vStartOfLine, vStartOfLine - vecDirn, Color_white);
				}
#endif

				Vector3 hitPoint = bLocalOffset ? vecOffset : vecOffset + VEC3V_TO_VECTOR3(GetTransform().GetPosition());
				ART::MessageParams msg;
				msg.addBool(NMSTR_PARAM(NM_START), true);
				msg.addInt(NMSTR_PARAM(NM_APPLYIMPULSE_PART_INDEX), nComponent);
				msg.addVector3(NMSTR_PARAM(NM_APPLYIMPULSE_IMPULSE), vecImpulse.x, vecImpulse.y, vecImpulse.z);
				msg.addVector3(NMSTR_PARAM(NM_APPLYIMPULSE_HIT_POINT), hitPoint.x, hitPoint.y, hitPoint.z);
				msg.addBool(NMSTR_PARAM(NM_APPLYIMPULSE_LOCAL_HIT_POINT_INFO), bLocalOffset);
				pedInst->PostARTMessage(NMSTR_MSG(NM_APPLYIMPULSE_MSG), &msg);
				bAppliedViaNM = true;
			}
		}
		
		if (!bAppliedViaNM)
		{
			Vector3 vecWorldPos = vecOffset + VEC3V_TO_VECTOR3(GetTransform().GetPosition());
			if (bLocalOffset)
			{
				const phBound* pCurPhysBound = GetCurrentPhysicsInst()->GetArchetype()->GetBound();
				if (pCurPhysBound->GetType() == phBound::COMPOSITE)
				{
					Assertf(nComponent >= 0 && nComponent < static_cast<const phBoundComposite*>(pCurPhysBound)->GetNumActiveBounds(), "CPhysical::ApplyAngImpulse - bad component.  input component is %d.  Num active bounds is %d.", nComponent, static_cast<const phBoundComposite*>(pCurPhysBound)->GetNumActiveBounds());
					Mat34V matResult;
					Transform(matResult, GetCurrentPhysicsInst()->GetMatrix(), static_cast<const phBoundComposite*>(pCurPhysBound)->GetCurrentMatrix(nComponent));
					RCC_MATRIX34(matResult).Transform(vecOffset, vecWorldPos);
				}
			}
			GetCollider()->ApplyImpulse(vecImpulse, vecWorldPos, nComponent);
		}
	}
}
void CPhysical::ApplyImpulseCg(const Vector3& vecImpulse, s32 ASSERT_ONLY(impulseFlags))
{
	Assert(GetCurrentPhysicsInst());
	if(GetCurrentPhysicsInst()==NULL)
		return;

	Assert(GetCurrentPhysicsInst()->IsInLevel());
#if __ASSERT
	if(!(impulseFlags&IF_InitialImpulse))
	{
		IMPULSE_CHECK("[CPhysical::ApplyImpulseCg()]", vecImpulse, DEFAULT_IMPULSE_LIMIT);
	}
#endif // __ASSERT
	Assert(vecImpulse.x==vecImpulse.x && vecImpulse.y==vecImpulse.y && vecImpulse.z==vecImpulse.z);
	//Assert(GetCurrentPhysicsInst()->GetClassType()!=PH_INST_FRAG_PED);

	// wake up if it's not active
	if(CPhysics::GetLevel()->IsInactive(GetCurrentPhysicsInst()->GetLevelIndex()))
		ActivatePhysics();

	if(GetCollider())
	{
		GetCollider()->ApplyImpulseCenterOfMass(vecImpulse);
	}
}
void CPhysical::ApplyAngImpulse(const Vector3& vecAngImpulse, s32 ASSERT_ONLY(impulseFlags))
{
	Assert(GetCurrentPhysicsInst());
	if(GetCurrentPhysicsInst()==NULL)
		return;

	Assert(GetCurrentPhysicsInst()->IsInLevel());
#if __ASSERT
	if(!(impulseFlags&IF_InitialImpulse))
	{
		IMPULSE_CHECK("[CPhysical::ApplyAngImpulse()]", vecAngImpulse, DEFAULT_IMPULSE_LIMIT);
	}
#endif // __ASSERT
	Assert(IsFiniteAll(RCC_VEC3V(vecAngImpulse)));

	// wake up if it's not active
	if(CPhysics::GetLevel()->IsInactive(GetCurrentPhysicsInst()->GetLevelIndex()))
		ActivatePhysics();

	if(GetCollider())
	{
		GetCollider()->ApplyAngImpulse(vecAngImpulse);
	}
}
void CPhysical::ApplyAngImpulse(const Vector3& vecImpulse, const Vector3& vecOffset, int nComponent, bool bLocalOffset, s32 ASSERT_ONLY(impulseFlags))
{
	Assert(GetCurrentPhysicsInst());
	if(GetCurrentPhysicsInst()==NULL)
		return;

	Assert(GetCurrentPhysicsInst()->IsInLevel());
#if __ASSERT
	if(!(impulseFlags&IF_InitialImpulse))
	{
		IMPULSE_CHECK("[CPhysical::ApplyAngImpulse()]", vecImpulse, DEFAULT_IMPULSE_LIMIT);
	}
	// Check that the application offset isn't far outside the bound.
	float fCentroidRadius = 0.0f;
	Vector3 vWorldCentroid;
	fCentroidRadius = GetBoundCentreAndRadius(vWorldCentroid);
	Assertf(vecOffset.Mag2() < 1.1f*1.1f*fCentroidRadius*fCentroidRadius,
		"[CPhysical::ApplyAngImpulse()] model name: %s, offset from centroid=%5.3f (allowed radius=%5.3f).", GetModelName(),
		vecOffset.Mag(), 1.1f*fCentroidRadius);
#endif // __ASSERT
	Assert(vecImpulse.x==vecImpulse.x && vecImpulse.y==vecImpulse.y && vecImpulse.z==vecImpulse.z);
	Assert(vecOffset.x==vecOffset.x && vecOffset.y==vecOffset.y && vecOffset.z==vecOffset.z);
	//Assert(GetCurrentPhysicsInst()->GetClassType()!=PH_INST_FRAG_PED);

	// wake up if it's not active
	if(CPhysics::GetLevel()->IsInactive(GetCurrentPhysicsInst()->GetLevelIndex()))
		ActivatePhysics();

	if(GetCollider())
	{
		bool bAppliedViaNM = false;
		// Send the impulse via NM message if it's an active agent
		if (GetCurrentPhysicsInst()->GetClassType()==PH_INST_FRAG_PED)
		{
			fragInstNMGta *pedInst = static_cast<fragInstNMGta*>(GetCurrentPhysicsInst());
			if (pedInst->GetType()->GetARTAssetID() >= 0 && pedInst->GetNMAgentID() >= 0)
			{
				Vector3 hitPoint = bLocalOffset ? vecOffset : vecOffset + VEC3V_TO_VECTOR3(GetTransform().GetPosition());
				ART::MessageParams msg;
				msg.addBool(NMSTR_PARAM(NM_START), true);
				msg.addInt(NMSTR_PARAM(NM_APPLYIMPULSE_PART_INDEX), nComponent);
				msg.addVector3(NMSTR_PARAM(NM_APPLYIMPULSE_IMPULSE), vecImpulse.x, vecImpulse.y, vecImpulse.z);
				msg.addVector3(NMSTR_PARAM(NM_APPLYIMPULSE_HIT_POINT), hitPoint.x, hitPoint.y, hitPoint.z);
				msg.addBool(NMSTR_PARAM(NM_APPLYIMPULSE_LOCAL_HIT_POINT_INFO), bLocalOffset);
				msg.addBool(NMSTR_PARAM(NM_APPLYIMPULSE_ANGULAR_IMPULSE), true);
				pedInst->PostARTMessage(NMSTR_MSG(NM_APPLYIMPULSE_MSG), &msg);
				bAppliedViaNM = true;
			}
		}

		if (!bAppliedViaNM)
		{
			Vector3 vecObjectOffset = vecOffset;
			if (bLocalOffset)
			{
				const phBound* pCurPhysBound = GetCurrentPhysicsInst()->GetArchetype()->GetBound();
				if (pCurPhysBound->GetType() == phBound::COMPOSITE)
				{
					Assertf(nComponent >= 0 && nComponent < static_cast<const phBoundComposite*>(pCurPhysBound)->GetNumActiveBounds(), "CPhysical::ApplyAngImpulse - bad component.  input component is %d.  Num active bounds is %d.", nComponent, static_cast<const phBoundComposite*>(pCurPhysBound)->GetNumActiveBounds());
					RCC_MATRIX34(static_cast<const phBoundComposite*>(pCurPhysBound)->GetCurrentMatrix(nComponent)).Transform(vecOffset, vecObjectOffset);
				}
			}
			GetCollider()->ApplyAngImpulse(CrossProduct(vecObjectOffset, vecImpulse), nComponent);
		}
	}
}

void CPhysical::ApplyExternalForce(const Vector3& vecForce, const Vector3& vecOffset, int nComponent, int nPartIndex, phInst* pInst, float fBreakingForce, float timeStepOverride ASSERT_ONLY(, bool ignoreCentroidCheck) )
{
#if __ASSERT
	if(!IsBaseFlagSet(fwEntity::IS_FIXED))
	{
		FORCE_CHECK("[CPhysical::ApplyExternalForce()]", vecForce, DEFAULT_ACCEL_LIMIT);
	}
	if(!ignoreCentroidCheck)
	{
		OFFSET_FROM_CENTROID_CHECK("CPhysical::ApplyExternalForce()]", vecOffset, 1.1f);
	}
	Assert(vecForce.x==vecForce.x && vecForce.y==vecForce.y && vecForce.z==vecForce.z);
	Assert(vecOffset.x==vecOffset.x && vecOffset.y==vecOffset.y && vecOffset.z==vecOffset.z);
	//Assert(GetCurrentPhysicsInst()->GetClassType()!=PH_INST_FRAG_PED);
#endif // __ASSERT

	ScalarV vTimeStep = ScalarV(Selectf(timeStepOverride, timeStepOverride, fwTimer::GetTimeStep() / CPhysics::GetNumTimeSlices()));

	if(pInst==NULL)
	{
		Assert(GetCurrentPhysicsInst());
		pInst = GetCurrentPhysicsInst();
	}

	if(pInst)
	{
		Assert(pInst->IsInLevel());
		if(fBreakingForce == -1.0f)
		{
			CPhysics::GetSimulator()->ApplyForceAndBreakingForce(vTimeStep.GetIntrin128(), pInst->GetLevelIndex(), vecForce, vecOffset + VEC3V_TO_VECTOR3(GetTransform().GetPosition()), nComponent, nPartIndex, vecForce.Mag());
		}
		else
		{
			CPhysics::GetSimulator()->ApplyForceAndBreakingForce(vTimeStep.GetIntrin128(), pInst->GetLevelIndex(), vecForce, vecOffset + VEC3V_TO_VECTOR3(GetTransform().GetPosition()), nComponent, nPartIndex, fBreakingForce);
		}
	}
}

void CPhysical::ApplyExternalImpulse(const Vector3& vecImpulse, const Vector3& vecOffset, int nComponent, int nPartIndex, phInst* pInst, float fBreakingImpulse, bool bCheckOffset, float timeStepOverride)
{
#if __ASSERT
	if(!IsBaseFlagSet(fwEntity::IS_FIXED))
	{
		IMPULSE_CHECK("[CPhysical::ApplyExternalImpulse()]", vecImpulse, DEFAULT_IMPULSE_LIMIT);
	}
#endif // __ASSERT

	if(bCheckOffset)
	{
		OFFSET_FROM_CENTROID_CHECK("[CPhysical::ApplyExternalImpulse()]", vecOffset, 1.1f);
		Assert(vecImpulse.x==vecImpulse.x && vecImpulse.y==vecImpulse.y && vecImpulse.z==vecImpulse.z);
		Assert(vecOffset.x==vecOffset.x && vecOffset.y==vecOffset.y && vecOffset.z==vecOffset.z);
	}
	//GY - commenting out assert that checks for applying impulses to ragdollls.
	//I can't think of a reason not to apply an impulse to a ragdoll.
	//Besides, its quite limiting for scripts if they can't exert any control over ragdolling peds.
	//Assert(GetCurrentPhysicsInst()->GetClassType()!=PH_INST_FRAG_PED);

	ScalarV vTimeStep = ScalarV(Selectf(timeStepOverride, timeStepOverride, fwTimer::GetTimeStep() / CPhysics::GetNumTimeSlices()));

	if(pInst==NULL)
	{
		Assert(GetCurrentPhysicsInst());
		pInst = GetCurrentPhysicsInst();
	}

	if(pInst)
	{
		if (!Verifyf(pInst->IsInLevel(), "Someone is trying to apply in impulse to an instance that is not inserted presently in the physics engine"))
		{
			return;
		}

		if(-1==fBreakingImpulse)
		{
			//No breaking impulse specified.
			//Use the magnitude of the impulse as the magnitude of the breaking impulse.
			PHSIM->ApplyImpetusAndBreakingImpetus(vTimeStep.GetIntrin128(), pInst->GetLevelIndex(), vecImpulse, vecOffset + VEC3V_TO_VECTOR3(GetTransform().GetPosition()), nComponent, nPartIndex, vecImpulse.Mag());
		}
		else
		{
#if __BANK
			if(CPhysics::ms_bPrintBreakingImpulsesApplied)
			{
				Displayf("BreakingImpulse %f", fBreakingImpulse);
			}
#endif

			//Breaking impulse specified.
			//Use the magnitude of the specified breaking impulse.
			PHSIM->ApplyImpetusAndBreakingImpetus(vTimeStep.GetIntrin128(), pInst->GetLevelIndex(), vecImpulse, vecOffset + VEC3V_TO_VECTOR3(GetTransform().GetPosition()), nComponent, nPartIndex, fBreakingImpulse);
		}
	}
}

#if __ASSERT
void CPhysical::DumpOutOfWorldInfo()
{
	const Vec3V currentPos = GetTransform().GetPosition();

	// Check collision flags
	const u32 mapCollisionFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_MAP_TYPE_VEHICLE;
	u32 archetypeIncludeFlags = 0;
	u32 boundIncludeFlags = 0;
	if(const phInst* pInst = GetCurrentPhysicsInst())
	{
		if(pInst->IsInLevel())
		{
			boundIncludeFlags = archetypeIncludeFlags = CPhysics::GetLevel()->GetInstanceIncludeFlags(pInst->GetLevelIndex());
			if(const phArchetype* pArchetype = pInst->GetArchetype())
			{
				if(const phBound* pBound = pArchetype->GetBound())
				{
					if(phBound::IsTypeComposite(pBound->GetType()))
					{
						const phBoundComposite* pBoundComposite = static_cast<const phBoundComposite*>(pBound);
						if(pBoundComposite->GetTypeAndIncludeFlags())
						{
							boundIncludeFlags = 0;
							for(int componentIndex = 0; componentIndex < pBoundComposite->GetNumBounds(); ++componentIndex)
							{
								if(pBoundComposite->GetBound(componentIndex))
								{
									boundIncludeFlags |= pBoundComposite->GetIncludeFlags(componentIndex);
								}
							}
						}
					}
				}
			}
		}
	}

	Displayf("Entity is currently at (%5.3f, %5.3f, %5.3f).", VEC3V_ARGS(currentPos));
	Displayf("Entity pop-type:%s.", CTheScripts::GetPopTypeName(PopTypeGet()));
	Displayf("Owned by Script:[%s].", GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT ? "T" : "F");
	Displayf("GetAllowFreezeWaitingOnCollision:[%s].", GetAllowFreezeWaitingOnCollision() ? "T" : "F");
	Displayf("ShouldFixIfNoCollisionLoadedAroundPosition:[%s]",ShouldFixIfNoCollisionLoadedAroundPosition() ? "T" : "F");
	Displayf("ShouldLoadCollision:[%s]", ShouldLoadCollision() ? "T" : "F");
	Displayf("GetIsAnyFixedFlagSet:[%s]",GetIsAnyFixedFlagSet() ? "T" : "F");
	Displayf("GetIsFixedUntilCollisionFlagSet:[%s]",GetIsFixedUntilCollisionFlagSet() ? "T" : "F");
	Displayf("GetIsFixedFlagSet:[%s]",GetIsFixedFlagSet() ? "T" : "F");
	Displayf("GetIsFixedByNetworkFlagSet:[%s]",GetIsFixedByNetworkFlagSet() ? "T" : "F");
	Displayf("IsCollisionLoadedAroundPosition:[%s]",IsCollisionLoadedAroundPosition() ? "T" : "F");
	Displayf("CPhysical::IsCollisionLoadedAroundPosition:[%s]", CPhysical::IsCollisionLoadedAroundPosition() ? "T" : "F");
	Displayf("IsProtectedBaseFlagSet(fwEntity::USES_COLLISION):[%s]",IsProtectedBaseFlagSet(fwEntity::USES_COLLISION) ? "T" : "F");
	Displayf("Archetype Include Flags: 0x%X, Map[%s]",archetypeIncludeFlags, (archetypeIncludeFlags & mapCollisionFlags) != 0 ? "T" : "F");
	Displayf("Bound Include Flags: 0x%X, Map[%s]",boundIncludeFlags, (boundIncludeFlags & mapCollisionFlags) != 0 ? "T" : "F");
	if(const phCollider* pCollider = GetCollider())
	{
		Displayf("Collider Velocity: %5.2f, %5.2f, %5.2f", VEC3V_ARGS(pCollider->GetVelocity()));
		Displayf("Collider Angular Velocity: %5.2f, %5.2f, %5.2f", VEC3V_ARGS(pCollider->GetAngVelocity()));
		Displayf("Last Instance Position: %5.2f, %5.2f, %5.2f", VEC3V_ARGS(pCollider->GetLastInstanceMatrix().GetCol3()));
	}
	else
	{
		Displayf("No Collider");
	}
	if(GetIsAttached())
	{
		Displayf("Attach details: parent=%s.", GetAttachParent() ? GetAttachParent()->GetModelName() : "unknown");
		if(fwAttachmentEntityExtension* attachment = GetAttachmentExtension())
		{
			for(int constraintIndex = 0; constraintIndex < attachment->GetNumConstraintHandles(); ++constraintIndex)
			{
				phConstraintHandle constraintHandle = attachment->GetConstraintHandle(constraintIndex);
				if(phConstraintBase* pConstraint = CPhysics::GetConstraintManager()->GetTemporaryPointer(constraintHandle))
				{
					Displayf("%s Constraint:",pConstraint->GetTypeName());
					if(const phInst* pInstA = pConstraint->GetInstanceA())
					{
						Displayf("\tInstance A: %s, (%5.2f, %5.2f, %5.2f)", pInstA->GetArchetype()->GetFilename(), VEC3V_ARGS(pInstA->GetPosition()));
					}
					else
					{
						Displayf("\tInst A: NULL");
					}
					if(const phInst* pInstB = pConstraint->GetInstanceB())
					{
						Displayf("\tInstance B: %s, (%5.2f, %5.2f, %5.2f)", pInstB->GetArchetype()->GetFilename(), VEC3V_ARGS(pInstB->GetPosition()));
					}
					else
					{
						Displayf("\tInst B: NULL");
					}
				}
			}
		}
	}
	else
	{
		Displayf("Not attached.");
	}
}

bool CPhysical::IsOffsetWithinCentroidRadius(const char* msg, const Vector3& vecOffset, float fCentroidRadiusScale) const
{
	Vec3V vWorldPos = GetTransform().GetPosition() + RCC_VEC3V(vecOffset);
	Mat34V worldMatrix;
	const phCollider* pCollider = GetCollider();

	if( pCollider )
	{
		worldMatrix = pCollider->GetMatrix();
		vWorldPos = worldMatrix.d() + RCC_VEC3V(vecOffset);
	}

	Vec3V vWorldCentroid;
	float fCentroidRadius = 0.0f;
	if(GetCurrentPhysicsInst() && GetCurrentPhysicsInst()->GetArchetype())
	{
		if( pCollider && !GetIsTypePed() )
		{
			vWorldCentroid = GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetWorldCentroid( worldMatrix );
		}
		else
		{
			vWorldCentroid = GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetWorldCentroid(GetCurrentPhysicsInst()->GetMatrix());
		}
		fCentroidRadius = GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetRadiusAroundCentroid();
	}
	else
	{
		fCentroidRadius = GetBoundCentreAndRadius(RC_VECTOR3(vWorldCentroid));
	}

	fCentroidRadius *= fCentroidRadiusScale;
	bool bIsOffsetWithinCentroidRadius = DistSquared(vWorldPos, vWorldCentroid).Getf() <= fCentroidRadius*fCentroidRadius;

	if( !bIsOffsetWithinCentroidRadius )
	{
		Warningf("%s model name: %s, offset from centroid=%5.3f (allowed radius=%5.3f).", msg, GetModelName(),
			Dist(vWorldPos, vWorldCentroid).Getf(), fCentroidRadius);
	}

	return bIsOffsetWithinCentroidRadius;
}
#endif // __ASSERT

/*
void CPhysical::ApplyBreakingImpulse(const Vector3& vecImpulse, const Vector3& vecBreakingImpulse, const Vector3& vecOffset, int nComponent, phInst* pInst)
{
	Assert(IsBaseFlagSet(fwEntity::IS_FIXED) || vecImpulse.Mag()/rage::Max(1.0f, GetMass()) < 150.0f);
#if __ASSERT
	Vector3 vecCentroidOffset = vecOffset + GetPosition() - GetBoundCentre();
	Assert(vecCentroidOffset.Mag() < 1.5f*GetBoundRadius());//GetBoundRadius());
#endif
	Assert(vecImpulse.x==vecImpulse.x && vecImpulse.y==vecImpulse.y && vecImpulse.z==vecImpulse.z);
	Assert(vecOffset.x==vecOffset.x && vecOffset.y==vecOffset.y && vecOffset.z==vecOffset.z);

	if(pInst==NULL)
		pInst = GetCurrentPhysicsInst();

	if(pInst==NULL || !pInst->IsInLevel() || CPhysics::GetLevel()->IsFixed(pInst->GetLevelIndex()))
		return;

	Vector3 vecPosition = vecOffset + GetPosition();

	/////////////////////
	// FIRST PART IS COPIED FROM phSimulator::ApplyImpetus()
	// Create an impact data for the impetus application, and make the impetus an impulse.

	phImpactData impactData;
	impactData.Set(0,0,0,0.0f,vecPosition,ORIGIN,ORIGIN,ORIGIN);
	// USE breaking impulse for break calculation /////////////////////
	impactData.SetImpetus(vecBreakingImpulse);
	impactData.SetInstanceA(pInst);
	impactData.SetComponentA(nComponent);
	impactData.SetIsForce(false);

	// Get the collider, making the object active if it isn't already and should be when hit.
	phCollider* collider = CPhysics::GetSimulator()->TestPromoteInstance(pInst,&impactData);
	bool isActive = (collider!=NULL);
	if(pInst->IsBreakable(isActive))
	{
		// Get the instance from the impact data, because TestPromoteInstance may have put in another instance.
		pInst = impactData.GetInstanceA();

		// Make a list of breakable instances for handling multiple breaks with a single impetus application.
		phInstBreakable* breakableInstList[phInstBreakable::MAX_NUM_BREAKABLE_INSTS];
		int numBreakInsts = 0;
		phInstBreakable* breakableInst = static_cast<phInstBreakable*>(pInst);
		breakableInstList[numBreakInsts++] = breakableInst;

		/////////////////////
		// HERE ON IS COPIED FROM phSimulator::BreakObjects()
		// Try to break the object into pieces, recursing to try to break any breakable broken pieces.
		// BreakObjects(NULL,breakableInstList,numBreakInsts,&impactData);

		// Create the list of broken parts, for use in recalculating collisions.
		phInst* brokenInstList[phInstBreakable::MAX_NUM_BROKEN_INSTS];
		int numBrokenInsts = 0;

		phBreakData breakData0,breakData1;
		phBreakData* breakData = &breakData0;
		phBreakData* testBreakData = &breakData1;

		float breakStrength;
		Vector3 preBreakImpulse;
		phInstBreakable* weakestInst = CPhysics::GetSimulator()->FindWeakestInst(NULL, breakableInstList, numBreakInsts, &breakStrength, &impactData, &breakData, &testBreakData);

		if (weakestInst)
		{
			// Apply the pre-breaking impulse to the weakest object, changing its motion instantly, and
			// set the impulse to be the post-breaking impulse.
			Assert(breakStrength>=0.0f && breakStrength<=1.0f);

			// APPLY normal impulse after that ///////////////////////////
			preBreakImpulse.Scale(vecImpulse, breakStrength);

			phCollider *collider = CPhysics::GetSimulator()->GetCollider(weakestInst);
			if (collider)
			{
				// Apply the pre-breaking impulse to the unbroken collider.
				collider->ApplyImpulseChangeMotion(preBreakImpulse,impactData.GetPositionA(),impactData.GetComponentA());
			}
			else
			{
				// Apply the pre-breaking impulse to the unbroken instance.
				weakestInst->ApplyImpulseChangeMotion(preBreakImpulse,impactData.GetPositionA(),impactData.GetComponentA());
			}

			// Break the weakest breakable instance.
			numBrokenInsts = weakestInst->BreakApart(NULL, breakableInstList, &numBreakInsts, brokenInstList, &impactData, *breakData);
			Assert(numBrokenInsts<=phInstBreakable::MAX_NUM_BROKEN_INSTS);
			Assert(numBreakInsts<=phInstBreakable::MAX_NUM_BREAKABLE_INSTS);

			// Convert the total impulse to the post-breaking impulse.
			Vector3 impetus(vecImpulse);
			impetus.Subtract(preBreakImpulse);
			impactData.SetImpetus(impetus);
		}
		else
		{
			// APPLY normal impulse after that ///////////////////////////
			impactData.SetImpetus(vecImpulse);
		}

		// Apply the post-breaking part of the applied impulse to the specified broken parts.
		//Assert(numBrokenInsts==0 || numBreakInsts==0);
		for (int index=0; index<numBrokenInsts; index++)
		{
			phCollider *collider = CPhysics::GetSimulator()->GetCollider(brokenInstList[index]);
			if (collider)
			{
				collider->ApplyImpetus(impactData);
			}
			else
			{
				brokenInstList[index]->ApplyImpetus(impactData);
			}
		}

		// Apply the post-breaking part of the applied impulse to any unbroken breakable parts.
		for (int index=0; index<numBreakInsts; index++)
		{
			phCollider *collider = CPhysics::GetSimulator()->GetCollider(breakableInstList[index]);
			if (collider)
			{
				collider->ApplyImpetus(impactData);
			}
			else
			{
				breakableInstList[index]->ApplyImpetus(impactData);
			}
		}
	}
	else
	{
		CPhysics::GetSimulator()->ApplyImpulse(pInst->GetLevelIndex(), vecImpulse, vecOffset + GetPosition(), nComponent);
	}
}
*/

Vector3 CPhysical::GetLocalSpeed(const Vector3& vecOffset, bool bWorldOffset, int nComponent) const
{
	Assertf(vecOffset == vecOffset, "Input vecOffset is invalid.");

	Vector3 vecReturnVel = VEC3_ZERO;
	Vector3 vecWorldOffset = vecOffset;
	if(!bWorldOffset)
		vecWorldOffset += VEC3V_TO_VECTOR3(GetTransform().GetPosition());

	if(const phCollider* pCollider = GetCollider())
	{
		vecReturnVel = VEC3V_TO_VECTOR3(pCollider->GetLocalVelocity(vecWorldOffset,nComponent));
		Assertf(IsFiniteAll(RCC_VEC3V(vecReturnVel)), "%s collider local velocity on '%s' is invalid:"
			"\n\tVel: %5.2f, %5.2f, %5.2f"
			"\n\tAngVel: %5.2f, %5.2f, %5.2f"
			"\n\tCollider Pos: %5.2f, %5.2f, %5.2f"
			"\n\tPoint Pos: %5.2f, %5.2f, %5.2f", 
			pCollider->IsArticulated() ? "Articulated" : "Rigid", 
			GetDebugName(),
			VEC3V_ARGS(pCollider->GetVelocity()), 
			VEC3V_ARGS(pCollider->GetAngVelocity()), 
			VEC3V_ARGS(pCollider->GetPosition()), 
			VEC3V_ARGS(RCC_VEC3V(vecWorldOffset)));
	}
	else if(const phInst* pInst = GetCurrentPhysicsInst())
	{
		// Use the simulator functions as these query the externally controlled velocity if needed
		RC_VEC3V(vecReturnVel) = pInst->GetExternallyControlledLocalVelocity(vecWorldOffset,nComponent);
		Assertf(IsFiniteAll(RCC_VEC3V(vecReturnVel)), "Velocity is invalid.");
	}

	return vecReturnVel;
}

Vector3 CPhysical::GetLocalSpeedIntegrated(const float fTimeStep, Vec3V_In vecOffset, bool bWorldOffset, int nComponent) const
{
	//Assert that the incoming position is valid.
	Assertf(IsFiniteAll(vecOffset), "Initial position is invalid.");
	Assert(IsLessThanAll( Abs(vecOffset),Vec3V(V_FLT_LARGE_6)) );
	
	//Assign the velocity, angular velocity, and position.
	Vec3V vVel(V_ZERO);
	Vec3V vAngVel(V_ZERO);
	Vec3V vPos(V_ZERO);

	//Grab the collider and physics instance.
	const phCollider* pCollider = GetCollider();
	const phInst* pInst = GetCurrentPhysicsInst();
	
	if(pCollider)
	{
		vVel = pCollider->GetVelocity();
		Assertf(IsFiniteAll(vVel), "Collider velocity is invalid.");
		Assert(IsLessThanAll( Abs(vVel),Vec3V(V_FLT_LARGE_6)) );
		
		vAngVel = pCollider->GetAngVelocity();
		Assertf(IsFiniteAll(vAngVel), "Collider angular velocity is invalid.");
		Assert(IsLessThanAll( Abs(vAngVel),Vec3V(V_FLT_LARGE_6)) );
		
		vPos = pCollider->GetPosition();
		Assertf(IsFiniteAll(vPos), "Collider position is invalid.");
		Assert(IsLessThanAll( Abs(vPos),Vec3V(V_FLT_LARGE_6)) );
	}
	else if(pInst)
	{
		//Grab the pre-physics update position.
		vPos = GetMatrixPrePhysicsUpdate().GetCol3();
		Assertf(IsFiniteAll(vPos), "Instance position is invalid.");

		// Use the simulator functions as these query the externally controlled velocity if needed
		//Grab the velocity including the component's movement
		vVel = pInst->GetExternallyControlledVelocity(bWorldOffset ? vecOffset : vecOffset + vPos, nComponent);
		Assertf(IsFiniteAll(vVel), "Instance velocity is invalid.");
		
		//Grab the angular velocity.
		vAngVel = pInst->GetExternallyControlledAngVelocity();
		Assertf(IsFiniteAll(vAngVel), "Instance angular velocity is invalid.");
	}
	else
	{
		return VEC3_ZERO;
	}
	
	//Generate the integrated velocity for the angular velocity component, applied across the timestep.
	Vec3V vVelIntegrated(V_ZERO);
	
	//Ensure the angular velocity is non-zero.
	if(!IsZeroAll(vAngVel))
	{
		//Generate the offset.
		Vec3V vOffset;
		if(bWorldOffset)
		{
			vOffset = Subtract(vecOffset, vPos);
		}
		else
		{
			vOffset = vecOffset;
		}
		Assertf(IsFiniteAll(vOffset), "Offset is invalid.");
		Assert(IsLessThanAll( Abs(vOffset),Vec3V(V_FLT_LARGE_6)) );

		//Calculate the initial velocity component from the angular velocity.
		vVelIntegrated = Cross(vAngVel, vOffset);
		Assertf(IsFiniteAll(vVelIntegrated), "Initial velocity is invalid.");
		Assert(IsLessThanAll( Abs(vVelIntegrated),Vec3V(V_FLT_LARGE_6)) );

		//Generate the axis for the angular velocity.
		Vec3V vAxis = NormalizeSafe(vAngVel, Vec3V(V_ZERO));
		Assertf(IsFiniteAll(vAxis), "Axis is invalid.");
		Assert(IsLessThanAll( Abs(vAxis), Vec3V(V_FLT_LARGE_6)) );
		
		//Ensure the axis is valid.
		if(!IsZeroAll(vAxis))
		{
			//Generate the magnitude of the angular velocity.
			ScalarV scMagAngVel = Mag(vAngVel);
			Assertf(IsFiniteAll(scMagAngVel), "Angular velocity magnitude.");
			Assert(IsLessThanAll( Abs(scMagAngVel),ScalarV(V_FLT_LARGE_6)) );
			
			//Ensure the magnitude is valid.
			if(!IsZeroAll(scMagAngVel))
			{				
				//Create a rotation from the axis/angle pair.
				QuatV qRotator = QuatVFromAxisAngle(vAxis, Scale(scMagAngVel, ScalarVFromF32(fTimeStep)));
				Assertf(IsFiniteAll(qRotator), "qRotator is invalid.");

				//Rotate the offset.
				vOffset = Transform(qRotator, vOffset);
				Assertf(IsFiniteAll(vOffset), "Rotated offset is invalid.");
				Assert(IsLessThanAll( Abs(vOffset), Vec3V(V_FLT_LARGE_6)) );

				//Calculate the final velocity component from the angular velocity, after the timestep.
				Vec3V vFinal = Cross(vAngVel, vOffset);
				Assertf(IsFiniteAll(vFinal), "Final velocity is invalid.");
				Assert(IsLessThanAll( Abs(vFinal), Vec3V(V_FLT_LARGE_6)) );
				
				//Average the initial and final velocities.
				vVelIntegrated = Scale(::Add(vVelIntegrated, vFinal), ScalarV(V_HALF));
				Assertf(IsFiniteAll(vVelIntegrated), "Integrated velocity is invalid.");
				Assert(IsLessThanAll( Abs(vVelIntegrated), Vec3V(V_FLT_LARGE_6)) );
			}
		}
	}
	
	return VEC3V_TO_VECTOR3(::Add(vVel, vVelIntegrated));
}

float CPhysical::GetMassAlongVector(const Vector3& vecNormal, const Vector3& vecWorldPos)
{
	float fMassRot = 1.0f;

	if(GetCollider())
	{
		Vector3 vecCgOffset = vecWorldPos - RCC_VECTOR3(GetCollider()->GetPosition());
		Vector3 vecTemp = vecCgOffset;
		vecTemp.Cross(vecNormal);
		vecTemp.Multiply(GetCurrentPhysicsInst()->GetArchetype()->GetInvAngInertia());
		vecTemp.Cross(vecCgOffset);

		fMassRot = vecTemp.Dot(vecNormal);
		fMassRot = 1.0f / (GetCurrentPhysicsInst()->GetArchetype()->GetInvMass() + fMassRot);
	}
	else if(GetCurrentPhysicsInst())
	{
		fMassRot = GetCurrentPhysicsInst()->GetArchetype()->GetMass();
	}

	return fMassRot;
}

float CPhysical::GetMassAlongVectorLocal(const Vector3& vecNormal, const Vector3& vecLocalPos)
{
	float fMassRot = 1.0f;

	if(GetCollider())
	{
		Vector3 vecTemp = vecLocalPos;
		vecTemp.Cross( vecNormal );
		vecTemp.Multiply(GetCurrentPhysicsInst()->GetArchetype()->GetInvAngInertia());
		vecTemp.Cross( vecLocalPos );

		fMassRot = vecTemp.Dot( vecNormal );
		fMassRot = 1.0f / ( GetCurrentPhysicsInst()->GetArchetype()->GetInvMass() + fMassRot );
	}
	else if(GetCurrentPhysicsInst())
	{
		fMassRot = GetCurrentPhysicsInst()->GetArchetype()->GetMass();
	}

	return fMassRot;
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	ResetWaterStatus
// Purpose		:	Reset the water status into an unknown state so a full
//					rescan and update occurs.
// Parameters	:	None.
// Returns		:	Nothing.
/////////////////////////////////////////////////////////////////////////////////
void CPhysical::ResetWaterStatus()
{
	m_nFlags.bPossiblyTouchesWater = true;
	m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate = false;
	m_Buoyancy.m_buoyancyFlags.bBuoyancyInfoUpToDate = false;
	//m_Buoyancy.SetStatusHighNDry(); B* 1236461, this messes up underwater timers and lots of code sets matricies or position
}


/////////////////////////////////////////////////////////////////////////////////
// Name			:	ProcessControl
// Purpose		:	Perfroms a frame update of the physical object.
// Parameters	:	None.
// Returns		:	Nothing.
/////////////////////////////////////////////////////////////////////////////////
bool CPhysical::ProcessControl()
{
	return true;
}


void CPhysical::ProcessPrePhysics()
{

	// 6/12/12 - cthomas - Prefetch the physics instance now, so it is ready to be used later 
	// (UpdatePossiblyTouchesWaterFlag(), etc.). Switched the call to UpdateFixedWaitingForCollision() 
	// to happen before the call to UpdatePossiblyTouchesWaterFlag(), since they don't appear to 
	// have any dependencies, and that allows a little more time for the prefetch to do its work.
	PrefetchObject(GetCurrentPhysicsInst());
	PrefetchObject(&m_Buoyancy);

    //bool bIsAsleep = false;
	//if(GetCollider() && GetCollider()->GetSleep() && GetCollider()->GetSleep()->IsAsleep())
    //    bIsAsleep = true;

	// Reset the in water flags
	if(GetIsInWater())
	{
		m_nPhysicalFlags.bWasInWater = true;
		// the is in water flag is dictated by the owner of the object in network games
		if(!IsNetworkClone())
		{
			SetIsInWater( false );
		}
	}
	else
	{
		m_nPhysicalFlags.bWasInWater = false;
	}


	// Update the fixed waiting for collision flag, this is done directly before the physics update so it
	// is current for the simulator update.
	// NOTE: UpdateFixedWaitingForCollision can deactivate the collider so we shouldn't store a pointer to it before the function
	UpdateFixedWaitingForCollision(GetCollider() != NULL);

	phCollider* pCollider = GetCollider();
	if(pCollider)
	{
		// Ragdoll peds are handled differently here since they aren't getting their groundPhysical set like animated peds.
		// Instead they use an asynch probe every few frames so they can't have their ref frame velocities getting cleared each frame.
		if (!(GetIsTypePed() && static_cast<CPed*>(this)->GetUsingRagdoll()) && !(GetIsTypeObject() && static_cast<CObject*>(this)->GetAsProjectile()))
		{
			//make sure the reference velocity frame is 0 as it gets set every frame
			pCollider->SetReferenceFrameVelocity(ORIGIN);
			pCollider->SetReferenceFrameAngularVelocity(ORIGIN);
		}
	}

	// Update kinematic physics activation / deactivation here
	if (ShouldUseKinematicPhysics() && CanUseKinematicPhysics())
	{
		if (!IsUsingKinematicPhysics())
		{
			if (EnableKinematicPhysics())
			{
				UpdatePhysicsFromEntity(false);
			}
		}
	}
	else
	{
		if (IsUsingKinematicPhysics())
		{
			DisableKinematicPhysics();
		}
	}

	if ( IsUsingKinematicPhysics() && !GetIsTypePed())
	{
		// need to set the reserved last instance matrix in the physics level here
		// for some reason this never gets set when the entity is in kinematic mode
		if (GetInstForKinematicMode() && GetInstForKinematicMode()->HasLastMatrix())
		{
			PHLEVEL->SetLastInstanceMatrix(GetInstForKinematicMode(), GetMatrix());
		}

		//reset the velocity of the entity to the requested animated velocity
		if (GetCollider() && GetAnimDirector())
		{
			fwDynamicEntityComponent *dynComp = GetDynamicComponent();
			
			if (dynComp)
			{
				GetCollider()->SetVelocity(dynComp->GetAnimatedVelocity());
				GetCollider()->SetAngVelocity(Vector3(0.0f, 0.0f, dynComp->GetAnimatedAngularVelocity()));
			}
		}
	}

	// Update the water flags only if it's actually moving. 
	// If the simulator is moving it, it will have a collider. If the game is moving it, it is responsible for marking the flag as out of date. 
	if(!m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate || pCollider)
	{
		// This needs to be updated to render objects correctly as far as being above/below water is concerned.
		UpdatePossiblyTouchesWaterFlag();
	}

	if (GetNetworkObject())
		static_cast<CNetObjEntity *>(GetNetworkObject())->UpdateBeforePhysics();
}

void CPhysical::ProcessPostPhysics()
{
	SetShouldUseKinematicPhysics(false);

	if (GetNetworkObject())
		static_cast<CNetObjEntity *>(GetNetworkObject())->UpdateAfterPhysics();
}

bool CPhysical::CanStraddlePortals() const
{
	const fwEntity* pAttachParent = GetAttachParentForced();
	return (pAttachParent && pAttachParent == CGameWorld::FindLocalPlayer());
}

bool CPhysical::EnableKinematicPhysics()
{
	phInst *pKinematicInst = GetInstForKinematicMode();
	if (pKinematicInst && pKinematicInst->IsInLevel())
	{
		m_nPhysicalFlags.bIsUsingKinematicPhysics = 1;

		return true;
	}

	return false;
}

bool CPhysical::DisableKinematicPhysics()
{
	phInst *pKinematicInst = GetInstForKinematicMode();
	if(pKinematicInst && pKinematicInst->IsInLevel() )
	{
		m_nPhysicalFlags.bIsUsingKinematicPhysics = 0;

		return true;
	}

	return false;
}

bool CPhysical::IsMountEntryExitAttachment() const
{
	return (GetAttachState() == ATTACH_STATE_PED_ON_MOUNT);
}

bool CPhysical::IsVehicleEntryExitAttachment() const
{
	int attachState = GetAttachState();

	if (attachState==ATTACH_STATE_PED_ENTER_CAR || attachState==ATTACH_STATE_PED_IN_CAR || attachState==ATTACH_STATE_PED_EXIT_CAR)
	{
		return true;
	}
	return false;
}

#if __BANK || !__NO_OUTPUT

const char *CPhysical::GetDebugName() const
{
    netObject *networkObject = GetNetworkObject();

    if(networkObject)
    {
        return networkObject->GetLogName();
    }
    else
    {
        return GetModelName();
    }
}

#endif // __BANK

#if __DEV
	dev_bool FORCE_COLLISION_NOT_LOADED = false;
#endif

bool CPhysical::IsCollisionLoadedAroundPosition()
{
#if __DEV
	if( FORCE_COLLISION_NOT_LOADED )
	{
		return false;
	}
#endif

	// The interior is suspending this object until it can be placed
	if( GetIsRetainedByInteriorProxy() )
	{
		return false;
	}

	// if in an interior and interior is in an active state, then collisions are available
	fwInteriorLocation intLoc = GetInteriorLocation();
	if (intLoc.IsValid())
	{
		CInteriorInst* pIntInst = CInteriorInst::GetInteriorForLocation(intLoc);
		Assert(pIntInst);

		if (!pIntInst->GetProxy()->HasStaticBounds())
		{
			if (pIntInst->GetCurrentPhysicsInst()
				&& (pIntInst->GetCurrentPhysicsInst()->IsInLevel()) )
			{
				return(true);
			} else {
				return(false);
			}
		}
	}

	// World collision isn't loaded around this entity
	Vec3V vThisPosition = GetTransform().GetPosition();
	if (g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(vThisPosition, fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER))
	{
		return true;
	}
	else
	{
		return false;
	}
}


///////////////////////////////////////////////////////////////////////////
// FUNCTION : PlacePhysicalRelativeToOtherPhysical
// PURPOSE :  Used by the script. An object gets placed relative to another object
//			  and it's taking its speed at the same time.
///////////////////////////////////////////////////////////////////////////

void CPhysical::PlacePhysicalRelativeToOtherPhysical(CPhysical *pBasePhysical, CPhysical *pToBePlacedPhysical, const Vector3& Offset)
{
	Vector3	NewCoors;
	// Find the coordinates of the second bloke
	NewCoors = pBasePhysical->TransformIntoWorldSpace(Offset);

	// Unfortunately the Scripts are being processed BEFORE the world. This gives
	// the car another frame to move and the objects seem to slide. To fight this we
	// take the velocity of the object into account for the next frame.
	NewCoors += pBasePhysical->GetVelocity() * (50.0f*fwTimer::GetTimeStep() * 0.9f);

	CGameWorld::Remove(pToBePlacedPhysical);

	pToBePlacedPhysical->SetMatrix(MAT34V_TO_MATRIX34(pBasePhysical->GetMatrix()), false, false, false);
	pToBePlacedPhysical->SetPosition(NewCoors, false, true, false);			// only update physics reps on the second call!

	pToBePlacedPhysical->SetVelocity(pBasePhysical->GetVelocity());

	// assume it's going outside - otherwise we need to do a probe
	CGameWorld::Add(pToBePlacedPhysical, CGameWorld::OUTSIDE );									// adding to game world will update game world rep
}



bool CPhysical::TestNoCollision(const phInst *pOtherInst)
{
	bool bNoCollision = false;
	u32 noCollisionFlags = GetNoCollisionFlags();
	const CEntity* pOtherEntity = CPhysics::GetEntityFromInst(pOtherInst);

	if(noCollisionFlags &NO_COLLISION_NETWORK_OBJECTS)
	{
		if(pOtherEntity && pOtherEntity->IsCollisionEnabled() && pOtherEntity->GetIsPhysical() && pOtherEntity!=this && NetworkUtils::GetNetworkObjectFromEntity(pOtherEntity))
		{
			bNoCollision = true;
		}
	}
	else
	if(pOtherInst && pOtherInst->GetUserData()==GetNoCollisionEntity())
		bNoCollision = true;

	if(bNoCollision)
	{
		// if we're interested in whether any impacts occured
		SetNoCollisionFlags((u8) (noCollisionFlags | NO_COLLISION_HIT_SHOULD_FIND_IMPACTS));
		return true;
	}

	return false;
}

bool CPhysical::GetIsPhysicalAParentAttachment(CPhysical* pPhysical) const
{
	CPhysical* pParent = static_cast<CPhysical*>(GetAttachParentForced());
	bool bReturn = false;
	while(!bReturn && pParent)
	{
		bReturn = (pParent == pPhysical);
		pParent = static_cast<CPhysical*>(pParent->GetAttachParentForced());
	}

	return bReturn;
}

#if __BANK
void CPhysical::DebugAttachToPhysicalBasic(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine, CPhysical* pPhysical, s16 nEntBone, u32 nAttachFlags, const Vector3* pVecOffset, const Quaternion* pQuatOrientation, s16 nMyBone)
#else
void CPhysical::AttachToPhysicalBasic(CPhysical* pPhysical, s16 nEntBone, u32 nAttachFlags, const Vector3* pVecOffset, const Quaternion* pQuatOrientation, s16 nMyBone )
#endif
{
	Assert(GetAttachState()!=ATTACH_STATE_PHYSICAL);
	Assert(pPhysical);
	ASSERT_ONLY( if(!Verifyf(pPhysical != this, "Attempting to perform a basic attachment of an entity to itself, which the attachment tree will not support")) return; )
	if(pPhysical==NULL) return;	

	fwAttachmentEntityExtension *extension = CreateAttachmentExtensionIfMissing();

	// Need to prevent loops of attachments
	// See if we are already attached to pPhysical
	if(extension->GetAttachParentForced() != pPhysical && GetIsPhysicalAParentAttachment(pPhysical))
	{
		// If we are actually in a detaching state then its OK to just clean up instantly
		if(extension->GetAttachState() == ATTACH_STATE_DETACHING)
		{
			// Instantly detach
			DetachFromParent(0);
		}
		else
		{
			if( extension->GetAttachParentForced() )
			{
				Assertf(false, "%s has parent %s already", CModelInfo::GetBaseModelInfoName(GetModelId()), extension->GetAttachParentForced()->GetModelName());
			}
			Assertf(false,"[0] Attaching %s to %s . This will create a closed loop of attachments! Attachment will abort", CModelInfo::GetBaseModelInfoName(GetModelId()), CModelInfo::GetBaseModelInfoName(pPhysical->GetModelId()) );
			return;
		}
	}

	// See if pPhysical is already attached to us:
	if(pPhysical->GetIsPhysicalAParentAttachment(this))
	{
		// If pPhysical is actually in a detaching state then its OK to just clean up instantly
		if(pPhysical->GetAttachState() == ATTACH_STATE_DETACHING)
		{
			// Instantly detach
			pPhysical->DetachFromParent(0);
		}
		else
		{
			Assertf(false,"[1] Attaching %s to %s . This will create a closed loop of attachments! Attachment will abort", CModelInfo::GetBaseModelInfoName(GetModelId()), CModelInfo::GetBaseModelInfoName(pPhysical->GetModelId()) );
			return;
		}
	}

#if GTA_REPLAY
	bool dontRecord = false;
	if(GetAttachParentForced() == pPhysical)
	{	// Don't record the attachment if it's already attached to this physical
		dontRecord = true;
	}
#endif // GTA_REPLAY

#if __ASSERT
	//Ensure that the parent's attach bone isn't one that can scale
	if(nEntBone > -1 && pPhysical->GetIsDynamic() && pPhysical->GetSkeleton())
	{
		const crBoneData *boneData = pPhysical->GetSkeleton()->GetSkeletonData().GetBoneData( nEntBone );
		physicsAssertf(boneData->HasDofs(crBoneData::SCALE) == false, "Attaching a %s to a %s's %s bone, which can scale",
			 CModelInfo::GetBaseModelInfoName(GetModelId()), CModelInfo::GetBaseModelInfoName(pPhysical->GetModelId()), boneData->GetName());
	}
#endif

	Assertf(!pVecOffset || pVecOffset->IsEqual(*pVecOffset), "Invalid offset specified for attachment.");
	Assertf(!pVecOffset || pVecOffset->Mag2() < 1000000.0f, "Invalid offset specified for attachment (offset too large).");
	Assertf(!pQuatOrientation || pQuatOrientation->IsEqual(*pQuatOrientation), "Invalid orientation specified for attachment.");

	extension->AttachToEntity(this, pPhysical, nEntBone, nAttachFlags, pVecOffset, pQuatOrientation, NULL, nMyBone );

#if __BANK
	extension->DebugSetInvocationData(strCodeFile, strCodeFunction, nCodeLine);
#endif

	if(GetIsTypePed())
		((CPed*)this)->DeactivatePhysicsAndDealWithRagdoll();
	else
		DeActivatePhysics();

	// When attached we never want to activate
	if(GetCurrentPhysicsInst() && GetCurrentPhysicsInst()->IsInLevel())
	{
		physicsAssertf(!CPhysics::GetLevel()->IsActive(GetCurrentPhysicsInst()->GetLevelIndex()),
			"object(%s) should have been deactivated already.", GetModelName());
		GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, true);

		if(!extension->GetAttachFlag(ATTACH_FLAG_COL_ON))
		{
			// Set cached include flags in the level
			if( GetCurrentPhysicsInst()->IsInLevel() )
			{
				CPhysics::GetLevel()->SetInstanceIncludeFlags(GetCurrentPhysicsInst()->GetLevelIndex(), u32(ArchetypeFlags::GTA_BASIC_ATTACHMENT_INCLUDE_TYPES));
			}
		}

		extension->SetAttachFlag(ATTACH_FLAG_NOT_IN_LEVEL_INITIALLY, !GetCurrentPhysicsInst()->IsInLevel());
		extension->SetAttachFlag(ATTACH_FLAG_NO_PHYSICS_INST_YET, false);
		if(pPhysical->GetCurrentPhysicsInst() && pPhysical->GetCurrentPhysicsInst()->GetInstFlag(phInst::FLAG_QUERY_EXTERN_VEL) && extension->GetAttachFlag(ATTACH_FLAG_COL_ON))
		{
			GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_QUERY_EXTERN_VEL,true);
		}	
	}
	else
	{
		// Physics inst not available yet. Flag this so that we can deal with this correctly when processing
		// attachment when it does stream in.
		extension->SetAttachFlag(ATTACH_FLAG_NOT_IN_LEVEL_INITIALLY, true);
		extension->SetAttachFlag(ATTACH_FLAG_NO_PHYSICS_INST_YET, true);
	}
	
	if(extension->GetAttachFlag(ATTACH_FLAG_COL_ON))
	{
		EnableCollision();		
	}
	else
	{
		bool completely = false; 
		if(NetworkInterface::IsGameInProgress() && GetNetworkObject() && GetNetworkObject()->IsClone())
		{
			CNetObjEntity* netEnt = SafeCast(CNetObjEntity, GetNetworkObject());
			if(netEnt)
			{
				completely = netEnt->GetDisableCollisionCompletely();
			}
		}
		// This prevents rigid body collision
		DisableCollision(NULL, completely);
	}

	if(IsInPathServer())
	{
		CAssistedMovementRouteStore::MaybeRemoveRouteForDoor(this);
		CPathServerGta::MaybeRemoveDynamicObject(this);
	}

	//If the entity being attached is a cloth then we need to transform the cloth
	//verts to the entity frame.
	bool bIsEnvClothObject = false;
	if(GetCurrentPhysicsInst())
	{
		if((GetCurrentPhysicsInst()->IsInLevel() &&
			CPhysics::GetLevel()->GetInstanceTypeFlags(GetCurrentPhysicsInst()->GetLevelIndex())==ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE)
			||
			(GetCurrentPhysicsInst()->GetArchetype() &&
			GetCurrentPhysicsInst()->GetArchetype()->GetTypeFlags()==ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE))
		{
			bIsEnvClothObject = true;
		}
	}

/* // NOTE: Fix for B*2588068 (disable the following if block ) . this is some very very old code which shouldn't be here at all - svetli ( it has beed removed on rdr3 for sometime now )
	if(bIsEnvClothObject)
	{
		//Process child attachments of physical to force a setting of the cloth entity transform.
		pPhysical->ProcessAllAttachments();
		//Now transform the cloth verts to the frame of the cloth entity.
		phInst* pEntityInst=GetCurrentPhysicsInst();
		Assertf(dynamic_cast<fragInst*>(pEntityInst), "Physical with cloth must be a frag");
		fragInst* pEntityFragInst=static_cast<fragInst*>(pEntityInst);
		Assertf(pEntityFragInst->GetCached(), "Cloth entity has no cache entry");
		if(pEntityFragInst->GetCached())
		{
			fragCacheEntry* pEntityCacheEntry=pEntityFragInst->GetCacheEntry();
			Assertf(pEntityCacheEntry, "Cloth entity has a null cache entry");
			fragHierarchyInst* pEntityHierInst=pEntityCacheEntry->GetHierInst();
			Assertf(pEntityHierInst, "Cloth entity has a null hier inst");
			environmentCloth* pEnvCloth=pEntityHierInst->envCloth;
#if NO_PIN_VERTS_IN_VERLET
			const int numPinVerts = pEnvCloth->GetCloth()->GetClothData().GetNumPinVerts();
#else
			const int numPinVerts = pEnvCloth->GetCloth()->GetPinCount();
#endif
			pEnvCloth->GetCloth()->GetClothData().TransformVertexPositions(GetMatrix(), numPinVerts);

		}
	}
*/

	if(nAttachFlags & ATTACH_FLAG_INITIAL_WARP)
	{
		Matrix34 matOffset;
		matOffset.Identity();

		matOffset.FromQuaternion(extension->GetAttachQuat());
		matOffset.d = extension->GetAttachOffset();
        Assert(matOffset.IsOrthonormal());

		Matrix34 matParent = MAT34V_TO_MATRIX34(extension->GetAttachParentForced()->GetMatrix());
		if(extension->GetOtherAttachBone() > -1 && ((CPhysical *) extension->GetAttachParentForced())->GetIsDynamic() && (static_cast<CPhysical*>(GetAttachParentForced()))->GetSkeleton())
		{
			const CDynamicEntity* pDynEnt = static_cast<CPhysical*>(extension->GetAttachParentForced());
			// check if global skeleton matrices are actually going to be valid in the first place (only peds seem to store this info)
						
			pDynEnt->GetGlobalMtx(extension->GetOtherAttachBone(), matParent);

			// B*1853481 & 1880214: Ensure we get a valid matrix even for damaged frag objects.
			// If the standard GetGlobalMtx call gives a zeroed matrix, try grabbing the matrix from the damaged skeleton.
			// Was previously using CVfxHelper::GetMatrixFromBoneIndex, but that doesn't use the entity GetGlobalMtx method; it accesses the skeleton directly.
			// This caused a problem as CVehicle implements a special version of that method to handle cases where mod kits are in use. For some reason modded parts have zeroed out bone matrices in the frag inst.
			if(matParent.a.IsZero())
			{			
				if(pDynEnt->GetFragInst() && pDynEnt->GetFragInst()->GetCacheEntry() && pDynEnt->GetFragInst()->GetCacheEntry()->GetHierInst())
				{
					if(pDynEnt->GetFragInst()->GetCacheEntry()->GetHierInst()->damagedSkeleton)
					{
						Mat34V matParentV;
						pDynEnt->GetFragInst()->GetCacheEntry()->GetHierInst()->damagedSkeleton->GetGlobalMtx(extension->GetOtherAttachBone(), matParentV);
						matParent.Set(MAT34V_TO_MATRIX34(matParentV));
					}
					else
					{
						matParent.Identity3x3();
						physicsDisplayf("Parent %s has bone %d with non-orthonormality %f, setting to identity", CModelInfo::GetBaseModelInfoName(pPhysical->GetModelId()), extension->GetOtherAttachBone(), matParent.MeasureNonOrthonormality());
					}

				}
			}
		}

		Assertf(matParent.IsOrthonormal(),"Parent %s has bone %d with non-orthonormality %f", CModelInfo::GetBaseModelInfoName(pPhysical->GetModelId()), extension->GetOtherAttachBone(), matParent.MeasureNonOrthonormality());

		if( extension->GetMyAttachBone() == -1 )
		{
			Matrix34 matNew;
			matNew.Dot(matOffset, matParent);

			matNew.Normalize(); // Normalise it to avoid any orthonormal asserts lower down, this fn is called infrequently anyway

			Assert(matNew.IsOrthonormal());
			extension->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, true); // Necessary to stop spurious assert in fwEntity::SetMatrix().
			SetMatrix(matNew);
			extension->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, false);
		}
	}

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord() && !dontRecord)
	{
		CReplayMgr::RecordEntityAttachment(GetReplayID(), pPhysical->GetReplayID());
	}
#endif // GTA_REPLAY

#if __BANK
	if(NetworkInterface::IsGameInProgress())
	{
		if(pPhysical->GetIsTypeVehicle() && pPhysical->PopTypeIsMission() && this->GetIsTypeObject() && this->PopTypeIsMission())
		{
			physicsDisplayf("[ATTACH DEBUG] Attaching %s to %s.", GetDebugNameFromObjectID(), pPhysical->GetDebugNameFromObjectID());
		}
	}
#endif
}

#if __BANK
void CPhysical::DebugAttachToWorldBasic(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine, u32 nAttachFlags, const Vector3 &vecWorldPos, const Quaternion* pQuatOrientation)
#else
void CPhysical::AttachToWorldBasic(u32 nAttachFlags, const Vector3 &vecWorldPos, const Quaternion* pQuatOrientation)
#endif
{
	fwAttachmentEntityExtension *extension = CreateAttachmentExtensionIfMissing();

	Assert(extension->GetAttachState() != ATTACH_STATE_PHYSICAL);

	extension->AttachToEntity(this, NULL, -1, nAttachFlags, &vecWorldPos, pQuatOrientation);

#if __BANK
	extension->DebugSetInvocationData(strCodeFile, strCodeFunction, nCodeLine);
#endif
	
	DeActivatePhysics();
	if(GetCurrentPhysicsInst())
	{
		GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, true);
		extension->SetAttachFlag(ATTACH_FLAG_NO_PHYSICS_INST_YET, false);
		extension->SetAttachFlag(ATTACH_FLAG_NOT_IN_LEVEL_INITIALLY, !GetCurrentPhysicsInst()->IsInLevel());
	}
	else
	{
		// Physics inst not available yet. Flag this so that we can deal with this correctly when processing
		// attachment when it does stream in.
		extension->SetAttachFlag(ATTACH_FLAG_NO_PHYSICS_INST_YET, true);
		extension->SetAttachFlag(ATTACH_FLAG_NOT_IN_LEVEL_INITIALLY, true);
	}
	
	if(extension->GetAttachFlag(ATTACH_FLAG_COL_ON))
	{
		EnableCollision();
	}
	else
	{
		DisableCollision();
	}

	if(IsInPathServer())
	{
		CAssistedMovementRouteStore::MaybeRemoveRouteForDoor(this);
		CPathServerGta::MaybeRemoveDynamicObject(this);
	}

	extension->SetAttachFlags((u32) (extension->GetAttachFlags() | ATTACH_FLAG_ATTACHED_TO_WORLD_BASIC));

#if __ASSERT
	// Checking for QNans...bogus attachment offsets.
	Assertf(vecWorldPos == vecWorldPos, "Invalid world position for attachment.");
	Vector3 vOffset = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
	Assertf(vOffset == vOffset, "Invalid physical position for attachment.");
	vOffset -= vecWorldPos;
	Assertf(vOffset.Mag2() < 1000000.0f, "Invalid offset specified for attachment (offset too large).");
	Assertf(!pQuatOrientation || pQuatOrientation->IsEqual(*pQuatOrientation), "Invalid orientation specified for attachment.");
#endif

}

#define MAKE_ALLOWED_SEPARATION_ZERO_IF_WARPING (1)

#if __BANK
bool CPhysical::DebugAttachToPhysicalUsingPhysics(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine, CPhysical* pPhysical, s16 nParentBone, s16 nMyBone, u32 nAttachFlags, const Vector3* pParentAnchorLocalSpace, const Quaternion* pQuatOrientation, const Vector3* pMyAnchorLocalSpace, float fStrength, bool bAllowInitialSeparation, float massInvScaleA, float massInvScaleB)
#else
bool CPhysical::AttachToPhysicalUsingPhysics(CPhysical* pPhysical, s16 nParentBone, s16 nMyBone, u32 nAttachFlags, const Vector3* pParentAnchorLocalSpace, const Quaternion* pQuatOrientation, const Vector3* pMyAnchorLocalSpace, float fStrength, bool bAllowInitialSeparation, float massInvScaleA, float massInvScaleB)
#endif
{	
	fwAttachmentEntityExtension *extension = CreateAttachmentExtensionIfMissing();

//1. Check preconditions
	// Make sure we aren't already attached physically, or if we are that we are attached to the same object as we are attaching to now
	Assert(extension->GetAttachState()!=ATTACH_STATE_PHYSICAL || pPhysical == extension->GetAttachParent());
	Assert(pPhysical);
	ASSERT_ONLY( if(!Verifyf(pPhysical != this, "Attempting to perform a physical attachment of an entity to itself, which the attachment tree will not support")) return false; )
	if(pPhysical==NULL)
    {
        return false;
    }

	// Need to prevent loops of attachments
	if(extension->GetAttachParentForced() != pPhysical && GetIsPhysicalAParentAttachment(pPhysical))
	{
		Assertf(false,"Attempting to attach a physical to a grandparent");
		return false;
	}

	if(nMyBone != -1 && 
		(!Verifyf(GetSkeleton(), "[%s] has no skeleton but has been passed a bone index of [%d]", GetModelName(), nMyBone) ||
		!Verifyf(nMyBone < GetSkeleton()->GetBoneCount(), "Bone index [%d] is not valid for skeleton [%s], range [0..%d]", nMyBone, GetModelName(), GetSkeleton()->GetBoneCount())))
	{
		// Clear the bone index
		nMyBone = -1;
	}

	if(nParentBone != -1 && 
		(!Verifyf(pPhysical->GetSkeleton(), "[%s] has no skeleton but has been passed a bone index of [%d]", pPhysical->GetModelName(), nParentBone) ||
		!Verifyf(nParentBone < pPhysical->GetSkeleton()->GetBoneCount(), "Bone index [%d] is not valid for skeleton [%s], range [0..%d]", nParentBone, pPhysical->GetModelName(), pPhysical->GetSkeleton()->GetBoneCount())))
	{
		// Clear the bone index
		nParentBone = -1;
	}

//2. Create the two nodes and edge between them in the tree of entities if necessary
	extension->AttachToEntity(this, pPhysical, nParentBone, nAttachFlags, pParentAnchorLocalSpace, pQuatOrientation, pMyAnchorLocalSpace, nMyBone);

#if __BANK
	extension->DebugSetInvocationData(strCodeFile, strCodeFunction, nCodeLine);
#endif

	if(!Verifyf(pPhysical->GetCurrentPhysicsInst(), "Parent [%s] has no physics", pPhysical->GetModelName()) ||
		!Verifyf(GetCurrentPhysicsInst(), "Physical [%s] has no physics", GetModelName()))
	{
		// Cannot physically attach objects that are not physically active
		extension->SetAttachFlag(ATTACH_FLAG_NO_PHYSICS_INST_YET, true);
		return false;
	}
	extension->SetAttachFlag(ATTACH_FLAG_NO_PHYSICS_INST_YET, false);
	Assert(extension->GetAttachFlag(ATTACH_FLAG_POS_CONSTRAINT));
	Assert(extension->GetAttachState()==ATTACH_STATE_PHYSICAL || extension->GetAttachState()==ATTACH_STATE_RAGDOLL);

	Vector3 vecEntAttachPosLocal = pParentAnchorLocalSpace ? *pParentAnchorLocalSpace : VEC3_ZERO;
	Vector3 vecMyAttachPosLocal = pMyAnchorLocalSpace ? * pMyAnchorLocalSpace : VEC3_ZERO;

	//3. Calculate permitted movement
	float fAllowedSeparation = 0.0f;

	if( bAllowInitialSeparation )
	{
		Vector3 vecEntAttachPosWorld;
		Vector3 vecMyAttachPosWorld;
		
		Matrix34 matMyAttach;
		Matrix34 matOtherAttach;

		if(nMyBone > -1)
		{
			GetGlobalMtx(nMyBone, matMyAttach);
		}
		else
		{
			matMyAttach = MAT34V_TO_MATRIX34(GetMatrix());
		}

		if(nParentBone > -1)
		{
			pPhysical->GetGlobalMtx(nParentBone, matOtherAttach);
		}
		else
		{
			matOtherAttach = MAT34V_TO_MATRIX34(pPhysical->GetMatrix());
		}

		matMyAttach.Transform(vecMyAttachPosLocal, vecMyAttachPosWorld);
		matOtherAttach.Transform(vecEntAttachPosLocal, vecEntAttachPosWorld);

		fAllowedSeparation = vecEntAttachPosWorld.Dist(vecMyAttachPosWorld);
	}

	//5. Do initial warp if requested so that separation is initially 0.0f
	if(extension->GetAttachFlag(ATTACH_FLAG_INITIAL_WARP))
	{
		Matrix34 matNew(Matrix34::IdentityType);

		// First get transform from other pivot point back to entity root
		// Therefore everything here is negated / transposed
		if(pMyAnchorLocalSpace)
		{
			matNew.d -= *pMyAnchorLocalSpace;
		}
		
		Matrix34 matOtherOffset(Matrix34::IdentityType);

		if(pQuatOrientation)
			matOtherOffset.FromQuaternion(*pQuatOrientation);
		if(pParentAnchorLocalSpace)
			matOtherOffset.d = *pParentAnchorLocalSpace;

		matNew.Dot(matOtherOffset);

		Matrix34 matParent;	
		matParent = MAT34V_TO_MATRIX34(pPhysical->GetMatrix());		

		matNew.Dot(matParent);

		SetMatrix(matNew,true,true,true);
	
#if MAKE_ALLOWED_SEPARATION_ZERO_IF_WARPING
		//Hack to make the function work as it used to
		fAllowedSeparation = 0.0f;
#endif
	}

//6. Ensure inst is active before making constraint
	ActivatePhysics();

//7. Work out components from bone indices
	u16 uParentComponent = 0;
	if(pPhysical->GetFragInst() && nParentBone > -1)
	{
		int nParentComponent = pPhysical->GetFragInst()->GetControllingComponentFromBoneIndex(nParentBone);
		if(nParentComponent < 0)
			nParentComponent = 0;
		uParentComponent = (u16)nParentComponent;
	}
	u16 uMyComponent = 0;
	if(GetFragInst() && nMyBone > -1)
	{
		int nMyComponent = GetFragInst()->GetControllingComponentFromBoneIndex(nMyBone);
		if(nMyComponent < 0)
			nMyComponent = 0;
		uMyComponent = (u16)nMyComponent;
		physicsAssertf(uMyComponent < GetFragInst()->GetNumCompositeParts(), "entity=%s, component=%d(max=%d)", GetModelName(), uMyComponent,
			GetFragInst()->GetNumCompositeParts());
	}

//8.  If a constraint already exists between the components, update it, or make a new constraint and store its handle in the tree
	phConstraintBase* pConstraint = extension->FindConstraint(GetCurrentPhysicsInst(),
																pPhysical->GetCurrentPhysicsInst(),
																uMyComponent, uParentComponent);
	if( pConstraint )
	{
		phConstraintAttachment* pConstraintAttach = static_cast<phConstraintAttachment*>( pConstraint );

		pConstraintAttach->SetBreakable(fStrength > 0.0f, fStrength);
		pConstraintAttach->SetLocalPosA( VECTOR3_TO_VEC3V(vecMyAttachPosLocal) );
		pConstraintAttach->SetLocalPosB( VECTOR3_TO_VEC3V(vecEntAttachPosLocal) );
	}
	else //Make a new constraint
	{
		if( physicsVerifyf( extension->GetNumConstraintHandles() < ATTACHMENT_MAX_CONSTRAINTS, "No room in the attachment extension for more constraints between these entities. Max number is %d", ATTACHMENT_MAX_CONSTRAINTS) )
		{
			phConstraintAttachment::Params  params;
											params.instanceA = GetCurrentPhysicsInst();
											params.instanceB = pPhysical->GetCurrentPhysicsInst();
											params.componentA = uMyComponent;
											params.componentB = uParentComponent;
											params.breakable = fStrength > 0.0f;
											params.breakingStrength = fStrength;
											params.constrainRotation = extension->GetAttachFlag(ATTACH_FLAG_ROT_CONSTRAINT);
											params.maxSeparation = fAllowedSeparation;
											params.localPosA = VECTOR3_TO_VEC3V(vecMyAttachPosLocal);
											params.localPosB = VECTOR3_TO_VEC3V(vecEntAttachPosLocal);
                                            params.massInvScaleA = massInvScaleA;
                                            params.massInvScaleB = massInvScaleB;

			phConstraintHandle attachmentHandle = PHCONSTRAINT->Insert(params);
			
			if(physicsVerifyf(attachmentHandle.IsValid(), "Unable to allocate an attachment constraint"))
			{
				extension->AddConstraintHandle(attachmentHandle);				
			}
			else
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}


//9. Notify other code of changes
	if(IsInPathServer())
	{
		CAssistedMovementRouteStore::MaybeRemoveRouteForDoor(this);
		CPathServerGta::MaybeRemoveDynamicObject(this);
	}

	return true;
}

#if __BANK
bool CPhysical::DebugAttachToPhysicalUsingPhysics(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine, CPhysical* pPhysical, s16 nParentBone, s16 nMyBone, u32 nAttachFlags, const Vector3& vecWorldPos, float fStrength)
#else
bool CPhysical::AttachToPhysicalUsingPhysics(CPhysical* pPhysical, s16 nParentBone, s16 nMyBone, u32 nAttachFlags, const Vector3& vecWorldPos, float fStrength)
#endif
{
	//Get matrix
	Matrix34 matMyAttach;
	Matrix34 matOtherAttach;

	if(nMyBone > -1)
	{
		GetGlobalMtx(nMyBone, matMyAttach);
	}
	else
	{
		matMyAttach = MAT34V_TO_MATRIX34(GetMatrix());
	}

	fragInst* pParentFragInst = pPhysical->GetFragInst();
	if (pParentFragInst)
	{
		s32 componentIndex = pParentFragInst->GetControllingComponentFromBoneIndex(nParentBone);
		if (componentIndex !=-1)
		{	
			if (pParentFragInst->GetTypePhysics() && pParentFragInst->GetTypePhysics()->GetAllChildren()[componentIndex])
			{
				u32 boneId = pParentFragInst->GetTypePhysics()->GetAllChildren()[componentIndex]->GetBoneID();
				nParentBone = (s16)pPhysical->GetBoneIndexFromBoneTag((eAnimBoneTag)boneId);
			}
		}
	}


	if(nParentBone > -1)
	{
		pPhysical->GetGlobalMtx(nParentBone, matOtherAttach);
	}
	else
	{
		matOtherAttach = MAT34V_TO_MATRIX34(pPhysical->GetMatrix());
	}

	//Calculate positions in local space and then call the other AttachToPhysicalUsingPhysics
	Vector3 vecEntAttachPosLocal;
	Vector3 vecMyAttachPosLocal;

	matMyAttach.UnTransform(vecWorldPos, vecMyAttachPosLocal);
	matOtherAttach.UnTransform(vecWorldPos, vecEntAttachPosLocal);

#if __BANK
    return DebugAttachToPhysicalUsingPhysics(strCodeFile, strCodeFunction, nCodeLine, pPhysical, nParentBone, nMyBone, nAttachFlags, &vecEntAttachPosLocal, NULL, &vecMyAttachPosLocal, fStrength, /* bAllowInitialSeparation */ false );
#else
	return AttachToPhysicalUsingPhysics(pPhysical, nParentBone, nMyBone, nAttachFlags, &vecEntAttachPosLocal, NULL, &vecMyAttachPosLocal, fStrength, /* bAllowInitialSeparation */ false );
#endif
}

#if __BANK
bool CPhysical::DebugAttachToWorldUsingPhysics(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine, s16 nMyBone, bool bRotConstraint, const Vector3& vecWorldPos, const Vector3* pVecMyOffset, float fStrength, bool bAutoActivateOnDetach, const Vector3* ASSERT_ONLY(vecRotMinLimits), const Vector3* ASSERT_ONLY(vecRotMaxLimits))
#else
bool CPhysical::AttachToWorldUsingPhysics(s16 nMyBone, bool bRotConstraint, const Vector3& vecWorldPos, const Vector3* pVecMyOffset, float fStrength, bool bAutoActivateOnDetach, const Vector3* ASSERT_ONLY(vecRotMinLimits), const Vector3* ASSERT_ONLY(vecRotMaxLimits))
#endif
{
	fwAttachmentEntityExtension *extension = CreateAttachmentExtensionIfMissing();

//1. Check preconditions
	if(extension->GetAttachParentForced() != NULL) return false;	//There is already an attach parent that isn't the world (detach from it before calling this)
	if(!GetCurrentPhysicsInst()) return false;						//Can't create a physical constraint without an inst. Abort

	physicsAssertf((vecRotMinLimits == NULL) || (vecRotMinLimits->GetX() == 0.0f && vecRotMinLimits->GetY() == 0.0f && vecRotMinLimits->GetZ() == 0.0f), "attachment to world can no longer specify rotational limits");
	physicsAssertf((vecRotMaxLimits == NULL) || (vecRotMaxLimits->GetX() == 0.0f && vecRotMaxLimits->GetY() == 0.0f && vecRotMaxLimits->GetZ() == 0.0f), "attachment to world can no longer specify rotational limits");

//2. Create node and edge between it and a "NULL node" in the tree (if necessary)
	u32 nAttachFlags = ATTACH_STATE_WORLD_PHYSICAL | ATTACH_FLAG_POS_CONSTRAINT | (bRotConstraint? ATTACH_FLAG_ROT_CONSTRAINT : 0) | (bAutoActivateOnDetach? ATTACH_FLAG_ACTIVATE_ON_DETACH : 0);
	extension->AttachToEntity(this, NULL, nMyBone, nAttachFlags, &vecWorldPos, NULL);

#if __BANK
	extension->DebugSetInvocationData(strCodeFile, strCodeFunction, nCodeLine);
#endif
	
	nMyBone = GetSkeleton() && Verifyf(nMyBone < GetSkeleton()->GetBoneCount(), "Bone index [%d] is not valid for skeleton [%s], range [0..%d]", nMyBone, GetModelName(), GetSkeleton()->GetBoneCount()) 
				? nMyBone : -1;
	
//3. Activate so that constraint is certainly enforced
	GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, false);
	ActivatePhysics();

//4. Calculate component from bone
	u16 uMyComponent = 0;
	if(GetFragInst() && nMyBone > -1)
	{
		int nMyComponent = GetFragInst()->GetComponentFromBoneIndex(nMyBone);
		if(nMyComponent < 0)
			nMyComponent = 0;
		uMyComponent = (u16)nMyComponent;
	}

	Vector3 vecMyAttachPosLocal;
	
	if( pVecMyOffset )
	{
		vecMyAttachPosLocal = *pVecMyOffset;
	}
	else
	{
		Matrix34 matMyAttach;

		if(nMyBone > -1)
		{
			GetGlobalMtx(nMyBone, matMyAttach);
		}
		else
		{
			matMyAttach = MAT34V_TO_MATRIX34(GetMatrix());
		}

		matMyAttach.UnTransform(vecWorldPos, vecMyAttachPosLocal);
	}

//5.  If a constraint already exists between the components, replace it, or make a new constraint and store its handle in the tree
	phConstraintAttachment* pConstraint = static_cast<phConstraintAttachment*>( extension->FindConstraint(GetCurrentPhysicsInst(),
																							NULL, uMyComponent, 0) );
	if( pConstraint )
	{
		if(pVecMyOffset)
		{
			pConstraint->SetLocalPosA(VECTOR3_TO_VEC3V(*pVecMyOffset));
		}
		else
		{
			pConstraint->SetWorldPosA(RCC_VEC3V(vecWorldPos));
		}

		pConstraint->SetWorldPosB(RCC_VEC3V(vecWorldPos));
	}
	else //Make a new constraint
	{
		if( physicsVerifyf( extension->GetNumConstraintHandles() < ATTACHMENT_MAX_CONSTRAINTS, "No room in the attachment extension for more constraints between these entities. Max number is %d", ATTACHMENT_MAX_CONSTRAINTS) )
		{
			phConstraintAttachment::Params  params;
											params.instanceA = GetCurrentPhysicsInst();
											params.componentA = uMyComponent;
											params.breakable = fStrength > 0.0f;
											params.breakingStrength = fStrength;
											params.constrainRotation = bRotConstraint;
											params.maxSeparation = 0.0f;
											params.localPosA = RCC_VEC3V(vecMyAttachPosLocal);
											params.localPosB = RCC_VEC3V(vecWorldPos);

			phConstraintHandle attachmentHandle = PHCONSTRAINT->Insert(params);
			
			if(physicsVerifyf(attachmentHandle.IsValid(), "Unable to allocate an attachment constraint"))
			{
				extension->AddConstraintHandle(attachmentHandle);
			}
			else
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	return true;
}

bool CPhysical::GetGlobalAttachMatrix(Mat34V_Ref attachMatInOut) const
{
	fwEntity* pParentEntity = GetAttachParent();
	if (pParentEntity)
	{
		const s16 iAttachBone = GetAttachBone();
		if (iAttachBone > -1)
		{
			Matrix34 tempMtx(Matrix34::IdentityType);
			pParentEntity->GetGlobalMtx(iAttachBone, tempMtx);
			attachMatInOut = MATRIX34_TO_MAT34V(tempMtx);
		}
		else
		{
			attachMatInOut = pParentEntity->GetTransform().GetMatrix();
		}
		return true;
	}

	return false;
}

void CPhysical::AttachParentPartBrokeOff(CPhysical* pNewParent)
{
	fwAttachmentEntityExtension *extension = CreateAttachmentExtensionIfMissing();

	// If this physical object is attached to a part which broke off, do something about it now.
	physicsAssert(pNewParent);
	physicsAssert(extension->GetAttachParentForced());
	
	// For now just switch our attach parent to the new entity. Could use flags to decide how
	// to respond, or make this function virtual and override it in a derived class. Start by storing
	// properties of current attachment so we can duplicate them on the new attachment.
	s16 nEntBone = extension->GetOtherAttachBone();
	u32 nAttachFlags = extension->GetAttachFlags();
	Vector3 vAttachOffset = extension->GetAttachOffset();
	Quaternion attachQuat = extension->GetAttachQuat();

	DetachFromParent(0);
	AttachToPhysicalBasic(pNewParent, nEntBone, nAttachFlags, &vAttachOffset, &attachQuat);
}

void CPhysical::DetachFromParentAndChildren(u16 nDetachFlags)
{
	fwAttachmentEntityExtension *extension = GetAttachmentExtension();

	if (!extension)
	{
		return;
	}

	CPhysical * child = static_cast<CPhysical*>( extension->GetChildAttachment() );

	//This may delete the attachment
	DetachFromParent(nDetachFlags);

	while(child)
	{
		fwAttachmentEntityExtension &childAttachExt = child->GetAttachmentExtensionRef();

		//Some tree edges may go into the detaching state whilst others will be completely removed from the tree
		CPhysical* sibling = static_cast<CPhysical*>( childAttachExt.GetSiblingAttachment() );
		child->DetachFromParent(nDetachFlags);
		child = sibling;
	}
}

void CPhysical::DetachFromParent(u16 nDetachFlags)
{
	if(GetType() != ENTITY_TYPE_VEHICLE)
	{
		// Vehicles can be destroyed from threads other than the update thread, and fwAltSkeletonExtension::RemoveExtension
		// is only safe to call from the update thread. However, vehicles don't have an fwAltSkeletonExtension anyway...
		fwAltSkeletonExtension::RemoveExtension(*this);
	}

	fwAttachmentEntityExtension *extension = GetAttachmentExtension();
	if(!extension) return;

	if( extension->GetAttachFlag( ATTACH_FLAG_NOT_NEEDED_ON_DETACH ) )
	{
		int EntityIndex;
		entity_commands::SetEntityAsNoLongerNeeded(this, EntityIndex);
	}

	Assertf(!extension->GetIsAttachedToGround() || !(nDetachFlags & DETACH_FLAG_NO_COLLISION_UNTIL_CLEAR), "Detaching ped from its ground physical and disabling collision between them is probably not what you want.");

#if __BANK
	if( extension->GetAttachParent() &&
		extension->GetAttachParent()->GetType() == ENTITY_TYPE_VEHICLE )
	{
		CVehicle* vehicle = static_cast< CVehicle* >( extension->GetAttachParent() );
		if( vehicle && 
			vehicle->GetIsHeli() )
		{
			CHeli* heli = static_cast< CHeli* >( vehicle );
			if( heli->GetIsCargobob() )
			{
				vehicleDisplayf("[PICKUP ROPE DEBUG] CPhysical::DetachFromParent - Detaching %s entity attached to parent Vehicle: %s", GetModelName(), heli->GetNetworkObject() ? heli->GetNetworkObject()->GetLogName() : heli->GetModelName() );
				sysStack::PrintStackTrace();
			}
		}
	}
#endif // #if __BANK

	if( nDetachFlags & DETACH_FLAG_DONT_REMOVE_BASIC_ATTACHMENTS )
	{
		if(extension->IsAttachStateBasicDerived()) 
			return;
	}

	physicsAssertf(!extension->GetAttachFlag(ATTACH_FLAG_IN_DETACH_FUNCTION), "DetachFromParent called recursively");
	extension->SetAttachFlag(ATTACH_FLAG_IN_DETACH_FUNCTION, true);

	if(!IsProtectedBaseFlagSet(fwEntity::USES_COLLISION) REPLAY_ONLY(&& GetOwnedBy() != ENTITY_OWNEDBY_REPLAY))
	{
		EnableCollision();
	}
	SetForceAddToBoundRadius(0.0f);

// 	if(m_pAttachParent && m_pAttachParent->GetIsTypePed())
// 	{
// 		((CPed*)m_pAttachParent.Get())->RemoveEntityFromAttachedList(this);
// 	}

	if(GetCurrentPhysicsInst())
	{
		GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, false);

		if(GetCurrentPhysicsInst()->IsInLevel())
		{
			// Reset the archetype flags back to model info default
			CBaseModelInfo* pModelInfo = GetBaseModelInfo();
			Assert(pModelInfo);

			u32 uOrigIncludeFlags = 0;

			fragInst* pFragInst = GetFragInst();		
			if(GetCurrentPhysicsInst() == pFragInst)
			{
				// Deal with frag, get original include flags from type
				Assert(pFragInst);
				Assert(pFragInst->GetTypePhysics());
				Assert(pFragInst->GetTypePhysics()->GetArchetype());
				uOrigIncludeFlags = pFragInst->GetTypePhysics()->GetArchetype()->GetIncludeFlags();
			}
			else if(Verifyf(pModelInfo->GetPhysics(), "Model '%s' has no physics archetype", pModelInfo->GetModelName()))
			{
				uOrigIncludeFlags = pModelInfo->GetPhysics()->GetIncludeFlags();
			}

			CPhysics::GetLevel()->SetInstanceIncludeFlags(GetCurrentPhysicsInst()->GetLevelIndex(), uOrigIncludeFlags);
		}
	}

	bool const bEnvCloth = ( GetCurrentPhysicsInst() && GetCurrentPhysicsInst()->GetArchetype() && GetCurrentPhysicsInst()->GetArchetype()->GetTypeFlags() == ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE );
	bool const bActivateFlagSet = ( (nDetachFlags & DETACH_FLAG_ACTIVATE_PHYSICS) || extension->GetAttachFlag(ATTACH_FLAG_ACTIVATE_ON_DETACH) ) && !bEnvCloth;
	
	if(bActivateFlagSet)
	{
		if(GetCurrentPhysicsInst() && GetCurrentPhysicsInst()->IsInLevel())
		{
			ActivatePhysics();
		}
#if __ASSERT
		else
		{
			physicsDisplayf("Can't activate %s on detach. phInst=%p", GetModelName(), GetCurrentPhysicsInst());
		}
#endif // __ASSERT
	}

	CPhysical* pAttachParent = (CPhysical*)extension->GetAttachParentForced();

#if __BANK
	if( pAttachParent &&
		pAttachParent->GetIsTypeVehicle() &&
		!GetIsTypePed() &&
		!( GetIsTypeObject() && static_cast<CObject*>(this)->GetAsProjectile() ) ) 
	{
		Displayf("[CPhysical::DetachFromParent] [Object: %s is detaching from Vehicle: %s]", GetModelName(), pAttachParent->GetModelName() );
		size_t	m_lastSetFlagsCallstack[8];
		sysStack::CaptureStackTrace(m_lastSetFlagsCallstack, 8, 1);
		sysStack::PrintCapturedStackTrace(m_lastSetFlagsCallstack, 8);
	}
#endif // #if __BANK


	if(extension->GetAttachState() == ATTACH_STATE_WORLD_PHYSICAL || extension->GetAttachState() == ATTACH_STATE_PHYSICAL || extension->GetAttachState() == ATTACH_STATE_RAGDOLL)
	{
		extension->RemoveAttachmentConstraints();
	}
	else if(pAttachParent && (nDetachFlags &DETACH_FLAG_APPLY_VELOCITY) && (nDetachFlags &DETACH_FLAG_ACTIVATE_PHYSICS))
	{
		bool bVelocitiesDone = false;

		if(nDetachFlags & DETACH_FLAG_USE_ROOT_PARENT_VELOCITY)
		{
			fwAttachmentEntityExtension *pRootextension = pAttachParent->GetAttachmentExtension();
			if(pRootextension)
			{
				CPhysical* pAttachRoot = (CPhysical*)pRootextension->GetAttachParentForced();
				if(pAttachRoot)
				{
					SetVelocity(pAttachRoot->GetVelocity());
					bVelocitiesDone = true;
				}
			}
		}

		if(!bVelocitiesDone && pAttachParent->GetIsTypePed() && extension->GetOtherAttachBone() > -1 && pAttachParent->GetFragInst())
		{
			// if we were attached to a ped bone, try to use the ragdoll current and last matrices to calculate velocity
			CPed* pAttachedToPed = static_cast<CPed*>(pAttachParent);
			Matrix34 matCurrent, matPrevious;
			
			// if((pAttachedToPed->GetFragInst()->GetPosition() - pAttachedToPed->GetFragInst()->GetLastMatrix().d).Mag2() < 1.0f
			if(IsLessThanAll(MagSquared(pAttachedToPed->GetFragInst()->GetMatrix().GetCol3() - PHSIM->GetLastInstanceMatrix(pAttachedToPed->GetFragInst()).GetCol3()), ScalarV(V_ONE))
			&& pAttachedToPed->GetFragInst()->GetBoneMatrix(matPrevious, extension->GetOtherAttachBone(), true)
			&& pAttachedToPed->GetFragInst()->GetBoneMatrix(matCurrent, extension->GetOtherAttachBone(), false))
			{
				Matrix34 matOffset;
				matOffset.FromQuaternion(extension->GetAttachQuat());
				matOffset.d = extension->GetAttachOffset();

				matPrevious.Dot(matOffset);
				matPrevious.Dot(RCC_MATRIX34(PHSIM->GetLastInstanceMatrix(pAttachedToPed->GetFragInst())));

				matCurrent.Dot(matOffset);
				matCurrent.Dot(RCC_MATRIX34(pAttachedToPed->GetFragInst()->GetMatrix()));

				Vector3 vecMoveSpeed = (matCurrent.d - matPrevious.d) * fwTimer::GetInvTimeStep();
				if(vecMoveSpeed.Mag2() > 30.0f*30.0f)
				{
					vecMoveSpeed.NormalizeFast();
					if(NetworkUtils::IsNetworkClone((CEntity *) extension->GetAttachParentForced()))
						vecMoveSpeed.Scale(5.0f);
					else
						vecMoveSpeed.Scale(30.0f);
				}

				SetVelocity(vecMoveSpeed);

				Matrix34 lastToCurrent;
				lastToCurrent.DotTranspose(matCurrent, matPrevious);
				Quaternion tempQuat;
				lastToCurrent.ToQuaternion(tempQuat);
				Vector3 angularDelta;
				float angle;
				tempQuat.ToRotation(angularDelta, angle);
				angularDelta.Scale(angle);

				Vector3 vecTurnSpeed = angularDelta * fwTimer::GetInvTimeStep();
				if(vecTurnSpeed.Mag2() > 10.0f*10.0f)
				{
					vecTurnSpeed.NormalizeFast();
					if(NetworkUtils::IsNetworkClone((CEntity *) GetAttachParentForced()))
						vecMoveSpeed.Scale(5.0f);
					else
						vecMoveSpeed.Scale(10.0f);
				}

				SetAngVelocity(vecTurnSpeed);
				bVelocitiesDone = true;
			}
		}

		if(pAttachParent->GetIsPhysical() && pAttachParent->GetCurrentPhysicsInst() && !bVelocitiesDone)
		{
			CPhysical *pAttachedToPhysical = static_cast<CPhysical*>(pAttachParent);
			SetVelocity(pAttachedToPhysical->GetLocalSpeed(VEC3V_TO_VECTOR3(GetTransform().GetPosition()), true));
			SetAngVelocity(pAttachedToPhysical->GetAngVelocity());
		}
	}

	// want to test this bound against the thing we were attached to
	// and also test against the world to see if we're going to fall through

	// set attach type to DETACHING so it doesn't collide with the thing it was attached to
	Matrix34 m = MAT34V_TO_MATRIX34(GetMatrix());

	bool bBoundTestHitSomething = false;
	// No point in doing a test against individual objects if we don't have any to test against...
	if(pAttachParent && (nDetachFlags&DETACH_FLAG_NO_COLLISION_UNTIL_CLEAR) && pAttachParent->GetCurrentPhysicsInst() && pAttachParent->GetCurrentPhysicsInst()->IsInLevel())
	{
		WorldProbe::CShapeTestBoundDesc boundTestDesc;
		boundTestDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
		boundTestDesc.SetBoundFromEntity(this);
		boundTestDesc.SetIncludeEntity(pAttachParent);
		boundTestDesc.SetTransform(&m);

		bBoundTestHitSomething = WorldProbe::GetShapeTestManager()->SubmitTest(boundTestDesc);
	}

	extension->SetAttachFlag(ATTACH_FLAG_IN_DETACH_FUNCTION, false);

	if(pAttachParent && (nDetachFlags&DETACH_FLAG_NO_COLLISION_UNTIL_CLEAR) && bBoundTestHitSomething)
	{
		extension->SetAttachFlags(ATTACH_STATE_DETACHING); //State in which ShouldFindImpacts will return false between the pair
	}
	else
	{
		extension->ClearParentAttachmentVars();
	}

	if(!IsInPathServer())
	{
		if(CPathServerGta::MaybeAddDynamicObject(this))
			CAssistedMovementRouteStore::MaybeAddRouteForDoor(this);
	}

	// allow child object to clean up 
	if (GetIsTypeObject())
	{
		// B* 1717278 - this is pretty horrible. when we clean up a vehicle with objects attached, and detach the child objects
		// we want to allow the objects to be potentially cleaned up even if the script has said otherwise.
		//
		// A better fix in the future would be to expose the required behaviours (ie all the attach / detach flags) to script.
		CObject* pObject = static_cast<CObject*>(this);
		if (pObject && !pObject->GetIsLeakedObject() && pObject->m_nObjectFlags.bOnlyCleanUpWhenOutOfRange)
		{
			pObject->m_nObjectFlags.bOnlyCleanUpWhenOutOfRange = false;
		}	
	}

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord() && (nDetachFlags & DETACH_FLAG_IN_PHYSICAL_DESTRUCTOR) == 0 && this->GetReplayID() != ReplayIDInvalid && pAttachParent && pAttachParent->GetReplayID() != ReplayIDInvalid)
	{
		CReplayMgr::RecordEntityDetachment(GetReplayID(), pAttachParent->GetReplayID());
	}
#endif // GTA_REPLAY

#if __BANK
	if(NetworkInterface::IsGameInProgress())
	{
		if(pAttachParent && pAttachParent->GetIsTypeVehicle() && pAttachParent->PopTypeIsMission() && this->GetIsTypeObject() && this->PopTypeIsMission())
		{
			physicsDisplayf("[ATTACH DEBUG] Detaching %s from %s.", GetDebugNameFromObjectID(), pAttachParent->GetDebugNameFromObjectID());
		}
	}
#endif
}

#if __BANK
void CPhysical::DebugProcessAttachment(const char * ATTACHMENT_ENABLE_UPDATE_RECORDS_PARAM(strCodeFile), 
										  const char* ATTACHMENT_ENABLE_UPDATE_RECORDS_PARAM(strCodeFunction), 
										  u32 ATTACHMENT_ENABLE_UPDATE_RECORDS_PARAM(nCodeLine) )
#else
void CPhysical::ProcessAttachment()
#endif
{
	fwAttachmentEntityExtension* extension = GetAttachmentExtension();
	if(!extension)
		return;

	extension->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, true);
#if __BANK
#if ATTACHMENT_ENABLE_UPDATE_RECORDS
	DebugProcessAttachmentImp(strCodeFile, strCodeFunction, nCodeLine);
#else // ATTACHMENT_ENABLE_UPDATE_RECORDS
	DebugProcessAttachmentImp("", "", 0);
#endif // ATTACHMENT_ENABLE_UPDATE_RECORDS
#else // __BANK
	ProcessAttachmentImp();
#endif // __BANK

	// verify that extension was deleted inside process attachement
	if(extension == GetAttachmentExtension())
	{
		extension->SetAttachFlag(ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE, false);
	}
}

#if __BANK
void CPhysical::DebugProcessAttachmentImp(const char * ATTACHMENT_ENABLE_UPDATE_RECORDS_PARAM(strCodeFile), 
									   const char* ATTACHMENT_ENABLE_UPDATE_RECORDS_PARAM(strCodeFunction), 
									   u32 ATTACHMENT_ENABLE_UPDATE_RECORDS_PARAM(nCodeLine) )
#else
void CPhysical::ProcessAttachmentImp()
#endif
{
	fwAttachmentEntityExtension* extension = GetAttachmentExtension();

	if(!extension)
	{
		return;
	}

	CPhysical* pAttachParent = (CPhysical*)extension->GetAttachParentForced();

#if __DEV && !__SPU
	// JB: 22/8/07 added this assert in attempt to catch crash where attached entity's drawable has a bad skeleton-data ptr
	if(pAttachParent)
	{
		CBaseModelInfo * pModelInfo = GetBaseModelInfo();
		rmcDrawable * pDrawable = pModelInfo->GetDrawable();
		if(pDrawable && m_pDrawHandler)
		{
			Assertf(pDrawable->GetSkeletonData() == m_pDrawHandler->GetDrawable()->GetSkeletonData(), "Mismatch between entity's m_SkeletonData and modelinfo's m_SkeletonData.");
		}
	}
#endif

	if(extension->IsAttachStateBasicDerived())
	{
		physicsAssertf(pAttachParent, "A basic attachment has lost its parent");

		if(GetCurrentPhysicsInst() && GetCurrentPhysicsInst()->IsInLevel())
		{
			if( !extension->GetAttachFlag(ATTACH_FLAG_COL_ON) )
			{
				bool bNeedToSetIncludeFlags = extension->GetAttachFlag(ATTACH_FLAG_NO_PHYSICS_INST_YET)
					|| extension->GetAttachFlag(ATTACH_FLAG_NOT_IN_LEVEL_INITIALLY);
				u32 nLevelIndex = GetCurrentPhysicsInst()->GetLevelIndex();
				u32 nInstIncludeFlags = CPhysics::GetLevel()->GetInstanceIncludeFlags(nLevelIndex);
				if(bNeedToSetIncludeFlags ||
					!Verifyf(nInstIncludeFlags == u32(ArchetypeFlags::GTA_BASIC_ATTACHMENT_INCLUDE_TYPES)
						|| 	nInstIncludeFlags == u32(ArchetypeFlags::GTA_BASIC_ATTACHMENT_AND_CAMERA_INCLUDE_TYPES)
						|| nInstIncludeFlags == u32(ArchetypeFlags::GTA_CAMERA_TEST)
						|| nInstIncludeFlags==0u,
						"Include flags != GTA_BASIC_ATTACHMENT_INCLUDE_TYPES or 0: 0x%X for %s. NetClone?%s. Attach fn:%s",
						CPhysics::GetLevel()->GetInstanceIncludeFlags(nLevelIndex), GetDebugName(),
						IsNetworkClone()?"yes":"no", extension->DebugGetInvokingFunction())
					)
				{
					if(NetworkInterface::IsGameInProgress() && GetNetworkObject())
					{
						CNetObjEntity* netEnt = SafeCast(CNetObjEntity, GetNetworkObject());
						if(!netEnt || !netEnt->GetDisableCollisionCompletely())
						{
							CPhysics::GetLevel()->SetInstanceIncludeFlags(nLevelIndex, u32(ArchetypeFlags::GTA_BASIC_ATTACHMENT_INCLUDE_TYPES));
						}
					}
					else
					{
						CPhysics::GetLevel()->SetInstanceIncludeFlags(nLevelIndex, u32(ArchetypeFlags::GTA_BASIC_ATTACHMENT_INCLUDE_TYPES));
					}
					// Reset these flags no that the physics has loaded and been added to the level.
					extension->SetAttachFlag(ATTACH_FLAG_NO_PHYSICS_INST_YET, false);
					extension->SetAttachFlag(ATTACH_FLAG_NOT_IN_LEVEL_INITIALLY, !GetCurrentPhysicsInst()->IsInLevel());
				}
			}

			//Let the vehicle attachments override the activation flags
			if(extension->GetAttachState() == ATTACH_STATE_BASIC)
			{
				if(!AssertVerify(GetCurrentPhysicsInst()->GetInstFlag(phInst::FLAG_NEVER_ACTIVATE) != 0u))
				{
					GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE,true);
				}

				if(!Verifyf(!CPhysics::GetLevel()->IsActive(GetCurrentPhysicsInst()->GetLevelIndex()), "Model(%s)", GetModelName()))
				{
					CPhysics::GetSimulator()->DeactivateObject(GetCurrentPhysicsInst());
				}
			}
		}

		// If the entity is attached to a ped and has the auto detach flag set 
		if (pAttachParent->GetIsTypePed() && ((CPed*)pAttachParent)->GetUsingRagdoll() && extension->GetAttachFlag(ATTACH_FLAG_AUTODETACH_ON_RAGDOLL))
		{
			DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
			return;
		}

		Matrix34 matOffset;
		matOffset.Identity();

		matOffset.FromQuaternion(extension->GetAttachQuat());
		matOffset.d = extension->GetAttachOffset();
		// Check for NaNs in the position vector.
		Assert(extension->GetAttachOffset().IsEqual(extension->GetAttachOffset()));

		Matrix34 matParent = MAT34V_TO_MATRIX34(pAttachParent->GetMatrix());
		Assertf(matParent.IsOrthonormal(), "Parent entity matrix found to be non-orthonormal (%5.3f).", matParent.MeasureNonOrthonormality());
		ASSERT_ONLY(bool bMatParentFromSkel = false;)

#if ENABLE_FRAG_OPTIMIZATION
		if(extension->GetOtherAttachBone() > -1 && pAttachParent->GetIsDynamic() && extension->GetOtherAttachBone() < (static_cast<CPhysical*>(pAttachParent))->GetBoneCount())
#else
		if(extension->GetOtherAttachBone() > -1 && pAttachParent->GetIsDynamic() && (static_cast<CPhysical*>(pAttachParent))->GetSkeleton())
#endif			
		{
			const CDynamicEntity* pDynEnt = static_cast<CPhysical*>(pAttachParent);

			const fwTransform* pTransform = pAttachParent->GetTransformPtr();
			if(pTransform->IsMatrixScaledTransform())
			{
				// find out the attaching position using scale and local matrix.
				// GetGlobalMtx() doesn't return correct position if the parent is using a scaled transform.
			
				// B*1853481: Replicating some logic from CVfxHelper::GetMatrixFromBoneIndex to ensure we get a valid matrix even for damaged frag objects.
				// CVfxHelper::GetMatrixFromBoneIndex provides a global matrix, but we need an object matrix here.
				const Vector3 vParentPos = matParent.d;
				
#if ENABLE_FRAG_OPTIMIZATION
				Matrix34 matLocal(Matrix34::IdentityType);
				int iBoneIndex = extension->GetOtherAttachBone();
				if (aiVerifyf(pDynEnt->GetBoneCount() > 0 && (u32)iBoneIndex < pDynEnt->GetBoneCount(), "Entity %s doesn't have a skeleton or bone index out of range %u(%u)", pDynEnt->GetModelName(), iBoneIndex, pDynEnt->GetBoneCount()))
				{
					pDynEnt->ComputeObjectMtx(iBoneIndex, RC_MAT34V(matLocal));
				}
#else
				Matrix34 matLocal = pDynEnt->GetObjectMtx(extension->GetOtherAttachBone());
#endif

				if(pDynEnt->GetFragInst() && pDynEnt->GetFragInst()->GetCacheEntry() && pDynEnt->GetFragInst()->GetCacheEntry()->GetHierInst())
				{
					const crSkeleton *const pDamagedSkel =  pDynEnt->GetFragInst()->GetCacheEntry()->GetHierInst()->damagedSkeleton;				
					if(matLocal.a.IsZero() && pDamagedSkel)
					{
						Mat34V boneMatDmg;
						boneMatDmg = pDamagedSkel->GetObjectMtx(extension->GetOtherAttachBone());
						matLocal.Set(MAT34V_TO_MATRIX34(boneMatDmg));
					}
				}
				
				// If we got back a zeroed out object matrix, but it has a valid global matrix, use the global matrix to calculate a valid object matrix.
				// This will happen for vehicles with modded parts.
				if(matLocal.a.IsZero())
				{
					Matrix34 matGlobal;
					pDynEnt->GetGlobalMtx(extension->GetOtherAttachBone(), matGlobal);

					if(!matGlobal.a.IsZero())
					{
						Matrix34 entMatrix = MAT34V_TO_MATRIX34(pDynEnt->GetMatrix());
						entMatrix.Inverse();

						matGlobal.Dot(entMatrix);
						matLocal.Set(matGlobal);
					}
				}				

				Matrix34 tempMat = matParent;
				tempMat.Dot(matLocal, matParent);
				matParent = tempMat;
				Assertf(matParent.IsOrthonormal(), "[1] Parent entity matrix found to be non-orthonormal (%5.3f).", matParent.MeasureNonOrthonormality());

				Vector3 vScaledOffset = matParent.d - vParentPos;
				float fScaleXY = ((fwMatrixScaledTransform*)pTransform)->GetScaleXY();
				float fScaleZ = ((fwMatrixScaledTransform*)pTransform)->GetScaleZ();
				vScaledOffset.x *= fScaleXY;
				vScaledOffset.y *= fScaleXY;
				vScaledOffset.z *= fScaleZ;
				matParent.d = vParentPos + vScaledOffset;
				Assertf(matParent.IsOrthonormal(), "[2] Parent entity matrix found to be non-orthonormal (%5.3f).", matParent.MeasureNonOrthonormality());

				if( extension->GetMyAttachBone() != -1 )
				{
					Matrix34 matMyBone = GetObjectMtx( extension->GetMyAttachBone() );
					matMyBone.Inverse3x3();
					//matOffset.Transform3x3( matMyBone.d );

					pDynEnt->GetGlobalMtx( extension->GetOtherAttachBone(), matParent );

					// the object rotation needs to be the rotation of the parent bone, in world space, minus the rotation of the child bone in local space
					// the position offset needs to be the bone offset, on the child, rotated by the parent object rotation
					matOffset.Dot3x3( matMyBone );
					matOffset.Dot3x3( matParent );

					matMyBone.d = -matMyBone.d;
					matOffset.Transform3x3( matMyBone.d );
					matOffset.d = matMyBone.d;
				}
			}
			else
			{
				pDynEnt->GetGlobalMtx(extension->GetOtherAttachBone(), matParent);
				Assertf(matParent.IsOrthonormal(), "[3] Parent entity %s matrix found to be non-orthonormal (%5.3f).", pAttachParent->GetModelName(), matParent.MeasureNonOrthonormality());

				// B*1853481 & 1880214: Ensure we get a valid matrix even for damaged frag objects.
				// If the standard GetGlobalMtx call gives a zeroed matrix, try grabbing the matrix from the damaged skeleton.
				// Was previously using CVfxHelper::GetMatrixFromBoneIndex, but that doesn't use the entity GetGlobalMtx method; it accesses the skeleton directly.
				// This caused a problem as CVehicle implements a special version of that method to handle cases where mod kits are in use. For some reason modded parts have zeroed out bone matrices in the frag inst.
				if(matParent.a.IsZero())
				{			
					if(pDynEnt->GetFragInst() && pDynEnt->GetFragInst()->GetCacheEntry() && pDynEnt->GetFragInst()->GetCacheEntry()->GetHierInst() 
						&& pDynEnt->GetFragInst()->GetCacheEntry()->GetHierInst()->damagedSkeleton)
					{
						Mat34V matParentV;
						pDynEnt->GetFragInst()->GetCacheEntry()->GetHierInst()->damagedSkeleton->GetGlobalMtx(extension->GetOtherAttachBone(), matParentV);
						matParent.Set(MAT34V_TO_MATRIX34(matParentV));
						Assertf(matParent.IsOrthonormal(), "[4] Parent entity %s matrix found to be non-orthonormal (%5.3f).", pAttachParent->GetModelName(), matParent.MeasureNonOrthonormality());
					}
				}
			}

			ASSERT_ONLY(bMatParentFromSkel = true;)
		}

		// Check to see if the physical parent disappeared from underneath us
		if( matParent.a.IsZero() )
		{
			DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS);
			return;
		}

		Matrix34 matNew;
		if(extension->GetAttachFlag(ATTACH_FLAG_OFFSET_IS_ABSOLUTE))
		{
			matNew = matOffset;
			matNew.d = matParent.d + matOffset.d;
		}
		else
		{
			matNew.Dot(matOffset, matParent);
		}

#if DR_ENABLED
		if (smb_RecordAttachments && sm_RecordAttachmentsFunc)
		{
			sm_RecordAttachmentsFunc(GetCurrentPhysicsInst(), pAttachParent->GetCurrentPhysicsInst(), matNew);
		}
#endif

#if __ASSERT || ATTACHMENT_ENABLE_UPDATE_RECORDS

		float nonOrthonormalityParent = matParent.MeasureNonOrthonormality();
		float nonOrthonormalityOffset = matOffset.MeasureNonOrthonormality();
		float nonOrthonormalityMatNew = matNew.MeasureNonOrthonormality();

#if __ASSERT
		bool bMatNewOrthonormal = matNew.IsOrthonormal();
		if(!bMatNewOrthonormal)
		{
			//Print information about the non-orthonormality
			Assertf(bMatNewOrthonormal, "ProcessAttachment.ATTACH_STATE_BASIC calculated matNew, which wasn't orthonormal: "
					"\n a(%.4f, %.4f, %.4f) b %.4f, %.4f, %.4f) c(%.4f, %.4f, %.4f) d(%.4f, %.4f, %.4f)\n"
					"Parent.NonOrth: %.4f Offset.NonOrth: %.4f New.NonOrth: %.4f\n"
					"Did matParent come from the skeleton?%s",
						matNew.a.x, matNew.a.y, matNew.a.z,
						matNew.b.x, matNew.b.y, matNew.b.z,
						matNew.c.x, matNew.c.y, matNew.c.z,
						matNew.d.x, matNew.d.y, matNew.d.z,
						nonOrthonormalityParent, nonOrthonormalityOffset, nonOrthonormalityMatNew,
						bMatParentFromSkel?"yes":"no");
			Assertf(bMatNewOrthonormal, "Parent: %s, Child: %s", CModelInfo::GetBaseModelInfoName(pAttachParent->GetModelId()), CModelInfo::GetBaseModelInfoName(GetModelId()));
		}
		// Check for NaNs in the new matrix.
		if(matNew.IsNotEqual(matNew))
		{
			Assertf(false, "ProcessAttachment::ATTACH_STATE_BASIC calculated matNew, which contains non-numbers:"
				"\n a(%.4f, %.4f, %.4f) b %.4f, %.4f, %.4f) c(%.4f, %.4f, %.4f) d(%.4f, %.4f, %.4f)\n"
				"Did matParent come from the skeleton?%s",
				matNew.a.x, matNew.a.y, matNew.a.z,
				matNew.b.x, matNew.b.y, matNew.b.z,
				matNew.c.x, matNew.c.y, matNew.c.z,
				matNew.d.x, matNew.d.y, matNew.d.z,
				bMatParentFromSkel?"yes":"no");
		}
#endif

#if ATTACHMENT_ENABLE_UPDATE_RECORDS
		float nonOrthonormalityMatCurrent = MAT34V_TO_MATRIX34( GetMatrix() ).MeasureNonOrthonormality();
		extension->AddUpdateRecord(strCodeFile, strCodeFunction, nCodeLine, nonOrthonormalityParent, nonOrthonormalityOffset, nonOrthonormalityMatCurrent, nonOrthonormalityMatNew);
#endif

#endif

#if GTA_REPLAY
		if(!CReplayMgr::IsReplayInControlOfWorld() || GetOwnedBy()!=ENTITY_OWNEDBY_REPLAY)
#endif // GTA_REPLAY
		{
			SetMatrix(matNew);
		}

		//Rage cloth begin. 
		//Is the physical a rage env cloth instance?
		//If the physical is a rage env cloth then we'll need to update the pinned verts with the latest 
		//transform.
		phInst* pInst = GetCurrentPhysicsInst();
		if(pInst && pInst->IsInLevel() && CPhysics::GetLevel()->GetInstanceTypeFlags(pInst->GetLevelIndex())==ArchetypeFlags::GTA_ENVCLOTH_OBJECT_TYPE)
		{
			//Yup, the physical is a rage cloth. 
			Assertf(dynamic_cast<fragInst*>(GetCurrentPhysicsInst()), "Object with cloth must be a frag");
			fragInst* pObjectFragInst=static_cast<fragInst*>(GetCurrentPhysicsInst());
			Assertf(pObjectFragInst->GetCached(), "Cloth object has no cache entry");
			if(pObjectFragInst->GetCached())
			{
				//The attachment frame.
				const Matrix34& attachFrame=matNew;

				//Pass the cloth attach frame to the env cloth.
				fragCacheEntry* pObjectCacheEntry=pObjectFragInst->GetCacheEntry();
				Assertf(pObjectCacheEntry, "Cloth object has a null cache entry");
				fragHierarchyInst* pObjectHierInst=pObjectCacheEntry->GetHierInst();
				Assertf(pObjectHierInst, "Cloth object has a null hier inst");
				environmentCloth* pEnvCloth=pObjectHierInst->envCloth;
				Assertf(pEnvCloth, "Cloth object has null env cloth ptr");
				pEnvCloth->SetInitialPosition( VECTOR3_TO_VEC3V(attachFrame.d) );

				if( pEnvCloth->GetBehavior()->IsActivateOnHitOverridden() )
				{
					pEnvCloth->GetBehavior()->SetActivateOnHit( false );
					pEnvCloth->GetBehavior()->SetActivateOnHitOverridden( false );
				}
			}
		}
		//Rage cloth end.

		if(GetPortalTracker() && pAttachParent->GetIsDynamic() REPLAY_ONLY(&& !CReplayMgr::IsReplayInControlOfWorld()) )
		{
			// In case we are attached to something, portal tracking works a little differently than normal
			// objects: first, we force the position and interior location of the child's tracker to be the
			// same of the parent's, then we move it to the current position to let it correctly traverse portals.
			// In this way we avoid bugs where the parent was teleported to an interior but the attachments weren't,
			// and still have an accurate interior location for them.

 			CPortalTracker*		tracker = GetPortalTracker();
 			CPortalTracker*		parentTracker = static_cast< CDynamicEntity* >( pAttachParent )->GetPortalTracker();
 			CInteriorInst*		pIntInst = parentTracker->GetInteriorInst();
		
			fwInteriorLocation parentLocation = pAttachParent->GetInteriorLocation();
			bool bShouldProcessDelta = GetIsReadyForInsertion();				 // only process deltas if ready to go in world
			bool parentIsAttachedToProtal = parentLocation.IsAttachedToPortal(); // you cannot add an object to a portal container.
			if (!parentIsAttachedToProtal && bShouldProcessDelta && tracker && parentTracker)
			{
				tracker->MoveToNewLocation( pIntInst, parentTracker->m_roomIdx );
				tracker->Teleport();
				tracker->Update( parentTracker->GetCurrentPos() );

				//only peds and entities attached to the local player will track from parent location
				//to their current location (too expensive otherwise)
				if ((tracker->GetOwner() && tracker->GetOwner()->GetType()==ENTITY_TYPE_PED) ||
					(pAttachParent == CGameWorld::FindLocalPlayer()))
				{
					UpdatePortalTracker();
				}
			}
		}
	}
	else if(extension->GetAttachState()==ATTACH_STATE_WORLD)
	{
		// For world attachments we auto detach this ped on ragdoll
		if (GetIsTypePed() && static_cast<CPed*>(this)->GetUsingRagdoll() && extension->GetAttachFlag(ATTACH_FLAG_AUTODETACH_ON_RAGDOLL))
		{
			DetachFromParent(0);
			return;
		}

		// Getting too many invalid cases of this assert. Removing it for now.
//		if(!Verifyf(!CPhysics::GetLevel()->IsActive(GetCurrentPhysicsInst()->GetLevelIndex()), "Model(%s)", GetModelName()))
//		{
//			CPhysics::GetSimulator()->DeactivateObject(GetCurrentPhysicsInst());
//		}

		Matrix34 matOffset;
		matOffset.Identity();

		matOffset.FromQuaternion(extension->GetAttachQuat());
		matOffset.d = extension->GetAttachOffset();

#if __ASSERT
		// Check for NaNs in the new matrix.
		if(matOffset.IsNotEqual(matOffset))
		{
			Assertf(false, "ProcessAttachment::ATTACH_STATE_WORLD calculated matNew, which contains non-numbers:"
				"\n a(%.4f, %.4f, %.4f) b %.4f, %.4f, %.4f) c(%.4f, %.4f, %.4f) d(%.4f, %.4f, %.4f)\n",
				matOffset.a.x, matOffset.a.y, matOffset.a.z,
				matOffset.b.x, matOffset.b.y, matOffset.b.z,
				matOffset.c.x, matOffset.c.y, matOffset.c.z,
				matOffset.d.x, matOffset.d.y, matOffset.d.z);
		}
#endif
		SetMatrix(matOffset);

		UpdatePortalTracker();
	}
	else if(extension->GetAttachState()==ATTACH_STATE_WORLD_PHYSICAL)
	{
	}
	else if(extension->GetAttachState()==ATTACH_STATE_PHYSICAL || extension->GetAttachState()==ATTACH_STATE_RAGDOLL)
	{
		physicsAssertf(pAttachParent, "A physical attachment has lost its parent");
	}
	else if(extension->GetAttachState()==ATTACH_STATE_DETACHING)
	{
		physicsAssertf(pAttachParent, "A detaching attachment has lost its parent");

		float fRange = GetBoundRadius() + pAttachParent->GetBoundRadius();
		if((GetBoundCentre() - pAttachParent->GetBoundCentre()).Mag2() > fRange*fRange)
		{
			extension->ClearParentAttachmentVars();
		}
	}
	else
	{
		Assertf(false, "unsupported attachment type in CPhysical");
	}
}

#if __BANK
void CPhysical::DebugProcessAllAttachments(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine)
#else
void CPhysical::ProcessAllAttachments()
#endif
{
	fwAttachmentEntityExtension *extension = GetAttachmentExtension();

	if (!extension)
	{
		return;
	}

	// Need to grab child and sibling here since the ProcessAttachment could delete the extension
	fwEntity *childAttachment = extension->GetChildAttachment();
	fwEntity *siblingAttachment = extension->GetSiblingAttachment();

	// Process this attachment and then any children
	if(extension->GetShouldProcessAttached())
	{
#if __BANK
		DebugProcessAttachment(strCodeFile, strCodeFunction, nCodeLine);
#else
		ProcessAttachment();
#endif
	}

	if(childAttachment)
	{
#if __BANK
	((CPhysical *) childAttachment)->DebugProcessAllAttachments(strCodeFile, strCodeFunction, nCodeLine);
#else
	((CPhysical *) childAttachment)->ProcessAllAttachments();
#endif
	}

	if(siblingAttachment)
	{
#if __BANK
	((CPhysical *) siblingAttachment)->DebugProcessAllAttachments(strCodeFile, strCodeFunction, nCodeLine);
#else
	((CPhysical *) siblingAttachment)->ProcessAllAttachments();
#endif
	}
}

void CPhysical::DeleteAllChildren(bool bCheckAttachFlags)
{
	fwAttachmentEntityExtension *attachExt = GetAttachmentExtension();

	if (attachExt)
	{
		CPhysical* pCurChild = (CPhysical *) attachExt->GetChildAttachment();
		while(pCurChild)
		{
			fwAttachmentEntityExtension &curChildAttachExt = pCurChild->GetAttachmentExtensionRef();

			CPhysical* pNextChild = (CPhysical *) curChildAttachExt.GetSiblingAttachment();
			if(curChildAttachExt.GetAttachFlag(ATTACH_FLAG_DELETE_WITH_PARENT) || !bCheckAttachFlags)
			{
				CGameWorld::RemoveEntity(pCurChild);
			}
			pCurChild = pNextChild;
		}
	}
}

void CPhysical::RemoveDeleteWithParentProgenyFromList(CEntity * list[], u32 count) const
{
	fwAttachmentEntityExtension const *attachExt = GetAttachmentExtension();

	if (attachExt)
	{
		CPhysical const* pCurChild = (const CPhysical *) attachExt->GetChildAttachment();
		while(pCurChild)
		{
			fwAttachmentEntityExtension const &curChildAttachExt = pCurChild->GetAttachmentExtensionRef();

			if(curChildAttachExt.GetAttachFlag(ATTACH_FLAG_DELETE_WITH_PARENT))
			{
				for(u32 i = 0 ; i < count ; i++)
				{
					if(list[i] == static_cast<const CEntity*>(pCurChild))
					{
						list[i] = NULL;
					}
				}
				pCurChild->RemoveDeleteWithParentProgenyFromList(list,count);
			}

			pCurChild = (const CPhysical *) curChildAttachExt.GetSiblingAttachment();
		}
	}
}

void CPhysical::GeneratePhysExclusionList(const CEntity** ppExclusionListStorage, int& nStorageUsed, int nMaxStorageSize, u32 includeFlags, u32 typeFlags, const CEntity* pOptionalExclude) const
{
	CDynamicEntity::GeneratePhysExclusionList(ppExclusionListStorage, nStorageUsed, nMaxStorageSize,includeFlags,typeFlags, pOptionalExclude);

	// Iterate through all child attachments
	CPhysical* pNextChild = (CPhysical *) GetChildAttachment();
	while(pNextChild && nStorageUsed < nMaxStorageSize)
	{
		if(pNextChild->GetPhysArch() && pNextChild->GetPhysArch()->MatchFlags(includeFlags,typeFlags))
		{
			// Only add the exclude list of the type and include flags match the test
			ppExclusionListStorage[nStorageUsed++] = pNextChild;
		}

		// Let this child generate an exclusion list of all of its children
		pNextChild->GeneratePhysExclusionList(ppExclusionListStorage,nStorageUsed,nMaxStorageSize,includeFlags,typeFlags,pOptionalExclude);

		// Move on to the sibling
		pNextChild = (CPhysical *) pNextChild->GetSiblingAttachment();
	}
}


bool CPhysical::DoRelationshipGroupsMatchForOnlyDamagedByCheck(const CRelationshipGroup* pRelGroupOfInflictor) const
{
// 	Assertf(sizeof(RelGroupOfInflictor) == sizeof(m_specificRelGroupIndex), "CPhysical::DoRelationshipGroupsMatchForOnlyDamagedByCheck - need to change type of RelGroupOfInflictor");
// 	if (is network session)
// 	{
// 		if (m_specificRelGroupIndex == CRelationshipManager::s_playerGroupIndex)
// 		{
// 			if ( ( (RelGroupOfInflictor >= RELGROUP_NETWORKPLAYER_1) && (RelGroupOfInflictor <= RELGROUP_NETWORKPLAYER_16) )
// 				|| ( (RelGroupOfInflictor >= RELGROUP_NETWORKTEAM_1) && (RelGroupOfInflictor <= RELGROUP_NETWORKTEAM_8) ) )
// 			{
// 				return true;
// 			}
// 		}
// 	}

	if (m_specificRelGroupHash == pRelGroupOfInflictor->GetName().GetHash())
	{
		return true;
	}

	return false;
}

bool CPhysical::CanPhysicalBeDamaged(const CEntity* pInflictor, u32 nWeaponUsedHash, bool bDoNetworkCloneCheck, bool bDisableDamagingOwner NOTFINAL_ONLY(, u32* uRejectionReason)) const
{
	// can't damage objects controlled by another machine
	if (IsNetworkClone() && bDoNetworkCloneCheck)
	{
		NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_NetworkClone; })
		return false;
	}

	if(m_nPhysicalFlags.bNotDamagedByAnything )
	{
		NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_NotDamagedByAnything; })
		return false;
	}

	//@@: range CPHYSICAL_CANPHYSICALBEDAMAGED {
	#if TAMPERACTIONS_ENABLED && TAMPERACTIONS_INVINCIBLECOP
		if(GetIsTypePed() && TamperActions::IsInvincibleCop(((CPed*)this)))
		{
			NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_TamperInvincibleCop; })
			return false;
		}
	#endif
	//@@: } CPHYSICAL_CANPHYSICALBEDAMAGED

	// Cache the CWeaponInfo
	const CItemInfo* pItemInfo = CWeaponInfoManager::GetInfo(nWeaponUsedHash);
	if(!pItemInfo || !pItemInfo->GetIsClassId(CWeaponInfo::GetStaticClassId()))
		return false;

	const CWeaponInfo* pWeaponInfo = static_cast<const CWeaponInfo*>(pItemInfo);
	Assert(pWeaponInfo);

	// Ped can't damage the thing they're attached to
	if(pInflictor && pInflictor->GetIsPhysical() && ((CPhysical*)pInflictor)->GetAttachParent()==this)
    {
		if(pWeaponInfo->GetDamageType()!=DAMAGE_TYPE_EXPLOSIVE && pWeaponInfo->GetDamageType()!=DAMAGE_TYPE_FIRE)
        {
			NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_AttachedToInflictor; })
		    return false;
        }
    }

	if( m_canOnlyBeDamagedByEntity != NULL &&
		pInflictor != m_canOnlyBeDamagedByEntity )
	{
		NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_OnlyDamagedByOtherEnt; })
		return false;
	}

	if( pInflictor == m_cantBeDamagedByEntity && 
		m_cantBeDamagedByEntity != NULL &&
		( nWeaponUsedHash == WEAPONTYPE_RAMMEDBYVEHICLE || 
		  nWeaponUsedHash == WEAPONTYPE_RUNOVERBYVEHICLE ||
		  nWeaponUsedHash == WEAPONTYPE_ROTORS ) )
	{
		if( pInflictor && pInflictor->GetIsPhysical() )
		{
			Displayf("[CPhysical::CanPhysicalBeDamaged] [Physical: %s %s] is ignoring collision with [Physical: %s %s]", 
						GetModelName(), GetNetworkObject() ? GetNetworkObject()->GetLogName() : "none",
						static_cast< const CPhysical*>( pInflictor )->GetModelName(), static_cast<const CPhysical*>( pInflictor )->GetNetworkObject() ? static_cast<const CPhysical*>( pInflictor )->GetNetworkObject()->GetLogName() : "none" );
		}
		return false;
	}

	// Ignore explosions for player damaged by checks
	// the ragdoll must be activated in the explosion code so the ped must be damaged.
	if(m_nPhysicalFlags.bOnlyDamagedByPlayer
		//&& nDamageType!= DAMAGE_TYPE_EXPLOSIVE // The damaged by check includes grenades rockets etc. that aren't covered in the weeapontype check belo
		&& nWeaponUsedHash!=WEAPONTYPE_DROWNING
		&& nWeaponUsedHash!=WEAPONTYPE_DROWNINGINVEHICLE
		&& nWeaponUsedHash!=WEAPONTYPE_BLEEDING
		&& nWeaponUsedHash!=WEAPONTYPE_FIRE)
	{
		if(pInflictor==NULL)
        {
			NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_OnlyDamagedByPlayerNullInflictor; })
			return false;
        }
		else if(pInflictor->GetIsTypePed())
        {
            if(!((CPed*)pInflictor)->IsPlayer())
            {
				NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_OnlyDamagedByPlayerNotPlayer; })
			    return false;
            }
        }
		else if(pInflictor->GetIsTypeVehicle())
        {
			const CVehicle* pVehicle = static_cast<const CVehicle*>(pInflictor);
            if(pVehicle->GetStatus()!=STATUS_PLAYER)
            {
				NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_OnlyDamagedByPlayerNonPlayerVehicle; })
			    return false;
            }
        }
        else
        {
			// If entity being damaged is a ped attached to a vehicle with no roof, we allow them to ragdoll out
			if (GetIsTypePed())
			{
				const CPed* pPed = static_cast<const CPed*>(this);
				if (pPed->GetIsInVehicle())
				{
					const CVehicle* pVehicle = pPed->GetMyVehicle();
					const s32 iSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
					if (pVehicle->IsSeatIndexValid(iSeatIndex))
					{
						const CVehicleSeatAnimInfo* pSeatAnimInfo = pVehicle->GetSeatAnimationInfo(iSeatIndex);
						if (pSeatAnimInfo && pSeatAnimInfo->GetRagdollWhenVehicleUpsideDown())
						{
							return true;
						}
					}
				}
			}
			NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_OnlyDamagedByPlayer; })
            return false;
        }
	}

	// Ignore explosions for relationship damaged by checks - 
	// the ragdoll must be activated in the explosion code so the ped must be damaged
	if( m_nPhysicalFlags.bOnlyDamagedByRelGroup
		//&& nDamageType!=DAMAGE_TYPE_EXPLOSIVE // The damaged by check includes grenades rockets etc. that aren't covered in the weeapontype check belo
		&& nWeaponUsedHash!=WEAPONTYPE_DROWNING
		&& (nWeaponUsedHash!=WEAPONTYPE_EXPLOSION || (nWeaponUsedHash == WEAPONTYPE_EXPLOSION && pInflictor && pInflictor->GetIsTypePed())) 
		&& nWeaponUsedHash!=WEAPONTYPE_DROWNINGINVEHICLE
		&& nWeaponUsedHash!=WEAPONTYPE_BLEEDING
		&& (nWeaponUsedHash!=WEAPONTYPE_FIRE || (nWeaponUsedHash == WEAPONTYPE_FIRE && pInflictor && pInflictor->GetIsTypePed())))
	{
		Assertf( !m_nPhysicalFlags.bNotDamagedByRelGroup, "CPhysical::CanPhysicalBeDamaged - can't set entity to be not damaged and only damaged by a relationship group at the same time");

		if(pInflictor==NULL)
        {
			NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_OnlyDamagedByRelGroupNullInflictor; })
			return false;
        }
		else if(pInflictor->GetIsTypePed())
        {
			if (!DoRelationshipGroupsMatchForOnlyDamagedByCheck(static_cast<const CPed*>(pInflictor)->GetPedIntelligence()->GetRelationshipGroup()))
            {
				NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_OnlyDamagedByRelGroupMismatch; })
			    return false;
            }
        }
		else if(pInflictor->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = (CVehicle*)pInflictor;
			if (!pVehicle->GetDriver())
			{
				NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_OnlyDamagedByRelGroupVehNoDriver; })
				return false;
			}
			if (!DoRelationshipGroupsMatchForOnlyDamagedByCheck(pVehicle->GetDriver()->GetPedIntelligence()->GetRelationshipGroup()))
			{
				NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_OnlyDamagedByRelGroupVehDriverGroupMismatch; })
				return false;
			}
		}
        else
        {
			NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_OnlyDamagedByRelGroup; })
            return false;
        }
	}

	// Ignore explosions for relationship damaged by checks - 
	// the ragdoll must be activated in the explosion code so the ped must be damaged
	if( m_nPhysicalFlags.bNotDamagedByRelGroup
		//&& nDamageType!=DAMAGE_TYPE_EXPLOSIVE // The damaged by check includes grenades rockets etc. that aren't covered in the weeapontype check below
		&& nWeaponUsedHash!=WEAPONTYPE_DROWNING
		&& nWeaponUsedHash!=WEAPONTYPE_EXPLOSION
		&& nWeaponUsedHash!=WEAPONTYPE_DROWNINGINVEHICLE
		&& nWeaponUsedHash!=WEAPONTYPE_BLEEDING
		&& nWeaponUsedHash!=WEAPONTYPE_FIRE)
	{
		Assertf( !m_nPhysicalFlags.bOnlyDamagedByRelGroup, "CPhysical::CanPhysicalBeDamaged - can't set entity to be not damaged and only damaged by a relationship group at the same time");
		if(pInflictor==NULL)
		{

		}
		else if(pInflictor->GetIsTypePed())
        {
            if(((CPed*)pInflictor)->GetPedIntelligence()->GetRelationshipGroup()->GetName().GetHash() == m_specificRelGroupHash )
            {
				NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_NotDamagedByRelGroup; })
			    return false;
            }
        }
		else if(pInflictor->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = (CVehicle*)pInflictor;
			if( pVehicle->GetDriver() && (pVehicle->GetDriver()->GetPedIntelligence()->GetRelationshipGroup()->GetName().GetHash() == m_specificRelGroupHash ))
			{
				NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_NotDamagedByRelGroupDriver; })
				return false;
			}
		}
        else
        {
        }
	}

    if(NetworkInterface::IsGameInProgress())
    {
	    if(m_nPhysicalFlags.bOnlyDamagedWhenRunningScript
		    && nWeaponUsedHash != WEAPONTYPE_DROWNING
		    && nWeaponUsedHash != WEAPONTYPE_DROWNINGINVEHICLE
		    && nWeaponUsedHash != WEAPONTYPE_BLEEDING)
	    {
		    if(pInflictor != NULL)
		    {
			    CPed* pPed = NULL;
			    if(pInflictor->GetIsTypePed())
				    pPed = (CPed*)(pInflictor);
			    else if(pInflictor->GetIsTypeVehicle())
			    {
				    CVehicle* pVehicle = (CVehicle*)(pInflictor);
				    pPed = pVehicle->GetDriver();
			    }

			    if(pPed && pPed->IsPlayer())
			    {
				    PhysicalPlayerIndex playerIndex = pPed->GetNetworkObject()->GetPlayerOwner()->GetPhysicalPlayerIndex();
				    const CScriptEntityExtension* pExtension = GetExtension<CScriptEntityExtension>(); 
				    if(pExtension)
				    {
					    scriptHandler* pScriptHandler = pExtension->GetScriptHandler();
					    if(pScriptHandler)
					    {
						    scriptHandlerNetComponent* pNetComponent = pScriptHandler->GetNetworkComponent();
						    if(pNetComponent)
						    {
							    if(!pNetComponent->IsPlayerAParticipant(playerIndex))
								{
									NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_OnlyDamagedOnScript; })
									return false;
								}
						    }
					    }
					    else
					    {
						    CGameScriptHandlerMgr::CRemoteScriptInfo* pRSI = CTheScripts::GetScriptHandlerMgr().GetRemoteScriptInfo(pExtension->GetScriptInfo()->GetScriptId(), true);
						    if(pRSI && !pRSI->IsPlayerAParticipant(playerIndex))
							{
								NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_OnlyDamagedOnScriptRSI; })
								return false;
							}
					    }
				    }
			    }	 
		    }
	    }
    }

	// Go through the different weapon types
	switch(pWeaponInfo->GetDamageType())
	{
	case DAMAGE_TYPE_MELEE:
		{
			bool bIgnoreNotDamagedByMeleeProof = false;
			if(GetIsTypePed())
			{
				bIgnoreNotDamagedByMeleeProof = static_cast<const CPed*>(this)->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowMeleeReactionIfMeleeProofIsOn);
			}

			//! melee proof still does anim, so carry through and reject later.
			if ( (m_nPhysicalFlags.bNotDamagedByMelee && !bIgnoreNotDamagedByMeleeProof) )
			{
				NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_NotDamagedByMelee; })
				return false;
			}

			if ((bDisableDamagingOwner && pInflictor==this))
			{
				NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_DisableDamagingOwner; })
				return false;
			}
		}
		break;

	case DAMAGE_TYPE_BULLET:
		if (m_nPhysicalFlags.bNotDamagedByBullets)
		{
			NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_NotDamagedByBullets; })
			return false;
		}
		break;

	case DAMAGE_TYPE_EXPLOSIVE:
		if (GetIgnoresExplosions())
		{
			NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_IgnoresExplosions; })
			return false;
		}
		if ((bDisableDamagingOwner && pInflictor==this))
		{
			NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_DisableDamagingOwner; })
			return false;
		}
		break;

	case DAMAGE_TYPE_FIRE:
		if (m_nPhysicalFlags.bNotDamagedByFlames)
		{
			NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_NotDamagedByFlames; })
			return false;
		}
		break;

	case DAMAGE_TYPE_SMOKE:
		if (m_nPhysicalFlags.bNotDamagedBySmoke)
		{
			NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_NotDamagedBySmoke; })
			return false;
		}
		break;

	default:
		break;
	}

	if(nWeaponUsedHash == WEAPONTYPE_RAMMEDBYVEHICLE || nWeaponUsedHash == WEAPONTYPE_RUNOVERBYVEHICLE || nWeaponUsedHash == WEAPONTYPE_FALL
#if 0 // CS
		|| nWeaponUsedHash == WEAPONTYPE_OBJECT
#endif // 0
		)
	{
		if (m_nPhysicalFlags.bNotDamagedByCollisions)
		{
			NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_NotDamagedByCollisions; })
			return false;
		}
	}

	return true;
}

float CPhysical::GetDistanceFromCentreOfMassToBaseOfModel(void) const
{
	CBaseModelInfo* pModelInfo=GetBaseModelInfo();
	const float z=pModelInfo->GetBoundingBoxMin().z;
	return (-z);
	/*
	const phBound* pBound=GetCurrentPhysicsInst()->GetArchetype()->GetBound();
	const float cmZ=pBound->GetCGOffset().z;
	const float LowestZ=pBound->GetBoundingBoxMin().z;
	return (cmZ-LowestZ);
	*/

//	float LowestZ;
//
//	CColModel& ColMod = GetColModel();
//	LowestZ = ColMod.GetBoundBoxMin().z;
//	return(-LowestZ);
}


//
// This flag keeps track of whether the physical is above water (even though the z coordinate might be low ie in a tunnel)
//

void CPhysical::UpdatePossiblyTouchesWaterFlag()
{
	if(GetBaseModelInfo())//Make sure we have a valid base model info.
	{
		Mat34V matrix;
		Vec3V boundingBoxLocalMin;
		Vec3V boundingBoxLocalMax;
		if(const phInst* instance = GetCurrentPhysicsInst())
		{
			const phBound* bound = instance->GetArchetype()->GetBound();
			boundingBoxLocalMin = bound->GetBoundingBoxMin();
			boundingBoxLocalMax = bound->GetBoundingBoxMax();
			matrix = instance->GetMatrix();
		}
		else
		{
			spdAABB boundingBox = GetBaseModelInfo()->GetBoundingBox();
			boundingBoxLocalMin = boundingBox.GetMin();
			boundingBoxLocalMax = boundingBox.GetMax();
			matrix = GetMatrix();
		}

		const Vec3V boundBoxLocalCenter = Average(boundingBoxLocalMin,boundingBoxLocalMax);
		const Vec3V boundBoxHalfExtents = geomBoxes::ComputeAABBExtentsFromOBB(matrix.GetMat33(),Subtract(boundingBoxLocalMax,boundBoxLocalCenter));
		const Vec3V curPos = Transform(matrix,boundBoxLocalCenter);
		const ScalarV curBottomZ = Subtract(curPos,boundBoxHalfExtents).GetZ();

		// Reset flag which shows we have completed filling in certain water sample based information in CBuoyancy::Process().
		m_Buoyancy.m_buoyancyFlags.bBuoyancyInfoUpToDate = false;

		// First check if we are in a river. If we are, set the possibly in water flag and return immediately. Otherwise
		// continue to the main water block tests. We do this here so that CBuoyancy::Process() will actually get called.
		if(m_Buoyancy.GetRiverBoundProbeResult())
		{
			Vec3V vRiverHitPos, vRiverHitNormal;
			m_Buoyancy.GetCachedRiverBoundProbeResult(RC_VECTOR3(vRiverHitPos),RC_VECTOR3(vRiverHitNormal));
	#if __BANK
			if(CBuoyancy::ms_bRiverBoundDebugDraw)
			{
				DEBUG_DRAW_ONLY(grcDebugDraw::Sphere(vRiverHitPos, 0.3f, Color_yellow, true, 100));
			}
	#endif // __BANK
			if(IsLessThanOrEqualAll(curBottomZ,vRiverHitPos.GetZ()))
			{
				m_nFlags.bPossiblyTouchesWater = true;
				m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate = true;
				return;
			}
		}

		// Check where the object is in relation to the main water level.
		// Add a little extra on to account for the max possible wave height
		// MN - using CGameWorldWaterHeight::GetWaterHeight() here instead of Water::GetWaterLevel() as it's more accurate
		const ScalarV vMaxWaveHeight(V_TEN);
		if (IsGreaterThanOrEqualAll(curBottomZ, rage::Add(ScalarVFromF32(CGameWorldWaterHeight::GetWaterHeight()),vMaxWaveHeight)))
		{	
			// The object is well above the main water level.
			m_nFlags.bPossiblyTouchesWater = false;
			m_Buoyancy.SetStatusHighNDry();

			m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate = true;
			return;
		}

		// Try to get the actual water level at the position of the object to use in the tests below
		float	curPosWaterLevelZ;
		if(!Water::GetWaterLevel(RCC_VECTOR3(curPos), &curPosWaterLevelZ, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL, this))
		{
			// There is no water here.
			m_nFlags.bPossiblyTouchesWater = false;
			m_Buoyancy.SetStatusHighNDry();

			m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate = true;
			return;
		}

		// Check if we can do a fast incremental update or if we need to do a full test.
		if(GetIsAnyFixedFlagSet())
		{
			// Do nothing as our position is not changing.
			m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate = false;
			return;
		}
		else
		{
			// Build a value without prior knowledge here.

			const bool bottomBelowWater				= IsLessThanOrEqualAll(curBottomZ, ScalarVFromF32(curPosWaterLevelZ)) != 0;
			const bool alreadyInWater				= (m_nPhysicalFlags.bWasInWater || GetIsInWater());
			CPortalTracker*	pPortalTracker			= GetPortalTracker();
			bool bInsideInteriorWithWater			= pPortalTracker->GetInteriorInst() ? pPortalTracker->GetInteriorInst()->IsWaterInside()  : false;
			if(!bottomBelowWater && !alreadyInWater && !bInsideInteriorWithWater)	
			{
				// Looks like we aren't in water
				m_nFlags.bPossiblyTouchesWater = false;
				m_Buoyancy.SetStatusHighNDry();
			}
			else
			{
				// The object possibly intersects with the water.
				m_nFlags.bPossiblyTouchesWater = true;
			}

		}

		m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate = true;
	}

}

void CPhysical::SetIsFixedWaitingForCollision( const bool bFixed )
{
	// This flag needs to be set before OnFixIfNoCollisioNLoadedAroundPosition
	const bool bWasFixed = IsProtectedBaseFlagSet(fwEntity::IS_FIXED_UNTIL_COLLISION);
	CEntity::SetIsFixedWaitingForCollision(bFixed);

	if( bWasFixed != bFixed )
	{
		OnFixIfNoCollisionLoadedAroundPosition(bFixed);
		Assertf(!bFixed || GetCollider() == NULL,"Was unable to deactivate object '%s' even though it should be fixed waiting for collision.",GetDebugName());
	}
}

void CPhysical::SetDontLoadCollision(bool bDontLoad)
{
	m_nPhysicalFlags.bDontLoadCollision = bDontLoad;

	if (AssertVerify(GetPortalTracker()))
	{
		if (!bDontLoad)
		{
			GetPortalTracker()->SetLoadsCollisions(true);	//force loading of interior collisions
			CPortalTracker::AddToActivatingTrackerList(GetPortalTracker());		// causes interiors to activate & stay active
		}
		else
		{
			GetPortalTracker()->SetLoadsCollisions(false);	//disable loading of interior collisions
			CPortalTracker::RemoveFromActivatingTrackerList(GetPortalTracker());
		}
	}
}

void CPhysical::CleanupMissionState()
{
	CDynamicEntity::CleanupMissionState();

	SetDontLoadCollision(true);

	// don't do the following for objects, some of them (eg safe doors) need to leave certain flags set
	if (!GetIsTypeObject())
	{
		if (!GetIsTypeVehicle() || ((static_cast<CVehicle*>(this))->m_nVehicleFlags.bUnfreezeWhenCleaningUp) )
		{
			SetFixedPhysics(false);
		}
		SetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT, true);

		// Don't enable collision for a ped inside a vehicle, the collision state is handled as part of the vehicle attachment code
		bool bEnableCollision = true;
		if (GetIsTypePed() && (static_cast<CPed*>(this))->GetIsInVehicle())
		{
			bEnableCollision = false;
		}

		if (bEnableCollision)
		{
			EnableCollision();
		}

		if(!m_bDontResetDamageFlagsOnCleanupMissionState)
		{
			m_nPhysicalFlags.bIgnoresExplosions				= false;
			m_nPhysicalFlags.bOnlyDamagedByPlayer			= false;
			m_nPhysicalFlags.bOnlyDamagedByRelGroup			= false;
			m_nPhysicalFlags.bNotDamagedByRelGroup			= false;
			m_nPhysicalFlags.bOnlyDamagedWhenRunningScript	= false;
			m_nPhysicalFlags.bNotDamagedByBullets			= false;
			m_nPhysicalFlags.bNotDamagedByFlames			= false;
			m_nPhysicalFlags.bNotDamagedByCollisions		= false;
			m_nPhysicalFlags.bNotDamagedByMelee				= false;
			m_nPhysicalFlags.bNotDamagedByAnything			= false;
			m_nPhysicalFlags.bNotDamagedByAnythingButHasReactions = false;
			m_nPhysicalFlags.bNotDamagedBySteam				= false;
			m_nPhysicalFlags.bNotDamagedBySmoke				= false;
		}
		m_specificRelGroupHash = 0;
	}
}

bool CPhysical::DoesGroundCauseRelativeMotion(const CPhysical* pGround)
{
	//Ensure the ground is valid.
	if(!pGround)
	{
		return false;
	}

	//Ensure the ground is not a ped.
	if(pGround->GetIsTypePed())
	{
		return false;
	}

	//Ensure the bounds are significant.
	if(pGround->GetBoundRadius() <= 0.25f)
	{
		return false;
	}

	return true;
}

void CPhysical::CalculateGroundVelocity(const CPhysical* pGround, Vec3V_In vPos, int nComponent, Vec3V_InOut vVel)
{
	//Ensure the ground causes relative motion.
	if(!DoesGroundCauseRelativeMotion(pGround))
	{
		vVel = Vec3V(V_ZERO);
		return;
	}

	//Calculate the ground velocity.
	vVel = VECTOR3_TO_VEC3V(pGround->GetLocalSpeed(VEC3V_TO_VECTOR3(vPos), true, nComponent));
	Assertf(IsFiniteAll(vVel), "Ground velocity is invalid.");
}

void CPhysical::CalculateGroundVelocityIntegrated(const CPhysical* pGround, Vec3V_In vPos, int nComponent, float fTimeStep, Vec3V_InOut vVel)
{
	//Ensure the ground causes relative motion.
	if(!DoesGroundCauseRelativeMotion(pGround))
	{
		vVel = Vec3V(V_ZERO);
		return;
	}

	//Calculate the ground velocity integrated.
	vVel = VECTOR3_TO_VEC3V(pGround->GetLocalSpeedIntegrated(fTimeStep, vPos, true, nComponent));
	Assertf(IsFiniteAll(vVel), "Integrated ground velocity is invalid.");
}

void CPhysical::CalculateGroundAngularVelocity(const CPhysical* pGround, int nComponent, Vec3V_Ref vAngVel) const
{
	//Ensure the ground causes relative motion.
	if(!DoesGroundCauseRelativeMotion(pGround))
	{
		vAngVel = Vec3V(V_ZERO);
		return;
	}

	//Generate the ped's up-vector.
	Vec3V vUp = GetTransform().GetC();

	//Grab the ground angular velocity.
	Vec3V groundAngularVelocity;
	if(const phCollider* pGroundCollider = pGround->GetCollider())
	{
		groundAngularVelocity = pGroundCollider->GetAngVelocity();
	}
	else if(const phInst* pGroundInst = pGround->GetCurrentPhysicsInst())
	{
		groundAngularVelocity = pGroundInst->GetExternallyControlledAngVelocity(nComponent);
	}
	else
	{
		groundAngularVelocity = Vec3V(V_ZERO);
	}
	Assertf(IsFiniteAll(groundAngularVelocity), "Ground angular velocity is invalid.");

	//Project the ground's angular velocity on to the up vector.
	vAngVel = Dot(vUp, groundAngularVelocity) * vUp;
}

CPhysical* CPhysical::GetGroundPhysicalForPhysical(const CPhysical& rPhysical)
{
	//Check the type of the physical.
	if(rPhysical.GetIsTypePed())
	{
		return static_cast<const CPed &>(rPhysical).GetGroundPhysical();
	}
	else if(rPhysical.GetIsTypeObject())
	{
		return static_cast<const CObject &>(rPhysical).GetGroundPhysical();
	}
	else
	{
		return NULL;
	}
}

CPhysical* CPhysical::GetRootGroundPhysicalForPhysical(const CPhysical& rPhysical)
{
	//Keep track of the ground hierarchy.
	static const int sMaxGroundHierarchy = 8;
	atRangeArray<CPhysical *, sMaxGroundHierarchy> aGroundHierarchy;
	s32 iGroundHierarchy = 0;

	//Find the root ground.
	const CPhysical* pCurrent = &rPhysical;
	while(iGroundHierarchy < sMaxGroundHierarchy)
	{
		//Find the ground.
		CPhysical* pGround = CPhysical::GetGroundPhysicalForPhysical(*pCurrent);
		
		//The break conditions are:
		//	1) The end of the ground hierarchy has been reached (there is no ground).
		//	2) The ground has been encountered before (cycle).
		
		//Ensure the ground is valid.
		if(!pGround)
		{
			break;
		}
		
		//Ensure the ground has not been encountered before.
		bool bFound = false;
		for(s32 i = 0; i < iGroundHierarchy; ++i)
		{
			if(aGroundHierarchy[i] == pGround)
			{
				bFound = true;
				break;
			}
		}
		if(bFound)
		{
			break;
		}
		
		//Add the ground.
		aGroundHierarchy[iGroundHierarchy++] = pGround;
		
		//Assign the current physical.
		pCurrent = pGround;
	}
	
	//Check if ground has been found.
	if(iGroundHierarchy == 0)
	{
		//No ground was found.
		return NULL;
	}
	else
	{
		//Return the last ground that was found successfully.
		return aGroundHierarchy[iGroundHierarchy - 1];
	}
}

bool CPhysical::IsEntityAParentAttachment(const fwEntity* entity) const
{
	fwEntity* pParent = GetAttachParentForced();
	bool bReturn = false;
	while(!bReturn && pParent)
	{
		bReturn = (pParent == entity);
		pParent = pParent->GetAttachParentForced();
	}

	return bReturn;
}

#if USE_PHYSICAL_HISTORY
float	CPhysicalHistory::HISTORY_FREQUENCY_MIN = 1.0f;
float	CPhysicalHistory::HISTORY_FREQUENCY_MAX = 5.0f;

void CPhysicalHistory::UpdateHistory( Matrix34& mThisMat, CEntity* pEntity, Color32 colour )
{
#if __FINAL
	I shouldn't be active in a final build, i use a lot of memory, someone has checked in USE_PHYSICAL_HISTORY 1 by mistake
#endif

	if( m_iHistorySize == 0 )
	{
		m_amHistory[0] = mThisMat;
		m_iHistorySize = 1;
		m_iHistoryStart = 0;
		m_aColors[0] = Color_white;
		return;
	}

	float fFrequency = HISTORY_FREQUENCY_MIN;
	CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
	if( pModelInfo )
	{
		Vector3 vBoundMin = pModelInfo->GetBoundingBoxMin();
		Vector3 vBoundMax = pModelInfo->GetBoundingBoxMax();
		fFrequency = MAX( vBoundMax.y - vBoundMin.y, HISTORY_FREQUENCY_MIN );
		fFrequency = MIN( fFrequency, HISTORY_FREQUENCY_MAX );
	}

	if( m_amHistory[m_iHistoryStart].d.Dist2(mThisMat.d) < rage::square(fFrequency) )
	{
		return;
	}

	++m_iHistoryStart;
	++m_iHistorySize;

	if( m_iHistoryStart >= MAX_HISTORY_SIZE )
		m_iHistoryStart = 0;

	if( m_iHistorySize > MAX_HISTORY_SIZE )
		m_iHistorySize = MAX_HISTORY_SIZE;

	m_amHistory[m_iHistoryStart] = mThisMat;
	m_aColors[m_iHistoryStart] = colour;
}

void CPhysicalHistory::RenderHistory( CEntity* pEntity )
{
	Vector3 vLastPos;
	Color32 lastColour;
	bool bHasPos = false;
	for( s32 i = 0; i < m_iHistorySize; i++ )
	{
		s32 iIndex = m_iHistoryStart - i;
		if( iIndex < 0 )
			iIndex += MAX_HISTORY_SIZE;

		CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
		if( pModelInfo )
		{
			Vector3 vBoundMin = pModelInfo->GetBoundingBoxMin();
			Vector3 vBoundMax = pModelInfo->GetBoundingBoxMax();
			CDebugScene::DrawBoundingBox(vBoundMin, vBoundMax, m_amHistory[iIndex], m_aColors[iIndex]);
		}
		grcDebugDraw::Axis(m_amHistory[iIndex], 1.0f, m_aColors[iIndex]);
		if( bHasPos )
			grcDebugDraw::Line(vLastPos, m_amHistory[iIndex].d, lastColour, m_aColors[iIndex]);

		bHasPos = true;
		vLastPos = m_amHistory[iIndex].d;
		lastColour = m_aColors[iIndex];
	}
}
#endif

#if __ASSERT
void CPhysical::PrintRegisteredPhysicsCreationBacktrace(CPhysical* pPhysical)
{
	if(pPhysical)
	{
		sysStack::PrintRegisteredBacktrace(pPhysical->m_physicsBacktrace);
	}
}

void CPhysical::RegisterPhysicsCreationBacktrace(CPhysical* pPhysical)
{
	if(pPhysical)
	{
		pPhysical->m_physicsBacktrace = sysStack::RegisterBacktrace();
	}
}
#endif

#if __BANK
void CPhysical::RenderDebugObjectIDNames()
{
	if(NetworkInterface::IsGameInProgress())
		return;

	const NetworkDebug::NetworkDisplaySettings &displaySettings = NetworkDebug::GetDebugDisplaySettings();
	if(!displaySettings.m_displayObjectInfo)
		return;

	s32 iIteratorFlags = IteratePeds | IterateVehicles;

	CEntityIterator entityIterator(iIteratorFlags, NULL);
	CEntity* pEntity = entityIterator.GetNext();

	while(pEntity)
	{
		if(pEntity->GetIsPhysical())
		{
			const CPhysical* pPhysical = static_cast<const CPhysical*>(pEntity);
			Vector3 centre = VEC3_ZERO;

			float radius = pPhysical->GetBoundRadius();
			pPhysical->GetBoundCentre(centre);

			if (camInterface::IsSphereVisibleInGameViewport(centre, radius))
			{
				Vector3 WorldCoors;
				if(pPhysical->GetIsTypePed() && static_cast<const CPed*>(pPhysical)->GetUsingRagdoll() && static_cast<const CPed*>(pPhysical)->GetRagdollInst())
				{
					Matrix34 matComponent = MAT34V_TO_MATRIX34(static_cast<const CPed*>(pPhysical)->GetMatrix());
					static_cast<const CPed*>(pPhysical)->GetRagdollComponentMatrix(matComponent, RAGDOLL_BUTTOCKS);
					WorldCoors = matComponent.d;
				}
				else
				{
					WorldCoors = VEC3V_TO_VECTOR3(pPhysical->GetTransform().GetPosition());
				}

				Vector2 screenCoords;
				float fScale = 1.0f;
				if(NetworkUtils::GetScreenCoordinatesForOHD(WorldCoors, screenCoords, fScale))
				{
					Color32 colour = CRGBA(128,128,255,255);
					DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, colour, fScale, pPhysical->GetDebugNameFromObjectID()));
				}
			}
		}

		pEntity = entityIterator.GetNext();
	}
}
#endif

void ReportHealthMismatch(u32 /*_param*/, u32 /*tampered*/)
{
	//LinkDataReporterPlugin_Report(LinkDataReporterCategories::LDRC_HEALTH);
	return;
}
