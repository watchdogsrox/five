//
// camera/helpers/Collision.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/Collision.h"

#include "fwsys/timer.h"
#include "phbound/support.h"
#include "phbullet/IterativeCast.h"
#include "profile/group.h"
#include "profile/page.h"
#include "profile/timebars.h"

#include "camera/camera_channel.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/Frame.h"
#include "camera/system/CameraMetadata.h"
#include "control/replay/ReplayExtensions.h"
#include "debug/DebugScene.h"
#include "objects/object.h"
#include "peds/ped.h"
#include "physics/gtaArchetype.h"
#include "physics/gtaInst.h"
#include "physics/gtaMaterialManager.h"
#include "physics/physics.h"
#include "renderer/River.h"
#include "renderer/Water.h"
#include "scene/Physical.h"
#include "vehicles/Trailer.h"

#include "control/replay/ReplaySettings.h"
#if GTA_REPLAY
#include "control/replay/replay.h"
#endif // GTA_REPLAY

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camCollision,0x1EB7AAE5)

PF_PAGE(camCollisionPage, "Camera: Collision Helper");

PF_GROUP(camCollisionMetrics);
PF_LINK(camCollisionPage, camCollisionMetrics);

PF_VALUE_FLOAT(camCollisionDampedOrbitDistance, camCollisionMetrics);
PF_VALUE_FLOAT(camCollisionPushBeyondExtraOrbitDistance, camCollisionMetrics);
PF_VALUE_FLOAT(camCollisionPostCollisionOrbitDistance, camCollisionMetrics);
PF_VALUE_FLOAT(camCollisionDesiredPullBackRadius, camCollisionMetrics);
PF_VALUE_FLOAT(camCollisionDampedPullBackRadius, camCollisionMetrics);

#if RSG_PC
REGISTER_TUNE_GROUP_FLOAT(c_fClampRadiusScale, 0.50f);
#endif

const float g_MinSafeOrbitDistanceAfterCollision	= 0.01f;
extern const float	g_InvalidWaterHeight			= -1000.0f;

phShapeTest<phShapeCapsule>		camCollision::ms_CapsuleTest;
phShapeTest<phShapeSweptSphere>	camCollision::ms_SweptSphereTest;
phShapeTest<phShapeSphere>		camCollision::ms_SphereTest;
phShapeTest<phShapeProbe>		camCollision::ms_ProbeTest;
phShapeTest<phShapeEdge>		camCollision::ms_EdgeTest;

atArray<const CEntity*>			camCollision::ms_EntitiesToIgnore;
atArray<const CEntity*>			camCollision::ms_ExtraEntitiesToPushBeyond;

const CEntity*					camCollision::ms_EntityToConsiderForTouchingChecks = NULL;

const u32	camCollision::ms_CollisionIncludeFlags					= ((static_cast<u32>(ArchetypeFlags::GTA_MAP_TYPE_MOVER) |
																		static_cast<u32>(ArchetypeFlags::GTA_VEHICLE_TYPE) |
																		static_cast<u32>(ArchetypeFlags::GTA_BOX_VEHICLE_TYPE) |
																		static_cast<u32>(ArchetypeFlags::GTA_OBJECT_TYPE) |
																		static_cast<u32>(ArchetypeFlags::GTA_GLASS_TYPE) |
																		static_cast<u32>(ArchetypeFlags::GTA_DEEP_SURFACE_TYPE)));

const u32	camCollision::ms_CollisionIncludeFlagsForClippingTests	= (camCollision::ms_CollisionIncludeFlags |
																		static_cast<u32>(ArchetypeFlags::GTA_RAGDOLL_TYPE) |
																		static_cast<u32>(ArchetypeFlags::GTA_HORSE_RAGDOLL_TYPE));

const float	camCollision::ms_MinRadiusReductionBetweenInterativeShapetests = (1.1f * ITERATIVE_CAST_INTERSECTION_TOLERANCE);

bool	camCollision::ms_ShouldIgnoreOcclusionWithBrokenFragments					= false;
bool	camCollision::ms_ShouldIgnoreOcclusionWithBrokenFragmentsOfIgnoredEntities	= false;
bool	camCollision::ms_ShouldIgnoreCollisionWithTouchingPedsAndVehicles			= false;
bool	camCollision::ms_ShouldLogExtraEntitiesToPushBeyond							= false;


void camCollision::InitClass()
{
	ms_CapsuleTest.SetIncludeInstanceCallback(IncludeInstanceCallback(InstanceRejectCallback));
	ms_CapsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);

	ms_SweptSphereTest.SetIncludeInstanceCallback(IncludeInstanceCallback(InstanceRejectCallback));
	ms_SweptSphereTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);

	ms_SphereTest.SetIncludeInstanceCallback(IncludeInstanceCallback(InstanceRejectCallback));
	ms_SphereTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);

	ms_ProbeTest.SetIncludeInstanceCallback(IncludeInstanceCallback(InstanceRejectCallback));
	ms_ProbeTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);

	ms_EdgeTest.SetIncludeInstanceCallback(IncludeInstanceCallback(InstanceRejectCallback));
	ms_EdgeTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
}

camCollision::camCollision(const camBaseObjectMetadata& metadata)
: camBaseObject(metadata)
, m_Metadata(static_cast<const camCollisionMetadata&>(metadata))
{
	Reset();

	m_ShouldIgnoreOcclusionWithSelectCollision = m_Metadata.m_ShouldIgnoreOcclusionWithSelectCollision;

	m_ShouldIgnoreDynamicCollision = false;

	if (!m_Metadata.m_ShouldReportAsCameraTypeTest)
	{
		ms_CapsuleTest.SetTypeFlags(TYPE_FLAGS_ALL);
		ms_SweptSphereTest.SetTypeFlags(TYPE_FLAGS_ALL);
		ms_SphereTest.SetTypeFlags(TYPE_FLAGS_ALL);
		ms_ProbeTest.SetTypeFlags(TYPE_FLAGS_ALL);
		ms_EdgeTest.SetTypeFlags(TYPE_FLAGS_ALL);
	}
	else
	{
		ms_CapsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
		ms_SweptSphereTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
		ms_SphereTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
		ms_ProbeTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
		ms_EdgeTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	}
}

void camCollision::Reset()
{
	m_HeadingDeltaSpring.Reset();
	m_PitchDeltaSpring.Reset();
	m_OrbitDistanceSpring.Reset(LARGE_FLOAT);
	m_PushBeyondOrbitDistanceSpring.Reset();
	m_PullBackTowardsCollisionSpring.Reset();
	m_CollisionRootPositionOnPreviousUpdate							= g_InvalidPosition;
	m_PreCollisionCameraPositionOnPreviousUpdate					= g_InvalidPosition;
	//NOTE: We really should not be reseting this variable, as it set by a public interface.
	//However correcting this may expose us to collision issues, so this will be left as-is for now.
	m_EntityToConsiderForTouchingChecksThisUpdate					= NULL;
	m_InitialCollisionTestRadius									= 0.0f;
	m_DampedOrbitDistanceOnPreviousUpdate							= LARGE_FLOAT;
	m_DampedOrbitDistanceRatioOnPreviousUpdate						= LARGE_FLOAT;
	m_OrbitDistanceRatioOnPreviousUpdate							= LARGE_FLOAT;
	m_SmoothedLocalWaterHeight										= g_InvalidWaterHeight;
	//NOTE: We really should not be reseting this variable, as it set by a public interface.
	//However it's apparent that correcting this would expose us to collision issues, so this will be left as-is for now.
	m_ShouldBypassThisUpdate										= false;
	m_ShouldBypassBuoyancyThisUpdate								= false;
	m_ShouldForcePopInThisUpdate									= true; //Force a pop-in to avoid collision on the next update.
	//NOTE: We really should not be reseting this variable, as it set by a public interface.
	//However correcting this may expose us to collision issues, so this will be left as-is for now.
	m_ShouldIgnoreCollisionWithTouchingPedsAndVehiclesThisUpdate	= false;
	m_ShouldPausePullBack											= false;
	m_WasPushedBeyondEntitiesOnPreviousUpdate						= false;
	m_WasCameraPoppedInOnPreviousUpdate								= false;
	m_WasCameraConstrainedOffAxisOnPreviousUpdate					= false;
}

void camCollision::IgnoreEntityThisUpdate(const CEntity& entity)
{
	AddEntityToList(entity, m_EntitiesToIgnoreThisUpdate, m_Metadata.m_ShouldReportAsCameraTypeTest);
}
void camCollision::PushBeyondEntityIfClippingThisUpdate(const CEntity& entity)
{
	AddEntityToList(entity, m_EntitiesToPushBeyondIfClippingThisUpdate, m_Metadata.m_ShouldReportAsCameraTypeTest);
}

void camCollision::AddEntityToList(const CEntity& entity, atArray<const CEntity*>& entityList, const bool shouldReportAsCameraTypeTest)
{
	//Ensure that we don't duplicate an existing entry.
	if(entityList.Find(&entity) != -1)
	{
		return;
	}

	//Since attachments can be made with collision disabled, we need to avoid adding entities without physics instances to the list, otherwise
	//we'll confuse the shapetests. Likewise, there is no point adding instances that are not in the level or instances that do not include the
	//camera test type include flag.
	const phInst* instance = entity.GetCurrentPhysicsInst();
	if(instance && instance->IsInLevel())
	{
		const phLevelNew* physicsLevel = CPhysics::GetLevel();
		if(physicsLevel)
		{
			const u16 levelIndex	= instance->GetLevelIndex();
			const u32 includeFlags	= physicsLevel->GetInstanceIncludeFlags(levelIndex);
			if((includeFlags & ArchetypeFlags::GTA_CAMERA_TEST) || !shouldReportAsCameraTypeTest)
			{
				entityList.Grow() = &entity;
			}
		}
	}
#if GTA_REPLAY
	else if (CReplayMgr::IsEditModeActive())
	{
		// HACK: looks like replay entities are not setup properly for physics
		//		 if the entity is part of the replay, add it here.
		if (ReplayEntityExtension::HasExtension(&entity))
		{
			entityList.Grow() = &entity;
		}
	}
#endif // GTA_REPLAY

	//If this entity is a vehicle and it has (immediate) child attachments or it is pulling a trailer, we should add these also.
	if(entity.GetIsTypeVehicle())
	{
		const fwEntity* childAttachment = entity.GetChildAttachment();
		while(childAttachment)
		{
			AddEntityToList(static_cast<const CEntity&>(*childAttachment), entityList, shouldReportAsCameraTypeTest);

			childAttachment = childAttachment->GetSiblingAttachment();
		}

		const CTrailer* trailer	= static_cast<const CVehicle&>(entity).GetAttachedTrailer();
		if(trailer)
		{
			AddEntityToList(*trailer, entityList, shouldReportAsCameraTypeTest);

			//Also ignore all of the trailer's immediate child attachments.
			childAttachment = trailer->GetChildAttachment();
			while(childAttachment)
			{
				AddEntityToList(static_cast<const CEntity&>(*childAttachment), entityList, shouldReportAsCameraTypeTest);

				childAttachment = childAttachment->GetSiblingAttachment();
			}
		}
	}
}

void camCollision::CloneDamping(const camCollision& sourceCollision)
{
	m_OrbitDistanceSpring.OverrideResult(sourceCollision.m_OrbitDistanceSpring.GetResult());
	m_OrbitDistanceSpring.OverrideVelocity(sourceCollision.m_OrbitDistanceSpring.GetVelocity());

	m_PushBeyondOrbitDistanceSpring.OverrideResult(sourceCollision.m_PushBeyondOrbitDistanceSpring.GetResult());
	m_PushBeyondOrbitDistanceSpring.OverrideVelocity(sourceCollision.m_PushBeyondOrbitDistanceSpring.GetVelocity());

	m_PullBackTowardsCollisionSpring.OverrideResult(sourceCollision.m_PullBackTowardsCollisionSpring.GetResult());
	m_PullBackTowardsCollisionSpring.OverrideVelocity(sourceCollision.m_PullBackTowardsCollisionSpring.GetVelocity());

	m_DampedOrbitDistanceOnPreviousUpdate		= sourceCollision.m_DampedOrbitDistanceOnPreviousUpdate;
	m_DampedOrbitDistanceRatioOnPreviousUpdate	= sourceCollision.m_DampedOrbitDistanceRatioOnPreviousUpdate;
	m_OrbitDistanceRatioOnPreviousUpdate		= sourceCollision.m_OrbitDistanceRatioOnPreviousUpdate;
}

void camCollision::PreUpdate()
{
	m_EntityToConsiderForTouchingChecksThisUpdate					= NULL;
	m_ShouldBypassThisUpdate										= false;
	m_ShouldForcePopInThisUpdate									= false;
	m_ShouldIgnoreCollisionWithTouchingPedsAndVehiclesThisUpdate	= false;

	m_EntitiesToPushBeyondIfClippingThisUpdate.Reset();

	m_EntitiesToIgnoreThisUpdate.Reset();
}

void camCollision::Update(const Vector3& collisionRootPosition, float collisionTestRadius, bool isBuoyant, Vector3& cameraPosition,
	float& headingDelta, float& pitchDelta)
{
	PF_PUSH_TIMEBAR_DETAIL("Camera Collision");

	m_InitialCollisionTestRadius = collisionTestRadius;

	m_IncludeFlagsForOcclusionTests	= m_ShouldIgnoreDynamicCollision ? static_cast<u32>(ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_GLASS_TYPE | ArchetypeFlags::GTA_DEEP_SURFACE_TYPE) : 
										(m_ShouldIgnoreOcclusionWithSelectCollision || m_Metadata.m_ShouldIgnoreOcclusionWithRagdolls) ?
										ms_CollisionIncludeFlags : ms_CollisionIncludeFlagsForClippingTests;

	if(m_ShouldBypassThisUpdate)
	{
		m_ShouldBypassThisUpdate = false;

		PF_POP_TIMEBAR_DETAIL();

		return;
	}

#if RSG_PC
	if(GRCDEVICE.CanUseStereo() && GRCDEVICE.IsStereoEnabled())
	{
		INSTANTIATE_TUNE_GROUP_FLOAT(3D_VISION, c_fClampRadiusScale, 0.25f, 1.0f, 0.001f);
		float fSeparationDistance = Min( Max(gVpMan.GetHalfEyeSeparation(), 0.0f), m_InitialCollisionTestRadius * c_fClampRadiusScale ) + SMALL_FLOAT;
		cameraDebugf1("Stereo enabled -- EyeSeparation %f, SeparationPercent %f, Convergence %f, Separation Dist %f, DistToAdd %f",
			GRCDEVICE.GetCachedEyeSeparation(), GRCDEVICE.GetDefaultSeparationPercentage() * 0.01f,
			GRCDEVICE.GetCachedConvergenceDistance(), gVpMan.GetHalfEyeSeparation(), fSeparationDistance);

		collisionTestRadius += fSeparationDistance;
		m_InitialCollisionTestRadius = collisionTestRadius;
	}
#endif // RSG_PC

	const Vector3 preCollisionCameraPosition(cameraPosition);
	const float preCollisionOrbitDistance = collisionRootPosition.Dist(cameraPosition);

	PF_PUSH_TIMEBAR_DETAIL("Compute Unoccluded Position");

	Vector3 orbitFront(collisionRootPosition - cameraPosition);
	orbitFront.NormalizeSafe();

	ms_ExtraEntitiesToPushBeyond.Reset();

	float desiredOrbitDistance;
	float desiredHeadingDelta;
	float desiredPitchDelta;
	float desiredPullBackRadius;
	const bool wasCameraPoppedIn = ComputeDesiredOrbitDistanceAndOrientationDelta(collisionRootPosition, cameraPosition,
									collisionTestRadius, desiredOrbitDistance, desiredHeadingDelta, desiredPitchDelta, desiredPullBackRadius);

	//Apply the desired orbit distance and report the orientation change, such that it is applied on the *next* update.

	bool isPullBackDesired;
	ComputeDesiredOrbitDistanceDampingBehaviour(collisionRootPosition, preCollisionCameraPosition, preCollisionOrbitDistance, desiredOrbitDistance,
		isPullBackDesired, m_ShouldPausePullBack);

	const float orbitDistanceOnPreviousUpdate = m_OrbitDistanceSpring.GetResult();

	const float springConstant	= isPullBackDesired ? m_Metadata.m_OrbitDistanceDamping.m_SpringConstant : 0.0f;
	float dampedOrbitDistance	= (isPullBackDesired && m_ShouldPausePullBack) ? orbitDistanceOnPreviousUpdate :
									m_OrbitDistanceSpring.Update(desiredOrbitDistance, springConstant, m_Metadata.m_OrbitDistanceDamping.m_SpringDampingRatio, true);
	if(isPullBackDesired)
	{
		//Prevent the damped orbit distance from overshooting the desired value, as it could then cut back.
		dampedOrbitDistance = Min(dampedOrbitDistance, desiredOrbitDistance);
	}

	m_HeadingDeltaSpring.OverrideResult(0.0f);
	headingDelta	= m_HeadingDeltaSpring.Update(desiredHeadingDelta, m_Metadata.m_RotationTowardsLos.m_SpringConstant,
						m_Metadata.m_RotationTowardsLos.m_SpringDampingRatio, true);

	m_PitchDeltaSpring.OverrideResult(0.0f);
	pitchDelta		= m_PitchDeltaSpring.Update(desiredPitchDelta, m_Metadata.m_RotationTowardsLos.m_SpringConstant,
						m_Metadata.m_RotationTowardsLos.m_SpringDampingRatio, true);

	PF_POP_TIMEBAR_DETAIL(); //"Compute Unoccluded Position"

	PF_PUSH_TIMEBAR_DETAIL("Avoid clipping");
	const bool wasClipping = AvoidClipping(collisionRootPosition, orbitFront, collisionTestRadius, desiredPullBackRadius, dampedOrbitDistance);
	if(wasClipping)
	{
		//Persist the pop-in to avoid clipping (zeroing the spring velocity) so that we can apply suitable damping to the orbit distance when it eventually increases.
		m_OrbitDistanceSpring.Reset(dampedOrbitDistance);

		//Latch on the pausing of pull-back by default whenever we are not pulling back.
		m_ShouldPausePullBack = true;
	}
	PF_POP_TIMEBAR_DETAIL(); //"Avoid clipping"

	float orbitDistanceToApply = dampedOrbitDistance;

	//NOTE: We do not allow the camera to be further away from the collision root position after collision, as that would result in a discontinuity.
	const float maxPullBackDistance	= Max(preCollisionOrbitDistance - orbitDistanceToApply, 0.0f);
	desiredPullBackRadius			= Min(desiredPullBackRadius, maxPullBackDistance);

	//Log any extra entities to push beyond that were detected in the preceding tests.
	const s32 numExtraEntitiesToPushBeyond = ms_ExtraEntitiesToPushBeyond.GetCount();
	for(s32 entityIndex=0; entityIndex<numExtraEntitiesToPushBeyond; entityIndex++)
	{
		const CEntity* entityToPushBeyond = ms_ExtraEntitiesToPushBeyond[entityIndex];
		if(entityToPushBeyond)
		{
			PushBeyondEntityIfClippingThisUpdate(*entityToPushBeyond);
		}
	}

	PF_PUSH_TIMEBAR_DETAIL("Push beyond entities");
	PushBeyondEntitiesIfClipping(collisionRootPosition, orbitFront, collisionTestRadius, desiredPullBackRadius, orbitDistanceToApply);
	PF_POP_TIMEBAR_DETAIL(); //"Push beyond entities"

	cameraPosition = collisionRootPosition - (orbitFront * orbitDistanceToApply);

	//NOTE: We must override the damped spring *before* buoyancy and pull back.
	m_OrbitDistanceSpring.OverrideResult(orbitDistanceToApply);

	bool hasConstrainedAgainstWater = false;
	if(!m_ShouldBypassBuoyancyThisUpdate)
	{
		//NOTE: The camera buoyancy must be updated last, as the implementation requires a post-collision camera position.
		PF_PUSH_TIMEBAR_DETAIL("Buoyancy");
		hasConstrainedAgainstWater	= UpdateBuoyancy(isBuoyant, preCollisionOrbitDistance, collisionTestRadius, desiredPullBackRadius, collisionRootPosition, cameraPosition);
		PF_POP_TIMEBAR_DETAIL(); //"Buoyancy"
	}
	m_ShouldBypassThisUpdate = false; //need to reset this here.

	float postCollisionOrbitDistance = hasConstrainedAgainstWater ? collisionRootPosition.Dist(cameraPosition) : orbitDistanceToApply;

	const float dampedPullBackRadiusOnPreviousUpdate = m_PullBackTowardsCollisionSpring.GetResult();

	//Prevent the desired pull back radius increasing above the damped pull back radius from the previous update by more than the distance that the
	//orbit distance (before buoyancy and pull back) has decreased this update. Otherwise the pull back can result in a net increase in the post-collision
	//orbit distance when the camera should be pushing in, which can appear like a pop.
	const float orbitDistanceDecreaseThisUpdate			= Max(orbitDistanceOnPreviousUpdate - orbitDistanceToApply, 0.0f);
	const float desiredPullBackRadiusDeltaThisUpdate	= desiredPullBackRadius - dampedPullBackRadiusOnPreviousUpdate;
	if(desiredPullBackRadiusDeltaThisUpdate >= (orbitDistanceDecreaseThisUpdate + SMALL_FLOAT))
	{
		desiredPullBackRadius = dampedPullBackRadiusOnPreviousUpdate + orbitDistanceDecreaseThisUpdate;
	}

	const bool shouldBlendIn = (desiredPullBackRadius >= dampedPullBackRadiusOnPreviousUpdate);

	//NOTE: We must cut when blending out to a lesser, but non-zero, desired radius. Otherwise the camera can be pushed through collision by the
	//damped pull back radius.
	const float pullBackSpringConstant	= shouldBlendIn ? m_Metadata.m_PullBackTowardsCollision.m_BlendInSpringConstant :
											((desiredPullBackRadius >= SMALL_FLOAT) ? 0.0f :
											m_Metadata.m_PullBackTowardsCollision.m_BlendOutSpringConstant);
	const float dampedPullBackRadius	= m_PullBackTowardsCollisionSpring.Update(desiredPullBackRadius, pullBackSpringConstant,
											m_Metadata.m_PullBackTowardsCollision.m_SpringDampingRatio, true);

	if(m_Metadata.m_ShouldPullBackByCapsuleRadius && (dampedPullBackRadius >= SMALL_FLOAT))
	{
		//TODO: Limit pull back to situations that really need it, such as when the camera is constrained very close to the attach parent.
		//TODO: Implement safe pull back by sweeping a near-clip quad shapetest from the near-clip position backwards along the orbit front.

		float pullBackRadius;

#if RSG_PC
		if(camInterface::IsTripleHeadDisplay()) 
		{
			pullBackRadius = 0.0f;
		}
		else
		{
			pullBackRadius = dampedPullBackRadius;
		}
#else
		pullBackRadius = dampedPullBackRadius;
#endif

		postCollisionOrbitDistance += pullBackRadius;

		//NOTE: We must recompute the orbit front, as the buoyancy implementation can translate the camera independently of the original orbit front.
		orbitFront = (collisionRootPosition - cameraPosition);
		orbitFront.NormalizeSafe();

		cameraPosition = collisionRootPosition - (orbitFront * postCollisionOrbitDistance);
	}

	m_DampedOrbitDistanceOnPreviousUpdate			= dampedOrbitDistance;
	m_DampedOrbitDistanceRatioOnPreviousUpdate		= dampedOrbitDistance / preCollisionOrbitDistance;
	m_OrbitDistanceRatioOnPreviousUpdate			= postCollisionOrbitDistance / preCollisionOrbitDistance;
	m_WasCameraPoppedInOnPreviousUpdate				= wasCameraPoppedIn;
	m_CollisionRootPositionOnPreviousUpdate			= collisionRootPosition;
	m_PreCollisionCameraPositionOnPreviousUpdate	= preCollisionCameraPosition;
	m_WasCameraConstrainedOffAxisOnPreviousUpdate	= hasConstrainedAgainstWater;

	PF_SET(camCollisionDampedOrbitDistance, dampedOrbitDistance);
	PF_SET(camCollisionPostCollisionOrbitDistance, postCollisionOrbitDistance);
	PF_SET(camCollisionDesiredPullBackRadius, desiredPullBackRadius);
	PF_SET(camCollisionDampedPullBackRadius, dampedPullBackRadius);

	PF_POP_TIMEBAR_DETAIL();
}

bool camCollision::ComputeDesiredOrbitDistanceAndOrientationDelta(const Vector3& collisionRootPosition, const Vector3& cameraPosition,
	float& collisionTestRadius, float& desiredOrbitDistance, float& desiredHeadingDelta, float& desiredPitchDelta, float& desiredPullBackRadius) const
{
	const Vector3 orbitDeltaBeforeMovement(collisionRootPosition - cameraPosition);
	const float orbitDistanceBeforeMovement = orbitDeltaBeforeMovement.Mag();
	Vector3 orbitFrontBeforeMovement(orbitDeltaBeforeMovement);
	orbitFrontBeforeMovement.NormalizeSafe();

	//Fall-back to the original follow distance and no desired orientation change.
	desiredOrbitDistance	= orbitDistanceBeforeMovement;
	desiredHeadingDelta		= 0.0f;
	desiredPitchDelta		= 0.0f;
	desiredPullBackRadius	= 0.0f;

	PF_PUSH_TIMEBAR_DETAIL("Initial Test");

	//Perform an initial directed capsule test between the attach position and the current camera position.

	if(!cameraVerifyf(collisionTestRadius >= SMALL_FLOAT,
		"A collision helper was unable to compute a valid camera position due to an invalid test radius."))
	{
		PF_POP_TIMEBAR_DETAIL();
		return false;
	}

	phIntersection initialIntersection;
	bool hasCapsuleCollided	= TestSweptSphereAgainstWorld(collisionRootPosition, cameraPosition, collisionTestRadius, initialIntersection,
								m_IncludeFlagsForOcclusionTests, m_ShouldIgnoreOcclusionWithSelectCollision, false, true);

	float collisionTestRadiusToConsiderForPullBack = collisionTestRadius;

	if(!hasCapsuleCollided)
	{
		//Reduce the test radius in readiness for use in a consecutive test.
		collisionTestRadius	-= ms_MinRadiusReductionBetweenInterativeShapetests;

		//The current camera position is unoccluded, so there is no further work required here.
		PF_POP_TIMEBAR_DETAIL();
		return false;
	}

	//NOTE: The initial capsule test provides a safe (popped-in) fall-back position. This may be used if no clear path to an unoccluded position can
	//be found.
	const float initialIntersectionTValue	= initialIntersection.GetT();
	desiredOrbitDistance					= initialIntersectionTValue * orbitDistanceBeforeMovement;

#if !__NO_OUTPUT
	const CEntity* hitEntity = nullptr;
	if(initialIntersection.GetInstance())
	{
		hitEntity = CPhysics::GetEntityFromInst(initialIntersection.GetInstance());
	}
	cameraDebugf2("Camera occlusion test is hitting entity %s with tVal %f", hitEntity ? hitEntity->GetModelName() : "NULL", initialIntersectionTValue);
#endif

	PF_POP_TIMEBAR_DETAIL();

	//Perform a series of directed capsules tests in order to find the unoccluded capsule position that is nearest to a preferred camera position.
	//Each successive capsule is rotated further away from the initial intersection that was determined above. We only use the initial intersection
	//normal to direct the movement of the capsule as it is most often better to bypass secondary occluders in search of an unoccluded position,
	//rather than risk working backwards.

	PF_PUSH_TIMEBAR_DETAIL("Capsule Sweep");

	Vector3 capsuleDirection(-orbitFrontBeforeMovement);

	bool hasFoundRotatedUnoccludedPosition = false;

	const float collisionRootDistanceDelta	= collisionRootPosition.Dist(m_CollisionRootPositionOnPreviousUpdate);
	const float invTimeStep					= fwTimer::GetCamInvTimeStep();
	const float collisionRootMoveSpeed		= collisionRootDistanceDelta * invTimeStep;

	const Vector3 preCollisionCameraDelta		= cameraPosition - m_PreCollisionCameraPositionOnPreviousUpdate;
	const float preCollisionCameraDistanceDelta	= preCollisionCameraDelta.Mag();
	const float preCollisionCameraMoveSpeed		= preCollisionCameraDistanceDelta * invTimeStep;

	//NOTE: We do not reorient around occluders when both the collision root and camera are roughly stationary, as it feel unnatural.
	const bool shouldForceStationaryPopIn	= (collisionRootMoveSpeed <= m_Metadata.m_OcclusionSweep.m_MaxCollisionRootSpeedToForcePopIn) &&
												(preCollisionCameraMoveSpeed <= m_Metadata.m_OcclusionSweep.m_MaxPreCollisionCameraSpeedToForcePopIn);

	//NOTE: If the camera was popped-in due to occlusion/collision on the previous update, the pop-in behaviour must continue until such time as
	//no further pop-in is required.
	const bool isValidToMoveTowardsLos	= (m_Metadata.m_ShouldMoveTowardsLos && !m_ShouldForcePopInThisUpdate && !shouldForceStationaryPopIn &&
											!(m_Metadata.m_ShouldPersistPopInBehaviour && m_WasCameraPoppedInOnPreviousUpdate));

	bool hasValidSweepAxis = false;
	Vector3 sweepAxis;
	if(isValidToMoveTowardsLos)
	{
		const float headingVelocityOnPreviousUpdate	= m_HeadingDeltaSpring.GetVelocity();
		const float pitchVelocityOnPreviousUpdate	= m_PitchDeltaSpring.GetVelocity();
		const Vector3 orientationVelocityEulersOnPreviousUpdate(pitchVelocityOnPreviousUpdate, 0.0f, headingVelocityOnPreviousUpdate);

		Quaternion orientationVelocityQuatOnPreviousUpdate;
		orientationVelocityQuatOnPreviousUpdate.FromEulers(orientationVelocityEulersOnPreviousUpdate, eEulerOrderYXZ);

		const float orientationSpeedOnPreviousUpdate		= orientationVelocityQuatOnPreviousUpdate.GetAngle();
		const float minOrientationSpeedToMaintainDirection	= m_Metadata.m_OcclusionSweep.m_MinOrientationSpeedToMaintainDirection * DtoR;

		if(orientationSpeedOnPreviousUpdate >= minOrientationSpeedToMaintainDirection)
		{
			//This camera was rotated towards an unoccluded position on the previous update, so we must sweep in the direction of travel in order to avoid
			//the oscillation that could otherwise result given a finite scan resolution.
			orientationVelocityQuatOnPreviousUpdate.GetUnitDirection(sweepAxis);
		}
		else
		{
			//Simply scan in an arc that is directed away from the initial intersection.
			const Vector3 intialIntersectionNormal = RCC_VECTOR3(initialIntersection.GetNormal());
			float angle;
			camFrame::ComputeAxisAngleRotation(capsuleDirection, intialIntersectionNormal, sweepAxis, angle);

			if(IsNearZero(angle))
			{
				//We could not compute a valid sweep orientation, so fall-back to world-up for our axis, so that we sweep heading.
				sweepAxis = ZAXIS;
			}
		}

		hasValidSweepAxis = true;
	}
	else if(m_Metadata.m_ShouldSweepToAvoidPopIn && !m_PreCollisionCameraPositionOnPreviousUpdate.IsClose(g_InvalidPosition, SMALL_FLOAT))
	{
		//We cannot reorient the camera, so sweep in the camera's direction of travel in search of clear LOS, so that we might avoid popping in.
		if(preCollisionCameraMoveSpeed >= m_Metadata.m_OcclusionSweep.m_MinCameraMoveSpeedToSweepInDirectionOfTravel)
		{
			Vector3 directionOfTravel;
			directionOfTravel.Normalize(preCollisionCameraDelta);

			float angle;
			camFrame::ComputeAxisAngleRotation(capsuleDirection, directionOfTravel, sweepAxis, angle);

			if(IsNearZero(angle))
			{
				//We could not compute a valid sweep orientation, so fall-back to world-up for our axis, so that we sweep heading.
				sweepAxis = ZAXIS;
			}

			hasValidSweepAxis = true;
		}
	}

	Vector3 rotatedUnoccludedOrbitFront(orbitFrontBeforeMovement);
	float rotatedUnoccludedOrbitDistance = orbitDistanceBeforeMovement;

	if(hasValidSweepAxis)
	{
		//Compute the basic orientation delta to be applied to the capsule each iteration.
		const float maxSweepAngleToApply			= isValidToMoveTowardsLos ? m_Metadata.m_OcclusionSweep.m_MaxSweepAngleWhenMovingTowardsLos :
														m_Metadata.m_OcclusionSweep.m_MaxSweepAngleWhenAvoidingPopIn;
		const float maxAngleToRotateBetweenTests	= (DtoR * maxSweepAngleToApply) /
														static_cast<float>(m_Metadata.m_OcclusionSweep.m_NumCapsuleTests);

		Quaternion orientationDeltaToApplyQuat;
		orientationDeltaToApplyQuat.FromRotation(sweepAxis, maxAngleToRotateBetweenTests);

		//Compute a 'preferred' orbit distance by pushing in the current (fully pulled back) camera position to the equivalent orbit
		//distance from the previous update. This should give results that do not unduly attempt to slide the camera back out to the full
		//non-collided orbit distance.

		float preferredOrbitDistance	= Min(m_DampedOrbitDistanceRatioOnPreviousUpdate, 1.0f) * orbitDistanceBeforeMovement;
		float minOrbitDistanceError		= Abs(desiredOrbitDistance - preferredOrbitDistance);

		const float invNumSweepCapsuleTests = 1.0f / static_cast<float>(Max(m_Metadata.m_OcclusionSweep.m_NumCapsuleTests, (u32)1));

		for(int capsuleTestIndex=0; capsuleTestIndex<m_Metadata.m_OcclusionSweep.m_NumCapsuleTests; capsuleTestIndex++)
		{
			Quaternion scaledOrientationDeltaToApplyQuat(orientationDeltaToApplyQuat);

			//NOTE: When scanning for unoccluded positions to rotate towards, we scale the orientation delta to focus our search close to the
			//camera, as we require greater accuracy when approaching the unoccluded position.
			if(isValidToMoveTowardsLos)
			{
				const float orientationDeltaScaling = 2.0f * SlowIn(static_cast<float>(capsuleTestIndex + 1) * invNumSweepCapsuleTests);
				scaledOrientationDeltaToApplyQuat.ScaleAngle(orientationDeltaScaling);
			}

			//Compute a capsule direction, and hence end position, for this iteration.
			//TODO: Ensure that we do not exceed the follow pitch limits of the camera.
			scaledOrientationDeltaToApplyQuat.Transform(capsuleDirection);
			const Vector3 capsuleEndPosition = collisionRootPosition + (capsuleDirection * orbitDistanceBeforeMovement);

			//Find the farthest unoccluded position along this capsule.
			phIntersection intersection;
			hasCapsuleCollided	= TestSweptSphereAgainstWorld(collisionRootPosition, capsuleEndPosition, collisionTestRadius,
									intersection, m_IncludeFlagsForOcclusionTests, m_ShouldIgnoreOcclusionWithSelectCollision);

			const float intersectionTValue		= hasCapsuleCollided ? intersection.GetT() : 1.0f;
			const float unoccludedOrbitDistance	= intersectionTValue * orbitDistanceBeforeMovement;
			const float orbitDistanceError		= Abs(unoccludedOrbitDistance - preferredOrbitDistance);
			if(orbitDistanceError < minOrbitDistanceError)
			{
				rotatedUnoccludedOrbitFront			= -capsuleDirection;
				rotatedUnoccludedOrbitDistance		= unoccludedOrbitDistance;
				minOrbitDistanceError				= orbitDistanceError;
				hasFoundRotatedUnoccludedPosition	= true;
			}
		}
	}

	//Reduce the test radius in readiness for use in a consecutive test.
	collisionTestRadius -= ms_MinRadiusReductionBetweenInterativeShapetests;

	PF_POP_TIMEBAR_DETAIL();

	PF_PUSH_TIMEBAR_DETAIL("Path Search");

	bool wasCameraPoppedIn = true;

	if(hasFoundRotatedUnoccludedPosition && cameraVerifyf(collisionTestRadius >= SMALL_FLOAT,
		"A collision helper was unable to compute a valid camera position due to an invalid test radius."))
	{
		//Attempt to find a direct path between positions at equal distances along the current orbit vector and the unoccluded orbit vector. We
		//ideally want to find the path at the farthest orbit distance.

		//NOTE: We do not test against the collision root position, as we know that the current camera position is occluded.
		//TODO: Compute a more accurate min follow distance, beyond which the follow capsules do not overlap and it is worth testing a path.
		const float minOrbitDistanceToTest		= collisionTestRadius * 2.0f;
		const float maxOrbitDistanceToTest		= rotatedUnoccludedOrbitDistance - ms_MinRadiusReductionBetweenInterativeShapetests;
		const float orbitDistanceRangeToTest	= maxOrbitDistanceToTest - minOrbitDistanceToTest;

		if(orbitDistanceRangeToTest > SMALL_FLOAT)
		{
			const float invNumPathFindingCapsuleTestsLessOne = 1.0f / static_cast<float>(Max(m_Metadata.m_PathFinding.m_MaxCapsuleTests, (u32)2) - 1);

			for(int i=0; i<m_Metadata.m_PathFinding.m_MaxCapsuleTests; i++)
			{
				const float orbitDistanceToTest			= maxOrbitDistanceToTest - (orbitDistanceRangeToTest * static_cast<float>(i) *
															invNumPathFindingCapsuleTestsLessOne);
				const Vector3 occludedPositionToTest	= collisionRootPosition - (orbitFrontBeforeMovement * orbitDistanceToTest);
				const Vector3 unoccludedPositionToTest	= collisionRootPosition - (rotatedUnoccludedOrbitFront * orbitDistanceToTest);

				//Perform a non-directed capsule test between the occluded and unoccluded positions.
				phIntersection intersection;
				hasCapsuleCollided	= TestCapsuleAgainstWorld(occludedPositionToTest, unoccludedPositionToTest, collisionTestRadius, intersection,
										m_IncludeFlagsForOcclusionTests, m_ShouldIgnoreOcclusionWithSelectCollision);
				if(!hasCapsuleCollided)
				{
					//We found a clear path from the 'popped-in' occluded position to an equivalent unoccluded position.

					if(isValidToMoveTowardsLos)
					{
						desiredHeadingDelta	= camFrame::ComputeHeadingDeltaBetweenFronts(orbitFrontBeforeMovement, rotatedUnoccludedOrbitFront);
						desiredPitchDelta	= camFrame::ComputePitchDeltaBetweenFronts(orbitFrontBeforeMovement, rotatedUnoccludedOrbitFront);

						//NOTE: We must report pop-in when movement towards an unoccluded position is not possible, as this allows us to
						//persist this behaviour, as necessary.
						wasCameraPoppedIn = false;
					}

					//Finally, pull the camera back as far as collision allows from the safe pop-in distance, at the *original* orbit orientation,
					//as we will not apply any orientation change until the next update.

					intersection.Reset();
					hasCapsuleCollided	= TestSweptSphereAgainstWorld(occludedPositionToTest, cameraPosition, collisionTestRadius, intersection,
											m_IncludeFlagsForOcclusionTests, m_ShouldIgnoreOcclusionWithSelectCollision);

					const float intersectionTValue	= hasCapsuleCollided ? intersection.GetT() : 1.0f;
					desiredOrbitDistance			= Lerp(intersectionTValue, orbitDistanceToTest, orbitDistanceBeforeMovement);

					collisionTestRadiusToConsiderForPullBack = collisionTestRadius;

					break;
				}
			}
		}
	}

	desiredPullBackRadius = collisionTestRadiusToConsiderForPullBack;

	//NOTE: We do not allow the camera to be further away from the collision root position after collision, as that would result in a discontinuity.
	desiredOrbitDistance = Clamp(desiredOrbitDistance, g_MinSafeOrbitDistanceAfterCollision, orbitDistanceBeforeMovement);

	//Reduce the test radius in readiness for use in a consecutive test.
	collisionTestRadius -= ms_MinRadiusReductionBetweenInterativeShapetests;

	PF_POP_TIMEBAR_DETAIL();

	return wasCameraPoppedIn;
}

void camCollision::ComputeDesiredOrbitDistanceDampingBehaviour(const Vector3& collisionRootPosition, const Vector3& preCollisionCameraPosition,
	float preCollisionOrbitDistance, float desiredOrbitDistance, bool& shouldPullBack, bool& shouldPausePullBack) const
{
	const float desiredOrbitDistanceRatio	= (desiredOrbitDistance / preCollisionOrbitDistance);
	shouldPullBack							= ((desiredOrbitDistance >= (m_DampedOrbitDistanceOnPreviousUpdate + m_Metadata.m_OrbitDistanceDamping.m_MaxDistanceErrorToIgnore)) &&
												(desiredOrbitDistanceRatio >= (m_DampedOrbitDistanceRatioOnPreviousUpdate + SMALL_FLOAT)));
	if(!shouldPullBack)
	{
		//Latch on the pausing of pull-back by default whenever we are not pulling back.
		shouldPausePullBack = true;

		return;
	}

	if(!shouldPausePullBack)
	{
		return;
	}

	//Pause pull-back if the camera and collision root are stationary, for improved continuity.
	const float collisionRootDistanceDelta	= collisionRootPosition.Dist(m_CollisionRootPositionOnPreviousUpdate);
	const float invTimeStep					= fwTimer::GetCamInvTimeStep();
	const float collisionRootMoveSpeed		= collisionRootDistanceDelta * invTimeStep;
	shouldPausePullBack						= (collisionRootMoveSpeed <= m_Metadata.m_OrbitDistanceDamping.m_MaxCollisionRootSpeedToPausePullBack);
	if(!shouldPausePullBack)
	{
		return;
	}

	const float preCollisionCameraDistanceDelta	= preCollisionCameraPosition.Dist(m_PreCollisionCameraPositionOnPreviousUpdate);
	const float preCollisionCameraMoveSpeed		= preCollisionCameraDistanceDelta * invTimeStep;
	shouldPausePullBack							= (preCollisionCameraMoveSpeed <= m_Metadata.m_OrbitDistanceDamping.m_MaxPreCollisionCameraSpeedToPausePullBack);	

	return;
}

bool camCollision::AvoidClipping(const Vector3& collisionRootPosition, const Vector3& orbitFront, float collisionTestRadius, float& desiredPullBackRadius,
	float& orbitDistanceToApply) const
{
	if(m_Metadata.m_ClippingAvoidance.m_MaxIterations < 1)
	{
		return false;
	}

	const Vector3 desiredCameraPosition	= collisionRootPosition - (orbitFront * orbitDistanceToApply);

	const Vector3 startPosition(desiredCameraPosition);

	const float capsuleLength	= Min(m_Metadata.m_ClippingAvoidance.m_CapsuleLengthForDetection, orbitDistanceToApply);
	const Vector3 endPosition	= startPosition + (orbitFront * capsuleLength);

	const u32 includeFlags		= m_ShouldIgnoreDynamicCollision ?
									static_cast<u32>(ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_GLASS_TYPE | ArchetypeFlags::GTA_DEEP_SURFACE_TYPE) : ms_CollisionIncludeFlagsForClippingTests;

	phIntersection intersection;
	const bool isClipping	= TestCapsuleAgainstWorld(startPosition, endPosition, collisionTestRadius, intersection, includeFlags,
								false, true, true);
	if(!isClipping)
	{
		return false;
	}

	const float stepSize		= orbitDistanceToApply / static_cast<float>(m_Metadata.m_ClippingAvoidance.m_MaxIterations);
	float orbitDistanceToTest	= orbitDistanceToApply;

	for(u32 i=0; i<m_Metadata.m_ClippingAvoidance.m_MaxIterations; i++)
	{
		orbitDistanceToTest = Max(orbitDistanceToTest - stepSize, 0.0f);

		const Vector3 testPosition = collisionRootPosition - (orbitFront * orbitDistanceToTest);

		//First, check that the test position is clear of collision, as we need to sweep from a valid start position. Don't worry about the collision root
		//position however, as we should use that irrespective.
		if(orbitDistanceToTest >= SMALL_FLOAT)
		{
			intersection.Reset();
			const bool hasHit = TestSphereAgainstWorld(testPosition, collisionTestRadius, intersection, includeFlags, false, true);
			if(hasHit)
			{
				continue;
			}
		}

		intersection.Reset();

		//NOTE: We must *not* treat polyhedral bounds as primitives when constraining the camera, as we do not do when avoiding occlusion and we must avoid strobing.
		//NOTE: We must reduce the radius of the swept spheres w.r.t. the sphere tests used to detect clipping. Otherwise we cannot be certain to consistently detect
		//a 'clipping' bound behind the camera.
		const float collisionTestRadiusForSweep	= collisionTestRadius - ms_MinRadiusReductionBetweenInterativeShapetests;
		const bool hasHit						= TestSweptSphereAgainstWorld(testPosition, desiredCameraPosition, collisionTestRadiusForSweep, intersection,
													includeFlags, false, false, true);

		const float intersectionTValue			= hasHit ? intersection.GetT() : 1.0f;
		orbitDistanceToApply					= Lerp(intersectionTValue, orbitDistanceToTest, orbitDistanceToApply);
		orbitDistanceToApply					= Max(orbitDistanceToApply, g_MinSafeOrbitDistanceAfterCollision);
		desiredPullBackRadius					= collisionTestRadiusForSweep;

		break;
	}

	return true;
}

void camCollision::PushBeyondEntitiesIfClipping(const Vector3& collisionRootPosition, const Vector3& orbitFront, float collisionTestRadius,
	float& desiredPullBackRadius, float& orbitDistanceToApply)
{
	PF_SET(camCollisionPushBeyondExtraOrbitDistance, 0.0f);

	const float initialOrbitDistance = orbitDistanceToApply;

	if(!cameraVerifyf(collisionTestRadius >= SMALL_FLOAT,
		"A collision helper was unable to push beyond entites if clipping due to an invalid test radius."))
	{
		m_PushBeyondOrbitDistanceSpring.Reset(initialOrbitDistance);
		m_WasPushedBeyondEntitiesOnPreviousUpdate = false;

		return;
	}

	const float minOrbitDistanceToPushBeyond = ComputeMinOrbitDistanceToPushBeyondEntitiesIfClipping(collisionRootPosition, orbitFront, initialOrbitDistance, collisionTestRadius);

	//Do not allow the orbit distance to be pushed in as a result of this process.
	float desiredOrbitDistance = Max(minOrbitDistanceToPushBeyond, initialOrbitDistance);

	//NOTE: We must cut to an increased minimum orbit distance in order to avoid clipping.
	float pushBeyondOrbitDistanceOnPreviousUpdate = m_PushBeyondOrbitDistanceSpring.GetResult();
	if(desiredOrbitDistance >= (pushBeyondOrbitDistanceOnPreviousUpdate + SMALL_FLOAT))
	{
		m_PushBeyondOrbitDistanceSpring.Reset(desiredOrbitDistance);
		pushBeyondOrbitDistanceOnPreviousUpdate = desiredOrbitDistance;
	}

	//Now we have reset/cut the damped spring as needed to push safely beyond entities, we can scale up the desired orbit distance some more before we
	//apply damping.
	desiredOrbitDistance	= minOrbitDistanceToPushBeyond * m_Metadata.m_PushBeyondEntitiesIfClipping.m_OrbitDistanceScalingToApplyWhenPushing;
	//Do not allow the orbit distance to be pushed in as a result of this process.
	desiredOrbitDistance	= Max(desiredOrbitDistance, initialOrbitDistance);

	const bool isPushBeyondDesiredThisUpdate = (desiredOrbitDistance >= (initialOrbitDistance + SMALL_FLOAT));
	if(!isPushBeyondDesiredThisUpdate && !m_WasPushedBeyondEntitiesOnPreviousUpdate)
	{
		m_PushBeyondOrbitDistanceSpring.Reset(initialOrbitDistance);

		return;
	}

	const bool shouldPushIn			= (desiredOrbitDistance <= (pushBeyondOrbitDistanceOnPreviousUpdate - SMALL_FLOAT));
	const float springConstant		= shouldPushIn ? m_Metadata.m_PushBeyondEntitiesIfClipping.m_PushInSpringConstant :
										m_Metadata.m_PushBeyondEntitiesIfClipping.m_PullBackSpringConstant;
	const float dampedOrbitDistance	= m_PushBeyondOrbitDistanceSpring.Update(desiredOrbitDistance, springConstant,
										m_Metadata.m_PushBeyondEntitiesIfClipping.m_SpringDampingRatio, true);

	const bool shouldPushBeyond = (dampedOrbitDistance >= (initialOrbitDistance + SMALL_FLOAT));
	if(!shouldPushBeyond)
	{
		m_PushBeyondOrbitDistanceSpring.Reset(initialOrbitDistance);
		m_WasPushedBeyondEntitiesOnPreviousUpdate = false;

		return;
	}

	orbitDistanceToApply = dampedOrbitDistance;

	//Avoid pulling back into other collision.
	const Vector3 desiredCameraPosition = collisionRootPosition - (orbitFront * orbitDistanceToApply);

	WorldProbe::CShapeTestCapsuleDesc capsuleTest;
	WorldProbe::CShapeTestHitPoint capsuleIsect;
	WorldProbe::CShapeTestResults capsuleResult(capsuleIsect);
	capsuleTest.SetResultsStructure(&capsuleResult);
	capsuleTest.SetIsDirected(true);
	capsuleTest.SetIncludeFlags(m_IncludeFlagsForOcclusionTests);
	//capsuleTest.SetStateIncludeFlags(phLevelBase::STATE_FLAG_FIXED);
	if (m_Metadata.m_ShouldReportAsCameraTypeTest)
	{
		capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	}
	capsuleTest.SetContext(WorldProbe::LOS_Camera);

	const s32 numEntitiesToPushBeyond = m_EntitiesToPushBeyondIfClippingThisUpdate.GetCount();
	if(numEntitiesToPushBeyond > 0)
	{
		const CEntity** entitiesToPushBeyond = m_EntitiesToPushBeyondIfClippingThisUpdate.GetElements();
		capsuleTest.SetExcludeEntities(entitiesToPushBeyond, numEntitiesToPushBeyond, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
	}

	capsuleTest.SetCapsule(collisionRootPosition, desiredCameraPosition, collisionTestRadius);

	const float preCollisionOrbitDistance = orbitDistanceToApply;

	const bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
	if(hasHit)
	{
		const float hitTValue	= capsuleResult[0].GetHitTValue();
		orbitDistanceToApply	*= hitTValue;

#if RSG_PC
		const bool shouldAllowOtherCollisionToConstrainCameraIntoEntities = !m_Metadata.m_PushBeyondEntitiesIfClipping.m_InheritDefaultShouldAllowOtherCollisionToConstrainCameraIntoEntitiesForTripleHead && camInterface::IsTripleHeadDisplay() ?
			m_Metadata.m_PushBeyondEntitiesIfClipping.m_ShouldAllowOtherCollisionToConstrainCameraIntoEntitiesForTripleHead :
			m_Metadata.m_PushBeyondEntitiesIfClipping.m_ShouldAllowOtherCollisionToConstrainCameraIntoEntities;

		if(!shouldAllowOtherCollisionToConstrainCameraIntoEntities)
#else
		if(!m_Metadata.m_PushBeyondEntitiesIfClipping.m_ShouldAllowOtherCollisionToConstrainCameraIntoEntities)
#endif
		{
			//Do not allow the orbit distance to constrain close enough to clip into the entities we pushed beyond.
			orbitDistanceToApply = Max(orbitDistanceToApply, minOrbitDistanceToPushBeyond);
		}

		//Do not allow the orbit distance to be pushed in as a result of this process.
		orbitDistanceToApply	= Max(orbitDistanceToApply, initialOrbitDistance);

		m_PushBeyondOrbitDistanceSpring.OverrideResult(orbitDistanceToApply);
	}

	//NOTE: We do not allow the camera to be further away from the collision root position after collision, as that would result in a discontinuity.
	const float maxPullBackDistance	= Max(preCollisionOrbitDistance - orbitDistanceToApply, 0.0f);
	desiredPullBackRadius			= Min(collisionTestRadius, maxPullBackDistance);

	m_WasPushedBeyondEntitiesOnPreviousUpdate = true;

	PF_SET(camCollisionPushBeyondExtraOrbitDistance, orbitDistanceToApply - initialOrbitDistance);
}

float camCollision::ComputeMinOrbitDistanceToPushBeyondEntitiesIfClipping(const Vector3& collisionRootPosition, const Vector3& orbitFront,
	float initialOrbitDistance, float collisionTestRadius) const
{
	const s32 numEntitiesToPushBeyond = m_EntitiesToPushBeyondIfClippingThisUpdate.GetCount();
	if(numEntitiesToPushBeyond <= 0)
	{
		return 0.0f;
	}

	const CEntity* const* entitiesToPushBeyond = m_EntitiesToPushBeyondIfClippingThisUpdate.GetElements();
	const u32 includeFlags		= m_ShouldIgnoreDynamicCollision ?
									static_cast<u32>(ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_GLASS_TYPE | ArchetypeFlags::GTA_DEEP_SURFACE_TYPE) : ms_CollisionIncludeFlagsForClippingTests;

	WorldProbe::CShapeTestCapsuleDesc capsuleTest;
	WorldProbe::CShapeTestHitPoint capsuleIsect;
	WorldProbe::CShapeTestResults capsuleResult(capsuleIsect);
	capsuleTest.SetResultsStructure(&capsuleResult);
	capsuleTest.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
	capsuleTest.SetIsDirected(true);
	capsuleTest.SetIncludeFlags(includeFlags);
	if (m_Metadata.m_ShouldReportAsCameraTypeTest)
	{
		capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
	}
	capsuleTest.SetContext(WorldProbe::LOS_Camera);

	float minOrbitDistanceToApply = 0.0f;

	for(s32 i=0; i<numEntitiesToPushBeyond; i++)
	{
		if(!entitiesToPushBeyond[i])
		{
			continue;
		}

		//Compute a rough estimate of how far out we must sweep a sphere from to be guaranteed to be outside of the bounds of the entity.
		float orbitDistanceToTest = initialOrbitDistance + (2.0f * collisionTestRadius);

		Vector3 boundCentre;
		const float boundRadius						= entitiesToPushBeyond[i]->GetBoundCentreAndRadius(boundCentre);
		const float distanceToBoundCentre			= collisionRootPosition.Dist(boundCentre);
		const float orbitDistanceToTestForEntity	= distanceToBoundCentre + boundRadius + (2.0f * collisionTestRadius);
		if(orbitDistanceToTestForEntity > orbitDistanceToTest)
		{
			orbitDistanceToTest = orbitDistanceToTestForEntity;
		}

		const Vector3 capsuleStartPosition = collisionRootPosition - (orbitFront * orbitDistanceToTest);

		capsuleResult.Reset();
		capsuleTest.SetIncludeInstance(NULL);
		capsuleTest.SetIncludeEntity(entitiesToPushBeyond[i], WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
		capsuleTest.SetCapsule(capsuleStartPosition, collisionRootPosition, collisionTestRadius);

		const bool hasHit = WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
		if(!hasHit)
		{
			continue;
		}

		const float hitTValue			= capsuleResult[0].GetHitTValue();
		float orbitDistanceAfterPush	= (1.0f - hitTValue) * orbitDistanceToTest;

		//NOTE: We need to adjust this distance slightly as it would otherwise be over-generous and we need to be careful to reduce the chances of
		//the camera visibly clipping into other collision after the push.

		orbitDistanceAfterPush			+= m_Metadata.m_PushBeyondEntitiesIfClipping.m_ExtraDistanceToPushAway;
		orbitDistanceAfterPush			-= collisionTestRadius;
		if(m_Metadata.m_ShouldPullBackByCapsuleRadius)
		{
			//NOTE: If we are going to pull back by the capsule radius, we can afford to reduce the minimum distance to push accordingly.
			orbitDistanceAfterPush		-= collisionTestRadius;
		}

		if(orbitDistanceAfterPush > minOrbitDistanceToApply)
		{
			minOrbitDistanceToApply = orbitDistanceAfterPush;
		}
	}

	return minOrbitDistanceToApply;
}

bool camCollision::UpdateBuoyancy(bool isBuoyant, float preCollisionOrbitDistance, float collisionTestRadius, float& desiredPullBackRadius,
	const Vector3& collisionRootPosition, Vector3& cameraPosition)
{
	const camCollisionMetadataBuoyancySettings& settings = m_Metadata.m_BuoyancySettings;
	if(!settings.m_ShouldApplyBuoyancy)
	{
		return false;
	}

	//Test for water up to twice the (pre-collision) orbit distance from the camera, as this ensures that the orbit can't generate a camera position
	//that is out of range of water that needs to be constrained against.
	const float maxDistanceToTestForWater = 2.0f * preCollisionOrbitDistance;

	float waterHeight = 0.0f;
	const bool hasFoundWater	= ComputeWaterHeightAtPosition(cameraPosition, maxDistanceToTestForWater, waterHeight, maxDistanceToTestForWater,
									settings.m_MinHitNormalDotWorldUpForRivers);
	if(!hasFoundWater)
	{
		m_SmoothedLocalWaterHeight = g_InvalidWaterHeight;
		return false;
	}

	if(IsClose(m_SmoothedLocalWaterHeight, g_InvalidWaterHeight))
	{
		m_SmoothedLocalWaterHeight	= waterHeight;
	}
	else
	{
		//Cheaply make the smoothing frame-rate independent.
		float smoothRate			= settings.m_WaterHeightSmoothRate * 30.0f * fwTimer::GetCamTimeStep();
		smoothRate					= Min(smoothRate, 1.0f);
		m_SmoothedLocalWaterHeight	= Lerp(smoothRate, m_SmoothedLocalWaterHeight, waterHeight);
	}

	const float heightDelta = cameraPosition.z - m_SmoothedLocalWaterHeight;
	if(settings.m_ShouldIgnoreBuoyancyStateAndAvoidSurface)
	{
		//Override the requested buoyancy state based upon the camera height relative to the water surface, so that we avoid the threshold.
		isBuoyant = (heightDelta >= 0.0f);
	}

	//NOTE: We apply a soft limit from twice the min delta, rather than a hard limit at the minimum.
	const float minSafeHeightDelta		= isBuoyant ? settings.m_MinHeightAboveWaterWhenBuoyant : settings.m_MinDepthUnderWaterWhenNotBuoyant;
	const float heightDeltaRatio		= heightDelta / (2.0f * minSafeHeightDelta);
	const float buoyancySign			= isBuoyant ? 1.0f : -1.0f;

	bool hasConstrainedAgainstWater;
	if((buoyancySign * heightDeltaRatio) >= SMALL_FLOAT)
	{
		const float pushAwayRatio		= SlowIn(1.0f - Min(Abs(heightDeltaRatio), 1.0f));
		const float distanceToPushAway	= pushAwayRatio * minSafeHeightDelta;
		cameraPosition.z				+= buoyancySign * distanceToPushAway;
		hasConstrainedAgainstWater		= (distanceToPushAway >= SMALL_FLOAT);
	}
	else
	{
		cameraPosition.z				= waterHeight + (buoyancySign * minSafeHeightDelta);
		hasConstrainedAgainstWater		= true;
	}

	if(hasConstrainedAgainstWater && cameraVerifyf(collisionTestRadius >= SMALL_FLOAT,
		"A collision helper was unable to compute a valid camera position under buoyancy due to an invalid test radius."))
	{
		//Ensure that we don't constrain the camera into collision.
		ComputeUnoccludedCameraPosition(collisionTestRadius, desiredPullBackRadius, collisionRootPosition, cameraPosition);
	}

	return hasConstrainedAgainstWater;
}

bool camCollision::ComputeWaterHeightAtPosition(const Vector3& positionToTest, float maxDistanceToTestForWater, float& waterHeight, float maxDistanceToCheckForRiverWater, float minHitNormalDotWorldUpForRivers, bool shouldConsiderFollowPed)
{	
	bool isOnOcean = false;
	return ComputeIsOverOceanOrRiver(positionToTest, maxDistanceToTestForWater, maxDistanceToCheckForRiverWater, waterHeight, isOnOcean, minHitNormalDotWorldUpForRivers, shouldConsiderFollowPed);
}

bool camCollision::ComputeIsOverOceanOrRiver(const Vector3& positionToTest, float maxDistanceToTestForWater, float maxDistanceToTestForRiver, float& waterHeight, bool& isInOcean, float minHitNormalDotWorldUpForRivers, bool shouldConsiderFollowPed)
{
	isInOcean = false;
	//Use the follow ped as a context for now, as using the camera's attach parent can become problematic when interpolating between cameras.
	const CPed* followPed			= shouldConsiderFollowPed ? camInterface::FindFollowPed() : NULL;
	const bool hasFoundOceanWater	= Water::GetWaterLevel(positionToTest, &waterHeight, true, maxDistanceToTestForWater, maxDistanceToTestForWater,
		NULL, followPed);

	//Now probe for river bounds. First, ensure that the test position is vaguely close to a river entity, as an optimisation.
	const bool isNearRiverEntity = River::IsPositionNearRiver(RCC_VEC3V(positionToTest), NULL, false, true, ScalarV(maxDistanceToTestForRiver));
	if(!isNearRiverEntity)
	{
		isInOcean = hasFoundOceanWater;
		return hasFoundOceanWater;
	}

	WorldProbe::CShapeTestProbeDesc probeTest;
	WorldProbe::CShapeTestHitPoint hitPoint;
	WorldProbe::CShapeTestResults result(hitPoint);
	probeTest.SetResultsStructure(&result);
	probeTest.SetContext(WorldProbe::ERiverBoundQuery);
	probeTest.SetIsDirected(false);
	probeTest.SetIncludeFlags(ArchetypeFlags::GTA_RIVER_TYPE);

	Vector3 startOffset;
	startOffset.SetScaled(ZAXIS, maxDistanceToTestForRiver);
	Vector3 endOffset;
	endOffset.SetScaled(ZAXIS, -maxDistanceToTestForRiver);

	Vector3 startPosition;
	startPosition.Add(positionToTest, startOffset);
	Vector3 endPosition;
	endPosition.Add(positionToTest, endOffset);

	probeTest.SetStartAndEnd(startPosition, endPosition);

	bool hasFoundRiverWater = WorldProbe::GetShapeTestManager()->SubmitTest(probeTest);
	if(hasFoundRiverWater)
	{
		const Vector3& hitNormal	= result[0].GetHitNormal();
		const float hitNormalDotZ	= hitNormal.Dot(ZAXIS);
		if(hitNormalDotZ >= minHitNormalDotWorldUpForRivers)
		{
			const Vector3& hitPosition = result[0].GetHitPosition();
			if(hasFoundOceanWater)
			{
				waterHeight = Max(waterHeight, hitPosition.z);
			}
			else
			{
				waterHeight = hitPosition.z;
			}
		}
		else
		{
			hasFoundRiverWater = false;
		}
	}

	return hasFoundOceanWater || hasFoundRiverWater;
}

bool camCollision::ComputeUnoccludedCameraPosition(float collisionTestRadius, float& desiredPullBackRadius, const Vector3& collisionRootPosition, Vector3& cameraPosition) const
{
	phIntersection intersection;
	const bool isOccluded	= TestSweptSphereAgainstWorld(collisionRootPosition, cameraPosition, collisionTestRadius, intersection, m_IncludeFlagsForOcclusionTests,
								m_ShouldIgnoreOcclusionWithSelectCollision);
	if(isOccluded)
	{
		const Vector3 orbitDelta(collisionRootPosition - cameraPosition);
		const float initialOrbitDistance = orbitDelta.Mag();
		Vector3 orbitFront(orbitDelta);
		orbitFront.NormalizeSafe();

		const float intersectionTValue	= intersection.GetT();
		float desiredOrbitDistance		= intersectionTValue * initialOrbitDistance;

		//NOTE: We do not allow the camera to be further away from the collision root position after collision, as that would result in a discontinuity.
		desiredOrbitDistance = Clamp(desiredOrbitDistance, g_MinSafeOrbitDistanceAfterCollision, initialOrbitDistance);

		cameraPosition = collisionRootPosition - (orbitFront * desiredOrbitDistance);

		//NOTE: We do not allow the camera to be further away from the collision root position after collision, as that would result in a discontinuity.
		const float maxPullBackDistance	= Max(initialOrbitDistance - desiredOrbitDistance, 0.0f);
		desiredPullBackRadius			= Min(collisionTestRadius, maxPullBackDistance);
	}

	return isOccluded;
}

bool camCollision::TestCapsuleAgainstWorld(const Vector3& startPosition, const Vector3& endPosition, float radius, phIntersection& intersection,
	u32 includeFlags, bool shouldApplyRestrictionsForOcclusion, bool shouldTreatPolyhedralBoundsAsPrimitives, bool shouldLogExtraEntitiesToPushBeyond) const
{
	//Copy across any settings that need to be queried in the static instance reject callback function.
	ms_EntitiesToIgnore												= m_EntitiesToIgnoreThisUpdate;
	ms_EntityToConsiderForTouchingChecks							= m_EntityToConsiderForTouchingChecksThisUpdate;
	ms_ShouldIgnoreOcclusionWithBrokenFragments						= shouldApplyRestrictionsForOcclusion && m_Metadata.m_ShouldIgnoreOcclusionWithBrokenFragments;
	ms_ShouldIgnoreOcclusionWithBrokenFragmentsOfIgnoredEntities	= shouldApplyRestrictionsForOcclusion && m_Metadata.m_ShouldIgnoreOcclusionWithBrokenFragmentsOfIgnoredEntities;
	ms_ShouldIgnoreCollisionWithTouchingPedsAndVehicles				= m_ShouldIgnoreCollisionWithTouchingPedsAndVehiclesThisUpdate;
	ms_ShouldLogExtraEntitiesToPushBeyond							= shouldLogExtraEntitiesToPushBeyond;

	phSegment segment(startPosition, endPosition);
	ms_CapsuleTest.InitCapsule(segment, radius, &intersection, 1);
	ms_CapsuleTest.SetIncludeFlags(includeFlags);
	ms_CapsuleTest.SetTreatPolyhedralBoundsAsPrimitives(shouldTreatPolyhedralBoundsAsPrimitives);
	ms_CapsuleTest.SetLevel(CPhysics::GetLevel());

	phMaterialMgrGta::Id materialFlags = PGTAMATERIALMGR->GetPackedPolyFlagValue(POLYFLAG_NO_CAM_COLLISION_ALLOW_CLIPPING);
	if(shouldApplyRestrictionsForOcclusion)
	{
		materialFlags |= PGTAMATERIALMGR->GetPackedPolyFlagValue(POLYFLAG_NO_CAM_COLLISION);
	}

	ms_CapsuleTest.GetShape().SetIgnoreMaterialFlags(materialFlags);

	const bool hasCollided = (ms_CapsuleTest.TestInLevel() > 0);

	return hasCollided;
}

bool camCollision::TestSweptSphereAgainstWorld(const Vector3& startPosition, const Vector3& endPosition, float radius, phIntersection& intersection,
	u32 includeFlags, bool shouldApplyRestrictionsForOcclusion, bool shouldTreatPolyhedralBoundsAsPrimitives, bool shouldLogExtraEntitiesToPushBeyond) const
{
	//Copy across any settings that need to be queried in the static instance reject callback function.
	ms_EntitiesToIgnore												= m_EntitiesToIgnoreThisUpdate;
	ms_EntityToConsiderForTouchingChecks							= m_EntityToConsiderForTouchingChecksThisUpdate;
	ms_ShouldIgnoreOcclusionWithBrokenFragments						= shouldApplyRestrictionsForOcclusion && m_Metadata.m_ShouldIgnoreOcclusionWithBrokenFragments;
	ms_ShouldIgnoreOcclusionWithBrokenFragmentsOfIgnoredEntities	= shouldApplyRestrictionsForOcclusion && m_Metadata.m_ShouldIgnoreOcclusionWithBrokenFragmentsOfIgnoredEntities;
	ms_ShouldIgnoreCollisionWithTouchingPedsAndVehicles				= m_ShouldIgnoreCollisionWithTouchingPedsAndVehiclesThisUpdate;
	ms_ShouldLogExtraEntitiesToPushBeyond							= shouldLogExtraEntitiesToPushBeyond;

	phSegment segment(startPosition, endPosition);
	ms_SweptSphereTest.InitSweptSphere(segment, radius, &intersection, 1);
	ms_SweptSphereTest.SetIncludeFlags(includeFlags);
	ms_SweptSphereTest.SetTreatPolyhedralBoundsAsPrimitives(shouldTreatPolyhedralBoundsAsPrimitives);
	ms_SweptSphereTest.SetLevel(CPhysics::GetLevel());

	phMaterialMgrGta::Id materialFlags = PGTAMATERIALMGR->GetPackedPolyFlagValue(POLYFLAG_NO_CAM_COLLISION_ALLOW_CLIPPING);
	if(shouldApplyRestrictionsForOcclusion)
	{
		materialFlags |= PGTAMATERIALMGR->GetPackedPolyFlagValue(POLYFLAG_NO_CAM_COLLISION);
	}

	ms_SweptSphereTest.GetShape().SetIgnoreMaterialFlags(materialFlags);

	const bool hasCollided = (ms_SweptSphereTest.TestInLevel() > 0);

	return hasCollided;
}

bool camCollision::TestSphereAgainstWorld(const Vector3& centre, float radius, phIntersection& intersection,
	u32 includeFlags, bool shouldApplyRestrictionsForOcclusion, bool shouldTreatPolyhedralBoundsAsPrimitives, bool shouldLogExtraEntitiesToPushBeyond) const
{
	//Copy across any settings that need to be queried in the static instance reject callback function.
	ms_EntitiesToIgnore												= m_EntitiesToIgnoreThisUpdate;
	ms_EntityToConsiderForTouchingChecks							= m_EntityToConsiderForTouchingChecksThisUpdate;
	ms_ShouldIgnoreOcclusionWithBrokenFragments						= shouldApplyRestrictionsForOcclusion && m_Metadata.m_ShouldIgnoreOcclusionWithBrokenFragments;
	ms_ShouldIgnoreOcclusionWithBrokenFragmentsOfIgnoredEntities	= shouldApplyRestrictionsForOcclusion && m_Metadata.m_ShouldIgnoreOcclusionWithBrokenFragmentsOfIgnoredEntities;
	ms_ShouldIgnoreCollisionWithTouchingPedsAndVehicles				= m_ShouldIgnoreCollisionWithTouchingPedsAndVehiclesThisUpdate;
	ms_ShouldLogExtraEntitiesToPushBeyond							= shouldLogExtraEntitiesToPushBeyond;

	ms_SphereTest.InitSphere(centre, radius, &intersection, 1);
	ms_SphereTest.SetIncludeFlags(includeFlags);
	ms_SphereTest.SetTreatPolyhedralBoundsAsPrimitives(shouldTreatPolyhedralBoundsAsPrimitives);
	ms_SphereTest.SetLevel(CPhysics::GetLevel());

	phMaterialMgrGta::Id materialFlags = PGTAMATERIALMGR->GetPackedPolyFlagValue(POLYFLAG_NO_CAM_COLLISION_ALLOW_CLIPPING);
	if(shouldApplyRestrictionsForOcclusion)
	{
		materialFlags |= PGTAMATERIALMGR->GetPackedPolyFlagValue(POLYFLAG_NO_CAM_COLLISION);
	}

	ms_SphereTest.GetShape().SetIgnoreMaterialFlags(materialFlags);

	const bool hasCollided = (ms_SphereTest.TestInLevel() > 0);

	return hasCollided;
}

bool camCollision::ComputeIsLineOfSightClear(const Vector3& startPosition, const Vector3& endPosition, const CEntity* entityToIgnore,
	u32 includeFlags, const CEntity* secondEntityToIgnore)
{
	ms_EntitiesToIgnore.Reset();
	if(entityToIgnore)
	{
		ms_EntitiesToIgnore.Grow() = entityToIgnore;
	}
	if(secondEntityToIgnore)
	{
		ms_EntitiesToIgnore.Grow() = secondEntityToIgnore;
	}

	ms_EntityToConsiderForTouchingChecks							= NULL;
	ms_ShouldIgnoreOcclusionWithBrokenFragments						= false;
	ms_ShouldIgnoreOcclusionWithBrokenFragmentsOfIgnoredEntities	= false;
	ms_ShouldIgnoreCollisionWithTouchingPedsAndVehicles				= false;
	ms_ShouldLogExtraEntitiesToPushBeyond							= false;

	phSegment segment(startPosition, endPosition);
	ms_EdgeTest.InitEdge(segment);
	ms_EdgeTest.SetIncludeFlags(includeFlags);
	ms_EdgeTest.SetLevel(CPhysics::GetLevel());

	const phMaterialMgrGta::Id materialFlags	= PGTAMATERIALMGR->GetPackedPolyFlagValue(POLYFLAG_NO_CAM_COLLISION) |
													PGTAMATERIALMGR->GetPackedPolyFlagValue(POLYFLAG_NO_CAM_COLLISION_ALLOW_CLIPPING);
	ms_EdgeTest.GetShape().SetIgnoreMaterialFlags(materialFlags);

	const bool hasCollided = (ms_EdgeTest.TestInLevel() > 0);

	return !hasCollided;
}

bool camCollision::ComputeDoesSphereCollide(const Vector3& centre, float radius, const CEntity* entityToIgnore, u32 includeFlags)
{
	ms_EntitiesToIgnore.Reset();
	if(entityToIgnore)
	{
		ms_EntitiesToIgnore.Grow() = entityToIgnore;
	}

	ms_EntityToConsiderForTouchingChecks							= NULL;
	ms_ShouldIgnoreOcclusionWithBrokenFragments						= false;
	ms_ShouldIgnoreOcclusionWithBrokenFragmentsOfIgnoredEntities	= false;
	ms_ShouldIgnoreCollisionWithTouchingPedsAndVehicles				= false;
	ms_ShouldLogExtraEntitiesToPushBeyond							= false;

	ms_SphereTest.InitSphere(centre, radius);
	ms_SphereTest.SetIncludeFlags(includeFlags);
	ms_SphereTest.SetLevel(CPhysics::GetLevel());

	const phMaterialMgrGta::Id materialFlags	= PGTAMATERIALMGR->GetPackedPolyFlagValue(POLYFLAG_NO_CAM_COLLISION) |
													PGTAMATERIALMGR->GetPackedPolyFlagValue(POLYFLAG_NO_CAM_COLLISION_ALLOW_CLIPPING);
	ms_SphereTest.GetShape().SetIgnoreMaterialFlags(materialFlags);

	const bool hasCollided = (ms_SphereTest.TestInLevel() > 0);

	return hasCollided;
}

bool camCollision::ShouldIgnoreEntity(atHashString modelName)
{
	static const u32 listSize = 1;
	static const atHashString list[listSize] = {
		ATSTRINGHASH("h4_p_h4_m_bag_var22_arm_s", 0xE7FAE3F1)
	};

	if (NetworkInterface::IsGameInProgress())
	{
		for (u32 i = 0; i < listSize; ++i)
		{
			if (modelName == list[i])
			{
				cameraDebugf1("Camera collision is ignoring hit entity %s", modelName.GetCStr());
				return true;
			}
		}
	}

	return false;
}

//The specified instance is rejected if false is returned.
bool camCollision::InstanceRejectCallback(const phInst* instance)
{
	if(!cameraVerifyf(instance, "The instance reject callback of a camera collision helper was called with a NULL instance"))
	{
		return false;
	}

	const CEntity* entity = CPhysics::GetEntityFromInst(instance);
	if(entity && entity->IsArchetypeSet())
	{
		//Check this entity and (optionally) its frag parent against this list of entities to ignore. Also (optionally) check if it's a broken
		//fragment.
		if(ShouldIgnoreEntity(entity->GetModelNameHash()))
		{
			return false;
		}

		const bool shouldIgnoreEntity = (ms_EntitiesToIgnore.Find(entity) != -1);
		if(shouldIgnoreEntity)
		{
			return false;
		}

		if(ms_ShouldIgnoreOcclusionWithBrokenFragments && IsFragInst(instance))
		{
			const fragInst* fragInstance	= static_cast<const fragInst*>(instance);
			const fragCacheEntry* entry		= fragInstance->GetCacheEntry();
			if(entry && entry->IsGroupBroken(0))
			{
				return false;
			}
		}

		if(ms_ShouldIgnoreOcclusionWithBrokenFragmentsOfIgnoredEntities && entity->GetIsTypeObject())
		{
			const CEntity* fragParent			=  static_cast<const CObject*>(entity)->GetFragParent();
			const bool shouldIgnoreFragParent	= (ms_EntitiesToIgnore.Find(fragParent) != -1);
			if(shouldIgnoreFragParent)
			{
				return false;
			}
		}

		const CVehicle* vehicleEntity	= entity->GetIsTypeVehicle() ? static_cast<const CVehicle*>(entity) : NULL;
		const bool isPushBeyondDisabled = vehicleEntity && vehicleEntity->GetVehicleModelInfo() && vehicleEntity->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DISABLE_CAMERA_PUSH_BEYOND);
		const bool isTouching			= (ms_EntityToConsiderForTouchingChecks && ms_EntityToConsiderForTouchingChecks->GetIsTouching(entity));
		if(isTouching && !isPushBeyondDisabled)
		{
			//This entity is very close, so check if we need to ignore/push beyond it.

			if(entity->GetIsPhysical() && ((CPhysical*)entity)->GetNoCollisionEntity() && (((CPhysical*)entity)->GetNoCollisionFlags() &
				NO_COLLISION_NETWORK_OBJECTS))
			{
				if(ms_ShouldLogExtraEntitiesToPushBeyond)
				{
					//Log this entity so that we can choose to push beyond it after the shapetest.
					ms_ExtraEntitiesToPushBeyond.Grow() = entity;
				}

				return false;
			}

			if(ms_ShouldIgnoreCollisionWithTouchingPedsAndVehicles && (entity->GetIsTypePed() || entity->GetIsTypeVehicle()))
			{
				if(ms_ShouldLogExtraEntitiesToPushBeyond)
				{
					//Log this entity so that we can choose to push beyond it after the shapetest.
					ms_ExtraEntitiesToPushBeyond.Grow() = entity;
				}

				return false;
			}
		}
	}

	return true;
}
