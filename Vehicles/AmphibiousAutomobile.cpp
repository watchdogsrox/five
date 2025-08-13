#include "vehicles/AmphibiousAutomobile.h"

#include "vehicles/Submarine.h"
#include "Peds/Ped.h"
#include "Physics/Floater.h"
#include "Physics/physics.h"
#include "Physics/PhysicsHelpers.h"
#include "Renderer/Water.h"
#include "renderer/River.h"
#include "system/control.h"
#include "System/controlMgr.h"
#include "Vfx/Systems/VfxWater.h"
#include "debug/DebugScene.h"
#include "camera/CamInterface.h"
#include "Stats/StatsMgr.h"
#include "Vfx/Decals/DecalManager.h"
#include "scene/world/GameWorldHeightMap.h"
#include "Vfx/Misc/Fire.h"
#include "vfx/Systems/VfxVehicle.h"
#include "debug/debugglobals.h"
#include "network/Events/NetworkEventTypes.h"
#include "audio/boataudioentity.h"
#include "audio/caraudioentity.h"
#include "audio/radioaudioentity.h"
#include "audio/radioslot.h"
#include "frontend/NewHud.h"
#include "vehicleAi/task/TaskVehicleAnimation.h"
#include "vehicles/Boat.h"

ENTITY_OPTIMISATIONS()
VEHICLE_OPTIMISATIONS()

extern float g_Boat_InteriorLight_ShutDownDistance;

dev_float CAmphibiousAutomobile::sm_fJetskiRiderResistanceMultMax = 1.0f;
dev_float CAmphibiousAutomobile::sm_fJetskiRiderResistanceMultMin = 0.5f;

dev_float CAmphibiousAutomobile::sm_fPropellerTimeOutOfWaterTolerance = 50.0f;

//////////////////////////////////////////////////////////////////////////
// Class CAmphibiousAutomobile
//
CAmphibiousAutomobile::CAmphibiousAutomobile(const eEntityOwnedBy ownedBy, u32 popType)	: CAutomobile(ownedBy, popType, VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE)
{
	InitAmphibiousAutomobile();
}

CAmphibiousAutomobile::CAmphibiousAutomobile(const eEntityOwnedBy ownedBy, u32 popType, VehicleType vehType) : CAutomobile(ownedBy, popType, vehType)
{
	InitAmphibiousAutomobile();
}

//
//
//
CAmphibiousAutomobile::~CAmphibiousAutomobile()
{	
}

bool CAmphibiousAutomobile::PlaceOnRoadAdjustInternal(float HeightSampleRangeUp, float HeightSampleRangeDown, bool bJustSetCompression)
{
	// Make this boat track the water surface by forcing the low LOD buoyancy processing.
	Vector3 vPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
	m_Buoyancy.ForceUpdateRiverProbeResultSynchronously(vPos);
	Assert(pHandling);
	Assert(pHandling->GetBoatHandlingData());
	m_Buoyancy.SetLowLodDraughtOffset(pHandling->GetBoatHandlingData()->m_fLowLodDraughtOffset);

	Matrix34 originalMat = MAT34V_TO_MATRIX34(GetMatrix());
	
	bool bPlaceOnGroundResult = false;

	bool bOriginalAnchorState = GetAnchorHelper().UsingLowLodMode();
	GetAnchorHelper().SetLowLodMode(true);
	Assert(pHandling);
	Assert(pHandling->GetBoatHandlingData());
	bool bFoundWater = ProcessLowLodBuoyancy();
	GetAnchorHelper().SetLowLodMode(bOriginalAnchorState);

	// Check if vehicle was placed on water so that we can do place on water specific stuff
	bool bPlacedOnWater = false;

	Matrix34 placeOnWaterMat = MAT34V_TO_MATRIX34(GetMatrix());

	SetMatrix(originalMat);

	bool bFoundGround = CAutomobile::PlaceOnRoadAdjustInternal(HeightSampleRangeUp, HeightSampleRangeDown, bJustSetCompression);

	Matrix34 placeOnGroundMat = MAT34V_TO_MATRIX34(GetMatrix());

	bPlaceOnGroundResult = bFoundGround || bFoundWater;

	if(bFoundGround && bFoundWater)
	{
		// If placing on water would place this higher, place on water
		if(placeOnGroundMat.d.GetZ() < placeOnWaterMat.d.GetZ() )
		{
			SetMatrix(placeOnWaterMat);
			bPlacedOnWater = true;
		}
		else
		{
			SetMatrix(placeOnGroundMat);
		}
	}
	else if(bFoundGround)
	{
		SetMatrix(placeOnGroundMat);
	}
	else if(bFoundWater)
	{
		SetMatrix(placeOnWaterMat);
		bPlacedOnWater = true;
	}
	// No ground and no water
	else if( !bFoundWater && !bFoundGround )
	{
		SetMatrix(originalMat);

		if( m_BoatHandling.GetPlaceOnWaterTimeout() == 0 )
		{
			m_BoatHandling.SetPlaceOnWaterTimeout( fwTimer::GetTimeInMilliseconds() );
		}

		u32 nTimeWaiting = fwTimer::GetTimeInMilliseconds() - m_BoatHandling.GetPlaceOnWaterTimeout();

		// We don't want to get stuck continually trying to place this boat if there is no water in this region. If we have
		// been waiting for longer than 60s to place this boat, remove it from the place on road queue.
		const u32 knPlaceOnWaterTimeout = 60000;

		if( !Verifyf( nTimeWaiting < knPlaceOnWaterTimeout,
			"A boat(%s) at (%5.3f, %5.3f, %5.3f) has been waiting %5.3f seconds to be placed on water.",
			GetModelName(), vPos.x, vPos.y, vPos.z, nTimeWaiting / 1000.0f ) )
		{
			m_nVehicleFlags.bPlaceOnRoadQueued = false;
			bPlaceOnGroundResult = true;
		}
		
		if(!bPlaceOnGroundResult)
		{
			m_nVehicleFlags.bPlaceOnRoadQueued = true;
		}
	}

	if(bPlacedOnWater)
	{
		ActivatePhysics();
		m_BoatHandling.SetPlaceOnWaterTimeout( 0 );
		m_nVehicleFlags.bPlaceOnRoadQueued = false;
	}

	if( bPlaceOnGroundResult )
	{
		m_BoatHandling.SetPlaceOnWaterTimeout( 0 );
	}

	return bPlaceOnGroundResult;
}

void CAmphibiousAutomobile::InitAmphibiousAutomobile()
{
	m_LastEngineModeSwitchTime = 0u;
	m_bReverseBrakeAndThrottle = false;

	m_AnchorHelper.SetParent(this);

	m_bUseBoatNetworkBlend = false;
}

bool CAmphibiousAutomobile::IsPropellerSubmerged()
{ 
	// Lower framerates would cause this to fail since the check is done less frequently. Scale the threshold up if the framerate is lower
	float fTimeStepMult = rage::Max( fwTimer::GetTimeStep() / 0.033f, 1.0f );

	float fPropOutOfWaterTime = ( float ) ( fwTimer::GetTimeInMilliseconds() - m_BoatHandling.GetLastTimePropInWater() );
	
	return fPropOutOfWaterTime < ( sm_fPropellerTimeOutOfWaterTolerance * fTimeStepMult );
}

void CAmphibiousAutomobile::AddPhysics()
{
	CAutomobile::AddPhysics();

	// if we're adding physics and the anchor flag was set, we actually need to do the anchoring now
	if(m_AnchorHelper.IsAnchored())
	{
		m_AnchorHelper.Anchor(true, true);
	}
}

int CAmphibiousAutomobile::InitPhys()
{
	CAutomobile::InitPhys();

	if( m_BoatHandling.GetWreckedAction() == BWA_UNDECIDED && 
		fwRandom::GetRandomNumberInRange(0.0f,1.0f) <= 0.5f)
	{
		m_BoatHandling.SetWreckedAction( BWA_SINK );
	}
	else
	{
		m_BoatHandling.SetWreckedAction( BWA_FLOAT );
	}

	if( GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE )
	{
		static dev_bool sbOverrideAngInertia = true;
		static dev_float sfSolverInvAngularMultX = 1.0f;

		if(sbOverrideAngInertia && GetCollider())
		{
			Vector3 vecSolverAngularInertia = VEC3V_TO_VECTOR3(GetCollider()->GetInvAngInertia());
			vecSolverAngularInertia.x *= sfSolverInvAngularMultX; //allow some pitch
			vecSolverAngularInertia.y = 0.0f;//prevent roll
			vecSolverAngularInertia.z = 0.0f;//prevent yaw

			GetCollider()->SetSolverInvAngInertiaResetOverride(vecSolverAngularInertia);
		}
	}

	return INIT_OK;
}

ePrerenderStatus CAmphibiousAutomobile::PreRender( const bool bIsVisibleInMainViewport )
{ 
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnPreRenderOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfPreRenderCallingEntity(), VEC3V_TO_VECTOR3(GetTransform().GetPosition()) );

	m_BoatHandling.UpdatePropeller( this );

	return CAutomobile::PreRender( bIsVisibleInMainViewport );
}

void CAmphibiousAutomobile::PreRender2(const bool bIsVisibleInMainViewport)
{	
	return CAutomobile::PreRender2(bIsVisibleInMainViewport);

	// check and update overrides to light status
	//bool bForcedOn = UpdateVehicleLightOverrideStatus();

	//bool bRemoteOn = IsNetworkClone() ? m_nVehicleFlags.bLightsOn : true; 

	// TODO: SOMETHING WITH BOAT LIGHTS
	//if( bRemoteOn && (true == m_nVehicleFlags.bEngineOn || (true == bForcedOn) ) && (GetVehicleLightsStatus()) )
	//{
	//	CVehicleModelInfo* pModelInfo=GetVehicleModelInfo();

	//	float cutoffDistanceTweak = CVehicle::GetLightsCutoffDistanceTweak();
	//	float camDist = rage::Sqrtf(CVfxHelper::GetDistSqrToCamera(GetTransform().GetPosition()));
	//	const float interiorLightsBrightness = rage::Clamp(((g_Boat_InteriorLight_ShutDownDistance + cutoffDistanceTweak) - camDist)*oo_BoatLights_FadeLength,0.0f,1.0f);

	//	if( interiorLightsBrightness )
	//	{
	//		extern ConfigBoatLightSettings g_BoatLights;

	//		DoLight(VEH_SIREN_1, g_BoatLights, interiorLightsBrightness, pModelInfo, false, 0.0f);
	//		DoLight(VEH_SIREN_2, g_BoatLights, interiorLightsBrightness, pModelInfo, false, 0.0f);

	//		if (m_nVehicleFlags.bInteriorLightOn)
	//		{
	//			float coronaFade = 1.0f - rage::Clamp(((g_Boat_corona_ShutDownDistance ) - camDist)*oo_Corona_FadeLength,0.0f,1.0f);
	//			DoLight(VEH_INTERIORLIGHT, g_BoatLights, interiorLightsBrightness, pModelInfo, cutoffDistanceTweak > 0.0f, coronaFade);
	//		}
	//	}
	//}

	//	//g_vfxVehicle.ProcessLowLodBoatWake(this);
	//	fragInst* pInst = GetFragInst();

	//	if( pInst &&
	//		GetDriver() )
	//	{
	//		pInst->PoseBoundsFromSkeleton( true, true, false );
	//	}
	//}
}

void CAmphibiousAutomobile::CheckForAudioModeSwitch(bool UNUSED_PARAM(isFocusVehicle) BANK_ONLY(, bool forceBoat))
{	
	audVehicleAudioEntity* vehicleAudioEntity = m_VehicleAudioEntity;

	if(vehicleAudioEntity && vehicleAudioEntity->GetAudioVehicleType() == AUD_VEHICLE_CAR)
	{
		audCarAudioEntity* carAudioEntity = (audCarAudioEntity*)vehicleAudioEntity;
		const bool inBoatMode = carAudioEntity->IsInAmphibiousBoatMode();
		const bool shouldBeInBoatMode = IsPropellerSubmerged() BANK_ONLY(|| forceBoat);
		bool modeSwitchValid = true;

		if(inBoatMode && !shouldBeInBoatMode)
		{			
			modeSwitchValid = !IsInAir(true);
		}

		if(modeSwitchValid && inBoatMode != shouldBeInBoatMode)
		{
			if(fwTimer::GetTimeInMilliseconds() - m_LastEngineModeSwitchTime > 1000)
			{
				m_LastEngineModeSwitchTime = fwTimer::GetTimeInMilliseconds();
				carAudioEntity->SetInAmphibiousBoatMode(shouldBeInBoatMode);
			}
		}
	}
} 


//
//
// physics
ePhysicsResult CAmphibiousAutomobile::ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice)
{
	if(GetCollider())
	{
		GetCollider()->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY_ON_ROOT_LINK, false);
		GetCollider()->GetInstance()->SetInstFlag(phInst::FLAG_NO_GRAVITY, false);
	}

	// Update the wheel in water flag based on whether the propeller is submerged
	for(int i = 0; i < GetNumWheels(); i++)
	{
		GetWheel(i)->SetWheelInWater(IsPropellerSubmerged());
	}

	if(IsNetworkClone())
	{
		if( IsPropellerSubmerged() && GetNumContactWheels() == 0 )
		{
			m_bUseBoatNetworkBlend = true;
		}
		else if( m_bUseBoatNetworkBlend && GetNumContactWheels() != 0 )
		{
			m_bUseBoatNetworkBlend = false;
		}
	}

	bool bSkipPhysicsUpdate = false;

	if(m_AnchorHelper.IsAnchored())
	{
		// Don't reactivate if we are in low LOD anchor mode (this would happen if we call ProcessIsAsleep() while in the water).
		CBoat::UpdateBuoyancyLodMode( this, m_AnchorHelper );

		if( m_AnchorHelper.UsingLowLodMode() )
		{
			return PHYSICS_DONE;
		}
	}

	if( m_vehicleAiLod.IsLodFlagSet( CVehicleAILod::AL_LodSuperDummy ) )
	{
		bSkipPhysicsUpdate = true;
	}

	if( GetCurrentPhysicsInst()==NULL || !GetCurrentPhysicsInst()->IsInLevel() )
	{
		bSkipPhysicsUpdate = true;
	}

	if(!bSkipPhysicsUpdate)
	{
		SetDamping();

		bool bSkipBuoyancy = false;

		// if network has re-spotted this car and we don't want it to collide with other network vehicles for a while
		ProcessRespotCounter( fTimeStep );

		// If boat is wrecked then we might want to sink it
		if( GetStatus()==STATUS_WRECKED && m_BoatHandling.GetWreckedAction() == BWA_SINK )
		{
			// If we are anchored then release anchor
			if( m_AnchorHelper.IsAnchored() )
			{
				m_AnchorHelper.Anchor( false );
			}

			dev_float AMPHIBIOUS_AUTOMOBILE_SINK_FORCEMULT_STEP = 0.002f;

			if(m_Buoyancy.m_fForceMult > AMPHIBIOUS_AUTOMOBILE_SINK_FORCEMULT_STEP)
			{
				m_Buoyancy.m_fForceMult -= AMPHIBIOUS_AUTOMOBILE_SINK_FORCEMULT_STEP;
			}
			else
			{
				m_Buoyancy.m_fForceMult = 0.0f;
			}

			// Check for boat well below water surface and then fix if below 20m
			dev_float AMPHIBIOUS_AUTOMOBILE_SUBMERGE_SLEEP_GLOBAL_Z = -20.0f;

			if( GetTransform().GetPosition().GetZf() < AMPHIBIOUS_AUTOMOBILE_SUBMERGE_SLEEP_GLOBAL_Z )
			{
				DeActivatePhysics();
				bSkipBuoyancy = true;
			}
		}

		// do buoyancy all the time for boats, will cause them to wake up
		// but allow to go to sleep if stranded on a beach
		if(!bSkipBuoyancy)
		{
			if(InheritsFromAmphibiousQuadBike())
			{
				m_Buoyancy.SetShouldStickToWaterSurface(true);
			}

			m_BoatHandling.SetInWater( m_Buoyancy.Process(this, fTimeStep, false, CPhysics::GetIsLastTimeSlice(nTimeSlice)) || m_AnchorHelper.UsingLowLodMode() );
			SetIsInWater(m_BoatHandling.IsInWater());
		}

		m_nVehicleFlags.bIsDrowning = false;

		m_BoatHandling.ProcessPhysics( this, fTimeStep );
	
	} // End if(!bSkipPhysicsUpdate)
	
	return CAutomobile::ProcessPhysics(fTimeStep, bCanPostpone, nTimeSlice);
}

void CAmphibiousAutomobile::UpdatePhysicsFromEntity(bool bWarp)
{
	bool bWasAnchored = GetAnchorHelper().IsAnchored();
	if(bWasAnchored && !GetAnchorHelper().UsingLowLodMode())
	{
		GetAnchorHelper().Anchor(false);
	}

	CAutomobile::UpdatePhysicsFromEntity(bWarp);

	if(bWasAnchored && !GetAnchorHelper().UsingLowLodMode())
	{
		GetAnchorHelper().Anchor(true);
	}
}

void CAmphibiousAutomobile::DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep)
{	
	if(m_AnchorHelper.IsAnchored())
	{
		ProcessLowLodBuoyancy();
	}

	CAutomobile::DoProcessControl(fullUpdate, fFullUpdateTimeStep);

	m_BoatHandling.DoProcessControl( this );
		
	CPlayerInfo *pPlayer = GetDriver() ? GetDriver()->GetPlayerInfo() : NULL;

	float fAirResistance = m_fDragCoeff;
	if( pPlayer && pPlayer->m_fForceAirDragMult > 0.0f )
	{
		fAirResistance *= pPlayer->m_fForceAirDragMult;
	}

	phArchetypeDamp* pArch = (phArchetypeDamp*)GetVehicleFragInst()->GetArchetype();

	Vector3 vecLinearV2Damping;
	vecLinearV2Damping.Set( fAirResistance, fAirResistance, 0.0f );

	if(InheritsFromAmphibiousQuadBike() && static_cast<CAmphibiousQuadBike*>(this)->IsWheelsFullyOut() && IsPropellerSubmerged())
	{
		TUNE_GROUP_FLOAT(AMPHIBIOUS_VEHICLE_TUNE, WHEEL_OUT_DRAG_MULT, 37.0f, -10.0f, 100.0f, 0.1f);
		vecLinearV2Damping *= WHEEL_OUT_DRAG_MULT;
	}

	if( pPlayer )
	{
		float fSlipStreamEffect = GetSlipStreamEffect();
		vecLinearV2Damping -= (vecLinearV2Damping * fSlipStreamEffect);
	}

	pArch->ActivateDamping( phArchetypeDamp::LINEAR_V2, vecLinearV2Damping );

	// Note: for the dummy instance, changing these values is a bit dubious since
	// multiple instances actually point to the same phArchetypeDamp object. In practice,
	// there are no known problems. We might want to not even do it at all, but now
	// when we don't have to do this on each frame for all AI vehicles, it shouldn't matter much.
	phInstGta* pDummyInst = GetDummyInst();
	if(pDummyInst)
	{	
		phArchetypeDamp* pDummyArch = ((phArchetypeDamp*)pDummyInst->GetArchetype());
		pDummyArch->ActivateDamping( phArchetypeDamp::LINEAR_V2, vecLinearV2Damping );
	}
}


bool CAmphibiousAutomobile::ProcessLowLodBuoyancy()
{
	// If a boat is in low LOD anchor mode, it is physically inactive and we place it based on the water level sampled at
	// points around the edges of the bounding box.

	bool bFoundWater = false;

	if( m_AnchorHelper.UsingLowLodMode() )
	{
		const Vector3 vOriginalPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

		// Indicate this boat is in low LOD anchor mode for debugging.
#if __BANK
		if( CBuoyancy::ms_bIndicateLowLODBuoyancy )
		{
			grcDebugDraw::Sphere( vOriginalPosition + Vector3(0.0f, 0.0f, 2.0f), 1.0f, Color_yellow );
		}
#endif // __BANK

		float fWaterZAtEntPos;
		WaterTestResultType waterTestResult = m_Buoyancy.GetWaterLevelIncludingRivers( vOriginalPosition, &fWaterZAtEntPos, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL, this );
		bFoundWater = waterTestResult > WATERTEST_TYPE_NONE;

		if( bFoundWater )
		{
			// We know the optimum stable height offset for boats so if this boat isn't at that offset yet, LERP it
			// there over a few frames.
			float fLowLodDraughtOffset = m_Buoyancy.GetLowLodDraughtOffset();
			float fDifferenceToIdeal = pHandling->GetBoatHandlingData()->m_fLowLodDraughtOffset - fLowLodDraughtOffset;
			if(fabs(fDifferenceToIdeal) > 0.001f)
			{
				dev_float sfStabiliseFactor = 0.1f;
				fLowLodDraughtOffset += sfStabiliseFactor * fDifferenceToIdeal;
				m_Buoyancy.SetLowLodDraughtOffset( fLowLodDraughtOffset );
			}

			// Now we actually move the boat to simulate buoyancy physics.
			Matrix34 boatMat = MAT34V_TO_MATRIX34(GetMatrix());
			Vector3 vPlaneNormal;
			ComputeLowLodBuoyancyPlaneNormal( boatMat, vPlaneNormal );

			// Apply correction for the orientation of the boat when in equilibrium.
			float fCorrectionRotAngle = pHandling->GetBoatHandlingData()->m_fLowLodAngOffset;
			if( fabs( fCorrectionRotAngle ) > SMALL_FLOAT )
			{
				boatMat.UnTransform3x3( vPlaneNormal );
				vPlaneNormal.RotateX( fCorrectionRotAngle );
				boatMat.Transform3x3( vPlaneNormal );
			}

			// Figure out how much to rotate the boat's matrix by to match the plane's orientation.
			Vector3 vRotationAxis;
			vRotationAxis.Cross( boatMat.c, vPlaneNormal );
			vRotationAxis.NormalizeFast();
			float fRotationAngle = boatMat.c.Angle( vPlaneNormal );

			// If we are far away from the equilibrium orientation for the boat (because of big waves or other external
			// forces on the boat) we limit the amount we move towards the ideal position each frame. Only do this for
			// the first few seconds after switching to low LOD mode or it damps the motion when following the waves.
			const u32 knTimeToLerpAfterSwitchToLowLod = 2500;
			u32 nTimeSpentLerping = fwTimer::GetTimeInMilliseconds() - m_BoatHandling.GetLowLodBuoyancyModeTimer();
			if( nTimeSpentLerping < knTimeToLerpAfterSwitchToLowLod )
			{
				float fX = float( nTimeSpentLerping ) / float( knTimeToLerpAfterSwitchToLowLod );
				float kfStabiliseFactor = rage::Min( fX * fX, 1.0f );
				fRotationAngle *= kfStabiliseFactor;
			}
			if( fabs( fRotationAngle ) > SMALL_FLOAT && vRotationAxis.IsNonZero() && vRotationAxis.FiniteElements() )
			{
				boatMat.RotateUnitAxis( vRotationAxis, fRotationAngle );
			}

			// If the boat rotates about its up-axis too much it can end up violating the physical constraints when
			// switching back to full physical buoyancy mode. We know the positions of the prow and stern constraints
			// so apply a LERP back towards this orientation if we start to deviate too much.
			float fConstraintYawDiff = m_AnchorHelper.GetYawOffsetFromConstrainedOrientation();
			const float fLerpBackTowardsConstraintYawFactor = 1.0f;
			fConstraintYawDiff *= fLerpBackTowardsConstraintYawFactor;
			if( fabs( fConstraintYawDiff ) > SMALL_FLOAT )
			{
				boatMat.RotateZ( fConstraintYawDiff );
			}

			CBoat::RejuvenateMatrixIfNecessary( boatMat );
			SetMatrix( boatMat );

			SetPosition( Vector3( vOriginalPosition.x, vOriginalPosition.y, fWaterZAtEntPos+fLowLodDraughtOffset ) );

			// Update the "is in water" flag as would be done in CBuoyancy::Process().
			SetIsInWater( true );
		}
	}

	return bFoundWater;
}

void CAmphibiousAutomobile::ComputeLowLodBuoyancyPlaneNormal(const Matrix34& boatMat, Vector3& vPlaneNormal)
{
	// Work out the position of the back bottom corners and the prow.
	Vector3 vBoundBoxMin = GetBoundingBoxMin();
	Vector3 vBoundBoxMax = GetBoundingBoxMax();

	const int nNumVerts = 3;
	Vector3 vBoundBoxBottomVerts[nNumVerts];
	vBoundBoxBottomVerts[0] = Vector3(vBoundBoxMin.x, vBoundBoxMin.y, vBoundBoxMin.z);
	vBoundBoxBottomVerts[1] = Vector3(vBoundBoxMax.x, vBoundBoxMin.y, vBoundBoxMin.z);
	vBoundBoxBottomVerts[2] = Vector3(0.5f*(vBoundBoxMin.x+vBoundBoxMax.x), vBoundBoxMax.y, vBoundBoxMin.z);

	for(int nVert = 0; nVert < nNumVerts; ++nVert)
	{
		boatMat.Transform(vBoundBoxBottomVerts[nVert]);
		float fWaterZ;
		CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(vBoundBoxBottomVerts[nVert], &fWaterZ);
		vBoundBoxBottomVerts[nVert].z = fWaterZ;

#if __BANK
		if(CBuoyancy::ms_bIndicateLowLODBuoyancy)
		{
			grcDebugDraw::Sphere(vBoundBoxBottomVerts[nVert], 0.1f, Color_red);
		}
#endif // __BANK
	}

	// Use the verts above to define a plane which can be used to set the orientation of the boat.
	vPlaneNormal.Cross(vBoundBoxBottomVerts[0]-vBoundBoxBottomVerts[1], vBoundBoxBottomVerts[0]-vBoundBoxBottomVerts[2]);
}

//
//
//
bool CAmphibiousAutomobile::WantsToBeAwake()
{ 
	int fNumContactWheels = GetNumContactWheels();
	int fNumWheels = GetNumWheels();
	bool bIsInWater = m_BoatHandling.IsInWater();
	bool bThrottle = ( GetThrottle() != 0.0f );
	bool bAutomobileWantsToBeAwake = CAutomobile::WantsToBeAwake();

	return ( ( fNumContactWheels != fNumWheels && bIsInWater && !m_AnchorHelper.IsAnchored() ) || bThrottle ) || ( bAutomobileWantsToBeAwake && !m_AnchorHelper.IsAnchored() ); 
}

void CAmphibiousAutomobile::Fix( bool resetFrag, bool allowNetwork )
{	
	m_BoatHandling.Fix();
	CAutomobile::Fix( resetFrag, allowNetwork );
}

void CAmphibiousAutomobile::SetDamping()
{
	CBoatHandlingData* pBoatHandling = pHandling->GetBoatHandlingData();

	if( pBoatHandling == NULL )
	{
		return;
	}

	bool bIsArticulated = false;
	if( GetCollider() )
	{
		bIsArticulated = GetCollider()->IsArticulated();
	}

	float fDampingMultiplier = 1.0f;

	if( GetDriver() )
	{
		if( GetDriver()->IsPlayer() && GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE )
		{
			CControl *pControl = GetDriver()->GetControlFromPlayer();
			if(pControl)
			{
				float fStickY = pControl->GetVehicleSteeringUpDown().GetNorm();
				fStickY = fStickY + 1.0f;
				fStickY = rage::Clamp(fStickY, 0.0f, 1.0f);
				fDampingMultiplier *= sm_fJetskiRiderResistanceMultMin + fStickY * ( sm_fJetskiRiderResistanceMultMax - sm_fJetskiRiderResistanceMultMin );
			}
		}
	}

	float fDV = bIsArticulated ? CVehicleModelInfo::STD_BOAT_LINEAR_V_COEFF : 2.0f * CVehicleModelInfo::STD_BOAT_LINEAR_V_COEFF;
	fDV *= fDampingMultiplier;
	((phArchetypeDamp*)GetVehicleFragInst()->GetArchetype())->ActivateDamping( phArchetypeDamp::LINEAR_V, Vector3( fDV, fDV, fDV ) );

	fDV = bIsArticulated ? 1.0f : 2.0f;
	fDV *= fDampingMultiplier;
	( (phArchetypeDamp*)GetVehicleFragInst()->GetArchetype() )->ActivateDamping( phArchetypeDamp::ANGULAR_V, fDV * RCC_VECTOR3( pBoatHandling->m_vecTurnResistance ) );
	( (phArchetypeDamp*)GetVehicleFragInst()->GetArchetype() )->ActivateDamping( phArchetypeDamp::ANGULAR_V2, fDV * Vector3( 0.02f, CVehicleModelInfo::STD_BOAT_ANGULAR_V2_COEFF_Y, 0.02f ) );
}

void CAmphibiousAutomobile::ApplyDeformationToBones(const void* basePtr)
{
	CAutomobile::ApplyDeformationToBones(basePtr);

	if(basePtr)
	{
		CBoatHandling* pBoatHandling = GetBoatHandling();

		Vector3 vecPos;
		if( GetBoneIndex( AMPHIBIOUS_AUTOMOBILE_STATIC_PROP ) != -1 )
		{
			// get the original position of the prop
			GetDefaultBonePositionForSetup( AMPHIBIOUS_AUTOMOBILE_STATIC_PROP, vecPos );

			pBoatHandling->SetPropDeformOffset1( VEC3V_TO_VECTOR3( GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset( basePtr, VECTOR3_TO_VEC3V( vecPos ) ) ) );
		}
		if( GetBoneIndex( AMPHIBIOUS_AUTOMOBILE_STATIC_PROP2 ) != -1 )
		{
			// get the original position of the prop
			GetDefaultBonePositionForSetup( AMPHIBIOUS_AUTOMOBILE_STATIC_PROP2, vecPos );

			pBoatHandling->SetPropDeformOffset2( VEC3V_TO_VECTOR3( GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset( basePtr, VECTOR3_TO_VEC3V( vecPos ) ) ) );
		}
	}
}

float CAmphibiousAutomobile::GetBoatEngineThrottle()
{
	float fThrottle = GetThrottle() - GetBrake();

	// Cars brake when going backwards fast and pressing accelerate, boats don't
	// Reverse throttle if this is the case so that accelerating doesn't make you go backwards
	if(GetShouldReverseBrakeAndThrottle())
	{
		fThrottle *= -1.0f;
	}

	if( !m_nVehicleFlags.bEngineOn && m_nVehicleFlags.bIsDrowning )
	{
		fThrottle = 0.0f;
	}

	return fThrottle;
}

//
//
//
CAmphibiousQuadBike::CAmphibiousQuadBike(const eEntityOwnedBy ownedBy, u32 popType) : CAmphibiousAutomobile(ownedBy, popType, VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE)
{
	m_fSuspensionRaiseAmount = 0.0f;
	m_bShouldTuckInWheels = false;
	m_fWheelShiftAmount = 0.0f;
	m_fWheelTiltAmount = 0.0f;
	m_bRetractButtonHasBeenUp = true;
	m_bToggleTuckInWheelsFromNetwork = false;

	m_fWheelsFullyRetractedTime = 0.0f;
}

//
//
//
CAmphibiousQuadBike::~CAmphibiousQuadBike()
{

}

bank_float sfQuadBikeSelfRightingTorqueScale = 15.0f;
bank_float sfQuadBikeAntiRollOverTorqueScale = -35.0f;

dev_float CAmphibiousQuadBike::sm_fWheelRaiseForTiltAndShift = 0.2f;
dev_float CAmphibiousQuadBike::sm_fWheelRetractRaiseAmount = 0.44f;
dev_float CAmphibiousQuadBike::sm_fWheelRetractShiftAmount = 0.1f;
dev_float CAmphibiousQuadBike::sm_fWheelRetractTiltAmount = 60.0f;
//
//
// physics
ePhysicsResult CAmphibiousQuadBike::ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice)
{
	//Use the fallen collider flag on quads all the time as they are open wheeled and not constrained
	if(!GetDriver())
	{
		for(int i=0; i<GetNumWheels(); i++)
		{
			GetWheel(i)->GetConfigFlags().SetFlag(WCF_BIKE_FALLEN_COLLIDER);
		}

		// Allow peds to flip overturned bikes by walking into the side
		if(const CCollisionRecord* pedCollisionRecord = GetFrameCollisionHistory()->GetMostSignificantCollisionRecordOfType(ENTITY_TYPE_PED))
		{
			const Mat34V quadBikeMatrix = GetMatrix();
			const Vec3V quadBikeUp = quadBikeMatrix.GetCol2();
			const Vec3V quadBikeForward = quadBikeMatrix.GetCol1();
			const Vec3V collisionNormal = RCC_VEC3V(pedCollisionRecord->m_MyCollisionNormal);

			// Only apply torques if the impact isn't with the front or bottom of the bike
			const ScalarV frontImpactAngleCosine = ScalarVFromF32(0.766f); // 40 degrees 
			const ScalarV bottomImpactAngleCosine = ScalarV(V_HALF); // 60 degrees
			if(And(IsLessThan(Abs(Dot(quadBikeForward,collisionNormal)),frontImpactAngleCosine),IsLessThan(Dot(quadBikeUp,collisionNormal),bottomImpactAngleCosine)).Getb())
			{
				// Only bother applying torque if we're offset to begin with
				const float minRollForSelfRighting = 20.0f*DtoR;
				float currentRoll = GetTransform().GetRoll();
				if(abs(currentRoll) > minRollForSelfRighting)
				{
					// The roll angle favors whichever direction gets us to the z-axis quicker but we always want to rotate
					//  so that the top of the bike goes towards the ped.
					Vec3V torque = Scale(quadBikeForward,ScalarVFromF32(currentRoll));
					if(Cross(collisionNormal,torque).GetZf() < 0)
					{
						// Negate the rotation axis and increase how much roll we need to cover since we're going the long way. 
						torque = Scale(Negate(quadBikeForward), ScalarVFromF32(currentRoll > 0 ? 2.0f*PI - currentRoll : -2.0f*PI - currentRoll));
					}

					// Apply a torque around the forward axis, relative to the roll error and angular inertia around the forward axis. 
					ApplyInternalTorque(RCC_VECTOR3(torque) * sfQuadBikeSelfRightingTorqueScale * GetAngInertia().y);
				}
			}
		}
	}

	if(GetDriver() && !IsPropellerSubmerged())// Do some autoleveling if in the air.
	{
		if( IsInAir() && pHandling->GetBikeHandlingData() && !GetFrameCollisionHistory()->GetMostSignificantCollisionRecord() && GetNumWheels() != 3 )
		{
			bool bDoAutoLeveling = true;
			if(GetDriver()->IsAPlayerPed())
			{
				Vec3V vPosition = GetTransform().GetPosition();
				float fMapMaxZ = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(vPosition.GetXf(), vPosition.GetYf());

				//Let the player perform rolls when really high in the air
				Vec3V vBBMinZGlobal = GetTransform().Transform(Vec3V(0.0f, 0.0f, GetBoundingBoxMin().z));
				if(vBBMinZGlobal.GetZf() > fMapMaxZ)
				{
					bDoAutoLeveling = false;
				}
			}

			if(bDoAutoLeveling)
			{
				AutoLevelAndHeading( pHandling->GetBikeHandlingData()->m_fInAirSteerMult );
			}
		}
		else if(GetNumContactWheels() != GetNumWheels() && GetCollider()) // Roll over protection
		{
			static dev_float sfRollOverProtectionVelocity = 1.5f;

			Vec3V vCurrentAngVelocity = GetCollider()->GetAngVelocity();

			vCurrentAngVelocity = UnTransform3x3Full(GetCollider()->GetMatrix(), vCurrentAngVelocity);

			if(rage::Abs(vCurrentAngVelocity.GetYf()) > sfRollOverProtectionVelocity)
			{
				float fAngVelocityY = rage::Clamp(vCurrentAngVelocity.GetYf(), -3.0f, 3.0f);
				const Mat34V quadBikeMatrix = GetMatrix();
				const Vec3V quadBikeForward = quadBikeMatrix.GetCol1();

				ApplyInternalTorque(RCC_VECTOR3(quadBikeForward) * fAngVelocityY * sfQuadBikeAntiRollOverTorqueScale * GetAngInertia().y);
			}
		}
	}
	
	if(ContainsLocalPlayer())
	{
		if( !IsWheelsFullyIn() && IsPropellerSubmerged() && GetBoatEngineThrottle() )
		{
			static dev_u32 duration = 80;
			static dev_s32 freq0 = 0;
			static dev_s32 freq1 = 80;
			CControlMgr::StartPlayerPadShake(duration, freq0, duration, freq1);
		}
	}

	if( IsWheelsFullyIn() )
	{
		m_fWheelsFullyRetractedTime += fTimeStep;
	}
	else
	{
		m_fWheelsFullyRetractedTime = 0.0f;
	}

	return CAmphibiousAutomobile::ProcessPhysics(fTimeStep, bCanPostpone, nTimeSlice);
}

void CAmphibiousQuadBike::ProcessPostPhysics()
{
	CAmphibiousAutomobile::ProcessPostPhysics();

	m_fVerticalVelocityToKnockOffThisBike = CVehicle::ms_fVerticalVelocityToKnockOffVehicle;
}

void CAmphibiousQuadBike::ToggleTuckInWheels()
{ 
	m_bShouldTuckInWheels = !m_bShouldTuckInWheels;
	m_VehicleAudioEntity->SetLandingGearRetracting(m_bShouldTuckInWheels);

	CNewHud::ForceSpecialVehicleStatusUpdate();
}

void CAmphibiousQuadBike::PreRender2( const bool bIsVisibleInMainViewport )
{
	TUNE_GROUP_FLOAT(AMPHIBIOUS_VEHICLE_TUNE, WHEEL_RAISE_SPEED, 0.4f, -10.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(AMPHIBIOUS_VEHICLE_TUNE, WHEEL_TILT_SPEED, 100.0f, -10.0f, 200.0f, 0.1f);
	TUNE_GROUP_FLOAT(AMPHIBIOUS_VEHICLE_TUNE, WHEEL_SHIFT_SPEED, 0.5f, -100.0f, 100.0f, 0.1f);
	TUNE_GROUP_FLOAT(AMPHIBIOUS_VEHICLE_TUNE, WHEEL_TILT_FOR_SHIFT, 30.0f, -100.0f, 100.0f, 0.1f);
	TUNE_GROUP_FLOAT(AMPHIBIOUS_VEHICLE_TUNE, WHEEL_RAISE_FOR_SHIFT, 0.35f, -100.0f, 100.0f, 0.1f);

	float fWheelMoveDirection = -1.0f;
	bool bUpdateWheelRaise = false;
	bool bUpdateWheelTilt = false;
	bool bUpdateWheelShift = false;

	if(m_bShouldTuckInWheels)
	{
		// Reverse the direction in which the wheels move
		fWheelMoveDirection = 1.0f;

		bUpdateWheelRaise = true;

		// Only allow the wheels to rotate and shift inwards after they've been raised a bit
		if(m_fSuspensionRaiseAmount >= sm_fWheelRaiseForTiltAndShift)
		{
			bUpdateWheelTilt = true;

			// To avoid clipping, only shift the wheels when they're tilted and raised a certain amount
			if( m_fWheelTiltAmount >= WHEEL_TILT_FOR_SHIFT && m_fSuspensionRaiseAmount >= WHEEL_RAISE_FOR_SHIFT )
			{
				bUpdateWheelShift = true;
			}
		}
	}
	else
	{
		bUpdateWheelShift = true;
		bUpdateWheelTilt = true;
		bUpdateWheelRaise = true;
	}

	// Update the suspension raise, shift inwards and tilt inwards
	if(bUpdateWheelRaise)
	{
		m_fSuspensionRaiseAmount = rage::Clamp(m_fSuspensionRaiseAmount + fwTimer::GetTimeStep() * WHEEL_RAISE_SPEED * fWheelMoveDirection, 0.0f, sm_fWheelRetractRaiseAmount);
	}

	if(bUpdateWheelTilt)
	{
		m_fWheelTiltAmount = rage::Clamp(m_fWheelTiltAmount + fwTimer::GetTimeStep() * WHEEL_TILT_SPEED * fWheelMoveDirection, 0.0f, sm_fWheelRetractTiltAmount);
	}

	if(bUpdateWheelShift)
	{
		m_fWheelShiftAmount = rage::Clamp(m_fWheelShiftAmount + fwTimer::GetTimeStep() * WHEEL_SHIFT_SPEED * fWheelMoveDirection, 0.0f, sm_fWheelRetractShiftAmount);
	}

	for( int i = 0; i < GetNumWheels(); i++ )
	{			
		CWheel* wheel = GetWheel( i );

		// Make sure the front wheels are straight
		if( wheel->GetConfigFlags().IsFlagSet( WCF_STEER ) && m_fSuspensionRaiseAmount != 0.0f )
		{
			float steeringAngle	= wheel->GetSteeringAngle();

			if( steeringAngle != 0.0f )
			{
				static dev_float removeSteeringRate = 10.0f;

				if( steeringAngle > 0.0f )
				{
					steeringAngle = Max( 0.0f, steeringAngle - ( removeSteeringRate * fwTimer::GetTimeStep() ) );
				}
				else
				{
					steeringAngle = Min( 0.0f, steeringAngle + ( removeSteeringRate * fwTimer::GetTimeStep() ) );
				}

				wheel->SetSteerAngle( steeringAngle );
			}
		}

		// Update the suspension and set the wheel raise if wheels not at end positions
		if(m_fSuspensionRaiseAmount != sm_fWheelRetractRaiseAmount && m_fSuspensionRaiseAmount != 0.0f)
		{
			wheel->SetSuspensionRaiseAmount( -m_fSuspensionRaiseAmount );
			wheel->GetConfigFlags().SetFlag( WCF_UPDATE_SUSPENSION );
			wheel->UpdateSuspension(NULL, false);
		}
	}

	CAmphibiousAutomobile::PreRender2(bIsVisibleInMainViewport);
}

bool CAmphibiousQuadBike::IsWheelTransitionFinished() const
{
	// If tucking in, check that wheels are fully tucked in
	if(m_bShouldTuckInWheels)
	{
		return	m_fSuspensionRaiseAmount == sm_fWheelRetractRaiseAmount && 
				m_fWheelShiftAmount == sm_fWheelRetractShiftAmount && 
				m_fWheelTiltAmount == sm_fWheelRetractTiltAmount;
	}
	// If not tucking in, check that wheels are fully out
	else
	{
		return	m_fSuspensionRaiseAmount == 0.0f &&
				m_fWheelShiftAmount == 0.0f &&
				m_fWheelTiltAmount == 0.0f;
	}
}

bool CAmphibiousQuadBike::WantsToBeAwake()
{
	return (!IsWheelTransitionFinished() || CAmphibiousAutomobile::WantsToBeAwake() );
}

void CAmphibiousQuadBike::ProcessPreComputeImpacts(phContactIterator impacts)
{
	if(IsWheelsFullyIn())
	{
		impacts.Reset();

		bool processingAudioImpacts = false;
		if(!impacts.AtEnd())
		{
			processingAudioImpacts = m_VehicleAudioEntity->GetCollisionAudio().StartProcessImpacts();
		}

		const Mat34V& boatMatrix = GetMatrixRef();
		const Vec3V boatUp = boatMatrix.GetCol2();

		static dev_float sfOutOfWaterTime = 10.0f;
		static dev_float sfInWaterFrictionMultiplier = 0.1f;
		static dev_float sfOutOfWaterFrictionMultiplier = 3.0f;

		bool resetSpeedBoostObjectID = true;

		// Compute a friction multiplier based on how long we've been out of the water. 
		float outOfWaterScale = Min( GetBoatHandling()->GetOutOfWaterTime(), m_fWheelsFullyRetractedTime );
		outOfWaterScale = Min( outOfWaterScale / sfOutOfWaterTime, 1.0f );

		float frictionMultiplier = Lerp( outOfWaterScale, sfInWaterFrictionMultiplier, sfOutOfWaterFrictionMultiplier );

		Vector3 lastSideImpact = VEC3_ZERO;
		while(!impacts.AtEnd())
		{
			// Process audio for one impact
			if(processingAudioImpacts)
			{
				m_VehicleAudioEntity->GetCollisionAudio().ProcessImpactBoat(impacts);
			}

			phInst* otherInstance = impacts.GetOtherInstance();

			// This is where we can hook up event generation for vehicles colliding against foliage bounds.
			if(otherInstance && otherInstance->GetArchetype()->GetTypeFlags()&ArchetypeFlags::GTA_FOLIAGE_TYPE)
			{
				// If the bound hit was actually a composite, check that the child bound that we hit is really a foliage bound.
				bool bReallyFoliageBound = false;
				if(otherInstance->GetArchetype()->GetBound()->GetType()==phBound::COMPOSITE)
				{
					phBoundComposite* pBoundComposite = static_cast<phBoundComposite*>(otherInstance->GetArchetype()->GetBound());
					if(pBoundComposite->GetTypeFlags(impacts.GetOtherComponent())&ArchetypeFlags::GTA_FOLIAGE_TYPE)
						bReallyFoliageBound = true;
				}
				else
				{
					bReallyFoliageBound = true;
				}

				if(bReallyFoliageBound)
				{
#if DEBUG_DRAW
					if(CPhysics::ms_bShowFoliageImpactsVehicles)
					{
						grcDebugDraw::Sphere(impacts.GetMyPosition(), 0.2f, Color_yellow);
						grcDebugDraw::Sphere(impacts.GetOtherPosition(), 0.05f, Color_grey, false);
						grcDebugDraw::Line(impacts.GetMyPosition(), impacts.GetOtherPosition(), Color_grey);
					}
#endif // DEBUG_DRAW

					// Now that we've noted the collision, disable the impact.
					impacts.DisableImpact();
					++impacts;
					continue;
				}
			}

			Vector3 vecNormal;
			impacts.GetMyNormal(vecNormal);

			// Only modify the friction if the bottom of the boat is being hit
			if(IsGreaterThanAll(Dot(RCC_VEC3V(vecNormal),boatUp),ScalarV(V_HALF)))
			{
				// Contacts don't get their friction recomputed each frame so we need to do that before multiplying it
				float materialFriction,elasticity;
				PGTAMATERIALMGR->FindCombinedFrictionAndElasticity(impacts.GetMyMaterialId(),impacts.GetOtherMaterialId(),materialFriction,elasticity);
				impacts.SetFriction(materialFriction*frictionMultiplier);
			}

			impacts++;
		}

		int count = impacts.CountElements();

		phCachedContactIteratorElement *buffer = Alloca(phCachedContactIteratorElement, count);

		impacts.Reset();

		phCachedContactIterator cachedImpacts;
		cachedImpacts.InitFromIterator( impacts, buffer, count );

		while(!cachedImpacts.AtEnd())
		{
			phInst* otherInstance = cachedImpacts.GetOtherInstance();
			CEntity* pOtherEntity = otherInstance ? (CEntity*)otherInstance->GetUserData() : NULL;

			if( ProcessBoostPadImpact( cachedImpacts, pOtherEntity ) )
			{
				cachedImpacts.DisableImpact();
				resetSpeedBoostObjectID = false;
			}
			++cachedImpacts;
		}

		if(processingAudioImpacts)
		{
			m_VehicleAudioEntity->GetCollisionAudio().EndProcessImpacts();
		}

		// if we're not touching any boost pads reset the current object
		if( resetSpeedBoostObjectID )
		{
			m_iCurrentSpeedBoostObjectID = 0;
			m_iPrevSpeedBoostObjectID = 0;
		}

	}
	else
	{
		CAmphibiousAutomobile::ProcessPreComputeImpacts(impacts);
	}
}

void CAmphibiousQuadBike::ForceWheelsOut()
{
	m_bShouldTuckInWheels = false;
	m_fSuspensionRaiseAmount = 0.0f;
	m_fWheelShiftAmount = 0.0f;
	m_fWheelTiltAmount = 0.0f;

	for( int i = 0; i < GetNumWheels(); i++ )
	{			
		CWheel* wheel = GetWheel( i );

		wheel->SetSuspensionRaiseAmount( -m_fSuspensionRaiseAmount );
		wheel->GetConfigFlags().SetFlag( WCF_UPDATE_SUSPENSION );
		wheel->UpdateSuspension(NULL, false);
	}

	ActivatePhysics();
}

void CAmphibiousQuadBike::ForceWheelsIn()
{
	m_bShouldTuckInWheels = true;
	m_fSuspensionRaiseAmount = sm_fWheelRetractRaiseAmount;
	m_fWheelShiftAmount = sm_fWheelRetractShiftAmount;
	m_fWheelTiltAmount = sm_fWheelRetractTiltAmount;

	for( int i = 0; i < GetNumWheels(); i++ )
	{			
		CWheel* wheel = GetWheel( i );

		wheel->SetSuspensionRaiseAmount( -m_fSuspensionRaiseAmount );
		wheel->GetConfigFlags().SetFlag( WCF_UPDATE_SUSPENSION );
		wheel->UpdateSuspension(NULL, false);
	}

	ActivatePhysics();
}