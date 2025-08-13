//
// name:        NetBlenderPed.cpp
// written by:  Daniel Yelland
// description: Network blender used by peds

// game includes
#include "network/objects/prediction/NetBlenderPed.h"

#include "animation/MovePed.h"
#include "peds/ped.h"
#include "peds/PedIntelligence.h"
#include "physics/gtainst.h"
#include "physics/physics.h"
#include "renderer/Water.h"
#include "task/motion/TaskMotionBase.h"
//
// rage includes
#include "fwanimation/directorcomponentragdoll.h"
#include "pharticulated/ArticulatedCollider.h"

// the number of ped network blenders required is the number of peds plus the number of players 
static const unsigned MAX_NET_PED_BLENDERS = MAX_NUM_NETOBJPEDS + MAX_NUM_NETOBJPLAYERS;

FW_INSTANTIATE_CLASS_POOL(CNetBlenderPed, MAX_NET_PED_BLENDERS, atHashString("CNetBlenderPed",0xeb9e8842));

NETWORK_OPTIMISATIONS()

#if __BANK
bool  CNetBlenderPed::ms_DisplayPushRoundCarsDebug      = false;
bool  CNetBlenderPed::ms_UseLocalPlayerForCarPushTarget = false;
#endif // __BANK

float CNetBlenderPed::ms_PushRoundCarsBoxExpandSize     = 0.45f;
float CNetBlenderPed::ms_PushRoundCarsForce             = 1000.0f;

static CPedBlenderDataAnimatedRagdollFallback s_pedBlenderDataAnimatedRagdollFallback;

CPedBlenderData *CNetBlenderPed::ms_pedBlenderDataAnimatedRagdoll = &s_pedBlenderDataAnimatedRagdollFallback;

// ===========================================================================================================================
// CPedPredictionData
// ===========================================================================================================================
CPedPredictionData::CPedPredictionData(const float heading, ObjectId standingOnObjectID, const Vector3 &standingOnLocalOffset, const u32 time)
{
    Reset(heading, standingOnObjectID, standingOnLocalOffset, time);
}

CPedPredictionData::~CPedPredictionData()
{
}

void CPedPredictionData::Reset(const float heading, ObjectId standingOnObjectID, const Vector3 &standingOnLocalOffset, const u32 time)
{
    m_snapshotPast.m_heading                  = heading;
    m_snapshotPast.m_headingTimestamp         = time;
    m_snapshotPast.m_StandingOnObjectID       = standingOnObjectID;
    m_snapshotPast.m_StandingOnLocalOffset    = standingOnLocalOffset;
    m_snapshotPast.m_StandingDataTimestamp    = time;

    m_snapshotPresent.m_heading               = heading;
    m_snapshotPresent.m_headingTimestamp      = time;
    m_snapshotPresent.m_StandingOnObjectID    = standingOnObjectID;
    m_snapshotPresent.m_StandingOnLocalOffset = standingOnLocalOffset;
    m_snapshotPresent.m_StandingDataTimestamp = time;

    m_snapshotFuture.m_heading                = heading;
    m_snapshotFuture.m_headingTimestamp       = time;
    m_snapshotFuture.m_StandingOnObjectID     = standingOnObjectID;
    m_snapshotFuture.m_StandingOnLocalOffset  = standingOnLocalOffset;
    m_snapshotFuture.m_StandingDataTimestamp  = time;
}

// ===========================================================================================================================
// CNetBlenderPed
// ===========================================================================================================================
CNetBlenderPed::CNetBlenderPed(CPed *ped, CPedBlenderData *blendData) :
netBlenderLinInterp(blendData, VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition()), ped->GetVelocity())
, m_Ped(ped)
, m_PredictionData(ped->GetCurrentHeading(), NETWORK_INVALID_OBJECT_ID, VEC3_ZERO, NetworkInterface::GetNetworkTime())
, m_BlendingHeading(false)
, m_CollisionTimer(0)
, m_CorrectionVector(VEC3_ZERO)
, m_LastTimestampStopped(0)
, m_BlendWhenStationary(false)
, m_BlendingInAllDirections(false)
, m_UseAnimatedRagdollBlending(false)
, m_DisableRagdollZBlending(false)
, m_RagdollAttached(false)
, m_TimeRagdollAttachDesired(0)
, m_StandingOnObject(false)
, m_IsRagdolling(false)
, m_IsOnStairs(false)
, m_CalculatedDesiredVelocity(false)
, m_LerpingOnObject(false)
, m_OnObjectLerpCurrentPos(VEC3_ZERO)
BANK_ONLY(, m_UsedAnimatedRagdollBlendingLastFrame(false))
{
}

CNetBlenderPed::~CNetBlenderPed()
{
}

void CNetBlenderPed::OnOwnerChange(blenderResetFlags resetFlag)
{
    // reset all snapshot data to time 0 so the first position update from the new owner is always applied
    netBlenderLinInterp::OnOwnerChange(resetFlag);
    m_PredictionData.Reset(GetHeadingFromObject(), m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID, m_PredictionData.GetSnapshotPresent().m_StandingOnLocalOffset, 0);

    // if the ped this blender is associated with has become locally controlled make sure
    // the ragdoll detachment is cleared
    if(m_Ped && !m_Ped->IsNetworkClone())
    {
        if(m_RagdollAttached)
        {
            DetachRagdoll();
			// need to clear this manually here since CNetBlenderPed::OnPedDetached will only get called for remotely controlled peds - which this ped no longer is...
			m_RagdollAttached = false;
        }
    }
}

void CNetBlenderPed::UpdateShouldBlendInAllDirections()
{
    if(m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID != NETWORK_INVALID_OBJECT_ID)
    {
        m_BlendingInAllDirections = false;
        return;
    }

    if(m_Ped->GetIsAnyFixedFlagSet())
    {
        return;
    }

	 CNetObjPed *netObjPed = SafeCast(CNetObjPed, m_Ped->GetNetworkObject());
    if(UsingInAirBlenderData() || (netObjPed && netObjPed->GetIsVaulting()))
    {
        m_BlendingInAllDirections = false;
        return;
    }

    Vector3 predictedVelocity = netBlenderLinInterp::GetPredictedVelocity(GetBlenderData()->m_MaxSpeedToPredict);

    Vector3 pedUp = VEC3V_TO_VECTOR3(m_Ped->GetTransform().GetC());
    float upMag   = predictedVelocity.Dot(pedUp);

    Vector3 worldAnimatedVelocity = VEC3V_TO_VECTOR3(m_Ped->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_Ped->GetAnimatedVelocity())));
    worldAnimatedVelocity.Normalize();
    float animMag   = predictedVelocity.Dot(worldAnimatedVelocity);

    float animToUpVelDiff = 0.0f;

    if(upMag > animMag)
    {
        animToUpVelDiff = upMag - animMag;
    }

    const float MIN_SPEED_TO_BLEND_IN_ALL_DIRECTIONS_SQR = rage::square(GetBlenderData()->m_MinAllDirThreshold);

    if(m_BlendingInAllDirections)
    {
        if(animToUpVelDiff < GetBlenderData()->m_StopAllDirThreshold || predictedVelocity.Mag2() < MIN_SPEED_TO_BLEND_IN_ALL_DIRECTIONS_SQR || upMag < GetBlenderData()->m_MinAllDirThreshold)
        {
            m_BlendingInAllDirections = false;
        }
    }
    else
    {
        if(animToUpVelDiff > GetBlenderData()->m_StartAllDirThreshold && predictedVelocity.Mag2() > MIN_SPEED_TO_BLEND_IN_ALL_DIRECTIONS_SQR && upMag > GetBlenderData()->m_MinAllDirThreshold)
        {
			m_BlendingInAllDirections = true;
        }
    }

    if(m_BlendingInAllDirections)
    {
        m_UseAnimatedRagdollBlending = true;
    }
}

#if __BANK
//PURPOSE
// Debug function for catching bad target velocities being set on the blender for the target object
//PARAMS
// velocity - The velocity to validate.
void CNetBlenderPed::ValidateTargetVelocity(const Vector3 &velocity)
{
	// ensure we don't set a velocity too high for the type of object being blended
	if(m_Ped && m_Ped->GetCurrentPhysicsInst() && m_Ped->GetCurrentPhysicsInst()->GetArchetype())
	{
		float maxSpeed = m_Ped->GetCurrentPhysicsInst()->GetArchetype()->GetMaxSpeed();

		if(velocity.XYMag2() > rage::square(maxSpeed))
		{
			const Vector3 &pastSnapPos    = netBlenderLinInterp::m_PredictionData.GetSnapshotPast().m_Position;
			const Vector3 &presentSnapPos = netBlenderLinInterp::m_PredictionData.GetSnapshotPresent().m_Position;
			const Vector3 &pastSnapVel    = netBlenderLinInterp::m_PredictionData.GetSnapshotPast().m_Velocity;
			const Vector3 &presentSnapVel = netBlenderLinInterp::m_PredictionData.GetSnapshotPresent().m_Velocity;
			gnetDebug1("Past    Pos Snap: (%.2f, %.2f, %.2f)->Timestamp: %d", pastSnapPos.x, pastSnapPos.y, pastSnapPos.z, netBlenderLinInterp::m_PredictionData.GetSnapshotPast().m_PositionTimestamp);
			gnetDebug1("Present Pos Snap: (%.2f, %.2f, %.2f)->Timestamp: %d", presentSnapPos.x, presentSnapPos.y, presentSnapPos.z, netBlenderLinInterp::m_PredictionData.GetSnapshotPresent().m_PositionTimestamp);
			gnetDebug1("Past    Vel Snap: (%.2f, %.2f, %.2f)->Timestamp: %d", pastSnapVel.x, pastSnapVel.y, pastSnapVel.z, netBlenderLinInterp::m_PredictionData.GetSnapshotPast().m_VelocityTimestamp);
			gnetDebug1("Present Vel Snap: (%.2f, %.2f, %.2f)->Timestamp: %d", presentSnapVel.x, presentSnapVel.y, presentSnapVel.z, netBlenderLinInterp::m_PredictionData.GetSnapshotPresent().m_VelocityTimestamp);

			ObjectId pastStandingOnObjectID    = m_PredictionData.GetSnapshotPast().m_StandingOnObjectID;
			ObjectId presentStandingOnObjectID = m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID;

			if(pastStandingOnObjectID != NETWORK_INVALID_OBJECT_ID)
			{
				const Vector3 &pastSnapStandingOffset = CNetBlenderPed::m_PredictionData.GetSnapshotPast().m_StandingOnLocalOffset;

				char logName[netObject::MAX_LOG_NAME];
				NetworkInterface::GetObjectManager().GetLogName(logName, netObject::MAX_LOG_NAME, pastStandingOnObjectID);
				gnetDebug1("Past Standing Data: Object:%s Pos Snap: (%.2f, %.2f, %.2f)->Timestamp: %d", logName, pastSnapStandingOffset.x, pastSnapStandingOffset.y, pastSnapStandingOffset.z, CNetBlenderPed::m_PredictionData.GetSnapshotPast().m_StandingDataTimestamp);
			}
			else
			{
				gnetDebug1("Past Standing Data: Not standing on object");
			}

			if(presentStandingOnObjectID != NETWORK_INVALID_OBJECT_ID)
			{
				const Vector3 &presentSnapStandingOffset = CNetBlenderPed::m_PredictionData.GetSnapshotPresent().m_StandingOnLocalOffset;

				char logName[netObject::MAX_LOG_NAME];
				NetworkInterface::GetObjectManager().GetLogName(logName, netObject::MAX_LOG_NAME, presentStandingOnObjectID);
				gnetDebug1("Present Standing Data: Object:%s Pos Snap: (%.2f, %.2f, %.2f)->Timestamp: %d", logName, presentSnapStandingOffset.x, presentSnapStandingOffset.y, presentSnapStandingOffset.z, CNetBlenderPed::m_PredictionData.GetSnapshotPresent().m_StandingDataTimestamp);
			}
			else
			{
				gnetDebug1("Present Standing Data: Not standing on object");
			}

			gnetAssertf(0, "Setting too large a velocity (%.2f, %.2f, %.2f) target object for %s!", velocity.x, velocity.y, velocity.z, m_Ped->GetNetworkObject() ? m_Ped->GetNetworkObject()->GetLogName() : "Non-networked ped");
		}
	}
}
#endif // __BANK

//
// Name         :   Update
// Purpose      :   Performs the blend and returns true if there was any change to the object
void CNetBlenderPed::Update()
{
    UpdateCollisionTimer();

    if(m_Ped->IsDead() || !IsBlendingOn())
    {
        if(m_RagdollAttached)
        {
            DetachRagdoll();
        }

        return;
    }

    bool blendingHeading = m_BlendingHeading;

    // Handle blending of the peds position
    if(m_Ped->GetUsingRagdoll())
    {
        m_CorrectionVector        = VEC3_ZERO;
        m_BlendingInAllDirections = false;

        // we have to do special blending for ragdolls
        UpdateRagdollBlending();

        // can't blend the ped's heading when they are ragdolling
        blendingHeading = false;
    }
    else
    {
        if(m_Ped->GetIsAnyFixedFlagSet())
        {
            CNetObjPed *netObjPed = SafeCast(CNetObjPed, GetNetworkObject());
            Vector3 targetPosition = GetCurrentPredictedPosition();

			if(netObjPed && netObjPed->IsHiddenUnderMapDueToVehicle())
			{
				targetPosition.z = GetPositionFromObject().z;
			}				

            DoLogarithmicBlend(targetPosition, LOGARITHMIC_BLEND_USE_POS);
        }

        UpdateShouldBlendInAllDirections();

        if(m_UseAnimatedRagdollBlending)
        {
            UpdateAnimatedRagdollFallbackBlending();
        }

        if(m_RagdollAttached)
        {
            DetachRagdoll();
        }

        m_TimeRagdollAttachDesired = 0;
    }

	m_DisableRagdollZBlending  = false;

    if(!m_UseAnimatedRagdollBlending)
    {
        CheckForStuckOnVehicle();

        // Handle blending of the peds heading
        if(blendingHeading)
        {
            UpdateHeadingBlending();
        }
    }

    BANK_ONLY(m_UsedAnimatedRagdollBlendingLastFrame = m_UseAnimatedRagdollBlending);

    m_UseAnimatedRagdollBlending = false;
}

void CNetBlenderPed::UpdateHeadingBlending()
{
    if(!IsBlendingOn() || static_cast<CNetObjPed*>(m_Ped->GetNetworkObject())->NetworkHeadingBlenderIsOverridden())
    {
		m_BlendingHeading = true;
        return;
    }

    gnetAssertf(!m_Ped->IsDead(), "Trying to blend heading for a dead ped!");
    gnetAssertf(!m_Ped->GetUsingRagdoll(), "Trying to blend heading for a ragdolling ped!");

    float newHeading   = GetHeadingFromObject();
    float headingDelta = m_PredictionData.GetSnapshotFuture().m_heading - newHeading;

    // get shortest delta
	headingDelta = fwAngle::LimitRadianAngle(headingDelta);

    if (GetBlenderData()->m_BlendHeading && rage::Abs(headingDelta) < GetBlenderData()->m_HeadingBlendMax)
    {
        if(rage::Abs(headingDelta) > GetBlenderData()->m_HeadingBlendMin)
        {
            // if we aren't predicting the heading gradually move
            // towards the target to smooth the motion
            float timeStepModifier = fwTimer::GetTimeStep() * 30.0f;

            float timeRatio = GetBlenderData()->m_HeadingBlend * timeStepModifier;

            // cap the time ratio
            if(timeRatio > 1.0f)
            {
                timeRatio = 1.0f;
            }

            newHeading = fwAngle::LimitRadianAngle(newHeading + headingDelta * timeRatio);
        }
        else
        {
            m_Ped->SetDesiredHeading(GetHeadingFromObject());
            m_BlendingHeading = false;
        }
    }
    else
    {
        newHeading = m_PredictionData.GetSnapshotFuture().m_heading;
        m_Ped->SetDesiredHeading(newHeading);
        m_BlendingHeading = false;
    }

    SetHeadingOnObject(newHeading);
}

// reset the prediction data ready for starting again
void CNetBlenderPed::Reset()
{
    netBlenderLinInterp::Reset();

    m_PredictionData.Reset(GetHeadingFromObject(), m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID, m_PredictionData.GetSnapshotPresent().m_StandingOnLocalOffset, GetLastSyncMessageTime());

    if(m_Ped)
    {
        m_Ped->SetDesiredHeading(GetHeadingFromObject());
    }
}

// go straight to the target of the latest snapshot received
void CNetBlenderPed::GoStraightToTarget()
{
	// stop this getting called on dead peds as it can cause them to end up in a weird orientation if they are in dead ragdoll frame
	if (m_Ped && m_Ped->GetMovePed().GetState()==CMovePed::kStateStaticFrame) 
	{
		return;
	}

    // we don't want to do BB checks when the object is being moved straight to it's target position
    bool oldDoBBChecksOnWarp = GetBlenderData()->m_DoBBChecksOnWarp;
    GetBlenderData()->m_DoBBChecksOnWarp = false;

    netBlenderLinInterp::GoStraightToTarget();

	if (m_Ped->GetIsInWater() && DisableZBlending())
	{
		Vector3 pedPos = VEC3V_TO_VECTOR3(m_Ped->GetTransform().GetPosition());
		Water::GetWaterLevel(pedPos, &pedPos.z, false, POOL_DEPTH, REJECTIONABOVEWATER, NULL);
		SetPositionOnObject(pedPos, false);
	}

    if(m_Ped && netBlenderLinInterp::GetPredictedVelocity(GetBlenderData()->m_MaxSpeedToPredict).Mag2() < 0.01f)
    {
        m_Ped->SetVelocity(VEC3_ZERO);
    }

    GetBlenderData()->m_DoBBChecksOnWarp = oldDoBBChecksOnWarp;

    SetHeadingOnObject(m_PredictionData.GetSnapshotPresent().m_heading);

    m_CorrectionVector = VEC3_ZERO;
}

//
// Name         :   Update
// Purpose      :   Performs the blend and returns true if there was any change to the object
void CNetBlenderPed::RecalculateTarget()
{
    if(GetBlenderData()->m_ExtrapolateHeading == false)
    {
        // if we aren't extrapolating, just set the target state
        // to the last snapshot data we received
        m_PredictionData.GetSnapshotFuture() = m_PredictionData.GetSnapshotPresent();
    }
    else
    {
        // ensure the present snapshot was not before the past snapshot (this should not be allowed to happen via the snapshot accessors)
        gnetAssert(m_PredictionData.GetSnapshotPast().m_headingTimestamp <= m_PredictionData.GetSnapshotPresent().m_headingTimestamp);

        // predict a snapshot into the future
        u32 currentTime      = NetworkInterface::GetNetworkTime();
        u32 estimatedLatency = (currentTime > GetLastSyncMessageTime()) ? (currentTime - GetLastSyncMessageTime()) : 0;

        u32 futureTime = NetworkInterface::GetNetworkTime() + estimatedLatency;

        u32 futureHeadingTime = futureTime;

        // adjust the future time if we are running behind time
        if(futureHeadingTime < m_PredictionData.GetSnapshotPresent().m_headingTimestamp)
        {
            futureHeadingTime = m_PredictionData.GetSnapshotPresent().m_headingTimestamp;
        }

        // predict the future heading
        float headingDelta     = m_PredictionData.GetSnapshotPresent().m_heading - m_PredictionData.GetSnapshotPast().m_heading;
        s32 headingTimeDelta = m_PredictionData.GetSnapshotPresent().m_headingTimestamp - m_PredictionData.GetSnapshotPast().m_headingTimestamp;

        float headingVelocity           = 0.0f;

		headingDelta = fwAngle::LimitRadianAngle(headingDelta);

        if(headingTimeDelta > 0)
        {
            headingVelocity = (headingDelta / headingTimeDelta);
        }

        float futureHeading = m_PredictionData.GetSnapshotPresent().m_heading + (headingVelocity * (futureHeadingTime - m_PredictionData.GetSnapshotPresent().m_headingTimestamp));

		futureHeading = fwAngle::LimitRadianAngle(futureHeading);

        m_PredictionData.GetSnapshotFuture().m_heading          = futureHeading;
        m_PredictionData.GetSnapshotFuture().m_headingTimestamp = futureHeadingTime;

        // reenable blending
        m_BlendingHeading = true;
    }
}

void CNetBlenderPed::ProcessPrePhysics()
{
    if(m_Ped->IsDead())
    {
        netBlenderLinInterp::m_BlendingPosition = true; // set blending on here so that the ped can adjust to the new position once it stops ragdolling
        m_BlendingHeading = true;
        m_CorrectionVector = VEC3_ZERO;
        return;
    }

    if (!IsBlendingOn())
    {
        m_CorrectionVector = VEC3_ZERO;
        return;
    }

    if(!GetNetworkObject()->CanBlend() || GetNetworkObject()->NetworkBlenderIsOverridden())
    {
        return;
    }

    if(m_UseAnimatedRagdollBlending || m_BlendingInAllDirections)
    {
        if(m_Ped)
        {
            m_Ped->SetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsuleControl, true);
        }

        return;
    }

    if(m_Ped->GetUsingRagdoll())
    {
        return;
    }

    if(m_Ped->GetIsAnyFixedFlagSet())
    {
        return;
    }

    if(UsingInAirBlenderData())
    {
        Vector3 predictedVelocity = GetPredictedVelocity(GetBlenderData()->m_MaxSpeedToPredict);
        UpdateVelocity(predictedVelocity, GetLastSyncMessageTime());
        netBlenderLinInterp::Update();

        return;
    }

    Vector3 oldDesiredVelocity = m_Ped->GetDesiredVelocity();

	bool bDisableZBlending = DisableZBlending();

    bool standingOnObject = (m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID != NETWORK_INVALID_OBJECT_ID);

    float maxSpeedToPredict = standingOnObject ? 10000.0f : GetBlenderData()->m_MaxSpeedToPredict;
    Vector3 predictedVelocity = GetPredictedVelocity(maxSpeedToPredict);
    UpdateVelocity(predictedVelocity, GetLastSyncMessageTime());

    bool animatedVelocityZero = m_Ped->GetAnimatedVelocity().Mag2() < 0.01f;

	m_StandingOnObject = false;

    Vector3 correctedDesiredVelocity = oldDesiredVelocity;

    if(m_CalculatedDesiredVelocity)
    {
        correctedDesiredVelocity += m_CorrectionVector;
        m_CalculatedDesiredVelocity = false;
    }
    else
    {
        gnetAssertf(m_Ped->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodPhysics), "Desired velocity has not been calculated for %s this frame!", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "Unknown");
    }

	if (bDisableZBlending)
	{
		correctedDesiredVelocity.z = oldDesiredVelocity.z;
	}

    m_Ped->SetDesiredVelocity(correctedDesiredVelocity);

    if(!m_Ped->GetUsingRagdoll())
    {
        float oldMaxVelDiff   = GetBlenderData()->m_NormalMode.m_MaxVelDiffFromTarget;
        float oldMinVelChange = GetBlenderData()->m_NormalMode.m_MinVelChange;
        float oldMaxVelChange = GetBlenderData()->m_NormalMode.m_MaxVelChange;

        // modify values based on MBR
        float desiredMoveBlendRatioX = 0.0f;
        float desiredMoveBlendRatioY = 0.0f;
        CNetObjPed *netObjPed = SafeCast(CNetObjPed, m_Ped->GetNetworkObject());
        netObjPed->GetDesiredMoveBlendRatios(desiredMoveBlendRatioX, desiredMoveBlendRatioY);

        Vector3 worldAnimatedVelocity = VEC3V_TO_VECTOR3(m_Ped->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_Ped->GetAnimatedVelocity())));

        if(desiredMoveBlendRatioX < 0.01f && desiredMoveBlendRatioY < 0.01f)
        {
            static float tweakMaxVelDiff   = 2.0f;
            static float tweakMinVelChange = 0.5f;
            GetBlenderData()->m_NormalMode.m_MaxVelDiffFromTarget = tweakMaxVelDiff;
            GetBlenderData()->m_NormalMode.m_MinVelChange         = tweakMinVelChange;

            if(m_CurrentVelChange < tweakMinVelChange)
            {
                m_CurrentVelChange = tweakMinVelChange;
            }

            // try to reduce backwards sliding
            Vector3 normalisedDesiredVel  = correctedDesiredVelocity;
            Vector3 normalisedAnimatedVel = worldAnimatedVelocity;
            normalisedDesiredVel.Normalize();
            normalisedAnimatedVel.Normalize();

            if(normalisedDesiredVel.Dot(normalisedAnimatedVel) < 0.0f)
            {
                static float tweakMaxInvertVelDiff = 0.5f;
                GetBlenderData()->m_NormalMode.m_MaxVelDiffFromTarget = tweakMaxInvertVelDiff;
            }

            // local and remote ped have stopped moving and MBR is 0, we will allow sliding if the position changes before the MBR changes
            if(m_Ped->GetVelocity().Mag2() < 0.01f && netBlenderLinInterp::GetPredictedVelocity(GetBlenderData()->m_MaxSpeedToPredict).Mag2() < 0.01f)
            {
                GetLastPositionReceived(&m_LastTimestampStopped);
                m_BlendWhenStationary = true;
            }
        }
        else
        {
            // modify values if we are lagging behind
            Vector3 objectPosition           = GetPositionFromObject();
            Vector3 predictedCurrentPosition = GetCurrentPredictedPosition();
            Vector3 delta                    = predictedCurrentPosition - objectPosition;

            float catchUpThreshold = standingOnObject ? 0.01f : 0.5f;

            if(delta.Mag2() > rage::square(catchUpThreshold))
            {
                Vector3 normalisedAnimatedVel = worldAnimatedVelocity;
                normalisedAnimatedVel.Normalize();
                delta.Normalize();

                if(normalisedAnimatedVel.Dot(delta) > 0.0f)
                {
                    static float catchUpMaxVelDiff   = 2.0f;
                    static float catchUpMaxVelChange = 1.0f;

                    GetBlenderData()->m_NormalMode.m_MaxVelDiffFromTarget = catchUpMaxVelDiff;
                    GetBlenderData()->m_NormalMode.m_MaxVelChange         = catchUpMaxVelChange;
                }
            }

            m_BlendWhenStationary = false;
        }

		netObject* pNetObject = NULL;
		bool objectStoodOnExists = false;
		if(standingOnObject)
        {
			pNetObject = NetworkInterface::GetNetworkObject(m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID);

            objectStoodOnExists = pNetObject != 0;

            // if the object the ped is standing on has been created on this machine we have to adjust the blended position
            if(objectStoodOnExists)
            {
				//Alter values for adjusting velocity that are used by the blender if we are on an object...
				GetBlenderData()->m_NormalMode.m_MaxVelDiffFromTarget = 5.0f;
				GetBlenderData()->m_NormalMode.m_MaxVelChange         = 5.0f;
				GetBlenderData()->m_NormalMode.m_MaxVelChangeToTarget = 10.0f;

                Vector3 oldTargetPosition = GetPositionFromObject();
                Vector3 newTargetPosition = GetCurrentPredictedPosition();
                const float epsilon = 0.01f;

                if(!oldTargetPosition.IsClose(newTargetPosition, epsilon))
                {
                    UpdatePosition(newTargetPosition, GetLastSyncMessageTime());
                }
                else
                {
                    if(fabs(m_Ped->GetTransform().GetPosition().GetZf() - GetCurrentPredictedPosition().z) > 0.25f)
                    {
                        m_PerformedAfterBlendCorrection = false;
                    }
                }

                m_Ped->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodPhysics);

				m_StandingOnObject = true;
            }
        }

        netBlenderLinInterp::Update();

        GetBlenderData()->m_NormalMode.m_MaxVelDiffFromTarget = oldMaxVelDiff;
        GetBlenderData()->m_NormalMode.m_MinVelChange         = oldMinVelChange;
        GetBlenderData()->m_NormalMode.m_MaxVelChange         = oldMaxVelChange;
    }

    // if the ped is not moving on their moving ground physical leave it motionless
    CPhysical *groundPhysical = m_Ped->GetGroundPhysical();

    if(groundPhysical && groundPhysical->GetNetworkObject() && groundPhysical->GetNetworkObject()->GetObjectID() == m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID)
    {
        const Vector3 groundVelocity = VEC3V_TO_VECTOR3(m_Ped->GetGroundVelocity());

        if(animatedVelocityZero && groundVelocity.Mag2() > 0.1f)
        {
            m_Ped->SetDesiredVelocity(groundVelocity);
        }
    }

    Vector3 newDesiredVelocity = m_Ped->GetDesiredVelocity();
    Vector3 correctionVector   = newDesiredVelocity - correctedDesiredVelocity;

    m_CorrectionVector += correctionVector;

    if(m_CorrectionVector.Mag2() > rage::square(GetBlenderData()->m_MaxCorrectionVectorMag))
    {
        m_CorrectionVector.Normalize();
        m_CorrectionVector.Scale(GetBlenderData()->m_MaxCorrectionVectorMag);
    }
    
    CheckForOnStairsFixup();

    if(bDisableZBlending)
    {
        newDesiredVelocity.z = oldDesiredVelocity.z;
        m_CorrectionVector.z = 0.0f;

        m_Ped->SetDesiredVelocity(newDesiredVelocity);
    }

    // if the animated velocity is zero stop blender corrections being applied to the ped,
    // better to leave the ped slightly out of sync than slide without animating
    bool allowSliding = standingOnObject;

    if(!allowSliding && m_BlendWhenStationary)
    {
        u32 lastSnapTime = 0;
        GetLastPositionReceived(&lastSnapTime);

        if(lastSnapTime > m_LastTimestampStopped)
        {
            CNetObjPed *netObjPed = SafeCast(CNetObjPed, m_Ped->GetNetworkObject());

            if(netObjPed && !netObjPed->HasPedStopped())
            {
                allowSliding = true;
            }
        }
    }

    if(animatedVelocityZero && !allowSliding)
    {
        m_Ped->SetDesiredVelocity(oldDesiredVelocity);
    }

    if(newDesiredVelocity.IsZero())
    {
        m_CorrectionVector = VEC3_ZERO;
    }

	Vector3 vPedDesiredVel = m_Ped->GetDesiredVelocity();

	if (m_Ped->GetIsInWater())
	{
		if (vPedDesiredVel.z > 0.0f && DisableZBlending())
		{
			Vector3 pedPos = VEC3V_TO_VECTOR3(m_Ped->GetTransform().GetPosition());
			float waterLevel;

			Water::GetWaterLevel(pedPos, &waterLevel, false, POOL_DEPTH, REJECTIONABOVEWATER, NULL);

			if (pedPos.z > waterLevel)
			{
				vPedDesiredVel.z = 0.0f;
			}
		}

		m_Ped->SetDesiredVelocity(vPedDesiredVel);
	}
	else
	{
        CPhysical *physicalToCheck = m_Ped->GetGroundPhysical();

        if(!physicalToCheck && m_Ped->GetPedResetFlag(CPED_RESET_FLAG_IsClimbing))
        {
            physicalToCheck = m_Ped->GetClimbPhysical();
        }

        bool skipClamp = physicalToCheck && physicalToCheck->GetIsTypeVehicle() && ((CVehicle*)physicalToCheck)->CanPedsStandOnTop();

        if(!skipClamp)
        {
#if FPS_MODE_SUPPORTED
            bool bFPSModeEnabled = m_Ped->IsFirstPersonShooterModeEnabledForPlayer(false, true, true);

            if(!bFPSModeEnabled || (bFPSModeEnabled && (m_Ped->GetPedResetFlag(CPED_RESET_FLAG_IsJumping) || (m_Ped->GetCurrentMotionTask() && m_Ped->GetCurrentMotionTask()->GetTaskType() != CTaskTypes::TASK_MOTION_AIMING))))
#endif // FPS_MODE_SUPPORTED
		    {
                // Find the difference between the desired velocity and the actual velocity of the ped and clamp that difference to prevent huge changes in velocity in a short time.
		        // Prevents issues where the desired velocity coming in from the network would completely override the physically correct velocity calculated locally.		
		        Vector3 vPedVel = m_Ped->GetVelocity();
		        Vector3 vVelDiff = vPedDesiredVel - vPedVel;

		        Vec3V vAdjustedVelDiff = m_Ped->GetCurrentMotionTask()->CalcVelChangeLimitAndClamp(*m_Ped,  VECTOR3_TO_VEC3V(vVelDiff), ScalarV(fwTimer::GetTimeStep()), m_Ped->GetGroundPhysical());
		        vPedVel += VEC3V_TO_VECTOR3(vAdjustedVelDiff);

		        if (bDisableZBlending)
		        {
			        vPedVel.z = oldDesiredVelocity.z;
		        }

		        m_Ped->SetDesiredVelocity(vPedVel);
            }
        }
	}
		
    // if the clone is in the air make sure this isn't because we have overshot a ledge
    Vector3 currentPos        = VEC3V_TO_VECTOR3(m_Ped->GetTransform().GetPosition());
    Vector3 predictedPosition = GetCurrentPredictedPosition();

    if(m_Ped->IsPlayer() && !m_Ped->GetIsInWater() && CPedGeometryAnalyser::IsInAir(*m_Ped))
    {
        Vector3 predictedVelocity = netBlenderLinInterp::GetPredictedVelocity(GetBlenderData()->m_MaxSpeedToPredict);
        Vector3 forwardVector     = GetForwardVectorFromObject();
        Vector3 posDelta          = currentPos - predictedPosition;
        posDelta.Normalize();

        if(predictedVelocity.IsClose(VEC3_ZERO, 0.01f) && forwardVector.Dot(posDelta) > 0.0f)
        {
            gnetDebug2("%s is falling when predicted velocity is zero (probably due to overshooting a ledge)!", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "Unknown");
            GoStraightToTarget();
        }
    }

    // check if a script ped or player are in a different interior state, if the position delta
    // is too high we need to check the ped has not got trapped behind a wall
    CNetObjPed *netObjPed = SafeCast(CNetObjPed, GetNetworkObject());

    if(netObjPed && (netObjPed->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT) || m_Ped->IsPlayer()))
    {
        if(!netObjPed->IsCloneInTargetInteriorState())
        {
            Vector3 posDelta = currentPos - predictedPosition;

            if(posDelta.Mag2() >= rage::square(GetBlenderData()->m_MaxPosDeltaDiffInteriors))
            {
                gnetDebug2("Performing probe for %s (checking for trapped in an interior)!", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "Unknown");

                Vector3 lastPosReceived = GetLastPositionReceived();

                if(TestForMapCollision(lastPosReceived, currentPos))
                {
                    gnetDebug2("Detected collision - clone is probably trapped in an interior - %s has popped!", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "Unknown");
                    GoStraightToTarget();
                }
            }
        }
    }

    CheckForIntersectingObjects();
}

void CNetBlenderPed::ProcessPostPhysics()
{
    netBlenderLinInterp::ProcessPostPhysics();

    bool wasLerpingOnObject = m_LerpingOnObject;
    m_LerpingOnObject       = false;

    if(m_Ped->IsDead())
    {
        return;
    }

    if (!IsBlendingOn())
    {
        return;
    }

    if(!GetNetworkObject()->CanBlend() || GetNetworkObject()->NetworkBlenderIsOverridden())
    {
        return;
    }

    if(m_UseAnimatedRagdollBlending)
    {
        return;
    }

    if(m_Ped->GetUsingRagdoll())
    {
        return;
    }

    if(m_Ped->GetIsAnyFixedFlagSet())
    {
        return;
    }

    bool standingOnObject = (m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID != NETWORK_INVALID_OBJECT_ID);

    if(standingOnObject)
    {
        // peds standing on objects have their position set directly, it's difficult
        // to get this to work by setting velocities in some cases (a fast moving boat in a stormy sea for example)
        netObject *networkObject = NetworkInterface::GetNetworkObject(m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID);

        if(ShouldLerpOnObject(networkObject))
        {
            m_LerpingOnObject = true;
            CEntity   *entity = networkObject ? networkObject->GetEntity() : 0;

            if(entity)
            {
                // calculate position on object from last update
                Vector3 objectPosition  = VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition());
                Vector3 localOffset     = VEC3V_TO_VECTOR3(entity->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_PredictionData.GetSnapshotPresent().m_StandingOnLocalOffset)));
                Vector3 targetPosition  = objectPosition + localOffset;
				
                if(!wasLerpingOnObject)
                {
                    m_OnObjectLerpCurrentPos = targetPosition - objectPosition;
                }

                Vector3 lastPosition    = objectPosition + m_OnObjectLerpCurrentPos;
                Vector3 positionDelta   = targetPosition - lastPosition;
                Vector3 blendedPosition = targetPosition;
                float   distSqr         = positionDelta.Mag();

                if(distSqr >= GetBlenderData()->m_PositionDeltaMin && distSqr < GetPositionMaxForUpdateLevel())
                {
                    blendedPosition = lastPosition + (positionDelta * GetBlenderData()->m_LerpOnObjectBlendRatio);
                }

                m_OnObjectLerpCurrentPos = blendedPosition - objectPosition;

                Vector3 currPos = GetPositionFromObject();
                bool warp = (currPos - blendedPosition).Mag2() > rage::square(4.0f);
                SetPositionOnObject(blendedPosition, warp);
            }
        }
    }
}

// return the last heading received for this object
float CNetBlenderPed::GetLastHeadingReceived()
{
    return m_PredictionData.GetSnapshotPresent().m_heading;
}

// Name         : UpdateHeading
// Purpose      : Updates the heading value for the blender
void CNetBlenderPed::UpdateHeading(float heading, u32 time)
{
    if(time >= m_PredictionData.GetSnapshotPresent().m_headingTimestamp)
    {
        // move snapshot B position to snapshot A, and copy new position to snapshot B
        m_PredictionData.GetSnapshotPast().m_heading          = m_PredictionData.GetSnapshotPresent().m_heading;
        m_PredictionData.GetSnapshotPast().m_headingTimestamp = m_PredictionData.GetSnapshotPresent().m_headingTimestamp;

        m_PredictionData.GetSnapshotPresent().m_heading          = heading;
        m_PredictionData.GetSnapshotPresent().m_headingTimestamp = time;
        m_BlendingHeading = true;

        RecalculateTarget();
    }
}

void CNetBlenderPed::UpdateStandingData(ObjectId standingOnObjectID, const Vector3 &standingOnLocalOffset, u32 time)
{
    if(time >= m_PredictionData.GetSnapshotPresent().m_StandingDataTimestamp)
    {
        // move snapshot B position to snapshot A, and copy new position to snapshot B
        m_PredictionData.GetSnapshotPast().m_StandingOnObjectID    = m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID;
        m_PredictionData.GetSnapshotPast().m_StandingOnLocalOffset = m_PredictionData.GetSnapshotPresent().m_StandingOnLocalOffset;
        m_PredictionData.GetSnapshotPast().m_StandingDataTimestamp = m_PredictionData.GetSnapshotPresent().m_StandingDataTimestamp;

        m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID    = standingOnObjectID;
        m_PredictionData.GetSnapshotPresent().m_StandingOnLocalOffset = standingOnLocalOffset;
        m_PredictionData.GetSnapshotPresent().m_StandingDataTimestamp = time;
    }
}

bool CNetBlenderPed::DisableZBlending() const
{
    if(UsingInAirBlenderData())
    {
        return false;
    }

    // animated peds let Z resolve locally, unless they are diving under water,
    // we check whether the local or remote are diving to handle transitions when surfacing
    CNetObjPed *netObjPed = SafeCast(CNetObjPed, m_Ped->GetNetworkObject());

	bool isDiving = false;

	if (m_Ped->GetIsInWater())
	{
		isDiving = netObjPed->GetIsDiving();

		if(m_Ped->GetPrimaryMotionTask())
		{
			isDiving |= m_Ped->GetPrimaryMotionTask()->IsDiving();
		}

		Vector3 pedPos = GetPositionFromObject();

		float waterZ;

		Water::GetWaterLevelNoWaves(pedPos, &waterZ, POOL_DEPTH, REJECTIONABOVEWATER, NULL);

		if (pedPos.z >= waterZ)
		{
			isDiving = false;
		}
	}

	// If this ped has turned into a bird, let the blender do Z blending
	if(m_Ped->GetCapsuleInfo()->IsBird())
	{
		return false;
	}
	
	return !m_Ped->GetUsingRagdoll() && !isDiving && !m_UseAnimatedRagdollBlending && !m_Ped->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected);
}

bool CNetBlenderPed::DoNoZBlendHeightCheck(float heightDelta) const
{
    // don't want to correct a peds height if they are falling (we can wait until they land)
    if(m_Ped)
    {
        CNetObjPed *netObjPed = SafeCast(CNetObjPed, m_Ped->GetNetworkObject());

        if(!(netObjPed && netObjPed->IsHiddenUnderMapDueToVehicle()) &&
           ! m_Ped->GetPedResetFlag(CPED_RESET_FLAG_IsFalling) &&
           !(DisableZBlending() && m_Ped->GetIsInWater()))
        {
            if(fabs(heightDelta) > GetBlenderData()->m_MaxHeightDelta)
            {
                return true;
            }
        }
    }

    return false;
}

const Vector3 CNetBlenderPed::GetPreviousPositionFromObject() const
{
    return m_Ped->GetPreviousPosition();
}

// get the current position from the object we are predicting
const Vector3 CNetBlenderPed::GetPositionFromObject() const
{
    return VEC3V_TO_VECTOR3(m_Ped->GetCurrentPhysicsInst() ? m_Ped->GetCurrentPhysicsInst()->GetMatrix().GetCol3() : m_Ped->GetTransform().GetPosition());
}

// get the current velocity of the object we are predicting
const Vector3 CNetBlenderPed::GetVelocityFromObject() const
{
    if(UsingInAirBlenderData() || m_UseAnimatedRagdollBlending)
    {
        return m_Ped->GetVelocity();
    }
    else
    {
        return m_Ped->GetDesiredVelocity();
    }
}

const Vector3 CNetBlenderPed::GetForwardVectorFromObject() const
{
    return VEC3V_TO_VECTOR3(m_Ped->GetTransform().GetB());
}

// get the current heading of the object we are predicting
float CNetBlenderPed::GetHeadingFromObject() const
{
    return m_Ped->GetCurrentHeading();
}

bool CNetBlenderPed::UsingInAirBlenderData() const
{
    const CPedBlenderData *pedBlenderData = SafeCast(const CPedBlenderData, GetBlenderData());

    if(pedBlenderData && pedBlenderData->IsInAirBlenderData())
    {
        return true;
    }

    return false;
}

// set the position of the network object we are blending
bool CNetBlenderPed::SetPositionOnObject(const Vector3 &position, bool warp)
{
    // we can't set the position on an object if we are attached to something
    if(m_Ped->GetIsAttached())
    {
        gnetWarning("Network blender cannot set position on %s as it is attached!", m_Ped->GetNetworkObject() ? m_Ped->GetNetworkObject()->GetLogName() : "Unknown");
        return true;
    }

	if(warp && !m_Ped->GetUsingRagdoll() && m_Ped->GetMovePed().GetState()!=CMovePed::kStateStaticFrame)
    {
        Vector3 lastPosReceived = GetLastPositionReceived();

        if(!lastPosReceived.IsClose(position, 0.1f))
        {
            // for script peds we want to make sure we don't
            // predict through the ground or buildings, so we need to do another probe here.
            // ideally this would be done for all peds, but we don't want the network code
            // doing too many world probes
            if(ShouldTestForMapCollision() && TestForMapCollision(lastPosReceived, position))
            {
				return true;                
            }
        }

        Vector3 newPosition = position;

        // if the last position we received was a ragdoll position we need to probe to make sure
        // using this to place an animated ped won't cause a ground intersection
        if(m_IsRagdolling)
        {
            const float groundToRootOffset = m_Ped->GetCapsuleInfo() ? m_Ped->GetCapsuleInfo()->GetGroundToRootOffset() : 1.0f;
            float groundZ = 0.0f;

            if(TestForGroundZ(newPosition, groundZ))
            {
                if(newPosition.z < groundZ + groundToRootOffset)
                {
                    newPosition.z = groundZ + groundToRootOffset;

                    gnetDebug2("CNetBlenderPed::SetPositionOnObject - Adjusting warp position from (%.2f,%.2f,%.2f) to (%.2f,%.2f,%.2f)", position.x, position.y, position.z, newPosition.x, newPosition.y, newPosition.z);
                }
            }
        }

		// We don't need to do a portal rescan if the ped has been fixed by network, as a rescan willbe done when the ped becomes unfixed 
		m_Ped->Teleport(newPosition, GetLastHeadingReceived(), false, !m_Ped->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK));

        if(m_Ped->GetVelocity().IsZero())
        {
            m_Ped->ResetStandData();
            m_Ped->ActivatePhysics();
        }

        m_CorrectionVector = VEC3_ZERO;
    }
    else
    {
        Vector3 newPos = position;

        // ignore z component if the ped is in dead ragdoll frame, or the ped is ragdolling on the other machine but not ours, or vice versa
        // (its z position will differ), we don't do this if we are standing on an object as the position will already have been fixed up
		bool standingOnObject = NetworkInterface::GetNetworkObject(m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID) != 0;
	
        if(!standingOnObject)
        {
			if (m_Ped->GetMovePed().GetState()==CMovePed::kStateStaticFrame || 
			    m_IsRagdolling != m_Ped->GetUsingRagdoll())
            {
                newPos.z = m_Ped->GetTransform().GetPosition().GetZf();
            }
        }

        // we can't set the position directly if the ped is ragdolling,
        // so we need to switch back to animated
        if(m_Ped->GetUsingRagdoll())
        {
            gnetAssertf(warp, "Setting the position of clone ragdolling ped without warping it! Ped will be switched to animated!");

            bool canChangePosition = !m_IsRagdolling;

            if(GetBlenderData()->m_DoBBChecksOnWarp)
            {
                if(DoCollisionTest(position))
                {
                    canChangePosition = false;
                }
            }

            if(canChangePosition)
            {
				nmEntityDebugf(m_Ped, "CNetBlenderPed::SetPositionOnObject switching to animated");
                m_Ped->SwitchToAnimated();

                // if we are standing on an object we have to update the z to take into account
                // we have switched to animated
                if(standingOnObject)
                {
                    newPos.z += (1.0f - m_PredictionData.GetSnapshotPresent().m_StandingOnLocalOffset.z);
                }

				// We don't need to do a portal rescan if the ped has been fixed by network, as a rescan willbe done when the ped becomes unfixed 
				m_Ped->Teleport(newPos, GetLastHeadingReceived(), false, !m_Ped->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK));

                m_CorrectionVector = VEC3_ZERO;
            }
        }
        else
        {
            m_Ped->SetPosition(newPos);
        }
    }

	return true;
}

bool CNetBlenderPed::ShouldTestForMapCollision()
{
	bool shouldTest = false;

	if((GetNetworkObject() && GetNetworkObject()->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT)) || m_Ped->IsPlayer())
	{
		shouldTest = true;

		CNetObjPed *netObjPed = SafeCast(CNetObjPed, GetNetworkObject());

		if (netObjPed && netObjPed->IsHiddenUnderMapDueToVehicle())
		{
			shouldTest = false;
		}
	}

	return shouldTest;
}

// set the velocity of the network object we are blending
void CNetBlenderPed::SetVelocityOnObject(const Vector3 &velocity)
{
    if(m_Ped->GetCurrentPhysicsInst() && m_Ped->GetCurrentPhysicsInst()->IsInLevel() && gnetVerify(m_Ped->GetCurrentPhysicsInst()->GetArchetype()))
    {
        Vector3 newVelocity = velocity;

        // ensure we don't set a velocity too high for the type of object being blended
        float maxSpeed = m_Ped->GetCurrentPhysicsInst()->GetArchetype()->GetMaxSpeed();

        if(newVelocity.Mag2() > (maxSpeed * maxSpeed))
        {
            float scale = maxSpeed / newVelocity.Mag();
            newVelocity *= scale;
        }

        bool setVelocityDirectly = m_UseAnimatedRagdollBlending || UsingInAirBlenderData();

		if (DisableZBlending())
		{
			newVelocity.z = m_Ped->GetVelocity().z;
		}

        if(setVelocityDirectly)
        {
            m_Ped->SetVelocity(newVelocity);
        }
        else
        {
            gnetAssertf(newVelocity.Mag2() <= rage::square(DEFAULT_IMPULSE_LIMIT), "Setting a desired velocity too high on %s! New desired velocity: %.2f, %.2f, %.2f. Old desired velocity: %.2f, %.2f, %.2f. Correction vector: %.2f, %.2f, %.2f", 
				m_Ped->GetNetworkObject() ? m_Ped->GetNetworkObject()->GetLogName() : "Non-networked ped", 
				newVelocity.x, newVelocity.y, newVelocity.z,
				m_Ped->GetDesiredVelocity().x, m_Ped->GetDesiredVelocity().y, m_Ped->GetDesiredVelocity().z,
				m_CorrectionVector.x, m_CorrectionVector.y, m_CorrectionVector.z);

            m_Ped->SetDesiredVelocity(newVelocity);
        }
    }
}

void CNetBlenderPed::SetHeadingOnObject(float heading)
{
    // we can't set the position on an object if we are attached to something
    if(m_Ped->GetIsAttached())
    {
        gnetWarning("Network blender cannot set heading on %s as it is attached!", m_Ped->GetNetworkObject() ? m_Ped->GetNetworkObject()->GetLogName() : "Unknown");
        return;
    }

    // we can't set the position on an object if we are attached to something
    if(m_Ped->GetUsingRagdoll())
    {
        gnetWarning("Network blender cannot set heading on %s as it is ragdolling!", m_Ped->GetNetworkObject() ? m_Ped->GetNetworkObject()->GetLogName() : "Unknown");
        return;
    }

    m_Ped->SetHeading(heading);
}

bool CNetBlenderPed::GetIsOnScreen() const
{
    return m_Ped && m_Ped->GetIsVisibleInSomeViewportThisFrame();
}

netObject *CNetBlenderPed::GetNetworkObject()
{
    if(m_Ped)
    {
        return m_Ped->GetNetworkObject();
    }

    return 0;
}

const netObject *CNetBlenderPed::GetNetworkObject() const
{
    if(m_Ped)
    {
        return m_Ped->GetNetworkObject();
    }

    return 0;
}

void CNetBlenderPed::UpdateRagdollBlending()
{
    Vector3 objectPosition = GetPositionFromObject();
    Vector3 targetPosition = GetCurrentPredictedPosition();

#if __BANK
    m_LastPredictedPos     = targetPosition;
    m_LastSnapPos          = netBlenderLinInterp::m_PredictionData.GetSnapshotPresent().m_Position;
    m_LastSnapVel          = netBlenderLinInterp::m_PredictionData.GetSnapshotPresent().m_Velocity;
    m_LastSnapTime         = netBlenderLinInterp::m_PredictionData.GetSnapshotPresent().m_PositionTimestamp;
    m_LastTargetTime       = netInterface::GetTimestampForPositionUpdates() + fwTimer::GetTimeStepInMilliseconds();
    m_LastDisableZBlending = false;
#endif // __BANK

    Vector3 positionDelta  = targetPosition - objectPosition;

    float dist = positionDelta.Mag();

    if(dist >= GetPositionMaxForRagdollBlending())
    {
        if(m_RagdollAttached)
        {
            DetachRagdoll();
        }

        SetPositionOnObject(targetPosition, true);

        if(!m_Ped->GetUsingRagdoll())
        {
            SetVelocityOnObject(netBlenderLinInterp::m_PredictionData.GetSnapshotPresent().m_Velocity);
            SetHeadingOnObject(m_PredictionData.GetSnapshotPresent().m_heading);
        }
    }
    else if(!m_RagdollAttached)
    {
        float positionTimeRatio = GetBlendRatioForUpdateLevel();

        // ensure we always move a reasonable distance towards the target position
        static float minBlendDistance = 0.01f;
        if((dist * positionTimeRatio) < minBlendDistance)
        {
            positionTimeRatio = minBlendDistance / dist;
        }

        // clamp the position time ratio and move towards the target
        if(positionTimeRatio >= 1.0f)
        {
            positionTimeRatio = 1.0f;
        }

        Vector3 blendedPosition = objectPosition + positionDelta * positionTimeRatio;
        Vector3 vecDesiredVel   = (blendedPosition - objectPosition) / fwTimer::GetTimeStep();

        // limit how much the blender pulls peds backwards from the direction they are moving on the owner - this looks bad
        Vector3 normalisedDesiredVelocity   = vecDesiredVel;
        Vector3 normalisedPredictedVelocity = GetPredictedVelocity(GetBlenderData()->m_MaxSpeedToPredict);
        normalisedDesiredVelocity.Normalize();
        normalisedPredictedVelocity.Normalize();

        const float BLENDING_BACKWARDS_DOT_THRESHOLD = -0.707f; // 1/sqrt(2) -> or angle differences of 135 degrees in either direction

        if(normalisedDesiredVelocity.Dot(normalisedPredictedVelocity) < BLENDING_BACKWARDS_DOT_THRESHOLD)
        {
            const float MAX_BACKWARDS_BLEND_SPEED = 2.0f;

            if(vecDesiredVel.Mag2() > rage::square(MAX_BACKWARDS_BLEND_SPEED))
            {
                vecDesiredVel.Normalize();
                vecDesiredVel.Scale(MAX_BACKWARDS_BLEND_SPEED);
            }
        }

        phArticulatedCollider *collider = dynamic_cast<phArticulatedCollider*>(CPhysics::GetSimulator()->GetCollider(m_Ped->GetRagdollInst()));

        Vector3 vecCurrentVel = collider ? RCC_VECTOR3(collider->GetVelocity()) : VEC3_ZERO;

        // limiting this code to when running in-air blenders currently, but really this should be done for
        // all ragdoll blending
        if(UsingInAirBlenderData())
        {
            u32   timeToBlendOverInMs  = GetBlenderData()->m_UseBlendVelSmoothing ? GetBlenderData()->m_BlendVelSmoothTime : fwTimer::GetTimeStepInMilliseconds();
            float timeToBlendOver      = static_cast<float>(timeToBlendOverInMs) / 1000.0f;
            vecDesiredVel              = (targetPosition - objectPosition) / timeToBlendOver;
            Vector3 predictedVelocity  = GetPredictedVelocity(GetBlenderData()->m_MaxSpeedToPredict);
            Vector3 velDiffFromTarget  = (vecDesiredVel - predictedVelocity);

            // separate flat blending from z blending, otherwise all z corrections will focus on Z when falling very fast
            float   zDiffFromTarget = velDiffFromTarget.z;
            velDiffFromTarget.z     = 0.0f;

            if(velDiffFromTarget.Mag2() > rage::square(GetBlenderData()->m_HighSpeedMode.m_MaxVelDiffFromTarget))
            {
                velDiffFromTarget.Normalize();
                velDiffFromTarget.Scale(GetBlenderData()->m_HighSpeedMode.m_MaxVelDiffFromTarget);
                vecDesiredVel = predictedVelocity + velDiffFromTarget;
            }

            if(fabs(zDiffFromTarget) > GetBlenderData()->m_HighSpeedMode.m_MaxVelDiffFromTarget)
            {
                zDiffFromTarget /= fabs(zDiffFromTarget);
                zDiffFromTarget *= GetBlenderData()->m_HighSpeedMode.m_MaxVelDiffFromTarget;
            }

            vecDesiredVel.z = predictedVelocity.z + zDiffFromTarget;

            Vector3 velDiffFromCurrent = (vecDesiredVel - vecCurrentVel);

            // separate flat blending from z blending, otherwise all z corrections will focus on Z when falling very fast
            float   zDiffFromCurrent = velDiffFromCurrent.z;
            velDiffFromCurrent.z     = 0.0f;

            if(velDiffFromCurrent.Mag2() > rage::square(GetBlenderData()->m_HighSpeedMode.m_MaxVelChange))
            {
                velDiffFromCurrent.Normalize();
                velDiffFromCurrent.Scale(GetBlenderData()->m_HighSpeedMode.m_MaxVelChange);
                vecDesiredVel = vecCurrentVel + velDiffFromCurrent;
            }

            if(fabs(zDiffFromCurrent) > GetBlenderData()->m_HighSpeedMode.m_MaxVelChange)
            {
                zDiffFromCurrent /= fabs(zDiffFromCurrent);
                zDiffFromCurrent *= GetBlenderData()->m_HighSpeedMode.m_MaxVelChange;
            }

            vecDesiredVel.z = vecCurrentVel.z + zDiffFromCurrent;

            UpdateVelocity(predictedVelocity, GetLastSyncMessageTime());
        }

        Vector3 vecAcceleration = (vecDesiredVel - vecCurrentVel) / fwTimer::GetTimeStep();

        if(vecAcceleration.Mag2() > rage::square(75.0f))
        {
            vecAcceleration.Normalize();
            vecAcceleration.Scale(75.0f);
        }

		if (m_DisableRagdollZBlending || !m_IsRagdolling)
		{
			vecAcceleration.z = 0.0f;
		}

        if(collider)
        {
            gnetAssertf(vecAcceleration.x == vecAcceleration.x, "Acceleration is not finite!");
            collider->GetBody()->ApplyGravityToLinks(vecAcceleration, ScalarV(fwTimer::GetTimeStep() / CPhysics::GetNumTimeSlices()).GetIntrin128ConstRef(), collider->GetGravityFactor());
        }
    }

    if(m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID == NETWORK_INVALID_OBJECT_ID || !m_Ped->GetUsingRagdoll())
    {
        if(m_RagdollAttached)
        {
            DetachRagdoll();
        }

        m_TimeRagdollAttachDesired = 0;
    }
    else
    {
        // check if the ragdolling ped is still on the object on this machine
        bool       standingOnObject = false;
        netObject *networkObject    = NetworkInterface::GetNetworkObject(m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID);
        CEntity   *entity           = networkObject ? networkObject->GetEntity() : 0;

        if(entity && entity->GetIsTypeVehicle())
        {
            WorldProbe::CShapeTestProbeDesc probeDesc;

            const float PROBE_DIST = 5.0f;
            Vector3 startPos = objectPosition + Vector3(0.0f, 0.0f, PROBE_DIST);
            Vector3 endPos   = objectPosition - Vector3(0.0f, 0.0f, PROBE_DIST);

	        phInst *inst = entity->GetFragInst();

            if(!inst)
            {
                inst = entity->GetCurrentPhysicsInst();
            }

            probeDesc.SetStartAndEnd(startPos, endPos);
	        probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
            probeDesc.SetIncludeInstance(inst);
            probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE);
            probeDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
            probeDesc.SetContext(WorldProbe::ENetwork);

	        if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
            {
				standingOnObject = true;
            }

			if(!m_RagdollAttached && !m_Ped->GetIsDeadOrDying())
			{
				if(fabs(m_PredictionData.GetSnapshotPresent().m_StandingOnLocalOffset.z) < GetBlenderData()->m_RagdollAttachZOffset || !standingOnObject)
				{
					// if we are standing on another network object - see if we are close enough to attach to it
					if(positionDelta.XYMag() <= GetBlenderData()->m_RagdollAttachDistance && fabs(positionDelta.z) <= GetBlenderData()->m_RagdollAttachDistance)
					{
						AttachRagdoll();
		
						// need to clear this here so that once we detach the timer will be reset
						m_TimeRagdollAttachDesired = 0;
					}
					else
					{
						if(m_TimeRagdollAttachDesired == 0)
						{
							m_TimeRagdollAttachDesired = fwTimer::GetSystemTimeInMilliseconds();
						}
						else
						{
							u32 timeElapsedSinceAttachmentDesired = fwTimer::GetSystemTimeInMilliseconds() - m_TimeRagdollAttachDesired;

							if(timeElapsedSinceAttachmentDesired > GetBlenderData()->m_RagdollTimeBeforeAttach)
							{
								// warp the ragdoll via it's current clone task
								CTaskNMControl *nmControlTask = SafeCast(CTaskNMControl, m_Ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_CONTROL));

								if(nmControlTask)
								{
									Vector3 warpPosition = targetPosition;

									nmControlTask->WarpCloneRagdollingPed(m_Ped, warpPosition);

									AttachRagdoll();

									m_TimeRagdollAttachDesired = 0;
								}
							}
						}
					}
				}
			}
		}
	}

    // reset the linear interpolation variables so we will do a normal blend when the ped stops ragdolling
    m_BlendLinInterpStartTime = NetworkInterface::GetNetworkTime();
    m_BlendingPosition      = true;
}

void CNetBlenderPed::UpdateAnimatedRagdollFallbackBlending()
{
	Vector3 predictedVelocity = GetPredictedVelocity(GetBlenderData()->m_MaxSpeedToPredict);
	UpdateVelocity(predictedVelocity, GetLastSyncMessageTime());

    CPedBlenderData *oldBlenderData = GetBlenderData();
    SetBlenderData(&s_pedBlenderDataAnimatedRagdollFallback);

    netBlenderLinInterp::Update();

    SetBlenderData(oldBlenderData);
}

float CNetBlenderPed::GetBlendRatioForUpdateLevel() const
{
    switch(GetEstimatedUpdateLevel())
    {
    case LOW_UPDATE_LEVEL:
        return GetBlenderData()->m_RagdollPosBlendLow;
    case MEDIUM_UPDATE_LEVEL:
        return GetBlenderData()->m_RagdollPosBlendMedium;
    case HIGH_UPDATE_LEVEL:
        return GetBlenderData()->m_RagdollPosBlendHigh;
    default:
        return GetBlenderData()->m_RagdollPosBlendHigh;
    }
}

float CNetBlenderPed::GetPositionMaxForRagdollBlending() const
{
    if(m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID == NETWORK_INVALID_OBJECT_ID || m_CollisionTimer > 0)
    {
        return GetBlenderData()->m_RagdollPositionMax;
    }
    else
    {
        return GetBlenderData()->m_RagdollOnObjPosDeltaMax;
    }
}

const Vector3 CNetBlenderPed::GetCurrentPredictedPosition() const
{
	Vector3 targetPosition = netBlenderLinInterp::m_PredictionData.GetSnapshotPresent().m_Position;

	if(m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID != NETWORK_INVALID_OBJECT_ID)
	{
		netObject *networkObject = NetworkInterface::GetNetworkObject(m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID);
		CEntity   *entity        = networkObject ? networkObject->GetEntity() : 0;

		if(entity)
		{
			if (networkObject->GetObjectType() == NET_OBJ_TYPE_BOAT)
			{
				if (networkObject->GetNetBlender() && 
					SafeCast(netBlenderLinInterp, networkObject->GetNetBlender())->DisableZBlending())
				{
					return GetCurrentPredictedPositionOnBoat();
				}
			}

			Vector3 localOffset = VEC3V_TO_VECTOR3(entity->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_PredictionData.GetSnapshotPresent().m_StandingOnLocalOffset)));

			const Vector3 &stoodOnObjectPos = VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition());

			targetPosition.x = stoodOnObjectPos.x + localOffset.x;
			targetPosition.y = stoodOnObjectPos.y + localOffset.y;

			// do a probe to find the z position
			const unsigned NUM_INTERSECTIONS = 3;

			WorldProbe::CShapeTestHitPoint probeIsects[NUM_INTERSECTIONS];
			WorldProbe::CShapeTestResults probeResult(probeIsects, NUM_INTERSECTIONS);
			WorldProbe::CShapeTestProbeDesc probeDesc;

			const float PROBE_DIST = 2.0f;
			Vector3 startPos = targetPosition + Vector3(0.0f, 0.0f, PROBE_DIST);
			Vector3 endPos   = targetPosition - Vector3(0.0f, 0.0f, PROBE_DIST);

			phInst *inst = entity->GetFragInst();

			if(!inst)
			{
				inst = entity->GetCurrentPhysicsInst();
			}

			probeDesc.SetStartAndEnd(startPos, endPos);
			probeDesc.SetResultsStructure(&probeResult);
			probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
			probeDesc.SetIncludeInstance(inst);
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE);
			probeDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
			probeDesc.SetContext(WorldProbe::ENetwork);

			bool hitDetected = false;

			if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
			{
				if(probeResult.GetResultsReady() && probeResult[0].GetHitDetected())
				{
					float probeZ   = probeResult[0].GetHitPosition().z;
					float bestDiff = fabs(probeZ - targetPosition.z);

					if(probeResult.GetNumHits() > 1)
					{
						for(unsigned index = 1; index < probeResult.GetNumHits(); index++)
						{
							float newZ    = probeResult[index].GetHitPosition().z;
							float newDiff = fabs(newZ - targetPosition.z);

							if(newDiff < bestDiff)
							{
								bestDiff  = newDiff;
								probeZ = newZ;
							}
						}
					}

#if DEBUG_DRAW
					if(GetBlenderData()->m_DisplayProbeHits)
					{
						const float DEBUG_SPHERE_RADIUS = 0.2f;

						for(unsigned index = 0; index < probeResult.GetNumHits(); index++)
						{
							bool isBest = IsClose(probeResult[index].GetHitPosition().z, probeZ);

							grcDebugDraw::Sphere(probeResult[index].GetHitPosition(), DEBUG_SPHERE_RADIUS, isBest ? Color_green : Color_blue, false);
						}
					}
#endif // DEBUG_DRAW

					// we don't use the probe Z position if it is too far away from the world Z
					// position received. This catches some edge cases when peds are moving onto
					// the lower levels of multi-leveled objects
					static const float PROBE_Z_THRESHOLD = 0.5f;

					if(bestDiff < PROBE_Z_THRESHOLD)
					{
						targetPosition.z = probeZ;

						// if the ragdoll state is the same on the owner as locally we can use the z offset
						// received from the network, otherwise we just add the ped capsule offset to the detected
						// intersection. This will target slightly higher if the local is ragdolling and the remote is not,
						// but this shouldn't happen often or long enough to be a noticeable issue
						if(m_Ped->GetUsingRagdoll() && m_Ped->GetUsingRagdoll() == m_IsRagdolling)
						{
							// use the untransformed offset for the z as this is how it is sent
							targetPosition.z += m_PredictionData.GetSnapshotPresent().m_StandingOnLocalOffset.z;
						}
						else
						{
							targetPosition.z += 1.0f;
						}

						hitDetected = true;
					}
				}
			}

			if(!hitDetected)
			{
				targetPosition.z = netBlenderLinInterp::m_PredictionData.GetSnapshotPresent().m_Position.z;
			}
			else
			{
				// account for the object we are standing upon velocity
				CPhysical *physical = SafeCast(CPhysical, entity);
				Vector3 predictedMovement = physical->GetVelocity() * fwTimer::GetTimeStep();
				targetPosition += predictedMovement;
			}
		}
	}
	else if(m_Ped && !m_Ped->GetUsingRagdoll())
	{
		u32     timestamp       = 0;
		Vector3 lastPosReceived = GetLastPositionReceived(&timestamp);

		u32 currentTime = NetworkInterface::GetNetworkTime();

		if(currentTime < timestamp)
		{
			currentTime = timestamp;
		}

		float timeElapsed = ((currentTime - timestamp) / 1000.0f) * fwTimer::GetTimeWarpActive();

		if(timeElapsed > GetBlenderData()->m_MaxPredictTime)
		{
			timeElapsed = GetBlenderData()->m_MaxPredictTime;
		}

		Vector3 currentVelocity = GetPredictedVelocity(GetBlenderData()->m_MaxSpeedToPredict);
		targetPosition          = lastPosReceived + (currentVelocity * timeElapsed);
	}

	return targetPosition;
}

const Vector3 CNetBlenderPed::GetCurrentPredictedPositionOnBoat() const
{
    Vector3 targetPosition = netBlenderLinInterp::m_PredictionData.GetSnapshotPresent().m_Position;

    if(m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID != NETWORK_INVALID_OBJECT_ID)
    {
        netObject *networkObject = NetworkInterface::GetNetworkObject(m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID);
        CEntity   *entity        = networkObject ? networkObject->GetEntity() : 0;

        if(entity)
        {
            Vector3 localOffset = VEC3V_TO_VECTOR3(entity->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_PredictionData.GetSnapshotPresent().m_StandingOnLocalOffset)));

            const Vector3 &stoodOnObjectPos = VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition());
			targetPosition = stoodOnObjectPos + localOffset;

            // account for the object we are standing upon velocity
            CPhysical *physical = SafeCast(CPhysical, entity);
            Vector3 predictedMovement = physical->GetVelocity() * fwTimer::GetTimeStep();
            targetPosition += predictedMovement;

#if DEBUG_DRAW
					if(GetBlenderData()->m_DisplayProbeHits)
					{
				        const float DEBUG_SPHERE_RADIUS = 0.25f;
				        grcDebugDraw::Sphere(targetPosition, DEBUG_SPHERE_RADIUS, Color_red, false);
					}
#endif // DEBUG_DRAW
        }
    }

    return targetPosition;
}

const Vector3 CNetBlenderPed::GetPredictedVelocity(float maxSpeedToPredict) const 
{
    if(UsingInAirBlenderData() || m_UseAnimatedRagdollBlending)
    {
        // see if the ped is running any clone task that has a synced velocity value to use
        CTask *task = m_Ped->GetPedIntelligence()->GetTaskActive();

        while(task)
        {
            if(task->IsClonedFSMTask())
            {
                CTaskFSMClone *clonedFSMTask = static_cast<CTaskFSMClone *>(task);

                if(clonedFSMTask->HasSyncedVelocity())
                {
                    return clonedFSMTask->GetSyncedVelocity();
                }
            }

            task = task->GetSubTask();
        }
    }

    CNetObjPed *netObjPed = SafeCast(CNetObjPed, m_Ped->GetNetworkObject());

    if(netObjPed && netObjPed->HasPedStopped() && !netObjPed->IsHiddenUnderMapDueToVehicle())
    {
        return VEC3_ZERO;
    }
    else
    {
        netObject *networkObject = 0;

        if(m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID    != NETWORK_INVALID_OBJECT_ID && 
           m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID    == m_PredictionData.GetSnapshotPast().m_StandingOnObjectID &&
           m_PredictionData.GetSnapshotPresent().m_StandingDataTimestamp >  m_PredictionData.GetSnapshotPast().m_StandingDataTimestamp)
        {
            networkObject = NetworkInterface::GetNetworkObject(m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID);
        }

        CPhysical *physical = (networkObject && networkObject->GetEntity() && networkObject->GetEntity()->GetIsPhysical()) ? static_cast<CPhysical *>(networkObject->GetEntity()) : 0;

        if(physical)
        {
            Vector3 predictedVelocity = VEC3_ZERO;

            // calculate the local velocity of the ped (relative to the object stood upon)
            Vector3 pastOffset          = VEC3V_TO_VECTOR3(physical->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_PredictionData.GetSnapshotPast().m_StandingOnLocalOffset)));
            Vector3 presentOffset       = VEC3V_TO_VECTOR3(physical->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_PredictionData.GetSnapshotPresent().m_StandingOnLocalOffset)));
            u32     presentSnapshotTime = m_PredictionData.GetSnapshotPresent().m_StandingDataTimestamp;
            u32     pastSnapshotTime    = m_PredictionData.GetSnapshotPast().m_StandingDataTimestamp;

            Vector3 positionDiff = (presentOffset - pastOffset);
            u32     timeDiff     = presentSnapshotTime - pastSnapshotTime;

            float timeElapsed = (timeDiff / 1000.0f) * fwTimer::GetTimeWarpActive();

            const float MAX_TIME_DIFF = 1.5f;

            if(timeElapsed < MAX_TIME_DIFF)
            {
                predictedVelocity = positionDiff / timeElapsed;
            }

            if(predictedVelocity.Mag2() > rage::square(maxSpeedToPredict))
            {
                predictedVelocity = VEC3_ZERO;
            }

            // add on the speed of the object stood upon
            predictedVelocity += physical->GetVelocity();

            return predictedVelocity;
        }
        else
        {
            return netBlenderLinInterp::GetPredictedVelocity(maxSpeedToPredict);
        }
    }
}

float CNetBlenderPed::GetCurrentPredictedHeading() 
{
	return m_PredictionData.GetSnapshotFuture().m_heading;
}

bool CNetBlenderPed::IsStandingOnObject(ObjectId &objectStoodOn, Vector3 &objectStoodOnOffset)
{
    objectStoodOn        = m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID;
    objectStoodOnOffset  = m_PredictionData.GetSnapshotPresent().m_StandingOnLocalOffset;

    return objectStoodOn != NETWORK_INVALID_OBJECT_ID;
}

void CNetBlenderPed::RegisterCollision(netObject *collisionObject, const float impulseMag)
{
    if(impulseMag > GetBlenderData()->m_CollisionModeThreshold)
    {
        if(collisionObject && collisionObject->GetEntity() && collisionObject->GetEntity()->GetIsTypeVehicle())
        {
            m_CollisionTimer = GetBlenderData()->m_CollisionModeDuration;
        }
    }
}

const Vector3 CNetBlenderPed::GetPredictedNextFramePosition(const Vector3 &UNUSED_PARAM(predictedCurrentPosition),
                                                            const Vector3 &UNUSED_PARAM(targetVelocity)) const
{
    Vector3 predictedCurrentPosition   = GetCurrentPredictedPosition();
    Vector3 currentVelocity            = GetPredictedVelocity(GetBlenderData()->m_MaxSpeedToPredict);
    Vector3 predictedNextFramePosition = predictedCurrentPosition + (currentVelocity * fwTimer::GetTimeStep());

    if(m_Ped && m_Ped->GetIsInWater())
    {
        CNetObjPed *netObjPed = SafeCast(CNetObjPed, m_Ped->GetNetworkObject());
        bool        isDiving  = netObjPed && netObjPed->GetIsDiving();

		float waterZ;

		Water::GetWaterLevelNoWaves(predictedNextFramePosition, &waterZ, POOL_DEPTH, REJECTIONABOVEWATER, NULL);

        if(!isDiving || predictedNextFramePosition.z > waterZ)
        {
            predictedNextFramePosition.z = m_Ped->GetTransform().GetPosition().GetZf();
        }
    }

    return predictedNextFramePosition;
}

const Vector3 CNetBlenderPed::GetPredictedFuturePosition(u32 timeInFutureMs) const
{
    if (m_StandingOnObject)
    {
        netObject *networkObject = NetworkInterface::GetNetworkObject(m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID);
        CPhysical *physical      = networkObject ? static_cast<CPhysical *>(networkObject->GetEntity()) : 0;

        if(physical)
        {
            float   timeInFuture            = (timeInFutureMs / 1000.0f) * fwTimer::GetTimeWarpActive();
            Vector3 predictedFuturePosition = VEC3V_TO_VECTOR3(physical->GetTransform().GetPosition()) + (physical->GetVelocity() * timeInFuture);
            Vector3 localOffset             = VEC3V_TO_VECTOR3(physical->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_PredictionData.GetSnapshotPresent().m_StandingOnLocalOffset)));

            predictedFuturePosition += localOffset;

		    return predictedFuturePosition;
        }
    }

    Vector3 predictedFuturePosition = netBlenderLinInterp::GetPredictedFuturePosition(timeInFutureMs);

    if(m_Ped && m_Ped->GetIsInWater())
    {
        CNetObjPed *netObjPed = SafeCast(CNetObjPed, m_Ped->GetNetworkObject());
        bool        isDiving  = netObjPed && netObjPed->GetIsDiving();

        if(!isDiving)
        {
            predictedFuturePosition.z = m_Ped->GetTransform().GetPosition().GetZf();
        }
    }

    return predictedFuturePosition;
}

void CNetBlenderPed::UpdateCollisionTimer()
{
     // decrement the collision timer if necessary
    if(m_CollisionTimer > 0)
    {
        if(fwTimer::GetTimeStepInMilliseconds() < m_CollisionTimer)
        {
            m_CollisionTimer -= fwTimer::GetTimeStepInMilliseconds();
        }
        else
        {
            m_CollisionTimer = 0;
        }
    }
}

bool CNetBlenderPed::DoCollisionTest(const Vector3 &position)
{
    if(m_Ped && m_Ped->GetCapsuleInfo())
    {
		const CBipedCapsuleInfo *bipedCapsuleInfo = m_Ped->GetCapsuleInfo()->GetBipedCapsuleInfo();	
		const CQuadrupedCapsuleInfo *quadCapsuleInfo = m_Ped->GetCapsuleInfo()->GetQuadrupedCapsuleInfo();	

        if(bipedCapsuleInfo || quadCapsuleInfo)
        {
            int     nTestTypeFlags = ArchetypeFlags::GTA_BOX_VEHICLE_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE;
            Vector3 startPos       = position + Vector3(0.0f, 0.0f, quadCapsuleInfo ? 0.0f : bipedCapsuleInfo->GetKneeHeight());
            Vector3 endPos         = position + Vector3(0.0f, 0.0f, quadCapsuleInfo ? quadCapsuleInfo->GetGroundToRootOffset() : bipedCapsuleInfo->GetHeadHeight());
           
			float radius = quadCapsuleInfo ? quadCapsuleInfo->GetMainBoundRadius() : bipedCapsuleInfo->GetRadius();
            WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
            capsuleDesc.SetCapsule(startPos, endPos, radius);
            capsuleDesc.SetIncludeFlags(nTestTypeFlags);
            capsuleDesc.SetExcludeEntity(m_Ped);
            capsuleDesc.SetContext(WorldProbe::ENetwork);

            if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
            {
                return true;
            }
        }
    }

    return false;
}

void CNetBlenderPed::CheckForStuckOnVehicle()
{
    CVehicle *closestVehicle = m_Ped->GetPedIntelligence()->GetVehicleScanner()->GetClosestVehicleInRange();

    if(closestVehicle)
    {
        Vector3  currentPos        = VEC3V_TO_VECTOR3(m_Ped->GetTransform().GetPosition());
        Vector3  predictedPosition = GetCurrentPredictedPosition();
        Matrix34 vehicleMatrix     = MAT34V_TO_MATRIX34(closestVehicle->GetTransform().GetMatrix());

#if __BANK
        if(ms_UseLocalPlayerForCarPushTarget)
        {
            predictedPosition = VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
        }
#endif // __BANK

        Vector3  unTransformedCurrentPos;
        Vector3  unTransformedPredictedPos;
        vehicleMatrix.UnTransform(currentPos,        unTransformedCurrentPos);
        vehicleMatrix.UnTransform(predictedPosition, unTransformedPredictedPos);

        // calculate which side of the box the ped currently is
        spdAABB tempBox;
        const spdAABB &boundBox = closestVehicle->GetLocalSpaceBoundBox(tempBox);

        Vector3 min = boundBox.GetMinVector3();
        Vector3 max = boundBox.GetMaxVector3();

        min.x -= ms_PushRoundCarsBoxExpandSize;
        max.x += ms_PushRoundCarsBoxExpandSize;
        min.y -= ms_PushRoundCarsBoxExpandSize;
        max.y += ms_PushRoundCarsBoxExpandSize;

        spdAABB expandedBoundBox(VECTOR3_TO_VEC3V(min), VECTOR3_TO_VEC3V(max));

#if __BANK
        if(ms_DisplayPushRoundCarsDebug)
        {
            grcDebugDraw::BoxOriented(expandedBoundBox.GetMin(), expandedBoundBox.GetMax(), closestVehicle->GetMatrix(), Color32(1.0f, 0.0f, 0.0f, 1.0f), false);
        }
#endif // __BANK

        if(expandedBoundBox.ContainsPoint(VECTOR3_TO_VEC3V(unTransformedCurrentPos)))
        {
#if __BANK
            if(ms_DisplayPushRoundCarsDebug)
            {
                spdAABB aabbVehicleLcl;
                aabbVehicleLcl = closestVehicle->GetLocalSpaceBoundBox(aabbVehicleLcl);
                grcDebugDraw::BoxOriented(aabbVehicleLcl.GetMin(), aabbVehicleLcl.GetMax(), closestVehicle->GetMatrix(), Color32(0.0f, 1.0f, 0.0f, 1.0f), false);
            }
#endif // __BANK

            min = boundBox.GetMinVector3();
            max = boundBox.GetMaxVector3();

            Vector3 unTransformedPushVector(VEC3_ZERO);

            if(unTransformedCurrentPos.x >= min.x && unTransformedCurrentPos.x <= max.x)
            {
                // current position is on the top or bottom of the box
                if(unTransformedPredictedPos.y >= min.y && unTransformedPredictedPos.y <= max.y)
                {
                    // predicted position is on the left or right of the box
                    if(unTransformedPredictedPos.x < min.x)
                    {
                        unTransformedPushVector = Vector3(-1.0f, 0.0f, 0.0f);
                    }
                    else if(unTransformedPredictedPos.x > max.x)
                    {
                        unTransformedPushVector = Vector3(1.0f, 0.0f, 0.0f);
                    }
                }
            }
            else if(unTransformedCurrentPos.y >= min.y && unTransformedCurrentPos.y <= max.y)
            {
                // current position is on the left or right of the box
                if(unTransformedPredictedPos.x >= min.x && unTransformedPredictedPos.x <= max.x)
                {
                    // predicted position is on the top or bottom of the box
                    if(unTransformedPredictedPos.y < min.y)
                    {
                        unTransformedPushVector = Vector3(0.0f, -1.0f, 0.0f);
                    }
                    else if(unTransformedPredictedPos.y > max.y)
                    {
                        unTransformedPushVector = Vector3(0.0f, 1.0f, 0.0f);
                    }
                }
            }

            if(unTransformedPushVector.Mag2() > 0.1f)
            {
                Vector3 pushVector;
                vehicleMatrix.Transform3x3(unTransformedPushVector, pushVector);

                Vector3 posDelta = predictedPosition - currentPos;
                float   pushMag  = posDelta.Dot(pushVector);

                if(fabs(pushMag) > 0.5f)
                {
#if __BANK
                    if(ms_DisplayPushRoundCarsDebug)
                    {
                        Vector3 predictedPosRelativeToPush = currentPos + (pushVector * pushMag);
                        grcDebugDraw::Line(currentPos, predictedPosRelativeToPush, Color32(0.0f, 0.0f, 1.0f, 1.0f));
                    }
#endif // __BANK

                    Vector3 pushForce = pushVector * ms_PushRoundCarsForce;

                    if(pushForce.z < 0.0f)
                    {
                        pushForce.z = 0.0f;
                    }

                    m_Ped->ApplyForceCg(pushForce);
                }
            }
        }
    }
}

void CNetBlenderPed::AttachRagdoll()
{
    if(m_Ped && gnetVerifyf(!m_RagdollAttached, "Trying to attach a ragdoll for a ped already attached by the blender!"))
    {
        netObject *networkObject = NetworkInterface::GetNetworkObject(m_PredictionData.GetSnapshotPresent().m_StandingOnObjectID);
        CEntity   *entity        = networkObject ? networkObject->GetEntity() : 0;

        if(entity && entity->GetIsPhysical())
        {
            float fPhysicalStrength = 0.0f; // infinite strength

            u32 nPhysicalAttachFlags = 0;
		    nPhysicalAttachFlags |= ATTACH_STATE_RAGDOLL;
		    nPhysicalAttachFlags |= ATTACH_FLAG_POS_CONSTRAINT;
		    nPhysicalAttachFlags |= ATTACH_FLAG_DO_PAIRED_COLL;
		    nPhysicalAttachFlags |= ATTACH_FLAG_AUTODETACH_ON_DEATH;
		    nPhysicalAttachFlags |= ATTACH_FLAG_COL_ON;

		    m_Ped->AttachToPhysicalUsingPhysics(static_cast<CPhysical *>(entity),
                                                -1,
                                                -1,
			                                    nPhysicalAttachFlags,
                                                GetCurrentPredictedPosition(),
                                                fPhysicalStrength);

		    gnetAssertf(m_Ped->GetAttachState()==ATTACH_STATE_RAGDOLL, "Failed to attach ragdolling ped!");

            m_RagdollAttached = true;
        }
    }
}

void CNetBlenderPed::OnPredictionPop() const
{
#if __BANK
    NetworkDebug::AddPredictionPop(m_Ped->GetNetworkObject()->GetObjectID(), m_Ped->GetNetworkObject()->GetObjectType(), false);
#endif // __BANK
}

void CNetBlenderPed::DetachRagdoll()
{
    if(m_Ped && gnetVerifyf(m_RagdollAttached, "Trying to detach a ragdoll for a ped not attached by the blender!"))
    {
        u16 nDetachFlags = 0;
	    nDetachFlags |= DETACH_FLAG_ACTIVATE_PHYSICS;
	    nDetachFlags |= DETACH_FLAG_APPLY_VELOCITY;
	    nDetachFlags |= DETACH_FLAG_IGNORE_SAFE_POSITION_CHECK;

	    if(m_Ped->GetIsAttached())
	    {
		    m_Ped->DetachFromParent(nDetachFlags);
	    }
    }
}

void CNetBlenderPed::OnPedDetached()
{
    if(m_RagdollAttached)
    {
        m_RagdollAttached = false;
    }
}

bool CNetBlenderPed::TestForMapCollision(const Vector3 &lastReceivedPos, const Vector3 &newPos)
{
    WorldProbe::CShapeTestProbeDesc probeDesc;
    WorldProbe::CShapeTestFixedResults<1> probeTestResults;
	probeDesc.SetResultsStructure(&probeTestResults);
    probeDesc.SetStartAndEnd(lastReceivedPos, newPos);
    probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER);
    probeDesc.SetContext(WorldProbe::ENetwork);

    gnetDebug2("TestForMapCollision - Performing probe for %s", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "Non-networked ped");

    if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
    {
        gnetDebug2("TestForMapCollision -  Cannot pop due to predicting through the map (%.2f,%.2f,%.2f)->(%.2f,%.2f,%.2f)", lastReceivedPos.x, lastReceivedPos.y, lastReceivedPos.z, newPos.x, newPos.y, newPos.z);
        return true;
    }

    return false;
}

bool CNetBlenderPed::TestForGroundZ(const Vector3 &position, float &groundZ)
{
    Vector3 probeStart = position + Vector3(0.0f, 0.0f, 1.0f);
    Vector3 probeEnd   = position - Vector3(0.0f, 0.0f, 1.0f);

    WorldProbe::CShapeTestProbeDesc probeDesc;
    WorldProbe::CShapeTestFixedResults<1> probeTestResults;
    probeDesc.SetResultsStructure(&probeTestResults);
    probeDesc.SetStartAndEnd(probeStart, probeEnd);
    probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER);
    probeDesc.SetContext(WorldProbe::ENetwork);

    gnetDebug2("TestForGroundZ - Performing test for ground Z for %s", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "Non-networked ped");

    if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
    {
        if(probeTestResults.GetResultsReady() && probeTestResults[0].GetHitDetected())
        {
            gnetDebug2("TestForGroundZ - Found ground Z at (%.2f,%.2f,%.2f)", probeTestResults[0].GetHitPosition().x, probeTestResults[0].GetHitPosition().y, probeTestResults[0].GetHitPosition().z);

            groundZ = probeTestResults[0].GetHitPosition().z;

            return true;
        }
    }

    return false;
}

bool CNetBlenderPed::ShouldLerpOnObject(netObject *networkObject)
{
    CVehicle *vehicle = NetworkUtils::GetVehicleFromNetworkObject(networkObject);

    if(vehicle && vehicle->InheritsFromBoat() && vehicle->GetVelocity().Mag2() > rage::square(GetBlenderData()->m_LerpOnObjectThreshold))
    {
        return true;
    }

    return false;
}

void CNetBlenderPed::CheckForOnStairsFixup()
{
    if(m_Ped && (m_Ped->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected) || m_IsOnStairs))
    {
        const float groundToRootOffset       = m_Ped->GetCapsuleInfo() ? m_Ped->GetCapsuleInfo()->GetGroundToRootOffset() : 1.0f;
        Vector3     objectPosition           = GetPositionFromObject();
        Vector3     predictedCurrentPosition = GetCurrentPredictedPosition();
        bool        applyStairCorrection     = false;

        if(m_Ped->GetGroundPos().z > PED_GROUNDPOS_RESET_Z)
        {
            predictedCurrentPosition.z = m_Ped->GetGroundPos().z + groundToRootOffset;
            applyStairCorrection = true;
        }
        else
        {
            const float PROBE_HEIGHT = 5.0f;

            Vector3 probeStart = objectPosition;
            Vector3 probeEnd   = objectPosition - Vector3(0.0f, 0.0f, PROBE_HEIGHT);

            WorldProbe::CShapeTestProbeDesc probeDesc;
            WorldProbe::CShapeTestFixedResults<1> probeTestResults;
	        probeDesc.SetResultsStructure(&probeTestResults);
            probeDesc.SetStartAndEnd(probeStart, probeEnd);
            probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER);
            probeDesc.SetContext(WorldProbe::ENetwork);

            gnetDebug2("Stair fixup - Performing probe for %s", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "Non-networked ped");

            if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
            {
                if(probeTestResults.GetResultsReady() && probeTestResults[0].GetHitDetected())
                {
                    predictedCurrentPosition.z = probeTestResults[0].GetHitPosition().z + groundToRootOffset;
                    applyStairCorrection       = true;
                }
            }
        }

        if(applyStairCorrection)
        {
            const float ON_STAIRS_VEL_CLAMP = 100.0f;

            Vector3 delta = predictedCurrentPosition - objectPosition;

            Vector3 newDesiredVelocity = m_Ped->GetDesiredVelocity();
            newDesiredVelocity.z       = delta.z / fwTimer::GetTimeStep();
            newDesiredVelocity.z       = Clamp(newDesiredVelocity.z, -ON_STAIRS_VEL_CLAMP, ON_STAIRS_VEL_CLAMP);
            m_Ped->SetDesiredVelocity(newDesiredVelocity);
        }
    }
}

static bool CheckForIntersectingObjectsCB(CEntity* pEntity, void* data)
{
    bool *foundObject = reinterpret_cast<bool *>(data);

	if (pEntity && pEntity->GetIsTypeObject())
	{
		CObject* pObject = static_cast<CObject*>(pEntity);

		// ignore weapons, pickups and anything attached to peds
		if (!pObject->GetWeapon() && !pObject->m_nObjectFlags.bIsPickUp && !(pObject->GetAttachParent() && ((CEntity*)pObject->GetAttachParent())->GetIsTypePed()))
		{
			*foundObject = true;
		}
	}

	return !(*foundObject);
}

void CNetBlenderPed::CheckForIntersectingObjects()
{
    // only do this for players at the moment
    if(m_Ped && m_Ped->IsPlayer() && !m_Ped->GetIsInWater())
    {
        // check if the ped is stationary both locally and remotely
        if(m_Ped->GetVelocity().Mag2() < 0.01f && netBlenderLinInterp::GetPredictedVelocity(GetBlenderData()->m_MaxSpeedToPredict).Mag2() < 0.01f)
        {
            Vector3     objectPosition           = GetPositionFromObject();
            Vector3     predictedCurrentPosition = GetCurrentPredictedPosition();

            static const float INTERSECTING_OBJECT_THRESHOLD = 1.0f;

            // note we only care when the ped is lower than the target position - hence no fabs
            if((predictedCurrentPosition.z - objectPosition.z) > INTERSECTING_OBJECT_THRESHOLD)
            {
                // only do this when we are close enough to the ped
                Vector3 delta = predictedCurrentPosition - objectPosition;

                static const float CHECK_FOR_INTERSECT_DIST_SQR = rage::square(100.0f);

                if(delta.XYMag2() < CHECK_FOR_INTERSECT_DIST_SQR)
                {
                    static const float TEST_SPHERE_RADIUS = 2.0f;

                    bool objectFound = false;

		            fwIsSphereIntersecting testSphere(spdSphere(RCC_VEC3V(objectPosition), ScalarV(TEST_SPHERE_RADIUS)));
		            CGameWorld::ForAllEntitiesIntersecting(&testSphere, CheckForIntersectingObjectsCB, (void*) &objectFound,
			            ENTITY_TYPE_MASK_OBJECT, SEARCH_LOCATION_EXTERIORS, SEARCH_LODTYPE_HIGHDETAIL, SEARCH_OPTION_FORCE_PPU_CODEPATH | SEARCH_OPTION_DYNAMICS_ONLY);

                    if(objectFound)
                    {
                        GoStraightToTarget();
                    }
                }
            }
        }
    }
}
