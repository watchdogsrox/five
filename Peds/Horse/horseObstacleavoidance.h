#ifndef HRS_OBSTACLE_AVOIDANCE_H
#define HRS_OBSTACLE_AVOIDANCE_H

#include "Peds/Horse/horseDefines.h"

#if ENABLE_HORSE

#include "horseControl.h"
#include "parser/manager.h"
#include "physics/WorldProbe/worldprobe.h"
#include "task/System/TaskHelpers.h"

class CPed;

namespace rage 
{ 
	class phBound;
	class phInst; 
	class phIntersection; 
	class phSegment; 
}

////////////////////////////////////////////////////////////////////////////////
// class hrsObstacleAvoidanceControl
// 
// PURPOSE: Encapsulate all obstacle avoidance-related tuning, state, and logic.  This
// class is used by the horse sim to determine whether a mount should override
// user input to avoid a small obstacle, such as a tree or rock.
//
class hrsObstacleAvoidanceControl : public hrsControl
{
public:

	hrsObstacleAvoidanceControl();
	void Init(CPed* owner) {m_pPed=owner;}

	// PURPOSE: Reset the internal state.
	void Reset(bool bResetPitch = true);

	// PURPOSE: Update the control.
	void Update(const float fCurrentSpeedMPS, const Matrix34 & mCurrMtx, float fCurrHeading, const float fSoftBrakeStopDist, const float fStickTurn, const bool bOnMountCheck = false);

	// PURPOSE: Calculate how fast the horse can go because it's in water
	float CalculateWaterMaxSpeed();

	// PURPOSE: auto-adjust the goal/dest heading to avoid peril
	// PARAMS:
	//	bHeadingInput - true if there is heading input, false otherwise
	//	fInOutGoalHeading - in/out param for goal heading
	//	fInOutDestHeading - in/out param for destination heading
	// RETURNS: true if the heading was modified, false otherwise.
	bool AdjustHeading(float & fInOutGoalHeading);

	// PURPOSE: auto-adjust the yaw to avoid peril
	// PARAMS:
	//	fInOutYaw - in/out param for yaw
	// RETURNS: true if the heading was modified, false otherwise.
	bool AdjustYaw(float & fInOutYaw);

	// PURPOSE: get how much to brake based on obstacles in front
	// RETURNS: how much to brake.
	float GetBrake() const { return IsEnabled() && IsMountedByPlayer() ? m_fBrake : 0.0f; }

	// PURPOSE: get the pitch of the terrain in front of us
	// RETURNS: forward terrain pitch
	float GetSurfacePitch() const { return IsEnabled() && IsMountedByPlayer() ? m_fSurfacePitch : 0.0f; }

	// PURPOSE: get is avoiding
	// RETURNS: is avoiding
	bool GetAvoiding() const { return m_bAvoiding; }

	//PURPOSE: force a stronger turn
	float GetHeadingScalar() const { return m_fHeadingScalar; }

	// PURPOSE: add debug widgets
	void AddWidgets(bkBank &);

	// PURPOSE: add static widgets
	static void AddStaticWidgets(bkBank &b);

	enum CorrectionResults 
	{
		CORRECT_CENTER,
		CORRECT_LEFT,
		CORRECT_RIGHT,
		CORRECT_STOP,
		CORRECT_FAIL
	};

	enum SurfaceProbeResult
	{
		SPR_MISS = 0,
		SPR_HIT_NON_GROUND,
		SPR_HIT_GROUND
	};

protected:

	// PURPOSE: Perform a physics test to see what obstacles there are to avoid and computes a heading to use from the results.
	void ComputeAvoidance(const float fCurrentSpeedMPS, const Matrix34 & mCurrMtx, float fCurrHeading, const float fSoftBrakeStopDist, const float fStickTurn, const bool bOnMountCheck = false);

	// PURPOSE: Perform two probes in front of the horse to determine surface pitch - we will use this to orientate the avoidance tests.
	float GetSurfacePitch(const Matrix34 & mTestMtx, const float fTestLength);

	// PURPOSE: Performs a probe to get the intersection point of the surface ahead
	SurfaceProbeResult DoSurfaceProbe(const phSegment & segment, phIntersection & out, const Vector3 *underHorseNormal) const;

	// PURPOSE: Have we intersected a polygon?
	bool GetIntersectionHitPoly(const phIntersection & intersection, const phBound * pBound = NULL) const;

	// PURPOSE: Find center and radius from intersection results
	void ProcessTestResults(const Matrix34 & mCurrMtx, WorldProbe::ResultIterator & it, Vector3 & vObstacleCenter, float & fObstacleRadius); 

	// PURPOSE: Gets the bounding radius and center point of the component we have hit from an intersection
	// NOTES: Works in XZ space, as we are only adjusting heading
	void GetIntersectionBoundRadiusAndCenter(const Matrix34 & currMat, const u16 partIndex, const u16 componentIndex, const Matrix34 & boundMat, const phBound * pBound, float & fOutRadius, Vector3 & vOutCenter) const;

	// PURPOSE: Grows a sphere to encompass the new sphere
	// NOTES: Works in XZ space, as we are only adjusting heading
	void GrowSphere(const Vector3 & vNewCenter, const float & fNewRadius, Vector3 & vInOutCenter, float & fInOutRadius, bool isVehicle) const;

public:

	struct Tune
	{
		Tune();
		void AddWidgets(bkBank &b);

		// PURPOSE: Register self and internal (private) structures
		static void RegisterParser()
		{
			static bool bInitialized = false;
			if( !bInitialized )
			{
				REGISTER_PARSABLE_CLASS(hrsObstacleAvoidanceControl::Tune);
				bInitialized = true;
			}
		}

		PAR_SIMPLE_PARSABLE;
	};

	void AttachTuning(const Tune & t) { m_pTune = &t; }
	void DetachTuning() { m_pTune = 0; }
	const Tune & GetCurrTune() const { return *m_pTune; }

private:	

	//
	// Members
	//

	const Tune * m_pTune;
	float m_fCorrectedHeading;
	float m_fMinYaw;
	float m_fMaxYaw;
	float m_fBrake;
	float m_fSurfacePitch;
	float m_fHeadingScalar;
	float m_fPreviousHeading;

	Vector3 m_vCliffPrediction;
	Vector3 m_vUnderHorseNormal;


	CorrectionResults CorrectionResult;
	CorrectionResults LastNonFailCorrectionResult;

	CNavmeshLineOfSightHelper m_NavMeshLineOfSightHelper;	//NavMesh helper for cliff detection.

	// The insts that we have detected with our surface probes
	static const int NUM_PROBES = 4;
	phInst * m_pSurfaceInsts[NUM_PROBES];

	// Flags
	bool m_bAvoiding;
	bool m_bQuickStopping;
	bool m_NavMeshBreakRecentlyDetected;
	bool m_bCliffDetectedLastFrame;

	// Owner ped - not a RegdPed as it is owned by CPed
	CPed* m_pPed;

	// Statics
	static float MIN_ENABLE_SPEED;
	static float REJECT_RADIUS;
	static float VEHICLE_REJECT_RADIUS;

	static float WATER_SLOW_SLOPE_FACTOR_DOWN_1;
	static float WATER_SLOW_SLOPE_FACTOR_UP_1;
	static float WATER_SLOW_SLOPE_FACTOR_DOWN_2;
	static float WATER_SLOW_SLOPE_FACTOR_UP_2;
	static float WATER_SLOW_MAX_DOWN;
	static float WATER_SLOW_MIN_DEPTH_1;
	static float WATER_SLOW_DEPTH_FACTOR_1;
	static float WATER_SLOW_MIN_DEPTH_2;
	static float WATER_SLOW_DEPTH_FACTOR_2;

	// Debug statics
#if __BANK
	static float TEST_LENGTH_OVERRIDE;
	static float TEST_PITCH_OVERRIDE;
	static bool  IGNORE_MIN_SPEED;
#endif
};

#endif // ENABLE_HORSE

#endif // HRS_OBSTACLE_AVOIDANCE_H
