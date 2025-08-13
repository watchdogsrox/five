//
// name:		NetBlenderVehicle.h
// written by:	Daniel Yelland
// description:	Network blender used by vehicles

#ifndef NETBLENDERVEHICLE_H
#define NETBLENDERVEHICLE_H

// game includes
#include "network/objects/prediction/NetBlenderPhysical.h"

// ===========================================================================================================================
// CVehicleBlenderData
// ===========================================================================================================================

struct CVehicleBlenderData : public CPhysicalBlenderData
{
    CVehicleBlenderData()
    {
        m_SteerAngleMax          = 1.0f;
        m_SteerAngleMin          = 0.1f;
        m_SteerAngleRatio        = 0.25f;
        m_NearbyVehicleThreshold = 10.0f;
    }

	const char *GetName() const { return "VEHICLE"; }

    float m_SteerAngleMax;          // The maximum steer angle delta before setting to the target directly
    float m_SteerAngleMin;          // The minimum steer angle delta before setting to the target directly
    float m_SteerAngleRatio;        // The ratio to blend the steer angle
    float m_NearbyVehicleThreshold; // The distance between vehicles before they are consider nearby for validating orientation blending
};


// ===========================================================================================================================
// CLargeSubmarineBlenderData
// ===========================================================================================================================

struct CLargeSubmarineBlenderData : public CVehicleBlenderData
{
	CLargeSubmarineBlenderData()
	{
		m_OrientationBlend = 0.02f;
		m_OrientationDeltaMin = 0.001f;
		m_MinOrientationDiffToStartBlend = 0.001f;	
	}

	const char *GetName() const { return "LARGE SUBMARINE"; }	
};

// ===========================================================================================================================
// CNetBlenderVehicle
// ===========================================================================================================================
class CTrailer;
class CVehicle;

class CNetBlenderVehicle : public CNetBlenderPhysical
{
public:

	CNetBlenderVehicle(CVehicle *vehicle, CVehicleBlenderData *blenderData);
	~CNetBlenderVehicle();

    float GetTargetSteerAngle() const { return m_TargetSteerAngle; }

    void SetTargetSteerAngle(float targetSteerAngle)     { m_TargetSteerAngle           = targetSteerAngle; }
    void SetAllowAngularVelocityUpdate(bool allowUpdate) { m_AllowAngularVelocityUpdate = allowUpdate;      }

	void HighSpeedEdgeFallDetector();

    virtual void OnOwnerChange(blenderResetFlags resetFlag = blenderResetFlags::RESET_POSITION_AND_VELOCITY);
	virtual void Update();
    virtual void GoStraightToTarget();
	virtual bool DisableZBlending() const;
    virtual void ProcessPostPhysics();
    virtual void UpdateBlenderMode(const float distanceFromTarget);
	virtual void ProcessEndOfBlendTime(Vector3 &objectPosition);

    bool IsDoingOrientationBlendChecks() const  { return m_DoExtraOrientationBlendingChecks; }
    void SetDoExtraOrientationBlendChecks(bool enable) { m_DoExtraOrientationBlendingChecks = enable; }
	void SetHighSpeedEdgeFallDetector(bool val) { m_HighSpeedEdgeFallDetector = val; }
#if __BANK
    float GetParentVehicleOffsetDelta(bool &blendedTooFar) const;
    bool  IsBlendingTrailer() const { return m_BlendingTrailer; }
#endif // __BANK

protected:

    virtual CVehicleBlenderData *GetBlenderData() { return static_cast<CVehicleBlenderData *>(netBlender::GetBlenderData()); }
    virtual const CVehicleBlenderData *GetBlenderData() const { return static_cast<const CVehicleBlenderData *>(netBlender::GetBlenderData()); }

    bool CanBlendWhenAttached() const;
    bool CanApplyNewOrientationMatrix(const Matrix34 &newMatrix) const;
    void UpdateOrientationBlending();
    void UpdateAngularVelocity();

    bool OnlyPredictRelativeToForwardVector() const;
    bool PreventBlendingAwayFromVelocity(float positionDelta) const;
    bool SetPositionOnObject(const Vector3 &position, bool warp);
    void SetVelocityOnObject(const Vector3 &velocity);
    void SetAngVelocityOnObject(const Vector3 &angVelocity);

private:

    virtual float GetOrientationBlendValue() const;

    void UpdateSteerAngle();
    void DoFixedPhysicsBlend();
    void DoTrailerBlend(CVehicle &parentVehicle, CTrailer &trailer);
    void UpdateTrailerAfterWarp(CVehicle &parentVehicle, CTrailer &trailer);
    void InformAttachedPedsOfWarp(const Vector3 &newPosition);
    bool TestForMapCollision(const Vector3 &lastReceivedPos, const Vector3 &newPos);

    Vector3 m_VelocityLastFrame;
    float   m_TargetSteerAngle;
    bool    m_BlendingTrailer;
    bool    m_AllowAngularVelocityUpdate;
    bool    m_DoExtraOrientationBlendingChecks;
	bool    m_HighSpeedEdgeFallDetector;         // Special option to be used in modes where high speed vehicles can drive around edges of maps/platforms
	u32     m_LastHighSpeedEdgeFallMapTest;

	bool    m_WasDoingFixedPhysicsBlending;
#if __BANK
    unsigned m_LastFrameProbeSubmitted;      // The last frame a world probe has been submitted for this target vehicle
    unsigned m_NumProbesInConsecutiveFrames; // Number of frames in a row a world probe has been submitted for this target vehicle
#endif // __BANK
};

#endif  // NETBLENDERVEHICLE_H
