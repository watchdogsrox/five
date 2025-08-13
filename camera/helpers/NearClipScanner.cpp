//
// camera/helpers/NearClipScanner.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#include "camera/helpers/NearClipScanner.h"

#include "vector/colors.h"
#include "grcore/debugdraw.h"

#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/gameplay/aim/FirstPersonAimCamera.h"
#include "camera/gameplay/aim/ThirdPersonPedAimInCoverCamera.h"
#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"
#include "camera/cinematic/camera/tracking/CinematicFirstPersonIdleCamera.h"
#include "camera/helpers/Collision.h"
#include "camera/scripted/ScriptedCamera.h"
#include "camera/system/CameraManager.h"
#include "camera/system/CameraMetadata.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "physics/gtaInst.h"
#include "physics/gtaMaterialManager.h"
#include "physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "renderer/rendertargets.h"
#include "renderer/Water.h"
#include "scene/world/GameWorldHeightMap.h"
#include "task/General/Phone/TaskMobilePhone.h"

CAMERA_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(camNearClipScanner,0x9098BCFE)

extern const u32 g_SelfieAimCameraHash;

const float g_ExtraDistanceToPullInAfterCollisionDuringSelfieGesture = 0.30f;

static void ComputeFrustumCorners_xzy(Vec3V corners[8], float nearZ, float farZ, float fovy_degrees, float aspect)
{
	const ScalarV tanVFOV = ScalarV(rage::Tanf(0.5f*fovy_degrees*DtoR));
	const ScalarV tanHFOV = tanVFOV*ScalarV(aspect);
	const ScalarV z0      = ScalarV(nearZ);
	const ScalarV z1      = ScalarV(farZ);

	corners[0] = Vec3V(-tanHFOV, ScalarV(V_ONE), +tanVFOV)*z1; // 0: LEFT  TOP FAR
	corners[1] = Vec3V(+tanHFOV, ScalarV(V_ONE), +tanVFOV)*z1; // 1: RIGHT TOP FAR
	corners[2] = Vec3V(+tanHFOV, ScalarV(V_ONE), -tanVFOV)*z1; // 2: RIGHT BOT FAR
	corners[3] = Vec3V(-tanHFOV, ScalarV(V_ONE), -tanVFOV)*z1; // 3: LEFT  BOT FAR
	corners[4] = Vec3V(-tanHFOV, ScalarV(V_ONE), +tanVFOV)*z0; // 4: LEFT  TOP NEAR
	corners[5] = Vec3V(+tanHFOV, ScalarV(V_ONE), +tanVFOV)*z0; // 5: RIGHT TOP NEAR
	corners[6] = Vec3V(+tanHFOV, ScalarV(V_ONE), -tanVFOV)*z0; // 6: RIGHT BOT NEAR
	corners[7] = Vec3V(-tanHFOV, ScalarV(V_ONE), -tanVFOV)*z0; // 7: LEFT  BOT NEAR
}

camNearClipScanner::camNearClipScanner(const camBaseObjectMetadata& metadata)
: camBaseObject(metadata)
, m_Metadata(static_cast<const camNearClipScannerMetadata&>(metadata))
#if __BANK
, m_DebugNearClipAlpha(100)
, m_ShouldDebugUseGameplayFrame(false)
, m_ShouldDebugRenderNearClip(false)
, m_ShouldDebugRenderCollisionFrustum(false)
, m_DebugNearClip(0.0f)
, m_DebugNearClipPosition(V_ZERO)
#endif // __BANK
{
	u32 typesToCollideWith	= ArchetypeFlags::GTA_ALL_TYPES_WEAPON | ArchetypeFlags::GTA_RAGDOLL_TYPE | ArchetypeFlags::GTA_UNSMASHED_TYPE |
								ArchetypeFlags::GTA_WHEEL_TEST | ArchetypeFlags::GTA_PROJECTILE_TYPE;
	//Exclude glass for now, as it can be insanely expensive to test our object against shattered panes.
	typesToCollideWith		&= ~ArchetypeFlags::GTA_GLASS_TYPE;
	m_ScaledQuadTest.SetIncludeFlags(typesToCollideWith);

	//Ignore any collision polys that are explicitly flagged for camera clipping.
	const phMaterialMgrGta::Id materialFlags = PGTAMATERIALMGR->GetPackedPolyFlagValue(POLYFLAG_NO_CAM_COLLISION_ALLOW_CLIPPING);
	m_ScaledQuadTest.GetShape().SetIgnoreMaterialFlags(materialFlags);

	m_ScaledQuadTest.SetIncludeInstanceCallback(IncludeInstanceCallback(InstanceRejectCallback));
}

float camNearClipScanner::ComputeMaxNearClipAtPosition(const Vector3& position, bool shouldConsiderFov, float fov) const
{
	//If applicable, push out the maximum near-clip at low altitude based upon the field of view, as zooming in increases the likelihood of z-fighting.
	const float maxNearClipAtLowAltitude	= shouldConsiderFov ? RampValueSafe(fov, m_Metadata.m_MinFovToConsiderForMaxNearClip,
												m_Metadata.m_MaxFovToConsiderForMaxNearClip, m_Metadata.m_MaxNearClipAtMinFov, m_Metadata.m_MaxNearClip) :
												m_Metadata.m_MaxNearClip;

	const float maxWorldHeight	= CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(
									position.x - m_Metadata.m_HalfBoxExtentForHeightMapQuery,
									position.y - m_Metadata.m_HalfBoxExtentForHeightMapQuery,
									position.x + m_Metadata.m_HalfBoxExtentForHeightMapQuery,
									position.y + m_Metadata.m_HalfBoxExtentForHeightMapQuery);
	const float c_fStartHeight	= maxWorldHeight + m_Metadata.m_NearClipAltitudeStartHeight;
	const float c_fEndHeight	= maxWorldHeight + m_Metadata.m_NearClipAltitudeEndHeight;
	const float maxNearClip		= RampValueSafe(position.z, c_fStartHeight, c_fEndHeight, maxNearClipAtLowAltitude,
									m_Metadata.m_MaxNearClipAtHighAltitude);

	return maxNearClip;
}

//Push-out the near clip distance as far as we can whilst avoiding collision with the world.
void camNearClipScanner::Update(const camBaseCamera* activeCamera, camFrame& sourceFrame)
{
#if __BANK
	const camFrame& gameplayFrame = camInterface::GetGameplayDirector().GetFrame();
	camFrame clonedGameplayFrame;
	clonedGameplayFrame.CloneFrom(gameplayFrame);

	camFrame& frame = m_ShouldDebugUseGameplayFrame ? clonedGameplayFrame : sourceFrame;
#else
	camFrame& frame = sourceFrame;
#endif // __BANK
	
	const float maxSafeNearClip = ComputeMaxSafeNearClipUsingQuadTest(activeCamera, frame);

	frame.SetNearClip(maxSafeNearClip);

#if __BANK
	if(m_ShouldDebugUseGameplayFrame || !camInterface::GetDebugDirector().IsFreeCamActive())
	{
		m_DebugFrameCache = frame;
	}

	DebugRender();
#endif // __BANK
}

float camNearClipScanner::ComputeMaxSafeNearClipUsingQuadTest(const camBaseCamera* activeCamera, const camFrame& frame)
{
	const Matrix34& worldMatrix	= frame.GetWorldMatrix();
	const float fov				= frame.GetFov();
	const float minNearClip		= frame.GetNearClip();

	//NOTE: We're excluding the first-person mounted cinematic cameras for now, as we can't afford to back off too far from the vehicle bounds.
	const bool isFirstPersonCamera	= (activeCamera && (activeCamera->GetIsClassId(camFirstPersonAimCamera::GetStaticClassId()) ||
										activeCamera->GetIsClassId(camCinematicFirstPersonIdleCamera::GetStaticClassId())));
	const Vector3& framePosition	= frame.GetPosition();
	//NOTE: We only take the FOV into account when computing the max near-clip for first-person cameras. It would be desirable to do this for all camera types,
	//but there is presently too much scope for a global change to result in visible issues with the extended near-clip clipping through geometry that has
	//misaligned collision associated with it - such as with the update order issue that can occur with the posing of ped ragdoll bounds.
	float frustumFarClipToTest		= ComputeMaxNearClipAtPosition(framePosition, isFirstPersonCamera, fov);

	//NOTE: An update order issue exists between the posing of the follow ped's ragdoll bounds and the camera update. This can result in visible clipping into the
	//follow ped during cover-to-cover transitions. So we must extend the test and push further way from collision in this state.
	const bool isFollowPedInSpecialCoverState	= (activeCamera && activeCamera->GetIsClassId(camThirdPersonPedAimInCoverCamera::GetStaticClassId()) &&
													camInterface::GetGameplayDirector().GetFollowPedCoverSettings().m_IsMovingFromCoverToCover);

	const CPed* followPed						= camInterface::FindFollowPed();

	bool isPlayingSelfieGesture		= false;
	const bool isSelfieCameraMode	= (activeCamera && activeCamera->GetIsClassId(camThirdPersonPedAimCamera::GetStaticClassId()) &&
										(activeCamera->GetNameHash() == g_SelfieAimCameraHash));
	if(isSelfieCameraMode && followPed)
	{
		const CTaskMobilePhone* mobilePhoneTask		= static_cast<const CTaskMobilePhone*>(followPed->GetPedIntelligence()->FindTaskSecondaryByType(CTaskTypes::TASK_MOBILE_PHONE));
		isPlayingSelfieGesture						= mobilePhoneTask && const_cast<CTaskMobilePhone*>(mobilePhoneTask)->GetIsPlayingAdditionalSecondaryAnim();
	}

	float extraDistanceToPullInAfterCollision	= (isFirstPersonCamera ? m_Metadata.m_ExtraDistanceToPullInAfterCollisionInFirstPerson :
													(isFollowPedInSpecialCoverState ? m_Metadata.m_ExtraDistanceToPullInAfterCollisionInSpecialCoverStates :
													(isPlayingSelfieGesture ? g_ExtraDistanceToPullInAfterCollisionDuringSelfieGesture : m_Metadata.m_ExtraDistanceToPullInAfterCollision)));

	//We must push out the test by the distance we are prepared to pull in after collision.
	frustumFarClipToTest		+= extraDistanceToPullInAfterCollision;

	if (activeCamera && activeCamera->GetIsClassId(camScriptedCamera::GetStaticClassId()))
	{
		const camScriptedCamera* scriptedCamera	= static_cast<const camScriptedCamera*>(activeCamera);
		const float scriptRequestedMaxNearClip	= scriptedCamera->GetCustomMaxNearClip();
		frustumFarClipToTest					= Max(frustumFarClipToTest, scriptRequestedMaxNearClip);
	}

	float maxSafeNearClip		= frustumFarClipToTest;
#if __BANK
	const bool bIsDebugCamera = camInterface::GetDominantRenderedCamera() == activeCamera ? camInterface::GetDominantRenderedDirector()->GetIsClassId(camDebugDirector::GetStaticClassId()) : false;
	if(!bIsDebugCamera)
	{
		m_DebugNearClip = maxSafeNearClip;
		m_DebugNearClipReason = "";
		m_DebugNearClipPosition.ZeroComponents();
	}
#endif // __BANK

	const float frustumDepth	= frustumFarClipToTest - minNearClip;
	if(frustumDepth < SMALL_FLOAT)
	{
#if __BANK
		if(!bIsDebugCamera)
		{
			m_DebugNearClip = minNearClip;
			m_DebugNearClipReason = "Frustum depth is less than SMALL_FLOAT";
		}
#endif // __BANK
		return minNearClip;
	}

	//NOTE: We consider the native 16:9 aspect ratio on consoles and derive the ratio from the scene resolution on PC.
#if RSG_PC
	const s32 sceneWidth		= VideoResManager::GetSceneWidth();
	const s32 sceneHeight		= VideoResManager::GetSceneHeight();
	const float viewportAspect	= static_cast<float>(sceneWidth) / static_cast<float>(sceneHeight);
#else // RSG_PC
	const float viewportAspect	= 16.0f / 9.0f;
#endif // RSG_PC

	//Compute the verts of the special frustum to test.
	Vector3 verts[8];
	ComputeFrustumCorners_xzy((Vec3V*)verts, minNearClip, frustumFarClipToTest, fov, viewportAspect);

	const bool isClippingWater = ComputeIsClippingWater(worldMatrix, verts);

#if __BANK
	if(m_ShouldDebugUseGameplayFrame || !camInterface::GetDebugDirector().IsFreeCamActive())
	{
		//Backup the verts of the initial (full-size) frustum in world-space.
		for(u32 i=0; i<g_NumBoundVerts; i++)
		{
			worldMatrix.Transform(verts[i], m_DebugBoundVerts[i]);
		}
	}
#endif // __BANK

	rage::Vec3V* corners = reinterpret_cast<rage::Vec3V*>(verts);
	rage::Vec3V vecFarCenter  = (corners[0] + corners[1] + corners[2] + corners[3]) * ScalarV(rage::V_QUARTER);
	rage::Vec3V vecNearCenter = (corners[4] + corners[5] + corners[6] + corners[7]) * ScalarV(rage::V_QUARTER);
	rage::Vec3V vecUp     = (corners[4] + corners[5])*ScalarV(rage::V_HALF) - vecNearCenter;	// center to midpoint of top   near edge
	rage::Vec3V vecRight  = (corners[5] + corners[6])*ScalarV(rage::V_HALF) - vecNearCenter;	// center to midpoint of right near edge
	rage::Vec2V nearHalfExt(rage::Mag(vecRight), rage::Mag(vecUp));
	vecUp     = (corners[0] + corners[1])*ScalarV(rage::V_HALF) - vecFarCenter;					// center to midpoint of top   far edge
	vecRight  = (corners[1] + corners[2])*ScalarV(rage::V_HALF) - vecFarCenter;					// center to midpoint of right far edge
	rage::Vec2V farHalfExt(rage::Mag(vecRight), rage::Mag(vecUp));

	Vector3 vFarCenter  = worldMatrix.d + (worldMatrix.b * frustumFarClipToTest);
	Vector3 vNearCenter = worldMatrix.d + (worldMatrix.b * minNearClip);
	vecFarCenter  = RCC_VEC3V(vFarCenter);
	vecNearCenter = RCC_VEC3V(vNearCenter);

	phSegmentV worldSegment(vecNearCenter, vecFarCenter);
	rage::Mat33V oSetupMat(RCC_VEC3V(worldMatrix.b), RCC_VEC3V(worldMatrix.c), RCC_VEC3V(worldMatrix.a));
	phIntersection intersection;
	m_ScaledQuadTest.InitScalingSweptQuad(oSetupMat, worldSegment, nearHalfExt, farHalfExt, &intersection, 1);

	UpdateExcludeInstances();

	//NOTE: We don't treat polyhedral bounds as primitives for cinematic mounted cameras, as the convex hull of a vehicle bound can present
	//problems for the cinematic first-person bonnet camera.
	const bool isRenderingMountedCamera = (activeCamera && activeCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()));
	m_ScaledQuadTest.SetTreatPolyhedralBoundsAsPrimitives(!isRenderingMountedCamera);

	const bool hasHit = (m_ScaledQuadTest.TestInLevel() > 0);
	const bool hasInitialIntersection = m_ScaledQuadTest.GetShape().SweepStartIntersectionFound();
	if(hasInitialIntersection)
	{
		//The shapetest intersected collision at the start of the sweep, so constrain to the minimum near-clip distance.
		maxSafeNearClip = minNearClip;
	}
	else if(hasHit)
	{
		float intersectionTValue = intersection.GetT();
		maxSafeNearClip = Lerp(intersectionTValue, minNearClip, frustumFarClipToTest);
	}

#if __BANK
	if(hasHit || hasInitialIntersection)
	{
		if(!bIsDebugCamera)
		{
			m_DebugNearClipPosition = intersection.GetPosition();

			m_DebugNearClipReason = atVarString("hasInitialIntersection:%s hasHit:%s intersection:%.3f,%.3f,%.3f",
				hasInitialIntersection ? "true" : "false",
				hasHit ? "true" : "false",
				m_DebugNearClipPosition.GetXf(), m_DebugNearClipPosition.GetYf(), m_DebugNearClipPosition.GetZf());

			const phInst *hitInstance = intersection.GetInstance();
			const CEntity *hitEntity = hitInstance ? CPhysics::GetEntityFromInst(hitInstance) : NULL;
			if(hitEntity)
			{
				m_DebugNearClipReason += atVarString(" hitEntity:%p %s %s",
					hitEntity,
					GetEntityTypeStr(hitEntity->GetType()),
					hitEntity->GetModelName());
			}
		}
	}
#endif // __BANK

	//Pull in by a custom amount if the the shapetest intersected the follow ped, to reduce clipping.
	if(!isFirstPersonCamera && intersection.IsAHit())
	{
		const phInst* hitInstance = intersection.GetInstance();
		if(hitInstance)
		{
			const CEntity* hitEntity = CPhysics::GetEntityFromInst(hitInstance);
			if(hitEntity)
			{
				if(hitEntity == followPed)
				{
					extraDistanceToPullInAfterCollision = Max(extraDistanceToPullInAfterCollision, m_Metadata.m_ExtraDistanceToPullInAfterCollisionWithFollowPed);
				}
				else if(hitEntity == camInterface::GetGameplayDirector().GetFollowVehicle())
				{
					extraDistanceToPullInAfterCollision = Max(extraDistanceToPullInAfterCollision, m_Metadata.m_ExtraDistanceToPullInAfterCollisionWithFollowVehicle);
				}
			}
		}
	}

	//Respect the requested minimum near-clip of first-person cameras, as we cannot afford to compromise the near-clip to the same degree.
	const float minNearClipAfterExtraPullIn	= isFirstPersonCamera ? minNearClip : m_Metadata.m_MinNearClipAfterExtraPullIn;
	maxSafeNearClip							= Max(minNearClipAfterExtraPullIn, maxSafeNearClip - extraDistanceToPullInAfterCollision);

	if(isClippingWater)
	{
		//If we detected that the extended frustum is intersecting water, ensure we are at least pulling in to the requested minimum near-clip.
		maxSafeNearClip = Min(maxSafeNearClip, minNearClip);
#if __BANK
		if(!bIsDebugCamera)
		{
			m_DebugNearClipReason += " isClippingWater";
		}
#endif // __BANK
	}

#if __BANK
	if(!bIsDebugCamera)
	{
		m_DebugNearClip = maxSafeNearClip;
	}
#endif // __BANK

	return maxSafeNearClip;
}

bool camNearClipScanner::ComputeIsClippingWater(const Matrix34& worldMatrix, const Vector3* verts) const
{
	if(m_Metadata.m_DistanceScalingForWaterTest < SMALL_FLOAT)
	{
		return false;
	}

	//Test for simulated water ahead of the camera, as this cannot be detected using shapetests.
	//TODO: Consider pushing the near-clip as much as possible without clipping into the water, rather than forcing the minimum near-clip.

	bool isClipping = false;

	//Test the four lines that connect the near and far planes of the frustum.
	for(int i=0; i<4; i++)
	{
		Vector3 startPosition;
		worldMatrix.Transform(verts[i], startPosition);
		Vector3 endPosition;
		worldMatrix.Transform(verts[i + 4], endPosition);

		//Ensure we test in a downward direction as the water system seems to require this.
		if(startPosition.z < endPosition.z)
		{
			Vector3 tempPosition(startPosition);
			startPosition	= endPosition;
			endPosition		= tempPosition;
		}

		if(!IsClose(m_Metadata.m_DistanceScalingForWaterTest, 1.0f))
		{
			const Vector3 initialTestDelta	= endPosition - startPosition;
			const float initialTestDistance = initialTestDelta.Mag();
			if(initialTestDistance < SMALL_FLOAT)
			{
				isClipping = true;
				break;
			}

			Vector3 testDirection;
			testDirection.InvScale(initialTestDelta, initialTestDistance);

			//NOTE: We split the extra distance between both the start and end positions when testing for water.
			const float halfExtraDistance = 0.5f * initialTestDistance * (m_Metadata.m_DistanceScalingForWaterTest - 1.0f);

			Vector3 offsetToApply;
			offsetToApply.SetScaled(testDirection, halfExtraDistance);

			startPosition	-= offsetToApply;
			endPosition		+= offsetToApply;
		}

// 		grcDebugDraw::Line(startPosition, endPosition, Color_red);

		Vector3 dummyPosition;
		const bool hasHitWater = Water::TestLineAgainstWater(startPosition, endPosition, &dummyPosition);
		if(hasHitWater)
		{
//			grcDebugDraw::Sphere(dummyPosition, 0.1f, Color_purple);

			isClipping = true;
			break;
		}
	}

	return isClipping;
}

void camNearClipScanner::UpdateExcludeInstances()
{
	atArray<const phInst*> excludeInstanceList;

	const CPed* pedMadeInvisible = camManager::GetPedMadeInvisible();
	if(pedMadeInvisible)
	{
		//The camera system has made a ped invisible, so ignore their capsule and ragdoll bounds, along with any weapon bounds.

		const phInst* animatedInst = pedMadeInvisible->GetAnimatedInst();
		if(animatedInst)
		{
			excludeInstanceList.Grow() = animatedInst;
		}

		const phInst* ragdollInst = pedMadeInvisible->GetRagdollInst();
		if(ragdollInst)
		{
			excludeInstanceList.Grow() = ragdollInst;
		}

		const CPedWeaponManager* weaponManager = pedMadeInvisible->GetWeaponManager();
		if(weaponManager)
		{
			const CObject* weaponObject = weaponManager->GetEquippedWeaponObject();
			if(weaponObject)
			{
				const phInst* weaponInst = weaponObject->GetCurrentPhysicsInst();
				if(weaponInst)
				{
					excludeInstanceList.Grow() = weaponInst;
				}
			}

			const CWeapon* weapon = weaponManager->GetEquippedWeapon();
			if(weapon)
			{
				const CWeapon::Components& weaponComponents = weapon->GetComponents();
				for(s32 i=0; i<weaponComponents.GetCount(); i++)
				{
					const CDynamicEntity* dynamicEntity = weaponComponents[i]->GetDrawable();
					if(dynamicEntity)
					{
						const phInst* weaponComponentInst = dynamicEntity->GetCurrentPhysicsInst();
						if(weaponComponentInst)
						{
							excludeInstanceList.Grow() = weaponComponentInst;
						}
					}
				}
			}
		}
	}

	const s32 numExcludeInstances = excludeInstanceList.GetCount();
	if(numExcludeInstances > 0)
	{
		m_ScaledQuadTest.SetExcludeInstanceList(excludeInstanceList.GetElements(), numExcludeInstances);
	}
	else
	{
		m_ScaledQuadTest.ClearExcludeInstanceList();
	}
}

//The specified instance is rejected if false is returned.
bool camNearClipScanner::InstanceRejectCallback(const phInst* instance)
{
	if(!cameraVerifyf(instance, "The instance reject callback of the camera near-clip scanner was called with a NULL instance"))
	{
		return false;
	}

	const CEntity* entity	= CPhysics::GetEntityFromInst(instance);
	const bool shouldReject	= (entity && !entity->GetIsVisible());

	return !shouldReject;
}

#if __BANK
void camNearClipScanner::AddWidgetsForInstance()
{
	if(m_WidgetGroup == NULL)
	{
		bkBank* bank = BANKMGR.FindBank("Camera");
		if(bank)
		{
			m_WidgetGroup = bank->PushGroup("Near-clip scanner", false);
			{
				bank->AddToggle("Use gameplay frame",		&m_ShouldDebugUseGameplayFrame);
				bank->AddToggle("Render near clip",			&m_ShouldDebugRenderNearClip);
				bank->AddSlider("Near clip alpha",			&m_DebugNearClipAlpha,					0, 255, 1);
				bank->AddToggle("Render collision frustum",	&m_ShouldDebugRenderCollisionFrustum);
			}
			bank->PopGroup(); //Near-clip scanner.
		}
	}
}

void camNearClipScanner::DebugRender()
{
	if(m_ShouldDebugRenderNearClip)
	{
		m_DebugFrameCache.DebugRenderNearClip(Color32((u32)255, (u32)0, (u32)255, m_DebugNearClipAlpha));
	}

	if(m_ShouldDebugRenderCollisionFrustum)
	{
		const u32 halfNumVerts = g_NumBoundVerts / 2;
		for(u32 i=0; i<halfNumVerts; i++)
		{
			const u32 nextIndex = (i + 1) % halfNumVerts;
			grcDebugDraw::Line(m_DebugBoundVerts[i], m_DebugBoundVerts[nextIndex], Color_blue);
			grcDebugDraw::Line(m_DebugBoundVerts[halfNumVerts + i], m_DebugBoundVerts[halfNumVerts + nextIndex], Color_blue);
			grcDebugDraw::Line(m_DebugBoundVerts[i], m_DebugBoundVerts[halfNumVerts + i], Color_blue);
		}
	}
}
#endif // __BANK
