//
// camera/helpers/Collision.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef CAMERA_COLLISION_H
#define CAMERA_COLLISION_H

#include "physics/shapetest.h"

#include "camera/base/BaseCamera.h"
#include "camera/base/BaseObject.h"
#include "camera/helpers/DampedSpring.h"

class camCollisionMetadata;
class CEntity;

//A camera collision helper that can adjust camera position in order to preserve line-of-sight to the attached entity.
class camCollision : public camBaseObject
{
	DECLARE_RTTI_DERIVED_CLASS(camCollision, camBaseObject);

public:
	FW_REGISTER_CLASS_POOL(camCollision);

	template <class _T> friend class camFactoryHelper; //Allow the factory helper class to access the protected constructor.

private:
	camCollision(const camBaseObjectMetadata& metadata);

public:
	static void			InitClass();

	static u32			GetCollisionIncludeFlags()							{ return ms_CollisionIncludeFlags; }
	static u32			GetCollisionIncludeFlagsForClippingTests()			{ return ms_CollisionIncludeFlagsForClippingTests; }
	static float		GetMinRadiusReductionBetweenInterativeShapetests()	{ return ms_MinRadiusReductionBetweenInterativeShapetests; }

	static bool			ComputeIsLineOfSightClear(const Vector3& startPosition, const Vector3& endPosition, const CEntity* entityToIgnore = NULL,
							u32 includeFlags = ms_CollisionIncludeFlags, const CEntity* secondEntityToIgnore = NULL);
	static bool			ComputeDoesSphereCollide(const Vector3& centre, float radius, const CEntity* entityToIgnore = NULL,
							u32 includeFlags = ms_CollisionIncludeFlags);

	static bool			ComputeWaterHeightAtPosition(const Vector3& positionToTest, float maxDistanceToTestForWater, float& waterHeight,
							float maxDistanceToCheckForRiverWater, float minHitNormalDotWorldUpForRivers = 0.5f, bool shouldConsiderFollowPed = true);

	static bool			ComputeIsOverOceanOrRiver(const Vector3& positionToTest, float maxDistanceToTestForWater, float maxDistanceToTestForRiver, float& waterHeight, bool& isInOcean, float minHitNormalDotWorldUpForRivers = 0.5f, bool shouldConsiderFollowPed = true);

	static void			AddEntityToList(const CEntity& entity, atArray<const CEntity*>& entityList, const bool shouldReportAsCameraTypeTest = true);

	void				Reset();
	void				IgnoreEntityThisUpdate(const CEntity& entity);
	void				IgnoreCollisionWithTouchingPedsAndVehiclesThisUpdate()		{ m_ShouldIgnoreCollisionWithTouchingPedsAndVehiclesThisUpdate = true; }
	void				PushBeyondEntityIfClippingThisUpdate(const CEntity& entity);
	void				SetEntityToConsiderForTouchingChecksThisUpdate(const CEntity& entity) { m_EntityToConsiderForTouchingChecksThisUpdate = &entity; }
	void				SetShouldIgnoreDynamicCollision(bool shouldIgnoreDynamicCollision) { m_ShouldIgnoreDynamicCollision = shouldIgnoreDynamicCollision; }

	const CEntity* const* GetEntitiesToIgnoreThisUpdate() const			{ return m_EntitiesToIgnoreThisUpdate.GetElements(); }
	s32					GetNumEntitiesToIgnoreThisUpdate() const		{ return m_EntitiesToIgnoreThisUpdate.GetCount(); }

	float				GetInitialCollisionTestRadius() const				{ return m_InitialCollisionTestRadius; }
	float				GetOrbitDistanceRatioOnPreviousUpdate() const		{ return Clamp(m_OrbitDistanceRatioOnPreviousUpdate, 0.0f, 1.0f); }
	bool				WasCameraConstrainedOffAxisOnPreviousUpdate() const	{ return m_WasCameraConstrainedOffAxisOnPreviousUpdate; }

	void				BypassThisUpdate()			{ m_ShouldBypassThisUpdate = true; }
	void				BypassBuoyancyThisUpdate()	{ m_ShouldBypassBuoyancyThisUpdate = true; }
	void				ForcePopInThisUpdate()		{ m_ShouldForcePopInThisUpdate = true; }

	void				SetShouldIgnoreOcclusionWithSelectCollision(bool state) { m_ShouldIgnoreOcclusionWithSelectCollision = state; }

	void				CloneDamping(const camCollision& sourceCollision);

	void				PreUpdate();

	void				Update(const Vector3& collisionRootPosition, float collisionTestRadius, bool isBuoyant,
							Vector3& cameraPosition, float& headingDelta, float& pitchDelta);

private:
	bool				ComputeDesiredOrbitDistanceAndOrientationDelta(const Vector3& collisionRootPosition, const Vector3& cameraPosition,
							float& collisionTestRadius, float& desiredOrbitDistance, float& desiredHeadingDelta, float& desiredPitchDelta,
							float& desiredPullBackRadius) const;
	void				ComputeDesiredOrbitDistanceDampingBehaviour(const Vector3& collisionRootPosition, const Vector3& preCollisionCameraPosition,
							float preCollisionOrbitDistance, float desiredOrbitDistance, bool& shouldPullBack, bool& shouldPausePullBack) const;
	bool				AvoidClipping(const Vector3& collisionRootPosition, const Vector3& orbitFront, float collisionTestRadius, float& desiredPullBackRadius,
							float& orbitDistanceToApply) const;
	void				PushBeyondEntitiesIfClipping(const Vector3& collisionRootPosition, const Vector3& orbitFront, float collisionTestRadius,
							float& desiredPullBackRadius, float& orbitDistanceToApply);
	float				ComputeMinOrbitDistanceToPushBeyondEntitiesIfClipping(const Vector3& collisionRootPosition, const Vector3& orbitFront,
							float initialOrbitDistance, float collisionTestRadius) const;
	bool				UpdateBuoyancy(bool isBuoyant, float preCollisionOrbitDistance, float collisionTestRadius, float& desiredPullBackRadius,
							const Vector3& collisionRootPosition, Vector3& cameraPosition);
	bool				ComputeUnoccludedCameraPosition(float collisionTestRadius, float& desiredPullBackRadius, const Vector3& collisionRootPosition, Vector3& cameraPosition) const;

	bool				TestCapsuleAgainstWorld(const Vector3& startPosition, const Vector3& endPosition, float radius, phIntersection& intersection,
							u32 includeFlags = ms_CollisionIncludeFlags, bool shouldApplyRestrictionsForOcclusion = true,
							bool shouldTreatPolyhedralBoundsAsPrimitives = false, bool shouldLogExtraEntitiesToPushBeyond = false) const;
	bool				TestSweptSphereAgainstWorld(const Vector3& startPosition, const Vector3& endPosition, float radius, phIntersection& intersection,
							u32 includeFlags = ms_CollisionIncludeFlags, bool shouldApplyRestrictionsForOcclusion = true,
							bool shouldTreatPolyhedralBoundsAsPrimitives = false, bool shouldLogExtraEntitiesToPushBeyond = false) const;
	bool				TestSphereAgainstWorld(const Vector3& centre, float radius, phIntersection& intersection,
							u32 includeFlags = ms_CollisionIncludeFlags, bool shouldApplyRestrictionsForOcclusion = true,
							bool shouldTreatPolyhedralBoundsAsPrimitives = false, bool shouldLogExtraEntitiesToPushBeyond = false) const;

	static bool			ShouldIgnoreEntity(atHashString modelName);
	static bool			InstanceRejectCallback(const phInst* instance);

	const camCollisionMetadata&	m_Metadata;

	camDampedSpring		m_HeadingDeltaSpring;
	camDampedSpring		m_PitchDeltaSpring;
	camDampedSpring		m_OrbitDistanceSpring;
	camDampedSpring		m_PushBeyondOrbitDistanceSpring;
	camDampedSpring		m_PullBackTowardsCollisionSpring;
	atArray<const CEntity*> m_EntitiesToIgnoreThisUpdate;
	atArray<const CEntity*> m_EntitiesToPushBeyondIfClippingThisUpdate;
	u32					m_IncludeFlagsForOcclusionTests;
	float				m_InitialCollisionTestRadius;

	// 16 byte alligned...
	Vector3				m_CollisionRootPositionOnPreviousUpdate;
	Vector3				m_PreCollisionCameraPositionOnPreviousUpdate;
	const CEntity*		m_EntityToConsiderForTouchingChecksThisUpdate;
	float				m_DampedOrbitDistanceOnPreviousUpdate;
	float				m_DampedOrbitDistanceRatioOnPreviousUpdate;
	float				m_OrbitDistanceRatioOnPreviousUpdate;
	float				m_SmoothedLocalWaterHeight;
	bool				m_ShouldBypassThisUpdate;
	bool				m_ShouldBypassBuoyancyThisUpdate;
	bool				m_ShouldForcePopInThisUpdate;
	bool				m_ShouldIgnoreCollisionWithTouchingPedsAndVehiclesThisUpdate;
	bool				m_ShouldPausePullBack;
	bool				m_ShouldIgnoreOcclusionWithSelectCollision;
	bool				m_ShouldIgnoreDynamicCollision;
	bool				m_WasPushedBeyondEntitiesOnPreviousUpdate;
	bool				m_WasCameraPoppedInOnPreviousUpdate;
	bool				m_WasCameraConstrainedOffAxisOnPreviousUpdate;

	static phShapeTest<phShapeCapsule>		ms_CapsuleTest;
	static phShapeTest<phShapeSweptSphere>	ms_SweptSphereTest;
	static phShapeTest<phShapeSphere>		ms_SphereTest;
	static phShapeTest<phShapeProbe>		ms_ProbeTest;
	static phShapeTest<phShapeEdge>			ms_EdgeTest;
	static atArray<const CEntity*> ms_EntitiesToIgnore;
	static atArray<const CEntity*> ms_ExtraEntitiesToPushBeyond;
	static const CEntity* ms_EntityToConsiderForTouchingChecks;
	static const u32	ms_CollisionIncludeFlags;
	static const u32	ms_CollisionIncludeFlagsForClippingTests;
	static const float	ms_MinRadiusReductionBetweenInterativeShapetests;
	static bool			ms_ShouldIgnoreOcclusionWithBrokenFragments;
	static bool			ms_ShouldIgnoreOcclusionWithBrokenFragmentsOfIgnoredEntities;
	static bool			ms_ShouldIgnoreCollisionWithTouchingPedsAndVehicles;
	static bool			ms_ShouldLogExtraEntitiesToPushBeyond;

private:
	//Forbid copy construction and assignment.
	camCollision(const camCollision& other);
	camCollision& operator=(const camCollision& other);
};

#endif // CAMERA_COLLISION_H
