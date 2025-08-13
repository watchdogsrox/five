//
// camera/helpers/ThirdPersonFrameInterpolator.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/ThirdPersonFrameInterpolator.h"

#include "fwmaths/angle.h"
#include "grcore/debugdraw.h"

#include "camera/gameplay/ThirdPersonCamera.h"
#include "camera/helpers/Collision.h"
#include "camera/system/CameraMetadata.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camThirdPersonFrameInterpolator,0x6E46CC34)

#if __BANK
bool camThirdPersonFrameInterpolator::ms_ShouldDebugRender = false;
#endif // __BANK


camThirdPersonFrameInterpolator::camThirdPersonFrameInterpolator(camThirdPersonCamera& sourceCamera, const camThirdPersonCamera& destinationCamera,
	u32 duration, bool shouldDeleteSourceCamera, u32 timeInMilliseconds)
: camFrameInterpolator(sourceCamera, destinationCamera, duration, shouldDeleteSourceCamera, timeInMilliseconds)
, m_PreviousOrientationDeltaForInterpolatedBaseFront(Quaternion::IdentityType)
, m_PreviousOrientationDeltaForInterpolatedOrbitFront(Quaternion::IdentityType)
, m_CachedSourceBasePivotPosition(VEC3_ZERO)
, m_CachedSourceBaseFront(VEC3_ZERO)
, m_CachedSourcePivotPosition(VEC3_ZERO)
, m_CachedSourcePreCollisionCameraPosition(VEC3_ZERO)
, m_CachedSourceCollisionRootPosition(VEC3_ZERO)
, m_CachedSourceLookAtPosition(VEC3_ZERO)
, m_InterpolatedBasePivotPosition(VEC3_ZERO)
, m_InterpolatedBaseFront(VEC3_ZERO)
, m_InterpolatedOrbitFront(VEC3_ZERO)
, m_InterpolatedPivotPosition(VEC3_ZERO)
, m_InterpolatedPreCollisionCameraPosition(VEC3_ZERO)
, m_InterpolatedCollisionRootPosition(VEC3_ZERO)
, m_CachedSourcePreToPostCollisionLookAtOrientationBlendValue(0.0f)
, m_InterpolatedPreToPostCollisionLookAtOrientationBlendValue(0.0f)
, m_IsCachedSourceBuoyant(true)
, m_AreSourceParametersCachedRelativeToDestination(false)
, m_ShouldApplyCollisionThisUpdate(false)
, m_HasClonedSourceCollisionDamping(false)
{
	CacheSourceCameraRelativeToDestinationInternal(sourceCamera, destinationCamera);
}

const camFrame& camThirdPersonFrameInterpolator::Update(const camBaseCamera& destinationCamera, u32 timeInMilliseconds)
{
	const camFrame& resultFrame = camFrameInterpolator::Update(destinationCamera, timeInMilliseconds);

	UpdateFrameShake(destinationCamera);

#if __BANK
	if(ms_ShouldDebugRender && m_ShouldApplyCollisionThisUpdate)
	{
		resultFrame.DebugRender(true);
	}
#endif // __BANK

	m_ShouldApplyCollisionThisUpdate	= false;
	m_ShouldApplyFrameShakeThisUpdate	= false;

	return resultFrame;
}

void camThirdPersonFrameInterpolator::CacheSourceCameraRelativeToDestination(const camBaseCamera& sourceCamera, const camFrame& destinationFrame,
	const camBaseCamera* destinationCamera)
{
	camFrameInterpolator::CacheSourceCameraRelativeToDestination(sourceCamera, destinationFrame, destinationCamera);

	if(sourceCamera.GetIsClassId(camThirdPersonCamera::GetStaticClassId()) && destinationCamera &&
		destinationCamera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
	{
		CacheSourceCameraRelativeToDestinationInternal(static_cast<const camThirdPersonCamera&>(sourceCamera),
			static_cast<const camThirdPersonCamera&>(*destinationCamera));
	}
}

void camThirdPersonFrameInterpolator::CacheSourceCameraRelativeToDestinationInternal(const camThirdPersonCamera& sourceCamera,
	const camThirdPersonCamera& destinationCamera)
{
	const u32 numSourceCameraUpdatesPerformed		= sourceCamera.GetNumUpdatesPerformed();
	const u32 numDestinationCameraUpdatesPerformed	= destinationCamera.GetNumUpdatesPerformed();
	if((numSourceCameraUpdatesPerformed == 0) || (numDestinationCameraUpdatesPerformed == 0))
	{
		//We have not yet updated the source and destination cameras successfully, so we cannot cache the source parameters.
		return;
	}

	//Cache the relevant parameters of the source third-person camera w.r.t. the (base) orbit matrix of the destination camera.

	const camFrameInterpolator* sourceFrameInterpolator = sourceCamera.GetFrameInterpolator();

	const camThirdPersonFrameInterpolator* sourceThirdPersonFrameInterpolator = NULL;
	if((sourceFrameInterpolator && sourceFrameInterpolator->GetIsClassId(camThirdPersonFrameInterpolator::GetStaticClassId())))
	{
		sourceThirdPersonFrameInterpolator = static_cast<const camThirdPersonFrameInterpolator*>(sourceFrameInterpolator);
	}

	//NOTE: We must query the interpolated attributes if the source camera is interpolating.
	const Vector3& sourceBasePivotPosition		= sourceThirdPersonFrameInterpolator ?
													sourceThirdPersonFrameInterpolator->GetInterpolatedBasePivotPosition() :
													sourceCamera.GetBasePivotPosition();
	const Vector3& sourceBaseFront				= sourceThirdPersonFrameInterpolator ?
													sourceThirdPersonFrameInterpolator->GetInterpolatedBaseFront() :
													sourceCamera.GetBaseFront();
	const Vector3& sourcePivotPosition			= sourceThirdPersonFrameInterpolator ?
													sourceThirdPersonFrameInterpolator->GetInterpolatedPivotPosition() :
													sourceCamera.GetPivotPosition();
	const Vector3& sourceCollisionRootPosition	= sourceThirdPersonFrameInterpolator ?
													sourceThirdPersonFrameInterpolator->GetInterpolatedCollisionRootPosition() :
													sourceCamera.GetCollisionRootPosition();

	const Vector3& sourcePreCollisionCameraPosition	= sourceCamera.GetFrame().GetPosition();

	const Vector3& destinationBaseFront = destinationCamera.GetBaseFront();
	Matrix34 destinationOrbitMatrix;
	camFrame::ComputeWorldMatrixFromFront(destinationBaseFront, destinationOrbitMatrix);
	destinationOrbitMatrix.d = destinationCamera.GetBasePivotPosition();

	destinationOrbitMatrix.UnTransform(sourceBasePivotPosition, m_CachedSourceBasePivotPosition);
	destinationOrbitMatrix.UnTransform3x3(sourceBaseFront, m_CachedSourceBaseFront);
	destinationOrbitMatrix.UnTransform(sourcePivotPosition, m_CachedSourcePivotPosition);
	destinationOrbitMatrix.UnTransform(sourcePreCollisionCameraPosition, m_CachedSourcePreCollisionCameraPosition);
	destinationOrbitMatrix.UnTransform(sourceCollisionRootPosition, m_CachedSourceCollisionRootPosition);

	m_CachedSourcePreToPostCollisionLookAtOrientationBlendValue	= sourceThirdPersonFrameInterpolator ?
																	sourceThirdPersonFrameInterpolator->
																	GetInterpolatedPreToPostCollisionLookAtOrientationBlendValue() :
																	sourceCamera.ComputePreToPostCollisionLookAtOrientationBlendValue();

	m_IsCachedSourceBuoyant = sourceCamera.IsBuoyant();

	m_AreSourceParametersCachedRelativeToDestination = true;
}

bool camThirdPersonFrameInterpolator::UpdateBlendLevel(const camFrame& sourceFrame, const camFrame& destinationFrame, u32 timeInMilliseconds,
	const camBaseCamera* destinationCamera)
{
	if(!m_AreSourceParametersCachedRelativeToDestination)
	{
		//We failed to cache the parameters of the source third-person camera, so skip to the end of the interpolation.
		//NOTE: This allows a collision update to be performed as necessary.
		m_BlendLevel = 1.0f;
		return true;
	}

	const bool isDestinationCameraBuoyant	= destinationCamera && destinationCamera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()) &&
												static_cast<const camThirdPersonCamera*>(destinationCamera)->IsBuoyant();
	if(m_IsCachedSourceBuoyant != isDestinationCameraBuoyant)
	{
		//The source and destination third-person cameras have conflicting buoyancy states, so we must skip to the end of the interpolation in order to
		//avoid interpolating through the surface of water.
		//NOTE: This allows a collision update to be performed as necessary.
		m_BlendLevel = 1.0f;
		return true;
	}

	const bool hasCut = camFrameInterpolator::UpdateBlendLevel(sourceFrame, destinationFrame, timeInMilliseconds, destinationCamera);

	return hasCut;
}

void camThirdPersonFrameInterpolator::InterpolatePosition(float blendLevel, const camFrame& sourceFrame, const camFrame& destinationFrame,
	const camBaseCamera* destinationCamera, Vector3& resultPosition)
{
	camFrameInterpolator::InterpolatePosition(blendLevel, sourceFrame, destinationFrame, destinationCamera, resultPosition);

	if(destinationCamera && destinationCamera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
	{
		const camThirdPersonCamera& destinationThirdPersonCamera = static_cast<const camThirdPersonCamera&>(*destinationCamera);

		const Vector3& destinationBasePivotPosition				= destinationThirdPersonCamera.GetBasePivotPosition();
		const Vector3& destinationBaseFront						= destinationThirdPersonCamera.GetBaseFront();
		const Vector3& destinationPivotPosition					= destinationThirdPersonCamera.GetPivotPosition();
		const Vector3& destinationPreCollisionCameraPosition	= destinationFrame.GetPosition();

		Matrix34 destinationOrbitMatrix;
		camFrame::ComputeWorldMatrixFromFront(destinationBaseFront, destinationOrbitMatrix);
		destinationOrbitMatrix.d = destinationBasePivotPosition;

		Vector3 sourceBasePivotPosition;
		destinationOrbitMatrix.Transform(m_CachedSourceBasePivotPosition, sourceBasePivotPosition);
		Vector3 sourceBaseFront;
		destinationOrbitMatrix.Transform3x3(m_CachedSourceBaseFront, sourceBaseFront);
		Vector3 sourcePivotPosition;
		destinationOrbitMatrix.Transform(m_CachedSourcePivotPosition, sourcePivotPosition);
		Vector3 sourceCollisionRootPosition;
		destinationOrbitMatrix.Transform(m_CachedSourceCollisionRootPosition, sourceCollisionRootPosition);
		Vector3 sourcePreCollisionCameraPosition;
		destinationOrbitMatrix.Transform(m_CachedSourcePreCollisionCameraPosition, sourcePreCollisionCameraPosition);

		m_InterpolatedBasePivotPosition.Lerp(blendLevel, sourceBasePivotPosition, destinationBasePivotPosition);
		camFrame::SlerpOrientation(blendLevel, sourceBaseFront, destinationBaseFront, m_InterpolatedBaseFront, (m_BlendLevel >= SMALL_FLOAT),
			&m_PreviousOrientationDeltaForInterpolatedBaseFront);

		//Interpolate the orbit-relative pivot positions, so that we can handle orbit orientation changes correctly.

		Matrix34 sourceOrbitMatrix;
		camFrame::ComputeWorldMatrixFromFront(sourceBaseFront, sourceOrbitMatrix);
		sourceOrbitMatrix.d = sourceBasePivotPosition;

		Vector3 sourceOrbitRelativePivotPosition;
		sourceOrbitMatrix.UnTransform(sourcePivotPosition, sourceOrbitRelativePivotPosition);

		Vector3 destinationOrbitRelativePivotPosition;
		destinationOrbitMatrix.UnTransform(destinationPivotPosition, destinationOrbitRelativePivotPosition);

		Vector3 interpolatedOrbitRelativePivotPosition;
		interpolatedOrbitRelativePivotPosition.Lerp(blendLevel, sourceOrbitRelativePivotPosition, destinationOrbitRelativePivotPosition);

		Matrix34 interpolatedOrbitMatrix;
		camFrame::ComputeWorldMatrixFromFront(m_InterpolatedBaseFront, interpolatedOrbitMatrix);
		interpolatedOrbitMatrix.d = m_InterpolatedBasePivotPosition;

		interpolatedOrbitMatrix.Transform(interpolatedOrbitRelativePivotPosition, m_InterpolatedPivotPosition);

		//Interpolate the orbit front and distance to compute an (orbitally) interpolated pre-collision camera position.

		const Vector3 sourceOrbitDelta	= sourcePivotPosition - sourcePreCollisionCameraPosition;
		const float sourceOrbitDistance	= sourceOrbitDelta.Mag();
		Vector3 sourceOrbitFront(sourceOrbitDelta);
		sourceOrbitFront.NormalizeSafe(YAXIS);

		const Vector3 destinationOrbitDelta		= destinationPivotPosition - destinationPreCollisionCameraPosition;
		const float destinationOrbitDistance	= destinationOrbitDelta.Mag();
		Vector3 destinationOrbitFront(destinationOrbitDelta);
		destinationOrbitFront.NormalizeSafe(YAXIS);

		const float interpolatedOrbitDistance = Lerp(blendLevel, sourceOrbitDistance, destinationOrbitDistance);

		camFrame::SlerpOrientation(blendLevel, sourceOrbitFront, destinationOrbitFront, m_InterpolatedOrbitFront, (m_BlendLevel >= SMALL_FLOAT),
			&m_PreviousOrientationDeltaForInterpolatedOrbitFront);

		resultPosition = m_InterpolatedPreCollisionCameraPosition = m_InterpolatedPivotPosition - (interpolatedOrbitDistance * m_InterpolatedOrbitFront);

		UpdateCollision(destinationThirdPersonCamera, sourceCollisionRootPosition, blendLevel, resultPosition);

		//TODO: Apply any translation that would result from interpolation of the source and destination shakes, or blend and apply these shakes
		//after interpolation (rather than in the post-effect update of the individual cameras.)

#if __BANK
		if(ms_ShouldDebugRender && m_ShouldApplyCollisionThisUpdate)
		{
			static float radius = 0.125f;
			grcDebugDraw::Sphere(RCC_VEC3V(m_InterpolatedBasePivotPosition), radius, Color_blue);
			grcDebugDraw::Line(RCC_VEC3V(m_InterpolatedBasePivotPosition), VECTOR3_TO_VEC3V(m_InterpolatedBasePivotPosition - m_InterpolatedBaseFront), Color_blue);
			grcDebugDraw::Line(RCC_VEC3V(m_InterpolatedBasePivotPosition), VECTOR3_TO_VEC3V(m_InterpolatedBasePivotPosition - sourceBaseFront), Color_orange);
			grcDebugDraw::Line(RCC_VEC3V(m_InterpolatedBasePivotPosition), VECTOR3_TO_VEC3V(m_InterpolatedBasePivotPosition - destinationBaseFront), Color_purple);
			grcDebugDraw::Sphere(RCC_VEC3V(m_InterpolatedPivotPosition), radius, Color_yellow);
			grcDebugDraw::Line(RCC_VEC3V(m_InterpolatedPivotPosition), VECTOR3_TO_VEC3V(m_InterpolatedPivotPosition - m_InterpolatedOrbitFront), Color_yellow);
			grcDebugDraw::Sphere(RCC_VEC3V(m_InterpolatedCollisionRootPosition), radius, Color_red);
		}
#endif // __BANK
	}
}

void camThirdPersonFrameInterpolator::UpdateCollision(const camThirdPersonCamera& destinationThirdPersonCamera, const Vector3& sourceCollisionRootPosition,
	float blendLevel, Vector3& cameraPosition)
{
	//NOTE: We must compute the interpolated collision root position whether or not we are applying collision this update, as another third-person
	//interpolation may depend on this being up-to-date.

	const Vector3& destinationCollisionRootPosition = destinationThirdPersonCamera.GetCollisionRootPosition();
	m_InterpolatedCollisionRootPosition.Lerp(blendLevel, sourceCollisionRootPosition, destinationCollisionRootPosition);

	if(!m_ShouldApplyCollisionThisUpdate)
	{
		return;
	}

	//TODO: Interpolate the collision helpers and use a local helper (removing the need for the temporary const_cast.)
	camCollision* destinationCollision = const_cast<camCollision*>(destinationThirdPersonCamera.GetCollision());
	if(!destinationCollision)
	{
		return;
	}

	//We must clone the current damping state of the source collision helper in order to avoid a discontinuity in the collision behaviour.
	if(!m_HasClonedSourceCollisionDamping && m_SourceCamera && m_SourceCamera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
	{
		const camThirdPersonCamera* sourceThirdPersonCamera	= static_cast<const camThirdPersonCamera*>(m_SourceCamera.Get());
		const camCollision* sourceCollision					= sourceThirdPersonCamera->GetCollision();
		if(sourceCollision)
		{
			destinationCollision->CloneDamping(*sourceCollision);
		}
	}

	m_HasClonedSourceCollisionDamping = true;

	//Use the initial collision radius that has already been configured for the destination collision helper camera for now.
	float collisionTestRadius = destinationCollision->GetInitialCollisionTestRadius();

	const float destinationToInterpolatedCollisionRootDist2 = destinationCollisionRootPosition.Dist2(m_InterpolatedCollisionRootPosition);
	if(destinationToInterpolatedCollisionRootDist2 >= VERY_SMALL_FLOAT)
	{
		//If we are applying collision, ensure that the interpolated collision root position is actually safe.
		//NOTE: We test from the destination root, as the we can rely on this position as being safe.

		if(cameraVerifyf(collisionTestRadius >= SMALL_FLOAT,
			"Unable to compute a valid interpolated collision root position due to an invalid test radius."))
		{
			WorldProbe::CShapeTestCapsuleDesc capsuleTest;
			WorldProbe::CShapeTestHitPoint capsuleTestIsect;
			WorldProbe::CShapeTestResults shapeTestResults(capsuleTestIsect);
			capsuleTest.SetResultsStructure(&shapeTestResults);
			capsuleTest.SetIsDirected(true);
			capsuleTest.SetTypeFlags(ArchetypeFlags::GTA_CAMERA_TEST);
			capsuleTest.SetContext(WorldProbe::LOS_Camera);

			//NOTE: We must include the clipping types here.
			const u32 collisionIncludeFlags = camCollision::GetCollisionIncludeFlagsForClippingTests();
			capsuleTest.SetIncludeFlags(collisionIncludeFlags);

			capsuleTest.SetCapsule(destinationCollisionRootPosition, m_InterpolatedCollisionRootPosition, collisionTestRadius);

			//Reduce the test radius in readiness for use in a consecutive test.
			collisionTestRadius -= camCollision::GetMinRadiusReductionBetweenInterativeShapetests();

			const s32 numEntitiesToIgnore = destinationCollision->GetNumEntitiesToIgnoreThisUpdate();
			if(numEntitiesToIgnore > 0)
			{
				const CEntity** entitiesToIgnore = const_cast<const CEntity**>(destinationCollision->GetEntitiesToIgnoreThisUpdate());
				capsuleTest.SetExcludeEntities(entitiesToIgnore, numEntitiesToIgnore, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
			}

			const bool hasHit		= WorldProbe::GetShapeTestManager()->SubmitTest(capsuleTest);
			const float hitTValue	= hasHit ? shapeTestResults[0].GetHitTValue() : 1.0f;
			m_InterpolatedCollisionRootPosition.Lerp(1.0f - hitTValue, destinationCollisionRootPosition);
		}
	}

	//Use the buoyancy state of the destination camera for now.
	const bool isBuoyant = destinationThirdPersonCamera.IsBuoyant();

	float dummyHeadingDelta;
	float dummyPitchDelta;
	destinationCollision->ForcePopInThisUpdate();
	destinationCollision->Update(m_InterpolatedCollisionRootPosition, collisionTestRadius, isBuoyant, cameraPosition,
		dummyHeadingDelta, dummyPitchDelta);
}

void camThirdPersonFrameInterpolator::InterpolateOrientation(float blendLevel, const camFrame& sourceFrame, const camFrame& destinationFrame,
	const camBaseCamera* destinationCamera, Matrix34& resultWorldMatrix)
{
	camFrameInterpolator::InterpolateOrientation(blendLevel, sourceFrame, destinationFrame, destinationCamera, resultWorldMatrix);

	if(destinationCamera && destinationCamera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
	{
		//Interpolate the look-at positions

		const camThirdPersonCamera& destinationThirdPersonCamera = static_cast<const camThirdPersonCamera&>(*destinationCamera);

		const Vector3& postCollisionCameraPosition	= m_ResultFrame.GetPosition();
		const Vector3& lookAtFrontOnPreviousUpdate	= m_ResultFrame.GetFront();

		const float destinationPreToPostCollisionBlendValue			= destinationThirdPersonCamera.ComputePreToPostCollisionLookAtOrientationBlendValue();
		m_InterpolatedPreToPostCollisionLookAtOrientationBlendValue	= Lerp(blendLevel, m_CachedSourcePreToPostCollisionLookAtOrientationBlendValue,
																		destinationPreToPostCollisionBlendValue);

		const camCollision* destinationCollision	= destinationThirdPersonCamera.GetCollision();
		const bool wasCameraConstrainedOffAxis		= (destinationCollision && destinationCollision->WasCameraConstrainedOffAxisOnPreviousUpdate());

		//Determine the destination look-at position, taking into account the post-collision camera position.

		Vector3 destinationLookAtFront;
		destinationThirdPersonCamera.ComputeLookAtFront(m_InterpolatedCollisionRootPosition, m_InterpolatedBasePivotPosition,
			m_InterpolatedBaseFront, m_InterpolatedOrbitFront, m_InterpolatedPivotPosition, m_InterpolatedPreCollisionCameraPosition, postCollisionCameraPosition,
			m_InterpolatedPreToPostCollisionLookAtOrientationBlendValue, wasCameraConstrainedOffAxis, destinationLookAtFront);

		Vector3 destinationLookAtPosition;
		camThirdPersonCamera::ComputeLookAtPositionFromFront(destinationLookAtFront, m_InterpolatedBasePivotPosition, m_InterpolatedOrbitFront,
			postCollisionCameraPosition, destinationLookAtPosition);

		//Determine the source look-at position, taking into account the post-collision camera position.
		//NOTE: The source look-at position is cached relative to the look-at matrix of the destination camera, rather than the base orbit matrix.

		Matrix34 destinationLookAtMatrix;
		camFrame::ComputeWorldMatrixFromFront(destinationLookAtFront, destinationLookAtMatrix);
		destinationLookAtMatrix.d = destinationLookAtPosition;

		Vector3 sourceLookAtPosition;

		if(m_SourceCamera && m_SourceCamera->GetIsClassId(camThirdPersonCamera::GetStaticClassId()))
		{
			const camThirdPersonCamera& sourceThirdPersonCamera = static_cast<const camThirdPersonCamera&>(*m_SourceCamera);

			Vector3 sourceLookAtFront;
			sourceThirdPersonCamera.ComputeLookAtFront(m_InterpolatedCollisionRootPosition, m_InterpolatedBasePivotPosition,
				m_InterpolatedBaseFront, m_InterpolatedOrbitFront, m_InterpolatedPivotPosition, m_InterpolatedPreCollisionCameraPosition, postCollisionCameraPosition,
				m_InterpolatedPreToPostCollisionLookAtOrientationBlendValue, wasCameraConstrainedOffAxis, sourceLookAtFront);

			camThirdPersonCamera::ComputeLookAtPositionFromFront(sourceLookAtFront, m_InterpolatedBasePivotPosition, m_InterpolatedOrbitFront,
				postCollisionCameraPosition, sourceLookAtPosition);

			//If the source camera is also interpolating, we must take into account its cached source look-at position.
			//TODO: Reconsider this, as it's pretty hacky.
			const camFrameInterpolator* sourceFrameInterpolator = sourceThirdPersonCamera.GetFrameInterpolator();
			if(sourceFrameInterpolator && sourceFrameInterpolator->GetIsClassId(camThirdPersonFrameInterpolator::GetStaticClassId()))
			{
				const Vector3& cachedSounceLookAtPosition	= static_cast<const camThirdPersonFrameInterpolator*>(sourceFrameInterpolator)->
																GetCachedSourceLookAtPosition();

				Matrix34 sourceLookAtMatrix;
				camFrame::ComputeWorldMatrixFromFront(sourceLookAtFront, sourceLookAtMatrix);
				sourceLookAtMatrix.d = sourceLookAtPosition;

				Vector3 sourceFrameInterpolatorSourceLookAtPosition;
				sourceLookAtMatrix.Transform(cachedSounceLookAtPosition, sourceFrameInterpolatorSourceLookAtPosition);

#if __BANK
				if(ms_ShouldDebugRender)
				{
					static float radius = 0.125f;
					grcDebugDraw::Cross(RCC_VEC3V(sourceFrameInterpolatorSourceLookAtPosition), radius * 1.25f, Color_white);
				}
#endif // __BANK

				const float sourceFrameInterpolatorBlendLevel = sourceFrameInterpolator->GetBlendLevel();

				sourceLookAtPosition.Lerp(1.0f - sourceFrameInterpolatorBlendLevel, sourceFrameInterpolatorSourceLookAtPosition);
			}

			destinationLookAtMatrix.UnTransform(sourceLookAtPosition, m_CachedSourceLookAtPosition);
		}
		else
		{
			destinationLookAtMatrix.Transform(m_CachedSourceLookAtPosition, sourceLookAtPosition);
		}

		//TODO: Consider SLERPing the look-at fronts, rather than LERPing the look-at positions. But note that we will still need to recompute an
		//effective look-at position from the resulting front, as this is required for any later interpolation helpers.
		Vector3 interpolatedLookAtPosition;
 		interpolatedLookAtPosition.Lerp(blendLevel, sourceLookAtPosition, destinationLookAtPosition);

		Vector3 interpolatedLookAtFront = interpolatedLookAtPosition - postCollisionCameraPosition;
		interpolatedLookAtFront.NormalizeSafe(lookAtFrontOnPreviousUpdate);

		float heading;
		float pitch;
		camFrame::ComputeHeadingAndPitchFromFront(interpolatedLookAtFront, heading, pitch);

		//Finally, manually interpolate the roll.
		const Matrix34& sourceWorldMatrix		= sourceFrame.GetWorldMatrix();
		const float sourceRoll					= camFrame::ComputeRollFromMatrix(sourceWorldMatrix);
		const Matrix34& destinationWorldMatrix	= destinationFrame.GetWorldMatrix();
		const float destinationRoll				= camFrame::ComputeRollFromMatrix(destinationWorldMatrix);
		const float interpolatedRoll			= fwAngle::LerpTowards(sourceRoll, destinationRoll, blendLevel);

		camFrame::ComputeWorldMatrixFromHeadingPitchAndRoll(heading, pitch, interpolatedRoll, resultWorldMatrix);

		//TODO: Apply any heading and/or pitch rotation that would result from interpolation of the source and destination shakes, or blend and apply
		//these shakes after interpolation (rather than in the post-effect update of the individual cameras.)

#if __BANK
		if(ms_ShouldDebugRender && m_ShouldApplyCollisionThisUpdate)
		{
			static float radius = 0.125f;
			grcDebugDraw::Sphere(RCC_VEC3V(interpolatedLookAtPosition), radius, Color_green);

			grcDebugDraw::Cross(RCC_VEC3V(sourceLookAtPosition), radius * 1.25f, Color_orange);
			grcDebugDraw::Cross(RCC_VEC3V(destinationLookAtPosition), radius * 1.25f, Color_purple);
		}
#endif // __BANK
	}
}

void camThirdPersonFrameInterpolator::UpdateFrameShake(const camBaseCamera& destinationCamera)
{
	if(!m_ShouldApplyFrameShakeThisUpdate)
	{
		return;
	}

	const_cast<camBaseCamera&>(destinationCamera).UpdateFrameShakers(m_ResultFrame, m_BlendLevel);

	if(m_SourceCamera)
	{
		m_SourceCamera->UpdateFrameShakers(m_ResultFrame, (1.0f - m_BlendLevel));
	}
}

#if __BANK
void camThirdPersonFrameInterpolator::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Third-person frame interpolator");
	{
		bank.AddToggle("Should debug render", &ms_ShouldDebugRender);
	}
	bank.PopGroup(); //Third-person frame interpolator
}
#endif // __BANK
