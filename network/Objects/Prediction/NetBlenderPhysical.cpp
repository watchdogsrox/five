//
// name:        NetBlenderPhysical.cpp
// written by:  Daniel Yelland
// description: Network blender used by physics objects

#include "network/objects/prediction/NetBlenderPhysical.h"

// rage includes
#include "phbound/boundcomposite.h"
#include "vector/matrix34.h"

// game includes
#include "debug//DebugScene.h"
#include "game/ModelIndices.h"
#include "network/Debug/NetworkDebug.h"
#include "objects/door.h"
#include "objects/object.h"
#include "peds/ped.h"
#include "scene/physical.h"
#include "vehicles/trailer.h"
#include "vehicles/vehicle.h"
#include "vehicles/wheel.h"

CompileTimeAssert(sizeof(CNetBlenderPhysical) <= LARGEST_NET_PHYSICAL_BLENDER_CLASS);

NETWORK_OPTIMISATIONS()
NETWORK_OBJECT_OPTIMISATIONS()

static const unsigned MAX_NET_PHYSICAL_BLENDERS = MAX_NUM_NETOBJVEHICLES + MAX_NUM_NETOBJOBJECTS + MAX_NUM_NETOBJPICKUPS + MAX_NUM_NETOBJDOORS;

FW_INSTANTIATE_BASECLASS_POOL(CNetBlenderPhysical, MAX_NET_PHYSICAL_BLENDERS, atHashString("CNetBlenderPhysical",0x95bc7e7), LARGEST_NET_PHYSICAL_BLENDER_CLASS);

// ===========================================================================================================================
// CPhysicalPredictionData
// ===========================================================================================================================

CPhysicalPredictionData::CPhysicalPredictionData(const Matrix34 &matrix,
                                                 const Vector3  &angVelocity,
                                                 const u32       time)
{
    Reset(matrix, angVelocity, time);
}

CPhysicalPredictionData::~CPhysicalPredictionData()
{
}

void CPhysicalPredictionData::Reset(const Matrix34 &matrix,
                                    const Vector3  &angVelocity,
                                    const u32      time)
{
    m_snapshotPast.m_matrix               = matrix;
    m_snapshotPast.m_angVelocity          = angVelocity;
    m_snapshotPast.m_matrixTimestamp      = time;
    m_snapshotPast.m_angVelocityTimestamp = time;

    m_snapshotPresent.m_matrix               = matrix;
    m_snapshotPresent.m_angVelocity          = angVelocity;
    m_snapshotPresent.m_matrixTimestamp      = time;
    m_snapshotPresent.m_angVelocityTimestamp = time;

    m_snapshotFuture.m_matrix               = matrix;
    m_snapshotFuture.m_angVelocity          = angVelocity;
    m_snapshotFuture.m_matrixTimestamp      = time;
    m_snapshotFuture.m_angVelocityTimestamp = time;
}

// ===========================================================================================================================
// CNetBlenderPhysical
// ===========================================================================================================================
namespace
{
    const s32 NO_COLLISION_TIME_INTERVAL = 5000;
}

CNetBlenderPhysical::CNetBlenderPhysical(CPhysical *pPhysical, CPhysicalBlenderData *pBlendData) :
netBlenderLinInterp(pBlendData, VEC3V_TO_VECTOR3(pPhysical->GetTransform().GetPosition()), pPhysical->GetVelocity())
, m_pPhysical(pPhysical)
, m_predictionData(MAT34V_TO_MATRIX34(pPhysical->GetMatrix()), pPhysical->GetAngVelocity(), NetworkInterface::GetNetworkTime())
, m_ParentVehicleOffset(VEC3_ZERO)
, m_blendPhysicalStartTime(NetworkInterface::GetNetworkTime())
, m_timeAngularVelocityReceived(NetworkInterface::GetNetworkTime())
, m_bBlendingOrientation(false)
, m_disableCollisionTimer(0)
, m_BBCheckTimer(0)
, m_IsPedStandingOnObject(false)
, m_HasParentVehicleOffset(false)
, m_IsGoingStraightToTarget(false)
#if __BANK
, m_NumBBChecksBeforeWarp(0)
, m_LastSnapMatrix(M34_IDENTITY)
, m_LastSnapAngVel(VEC3_ZERO)
#endif // __BANK
{
}

CNetBlenderPhysical::~CNetBlenderPhysical()
{
}

void CNetBlenderPhysical::OnOwnerChange(blenderResetFlags resetFlag)
{
    // reset all snapshot data to time 0 so the first position update from the new owner is always applied
    netBlenderLinInterp::OnOwnerChange(resetFlag);

    Matrix34 currMatrix      = GetMatrixFromObject();
    Vector3  currAngVelocity = GetAngVelocityFromObject();
    m_predictionData.Reset(currMatrix, currAngVelocity, 0);

    // we need to enable collisions again and reset the BB check timer if this object becomes locally controlled.
    if(m_disableCollisionTimer > 0 && m_pPhysical && !m_pPhysical->IsNetworkClone())
    {
        m_pPhysical->SetNoCollision(0, 0);
        m_disableCollisionTimer = 0;
        m_BBCheckTimer = 0;
    }
}

u32 CNetBlenderPhysical::GetTimeWithNoCollision() const
{
    if(m_disableCollisionTimer > 0 && m_disableCollisionTimer < NO_COLLISION_TIME_INTERVAL)
    {
        return NO_COLLISION_TIME_INTERVAL - m_disableCollisionTimer;
    }

    return 0;
}

//
// Name         :   Update
// Purpose      :   Performs the blend and returns true if there was any change to the object
void CNetBlenderPhysical::Update()
{
    UpdateNoCollisionTimer();

    if (!IsBlendingOn())
        return;

#if __BANK
    m_LastSnapMatrix = m_predictionData.GetSnapshotPresent().m_matrix;
    m_LastSnapAngVel = m_predictionData.GetSnapshotPresent().m_angVelocity;
#endif // __BANK

    if(m_pPhysical && m_pPhysical->GetIsTypeObject() && m_pPhysical->GetIsAnyFixedFlagSet())
    {
        DoLogarithmicBlend(GetCurrentPredictedPosition(), LOGARITHMIC_BLEND_USE_POS);
    }
    else
    {
        netBlenderLinInterp::Update();
    }

    UpdateAngularVelocity();

#if DEBUG_DRAW
    if(ShouldDisplayOrientationDebug())
    {
        const float orientationDebugScale = GetBlenderData()->m_DisplayOrientationDebugScale;

        // display line from current position
        Vector3 localForwardVector  = GetForwardVectorFromObject();
        Vector3 remoteForwardVector = GetLastMatrixReceived().b;
        Vector3 localStartPosition  = GetPositionFromObject();
        Vector3 remoteStartPosition = GetLastPositionReceived();
        Vector3 localEndPosition    = localStartPosition  + (localForwardVector  * orientationDebugScale);
        Vector3 remoteEndPosition   = remoteStartPosition + (remoteForwardVector * orientationDebugScale);

        grcDebugDraw::Line(localStartPosition, localEndPosition, Color_blue, Color_blue);
        grcDebugDraw::Line(remoteStartPosition, remoteEndPosition, Color_green, Color_green);
        grcDebugDraw::Sphere(localEndPosition, 0.2f, Color_blue, false);
        grcDebugDraw::Sphere(remoteEndPosition, 0.2f, Color_green, false);
    }
#endif // DEBUG_DRAW
}

// Name         : Reset
// Purpose      : Resets the blender ready to start a new blend
void CNetBlenderPhysical::Reset()
{
    netBlenderLinInterp::Reset();

    Matrix34 currMatrix      = GetMatrixFromObject();
    Vector3  currAngVelocity = GetAngVelocityFromObject();

    m_predictionData.Reset(currMatrix, currAngVelocity, GetLastSyncMessageTime());

    UpdateNoCollisionTimer();
}

// Name         : GoStraightToTarget
// Purpose      : Goes directly to the blenders last received snapshot
void CNetBlenderPhysical::GoStraightToTarget()
{
    m_IsGoingStraightToTarget = true;

    // we don't want to do BB checks when the object is being moved straight to it's target position
    bool oldDoBBChecksOnWarp = GetBlenderData()->m_DoBBChecksOnWarp;
    GetBlenderData()->m_DoBBChecksOnWarp = false;

    netBlenderLinInterp::GoStraightToTarget();

    GetBlenderData()->m_DoBBChecksOnWarp = oldDoBBChecksOnWarp;

    Matrix34 newMatrix = m_predictionData.GetSnapshotPresent().m_matrix;
    newMatrix.d = GetPositionFromObject();

    SetMatrixOnObject     (newMatrix, true);
    SetAngVelocityOnObject(m_predictionData.GetSnapshotPresent().m_angVelocity);

    m_IsGoingStraightToTarget = false;
}

// Name         : RecalculateTarget
// Purpose      : Recalculates the target position to blend towards
void CNetBlenderPhysical::RecalculateTarget()
{
    m_predictionData.GetSnapshotFuture() = m_predictionData.GetSnapshotPresent();
}

void CNetBlenderPhysical::ProcessPostPhysics()
{
    netBlenderLinInterp::ProcessPostPhysics();

    if(IsBlendingOn())
    {
        UpdateOrientationBlending();
    }

    m_IsPedStandingOnObject = false;
}

// Name         : GetLastMatrixReceived
// Purpose      : Returns the last matrix value added to the blender
Matrix34 CNetBlenderPhysical::GetLastMatrixReceived(u32 *timestamp)
{
    if(timestamp)
    {
        *timestamp = m_predictionData.GetSnapshotPresent().m_matrixTimestamp;
    }

    return m_predictionData.GetSnapshotPresent().m_matrix;
}

// Name         : GetLastAngVelocityReceived
// Purpose      : Returns the last angular velocity value added to the blender
Vector3 CNetBlenderPhysical::GetLastAngVelocityReceived(u32 *timestamp)
{
    if(timestamp)
    {
        *timestamp = m_predictionData.GetSnapshotPresent().m_angVelocityTimestamp;
    }

    return m_predictionData.GetSnapshotPresent().m_angVelocity;
}

// Name         : UpdateMatrix
// Purpose      : Updates the matrix for the blender
void CNetBlenderPhysical::UpdateMatrix(const Matrix34 &matrix, u32 time)
{
    if(time >= m_predictionData.GetSnapshotPresent().m_matrixTimestamp)
    {
        // move snapshot B position to snapshot A, and copy new position to snapshot B
        m_predictionData.GetSnapshotPast().m_matrix             = m_predictionData.GetSnapshotPresent().m_matrix;
        m_predictionData.GetSnapshotPast().m_matrixTimestamp    = m_predictionData.GetSnapshotPresent().m_matrixTimestamp;
        m_predictionData.GetSnapshotPresent().m_matrix          = matrix;
        m_predictionData.GetSnapshotPresent().m_matrixTimestamp = time;

        Vector3 velocity = GetVelocityFromObject();

        const float RESTRICT_ORIENTATION_BLEND_START_VEL_THRESHOLD = 5.0f;

        Quaternion currentOrientation;
        Quaternion newOrientation;
        GetQuaternionsFromMatrices(GetMatrixFromObject(), matrix, currentOrientation, newOrientation);

        float angleDiff = newOrientation.RelAngle(currentOrientation);

        if(angleDiff >= GetBlenderData()->m_MinOrientationDiffToStartBlend || (velocity.Mag2() >= rage::square(RESTRICT_ORIENTATION_BLEND_START_VEL_THRESHOLD)))
        {
            m_blendPhysicalStartTime = NetworkInterface::GetNetworkTime();
            m_bBlendingOrientation = true;
        }

        RecalculateTarget();
    }
}

// Name         : UpdateAngVelocity
// Purpose      : Updates the angular velocity for the blender
void CNetBlenderPhysical::UpdateAngVelocity(const Vector3 &angVelocity, u32 time)
{
    if(time >= m_predictionData.GetSnapshotPresent().m_angVelocityTimestamp)
    {
        // move snapshot B position to snapshot A, and copy new position to snapshot B
        m_predictionData.GetSnapshotPast().m_angVelocity             = m_predictionData.GetSnapshotPresent().m_angVelocity;
        m_predictionData.GetSnapshotPast().m_angVelocityTimestamp    = m_predictionData.GetSnapshotPresent().m_angVelocityTimestamp;
        m_predictionData.GetSnapshotPresent().m_angVelocity          = angVelocity;
        m_predictionData.GetSnapshotPresent().m_angVelocityTimestamp = time;

        m_timeAngularVelocityReceived = NetworkInterface::GetNetworkTime();

        RecalculateTarget();
    }
}

// Name         : GetMatrixFromObject
// Purpose      : Gets the current matrix from the blender target object
Matrix34 CNetBlenderPhysical::GetMatrixFromObject()
{
    // need to use physinst matrix here because when UpdateMatrix() gets called during an object sync the
    // physical's matrix has already been set to the target position
    return m_pPhysical->GetCurrentPhysicsInst() ? RCC_MATRIX34(m_pPhysical->GetCurrentPhysicsInst()->GetMatrix()) : MAT34V_TO_MATRIX34(m_pPhysical->GetMatrix());
}

// Name         : GetAngVelocityFromObject
// Purpose      : Gets the current angular velocity from the blender target object
Vector3 CNetBlenderPhysical::GetAngVelocityFromObject()
{
    return m_pPhysical->GetAngVelocity();
}

const Vector3 CNetBlenderPhysical::GetPreviousPositionFromObject() const
{
    return m_pPhysical->GetPreviousPosition();
}

// get the current position from the object we are predicting
const Vector3 CNetBlenderPhysical::GetPositionFromObject() const
{
    // This code has been slightly refactored due to a compiler bug
    if(m_pPhysical->GetCurrentPhysicsInst())
    {
        return VEC3V_TO_VECTOR3(m_pPhysical->GetCurrentPhysicsInst()->GetMatrix().GetCol3());
    }
    else
    {
        Vec3V position = m_pPhysical->GetTransform().GetPosition();
        return RCC_VECTOR3(position);
    }
}

// get the current velocity of the object we are predicting
const Vector3 CNetBlenderPhysical::GetVelocityFromObject() const
{
    return m_pPhysical->GetVelocity();
}

const Vector3 CNetBlenderPhysical::GetForwardVectorFromObject() const
{
    return VEC3V_TO_VECTOR3(m_pPhysical->GetTransform().GetB());
}

// set the position of the network object we are blending
bool CNetBlenderPhysical::SetPositionOnObject(const Vector3 &position, bool warp)
{
	bool success = true;
    // we can't set the position on an object if we are attached to something
    if(m_pPhysical->GetIsAttached() && !CanBlendWhenAttached())
    {
        gnetWarning("Network blender cannot set position on %s as it is attached!", m_pPhysical->GetNetworkObject() ? m_pPhysical->GetNetworkObject()->GetLogName() : "Unknown");
        return false;
    }

    // activate the physics on the object if we are blending position with a zero velocity
    // otherwise the object won't collide with world objects correctly
    if(m_pPhysical->GetVelocity().IsZero() && warp && !m_pPhysical->GetIsTypeObject())
    {
        m_pPhysical->ActivatePhysics();
    }

    if(warp)
    {
        // don't pop to a predicted position for pickups until they come to rest,
        // otherwise they can fall through the map
        if(!m_IsGoingStraightToTarget && GetNetworkObject() && GetNetworkObject()->GetObjectType() == NET_OBJ_TYPE_PICKUP)
        {
            if(GetLastVelocityReceived().Mag2() > 0.01f)
            {
                return false;
            }
        }

        bool canChangePosition = true;
        bool bDisableCollision = false;

        if(GetBlenderData()->m_DoBBChecksOnWarp)
        {
			bool respotting = false;
			if(m_pPhysical->GetIsTypeVehicle())
			{
				CVehicle* vehicle = SafeCast(CVehicle, m_pPhysical);
				if(vehicle && vehicle->IsBeingRespotted())
					respotting = true;
			}

			bool hitImportantVehicle = false;
            if(!respotting && DoCollisionTest(position, hitImportantVehicle))
            {
				success = false;
                canChangePosition = false;

                bool processBBTimer = true;
                // if the object is overlapping with the local player vehicle always leave it alone
                // unless it is really far out of sync
                if(hitImportantVehicle || (m_pPhysical->GetIsTypeVehicle() && IsImportantVehicle(SafeCast(CVehicle, m_pPhysical))))
                {
                    const float IMPORTANT_VEHICLE_INTERSECTION_THRESHOLD = 20.0f;

                    float distanceSqr = (GetCurrentPredictedPosition()-GetPositionFromObject()).XYMag2();

                    if(distanceSqr < rage::square(IMPORTANT_VEHICLE_INTERSECTION_THRESHOLD))
                    {
                        processBBTimer = false;
                    }
                }

                if(processBBTimer)
                {
                    // time to perform bounding box checks when another object is overlapping
                    // where we want to force the position of the target object to
                    const s32 BB_CHECK_TIME_INTERVAL = 5000;

                    if(m_BBCheckTimer == 0)
                    {
                        m_BBCheckTimer = BB_CHECK_TIME_INTERVAL;
                    }
                    else
                    {
                        m_BBCheckTimer -= fwTimer::GetSystemTimeStepInMilliseconds();
            
                        if(m_BBCheckTimer <= 0)
                        {
                            m_BBCheckTimer = 0;
                            canChangePosition = true;
                            bDisableCollision = true;
                        }
                    }
                }
            }
            else
            {
                m_BBCheckTimer = 0;
            }
        }

        if(canChangePosition)
        {
#if __BANK
            NetworkDebug::AddNumBBChecksBeforeWarp(m_NumBBChecksBeforeWarp, GetNetworkObject() ? GetNetworkObject()->GetObjectID() : NETWORK_INVALID_OBJECT_ID);
            m_NumBBChecksBeforeWarp = 0;
#endif // __BANK

			Matrix34 matOld = MAT34V_TO_MATRIX34(m_pPhysical->GetMatrix());
            m_pPhysical->SetPosition(position, true, true, true);

#if GTA_REPLAY
			// This hasn't warped the entity a large distance, remove warp flag from replay.
			CReplayMgr::CancelRecordingWarpedOnEntity(m_pPhysical);
#endif

            static const unsigned MAX_OBJECTS_TO_DO_DOOR_CHECK = 2;
            CPhysical *objectsToCheck[MAX_OBJECTS_TO_DO_DOOR_CHECK];
            unsigned numObjectsToCheck = 0;

            objectsToCheck[numObjectsToCheck++] = m_pPhysical;

            if(m_pPhysical->GetIsTypeVehicle())
            {
                CVehicle *vehicle = static_cast<CVehicle *>(m_pPhysical);
                vehicle->ResetAfterTeleport();
                
                CTrailer *trailer = vehicle->GetAttachedTrailer();
				// If it's the big trailer, translate so that it keeps its rotation
				bool translateOnly = false;
				if (trailer && trailer->GetModelIndex() == MI_TRAILER_TRAILERLARGE)
				{
					translateOnly = true;
				}
					
				vehicle->UpdateGadgetsAfterTeleport(matOld, true, true, true, translateOnly);

                if(trailer)
                {
                    objectsToCheck[numObjectsToCheck++] = trailer;
                }

                if(bDisableCollision)
                {
                    gnetDebug2("Disabling collision for %s as another vehicle is in the way!", m_pPhysical->GetNetworkObject()->GetLogName());
					
			        // disable collisions after a prediction pop until the object is clear
			        m_pPhysical->SetNoCollision(m_pPhysical, NO_COLLISION_NETWORK_OBJECTS|NO_COLLISION_RESET_WHEN_NO_BBOX);
			        m_disableCollisionTimer = NO_COLLISION_TIME_INTERVAL;
                }
            }

            // check if we are going to be intersecting any doors, if so we need to force them open
            CDoor::ForceOpenIntersectingDoors(objectsToCheck, numObjectsToCheck, false);

            if(m_IsPedStandingOnObject)
            {
                UpdatePedsStandingOnObjectAfterWarp(matOld, MAT34V_TO_MATRIX34(m_pPhysical->GetTransform().GetMatrix()));
            }
        }
    }
    else
    {
#if __ASSERT
        phCollider *collider = m_pPhysical->GetCollider();

        const Vector3 &oldPos = RCC_VECTOR3(collider->GetLastInstanceMatrix().GetCol3ConstRef());

        if(collider && ((oldPos - position).Mag2() >= 100.0f))
        {
            gnetAssertf(0, "%s is moving too far between physics updates! (%.2f, %.2f, %.2f)->(%.2f, %.2f, %.2f)", m_pPhysical->GetNetworkObject()->GetLogName(), oldPos.x, oldPos.y, oldPos.z, position.x, position.y, position.z);
        }
#endif
        m_pPhysical->SetPosition(position);
    }

	return success;
}

// set the velocity of the network object we are blending
void CNetBlenderPhysical::SetVelocityOnObject(const Vector3 &velocity)
{
    if(m_pPhysical->GetCurrentPhysicsInst() && m_pPhysical->GetCurrentPhysicsInst()->IsInLevel() && gnetVerify(m_pPhysical->GetCurrentPhysicsInst()->GetArchetype()))
    {
        Vector3 newVelocity = velocity;

        // ensure we don't set a velocity too high for the type of object being blended
        float maxSpeed = m_pPhysical->GetCurrentPhysicsInst()->GetArchetype()->GetMaxSpeed();

        if(newVelocity.Mag2() > (maxSpeed * maxSpeed))
        {
            float scale = maxSpeed / newVelocity.Mag();
            newVelocity *= scale;
        }

        // ensure the capping works - we are still getting assert from CPhysical::SetVelocity()
        BANK_ONLY(gnetAssertf(newVelocity.Mag2() <= ((maxSpeed * maxSpeed)+0.01f), "Failed capping velocity for %s! Speed Sqrd: %f, Max Speed Sqrd: %f", m_pPhysical->GetDebugName(), velocity.Mag2(), ((maxSpeed * maxSpeed)+0.01f));)

        m_pPhysical->SetVelocity(newVelocity);
    }
}


// Name         : SetMatrixOnObject
// Purpose      : Sets the specified matrix on the blender target object
void CNetBlenderPhysical::SetMatrixOnObject(Matrix34 &matrix, bool bWarp, bool bUpdatePedsStandingOnObject)
{
    // we can't set the matrix on an object if we are attached to something
    if(m_pPhysical->GetIsAttached() && !CanBlendWhenAttached())
    {
        gnetWarning("Network blender cannot set matrix on %s as it is attached!", m_pPhysical->GetNetworkObject() ? m_pPhysical->GetNetworkObject()->GetLogName() : "Unknown");
        return;
    }

    // stop the entity at the edge of the world
    if (matrix.d.x < (WORLDLIMITS_XMIN+1.0f) || matrix.d.x > (WORLDLIMITS_XMAX-1.0f) ||
        matrix.d.y < (WORLDLIMITS_YMIN+1.0f) || matrix.d.y > (WORLDLIMITS_YMAX-1.0f) ||
        matrix.d.z < (WORLDLIMITS_ZMIN+1.0f) || matrix.d.z > (WORLDLIMITS_ZMAX-1.0f))
    {
        return;
    }

	Matrix34 matOld = MAT34V_TO_MATRIX34(m_pPhysical->GetMatrix());
    m_pPhysical->SetMatrix(matrix, true, true, bWarp);

    if(bWarp)
    {
        if(m_pPhysical->GetIsTypeVehicle())
        {
            CVehicle *vehicle = static_cast<CVehicle *>(m_pPhysical);

			bool translateOnly = false;
			
			// If it's the big trailer, translate so that it keeps its rotation
			CTrailer *trailer = vehicle->GetAttachedTrailer();
			if (trailer && trailer->GetModelIndex() == MI_TRAILER_TRAILERLARGE)
			{
				translateOnly = true;
			}

            vehicle->UpdateGadgetsAfterTeleport(matOld, true, true, true, translateOnly);
        }
    }

    if(bUpdatePedsStandingOnObject && m_IsPedStandingOnObject)
    {
        UpdatePedsStandingOnObjectAfterWarp(matOld, MAT34V_TO_MATRIX34(m_pPhysical->GetTransform().GetMatrix()));
    }
}

// Name         : SetAngVelocityOnObject
// Purpose      : Sets the specified angular velocity on the blender target object
void CNetBlenderPhysical::SetAngVelocityOnObject(const Vector3 &angVelocity)
{
    if(m_pPhysical->GetCurrentPhysicsInst() && m_pPhysical->GetCurrentPhysicsInst()->IsInLevel())
    {
        m_pPhysical->SetAngVelocity(angVelocity);
    }
}

void CNetBlenderPhysical::OnPredictionPop() const
{
#if __BANK
    if(GetBlenderData()->m_DoBBChecksOnWarp)
    {
        if(m_BBCheckTimer == 0)
        {
            NetworkDebug::AddPredictionPop(m_pPhysical->GetNetworkObject()->GetObjectID(), m_pPhysical->GetNetworkObject()->GetObjectType(), false);
        }
    }
#endif // __BANK
}

bool CNetBlenderPhysical::GetIsOnScreen() const
{
    return m_pPhysical && m_pPhysical->GetIsVisibleInSomeViewportThisFrame();
}

void CNetBlenderPhysical::UpdateOrientationBlending()
{
    Quaternion currentOrientation;
    Quaternion targetOrientation;
    Matrix34 newMatrix = GetMatrixFromObject();
    GetQuaternionsFromMatrices(newMatrix, m_predictionData.GetSnapshotFuture().m_matrix, currentOrientation, targetOrientation);

    bool changed = true;

    float angleDiff = targetOrientation.RelAngle(currentOrientation);

    if(m_bBlendingOrientation || (m_predictionData.GetSnapshotFuture().m_angVelocity.Mag2() < 0.01f))
    {
        if (angleDiff >= GetBlenderData()->m_OrientationDeltaMax)
        {
            // pop to new orientation
            newMatrix.Set3x3(m_predictionData.GetSnapshotFuture().m_matrix);

            if(CanApplyNewOrientationMatrix(newMatrix))
            {
                SetMatrixOnObject(newMatrix, false, true);
                changed = true;
                m_bBlendingOrientation = false;
            }

            Vector3 angVel = GetLastAngVelocityReceived();

            if(angVel.Mag2() < 0.01f)
            {
                SetAngVelocityOnObject(angVel);

                Vector3 velocity = GetLastVelocityReceived();

                if(velocity.Mag2() < 0.01f)
                {
                    SetPositionOnObject(GetLastPositionReceived(), true);
                    SetVelocityOnObject(velocity);

                    m_pPhysical->DeActivatePhysics();
                }
            }

            if(m_pPhysical->GetIsTypeVehicle())
            {
                CVehicle *vehicle = static_cast<CVehicle *>(m_pPhysical);
                vehicle->UpdateAttachedClonePedsAfterWarp();
            }
        }
    }

    if(!m_bBlendingOrientation)
        return;

    bool updatePedsStandingOnObject = false;
    if(!GetBlenderData()->m_BlendOrientation)
    {
        newMatrix.Set3x3(m_predictionData.GetSnapshotFuture().m_matrix);
        updatePedsStandingOnObject = true;
        changed                    = true;
    }
    else
    {
        u32 timeBlending = NetworkInterface::GetNetworkTime() - m_blendPhysicalStartTime;

        if (angleDiff > GetBlenderData()->m_OrientationDeltaMin)
        {
            // this is the blend stop time rather than ramp time as we don't want to stop blending too quickly
            if(timeBlending <= GetBlenderData()->m_BlendOrientationStopTime)
            {
                float fOrientationTimeRatio = GetOrientationBlendValue();
                gnetAssert(fOrientationTimeRatio >= 0.0f && fOrientationTimeRatio <= 1.0f);

                currentOrientation.SlerpNear(fOrientationTimeRatio, targetOrientation);

                gnetAssert(currentOrientation.Mag2() >= square(0.999f) && currentOrientation.Mag2() <= square(1.001f));
                newMatrix.FromQuaternion(currentOrientation);

                float det = newMatrix.Determinant3x3();

                if (det == 0.0f)
                {
                    newMatrix.MakeUpright();
                }

                changed = true;
            }
            else
            {
                // we finished blending, pop to correct position if needed
                bool  onScreen    = GetIsOnScreen();
                float popDistance = onScreen ? GetBlenderData()->m_MaxOnScreenOrientationDeltaAfterBlend : GetBlenderData()->m_MaxOffScreenOrientationDeltaAfterBlend;

                if(angleDiff > popDistance)
                {
                    newMatrix.Set3x3(m_predictionData.GetSnapshotFuture().m_matrix);
                    changed                    = true;
                    m_bBlendingOrientation     = false;
                    updatePedsStandingOnObject = true;
                }
            }
        }
        else
        {
            m_bBlendingOrientation = false;
        }
    }

    // set the new matrix on the object
    if (changed)
    {
        if(CanApplyNewOrientationMatrix(newMatrix))
        {
            SetMatrixOnObject(newMatrix, false, updatePedsStandingOnObject);
        }
    }
}

float CNetBlenderPhysical::GetOrientationBlendValue() const
{
    return GetBlenderData()->m_OrientationBlend;
}

void CNetBlenderPhysical::UpdateAngularVelocity()
{
    u32 latestSnapshotTime = m_timeAngularVelocityReceived;

    if(m_predictionData.GetSnapshotPresent().m_matrixTimestamp > latestSnapshotTime)
    {
        latestSnapshotTime = m_predictionData.GetSnapshotPresent().m_matrixTimestamp;
    }

    u32 currentTime = NetworkInterface::GetNetworkTime();

    if((currentTime - latestSnapshotTime) < GetBlenderData()->m_TimeToApplyAngVel)
    {
        SetAngVelocityOnObject(m_predictionData.GetSnapshotPresent().m_angVelocity);
    }
}

void CNetBlenderPhysical::UpdateNoCollisionTimer()
{
    if(m_disableCollisionTimer > 0)
    {
        m_disableCollisionTimer -= fwTimer::GetSystemTimeStepInMilliseconds();
        
        if(m_disableCollisionTimer <= 0)
        {
            gnetDebug2("Re-enabling collision for %s as timer has run out!", m_pPhysical->GetNetworkObject()->GetLogName());

            m_pPhysical->SetNoCollision(0, 0);
            m_disableCollisionTimer = 0;
        }
        else if(m_pPhysical->GetNoCollisionFlags() == 0)
        {
            m_disableCollisionTimer = 0;
        }
    }
}

netObject *CNetBlenderPhysical::GetNetworkObject()
{
    if(m_pPhysical)
    {
        return m_pPhysical->GetNetworkObject();
    }

    return 0;
}

const netObject *CNetBlenderPhysical::GetNetworkObject() const
{
    if(m_pPhysical)
    {
        return m_pPhysical->GetNetworkObject();
    }

    return 0;
}

bool CNetBlenderPhysical::IsImportantVehicle(CVehicle *vehicle) const
{
    if(vehicle && (vehicle->ContainsPlayer() || vehicle->PopTypeIsMission()))
    {
        return true;
    }

    return false;
}

bool CNetBlenderPhysical::DoCollisionTest(const Vector3 &position, bool &hitImportantVehicle)
{
    hitImportantVehicle = false;

    if(m_pPhysical && m_pPhysical->GetIsTypeVehicle())
    {
        CVehicle *vehicle = static_cast<CVehicle *>(m_pPhysical);

        CBaseModelInfo *modelInfo = vehicle->GetBaseModelInfo();

        if(gnetVerifyf(modelInfo->GetFragType(), "Vehicle has invalid model info!"))
        {
            Matrix34 testMatrix = GetMatrixFromObject();
            testMatrix.d = position;

            int nTestTypeFlags = ArchetypeFlags::GTA_BOX_VEHICLE_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE;

	        WorldProbe::CShapeTestBoundingBoxDesc bboxDesc;
	        bboxDesc.SetBound(modelInfo->GetFragType()->GetPhysics(0)->GetCompositeBounds());
	        bboxDesc.SetTransform(&testMatrix);
	        bboxDesc.SetIncludeFlags(nTestTypeFlags);

            static const unsigned MAX_ENTITIES_TO_EXCLUDE = 2;

            const CEntity *excludeEntities[MAX_ENTITIES_TO_EXCLUDE];

            unsigned numEntitiesToExclude = 0;

            excludeEntities[numEntitiesToExclude++] = vehicle;

            if(vehicle->HasTrailer())
            {
                excludeEntities[numEntitiesToExclude++] = vehicle->GetAttachedTrailer();
            }

			if(vehicle->GetNetworkObject())
			{
				CNetObjVehicle* netVeh = SafeCast(CNetObjVehicle, vehicle->GetNetworkObject());
				if(netVeh)
				{
					if(netVeh->IsScriptObject() && netVeh->GetIsCloneAttached() && !netVeh->IsPhysicallyAttached())
					{
						excludeEntities[numEntitiesToExclude++] = netVeh->GetEntityAttachedTo();
					}
				}
			}

            bboxDesc.SetExcludeEntities(excludeEntities, numEntitiesToExclude);
            bboxDesc.SetContext(WorldProbe::ENetwork);

            WorldProbe::CShapeTestFixedResults<1> probeResult;
            bboxDesc.SetResultsStructure(&probeResult);

#if __BANK
			NetworkDebug::AddPredictionBBCheck();
            m_NumBBChecksBeforeWarp++;
#endif // __BANK

	        if(WorldProbe::GetShapeTestManager()->SubmitTest(bboxDesc))
	        {
#if __BANK
                const char *vehicleName = "Unknown Vehicle (probe results not available)";
#endif // __BANK

                if(probeResult.GetResultsReady() && probeResult[0].GetHitDetected())
                {
                    if(probeResult[0].GetHitEntity() && probeResult[0].GetHitEntity()->GetIsTypeVehicle())
                    {
                        CVehicle *hitVehicle = SafeCast(CVehicle, probeResult[0].GetHitEntity());

                        if(IsImportantVehicle(hitVehicle))
                        {
                            hitImportantVehicle = true;
                        }

                        // HACK - disregard hits with cargoplane vehicles - they are very large and can fit other vehicles inside
                        bool isCargoPlane = hitVehicle->GetModelIndex()==MI_PLANE_CARGOPLANE.GetModelIndex();

                        if(isCargoPlane)
                        {
                            return false;
                        }
                    }
#if __BANK
                    netObject *networkObject = NetworkUtils::GetNetworkObjectFromEntity(probeResult[0].GetHitEntity());

                    if(networkObject)
                    {
                        vehicleName = networkObject->GetLogName();
                    }
                    else
                    {
                        vehicleName = "Non-networked vehicle";
                    }
#endif // __BANK
                }
#if __BANK
                if(m_pPhysical->GetNetworkObject())
                {
                    gnetDebug2("Failed to pop %s as %s is in the way!", m_pPhysical->GetNetworkObject()->GetLogName(), vehicleName);
                }
#endif // __BANK

                return true;
            }
        }
    }

    return false;
}

void CNetBlenderPhysical::UpdatePedsStandingOnObjectAfterWarp(const Matrix34 &oldMat, const Matrix34 &newMat)
{
    CNetObjPed::Pool *pedPool = CNetObjPed::GetPool();

    if(pedPool)
    {
        s32 index = pedPool->GetSize();

        while(index--)
        {
            CNetObjPed *netObjPed = pedPool->GetSlot(index);
            CPed       *ped       = netObjPed ? netObjPed->GetPed() : 0;
	
            if( ped && !ped->IsNetworkClone() && !ped->GetIsAttached())
			{
				bool bPedInAir = false;
				CPhysical* pGroundPhysical = ped->GetGroundPhysical();

				if (!pGroundPhysical && ped->GetLastValidGroundPhysical())
				{
					const bool isJumping = ped->GetPedResetFlag(CPED_RESET_FLAG_IsJumping);
					const bool isInAir   = ped->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir);
					const bool isFalling = ped->GetPedResetFlag(CPED_RESET_FLAG_IsFalling);
					bPedInAir = (isJumping || isInAir || isFalling);
					if (bPedInAir)
						pGroundPhysical = ped->GetLastValidGroundPhysical();
				}

				if (pGroundPhysical && (pGroundPhysical == m_pPhysical)) 
				{
					static const float STANDING_PEDS_WARP_TOLERANCE = 15.0f;

					Vector3 warpDiff = oldMat.d - newMat.d;

					if(warpDiff.XYMag2() < rage::square(STANDING_PEDS_WARP_TOLERANCE) && (fabs(warpDiff.z) < STANDING_PEDS_WARP_TOLERANCE))
					{
						trainDebugf2("CNetBlenderPhysical::UpdatePedsStandingOnObjectsAfterWarp--CacheGround/Teleport/RestoreCacheGround");

						Vector3 vPedVelocity = ped->GetVelocity();

						//Store the current ground information as it will be cleared in the Teleport with a call that happens there to ResetStandData
						CPhysical* pTempGroundPhysical = NULL;
						Vec3V vGroundVelocity, vGroundVelocityIntegrated, vGroundAngularVelocity, vGroundOffset, vGroundNormalLocal;
						int iGroundPhysicalComponent;
						ped->GetGroundData(pTempGroundPhysical, vGroundVelocity, vGroundVelocityIntegrated, vGroundAngularVelocity, vGroundOffset, vGroundNormalLocal, iGroundPhysicalComponent);

						Vector3 diff = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition()) - oldMat.d;

                        gnetDebug2("Update Ped Standing (%s->%s) - Old World Space Diff (%.2f, %.2f, %.2f)", ped->GetNetworkObject() ? ped->GetNetworkObject()->GetLogName() : "Non-networked ped", m_pPhysical->GetNetworkObject() ? m_pPhysical->GetNetworkObject()->GetLogName() : "Non-networked physical", diff.x, diff.y, diff.z);

						Vector3 localOffset;
						oldMat.UnTransform3x3(diff, localOffset);

						gnetDebug2("Update Ped Standing (%s->%s) - Local Space Diff (%.2f, %.2f, %.2f)", ped->GetNetworkObject() ? ped->GetNetworkObject()->GetLogName() : "Non-networked ped", m_pPhysical->GetNetworkObject() ? m_pPhysical->GetNetworkObject()->GetLogName() : "Non-networked physical", diff.x, diff.y, diff.z);

						newMat.Transform3x3(localOffset, diff);

						gnetDebug2("Update Ped Standing (%s->%s) - New World Space Diff (%.2f, %.2f, %.2f)", ped->GetNetworkObject() ? ped->GetNetworkObject()->GetLogName() : "Non-networked ped", m_pPhysical->GetNetworkObject() ? m_pPhysical->GetNetworkObject()->GetLogName() : "Non-networked physical", diff.x, diff.y, diff.z);

						Vector3 newPedPos = newMat.d + diff;

                        Vector3 pedDiff = newPedPos - VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());

                        if(pedDiff.XYMag2() < rage::square(STANDING_PEDS_WARP_TOLERANCE) && (fabs(pedDiff.z) < STANDING_PEDS_WARP_TOLERANCE))
                        {
						    ped->Teleport(newPedPos, ped->GetCurrentHeading(), true, !ped->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK));

						    //Restore the cached ground information from above - re-attach the ped to the ground
						    ped->SetGroundData(pGroundPhysical, vGroundVelocity, vGroundVelocityIntegrated, vGroundAngularVelocity, vGroundOffset, vGroundNormalLocal, iGroundPhysicalComponent);

						    //Restore ped velocity
						    ped->SetVelocity(vPedVelocity);
                        }
					}
				}
            }
        }
    }
}


bool CNetBlenderPhysical::DisableZBlending() const
{
	const CPhysical *physical = GetPhysical();

	if (physical && physical->GetIsTypeObject())
	{
        const CObject *object = static_cast<const CObject*>(physical);

        if(object->m_nObjectFlags.bFloater || object->m_nObjectFlags.bIsPickUp)
        {
            return true;
        }

        const CNetObjObject* netObj = static_cast<const CNetObjObject*>(object->GetNetworkObject());
        if (netObj && netObj->IsArenaBall())
        {
            bool bArenaBallDisableZBlending = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ARENA_BALL_DISABLE_Z_BLENDING", 0xd9802aba), true);
            if (bArenaBallDisableZBlending)
            {
                return true;
            }
        }
	}

	return false;
}

bool CNetBlenderPhysical::GoStraightToTargetUsesPrediction() const
{
    // pickups don't use prediction when calling GoStraightToTarget() - they can predict through the map
    return (GetNetworkObject() && (GetNetworkObject()->GetObjectType() != NET_OBJ_TYPE_PICKUP));
}

void CNetBlenderPhysical::GetQuaternionsFromMatrices(const Matrix34 &currentMatrix, const Matrix34 &newMatrix, Quaternion &currentQuat, Quaternion &newQuat)
{
    gnetAssertf(currentMatrix.IsOrthonormal(), "Current matrix is not orthonormal!");
    gnetAssertf(newMatrix.IsOrthonormal(), "New matrix is not orthonormal!");
    currentQuat.FromMatrix34(currentMatrix);
    newQuat.FromMatrix34(newMatrix);

    if ((currentQuat.Dot(newQuat))<0.0f)
    {
        currentQuat.Negate();
    }

    bool currentQuatValid = (currentQuat.Mag2() >= square(0.999f) && currentQuat.Mag2() <= square(1.001f));
    bool newQuatValid     = (newQuat.Mag2()  >= square(0.999f) && newQuat.Mag2()  <= square(1.001f));

    if(!currentQuatValid)
    {
        currentQuat.Normalize();
    }

    if(!newQuatValid)
    {
        newQuat.Normalize();
    }
}

#if DEBUG_DRAW

bool CNetBlenderPhysical::ShouldDisplayOrientationDebug() const
{
    if(GetBlenderData()->m_DisplayOrientationDebug)
    {
        if(GetBlenderData()->m_RestrictDebugDisplayToFocusEntity)
        {
            if(m_pPhysical && m_pPhysical == CDebugScene::FocusEntities_Get(0))
            {
                return true;
            }
        }
        else
        {
            return true;
        }
    }

    return false;
}

#endif // DEBUG_DRAW

CObjectBlenderDataArenaBall::CObjectBlenderDataArenaBall()
{
    m_LogarithmicBlendRatio = 0.2f;         // The ratio to use when blending to the target position logarithmically

    m_AllowAdjustVelChangeFromUpdates = true;         // indicates we are |allowing the per frame velocity change to be adjusted to match the changes coming over the network
    m_UseBlendVelSmoothing = true;         // indicates whether the blender will smooth velocity changes over a time interval, or just work frame to frame
    m_BlendVelSmoothTime = 150;          // the time in ms to base blender behaviour over (looks at where the object will be this time in the future)

    m_BlendRampTime = 750;
    m_BlendStopTime = 1000;

    m_LowSpeedThresholdSqr = 1.0f;

    // never use high speed mode
    m_HighSpeedThresholdSqr = 10000.f;

    m_NormalMode.m_MinVelChange = 0.4f; // minimum change in velocity for the blender to apply
    m_NormalMode.m_MaxVelChange = 3.0f; // maximum change in velocity for the blender to apply
    m_NormalMode.m_ErrorIncreaseVel = 0.3f; // current velocity change rate to used
    m_NormalMode.m_ErrorDecreaseVel = 0.2f; // the error distance to start increasing the change rate
    m_NormalMode.m_VelChangeRate = 0.3f; // the error distance to start decreasing the change rate

    const float POP_DIST = 5.0f;
    m_NormalMode.m_PositionDeltaMaxLow = POP_DIST;
    m_NormalMode.m_PositionDeltaMaxMedium = POP_DIST;
    m_NormalMode.m_PositionDeltaMaxHigh = POP_DIST;
}
