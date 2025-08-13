
#ifndef INC_CBUOYANCY_H
#define INC_CBUOYANCY_H

// used as a flag to indicate water level status at each calculation point
typedef enum
{
	NOT_IN_WATER,
	PARTIALLY_IN_WATER,
	FULLY_IN_WATER
} tWaterLevel;


// Game headers
#include "physics/WaterTestHelper.h"

// RAGE headers
#include "vector/vector3.h" 
#include "fragment/typechild.h"

class CPhysical;
class CEntity;

#define DEBUG_BUOYANCY (__DEV)

#define MAX_WATER_SAMPLES	(45)

#define DINGHY_BUOYANCY_MULT_FOR_GENERIC_SAMPLES (1.0f)

class CWaterSample
{
public:

	CWaterSample();
	~CWaterSample();

	void Set(CWaterSample &sample);

	// Vector offset of this sample from its parent
	Vector3 m_vSampleOffset;

	// Height of water sample
	float m_fSize;

	// Force multiplier for this sample
	float m_fBuoyancyMult;

	// Sample is offset relative to this component
	u16 m_nComponent;
    u8 m_bKeel : 1;
	u8 m_bPontoon : 1;
};


class CBuoyancyInfo
{
public:
	CBuoyancyInfo();
	CBuoyancyInfo(const CBuoyancyInfo& otherInfo) { CopyFrom(otherInfo); }
	~CBuoyancyInfo();

	CBuoyancyInfo* Clone();
	void CopyFrom(const CBuoyancyInfo& otherInfo);

	// Array of water samples
	CWaterSample* m_WaterSamples;	
	float m_fBuoyancyConstant;

	float m_fLiftMult;
	float m_fKeelMult;

	float m_fDragMultXY;
	float m_fDragMultZUp;
	float m_fDragMultZDown;

	s16 m_nNumWaterSamples;

	s8 m_nNumBoatWaterSampleRows;
	s8* m_nBoatWaterSampleRowIndices;			// the first water sample index in each row of boat water samples
};

// Structure to easily pass the force and torque accumulators for each water sample.
struct SForceAccumulator
{
	Vector3 vDragForce;
	Vector3 vDragTorque;

	Vector3 vDampingForce;
	Vector3 vDampingTorque;

	// Called once per CBuoyancy::Process() call before iterating over the water samples.
	void Clear()
	{
		vDragForce.Zero();
		vDragTorque.Zero();

		vDampingForce.Zero();
		vDampingTorque.Zero();
	}
};

class CBuoyancy
{
public:

	CBuoyancy();
	~CBuoyancy();

	void GetVehicleBBoxForBuoyancy(CPhysical *pParent, Vector3& vecBBoxMin, Vector3& vecBBoxMax);

	bool Process(CPhysical *pParent, float fTimeStep, bool bArticulatedBody, bool lastTimeSlice, float* pBuoyancyAccel=NULL);
	void ResetBuoyancy();

	__forceinline float ComputeCrossSectionalLengthForDrag(CPhysical* pParent);

	// These functions each compute a different force from the water on each water sample. Some apply the forces directly while
	// others accumulate their forces (should be obvious from the params which ones these are!) and apply them altogether
	// to avoid unrealistically high forces.
#if __DEBUG
	Vector3 ComputeSampleBuoyancyForce(float fTimeStep, CPhysical* pParent, float fMass, s32 nSample, s32 nComponent, const Vector3& vTestPointWorld,
		const Vector3& vWaterNormal, float fBuoyancyConstant, float fPartBuoyancyMultiplier, float fDepthFactor);
	void ComputeSampleLiftForce(float fTimeStep, CPhysical* pParent, float fMass, s32 nSample, s32 nComponent, const Vector3& vTestPointWorld,
		const Vector3& vWaterNormal, const Vector3& vSpeedAtPointIncFlow, float fWaterSpeedVert);
	void ComputeSampleKeelForce(float fTimeStep, CPhysical* pParent, float fMass, s32 nSample, s32 nComponent, const Vector3& vTestPointWorld,
		const Vector3& vLocalPosition, const Vector3& vWaterNormal, const Vector3& vSpeedAtPoint, const Vector3& vSpeedAtPointIncFlow,
		const Vector3& vLocalFlowVelocity);
	Vector3 ComputeSampleDragForce(CPhysical* pParent, SForceAccumulator* forceAccumulators, float fMass, s32 nSample, s32 nComponent,
		const Vector3& vTestPointWorld, const Vector3& vFlowVelocity, float fCrossSectionalLength, float fDragCoefficient);
	Vector3 ComputeSampleDampingForce(CPhysical* pParent, SForceAccumulator* forceAccumulators, float fMass, s32 nSample,
		s32 nComponent, const Vector3& vTestPointWorld, const Vector3& vSpeedAtPoint, float fPartBuoyancyMultiplier);
	void ComputeSampleStickToSurfaceForce(float fTimeStep, CPhysical* pParent, float fMass, s32 nSample, s32 nComponent, const Vector3& vTestPointWorld,
		const Vector3& vSpeedIncFlow);
#else
	__forceinline Vector3 ComputeSampleBuoyancyForce(float fTimeStep, CPhysical* pParent, float fMass, s32 nSample, s32 nComponent, const Vector3& vTestPointWorld,
		const Vector3& vWaterNormal, float fBuoyancyConstant, float fPartBuoyancyMultiplier, float fDepthFactor);
	__forceinline void ComputeSampleLiftForce(float fTimeStep, CPhysical* pParent, float fMass, s32 nSample, s32 nComponent, const Vector3& vTestPointWorld,
		const Vector3& vWaterNormal, const Vector3& vSpeedAtPointIncFlow, float fWaterSpeedVert);
	__forceinline void ComputeSampleKeelForce(float fTimeStep, CPhysical* pParent, float fMass, s32 nSample, s32 nComponent, const Vector3& vTestPointWorld,
		const Vector3& vLocalPosition, const Vector3& vWaterNormal, const Vector3& vSpeedAtPoint, const Vector3& vSpeedAtPointIncFlow,
		const Vector3& vLocalFlowVelocity);
	__forceinline Vector3 ComputeSampleDragForce(CPhysical* pParent, SForceAccumulator* forceAccumulators, float fMass, s32 nSample, s32 nComponent,
		const Vector3& vTestPointWorld, const Vector3& vFlowVelocity, float fCrossSectionalLength, float fDragCoefficient);
	__forceinline Vector3 ComputeSampleDampingForce(CPhysical* pParent, SForceAccumulator* forceAccumulators, float fMass, s32 nSample,
		s32 nComponent, const Vector3& vTestPointWorld, const Vector3& vSpeedAtPoint, float fPartBuoyancyMultiplier);
	__forceinline void ComputeSampleStickToSurfaceForce(float fTimeStep, CPhysical* pParent, float fMass, s32 nSample, s32 nComponent, const Vector3& vTestPointWorld,
		const Vector3& vSpeedIncFlow);
#endif // #if __DEBUG

	// Call this before using values computed in Process() like GetWaterLevelOnSample(), GetAvgAbsWaterLevel(), etc.
	bool GetBuoyancyInfoUpdatedThisFrame() const { return m_buoyancyFlags.bBuoyancyInfoUpToDate == 1; }

	void	SetStatusHighNDry(void) {m_waterLevelStatus = NOT_IN_WATER;}
	int		GetStatus() const {return m_waterLevelStatus;}
	void	SetStatus(int status) { m_waterLevelStatus = (tWaterLevel)status; }

	float	GetWaterLevelOnSample(int iSampleIndex) const;

	float	GetAbsWaterLevel() const {return m_fAbsWaterLevel;}
	float	GetAvgAbsWaterLevel() const {return m_fAvgAbsWaterLevel;}
	float	GetSubmergedLevel() const { return m_fSubmergedLevel; }

	// Low LOD buoyancy mode:
	bool CanUseLowLodBuoyancyMode(const CPhysical* pPhysical) const;
	void UpdateBuoyancyLodMode(CPhysical* pPhysical);
	void ProcessLowLodBuoyancy(CPhysical* pPhysical);
	void RejuvenateMatrixIfNecessary(Matrix34& matrix);
	void ComputeLowLodBuoyancyPlaneNormal(const CPhysical* pPhysical, const Matrix34& entMat, Vector3& vPlaneNormal);
	float GetBoatAnchorLodDistance() const { return ms_fBoatAnchorLodDistance; }
	float GetLowLodDraughtOffset() const { return m_fLowLODDraughtOffset; }
	void SetLowLodDraughtOffset(const float fOffset) { m_fLowLODDraughtOffset = fOffset; }
	const Vector3& GetVelocityWhenInLowLodMode() const { return m_vLowLodVelocityXYZLowLodTimerW; }
	float GetTimeInLowLodMode() const { return m_vLowLodVelocityXYZLowLodTimerW.w; }
	void SetTimeInLowLodMode(float fTimeInSeconds) { m_vLowLodVelocityXYZLowLodTimerW.w = fTimeInSeconds; }

	// Allows certain code to override the buoyancy info set up in the model info
	void SetBuoyancyInfoOverride(CBuoyancyInfo* pBuoyancyInfoOverride) { m_pBuoyancyInfoOverride = pBuoyancyInfoOverride; }
	CBuoyancyInfo* GetBuoyancyInfoOverride() const { return m_pBuoyancyInfoOverride; }

	// Returns m_pBuoyancyInfoOverride if valid, else returns the buoyancy info set up in the modelInfo
	CBuoyancyInfo* GetBuoyancyInfo(const CPhysical* pParent) const;

	inline void SetShouldStickToWaterSurface(bool bStickToWater) { m_buoyancyFlags.bShouldStickToWaterSurface = bStickToWater; }
	inline bool GetShouldStickToWaterSurface() { return m_buoyancyFlags.bShouldStickToWaterSurface; }

	inline void SetPedWeightBeltActive(bool set) { m_buoyancyFlags.bPedWeightBeltActive = set; }
	inline bool GetPedWeightBeltActive() { return m_buoyancyFlags.bPedWeightBeltActive; }

	const Vector3& GetLastRiverVelocity() const {return m_WaterTestHelper.GetLastRiverVelocity();}
	const Vector3& GetLastWaterNormal() const {return m_WaterTestHelper.GetLastWaterNormal();}

	bool GetWaterHelperRiverHitStored() const { return m_WaterTestHelper.GetRiverHitStored(); }

#if __WIN32PC
	float GetKeelSteerForce() const {return m_fKeelSideForce; }
#endif // __WIN32PC

public:
	void SubmitRiverBoundProbe(Vector3& vPos);
	bool GetRiverBoundProbeResult();
	bool GetRiverBoundProbeResult(Vector3& vRiverHitPos, Vector3& vRiverHitNormal);
	bool GetCachedRiverBoundProbeResult(Vector3& vRiverHitPos, Vector3& vRiverHitNormal) const;
	bool ForceUpdateRiverProbeResultSynchronously(Vector3& vPos);
	// Used for example, when warping.
	void InvalidateCachedRiverBoundProbeResult() { m_WaterTestHelper.InvalidateCachedRiverBoundProbeResult(); }

	// Internally calls Water::GetWaterLevel() and uses the cached river results to decide if the parent physical
	// is in a river or a Water block (the boolean return value). Water level is returned in pWaterZ if non-NULL.
	WaterTestResultType GetWaterLevelIncludingRivers(const Vector3& vPos, float* pWaterZ, bool bForceResult, float fPoolDepth, float fRejectionAboveWater,
		bool* pShouldPlaySeaSounds, const CPhysical* pContextEntity=NULL, bool bHighDetail = false) const;
	// Like the above call with the exception that it calls Water::GetWaterLevelNoWaves() instead of Water::GetWaterLevel().
	WaterTestResultType GetWaterLevelIncludingRiversNoWaves(const Vector3& vPos, float* pWaterZ, float fPoolDepth, float fRejectionAboveWater,
		bool* pShouldPlaySeaSounds, const CPhysical* pContextEntity=NULL);
	float GetRiverFlowVelocityScaleFactor() const { return m_fFlowVelocityScaleFactor; }
	static float GetDefaultRiverFlowVelocityScaleFactor() { return ms_fFlowVelocityScaleFactor; }

private:

#if __DEBUG
	void ClampResistiveForceAndTorque(float fTimeStep, const CPhysical* pParent, Vector3& vForce, Vector3& vTorque);
	// Decide upon a drag coefficient for this particular object.
	void SelectDragCoefficient(CPhysical* pParent);
#else
	__forceinline void ClampResistiveForceAndTorque(float fTimeStep, const CPhysical* pParent, Vector3& vForce, Vector3& vTorque);
	// Decide upon a drag coefficient for this particular object.
	__forceinline void SelectDragCoefficient(CPhysical* pParent);
#endif // #if __DEBUG

public:
#if __BANK
	friend class CPedDebugVisualiser;
	friend class CMovementTextDebugInfo;
	static void AddWidgetsToBank(bkBank& bank);
	static void DisplayBuoyancyInfo();
#endif // __BANK

#if DEBUG_BUOYANCY
	class CForceDebug
	{
	public:
		CForceDebug() : m_nNumTextLines(0), m_forceDebugColour(Color_yellow) {}
		
		char m_zForceDebugText[100];
		u32 m_nNumTextLines;
		Color32 m_forceDebugColour;
	};
	CForceDebug m_forceDebug;
#endif // DEBUG_BUOYANCY

private:
#if __PPU
	friend class CVehicleOffsetInfo;
#endif
	
	CWaterTestHelper m_WaterTestHelper;

	// Store the velocity computed for this object when in low LOD buoyancy mode.
	Vector3 m_vLowLodVelocityXYZLowLodTimerW;

	atMap<int, int> m_componentIndexToForceAccumMap;

	tWaterLevel m_waterLevelStatus;
	
	float* m_fSampleSubmergeLevel;
	
	float  m_fAbsWaterLevel;   // the highest water level on the CBuoyancy. It is not scaled by CBuoyancyInfo's size
	float  m_fAvgAbsWaterLevel; // the averaged level on the CBuoyancy. It is not scaled by CBuoyancyInfo's size
	float  m_fSubmergedLevel;
	
	// Allows certain code to override the buoyancy info set up in the ModelInfo
	CBuoyancyInfo* m_pBuoyancyInfoOverride;

	float m_fLowLODDraughtOffset;

#if DEBUG_BUOYANCY
	// Force and torque accumulators.
	float m_fTotalBuoyancyForce;
	float m_fTotalLiftForce;
	float m_fTotalKeelForce;
	float m_fTotalFlowDragForce;
	float m_fTotalBasicDragForce;
#endif // DEBUG_BUOYANCY

#if __WIN32PC
	float m_fKeelSideForce;
#endif // __WIN32PC

	rage::phInst* m_pInstForComponentMap; // Used to decide if we need to refresh the map.
	u16 m_nNumComponentsWithSamples;
	s16 m_nNumInSampleLevelArray;

public:
	float m_fForceMult;

private:
	/////////////////////////////////
	float m_fFlowVelocityScaleFactor;

	float m_fDragCoefficientInUse;


public:
	struct CBuoyancyFlags
	{
		u8 bLowLodBuoyancyMode			: 1;
		u8 bWasActiveWhenLodded			: 1;
		u8 bScriptLowLodOverride		: 1;
		u8 bPedWeightBeltActive			: 1;
		u8 bShouldStickToWaterSurface	: 1;
		u8 bUseWidestRiverBoundTolerance: 1;
		u8 bBuoyancyInfoUpToDate		: 1;
		u8 bReduceLateralBuoyancyForce	: 1;
	};
	CBuoyancyFlags m_buoyancyFlags;


#if __BANK
public:
	static bool ms_bDrawBuoyancyTests;
	static bool ms_bDrawBuoyancySampleSpheres;
	static bool ms_bDisplayBuoyancyForces;
	static bool ms_bDebugDisplaySubmergedLevels;
	static bool ms_bWriteBuoyancyForceToTTY;
	static s32 ms_nSampleToDisplay; // Set to "-1" to draw them all.

	// Buoyancy LOD.
	static bool ms_bIndicateLowLODBuoyancy;

	// Selectively disable the different buoyancy forces to help diagnose weird behaviour.
	static bool ms_bDisableBuoyancyForceXY;
	static bool ms_bDisableLiftForce;
	static bool ms_bDisableKeelForce;
	static bool ms_bDisableFlowInducedForce;
	static bool ms_bDisableDragForce;
	static bool ms_bDisableSurfaceStickForce;
	static bool ms_bDisablePedWeightBeltForce;

	// River debug:
	static bool ms_bVisualiseRiverFlowField;
	static bool ms_bDebugDrawCrossSection;
	static bool ms_bRiverBoundDebugDraw;
	static float ms_fGridSpacing;
	static float ms_fHeightOffset;
	static float ms_fScaleFactor;
	static int ms_nGridSizeX;
	static int ms_nGridSizeY;
#else // __BANK
private:
#endif // __BANK
	static float ms_fDragCoefficient;
    static float ms_fCarDragCoefficient;
    static float ms_fBikeDragCoefficient;
	static float ms_fPedDragCoefficient;
	static float ms_fProjectileDragCoefficient;
	static float ms_fFlowVelocityScaleFactor;
    static float ms_fVehicleMaximumSpeedToApplyRiverForces;
	static float ms_fFullKeelForceAtRudderLimit;
	static float ms_fKeelDragMult;
    static float ms_fKeelForceFactorSteerMultKeelSpheres;
	static float ms_fMinKeelForceFactor;
	static float ms_fKeelForceThrottleThreshold;
	static float ms_fMaxKeelRudderExclusionFraction;
	static float ms_fBoatAnchorLodDistance;
	static float ms_fObjectLodDistance;
};


__forceinline void CBuoyancy::SubmitRiverBoundProbe(Vector3& vPos)
{
	m_WaterTestHelper.SubmitAsyncRiverBoundProbe(vPos, m_buoyancyFlags.bUseWidestRiverBoundTolerance==1);
}

__forceinline bool CBuoyancy::GetRiverBoundProbeResult()
{
	return m_WaterTestHelper.GetAsyncRiverBoundProbeResult();
}

__forceinline bool CBuoyancy::GetRiverBoundProbeResult(Vector3& vRiverHitPos, Vector3& vRiverHitNormal)
{
	return m_WaterTestHelper.GetAsyncRiverBoundProbeResult(vRiverHitPos, vRiverHitNormal);
}

__forceinline bool CBuoyancy::GetCachedRiverBoundProbeResult(Vector3& vRiverHitPos, Vector3& vRiverHitNormal) const
{
	return m_WaterTestHelper.GetCachedRiverBoundProbeResult(vRiverHitPos, vRiverHitNormal);
}

__forceinline bool CBuoyancy::ForceUpdateRiverProbeResultSynchronously(Vector3& vPos)
{
	return m_WaterTestHelper.ForceUpdateRiverProbeResultSynchronously(vPos);
}

__forceinline WaterTestResultType CBuoyancy::GetWaterLevelIncludingRivers(const Vector3& vPos, float* pWaterZ, bool bForceResult, float fPoolDepth, float fRejectionAboveWater,
	bool* pShouldPlaySeaSounds, const CPhysical* pContextEntity, bool bHighDetail) const
{
	return m_WaterTestHelper.GetWaterLevelIncludingRivers(vPos, pWaterZ, bForceResult, fPoolDepth, fRejectionAboveWater, pShouldPlaySeaSounds,
		pContextEntity, bHighDetail);
}

__forceinline WaterTestResultType CBuoyancy::GetWaterLevelIncludingRiversNoWaves(const Vector3& vPos, float* pWaterZ, float fPoolDepth, float fRejectionAboveWater,
	bool* pShouldPlaySeaSounds, const CPhysical* pContextEntity)
{
	return m_WaterTestHelper.GetWaterLevelIncludingRiversNoWaves(vPos, pWaterZ, fPoolDepth, fRejectionAboveWater, pShouldPlaySeaSounds,
		pContextEntity);
}

#endif //INC_CBUOYANCY_H


