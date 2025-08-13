//
// Title	:	Boat.cpp
// Author	:	William Henderson
// Started	:	23/02/00
//
//
//
//
//

//rage headers
#include "math/vecmath.h"
#include "physics/constrainthalfspace.h"

// Framework headers
#include "grcore/debugdraw.h"
#include "fwmaths/angle.h"
#include "fwsys/timer.h"
#include "fwutil/PtrList.h"

// Game headers
#include "camera/CamInterface.h"
#include "vehicleAi/vehicleintelligence.h"
#include "control/record.h"
#include "control/replay/replay.h"
#include "debug/debugglobals.h"
#include "debug/debugscene.h"
#include "event/events.h"
#include "Event/EventDamage.h"
#include "Event/EventShocking.h"
#include "Event/ShockingEvents.h"
#include "game/modelIndices.h"
#include "Stats/StatsMgr.h"
#include "modelInfo/modelInfo.h"
#include "modelInfo/vehicleModelInfo.h"
#include "network/Events/NetworkEventTypes.h"
#include "objects/object.h"
#include "peds/ped.h"
#include "peds/pedfactory.h"
#include "peds/pedIntelligence.h"
#include "peds/Ped.h"
#include "physics/gtaInst.h"
#include "physics/physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "renderer/lights/lights.h"
#include "renderer/River.h"
#include "renderer/water.h"
#include "scene/world/gameWorld.h"
#include "system/pad.h"
#include "system/controlMgr.h"
#include "timecycle/timecycleconfig.h"
#include "vehicles/AmphibiousAutomobile.h"
#include "vehicles/boat.h"
#include "vehicles/planes.h"
#include "vehicles/vehiclepopulation.h"
#include "vehicles/VehicleGadgets.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Misc/Fire.h"
#include "vfx/misc/Coronas.h"
#include "vfx/Systems/VfxVehicle.h"
#include "vfx/Systems/VfxWater.h"
#include "vfx/vfxhelper.h"
#include "weapons/explosion.h"

AI_VEHICLE_OPTIMISATIONS()
ENTITY_OPTIMISATIONS()
VEHICLE_OPTIMISATIONS()


EXT_PF_PAGE(GTA_Buoyancy);
EXT_PF_GROUP(Buoyancy_timers);
EXT_PF_TIMER(ProcessBuoyancy);
EXT_PF_TIMER(ProcessBuoyancyTotal);

PF_TIMER(ProcessLowLodBuoyancyTimer, Buoyancy_timers);

// PURPOSE: Define the fractional error allowed on the matrix axes for orthonormality when processing low LOD buoyancy.
#define REJUVENATE_ERROR 0.01f

//
//
//
//
const float SECOND_LOD_DISTANCE_BOAT	= 150.0f;
//extern s32	numWaterDropOnScreen;

dev_float CHANCE_BOAT_SINKS_WHEN_WRECKED = 0.5f;
dev_float BOAT_SUBMERGE_SLEEP_GLOBAL_Z = -20.0f;	// Needs to be deeper than automobiles since boats can be much bigger
SearchLightInfo CBoat::ms_SearchLightInfo;

float CBoat::ms_InWaterFrictionMultiplier = 0.1f;
float CBoat::ms_DriveForceOutOfWaterFrictionMultiplier = 1.5f;
float CBoat::ms_OutOfWaterFrictionMultiplier = 3.0f;
float CBoat::ms_OutOfWaterTime = 10.0f;
u32 CBoat::ms_OutOfWaterShockingEventTime = 1000;

u32 CBoat::ms_nMaxPropOutOfWaterDuration = 20000;


PF_PAGE(GTA_BoatDynamics, "Boat Dynamics");
PF_GROUP(Boat_Dynamics);
PF_LINK(GTA_BoatDynamics, Boat_Dynamics);


PF_VALUE_FLOAT(BoatSteerInput, Boat_Dynamics);
PF_VALUE_FLOAT(BoatRudderForce, Boat_Dynamics);
PF_VALUE_FLOAT(BoatHeading, Boat_Dynamics);
PF_VALUE_FLOAT(BoatDriveInWater, Boat_Dynamics);
PF_VALUE_FLOAT(BoatSpeed, Boat_Dynamics);
PF_VALUE_FLOAT(BoatPolarSpeed, Boat_Dynamics);
PF_VALUE_FLOAT(BoatVelocityHeading, Boat_Dynamics);



#define INVALID_LOCKED_HEADING (-9999.99f)

#if USE_SIXAXIS_GESTURES
bank_float CBoat::MOTION_CONTROL_PITCH_MIN = -0.5f;
bank_float CBoat::MOTION_CONTROL_PITCH_MAX = 0.5f;
bank_float CBoat::MOTION_CONTROL_ROLL_MIN = -0.6f;
bank_float CBoat::MOTION_CONTROL_ROLL_MAX = 0.6f;
#endif // USE_SIXAXIS_GESTURES

//dev_float BOAT_PROP_RADIUS = 0.2f; //Now tuneable in handling.dat under PropRadius
dev_float BOAT_RUDDER_FORCE_CLAMP = 20.0f;

// Sin of angle from horizontal which -Z vector of boat must be less than for boat to count as capsized
dev_float BOAT_CAPSIZE_ANGLE_SIN			= 0.866f;
dev_float BOAT_CAPSIZE_XY_VEL_SQ_THRESHOLD = 12.0f;		// Boat must be traveling slower than this to count as capsized
dev_float BOAT_CAPSIZE_TIME				= 5.0f;			// Boat must be capsized for this long before player takes damage and drowning flag is set

dev_float BOAT_SINK_FORCEMULT_STEP = 0.002f;

static bank_float sfPitchFowradVelThreshold2 = 100.0f;
static bank_float sfPitchForce = 2.0f;
static bank_float sfDesiredPitchAngle = 0.20f;
static dev_float sfForceDesiredPitch = 0.15f;// if the angle difference between the current and desired pitch is greater than this do something to prevent it getting worse
static dev_float sfForceDesiredPitchInWater = 0.3f;
static dev_float sfDecreaseDesiredPitchForce = 0.1f;

dev_float sfPropSpinRate = 50.0f;

eHierarchyId BoatPropIds[] = {BOAT_STATIC_PROP, BOAT_STATIC_PROP2};

CBoatHandling::CBoatHandling()
{
	m_fPropellerAngle = 0.0f;
	m_fPropellerDepth = 0.0f;
	m_fRudderAngle = 0.0f;
	m_nNumPropellers = 0;

	m_vecPropDefomationOffset.Zero();
	m_vecPropDefomationOffset2.Zero();

	m_nBoatFlags.bBoatInWater = 1;
	m_nBoatFlags.bBoatEngineInWater = 1;
	m_nBoatFlags.nBoatWreckedAction = BWA_UNDECIDED;
	m_nBoatFlags.bPropellerSubmerged = 0;

	// boats always in water, so force full water check for them
	m_fOutOfWaterCounter = 0.0f;
	m_fOutOfWaterForAnimsCounter = 0.0f;
	m_fBoatCapsizedTimer = 0.0f;

	m_fPitchFwdInput = 0.0f;

	m_fAnchorBuoyancyLodDistance = -1.0f;
	m_nLowLodBuoyancyModeTimer = 0;

	m_nPlaceOnWaterTimeout = 0;

	m_nProwBounceTimer = fwTimer::GetTimeInMilliseconds();
	m_nIntervalToNextProwBump = fwRandom::GetRandomNumberInRange(1000, 3000);
	m_nBoatFlags.bRaisingProw = 0;

	m_nLastTimePropInWater = 0;

	m_isWedgedBetweenTwoThings = false;
}

void CBoatHandling::SetModelId( CVehicle* pVehicle, fwModelId modelId )
{
	if( !pVehicle->InheritsFromAmphibiousAutomobile() )
	{
		// Figure out how many propellers this model has.
		for( int nPropIndex = 0; nPropIndex < nMaxNumBoatPropellers; ++nPropIndex )
		{
			eHierarchyId nId = BoatPropIds[ nPropIndex ];
			if( pVehicle->GetBoneIndex(nId) > -1 && physicsVerifyf( m_nNumPropellers < nMaxNumBoatPropellers, "Out of room for boat propellers" ) )
			{
				// Found a valid propeller.
				m_propellersBoneIndicies[ m_nNumPropellers++ ] = pVehicle->GetBoneIndex(nId);
			}
		}
	}
	else
	{
		for(int nPropIndex = 0 ; nPropIndex < AMPHIBIOUS_AUTOMOBILE_NUM_PROPELLERS; nPropIndex++ )
		{
			eHierarchyId nId = (eHierarchyId)( AMPHIBIOUS_AUTOMOBILE_STATIC_PROP + nPropIndex );

			if( pVehicle->GetBoneIndex( nId ) > -1 && physicsVerifyf( m_nNumPropellers < AMPHIBIOUS_AUTOMOBILE_NUM_PROPELLERS,"Out of room for sub propellers" ) )
			{
				// Found a valid propeller
				m_propellersBoneIndicies[ m_nNumPropellers++ ] = pVehicle->GetBoneIndex(nId);
			}
		}
	}
}

void CBoatHandling::DoProcessControl( CVehicle* pVehicle )
{
	//update the rudder angle
	UpdateRudder( pVehicle );

	// Update the modeling of random forces to bounce the prow of the boat when moving at speed.
	Vector3 vRiverHitPos, vRiverHitNormal;
	if(!pVehicle->m_Buoyancy.GetCachedRiverBoundProbeResult( vRiverHitPos, vRiverHitNormal ) && pVehicle->GetStatus() == STATUS_PLAYER )
	{
		UpdateProw( pVehicle );
	}
}


//
//
//
void CBoatHandling::UpdateRudder( CVehicle* pVehicle )
{
	float fSpeedSteerMod = 1.0f;
	float fSpeeedSteerModMult = 0.5f * pVehicle->pHandling->m_fTractionBiasFront;

	if( pVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE || 
		pVehicle->GetVehicleType() == VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE )
	{
		static float s_amphibiousSteerModMult = 1.325f;
		fSpeeedSteerModMult = pVehicle->pHandling->m_fSteeringLock * s_amphibiousSteerModMult;
	}
	fSpeedSteerMod = DotProduct( pVehicle->GetVelocity(), VEC3V_TO_VECTOR3( pVehicle->GetTransform().GetB() ) );

	if( fSpeedSteerMod > 1.0f )
	{
		fSpeedSteerMod = rage::Powf( fSpeeedSteerModMult, fSpeedSteerMod );
	}
	else
	{
		fSpeedSteerMod = 1.0f;
	}

	// rudder angle 
	m_fRudderAngle = pVehicle->m_vehControls.m_steerAngle * fSpeedSteerMod;
}


void CBoatHandling::UpdateProw( CVehicle* pVehicle )
{
	// Randomly bump the prow out of the water when a boat is moving at speed to give a more interesting effect of motion.

	TUNE_FLOAT( BUOYANCY_MaxAccel, 27.0f, 0.0f, 1000.0f, 0.1f );
	TUNE_INT( BUOYANCY_MinIntervalToWaitForNextBump,  200, 0, 60000, 1 );
	TUNE_INT( BUOYANCY_MaxIntervalToWaitForNextBump, 1000, 0, 60000, 1 );
	TUNE_INT( BUOYANCY_DurationOfForceRamp, 450, 0, 60000, 1 );
	TUNE_FLOAT( BUOYANCY_SpeedForMaxEffect, 20.0f, 0.0f, 50.0f, 0.1f );
	TUNE_FLOAT( BUOYANCY_MinForceMagScaling, 0.2f, 0.0f, 1.0f, 0.1f );

	// See if the prow is in the water
	Vector3 vecProwSubmergeTestPos = pVehicle->GetBoundingBoxMax();
	vecProwSubmergeTestPos.x = 0.0f;
	static dev_float sfProwYMult = 0.5f;
	vecProwSubmergeTestPos.y *= sfProwYMult;
	float fBoundMinimumZ = vecProwSubmergeTestPos.z = pVehicle->GetBoundingBoxMin().z;

	vecProwSubmergeTestPos = pVehicle->TransformIntoWorldSpace(vecProwSubmergeTestPos);


	float fProwInWater = 0.0f;
	float fWaterLevel = -1000.0f;
	if( pVehicle->GetTransform().GetC().GetZf() > 0.0f &&
		CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers( vecProwSubmergeTestPos, &fWaterLevel, true ) )
	{
		if(fWaterLevel > vecProwSubmergeTestPos.z)
		{
			fProwInWater = rage::Min(1.0f, (fWaterLevel - vecProwSubmergeTestPos.z));
		}	
	}

	u32 nCurrentTime = fwTimer::GetTimeInMilliseconds();

	static dev_float sfForceProwHitDepth = 0.75f;// how much of the prow needs to go in the water before we force a hit.

	float fProwHitDepthRatio = fProwInWater / (-fBoundMinimumZ); //ratio of prow in the water.

	if( !m_nBoatFlags.bRaisingProw || ( fProwHitDepthRatio > sfForceProwHitDepth ) )
	{
		if( ( nCurrentTime - m_nProwBounceTimer > m_nIntervalToNextProwBump && 
			fProwHitDepthRatio > 0.0f ) || 
			( fProwHitDepthRatio > sfForceProwHitDepth ) )
		{
			static dev_float sfForceProwInWaterMin = 0.25f;
			fProwInWater = rage::Max( sfForceProwInWaterMin, fProwInWater );
			m_fProwForceMag = fwRandom::GetRandomNumberInRange( BUOYANCY_MinForceMagScaling, 1.0f ) * BUOYANCY_MaxAccel * pVehicle->GetMass() * fProwHitDepthRatio;

			// Scale by speed (must be going forwards).
			float fSpeed = pVehicle->GetVelocity().Dot( VEC3V_TO_VECTOR3( pVehicle->GetVehicleForwardDirection() ) );
			fSpeed = Clamp( fSpeed, 0.0f, BUOYANCY_SpeedForMaxEffect );
			float fSpeedScale = fSpeed / BUOYANCY_SpeedForMaxEffect;

			m_fProwForceMag *= fSpeedScale * fSpeedScale;

			// Now scale the effect by boat model.
			Assert( pVehicle->pHandling );
			Assert( pVehicle->pHandling->GetBoatHandlingData() );
			m_fProwForceMag *= pVehicle->pHandling->GetBoatHandlingData()->m_fProwRaiseMult;

			m_nIntervalToNextProwBump = BUOYANCY_MinIntervalToWaitForNextBump + 
				static_cast<u32>( ( BUOYANCY_MaxIntervalToWaitForNextBump - BUOYANCY_MinIntervalToWaitForNextBump ) * fwRandom::GetRandomNumberInRange( 0.0f, 1.0f ) );
			m_nProwBounceTimer = nCurrentTime;
			m_nBoatFlags.bRaisingProw = 1;
		}
	}

	if( m_nBoatFlags.bRaisingProw )
	{
		if( pVehicle->GetIsInWater() && 
			nCurrentTime - m_nProwBounceTimer < BUOYANCY_DurationOfForceRamp )
		{
			float fTimeFraction = 1.0f - ( static_cast<float>( nCurrentTime - m_nProwBounceTimer ) / static_cast<float>( BUOYANCY_DurationOfForceRamp ) );

			Vector3 vProwForce( 0.0f, 0.0f, m_fProwForceMag * fTimeFraction );
			pVehicle->ApplyForce( vProwForce, VEC3V_TO_VECTOR3( pVehicle->GetVehicleForwardDirection() ) * 0.8f * pVehicle->GetBoundRadius() );
		}
		else
		{
			m_nProwBounceTimer = nCurrentTime;
			m_nBoatFlags.bRaisingProw = 0;
		}
	}
}

void CBoatHandling::UpdatePropeller( CVehicle* pVehicle )
{
	// update the propeller angle
	if( pVehicle->m_Transmission.GetRevRatio() > 0.2f )
	{
		// sign changes if in fwd or reverse
		float fSpinSign = pVehicle->m_Transmission.GetGear() ? 1.0f : -1.0f;

		float fRevRatio = pVehicle->m_Transmission.GetRevRatio();
		if(pVehicle->InheritsFromAmphibiousAutomobile())
		{
			Assertf(pVehicle->GetSecondTransmission(), "Amphibious automobile does not have a second transmission for its boat engine!");

			fRevRatio = pVehicle->GetSecondTransmission()->GetRevRatio();
		}

		// Only update propeller angle for amphibious automobiles if they are in water
		if( !pVehicle->InheritsFromAmphibiousAutomobile() || static_cast<CAmphibiousAutomobile*>(pVehicle)->IsPropellerSubmerged() )
		{
			m_fPropellerAngle += fSpinSign * sfPropSpinRate * fRevRatio * fwTimer::GetTimeStep();
			m_fPropellerAngle = fwAngle::LimitRadianAngleFast( m_fPropellerAngle );
		}
	}

	CVehicleModelInfo* pModelInfo = pVehicle->GetVehicleModelInfo();
	s32 prop1BoneId = -1;
	s32 prop2BoneId = -1;

	if( pVehicle->InheritsFromBoat() )
	{
		pVehicle->SetComponentRotation( BOAT_RUDDER, ROT_AXIS_Z, -m_fRudderAngle, true);
		pVehicle->SetComponentRotation( BOAT_RUDDER2, ROT_AXIS_Z, -m_fRudderAngle, true);

		pVehicle->SetComponentRotation( BOAT_STATIC_PROP, ROT_AXIS_Y, m_fPropellerAngle,   true, &m_vecPropDefomationOffset );
		pVehicle->SetComponentRotation( BOAT_MOVING_PROP, ROT_AXIS_Y, -m_fPropellerAngle, true, &m_vecPropDefomationOffset );

		pVehicle->SetComponentRotation( BOAT_STATIC_PROP2, ROT_AXIS_Y, -m_fPropellerAngle, true, &m_vecPropDefomationOffset2 );
		pVehicle->SetComponentRotation( BOAT_MOVING_PROP2, ROT_AXIS_Y, m_fPropellerAngle, true, &m_vecPropDefomationOffset2 );

		prop1BoneId = pModelInfo->GetBoneIndex( BOAT_STATIC_PROP );
		prop2BoneId = pModelInfo->GetBoneIndex( BOAT_STATIC_PROP2 );
	}
	else
	{
		pVehicle->SetComponentRotation( AMPHIBIOUS_AUTOMOBILE_RUDDER, ROT_AXIS_Z, -m_fRudderAngle, true);
		pVehicle->SetComponentRotation( AMPHIBIOUS_AUTOMOBILE_RUDDER2, ROT_AXIS_Z, -m_fRudderAngle, true);

		pVehicle->SetComponentRotation( AMPHIBIOUS_AUTOMOBILE_STATIC_PROP, ROT_AXIS_Y, m_fPropellerAngle,   true, &m_vecPropDefomationOffset );
		pVehicle->SetComponentRotation( AMPHIBIOUS_AUTOMOBILE_MOVING_PROP, ROT_AXIS_Y, -m_fPropellerAngle, true, &m_vecPropDefomationOffset );

		pVehicle->SetComponentRotation( AMPHIBIOUS_AUTOMOBILE_STATIC_PROP2, ROT_AXIS_Y, -m_fPropellerAngle, true, &m_vecPropDefomationOffset2 );
		pVehicle->SetComponentRotation( AMPHIBIOUS_AUTOMOBILE_MOVING_PROP2, ROT_AXIS_Y, m_fPropellerAngle, true, &m_vecPropDefomationOffset2 );

		prop1BoneId = pModelInfo->GetBoneIndex( AMPHIBIOUS_AUTOMOBILE_STATIC_PROP );
		prop2BoneId = pModelInfo->GetBoneIndex( AMPHIBIOUS_AUTOMOBILE_STATIC_PROP2 );
	}

	// no visual effects if the boat is wrecked
	if( pVehicle->GetStatus() == STATUS_WRECKED )
	{
		return;
	}

	// check that we have a valid propeller matrix
	Matrix34 mMatProp1;
	 
	if( prop1BoneId > -1 )
	{
		pVehicle->GetGlobalMtx( prop1BoneId, mMatProp1 );
	}

	Matrix34 mMatProp2;
	if( prop2BoneId > -1 )
	{
		pVehicle->GetGlobalMtx( prop2BoneId, mMatProp2 );
	}

	if( prop1BoneId <= -1 && prop2BoneId <= -1 )
	{
		// neither propeller exists - return
		return;
	}

	// calculate the world position
	s32 numProps = 0;
	Vector3 propPos( 0.0f, 0.0f, 0.0f );
	if( prop1BoneId > -1 )
	{
		propPos += mMatProp1.d;
		numProps++;
	}
	if( prop2BoneId > -1 )
	{
		propPos += mMatProp2.d;
		numProps++;
	}

	Assert( numProps > 0 );
	propPos /= (float)numProps;

	// get the water height at this position
	m_fPropellerDepth = 0.0f;
	float waterZ;
	
	if( pVehicle->m_Buoyancy.GetWaterLevelIncludingRivers( propPos, &waterZ, false, POOL_DEPTH, REJECTIONABOVEWATER, NULL ) )
	{
		// check if the propeller is under water
		if( waterZ > propPos.z )
		{
			// set the propeller depth
			m_fPropellerDepth = waterZ - propPos.z;

			// bring the position back into boat space
			//			grcDebugDraw::Sphere(propPos, 1.0f, Color32(1.0f, 0.0f, 1.0f, 1.0f));

			// do the effect
			g_vfxWater.UpdatePtFxBoatPropeller( pVehicle, pVehicle->GetTransform().UnTransform( VECTOR3_TO_VEC3V( propPos ) ) );
		}
	}
}


ePhysicsResult CBoatHandling::ProcessPhysics( CVehicle* pVehicle, float fTimeStep)
{
	if(m_nBoatFlags.bBoatInWater)
	{
		m_fOutOfWaterCounter = 0.0f;
		m_fOutOfWaterForAnimsCounter = 0.0f;
		// Check for capsized
		if( pVehicle->GetTransform().GetC().GetZf() < -BOAT_CAPSIZE_ANGLE_SIN )
		{
			// Check velocity is low enough to count as capsized
			if( ( pVehicle->GetVelocity().x * pVehicle->GetVelocity().x + pVehicle->GetVelocity().y * pVehicle->GetVelocity().y ) < BOAT_CAPSIZE_XY_VEL_SQ_THRESHOLD )
			{
				if(m_fBoatCapsizedTimer < BOAT_CAPSIZE_TIME)
				{
					m_fBoatCapsizedTimer += fTimeStep;
				}
				else
				{
					pVehicle->m_nVehicleFlags.bIsDrowning = true;

					// GTAV - B*2826298 - Wreck the tug boat if it is capsized
					if( MI_BOAT_TUG.IsValid() && pVehicle->GetModelIndex() == MI_BOAT_TUG )
					{
						pVehicle->SetIsWrecked();
					}
				}
			}
			// Else if we're above move threshold don't increase timer but don't reset either
		}
		else
		{
			m_fBoatCapsizedTimer = 0.0f;
		}
	}
	else if( !pVehicle->IsUpsideDown() )
	{
		m_fOutOfWaterForAnimsCounter += fwTimer::GetTimeStep();
		u32 nTimePropOutOfWater = fwTimer::GetTimeInMilliseconds() - m_nLastTimePropInWater;
		if( !pVehicle->GetIsJetSki() || nTimePropOutOfWater > CBoat::ms_nMaxPropOutOfWaterDuration )
		{
			m_fOutOfWaterCounter += fwTimer::GetTimeStep();
			m_fBoatCapsizedTimer = 0.0f;
		}
	}

	// Throw an event when boats are being driven out of the water for too long so other peds will react.
	if( pVehicle->GetDriver() && pVehicle->GetDriver()->IsAPlayerPed() && CTimeHelpers::GetTimeSince(m_nLastTimePropInWater) > CBoat::ms_OutOfWaterShockingEventTime )
	{
		CEventShockingMadDriverExtreme shockingEvent( *pVehicle->GetDriver() );
		CShockingEventsManager::Add(shockingEvent);
	}

	// skip default physics update, gravity and air resistance applied here
	if( pVehicle->ProcessIsAsleep() )
	{
		if(!pVehicle->InheritsFromAmphibiousAutomobile())
		{
			pVehicle->m_Transmission.ProcessBoat(pVehicle, 0.0f, fTimeStep);
		}
		else
		{
			Assertf(pVehicle->GetSecondTransmission(), "Amphibious automobile does not have a second transmission for its boat engine!");

			pVehicle->GetSecondTransmission()->ProcessBoat(pVehicle, 0.0f, fTimeStep);
		}

		return PHYSICS_DONE;
	}

	// Work out where to apply the thrust force. If we have no propeller bones set up, just use a position based on the
	// bounding box. Otherwise, we just use the position if we have one propellor and a spatial average if there are more
	// than one.
	Vector3 vecThrustPos = pVehicle->GetBoundingBoxMin();
	vecThrustPos.x = 0.0f;
	vecThrustPos.z *= pVehicle->pHandling->GetBoatHandlingData()->m_fRudderOffsetForceZMult;
	vecThrustPos = pVehicle->TransformIntoWorldSpace(vecThrustPos);
	
	if( m_nNumPropellers > 0 )
	{
		vecThrustPos = VEC3_ZERO;
		for( int nPropIndex = 0; nPropIndex < m_nNumPropellers; ++nPropIndex )
		{
			Matrix34 matProp;

			if( m_propellersBoneIndicies[ nPropIndex ] > -1 )
			{
				pVehicle->GetGlobalMtx( m_propellersBoneIndicies[ nPropIndex ], matProp );
			}
			vecThrustPos += matProp.d;
		}
		vecThrustPos.Scale(1.0f/m_nNumPropellers);

#if __BANK
		// Show the computed average propeller position which will determine whether the prop is in the water.
		if( CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING )
		{
			grcDebugDraw::Sphere(vecThrustPos, 0.05f, Color_blue);
		}
#endif // __BANK
		Assert(vecThrustPos.Mag2() > 0.0f);
	}

	const CCollisionRecord* pCollisionRecord = pVehicle->GetFrameCollisionHistory()->GetMostSignificantCollisionRecord();


	// See if the prop / rudder is actually in the water. Offsets from the bone position using the value in handling.dat but
	// moves it back before applying the force offset below. We only move the prop down if we aren't hitting land.
	Vector3 vecPropellerSubmergeTestPos = vecThrustPos;
	if( !pCollisionRecord )
	{
		vecPropellerSubmergeTestPos += VEC3V_TO_VECTOR3( pVehicle->GetTransform().GetC() ) * pVehicle->pHandling->GetBoatHandlingData()->m_fRudderOffsetSubmerge;
	}
#if __BANK && __DEV
	if( CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING )
	{
		grcDebugDraw::Sphere( vecPropellerSubmergeTestPos, pVehicle->pHandling->GetBoatHandlingData()->m_fPropRadius, Color_green );
	}
#endif // __BANK && __DEV
	float fDriveInWater = 0.0f;
	float fWaterLevel = -1000.0f;
	const bool bUseWidestTolerance = pVehicle->m_Buoyancy.m_buoyancyFlags.bUseWidestRiverBoundTolerance;
	CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers( vecPropellerSubmergeTestPos, &fWaterLevel, true, -1.0f, bUseWidestTolerance );

	if( pVehicle->GetTransform().GetC().GetZf() > 0.0f )
	{
		float fPropRadius = pVehicle->pHandling->GetBoatHandlingData()->m_fPropRadius;
		if( pCollisionRecord )
		{
			static dev_float sfPropSizeReductionWhenHittingLand = 0.1f;
			fPropRadius = sfPropSizeReductionWhenHittingLand;
		}

		m_nBoatFlags.bPropellerSubmerged = 0;
		if( fWaterLevel > vecPropellerSubmergeTestPos.z - fPropRadius )
		{
			m_nBoatFlags.bPropellerSubmerged = 1;
			fDriveInWater = rage::Min( 1.0f, ( fWaterLevel - vecPropellerSubmergeTestPos.z + fPropRadius ) / fPropRadius );

#if __DEV
			PF_SET( BoatDriveInWater, fDriveInWater );
#endif

			m_nLastTimePropInWater = fwTimer::GetTimeInMilliseconds();
		}
		else if( pVehicle->GetIsJetSki() && pCollisionRecord )
		{
			// Since it's easy to get the jetski stuck on river banks and beaches, allow the propeller to exert a force
			// while out of the water but scale it down based on time out of water.
			u32 nTimePropOutOfWater = fwTimer::GetTimeInMilliseconds() - m_nLastTimePropInWater;

			fDriveInWater = rage::Clamp( (float)nTimePropOutOfWater / (float)CBoat::ms_nMaxPropOutOfWaterDuration, 0.0f, 1.0f );
			fDriveInWater = 1.0f - fDriveInWater;
			// Use a quartic to make the drive force drop off quicker.
			fDriveInWater *= fDriveInWater;
			fDriveInWater *= fDriveInWater;

#if __DEV
			PF_SET( BoatDriveInWater, fDriveInWater );
#endif
		}
	}

	phCollider* pCollider = pVehicle->GetCollider();
	if( pCollider )
	{
		if( pCollider->GetSleep() )
		{
			if( !m_nBoatFlags.bBoatInWater || pVehicle->GetTransform().GetC().GetZf() < 0.0f )
			{
				pCollider->GetSleep()->SetVelTolerance2(phSleep::GetDefaultVelTolerance2());
				pCollider->GetSleep()->SetAngVelTolerance2(phSleep::GetDefaultAngVelTolerance2());
			}
			else
			{
				pCollider->GetSleep()->SetVelTolerance2( 0.0001f );
				pCollider->GetSleep()->SetAngVelTolerance2( 0.0001f );
			}
		}
		//Increased gravity factor for boats, do this here as boats that have come off trailers wont have this set properly.
		static dev_float sfGravityFactor = 1.1f;
		static dev_float sfInAirGravityFactor = 1.5f;
		
		if( fDriveInWater > 0.0f )
		{
			pCollider->SetGravityFactor( sfGravityFactor );
		}
		else
		{
			if(!pVehicle->InheritsFromAmphibiousAutomobile())
			{
				pCollider->SetGravityFactor( sfInAirGravityFactor );
			}
			else
			{
				pCollider->SetGravityFactor(0.0f);
			}
		}
	}

	float fDriveForce = 0.0f;

	if(!pVehicle->InheritsFromAmphibiousAutomobile())
	{
		fDriveForce = pVehicle->m_Transmission.ProcessBoat( pVehicle, fDriveInWater, fTimeStep );
	}
	else
	{
		// If amphibious vehicle, process a separate transmission box as the main one is used for the car functionality
		Assertf(pVehicle->GetSecondTransmission(), "Amphibious automobile does not have a second transmission for its boat engine!");

		fDriveForce = pVehicle->GetSecondTransmission()->ProcessBoat(pVehicle, fDriveInWater, fTimeStep);
	}

	// add rudder offset after we've seen if it's in the water, so we can tune handling characteristics separately
	vecThrustPos.Add( VEC3V_TO_VECTOR3( pVehicle->GetTransform().GetC()) * pVehicle->pHandling->GetBoatHandlingData()->m_fRudderOffsetForce );

#if __BANK && __DEV
	// Show the final position at which the thrust will be applied.
	if( CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING )
	{
		grcDebugDraw::Sphere( vecThrustPos, 0.06f, Color_yellow );
	}
#endif // __BANK && __DEV

	const Vector3 vBoatPosition = VEC3V_TO_VECTOR3( pVehicle->GetTransform().GetPosition() );

	if( fDriveInWater > 0.0f )
	{
		// calculate components of rudder angle
		Matrix34 rudderMatrix = MAT34V_TO_MATRIX34( pVehicle->GetMatrix() );
		rudderMatrix.RotateLocalZ( -m_fRudderAngle );

		Vector3 vecThrust = VEC3V_TO_VECTOR3( pVehicle->GetTransform().GetB() );
		// only some boats have vectored thrust

		if( pVehicle->pHandling->DriveRearWheels() )
		{
			vecThrust = rudderMatrix.b;

			// want to flatten thrust as it points to the side so it doesn't kick the rear end of the boat up into the air.
			if(vecThrust.z > 0.0f)
			{
				const Vector3 vecRight = VEC3V_TO_VECTOR3( pVehicle->GetTransform().GetA() );
				float fSideThrust = DotProduct( vecRight, vecThrust );
				vecThrust.z -= rage::Max( 0.0f, fSideThrust * vecRight.z );
			}

			//If we have an impeller offset in the handling then setup an impeller force
			float fImpellerOffset = pVehicle->pHandling->GetBoatHandlingData()->m_fImpellerOffset;
			if( fImpellerOffset > 0.0f )
			{
				// Impeller sucking force
				Vector3 vecImpellerPos = pVehicle->GetBoundingBoxMin();
				vecImpellerPos.x = 0.0f;
				vecImpellerPos.y *= fImpellerOffset;
				vecImpellerPos.z = 0.0f;
				vecImpellerPos = pVehicle->TransformIntoWorldSpace(vecImpellerPos);

				float fImpellerInWater = 0.0f;
				float fWaterLevel = -1000.0f;
				if( pVehicle->GetTransform().GetC().GetZf() > 0.0f &&
					CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers( vecImpellerPos, &fWaterLevel, true, -1.0f, bUseWidestTolerance ) )
				{
					const float fImpellerRadius = 0.1f;

					if(fWaterLevel > vecPropellerSubmergeTestPos.z - fImpellerRadius)
					{
						fImpellerInWater = rage::Min( 1.0f, ( fWaterLevel - vecPropellerSubmergeTestPos.z + fImpellerRadius ) / fImpellerRadius );
					}	
				}

				Vector3 vecC = -VEC3V_TO_VECTOR3( pVehicle->GetTransform().GetC() );
				pVehicle->ApplyForce( vecC * fDriveForce * fImpellerInWater * pVehicle->pHandling->GetBoatHandlingData()->m_fImpellerForceMult * pVehicle->GetMass(), vecImpellerPos - vBoatPosition );
			}
		}

		pVehicle->ApplyForce( vecThrust * fDriveForce * pVehicle->GetMass(), vecThrustPos - vBoatPosition );

		// Compute relative velocity of the rudder through the water, taking flow into account if applicable/desired.
		Vector3 vecLocalSpeed;
		vecLocalSpeed = pVehicle->GetLocalSpeed( vecThrustPos, true );
		Vector2 vFlow(0.0f, 0.0f);
		// If we are in a river instead of a "water" block, the flow will be non-zero and defined by the river flow textures.
		Vector3 vRiverHitPos, vRiverHitNormal;
		if( pVehicle->m_Buoyancy.GetCachedRiverBoundProbeResult( vRiverHitPos, vRiverHitNormal ) )
		{
			River::GetRiverFlowFromPosition( RCC_VEC3V( vecThrustPos ), vFlow );
			vFlow.Scale( 1.0f / River::GetRiverPushForce() );
		}
		Vector3 vRiverVelocity(vFlow.x, vFlow.y, 0.0f);
		vRiverVelocity.Scale( pVehicle->m_Buoyancy.GetRiverFlowVelocityScaleFactor() );

		vecLocalSpeed -= Vector3(vRiverVelocity.x, vRiverVelocity.y, 0.0f);

		float fSideDot = vecLocalSpeed.Dot( rudderMatrix.a );
		float fRudderForce = fSideDot * fSideDot;
		fRudderForce = Min( fRudderForce, rage::Abs( fSideDot ) );
		fRudderForce = Min( fRudderForce, BOAT_RUDDER_FORCE_CLAMP );

		fRudderForce *= pVehicle->pHandling->GetBoatHandlingData()->m_fRudderForce;

		if( fSideDot > 0.0f )
		{
			fRudderForce *= -1.0f;
		}

#if __DEV
		PF_SET( BoatRudderForce, fRudderForce / BOAT_RUDDER_FORCE_CLAMP );
#endif

		pVehicle->ApplyForce( rudderMatrix.a * fRudderForce * pVehicle->GetMass(), vecThrustPos - vBoatPosition );

#if __BANK && __DEV
		// Show the final direction at which the thrust will be applied.
		if( CVehicle::ms_nVehicleDebug == VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug == VEH_DEBUG_HANDLING )
		{
			grcDebugDraw::Line(vecThrustPos, vecThrustPos + vecThrust * 5.00f, Color_yellow);
		}
#endif // __BANK && __DEV
	}
	else if( !m_nBoatFlags.bBoatInWater && !pVehicle->InheritsFromAmphibiousAutomobile() )
	{
		if( !pCollisionRecord )
		{

			static dev_bool sfAutoRoll = true;
			if( sfAutoRoll )
			{
				float currentRoll = pVehicle->GetTransform().GetRoll();
				static dev_float autoRollAmount = 2.5f;
				pVehicle->ApplyTorque( ((currentRoll) * autoRollAmount) * pVehicle->GetAngInertia().y * VEC3V_TO_VECTOR3( pVehicle->GetTransform().GetB() ) );
			}

			static dev_bool sfAutoHeading = false;
			if(sfAutoHeading)
			{
				static dev_float sfVelocityMax = 40.0f;
				float fVelocityMag = pVehicle->GetVelocity().Mag();

				float fVelocityScale = fVelocityMag/sfVelocityMax;
				const float fVelocityHeading = rage::Atan2f( -pVehicle->GetVelocity().x, pVehicle->GetVelocity().y );
				float fCurrentHeading = pVehicle->GetTransform().GetHeading();

				float fDeltaHeading = fCurrentHeading - fVelocityHeading;
				fDeltaHeading = fwAngle::LimitRadianAngleFast(fDeltaHeading);

				TUNE_FLOAT(BOAT_AUTO_HEAD_MULT, -5.0f, -300.0f, 300.0f, 1.00f);

				Vector3 vAutoHeadingTorque( ((fDeltaHeading) * BOAT_AUTO_HEAD_MULT * fVelocityScale) * pVehicle->GetAngInertia().z * VEC3V_TO_VECTOR3( pVehicle->GetTransform().GetC() ) );
				pVehicle->ApplyTorque( vAutoHeadingTorque );
			}
		}
	}

#if __DEV
	PF_SET( BoatHeading, fwAngle::LimitRadianAngle( pVehicle->GetTransform().GetHeading() ) );
	const float fVelocityHeading = rage::Atan2f( -pVehicle->GetVelocity().x, pVehicle->GetVelocity().y );
	PF_SET( BoatVelocityHeading, fVelocityHeading );
	PF_SET( BoatSteerInput, m_fRudderAngle );
	PF_SET( BoatSpeed, pVehicle->GetVelocity().Mag() / 30.0f );//normalise this a bit
	PF_SET( BoatPolarSpeed, pVehicle->GetAngVelocity().z );
#endif

	if (!pCollisionRecord)
	{
		static dev_bool sfAutoLevel = true;
		static dev_bool sfAutoLevelForAmphibiousAutomobiles = false;
		if(sfAutoLevel &&
			( !pVehicle->InheritsFromAmphibiousAutomobile() ||
			  sfAutoLevelForAmphibiousAutomobiles ) )
		{
			float fMaxPitch = m_nBoatFlags.bBoatInWater ? sfForceDesiredPitchInWater : sfForceDesiredPitch;
			float currentPitch = pVehicle->GetTransform().GetPitch();
			float fRoll = pVehicle->GetTransform().GetRoll();

			if( abs( fRoll ) < 0.7f && 
				( ( rage::Abs( m_fPitchFwdInput ) < 0.01f && !m_nBoatFlags.bBoatInWater ) || 
				  rage::Abs( currentPitch ) - sfDesiredPitchAngle > fMaxPitch ) )//if no stick is pressed or the desired pitch is miles off.
			{
				static dev_float autoLevelAmount = -15.0f;
				pVehicle->ApplyTorque( (rage::Sign( currentPitch ) * ( rage::Abs( currentPitch )-sfDesiredPitchAngle ) * autoLevelAmount ) * pVehicle->GetAngInertia().x * VEC3V_TO_VECTOR3( pVehicle->GetTransform().GetA() ) );
			}
		}
	}

	if( pVehicle->GetVelocity().Mag2() > sfPitchFowradVelThreshold2 )
	{
		// Don't want this to affect boats over a certain size by as much
		static dev_float MAX_ANG_INERTIA = 50000.0f;
		const float fAngInertiaMult = rage::Min( pVehicle->GetAngInertia().x, MAX_ANG_INERTIA );

		float currentPitch = pVehicle->GetTransform().GetPitch();
		float fDesiredPitchDelta = currentPitch-sfDesiredPitchAngle;
		float fPitchForce = sfPitchForce;
		float fPitchInput = rage::Clamp(m_fPitchFwdInput, -1.0f, 1.0f);

		if( ( MI_BOAT_TORO.IsValid() && pVehicle->GetModelId() == MI_BOAT_TORO ) ||
			( MI_BOAT_TORO2.IsValid() && pVehicle->GetModelId() == MI_BOAT_TORO2 ) )
		{
			static dev_float sfPitchMult = 0.5f;
			fPitchForce = sfPitchForce * sfPitchMult;
		}

		if( fDesiredPitchDelta < sfForceDesiredPitch || fPitchInput < 0.0f )//Don't let the player increase the pitch angle if we are already highly pitched
		{
			float fDesiredPitchMult = 1.0f;

			if( fDesiredPitchDelta > sfDecreaseDesiredPitchForce && fPitchInput > 0.0f )//reduce the pitch force once the player is nearing the limit of allowed pitch.
			{
				fDesiredPitchMult = 1.0f - ( ( fDesiredPitchDelta - sfDecreaseDesiredPitchForce ) / ( sfForceDesiredPitch - sfDecreaseDesiredPitchForce ) );
			}
			// Pitch the boat
			pVehicle->ApplyTorque(fPitchInput * VEC3V_TO_VECTOR3( pVehicle->GetTransform().GetA() ) * fAngInertiaMult * fPitchForce * fDesiredPitchMult );
		}
	}

	return PHYSICS_DONE;
}

//
//
// initialise data in constructor
//
CBoat::CBoat( const eEntityOwnedBy ownedBy, u32 popType ) : CVehicle( ownedBy, popType, VEHICLE_TYPE_BOAT )
{
	m_nPhysicalFlags.bIsInWater = TRUE;
	m_nVehicleFlags.bUseDeformation = true;
	m_nVehicleFlags.bCanMakeIntoDummyVehicle = false;
	m_nVehicleFlags.bInteriorLightOn = true; //by default boat interior light is on

	m_AnchorHelper.SetParent(this);
}//end of CBoat::CBoat()...


//
//
//
//
CBoat::~CBoat()
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnDestroyOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfDestroyCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );
}

void CBoat::SetModelId(fwModelId modelId)
{
	CVehicle::SetModelId( modelId );
	m_BoatHandling.SetModelId( this, modelId );
}

int CBoat::InitPhys()
{
	CVehicle::InitPhys();

	if( m_BoatHandling.GetWreckedAction() == BWA_UNDECIDED && 
		fwRandom::GetRandomNumberInRange(0.0f,1.0f) <= CHANCE_BOAT_SINKS_WHEN_WRECKED)
	{
		m_BoatHandling.SetWreckedAction( BWA_SINK );
	}
	else
	{
		m_BoatHandling.SetWreckedAction( BWA_FLOAT );
	}

	if( GetIsJetSki() )
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

void CBoat::AddPhysics()
{
	CPhysical::AddPhysics();

	// if we're adding physics and the anchor flag was set, we actually need to do the anchoring now
	if(m_AnchorHelper.IsAnchored())
		m_AnchorHelper.Anchor(true, true);
}

void CBoat::RemovePhysics()
{
	CPhysical::RemovePhysics();
}

//
void CBoat::UpdatePropeller()
{
	m_BoatHandling.UpdatePropeller( this );
}




//
//
//
//
//

float g_Boat_InteriorLight_ShutDownDistance;
float g_Boat_Lights_FadeLength;
#define oo_BoatLights_FadeLength (1.0f / g_Boat_Lights_FadeLength)
float g_Boat_corona_ShutDownDistance;
float g_Boat_corona_FadeLength;
#define oo_Corona_FadeLength (1.0f / g_Boat_corona_FadeLength)

float g_Boat_coronaProperties_size;
float g_Boat_coronaProperties_intensity;
float g_Boat_coronaProperties_zBias;

ePrerenderStatus CBoat::PreRender(const bool bIsVisibleInMainViewport)
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnPreRenderOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfPreRenderCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	if (!IsDummy() && !fwTimer::IsGamePaused())
	{
		UpdatePropeller();

		// vehicle damage fx
		GetVehicleDamage()->PreRender();
	}

	return CVehicle::PreRender(bIsVisibleInMainViewport);
}

void CBoat::DoLight(s32 boneID, ConfigBoatLightSettings &lightParam, float fade, CVehicleModelInfo* pModelInfo, bool doCorona, float coronaFade)
{
	s32		BoneId = pModelInfo->GetBoneIndex(boneID);

	if (BoneId>-1)
	{
		Matrix34 mBoneMtx;
		GetGlobalMtx(BoneId, mBoneMtx);

		Vector3 worldDir = VEC3V_TO_VECTOR3(GetTransform().Transform3x3(-Vec3V(V_Z_AXIS_WZERO)));
		Vector3 worldTan = VEC3V_TO_VECTOR3(GetTransform().Transform3x3(Vec3V(V_Y_AXIS_WZERO)));

		fwInteriorLocation interiorLocation = this->GetInteriorLocation();

		CLightSource light(
			LIGHT_TYPE_POINT, 
			LIGHTFLAG_VEHICLE | LIGHTFLAG_NO_SPECULAR, 
			mBoneMtx.d,
			VEC3V_TO_VECTOR3(lightParam.color),
			lightParam.intensity * fade, 
			LIGHT_ALWAYS_ON);
		light.SetDirTangent(worldDir, worldTan);
		light.SetRadius(lightParam.radius);
		light.SetInInterior(interiorLocation);
		Lights::AddSceneLight(light);
		
		
		if( doCorona )
		{
			g_coronas.Register(RCC_VEC3V(mBoneMtx.d),
								g_Boat_coronaProperties_size, 
								Color32(lightParam.color), 
								g_Boat_coronaProperties_intensity * fade * coronaFade, 
								g_Boat_coronaProperties_zBias, 
								Vec3V(V_ZERO),
								0.0f,
						   		0.0f,
								0.0f,
								CORONA_DONT_REFLECT);
		
		}
	}
}
void CBoat::PreRender2(const bool bIsVisibleInMainViewport)
{
	CVehicle::PreRender2(bIsVisibleInMainViewport);

	// check and update overrides to light status
	bool bForcedOn = UpdateVehicleLightOverrideStatus();
	
	bool bRemoteOn = IsNetworkClone() ? m_nVehicleFlags.bLightsOn : true; 

	if( bRemoteOn && (true == m_nVehicleFlags.bEngineOn || (true == bForcedOn) ) && (GetVehicleLightsStatus()) )
	{
		CVehicleModelInfo* pModelInfo=GetVehicleModelInfo();
		
		float cutoffDistanceTweak = CVehicle::GetLightsCutoffDistanceTweak();
		float camDist = rage::Sqrtf(CVfxHelper::GetDistSqrToCamera(GetTransform().GetPosition()));
		const float interiorLightsBrightness = rage::Clamp(((g_Boat_InteriorLight_ShutDownDistance + cutoffDistanceTweak) - camDist)*oo_BoatLights_FadeLength,0.0f,1.0f);
		
		if( interiorLightsBrightness )
		{
			extern ConfigBoatLightSettings g_BoatLights;
			
			DoLight(VEH_SIREN_1, g_BoatLights, interiorLightsBrightness, pModelInfo, false, 0.0f);
			DoLight(VEH_SIREN_2, g_BoatLights, interiorLightsBrightness, pModelInfo, false, 0.0f);
			
			if (m_nVehicleFlags.bInteriorLightOn)
			{
				float coronaFade = 1.0f - rage::Clamp(((g_Boat_corona_ShutDownDistance ) - camDist)*oo_Corona_FadeLength,0.0f,1.0f);
				DoLight(VEH_INTERIORLIGHT, g_BoatLights, interiorLightsBrightness, pModelInfo, cutoffDistanceTweak > 0.0f, coronaFade);
			}
		}
	}
	
	DoVehicleLights(LIGHTS_IGNORE_INTERIOR_LIGHT);

	g_vfxVehicle.ProcessLowLodBoatWake(this);
}


#if __BANK
void CBoat::RenderDebug() const
{
    CVehicle::RenderDebug();

    // draw stuck check
    if((CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING)
        && (GetStatus()==STATUS_PLAYER))
    {
        static u16 TEST_STUCK_CHECK = 10000;
        int nPrintLine = 20;
        if(GetVehicleDamage()->GetIsStuck(VEH_STUCK_JAMMED, TEST_STUCK_CHECK))
            grcDebugDraw::PrintToScreenCoors("Stuck - Jammed", 2, nPrintLine++);
        if(GetVehicleDamage()->GetIsStuck(VEH_STUCK_ON_SIDE, TEST_STUCK_CHECK))
            grcDebugDraw::PrintToScreenCoors("Stuck - OnSide", 2, nPrintLine++);
        if(GetVehicleDamage()->GetIsStuck(VEH_STUCK_ON_ROOF, TEST_STUCK_CHECK))
            grcDebugDraw::PrintToScreenCoors("Stuck - OnRoof", 2, nPrintLine++);
        if(GetVehicleDamage()->GetIsStuck(VEH_STUCK_HUNG_UP, TEST_STUCK_CHECK))
            grcDebugDraw::PrintToScreenCoors("Stuck - HungUp", 2, nPrintLine++);
    }
}
#endif // __BANK


/*
//
//
// render boat
//
void CBoat::Render(eRenderMode renderMode)
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnRenderOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfRenderCallingEntity(), this->GetPosition() );

	CVehicle::Render(renderMode);
}
*/


//
//
// messy, need tidied up; kinda copied & hacked from cAutomobile
//
dev_float BOAT_STEER_SMOOTH_RATE = 5.0f;
dev_float BOAT_PITCH_FWD_SMOOTH_RATE = 10.0f;


CSearchLight* CBoat::GetSearchLight()
{ 
	CVehicleWeaponMgr* pWeaponMgr = GetVehicleWeaponMgr();

	if(pWeaponMgr)
	{
		for(int i = 0; i < pWeaponMgr->GetNumVehicleWeapons(); i++)
		{
			CVehicleWeapon* pWeapon = pWeaponMgr->GetVehicleWeapon(i);
			if(pWeapon && pWeapon->GetType() == VGT_SEARCHLIGHT)
			{
				return static_cast<CSearchLight*>(pWeapon);
			}
		}
	}

	return NULL; 
}

//
void CBoat::DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep)
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnProcessControlOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfProcessControlCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );
#ifdef DEBUG

	for(s32 iSeat=0; iSeat<GetMaxSeats(); iSeat++)
	{
		if (GetPedInSeat(iSeat))
		{
			AssertEntityPointerValid_NotInWorld(GetPedInSeat(loop));
		}
	}
#endif
	//We don't record attachparent on replay so ignore this assert.
	Assert(!m_nVehicleFlags.bCanMakeIntoDummyVehicle || GetAttachParentVehicle() || GetDummyAttachmentParent() REPLAY_ONLY(|| CReplayMgr::IsEditModeActive()));

//	m_VehicleAudioEntity.Update(); //Process the vehicle audio entity too, so we get occlusion and radio.

//	CCarAI::ClearIndicatorsForCar(this);

	if (PopTypeGet() == POPTYPE_RANDOM_PARKED && GetDriver())
	{
		// The vehicle now has a driver so it is no longer just a placed vehicle (as it
		// can drive away now).
		PopTypeSet(POPTYPE_RANDOM_AMBIENT);
	}

	//vehicle tasks are processed in here
	if (!m_nVehicleFlags.bHasProcessedAIThisFrame)
	{
		ProcessIntelligence(fullUpdate, fFullUpdateTimeStep);
	}

	m_BoatHandling.DoProcessControl( this );

	GetVehicleDamage()->Process(fwTimer::GetTimeStep());

	ProcessSirenAndHorn(true);

	UpdateAirResistance();

	// If a boat is in low LOD anchor mode, it is physically inactive and we place it based on the water level sampled at
	// points around the edges of the bounding box.
	ProcessLowLodBuoyancy();

	CVehicle::DoProcessControl(fullUpdate, fFullUpdateTimeStep);
}// end of CBoat::ProcessControl()...

void CBoat::UpdateAirResistance()
{
	CPlayerInfo *pPlayer = GetDriver() ? GetDriver()->GetPlayerInfo() : NULL;
	
	float fAirResistance = m_fDragCoeff;
	if (pPlayer && pPlayer->m_fForceAirDragMult > 0.0f)
	{
		fAirResistance = m_fDragCoeff * pPlayer->m_fForceAirDragMult;
	}

	phArchetypeDamp* pArch = (phArchetypeDamp*)GetVehicleFragInst()->GetArchetype();

	Vector3 vecLinearV2Damping;
	vecLinearV2Damping.Set(fAirResistance, fAirResistance, 0.0f);
	TUNE_GROUP_FLOAT(VEHICLE_DAMPING, BOAT_V2_DAMP_MULT, 1.0f, 0.0f, 5.0f, 0.01f);

	if( pPlayer )
	{
		float fSlipStreamEffect = GetSlipStreamEffect();
		vecLinearV2Damping -= (vecLinearV2Damping * fSlipStreamEffect);
	}

	pArch->ActivateDamping(phArchetypeDamp::LINEAR_V2, vecLinearV2Damping * BOAT_V2_DAMP_MULT);

	// Note: for the dummy instance, changing these values is a bit dubious since
	// multiple instances actually point to the same phArchetypeDamp object. In practice,
	// there are no known problems. We might want to not even do it at all, but now
	// when we don't have to do this on each frame for all AI vehicles, it shouldn't matter much.
	phInstGta* pDummyInst = GetDummyInst();
	if(pDummyInst)
	{	
		phArchetypeDamp* pDummyArch = ((phArchetypeDamp*)pDummyInst->GetArchetype());
		pDummyArch->ActivateDamping(phArchetypeDamp::LINEAR_V2, vecLinearV2Damping);
	}
}


void CBoat::SetDamping()
{
   CBoatHandlingData* pBoatHandling = pHandling->GetBoatHandlingData();
    if(pBoatHandling == NULL)
        return;

    bool bIsArticulated = false;
    if(GetCollider())
    {
        bIsArticulated = GetCollider()->IsArticulated();
    }

    float fDampingMultiplier = 1.0f;

    static dev_float sfDampingMultiplier = 2.5f;
    if(GetDriver())
    {
        if( GetDriver()->GetPedModelInfo()->GetModelNameHash() == MI_PLAYERPED_PLAYER_ONE.GetName().GetHash() ) // Franklin
        {
            fDampingMultiplier = sfDampingMultiplier;
        }
        else if( GetDriver()->GetPedModelInfo()->GetModelNameHash() == MI_PLAYERPED_PLAYER_TWO.GetName().GetHash() ) // Trevor
        {
            fDampingMultiplier = sfDampingMultiplier;
        }

		if(GetDriver()->IsPlayer() && GetIsJetSki())
		{
			CControl *pControl = GetDriver()->GetControlFromPlayer();
			if(pControl)
			{
				float fStickY = pControl->GetVehicleSteeringUpDown().GetNorm();
				fStickY = fStickY + 1.0f;
				fStickY = rage::Clamp(fStickY, 0.0f, 1.0f);
				fDampingMultiplier *= CAmphibiousAutomobile::sm_fJetskiRiderResistanceMultMin + fStickY * (CAmphibiousAutomobile::sm_fJetskiRiderResistanceMultMax - CAmphibiousAutomobile::sm_fJetskiRiderResistanceMultMin);
			}
		}
    }

    float fDV = bIsArticulated ? CVehicleModelInfo::STD_BOAT_LINEAR_V_COEFF : 2.0f*CVehicleModelInfo::STD_BOAT_LINEAR_V_COEFF;
    fDV *= fDampingMultiplier;
    ((phArchetypeDamp*)GetVehicleFragInst()->GetArchetype())->ActivateDamping(phArchetypeDamp::LINEAR_V, Vector3(fDV, fDV, fDV));

    fDV = bIsArticulated ? 1.0f : 2.0f;
    fDV *= fDampingMultiplier;
    ((phArchetypeDamp*)GetVehicleFragInst()->GetArchetype())->ActivateDamping(phArchetypeDamp::ANGULAR_V, fDV * RCC_VECTOR3(pBoatHandling->m_vecTurnResistance));
    ((phArchetypeDamp*)GetVehicleFragInst()->GetArchetype())->ActivateDamping(phArchetypeDamp::ANGULAR_V2, fDV * Vector3(0.02f, CVehicleModelInfo::STD_BOAT_ANGULAR_V2_COEFF_Y, 0.02f));
}

#if __BANK
void CBoat::DisplayAngularOffsetForLowLod( CVehicle* pVehicle  )
{
	// Callback function to display the angle between world Z and the up vector for this boat. This number can
	// then be used in handling.dat as "LowLodAngOffset".

	Matrix34 boatMat = MAT34V_TO_MATRIX34(pVehicle->GetMatrix());
	float fRotationAngle = boatMat.c.Angle(ZAXIS);
	physicsDisplayf("[%s] LowLodAngOffset = %10.8f", pVehicle->GetModelName(), fRotationAngle);
}
void CBoat::DisplayDraughtOffsetForLowLod( CVehicle* pVehicle )
{
	// Callback function to display the vertical offset for this boat. This number can
	// then be used in handling.dat as "DraughtOffset".

	Matrix34 boatMat = MAT34V_TO_MATRIX34(pVehicle->GetMatrix());
	// We assume the water has been made flat as part of this debug process.
	float fWaterHeightAtPos = 0.0f;
	CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(boatMat.d, &fWaterHeightAtPos);
	float fDraughtOffset = boatMat.d.z - fWaterHeightAtPos;
	physicsDisplayf("[%s] LowLodDraughtOffset = %10.8f", pVehicle->GetModelName(), fDraughtOffset);
}
#endif // __BANK

void CBoat::UpdateBuoyancyLodMode( CVehicle* pVehicle, CAnchorHelper& anchorHelper )
{
	PF_START(ProcessLowLodBuoyancyTimer);

	CBoatHandling* pBoatHandling = pVehicle->GetBoatHandling();

	if(anchorHelper.IsAnchored())
	{
		float fBuoyancyLodDist = pVehicle->m_Buoyancy.GetBoatAnchorLodDistance();
		// Check if this boat overrides the LOD distance.
		if( pBoatHandling && pBoatHandling->OverridesLowLodBuoyancyDistance() )
		{
			fBuoyancyLodDist = pBoatHandling->GetLowLodBuoyancyDistance();
		}

		CPed* pPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
		if(Verifyf(pPlayer, "[CBoat::UpdateBuoyancyLodMode()] No Player ped found."))
		{
			Vector3 vPlayerTestPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
			float fPlayerBoundRadius = pPlayer->GetBoundRadius();
			if(pPlayer->GetIsInVehicle())
			{
				CVehicle* pPlayerVehicle = pPlayer->GetMyVehicle();
				vPlayerTestPos = pPlayerVehicle->GetBoundCentre();
				fPlayerBoundRadius = pPlayerVehicle->GetBoundRadius();
			}
			float fLodDistSq = fBuoyancyLodDist + pVehicle->GetBoundRadius() + fPlayerBoundRadius;
			fLodDistSq *= fLodDistSq;

			float fDistanceSqToPlayer = ( pVehicle->GetBoundCentre() - vPlayerTestPos ).Mag2();
			
			// If a multi-player game is running we need to consider the closest player of any type.
			if(NetworkInterface::IsGameInProgress())
			{
				// Get the network entity for this boat.
				CNetObjEntity* pNetObjEntity = static_cast<CNetObjEntity*>(pVehicle->GetNetworkObject());
				if(pNetObjEntity)
				{
					// Compute the distance squared to nearest multi-player player.
					fDistanceSqToPlayer = pNetObjEntity->GetDistanceToNearestPlayerSquared();
				}
			}

			if( fDistanceSqToPlayer > fLodDistSq || anchorHelper.ShouldAlwaysUseLowLodMode() )
			{
				if( !anchorHelper.UsingLowLodMode() )
				{
					// Switch to low LOD buoyancy mode - clean up the anchor constraint and set the flag so the matrix gets
					// moved with the water surface.
					phInst* pPhysInst = pVehicle->GetCurrentPhysicsInst();

					// Set the low-lod mode before deactivating the object, otherwise it won't be allowed to deactivate
					anchorHelper.SetLowLodMode( true );
					if( pPhysInst )
					{
						if( pPhysInst->IsInLevel() && CPhysics::GetLevel()->IsActive( pPhysInst->GetLevelIndex() ) )
						{
							pVehicle->RemoveConstraints( pPhysInst );
							CPhysics::GetSimulator()->DeactivateObject( pPhysInst->GetLevelIndex() );
						}

						// Don't want this boat activating unless it's due to us turning off low LOD mode.
						pPhysInst->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, true);
					}

					// Cache the vertical offset of the boat with respect to the water height at the entity position
					// so we get the draught right.
					Matrix34 boatMat = MAT34V_TO_MATRIX34( pVehicle->GetMatrix() );
					float fWaterZAtEntPos;
					pVehicle->m_Buoyancy.GetWaterLevelIncludingRivers( boatMat.d, &fWaterZAtEntPos, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL, pVehicle );
					pVehicle->m_Buoyancy.SetLowLodDraughtOffset( boatMat.d.z - fWaterZAtEntPos );

					if( pBoatHandling )
					{
						pBoatHandling->SetLowLodBuoyancyModeTimer( fwTimer::GetTimeInMilliseconds() );
					}
				}
			}
			else
			{
				if(anchorHelper.UsingLowLodMode())
				{
					// If we are currently in low LOD anchor mode, we need to create constraints and toggle the low LOD
					// flag off.
					phInst* pPhysInst = pVehicle->GetCurrentPhysicsInst();
					if(pPhysInst)
					{
						// Now this boat can activate again.
						pPhysInst->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, false);

						// ... and activate it since we no longer check for sleep overrides once a frame.
						if(pPhysInst->IsInLevel())
							CPhysics::GetSimulator()->ActivateObject(pPhysInst->GetLevelIndex());

						anchorHelper.CreateAnchorConstraints();
					}

					// Update the "is in water" flag as would be done in CBuoyancy::Process().
					pVehicle->SetIsInWater(true);

					anchorHelper.SetLowLodMode(false);
				}
			}
		}
	}
	else
	{
		// Shouldn't be using low LOD anchor mode if this boat isn't anchored.
		anchorHelper.SetLowLodMode(false);
	}

	PF_STOP(ProcessLowLodBuoyancyTimer);
	PF_SETTIMER(ProcessBuoyancyTotal, PF_READ_TIME(ProcessBuoyancy) + PF_READ_TIME(ProcessLowLodBuoyancyTimer));
}

void CBoat::RejuvenateMatrixIfNecessary(Matrix34& matrix)
{
	// See how far away from normal and orthogonal the matrix gets and renormalise it if necessary.
	if(!matrix.IsOrthonormal(REJUVENATE_ERROR))
	{
#if __ASSERT
		Displayf("[CBoat::ProcessLowLodBuoyancy] Non orthonormal matrix detected. Rejuvenating...");
#endif // __ASSERT

		if(matrix.a.IsNonZero() && matrix.b.IsNonZero() && matrix.c.IsNonZero())
		{
			matrix.b.Normalize();
			matrix.c.Cross(matrix.a, matrix.b);
			matrix.c.Normalize();
			matrix.a.Cross(matrix.b, matrix.c);
		}
		else
		{
			// Protection against very bad matrices...
			matrix.Identity3x3();
		}
	}
}

bool CBoat::ProcessLowLodBuoyancy()
{
	// If a boat is in low LOD anchor mode, it is physically inactive and we place it based on the water level sampled at
	// points around the edges of the bounding box.

	PF_START(ProcessLowLodBuoyancyTimer);

	bool bFoundWater = false;

	if(m_AnchorHelper.UsingLowLodMode())
	{
		const Vector3 vOriginalPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

		// Indicate this boat is in low LOD anchor mode for debugging.
#if __BANK
		if(CBuoyancy::ms_bIndicateLowLODBuoyancy)
		{
			grcDebugDraw::Sphere(vOriginalPosition + Vector3(0.0f, 0.0f, 2.0f), 1.0f, Color_yellow);
		}
#endif // __BANK

		float fWaterZAtEntPos;
		WaterTestResultType waterTestResult = m_Buoyancy.GetWaterLevelIncludingRivers(vOriginalPosition, &fWaterZAtEntPos, true, POOL_DEPTH, REJECTIONABOVEWATER, NULL, this);
		bFoundWater = waterTestResult > WATERTEST_TYPE_NONE;
		if(bFoundWater)
		{
			// We know the optimum stable height offset for boats so if this boat isn't at that offset yet, LERP it
			// there over a few frames.
			float fLowLodDraughtOffset = m_Buoyancy.GetLowLodDraughtOffset();
			float fDifferenceToIdeal = pHandling->GetBoatHandlingData()->m_fLowLodDraughtOffset - fLowLodDraughtOffset;
			if(fabs(fDifferenceToIdeal) > 0.001f)
			{
				dev_float sfStabiliseFactor = 0.1f;
				fLowLodDraughtOffset += sfStabiliseFactor * fDifferenceToIdeal;
				m_Buoyancy.SetLowLodDraughtOffset(fLowLodDraughtOffset);
			}

			// Now we actually move the boat to simulate buoyancy physics.
			Matrix34 boatMat = MAT34V_TO_MATRIX34(GetMatrix());
			Vector3 vPlaneNormal;
			ComputeLowLodBuoyancyPlaneNormal(boatMat, vPlaneNormal);

			// Apply correction for the orientation of the boat when in equilibrium.
			float fCorrectionRotAngle = pHandling->GetBoatHandlingData()->m_fLowLodAngOffset;
			if(fabs(fCorrectionRotAngle) > SMALL_FLOAT)
			{
				boatMat.UnTransform3x3(vPlaneNormal);
				vPlaneNormal.RotateX(fCorrectionRotAngle);
				boatMat.Transform3x3(vPlaneNormal);
			}

			// Figure out how much to rotate the boat's matrix by to match the plane's orientation.
			Vector3 vRotationAxis;
			vRotationAxis.Cross(boatMat.c, vPlaneNormal);
			vRotationAxis.NormalizeFast();
			float fRotationAngle = boatMat.c.Angle(vPlaneNormal);

			// If we are far away from the equilibrium orientation for the boat (because of big waves or other external
			// forces on the boat) we limit the amount we move towards the ideal position each frame. Only do this for
			// the first few seconds after switching to low LOD mode or it damps the motion when following the waves.
			const u32 knTimeToLerpAfterSwitchToLowLod = 2500;
			u32 nTimeSpentLerping = fwTimer::GetTimeInMilliseconds() - m_BoatHandling.GetLowLodBuoyancyModeTimer();
			if(nTimeSpentLerping < knTimeToLerpAfterSwitchToLowLod)
			{
				float fX = float(nTimeSpentLerping)/float(knTimeToLerpAfterSwitchToLowLod);
				float kfStabiliseFactor = rage::Min(fX*fX, 1.0f);
				fRotationAngle *= kfStabiliseFactor;
			}
			if(fabs(fRotationAngle) > SMALL_FLOAT && vRotationAxis.IsNonZero() && vRotationAxis.FiniteElements())
			{
				boatMat.RotateUnitAxis(vRotationAxis, fRotationAngle);
			}

			// If the boat rotates about its up-axis too much it can end up violating the physical constraints when
			// switching back to full physical buoyancy mode. We know the positions of the prow and stern constraints
			// so apply a LERP back towards this orientation if we start to deviate too much.
			float fConstraintYawDiff = m_AnchorHelper.GetYawOffsetFromConstrainedOrientation();
			const float fLerpBackTowardsConstraintYawFactor = 1.0f;
			fConstraintYawDiff *= fLerpBackTowardsConstraintYawFactor;
			if(fabs(fConstraintYawDiff) > SMALL_FLOAT)
			{
				boatMat.RotateZ(fConstraintYawDiff);
			}

			RejuvenateMatrixIfNecessary(boatMat);
			SetMatrix(boatMat);

			SetPosition(Vector3(vOriginalPosition.x, vOriginalPosition.y, fWaterZAtEntPos+fLowLodDraughtOffset));

			// Update the "is in water" flag as would be done in CBuoyancy::Process().
			SetIsInWater(true);
		}
	}

	PF_STOP(ProcessLowLodBuoyancyTimer);
	PF_SETTIMER(ProcessBuoyancyTotal, PF_READ_TIME(ProcessBuoyancy) + PF_READ_TIME(ProcessLowLodBuoyancyTimer));

	return bFoundWater;
}

void CBoat::ComputeLowLodBuoyancyPlaneNormal(const Matrix34& boatMat, Vector3& vPlaneNormal)
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

ePhysicsResult CBoat::ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice)
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnProcessPhysicsOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfProcessPhysicsCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );

	// Don't reactivate if we are in low LOD anchor mode (this would happen if we call ProcessIsAsleep() while in the water).
	UpdateBuoyancyLodMode( this, GetAnchorHelper() );
	if( GetAnchorHelper().UsingLowLodMode() )
	{
		return PHYSICS_DONE;
	}

	if( m_vehicleAiLod.IsLodFlagSet( CVehicleAILod::AL_LodSuperDummy ) )
	{
		return PHYSICS_DONE;
	}

	//Attempt to defer physics if we are a clone
	if( ms_enablePatrolBoatSingleTimeSliceFix && IsNetworkClone() && bCanPostpone )
	{
		if(	(MI_BOAT_PATROLBOAT.IsValid() && GetModelIndex() == MI_BOAT_PATROLBOAT) ||
			(MI_BOAT_DINGHY5.IsValid() && GetModelIndex() == MI_BOAT_DINGHY5)	)
		{
			return PHYSICS_POSTPONE;
		}
	}

	CVehicle::ProcessPhysics(fTimeStep, false, nTimeSlice);

	if( GetCurrentPhysicsInst()==NULL || !GetCurrentPhysicsInst()->IsInLevel() )
	{
		return PHYSICS_DONE;
	}

	for( int i = 0; i < m_pVehicleGadgets.size(); i++)
	{
		m_pVehicleGadgets[i]->ProcessPhysics(this,fTimeStep,nTimeSlice);
	}

	SetDamping();

	bool bSkipBuoyancy = false;

	// if network has re-spotted this car and we don't want it to collide with other network vehicles for a while
	ProcessRespotCounter(fTimeStep);

	// If boat is wrecked then we might want to sink it
	if(GetStatus()==STATUS_WRECKED && m_BoatHandling.GetWreckedAction() == BWA_SINK)
	{
		// If we are anchored then release anchor
		if(GetAnchorHelper().IsAnchored())
			GetAnchorHelper().Anchor(false);

		if(m_Buoyancy.m_fForceMult > 0.0f)
		{
			m_Buoyancy.m_fForceMult -= BOAT_SINK_FORCEMULT_STEP;
		}
		else
		{
			m_Buoyancy.m_fForceMult = 0.0f;
		}

		// Check for boat well below water surface and then fix if below 10m
		if(GetTransform().GetPosition().GetZf() < BOAT_SUBMERGE_SLEEP_GLOBAL_Z)
		{
			DeActivatePhysics();
			bSkipBuoyancy = true;
		}
	}

	float fBuoyancyTimeStep = fTimeStep;
	//Only apply Buoyancy on the last update for the articulated patrolboat and dinghy5 as the articulated body is not behaving correctly if I apply on each timeslice.
	if( IsNetworkClone() )
	{
		if(	(MI_BOAT_PATROLBOAT.IsValid() && GetModelIndex() == MI_BOAT_PATROLBOAT) ||
			(MI_BOAT_DINGHY5.IsValid() && GetModelIndex() == MI_BOAT_DINGHY5)	)
		{
			if(!CPhysics::GetIsLastTimeSlice(nTimeSlice))
			{
				bSkipBuoyancy = true;
			}
			else
			{
				if(ms_enablePatrolBoatSingleTimeSliceFix)
				{
					//Double the buoyancy for a cloned patrol boat when we only have 1 time slice. This is a workaround for an articulated body issue shown up by network blend setting a velocity directly.
					fBuoyancyTimeStep = fTimeStep * (CPhysics::GetNumTimeSlices() == 1 ? 2.0f : CPhysics::GetNumTimeSlices());
				}
				else
				{
					fBuoyancyTimeStep = fTimeStep * CPhysics::GetNumTimeSlices();
				}

			}
		}

	}

	// do buoyancy all the time for boats, will cause them to wake up
	// but allow to go to sleep if stranded on a beach
	if(!bSkipBuoyancy)
	{
		m_BoatHandling.SetInWater( m_Buoyancy.Process(this, fBuoyancyTimeStep, false, CPhysics::GetIsLastTimeSlice(nTimeSlice)) || GetAnchorHelper().UsingLowLodMode() );
		SetIsInWater(m_BoatHandling.IsInWater());
	}

	m_nVehicleFlags.bIsDrowning = false;
	m_BoatHandling.ProcessPhysics( this, fTimeStep );

	for( int i = 0; i < m_pVehicleGadgets.size(); i++)
	{
		m_pVehicleGadgets[i]->ProcessPhysics( this, fTimeStep, nTimeSlice );
	}
	if(m_pVehicleWeaponMgr)
	{
		m_pVehicleWeaponMgr->ProcessPhysics( this, fTimeStep, nTimeSlice );
	}

	// Increase the Solver ang inertia so when the chassis hits the ground we dont spin.
	if( GetCollider() && GetIsJetSki() ) 
	{
		// GTAV - B*1802890 - If the jetski is anchored this can make it go a bit weird.
		if( !GetAnchorHelper().IsAnchored() )
		{
			// B*1807324: Don't allow the inverse angular inertia to be overriden if the jetski is trapped between two solid things else the solver has a hard time determining a non-violent solution.
			GetCollider()->UseSolverInvAngInertiaResetOverride( ( (GetDriver() && m_BoatHandling.IsInWater() && !m_BoatHandling.IsWedgedBetweenTwoThings() ) ? true : false) );
		}

		// This will be flagged again if the trapped conditions persist to the next frame.
		m_BoatHandling.SetIsWedgedBetweenTwoThings( false );
	}

	return PHYSICS_DONE;
}

ePhysicsResult CBoat::ProcessPhysics2( float fTimeStep, int UNUSED_PARAM( nTimeSlice ) )
{
	for( int i = 0; i < m_pVehicleGadgets.size(); i++)
	{
		m_pVehicleGadgets[ i ]->ProcessPhysics2(this,fTimeStep);
	}

	return PHYSICS_DONE;
}


void CBoat::UpdateEntityFromPhysics(phInst *pInst, int nLoop)
{
	CVehicle::UpdateEntityFromPhysics(pInst, nLoop);
}

void CBoat::SetupMissionState()
{
	bool bPopTypeMision = PopTypeIsMission();

	CVehicle::SetupMissionState();

	// some mission state should not be set when random vehicles are becoming mission vehicles.  The poptype will already be set as a mission
	// type if the vehicle has been created purely to be a mission vehicle. 
	if (bPopTypeMision)
	{
		SetRandomEngineTemperature(0.5f);
		SwitchEngineOn(true);	
	}
}

void CBoat::UpdatePhysicsFromEntity(bool bWarp)
{
	bool bWasAnchored = GetAnchorHelper().IsAnchored();
	if(bWasAnchored && !GetAnchorHelper().UsingLowLodMode())
		GetAnchorHelper().Anchor(false);

	CVehicle::UpdatePhysicsFromEntity(bWarp);

	if(bWasAnchored && !GetAnchorHelper().UsingLowLodMode())
		GetAnchorHelper().Anchor(true);
}

//
void CBoat::ProcessPreComputeImpacts(phContactIterator impacts)
{
	impacts.Reset();

	bool processingAudioImpacts = false;
	bool resetSpeedBoostObjectID = true; 
	if(!impacts.AtEnd())
	{
		processingAudioImpacts = m_VehicleAudioEntity->GetCollisionAudio().StartProcessImpacts();
	}

	const Mat34V& boatMatrix = GetMatrixRef();
	const Vec3V boatUp = boatMatrix.GetCol2();

	// Compute a friction multiplier based on how long we've been out of the water. 
	float outOfWaterScale = Min( m_BoatHandling.GetOutOfWaterTime() / ms_OutOfWaterTime, 1.0f );
	float frictionMultiplier = Lerp( outOfWaterScale, ms_InWaterFrictionMultiplier, ms_OutOfWaterFrictionMultiplier );

	// If we are allowing the propeller to provide a drive force even though we are out of water, set a unique
	// friction multiplier during this period.
	u32 nTimePropOutOfWater = fwTimer::GetTimeInMilliseconds() - m_BoatHandling.GetLastTimePropInWater();
	if(GetIsJetSki() && nTimePropOutOfWater < ms_nMaxPropOutOfWaterDuration)
	{
		frictionMultiplier = ms_DriveForceOutOfWaterFrictionMultiplier;
	}

	int count = impacts.CountElements();

	phCachedContactIteratorElement *buffer = Alloca( phCachedContactIteratorElement, count );

	impacts.Reset();

	phCachedContactIterator cachedImpacts;
	cachedImpacts.InitFromIterator( impacts, buffer, count );

	while(!cachedImpacts.AtEnd())
	{
		phInst* otherInstance = cachedImpacts.GetOtherInstance();
		CEntity* pOtherEntity = otherInstance ? (CEntity*)otherInstance->GetUserData() : NULL;

		if( pOtherEntity &&
			ProcessBoostPadImpact( cachedImpacts, pOtherEntity ) )
		{
			cachedImpacts.DisableImpact();
			resetSpeedBoostObjectID = false;
		}
		++cachedImpacts;
	}

	impacts.Reset();

	Vector3 lastSideImpact = VEC3_ZERO;
	while(!impacts.AtEnd())
	{
		// Process audio for one impact
		if(processingAudioImpacts)
		{
			m_VehicleAudioEntity->GetCollisionAudio().ProcessImpactBoat(impacts);
		}

		Vector3 vecNormal;
		impacts.GetMyNormal(vecNormal);
		phInst* pOtherInst = impacts.GetOtherInstance();

		// This is where we can hook up event generation for boats colliding against foliage bounds. Currently,
		// we just ignore these collisions.
		if(pOtherInst && pOtherInst->IsInLevel()
			&& CPhysics::GetLevel()->GetInstanceTypeFlags(pOtherInst->GetLevelIndex())&ArchetypeFlags::GTA_FOLIAGE_TYPE)
		{
#if DEBUG_DRAW
			if(CPhysics::ms_bShowFoliageImpactsVehicles)
			{
				grcDebugDraw::Sphere(impacts.GetMyPosition(), 0.2f, Color_red);
				grcDebugDraw::Sphere(impacts.GetOtherPosition(), 0.05f, Color_grey, false);
				grcDebugDraw::Line(impacts.GetMyPosition(), impacts.GetOtherPosition(), Color_grey);
			}
#endif // DEBUG_DRAW

			// Now that we've noted the collision, disable the impact.
			impacts.DisableImpact();
			++impacts;
			continue;
		}

		// Only modify the friction if the bottom of the boat is being hit
		if(IsGreaterThanAll(Dot(RCC_VEC3V(vecNormal),boatUp),ScalarV(V_HALF)))
		{
			// Contacts don't get their friction recomputed each frame so we need to do that before multiplying it
			float materialFriction,elasticity;
			PGTAMATERIALMGR->FindCombinedFrictionAndElasticity(impacts.GetMyMaterialId(),impacts.GetOtherMaterialId(),materialFriction,elasticity);
			impacts.SetFriction(materialFriction*frictionMultiplier);
		}

		if(GetIsJetSki())
		{
			// B*1807324: Keep track of the last impact that was angled into the side of the jetski. 
			// If another impact is found that is sufficiently opposing the last side impact found, the jetski is probably wedged between two bits of collision so we need to prevent the inverse angular inertia from being tweaked else the solver cannot provide a good solution.
			float impactNormalOnX = vecNormal.Dot(VEC3V_TO_VECTOR3(GetMatrix().a()));
		
			// Don't count glancing hits to the jetski as side impacts.
			if(impactNormalOnX > 0.3f || impactNormalOnX < -0.3f)
			{
				float impactOnLastSideImpact = vecNormal.Dot(lastSideImpact);
				if(impactOnLastSideImpact < 0.0f)
				{
					// This gets reset in ProcessPhysics.
					m_BoatHandling.SetIsWedgedBetweenTwoThings( true );
				}
				else
				{
					lastSideImpact = vecNormal;
				}
			}
		}

		CEntity* pOtherEntity = pOtherInst ? (CEntity*)pOtherInst->GetUserData() : NULL;

		// B*2813022: If a vehicle is on top of the tug boat reduce the mass scales so it doesn't affect it too much
		if( pOtherEntity && pOtherEntity->GetIsTypeVehicle() &&
			GetModelIndex() == MI_BOAT_TUG && pOtherEntity->GetModelIndex() != MI_BOAT_TUG &&
			GetMass() > static_cast<CVehicle*>( pOtherEntity )->GetMass() )
		{				
			if(IsTrue(Dot(impacts.GetOtherPosition() - GetTransform().GetPosition(), RCC_VEC3V(ZAXIS)) > ScalarV(0.5f)))			
			{
				impacts.SetMassInvScales(0.02f, 1.0f); 
			}
		}


		impacts++;
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



///////////////////////////////////////////////////////////////////////////////////
//
// FUNCTION : BlowUpCar
// PURPOSE :  Does everything needed to destroy a boat
//
///////////////////////////////////////////////////////////////////////////////////

void CBoat::BlowUpCar(CEntity *pCulprit, bool bInACutscene, bool bAddExplosion, bool ASSERT_ONLY(bNetCall), u32 weaponHash, bool bDelayedExplosion)
{	
#if __DEV
	if (gbStopVehiclesExploding)
	{
		return;
	}
#endif

#if __BANK
	// Adding this here because this function doesn't call the base class version CVehicle::BlowUpCar().
	if(NetworkInterface::IsNetworkOpen())
	{
		bool prevDebugLevel = CVehicle::ms_bDebugVehicleIsDriveableFail;
		CVehicle::ms_bDebugVehicleIsDriveableFail = true;
		GetVehicleDamage()->GetIsDriveable(true, true, true);
		CVehicle::ms_bDebugVehicleIsDriveableFail = prevDebugLevel;

		Displayf("[CBoat::BlowUpCar()]");
		sysStack::PrintStackTrace();
	}
#endif // __BANK

	if (GetStatus() == STATUS_WRECKED)
	{
		return;	// Don't blow cars up a 2nd time
	}

	// Make sure we turn remove any anchor when blowing the boat up. This also makes sure we switch to full physics if in low LOD anchor mode.
	if( GetAnchorHelper().IsAnchored() )
	{
		GetAnchorHelper().Anchor(false);
		// Let's have any anchored boats sink by default since we don't want to end up paying full physics when they were potentially in
		// low LOD buoyancy mode.
		m_BoatHandling.SetWreckedAction( BWA_SINK );
	}

	// we can't blow up boats controlled by another machine
    // but we still have to change their status to wrecked
    // so the boat doesn't blow up if we take control of an
    // already blown up car
	if (IsNetworkClone())
    {
		Assertf(bNetCall, "Trying to blow up clone %s", GetNetworkObject()->GetLogName());

		KillPedsInVehicle(pCulprit, weaponHash);
    	KillPedsGettingInVehicle(pCulprit);

 		m_nPhysicalFlags.bRenderScorched = TRUE;  
		SetTimeOfDestruction();
		
		SetIsWrecked();

		// Switch off the engine. (For sound purposes)
		this->SwitchEngineOff(false);
		this->m_OverrideLights = NO_CAR_LIGHT_OVERRIDE;
		this->m_nVehicleFlags.bLightsOn = FALSE;

		g_decalMan.Remove(this);

		//Check to see that it is the player
		if (pCulprit && pCulprit->GetIsTypePed() && ((CPed*)pCulprit)->IsLocalPlayer())
		{
			CStatsMgr::RegisterVehicleBlownUpByPlayer(this);
		}

		return;
    }

	if (NetworkUtils::IsNetworkCloneOrMigrating(this))
	{
		// the vehicle is migrating. Create a weapon damage event to blow up the vehicle, which will be sent to the new owner. If the migration fails
		// then the vehicle will be blown up a little later.
		CBlowUpVehicleEvent::Trigger(*this, pCulprit, bAddExplosion, weaponHash, bDelayedExplosion);
		return;
	}

	//Total damage done for the damage trackers
	float totalDamage = GetHealth() + m_VehicleDamage.GetEngineHealth() + m_VehicleDamage.GetPetrolTankHealth();
	for(s32 i=0; i<GetNumWheels(); i++)
	{
		totalDamage += m_VehicleDamage.GetTyreHealth(i);
		totalDamage += m_VehicleDamage.GetSuspensionHealth(i);
	}
	totalDamage = totalDamage > 0.0f ? totalDamage : 1000.0f;

//	AddToMoveSpeedZ(0.13f);
	ApplyImpulseCg(Vector3(0.0f, 0.0f, 6.5f));

	SetWeaponDamageInfo(pCulprit, weaponHash, fwTimer::GetTimeInMilliseconds());

	//Set the destruction information.
	SetDestructionInfo(pCulprit, weaponHash);
	
	SetIsWrecked();

	this->m_nPhysicalFlags.bRenderScorched = TRUE;  // need to make Scorched BEFORE components blow off


	// do it before SpawnFlyingComponent() so this bit is propagated to all vehicle parts before they go flying:
	// let know pipeline, that we don't want any extra passes visible for this clump anymore:
//	CVisibilityPlugins::SetClumpForAllAtomicsFlag((RpClump*)this->m_pRwObject, VEHICLE_ATOMIC_PIPE_NO_EXTRA_PASSES); rage

	SetHealth(0.0f);	// Make sure this happens before AddExplosion or it will blow up twice

	KillPedsInVehicle(pCulprit, weaponHash);
	
	// Switch off the engine. (For sound purposes)
	this->SwitchEngineOff(false);
	this->m_OverrideLights = NO_CAR_LIGHT_OVERRIDE;
	this->m_nVehicleFlags.bLightsOn = FALSE;

	if( bAddExplosion )
	{
		AddVehicleExplosion(pCulprit, bInACutscene, bDelayedExplosion);
	}

	//Update Damage Trackers
	GetVehicleDamage()->UpdateDamageTrackers(pCulprit, weaponHash, DAMAGE_TYPE_EXPLOSIVE, totalDamage, false);

	//Check to see that it is the player
	if (pCulprit && ((pCulprit->GetIsTypePed() && ((CPed*)pCulprit)->IsLocalPlayer()) || pCulprit == CGameWorld::FindLocalPlayerVehicle()))
	{
		CStatsMgr::RegisterVehicleBlownUpByPlayer(this);
	}

	g_decalMan.Remove(this);

	CPed* fireCulprit = NULL;
	if (pCulprit && pCulprit->GetIsTypePed())
	{
		fireCulprit = static_cast<CPed*>(pCulprit);
	}
	g_vfxVehicle.ProcessWreckedFires(this, fireCulprit, FIRE_DEFAULT_NUM_GENERATIONS);

	NetworkInterface::ForceHealthNodeUpdate(*this);
	m_nVehicleFlags.bBlownUp = true;


}// end of CBoat::BlowUpCar()...

//////////////////////////////////////////////////////////////////////////
// Damage the vehicle even more, close to these bones during BlowUpCar
//////////////////////////////////////////////////////////////////////////
const int BOAT_BONE_COUNT_TO_DEFORM = 15;
const eHierarchyId ExtraBoatBones[BOAT_BONE_COUNT_TO_DEFORM] = 
{ 
	VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS, //default CPU code path has 4 max impacts, do the most important ones first
	VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS, VEH_CHASSIS,
	BOAT_STATIC_PROP, BOAT_MOVING_PROP, BOAT_STATIC_PROP2, BOAT_MOVING_PROP2, BOAT_RUDDER, BOAT_RUDDER2, BOAT_HANDLEBARS,
};

const eHierarchyId* CBoat::GetExtraBonesToDeform(int& extraBoneCount)
{
	extraBoneCount = BOAT_BONE_COUNT_TO_DEFORM;
	return ExtraBoatBones;
}

void CBoat::Fix(bool resetFrag, bool allowNetwork)
{
	CVehicle::Fix(resetFrag, allowNetwork);

	// deformation has been reset, so reset offset
	m_BoatHandling.Fix();
}


void CBoat::ApplyDeformationToBones(const void* basePtr)
{
	if( NULL != basePtr )
	{
		Vector3 vecPos;
		if( GetBoneIndex( BOAT_STATIC_PROP ) != -1 )
		{
			// get the original position of the prop
			GetDefaultBonePositionForSetup( BOAT_STATIC_PROP, vecPos );

			m_BoatHandling.SetPropDeformOffset1( VEC3V_TO_VECTOR3( GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset( basePtr, VECTOR3_TO_VEC3V( vecPos ) ) ) );
		}
		if( GetBoneIndex( BOAT_STATIC_PROP2 ) != -1 )
		{
			// get the original position of the prop
			GetDefaultBonePositionForSetup( BOAT_STATIC_PROP2, vecPos );

			m_BoatHandling.SetPropDeformOffset2( VEC3V_TO_VECTOR3( GetVehicleDamage()->GetDeformation()->ReadFromVectorOffset( basePtr, VECTOR3_TO_VEC3V( vecPos ) ) ) );
		}
	}
}


bool CBoat::IsInAir(bool UNUSED_PARAM(bCheckForZeroVelocity)) const
{
	const Vector3 vPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
	float fWaterLevel;
	if(m_Buoyancy.GetWaterLevelIncludingRivers(vPos, &fWaterLevel, false, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
	{
		static float IN_AIR_HEIGHT = 1.5f;
		if(vPos.z - fWaterLevel > IN_AIR_HEIGHT)
		{
			if(GetFrameCollisionHistory()->GetMaxCollisionImpulseMagLastFrame() > 0.0f)
			{
				return true;
			}
		}
	}

	return false;
}

bool CBoat::HasLandedStably() const
{
	return m_BoatHandling.IsInWater() && GetTransform().GetC().GetZf() > 0.6f;
}

bool CBoat::PlaceOnRoadAdjustInternal(float UNUSED_PARAM(HeightSampleRangeUp), float UNUSED_PARAM(HeightSampleRangeDown), bool UNUSED_PARAM(bJustSetCompression))
{
	// Make this boat track the water surface by forcing the low LOD buoyancy processing.
	Vector3 vPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
	m_Buoyancy.ForceUpdateRiverProbeResultSynchronously(vPos);
	bool bOriginalAnchorState = GetAnchorHelper().UsingLowLodMode();
	GetAnchorHelper().SetLowLodMode(true);
	Assert(pHandling);
	Assert(pHandling->GetBoatHandlingData());
	m_Buoyancy.SetLowLodDraughtOffset(pHandling->GetBoatHandlingData()->m_fLowLodDraughtOffset);
	bool bFoundWater = ProcessLowLodBuoyancy();
	GetAnchorHelper().SetLowLodMode(bOriginalAnchorState);

	ActivatePhysics();

	if(bFoundWater)
	{
		m_nVehicleFlags.bPlaceOnRoadQueued = false;
		m_BoatHandling.SetPlaceOnWaterTimeout( 0 );
		return true;
	}
	else
	{
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
			return true;
		}

		m_nVehicleFlags.bPlaceOnRoadQueued = true;
		return false;
	}
}

#if __BANK
void CBoat::InitWidgets(bkBank& bank)
{
	bank.PushGroup("Boats");
		bank.PushGroup("Boat pitch",false);
			bank.AddSlider("Boat pitch force",&sfPitchForce,0.0f,1000.0f,0.1f);
			bank.AddSlider("Boat pitch vel threshold sq",&sfPitchFowradVelThreshold2,0.0f,1000.0f,0.1f);
		bank.PopGroup();

		bank.AddSlider("In water ground friction multiplier",&ms_InWaterFrictionMultiplier,0.0f,5.0f,0.1f);
		bank.AddSlider("Out of water ground friction multiplier",&ms_OutOfWaterFrictionMultiplier,0.0f,5.0f,0.1f);
		bank.AddSlider("Out of water time",&ms_OutOfWaterTime,0.01f,20.0f,0.1f);

		bank.AddSlider("Max prop out of water duration", &ms_nMaxPropOutOfWaterDuration, 0, 60000, 500);
	bank.PopGroup();
}
#endif // __BANK

#if __WIN32PC
void CBoat::ProcessForceFeedback()
{
	if(m_BoatHandling.IsInWater())
	{
		// The scaler to apply to a boat's keel force feedback.
		TUNE_FLOAT(BOAT_KEEL_FORCE_FEEDBACK_SCALER, 0.0083f, 0.0000f, 1.0f, 0.0001f);

		// Used to tune how quick the force feedback response is.
		TUNE_FLOAT(BOAT_FORCE_FEEDBACK_RESPONSE, 1.0f, 0.01f, 5.0f, 0.01f);

		// Used to tune the resistance in the device's resistance force.
		TUNE_FLOAT(BOAT_FORCE_FEEDBACK_RESISTANCE_SCALER, 0.2f, 0.01f, 1.0f, 0.01f);

		const ioValue& leftRightValue = CControlMgr::GetMainCameraControl(false).GetVehicleSteeringLeftRight();

		float force            = m_Buoyancy.GetKeelSteerForce() * BOAT_KEEL_FORCE_FEEDBACK_SCALER;
		float inputVelocity    = (leftRightValue.GetNorm(ioValue::NO_DEAD_ZONE) - leftRightValue.GetLastNorm(ioValue::NO_DEAD_ZONE)) / fwTimer::GetTimeStep();

		inputVelocity         *= BOAT_FORCE_FEEDBACK_RESISTANCE_SCALER;

		float forceCompensated = (1.0f/(1.0f + exp((-force - inputVelocity)/BOAT_FORCE_FEEDBACK_RESPONSE))-0.5f) * 2.0f;
		CControlMgr::ApplyDirectionalForce(forceCompensated, 0, 100);
	}
}
#endif // __WIN32PC
