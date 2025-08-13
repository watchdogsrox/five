//
// name:        NetBlenderPed.h
// written by:  Daniel Yelland
// description: Network blender used by peds

#ifndef NETBLENDERPED_H
#define NETBLENDERPED_H

// game includes
#include "fwmaths/angle.h"
#include "fwnet/netblenderlininterp.h"
#include "fwdebug/debugdraw.h"
// ===========================================================================================================================
// CPedPredictionData
// ===========================================================================================================================
class CPedPredictionData
{
    struct netSnapshot;

public:

    CPedPredictionData(const float heading, ObjectId standingOnObjectID, const Vector3 &standingOnLocalOffset, const u32 time);
    ~CPedPredictionData();

    void Reset(const float heading, ObjectId standingOnObjectID, const Vector3 &standingOnLocalOffset, const u32 time);

    netSnapshot &GetSnapshotPast()    { return m_snapshotPast;    }
    netSnapshot &GetSnapshotPresent() { return m_snapshotPresent; }
    netSnapshot &GetSnapshotFuture()  { return m_snapshotFuture;  }

    const netSnapshot &GetSnapshotPast()    const { return m_snapshotPast;    }
    const netSnapshot &GetSnapshotPresent() const { return m_snapshotPresent; }
    const netSnapshot &GetSnapshotFuture()  const { return m_snapshotFuture;  }

private:

    CPedPredictionData();

    // snapshot of data to predict from
    struct netSnapshot
    {
        float    m_heading;
        u32      m_headingTimestamp;
        ObjectId m_StandingOnObjectID;
        Vector3  m_StandingOnLocalOffset;
        u32      m_StandingDataTimestamp;
    };

    netSnapshot m_snapshotPast;    // first snapshot (past)
    netSnapshot m_snapshotPresent; // second snapshot (present)
    netSnapshot m_snapshotFuture;  // third snapshot (future)
};

// ===========================================================================================================================
// CPedBlenderData
// ===========================================================================================================================

struct CPedBlenderData : public CLinInterpBlenderData
{
    CPedBlenderData()
    {
        m_BlendingOn                          = true;   // blending on
        m_BlendHeading                        = true;   // blend heading
        m_DoBBChecksOnWarp                    = true;   // do bounding box checks with other entities prior to moving the target object large distances
        m_AllowAdjustVelChangeFromUpdates     = false;  // indicates we are allowing the per frame velocity change to be adjusted to match the changes coming over the network
        m_UseBlendVelSmoothing                = false;  // indicates whether the blender will smooth velocity changes over a time interval, or just work frame to frame
        m_BlendVelSmoothTime                  = 150;    // the time in ms to base blender behaviour over (looks at where the object will be this time in the future)
        m_PositionDeltaMin                    = 0.05f;  // position delta min
        m_MaxPredictTime                      = 1.5f;   // maximum time to predict
        m_ApplyVelocity                       = true;   // indicates whether velocities from the network should be used
        m_PredictAcceleration                 = false;  // indicates whether the blender should predict acceleration of the target object
        m_MaxSpeedToPredict                   = 20.0f;  // maximum speed to predict a ped is moving at (when estimating velocities from position updates)
        m_MaxHeightDelta                      = 2.5f;   // maximum height delta for a ped before correcting the position (used when z blending turned off for peds)
        m_HeadingBlend                        = 0.2f;   // heading blend
        m_HeadingBlendMin                     = 0.02f;  // heading blend min
        m_HeadingBlendMax                     = TWO_PI; // heading blend max
        m_ExtrapolateHeading                  = false;  // extrapolate heading flag
        m_BlendRampTime                       = 1500;   // time to ramp towards the target destination (scale by m_positionBlend)
        m_BlendStopTime                       = 2000;   // time to linear interpolate towards target destination
        m_VelocityErrorThreshold              = 0.2f;   // The velocity error value that when detected will change the blender mode
        m_NormalModePositionThreshold         = 1.0f;   // The distance from the blend target that will return the blender mode to normal (when in velocity error mode)
        m_LowSpeedThresholdSqr                = 1.0f;   // The blend target speed when the blender will move to low-speed mode
        m_HighSpeedThresholdSqr               = 900.0f; // The blend target speed when the blender will move to high-speed mode
        m_MaxOnScreenPositionDeltaAfterBlend  = 1.0f;   // The maximum position the object is allowed to be from its target once blending has stopped
        m_MaxOffScreenPositionDeltaAfterBlend = 0.2f;   // The maximum position the object is allowed to be from its target once blending has stopped
        m_MaxPosDeltaDiffInteriors            = 1.0f;   // The maximum position delta the object is allowed to be from its target when the interior state differs
        m_LogarithmicBlendRatio               = 0.5f;   // The ratio to use when blending to the target position logarithmically
        m_LogarithmicBlendMaxVelChange        = 75.0f;  // The maximum velocity change allowed when blending logarithmically
        m_LerpOnObjectBlendRatio              = 0.5f;   // The ratio to use when blending using lerp on object mode
        m_LerpOnObjectThreshold               = 10.0f;  // The speed threshold before switching on lerp on object mode
        m_StartAllDirThreshold                = 1.0f;   // Threshold to start blending in all directions (this is the difference between the flat predicted velocity and the general predicted velocity)
        m_StopAllDirThreshold                 = 0.5f;   // Threshold to stop blending in all directions (this is the difference between the flat predicted velocity and the general predicted velocity)
        m_MinAllDirThreshold                  = 1.0f;   // The minimum predicted velocity to allow blending in all directions
        m_MaxCorrectionVectorMag              = 5.0f;   // The maximum magnitude of the correction vector

        // collisions
        m_CollisionModeDuration          = 3000;   // time period to enable custom blender behaviour after a collision
        m_CollisionModeThreshold         = 250.0f; // collision impulse threshold before using custom blender behaviour after a collision

        // ragdoll blending
        m_RagdollPosBlendLow             = 0.05f;  // position blend for objects receiving low level updates
        m_RagdollPosBlendMedium          = 0.1f;   // position blend for objects receiving medium level updates
        m_RagdollPosBlendHigh            = 0.2f;   // position blend  for objects receiving high level updates
        m_RagdollPositionMax             = 7.5f;   // position delta max when ragdolling
        m_RagdollOnObjPosDeltaMax        = 5.0f;   // position delta max when ragdolling on top of a network object
        m_RagdollAttachDistance          = 0.35f;  // distance before ragdoll is attached to what it is standing upon
        m_RagdollAttachZOffset           = 0.3f;   // the distance from the ground to attach the ragdoll
        m_RagdollTimeBeforeAttach        = 1000;   // time to wait before warping the ragdoll to the desired attach position

        // low speed mode
        m_LowSpeedMode.m_MinVelChange           = 0.1f;  // minimum change in velocity for the blender to apply
        m_LowSpeedMode.m_MaxVelChange           = 0.5f;  // maximum change in velocity for the blender to apply
        m_LowSpeedMode.m_ErrorIncreaseVel       = 0.3f;  // the error distance to start increasing the change rate
        m_LowSpeedMode.m_ErrorDecreaseVel       = 0.2f;  // the error distance to start decreasing the change rate
        m_LowSpeedMode.m_SmallVelocitySquared   = 0.2f;  // velocity change for which we don't try to correct
        m_LowSpeedMode.m_VelChangeRate          = 0.1f;  // current velocity change rate to used
        m_LowSpeedMode.m_MaxVelDiffFromTarget   = 1.0f;  // the maximum velocity difference from the last received velocity allowed
        m_LowSpeedMode.m_MaxVelChangeToTarget   = 0.5f;  // the maximum velocity change to the last received velocity allowed (used when the blender has accelerated faster than the target speed)
        m_LowSpeedMode.m_PositionDeltaMaxLow    = 4.0f;  // the position delta before a pop for objects being updated at a low update rate
        m_LowSpeedMode.m_PositionDeltaMaxMedium = 4.0f;  // the position delta before a pop for objects being updated at a medium update rate
        m_LowSpeedMode.m_PositionDeltaMaxHigh   = 4.0f;  // the position delta before a pop for objects being updated at a high update rate

        // normal mode
        m_NormalMode.m_MinVelChange           = 0.1f;  // minimum change in velocity for the blender to apply
        m_NormalMode.m_MaxVelChange           = 0.5f;  // maximum change in velocity for the blender to apply
        m_NormalMode.m_ErrorIncreaseVel       = 0.3f;  // the error distance to start increasing the change rate
        m_NormalMode.m_ErrorDecreaseVel       = 0.2f;  // the error distance to start decreasing the change rate
        m_NormalMode.m_SmallVelocitySquared   = 0.2f;  // velocity change for which we don't try to correct
        m_NormalMode.m_VelChangeRate          = 0.1f;  // current velocity change rate to used
        m_NormalMode.m_MaxVelDiffFromTarget   = 1.0f;  // the maximum velocity difference from the last received velocity allowed
        m_NormalMode.m_MaxVelChangeToTarget   = 0.5f;  // the maximum velocity change to the last received velocity allowed (used when the blender has accelerated faster than the target speed)
        m_NormalMode.m_PositionDeltaMaxLow    = 4.0f;  // the position delta before a pop for objects being updated at a low update rate
        m_NormalMode.m_PositionDeltaMaxMedium = 4.0f;  // the position delta before a pop for objects being updated at a medium update rate
        m_NormalMode.m_PositionDeltaMaxHigh   = 4.0f;  // the position delta before a pop for objects being updated at a high update rate

        // velocity error mode
        m_HighSpeedMode.m_MinVelChange           = 0.1f;  // minimum change in velocity for the blender to apply
        m_HighSpeedMode.m_MaxVelChange           = 1.5f;  // maximum change in velocity for the blender to apply
        m_HighSpeedMode.m_ErrorIncreaseVel       = 0.5f;  // the error distance to start increasing the change rate
        m_HighSpeedMode.m_ErrorDecreaseVel       = 0.3f;  // the error distance to start decreasing the change rate
        m_HighSpeedMode.m_SmallVelocitySquared   = 0.8f;  // velocity change for which we don't try to correct
        m_HighSpeedMode.m_VelChangeRate          = 0.1f;  // current velocity change rate to used
        m_HighSpeedMode.m_MaxVelDiffFromTarget   = 1.0f;  // the maximum velocity difference from the last received velocity allowed
        m_HighSpeedMode.m_MaxVelChangeToTarget   = 0.5f;  // the maximum velocity change to the last received velocity allowed (used when the blender has accelerated faster than the target speed)
        m_HighSpeedMode.m_PositionDeltaMaxLow    = 4.0f;  // the position delta before a pop for objects being updated at a low update rate
        m_HighSpeedMode.m_PositionDeltaMaxMedium = 4.0f;  // the position delta before a pop for objects being updated at a medium update rate
        m_HighSpeedMode.m_PositionDeltaMaxHigh   = 4.0f;  // the position delta before a pop for objects being updated at a high update rate

#if DEBUG_DRAW
        m_DisplayProbeHits                       = false;
#endif // DEBUG_DRAW
    }

    virtual ~CPedBlenderData() {}

    virtual const char *GetName() const = 0;

    virtual bool IsInAirBlenderData() const { return false; }

    bool   m_BlendHeading;             // enable / disable blending of heading
    bool   m_DoBBChecksOnWarp;         // enable / disable application of bounding box checks when this object is moved a large distance by the blender
    bool   m_ExtrapolateHeading;       // whether we should try to extrapolate a ped's heading
    float  m_MaxSpeedToPredict;        // maximum speed to predict a ped is moving at (when estimating velocities from position updates)
    float  m_MaxHeightDelta;           // maximum height delta for a ped before correcting the position (used when z blending turned off for peds)
    float  m_HeadingBlend;             // rate of change of heading angle
    float  m_HeadingBlendMin;          // delta at which heading is ignored
    float  m_HeadingBlendMax;          // delta at which heading is snapped
    float  m_MaxPosDeltaDiffInteriors; // The maximum position delta the object is allowed to be from its target when the interior state differs
    float  m_RagdollPosBlendLow;       // The amount to blend towards the target position per frame (low level updates)
    float  m_RagdollPosBlendMedium;    // The amount to blend towards the target position per frame (medium level updates)
    float  m_RagdollPosBlendHigh;      // The amount to blend towards the target position per frame (high level updates)
    float  m_RagdollPositionMax;       // The maximum position delta for a ragdolling ped
    float  m_RagdollOnObjPosDeltaMax;  // The maximum position delta for a ragdolling ped when standing on an object
    float  m_RagdollAttachDistance;    // The distance from the blender target position before a ragdoll can be attached to what it is standing upon
    float  m_RagdollAttachZOffset;     // The distance from the ground to attach the ragdoll
    u32    m_RagdollTimeBeforeAttach;  // The time to wait before warping the ragdoll to the desired attach position
    u32    m_CollisionModeDuration;    // time period to enable custom blender behaviour after a collision
    float  m_CollisionModeThreshold;   // collision impulse threshold before using custom blender behaviour after a collision
    float  m_LerpOnObjectBlendRatio;   // The ratio to use when blending using lerp on object mode
    float  m_LerpOnObjectThreshold;    // The speed threshold before switching on lerp on object mode
    float  m_StartAllDirThreshold;     // Threshold to start blending in all directions (this is the difference between the flat predicted velocity and the general predicted velocity)
    float  m_StopAllDirThreshold;      // Threshold to stop blending in all directions (this is the difference between the flat predicted velocity and the general predicted velocity)
    float  m_MinAllDirThreshold;       // The minimum predicted velocity to allow blending in all directions
    float  m_MaxCorrectionVectorMag;   // The maximum magnitude of the correction vector
#if DEBUG_DRAW
    bool  m_DisplayProbeHits;
#endif // DEBUG_DRAW
};

struct CPedBlenderDataInWater : public CPedBlenderData
{
    CPedBlenderDataInWater()
    {
        m_NormalMode.m_MaxVelDiffFromTarget    = 2.0f;  // the maximum velocity difference from the last received velocity allowed
        m_HighSpeedMode.m_MaxVelDiffFromTarget = 2.0f;  // the maximum velocity difference from the last received velocity allowed
    }

    const char *GetName() const { return "IN WATER"; }
};

struct CPedBlenderDataInAir : public CPedBlenderData
{
    CPedBlenderDataInAir()
    {
        m_ApplyVelocity     = true;   // indicates whether velocities from the network should be used
        m_MaxSpeedToPredict = 100.0f; // maximum speed to predict a ped is moving at (when estimating velocities from position updates)

        // normal mode
        m_NormalMode.m_MaxVelChange           = 1.5f; // maximum change in velocity for the blender to apply
        m_NormalMode.m_MaxVelDiffFromTarget   = 3.0f; // the maximum velocity difference from the last received velocity allowed
        m_NormalMode.m_PositionDeltaMaxLow    = 7.5f; // the position delta before a pop for objects being updated at a low update rate
        m_NormalMode.m_PositionDeltaMaxMedium = 7.5f; // the position delta before a pop for objects being updated at a medium update rate
        m_NormalMode.m_PositionDeltaMaxHigh   = 7.5f; // the position delta before a pop for objects being updated at a high update rate

        // velocity error mode
        m_HighSpeedMode.m_ErrorIncreaseVel       = 0.3f;  // current velocity change rate to used
        m_HighSpeedMode.m_ErrorDecreaseVel       = 0.2f;  // the error distance to start increasing the change rate
        m_HighSpeedMode.m_VelChangeRate          = 0.1f;  // the error distance to start decreasing the change rate
        m_HighSpeedMode.m_MaxVelChange           = 3.0f;  // maximum change in velocity for the blender to apply
        m_HighSpeedMode.m_MaxVelDiffFromTarget   = 20.0f; // the maximum velocity difference from the last received velocity allowed
        m_HighSpeedMode.m_MaxVelChangeToTarget   = 4.0f;  // the maximum velocity change to the last received velocity allowed (used when the blender has accelerated faster than the target speed)
        m_HighSpeedMode.m_PositionDeltaMaxLow    = 7.5f;  // the position delta before a pop for objects being updated at a low update rate
        m_HighSpeedMode.m_PositionDeltaMaxMedium = 7.5f;  // the position delta before a pop for objects being updated at a medium update rate
        m_HighSpeedMode.m_PositionDeltaMaxHigh   = 7.5f;  // the position delta before a pop for objects being updated at a high update rate
    }

    const char *GetName() const { return "IN AIR"; }

    virtual bool IsInAirBlenderData() const { return true; }
};

struct CPedBlenderDataTennis : public CPedBlenderData
{
    CPedBlenderDataTennis()
    {
        m_NormalMode.m_MaxVelChange            = 2.5f;  // maximum change in velocity for the blender to apply
        m_NormalMode.m_MaxVelDiffFromTarget    = 3.5f;  // the maximum velocity difference from the last received velocity allowed
        m_HighSpeedMode.m_MaxVelChange         = 2.5f;  // maximum change in velocity for the blender to apply
        m_HighSpeedMode.m_MaxVelDiffFromTarget = 3.5f;  // the maximum velocity difference from the last received velocity allowed
    }

    const char *GetName() const { return "TENNIS"; }
};

struct CPedBlenderDataOnFoot : public CPedBlenderData
{
    CPedBlenderDataOnFoot()
    {
    }

    const char *GetName() const { return "ON FOOT"; }
};

struct CPedBlenderDataAnimatedRagdollFallback : public CPedBlenderData
{
    CPedBlenderDataAnimatedRagdollFallback()
    {        
        m_ApplyVelocity                 = true;   // indicates whether velocities from the network should be used
        m_MaxSpeedToPredict             = 100.0f; // maximum speed to predict a ped is moving at (when estimating velocities from position updates)
        m_LogarithmicBlendRatio         = 0.5f;   // The ratio to use when blending to the target position logarithmically
        m_LogarithmicBlendMaxVelChange  = 75.0f;  // The maximum velocity change allowed when blending logarithmically

        // normal mode
        m_NormalMode.m_MaxVelChange           = 1.5f; // maximum change in velocity for the blender to apply
        m_NormalMode.m_MaxVelDiffFromTarget   = 5.0f; // the maximum velocity difference from the last received velocity allowed
        m_NormalMode.m_PositionDeltaMaxLow    = 7.5f; // the position delta before a pop for objects being updated at a low update rate
        m_NormalMode.m_PositionDeltaMaxMedium = 7.5f; // the position delta before a pop for objects being updated at a medium update rate
        m_NormalMode.m_PositionDeltaMaxHigh   = 7.5f; // the position delta before a pop for objects being updated at a high update rate

        // velocity error mode
        m_HighSpeedMode.m_ErrorIncreaseVel       = 0.3f;  // current velocity change rate to used
        m_HighSpeedMode.m_ErrorDecreaseVel       = 0.2f;  // the error distance to start increasing the change rate
        m_HighSpeedMode.m_VelChangeRate          = 0.1f;  // the error distance to start decreasing the change rate
        m_HighSpeedMode.m_MaxVelChange           = 1.5f;  // maximum change in velocity for the blender to apply
        m_HighSpeedMode.m_MaxVelDiffFromTarget   = 10.0f; // the maximum velocity difference from the last received velocity allowed
        m_HighSpeedMode.m_MaxVelChangeToTarget   = 4.0f;  // the maximum velocity change to the last received velocity allowed (used when the blender has accelerated faster than the target speed)
        m_HighSpeedMode.m_PositionDeltaMaxLow    = 7.5f;  // the position delta before a pop for objects being updated at a low update rate
        m_HighSpeedMode.m_PositionDeltaMaxMedium = 7.5f;  // the position delta before a pop for objects being updated at a medium update rate
        m_HighSpeedMode.m_PositionDeltaMaxHigh   = 7.5f;  // the position delta before a pop for objects being updated at a high update rate
    }

    const char *GetName() const { return "ANIMATED RAGDOLL FALLBACK"; }
};

struct CPedBlenderDataFirstPersonMode : public CPedBlenderData
{
    CPedBlenderDataFirstPersonMode()
    {
        m_MaxOnScreenPositionDeltaAfterBlend = 3.0f;   // The maximum position the object is allowed to be from its target once blending has stopped
    }

    const char *GetName() const { return "FIRST PERSON MODE"; }
};

struct CPedBlenderDataHiddenUnderMapForVehicle : public CPedBlenderData
{
    CPedBlenderDataHiddenUnderMapForVehicle()
    {
        m_LogarithmicBlendRatio                  = 0.2f;
        m_MaxSpeedToPredict                      = 75.0f;

        m_NormalMode.m_PositionDeltaMaxLow       = 20.0f; // the position delta before a pop for objects being updated at a low update rate
        m_NormalMode.m_PositionDeltaMaxMedium    = 20.0f; // the position delta before a pop for objects being updated at a medium update rate
        m_NormalMode.m_PositionDeltaMaxHigh      = 20.0f; // the position delta before a pop for objects being updated at a high update rate

        m_HighSpeedMode.m_PositionDeltaMaxLow    = 20.0f;  // the position delta before a pop for objects being updated at a low update rate
        m_HighSpeedMode.m_PositionDeltaMaxMedium = 20.0f;  // the position delta before a pop for objects being updated at a medium update rate
        m_HighSpeedMode.m_PositionDeltaMaxHigh   = 20.0f;  // the position delta before a pop for objects being updated at a high update rate
    }

    const char *GetName() const { return "HIDDEN UNDER MAP FOR VEHICLE"; }
};

// ===========================================================================================================================
// CNetBlenderPed
// ===========================================================================================================================
class CPed;

class CNetBlenderPed : public netBlenderLinInterp
{
public:

    FW_REGISTER_CLASS_POOL(CNetBlenderPed);

    CNetBlenderPed(CPed *ped, CPedBlenderData *blendData);

    virtual ~CNetBlenderPed();

    const char *GetBlenderDataName() const 
    {
#if __BANK
        if(UsedAnimatedRagdollBlendingLastFrame() && ms_pedBlenderDataAnimatedRagdoll)
        {
            return ms_pedBlenderDataAnimatedRagdoll->GetName();
        }
#endif // __BANK

        return GetBlenderData()->GetName(); 
    }

    virtual void OnOwnerChange(blenderResetFlags resetFlag = RESET_POSITION_AND_VELOCITY);
    virtual void Update();
    virtual void Reset();
    virtual void GoStraightToTarget();
    virtual void RecalculateTarget();
    virtual void ProcessPrePhysics();
    virtual void ProcessPostPhysics();
	virtual bool DisableZBlending() const;

    float GetLastHeadingReceived();

    void UpdateHeading(float heading, u32 time);
    void UpdateStandingData(ObjectId standingOnObjectID, const Vector3 &standingOnLocalOffset, u32 time);

	virtual const Vector3 GetCurrentPredictedPosition() const;
	const Vector3 GetCurrentPredictedPositionOnBoat() const;
	virtual const Vector3 GetPredictedVelocity(float maxSpeedToPredict) const;
	virtual float GetCurrentPredictedHeading();

    const Vector3 GetPredictedVelocity() const { return GetPredictedVelocity(GetBlenderData()->m_MaxSpeedToPredict); }

    bool IsBlendingHeading() const { return m_BlendingHeading; }

    bool IsStandingOnObject(ObjectId &objectStoodOn, Vector3 &objectStoodOnOffset);
    
    bool IsRagdolling() const { return m_IsRagdolling; }
    void SetIsRagdolling(bool isRagdolling) { m_IsRagdolling = isRagdolling; }
    void SetIsOnStairs(bool isOnStairs)   { m_IsOnStairs = isOnStairs; }

    void RegisterCollision(netObject *collisionObject, const float impulseMag);

    Vector3 GetCorrectionVector() const { return m_CorrectionVector; }

    void UseAnimatedRagdollFallbackBlendingThisFrame() { m_UseAnimatedRagdollBlending = true; }

#if __BANK
    bool UsedAnimatedRagdollBlendingLastFrame() const { return m_UsedAnimatedRagdollBlendingLastFrame; }
#endif // __BANK

	void DisableRagdollZBlending() { m_DisableRagdollZBlending = true; }

    bool HasAttachedRagdoll() const { return m_RagdollAttached; }

    bool IsBlendingInAllDirections() const { return m_BlendingInAllDirections; }

    void OnDesiredVelocityCalculated() { m_CalculatedDesiredVelocity = true; }

    void OnPredictionPop() const;

    //PURPOSE
    // Detaches the ragdolling ped from the object they were standing upon
    void DetachRagdoll();

    void UpdateHeadingBlending();

    void OnPedDetached();

#if __BANK
    static bool ms_DisplayPushRoundCarsDebug;
    static bool ms_UseLocalPlayerForCarPushTarget;
#endif // __BANK
    static float ms_PushRoundCarsBoxExpandSize;
    static float ms_PushRoundCarsForce;

    static CPedBlenderData *ms_pedBlenderDataAnimatedRagdoll;

private:

    virtual CPedBlenderData *GetBlenderData() { return static_cast<CPedBlenderData *>(netBlender::GetBlenderData()); }
    virtual const CPedBlenderData *GetBlenderData() const { return static_cast<const CPedBlenderData *>(netBlender::GetBlenderData()); }

    bool GetIsOnScreen() const;

    bool           DoNoZBlendHeightCheck(float heightDelta) const;
    const Vector3  GetPreviousPositionFromObject()      const;
    const Vector3  GetPositionFromObject()              const;
    const Vector3  GetVelocityFromObject()              const;
    const Vector3  GetForwardVectorFromObject()         const;
    float          GetHeadingFromObject()               const;
    bool           UsingInAirBlenderData()              const;

    bool SetPositionOnObject(const Vector3 &position, bool warp);
	bool ShouldTestForMapCollision();
    void SetVelocityOnObject(const Vector3 &velocity);
    void SetHeadingOnObject(float heading);

    netObject *GetNetworkObject();
    const netObject *GetNetworkObject() const;

    void UpdateRagdollBlending();
    void UpdateAnimatedRagdollFallbackBlending();

    //PURPOSE
    // Returns the blend ratio to use based on the blender update level
    float GetBlendRatioForUpdateLevel() const;

    //PURPOSE
    // Returns the maximum distance from the blend target before popping when blending a ragdolling ped
    float GetPositionMaxForRagdollBlending() const;

    virtual const Vector3 GetPredictedNextFramePosition(const Vector3 &predictedCurrentPosition,
                                                        const Vector3 &targetVelocity) const;

    virtual const Vector3 GetPredictedFuturePosition(u32 timeInFutureMs) const;

    //PURPOSE
    // Counts down the timer set at the last collision (if it is non-zero), blender behaviour differs
    // after a collision has been detected
    void UpdateCollisionTimer();

    //PURPOSE
    // Does a collision check against the ped capsule at the specified position with other objects
    bool DoCollisionTest(const Vector3 &position);

    //PURPOSE
    // Checks whether the clone ped has got stuck on a vehicle and helps it get out of it if so
    void CheckForStuckOnVehicle();

    //PURPOSE
    // Attaches the ragdolling ped to the object they are standing upon
    void AttachRagdoll();

    //PURPOSE
    // Checks whether we are predicting through map collision
    bool TestForMapCollision(const Vector3 &lastReceivedPos, const Vector3 &newPos);

    //PURPOSE
    // Finds the ground Z position at the specified position
    bool TestForGroundZ(const Vector3 &position, float &groundZ);

    //PURPOSE
    // Returns whether the blender should do some additional lerping of the ped position relative to the
    // object stood upon. This is sometimes necessary for peds standing on the back of fast moving objects
    // subject to lots of external forces (e.g. a ped standing on the back off a boat in stormy weather) to
    // prevent the clone from falling off the object locally before this happens on the local machine
    bool ShouldLerpOnObject(netObject *networkObject);

    //PURPOSE
    // Checks whether the ped velocity needs fixing up to keep the ped on the stairs they are walking on
    void CheckForOnStairsFixup();

    //PURPOSE
    // Checks whether the ped is intersecting an object - this can happen for far away stationary peds standing on top of
    // objects that may not be streamed in on the local machine
    void CheckForIntersectingObjects();

    //PURPOSE
    // Checks whether we should activate network blending in all directions for this ped, rather than just relative to
    // the forward vector which we do as standard.
    void UpdateShouldBlendInAllDirections();

#if __BANK
	//PURPOSE
	// Debug function for catching bad target velocities being set on the blender for the target object
	//PARAMS
	// velocity - The velocity to validate.
	virtual void ValidateTargetVelocity(const Vector3 &velocity);
#endif // __BANK

    CPed               *m_Ped;						  // the blender target ped
    CPedPredictionData  m_PredictionData;			  // the ped prediction data
    bool                m_BlendingHeading;			  // whether we are currently blending the heading
    u32                 m_CollisionTimer;			  // Countdown timer after a collision is detected
    Vector3             m_CorrectionVector;			  // Cumulative correction vector for ped velocities
    u32                 m_LastTimestampStopped;       // The timestamp of the last position received when this ped has stopped (locally and on the owner)
    bool                m_BlendWhenStationary;        // Indicates whether the blender should move the ped when it is standing still
    bool                m_BlendingInAllDirections;    // Indicates whether the blender should blend in all directions, or only relative to the forward vector
    bool                m_UseAnimatedRagdollBlending; // A reset flag, switching the blender to use animated ragdoll fallback blending for a frame
	bool				m_DisableRagdollZBlending;	  // A reset flag, disabling the ragdoll Z blending for 1 frame. Used by certain ragdoll tasks.
    bool                m_RagdollAttached;            // Indicates the ped's ragdoll has been attached to the entity they are standing on by the blender
	bool				m_StandingOnObject;           // is the ped standing on an object?
    bool                m_IsRagdolling;               // is the ped ragdolling?
    bool                m_IsOnStairs;                 // is the ped on the stairs on the remote machine?
    bool                m_CalculatedDesiredVelocity;  // has the desired velocity been calculated locally for this ped this frame?
    u32                 m_TimeRagdollAttachDesired;   // The time the blender starts wanting to attach the ped to the object they are standing upon
    bool                m_LerpingOnObject;            // are we lerping the ped position relative to an object they are standing upon
    Vector3             m_OnObjectLerpCurrentPos;     // the last relative offset from the object stood upon used for lerping

#if __BANK
    bool                m_UsedAnimatedRagdollBlendingLastFrame; // Indicates animated ragdoll blending was used in the last frame (debugging purposes)
#endif // __BANK
};

#endif  // NETBLENDERPED_H
