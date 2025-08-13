//
// name:        NetBlenderPhysical.h
// written by:  John Gurney
// description: Network blender used by physics objects

#ifndef NETBLENDERPHYSICAL_H
#define NETBLENDERPHYSICAL_H

// rage includes
#include "grcore/debugdraw.h"
#include "vector/matrix34.h"

// framework includes
#include "fwnet/netblenderlininterp.h"

// game includes
#include "vehicles/vehicle.h"

// this must be a multiple of 16 otherwise we will get alignment problems on Xbox 360
#define LARGEST_NET_PHYSICAL_BLENDER_CLASS (__64BIT ? 848 : 800)

// ===========================================================================================================================
// CPhysicalPredictionData
// ===========================================================================================================================
class CPhysicalPredictionData
{
    struct netSnapshot;

public:

    CPhysicalPredictionData(const Matrix34 &matrix,
                            const Vector3  &angVelocity,
                            const u32       time);
    ~CPhysicalPredictionData();

    void Reset(const Matrix34 &matrix,
               const Vector3  &angVelocity,
               const u32       time);

    netSnapshot &GetSnapshotPast()    { return m_snapshotPast;    }
    netSnapshot &GetSnapshotPresent() { return m_snapshotPresent; }
    netSnapshot &GetSnapshotFuture()  { return m_snapshotFuture;  }

    const netSnapshot &GetSnapshotPast()    const { return m_snapshotPast;    }
    const netSnapshot &GetSnapshotPresent() const { return m_snapshotPresent; }
    const netSnapshot &GetSnapshotFuture()  const { return m_snapshotFuture;  }

private:

    CPhysicalPredictionData();

	// snapshot of data to predict from
    struct netSnapshot
    {
        Matrix34 m_matrix;
        Vector3  m_angVelocity;
        u32      m_matrixTimestamp;
        u32      m_angVelocityTimestamp;
    };

    netSnapshot m_snapshotPast;    // first snapshot (past)
    netSnapshot m_snapshotPresent; // second snapshot (present)
    netSnapshot m_snapshotFuture;  // third snapshot (future)
};

// ===========================================================================================================================
// CPhysicalBlenderData
// ===========================================================================================================================

struct CPhysicalBlenderData : public CLinInterpBlenderData
{
    CPhysicalBlenderData()
    {
        m_BlendingOn                             = true;         // blending on
        m_BlendOrientation                       = true;         // blend orientation
        m_DoBBChecksOnWarp                       = true;         // do bounding box checks with other entities prior to moving the target object large distances
        m_AllowAdjustVelChangeFromUpdates        = true;         // indicates we are allowing the per frame velocity change to be adjusted to match the changes coming over the network
        m_UseBlendVelSmoothing                   = true;         // indicates whether the blender will smooth velocity changes over a time interval, or just work frame to frame
        m_BlendVelSmoothTime                     = 150;          // the time in ms to base blender behaviour over (looks at where the object will be this time in the future)
        m_PositionDeltaMin                       = 0.05f;        // position delta min
        m_MaxPredictTime                         = 1.5f;         // maximum time to predict
        m_ApplyVelocity                          = true;         // indicates whether velocities from the network should be used
        m_PredictAcceleration                    = false;        // indicates whether the blender should predict acceleration of the target object
        m_OrientationDeltaMax                    = 3.14f / 4.0f; // orientation delta max
        m_OrientationDeltaMin                    = 0.01f;        // orientation delta min
        m_OrientationBlend                       = 0.1f;         // orientation blend
        m_MinOrientationDiffToStartBlend         = 0.05f;        // minimum orientation delta to start a new orientation blend
        m_BlendRampTime                          = 1500;         // time to ramp towards the target destination (scale by m_PositionBlend)
        m_BlendStopTime                          = 2000;         // time to linear interpolate towards target position
        m_BlendOrientationStopTime               = 2000;         // time to linear interpolate towards target orientation
        m_TimeToApplyAngVel                      = 100;          // time in milliseconds to apply the last received angular velocity
        m_VelocityErrorThreshold                 = 0.2f;         // The velocity error value that when detected will change the blender mode
        m_NormalModePositionThreshold            = 1.0f;         // The distance from the blend target that will return the blender mode to normal (when in velocity error mode)
        m_LowSpeedThresholdSqr                   = 1.0f;         // The blend target speed when the blender will move to low-speed mode
        m_HighSpeedThresholdSqr                  = 900.0f;       // The blend target speed when the blender will move to high-speed mode
        m_MaxOnScreenPositionDeltaAfterBlend     = 0.5f;         // The maximum position the object is allowed to be from its target once blending has stopped
        m_MaxOffScreenPositionDeltaAfterBlend    = 0.25f;        // The maximum position the object is allowed to be from its target once blending has stopped
        m_MaxOnScreenOrientationDeltaAfterBlend  = 0.15f;        // The maximum orientation delta the object is allowed to be from its target once blending has stopped
        m_MaxOffScreenOrientationDeltaAfterBlend = 0.01f;        // The maximum orientation delta the object is allowed to be from its target once blending has stopped
        m_LogarithmicBlendRatio                  = 0.5f;         // The ratio to use when blending to the target position logarithmically
        m_LogarithmicBlendMaxVelChange           = 75.0f;        // The maximum velocity change allowed when blending logarithmically
#if DEBUG_DRAW
        m_DisplayOrientationDebug                = false;        // displays debug draw to visualise orientation blending
        m_RestrictDebugDisplayToFocusEntity      = false;        // restricts the orientation blending debug visualisation to the focus entity only
        m_DisplayOrientationDebugScale           = 100.0f;       // changes how far the visualisation projects out
#endif // DEBUG_DRAW

        // low speed mode
        m_LowSpeedMode.m_MinVelChange           = 0.1f; // minimum change in velocity for the blender to apply
        m_LowSpeedMode.m_MaxVelChange           = 1.5f; // maximum change in velocity for the blender to apply
        m_LowSpeedMode.m_ErrorIncreaseVel       = 0.3f; // current velocity change rate to used
        m_LowSpeedMode.m_ErrorDecreaseVel       = 0.2f; // the error distance to start increasing the change rate
        m_LowSpeedMode.m_SmallVelocitySquared   = 0.1f; // velocity change for which we don't try to correct
        m_LowSpeedMode.m_VelChangeRate          = 0.1f; // the error distance to start decreasing the change rate
        m_LowSpeedMode.m_MaxVelDiffFromTarget   = 0.5f; // the maximum velocity difference from the last received velocity allowed
        m_LowSpeedMode.m_MaxVelChangeToTarget   = 0.5f; // the maximum velocity change to the last received velocity allowed (used when the blender has accelerated faster than the target speed)
        m_LowSpeedMode.m_PositionDeltaMaxLow    = 5.0f; // the position delta before a pop for objects being updated at a low update rate
        m_LowSpeedMode.m_PositionDeltaMaxMedium = 5.0f; // the position delta before a pop for objects being updated at a medium update rate
        m_LowSpeedMode.m_PositionDeltaMaxHigh   = 5.0f; // the position delta before a pop for objects being updated at a high update rate

        // normal mode
        m_NormalMode.m_MinVelChange           = 0.1f; // minimum change in velocity for the blender to apply
        m_NormalMode.m_MaxVelChange           = 1.5f; // maximum change in velocity for the blender to apply
        m_NormalMode.m_ErrorIncreaseVel       = 0.3f; // current velocity change rate to used
        m_NormalMode.m_ErrorDecreaseVel       = 0.2f; // the error distance to start increasing the change rate
        m_NormalMode.m_SmallVelocitySquared   = 1.0f; // velocity change for which we don't try to correct
        m_NormalMode.m_VelChangeRate          = 0.1f; // the error distance to start decreasing the change rate
        m_NormalMode.m_MaxVelDiffFromTarget   = 3.0f; // the maximum velocity difference from the last received velocity allowed
        m_NormalMode.m_MaxVelChangeToTarget   = 0.5f; // the maximum velocity change to the last received velocity allowed (used when the blender has accelerated faster than the target speed)
        m_NormalMode.m_PositionDeltaMaxLow    = 5.0f; // the position delta before a pop for objects being updated at a low update rate
        m_NormalMode.m_PositionDeltaMaxMedium = 5.0f; // the position delta before a pop for objects being updated at a medium update rate
        m_NormalMode.m_PositionDeltaMaxHigh   = 5.0f; // the position delta before a pop for objects being updated at a high update rate

        // velocity error mode
        m_HighSpeedMode.m_MinVelChange           = 0.1f;  // minimum change in velocity for the blender to apply
        m_HighSpeedMode.m_MaxVelChange           = 1.5f;  // maximum change in velocity for the blender to apply
        m_HighSpeedMode.m_ErrorIncreaseVel       = 0.3f;  // current velocity change rate to used
        m_HighSpeedMode.m_ErrorDecreaseVel       = 0.2f;  // the error distance to start increasing the change rate
        m_HighSpeedMode.m_SmallVelocitySquared   = 1.0f;  // velocity change for which we don't try to correct
        m_HighSpeedMode.m_VelChangeRate          = 0.1f;  // the error distance to start decreasing the change rate
        m_HighSpeedMode.m_MaxVelDiffFromTarget   = 20.0f; // the maximum velocity difference from the last received velocity allowed
        m_HighSpeedMode.m_MaxVelChangeToTarget   = 4.0f;  // the maximum velocity change to the last received velocity allowed (used when the blender has accelerated faster than the target speed)
        m_HighSpeedMode.m_PositionDeltaMaxLow    = 5.0f;  // the position delta before a pop for objects being updated at a low update rate
        m_HighSpeedMode.m_PositionDeltaMaxMedium = 5.0f;  // the position delta before a pop for objects being updated at a medium update rate
        m_HighSpeedMode.m_PositionDeltaMaxHigh   = 5.0f;  // the position delta before a pop for objects being updated at a high update rate
    }

	virtual ~CPhysicalBlenderData() {}
	 
	virtual const char *GetName() const { return "PHYSICAL"; };

    bool     m_BlendOrientation;                       // enable / disable blending of orientation
    bool     m_DoBBChecksOnWarp;                       // enable / disable application of bounding box checks when this object is moved a large distance by the blender
    float    m_OrientationDeltaMax;                    // if the orientation difference between current and target is greater than this, pop orientation to angle
    float    m_OrientationDeltaMin;                    // if the orientation difference between current and target is less than this leave the object where it is
    float    m_MaxOnScreenOrientationDeltaAfterBlend;  // The maximum orientation delta the object is allowed to be from its target once blending has stopped
    float    m_MaxOffScreenOrientationDeltaAfterBlend; // The maximum orientation delta the object is allowed to be from its target once blending has stopped
    float    m_OrientationBlend;                       // rate of change of orientation angle
    float    m_MinOrientationDiffToStartBlend;         // minimum orientation delta to start a new orientation blend
    unsigned m_TimeToApplyAngVel;                      // time in milliseconds to apply the last received angular velocity
    unsigned m_BlendOrientationStopTime;               // This is the maximum time the blender can interpolate orientation without an additional update

#if DEBUG_DRAW
    bool  m_DisplayOrientationDebug;                   // displays debug draw to visualise orientation blending
    bool  m_RestrictDebugDisplayToFocusEntity;         // restricts the orientation blending debug visualisation to the focus entity only
    float m_DisplayOrientationDebugScale;              // changes how far the visualisation projects out
#endif // DEBUG_DRAW
};

struct CObjectBlenderDataParachute : public CPhysicalBlenderData
{
	CObjectBlenderDataParachute()
	{
		m_NormalMode.m_PositionDeltaMaxLow    		= 25.0f; // the position delta before a pop for objects being updated at a low update rate
		m_NormalMode.m_PositionDeltaMaxMedium 		= 25.0f; // the position delta before a pop for objects being updated at a medium update rate
		m_NormalMode.m_PositionDeltaMaxHigh   		= 25.0f; // the position delta before a pop for objects being updated at a high update rate

		m_HighSpeedMode.m_PositionDeltaMaxLow    	= 25.0f;  // the position delta before a pop for objects being updated at a low update rate
		m_HighSpeedMode.m_PositionDeltaMaxMedium 	= 25.0f;  // the position delta before a pop for objects being updated at a medium update rate
		m_HighSpeedMode.m_PositionDeltaMaxHigh   	= 25.0f;  // the position delta before a pop for objects being updated at a high update rate
        m_HighSpeedMode.m_MinVelChange           	= 0.1f;   // minimum change in velocity for the blender to apply
        m_HighSpeedMode.m_MaxVelChange           	= 5.0f;   // maximum change in velocity for the blender to apply
	}

	const char *GetName() const { return "PARACHUTE"; }
};


struct CObjectBlenderDataStandard : public CPhysicalBlenderData
{
    CObjectBlenderDataStandard()
    {
		m_LogarithmicBlendRatio               = 0.2f;  // The ratio to use when blending to the target position logarithmically
    }

	const char *GetName() const { return "STANDARD"; }
};

struct CObjectBlenderDataHighPrecision : public CPhysicalBlenderData
{
    CObjectBlenderDataHighPrecision()
    {
        m_BlendRampTime                       = 150;   // time to ramp towards the target destination (scale by m_PositionBlend)
        m_BlendStopTime                       = 200;   // time to linear interpolate towards target position
        m_MaxOnScreenPositionDeltaAfterBlend  = 0.1f;  // The maximum position the object is allowed to be from its target once blending has stopped
        m_MaxOffScreenPositionDeltaAfterBlend = 0.1f;  // The maximum position the object is allowed to be from its target once blending has stopped

        m_LowSpeedMode.m_SmallVelocitySquared  = 0.1f;  // velocity change for which we don't try to correct
        m_NormalMode.m_SmallVelocitySquared    = 0.1f;  // velocity change for which we don't try to correct
        m_HighSpeedMode.m_SmallVelocitySquared = 0.1f;  // velocity change for which we don't try to correct
    }

	const char *GetName() const { return "HIGH PRECISION"; }
};

struct CObjectBlenderDataArenaBall : public CPhysicalBlenderData
{
    CObjectBlenderDataArenaBall();

    const char *GetName() const { return "ARENA BALL"; }
};

// ===========================================================================================================================
// CNetBlenderPhysical
// ===========================================================================================================================
class CPhysical;

class CNetBlenderPhysical : public netBlenderLinInterp
{
public:

    FW_REGISTER_CLASS_POOL(CNetBlenderPhysical);

    CNetBlenderPhysical(CPhysical *pPhysical, CPhysicalBlenderData *pBlendData);
    ~CNetBlenderPhysical();

    s32 GetBBCheckTimer() const { return m_BBCheckTimer; }
    s32 GetDisableCollisionTimer() const { return m_disableCollisionTimer; }
    u32 GetTimeWithNoCollision() const;

    Vector3 GetParentVehicleOffset() const { return m_ParentVehicleOffset; }
    void    SetParentVehicleOffset(bool hasOffset, const Vector3 &parentVehicleOffset) { m_HasParentVehicleOffset = hasOffset; m_ParentVehicleOffset = parentVehicleOffset; }

    void MarkPedStandingOnObject() { m_IsPedStandingOnObject = true; }

#if __BANK
    const Matrix34 &GetLastSnapMatrix()      const { return m_LastSnapMatrix; }
    const Vector3  &GetLastSnapAngVel()      const { return m_LastSnapAngVel; }
#endif // __BANK

    bool IsBlendingOrientation() const { return m_bBlendingOrientation; }
	
    virtual void OnOwnerChange(blenderResetFlags resetFlag = RESET_POSITION_AND_VELOCITY);
    virtual void Update();
    virtual void Reset();
    virtual void GoStraightToTarget();
    virtual void RecalculateTarget();
    virtual void ProcessPostPhysics();
	virtual bool DisableZBlending() const;

	void SetPhysical(CPhysical* pPhysical) { m_pPhysical = pPhysical; }

    Matrix34 GetLastMatrixReceived(u32 *timestamp = 0);
    Vector3  GetLastAngVelocityReceived(u32 *timestamp = 0);

    void UpdateMatrix(const Matrix34 &matrix, u32 time);
    void UpdateAngVelocity(const Vector3  &angVelocity, u32 time);
	
	virtual bool IsBoatBlender() { return false; }

protected:

    virtual CPhysicalBlenderData *GetBlenderData() { return static_cast<CPhysicalBlenderData *>(netBlender::GetBlenderData()); }
    virtual const CPhysicalBlenderData *GetBlenderData() const { return static_cast<const CPhysicalBlenderData *>(netBlender::GetBlenderData()); }

    CPhysical *GetPhysical() { return m_pPhysical; }
    const CPhysical *GetPhysical() const { return m_pPhysical; }

    bool GetIsOnScreen() const;

    virtual bool CanBlendWhenAttached() const { return false; }
    virtual bool CanApplyNewOrientationMatrix(const Matrix34 &UNUSED_PARAM(newMatrix)) const { return true; }

    virtual void UpdateOrientationBlending();
    virtual void UpdateAngularVelocity();
    void UpdateNoCollisionTimer();

    virtual float GetOrientationBlendValue() const;

    const Vector3  GetPreviousPositionFromObject() const;
    const Vector3  GetPositionFromObject() const;
    const Vector3  GetVelocityFromObject() const;
    const Vector3  GetForwardVectorFromObject() const;
    Matrix34       GetMatrixFromObject();
    Vector3        GetAngVelocityFromObject();

    bool SetPositionOnObject(const Vector3 &position, bool warp);
    void SetVelocityOnObject(const Vector3 &velocity);
    void SetMatrixOnObject(Matrix34 &matrix, bool bWarp, bool bUpdatePedsStandingOnObject = false);
    virtual void SetAngVelocityOnObject(const Vector3 &angVelocity);

    void OnPredictionPop() const;

    netObject *GetNetworkObject();
    const netObject *GetNetworkObject() const;

    bool IsImportantVehicle(CVehicle *vehicle) const;

    bool DoCollisionTest(const Vector3 &position, bool &hitImportantVehicle);

    void UpdatePedsStandingOnObjectAfterWarp(const Matrix34 &oldMat, const Matrix34 &newMat);

    bool GoStraightToTargetUsesPrediction() const;

    void GetQuaternionsFromMatrices(const Matrix34 &currentMatrix, const Matrix34 &newMatrix, Quaternion &currentQuat, Quaternion &newQuat);

#if DEBUG_DRAW
    bool ShouldDisplayOrientationDebug() const;
#endif // DEBUG_DRAW

protected:

    CPhysical              *m_pPhysical;                   // physical object to predict
    CPhysicalPredictionData m_predictionData;              // data used for prediction
    Vector3                 m_ParentVehicleOffset;         // parent vehicle offset (used when attached)
    u32                     m_blendPhysicalStartTime;      // time we start blending upon receiving an update
    u32                     m_timeAngularVelocityReceived; // network time we received the last angular velocity update (used to determine how long we apply this for)
    bool                    m_bBlendingOrientation;        // whether we are current blending the orientation
    s32                     m_disableCollisionTimer;       // timer to disable collisions after a prediction pop
    s32                     m_BBCheckTimer;                // timer to do bounding box collisions after a prediction pop
    bool                    m_IsPedStandingOnObject;       // indicates whether a ped is standing on top of the target object
    bool                    m_HasParentVehicleOffset;      // indicates whether the parent vehicle offset is valid
    bool                    m_IsGoingStraightToTarget;     // indicates whether the blender is in the process of moving the object to straight to it's target position
#if __BANK
    u32                     m_NumBBChecksBeforeWarp;       // the number of BB checks done on this object before it reached it's target
    Matrix34                m_LastSnapMatrix;              // the last matrix used by the blender
    Vector3                 m_LastSnapAngVel;              // the last angular velocity used by the blender
#endif // __BANK
};

#endif  // NETBLENDERPHYSICAL_H
