// File header
#include "horseObstacleAvoidance.h"

#if ENABLE_HORSE

#include "Peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Motion/Locomotion/TaskHorseLocomotion.h"
#include "Task/System/TaskHelpers.h"

// Rage includes
#if __BANK
#include "bank/bank.h"
#endif
#include "math/angmath.h"
#include "phbound/boundbox.h"
#include "phbound/boundcapsule.h"
#include "phbound/boundsphere.h"
#include "physics/debugevents.h"
#include "physics/inst.h"
#include "physics/levelnew.h"
#include "physics/physics.h"
#include "vector/colors.h"
#include "phbound/boundbvh.h"
#include "phbound/boundcomposite.h"

AI_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
////////////////////////// hrsObstacleAvoidanceControl /////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Statics
float hrsObstacleAvoidanceControl::MIN_ENABLE_SPEED = 1.9f;
float hrsObstacleAvoidanceControl::REJECT_RADIUS = 3.0f;
float hrsObstacleAvoidanceControl::VEHICLE_REJECT_RADIUS = 5.0f;

float hrsObstacleAvoidanceControl::WATER_SLOW_MIN_DEPTH_1 = 0.35f;
float hrsObstacleAvoidanceControl::WATER_SLOW_MIN_DEPTH_2 = 0.7f;
float hrsObstacleAvoidanceControl::WATER_SLOW_SLOPE_FACTOR_DOWN_1 = 1.0f;
float hrsObstacleAvoidanceControl::WATER_SLOW_SLOPE_FACTOR_UP_1 = 5.0f;
float hrsObstacleAvoidanceControl::WATER_SLOW_SLOPE_FACTOR_DOWN_2 = 5.0f;
float hrsObstacleAvoidanceControl::WATER_SLOW_SLOPE_FACTOR_UP_2 = 10.0f;
float hrsObstacleAvoidanceControl::WATER_SLOW_MAX_DOWN = 0.9f;
float hrsObstacleAvoidanceControl::WATER_SLOW_DEPTH_FACTOR_1 = 0.5f;
float hrsObstacleAvoidanceControl::WATER_SLOW_DEPTH_FACTOR_2 = 1.5f;

#if __BANK
float hrsObstacleAvoidanceControl::TEST_LENGTH_OVERRIDE = 0.0f;
float hrsObstacleAvoidanceControl::TEST_PITCH_OVERRIDE = 0.0f;
bool  hrsObstacleAvoidanceControl::IGNORE_MIN_SPEED = false;
#endif

////////////////////////////////////////////////////////////////////////////////

namespace rage
{
#if __PFDRAW
	PFD_DECLARE_GROUP(HorseInputs);
	PFD_DECLARE_ITEM(AvoidanceCapsule, Color_green, HorseInputs);
	PFD_DECLARE_ITEM(AvoidanceBoundsDetection, Color_blue, HorseInputs);
	PFD_DECLARE_ITEM(SurfacePitchDetection, Color_orange, HorseInputs);
#endif
}

////////////////////////////////////////////////////////////////////////////////

hrsObstacleAvoidanceControl::hrsObstacleAvoidanceControl()
: m_NavMeshLineOfSightHelper()
, m_fPreviousHeading(0)
, m_bQuickStopping(false)
{
	Reset();
}

////////////////////////////////////////////////////////////////////////////////

void hrsObstacleAvoidanceControl::Reset(bool bResetPitch)
{
	m_fCorrectedHeading =  0.0f;
	m_fMinYaw           = -PI;
	m_fMaxYaw           =  PI;
	m_fBrake            =  0.0f;
	m_fHeadingScalar	=  0.0f;
	m_bAvoiding         =  false;
	m_bCliffDetectedLastFrame = false;
	CorrectionResult	=  CORRECT_CENTER;
	LastNonFailCorrectionResult = CORRECT_CENTER;
	m_vCliffPrediction.Zero();
	m_vUnderHorseNormal = ZAXIS;

	for( int i = 0; i < NUM_PROBES; i++)
	{
		m_pSurfaceInsts[i] = NULL;
	}

	if( bResetPitch )
	{
		m_fSurfacePitch =  BANK_ONLY(TEST_PITCH_OVERRIDE != 0.0f ? TEST_PITCH_OVERRIDE :) 0.0f;
	}
}

////////////////////////////////////////////////////////////////////////////////

void hrsObstacleAvoidanceControl::Update(const float fCurrentSpeedMPS, const Matrix34 & mCurrMtx, float fCurrHeading, const float fSoftBrakeStopDist, const float fStickTurn, const bool bOnMountCheck)
{	
	static dev_u32 s_uDetectedHandholdCooldown = 500; 

	bool bDetectingAutoJump = fwTimer::GetTimeInMilliseconds() < 
		(m_pPed->GetPedIntelligence()->GetClimbDetector().GetLastDetectedHandHoldTime()+s_uDetectedHandholdCooldown);

	if( IsEnabled() && IsMountedByPlayer() && !bDetectingAutoJump && ( bOnMountCheck || BANK_ONLY(IGNORE_MIN_SPEED ||) fCurrentSpeedMPS > MIN_ENABLE_SPEED || !AreNearlyEqual(fCurrHeading,m_fPreviousHeading) || m_bCliffDetectedLastFrame || m_bQuickStopping ) )
	{
		m_bCliffDetectedLastFrame = false;
		ComputeAvoidance(fCurrentSpeedMPS, mCurrMtx, fCurrHeading, fSoftBrakeStopDist, fStickTurn, bOnMountCheck);
		m_fPreviousHeading = fCurrHeading;
	}
	else
	{
		Reset();		
		m_NavMeshLineOfSightHelper.ResetTest();
		m_NavMeshBreakRecentlyDetected = false;
	}	
}

float hrsObstacleAvoidanceControl::CalculateWaterMaxSpeed()
{
	PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*m_pPed->GetCurrentPhysicsInst(), m_fSurfacePitch, "m_fSurfacePitch"));
	if (IsMountedByPlayer() && m_pPed->m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate && m_pPed->m_nFlags.bPossiblyTouchesWater)
	{
		// If the horse is actually swimming, don't max the speed, It will get already slowed down by the different sets of swimming animations
		if ( m_pPed->GetIsSwimming() )
		{
			return 1.0f;
		}

		//If we're in water...
		Vec3V vHorsePosition = m_pPed->GetTransform().GetPosition();
		float fThisZ		= vHorsePosition.GetZf();
		const float kfAbsWaterLevel = m_pPed->m_Buoyancy.GetAbsWaterLevel();
		float fWaterDepth	= (kfAbsWaterLevel - fThisZ) + m_pPed->GetCapsuleInfo()->GetGroundToRootOffset();
		PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*m_pPed->GetCurrentPhysicsInst(), fWaterDepth, "fWaterDepth - horse"));
		if (fWaterDepth > 0.0f)
		{
			//We're in water, do an extra probe in front of the horse to see if its getting deeper...
			const int MAX_INTERSECTIONS_PER_PROBE = 4;
			WorldProbe::CShapeTestResults capsuleResults(MAX_INTERSECTIONS_PER_PROBE);
			WorldProbe::CShapeTestCapsuleDesc sweptSphereTest;
			Vec3V vHorseDir = m_pPed->GetTransform().GetMatrix().GetCol1();
			Vec3V vProbeStart = vHorsePosition + (vHorseDir * ScalarV(1.5f));
			Vec3V vProbeEnd = vProbeStart + (vHorseDir * ScalarV(5.0f)) + Vec3V(0.0f, 0.0f, -4.0f);

			sweptSphereTest.SetCapsule(RCC_VECTOR3(vProbeStart), RCC_VECTOR3(vProbeEnd), 0.25f);
			sweptSphereTest.SetResultsStructure(&capsuleResults);
			sweptSphereTest.SetIsDirected(true);

			static const int iBoundFlags   = ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE;
			static const int iExcludeFlags = ArchetypeFlags::GTA_PED_TYPE;
			sweptSphereTest.SetIncludeFlags(iBoundFlags);
			sweptSphereTest.SetExcludeTypeFlags(iExcludeFlags);	

			// Add the horse/rider to the exclusions
			//const int MAX_EXCLUSIONS_FOR_ACTOR = 2;
			//const int MAX_EXCLUSIONS = MAX_EXCLUSIONS_FOR_ACTOR + NUM_PROBES;
			//const phInst* pExcludeInstList[MAX_EXCLUSIONS];
			//pExcludeInstList[0] = m_pPed->GetCurrentPhysicsInst();
			//pExcludeInstList[1] = m_pPed->GetSeatManager()->GetDriver()->GetCurrentPhysicsInst();
			//sweptSphereTest.SetExcludeInstances(pExcludeInstList, MAX_EXCLUSIONS_FOR_ACTOR);

			WorldProbe::GetShapeTestManager()->SubmitTest(sweptSphereTest);
			for(WorldProbe::ResultIterator it = capsuleResults.begin(); it < capsuleResults.last_result(); ++it)
			{
				float fNewDepth = kfAbsWaterLevel - it->GetHitPosition().z;
				if (fNewDepth > fWaterDepth)
				{
					fWaterDepth = Max(fWaterDepth, kfAbsWaterLevel - it->GetHitPosition().z);
					PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*m_pPed->GetCurrentPhysicsInst(), fWaterDepth, "fWaterDepth - ahead"));
					PDR_ONLY(debugPlayback::RecordTaggedVectorValue(*m_pPed->GetCurrentPhysicsInst(), RCC_VEC3V(it->GetHitPosition()), "Water check pos", debugPlayback::eVectorTypePosition));
				}
			}
		}

		//PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*m_pPed->GetCurrentPhysicsInst(), fWaterDepth, "fWaterDepth - used"));
		if (fWaterDepth > WATER_SLOW_MIN_DEPTH_1)
		{
			float fEffectiveWaterDepth = fWaterDepth - WATER_SLOW_MIN_DEPTH_1;
			float fDepthEffect = fEffectiveWaterDepth * WATER_SLOW_DEPTH_FACTOR_1;
			PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*m_pPed->GetCurrentPhysicsInst(), fDepthEffect, "fDepthEffect1"));

			Vector3 vGN = m_pPed->GetGroundNormal();
			Vector3 vYAxisFlat = VEC3V_TO_VECTOR3(m_pPed->GetMatrix().GetCol1());
			vYAxisFlat.z = 0.0f;
			float fSlopeFactor = vYAxisFlat.Dot(vGN);
			PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*m_pPed->GetCurrentPhysicsInst(), fSlopeFactor, "fSlopeFactor"));

			float fSlopeEffect = fSlopeFactor * fEffectiveWaterDepth;
			fSlopeEffect *= fSlopeEffect > 0.0f ? WATER_SLOW_SLOPE_FACTOR_DOWN_1 : WATER_SLOW_SLOPE_FACTOR_UP_1 ;
			PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*m_pPed->GetCurrentPhysicsInst(), fSlopeEffect, "fSlopeEffect_1"));

			if (fWaterDepth > WATER_SLOW_MIN_DEPTH_2)
			{
				float fEffectiveWaterDepth2 = fWaterDepth - WATER_SLOW_MIN_DEPTH_2;
				float fWaterDepthEffect2 = fEffectiveWaterDepth2 * WATER_SLOW_DEPTH_FACTOR_2;
				PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*m_pPed->GetCurrentPhysicsInst(), fWaterDepthEffect2, "fDepthEffect2"));
				fDepthEffect += fWaterDepthEffect2;

				float fSlopeEffect2 = fSlopeFactor * fEffectiveWaterDepth2;
				fSlopeEffect2 *= fSlopeEffect2 > 0.0f ? WATER_SLOW_SLOPE_FACTOR_DOWN_2 : WATER_SLOW_SLOPE_FACTOR_UP_2 ;
				PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*m_pPed->GetCurrentPhysicsInst(), fSlopeEffect2, "fSlopeEffect2"));
				fSlopeEffect += fSlopeEffect2;
			}
			PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*m_pPed->GetCurrentPhysicsInst(), fSlopeEffect, "fSlopeEffect"));
			float fWaterMaxSpd = 1.0f - WATER_SLOW_MAX_DOWN * Clamp(fDepthEffect+fSlopeEffect, 0.0f, 1.0f);
			PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*m_pPed->GetCurrentPhysicsInst(), fDepthEffect, "fDepthEffectTotal"));
			PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*m_pPed->GetCurrentPhysicsInst(), fWaterMaxSpd, "fWaterMaxSpd"));
			return fWaterMaxSpd;
		}
	}
	PDR_ONLY(debugPlayback::RecordTaggedFloatValue(*m_pPed->GetCurrentPhysicsInst(), 1.0f, "fWaterMaxSpd"));
	return 1.0f;
}

////////////////////////////////////////////////////////////////////////////////

bool hrsObstacleAvoidanceControl::AdjustHeading(float & fInOutGoalHeading)
{
	bool retval = false;
	if( IsEnabled() && IsMountedByPlayer() && m_bAvoiding )
	{
		fInOutGoalHeading = m_fCorrectedHeading;
		retval = true;
	}
	return retval;
}

////////////////////////////////////////////////////////////////////////////////

bool hrsObstacleAvoidanceControl::AdjustYaw(float & fInOutYaw)
{
	if( IsEnabled() && IsMountedByPlayer() && m_bAvoiding )
	{
		float fPrevYaw = fInOutYaw;
		fInOutYaw = Clamp(fInOutYaw, m_fMinYaw, m_fMaxYaw);
		return (fPrevYaw != fInOutYaw);
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void hrsObstacleAvoidanceControl::AddWidgets(bkBank & BANK_ONLY(b))
{
#if __BANK
	b.PushGroup("ObstacleAvoidanceControl", false);
	hrsControl::AddWidgets(b);
	b.PopGroup();
#endif
}

void hrsObstacleAvoidanceControl::AddStaticWidgets(bkBank & BANK_ONLY(b))
{
#if __BANK
	b.PushGroup("ObstacleAvoidanceControl", false);
	b.AddSlider("Test Length Override", &TEST_LENGTH_OVERRIDE, 0.0f, 20.0f, 0.01f);
	b.AddSlider("Test Pitch Override", &TEST_PITCH_OVERRIDE, -PI/4.0f, PI/4.0f, 0.01f);
	b.AddToggle("Ignore Min Speed", &IGNORE_MIN_SPEED);
	b.AddSlider("Min Enable Speed", &MIN_ENABLE_SPEED, 0.0f, 20.0f, 0.01f);
	b.AddSlider("Reject Radius", &REJECT_RADIUS, 0.0f, 20.0f, 0.01f);

	b.PushGroup("Water", false);
	b.AddSlider("Slope factor UP 1", &WATER_SLOW_SLOPE_FACTOR_UP_1, 0.0f, 50.0f, 0.01f);
	b.AddSlider("Slope factor DOWN 1", &WATER_SLOW_SLOPE_FACTOR_DOWN_1, 0.0f, 50.0f, 0.01f);
	b.AddSlider("Slope factor UP 2", &WATER_SLOW_SLOPE_FACTOR_UP_2, 0.0f, 50.0f, 0.01f);
	b.AddSlider("Slope factor DOWN 2", &WATER_SLOW_SLOPE_FACTOR_DOWN_2, 0.0f, 50.0f, 0.01f);
	b.AddSlider("Max slope down", &WATER_SLOW_MAX_DOWN, 0.0f, 1.0f, 0.01f);
	b.AddSlider("Min depth 1", &WATER_SLOW_MIN_DEPTH_1, 0.0f, 1.0f, 0.01f);
	b.AddSlider("Depth factor 1", &WATER_SLOW_DEPTH_FACTOR_1, 0.0f, 10.0f, 0.01f);
	b.AddSlider("Min depth 2", &WATER_SLOW_MIN_DEPTH_2, 0.0f, 1.0f, 0.01f);
	b.AddSlider("Depth factor 2", &WATER_SLOW_DEPTH_FACTOR_2, 0.0f, 10.0f, 0.01f);
	b.PopGroup();

	b.PopGroup();
#endif
}

////////////////////////////////////////////////////////////////////////////////
// static float sfBrakeFactor = 1.0f;
// static float sfUseStickFactor = 0.5f;
void hrsObstacleAvoidanceControl::ComputeAvoidance(const float fCurrentSpeedMPS, const Matrix34 & mCurrMtx, float fCurrHeading, const float fSoftBrakeStopDist, const float fStickTurn, const bool bOnMountCheck)
{
	//
	// Perform a capsule test in front the of the actor to detect obstacles
	//

	static const int NUM_CAPSULES = 2;
	static const Vector3 CAPSULE_OFFSETS[NUM_CAPSULES] = 
	{
		Vector3(0.0f, 1.1f, 0.85f), 
		Vector3(0.0f, 1.1f, 1.4f), 
	};

	// Transform the offsets into the correct space
	Vector3 vCapsuleOffsets[NUM_CAPSULES];
	for( int i = 0; i < NUM_CAPSULES; i++ )
	{
		mCurrMtx.Transform3x3(CAPSULE_OFFSETS[i], vCapsuleOffsets[i]);
	}

	// We will scale the capsule length by the speed/stop distance, so we look ahead further when going faster
	static const float CAPSULE_LENGTH = 1.0f;
	static const float BRAKE_STOP_DIST_FACTOR = 0.8f;
	float fLength = CAPSULE_LENGTH + (fSoftBrakeStopDist * BRAKE_STOP_DIST_FACTOR);
#if __BANK
	if( TEST_LENGTH_OVERRIDE != 0.0f )
	{
		fLength = TEST_LENGTH_OVERRIDE;
	}
#endif // __BANK

	// Copy the matrix, so we can modify it
	Matrix34 mMtx(mCurrMtx);	
	if( !bOnMountCheck )
	{
		const float fCurrentPitch = GetSurfacePitch(mMtx, fLength);

		//subtract the horse's pitch
		float hrsPitch = m_pPed->GetCurrentPitch();		
		//Displayf("horse pitch: %f", hrsPitch);

		// Lerp towards the pitch value, so it is slightly smoothed
		static const float PITCH_RATE = 0.1f;
		m_fSurfacePitch += Clamp((fCurrentPitch!=0.0f ? (fCurrentPitch-hrsPitch) : fCurrentPitch) - m_fSurfacePitch, -PITCH_RATE, PITCH_RATE);

		// Rotate the matrix by the surface pitch
		mMtx.RotateLocalX(m_fSurfacePitch);


		//Check if a LOS NavMesh test is active.
		if (!m_pPed->GetGroundPhysical() && !m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected))
		{
			if(m_NavMeshLineOfSightHelper.IsTestActive())
			{
				//Get the results.
				SearchStatus searchResult;
				bool bLosIsClear, bNoSurfaceAtCoords;
				int iNumPts=0;
				if(m_NavMeshLineOfSightHelper.GetTestResults(searchResult, bLosIsClear, bNoSurfaceAtCoords, iNumPts, NULL) && (searchResult != SS_StillSearching))
				{
					//Check if the line of sight is clear.
					if(bLosIsClear) 
					{
						m_NavMeshBreakRecentlyDetected=false;
					} 
					else
					{
						m_NavMeshBreakRecentlyDetected=true;
					}
				}
			}
			//Check if a LOS test is active.
			else 
			{			
				//Start a nav mesh line of sight test from the current position to the new position.
				Vector3 vStart(mCurrMtx.d);
				Vector3 vEnd(vStart);
				vStart += mMtx.b; 	//TMS - Offset slightly to fix a problem where missing navmesh around small fixed objects (tree trunks) gets considered as a cliff
									//		In general in these cases the probes will be ignored anyway as the probes take precedence.
				vEnd   += mMtx.b * (fLength + 0.5f);			

				Vector3 vOnNavMeshEnd(vEnd);
				CNavMesh * pNavMesh;
				TNavMeshPoly * pPoly;
				const float DISTANCE_TO_CHECK = 10.0f;
				//first do below check
				EPathServerErrorCode eRet = CPathServer::m_PathServerThread.GetNavMeshPolyForRouteEndPoints(vEnd, NULL, pNavMesh, pPoly, &vOnNavMeshEnd, DISTANCE_TO_CHECK, 0.0f, false, true, kNavDomainRegular, false);
				if(eRet!=PATH_NO_ERROR)
				{
					//nothing found below - try above check
					eRet = CPathServer::m_PathServerThread.GetNavMeshPolyForRouteEndPoints(vEnd, NULL, pNavMesh, pPoly, &vOnNavMeshEnd, 0.0f, DISTANCE_TO_CHECK, false, true, kNavDomainRegular, false);
				}

				if(eRet==PATH_NO_ERROR)
				{
					m_NavMeshLineOfSightHelper.StartLosTest(vStart, vOnNavMeshEnd);
				}
				else
				{
					m_NavMeshBreakRecentlyDetected=true;
				}
				PF_DRAW_ONLY(PFD_SurfacePitchDetection.DrawLine(vStart, vOnNavMeshEnd));
			}
		}
	}

	//
	// Collision testing
	//

	// Radius
	static /*const*/ float CAPSULE_RADIUS = 0.3f;	//0.45->0.2 smaller, prevents getting stuck in walls
													//0.2->0.3 for better tree avoidance
	static const float RADIUS_SCALE_FACTOR = 0.0f;
	const float fCapsuleRadius = CAPSULE_RADIUS + (fSoftBrakeStopDist * RADIUS_SCALE_FACTOR);	
	int iIntersections = 0;	

	// Setup exclusions - this includes the horse/rider and the ground surface
	const int MAX_EXCLUSIONS_FOR_ACTOR = 2; 
	const int MAX_EXCLUSIONS = MAX_EXCLUSIONS_FOR_ACTOR + NUM_PROBES;
	const phInst* pExcludeInstList[MAX_EXCLUSIONS];

	// Add the horse/rider to the exclusions
	pExcludeInstList[0] = m_pPed->GetCurrentPhysicsInst();
	pExcludeInstList[1] = m_pPed->GetSeatManager()->GetDriver() ? m_pPed->GetSeatManager()->GetDriver()->GetCurrentPhysicsInst() : NULL;
	int iNumExclusions = 2; //ConfigureActorExcludeInsts(parent.GetGuid(), pExcludeInstList, MAX_EXCLUSIONS_FOR_ACTOR, kExcludeInventoryItemsBit | kExcludeMountBit | kExcludeMoverInstsBit | kExcludeGameCameraExclusionListBit | kHogTieVictimOnShoulderBit);
	// Add the surface insts
	for( int i = 0; i < NUM_PROBES; i++ )
	{
		if( m_pSurfaceInsts[i] )
		{
			//TODO in GTA "buildings" are terrain too :(
			//pExcludeInstList[iNumExclusions++] = m_pSurfaceInsts[i];
		}
	}

	Vector3 vObstacleCenter(Vector3::ZeroType);
	bool bHasObstacle=false;
	//float fObstacleRadius = 0.0f;

	// Test flags
	static const int iBoundFlags   = ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE; // sagBoundFlags::BOUNDFLAG_MOVERBOUND;
	static const int iExcludeFlags = ArchetypeFlags::GTA_PED_TYPE; // sagBoundFlags::BOUNDFLAG_TERRAIN | sagBoundFlags::BOUNDFLAG_CREATURE;

	// Perform collision tests
	static const int NUM_FAN_CAPSULES = 3;
	static const float CAPSULE_THETA = PI/8.0f;
	Vector3 fanCapsuleOffset;
	fanCapsuleOffset.Average(vCapsuleOffsets[0],vCapsuleOffsets[1]);
	Matrix34 capsuleMtx(mMtx);
	capsuleMtx.RotateZ(-CAPSULE_THETA);
	float clearDistance[NUM_FAN_CAPSULES];
	float wallFactor[NUM_FAN_CAPSULES];
	const int MAX_INTERSECTIONS_PER_PROBE = 4;
	Vector3 vCapsuleEnds[3];
	for( int i = 0; i < NUM_FAN_CAPSULES; i++ )
	{
		// Start point
		Vector3 vStart(capsuleMtx.d + fanCapsuleOffset);

		// End point
		// mCurrMtx.c is the heading vector
		// We point backwards, so invert fLength
		float capLength = fLength;
		//if (i!=1)
		//	capLength=Max(1.7f, capLength*0.6f);
		vCapsuleEnds[i].Set((vStart + (capsuleMtx.b * capLength)));

		//
		capsuleMtx.RotateZ(CAPSULE_THETA);

		// Rendering
		PF_DRAW_ONLY(PFD_AvoidanceCapsule.DrawCapsule(vStart, vCapsuleEnds[i], fCapsuleRadius));

		// Perform a sphere test
		WorldProbe::CShapeTestFixedResults<MAX_INTERSECTIONS_PER_PROBE> sphereResults;		
		if (i==1) //only the center sphere
		{
			WorldProbe::CShapeTestSphereDesc sphereTest;	
			sphereTest.SetSphere(vStart, fCapsuleRadius);
			sphereTest.SetResultsStructure(&sphereResults);
			sphereTest.SetIncludeFlags(iBoundFlags);
			sphereTest.SetExcludeTypeFlags(iExcludeFlags);	
			sphereTest.SetExcludeInstances(pExcludeInstList, iNumExclusions);

			WorldProbe::GetShapeTestManager()->SubmitTest(sphereTest);
			iIntersections += sphereResults.GetNumHits();	
		}		

		WorldProbe::CShapeTestFixedResults<MAX_INTERSECTIONS_PER_PROBE> capsuleResults;
		// Perform a swept sphere test - this does not test the first sphere
		WorldProbe::CShapeTestCapsuleDesc sweptSphereTest;
		sweptSphereTest.SetCapsule(vStart, vCapsuleEnds[i], fCapsuleRadius);
		sweptSphereTest.SetResultsStructure(&capsuleResults);
		sweptSphereTest.SetIsDirected(true);
		sweptSphereTest.SetIncludeFlags(iBoundFlags);
		sweptSphereTest.SetExcludeTypeFlags(iExcludeFlags);	
		sweptSphereTest.SetExcludeInstances(pExcludeInstList, iNumExclusions);

		WorldProbe::GetShapeTestManager()->SubmitTest(sweptSphereTest);
		iIntersections += capsuleResults.GetNumHits();			
	
		clearDistance[i]=1.0f;   
		wallFactor[i] = 0.0f;
		if( iIntersections > 0 )
		{
			if( bOnMountCheck )
			{
				Reset(false);
				// Set to actively avoiding for purposes on the OnMount check
				m_bAvoiding = true;
				return;
			}
			else
			{	
				static float TERRAIN_REJECTION_Z = 0.5f;				
				for(WorldProbe::ResultIterator it = sphereResults.begin(); it < sphereResults.last_result(); ++it) 				
				{
					if (it->GetHitPolyNormal().z < TERRAIN_REJECTION_Z) 
					{				
						if (it->GetHitTValue() < clearDistance[i])
						{
							clearDistance[i] = it->GetHitTValue();
							wallFactor[i] = Abs(Dot(it->GetNormal(), RCC_VEC3V(mCurrMtx.b)).Getf());

							if (i==1)
							{
								vObstacleCenter = it->GetHitPosition();
								bHasObstacle=true;
					}
					}
					}
					//ProcessTestResults(mCurrMtx, it, vObstacleCenter, fObstacleRadius);					
				}

				for(WorldProbe::ResultIterator it = capsuleResults.begin(); it < capsuleResults.last_result(); ++it) 				
				{
					if (it->GetHitPolyNormal().z < TERRAIN_REJECTION_Z) 
					{
						if (it->GetHitTValue() < clearDistance[i])
						{
							clearDistance[i] = it->GetHitTValue();
							wallFactor[i] = Abs(Dot(it->GetNormal(), RCC_VEC3V(mCurrMtx.b)).Getf());

							if (i==1)
							{
								vObstacleCenter = it->GetHitPosition();
								bHasObstacle=true;
							}
						}
					}
					//ProcessTestResults(mCurrMtx, it, vObstacleCenter, fObstacleRadius);					
				}
			}			
		}
	}

	if (CorrectionResult != CORRECT_FAIL)
	{
		LastNonFailCorrectionResult = CorrectionResult;
	}
	//Process results

	if (clearDistance[1] == 1.0f)
	{
		//Check for close results left or right that might cause intersections?
		CorrectionResult = CORRECT_CENTER;
	}
	else if ((clearDistance[1]==0.0f) && (wallFactor[1] > 0.5f))
		CorrectionResult = CORRECT_FAIL;
	else if (clearDistance[2]==1.0f)
		CorrectionResult = CORRECT_LEFT;
	else if (clearDistance[0]==1.0f)
		CorrectionResult = CORRECT_RIGHT;
	else 
	{	
		if (clearDistance[2]<clearDistance[0]) {
			CorrectionResult = clearDistance[0] < clearDistance[1] ? CORRECT_CENTER : CORRECT_RIGHT;
		} else
			CorrectionResult = clearDistance[2] < clearDistance[1] ? CORRECT_CENTER : CORRECT_LEFT;			
		//CorrectionResult = CORRECT_STOP;
	}

	//TMS: Add more steering when running against vertical faces
	float headingImpactScale = wallFactor[1] + 1.0f-clearDistance[1];

	//cliff?
	bool bCliffDetected = false;
	if (m_NavMeshBreakRecentlyDetected && !m_vCliffPrediction.IsZero()) {		
		bCliffDetected = true;		
		vObstacleCenter = m_vCliffPrediction;
		vObstacleCenter.z = mCurrMtx.d.z;
		bHasObstacle = true;
		//fObstacleRadius = fLength;
	}

	if( iIntersections > 0  || bCliffDetected || m_bQuickStopping)
	{	
		//PF_DRAW_ONLY(PFD_AvoidanceBoundsDetection.DrawLine(mMtx.d, mMtx.d + (vAvoidance * fLength), Color_purple));

		bool bTurnLeft = false;
		static float STICK_FACTOR = 0.5f;
		if (bCliffDetected)
		{
			// Compute the avoidance vector
			if (CorrectionResult == CORRECT_CENTER)
			{//otherwise just turn the way the above wanted.			
				//check for firm ground
				// Construct start and end points
				Vector3 vStart(vCapsuleEnds[0]);
				Vector3 vEnd(vStart);
				vEnd.z = mCurrMtx.d.z - (1.0f);// + fProbeMaxExtraDistBelow * fRatio);

				phSegment segment(vStart, vEnd);
				phIntersection intersection;

				// Debug draw
				PF_DRAW_ONLY(PFD_SurfacePitchDetection.DrawLine(segment.A, segment.B));

				if( DoSurfaceProbe(segment, intersection, &m_vUnderHorseNormal) == SPR_HIT_GROUND )
					CorrectionResult = CORRECT_RIGHT;
				else 
					CorrectionResult = CORRECT_LEFT;

			}

			static_cast<CTaskHorseLocomotion*>(m_pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest())->RequestQuickStop(
				LastNonFailCorrectionResult==CORRECT_RIGHT ? CTaskHorseLocomotion::QUICKSTOP_RIGHT : (LastNonFailCorrectionResult==CORRECT_LEFT ? CTaskHorseLocomotion::QUICKSTOP_LEFT : CTaskHorseLocomotion::QUICKSTOP_FORWARD));
		}		
		
// 		//TMS: Remove the fail quick stop until it can be tuned better
// 		if (CorrectionResult == CORRECT_FAIL)
// 		{
// 			if (m_pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest()->GetTaskType() == CTaskTypes::TASK_ON_FOOT_HORSE) {
// 				static_cast<CTaskHorseLocomotion*>(m_pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest())->RequestQuickStop(
// 					LastNonFailCorrectionResult==CORRECT_RIGHT ? CTaskHorseLocomotion::QUICKSTOP_RIGHT : (LastNonFailCorrectionResult==CORRECT_LEFT ? CTaskHorseLocomotion::QUICKSTOP_LEFT : CTaskHorseLocomotion::QUICKSTOP_FORWARD));
// 				m_bQuickStopping = true;					
// 			}			
// 			Reset(false);
// 			//Displayf("Correction (%f, %f, %f): CORRECT_FAIL", clearDistance[2], clearDistance[1], clearDistance[0]);
// 		} 
// 		else if (m_bQuickStopping)
// 		{
// 			if (m_pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest()->GetTaskType() == CTaskTypes::TASK_ON_FOOT_HORSE) {
// 				static_cast<CTaskHorseLocomotion*>(m_pPed->GetPedIntelligence()->GetMotionTaskActiveSimplest())->RequestCancelQuickStop();					
// 			}
// 			m_bQuickStopping = false;
// 		}
		
		if(CorrectionResult > CORRECT_CENTER )
		{
			if((-fStickTurn > STICK_FACTOR || (-fStickTurn > -STICK_FACTOR && CorrectionResult==CORRECT_LEFT)))
			{
				bTurnLeft = true;
			}

			if(bTurnLeft)
			{
				static float fMax = 1.0f;
				m_fMinYaw = PI;
				m_fMaxYaw = fMax;
			}
			else
			{
				static float fMin = -1.0f;
				m_fMinYaw = fMin;
				m_fMaxYaw = -PI;
			}

#if 0
			const char* resultStrings[5] = {"CORRECT_CENTER",
				"CORRECT_LEFT",
				"CORRECT_RIGHT",
				"CORRECT_STOP",
				"CORRECT_FAIL"};
			if (bCliffDetected)
				Displayf("Correction CLIFF Detected, going %s", bTurnLeft ? "LEFT" : "RIGHT");
			else
				Displayf("Correction (%f, %f, %f): %s, going %s", clearDistance[2], clearDistance[1], clearDistance[0], resultStrings[CorrectionResult], bTurnLeft ? "LEFT" : "RIGHT");
#endif

			if (bHasObstacle && bCliffDetected)
			{
				Vector3 vDiff(vObstacleCenter - mCurrMtx.d);
				static float DIST = 8.0f;
				m_fBrake = vDiff.Mag() / DIST;
				m_fBrake = Min(m_fBrake, 1.0f);
				m_fBrake = 1.0f - m_fBrake;
			}

			static dev_float CLIFF_HEADING_SCALAR = 2.0f;
			static dev_float MIN_CLIFF_AVOIDANCE_SPEED = 3.0f;
			if (bCliffDetected) {
				m_fBrake = Min(0.25f+m_fBrake, 1.0f);
				m_bCliffDetectedLastFrame = true;
				m_fHeadingScalar = CLIFF_HEADING_SCALAR;				
				if (fCurrentSpeedMPS < MIN_CLIFF_AVOIDANCE_SPEED) //too slow, just stop!
					m_fBrake = 1.0f;
			} else
				m_fHeadingScalar = 0;
			
			//Vector3 vHeading(-mMtx.b);
			if(m_fMinYaw < 0.0f)
			{
				m_fCorrectedHeading = fCurrHeading + m_fMinYaw * headingImpactScale;								
			}
			else if(m_fMaxYaw > 0.0f)
			{
				m_fCorrectedHeading = fCurrHeading + m_fMaxYaw * headingImpactScale;								
			} 
			m_fCorrectedHeading = fwAngle::LimitRadianAngle(m_fCorrectedHeading);			

			// Set to actively avoiding
			m_bAvoiding = true;
		}
		else
		{
			Reset(false);
		}
	}
	else
	{
		Reset(false);
	}
}

void hrsObstacleAvoidanceControl::ProcessTestResults(const Matrix34 & mCurrMtx, WorldProbe::ResultIterator & it, Vector3 & vObstacleCenter, float & fObstacleRadius) 
{
	// Get the bounding sphere for the intersection
	Vector3 vCenter;
	float fRadius;	
	bool isVehicle = false;
	if (it->GetHitInst()->GetArchetype()->GetTypeFlag(ArchetypeFlags::GTA_VEHICLE_TYPE)) 
	{
		CEntity *entity = NULL;
		const phInst *instance = it->GetHitInst();
		if(instance)
		{
			entity = CPhysics::GetEntityFromInst((phInst *)instance);
			if (entity) 
			{				
				//MAT34V_TO_MATRIX34(it->GetHitInst()->GetMatrix()).Transform(entity->GetBoundingSphereOffset(), vCenter);
				spdSphere sphere = entity->GetBoundSphere();
				fRadius = sphere.GetRadiusf();
				vCenter = VEC3V_TO_VECTOR3(sphere.GetCenter());
				isVehicle=true;
			}
		}
	}
	else 
	{
		static float TERRAIN_REJECTION_Z = 0.5f;
		if (it->GetHitPolyNormal().z > TERRAIN_REJECTION_Z) 
		{
			//Displayf("Rejecting %f", it->GetHitPolyNormal().z);
			return;
		} else
			GetIntersectionBoundRadiusAndCenter(mCurrMtx, it->GetHitPartIndex(), it->GetHitComponent(), MAT34V_TO_MATRIX34(it->GetHitInst()->GetMatrix()), it->GetHitInst()->GetArchetype()->GetBound(), fRadius, vCenter);
	}
	
	// Grow the obstacle sphere by this new sphere
	GrowSphere(vCenter, fRadius, vObstacleCenter, fObstacleRadius, isVehicle);
	PF_DRAW_ONLY(PFD_AvoidanceCapsule.DrawSphere(0.1f, it->GetHitPosition(), Color_red));	
}

////////////////////////////////////////////////////////////////////////////////


float hrsObstacleAvoidanceControl::GetSurfacePitch(const Matrix34 & mTestMtx, const float fTestLength)
{
#if __BANK
	if( TEST_PITCH_OVERRIDE != 0.0f )
	{
		// Return debug pitch value
		return TEST_PITCH_OVERRIDE;
	}
#endif

	// Clear the surface insts
	for( int i = 0; i < NUM_PROBES; i++)
	{
		m_pSurfaceInsts[i] = NULL;
	}

	// Perform probe tests to get terrain intersections and compute the pitch of the surface 
	// in front of the horse relative the mTestMtx.

	// Get the heights of the probe
	static float MIN_DIST_ABOVE = 1.0f;
	static float MIN_DIST_BELOW = 4.0f;
	float fProbeMaxExtraDistAbove = Max( (fTestLength * 0.5f) - MIN_DIST_ABOVE, 0.0f);
	//float fProbeMaxExtraDistBelow = Max(fTestLength - MIN_DIST_BELOW, 0.0f);

	// Cache the forward vector
	const Vector3 vForwards(mTestMtx.b);

	phIntersection intersections[NUM_PROBES];

	static float MAX_DISTANCE_BETWEEN_PROBES = 1.75f;
	static const int MIN_PROBES = 3;
	static const int MAX_PROBES = NUM_PROBES;
	int iNumProbes = Clamp(static_cast<int>(fTestLength / MAX_DISTANCE_BETWEEN_PROBES), MIN_PROBES, MAX_PROBES);
	bool cliffFound = false;
	m_vUnderHorseNormal = ZAXIS;
	float fPreviousZ = mTestMtx.d.z;
	for( int i = 0; i < iNumProbes; i++ )
	{
		float fRatio = static_cast<float>(i) / static_cast<float>(iNumProbes-1);

		// Construct start and end points
		Vector3 vStart(mTestMtx.d);
		vStart   += vForwards * fRatio * fTestLength;
		vStart.z += MIN_DIST_ABOVE + fProbeMaxExtraDistAbove * fRatio;
		Vector3 vEnd(vStart);
		vEnd.z    = fPreviousZ - (MIN_DIST_BELOW);// + fProbeMaxExtraDistBelow * fRatio);

		phSegment segment(vStart, vEnd);

		// Debug draw
		PF_DRAW_ONLY(PFD_SurfacePitchDetection.DrawLine(segment.A, segment.B));

		SurfaceProbeResult spr = DoSurfaceProbe(segment, intersections[i], (i == 0 ? NULL : &m_vUnderHorseNormal) );
		if( spr == SPR_HIT_GROUND )
		{
			PF_DRAW_ONLY(PFD_SurfacePitchDetection.DrawSphere(0.05f, VEC3V_TO_VECTOR3(intersections[i].GetPosition())));
			if( i == 0 )
			{
				m_vUnderHorseNormal = VEC3V_TO_VECTOR3( intersections[i].GetNormal() );
			}
			fPreviousZ = intersections[i].GetPosition().GetZf();
		}
		else 
		{
			if (spr == SPR_MISS)
			{
				//Do wall check
				if (i>0)
				{
					vEnd = vStart;
					Vector3 vStart(mTestMtx.d);
					vStart.z += MIN_DIST_ABOVE;	
					phSegment wallSegment(vStart, vEnd);					
					phIntersection iSect;
					if( !PHLEVEL->TestProbe(wallSegment, &iSect, NULL, ArchetypeFlags::GTA_MAP_TYPE_MOVER, TYPE_FLAGS_ALL, phLevelBase::STATE_FLAGS_ALL) )
					{
						cliffFound=true;
						if (m_vCliffPrediction.IsZero())
						{
							m_vCliffPrediction = segment.A;
						}
					} //else hit something blocking cliff
				}

			}
			
			if( i == 0 )
			{
				// Early out if the first probe fails
				// This is under the horses front legs, so should always succeed
				return 0.0f;
			}
		}
	}
	if (!cliffFound)
		m_vCliffPrediction.Zero();

	// Assign the inst of the first intersection
	m_pSurfaceInsts[0] = intersections[0].GetInstance();

	float fSmallestPitch = 0.0f;
	int iBest = -1;
	bool bHitStairs = false;

	for( int i = 1; i < iNumProbes; i++ )
	{
		if( intersections[i].IsAHit() )
		{
			Vector3 vDiff(VEC3V_TO_VECTOR3( intersections[i].GetPosition() - intersections[0].GetPosition()));
			// The pitch is the angle between the height and the length of the forwards vector
			float fXYMag = Sqrtf(vDiff.x*vDiff.x + vDiff.y*vDiff.y);
			float fPitch = rage::Atan2f(vDiff.z, fXYMag);
			fPitch = CanonicalizeAngle(fPitch);

			// Restrict the pitch - if bigger than this, then its an invalid result
			static /*const*/ float BIGGEST_PITCH = PI/4.0f;

			if (PGTAMATERIALMGR->GetPolyFlagStairs(intersections[i].GetMaterialId()))
				bHitStairs = true;

			if( iBest == -1 || (Abs(fPitch) < Abs(fSmallestPitch) && Abs(fPitch) < BIGGEST_PITCH) )
			{
				iBest = i;
				fSmallestPitch = Clamp(fPitch, -BIGGEST_PITCH, BIGGEST_PITCH);

				// Assign any insts used as part of the surface calculation
				m_pSurfaceInsts[i] = intersections[i].GetInstance();
			}
		}
	}

	m_pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected, bHitStairs);

	return fSmallestPitch;
}

////////////////////////////////////////////////////////////////////////////////

hrsObstacleAvoidanceControl::SurfaceProbeResult hrsObstacleAvoidanceControl::DoSurfaceProbe(const phSegment & segment, phIntersection & out, const Vector3 *underHorseNormal) const
{
	if( PHLEVEL->TestProbe(segment, &out, NULL, ArchetypeFlags::GTA_MAP_TYPE_MOVER, TYPE_FLAGS_ALL, phLevelBase::STATE_FLAGS_ALL) )
	{
		if( underHorseNormal )
		{
			static dev_float NORMAL_TOLERANCE = 0.7f;
			static dev_float NORMAL_DELTA_TOLERANCE = 0.f;
			// For a mover bound to be a valid result, the normal needs to be more or less pointing upwards, 
			// and it has to hit a poly bounds
			float flatness = VEC3V_TO_VECTOR3( out.GetNormal()).Dot(ZAXIS);
			float underHorseFlatness = underHorseNormal->Dot(ZAXIS);
			if( ( flatness - underHorseFlatness > NORMAL_DELTA_TOLERANCE || 
				flatness >= NORMAL_TOLERANCE ) && 
				(GetIntersectionHitPoly(out) || PGTAMATERIALMGR->GetPolyFlagStairs(out.GetMaterialId())) ) //or stairs which are type BOX
			{
				return SPR_HIT_GROUND;
			}
			else
			{
				out.Zero();
				return SPR_HIT_NON_GROUND;
			}
		}
		else
		{
			return SPR_HIT_GROUND;
		}
	}
	
// 	if( PHLEVEL->TestProbe(segment, &out, NULL, sagBoundFlags::BOUNDFLAG_TERRAIN) )
// 	{
// 		return true;
// 	}

	// Failed to find a valid intersection
	out.Zero();
	return SPR_MISS;
}

////////////////////////////////////////////////////////////////////////////////

bool hrsObstacleAvoidanceControl::GetIntersectionHitPoly(const phIntersection & intersection, const phBound * pBound) const
{
	if( intersection.IsAHit() )
	{
		// Use the passed in phBound first
		const phBound * pHitBound = pBound;
		if( !pHitBound )
		{
			if( intersection.GetInstance() )
			{
				// If it doesn't exist, revert to the intersection bound
				pHitBound = intersection.GetInstance()->GetArchetype()->GetBound();
			}
		}

		if( pHitBound )
		{
			switch( pHitBound->GetType() )
			{
			case phBound::BVH:
				{
					const phBoundBVH * bvh = static_cast<const phBoundBVH*>(pHitBound);
					const phPrimitive & prim = bvh->GetPrimitive(intersection.GetPartIndex());
					return prim.GetType() == PRIM_TYPE_POLYGON;
				}

			case phBound::COMPOSITE:
				{
					const phBoundComposite * cmp = static_cast<const phBoundComposite*>(pHitBound);
					return GetIntersectionHitPoly(intersection, cmp->GetBound(intersection.GetComponent()));
				}

			default:
				break;
			}
		}
	}

	// Not a poly bounds
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void hrsObstacleAvoidanceControl::GetIntersectionBoundRadiusAndCenter(const Matrix34 & currMat, const u16 partIndex, const u16 componentIndex, const Matrix34 & boundMat, const phBound * pBound, float & fOutRadius, Vector3 & vOutCenter) const
{
	// Invalidate fOutRadius
	fOutRadius = -1.0f;

	if( pBound )
	{
		switch( pBound->GetType() )
		{
		case phBound::SPHERE:
			{
				const phBoundSphere * sph = static_cast<const phBoundSphere*>(pBound);
				fOutRadius = sph->GetRadius();
				vOutCenter = boundMat.d;
			}
			break;

		case phBound::CAPSULE:
			{
				const phBoundCapsule * cap = static_cast<const phBoundCapsule*>(pBound);
				PF_DRAW_ONLY(PFD_AvoidanceBoundsDetection.DrawCapsule(boundMat, cap->GetLength(), cap->GetRadius()));

				Vector3 vWorldEnd0(boundMat.d + boundMat.c*cap->GetLength()* 0.5f);
				Vector3 vWorldEnd1(boundMat.d + boundMat.c*cap->GetLength()*-0.5f);
				Vector3 vWorldShaft(vWorldEnd1 - vWorldEnd0);
				vWorldShaft *= 0.5f;

				fOutRadius = cap->GetRadius() + Sqrtf(vWorldShaft.x*vWorldShaft.x+vWorldShaft.y*vWorldShaft.y);
				vOutCenter = boundMat.d;
			}
			break;

		case phBound::BOX:
			{
				const phBoundBox * box = static_cast<const phBoundBox*>(pBound);

				Vector3 vHalfBoxSize(VEC3V_TO_VECTOR3(box->GetBoxSize()) * 0.5f);
				PF_DRAW_ONLY(PFD_AvoidanceBoundsDetection.DrawBox(boundMat, vHalfBoxSize));

				boundMat.Transform3x3(vHalfBoxSize);
				fOutRadius = Sqrtf(vHalfBoxSize.x*vHalfBoxSize.x+vHalfBoxSize.y*vHalfBoxSize.y);
				vOutCenter = boundMat.d;
			}
			break;

		case phBound::BVH:
			{
				const phBoundBVH * bvh = static_cast<const phBoundBVH*>(pBound);
				const phPrimitive & prim = bvh->GetPrimitive(partIndex);

				PrimitiveType primType = prim.GetType();
				switch( primType )
				{
				case PRIM_TYPE_SPHERE:
					{
						const phPrimSphere & primSphere = prim.GetSphere();
						fOutRadius = primSphere.GetRadius();
						vOutCenter = VEC3V_TO_VECTOR3(bvh->GetVertex(primSphere.GetCenterIndex()));
					}
					break;

				case PRIM_TYPE_CAPSULE:
					{
						const phPrimCapsule & primCapsule = prim.GetCapsule();

						const Vector3 vCapsuleEnd0 = VEC3V_TO_VECTOR3(bvh->GetVertex(primCapsule.GetEndIndex0()));
						const Vector3 vCapsuleEnd1 = VEC3V_TO_VECTOR3(bvh->GetVertex(primCapsule.GetEndIndex1()));
#if __PFDRAW
						if( PFD_AvoidanceBoundsDetection.GetEnabled() )
						{
							Matrix34 capsuleMatrix;
							const Vector3 vCapsuleShaft(vCapsuleEnd1 - vCapsuleEnd0);
							// Check if the shaft axis is parallel to the y-axis.
							if( vCapsuleShaft.x != 0.0f && vCapsuleShaft.y != 0.0f )
							{
								capsuleMatrix.a.Cross(vCapsuleShaft, YAXIS);
								capsuleMatrix.a.Normalize();
								capsuleMatrix.c = vCapsuleShaft;
								capsuleMatrix.c.Normalize();
								capsuleMatrix.b.Cross(capsuleMatrix.a, capsuleMatrix.c);
							}
							else
							{
								capsuleMatrix.Identity();
							}
							capsuleMatrix.d.Average(vCapsuleEnd0, vCapsuleEnd1);
							capsuleMatrix.Dot(boundMat);
							PFD_AvoidanceBoundsDetection.DrawCapsule(capsuleMatrix, vCapsuleShaft.Mag(), primCapsule.GetRadius());
						}
#endif // __PFDRAW

						Vector3 vWorldEnd0(vCapsuleEnd0);
						boundMat.Transform(vWorldEnd0);
						Vector3 vWorldEnd1(vCapsuleEnd1);
						boundMat.Transform(vWorldEnd1);

						Vector3 vWorldShaft(vWorldEnd1 - vWorldEnd0);
						vWorldShaft *= 0.5f;

						fOutRadius = primCapsule.GetRadius() + Sqrtf(vWorldShaft.x*vWorldShaft.x+vWorldShaft.y*vWorldShaft.y);
						vOutCenter.Average(vWorldEnd0, vWorldEnd1);
					}
					break;

				case PRIM_TYPE_BOX:
					{
						const phPrimBox & primBox = prim.GetBox();

						const int iVertIndex0 = primBox.GetVertexIndex(0);
						const int iVertIndex1 = primBox.GetVertexIndex(1);
						const int iVertIndex2 = primBox.GetVertexIndex(2);
						const int iVertIndex3 = primBox.GetVertexIndex(3);

						const Vector3 vVert0 = VEC3V_TO_VECTOR3(bvh->GetVertex(iVertIndex0));
						const Vector3 vVert1 = VEC3V_TO_VECTOR3(bvh->GetVertex(iVertIndex1));
						const Vector3 vVert2 = VEC3V_TO_VECTOR3(bvh->GetVertex(iVertIndex2));
						const Vector3 vVert3 = VEC3V_TO_VECTOR3(bvh->GetVertex(iVertIndex3));

						const Vector3 vBoxX = VEC3_HALF * (vVert1 + vVert3 - vVert0 - vVert2);
						const Vector3 vBoxY = VEC3_HALF * (vVert0 + vVert3 - vVert1 - vVert2);
						const Vector3 vBoxZ = VEC3_HALF * (vVert0 + vVert1 - vVert2 - vVert3);
						const Vector3 vBoxCenter = VEC3_QUARTER * (vVert0 + vVert1 + vVert2 + vVert3);

						Matrix34 boxMatrix;
						boxMatrix.a.Normalize(vBoxX);
						boxMatrix.b.Normalize(vBoxY);
						boxMatrix.c.Normalize(vBoxZ);
						boxMatrix.d.Set(vBoxCenter);
						boxMatrix.Dot(boundMat);

						Vector3 vBoxSize(vBoxX.Mag(), vBoxY.Mag(), vBoxZ.Mag());
						Vector3 vHalfBoxSize(vBoxSize * 0.5f);
						PF_DRAW_ONLY(PFD_AvoidanceBoundsDetection.DrawBox(boxMatrix, vHalfBoxSize));

						boxMatrix.Transform3x3(vHalfBoxSize);
						fOutRadius = Sqrtf(vHalfBoxSize.x*vHalfBoxSize.x+vHalfBoxSize.y*vHalfBoxSize.y);
						vOutCenter = boxMatrix.d;
					}
					break;

				case PRIM_TYPE_POLYGON:
					{
						const phPolygon & poly = prim.GetPolygon();

						// If the radius is big, try to get a better result from the polygons we've hit
						// For small objects, we'll just default to the bound radius
						static const float BIG_RADIUS = 5.0f;
						if( bvh->GetRadiusAroundCentroid() > BIG_RADIUS )
						{
							// Transform verts by bound and calculate poly mid point
							Vector3 vVerts[POLY_MAX_VERTICES];

							// Get the bounds of the poly
							Matrix34 m(boundMat);
							m.DotTranspose(currMat);

							Vector3 vMin( FLT_MAX,  FLT_MAX,  FLT_MAX);
							Vector3 vMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
							for( int i = 0; i < POLY_MAX_VERTICES; i++)
							{
								m.Transform(VEC3V_TO_VECTOR3(bvh->GetVertex(poly.GetVertexIndex(i))), vVerts[i]);
								vMin.x = Min(vMin.x, vVerts[i].x);
								vMin.y = Min(vMin.y, vVerts[i].y);
								vMin.z = Min(vMin.z, vVerts[i].z);
								vMax.x = Max(vMax.x, vVerts[i].x);
								vMax.y = Max(vMax.y, vVerts[i].y);
								vMax.z = Max(vMax.z, vVerts[i].z);
							}

							// Treat the center point 
							vOutCenter.Average(vMin, vMax);

							fOutRadius = 0.0f;
							for( int i = 0; i < POLY_MAX_VERTICES; i++ )
							{
								float fDiffLenSq = (vVerts[i] - vOutCenter).Mag2();								
								if(fDiffLenSq > fOutRadius)
								{
									fOutRadius = fDiffLenSq;
								}
							}
							fOutRadius = Sqrtf(fOutRadius);

							currMat.Transform(vOutCenter);

#if __PFDRAW
							for( int i = 0; i < POLY_MAX_VERTICES; i++ )
							{
								int iNextVert = i+1;
								if( iNextVert == POLY_MAX_VERTICES )
								{
									iNextVert = 0;
								}

								PFD_AvoidanceBoundsDetection.DrawLine(vVerts[i], vVerts[iNextVert]);
							}
#endif // __PFDRAW
						}
					}
					break;

				default:
					break;
				}
			}
			break;

		case phBound::COMPOSITE:
			{
				const phBoundComposite * cmp = static_cast<const phBoundComposite*>(pBound);
				Matrix34 cmpBoundMat;
				cmpBoundMat.Dot(MAT34V_TO_MATRIX34(cmp->GetCurrentMatrix(componentIndex)), boundMat);
				return GetIntersectionBoundRadiusAndCenter(currMat, partIndex, componentIndex, cmpBoundMat, cmp->GetBound(componentIndex), fOutRadius, vOutCenter);
			}

		default:
			break;
		}
	}

	if( fOutRadius < 0.0f )
	{
		// Nothing set yet, default to pBound result
		fOutRadius = pBound->GetRadiusAroundCentroid();
		boundMat.Transform(VEC3V_TO_VECTOR3(pBound->GetCentroidOffset()), vOutCenter);
	}

	PF_DRAW_ONLY(PFD_AvoidanceBoundsDetection.DrawSphere(fOutRadius, vOutCenter));
}

////////////////////////////////////////////////////////////////////////////////

// TODO -- use spdSphere::GrowSphere
void hrsObstacleAvoidanceControl::GrowSphere(const Vector3 & vNewCenter, const float & fNewRadius, Vector3 & vInOutCenter, float & fInOutRadius, bool isVehicle) const
{
	if( fNewRadius > ( isVehicle ? VEHICLE_REJECT_RADIUS : REJECT_RADIUS ) )
	{
		// Obstacle too big
		return;
	}

	// If radius is 0.0f, just assign the new values over
	if( fInOutRadius == 0.0f )
	{
		vInOutCenter = vNewCenter;
		fInOutRadius = fNewRadius;
	}
	else
	{
		const Vector3 & v1 = vInOutCenter;
		// Make the height of both points the same, which makes this effectively in XZ space
		Vector3 v2 = vNewCenter;
		v2.z = vInOutCenter.z;

		// Get a direction vector between the points
		Vector3 vDir = v2 - v1;
		if( vDir.Mag2() > 0.0f )
		{
			vDir.NormalizeFast();

			// The points are extended by their radii 
			const Vector3 v1r = v1 - vDir * fInOutRadius;
			const Vector3 v2r = v2 + vDir * fNewRadius;

			// Get the distance between the points + radii
			const Vector3 vLength = v1r - v2r;

			// Get the length
			const float fLengthSq = vLength.Mag2();

			if( fLengthSq < square(fInOutRadius * 2.0f) )
			{
				// Do nothing, the new sphere is inside the current sphere
			}
			else if( fLengthSq < square(fNewRadius * 2.0f) )
			{
				// The new sphere encompasses the old sphere
				vInOutCenter = vNewCenter;
				fInOutRadius = fNewRadius;
			}
			else
			{
				// Expand to fit the new sphere
				vInOutCenter.Average(v1r, v2r);
				fInOutRadius = Sqrtf(fLengthSq) * 0.5f;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// Tune /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

hrsObstacleAvoidanceControl::Tune::Tune()
{
}

////////////////////////////////////////////////////////////////////////////////

void hrsObstacleAvoidanceControl::Tune::AddWidgets(bkBank & BANK_ONLY(b))
{
#if __BANK
	b.PushGroup("Obstacle Avoidance Tuning", false);
	b.PopGroup();
#endif
}

#endif // ENABLE_HORSE