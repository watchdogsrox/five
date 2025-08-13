//
// name:		NetBlenderVehicle.cpp
// written by:	Daniel Yelland
// description:	Network blender used by vehicles

#include "network/objects/prediction/NetBlenderVehicle.h"

#include "network/Network.h"
#include "network/objects/entities/NetObjTrailer.h"
#include "network/objects/entities/NetObjVehicle.h"
#include "peds/Ped.h"
#include "peds/Pedintelligence.h"
#include "physics/worldprobe/WorldProbe.h"
#include "vehicles/Planes.h"
#include "vehicles/Trailer.h"
#include "vehicles/Vehicle.h"
#include "vehicleAi/Vehicleintelligence.h"

CompileTimeAssert(sizeof(CNetBlenderVehicle) <= LARGEST_NET_PHYSICAL_BLENDER_CLASS);

NETWORK_OPTIMISATIONS()
NETWORK_OBJECT_OPTIMISATIONS()

CNetBlenderVehicle::CNetBlenderVehicle(CVehicle *vehicle, CVehicleBlenderData *blenderData) :
CNetBlenderPhysical(vehicle, blenderData)
, m_VelocityLastFrame(VEC3_ZERO)
, m_TargetSteerAngle(0.0f)
, m_BlendingTrailer(false)
, m_AllowAngularVelocityUpdate(true)
, m_DoExtraOrientationBlendingChecks(false)
, m_HighSpeedEdgeFallDetector(false)
, m_LastHighSpeedEdgeFallMapTest(0)
, m_WasDoingFixedPhysicsBlending(false)
BANK_ONLY(, m_LastFrameProbeSubmitted(0))
BANK_ONLY(, m_NumProbesInConsecutiveFrames(0))
{
}

CNetBlenderVehicle::~CNetBlenderVehicle()
{
}


void CNetBlenderVehicle::OnOwnerChange(blenderResetFlags resetFlag)
{
	CNetBlenderPhysical::OnOwnerChange(resetFlag);

	m_VelocityLastFrame = GetVelocityFromObject();
}

void CNetBlenderVehicle::Update()
{
    if (!IsBlendingOn())
        return;

    CVehicle *vehicle = SafeCast(CVehicle, m_pPhysical);

    if(vehicle)
    {
		bool initialPredictAcceleration = GetBlenderData()->m_PredictAcceleration;
		// hacky: stop predicting acceleration for this plane when it's stationary, it can do some weird things
		if(vehicle->GetModelIndex() == MI_PLANE_BOMBUSHKA || vehicle->GetModelIndex() == MI_PLANE_TITAN || vehicle->GetModelIndex() == MI_PLANE_VOLATOL)
		{
			CPlane* pPlane = static_cast<CPlane*>(vehicle);
			if (pPlane && pPlane->HasContactWheels())
			{		
				const unsigned MAX_VEL_UPDATE_DELAY = 500;
				float lastVelMag = GetLastVelocityReceived().Mag();
				u32 lastVelTimestamp = 0;
				u32 lastPosTimestamp = 0;								
				GetLastVelocityReceived(&lastVelTimestamp);	
				GetLastPositionReceived(&lastPosTimestamp);
				if(lastVelMag < 0.1f && (lastVelTimestamp - lastPosTimestamp > MAX_VEL_UPDATE_DELAY))
				{
					GetBlenderData()->m_PredictAcceleration = false;
				}
			}
		}

        UpdateSteerAngle();

        // fixed vehicle have a custom method of network blending (because vehicle blending moves
        // the vehicle around using velocities normally, and fixed vehicles have 0 velocity)
        if(vehicle->GetIsAnyFixedFlagSet())
        {
			m_WasDoingFixedPhysicsBlending = true;
            DoFixedPhysicsBlend();
            return;
        }
		else
		{
			// We just stopped doing fixed physics blending, so give the vehicle the velocity it has as a start
			if (m_WasDoingFixedPhysicsBlending)
			{
				m_WasDoingFixedPhysicsBlending = false;

				vehicle->SetVelocity(GetLastVelocityReceived());	
				gnetDebug2("%s We just stopped doing fixed physics blending, setting the vel of vehicle to %f %f %f. The velocity is now %f %f %f", 
					vehicle->GetNetworkObject() ? vehicle->GetNetworkObject()->GetLogName() : "Unknown vehicle", 
					GetLastVelocityReceived().GetX(), GetLastVelocityReceived().GetY(), GetLastVelocityReceived().GetZ(),
					vehicle->GetVelocity().GetX(), vehicle->GetVelocity().GetY(), vehicle->GetVelocity().GetZ());
			}			
		}

        const eVehicleDummyMode currentMode = vehicle->GetVehicleAiLod().GetDummyMode();

        if(currentMode == VDM_SUPERDUMMY)
        {
            // super dummy vehicles don't move very physically on the local machine (they change velocities
            // in a very quick and steppy way) As locally these changes are not smooth we need to make
            // larger velocity changes via the blender too for stopping vehicles
            Vector3 currentPosition    = GetPositionFromObject();
            Vector3 currentVelocity    = GetVelocityFromObject();
            Vector3 predictionPosition = GetCurrentPredictedPosition();
            Vector3 targetVelocity     = GetLastVelocityReceived();
            Vector3 deltaPos           = currentPosition - predictionPosition;

            // if behind and slower or ahead and faster set directly to target velocity
            bool targetStopped = targetVelocity.Mag2() < 0.001f;

            if(targetStopped)
            {
                Vector3 normalizedTargetVelocity = targetVelocity;
                normalizedTargetVelocity.Normalize();
                deltaPos.Normalize();

                static const float MAX_SUPER_DUMMY_VEL_CHANGE_SQR = 5.0f;

                Vector3 velocityDiff = targetVelocity - currentVelocity;

                if(velocityDiff.Mag2() > rage::square(MAX_SUPER_DUMMY_VEL_CHANGE_SQR))
                {
                    velocityDiff.Normalize();
                    velocityDiff.Scale(MAX_SUPER_DUMMY_VEL_CHANGE_SQR);
                }

                Vector3 newVelocity = currentVelocity + velocityDiff;

                SetVelocityOnObject(newVelocity);
            }
        }

		HighSpeedEdgeFallDetector();

        CNetBlenderPhysical::Update();

        m_VelocityLastFrame = GetVelocityFromObject();

        // if physics is disabled in superdummy mode we need to move the car ourselves here
        if((currentMode == VDM_SUPERDUMMY) && CVehicleAILodManager::ms_bDisablePhysicsInSuperDummyMode && vehicle->GetIsStatic())
        {
            Vector3 currentPosition = GetPositionFromObject();
            Vector3 newPosition     = currentPosition + (m_VelocityLastFrame * fwTimer::GetTimeStep());
            SetPositionOnObject(newPosition, false);

            CVehicle *dummyAttached = vehicle->GetDummyAttachmentChild(0);

            if(dummyAttached && dummyAttached->GetNetworkObject() && dummyAttached->InheritsFromTrailer())
            {
                CTrailer *trailer = static_cast<CTrailer *>(dummyAttached);

                CNetBlenderVehicle *netBlenderTrailer = SafeCast(CNetBlenderVehicle, trailer->GetNetworkObject()->GetNetBlender());

                currentPosition = netBlenderTrailer->GetPositionFromObject();
                newPosition     = currentPosition + (m_VelocityLastFrame * fwTimer::GetTimeStep());
                netBlenderTrailer->SetPositionOnObject(newPosition, false);
            }
        }
		
        // blend the position of any attached trailers
        CTrailer *trailer = vehicle->GetAttachedTrailer();
        if(trailer)
        {
            DoTrailerBlend(*vehicle, *trailer);
        }

		GetBlenderData()->m_PredictAcceleration = initialPredictAcceleration;
    }
}

void CNetBlenderVehicle::HighSpeedEdgeFallDetector()
{
	if (m_HighSpeedEdgeFallDetector)
	{		
		CVehicle *vehicle = SafeCast(CVehicle, m_pPhysical);

		if (vehicle->HasContactWheels())
		{
			return;
		}

		static const int CHECK_FREQUENCY = 80;

		static const float MAX_Z_VEL_THRESHOLD = 1.5f;
		static const float MAX_Z_POS_THRESHOLD = 1.0f;
		const Vector3 targetVelocity  = GetLastVelocityReceived();
		const Vector3 currentVelocity = vehicle->GetVelocity();
		const Vector3 targetPosition  = GetLastPositionReceived();
		const Vector3 &currPos        = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition());

		float velocityZDelta = targetVelocity.GetZ() - currentVelocity.GetZ();
		float positionZDelta = targetPosition.GetZ() - currPos.GetZ();
		// Locally we are falling (possibly went over the edge)
		if ((velocityZDelta > MAX_Z_VEL_THRESHOLD) && (positionZDelta >= MAX_Z_POS_THRESHOLD))
		{
			u32 currentTime = fwTimer::GetTimeInMilliseconds();
			gnetDebug2("CNetBlenderVehicle::HighSpeedEdgeFallDetector() %s triggered by velocityZDelta %f positionZDelta %f", vehicle->GetNetworkObject()->GetLogName(), velocityZDelta, positionZDelta);
			if (m_LastHighSpeedEdgeFallMapTest + CHECK_FREQUENCY <= currentTime) 
			{
				m_LastHighSpeedEdgeFallMapTest = currentTime;
				if(TestForMapCollision(targetPosition, currPos))
				{
					gnetDebug2("CNetBlenderVehicle::HighSpeedEdgeFallDetector() %s map collision confirmed, going straight to target!", vehicle->GetNetworkObject()->GetLogName());
					GoStraightToTarget();
				}				
			}		
		}			
	}
}

void CNetBlenderVehicle::GoStraightToTarget()
{
    CVehicle *vehicle = SafeCast(CVehicle, m_pPhysical);

    if(vehicle && vehicle->InheritsFromTrailer() && vehicle->GetAttachParent() && vehicle->GetAttachParent()->GetType() == ENTITY_TYPE_VEHICLE)
    {
        CPhysical          *attachParent = SafeCast(CPhysical, vehicle->GetAttachParent());
        CNetBlenderVehicle *netBlender   = attachParent->GetNetworkObject() ? SafeCast(CNetBlenderVehicle, attachParent->GetNetworkObject()->GetNetBlender()) : 0;

        if(netBlender)
        {
            netBlender->GoStraightToTarget();
        }

        vehicle->m_vehControls.SetSteerAngle( m_TargetSteerAngle );
    }
    else
    {
        CNetBlenderPhysical::GoStraightToTarget();
    }

    m_VelocityLastFrame = GetVelocityFromObject();
}

bool CNetBlenderVehicle::DisableZBlending() const
{
	CVehicle *vehicle = SafeCast(CVehicle, m_pPhysical);

	if (vehicle)
	{
		const u32 modelNameHash = vehicle->GetModelNameHash();

		if (modelNameHash == ATSTRINGHASH("TULA", 0x3E2E4F8A) || modelNameHash == ATSTRINGHASH("seabreeze", 0xE8983F9F))
		{
			CNetObjVehicle *netObjVehicle = SafeCast(CNetObjVehicle, vehicle->GetNetworkObject());

			if (netObjVehicle)
			{
				bool locallyInWater  = vehicle->GetIsInWater();
				bool remotelyInWater = netObjVehicle->IsNetworkObjectInWater();

				if(locallyInWater || remotelyInWater)
				{
					return true;
				}
			}			
		}
	}

	return CNetBlenderPhysical::DisableZBlending();
}


void CNetBlenderVehicle::ProcessPostPhysics()
{
    CNetBlenderPhysical::ProcessPostPhysics();

    // if we are doing a fixed physics blend we need to ensure
    // the orientation of any attached trailer is blended
    CVehicle *vehicle = SafeCast(CVehicle, m_pPhysical);

    if(vehicle && vehicle->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK))
    {
        CTrailer *trailer = vehicle->GetAttachedTrailer();

        if(trailer && trailer->GetNetworkObject())
        {
            CNetBlenderPhysical *netBlenderPhysical = SafeCast(CNetBlenderPhysical, trailer->GetNetworkObject()->GetNetBlender());

            netBlenderPhysical->ProcessPostPhysics();
        }
    }
}

void CNetBlenderVehicle::UpdateBlenderMode(const float distanceFromTarget)
{
    CNetBlenderPhysical::UpdateBlenderMode(distanceFromTarget);

    CVehicle *vehicle = SafeCast(CVehicle, m_pPhysical);

    static float DIST_THRESHOLD  = 0.3f;
    static float STEER_THRESHOLD = 0.15f;

    if(vehicle && vehicle->InheritsFromBike() && 
      (fabsf(vehicle->m_vehControls.m_steerAngle) >= STEER_THRESHOLD) &&
      (distanceFromTarget >= DIST_THRESHOLD))
    {
        SetBlenderMode(BLEND_MODE_HIGH_SPEED);
    }
}

void CNetBlenderVehicle::ProcessEndOfBlendTime(Vector3 &objectPosition)
{
	CVehicle *vehicle = SafeCast(CVehicle, m_pPhysical);
	if (vehicle)
	{
		if (vehicle->GetModelIndex() == MI_CAR_DELUXO)
		{
			const bool bIsDeluxoFlying = vehicle->GetSpecialFlightModeRatio() >= 1.0f - SMALL_FLOAT;
			if (bIsDeluxoFlying)
			{
				return;
			}
		}
	}

	netBlenderLinInterp::ProcessEndOfBlendTime(objectPosition);
}


#if __BANK

float CNetBlenderVehicle::GetParentVehicleOffsetDelta(bool &blendedTooFar) const
{
    CVehicle *vehicle = SafeCast(CVehicle, m_pPhysical);

    float offsetDelta = 0.0f;

    if(vehicle && vehicle->InheritsFromTrailer())
    {
        CTrailer *trailer       = SafeCast(CTrailer, vehicle);
        CVehicle *parentVehicle = 0;

        if(trailer && trailer->GetAttachParent() && static_cast<CPhysical *>(trailer->GetAttachParent())->GetIsTypeVehicle())
        {
            parentVehicle = SafeCast(CVehicle, trailer->GetAttachParent());
        }

        if(trailer && parentVehicle)
        {
            Vector3 trailerPos = VEC3V_TO_VECTOR3(trailer->GetTransform().GetPosition());
            Vector3 parentPos  = VEC3V_TO_VECTOR3(parentVehicle->GetTransform().GetPosition());
            Vector3 offset     = trailerPos - parentPos;
            Vector3 offsetDiff = offset - m_ParentVehicleOffset;
            offsetDelta = offsetDiff.Mag();

            Vector3 velocityDir = GetLastVelocityReceived();
            velocityDir.Normalize();
            offsetDiff.Normalize();

            blendedTooFar = velocityDir.Dot(offsetDiff) > 0.0f;
        }
    }

    return offsetDelta;
}

#endif // __BANK

bool CNetBlenderVehicle::CanBlendWhenAttached() const
{
    CVehicle *vehicle = SafeCast(CVehicle, m_pPhysical);

    if(vehicle && vehicle->InheritsFromTrailer())
    {
        CNetObjTrailer *netObjTrailer = SafeCast(CNetObjTrailer, vehicle->GetNetworkObject());
        CVehicle       *parentVehicle = netObjTrailer ? netObjTrailer->GetParentVehicle() : 0;
        
        if(parentVehicle && parentVehicle->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK))
        {
            return true;
        }
    }

    return CNetBlenderPhysical::CanBlendWhenAttached();
}

bool CNetBlenderVehicle::CanApplyNewOrientationMatrix(const Matrix34 &newMatrix) const
{
    if(m_DoExtraOrientationBlendingChecks)
    {
        CVehicle *vehicle = SafeCast(CVehicle, m_pPhysical);

        if(vehicle && vehicle->GetIntelligence())
        {
            bool distanceExceeded = false;

            const unsigned MAX_VEHICLES_TO_PROBE = 10;
            const CEntity *vehiclesToProbe[MAX_VEHICLES_TO_PROBE];
            unsigned  numVehiclesToProbe = 0;

            const CEntityScannerIterator nearbyVehicles = vehicle->GetIntelligence()->GetVehicleScanner().GetIterator();

            for(const CEntity* nearbyEntity = nearbyVehicles.GetFirst(); nearbyEntity && !distanceExceeded; nearbyEntity = nearbyVehicles.GetNext())
            {
                const CVehicle* nearbyVehicle = static_cast<const CVehicle*>(nearbyEntity);

                if(nearbyVehicle && nearbyVehicle->GetNetworkObject())
                {
                    float distanceSqr = (VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(nearbyVehicle->GetTransform().GetPosition())).Mag2();

                    if(distanceSqr < rage::square(GetBlenderData()->m_NearbyVehicleThreshold))
                    {
                        if(numVehiclesToProbe < MAX_VEHICLES_TO_PROBE)
                        {
                            vehiclesToProbe[numVehiclesToProbe] = nearbyVehicle;
                            numVehiclesToProbe++;
                        }
                    }
                    else
                    {
                        distanceExceeded = true;
                    }
                }
            }

            if(numVehiclesToProbe > 0)
            {
                WorldProbe::CShapeTestBoundDesc boundTestDesc;
#if __BANK
                WorldProbe::CShapeTestFixedResults<1> boundTestResults;
                boundTestDesc.SetResultsStructure(&boundTestResults);
#endif // __BANK
                boundTestDesc.SetBoundFromEntity(vehicle);
                boundTestDesc.SetTransform(&newMatrix);
                boundTestDesc.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE);
                boundTestDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
                boundTestDesc.SetIncludeEntities(vehiclesToProbe, numVehiclesToProbe, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
                boundTestDesc.SetContext(WorldProbe::ENetwork);

                if(WorldProbe::GetShapeTestManager()->SubmitTest(boundTestDesc))
                {
                    gnetDebug2("CanApplyNewOrientationMatrix - Can't apply new orientation matrix for %s as it would lead to objects intersecting", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "Unknown vehicle");

#if __BANK
                    if(boundTestResults.GetResultsReady() && boundTestResults[0].GetHitDetected())
                    {
                        CEntity *hitEntity = boundTestResults[0].GetHitEntity();

                        if(hitEntity && hitEntity->GetIsTypeVehicle())
                        {
                            CVehicle *hitVehicle = SafeCast(CVehicle, hitEntity);
                            phInst   *inst1      = vehicle->GetFragInst() ? vehicle->GetFragInst() : vehicle->GetCurrentPhysicsInst();
                            phInst   *inst2      = hitVehicle->GetFragInst() ? hitVehicle->GetFragInst() : hitVehicle->GetCurrentPhysicsInst();
                            phBound  *bound1     = inst1->GetArchetype()->GetBound();
                            phBound  *bound2     = inst2->GetArchetype()->GetBound();

                            NetworkDebug::AddOrientationBlendFailure(bound1, bound2, vehicle->GetMatrix(), hitVehicle->GetMatrix());
                        }
                    }
#endif // __BANK

                    return false;
                }
            }
        }
    }

    return CNetBlenderPhysical::CanApplyNewOrientationMatrix(newMatrix);
}

void CNetBlenderVehicle::UpdateOrientationBlending()
{
    CVehicle *vehicle = SafeCast(CVehicle, m_pPhysical);

    if(vehicle)
    {
        const eVehicleDummyMode currentMode = vehicle->GetVehicleAiLod().GetDummyMode();

        if(currentMode != VDM_SUPERDUMMY)
        {
            CNetBlenderPhysical::UpdateOrientationBlending();
        }
    }
}

void CNetBlenderVehicle::UpdateAngularVelocity()
{
    CVehicle *vehicle = SafeCast(CVehicle, m_pPhysical);

    if(vehicle)
    {
        const eVehicleDummyMode currentMode = vehicle->GetVehicleAiLod().GetDummyMode();

        if(currentMode != VDM_SUPERDUMMY)
        {
            if(m_AllowAngularVelocityUpdate)
            {
                CNetBlenderPhysical::UpdateAngularVelocity();
				const u32 modelNameHash = vehicle->GetModelIndex();

				bool isAnAutomobileBike = (modelNameHash == MI_TRIKE_RROCKET);

				// Hack. We don't know why this is happening and have no clue how to make a proper fix. The model check is just the icing on the cake.
				if (((vehicle->InheritsFromBike() && vehicle->ContainsPlayer()) || isAnAutomobileBike) && m_predictionData.GetSnapshotPresent().m_angVelocity.Mag2() < 0.01f)
				{
					SetAngVelocityOnObject(VEC3_ZERO);			
				}

				if ((vehicle->InheritsFromBike() && vehicle->ContainsPlayer()) || isAnAutomobileBike)
				{
					Quaternion currentOrientation;
					Quaternion targetOrientation;
					Matrix34 newMatrix = GetMatrixFromObject();
					GetQuaternionsFromMatrices(newMatrix, m_predictionData.GetSnapshotFuture().m_matrix, currentOrientation, targetOrientation);

					float angleDiff = targetOrientation.RelAngle(currentOrientation);

					TUNE_FLOAT(BIKE_FIX_MIN_ANGLE, 0.02f, -1.f, 1.f, 0.01f);

					if(angleDiff > BIKE_FIX_MIN_ANGLE && m_bBlendingOrientation == false)
					{
						m_bBlendingOrientation = true;
						m_blendPhysicalStartTime = NetworkInterface::GetNetworkTime();
					}
				}			
            }
            else
            {
                // we are now simulating physics on this vehicle when it is in super-dummy mode
                // on the local machine. This can cause the angular velocity to overshoot so we may
                // need an adjustment here
                Matrix34 matrixPast       = CNetBlenderPhysical::m_predictionData.GetSnapshotPast().m_matrix;
                Matrix34 matrixPresent    = CNetBlenderPhysical::m_predictionData.GetSnapshotPresent().m_matrix;
                u32      pastTimestamp    = CNetBlenderPhysical::m_predictionData.GetSnapshotPast().m_matrixTimestamp;
                u32      presentTimestamp = CNetBlenderPhysical::m_predictionData.GetSnapshotPresent().m_matrixTimestamp;

                Matrix34 inverseMatrixPast = matrixPast;
                inverseMatrixPast.Inverse();
                Matrix34 matrixDiff = matrixPresent;
                matrixDiff.Dot(inverseMatrixPast);

                Vector3 eulersDiff = matrixDiff.GetEulers();
                u32     timeDiff   = (presentTimestamp > pastTimestamp) ? (presentTimestamp - pastTimestamp) : 0;

                float timeElapsed = timeDiff / 1000.0f * fwTimer::GetTimeWarpActive();

                const float MAX_TIME_DIFF = 1.5f;

                if(timeElapsed != 0 && timeElapsed < MAX_TIME_DIFF)
                {
                    Vector3 angularVelocity = vehicle->GetAngVelocity();
                    Vector3 predictedAngVel = eulersDiff / timeElapsed;                    

                    if(angularVelocity.Mag2() > predictedAngVel.Mag2())
                    {
                        TUNE_FLOAT(ANG_VEL_BLEND_RATE, 0.2f, 0.0f, 1.0f, 0.05f);

                        float angVelMag    = angularVelocity.Mag();
                        float predictedMag = predictedAngVel.Mag();
                        float angVelDiff   = angVelMag - predictedMag;

                        TUNE_FLOAT(MIN_ANG_VEL_CHANGE, -2.0f, -TWO_PI, TWO_PI, PI/180.0f);
                        TUNE_FLOAT(MAX_ANG_VEL_CHANGE, 2.0f,  -TWO_PI, TWO_PI, PI/180.0f);

                        if(angVelDiff > MIN_ANG_VEL_CHANGE && angVelDiff < MAX_ANG_VEL_CHANGE)
                        {
                            float newAngVelMag = angVelMag - angVelDiff * ANG_VEL_BLEND_RATE;

                            angularVelocity.Normalize();
                            angularVelocity.Scale(newAngVelMag);

                            SetAngVelocityOnObject(angularVelocity);
                        }
                    }
                }
            }
        }
    }
}

bool CNetBlenderVehicle::OnlyPredictRelativeToForwardVector() const
{
    bool predictForwardOnly = false;

    // we only predict relative to the forward vector for vehicles in the
    // super dummy mode. This is because they don't have
    // gravity applied to them which can lead to vehicles bouncing at low update rates
    CVehicle *vehicle = SafeCast(CVehicle, m_pPhysical);

    if(vehicle)
    {
        const eVehicleDummyMode currentMode = vehicle->GetVehicleAiLod().GetDummyMode();

        if(currentMode == VDM_SUPERDUMMY)
        {
            predictForwardOnly = true;
        }
    }

    return predictForwardOnly;
}

bool CNetBlenderVehicle::PreventBlendingAwayFromVelocity(float positionDelta) const
{
    CVehicle *vehicle = SafeCast(CVehicle, m_pPhysical);

	// Exclude jetpack from this
    if(vehicle && GetIsOnScreen() && vehicle->GetModelIndex() != MI_JETPACK_THRUSTER)
    {
        static const float POSITION_THRESHOLD = 0.5f;
        static const float VELOCITY_THRESHOLD_SQR = rage::square(1.0f);
        // prevent blending the vehicle backwards when it is traveling at low speeds and is only slightly out of sync
        if(positionDelta <= POSITION_THRESHOLD && vehicle->GetVelocity().Mag2() <= VELOCITY_THRESHOLD_SQR)
        {
            return true;
        }
    }

    return false;
}

bool CNetBlenderVehicle::SetPositionOnObject(const Vector3 &position, bool warp)
{
	bool success = true;
    Vector3 newPosition = position;

    CVehicle *vehicle = SafeCast(CVehicle, m_pPhysical);

    if(warp && !vehicle->IsDummy())
    {
        Vector3 lastPosReceived = GetLastPositionReceived();

        if(!lastPosReceived.IsClose(newPosition, 0.1f))
        {
            // If this vehicle contains a real player, we want to make sure we don't predict through 
			// the ground or buildings, so we need to do another probe here. Ideally this would be 
			// done for all vehicles, but we don't want the network code doing too many world probes
            if(IsImportantVehicle(vehicle))
            {
				// We don't want to test for map collision if the vehicle is a quad bike because of certain 
				// cases when a quad amphibious is warped from water during races to the start point 
				bool shouldTestForMapCollision = true;
				if (vehicle->InheritsFromAmphibiousQuadBike() && IsBoatBlender())
				{
					shouldTestForMapCollision = false;
				}

				if (shouldTestForMapCollision)
				{
					if(TestForMapCollision(lastPosReceived, newPosition))
					{
						return false;
					}
				}             
            }

            const spdAABB &boundBox = vehicle->GetBaseModelInfo()->GetBoundingBox();
            float halfHeight = ((boundBox.GetMax().GetZf() - boundBox.GetMin().GetZf()) * 0.5f);

            Vector3 adjust = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetC()) * halfHeight;

            newPosition.x += adjust.x;
            newPosition.y += adjust.y;

            const float    probeHalfLength   = 1.5f;
            const unsigned NUM_INTERSECTIONS = 3;

            WorldProbe::CShapeTestProbeDesc probeDesc;
	        WorldProbe::CShapeTestHitPoint probeIsects[NUM_INTERSECTIONS];
	        WorldProbe::CShapeTestResults probeResult(probeIsects, NUM_INTERSECTIONS);
	        probeDesc.SetStartAndEnd(Vector3(newPosition.x, newPosition.y, newPosition.z + probeHalfLength), Vector3(newPosition.x, newPosition.y, newPosition.z - probeHalfLength));
	        probeDesc.SetResultsStructure(&probeResult);
	        probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	        probeDesc.SetContext(WorldProbe::ENetwork);

#if __BANK
                // ensure we are not continually doing world probes for this vehicle
                unsigned currentFrameCount = fwTimer::GetFrameCount();

                if((currentFrameCount > m_LastFrameProbeSubmitted) && ((currentFrameCount - m_LastFrameProbeSubmitted) <= 1))
                {
                    m_NumProbesInConsecutiveFrames++;
                }
                else
                {
                    m_NumProbesInConsecutiveFrames = 0;
                }

                m_LastFrameProbeSubmitted = currentFrameCount;

                const unsigned MAX_PROBES_BEFORE_ASSERT = 3;

                if(m_NumProbesInConsecutiveFrames >= MAX_PROBES_BEFORE_ASSERT)
                {
                    gnetWarning("Performing too many world probes via the vehicle network blender for %s!", vehicle->GetNetworkObject() ? vehicle->GetNetworkObject()->GetLogName() : "Non-networked vehicle");
                }
#endif // __BANK

            if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
            {
                gnetDebug2("Frame %d: Performing probe for %s", fwTimer::GetFrameCount(), vehicle->GetNetworkObject() ? vehicle->GetNetworkObject()->GetLogName() : "Non-networked vehicle");

                float groundPos = probeResult[0].GetHitPosition().z;

                if(probeResult.GetNumHits() > 1)
                {
                    float bestDiff = fabs(groundPos - newPosition.z);

                    for(unsigned index = 1; index < probeResult.GetNumHits(); index++)
                    {
                        float newZ    = probeResult[index].GetHitPosition().z;
                        float newDiff = fabs(newZ - newPosition.z);

                        if(newDiff < bestDiff)
                        {
                            bestDiff  = newDiff;
                            groundPos = newZ;
                        }
                    }
                }

                newPosition.z = groundPos + adjust.z;
            }
        }

        InformAttachedPedsOfWarp(newPosition);
    }

    success = CNetBlenderPhysical::SetPositionOnObject(newPosition, warp);

    if(warp)
    {
        CTrailer *trailer = vehicle->GetAttachedTrailer();

        if(trailer)
        {
            UpdateTrailerAfterWarp(*vehicle, *trailer);
        }
    }

	return success;
}

void CNetBlenderVehicle::SetVelocityOnObject(const Vector3 &velocity)
{
    CVehicle *vehicle = SafeCast(CVehicle, m_pPhysical);

	Vector3 velocityToSet = velocity;

	if (vehicle->GetModelIndex() == MI_CAR_DELUXO)
	{
		const bool bIsDeluxoFlying = vehicle->GetSpecialFlightModeRatio() >= 1.0f - SMALL_FLOAT;
		if (bIsDeluxoFlying)
		{
			if (IsClose(velocityToSet.GetZ(), 0.0f, 0.25f))
			{
				velocityToSet.SetZ(0.0f);

				if(velocityToSet.IsZero())
				{
					return;
				}
			}
		}
	}

    const eVehicleDummyMode currentMode = vehicle->GetVehicleAiLod().GetDummyMode();

    if(currentMode == VDM_SUPERDUMMY && CVehicleAILodManager::ms_bDisablePhysicsInSuperDummyMode && vehicle->GetIsStatic())
    {
        gnetAssertf(velocityToSet.Mag2() < 62500.0f, "Setting too high a velocity on superdummy %s!", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "vehicle");
        vehicle->SetSuperDummyVelocity(velocityToSet);

        CVehicle *dummyAttached = vehicle->GetDummyAttachmentChild(0);

        if(dummyAttached && dummyAttached->InheritsFromTrailer())
        {
            CTrailer *trailer = static_cast<CTrailer *>(dummyAttached);
            trailer->SetSuperDummyVelocity(velocityToSet);
        }
    }
    else
    {
        CNetBlenderPhysical::SetVelocityOnObject(velocityToSet);
    }
}

void CNetBlenderVehicle::SetAngVelocityOnObject(const Vector3 &angVelocity)
{
    CVehicle *vehicle = SafeCast(CVehicle, m_pPhysical);

    const eVehicleDummyMode currentMode = vehicle->GetVehicleAiLod().GetDummyMode();

    bool inactiveSuperDummy = (currentMode == VDM_SUPERDUMMY) && CVehicleAILodManager::ms_bDisablePhysicsInSuperDummyMode && vehicle->GetIsStatic();

    if(!inactiveSuperDummy)
    {
        CNetBlenderPhysical::SetAngVelocityOnObject(angVelocity);
    }
}

float CNetBlenderVehicle::GetOrientationBlendValue() const
{
    CVehicle *vehicle = SafeCast(CVehicle, m_pPhysical);

    TUNE_FLOAT(STEER_THRESHOLD, 0.15f, 0.0f, PI, 0.1f);
    TUNE_FLOAT(FASTER_ORIENTATION_BLEND, 0.25f, 0.0f, 1.0f, 0.1f);
    TUNE_FLOAT(VEL_THRESHOLD, 15.0f, 0.0f, 20.0f, 0.1f);

    float VEL_THRESHOLD_SQR = rage::square(VEL_THRESHOLD);

    if(vehicle && vehicle->GetVelocity().Mag2() > VEL_THRESHOLD_SQR &&
      (fabsf(vehicle->m_vehControls.m_steerAngle) >= STEER_THRESHOLD))
    {
        return FASTER_ORIENTATION_BLEND;
    }

    return CNetBlenderPhysical::GetOrientationBlendValue();
}

void CNetBlenderVehicle::UpdateSteerAngle()
{
    CVehicle *vehicle = SafeCast(CVehicle, m_pPhysical);

    if(vehicle)
    {
        float currentSteerAngle  = vehicle->m_vehControls.GetSteerAngle();
        float steerAngleDelta    = m_TargetSteerAngle - currentSteerAngle;
        float steerAngleDeltaMag = fabs(steerAngleDelta);

        if(steerAngleDeltaMag > 0.01f)
        {
            if(steerAngleDeltaMag <= GetBlenderData()->m_SteerAngleMin || steerAngleDeltaMag >= GetBlenderData()->m_SteerAngleMax || !GetIsOnScreen())
            {
                vehicle->m_vehControls.SetSteerAngle(m_TargetSteerAngle);
            }
            else
            {
                currentSteerAngle += (steerAngleDelta * GetBlenderData()->m_SteerAngleRatio);

                vehicle->m_vehControls.SetSteerAngle(currentSteerAngle);
            }
        }
    }
}

void CNetBlenderVehicle::DoFixedPhysicsBlend()
{
    CVehicle *vehicle = SafeCast(CVehicle, m_pPhysical);

    if(vehicle && vehicle->GetNetworkObject())
    {
        DoLogarithmicBlend(GetCurrentPredictedPosition(), LOGARITHMIC_BLEND_USE_POS);

        // now move any attached trailer to the new position
        CTrailer *trailer = vehicle->GetAttachedTrailer();

        if(trailer && trailer->GetNetworkObject())
        {
            CNetBlenderPhysical *netBlenderPhysical = SafeCast(CNetBlenderPhysical, trailer->GetNetworkObject()->GetNetBlender());

            if(netBlenderPhysical)
            {
                Vector3 oldPosition  = VEC3V_TO_VECTOR3(trailer->GetTransform().GetPosition());
                Vector3 parentOffset = netBlenderPhysical->GetParentVehicleOffset();
                Vector3 newPosition  = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition()) + parentOffset;
                bool    warp         = (oldPosition - newPosition).Mag2() >= 100.0f;

                trailer->SetPosition(newPosition, true, true, warp);
            }
        }
    }
}

void CNetBlenderVehicle::DoTrailerBlend(CVehicle &parentVehicle, CTrailer &trailer)
{
    if(trailer.IsNetworkClone())
    {
        CNetBlenderVehicle *netBlenderTrailer = SafeCast(CNetBlenderVehicle, trailer.GetNetworkObject()->GetNetBlender());

        if(netBlenderTrailer && netBlenderTrailer->m_HasParentVehicleOffset)
        {
            // do a simple blend to the trailer parent attach offset
            Vector3 currentPosition = VEC3V_TO_VECTOR3(trailer.GetTransform().GetPosition());
            Vector3 targetPosition  = VEC3V_TO_VECTOR3(parentVehicle.GetTransform().GetPosition()) + netBlenderTrailer->GetParentVehicleOffset();
            Vector3 targetVelocity  = netBlenderTrailer->GetLastVelocityReceived();
            Vector3 posDiff         = targetPosition - currentPosition;
            float   distSqr         = posDiff.Mag2();

            TUNE_FLOAT(START_TRAILER_BLEND_DELTA, 0.2f, 0.1f, 10.0f, 0.05f);
            TUNE_FLOAT(STOP_TRAILER_BLEND_DELTA, 0.1f, 0.1f, 10.0f, 0.05f);

            if(!netBlenderTrailer->m_BlendingTrailer)
            {
                if(distSqr < START_TRAILER_BLEND_DELTA)
                {
                    return;
                }

                netBlenderTrailer->m_BlendingTrailer = true;
            }

            if(distSqr >= rage::square(GetPositionMaxForUpdateLevel()))
            {
                // trailer is too far away from target position - correct both the truck and trailer
                GoStraightToTarget();

                netBlenderTrailer->m_BlendingTrailer = false;
            }
            else if(distSqr <= rage::square(STOP_TRAILER_BLEND_DELTA))
            {
                netBlenderTrailer->SetVelocityOnObject(targetVelocity);

                netBlenderTrailer->m_BlendingTrailer = false;
            }
            else
            {
                Vector3 currentVelocity = trailer.GetVelocity();
                Vector3 blendedVelocity = (targetPosition - currentPosition) / fwTimer::GetTimeStep();
                Vector3 velocityDiff    = blendedVelocity - currentVelocity;

                // only change the velocity by the specified amount per frame
                TUNE_FLOAT(TRAILER_MAX_VEL_CHANGE, 0.5f, 0.1f, 5.0f, 0.1f);

                if(velocityDiff.Mag2() > rage::square(TRAILER_MAX_VEL_CHANGE))
                {
                    velocityDiff.Normalize();
                    velocityDiff.Scale(TRAILER_MAX_VEL_CHANGE);

                    blendedVelocity = currentVelocity + velocityDiff;
                }

                // don't allow the velocity to exceed the maximum difference from the target
                TUNE_FLOAT(TRAILER_MAX_VEL_DIFF_FROM_TARGET, 5.0f, 0.0f, 100.0f, 0.1f);

                velocityDiff = blendedVelocity - targetVelocity;

                if(velocityDiff.Mag2() > rage::square(TRAILER_MAX_VEL_DIFF_FROM_TARGET))
                {
                    velocityDiff.Normalize();
                    velocityDiff.Scale(TRAILER_MAX_VEL_DIFF_FROM_TARGET);

                    blendedVelocity = targetVelocity + velocityDiff;
                }

                netBlenderTrailer->SetVelocityOnObject(blendedVelocity);
            }
        }
    }
}

void CNetBlenderVehicle::UpdateTrailerAfterWarp(CVehicle &parentVehicle, CTrailer &trailer)
{
	
    if(trailer.GetNetworkObject())
    {
        CNetBlenderVehicle *netBlenderTrailer = SafeCast(CNetBlenderVehicle, trailer.GetNetworkObject()->GetNetBlender());

        if(netBlenderTrailer && netBlenderTrailer->m_HasParentVehicleOffset)
        {	
			// Don't do this for the large trailer as it will mess up the attachment state
			if (trailer.GetModelIndex() != MI_TRAILER_TRAILERLARGE)
			{
				// detach the trailer before warping it, it will be reattached if necessary in the next network object update          
				trailer.DetachFromParent(0);
			}

            Matrix34 targetMatrix = netBlenderTrailer->GetLastMatrixReceived();
            targetMatrix.d        = VEC3V_TO_VECTOR3(parentVehicle.GetTransform().GetPosition()) + netBlenderTrailer->GetParentVehicleOffset();

            trailer.SetMatrix(targetMatrix, true, true, true);
        }
    }
}

void CNetBlenderVehicle::InformAttachedPedsOfWarp(const Vector3 &newPosition)
{
    CVehicle *vehicle = SafeCast(CVehicle, m_pPhysical);

    // if the vehicle position has changed too much don't let any peds getting in or
    // out of the vehicle to be warped along with the vehicle
    Vector3 currentPosition = VEC3V_TO_VECTOR3(vehicle->GetTransform().GetPosition());

    static const float WARP_DISTANCE_TO_DETACH_PEDS_SQR = rage::square(10.0f);

    if((currentPosition - newPosition).Mag2() > WARP_DISTANCE_TO_DETACH_PEDS_SQR)
    {
        const fwAttachmentEntityExtension *attachExtension = vehicle->GetAttachmentExtension();

        if(attachExtension)
        {
	        CPhysical *currentChild = static_cast<CPhysical*>(attachExtension->GetChildAttachment());

	        while(currentChild)
	        {
		        fwAttachmentEntityExtension &currentChildAttachExtension = currentChild->GetAttachmentExtensionRef();

		        CPhysical *nextChild = static_cast<CPhysical*>(currentChildAttachExtension.GetSiblingAttachment());

                if(!currentChild->IsNetworkClone())
                {
                    if(currentChild->GetIsTypePed())
                    {
			            CPed *childPed = static_cast<CPed*>(currentChild);

                        if(childPed)
                        {
                            if(currentChildAttachExtension.GetAttachState() == ATTACH_STATE_PED_ENTER_CAR ||
                               currentChildAttachExtension.GetAttachState() == ATTACH_STATE_PED_EXIT_CAR  ||
                               childPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_VAULT) ||
                               childPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_DROP_DOWN))
                            {
                                childPed->GetPedIntelligence()->FlushImmediately(true);
			                }
		                }
                    }
                }

		        currentChild = nextChild;
	        }
        }
    }
}

bool CNetBlenderVehicle::TestForMapCollision(const Vector3 &lastReceivedPos, const Vector3 &newPos)
{
    WorldProbe::CShapeTestProbeDesc probeDesc;
    WorldProbe::CShapeTestFixedResults<1> probeTestResults;
	probeDesc.SetResultsStructure(&probeTestResults);
    probeDesc.SetStartAndEnd(lastReceivedPos, newPos);
    probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_VEHICLE);
    probeDesc.SetContext(WorldProbe::ENetwork);

    gnetDebug2("TestForMapCollision - Performing probe for %s (%.2f,%.2f,%.2f)->(%.2f,%.2f,%.2f)", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "Non-networked vehicle", lastReceivedPos.x, lastReceivedPos.y, lastReceivedPos.z, newPos.x, newPos.y, newPos.z);

    if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
    {
        gnetDebug2("TestForMapCollision -  Cannot pop due to predicting through the map!");
        return true;
    }

    return false;
}
