//
// physics/gtaInst.cpp
//

// Game headers
#include "ai/debug/types/AnimationDebugInfo.h"
#include "audio/collisionaudioentity.h"
#include "event/events.h"
#include "event/EventGroup.h"
#include "event/EventMovement.h"
#include "event/EventScript.h"
#include "Ik/IkManager.h"
#include "modelinfo/PedModelInfo.h"
#include "modelInfo/vehicleModelInfo.h" 
#include "network/General/NetworkUtil.h"
#include "Objects/Door.h"
#include "objects/object.h"
#include "pathserver/PathServer.h"
#include "pathserver/ExportCollision.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "physics/gtaInst.h"
#include "physics/gtaMaterialManager.h"
#include "physics/physics.h"
#include "physics/sleep.h"
#include "physics/VehicleFragImpulseFunction.h"
#include "physics/WorldProbe/worldprobe.h"
#include "pickups/Pickup.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/physics/NmDebug.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMBalance.h"
#include "Task/Physics/TaskNMSlungOverShoulder.h"
#include "Task/Physics/TaskRageRagdoll.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Vehicles/Boat.h"
#include "Vehicles/Planes.h"
#include "Vehicles/Train.h"
#include "Vehicles/Vehicle.h"
#include "Vehicles/Submarine.h"
#include "Vehicles/AmphibiousAutomobile.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "Vfx/VisualEffects.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Systems/VfxTrail.h"
#include "Vfx/Systems/VfxMaterial.h"
#include "Weapons/Projectiles/Projectile.h"
#include "Weapons/Weapon.h"
#include "game/ModelIndices.h"

// Rage headers
#include "crskeleton/skeleton.h"
#include "crskeleton/skeletondata.h"
#include "fragmentnm/manager.h"
#include "fragmentnm/nm_channel.h "
#include "fragment/typeChild.h"
#include "fragment/typeGroup.h"
#include "fwanimation/animmanager.h"
#include "fwanimation/animdirector.h"
#include "fwanimation/directorcomponentsyncedscene.h"
#include "fwanimation/directorcomponentragdoll.h"
#include "parser/manager.h"
#include "physics/collider.h"
#include "physics/debugcontacts.h"
#include "physics/intersection.h"
#include "phbound/boundcapsule.h"
#include "phbound/boundGeomSecondSurface.h"
#include "phcore/materialmgr.h"
#include "pharticulated/articulatedcollider.h"
#include "phsolver/contactmgr.h"
#include "vectormath/classfreefuncsv.h"

PHYSICS_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()
FRAGMENT_OPTIMISATIONS()

// The size of these pools will be set in the InitPool() calls, because
// the default size depends on the ped and/or vehicle pools, unknown until
// we've loaded the configuration file.
FW_INSTANTIATE_CLASS_POOL_SPILLOVER(phInstGta, 0, 0.56f, atHashString("phInstGta",0xeb9564e1));
FW_INSTANTIATE_CLASS_POOL_SPILLOVER(fragInstGta, 0,	0.15f, atHashString("fragInstGta",0x6851786c));
FW_INSTANTIATE_CLASS_POOL_SPILLOVER(fragInstNMGta, 0, 0.26f, atHashString("fragInstNMGta",0x2fead7a6));

PARAM(logusedragdolls, "Logs the maximum number of NM agents, rage ragdolls and anim fallbacks the get used.");

#if __DEV
phInst* spDebugBreakCar = NULL;
phInst* spDebugBreakRagdoll = NULL;
phInst* spDebugBreakGround = NULL;
#endif

#if __DEV
void InvalidStateDumpHelper(const phInst* pInst)
{
	Displayf("InvalidStateDumpHelper - 0x%p - '%s'", pInst, pInst->GetArchetype() ? pInst->GetArchetype()->GetFilename() : NULL);
	if(const CEntity* pEntity = CPhysics::GetEntityFromInst(pInst))
	{
		Displayf("\tEntity - 0x%p", pEntity);
		Displayf("\tCEntity::GetType - %i",pEntity->GetType());
		Displayf("\tCEntity::GetDebugName - '%s'",pEntity->GetDebugName());
		Displayf("\tCEntity::GetModelName - '%s'",pEntity->GetModelName());
		Displayf("\tCEntity::IsCollisionEnabled - %s",pEntity->IsCollisionEnabled() ? "T" : "F");
		Displayf("\tCEntity::GetIsAnyFixedFlagSet - %s",pEntity->GetIsAnyFixedFlagSet() ? "T" : "F");
		Displayf("\tCEntity::IsStatic - %s",pEntity->GetIsStatic() ? "T" : "F");
		Vec3V position = pEntity->GetTransform().GetPosition();
		Displayf("\tCEntity::GetTransform::GetPosition - <%f, %f, %f>",VEC3V_ARGS(position));
		if(pEntity->GetIsPhysical())
		{
			const CPhysical* pPhysical = static_cast<const CPhysical*>(pEntity);
			Vec3V velocity = RCC_VEC3V(pPhysical->GetVelocity());
			Vec3V angVelocity = RCC_VEC3V(pPhysical->GetAngVelocity());
			Displayf("\tCPhysical::GetVelocity - <%f, %f, %f>",VEC3V_ARGS(velocity));
			Displayf("\tCPhysical::GetAngVelocity - <%f, %f, %f>",VEC3V_ARGS(angVelocity));
			if(pPhysical->GetIsTypeVehicle())
			{
				const CVehicle* pVehicle = static_cast<const CVehicle*>(pPhysical);
				Displayf("\tVehicle Info:");
				Displayf("\t\tCVehicle::IsDummy: %s", pVehicle->IsDummy() ? "T" : "F");
				Displayf("\t\tCVehicle::IsSuperDummy: %s", pVehicle->IsSuperDummy() ? "T" : "F");
				Displayf("\t\tCVehicle::IsClone: %s", pVehicle->IsNetworkClone() ? "T" : "F");
			}
		}
		else
		{
			Displayf("\tNon-Physical Entity");
		}
	}
	else
	{
		Displayf("\tNo Entity");
	}
}
#endif // __DEV

atFixedArray<RegdPed, 40> g_SettledPeds;

/////////////////////////////////////////////////////
// phInstGta
/////////////////////////////////////////////////////

phInstGta::phInstGta (s32 nInstType) : phfwInst(nInstType)
{
}

phInstGta::~phInstGta()
{
}

//
// this function lets us skip out of collision tests
//
bool phInstGta::ShouldFindImpacts(const phInst* pOtherInst) const
{
	FastAssert(pOtherInst);
#if __DEV
	if(pOtherInst==spDebugBreakCar && this==spDebugBreakGround)
		Printf("Found combo");
#endif

#if ENABLE_NETWORK_LOGGING
	// pickups log the reasons why ShouldFindImpacts() may fail, as there are some bugs with ProcessPreComputeImpacts never getting called on pickups, 
	// making them uncollectable. This is a mission breaker.
	CPickup *pPickup = NULL;

	if(NetworkInterface::IsGameInProgress())
	{
		if (GetUserData() && pOtherInst->GetUserData())
		{
			if (GetClassType()==PH_INST_OBJECT && pOtherInst->GetClassType()==PH_INST_PED)
			{
				CObject *pObject = (CObject *)GetUserData();
				CPed *pPed = (CPed *)pOtherInst->GetUserData();

				if (pObject->m_nObjectFlags.bIsPickUp && pObject->GetNetworkObject() && pPed->IsLocalPlayer())
				{
					pPickup = (CPickup*)pObject;
				}
			}
			else if (GetClassType()==PH_INST_PED && pOtherInst->GetClassType()==PH_INST_OBJECT)
			{
				CObject *pObject = (CObject *)pOtherInst->GetUserData();
				CPed *pPed = (CPed *)GetUserData();

				if (pObject->m_nObjectFlags.bIsPickUp && pObject->GetNetworkObject() && pPed->IsLocalPlayer())
				{
					pPickup = (CPickup*)pObject;
				}
			}
		}
	}

#endif
	if(GetInstFlag(FLAG_NO_COLLISION))
	{
		LOGGING_ONLY(if (pPickup) pPickup->ShouldFindImpactsFailed("FLAG_NO_COLLISION set"));
		return false;
	}

	if(CEntity* pEntity = (CEntity*)GetUserData())
	{
		if(pEntity->GetIsPhysical())
		{
			CPhysical* pPhysical = static_cast<CPhysical*>(pEntity);
			CEntity* pOtherEntity = (CEntity*)pOtherInst->GetUserData();
			const bool isOtherEntityPhysical = pOtherEntity && pOtherEntity->GetIsPhysical();
			if(pEntity->GetIsTypePed())
			{
				CPed *pPed = static_cast<CPed*>(pEntity);

				if(pEntity == pOtherEntity)
				{
					LOGGING_ONLY(if (pPickup) pPickup->ShouldFindImpactsFailed("User datas the same"));
					return false;
				}

				if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
				{
					if (pOtherEntity && pOtherEntity->GetIsTypeVehicle() && pOtherEntity == pPed->GetMyVehicle())
					{
						if(!pPed->GetPedResetFlag(CPED_RESET_FLAG_IsGoingToStandOnExitedVehicle))
						{
							LOGGING_ONLY(if (pPickup) pPickup->ShouldFindImpactsFailed("CPED_RESET_FLAG_IsExitingVehicle set on player"));
							return false;
						}
					}
				}
				else if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
				{
					LOGGING_ONLY(if (pPickup) pPickup->ShouldFindImpactsFailed("CPED_CONFIG_FLAG_InVehicle set on player"));
					return false;
				}

				// Optimization Sanity Check: FLAG_NO_COLLISION should be set in this case
				Assert(pPed->GetRagdollState() <= RAGDOLL_STATE_ANIM_DRIVEN);
				
				if(isOtherEntityPhysical)
				{
					if(pPed->GetRagdollOnCollisionIgnorePhysical() && 
						pPed->GetRagdollOnCollisionIgnorePhysical() == pOtherEntity)
					{
						LOGGING_ONLY(if (pPickup) pPickup->ShouldFindImpactsFailed("RagdollOnCollisionIgnoreEntity"));
						return false;
					}
				}
			}
			else if(pPhysical->GetIsTypeObject())
			{
				// don't let doors collide with other doors
				if(GetInstFlag(FLAG_IS_DOOR) && !ShouldFindImpactsDoorHelper(pOtherInst))
				{
					return false;
				}
			}

			// in MP collisions between certain vehicles can be disabled (eg ghost vehicles in a race)
			if (NetworkInterface::IsGameInProgress() && pOtherEntity && NetworkInterface::AreInteractionsDisabledInMP(*pPhysical, *pOtherEntity))
			{
				NetworkInterface::RegisterDisabledCollisionInMP(*pPhysical, *pOtherEntity);
				return false;
			}

			// Optimization Sanity Check: FLAG_IS_DOOR shouldn't be set on non CObjects
			Assert(pPhysical->GetIsTypeObject() || !GetInstFlag(FLAG_IS_DOOR));

			if (pPhysical->GetAnimDirector())
			{
				// are we in a synchronized scene?
				fwAnimDirectorComponentSyncedScene* pSceneComponent = static_cast<fwAnimDirectorComponentSyncedScene*>(pPhysical->GetAnimDirector()->GetComponentByType(fwAnimDirectorComponent::kComponentTypeSyncedScene));
				if (pSceneComponent && pSceneComponent->IsPlayingSyncedScene())
				{
					fwSyncedSceneId sceneId = pSceneComponent->GetSyncedSceneId();
					fwEntity* sceneAttachParent = fwAnimDirectorComponentSyncedScene::GetSyncedSceneAttachEntity(sceneId);
					// don't collide with the entity our scene is attached to
					if (sceneAttachParent && sceneAttachParent==pOtherEntity)
					{
						LOGGING_ONLY(if (pPickup) pPickup->ShouldFindImpactsFailed("In synced scene"));
						return false;
					}
				}
			}

			// Going to eat a cache miss anyways for pOtherEntity->GetIsPhysical when the mirrored ShouldFindImpacts happens, might
			//   as well early out on ground. 
			if(isOtherEntityPhysical)
			{
				if (pPhysical->IsUsingKinematicPhysics())
				{
					CPhysical* pOtherPhys = static_cast<CPhysical*>(pOtherEntity);
					if (pOtherPhys->IsUsingKinematicPhysics())
					{
						// Allow kinematic ped vs. kinematic ped collisions
						if(!pPhysical->GetIsTypePed() || !pOtherPhys->GetIsTypePed())
						{
							LOGGING_ONLY(if (pPickup) pPickup->ShouldFindImpactsFailed("Ped using kinematic physics"));
							return false;
						}
					}
				}

				// Check if we are colliding with our attachment
				// Do not use getAttachEntity since this will return NULL if we are detaching
				fwAttachmentEntityExtension *attachExt = pPhysical->GetAttachmentExtension();

				if(attachExt && attachExt->GetAttachParentForced())
				{
					if(attachExt->GetAttachParentForced()->GetCurrentPhysicsInst()==pOtherInst)
					{
						CVehicle *pParentVehicle = NULL;
						if( ((CPhysical*)attachExt->GetAttachParentForced())->GetIsTypeVehicle())
						{
							pParentVehicle = (CVehicle *) attachExt->GetAttachParentForced();
						}

						// physically attached entities will collide together ONLY if flag is set
						if(attachExt->GetAttachState()==ATTACH_STATE_PHYSICAL
							|| attachExt->GetAttachState()==ATTACH_STATE_RAGDOLL)
						{
							if(!attachExt->GetAttachFlag(ATTACH_FLAG_DO_PAIRED_COLL))
							{
								LOGGING_ONLY(if (pPickup) pPickup->ShouldFindImpactsFailed("Attached (physical/ragdoll)"));
								return false;
							}
						}
						else if((attachExt->GetAttachState() == ATTACH_STATE_DETACHING) && pParentVehicle && pParentVehicle->InheritsFromPlane() 
							&& pParentVehicle->GetStatus() == STATUS_WRECKED)
						{
							// Allow collision between wrecked plane and its pilot while he is detaching from the plane.
						}
						// all other attachment types don't collide with each other
						// in most case GetUsesCollision() should return false so won't get this far anyway
						else
						{
							LOGGING_ONLY(if (pPickup) pPickup->ShouldFindImpactsFailed("Attached"));
							return false;
						}
					}
				}
			}

			// do this check last to allow a chance to skip out earlier and let NoCollision reset
			if(pPhysical->GetNoCollisionEntity() && pPhysical->TestNoCollision(pOtherInst))
			{
				LOGGING_ONLY(if (pPickup) pPickup->ShouldFindImpactsFailed("TestNoCollision returned true"));
				return false;
			}
		}
	}

	LOGGING_ONLY(if (pPickup) pPickup->ShouldFindImpactsSuccess());

	return true;
}


bool phInstGta::ShouldFindImpactsDoorHelper(const phInst* pOtherInst)
{
	if(pOtherInst && pOtherInst->IsInLevel() && !CPhysics::GetLevel()->GetInstanceTypeFlag(pOtherInst->GetLevelIndex(), ArchetypeFlags::GTA_EXPLOSION_TYPE))
	{
		if(pOtherInst->GetInstFlag(FLAG_IS_DOOR))
		{
			return false;
		}
		else if(CPhysics::GetLevel()->IsFixed(pOtherInst->GetLevelIndex()))
		{
			return false;
		}
		else if(pOtherInst->GetUserData() && ((CEntity*)pOtherInst->GetUserData())->GetIsAnyFixedFlagSet())
		{
			return false;
		}
		else if(IsFragInst(pOtherInst) && !static_cast<const fragInst*>(pOtherInst)->GetBroken())
		{
			// If the other fragment is attached to the world don't collide with it. This should only be possible
			//   if art/script place these two objects together and we make the assumption that nothing should be 
			//   blocking the doors path by default, only if it gets moved into its way. 
			return false;
		}
	}

	return true;
}

#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE	// Disable now that we've disabled second surface and its replacement.
#if !HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
void ModifyImpactsForSecondSurface(phInst* thisInst, phContactIterator impacts, float fInterp)
#else
void ModifyImpactsForSecondSurface(phInst*, phContactIterator, float )
#endif
{
#if !HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
	if(fInterp != 0.0f)
	{
		impacts.Reset();

		for( ; !impacts.AtEnd(); ++impacts)
		{
			// Eh?  What purpose was this line serving?
			//if (&impacts.GetManifold() && &impacts.GetContact())
			{
				if (phInst* otherInst = impacts.GetOtherInstance())
				{
					phBound* otherBound = otherInst->GetArchetype()->GetBound();
					int otherBoundType = otherBound->GetType();
					if (otherBoundType == phBound::BVH)
					{
						phBoundBVH* otherBvhBound = reinterpret_cast<phBoundBVH*>(otherBound);

						if (otherBvhBound->GetHasSecondSurface())
						{
							int polyIndex = impacts.GetOtherElement();
							const phPolygon& poly = otherBvhBound->GetPolygon(polyIndex);

							phBoundGeometrySecondSurfacePolygonCalculator secondSurfaceCalculator;	
							Vector3 avSecondSurfacePolyVertices[POLY_MAX_VERTICES];
							Vector3 vSecondSurfacePolyNormal;
							float fSecondSurfacePolyArea;
							secondSurfaceCalculator.SetSecondSurfaceInterp(fInterp);
							secondSurfaceCalculator.ComputeSecondSurfacePolyVertsAndNormal(*otherBvhBound,poly,avSecondSurfacePolyVertices,vSecondSurfacePolyNormal,fSecondSurfacePolyArea);
							const Vec3V topSurfacePolyUnitNormal=VECTOR3_TO_VEC3V(otherBvhBound->GetPolygonUnitNormal(polyIndex));
							const Vec3V polyUnitNormal = RCC_VEC3V(vSecondSurfacePolyNormal);

							Vec3V vertex0 = VECTOR3_TO_VEC3V(otherBvhBound->GetCompressedVertex(poly.GetVertexIndex(0)));

							Mat34V lastMatrix = PHSIM->GetLastInstanceMatrix(thisInst);
							phBound* thisBound = thisInst->GetArchetype()->GetBound();
							if (thisBound->GetType() == phBound::COMPOSITE)
							{
								phBoundComposite* thisComposite = static_cast<phBoundComposite*>(thisBound);
								if (phBound* thisPartBound = thisComposite->GetBound(impacts.GetMyComponent()))
								{
									thisBound = thisPartBound;
								}

								Mat34V partLast = thisComposite->GetLastMatrix(impacts.GetMyComponent());
								Transform(lastMatrix, partLast);
							}

							Vec3V myLocal;
							if (thisInst == impacts.GetInstanceA())
							{
								myLocal = impacts.GetContact().GetLocalPosA();
							}
							else
							{
								myLocal = impacts.GetContact().GetLocalPosB();
							}
							Vec3V contactAtLast = Transform(lastMatrix, myLocal);
							Vec3V sweepDelta = contactAtLast - VECTOR3_TO_VEC3V(impacts.GetOtherPosition());
							ScalarV depthAtLast = Dot(sweepDelta, polyUnitNormal);
							Vec3V sweepTangent = depthAtLast * polyUnitNormal - sweepDelta;

							ScalarV distToBottomPlane=Dot(polyUnitNormal,VECTOR3_TO_VEC3V(impacts.GetOtherPosition()-avSecondSurfacePolyVertices[0]));
							Vec3V worldPosB=Add(VECTOR3_TO_VEC3V(impacts.GetOtherPosition()),Scale(polyUnitNormal,-distToBottomPlane));
							
							// Adjust along tangent so that wheels are above the correct point
							worldPosB += sweepTangent * distToBottomPlane * InvertSafe(depthAtLast, ScalarV(V_ZERO));

							Vec3V extents = thisBound->GetBoundingBoxMax() - thisBound->GetBoundingBoxMin();
							ScalarV maxSecondSurfaceDepth = Min(Min(SplatX(extents), SplatY(extents)), SplatZ(extents));

							ScalarV secondSurfaceDepthB=-Dot(topSurfacePolyUnitNormal,worldPosB-vertex0);
							secondSurfaceDepthB = Min(maxSecondSurfaceDepth, secondSurfaceDepthB);
							ScalarV zero(V_ZERO);
							BoolV cond=IsGreaterThan(secondSurfaceDepthB,zero);
							secondSurfaceDepthB=SelectFT(cond, zero, secondSurfaceDepthB);
							impacts.SetDepth(impacts.GetDepthV() - secondSurfaceDepthB);
							impacts.SetMyNormal(vSecondSurfacePolyNormal);
							impacts.SetOtherPositionDoNotUse(worldPosB);
							impacts.SetUserData(secondSurfaceDepthB);
						}
					}
				}
			}
		}
	}
#endif // !HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
}
#endif // HACK_GTA4_BOUND_GEOM_SECOND_SURFACE

void phInstGta::PreComputeImpacts(phContactIterator impacts)
{
	if(!impacts.GetMyInstance() || !PHLEVEL->LegitLevelIndex(impacts.GetMyInstance()->GetLevelIndex()))
	{
		// If our instance is now invalid, don't do anything
		return;
	}

#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
	ModifyImpactsForSecondSurface(this, impacts, GetBoundGeomIntersectionSecondSurfaceInterpolation());
#endif	// HACK_GTA4_BOUND_GEOM_SECOND_SURFACE

	phInst::PreComputeImpacts(impacts);

	if(GetUserData() && ((CEntity *)GetUserData())->GetIsPhysical())
	{
		CPhysical* pPhysical = (CPhysical*)GetUserData();
		if(pPhysical->DoPreComputeImpactsTest())
		{
			impacts.Reset();
			while(!impacts.AtEnd())
			{
				if(pPhysical->TestNoCollision(impacts.GetOtherInstance()))
				{
					impacts.DisableImpact();
				}
				impacts++;
			}
			impacts.Reset();
		}

		pPhysical->ProcessPreComputeImpacts(impacts);
	}
}


Vec3V_Out phInstGta::GetExternallyControlledVelocity () const
{
	Vec3V velocity(V_ZERO);

	CEntity* pEntity = (CEntity*)GetUserData();
	if(pEntity)
	{
		phInst* pInst = pEntity->GetCurrentPhysicsInst();
		phCollider* pCollider = CPhysics::GetSimulator()->GetCollider(pInst);
		if(pInst && pInst->IsInLevel() && pInst != this && pCollider)
		{
			velocity = pCollider->GetVelocity();
		}
		else
		{
			bool bInstActive = PHLEVEL->LegitLevelIndex(GetLevelIndex()) && PHLEVEL->IsActive(GetLevelIndex());
			if (!bInstActive && pEntity->GetIsPhysical())
			{
				CPhysical* pPhysical = (CPhysical*)pEntity;
				// ***HACK***
				// This logic should probably be contained within CPhysical::GetVelocity but it too late in the project to make such sweeping changes. 
				// Here (and the equivalent function in fragInstGta and fragInstNmGta) seemed to be the only place where this logic was required (although there are probably
				// other areas that would benefit) so the change was made here.
				if(pPhysical->GetIsAttached() && !pPhysical->GetIsAttachedToGround() && pPhysical->GetAttachParent() != NULL && static_cast<CEntity*>(pPhysical->GetAttachParent())->GetIsPhysical() &&
					(pPhysical->GetIsTypePed() || (pPhysical->GetIsTypeObject() && static_cast<CEntity*>(pPhysical->GetAttachParent())->GetIsTypeVehicle() && static_cast<CVehicle*>(pPhysical->GetAttachParent())->InheritsFromTrain())))
				{
					CPhysical* pAttachParent = static_cast<CPhysical*>(pPhysical->GetAttachParent());
					pPhysical->CalculateGroundVelocity(pAttachParent, pPhysical->GetTransform().GetPosition(), 0, velocity);

					// For peds climbing out of certain vehicles (such as the Titan) we need to set a valid velocity while they are attached
					// and inactive so that they will activate any peds standing waiting at the exit point.
					if(MagSquared(velocity).Getf() < SMALL_FLOAT && pAttachParent->GetIsTypeVehicle()
						&& static_cast<CVehicle*>(pAttachParent)->GetLayoutInfo()->GetClimbUpAfterOpenDoor()
						&& pPhysical->GetAttachState()==ATTACH_STATE_PED_EXIT_CAR)
					{
						velocity = VECTOR3_TO_VEC3V(pPhysical->GetAnimatedVelocity());
					}
				}
				else
				{
					velocity = RCC_VEC3V(pPhysical->GetVelocity());
				}
			}
		}
	}

	return velocity;
}


Vec3V_Out phInstGta::GetExternallyControlledAngVelocity () const
{
	Vector3 angVelocity(ORIGIN);

	CEntity* pEntity = (CEntity*)GetUserData();
	if(pEntity)
	{
		phInst* pInst = pEntity->GetCurrentPhysicsInst();
		phCollider* pCollider = CPhysics::GetSimulator()->GetCollider(pInst);
		if(pInst && pInst->IsInLevel() && pInst != this && pCollider)
		{
			angVelocity = RCC_VECTOR3(pCollider->GetAngVelocity());
		}
		else
		{
			bool bInstActive = PHLEVEL->LegitLevelIndex(GetLevelIndex()) && PHLEVEL->IsActive(GetLevelIndex());
			if (!bInstActive && pEntity->GetIsPhysical())
			{
				CPhysical* pPhysical = (CPhysical*)pEntity;
				angVelocity.Set(pPhysical->GetAngVelocity());
			}
		}
	}

	return RCC_VEC3V(angVelocity);
}


phInst *phInstGta::PrepareForActivation(phCollider **collider, phInst* otherInst, const phConstraintBase * constraint)
{
	CEntity* pThisEntity = CPhysics::GetEntityFromInst(this);
//	// FA: temp asserts to cache vehicle cache issues
//#if __ASSERT
//	if (pThisEntity && pThisEntity->GetIsTypeVehicle())
//	{
//		CVehicle* veh = (CVehicle*)pThisEntity;
//		Assert(!veh->GetIsInPopulationCache());
//	}
//
//	CEntity* other = CPhysics::GetEntityFromInst(otherInst);
//	if (other && other->GetIsTypeVehicle())
//	{
//		CVehicle* veh = (CVehicle*)other;
//		Assert(!veh->GetIsInPopulationCache());
//	}
//#endif

#if NAVMESH_EXPORT
	const bool bExportingCollision = CNavMeshDataExporter::IsExportingCollision();
#else
	const bool bExportingCollision = false;
#endif

	if(pThisEntity)
	{
		// Allow the object to activate and check for fixed collision next frame
		/*if(pThisEntity->GetIsPhysical())
		{
			// If we aren't fixed, check for nearby collision
			if(!pThisEntity->GetIsFixedUntilCollisionFlagSet())
			{
				static_cast<CPhysical*>(pThisEntity)->UpdateFixedWaitingForCollision(true);
				Assert(CPhysics::GetSimulator()->GetCollider(this)==NULL);
			}
		}*/
		// don't let this instance activate if the fixed flag is set
		if(pThisEntity->GetIsAnyFixedFlagSet())
		{
			return NULL;
		}
	}

	// flag can also be set on the phInst to not activate
	if(GetInstFlag(FLAG_NEVER_ACTIVATE))
		return NULL;

	// Need to know if someone is enabling the ped capsule after we've specifically disabled it.
	// Assertf(!pThisEntity->GetIsTypePed() || !((CPed*)pThisEntity)->GetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsule), "Should not enable the ped capsule whilst the DisablePedCapsule reset flag is set!");

	CEntity* pOtherEntity = NULL;

	if(otherInst && otherInst->GetUserData())
	{
		pOtherEntity = CPhysics::GetEntityFromInst(otherInst);

		if( pOtherEntity )
		{
			// Kinematic peds shouldn't activate when bumped by the player.
			if(pThisEntity->GetIsTypePed() && static_cast<CPed*>(pThisEntity)->IsUsingKinematicPhysics()
				&& pOtherEntity->GetIsTypePed() && static_cast<CPed*>(pOtherEntity)->IsPlayer())
			{
				return NULL;
			}

			if((pThisEntity->GetIsTypeVehicle() || pThisEntity->GetIsTypeObject()) && pOtherEntity->GetIsTypePed())
			{
				// we don't want to activate non-doors that pathfinding is trying to avoid
				if(!static_cast<CObject*>(pThisEntity)->IsADoor())
				{
					CPed* pPed = static_cast<CPed*>(pOtherEntity);

					// Don't activate if we're being hit by a ped that can't push us
					if (pThisEntity->GetIsTypeObject() && pPed->ShouldObjectBeImpossibleToPush(*pThisEntity, *this, PHSIM->GetCollider(this), 0))
					{
						return NULL;
					}
			
					// Don't allow animated NPCs to activate vehicles or objects.				
					if(!pPed->IsPlayer() && pPed->GetRagdollState()<RAGDOLL_STATE_ANIM_DRIVEN)
					{
						// If we are stuck and running into an object then allow it to be activated after a short amount of time
						if(pThisEntity->GetIsTypeObject() && pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= CStaticMovementScanner::ms_iStaticActivateObjectLimit)
						{

						}
						else if (CPathServerGta::ShouldPedAvoidObject(pPed, pThisEntity))
						{
							return NULL;
						}
					}	
				}
			}
		}
	}

	phInst *pReturn = phInst::PrepareForActivation(collider, otherInst, constraint); 

	if(pReturn)
	{
		CEntity* pEntity = CPhysics::GetEntityFromInst(this);

		if(pEntity)
		{
			if(pEntity->GetIsPhysical())
			{
				if(!((CDynamicEntity *) pEntity)->GetIsOnSceneUpdate())
					((CDynamicEntity *) pEntity)->AddToSceneUpdate();
			}

			if(pEntity->GetIsTypeObject())
			{
				CEntity* pOtherEntity = NULL; 

				if( otherInst && otherInst->GetUserData() )
				{
					pOtherEntity = CPhysics::GetEntityFromInst(otherInst);
				}

				if(!((CObject*)pEntity)->m_nObjectFlags.bHasBeenUprooted)
				{
					if(!bExportingCollision)
					{
						g_CollisionAudioEntity.ReportUproot(pEntity);
					}

					// Climbable objects; need deactivating when uprooted
					if(((CObject*)pEntity)->IsADoor()==false && pEntity->GetBaseModelInfo()->GetIsClimbableByAI())
					{
						Vector3 vCenter;
						((CObject*)pEntity)->GetBoundCentre(vCenter);
						CPathServer::DeferredDisableClimbAtPosition(vCenter, false, true); 
					}
				}

				((CObject*)pEntity)->m_nObjectFlags.bHasBeenUprooted = true;

				// need to store what woke this entity up if it is a world object
				((CObject*)pEntity)->ObjectHasBeenMoved(pOtherEntity);
			}
		}
	}

	return pReturn;
}

void phInstGta::OnActivate(phInst* otherInst)
{
	if(CEntity* pEntity = CPhysics::GetEntityFromInst(this))
	{
		pEntity->OnActivate(this,otherInst);
	}

	if(CExplosionManager::InsideExplosionUpdate())
	{
		CExplosionManager::RegisterExplosionActivation(GetLevelIndex());
	}
}

bool phInstGta::PrepareForDeactivation(bool UNUSED_PARAM(colliderManagedBySim), bool UNUSED_PARAM(forceDeactivate))
{
	// Don't allow peds to deactivate when standing near water unless they are attaching to something or are in
	// low LOD buoyancy mode.
	if(CEntity* pEntity = CPhysics::GetEntityFromInst(this))
	{
		// Don't prevent deactivation if we're fixed
		if(!pEntity->GetIsAnyFixedFlagSet())
		{
			// Don't prevent deactivation of peds who are in low physics lod based upon proximity to water.
			if(pEntity->GetIsTypePed() && (((CPed*)pEntity)->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics)) )
			{
				return true;
			}

			if(pEntity->GetIsPhysical())
			{
				CPhysical* pPhysical = static_cast<CPhysical*>(pEntity);

				// If we are attached, we want to allow deactivation.
				fwAttachmentEntityExtension* pAttachExt = pEntity->GetAttachmentExtension();
				bool bIsAttaching = pAttachExt && pAttachExt->GetIsAttached() &&
					(pAttachExt->IsAttachStateBasicDerived() || pAttachExt->GetAttachState()==ATTACH_STATE_WORLD);

				if(!bIsAttaching && pPhysical->m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate && pEntity->m_nFlags.bPossiblyTouchesWater
					&& !pPhysical->m_Buoyancy.m_buoyancyFlags.bLowLodBuoyancyMode)
				{
					return false;
				}
			}
		}
	}

	bool bReturn = true;//phInst::PrepareForDeactivation(colliderManagedBySim);
/*
	if(GetUserData() && bReturn)
	{
		if(((CEntity *)GetUserData())->GetIsPhysical())
		{
			// if we've just gone static, test if we still require ProcessControl
			if(!((CEntity *)GetUserData())->RequiresProcessControl())
			{
				((CPhysical *)GetUserData())->RemoveFromProcessControlList();
			}
		}

#if __DEV
		if(((CEntity *)GetUserData())->GetIsTypeVehicle())
			((CVehicle *)GetUserData())->m_pDebugColliderDO_NOT_USE = NULL;
#endif
	}
*/
	return bReturn;
}

void phInstGta::OnDeactivate()
{
	if(CEntity* pEntity = CPhysics::GetEntityFromInst(this))
	{
		pEntity->OnDeactivate(this);
	}
}


#if __DEV
void phInstGta::InvalidStateDump() const
{
	phInst::InvalidStateDump();
	InvalidStateDumpHelper(this);
}
#endif // __DEV

#if __BANK
extern bool gbVehicleImpulseModification;
#endif

// The directory in which xml data for clip sets is stored
const char * g_VehicleFragImpulseFunctionFile = "common:/data/VehicleFragImpulseFunction.xml";

VehicleFragImpulseFunction g_VehicleFragImpulseFunction;

void fragInstGta::LoadVehicleFragImpulseFunction()
{
 	PARSER.LoadObject(g_VehicleFragImpulseFunctionFile, "xml", g_VehicleFragImpulseFunction);
}

#if __BANK
void fragInstGta::SaveVehicleFragImpulseFunction()
{
 	PARSER.SaveObject(g_VehicleFragImpulseFunctionFile, "xml", &g_VehicleFragImpulseFunction);
}
#endif // __BANK

void fragInstGta::ReleaseVehicleFragImpulseFunction()
{
	g_VehicleFragImpulseFunction.m_Ranges.Reset();
}

void fragInstGta::AddVehicleFragImpulseFunctionWidgets(bkBank& BANK_ONLY(bank))
{
#if __BANK
	bank.PushGroup("Vehicle Frag Impulse Function");
 	PARSER.AddWidgets(bank, &g_VehicleFragImpulseFunction);
	bank.AddButton("Load", datCallback(CFA(fragInstGta::LoadVehicleFragImpulseFunction)));
	bank.AddButton("Save", datCallback(CFA(fragInstGta::SaveVehicleFragImpulseFunction)));
	bank.PopGroup();
#endif
}

ScalarV_Out fragInstGta::ModifyImpulseMag (int iMyComponent, int iOtherComponent, int iNumComponentImpulses, ScalarV_In fImpulseMagSquared, const phInst* pOtherInst) const
{
	//Set the default breaking impulse value to -1 
	//(this forces the impulse used to determine if the frag breaks to be equal to the contact impulse).
	ScalarV breakingImpulse(V_NEGONE);

	//If we've got no frag object then just return and use the raw impulse as the impulse for the breaking test.
	//We might still have a frag in the contact pair but if we do have a frag then it must be a vehicle or a 
	//ped in ragdoll mode.  We can ignore ragdolling peds because they don't break so are unaffected by 
	//frag tuning.  We can ignore vehicles too because their breaking strengths are tuned in code and we can 
	//happily rely on the contact impulse to determine if the vehicle breaks apart.  
	//Non-frags can obviously be ignored because logically they can never break apart.
	if(!pOtherInst)
	{
		return breakingImpulse;
	}

	//Get the flags that describe the types of object that can break/damage the frag.
	const int iOtherClassType = pOtherInst->GetClassType();
	
	fragPhysicsLOD& rMyPhysics = *GetTypePhysics();
	int iMyGroupIndex = rMyPhysics.GetAllChildren()[iMyComponent]->GetOwnerGroupPointerIndex();
	fragTypeGroup* pMyGroup = rMyPhysics.GetAllGroups()[iMyGroupIndex];

	//We've determined that we've got a frag object that will break apart if a breaking impulse
	//exceeds a strength set by frag tuning.  We want to compute a breaking impulse value
	//to help the prop artists more intuitively set breaking strengths.
	//Take a look at what the frag object has hit and work out a breaking impulse accordingly.
	switch(iOtherClassType)
	{
		case PH_INST_PED:
		{
			//Our frag object has hit a bound of an animated ped.
			//We could end up with multiple ped bounds colliding with the same frag component
			//so only consider the main bound of the ped because we don't want to count the 
			//ped multiple times when summing all the breaking impulses.
			const CEntity* pEntity=static_cast<const CEntity*>(pOtherInst->GetUserData());
			if(pEntity && 0==iOtherComponent)
			{
				//Our frag object has hit the main bound of an animated ped.
				//We're going to work out an approximate ped momentum based on the move speed of the ped.
				//Based on the move state of the ped and a ped mass of 50kg then we can write down
				//ped momenta based on a walk speed of 3.5mph, a run speed of 5mph, a sprint speed of 10mph.
				//In kgms^1 that equates to 75kgms^-1 for walking, 100kgms^-1 for running, 200kgms^-1 for sprinting.
				//We're going to ignore the contact normals and the speed of the frag object and see how we get on.
				Assertf(pEntity->GetIsTypePed(), "Non-frag inst should be ped");
				const CPed* pPed=static_cast<const CPed*>(pEntity);

				//float sprintingSpeed = 4.4704f; // 10.0 mph = 4.4704 mps
				float runningSpeed = 2.2352f;	  //  5.0 mph = 2.2352 mps
				float walkingSpeed = 1.5646f;	  //  3.5 mph = 1.5646 mps				
				float idleSpeed = 0.0f;			  //  in case we need an epsilon

				float velocitySquared = pPed->GetVelocity().Mag2();
				if( pPed->GetPedResetFlag(CPED_RESET_FLAG_ForceRunningSpeedForFragSmashing) )
				{
					// Higher impulse for custom breaking
					breakingImpulse=ScalarVFromF32(250);
				}
				else if (velocitySquared > runningSpeed*runningSpeed)
				{
					// Equivalent of sprinting
					breakingImpulse=ScalarVFromF32(200);
				}
				else if (velocitySquared > walkingSpeed*walkingSpeed)
				{
					// Equivalent of running
					breakingImpulse=ScalarVFromF32(100);
				}
				else if (velocitySquared > idleSpeed*idleSpeed)
				{
					// Equivalent of walking
					breakingImpulse=ScalarVFromF32(75);
				}
				else
				{
					breakingImpulse=ScalarV(V_ZERO);
				}

#if __BANK
				if(CPhysics::ms_bPrintBreakingImpulsesForPeds)
				{
					Displayf("Ped contact breakingImpulse %f", breakingImpulse.Getf());
				}
#endif
			}
			else
			{
				//Frag hit a bound other the than the main animated ped bound, or we have no entity.
				//Just ignore this collision.
				breakingImpulse=ScalarV(V_ZERO);
			}
			// Since the out-impulse isn't based on the in-impulse at all we need to make sure the impulse is distributed between the contacts with impulses. 
			// Otherwise the total impulse is dependent on the arbitrary number of contacts. 
			breakingImpulse = InvScaleSafe(breakingImpulse,ScalarVFromF32((float)iNumComponentImpulses),ScalarV(V_ZERO));

			breakingImpulse *= ScalarV(pMyGroup->GetPedScale());

			break;
		}

		case PH_INST_VEH:
		case PH_INST_FRAG_VEH:
		{
			// Only clamp breaking impulses for fragments still attached to the world. Since fixed root props are articulated they're marked as broken always. 
			bool isFixedRoot = GetCacheEntry() && GetCacheEntry()->GetHierInst()->body && GetCacheEntry()->GetHierInst()->body->RootIsFixed();
			if ((!GetBroken() || isFixedRoot) BANK_ONLY(&& gbVehicleImpulseModification))
			{
				atArray<VehicleFragImpulseRange>& impulseTable = g_VehicleFragImpulseFunction.m_Ranges;

				breakingImpulse=ScalarV(V_ZERO);

				ScalarV priorThreshold(V_ZERO);
				int index;
				for (index = 0; index < impulseTable.GetCount(); ++index)
				{
					ScalarV inputThreshold(impulseTable[index].m_InputThreshold);
					if (IsTrue(fImpulseMagSquared < inputThreshold * inputThreshold))
					{
						breakingImpulse = Ramp(Sqrt(fImpulseMagSquared),
							priorThreshold, inputThreshold,
							ScalarVFromF32(impulseTable[index].m_OutputMin), ScalarVFromF32(impulseTable[index].m_OutputMax));
						break;
					}
					priorThreshold = inputThreshold;
				}

				const CEntity* pEntity=static_cast<const CEntity*>(pOtherInst->GetUserData());
				Assertf(pEntity && pEntity->GetIsTypeVehicle(), "Non-frag inst should be vehicle");
				const CVehicle* pVehicle=static_cast<const CVehicle*>(pEntity);

				//also allow us to set the max value if the bActAsIfHighSpeedForFragSmashing vehicle reset flag has been set
				if (index == impulseTable.GetCount() && index > 0)
				{
					breakingImpulse = ScalarVFromF32(impulseTable[index - 1].m_OutputMax);
				}
				else if (pVehicle && pVehicle->m_nVehicleFlags.bActAsIfHighSpeedForFragSmashing && impulseTable.GetCount() > 0)
				{
					breakingImpulse = ScalarVFromF32(impulseTable[impulseTable.GetCount()-1].m_OutputMax);
				}

#if __BANK
				if(CPhysics::ms_bPrintBreakingImpulsesForVehicles && IsTrue(fImpulseMagSquared > ScalarV(V_ZERO)))
				{
					Displayf("Vehicle contact impulse %f : breakingImpulse %f", sqrtf(fImpulseMagSquared.Getf()), breakingImpulse.Getf());
				}
#endif
			}
			else
			{
				breakingImpulse = Sqrt(fImpulseMagSquared);
			}

			breakingImpulse *= ScalarV(pMyGroup->GetVehicleScale());

			break;
		}

		case PH_INST_FRAG_PED:
		{
			breakingImpulse = Scale(Sqrt(fImpulseMagSquared), ScalarV(pMyGroup->GetRagdollScale()));

			break;
		}

		case PH_INST_PROJECTILE:
		{
			breakingImpulse = Scale(Sqrt(fImpulseMagSquared), ScalarV(pMyGroup->GetWeaponScale()));

			break;
		}

		default:
		{
			breakingImpulse = Scale(Sqrt(fImpulseMagSquared), ScalarV(pMyGroup->GetObjectScale()));
		}
	}
	//Need to extend this as we see fit.

	return breakingImpulse;
}



/*
phInstGtaBoat::phInstGtaBoat (s32 nInstType)
: phInstGta(nInstType)
{
	m_LastMatrix.Identity();
}

phInstGtaBoat::phInstGtaBoat (datResource & rsc)
: phInstGta(rsc)
{
	m_LastMatrix.Identity();
}


const Matrix34& phInstGtaBoat::GetLastMatrix() const
{
return m_LastMatrix;
}

void phInstGtaBoat::SetLastMatrix (const Matrix34& last)
{
m_LastMatrix = last;
}
*/

/////////////////////////////////////////////////////
// fragInstGta
/////////////////////////////////////////////////////


fragInstGta::fragInstGta(s32 instType, const fragType* type, const Matrix34& matrix, u32 guid) : phfwFragInst(type, matrix, guid), m_classType(instType)
{
	m_nDontBreakCompFlags = 0;
}

fragInstGta::fragInstGta () : phfwFragInst(), m_classType(PH_INST_FRAG_GTA)
{
	// assume anything created with the default constructor is an object (from the fragment cache probably)
	m_nDontBreakCompFlags = 0;
}

fragInstGta::~fragInstGta()
{
}


void fragInstGta::SetDontBreakFlagAllChildren(s64 nChild)
{
	Assert(nChild >= 0 && nChild < GetTypePhysics()->GetNumChildren());

	const int MAX_NUM_GROUPS_TO_VISIT = 256;
	int groupsToVisit[MAX_NUM_GROUPS_TO_VISIT];
	int numGroupsToVisit = 1;
	groupsToVisit[0] = GetTypePhysics()->GetAllChildren()[nChild]->GetOwnerGroupPointerIndex();

	while (numGroupsToVisit > 0)
	{
		const fragTypeGroup* pGroup = GetTypePhysics()->GetAllGroups()[groupsToVisit[--numGroupsToVisit]];

		for (int childIndex = 0; childIndex < pGroup->GetNumChildren(); ++childIndex)
		{
			// set flag so this can't break off until we want it to.
			SetDontBreakFlag(BIT(static_cast<s64>(childIndex + pGroup->GetChildFragmentIndex())));
		}

		for (int groupIndex = 0; numGroupsToVisit < MAX_NUM_GROUPS_TO_VISIT && groupIndex < pGroup->GetNumChildGroups(); ++groupIndex)
		{
			// push this group onto the stack for visitation
			groupsToVisit[numGroupsToVisit++] = groupIndex + pGroup->GetChildGroupsPointersIndex();
		}
	}
}


void fragInstGta::ClearDontBreakFlagAllChildren(s64 nChild)
{
	Assert(nChild >= 0 && nChild < GetTypePhysics()->GetNumChildren());

	const int MAX_NUM_GROUPS_TO_VISIT = 256;
	int groupsToVisit[MAX_NUM_GROUPS_TO_VISIT];
	int numGroupsToVisit = 1;
	groupsToVisit[0] = GetTypePhysics()->GetAllChildren()[nChild]->GetOwnerGroupPointerIndex();

	while (numGroupsToVisit > 0)
	{
		const fragTypeGroup* pGroup = GetTypePhysics()->GetAllGroups()[groupsToVisit[--numGroupsToVisit]];

		for (int childIndex = 0; childIndex < pGroup->GetNumChildren(); ++childIndex)
		{
			// set flag so this can't break off until we want it to.
			ClearDontBreakFlag(BIT(static_cast<s64>(childIndex + pGroup->GetChildFragmentIndex())));
		}

		for (int groupIndex = 0; numGroupsToVisit < MAX_NUM_GROUPS_TO_VISIT && groupIndex < pGroup->GetNumChildGroups(); ++groupIndex)
		{
			// push this group onto the stack for visitation
			groupsToVisit[numGroupsToVisit++] = groupIndex + pGroup->GetChildGroupsPointersIndex();
		}
	}
}


// this function lets us skip out of collision tests
//
bool fragInstGta::ShouldFindImpacts(const phInst* pOtherInst) const
{
#if __DEV
	if(this==spDebugBreakCar && pOtherInst==spDebugBreakGround)
		Printf("Found combo");
	if(this==spDebugBreakCar && pOtherInst==spDebugBreakRagdoll)
		Printf("Found combo");
#endif

	if (!fragInst::ShouldFindImpacts(pOtherInst))
	{
		return false;
	}

	CEntity* pEntity = (CEntity*)GetUserData();
	if(pEntity && pEntity->GetIsPhysical())
	{
		CPhysical* pPhysical = static_cast<CPhysical*>(pEntity);

		const CEntity* pOtherEntity = CPhysics::GetEntityFromInst(pOtherInst);
		if(pOtherInst && GetUserData() == pOtherInst->GetUserData())
			return false;

		const bool isOtherEntityPhysical = pOtherEntity && pOtherEntity->GetIsPhysical();
		if(isOtherEntityPhysical)
		{
			const CPhysical* pOtherPhys = static_cast<const CPhysical*>(pOtherEntity);
			// Check if we are colliding with our attachment
			// Do not use getAttachEntity since this will return NULL if we are detaching
			fwAttachmentEntityExtension *attachExt = pPhysical->GetAttachmentExtension();

			if(attachExt && attachExt->GetAttachParentForced())
			{
				if(attachExt->GetAttachParentForced()->GetCurrentPhysicsInst()==pOtherInst)
				{
					// physically attached entities will collide together ONLY if flag is set
					if(attachExt->GetAttachState()==ATTACH_STATE_PHYSICAL
						|| attachExt->GetAttachState()==ATTACH_STATE_RAGDOLL)
					{
						if(!attachExt->GetAttachFlag(ATTACH_FLAG_DO_PAIRED_COLL))
							return false;
					}
					// all other attachment types don't collide with each other
					// in most case GetUsesCollision() should return false so won't get this far anyway
					else
						return false;
				}
			}

			if (pPhysical->IsUsingKinematicPhysics())
			{
				if (pOtherPhys->IsUsingKinematicPhysics())
				{
					return false;
				}
			}

			if(pPhysical->GetIsTypeVehicle()) 
			{
				CVehicle* pVehicle = static_cast<CVehicle*>(pPhysical);
				if(!pVehicle->ProcessShouldFindImpactsWithPhysical(pOtherInst))
				{
					return false;
				}

				if(pVehicle->GetVehicleType()==VEHICLE_TYPE_TRAIN)
				{
					// Don't let trains collide with other trains unless at least one of them is active (we might be
					// dropping a carriage onto another as in the "Big Score" heist mission in GTA5.
					if(pOtherInst && pOtherPhys && pOtherPhys->GetIsTypeVehicle() &&
						static_cast<const CVehicle*>(pOtherPhys)->GetVehicleType()==VEHICLE_TYPE_TRAIN)
					{
						u32 nLevelIndexThis = GetLevelIndex();
						u32 nLevelIndexOther = pOtherInst->GetLevelIndex();
						if((CPhysics::GetLevel()->IsInLevel(nLevelIndexThis) && CPhysics::GetLevel()->IsActive(nLevelIndexThis))
							|| (CPhysics::GetLevel()->IsInLevel(nLevelIndexOther) && CPhysics::GetLevel()->IsActive(nLevelIndexOther)) )
						{
							// Do nothing in this case in case some later check should force us to return false.
						}
						else
						{
							return false;
						}
					}
				}
			}
		}

		if(pPhysical->GetIsTypeObject())
		{
			// Don't let doors collide with other doors.
			// Note: calls into phInstGta to share code.
			if(GetInstFlag(phInstGta::FLAG_IS_DOOR) && !phInstGta::ShouldFindImpactsDoorHelper(pOtherInst))
			{
				return false;
			}
		}

		// in MP collisions between certain vehicles can be disabled (eg ghost vehicles in a race)
		if (NetworkInterface::IsGameInProgress() && pOtherEntity && NetworkInterface::AreInteractionsDisabledInMP(*pPhysical, *pOtherEntity))
		{
			NetworkInterface::RegisterDisabledCollisionInMP(*pPhysical, *pOtherEntity);
			return false;
		}

		Assert(pPhysical->GetIsTypeObject() || !GetInstFlag(phInstGta::FLAG_IS_DOOR));

		// do this last to give a chance to skip out, and let NoCollision be reset
		if(const fwEntity* pNoCollisionEntity = pPhysical->GetNoCollisionEntity())
		{
			if(pPhysical->TestNoCollision(pOtherInst))
			{
				return false;
			}

			// Optimization for vehicle explosions: If both objects aren't colliding with the parent vehicle then disable collision between them
			// As soon as one of them clears the vehicle they'll start colliding again
			if( pPhysical->GetIsTypeObject() && pOtherEntity && pOtherEntity->GetIsTypeObject())
			{
				const CObject* pObject = static_cast<const CObject*>(pPhysical);
				const CObject* pOtherObject = static_cast<const CObject*>(pOtherEntity);
				if(pObject->m_nObjectFlags.bVehiclePart && 
					pOtherObject->GetFragParent() == pNoCollisionEntity && 
					pOtherObject->GetNoCollisionEntity() == pNoCollisionEntity)
				{
					return false;
				}
			}
		}
	}

	return true;
}


void fragInstGta::PreComputeImpacts(phContactIterator impacts)
{
#if __DEV
	if(this==spDebugBreakCar)
		Printf("Found combo");
#endif

#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
	ModifyImpactsForSecondSurface(this, impacts, GetBoundGeomIntersectionSecondSurfaceInterpolation());
#endif	// HACK_GTA4_BOUND_GEOM_SECOND_SURFACE

	fragInst::PreComputeImpacts(impacts);

	if(!impacts.GetMyInstance() || !PHLEVEL->LegitLevelIndex(impacts.GetMyInstance()->GetLevelIndex()))
	{
		// If our instance is now invalid, don't do anything
		return;
	}

	if(GetUserData() && ((CEntity *)GetUserData())->GetIsPhysical())
	{
		CPhysical* pPhysical = (CPhysical*)GetUserData();
		if(pPhysical->DoPreComputeImpactsTest())
		{
			while(!impacts.AtEnd())
			{
				if(pPhysical->TestNoCollision(impacts.GetOtherInstance()))
				{
					impacts.DisableImpact();
				}
				impacts++;
			}
		}

		pPhysical->ProcessPreComputeImpacts(impacts);
	}
}

phInst *fragInstGta::PrepareForActivation(phCollider **collider, phInst* otherInst, const phConstraintBase * constraint)
{
//	// FA: temp asserts to cache vehicle cache issues
//#if __ASSERT
//	CEntity* pThisEntity = CPhysics::GetEntityFromInst(this);
//	if (pThisEntity && pThisEntity->GetIsTypeVehicle())
//	{
//		CVehicle* veh = (CVehicle*)pThisEntity;
//		Assert(!veh->GetIsInPopulationCache());
//	}
//
//	CEntity* other = CPhysics::GetEntityFromInst(otherInst);
//	if (other && other->GetIsTypeVehicle())
//	{
//		CVehicle* veh = (CVehicle*)other;
//		Assert(!veh->GetIsInPopulationCache());
//	}
//#endif

	CEntity* pEntity = CPhysics::GetEntityFromInst(this);

	// flag can also be set on the phInst to not activate
	if(GetInstFlag(FLAG_NEVER_ACTIVATE))
	{
		return NULL;
	}

	if(pEntity)
	{
		// Allow the instance to activate and become properly fixed next frame
		/*if(pEntity->GetIsPhysical())
		{
			// If we aren't fixed, check for nearby collision
			if(!pEntity->GetIsFixedUntilCollisionFlagSet())
			{
				static_cast<CPhysical*>(pEntity)->UpdateFixedWaitingForCollision(true);
				Assert(CPhysics::GetSimulator()->GetCollider(this)==NULL);
			}
		}*/

		// We still need to update the FixedWaitingForCollision flag for parked supper dummies, as they can be possibly push into the ground by other vehicles in one physics update
		if(pEntity->GetIsTypeVehicle() && static_cast<CVehicle*>(pEntity)->IsParkedSuperDummy())
		{
			// If we aren't fixed, check for nearby collision
			if(!pEntity->GetIsFixedUntilCollisionFlagSet())
			{
				static_cast<CPhysical*>(pEntity)->UpdateFixedWaitingForCollision(true);
				Assert(CPhysics::GetSimulator()->GetCollider(this)==NULL);
			}
		}

		// don't let this instance activate if the fixed flag is set
		if(pEntity->GetIsAnyFixedFlagSet())
		{
			return NULL;
		}

		// if this isn't the phInst that's controlling the object at this time, don't activate this one
		if(pEntity->GetCurrentPhysicsInst()!=this)
		{
			return NULL;
		}

		if(pEntity->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
			if(pVehicle->IsRunningCarRecording() && pVehicle->CanBeInactiveDuringRecording() 
				&& (CVehicle::sm_bForceRecordedVehicleToBeInactive || pVehicle->m_nVehicleFlags.bForceInactiveDuringPlayback)
				&& !pVehicle->m_nVehicleFlags.bSwitchToAiRecordingThisFrame)
			{
				return NULL;
			}

#if __ASSERT
			if(CVehicleAILodManager::ms_bFreezeParkedSuperDummyWhenCollisionsNotLoaded && pVehicle->IsParkedSuperDummy())
			{
				Assertf(pVehicle->IsCollisionLoadedAroundPosition(), "Trying to activate a parked supper dummy where gound collision is not loaded, activation aborted. vehicle 0x%p, fixedUntilCollisionFlag %x, bShouldFixIfNoCollision %x, AllowFreezeIfNoCollision %x, ShouldFixIfNoCollisionLoadedAroundPosition %x", 
					pVehicle, pVehicle->GetIsFixedUntilCollisionFlagSet(), pVehicle->m_nVehicleFlags.bShouldFixIfNoCollision, pVehicle->m_nPhysicalFlags.bAllowFreezeIfNoCollision, pVehicle->ShouldFixIfNoCollisionLoadedAroundPosition());
			}
#endif
		}

		if(otherInst && otherInst->GetUserData())
		{
			CEntity* pOtherEntity = CPhysics::GetEntityFromInst(otherInst);
			
			if(pOtherEntity->GetIsTypePed())
			{
				if(pEntity->GetIsTypeVehicle())
				{
					CPed* pPed = static_cast<CPed*>(pOtherEntity);
					// Don't allow animated NPCs to activate vehicles or frag objects.
					if(!pPed->IsPlayer() && pPed->GetRagdollState()<RAGDOLL_STATE_ANIM_DRIVEN)
					{
						// Check if the ped is anywhere near an open car door.
						if(pEntity->GetIsTypeVehicle())
						{
							CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
							bool bPedNearOpenDoor = false;
							for(int i = 0; i < pVehicle->GetNumDoors(); ++i)
							{
								CCarDoor* pDoor = pVehicle->GetDoor(i);
								if(!pDoor->GetIsClosed())
								{
									Assert(pVehicle->GetVehicleFragInst());
									Assert(pVehicle->GetVehicleFragInst()->GetArchetype());
									Assert(pVehicle->GetVehicleFragInst()->GetArchetype()->GetBound());
									Assert(pVehicle->GetVehicleFragInst()->GetArchetype()->GetBound()->GetType()==phBound::COMPOSITE);
									int nBoneIndex = pVehicle->GetBoneIndex(pDoor->GetHierarchyId());
									int nBoundIndex = -1;
									if(pVehicle->GetVehicleFragInst() && nBoneIndex>-1)
									{
										nBoundIndex = pVehicle->GetVehicleFragInst()->GetComponentFromBoneIndex(nBoneIndex);
									}
									if(nBoundIndex!=-1)
									{
										phBoundComposite* pCompBound = static_cast<phBoundComposite*>(pVehicle->GetVehicleFragInst()->GetArchetype()->GetBound());
										Mat34V matDoor = pVehicle->GetMatrix();
										Transform(matDoor, pCompBound->GetCurrentMatrix(nBoundIndex));
										phBound* pDoorBound = pCompBound->GetBound(nBoundIndex);
										if(pDoorBound)
										{
											Vec3V vDoorCentroidPos = pDoorBound->GetWorldCentroid(matDoor);
											float fDoorCentroidRadius = pDoorBound->GetRadiusAroundCentroid();
											float fPedCapsuleRadius = pPed->GetCapsuleInfo()->GetHalfWidth();
											//grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(vDoorCentroidPos), fDoorCentroidRadius, Color_yellow, false);
											if(MagSquared(pPed->GetTransform().GetPosition()-vDoorCentroidPos).Getf()
												< rage::square(fDoorCentroidRadius+fPedCapsuleRadius))
											{
												bPedNearOpenDoor = true;
											}
										}
									}
								}
							}
							if(!bPedNearOpenDoor)
							{
								return NULL;
							}
						}
					}
				}
				else if(pEntity->GetIsTypeObject())
				{
					// Allow peds to still activate doors.
					if(!static_cast<CObject*>(pEntity)->IsADoor())
					{
						CPed* pPed = static_cast<CPed*>(pOtherEntity);

						// Don't activate if we're being hit by a ped that can't push us
						if (!GetType()->GetHasAnyArticulatedParts() && pPed->ShouldObjectBeImpossibleToPush(*pEntity, *this, PHSIM->GetCollider(this), 0))
						{
							return NULL;
						}

						// Don't allow animated NPCs to activate vehicles or frag objects.
						if(!pPed->IsPlayer() && pPed->GetRagdollState()<RAGDOLL_STATE_ANIM_DRIVEN)
						{
							// If we are stuck and running into an object then allow it to be activated after a short amount of time
							if(pPed->GetPedIntelligence()->GetEventScanner()->GetStaticMovementScanner().GetStaticCounter() >= CStaticMovementScanner::ms_iStaticActivateObjectLimit)
							{

							}
							else if (CPathServerGta::ShouldPedAvoidObject(pPed, pEntity))
							{
								return NULL;
							}
						}
					}
				}
			} // If(other object == ped).
		}
	}

#if 0
	if(impactList && GetType()->GetMinMoveForce() > 0.0f)
	{
		phInst *pOtherInst = impactList->GetInstanceA();
		if(pOtherInst==this || pOtherInst==NULL)
			pOtherInst = impactList->GetInstanceB();

		if(pOtherInst && pOtherInst->GetClassType()==PH_INST_PED)
		{
			return NULL;
		}
	}
#endif

	// ok to go ahead and activate
	phInst *pReturn = fragInst::PrepareForActivation(collider, otherInst, constraint); 

	if(pReturn && pEntity)
	{
		if(pEntity->GetIsPhysical())
		{
			if(!((CDynamicEntity *) pEntity)->GetIsOnSceneUpdate())
				((CDynamicEntity *) pEntity)->AddToSceneUpdate();
		}

		if(pEntity->GetIsTypeObject())
		{

			CEntity * pOtherEntity = NULL;
			if(otherInst)
			{
				pOtherEntity = CPhysics::GetEntityFromInst(otherInst);
			}

			if(!((CObject*)pEntity)->m_nObjectFlags.bHasBeenUprooted)
			{
				g_CollisionAudioEntity.ReportUproot(pEntity);

				// Climbable objects; need deactivating when uprooted
				if (((CObject*)pEntity)->IsADoor()==false && pEntity->GetBaseModelInfo()->GetIsClimbableByAI())
				{
					Vector3 vCenter;
					((CObject*)pEntity)->GetBoundCentre(vCenter);
					CPathServer::DeferredDisableClimbAtPosition(vCenter, false, true); 
				}
			}

			((CObject*)pEntity)->m_nObjectFlags.bHasBeenUprooted = true;

			// need to store what woke this entity up if it is a world object
			((CObject*)pEntity)->ObjectHasBeenMoved(pOtherEntity);

			if (*collider && (*collider)->IsArticulated())
			{
				// Give a smaller than default internal velocity limit
				(*collider)->GetSleep()->SetInternalMotionTolerance2Sum(0.002f);
			}
		}

		if(pEntity->GetIsTypeVehicle())
		{
			if (*collider && (*collider)->IsArticulated())
			{
				static bool sbTestArticulatedLargeRoot = true;
				static_cast<phArticulatedCollider*>(*collider)->SetArticulatedLargeRoot(sbTestArticulatedLargeRoot);
			}
		}
	}

	return pReturn;

}


bool fragInstGta::PrepareForDeactivation(bool colliderManagedBySim, bool forceDeactivate)
{
	bool bReturn = fragInst::PrepareForDeactivation(colliderManagedBySim, forceDeactivate);

	if(CEntity* pEntity = CPhysics::GetEntityFromInst(this))
	{
		if(!pEntity->GetIsAnyFixedFlagSet())
		{
			if(pEntity->GetIsTypeVehicle())
			{
				CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
				// B* 1089571: Don't refuse to deactivate vehicles owned by cutscenes, even if they are touching water.
				if(pVehicle->GetOwnedBy()!=ENTITY_OWNEDBY_CUTSCENE)
				{
					// Vehicles in water need to be kept awake (other than low-lod boats).
					bool bUsingLowLodAnchorMode = false;
					if(pVehicle->InheritsFromBoat())
					{
						bUsingLowLodAnchorMode = static_cast<CBoat*>(pVehicle)->GetAnchorHelper().UsingLowLodMode();
					}
					else if(pVehicle->InheritsFromSubmarine())
					{
						CSubmarineHandling* pSubHandling = pVehicle->GetSubHandling();
						bUsingLowLodAnchorMode = pSubHandling->GetAnchorHelper().UsingLowLodMode();
					}
					else if(pVehicle->InheritsFromAmphibiousAutomobile())
					{
						bUsingLowLodAnchorMode = static_cast<CAmphibiousAutomobile*>(pVehicle)->GetAnchorHelper().UsingLowLodMode();
					}

					if(!pVehicle->GetIsAquatic() || !bUsingLowLodAnchorMode)
					{
						// Keep awake if partially submerged in water...
						if(pVehicle->m_Buoyancy.GetStatus()==PARTIALLY_IN_WATER)
						{
							return false;
						}
						// ... or fully submerged and above the submerge sleep threshold.
						else if(pVehicle->m_Buoyancy.GetStatus()==FULLY_IN_WATER)
						{
							if(pVehicle->GetTransform().GetPosition().GetZf() > CVehicle::ms_fVehicleSubmergeSleepGlobalZ)
							{
								return false;
							}
						}
					}
				}
			}
			else if(pEntity->GetIsTypeObject())
			{
				// If this is an object, reset the flag which indicates it is attached to the handler vehicle.
				static_cast<CObject*>(pEntity)->SetAttachedToHandler(false);
			}
		}
	}

	return bReturn;
}

void fragInstGta::OnActivate(phInst* otherInst)
{
	if(CEntity* pEntity = CPhysics::GetEntityFromInst(this))
	{
		pEntity->OnActivate(this,otherInst);
	}

	if(CExplosionManager::InsideExplosionUpdate())
	{
		CExplosionManager::RegisterExplosionActivation(GetLevelIndex());
	}
}

void fragInstGta::OnDeactivate()
{
	if(CEntity* pEntity = CPhysics::GetEntityFromInst(this))
	{
		pEntity->OnDeactivate(this);
	}
}

void fragInstGta::PrepareBrokenInst(fragInst *brokenInst) const
{
	// temporarily set user data so inst knows it's parent when it's inserted
	brokenInst->SetUserData((void*)this);
	brokenInst->SetInstFlag(phInstGta::FLAG_USERDATA_PARENT_INST, true);
}

void fragInstGta::PartsBrokeOff(const ComponentBits& brokenGroups, phInst* newInst)
{
	// Safely get and cast child/parent pointers. I don't think these can ever fail.
	CEntity* pParentEntity = CPhysics::GetEntityFromInst(this);
	if(!AssertVerify(pParentEntity && pParentEntity->GetIsPhysical()))
	{
		return;
	}
	CPhysical* pParentPhysical = static_cast<CPhysical*>(pParentEntity);

	CPhysical* pNewPhysical = NULL;
	fragInst* newFragInst = NULL;
	if(newInst)
	{
		CEntity* pNewEntity = CPhysics::GetEntityFromInst(newInst);
		if(!AssertVerify(pNewEntity && pNewEntity->GetIsPhysical() && IsFragInst(newInst)))
		{
			return;
		}
		pNewPhysical = static_cast<CPhysical*>(pNewEntity);
		newFragInst = static_cast<fragInst*>(newInst);
	}
	
	// If any part breaks off a frag which has a dynamic cover bound, just disable this bound.
	if(pParentEntity->GetBaseModelInfo() && pParentEntity->GetBaseModelInfo()->TestAttribute(MODEL_ATTRIBUTE_HAS_DYNAMIC_COVER_BOUND))
	{
		if(!pParentPhysical->CoverBoundsAlreadyRemoved())
		{
			pParentEntity->GetBaseModelInfo()->ClearDynamicCoverCollisionBounds(pParentEntity->GetPhysArch());
			pParentPhysical->SetCoverBoundsAlreadyRemoved(true);
		}

		if(pNewPhysical && !pNewPhysical->CoverBoundsAlreadyRemoved())
		{
			pNewPhysical->GetBaseModelInfo()->ClearDynamicCoverCollisionBounds(pParentEntity->GetPhysArch());
			pNewPhysical->SetCoverBoundsAlreadyRemoved(true);
		}
	}

	// Iterate over all breaking fragGroups and fragChildren/components
	const fragPhysicsLOD* physicsLOD = GetTypePhysics();
	u8 firstBrokenGroupIndex = 0xFF;
	for(u8 groupIndex=0; groupIndex < physicsLOD->GetNumChildGroups(); groupIndex++)
	{
		if(brokenGroups.IsSet(groupIndex))
		{
			const fragTypeGroup& group = *physicsLOD->GetGroup(groupIndex);
			if(firstBrokenGroupIndex == 0xFF)
			{
				// Cache off the first broken group
				firstBrokenGroupIndex = groupIndex;
			}

			// Iterate over all the fragChildren/Components in the group
			for (int groupChildIndex = 0; groupChildIndex < group.GetNumChildren(); groupChildIndex++)
			{
				int childIndex = groupChildIndex + group.GetChildFragmentIndex();

				if(pParentEntity->GetIsTypeVehicle())
				{
					CVehicle* pParentVehicle = static_cast<CVehicle*>(pParentEntity);

					// Mark doors as broken off
					for(int doorIndex = 0; doorIndex < pParentVehicle->GetNumDoors(); ++doorIndex)
					{
						CCarDoor* pDoor = pParentVehicle->GetDoor(doorIndex);
						if(pDoor->GetFragChild() == childIndex)
						{
#if __BANK
							vehicledoorDebugf3("fragInstGta::PartsBrokeOff() CCarDoor::IS_BROKEN_OFF [%s][%s][%p]: Scriptindex - %i", AILogging::GetDynamicEntityNameSafe(pParentVehicle), pParentVehicle->GetModelName(), pParentVehicle,
								pDoor->GetHierarchyId());
							if (Channel_vehicledoor.FileLevel >= DIAG_SEVERITY_DEBUG3)
							{
								sysStack::PrintStackTrace();
							}
#endif
							pDoor->SetFlag(CCarDoor::IS_BROKEN_OFF);
						}
					}

					for(int wheelIndex = 0; wheelIndex < pParentVehicle->GetNumWheels(); ++wheelIndex)
					{
						CWheel* pWheel = pParentVehicle->GetWheel(wheelIndex);
						if( pWheel->GetFragChild() == childIndex &&
                            pParentVehicle->GetVehicleModelInfo()->GetVehicleClass() != VC_OPEN_WHEEL )
						{
							// Additionally, to stop brake calipers from drawing we have to zero the hub matrix
							pWheel->GetConfigFlags().SetFlag(WCF_DONT_RENDER_HUB);
						}
					}

					if(pNewPhysical)
					{
						// Update type/include flags of this component
						pParentVehicle->UpdateCollisionOnPartBrokenOff(childIndex, newFragInst);

						// Let glass know that this part broke off
						g_vehicleGlassMan.UpdateBrokenPart(pParentEntity, pNewPhysical, childIndex);

						if(pParentVehicle->InheritsFromPlane())
						{
							// Iterate over children of any parts which break off planes and reduce their buoyancy force so that
							// we don't get wings, rudders, etc. which float in weird ways.
							CPlane* pPlane = static_cast<CPlane*>(pParentEntity);
							pPlane->GetAircraftDamage().UpdateBuoyancyOnPartBreakingOff(pPlane, childIndex);
						}
					}
				}

				if(pNewPhysical)
				{
					// Transfer attachments
					CPhysical* pAttachChild = static_cast<CPhysical*>(pParentPhysical->GetChildAttachment());
					while(pAttachChild)
					{
						// Get the next physical attached to this object now since we might transfer the current one
						CPhysical* pNextAttachChild = static_cast<CPhysical*>(pAttachChild->GetSiblingAttachment());
						if(GetControllingComponentFromBoneIndex(pAttachChild->GetAttachBone()) == childIndex)
						{
							// If the component controlling this attachment's bone got broken, move the attachment.
							pAttachChild->AttachParentPartBrokeOff(pNewPhysical);
						}
						pAttachChild = pNextAttachChild;
					}
				}
			}
		}
	}
	Assert(firstBrokenGroupIndex != 0xFF);


	// this group (group i) broke off its parent - do audio and ptfx

	// find the bone tag (we're looking at groups here so need to get first child of group to get to bone)
	const fragTypeGroup& firstBrokenGroup = *physicsLOD->GetGroup(firstBrokenGroupIndex);
	u8 firstBreakingChildIndex = firstBrokenGroup.GetChildFragmentIndex();
	u16 childBoneID = physicsLOD->GetChild(firstBreakingChildIndex)->GetBoneID();
	if (pParentEntity->GetIsTypeVehicle())
	{
		// NOTE: I think this might need to be called per-component but I'm not sure. Keeping old behavior for now.
		CVehicle* pParentVehicle = static_cast<CVehicle*>(pParentEntity);
		pParentVehicle->GetVehicleDamage()->UpdateLightsOnBreakOff(firstBreakingChildIndex);
	}

	if(pNewPhysical)
	{
		if(CExplosionManager::InsideExplosionUpdate())
		{
			CExplosionManager::RegisterExplosionBreak();
		}

		// If the entity associated with the current frag is a CPhysical (likely), then we need to check
		// if it is maintaining any attachments on this group and let it know that its attachment parent will now have
		// a different physics inst.
		if(pParentEntity && pParentEntity->GetIsPhysical())
		{
			CPhysical* pParentPhysical = static_cast<CPhysical*>(pParentEntity);
			physicsAssertf(pParentPhysical, "pParentEntity: 0x%p.", pParentEntity);

			// If the object which is breaking is constrained to a forklift, just break the constraint.
			if(CPhysics::GetConstraintManager()->IsConstrained(this))
			{
				CPhysical* pAttachParent = static_cast<CPhysical*>(pParentPhysical->GetAttachParentForced());
				if(pAttachParent && pAttachParent->GetIsTypeVehicle())
				{
					CVehicle* pVehicle = static_cast<CVehicle*>(pAttachParent);

					// See if we have a pair of forks.
					for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); ++i)
					{
						CVehicleGadget* pVehicleGadget = pVehicle->GetVehicleGadget(i);

						if(pVehicleGadget->GetType() == VGT_FORKS)
						{
							CVehicleGadgetForks* pForks = static_cast<CVehicleGadgetForks*>(pVehicleGadget);

							pForks->DetachPalletImmediatelyAndCleanUp(pVehicle);
						}
					}
				}
			}
		}

		// get the parent boneID as well
		u8 firstParent = firstBrokenGroup.GetParentGroupPointerIndex();
		if(firstParent != 0xFF)
		{
			u16 parentBoneID = GetTypePhysics()->GetChild(GetTypePhysics()->GetGroup(firstParent)->GetChildFragmentIndex())->GetBoneID();

			// do any vfx
			pParentEntity->ProcessFxFragmentBreak(childBoneID, false, parentBoneID);
		}

		// do any vfx
		pNewPhysical->ProcessFxFragmentBreak(childBoneID, true);

		g_CollisionAudioEntity.ReportFragmentBreak(newInst, firstBreakingChildIndex);
	}

	// Update the parent object as it is likely smaller or even removed 
	if(pParentEntity->GetIsTypeObject())
	{
		CObject* pObject = static_cast<CObject*>(pParentEntity);
		if(pObject->m_nObjectFlags.bBoundsNeedRecalculatingForNavigation)
		{
			if(pObject->IsInPathServer())
			{
				CPathServerGta::UpdateDynamicObject(pObject, true);
			}
		}
	}

	// B*1760493: Force the TACO van roof sign to have a greater mass so it cannot be pushed around by peds.
	if(pParentEntity->GetIsTypeVehicle() && pNewPhysical)
	{	
		if(pNewPhysical->GetModelIndex() == MI_CAR_TACO)
		{
			pNewPhysical->SetMass(200.0f);			
		}
	}
}

void fragInstGta::PartsRestored(const ComponentBits& restoredGroups)
{
	CEntity* pEntity = CPhysics::GetEntityFromInst(this);
	if(!AssertVerify(pEntity && pEntity->GetIsPhysical()))
	{
		return;
	}
	CPhysical* pPhysical = static_cast<CPhysical*>(pEntity);

	// Loop over each restored group
	const fragPhysicsLOD* physicsLOD = GetTypePhysics();
	for(int groupIndex = 0; groupIndex < physicsLOD->GetNumChildGroups(); ++groupIndex)
	{
		if(restoredGroups.IsSet(groupIndex))
		{
			if(pPhysical->GetIsTypeVehicle())
			{
				CVehicle* pVehicle = static_cast<CVehicle*>(pPhysical);

				// Update the cached broken off flag of the door
				for(int doorIndex = 0; doorIndex < pVehicle->GetNumDoors(); ++doorIndex)
				{
					CCarDoor* pDoor = pVehicle->GetDoor(doorIndex);
					if(pDoor->GetFragGroup() == groupIndex)
					{
						pDoor->ClearFlag(CCarDoor::IS_BROKEN_OFF);
					}
				}

				// Fix any wheels that are being restored
				for(int wheelIndex = 0; wheelIndex < pVehicle->GetNumWheels(); ++wheelIndex)
				{
					CWheel* pWheel = pVehicle->GetWheel(wheelIndex);
					int wheelChildIndex = pWheel->GetFragChild();
					if(wheelChildIndex >= 0 && (int)physicsLOD->GetChild(wheelChildIndex)->GetOwnerGroupPointerIndex() == groupIndex)
					{
						pWheel->ResetDamage();
					}
				}
			}
		}
	}
}

void fragInstGta::CollisionDamage(const Collision& msg, phContactIterator *impacts)
{
	CEntity* pEntity = CPhysics::GetEntityFromInst(this);
	// can't break or damage frozen/fixed fragments in any way
	if(!pEntity)
		return;

	if(!IsInLevel())
		return;

	int component = msg.component;
	Assert(component < GetTypePhysics()->GetNumChildren());
	// this results in a fatal crash, try and avoid it by jumping out
	if(component >= GetTypePhysics()->GetNumChildren())
		return;

	fragTypeChild* child = GetTypePhysics()->GetAllChildren()[component];
	int groupIndex = child->GetOwnerGroupPointerIndex();
	fragTypeGroup* breakGroup = GetTypePhysics()->GetAllGroups()[groupIndex];

	float fOldHealth = breakGroup->GetDamageHealth();
	bool bWasBroken = false;

	Matrix34 matChild;
	u32 boneIndex = GetType()->GetBoneIndexFromID(child->GetBoneID());
	bool bGotMatrix = false;

	if(GetCached())
	{
		fOldHealth = GetCacheEntry()->GetHierInst()->groupInsts[groupIndex].damageHealth;
		bWasBroken = GetCacheEntry()->GetHierInst()->groupBroken->IsSet(groupIndex);
		GetSkeleton()->GetGlobalMtx(boneIndex, RC_MAT34V(matChild));
		bGotMatrix = true;
	}

	fragInst::CollisionDamage(msg, impacts);

	// if it's not cached then it can't have broken!
	if(GetCached())
	{
		float fHealth = GetCacheEntry()->GetHierInst()->groupInsts[groupIndex].damageHealth;
		bool bIsBroken = GetCacheEntry()->GetHierInst()->groupBroken->IsSet(groupIndex);

		// didn't get the original matrix, because it wasn't cached before, so safe to calculate it using the shared skeleton
		if(!bGotMatrix && ((bIsBroken && !bWasBroken) || (fHealth < 0.0f && fOldHealth >= 0.0f)))
		{
			GetType()->GetSkeletonData().ComputeGlobalTransform(boneIndex, pEntity->GetTransform().GetMatrix(), RC_MAT34V(matChild));
		}

		if(bIsBroken)
		{
			if(!bWasBroken)
			{
				// component just disappeared - do audio and ptfx

				// set damaged flag so scripts can tell this object has been damaged
				if(pEntity->GetIsTypeObject())
				{
					pEntity->m_nFlags.bRenderDamaged = true;

					// If we have just broken a pane of glass, notify the pathserver which can then
					// remove the dynamic object for this entity - allowing peds to navigate through.
					if(((CObject*)pEntity)->m_nDEflags.bIsBreakableGlass && pEntity->IsInPathServer())
					{
						if( !CPathServerGta::SpecialCaseDontRemoveSmashedGlass(((CObject*)pEntity)))
						{
							CPathServerGta::MaybeRemoveDynamicObject(pEntity);
						}
					}
				}

				// find the bonetag
				fragTypeChild** ppChildren = GetTypePhysics()->GetAllChildren();
				Assert(component>=0);
				Assert(component<GetTypePhysics()->GetNumChildren());
				u32 boneTag = ppChildren[component]->GetBoneID();

				// stop any current vfx on this entity as it's now going to destroy
//				g_ptFx.RemoveEntityPtFx(pEntity);

				// do any vfx
				pEntity->ProcessFxFragmentDestroy(boneTag, matChild);

				g_CollisionAudioEntity.ReportFragmentBreak(this, component);
				
				// Object might not exist in physics world anymore - 
				// check for this case and destroy the object if necessary
				// (fragInst::CollisionDamage may have set all the bounds to NULL)
				if(pEntity)
				{
					fragCacheEntry* pCacheEntry = GetCacheEntry();
					const phBoundComposite* pCompositeBound = static_cast<phBoundComposite*>(pCacheEntry->GetBound());
					bool bFoundValidBound = false;
					for(int boundIndex = 0; boundIndex < pCompositeBound->GetNumBounds() && !bFoundValidBound; boundIndex++)
					{
						if(pCompositeBound->GetBound(boundIndex))
							bFoundValidBound = true;
					}

					bool bHasGlass = false;
					if(fragHierarchyInst *pHierInst = pCacheEntry->GetHierInst())
					{
						bHasGlass = pHierInst->numGlassInfos > 0;
					}


					CObject *pObject = pEntity->GetIsTypeObject() ? static_cast<CObject *>(pEntity) : NULL;
					bool bOkayToRemove = true;

					if (pObject)
					{
						if (ENTITY_OWNEDBY_SCRIPT == pObject->GetOwnedBy())
						{
							bOkayToRemove = false;	//	Only scripts should remove mission peds, cars, objects
						}
					}

					if (bOkayToRemove)
					{
						netObject *pNetObj = NetworkUtils::GetNetworkObjectFromEntity(pEntity);

						if (pNetObj && (pNetObj->IsClone() || !pNetObj->CanDelete()))
						{
							//ambient props can be deleted as clones as their network object persists
							if (!pObject || !CNetObjObject::IsAmbientObjectType(pObject->GetOwnedBy()))		
							{
								bOkayToRemove = false;
							}
						}
					}

					//Add damage entity event
					if (pEntity->GetIsPhysical() && TriggerEventEntityDestroyed(static_cast<CPhysical*>(pEntity)))
					{
						GetEventGlobalGroup()->Add(CEventEntityDestroyed(static_cast<CPhysical*>(pEntity)));
					}

					if(!bFoundValidBound
					&& !bHasGlass
					&& !pEntity->GetIsAnyManagedProcFlagSet()		// Procedural objects will clean up themselves
					&& bOkayToRemove)
					{
						pEntity->FlagToDestroyWhenNextProcessed();
					}
				}
			}
		}
		else if(fHealth < 0.0f && fOldHealth >= 0.0f)
		{
			// component just switched to damage model - do audio and ptfx

			//Add damage entity event
			if (pEntity->GetIsPhysical() && TriggerEventEntityDestroyed(static_cast<CPhysical*>(pEntity)))
			{
				GetEventGlobalGroup()->Add(CEventEntityDestroyed(static_cast<CPhysical*>(pEntity)));
			}

			// set damaged flag so scripts can tell this object has been damaged
			if(pEntity->GetIsTypeObject())
			{
				pEntity->m_nFlags.bRenderDamaged = true;
			}

			// find the bonetag
			fragTypeChild** ppChildren = GetTypePhysics()->GetAllChildren();
			Assert(component>=0);
			Assert(component<GetTypePhysics()->GetNumChildren());
			u32 boneTag = ppChildren[component]->GetBoneID();

			// stop any current vfx on this entity as it's now going to destroy
//			g_ptFx.RemoveEntityPtFx(pEntity);

			// do any vfx
			pEntity->ProcessFxFragmentDestroy(boneTag, matChild);

			// remove any decals from this entity
			g_decalMan.Remove(pEntity, boneIndex);
		}
	}
}

//Called locally on a frag to detect if the destroy event should be triggered.
bool  fragInstGta::TriggerEventEntityDestroyed(CPhysical* entity)
{
	bool addDestroyEvent = true;

	if (entity && NetworkInterface::IsGameInProgress())
	{
		bool addDestroyEvent = false;

		//If it has a weapon damage entity then the event should be triggered.
		//  - Child Object won't need this and wont have a valid Weapon Damage Entity. And this avoids
		//     triggering the event for the child object's.
		CEntity* damageEntity = entity->GetWeaponDamageEntity();

		if (damageEntity)
		{
			//Last Damage Entity is Available.
			CPed* ped = NULL;

			if (damageEntity->GetIsTypePed())
			{
				ped = static_cast<CPed *>(damageEntity);
			}
			else if (damageEntity->GetIsTypeVehicle())
			{
				CVehicle* vehicle = static_cast<CVehicle *>(damageEntity);
				ped = vehicle->GetDriver();
			}

			//Only trigger the event if it was damaged by the local player.
			if (ped && ped->IsLocalPlayer())
			{
				addDestroyEvent = true;
				ASSERT_ONLY( gnetDebug1("Add destroy event for %s", entity->GetDebugName()); )
			}
		}
	}

	return addDestroyEvent;
}

bool fragInstGta::IsBreakable(phInst* otherInst) const 
{
	const CEntity* pEntity = CPhysics::GetEntityFromInst(this);

	// B*2040313 - Shooting range rail gun targets no longer fragging.
	// This should probably be done but we'll disable it for the time being
	// as it is no worse than last gen and it is causing a bug.
	//// Don't let the instance be broken apart if the instance has been flagged to never activated.
	//if(GetInstFlag(FLAG_NEVER_ACTIVATE))
	//{
	//	return false;
	//}

	if(pEntity)
	{		
		// Don't let this instance be broken apart if any fixed flags are set.
		if(pEntity->GetIsAnyFixedFlagSet())
		{
			return false;
		}
	}

	return fragInst::IsBreakable(otherInst);
}

bool fragInstGta::FindBreakStrength(const Vector3* componentImpulses, const Vector4* componentPositions, float* breakStrength, phBreakData* breakData) const
{
	// want the option to override breaking on specific components (car doors)
	if(m_nDontBreakCompFlags)
	{
		static Vector3 breakingImpulses[phInstBreakable::MAX_NUM_BREAKABLE_COMPONENTS];
		for(s64 i=0; i<GetTypePhysics()->GetNumChildren(); i++)
		{
			if(m_nDontBreakCompFlags & static_cast<s64>(BIT(i)))
				breakingImpulses[i].Zero();
			else
				breakingImpulses[i].Set(componentImpulses[i]);
		}

		return fragInst::FindBreakStrength(breakingImpulses, componentPositions, breakStrength, breakData);
	}

	return fragInst::FindBreakStrength(componentImpulses, componentPositions, breakStrength, breakData);
}

bool fragInstGta::IsGroupBreakable(int nGroupIndex) const
{
	if(m_nDontBreakCompFlags)
	{
		Assert(nGroupIndex > -1 && nGroupIndex < GetTypePhysics()->GetNumChildGroups());
		fragTypeGroup* pGroup = GetTypePhysics()->GetAllGroups()[nGroupIndex];
		for(int i = 0; i < pGroup->GetNumChildren(); i++)
		{
			int iChild = pGroup->GetChildFragmentIndex() + i;
			if(m_nDontBreakCompFlags & static_cast<s64>(BIT(iChild)))
			{
				return false;
			}
		}
	}

	return true;
}

bool fragInstGta::ShouldBeInCacheToDraw() const
{
	if(fragInst::ShouldBeInCacheToDraw())
		return true;

	if(GetClassType()==PH_INST_FRAG_VEH || GetClassType()==PH_INST_FRAG_PED)
		return true;

	return false;
}

void fragInstGta::LosingCacheEntry()
{
	Assertf(CSystem::IsThisThreadId(SYS_THREAD_UPDATE), "Fragment cache entry being lost from outside the main thread (callstack please)!");
//	Assertf(GetInstFlag(FLAG_BEING_DELETED) || !(GetUserData() && ((CEntity*)GetUserData())->GetIsTypeVehicle()), "%s:Vehicle lost cache entry", CModelInfo::GetModelInfo (((CEntity*)GetUserData())->GetModelIndex()));
	if(CEntity* pEntity = CPhysics::GetEntityFromInst(this))
	{
		pEntity->LosingFragCacheEntry();
	}

	fragInst::LosingCacheEntry();
}

fragCacheEntry* fragInstGta::PutIntoCache()
{
	fragCacheEntry* newEntry = fragInst::PutIntoCache();
	if(newEntry)
	{
		if(CEntity* pEntity = CPhysics::GetEntityFromInst(this))
		{
			pEntity->GainingFragCacheEntry(*newEntry);
		}
	}
	return newEntry;
}


Mat34V_ConstRef fragInstGta::GetLastMatrix() const
{
	return PHLEVEL->GetLastInstanceMatrix(this);
}


Vec3V_Out fragInstGta::GetExternallyControlledVelocity () const
{
	Vec3V velocity(V_ZERO);

	CEntity* pEntity = (CEntity*)GetUserData();
	if(pEntity)
	{
		phInst* pInst = pEntity->GetCurrentPhysicsInst();
		phCollider* pCollider = CPhysics::GetSimulator()->GetCollider(pInst);
		if(pInst && pInst->IsInLevel() && pInst != this && pCollider)
		{
			velocity = pCollider->GetVelocity();
		}
		else
		{
			bool bInstActive = PHLEVEL->LegitLevelIndex(GetLevelIndex()) && PHLEVEL->IsActive(GetLevelIndex());
			if (!bInstActive && pEntity->GetIsPhysical())
			{
				CPhysical* pPhysical = (CPhysical*)pEntity;
				// ***HACK***
				// This logic should probably be contained within CPhysical::GetVelocity but it too late in the project to make such sweeping changes. 
				// Here (and the equivalent function in phInstGta) seemed to be the only place where this logic was required (although there are probably
				// other areas that would benefit) so the change was made here.
				if(pPhysical->GetIsAttached() && pPhysical->GetAttachParent() != NULL && static_cast<CEntity*>(pPhysical->GetAttachParent())->GetIsTypeVehicle() && static_cast<CVehicle*>(pPhysical->GetAttachParent())->InheritsFromTrain())
				{
					CPhysical* pAttachParent = static_cast<CPhysical*>(pPhysical->GetAttachParent());
					pPhysical->CalculateGroundVelocity(pAttachParent, pPhysical->GetTransform().GetPosition(), 0, velocity);
				}
				else
				{
					velocity = RCC_VEC3V(pPhysical->GetVelocity());
				}
			}
		}
	}

	return velocity;
}

Vec3V_Out fragInstGta::GetExternallyControlledAngVelocity () const
{
	Vector3 angVelocity(ORIGIN);

	CEntity* pEntity = (CEntity*)GetUserData();
	if(pEntity)
	{
		phInst* pInst = pEntity->GetCurrentPhysicsInst();
		phCollider* pCollider = CPhysics::GetSimulator()->GetCollider(pInst);
		if(pInst && pInst->IsInLevel() && pInst != this && pCollider)
		{
			angVelocity = RCC_VECTOR3(pCollider->GetAngVelocity());
		}
		else
		{
			bool bInstActive = PHLEVEL->LegitLevelIndex(GetLevelIndex()) && PHLEVEL->IsActive(GetLevelIndex());
			if (!bInstActive && pEntity->GetIsPhysical())
			{
				CPhysical* pPhysical = (CPhysical*)pEntity;
				angVelocity.Set(pPhysical->GetAngVelocity());
			}
		}
	}

	return RCC_VEC3V(angVelocity);
}

ScalarV_Out fragInstGta::GetInvTimeStepForComponentExternalVelocity () const
{
	if(const CEntity* pEntity = CPhysics::GetEntityFromInst(this))
	{
		// Only use component external velocity on inactive animating ambient fragments.
		// It shouldn't cause any issues with peds/vehicles but it is a bit slow and doesn't solve any known issues with peds/vehicles. 
		if(pEntity->GetIsTypeObject() && pEntity->GetAnimDirector() && pEntity->GetIsStatic())
		{
			return Invert(ScalarVFromF32(fwTimer::GetTimeStep()));
		}
	}

	return ScalarV(V_ZERO);
}

#if __DEV
void fragInstGta::InvalidStateDump() const
{
	fragInst::InvalidStateDump();
	InvalidStateDumpHelper(this);
}
#endif // __DEV

/////////////////////////////////////////////////////
// fragInstNMGta  (Natural Motion - but used for all peds even if NM is not defined)
/////////////////////////////////////////////////////
//
dev_float fragInstNMGta::ms_fVehiclePedActivationSpeed			= 0.5f;
dev_float fragInstNMGta::ms_fVehiclePlayerActivationSpeed		= 2.0f;
dev_float fragInstNMGta::ms_fPlayerPedActivationSpeed			= 1.0f;
dev_float fragInstNMGta::ms_fPlayerGroupPedActivationSpeed		= 2.0f;

float fragInstNMGta::m_ArmedPedBumpResistance	= 5.0f;

#if __BANK
bool fragInstNMGta::ms_bLogUsedRagdolls = false;
int fragInstNMGta::ms_MaxNMAgentsUsedCurrentLevel = 0;
int fragInstNMGta::ms_MaxRageRagdollsUsedCurrentLevel = 0;
int fragInstNMGta::ms_MaxNMAgentsUsedInComboCurrentLevel = 0;
int fragInstNMGta::ms_MaxRageRagdollsUsedInComboCurrentLevel = 0;
int fragInstNMGta::ms_MaxTotalRagdollsUsedCurrentLevel = 0;
int fragInstNMGta::ms_NumFallbackAnimsUsedCurrentLevel = 0;

int fragInstNMGta::ms_MaxNMAgentsUsedGlobally = 0;
int fragInstNMGta::ms_MaxRageRagdollsUsedGlobally = 0;
int fragInstNMGta::ms_MaxNMAgentsUsedInComboGlobally = 0;
int fragInstNMGta::ms_MaxRageRagdollsUsedInComboGlobally = 0;
int fragInstNMGta::ms_MaxTotalRagdollsUsedGlobally = 0;
int fragInstNMGta::ms_NumFallbackAnimsUsedGlobally = 0;
#endif

//bool fragInstNMGta::ms_bUseBulletForces						= true;
bool fragInstNMGta::ms_bLogRagdollForces					= false;


fragInstNMGta::fragInstNMGta(s32 nInstType, const fragType* type, const Matrix34& matrix, u32 guid)
:	fragInstNM(type, matrix, guid)
{
	m_nType = nInstType;
	SetEntity(NULL);
	m_feedbackInterfaceGta.SetParentInst(this);
	m_bDontZeroMatricesOnActivation = false;
	m_iDisableCollisionMask = 0;
	m_ImpulseModifier = CTaskRageRagdoll::GetMaxImpulseModifier();
	m_PendingImpulseModifier = -1.0f;
	m_CounterImpulseCount = -1;
	m_IncomingAnimVelocityScaleReset = -1.0f;
	m_AllowDeadPedActivationThisFrame = false;
	m_fActivationStartTime = 0;
	m_SuppressHighRagdollLODOnActivation = false;
	m_RequestedPhysicsLODOnActivation = -1;
	m_EjectedFromVehicleFrames = -1;
	m_VehicleBeingEjectedFrom = NULL;
	m_RootBonePosAtBoundsUpdate = Vec3V(V_ZERO);
	m_pLastImpactDamageEntity = NULL; 
	m_uTimeSinceImpactDamage = 0;
	m_fAccumulatedImpactDamage = 0.0f;
	m_LastRRApplyBulletTime = 0;
	m_bBulletLoosenessActive = false;
	m_WheelTwitchStarted = false;
	m_ContactedWheel = false;
	m_uTimeTireImpulseWasApplied = 0;
	m_fTireImpulseAppliedRatio = 0.0f;
#if __BANK
	ms_bLogUsedRagdolls = PARAM_logusedragdolls.Get();
#endif
}

fragInstNMGta::fragInstNMGta(datResource & rsc)
:	fragInstNM(rsc)
{
	Assertf(false, "fragInstNMGta::fragInstNMGta(datResource & rsc) - wanted to see if we hit this");
	m_nType = PH_INST_FRAG_GTA;
	SetEntity(NULL);
	m_feedbackInterfaceGta.SetParentInst(this);
	m_bDontZeroMatricesOnActivation = false;
	m_iDisableCollisionMask = 0;
	m_ImpulseModifier = CTaskRageRagdoll::GetMaxImpulseModifier();
	m_PendingImpulseModifier = -1.0f;
	m_CounterImpulseCount = -1;
	m_AllowDeadPedActivationThisFrame = false;
	m_fActivationStartTime = 0;
	m_SuppressHighRagdollLODOnActivation = false;
	m_RequestedPhysicsLODOnActivation = -1;
	m_EjectedFromVehicleFrames = -1;
	m_VehicleBeingEjectedFrom = NULL;
	m_RootBonePosAtBoundsUpdate = Vec3V(V_ZERO);
	m_pLastImpactDamageEntity = NULL; 
	m_uTimeSinceImpactDamage = 0;
	m_fAccumulatedImpactDamage = 0.0f;
	m_LastRRApplyBulletTime = 0;
	m_bBulletLoosenessActive = false;
	m_WheelTwitchStarted = false;
	m_ContactedWheel = false;
	m_uTimeTireImpulseWasApplied = 0;
	m_fTireImpulseAppliedRatio = 0.0f;
}


fragInstNMGta::~fragInstNMGta()
{
	Assert(!IsInLevel());
}

//
// this function lets us skip out of collision tests
//
bool fragInstNMGta::ShouldFindImpacts(const phInst* pOtherInst) const
{
#if __DEV
	if(this==spDebugBreakRagdoll && pOtherInst==spDebugBreakCar)
		Printf("Found combo");
#endif
	if (this == pOtherInst)
	{
		if (CPhysics::GetLevel()->IsActive(this->GetLevelIndex()))
		{
			PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, true, "Already active"));
			return true;
		}
		else
		{
			Assertf(0, "I'm activating myself!");
		}
	}

	if(GetUserData()==NULL)
	{
		PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, false, "No user data"));
		return false;
	}

	CPed* pPed = (CPed*)GetUserData();

	if(!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_AttachedToVehicle ))
	{
		if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && !pPed->GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
		{
			bool bShouldFindImpacts = false;
			const CVehicle* pVehicle = pPed->GetMyVehicle();
			if (pVehicle)
			{
				const s32 iMySeat = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
				if (pVehicle->IsSeatIndexValid(iMySeat))
				{
					CEntity* pOtherEntity = static_cast<CEntity*>(pOtherInst->GetUserData());
					bool bProjectile = false;
					if(pOtherEntity && pOtherEntity->GetIsTypeObject())
					{
						bProjectile = static_cast<CObject*>(pOtherEntity)->GetAsProjectile() ? true : false;
					}

					if(bProjectile && (pVehicle->InheritsFromQuadBike() || pVehicle->InheritsFromBike() || pVehicle->InheritsFromAmphibiousQuadBike()))
					{
						bShouldFindImpacts = true;
					}

					const CVehicleSeatAnimInfo* pSeatAnimInfo = pVehicle->GetSeatAnimationInfo(iMySeat);
					if (pSeatAnimInfo && pSeatAnimInfo->GetKeepCollisionOnWhenInVehicle())
					{
						bShouldFindImpacts = true;
					}
				}
			}

			if (!bShouldFindImpacts)
			{
				PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, false, "In vehicle"));
				return false;
			}
		}
	}
// 	else
// 	{
// 		CEntity* pEnt = (CEntity*)(pOtherInst->GetUserData());
// 		if (pEnt && pEnt->GetIsTypeVehicle() && pEnt == pPed->GetVehiclePedInside())
// 		{
// 			return false;
// 		}
// 	}


	/*** We shouldn't get in here if collision is disabled for this ped as the include flags should have been set correctly.
	if(!pPed->IsCollisionEnabled())
	{
		if(pedAttachExtension && pedAttachExtension->GetIsAttached() && pedAttachExtension->GetAttachFlag(ATTACH_FLAG_AUTODETACH_ON_RAGDOLL) )
		{
			// Prevent ragdolling when getting onto the seat
			if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE_SEAT))
			{
				return false;
			}

			// Continue and allow ragdoll activation
		}
		else if(pOtherInst && (pOtherInst->GetClassType() == PH_INST_EXPLOSION) && pPed->GetPedResetFlag(CPED_RESET_FLAG_ForceExplosionCollisions))
		{
			// Continue and allow ragdoll activation
		}
		else
		{
			return false;
		}
		}**
	*/

	// check against other entity
	CEntity* pOtherEntity = static_cast<CEntity*>(pOtherInst->GetUserData());
	if(pOtherEntity && pOtherEntity->GetIsPhysical())
	{
		if( pPed->GetRagdollOnCollisionIgnorePhysical() && 
			pPed->GetRagdollOnCollisionIgnorePhysical() == pOtherEntity)
		{
			PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, false, "Ignore entity"));
			return false;
		}

		const fwAttachmentEntityExtension *pedAttachExtension = pPed->GetAttachmentExtension();
		// Check if we are colliding with our attachment
		// Do not use getAttachEntity since this will return NULL if we are detaching
		if(pedAttachExtension && pedAttachExtension->GetAttachParentForced())	
		{
			if(pedAttachExtension->GetAttachParentForced()->GetCurrentPhysicsInst()==pOtherInst)
			{
				// physically attached entities will collide together ONLY if flag is set
				if(pedAttachExtension->GetAttachState()==ATTACH_STATE_PHYSICAL
					|| pedAttachExtension->GetAttachState()==ATTACH_STATE_RAGDOLL)
				{
					if(!pedAttachExtension->GetAttachFlag(ATTACH_FLAG_DO_PAIRED_COLL))
					{
						PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, false, "ATTACH_FLAG_DO_PAIRED_COLL"));
						return false;
					}
				}
				// all other attachment types don't collide with each other
				// in most case GetUsesCollision() should return false so won't get this far anyway
				else
				{
					PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, false, "other attachment type"));
					return false;
				}
			}
		}
	}
	
	// all the fixed-type reasons for a ragdoll not activating
	if( !CPhysics::CanUseRagdolls() || 
		pPed->GetIsAnyFixedFlagSet() || 
		GetInstFlag(FLAG_NEVER_ACTIVATE) || 
		pPed->GetRagdollState()==RAGDOLL_STATE_ANIM_PRESETUP || 
		( pPed->GetRagdollState()==RAGDOLL_STATE_ANIM_LOCKED && !pPed->IsUsingJetpack() ) ) // B*1919511 - Ignore the ragdoll state if they're using a jetpack
	{
#if __ASSERT
		if (CPhysics::GetLevel()->IsActive(this->GetLevelIndex()))
			pPed->SpewRagdollTaskInfo();
        Assertf(!CPhysics::GetLevel()->IsActive(this->GetLevelIndex()),//
                   "This peds ragdoll shouldn't have activated (If statement BitFlags %d%d%d%d%d%d%d%d)!",
                   !CPhysics::CanUseRagdolls()                          ? 1 : 0,
                   pPed->GetIsAnyFixedFlagSet()                         ? 1 : 0,
				   pPed->GetIsFixedFlagSet()							? 1 : 0,
				   pPed->GetIsFixedByNetworkFlagSet()                   ? 1 : 0,
				   pPed->GetIsFixedUntilCollisionFlagSet()              ? 1 : 0,
                   GetInstFlag(FLAG_NEVER_ACTIVATE)						? 1 : 0,
                   pPed->GetRagdollState()==RAGDOLL_STATE_ANIM_PRESETUP ? 1 : 0,
                   pPed->GetRagdollState()==RAGDOLL_STATE_ANIM_LOCKED   ? 1 : 0);
#endif

		// explosion behaviours are still allowed to find impacts, since it's going to deal with the results differently
		if(!pOtherInst || pOtherInst->GetClassType() != PH_INST_EXPLOSION)
		{
			PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, false, "!PH_INST_EXPLOSION"));
			return false;
		}
	}

	// if inst's both have the same parent, don't collide
	if(pOtherInst && pOtherInst->GetUserData() == GetUserData())
	{
		PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, false, "Same parent"));
		return false;
	}

	// do this check last to allow chance to skip out earlier and let NoCollision be reset
	if(pPed->GetNoCollisionEntity() && pPed->TestNoCollision(pOtherInst))
	{
		PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, false, "GetNoCollisionEntity"));
		return false;
	}

	if(pPed->GetRagdollState()==RAGDOLL_STATE_ANIM)
	{
		if(pOtherInst && pOtherInst->IsInLevel())
		{
			// Always consider hits with explosions
			if( pOtherInst->GetClassType() == PH_INST_EXPLOSION )
			{
				PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, true, "Other == PH_INST_EXPLOSION"));
				return true;
			}

			if(pOtherEntity)
			{
				if (pPed->GetAnimDirector())
				{
					// are we in a synchronized scene?
					fwAnimDirectorComponentSyncedScene* pSceneComponent = static_cast<fwAnimDirectorComponentSyncedScene*>(pPed->GetAnimDirector()->GetComponentByType(fwAnimDirectorComponent::kComponentTypeSyncedScene));
					if (pSceneComponent && pSceneComponent->IsPlayingSyncedScene())
					{
						fwSyncedSceneId sceneId = pSceneComponent->GetSyncedSceneId();
						fwEntity* sceneAttachParent = fwAnimDirectorComponentSyncedScene::GetSyncedSceneAttachEntity(sceneId);
						// don't collide with the entity our scene is attached to
						if (sceneAttachParent && sceneAttachParent==pOtherEntity)
						{
							PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, false, "SyncedSceneAttachParent"));
							return false;
						}
					}
				}
				// kinematic entities don't collide with each other
				if (pOtherEntity && pOtherEntity->GetIsPhysical() && pPed->IsUsingKinematicPhysics())
				{
					CPhysical* pOtherPhys = static_cast<CPhysical*>(pOtherEntity);
					if (pOtherPhys->IsUsingKinematicPhysics())
					{
						PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, false, "IsUsingKinematicPhysics"));
						return false;
					}
				}

				// always allow collision between dead peds and an active ragdoll (note - this does not necessarily result in activating
				// the dead ped (see fragInstNMGTA::PrepareForActivation), but we don't want ragdolling peds falling through corpses. 
				if ( pOtherEntity->GetIsTypePed() && ((CPed*)pOtherEntity)->GetUsingRagdoll() && pPed->IsDead())
				{
					PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, true, "ragdoll vs dead ragdoll"));
					return true;
				}
				// the player ped can knock peds over
				else if((!GetInstFlag(phInstGta::FLAG_NM_NO_PLAYER_PED_ACTIVATE) || pPed->IsDead())	&& pOtherEntity->GetIsTypePed() && ((CPed*)pOtherEntity)->IsPlayer())
				{
					// Players can collide with dead ragdolls
					if (pPed->IsDead() && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))
					{
						PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, true, "player vs dead ragdoll"));
						return true;
					}

					if( CTaskNMBehaviour::CanUseRagdoll(pPed, ((CPed*)pOtherEntity)->GetUsingRagdoll() ? RAGDOLL_TRIGGER_IMPACT_PLAYER_PED_RAGDOLL : RAGDOLL_TRIGGER_IMPACT_PLAYER_PED, pOtherEntity))
					{
						PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, true, "fVel > fThreshold"));
						return true;
					}
				}
				else if(pOtherEntity->GetIsTypeObject())
				{
					// Allow the ragdoll bounds of animated peds to collide with pickups (with caveats that are handled in PreComputeImpacts and PrepareForActivation)
					if(pOtherInst->GetArchetype() && CPhysics::GetLevel()->GetInstanceTypeFlag(pOtherInst->GetLevelIndex(), ArchetypeFlags::GTA_PICKUP_TYPE))
					{
						return true;
					}
					else if(((CObject*)pOtherEntity)->GetAsProjectile())
					{
						PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, true, "GetAsProjectile"));
						return true;
					}
					else if (pPed->IsDead() && ((CObject*)pOtherEntity)->IsADoor())
					{
						// Allow through if dead animating and contacting a door
						PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, true, "Frozen corpse touching door"));
						return true;
					}
					else
					{ 
						// Allow objects to cause peds to ragdoll only if they exceed a certain momentum threshold, or if they're frozen corpses.
						const float kfObjectMomentumThreshold = 500.0f;

						float fObjMass = static_cast<CObject*>(pOtherEntity)->GetMass();
						Vector3 vObjVel = static_cast<CObject*>(pOtherEntity)->GetVelocity();
						Vector3 vOffset = VEC3V_TO_VECTOR3(GetPosition()) - VEC3V_TO_VECTOR3(pOtherInst->GetPosition());
						vOffset.NormalizeSafe();
						float fDotVel	= vOffset.Dot(vObjVel);
						float fForce	= fDotVel*fObjMass;
						float fObjMomentumSq = fForce * fForce;
						
						if((fObjMomentumSq > kfObjectMomentumThreshold*kfObjectMomentumThreshold || pPed->IsDead())
							&& CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_IMPACT_OBJECT, pOtherEntity, fForce))
						{
							return true;
						}
					}
				}
				else if (pOtherEntity->GetIsTypePed())
				{
					CPed* pOtherPed = (CPed*) pOtherInst->GetUserData();
					if (pOtherPed)
					{ 
						if (pOtherPed->GetUsingRagdoll())
						{
							// Let other ragdolls at least collide with frozen corpses (won't actually wake up unless underneath the frozen corpse)
							if (pPed->IsDead() && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))
							{
								PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, true, "wake up frozen corpses"));
								return true;
							}

							// Let collision through with carried ped ragdoll if this is the carrier
							if (CTaskNMSlungOverShoulder* pTask = static_cast<CTaskNMSlungOverShoulder*>(
								pOtherPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_SLUNG_OVER_SHOULDER)))
							{
								if (pTask->GetCarrierPed() == pPed)
								{
									PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, true, "GetCarrierPed() == pPed"));
									return true;
								}
							}

							// Allow moving ragdolls to activate live non-player peds
							if (CTaskNMBehaviour::CanUseRagdoll(pPed, pOtherPed->IsPlayer() ? RAGDOLL_TRIGGER_IMPACT_PLAYER_PED_RAGDOLL : RAGDOLL_TRIGGER_IMPACT_PED_RAGDOLL, pOtherPed))
							{
								PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, true, "moving ragdoll activation"));
								return true;
							}
						}
						else  // Both peds are animated
						{
							// If this a frozen corpse colliding with an animated ped that can activate based on collisions, allow the contact through 
							if (pPed->IsDead() && pOtherPed->GetActivateRagdollOnCollision() && 
								CTaskNMBehaviour::CanUseRagdoll(pOtherPed, RAGDOLL_TRIGGER_ACTIVATE_ON_COLLISION))
							{
								PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, true, "frozen corpse vs. bActivateRagdollOnCollision"));
								return true;
							}
						}
					}
				}
				else if (pOtherEntity->GetIsTypeVehicle())
				{
					PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, true, "ped vs. vehicle"));
					return true;
				}
			}
		}

		bool bActivateRagdollOnCollision = 	pPed->GetActivateRagdollOnCollision();
		// if we want to activate the ragdoll on hitting an object, we need to continue on to precomputeimpacts
		if (bActivateRagdollOnCollision && (pPed->GetAllowedRagdollFixed() || !CPhysics::GetLevel()->IsFixed(pOtherInst->GetLevelIndex())) && CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_ACTIVATE_ON_COLLISION))
		{
			PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, true, "bActivateRagdollOnCollision"));
			return true;
		}

/**************************************************/
		PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, false, "RAGDOLL_STATE_ANIM passed"));
		return false;
	}

	// Ensure that injuredOnGround is disabled if possibly colliding with another corpse
	if (pOtherEntity && pOtherEntity->GetIsTypePed() && pPed->GetUsingRagdoll() && pPed->IsDead())
	{
		CPed* pOtherPed = (CPed*) pOtherInst->GetUserData();
		if (pOtherPed && pOtherPed->IsDead())
		{
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_DisableInjuredOnGround);
		}
	}


	PDR_ONLY(debugPlayback::RecordNMFindImpacts(this, pOtherInst, true, "All tests passed"));
	return true;
}

void fragInstNMGta::PreComputeImpacts(phContactIterator impacts)
{
#if __DEV
	if(this==spDebugBreakRagdoll)
		Printf("Found combo");
#endif

#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
	ModifyImpactsForSecondSurface(this, impacts, GetBoundGeomIntersectionSecondSurfaceInterpolation());
#endif	// HACK_GTA4_BOUND_GEOM_SECOND_SURFACE

	fragInstNM::PreComputeImpacts(impacts);

	if(!impacts.GetMyInstance() || !PHLEVEL->LegitLevelIndex(impacts.GetMyInstance()->GetLevelIndex()))
	{
		// If our instance is now invalid, don't do anything
		return;
	}

	if(GetUserData() && ((CEntity *)GetUserData())->GetIsPhysical())
	{
		CPhysical* pPhysical = (CPhysical*)GetUserData();
		const bool bDoPreComputeImpactsTest = pPhysical->DoPreComputeImpactsTest();
		const bool bDoDisableCollisionMaskTest = m_iDisableCollisionMask > 0;
		if(bDoPreComputeImpactsTest || bDoDisableCollisionMaskTest)
		{
			while(!impacts.AtEnd())
			{
				if(bDoPreComputeImpactsTest && pPhysical->TestNoCollision(impacts.GetOtherInstance()))
				{
					PDR_ONLY(debugPlayback::RecordContact(impacts));
					PDR_ONLY(debugPlayback::RecordModificationToContact(impacts, "Disabled", "TestNoCollision"));
					impacts.DisableImpact();
				}
				else if(bDoDisableCollisionMaskTest &&
					BIT(impacts.GetMyComponent()) & m_iDisableCollisionMask
					&& !impacts.IsConstraint()
					)
				{
					PDR_ONLY(debugPlayback::RecordContact(impacts));
					PDR_ONLY(debugPlayback::RecordModificationToContact(impacts, "Disabled", "DisableCollisionMask"));
					impacts.DisableImpact();
				}
				impacts++;
			}
		}

		if(pPhysical->GetIsTypePed())
		{
			CPed* pPed=static_cast<CPed*>(pPhysical);
			pPed->ProcessPreComputeImpacts(impacts);
		}
	}
}


bool fragInstNMGta::CheckCanActivate(const CPed* pPed, bool ASSERT_ONLY(bAssert)) const
{
	// don't let this instance activate if the fixed flag is set
	if(pPed->GetIsAnyFixedFlagSet())
	{
#if __ASSERT
		if(bAssert)
			Assertf(false, "%s:Ragdoll failed to activate because of AnyFixedFlagSet, Fixed: %d, FixedByNetwork: %d, FixedForCollision: %d", pPed->GetModelName(), pPed->IsBaseFlagSet(fwEntity::IS_FIXED), pPed->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK), pPed->IsProtectedBaseFlagSet(fwEntity::IS_FIXED_UNTIL_COLLISION));
#endif // __ASSERT			
		return false;
	}

	// flag can also be set on the phInst to not activate
	if(GetInstFlag(FLAG_NEVER_ACTIVATE))
	{
#if __ASSERT
		if(bAssert)
			Assertf(false, "%s:Ragdoll failed to activate because of DONT_ACTIVATE flag", pPed->GetModelName());
#endif // __ASSERT			
		return false;
	}

	// Make sure Ragdolls don't get activated physically if they're not active due to animation being in control
	if(pPed->GetRagdollState() < RAGDOLL_STATE_ANIM || pPed->GetRagdollState() > RAGDOLL_STATE_PHYS)
	{
#if __ASSERT
		if(bAssert)
			Assertf(false, "%s:Ragdoll failed to activate because of ragdoll state", pPed->GetModelName());
#endif // __ASSERT			
		return false;
	}

	return true;
}


dev_bool sbForceZeroLastMatricesA = true;	// zero last matrices if std activation
dev_bool sbForceZeroLastMatricesB = false;	// higher level code can request not to zero matrices on activation
dev_bool sbForceZeroLastMatricesC = false; // zero last matrices if physical activation or ped is seated
//

void fragInstNMGta::SetImpactDamageEntity(CEntity *entity) 
{ 
	m_pLastImpactDamageEntity = entity; 
	m_uTimeSinceImpactDamage = fwTimer::GetTimeInMilliseconds(); 
}

u32 fragInstNMGta::GetTimeSinceImpactDamage() const 
{ 
	return fwTimer::GetTimeInMilliseconds() - m_uTimeSinceImpactDamage; 
}

Vec3V_Out fragInstNMGta::SendAnimationAverageVelToNM(float fTimeStep)
{
	Vec3V vecVelFromLastMatrix = Vec3V(V_ZERO);
	ScalarV timeStepV;
	timeStepV.Setf(fTimeStep);
	if (IsGreaterThanAll(timeStepV, ScalarV(V_ZERO)))
	{
		Assertf(IsFiniteStable(GetMatrix().GetCol3()),  "GetMatrix().GetCol3() is invalid");
		Assertf(IsFiniteStable(PHSIM->GetLastInstanceMatrix(this).GetCol3()),  "PHSIM->GetLastInstanceMatrix(this).GetCol3() is invalid");
		vecVelFromLastMatrix = GetMatrix().GetCol3() - PHSIM->GetLastInstanceMatrix(this).GetCol3();
		vecVelFromLastMatrix /= timeStepV;
		if(!physicsVerifyf(IsFiniteStable(vecVelFromLastMatrix), "vecVelFromLastMatrix is invalid. timeStepV is %f", timeStepV.Getf()))
		{
			vecVelFromLastMatrix.ZeroComponents();
		}
	}
	FRAGNMASSETMGR->SetFromAnimationAverageVel(RCC_VECTOR3(vecVelFromLastMatrix));
	return vecVelFromLastMatrix;
}

void fragInstNMGta::OnActivate(phInst* otherInst)
{
	if(CEntity* pEntity = CPhysics::GetEntityFromInst(this))
	{
		pEntity->OnActivate(this, otherInst);
	}

	if(CExplosionManager::InsideExplosionUpdate())
	{
		CExplosionManager::RegisterExplosionActivation(GetLevelIndex());
	}
}

void fragInstNMGta::OnDeactivate()
{
	if(CEntity* pEntity = CPhysics::GetEntityFromInst(this))
	{
		pEntity->OnDeactivate(this);
	}
}

phInst *fragInstNMGta::PrepareForActivation(phCollider **collider, phInst* otherInst, const phConstraintBase * constraint)
{
	CPed* pPed = (CPed*)GetUserData();
	Assert(pPed);

	if(GetInstFlag(FLAG_NEVER_ACTIVATE))
	{
		nmDebugf2("[%u] PrepareForActivation:%s(%p) - ABORTED. FLAG_NEVER_ACTIVATE is set.", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed);
		return NULL;
	}

	if(GetUserData()==NULL)
	{
		nmDebugf2("[%u] PrepareForActivation:%s(%p) - ABORTED. GetUserData()==NULL.", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed);
		return NULL;
	}

	// *See CTaskNMBehaviour::CanUseRagdoll* fragInstNMGta::PrepareForActivation will no longer call UpdateFixedWaitingForCollision which 
	//   allows ragdolls to activate until the next physics update where it will get properly fixed. This should be
	//   no different than an active ragdoll losing collision and deactivating. The benefit is that we don't have to
	//   call UpdateFixedWaitingForCollision in CTaskNMBehaviour::CanUseRagdoll. 
	/*
	// If we aren't fixed, check for nearby collision
	if(!pPed->GetIsFixedUntilCollisionFlagSet())
	{
		pPed->UpdateFixedWaitingForCollision(true);
		Assert(CPhysics::GetSimulator()->GetCollider(this)==NULL);
	}
	*/
	CEntity* pOtherEntity = otherInst ? CPhysics::GetEntityFromInst(otherInst) : NULL;

	if(otherInst && otherInst->GetUserData() && pPed->IsDead())
	{
		// Disable corpse activations from non-players and ragdolling players unless the specifically instructed to do so
		if(pOtherEntity->GetIsTypePed())
		{
			CPed* pOtherPed = static_cast<CPed*>(pOtherEntity);
			if(((!pOtherPed->IsPlayer() || pOtherPed->GetUsingRagdoll()) && !m_AllowDeadPedActivationThisFrame) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollOnPedCollisionWhenDead))
			{ 
				nmDebugf2("[%u] PrepareForActivation:%s(%p) - ABORTED. Disable corpse activations from non-players and ragdolling players unless the specifically instructed to do so", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed);
				return NULL;
			}
		}
		
		// Disable reactivations from another vehicle when our guy is dead and we are allowed to (determined by a ped config flag
		// which can be set from script).
		if(pOtherEntity->GetIsTypeVehicle() && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollOnVehicleCollisionWhenDead))
		{
			nmDebugf2("[%u] PrepareForActivation:%s(%p) - ABORTED. Disable reactivations from another vehicle when our guy is dead and we are allowed to.", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed);
			return NULL;
		}		
	}

#if __ASSERT && __BANK
	if(CPed::sm_deadRagdollDebug && pPed->IsDead())
	{	
		Displayf("----==== [B*1860478] ====----");		

		if(otherInst && pOtherEntity)
		{	
			CNetObjGame *netObj = 0;
			if(pOtherEntity->GetIsDynamic())
				netObj = static_cast<CDynamicEntity *>(pOtherEntity)->GetNetworkObject();

			Displayf("[%u] Dead ped ragdoll just got activated. Other Entity: %s [%s][Type %i] | otherInst: %p | Other Level Index: %i", fwTimer::GetFrameCount(), pOtherEntity->GetDebugName(), netObj ? netObj->GetLogName() : "", pOtherEntity->GetType(), otherInst, otherInst->GetLevelIndex());
		}
		else
			Displayf("[%u] Dead ped ragdoll just got activated. otherInst: %p | Other Level Index: %i", fwTimer::GetFrameCount(), otherInst, otherInst ? otherInst->GetLevelIndex() : -1);

		sysStack::PrintStackTrace();
		Displayf("----==== [END B*1860478] ====----");
	}
#endif

	// Don't allow collisions with pickups to activate a ragdoll
	if(otherInst && otherInst->GetArchetype() && CPhysics::GetLevel()->GetInstanceTypeFlag(otherInst->GetLevelIndex(), ArchetypeFlags::GTA_PICKUP_TYPE))
	{
		nmDebugf2("[%u] PrepareForActivation:%s(%p) - ABORTED. Don't allow collisions with pickups to activate a ragdoll.", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed);
		return NULL;
	}

	// Disable activations from a ground physical vehicle that supports standing	

	CPhysical *pGroundPhysical = pPed->GetGroundPhysical();
	if(!pGroundPhysical && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_RidingTrain))
	{
		pGroundPhysical = pPed->GetLastValidGroundPhysical();
	}

	if ( (pGroundPhysical || pPed->GetClimbPhysical(true)) && otherInst)
	{
		if(pOtherEntity && pOtherEntity->GetIsTypeVehicle())
		{
			//! Walking between carriages on a train.
			bool bTrainLink = false;
			CVehicle* pVehicle = static_cast<CVehicle*>(pOtherEntity);
			if(pVehicle->InheritsFromTrain() && pGroundPhysical)
			{
				CTrain *pTrain = static_cast<CTrain*>(pVehicle);
				if(pTrain->GetLinkedToBackward() == pGroundPhysical || pTrain->GetLinkedToForward() == pGroundPhysical)
				{
					bTrainLink = true;
				}
			}

			if((pOtherEntity == pGroundPhysical) || (pOtherEntity == pPed->GetClimbPhysical(true)) || bTrainLink)
			{
				CVehicle* pVehicle = static_cast<CVehicle*>(pOtherEntity);
				Assert(pVehicle->GetVehicleModelInfo());
				if(pVehicle->CanPedsStandOnTop())
				{
					// Don't allow the ped to ragdoll unless the ground acceleration is extreme.
					// This check should match the one in ShouldFindImpacts - it's needed here as well since if this is the first
					// frame that the ground physical was detected, ShouldFindImpacts wouldn't have had a chance to screen the contacts.
					//if (!IsHighGoundAcceleration(pPed))

					nmDebugf2("[%u] PrepareForActivation:%s(%p) - ABORTED. Disable activations from a ground physical vehicle that supports standing.", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed);
					return NULL;
				}
			}
		}
	}

	// standard checks for ragdoll being able to activate (e.g. not fixed, not ragdoll state locked)
	if(!CheckCanActivate(pPed, false))
	{
		nmDebugf2("[%u] PrepareForActivation:%s(%p) - ABORTED. standard checks for ragdoll being able to activate (e.g. not fixed, not ragdoll state locked).", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed);
		return NULL;
	}

	// Dead peds can be activated by sleep islands, so we have to check its ok to ragdoll them here.
	if (pPed->IsDead())
	{
		if (!CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))
		{
			nmDebugf2("[%u] PrepareForActivation:%s(%p) - ABORTED. Dead peds can be activated by sleep islands, so we have to check its ok to ragdoll them here..", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed);
			return NULL;
		}
	}

	// This path gets hit from within the InsertAgent below, we don't want to interfere at that time
	if(m_AgentId != -1)
		return this;

	if (otherInst)
	{
		CEntity* pOtherEnt = (CEntity*)(otherInst->GetUserData());
		if (pOtherEnt)
		{
			if(pOtherEnt->GetIsTypePed())
			{
				CPed* pOtherPed = (CPed*) otherInst->GetUserData();
				if (pOtherPed && pOtherPed->GetRagdollState() >= RAGDOLL_STATE_PHYS_ACTIVATE)
				{
					// Don't activate due to carrying a ragdoll over your shoulder
					if (CTaskNMSlungOverShoulder* pTask = static_cast<CTaskNMSlungOverShoulder*>(
						pOtherPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_SLUNG_OVER_SHOULDER)))
					{
						if (pTask->GetCarrierPed() == pPed)
						{
							nmDebugf2("[%u] PrepareForActivation:%s(%p) - ABORTED. Don't activate due to carrying a ragdoll over your shoulder.", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed);
							return NULL;
						}
					}
				}
			}
			else if(pOtherEnt->GetIsTypeObject())
			{
				const CObject* pObject = static_cast<CObject *>(pOtherEnt);

				const CProjectile* pProjectile = pObject->GetAsProjectile();
				if(pProjectile)
				{
					//Don't activate due to a projectile hitting you at less than a certain speed.
					//This was added to prevent peds from tripping over active grenades rolling on the ground.
					if(pProjectile->GetVelocity().Mag2() < square(2.0f))
					{
						nmDebugf2("[%u] PrepareForActivation:%s(%p) - ABORTED. Don't activate due to a projectile hitting you at less than a certain speed..", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed);
						return NULL;
					}

					const CEntity* pProjectileOwner = pProjectile->GetOwner();
					if (pProjectileOwner && pProjectileOwner->GetIsTypePed() && static_cast<const CPed*>(pProjectileOwner)->GetMyMount()==pPed)
					{
						nmDebugf2("[%u] PrepareForActivation:%s(%p) - ABORTED. if this projectile was tossed by my rider, ignore itL.", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed);
						return NULL; //if this was tossed by my rider, ignore it
					}

					// check whether we should activate for this projectile
					// (have to do this here rather than in shouldfindimpacts because
					// we need the projectile to bounce of the ragdoll bounds, but not activate the ragdoll
					// if it is too slow or small)
					Vec3V vecVel = VECTOR3_TO_VEC3V(pProjectile->GetVelocity());
					const float fVel = Mag(vecVel).Getf();
					Vec3V vecOffset = GetPosition() - otherInst->GetPosition();
					ScalarV vDotVel = Dot(vecOffset, vecVel);
					float fDotVel = vDotVel.Getf();

					float fThreshold = ms_fPlayerPedActivationSpeed;

					if(pPed->GetIsCrouching()) // crouching peds are generally more stable (also they are less able to balance effectively)
					{
						fThreshold *= 2.0f;
					}

					float fForce = fDotVel*((CObject*)pOtherEnt)->GetMass();

					if(fVel < fThreshold || !CTaskNMBehaviour::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_IMPACT_OBJECT, pOtherEnt, fForce))
					{
						nmDebugf2("[%u] PrepareForActivation:%s(%p) - ABORTED. check whether we should activate for this projectile.", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed);
						return NULL;
					}
				}
			}
		}

		if(pPed->GetKeepInactiveRagdollContacts())
		{
			nmDebugf2("[%u] PrepareForActivation:%s(%p) - ABORTED. Keeping inactive ragdoll contacts.", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed);
			return NULL;
		}
	}

	if (PHSIM->IsUpdateRunning())
	{
		// Update the ragdoll's inst matrices.  These matrices should only be different 
		// if activated in CollisionActivations or due to a constraint.  At that point the animated inst matrix has been updated via the collider position integration 
		// but the ragdoll's inst doesn't get set until PostPhysics in CPed::UpdateEntityFromPhysics();
		if(HasLastMatrix())
		{
			// Since we activating in the middle of physics update, the ragdoll matrix and time step are mismatching
			// Recompute the last matrix from the mover instance matrix and last time step stored in NM engine
			Mat34V matLast = GetMatrix();
			Vec3V vDisplacement = Scale(VECTOR3_TO_VEC3V(pPed->GetVelocity()), ScalarVFromF32(fwTimer::GetTimeStep()));
			matLast.SetCol3(pPed->GetAnimatedInst()->GetMatrix().d() - vDisplacement);
			CPhysics::GetLevel()->SetLastInstanceMatrix(this, matLast);
		}
		pPed->SetPedResetFlag( CPED_RESET_FLAG_SetLastMatrixDone, true );
		SetMatrix(pPed->GetAnimatedInst()->GetMatrix());
	}

	nmDebugf2("[%u] Activating ragdoll: %s(%p)", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed);

	// Set the ragdoll LOD
	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
	{
		SetCurrentPhysicsLOD(fragInst::RAGDOLL_LOD_HIGH);
	}
	else if (m_RequestedPhysicsLODOnActivation > -1)
	{
		SetCurrentPhysicsLOD((ePhysicsLOD) m_RequestedPhysicsLODOnActivation);
	}
	else if (!GetSuppressHighRagdollLODOnActivation())
	{
		SetCurrentPhysicsLOD(fragInst::RAGDOLL_LOD_HIGH);
	}
	SetSuppressHighRagdollLODOnActivation(false);
	m_RequestedPhysicsLODOnActivation = -1;

#if __ASSERT
	pPed->CollectRagdollStack(CPed::kStackPrepareForActivation);
#endif //___ASSERT

	if(pPed->GetRagdollState() < RAGDOLL_STATE_PHYS_ACTIVATE)
		pPed->SetRagdollState(RAGDOLL_STATE_PHYS_ACTIVATE);

#if LAZY_RAGDOLL_BOUNDS_UPDATE
	// Update bounds now if they haven't been getting updated
	if (!pPed->AreRagdollBoundsUpToDate())
	{
		PoseBoundsFromSkeleton(true, false);
		Assert(GetCacheEntry());
		GetCacheEntry()->GetBound()->UpdateLastMatricesFromCurrent();
	}
#endif

	phBoundComposite* pOriginalBound = (phBoundComposite*)GetArchetype()->GetBound();
	for(int i=0; i<pOriginalBound->GetNumBounds(); i++)
	{
		// check current and last matrices for NAN's
		Assert(RCC_MATRIX34(pOriginalBound->GetCurrentMatrix(i)).a==RCC_MATRIX34(pOriginalBound->GetCurrentMatrix(i)).a
			&& RCC_MATRIX34(pOriginalBound->GetCurrentMatrix(i)).b==RCC_MATRIX34(pOriginalBound->GetCurrentMatrix(i)).b
			&& RCC_MATRIX34(pOriginalBound->GetCurrentMatrix(i)).c==RCC_MATRIX34(pOriginalBound->GetCurrentMatrix(i)).c
			&& RCC_MATRIX34(pOriginalBound->GetCurrentMatrix(i)).d==RCC_MATRIX34(pOriginalBound->GetCurrentMatrix(i)).d);
		Assert(RCC_MATRIX34(pOriginalBound->GetLastMatrix(i)).a==RCC_MATRIX34(pOriginalBound->GetLastMatrix(i)).a
			&& RCC_MATRIX34(pOriginalBound->GetLastMatrix(i)).b==RCC_MATRIX34(pOriginalBound->GetLastMatrix(i)).b
			&& RCC_MATRIX34(pOriginalBound->GetLastMatrix(i)).c==RCC_MATRIX34(pOriginalBound->GetLastMatrix(i)).c
			&& RCC_MATRIX34(pOriginalBound->GetLastMatrix(i)).d==RCC_MATRIX34(pOriginalBound->GetLastMatrix(i)).d);
		
		// check for orthonormal
		if(!pOriginalBound->GetCurrentMatrix(i).IsOrthonormal3x3(ScalarV(V_FLT_SMALL_2)))
		{
			Warningf("Current bound matrix %d is not orthonormal (%f,%f,%f)\n", i, Mag(pOriginalBound->GetCurrentMatrix(i).GetCol0()).Getf(), Mag(pOriginalBound->GetCurrentMatrix(i).GetCol1()).Getf(), Mag(pOriginalBound->GetCurrentMatrix(i).GetCol2()).Getf());
			// in the unlikely event that this happens, normalise the matrix so we don't hit lots of other potential problems inside the physics
			Matrix34 matNormalised = RCC_MATRIX34(pOriginalBound->GetCurrentMatrix(i));
			matNormalised.NormalizeSafe();
			pOriginalBound->SetCurrentMatrix(i, RCC_MAT34V(matNormalised));
		}
		if(!pOriginalBound->GetLastMatrix(i).IsOrthonormal3x3(ScalarV(V_FLT_SMALL_2)))
		{
			Warningf("Last bound matrix %d is not orthonormal (%f,%f,%f)\n", i, Mag(pOriginalBound->GetLastMatrix(i).GetCol0()).Getf(), Mag(pOriginalBound->GetLastMatrix(i).GetCol1()).Getf(), Mag(pOriginalBound->GetLastMatrix(i).GetCol2()).Getf());
			// in the unlikely event that this happens, normalise the matrix so we don't hit lots of other potential problems inside the physics
			Matrix34 matNormalised = RCC_MATRIX34(pOriginalBound->GetLastMatrix(i));
			matNormalised.NormalizeSafe();
			pOriginalBound->SetLastMatrix(i, RCC_MAT34V(matNormalised));
		}
		
		// check the positional offset isn't to great
#if __ASSERT
		bool bValidOffset = true;
		if (!Verifyf(MagSquared(pOriginalBound->GetCurrentMatrix(i).GetCol3()).Getf() < 200.0f, 
			"Current bound matrix %d offset is %f - please check ped's animations to ensure correct mover/root bone DOFs (see TTY/log for animation list)", 
			i, Mag(pOriginalBound->GetCurrentMatrix(i).GetCol3()).Getf()))
		{
			bValidOffset = false;
		}
		if (!Verifyf(MagSquared(pOriginalBound->GetLastMatrix(i).GetCol3()).Getf() < 200.0f, 
			"Last bound matrix %d offset is %f - please check ped's animations to ensure correct mover/root bone DOFs (see TTY/log for animation list)", 
			i, Mag(pOriginalBound->GetLastMatrix(i).GetCol3()).Getf()))
		{
			bValidOffset = false;
		}
		if (!bValidOffset)
		{
			CAIDebugInfo::DebugInfoPrintParams printParams;
			CAnimationDebugInfo tempDebugInfo(pPed, printParams);
			tempDebugInfo.Print();
			tempDebugInfo.SetLogType(AILogging::TTY);
			tempDebugInfo.Print();
		}
#endif // __ASSERT
	}

#if __DEV
	// shouldn't need to set these every activation, only in CPhysics::Init, but for now allows us to easily modify them
	FRAGNMASSETMGR->SetFromAnimationMaxSpeed(CPhysics::GetRagdollActivationAnimVelClamp());
	FRAGNMASSETMGR->SetFromAnimationMaxAngSpeed(CPhysics::GetRagdollActivationAnimAngVelClamp());
#endif

	// Check if NM agents are disabled
	bool bWasNMActivationBlocked = IsNMActivationBlocked();
	if (CTaskNMBehaviour::sm_OnlyUseRageRagdolls)
		SetBlockNMActivation(true);

	// Reset the counter impulse counter
	m_CounterImpulseCount = -1;

	// Determine if the last matrix positons need to be set to zero
	bool bZeroLastMatrices = sbForceZeroLastMatricesA;
	if(m_bDontZeroMatricesOnActivation)
		bZeroLastMatrices = sbForceZeroLastMatricesB;
	else if(pPed->GetIsAttached() && pPed->GetAttachFlag(ATTACH_FLAG_COL_ON))
		bZeroLastMatrices = sbForceZeroLastMatricesC;
	SetZeroLastMatricesOnActivation(bZeroLastMatrices);
	
	//Check if there is an override for the velocity scale.
	float fVelocityScale = 1.0f / CPhysics::GetNumTimeSlices();
	if(m_IncomingAnimVelocityScaleReset != -1.0f)
	{
		//Set the velocity scale.
		fVelocityScale *= m_IncomingAnimVelocityScaleReset;

		//The velocity scale is a reset value, it is only good once.
		m_IncomingAnimVelocityScaleReset = -1.0f;
	}
	
	//Set the velocity scale.
	FRAGNMASSETMGR->SetIncomingAnimVelocityScale(fVelocityScale);

	// Determine the average animation velocity, about which we'll clamp velocities
	if (fVelocityScale == 0.0f)
	{
		// If the scale is 0.0, then we also set the average animation velocity to 0 in order to have the ragdoll
		// start with zero velocity, as that seems to be the desired useage.
		FRAGNMASSETMGR->SetFromAnimationAverageVel(Vector3::ZeroType);
	}
	else
	{
		// This will set the animation velocity based on the last and current matrices if CPed::SwitchToRagdollInternal
		// was not called prior to this.
		SendAnimationAverageVelToNM(fwTimer::GetTimeStep());
	}

	// need to detach the ped if it's currently attached to something
	if (pPed->GetIsAttached())
	{
		u16 iDetachFlags = 0;
		bool bDetachInPlace = false;
		if (pPed->GetAttachState() == ATTACH_STATE_PED_ENTER_CAR ||  pPed->GetAttachState() == ATTACH_STATE_PED_EXIT_CAR)
		{
			bDetachInPlace = true;
		}
		else if (pPed->GetAttachState() == ATTACH_STATE_PED_IN_CAR)
		{
			if (pPed->GetAttachParent() && static_cast<CEntity*>(pPed->GetAttachParent())->GetIsTypeVehicle())
			{
				const CVehicle* pVeh = static_cast<const CVehicle*>(pPed->GetAttachParent());
				const s32 iSeatIndex = pVeh->GetSeatManager()->GetPedsSeatIndex(pPed);
				if (pVeh->IsSeatIndexValid(iSeatIndex))
				{
					const CVehicleSeatAnimInfo* pSeatAnimInfo = pVeh->GetSeatAnimationInfo(iSeatIndex);
					if (pSeatAnimInfo->GetCanDetachViaRagdoll())
					{
						bDetachInPlace = true;
					}
				}
			}
		}
		// If we're dead and not using the ATTACH_STATE_PED_IN_CAR state but we're attached to a vehicle then allow us to be detached in-place
		else if (pPed->IsDead() && pPed->GetAttachParent() != NULL && static_cast<CEntity*>(pPed->GetAttachParent())->GetIsTypeVehicle())
		{
			const CVehicle* pVehicle = static_cast<const CVehicle*>(pPed->GetAttachParent());
			if(pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT || pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE || pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
			{
				pPed->ProcessAllAttachments();
				bDetachInPlace = true;
			}
		}

		if (bDetachInPlace)
		{
			iDetachFlags |= DETACH_FLAG_ACTIVATE_PHYSICS|DETACH_FLAG_EXCLUDE_VEHICLE|DETACH_FLAG_IGNORE_SAFE_POSITION_CHECK;
		}
		pPed->DetachFromParent(iDetachFlags);
	}

	pPed->GetSkeleton()->GetObjectMtx(0).GetCol3Ref() = pPed->GetSkeleton()->GetLocalMtx(0).GetCol3Ref() = m_RootBonePosAtBoundsUpdate;

	// ok to go ahead and activate
	phInst *pReturn = fragInstNM::PrepareForActivation(collider, otherInst, constraint); 

	// Reset the average velocity
	FRAGNMASSETMGR->SetFromAnimationAverageVel(Vector3(Vector3::ZeroType), true);
	
	//Restore the default velocity scale.
	FRAGNMASSETMGR->SetIncomingAnimVelocityScale(1.0f);

	// Reset NM disabled state
	if (CTaskNMBehaviour::sm_OnlyUseRageRagdolls && !bWasNMActivationBlocked)
		SetBlockNMActivation(false);

	if(pReturn)
	{
		// Set the ragdoll's damping parameters (accessed via rag tuning in rag->Physics->Simulator->Ragdoll tuning)
		if (pPed->GetRagdollInst()->GetArchetype())
		{
			phArchetypeDamp* pArch = static_cast<phArchetypeDamp*>(pPed->GetRagdollInst()->GetArchetype());
			pArch->ActivateDamping(phArchetypeDamp::LINEAR_C, Vector3(phArticulatedBody::sm_RagdollDampingLinC, phArticulatedBody::sm_RagdollDampingLinC, phArticulatedBody::sm_RagdollDampingLinC));
			pArch->ActivateDamping(phArchetypeDamp::LINEAR_V, Vector3(phArticulatedBody::sm_RagdollDampingLinV, phArticulatedBody::sm_RagdollDampingLinV, phArticulatedBody::sm_RagdollDampingLinV));
			pArch->ActivateDamping(phArchetypeDamp::LINEAR_V2, Vector3(phArticulatedBody::sm_RagdollDampingLinV2, phArticulatedBody::sm_RagdollDampingLinV2, phArticulatedBody::sm_RagdollDampingLinV2));
			pArch->ActivateDamping(phArchetypeDamp::ANGULAR_C, Vector3(phArticulatedBody::sm_RagdollDampingAngC, phArticulatedBody::sm_RagdollDampingAngC, phArticulatedBody::sm_RagdollDampingAngC));
			pArch->ActivateDamping(phArchetypeDamp::ANGULAR_V, Vector3(phArticulatedBody::sm_RagdollDampingAngV, phArticulatedBody::sm_RagdollDampingAngV, phArticulatedBody::sm_RagdollDampingAngV));
			pArch->ActivateDamping(phArchetypeDamp::ANGULAR_V2, Vector3(phArticulatedBody::sm_RagdollDampingAngV2, phArticulatedBody::sm_RagdollDampingAngV2, phArticulatedBody::sm_RagdollDampingAngV2));
		}

		// Note the activation time
		m_fActivationStartTime = fwTimer::GetTimeInMilliseconds();

#if LAZY_RAGDOLL_BOUNDS_UPDATE
		// Indicate that the bounds are up to date (since physics is updating them each frame)
		pPed->SetRagdollBoundsUpToDate(true);	
		pPed->RequestRagdollBoundsUpdate(0);  // Resetting the animated update request frames
#endif

		// Not really sure if it should be guaranteed that the instance is already in the level. If this assert fires and we get bugs about it,
		// the verify can be removed and an else block should be added to set the archetype type/include flags instead.
		if(AssertVerify(pReturn->IsInLevel()))
		{
			CPhysics::GetLevel()->SetInstanceTypeFlags(pReturn->GetLevelIndex(), GetTypePhysics()->GetArchetype()->GetTypeFlags());
			CPhysics::GetLevel()->SetInstanceIncludeFlags(pReturn->GetLevelIndex(), GetTypePhysics()->GetArchetype()->GetIncludeFlags());
		}

		phBoundComposite* pNewBound = static_cast<phBoundComposite*>(pReturn->GetArchetype()->GetBound());
		phBoundComposite* pOrigBound = GetTypePhysics()->GetCompositeBounds();
		
		Assertf(pOrigBound->GetNumBounds() == pNewBound->GetNumBounds(), "Bound count in pOrigBound (%d) does not match bound count in pNewBound (%d)", pOrigBound->GetNumBounds(), pNewBound->GetNumBounds());

		for(int i=0; i<pOrigBound->GetNumBounds(); i++)
		{
			if(pNewBound->GetBound(i) && pNewBound->GetBound(i)->GetType()==phBound::CAPSULE
			&& pOrigBound->GetBound(i) && pOrigBound->GetBound(i)->GetType()==phBound::CAPSULE)
			{
				float fOrigRadius = static_cast<phBoundCapsule*>(pOrigBound->GetBound(i))->GetRadius();
				static_cast<phBoundCapsule*>(pNewBound->GetBound(i))->SetCapsuleRadius(fOrigRadius);
			}
		}

		// Inform the vehicle ejection code of any vehicle association
		CheckForVehicleAssociation();

		if(GetNMAgentID()!=-1)
		{
			// Disabling this 'fix' by default now because certain behaviors (pointGun namely) use ITMs even after configureCharacter has been called below so
			// we can't put the ITMs back to how they were...
#if ART_ENABLE_BSPY && 0
			// Looks like there is a bug in bSpy where it is using the current transforms sent for the character configuration as the current transforms for the actual bounds
			// To get around this we resend the current bound transforms after doing the character configuration
			int numChildren = GetTypePhysics()->GetNumChildren();

			// Store the current transforms prior to character configuration (which overwrites the current transforms)
			Matrix34* storedMatrices = Alloca(Matrix34, numChildren);
			Matrix34* currentMatrices = FRAGNMASSETMGR->GetWorldCurrentMatrices(GetNMAgentID());
			memcpy(storedMatrices, currentMatrices, sizeof(Matrix34) * numChildren);
#endif

			bool bWeaponInLeftHand = false;
			bool bWeaponInRightHand = false;

			crSkeletonData *pSkeletonData = pPed->GetDrawable()->GetSkeletonData();
			if (pSkeletonData)
			{
				crSkeleton tempSkeleton;
				tempSkeleton.Init(*pSkeletonData, NULL);

				// ensure matrix is in memory all the time
				Mat34V tempMatrix = this->GetMatrix();
				tempSkeleton.SetParentMtx(&tempMatrix);

				// Grab the target pose for nm to aim for (only need to fill in the local matrices)
				pPed->GetZeroPoseForNM(tempSkeleton, bWeaponInLeftHand, bWeaponInRightHand);
				
				// Update the object space matrices
				tempSkeleton.Update();

#if __BANK
				// Allow incoming transforms to be debugged in-game.
				if(CNmDebug::ms_bDrawTransforms)
				{
					CNmDebug::SetComponentTMsFromSkeleton(tempSkeleton);
				}
#endif // __BANK

				fragInstNM::SetComponentTMsFromSkeleton(tempSkeleton);//name change fragInstNM::ActivePose(*pTempSkeleton);
			}

			// Bias the COM down and widen the stance for armed peds
			static float stanceBiasSWAT = 0.1f;
			static float COMBiasSWAT = -0.05f;
			static float stanceBiasOther = 0.05f;
			static float COMBiasOther = -0.025f;
			float stanceBias = 0.0f;
			float COMBias = 0.0f;

			if (bWeaponInRightHand || bWeaponInLeftHand)
			{
				if (pPed->GetPedType() == PEDTYPE_SWAT)
				{	
					stanceBias = stanceBiasSWAT;
					COMBias = COMBiasSWAT;
				}
				else
				{
					stanceBias = stanceBiasOther;
					COMBias = COMBiasOther;
				}
			}

			ConfigureCharacter(GetNMAgentID(), true, !bWeaponInLeftHand, !bWeaponInRightHand, stanceBias, COMBias);

#if ART_ENABLE_BSPY && 0
			memcpy(currentMatrices, storedMatrices, sizeof(Matrix34) * numChildren);

			ART::gRockstarARTInstance->setComponentTMs(ART::gRockstarARTInstance->getCharacterFromAgentID(GetNMAgentID()),
				currentMatrices,
				numChildren,
				ART::kITSNone,
				ART::kITSourceCurrent);
#endif

			SetARTFeedbackInterfaceGta();
			setProbeTypeIncludeFlags(GetNMAgentID(), ArchetypeFlags::GTA_WEAPON_TYPES);

			//GetBulletForce()->Reset();
		}
	}

	if (*collider)
	{
		// Stop live peds from sleeping
		if((*collider)->GetSleep())
		{
			if (pPed->GetDeathState() != DeathState_Dead)
			{
				(*collider)->GetSleep()->SetActiveSleeper(true);
				(*collider)->GetSleep()->SetVelTolerance2(0.0f);
				(*collider)->GetSleep()->SetAngVelTolerance2(0.0f);
				(*collider)->GetSleep()->SetInternalMotionTolerance2Sum(0.0f);
			}
		}

		if ((*collider)->IsArticulated())
		{
			static int tempDisable = 1;
			static_cast<phArticulatedCollider*>(*collider)->TemporarilyDisableSelfCollision(tempDisable);

			// Initialize the collider velocity to the root link velocity, in case anyone queries it before the collider update runs
			(*collider)->SetVelocityOnly(((phArticulatedCollider*)(*collider))->GetBody()->GetLinearVelocityNoProp(0).GetIntrin128ConstRef());
			(*collider)->SetAngVelocityOnly(((phArticulatedCollider*)(*collider))->GetBody()->GetAngularVelocityNoProp(0).GetIntrin128ConstRef());
		}
	}

	pPed->GetAnimDirector()->PoseRagdollFrameFromCreature(*pPed->GetAnimDirector()->GetCreature());

	// Set the skeleton back to the stored ragdoll frame root
	// we need to do this as the root bone may have been changed
	// since the last call to PoseBoundsFromSkeleton
	if (pPed->GetAnimDirector() && pPed->GetCreature() && pPed->GetSkeleton())
	{
		fwAnimDirectorComponentRagDoll* comp = static_cast<fwAnimDirectorComponentRagDoll*>(pPed->GetAnimDirector()->GetComponentByType(fwAnimDirectorComponent::kComponentTypeRagDoll));
		if(comp)
		{
			comp->PoseSkelRootFromRagdollRootFrame(*pPed->GetCreature());
			pPed->GetSkeleton()->Update();
		}
	}

	// Reset the corpse friction override
	pPed->SetCorpseRagdollFriction(-1.0f);

#if __BANK
	// Collect data regarding how many ragdolls are used
	if (ms_bLogUsedRagdolls)
	{
		CheckRagdollsUsedDebug();
	}
#endif

	return pReturn;

}

bool fragInstNMGta::PrepareForDeactivation(bool colliderManagedBySim, bool forceDeactivate)
{
	//Displayf("Ragdoll Deactivated (frame:%d)\n", fwTimer::GetFrameCount());
	bool bReturn = fragInstNM::PrepareForDeactivation(colliderManagedBySim, forceDeactivate);

	// Reset the activation time
	m_fActivationStartTime = 0;

	// If a ped is being deactivated due to being asleep, add it to a queue so that it can be switched to animation in post-physics
	CPed* pPed = (CPed*)GetUserData();
	nmEntityDebugf(pPed, "fragInstNMGta::PrepareForDeactivation - Deactivating ragdoll %s. %s", bReturn ? "SUCCESSFUL" : "FAILED", pPed && pPed->GetCollider() && pPed->GetCollider()->GetSleep()->IsAsleep() ? "Ragdoll is asleep" : "Not sleeping");
	if (pPed && pPed->GetCollider() && pPed->GetCollider()->GetSleep()->IsAsleep())
	{
		Assert(g_SettledPeds.GetCount() < g_SettledPeds.GetMaxCount());
		if (g_SettledPeds.GetCount() < g_SettledPeds.GetMaxCount())
			g_SettledPeds.Push(RegdPed(pPed));
	}

	if (GetCached())
	{
		GetCacheEntry()->ActivateInstabilityPrevention(-1);
	}

	return bReturn;
}

void fragInstNMGta::PoseBoundsFromSkeleton(bool onlyAdjustForInternalMotion, bool updateLastBoundsMatrices, bool, s8, const s8*)
{
	GrabRootFrame();

	if (fragCacheEntry* entry = GetCacheEntry())
	{
		if (updateLastBoundsMatrices)
			entry->GetBound()->UpdateLastMatricesFromCurrent();
		
		const fragPhysicsLOD* physicsLOD = GetTypePhysics();
		const u32* componentToBoneIndex = reinterpret_cast<const u32*>(entry->GetComponentToBoneIndexMap());
		u32 numChildren = physicsLOD->GetNumChildren();
		fragTypeChild** child = physicsLOD->GetAllChildren();
		const Mat34V* objectMtxs = GetSkeleton()->GetObjectMtxs();
		Mat34V* currentBound = const_cast<Mat34V*>(entry->GetBound()->GetCurrentMatrices());

		u32 count4 = numChildren >> 2;
		for (; count4; count4--)
		{
			Mat34V instanceFromBone0 = objectMtxs[componentToBoneIndex[0]];
			Mat34V instanceFromBone1 = objectMtxs[componentToBoneIndex[1]];
			Mat34V instanceFromBone2 = objectMtxs[componentToBoneIndex[2]];
			Mat34V instanceFromBone3 = objectMtxs[componentToBoneIndex[3]];

			// TODO - please give me an array of bound matrices
			Mat34V boneFromComponent0 = RCC_MAT34V((child[0])->GetUndamagedEntity()->GetBoundMatrix());
			Mat34V boneFromComponent1 = RCC_MAT34V((child[1])->GetUndamagedEntity()->GetBoundMatrix());
			Mat34V boneFromComponent2 = RCC_MAT34V((child[2])->GetUndamagedEntity()->GetBoundMatrix());
			Mat34V boneFromComponent3 = RCC_MAT34V((child[3])->GetUndamagedEntity()->GetBoundMatrix());

			Transform(currentBound[0], instanceFromBone0, boneFromComponent0);
			Transform(currentBound[1], instanceFromBone1, boneFromComponent1);
			Transform(currentBound[2], instanceFromBone2, boneFromComponent2);
			Transform(currentBound[3], instanceFromBone3, boneFromComponent3);

			componentToBoneIndex += 4;
			currentBound += 4;
			child += 4;
		}
		
		u32 count1 = numChildren & 3;
		for (; count1; count1--)
		{
			Mat34V instanceFromBone = objectMtxs[*componentToBoneIndex];
			Mat34V boneFromComponent = RCC_MAT34V((*child)->GetUndamagedEntity()->GetBoundMatrix());
			Transform(*currentBound, instanceFromBone, boneFromComponent);

			componentToBoneIndex += 1;
			currentBound += 1;
			child += 1;
		}

		// MIGRATE_FIXME - the fixed true value passed into this can now be drived by the argument
		phBoundComposite *bnd = entry->GetBound();
		bnd->CalculateCompositeExtents(onlyAdjustForInternalMotion, true);

		//Revisit this later
		//Updating the physics level should really NOT be done from here -- instead the calling code should handle it itself
		//since not every call to PoseBoundsFromSkeelton would require a level update
		Assert(IsInLevel());
		const phArchetype* pArch = GetArchetype();
		if(pArch)
		{
			const phBound* pInstBound = pArch->GetBound();
			if(pInstBound == bnd)
			{
				const int levelIndex = GetLevelIndex();
				PHLEVEL->UpdateObjectLocationAndRadius(levelIndex, (Mat34V_Ptr)(NULL));
			}
		}
	}
}

void fragInstNMGta::GrabRootFrame() 
{ 
	CPed* pPed = (CPed*)GetUserData();
	if (pPed)
	{
		m_RootBonePosAtBoundsUpdate = GetSkeleton()->GetLocalMtx(0).GetCol3();

		// pose the ragdoll frame root. We use this to keep the root bone posed correctly
		// when we switch to ragdoll (as it is not mapped directly to a ragdoll bound).
		if (pPed->GetAnimDirector() && pPed->GetCreature())
		{
			fwAnimDirectorComponentRagDoll* comp = static_cast<fwAnimDirectorComponentRagDoll*>(pPed->GetAnimDirector()->GetComponentByType(fwAnimDirectorComponent::kComponentTypeRagDoll));
			if(comp)
			{
				comp->PoseRagdollRootFrame(*pPed->GetCreature());
			}
		}
	}
}


void fragInstNMGta::SetNMAssetSize(int newSize)
{
	int currentAssetID = GetARTAssetID();
	if (currentAssetID < 0)
	{
		Assertf(0, "fragInstNMGta::SwitchNMAssetSize() called on a non-NM agent");
		return;
	}

	// Determine what the new asset ID should be (based on gender and current size)
	int currentSize = currentAssetID <= 1 ? 0 : 1;
	if (currentSize == newSize)
	{
		Warningf("fragInstNMGta::SwitchNMAssetSize() - Input size is already the current size");
		return;
	}

	// Determine what the new asset ID should be (based on gender and current size)
	int newID = 0;
	if (currentAssetID == 0 || currentAssetID == 2)
		newID = currentAssetID == 0 ? 2 : 0;
	else
		newID = currentAssetID == 1 ? 3 : 1;

	// Set the new art asset ID, physics type data, and ragdoll LOD
	Displayf("Switching NM art asset ID to %d.", newID);
	((fragType*) GetType())->SetARTAssetID(newID);
	((fragType*) GetType())->SetPhysicsLODGroup(CPhysics::GetManagedPhysicsLODGroup(GetType()->GetARTAssetID()).GetPhysicsLODGroup());
	m_CurrentLOD = RAGDOLL_LOD_HIGH; 

	PHSIM->GetContactMgr()->RemoveAllContactsWithInstance(this);

	// Need to do more if there's a cache entry
	fragCacheEntry* cacheEntry = GetCacheEntry();
	if (cacheEntry)
	{
		cacheEntry->ReloadPhysicsData();
		ASSERT_ONLY(cacheEntry->GetBound()->CheckCachedMinMaxConsistency());
	}
}

void fragInstNMGta::SetCurrentPhysicsLOD(ePhysicsLOD newLOD) 
{ 
	CPed* pPed = (CPed*)GetUserData();
	if (pPed)
	{
		pPed->RemoveConstraints(this);
	}

	fragInst::SetCurrentPhysicsLOD(newLOD);
}

void fragInstNMGta::CheckForVehicleAssociation()
{
	CPed* pPed = (CPed*)GetUserData();
	if (pPed)
	{
		CVehicle *pVehicle = pPed->GetMyVehicle();
		// Only allow associating with a NULL vehicle if we just left - otherwise peds that are running to a car with CPED_RESET_FLAG_IsEnteringOrExitingVehicle set will get
		//  an association. Once SetAssociatedVehicle(NULL) gets called we will become "ejected" from the next vehicle we touch since IsAssociatedWithVehicle will return true
		//  always. 
		if(pVehicle || pPed->GetHasJustLeftVehicle())
		{
			bool isInvalidVehicleForEjection = pVehicle && (pVehicle->GetVehicleType() != VEHICLE_TYPE_CAR);

			if (pVehicle && !isInvalidVehicleForEjection)
			{
				for( s32 iSeat = 0; iSeat < pVehicle->GetSeatManager()->GetMaxSeats(); iSeat++ )
				{
					const CVehicleSeatAnimInfo* pSeatAnimInfo = pVehicle->GetSeatAnimationInfo(iSeat);
					Assert(pSeatAnimInfo);
					if(pSeatAnimInfo && pSeatAnimInfo->GetCanDetachViaRagdoll())
					{
						isInvalidVehicleForEjection = true;
					}
				}
			}

			if (!isInvalidVehicleForEjection && (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) || pPed->GetHasJustLeftVehicle() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringOrExitingVehicle)))
			{
				SetAssociatedWithVehicle(pVehicle);
			}
		}
	}
}

void fragInstNMGta::SetAssociatedWithVehicle(CVehicle *pVehicle) 
{ 
	m_VehicleBeingEjectedFrom = pVehicle; 
	if (m_EjectedFromVehicleFrames < 0)
	{
		m_EjectedFromVehicleFrames = -3;
	}
};

bool fragInstNMGta::IsAssociatedWithVehicle(CVehicle *pVehicle) const 
{ 
	bool bIsAssociated = false;

	if (!m_VehicleBeingEjectedFrom || (m_VehicleBeingEjectedFrom && m_VehicleBeingEjectedFrom == pVehicle))
	{
		bIsAssociated = m_EjectedFromVehicleFrames < -1; 
	}

	return bIsAssociated;
}

void fragInstNMGta::SetRagdollEjectedFromVehicle(CVehicle *vehicle) 
{ 
	m_VehicleBeingEjectedFrom = vehicle; 
	m_EjectedFromVehicleFrames = 30; 
};

void fragInstNMGta::ProcessVehicleEjection(float fTimeStep)
{
	// Check if we're associated with a vehicle, and if so increment - this ensures that we get a couple frames of association
	if (m_EjectedFromVehicleFrames < -1)
	{
		m_EjectedFromVehicleFrames++;
	}
	else if (m_EjectedFromVehicleFrames > -1) // other wise decrement
	{
		m_EjectedFromVehicleFrames--;
	}

	// If the vehicle you're ejecting from is a train - push to the side
	if (IsEjectingFromVehicle() && GetEjectingVehicle() && GetEjectingVehicle()->InheritsFromTrain())
	{
		// Update general ragdoll related items here
		CPed *pPed = (CPed *)GetUserData();

		Vec3V vPushDir;
		vPushDir = Cross(RCC_VEC3V(GetEjectingVehicle()->GetVelocity()), Vec3V(V_UP_AXIS_WONE));
		vPushDir = NormalizeSafe(vPushDir, Vec3V(V_ZERO));
		Vec3V vecCarToPed = Subtract(pPed->GetTransform().GetPosition(), GetEjectingVehicle()->GetTransform().GetPosition());
		ScalarV vDot = Dot(vecCarToPed, vPushDir);
		static float sfPushMult = 10.0f;
		float integratedPushMult = fTimeStep * sfPushMult;
		if (vDot.Getf() < 0.0f)
		{
			integratedPushMult *= -1.0f;
		}
		vPushDir = Scale(vPushDir, ScalarVFromF32(integratedPushMult));

		phArticulatedBody *body = pPed->GetRagdollInst()->GetArticulatedBody();
		for (int iLink = 0; iLink < body->GetNumBodyParts(); iLink++)
		{
			if (GetBodyPartGroup(iLink) == fragInstNMGta::kSpine)
			{
				body->ApplyImpulse(iLink, Scale(vPushDir, body->GetMass(iLink)), Vec3V(V_ZERO));
			}
		}
	}
}

void fragInstNMGta::ResetVehicleEjectionMembers()
{
	m_EjectedFromVehicleFrames = -1; 
	m_VehicleBeingEjectedFrom = NULL;
}


#if __BANK
void fragInstNMGta::CheckRagdollsUsedDebug()
{
	// Collect data regarding how many NM agents are used
	int numActiveAgents = FRAGNMASSETMGR->GetAgentCapacity(0) - FRAGNMASSETMGR->GetAgentCount(0);
	int numActiveRageRagdolls = FRAGNMASSETMGR->GetNumActiveNonNMRagdolls();
	if (CTheScripts::GetPlayerIsOnAMission() && numActiveAgents > ms_MaxNMAgentsUsedCurrentLevel)
	{
		Displayf("[RAGDOLL WATERMARK] [%u] The maximum number of NM AGENTS used during [%s] has increased to %d.", 
			fwTimer::GetTimeInMilliseconds(), CDebug::GetCurrentMissionName(), numActiveAgents);
		ms_MaxNMAgentsUsedCurrentLevel = numActiveAgents;
	}
	if (numActiveAgents > ms_MaxNMAgentsUsedGlobally)
	{
		Displayf("[RAGDOLL WATERMARK] [%u] The maximum number of NM AGENTS used GLOBALLY has increased to %d.", 
			fwTimer::GetTimeInMilliseconds(), numActiveAgents);
		ms_MaxNMAgentsUsedGlobally = numActiveAgents;
	}
	if (CTheScripts::GetPlayerIsOnAMission() && numActiveRageRagdolls > ms_MaxRageRagdollsUsedCurrentLevel)
	{
		Displayf("[RAGDOLL WATERMARK] [%u] The maximum number of RAGE RAGDOLLS used during [%s] has increased to %d.", 
			fwTimer::GetTimeInMilliseconds(), CDebug::GetCurrentMissionName(), numActiveRageRagdolls);
		ms_MaxRageRagdollsUsedCurrentLevel = numActiveRageRagdolls;
	}
	if (numActiveRageRagdolls > ms_MaxRageRagdollsUsedGlobally)
	{
		Displayf("[RAGDOLL WATERMARK] [%u] The maximum number of RAGE RAGDOLLS used GLOBALLY has increased to %d.", 
			fwTimer::GetTimeInMilliseconds(), numActiveRageRagdolls);
		ms_MaxRageRagdollsUsedGlobally = numActiveRageRagdolls;
	}
	if (CTheScripts::GetPlayerIsOnAMission() && numActiveAgents + numActiveRageRagdolls > ms_MaxTotalRagdollsUsedCurrentLevel)
	{
		Displayf("[RAGDOLL WATERMARK] [%u] The peak usage number of ragdolls used simultaneously during [%s] has increased to %d.  %d of those were NM agents and %d were rage ragdolls.",
			fwTimer::GetTimeInMilliseconds(), CDebug::GetCurrentMissionName(), numActiveAgents + numActiveRageRagdolls, numActiveAgents, numActiveRageRagdolls);
		ms_MaxTotalRagdollsUsedCurrentLevel = numActiveAgents + numActiveRageRagdolls;
		ms_MaxNMAgentsUsedInComboCurrentLevel = numActiveAgents;
		ms_MaxRageRagdollsUsedInComboCurrentLevel = numActiveRageRagdolls;
	}
	if (numActiveAgents + numActiveRageRagdolls > ms_MaxTotalRagdollsUsedGlobally)
	{
		Displayf("[RAGDOLL WATERMARK] [%u] The peak usage number of ragdolls used simultaneously GLOBALLY has increased to %d.  %d of those were NM agents and %d were rage ragdolls.",
			fwTimer::GetTimeInMilliseconds(), numActiveAgents + numActiveRageRagdolls, numActiveAgents, numActiveRageRagdolls);
		ms_MaxTotalRagdollsUsedGlobally = numActiveAgents + numActiveRageRagdolls;
		ms_MaxNMAgentsUsedInComboGlobally = numActiveAgents;
		ms_MaxRageRagdollsUsedInComboGlobally = numActiveRageRagdolls;
	}
}
#endif

bool fragInstNMGta::ShouldBeInCacheToDraw() const
{
	return true;
}


void fragInstNMGta::LosingCacheEntry()
{
	Assertf(GetInstFlag(phInstGta::FLAG_BEING_DELETED) || !GetUserData(), "%s:fragInstNM - lost cache entry", ((CEntity*)GetUserData())->GetModelName());
	if(CEntity* pEntity = CPhysics::GetEntityFromInst(this))
	{
		pEntity->LosingFragCacheEntry();
	}
	fragInstNM::LosingCacheEntry();
}

fragCacheEntry* fragInstNMGta::PutIntoCache()
{
	fragCacheEntry* newEntry = fragInstNM::PutIntoCache();
	if(newEntry)
	{
		if(CEntity* pEntity = CPhysics::GetEntityFromInst(this))
		{
			pEntity->GainingFragCacheEntry(*newEntry);
		}
	}
	return newEntry;
}

// ragdolls never break apart
bool fragInstNMGta::FindBreakStrength(const Vector3* UNUSED_PARAM(componentImpulses), const Vector4* UNUSED_PARAM(componentPositions), float* UNUSED_PARAM(breakStrength), phBreakData* UNUSED_PARAM(breakData)) const
{
	return false;
}

bool fragInstNMGta::IsBreakable(phInst* ) const
{
	return false;
}


void fragInstNMGta::SetActivePoseFromClip(const crClip* pClip, const float fPhase, bool UNUSED_PARAM(bUseWorldCoords))
{
	// If bUseWorldCoords is set to true (default is false), transform the matrices by the ped's current
	// position and orientation before passing to NM.

	Matrix34 parentMatrix;
	parentMatrix.Identity();
	CPed* pPed = (CPed*)GetUserData();
	if(pPed && pClip)
	{
		// Create skeleton
		crSkeletonData *pSkeletonData = pPed->GetDrawable()->GetSkeletonData();
		if (pSkeletonData)
		{
			crSkeleton* pTempSkeleton = rage_new crSkeleton();
			pTempSkeleton->Init(*pSkeletonData, NULL);

			// ensure matrix is in memory all the time
			Mat34V tempMatrix = this->GetMatrix();
			pTempSkeleton->SetParentMtx(&tempMatrix);

			pPed->GetAnimDirector()->PoseSkeletonUsingRagdollFrame(*pTempSkeleton, pClip, fPhase);
			pTempSkeleton->Update();

#if __BANK
			// Allow incoming transforms to be debugged in-game.
			if(CNmDebug::ms_bDrawTransforms)
			{
				CNmDebug::SetComponentTMsFromSkeleton(*pTempSkeleton);
			}
#endif // __BANK

			fragInstNM::SetComponentTMsFromSkeleton(*pTempSkeleton);//name change fragInstNM::ActivePose(*pTempSkeleton);

			delete pTempSkeleton;
		}
	}
}

void fragInstNMGta::SetIncomingTransformsFromAnim(const crClip* pClip, const float fPhase)
{
	fragInstNM::SetLastComponentTMsFromCurrent();

	CPed* pPed = (CPed*)GetUserData();
	if(pPed && pClip)
	{
		// Create skeleton
		crSkeletonData *pSkeletonData = pPed->GetDrawable()->GetSkeletonData();
		if (pSkeletonData)
		{
			crSkeleton* pTempSkeleton = rage_new crSkeleton();
			pTempSkeleton->Init(*pSkeletonData, NULL);

			// ensure matrix is in memory all the time
			Mat34V tempMatrix = this->GetMatrix();
			pTempSkeleton->SetParentMtx(&tempMatrix);

			pPed->GetAnimDirector()->PoseSkeletonUsingRagdollFrame(*pTempSkeleton, pClip, fPhase);
			pTempSkeleton->Update();

			fragInstNM::SetComponentTMsFromSkeleton(*pTempSkeleton);//name change fragInstNM::ActivePose(*pTempSkeleton);

			delete pTempSkeleton;
		}
	}
}

void fragInstNMGta::SetActivePoseFromSkel(const crSkeleton* pSkel)
{
	CPed* pPed = (CPed*)GetUserData();
	if(pPed && pSkel)
	{
		// Create a temp skeleton
		crSkeleton* pTempSkeleton = rage_new crSkeleton();
		if (pTempSkeleton)
		{
			pTempSkeleton->Init(pSkel->GetSkeletonData(), NULL);
			pTempSkeleton->CopyFrom(*pSkel);
			pTempSkeleton->Update();

			// ensure matrix is in memory all the time
			Mat34V tempMatrix(V_IDENTITY);
			pTempSkeleton->SetParentMtx(&tempMatrix);

			fragInstNM::SetComponentTMsFromSkeleton(*pTempSkeleton);
			delete pTempSkeleton;
		}
	}
}


Vec3V_Out fragInstNMGta::GetExternallyControlledVelocity () const
{
	Vec3V velocity(V_ZERO);

	CEntity* pEntity = (CEntity*)GetUserData();
	if(pEntity)
	{
		phInst* pInst = pEntity->GetCurrentPhysicsInst();
		phCollider* pCollider = (pInst && pInst->IsInLevel() && pInst != this) ? CPhysics::GetSimulator()->GetCollider(pInst) : nullptr;
		if(pCollider)
		{
			velocity = pCollider->GetVelocity();
		}
		else
		{
			bool bInstActive = PHLEVEL->LegitLevelIndex(GetLevelIndex()) && PHLEVEL->IsActive(GetLevelIndex());
			if (!bInstActive && pEntity->GetIsPhysical())
			{
				CPhysical* pPhysical = (CPhysical*)pEntity;
				// ***HACK***
				// This logic should probably be contained within CPhysical::GetVelocity but it too late in the project to make such sweeping changes.
				// Here (and the equivalent function in phInstGta) seemed to be the only place where this logic was required (although there are probably
				// other areas that would benefit) so the change was made here.
				if(pPhysical->GetIsTypePed() && pPhysical->GetIsAttached() && !pPhysical->GetIsAttachedToGround() && pPhysical->GetAttachParent() != NULL && static_cast<CEntity*>(pPhysical->GetAttachParent())->GetIsPhysical())
				{
					pPhysical->CalculateGroundVelocity(static_cast<CPhysical*>(pPhysical->GetAttachParent()), pPhysical->GetTransform().GetPosition(), 0, velocity);
				}
				else
				{
					velocity = RCC_VEC3V(pPhysical->GetVelocity());
				}
			}
		}
	}

	return velocity;
}

void fragInstNMGta::SetARTFeedbackInterfaceGta()
{
	fragInstNM::SetARTFeedbackInterface(&m_feedbackInterfaceGta);
}

// transform magnitude of impulse to equalize the effect over the body
// nComponent is the ragdoll component the force will be applied to
// fMassInfluence is how much the mass of the component will influence the magnitude (0.0 == original force, 1.0f == completely scaled by mass)
void fragInstNMGta::ScaleImpulseByComponentMass(Vector3& vecImpulse, int nComponent, float fMassInfluence)
{
	vecImpulse *= ScaleImpulseByComponentMass(nComponent, fMassInfluence);
}

float fragInstNMGta::ScaleImpulseByComponentMass(int nComponent, float fMassInfluence)
{
	float fAverageMass = GetTypePhysics()->GetTotalUndamagedMass() / GetTypePhysics()->GetNumChildren();
	float fMassProportion = GetTypePhysics()->GetAllChildren()[nComponent]->GetUndamagedMass() / fAverageMass;
	fMassProportion = rage::Clamp(fMassProportion, 0.2f, 1.5f);

	return (fMassProportion*fMassInfluence + 1.0f*(1.0f-fMassInfluence));
}

void fragInstNMGta::ApplyBulletForce(CPed* pPed, float fMomentum, const Vector3& vecWorldNormal, const Vector3& vecWorldPos, int nComponent, u32 nWeaponHash)
{
	// If the ped isn't dead, don't allow the impulse modifier to exceed 1.0
	if (pPed->GetDeathState() != DeathState_Dead)
		m_ImpulseModifier = Min(1.0f, m_ImpulseModifier); 

	// The impulse of rapid firing should diminish over a short time (except for shotgun blasts)
	static float initialHitThreshold = 0.98f;
	bool bIsInitialHit = m_ImpulseModifier >= CTaskRageRagdoll::GetMaxImpulseModifier()*initialHitThreshold;
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(nWeaponHash);
	if (!(pWeaponInfo && pWeaponInfo->GetGroup() == WEAPONGROUP_SHOTGUN))
	{
		if (m_PendingImpulseModifier != -1.0f)
		{
			m_ImpulseModifier = m_PendingImpulseModifier;
			m_PendingImpulseModifier = -1.0f;
		}
		fMomentum *= m_ImpulseModifier;
		m_ImpulseModifier = Max(CTaskRageRagdoll::GetMinImpulseModifier(), m_ImpulseModifier - CTaskRageRagdoll::GetImpulseReductionPerShot()); 
	}

	// Clamp the bullet force (actually an impulse) we are going to apply so that we don't send non-human animals
	// like chickens flying across the map. The dev_float below defines the maximum velocity change that the
	// computed impulse is allowed to generate.
	static float s_smallMass = 50.0f;
	if (GetARTAssetID() == -1 && pPed->GetMass() <= s_smallMass)
	{
		static dev_float fMaxImpVel = 2.0;
		float fMass = rage::Max(1.0f, pPed->GetMass());
		if(fMomentum/fMass > fMaxImpVel)
			fMomentum = fMaxImpVel*fMass;
	}

	// Modify impulses to animals
	if (GetARTAssetID() == -1)
	{
		phArticulatedBody *body = GetArticulatedBody();
		float fMass = body->GetMass(nComponent).Getf();
		fMomentum *= Min(Max(CTaskRageRagdoll::GetAnimalImpulseMultMin(), CTaskRageRagdoll::GetAnimalMassMult() * fMass), CTaskRageRagdoll::GetAnimalImpulseMultMax());
	}

#if __DEV
	if (CTaskNMBehaviour::sm_DoOverrideBulletImpulses)
		fMomentum = CTaskNMBehaviour::sm_OverrideImpulse;
#endif

	// First check for a pending impulse and apply it
	if (m_CounterImpulseCount >= 0)
	{
		m_CounterImpulseCount = 1;
		UpdateCounterImpulse(pPed, true);
	}

	// Get a safe component (can become invalidated when ragdoll lods change)
	phArticulatedBody *body = GetArticulatedBody();
	if (body)
		nComponent = Clamp(nComponent, 0, body->GetNumBodyParts()-1);

	// Store wound data in the ped
	CPed::WoundData wound;
	wound.valid = true;
	wound.component = nComponent;
	wound.localHitLocation = vecWorldPos; 
	wound.localHitNormal = vecWorldNormal; 
	wound.impulse = fMomentum;

	bool localHitPos = false;

	// Get the position and normal in local space
	Matrix34 ragdollComponentMatrix;
	if(pPed && pPed->GetRagdollComponentMatrix(ragdollComponentMatrix, nComponent))
	{
		ragdollComponentMatrix.UnTransform(wound.localHitLocation);
		Assert(wound.localHitLocation.Mag2() < 5.0f);
		ragdollComponentMatrix.UnTransform3x3(wound.localHitNormal);
		wound.localHitNormal.Normalize();
		localHitPos = true;
	}
		
	pPed->AddWoundData(wound, NULL, true);

	m_LastRRApplyBulletTime = fwTimer::GetTimeInMilliseconds();

	// Lower joint stiffnesses for half a second each time a bullet comes it
	if (body)
	{
		body->SetStiffness(CTaskRageRagdoll::GetTempInitialStiffnessWhenShot());
		m_bBulletLoosenessActive = true;
	}

	// Apply half the impulse as a counter impulse
	pPed->ApplyImpulse(fMomentum*vecWorldNormal*(-1.0f * CTaskRageRagdoll::GetCounterImpulseRatio()), localHitPos ? wound.localHitLocation : vecWorldPos - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), nComponent, localHitPos);
	m_CounterImpulseCount = 2;

	// If this is an initial hit, double the impulse
	if (bIsInitialHit)
	{
		pPed->GetPrimaryWoundData()->impulse *= CTaskRageRagdoll::GetInitialHitImpulseMult();
	}
}

void fragInstNMGta::UpdateCounterImpulse(CPed* pPed, bool UNUSED_PARAM(bHurriedCounterImpulse))
{
	m_CounterImpulseCount = Max(-1, m_CounterImpulseCount-1);
	if (m_CounterImpulseCount == 0)
	{
		// Get a safe component (can become invalidated when ragdoll lods change)
		int nComponent = pPed->GetPrimaryWoundData()->component;
		phArticulatedBody *body = GetArticulatedBody();
		if (body)
			nComponent = Clamp(nComponent, 0, body->GetNumBodyParts()-1);

		// Get the hit position and normal in world space
		Vector3 vecWorldHitPos, vecWorldHitNorm;
		Matrix34 ragdollComponentMatrix;
		bool localHitPos = false;
		if(pPed && pPed->GetRagdollComponentMatrix(ragdollComponentMatrix, nComponent))
		{
			ragdollComponentMatrix.Transform(pPed->GetPrimaryWoundData()->localHitLocation, vecWorldHitPos);
			ragdollComponentMatrix.Transform3x3(pPed->GetPrimaryWoundData()->localHitNormal, vecWorldHitNorm);
			vecWorldHitNorm.Normalize();
			localHitPos = true;
		}

		pPed->ApplyImpulse(pPed->GetPrimaryWoundData()->impulse * vecWorldHitNorm, 
			localHitPos ? pPed->GetPrimaryWoundData()->localHitLocation : vecWorldHitPos - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), 
			nComponent, localHitPos);

		m_CounterImpulseCount = -1;
	}
}

void fragInstNMGta::ProcessRagdoll(float fTimeStep)
{
	// Update general ragdoll related items here
	CPed *pPed = (CPed *)GetUserData();

	// Reduce linear damping if falling for a more realistic falling speed
	static float fallingSpeedLim = -10.0f;
	static float terminalVelocity = -55.0f;
	float fallingSpeed = pPed->GetCollider()->GetVelocity().GetZf();
	if (fallingSpeed <= fallingSpeedLim && fallingSpeed >= terminalVelocity)
	{
		static_cast<phArticulatedCollider*>(pPed->GetCollider())->SetReducedLinearDampingFrames();
	}

	pPed->GetRagdollInst()->ProcessVehicleEjection(fTimeStep);

	// Update params related to contacting a wheel
	if (!m_ContactedWheel)
	{
		if(m_WheelTwitchStarted)
		{
			static_cast<phArticulatedCollider*>(pPed->GetCollider())->GetBody()->GetJoint(RAGDOLL_KNEE_LEFT_JOINT).SetMinStiffness(0.0f);
			static_cast<phArticulatedCollider*>(pPed->GetCollider())->GetBody()->GetJoint(RAGDOLL_KNEE_RIGHT_JOINT).SetMinStiffness(0.0f);
		}

		m_WheelTwitchStarted = false;
		m_uTimeTireImpulseWasApplied = 0;
		m_fTireImpulseAppliedRatio = 0.0f;
	}
	else
	{
		if(m_WheelTwitchStarted && pPed->GetDeathState() == DeathState_Dead)
		{
			static dev_float sfKneeWheelTwitchStiffness = 0.95f;
			static_cast<phArticulatedCollider*>(pPed->GetCollider())->GetBody()->GetJoint(RAGDOLL_KNEE_LEFT_JOINT).SetMinStiffness(sfKneeWheelTwitchStiffness);
			static_cast<phArticulatedCollider*>(pPed->GetCollider())->GetBody()->GetJoint(RAGDOLL_KNEE_RIGHT_JOINT).SetMinStiffness(sfKneeWheelTwitchStiffness);
		}
	}
	m_ContactedWheel = false;

	// If being pressed down by a train, prevent instability 
	if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsTrainCrushingRagdoll))
	{
		pPed->GetRagdollInst()->GetCacheEntry()->ActivateInstabilityPrevention(60);
	}

	// Rage ragdoll processing
	if (GetNMAgentID() == -1)
	{
		phArticulatedBody *body = GetArticulatedBody();

		// Restore normal joint stiffness if enough time has passed since getting shot
		static u32 loosenessTime = 500u;
		if (m_bBulletLoosenessActive && (fwTimer::GetTimeInMilliseconds() - m_LastRRApplyBulletTime > loosenessTime) && body)
		{
			static float stiff = 0.5f;
			body->SetStiffness(stiff);
			m_bBulletLoosenessActive = false;
			pPed->SetCorpseRagdollFriction(-1.0f);
		}

		// Update allowed impulse recovery
		m_ImpulseModifier = Min(CTaskRageRagdoll::GetMaxImpulseModifier(), m_ImpulseModifier + fTimeStep * CTaskRageRagdoll::GetImpulseRecoveryPerSecond()); 

		UpdateCounterImpulse(pPed);

		// Assign non-NM agent ragdolls to the rage ragdoll pool if there's room (and they're not already assigned there)
		if (pPed->GetUsingRagdoll() && pPed->GetRagdollInst()->GetNMAgentID() == -1 && pPed->GetCurrentRagdollPool() != CTaskNMBehaviour::kRagdollPoolRageRagdoll && 
			CTaskNMBehaviour::GetRagdollPool(CTaskNMBehaviour::kRagdollPoolRageRagdoll).GetFreeSlots()>0)
		{
			CTaskNMBehaviour::AddToRagdollPool(CTaskNMBehaviour::kRagdollPoolRageRagdoll, *pPed);
		}

		// Handle corpse friction
		if (m_bBulletLoosenessActive)
		{
			static float f1 = 2.0f;
			pPed->SetCorpseRagdollFriction(f1);
		}
	}
}


ARTFeedbackInterfaceGta::ARTFeedbackInterfaceGta()
:	ART::ARTFeedbackInterface(),
	m_pParentInst(NULL)
{
}

ARTFeedbackInterfaceGta::~ARTFeedbackInterfaceGta()
{
}

int ARTFeedbackInterfaceGta::onBehaviourFailure()
{
	if(GetParentInst() && GetParentInst()->GetUserData())
	{
		CPed* pPed = (CPed*)GetParentInst()->GetUserData();
		CTaskNMBehaviour* pTaskNM = pPed->GetPedIntelligence()->GetLowestLevelNMTask(pPed);

		if(pTaskNM)
		{
			pTaskNM->BehaviourFailure(pPed, this);
			return 1;
		}
	}
	else
		Displayf("\nbehaviour failure - unknown ped\n");

	return 0;
}

int ARTFeedbackInterfaceGta::onBehaviourSuccess()
{
	if(GetParentInst() && GetParentInst()->GetUserData())
	{
		CPed* pPed = (CPed*)GetParentInst()->GetUserData();
		CTaskNMBehaviour* pTaskNM = pPed->GetPedIntelligence()->GetLowestLevelNMTask(pPed);

		if(pTaskNM)
		{
			pTaskNM->BehaviourSuccess(pPed, this);
			return 1;
		}
	}
	else
		Displayf("\nbehaviour success - unknown ped\n");

	return 0;
}

int ARTFeedbackInterfaceGta::onBehaviourStart()
{
	if(GetParentInst() && GetParentInst()->GetUserData())
	{
		CPed* pPed = (CPed*)GetParentInst()->GetUserData();
		CTaskNMBehaviour* pTaskNM = pPed->GetPedIntelligence()->GetLowestLevelNMTask(pPed);

		if(pTaskNM)
		{
			pTaskNM->BehaviourStart(pPed, this);
			return 1;
		}
	}
	else
		Displayf("\nbehaviour start - unknown ped\n");

	return 0;
}

int ARTFeedbackInterfaceGta::onBehaviourFinish()
{
	if(GetParentInst() && GetParentInst()->GetUserData())
	{
		CPed* pPed = (CPed*)GetParentInst()->GetUserData();
		CTaskNMBehaviour* pTaskNM = pPed->GetPedIntelligence()->GetLowestLevelNMTask(pPed);

		if(pTaskNM)
		{
			pTaskNM->BehaviourFinish(pPed, this);
			return 1;
		}
	}
	else
		Displayf("\nbehaviour finish - unknown ped\n");

	return 0;
}

int ARTFeedbackInterfaceGta::onBehaviourEvent()
{
	if(GetParentInst() && GetParentInst()->GetUserData())
	{
		CPed* pPed = (CPed*)GetParentInst()->GetUserData();
		CTaskNMBehaviour* pTaskNM = pPed->GetPedIntelligence()->GetLowestLevelNMTask(pPed);

		if(pTaskNM)
		{
			pTaskNM->BehaviourEvent(pPed, this);
			return 1;
		}
	}
	else
		Displayf("\nbehaviour event - unknown ped\n");

	return 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
fragInstNMGta::eBodyPartGroup fragInstNMGta::GetBodyPartGroup(int component)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CPed *pPed = (CPed *)GetUserData();
	if (pPed)
	{
		eAnimBoneTag boneTag = pPed->GetBoneTagFromRagdollComponent(component);

		// Check for neck
		if (boneTag == BONETAG_NECK || boneTag == BONETAG_NECK2 || boneTag == BONETAG_HEAD)
		{
			return fragInstNMGta::kNeck;
		}
		// Check for the spine
		else if (boneTag == BONETAG_ROOT || boneTag == BONETAG_PELVISROOT || boneTag == BONETAG_PELVIS || boneTag == BONETAG_PELVIS1 || boneTag == BONETAG_SPINE_ROOT || 
			boneTag == BONETAG_SPINE0 || boneTag == BONETAG_SPINE1 || boneTag == BONETAG_SPINE2 || boneTag == BONETAG_SPINE3)
		{
			return fragInstNMGta::kSpine;
		}

		// otherwise it's part of a limb 
		return kLimb;
	}

	return fragInstNMGta::kInvalidPartGroup;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void fragInstNMGta::ProcessTwoStageRamp(fragInstNMGta::eRampType type, fragInstNMGta::eBodyPartGroup partGroup, float startStifnessIn, float midStifnessIn, 
	float endStiffnessIn, float durationStartToMid, float durationMidToEnd, float timeElapsed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	phArticulatedBody *body = GetArticulatedBody();
	if (body && partGroup != fragInstNMGta::kInvalidPartGroup)
	{
		// Bail if time has elapsed
		if (timeElapsed > durationStartToMid + durationMidToEnd)
		{
			return;
		}

		// Match the duration and stiffnesses to the stage
		float duration = durationStartToMid;
		float startStiffness = startStifnessIn;
		float endStiffness = midStifnessIn;
		float timePassedRatio = ClampRange(timeElapsed, 0.0f, durationStartToMid);
		if (timeElapsed > durationStartToMid)
		{
			duration = durationMidToEnd;
			startStiffness = midStifnessIn;
			endStiffness = endStiffnessIn;
			timePassedRatio = ClampRange(timeElapsed-durationStartToMid, 0.0f, durationMidToEnd);
		}

		// Compute the stiffness to apply
		float stiffness = Lerp(timePassedRatio, startStiffness, endStiffness);

		// Apply stiffness
		for (int iJoint = 0; iJoint < body->GetNumJoints(); iJoint++)
		{
			phJoint &joint = body->GetJoint(iJoint);
			if (GetBodyPartGroup(joint.GetChildLinkIndex()) == partGroup)
			{
				if (type == kStiffness)
				{
					joint.SetStiffness(stiffness);
				}
				else if (type == kProportionalGain)
				{
					switch (joint.GetJointType())
					{
					case phJoint::JNT_1DOF:
						{
							phJoint1Dof& joint1Dof = *static_cast<phJoint1Dof*>(&body->GetJoint(iJoint));
							joint1Dof.SetMuscleAngleStrength(stiffness);
							break;
						}

					case phJoint::JNT_3DOF:
						{
							phJoint3Dof& joint3Dof = *static_cast<phJoint3Dof*>(&body->GetJoint(iJoint));
							joint3Dof.SetMuscleAngleStrength(stiffness);
							break;
						}
					}
				}
			}
		}
	}
}

#if __DEV
void fragInstNMGta::InvalidStateDump() const
{
	fragInstNM::InvalidStateDump();
	InvalidStateDumpHelper(this);
}
#endif // __DEV

//float CBulletForce::ms_fPedBulletApplyTime = 0.30f;
//float CBulletForce::ms_fPedMeleeApplyTime = 0.15f;
//float CBulletForce::ms_fPedBulletMassMult = 1.0f;
//float CBulletForce::ms_fPedMeleeMassMult = 1.5f;
//
//CBulletForce::CBulletForce()
//{
//	m_pTarget = NULL;
//	m_vecForce.Zero();
//	m_vecPos.Zero();
//	m_fTime = 0.0f;
//
//	m_nComponent = 0;
//	m_nFlags = 0;
//}
//
//void CBulletForce::InitPedImpact(CPed* pPed, float fMomentum, const Vector3& vecDir, const Vector3& vecWorldPos, int nComponent, u32 nWeaponHash)
//{
//	Assert(pPed);
//	if(pPed==NULL)
//		return;
//
//	if(pPed!=m_pTarget)
//	{
//		m_pTarget = pPed;
//	}
//
//	// reset flags
//	m_nFlags = 0;
//
//	const CWeaponInfo* pInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(nWeaponHash);
//	if(pInfo && pInfo->GetIsMelee())
//	{
//		m_fMassMult = ms_fPedMeleeMassMult;
//		if(m_fTime <= 0.0f)
//			m_fTime = ms_fPedMeleeApplyTime;
////		else
////			Assert(false);
//	}
//	else
//	{
//		m_fMassMult = ms_fPedBulletMassMult;
//		m_fTime = ms_fPedBulletApplyTime;
//	}
//
//	fMomentum *= 1.0f / m_fTime;
//	m_vecForce = vecDir * fMomentum;
//
//	Matrix34 ragdollRootMatrix;
//	if(pPed->GetRagdollComponentMatrix(ragdollRootMatrix, 0))
//	{
//		Assertf(ragdollRootMatrix.d.Dist2(vecWorldPos) < 9.0f, "hitPos is more then 3 meters away from ped position!");
//		m_vecPos.Subtract(vecWorldPos, ragdollRootMatrix.d);
//	}
//	else
//	{
//		m_vecPos.Zero();
//	}
//
//	Assert((unsigned)nComponent < 65536);
//	m_nComponent = (u16)nComponent;
//}
//
//void CBulletForce::ProcessPhysics(float fTimeStep)
//{
//	Assert(m_pTarget && m_fTime > 0.0f);
//	fragInstNMGta* pRagdoll = NULL;
//	if(m_pTarget && m_pTarget->GetIsTypePed())
//		pRagdoll = ((CPed*)m_pTarget.Get())->GetRagdollInst();
//
//	float fApplyFrac = 1.0f;
//	if(m_fTime > fTimeStep)
//		m_fTime -= fTimeStep;
//	else
//	{
//		fApplyFrac = m_fTime / fTimeStep;
//		m_fTime = 0.0f;
//	}
//
//	float fMassScale = m_fMassMult;
//	float fMassScaleRemaining = m_fMassMult - fMassScale;
//	if(pRagdoll)
//		pRagdoll->ScaleImpulseByComponentMass(m_nComponent, 0.8f);
//	
//	const Vector3 vTargetPosition = VEC3V_TO_VECTOR3(m_pTarget->GetTransform().GetPosition());
//	Vector3 vecApplyWorldPos(m_vecPos);
//	if(m_pTarget->GetIsTypePed())
//	{
//		Matrix34 ragdollRootMatrix;
//		if(((CPed*)m_pTarget.Get())->GetRagdollComponentMatrix(ragdollRootMatrix, 0))
//			vecApplyWorldPos.Add(ragdollRootMatrix.d);
//		else
//			vecApplyWorldPos.Set(vTargetPosition);
//	}
//
//	m_pTarget->ApplyForce(fApplyFrac * fMassScale * m_vecForce, vecApplyWorldPos - vTargetPosition, m_nComponent);
//
//	int nGroup = pRagdoll->GetTypePhysics()->GetAllChildren()[m_nComponent]->GetOwnerGroupPointerIndex();
//	int nParentGroup = pRagdoll->GetTypePhysics()->GetAllGroups()[nGroup]->GetParentGroupPointerIndex();
//
//	while(pRagdoll && fMassScaleRemaining > 0.0f && nParentGroup > 0)
//	{
//		int nParentFirstChild = pRagdoll->GetTypePhysics()->GetAllGroups()[nParentGroup]->GetChildFragmentIndex();
//		fMassScale = pRagdoll->ScaleImpulseByComponentMass(nParentFirstChild, 0.8f);
//		if(fMassScale > fMassScaleRemaining)
//		{
//			fMassScale = fMassScaleRemaining;
//			fMassScaleRemaining = -1.0f;
//		}
//		else
//			fMassScaleRemaining -= fMassScale;
//
//		m_pTarget->ApplyForce(fApplyFrac * fMassScale * m_vecForce, vecApplyWorldPos - VEC3V_TO_VECTOR3(m_pTarget->GetTransform().GetPosition()), nParentFirstChild);
//
//		nParentGroup = pRagdoll->GetTypePhysics()->GetAllGroups()[nParentGroup]->GetParentGroupPointerIndex();
//	}
//
//	if(m_fTime <= 0.0f)
//	{
//		m_pTarget = NULL;
//	}
//}
//
//void CBulletForce::Reset()
//{
//	m_pTarget = NULL;
//}




