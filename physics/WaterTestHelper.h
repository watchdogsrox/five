//
// File: WaterTestHelper.h
// Started: 23/4/12
// Creator: Richard Archibald
// Purpose: Helper class to combine water related queries.
//
#ifndef WATERTESTHELPER_H_
#define WATERTESTHELPER_H_

// RAGE headers:

// Framework headers:

// Game headers:
#include "physics/WorldProbe/WorldProbe.h"
#include "task/System/AsyncProbeHelper.h"

enum WaterTestResultType
{
	WATERTEST_TYPE_NONE		= 0,
	WATERTEST_TYPE_OCEAN,
	WATERTEST_TYPE_RIVER
};

class CWaterTestHelper
{
	// TODO: Remove this friendship once the helper class's usage is settled.
	friend class CBuoyancy;

public:
	CWaterTestHelper();


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// These functions don't require an instance of CWaterTestHelper as they perform synchronous probes. There is some protection against
	// issuing wasteful probes but this just amounts to checking if the requested position is near a river.
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
private:
	// Perform a synchronous probe which only returns true if it hits water.
	static bool ProbeForRiver(const Vector3& vStartPos, const Vector3& vEndPos, WorldProbe::CShapeTestResults* pProbeResults=NULL);
public:

	// checks if it is near a river entity based
	static bool IsNearRiver(Vec3V_In vPos, bool bUseWidestToleranceBoundingBoxTest = false);
	// Performs a vertical probe for river bounds after first deciding if it should based on whether the horizontal position
	// is within a river entity.
	static bool GetRiverHeightAtPosition(const Vector3& vPos, float* pfRiverHeightAtPos=NULL, bool bUseWidestToleranceBoundingBoxTest = false);
	// Same as the function for rivers, but this version also consider ocean/lake water tech. Default behaviour is to
	// consider the wave disturbance when finding the height on oceans/lakes.
	static bool GetWaterHeightAtPositionIncludingRivers(const Vector3& vPos, float* pfWaterHeightAtPos=NULL, bool bConsiderWaves=true,
		float fRejectionAboveWaterOverride = -1.0f, bool bUseWidestToleranceBoundingBoxTest = false, bool bHighDetail=false);
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// These functions work with asynchronous probes and only fire off a new probe if the requested position is further
	// away from the last position (cached internally so these functions need an instance of this class) than some threshold.
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	void SubmitAsyncRiverBoundProbe(Vector3& vPos, bool bUseWidestToleranceBoundingBoxTest=false);
	bool GetAsyncRiverBoundProbeResult();
	bool GetAsyncRiverBoundProbeResult(Vector3& vRiverHitPos, Vector3& vRiverHitNormal);
	bool GetCachedRiverBoundProbeResult(Vector3& vRiverHitPos, Vector3& vRiverHitNormal) const;
	// Used for example, when warping.
	void InvalidateCachedRiverBoundProbeResult() { SetRiverHitStored(false); }
	// When an object is first created, it won't have a valid river hit. This function allows it to be filled in with a synchronous probe.
	bool ForceUpdateRiverProbeResultSynchronously(Vector3& vPos);
	// Internally calls Water::GetWaterLevel() and uses the cached river results to decide if the parent physical
	// is in a river or a Water block (the boolean return value). Water level is returned in pWaterZ if non-NULL.
	WaterTestResultType GetWaterLevelIncludingRivers(const Vector3& vPos, float* pWaterZ, bool bForceResult, float fPoolDepth, float fRejectionAboveWater,
		bool* pShouldPlaySeaSounds, const CPhysical* pContextEntity=NULL, bool bHighDetail=false) const;
	// Like the above call with the exception that it calls Water::GetWaterLevelNoWaves() instead of Water::GetWaterLevel().
	WaterTestResultType GetWaterLevelIncludingRiversNoWaves(const Vector3& vPos, float* pWaterZ, float fPoolDepth, float fRejectionAboveWater,
		bool* pShouldPlaySeaSounds, const CPhysical* pContextEntity=NULL);
	const Vector3& GetLastRiverVelocity() const {return m_vLastRiverVelocity;}
	const Vector3& GetLastWaterNormal() const {return m_vLastWaterNormal;}
	////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


private:
	void SetLastAsyncRiverProbePos(const Vector3& lastAsyncRiverProbePos) { m_vLastAsyncRiverProbePosXYZRiverHitStoredW.SetXYZ(RCC_VEC3V(lastAsyncRiverProbePos)); }
	const Vector3& GetLastAsyncRiverProbePos() const { return (const Vector3&)m_vLastAsyncRiverProbePosXYZRiverHitStoredW; }
	void SetRiverHitStored(bool riverHitStored) { m_vLastAsyncRiverProbePosXYZRiverHitStoredW.SetWi(riverHitStored ? 1 : 0); }
	bool GetRiverHitStored() const { return m_vLastAsyncRiverProbePosXYZRiverHitStoredW.GetWi() != 0; }

	// XYZ - vLastAsyncRiverProbePos - Store the last position for which an asynchronous river probe was requested. Used to determine whether we have moved far enough to
	// merit submitting a new one.
	// W - RiverHitStored - Are the cached results from last asynchronous river probe (above) valid?
	Vec4V m_vLastAsyncRiverProbePosXYZRiverHitStoredW;

	// Relevant intersection data from the last asynchronous probe which found a river.
	Vector3 m_vLastRiverHitNormal;
	Vector3 m_vLastRiverHitPos;
	Vector3 m_vLastRiverVelocity; //for ped application
	Vector3 m_vLastWaterNormal;

	// Use an asynchronous probe helper to manage the probes used to determine if an object is in a river and, if so, how deep.
	CAsyncProbeHelper m_probeTestHelper;

	// For debug widgets:
#if __BANK
public:
	static bool ms_bDisableRiverProbes;
#else // __BANK
private:
#endif // __BANK
	static float ms_fDistanceThresholdForRiverProbe;


	// Some internal constants:
	static const float ms_fProbeVerticalOffset;
};

__forceinline bool CWaterTestHelper::GetAsyncRiverBoundProbeResult()
{
	// Don't try to retrieve results for a non-existent probe.
	if(m_probeTestHelper.IsProbeActive())
	{
		// Retrieve the results from the river bound probe if available.
		ProbeStatus probeStatus;
		Vector3 vRiverHitPos, vRiverHitNormal;
		if(m_probeTestHelper.GetProbeResultsWithIntersectionAndNormal(probeStatus, &vRiverHitPos, &vRiverHitNormal))
		{
			physicsAssert(probeStatus==PS_Blocked || probeStatus==PS_Clear);

			// Store the fresh results. These will be reused next time if the next probe is late.
			SetRiverHitStored(probeStatus==PS_Blocked);
			m_vLastRiverHitPos = vRiverHitPos;
			m_vLastRiverHitNormal = vRiverHitNormal;

			return GetRiverHitStored();
		}
		else
		{
			// Probe not done yet. Just return the cached values.
			return GetRiverHitStored();
		}
	}
	else
	{
		// We are not waiting on any probe at the moment, just return the cached values.
		return GetRiverHitStored();
	}
}

#endif // WATERTESTHELPER_H_
