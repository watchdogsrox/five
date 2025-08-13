//
// task/climbdetector.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef CLIMB_DETECTOR_H
#define CLIMB_DETECTOR_H

// Forward declarations
namespace rage { class Color32; }
namespace rage { class Vector3; }

// Game headers
#include "debug/debugdrawstore.h"
#include "peds/ped.h"
#include "task/movement/climbing/climbdebug.h"
#include "task/movement/climbing/climbhandhold.h"

const u32			CLIMB_DETECTOR_MAX_INTERSECTIONS		= 4;
const u32			CLIMB_DETECTOR_MAX_HORIZ_TESTS			= 6;
const u32			CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS	= CLIMB_DETECTOR_MAX_HORIZ_TESTS * CLIMB_DETECTOR_MAX_INTERSECTIONS * 2;

struct sPedTestParams
{
	sPedTestParams() 
		: fParallelDot(0.0f)
		, fForwardDot(0.0f)
		, fStickDot(0.0f)
		, fVaultDistStanding(0.0f) 
		, fVaultDistStandingVehicle(0.0f) 
		, fVaultDistStepUp(0.0f)
		, fJumpVaultHeightThresholdMin(0.0f)
		, fJumpVaultHeightThresholdMax(0.0f)
		, fJumpVaultHeightAllowanceOnSlope(0.0f)
		, fJumpVaultDepthThresholdMin(0.0f)
		, fJumpVaultDepthThresholdMax(0.0f)
		, fJumpVaultMinHorizontalClearance(0.0f)
		, fJumpVaultMBRThreshold(0.0f)
		, fAutoJumpVaultDistMin(0.0f)
		, fAutoJumpVaultDistMax(0.0f)
		, fAutoJumpVaultDistCutoff(0.0f)
		, fMaxVaultDistance(0.0f)
		, fOverrideMinClimbHeight(-1.0f)
		, fOverrideMinClimbPedHeight(-1.0f)
		, fOverrideMaxClimbHeight(-1.0f)
		, fUseTestOrientationHeight(0.0f)
		, fCurrentMBR(0.0f)
		, fHorizontalTestDistanceOverride(-1.0f)
		, bCloneUseTestDirection(false)
		, fZClearanceAdjustment(0.0f)
		, bDoJumpVault(false) 
		, bOnlyDoJumpVault(false)
		, bDoStandingVault(true)
		, bShowStickIntent(true)
		, bDepthThresholdTestInsideRange(false)
		, bDo2SidedHeightTest(true)
		, bCanDoStandingAutoStepUp(true)
		, bForceSynchronousTest(false)
		, fStepUpDistanceCutoff(0.0f)
		, vCloneHandHoldPoint(Vector3::ZeroType)
		, vCloneHandHoldNormal(Vector3::ZeroType)
		, uCloneMinNumDepthTests(0)
	{}

	float fParallelDot;
	float fForwardDot;
	float fStickDot;

	float fVaultDistStanding;
	float fVaultDistStandingVehicle;
	float fVaultDistStepUp;

	float fJumpVaultHeightThresholdMin;
	float fJumpVaultHeightThresholdMax;
	float fJumpVaultHeightAllowanceOnSlope;
	float fJumpVaultDepthThresholdMin;
	float fJumpVaultDepthThresholdMax;
	float fJumpVaultMinHorizontalClearance;
	float fJumpVaultMBRThreshold;
	float fAutoJumpVaultDistMin;
	float fAutoJumpVaultDistMax;
	float fAutoJumpVaultDistCutoff;

	float fMaxVaultDistance;

	float fOverrideMinClimbHeight;
	float fOverrideMinClimbPedHeight;
	float fOverrideMaxClimbHeight;

	float fUseTestOrientationHeight;

	float fCurrentMBR;

	float fHorizontalTestDistanceOverride;

	float fStepUpDistanceCutoff;

	Vector3 vCloneHandHoldPoint;
	Vector3 vCloneHandHoldNormal;
	u8		uCloneMinNumDepthTests;//force the clone to do the same number of tests that were done on the remote to ensure gaps are ignored locally that were not seen on the remote

	float fZClearanceAdjustment;

	bool bCloneUseTestDirection;
	bool bDoJumpVault;
	bool bDoStandingVault;
	bool bOnlyDoJumpVault;
	bool bShowStickIntent;
	bool bDepthThresholdTestInsideRange;
	bool bDo2SidedHeightTest;
	bool bForceSynchronousTest;
	bool bCanDoStandingAutoStepUp;
};

struct sPedTestResults
{
	sPedTestResults() : m_iResultFlags(0) {}

	enum ePedTestResult
	{
		ePTR_CanVault			= BIT0,
		ePTR_GoingToVaultSoon	= BIT1,
		ePTR_CanJumpVault		= BIT2,
		ePTR_CanAutoJumpVault	= BIT3,
	};

	void SetCanVault(bool bVal)			{ m_iResultFlags.ChangeFlag(ePTR_CanVault, bVal); }
	void SetGoingToVaultSoon(bool bVal)	{ m_iResultFlags.ChangeFlag(ePTR_GoingToVaultSoon, bVal); }
	void SetCanJumpVault(bool bVal)		{ m_iResultFlags.ChangeFlag(ePTR_CanJumpVault, bVal); }
	void SetCanAutoJumpVault(bool bVal)	{ m_iResultFlags.ChangeFlag(ePTR_CanAutoJumpVault, bVal); }

	bool GetCanVault() const			{ return m_iResultFlags.IsFlagSet(ePTR_CanVault); }
	bool GetGoingToVaultSoon() const	{ return m_iResultFlags.IsFlagSet(ePTR_GoingToVaultSoon); }
	bool GetCanJumpVault() const		{ return m_iResultFlags.IsFlagSet(ePTR_CanJumpVault); }
	bool GetCanAutoJumpVault() const	{ return m_iResultFlags.IsFlagSet(ePTR_CanAutoJumpVault); }

	void Reset() { m_iResultFlags.ClearAllFlags(); }

	fwFlags8 m_iResultFlags;

	bool IsValid() const
	{
		return m_iResultFlags > 0;
	}
};

class CClimbDetector
{
	BANK_ONLY(friend CClimbDebug);
public:

	enum eSearchResult
	{
		eSR_Failed = 0,
		eSR_Deferred,
		eSR_Succeeded,
		eSR_ResubmitSynchronously,
	};

	CClimbDetector(CPed& ped);

	// PURPOSE: Update our current detected hand hold
	void Process(Matrix34 *overRideMtx = NULL, sPedTestParams *pPedParams = NULL, sPedTestResults *pPedResults = NULL, bool bDoClimbingCheck = true);

	// PURPOSE: Get the hand hold detected as part of Process
	bool GetDetectedHandHold(CClimbHandHoldDetected& handHold) const;

	// PURPOSE: Get the last hand hold time detected.
	u32 GetLastDetectedHandHoldTime() const;

	// PURPOSE: Returns true if we have a detected handhold and it was the result of an auto vault.
	bool IsDetectedHandholdFromAutoVault() const;

	// PURPOSE: Returns true if we should orient player to handhold.
	bool CanOrientToDetectedHandhold() const { return m_flags.IsFlagSet(VF_OrientToHandhold); }

	// PURPOSE: Scan for vertical edges in front of us - this is an on demand scan
	eSearchResult SearchForHandHold(const Matrix34& testMat, CClimbHandHoldDetected& outHandHold, sPedTestParams *pPedParams = NULL, sPedTestResults *pPedResults = NULL);

	// PURPOSE: used by clones to directly calculate the handhold from a given point and normal that is expected to correspond to a known existing handhold on a vehicle or geometry
	eSearchResult GetCloneVaultHandHold(const Matrix34& testMat, sPedTestParams *pPedParams );

	// PURPOSE: Reset climb flags.
	void ResetFlags();
	void ResetPendingResults();

	// PURPOSE: Reset delay timers (necessary if we don't process the climb detector during a frame).
	void ResetAutoVaultDelayTimers();

	// PURPOSE: Reuse intersection tests to determine if we should block player jump.
	bool IsJumpBlocked() const { return m_bBlockJump; }

#if __BANK
	// PURPOSE: Debug rendering
	static void Debug();

	// PURPOSE: Access to debug draw store
	static CDebugDrawStore& GetDebugDrawStore();
#endif // __BANK

private:
	
	// PURPOSE: Storage for a set of intersections that identity an edge
	struct sEdge
	{
		sEdge() : iStart(0), iEnd(0), vMidPoint(Vector3::ZeroType), fHeading(0.0f), fScore(0.0f), vNormals(Vector3::ZeroType) {}
		sEdge(const s32 iStart, const s32 iEnd, const Vector3& vMidPoint, const f32 fHeading, const f32 fScore, const Vector3& vNormal) : iStart(iStart), iEnd(iEnd), vMidPoint(vMidPoint), fHeading(fHeading), fScore(fScore), vNormals(vNormal), nNumNormals(1) {}

		// PURPOSE: Function for sorting edges
		static s32 PairSort(const sEdge* a, const sEdge* b) { return a->fScore < b->fScore ? -1 : 1; }

		// PURPOSE: Starting intersection index
		s32 iStart;

		// PURPOSE: Ending intersection index
		s32 iEnd;

		// PURPOSE: Mid point of edge
		Vector3 vMidPoint;

		// PURPOSE: Heading
		f32 fHeading;

		// PURPOSE: Used to determine best edge
		f32 fScore;

		// PURPOSE: Normals associated with this edge.
		Vector3 vNormals;
		int nNumNormals;
	};

	struct sShapeTestResult
	{
		sShapeTestResult();
		~sShapeTestResult();

		void Reset();

		WorldProbe::CShapeTestResults *pProbeResult;
#if __BANK
		u32 iFrameDelay;
#endif
	};

	enum eAsyncTestPhaseState
	{
		eATS_InProgress = 0,
		eATS_Free,
	};

	enum eHandholdTest
	{
		eHT_Main = 0,
		eHT_Adjacent1,
		eHT_Adjacent2,
		eHT_NumTests,
	};

	// PURPOSE: Determine the exact hand hold position
	bool ComputeHandHoldPoint(const Matrix34& testMat, 
		const WorldProbe::CShapeTestHitPoint& intersection, 
		const f32 fStartHeight, 
		const f32 fEndHeight, 
		const Vector3 &vAvgIntersectionNormal,
		sPedTestParams *pPedParams, 
		WorldProbe::CShapeTestHitPoint (&outIntersection)[eHT_NumTests]) const;

	bool GetCloneVaultHandHoldPoint(sPedTestParams *pPedParams, WorldProbe::CShapeTestHitPoint (&outIntersection)[eHT_NumTests]) const;

	bool DoValidationTestInDirection(const Matrix34& testMat, 
		int iEdge,
		const Vector3 &vTestDirection,
		CClimbHandHoldDetected& outHandHold, 
		sPedTestParams *pPedParams, 
		sPedTestResults *pPedResults, 
		const float (&minIntesectionDistances)[CLIMB_DETECTOR_MAX_HORIZ_TESTS],
		const WorldProbe::CShapeTestHitPoint *pIntersections[CLIMB_DETECTOR_MAX_HORIZ_TESTS]);

	// PURPOSE: Once the climb point is found, validate it and determine the space around the hand hold
	bool ValidateHandHoldAndDetermineVolume(const Matrix34& testMat, 
		const Vector3& vSurfaceNormal, 
		CClimbHandHoldDetected& inOutHandHold, 
		sPedTestParams *pPedParams, 
		sPedTestResults *pPedResults, 
		const float (&minIntesectionDistances)[CLIMB_DETECTOR_MAX_HORIZ_TESTS],
		const WorldProbe::CShapeTestHitPoint *pIntersections[CLIMB_DETECTOR_MAX_HORIZ_TESTS]);

	// PURPOSE. Helper function to get correct intersection from a batch test (as we have seperate intersections for sphere and capsule test).
	bool GetSweptSphereIntersection(const WorldProbe::CShapeTestHitPoint& intersectionSphere, const WorldProbe::CShapeTestHitPoint& intersectionSweptSphere, WorldProbe::CShapeTestHitPoint &outIntersection) const;

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Wraps the capsule test
	s32 TestCapsule(const Vector3& vP0, const Vector3& vP1, const f32 fRadius, WorldProbe::CShapeTestHitPoint* pIntersections, const u32 uNumIntersections, const bool bDoInitialSphereTest = true) const;
	bool NetworkSetExcludedSpectatorPlayerEntities(WorldProbe::CShapeTestCapsuleDesc & capsuleDesc ) const;

	// PURPOSE: Wraps tests (line and capsule versions) with optional async.
	bool TestLineAsync(const Vector3& vP0, const Vector3& vP1, sShapeTestResult &result, u32 nMaxResults, bool bAsync) const;
	bool TestCapsuleAsync(const Vector3& vP0, const Vector3& vP1, const f32 fRadius, sShapeTestResult &result, u32 nMaxResults, bool bAsync) const;

	// PURPOSE: Perform a capsule test offset along the direction vector by the specified amount
	s32 TestOffsetCapsule(const Vector3& vP0, const Vector3& vP1, const f32 fRadius, WorldProbe::CShapeTestHitPoint* pIntersections, const u32 uNumIntersections, const Vector3& vDir, const f32 fOffset, const bool bDoInitialSphereTest = true) const;

	// PURPOSE: Gets an entity from an intersection. Can return NULL.
	const CEntity *GetEntityFromIntersection(const WorldProbe::CShapeTestHitPoint& intersection) const;

	// PURPOSE: Test whether the intersection entity is an invalid door?
	bool IsIntersectionEntityAnUnClimbableDoor(const CEntity* pEntity, const WorldProbe::CShapeTestHitPoint& intersection, bool bTryingToAttachToDoor = false, bool bInvalidatingClimb = false) const;

	// PURPOSE: Check the passed in intersection is climbable
	bool DoesIntersectionInvalidateClimb(const WorldProbe::CShapeTestHitPoint& intersection) const;

	// PURPOSE: Check the passed in intersection is climbable
	bool GetIsValidIntersection(const WorldProbe::CShapeTestHitPoint& intersection) const;

	bool FindFirstValidIntersection(const WorldProbe::CShapeTestHitPoint* pIntersection, u32 uNumIntersection, WorldProbe::CShapeTestHitPoint& validIntersection) const;

	// PURPOSE: Check if we are going to climb on a small or pushable object.
	bool IsClimbingSmallOrPushableObject(const CPhysical *pPhysical, const phInst* pHitInst) const;

	// PURPOSE: Check edge is valid.
	bool GetIsEdgeValid(const sEdge& edge, const WorldProbe::CShapeTestHitPoint* pIntersectionStart, const WorldProbe::CShapeTestHitPoint* pIntersectionEnd) const;

	// PURPOSE: Check an intersection is not on a climbable slope.
	bool IsIntersectionOnSlope(const Matrix34& testMat, const WorldProbe::CShapeTestHitPoint& intersection, const Vector3 &vSlopeNormalAVG) const;

	// PURPOSE: Check handhold is not on a slope.
	bool IsHandholdOnSlope(const Matrix34& testMat, const Vector3& vSurfaceNormal, const CClimbHandHoldDetected& inHandHold) const;
	bool DoSlopeTest(const Matrix34& testMat, const Vector3& vSurfaceNormal, const CClimbHandHoldDetected& inHandHold, const Vector3 &vTestNormal, float fDistToLineThreshold) const;

	// PURPOSE: Do ped validation tests (i.e. angle/distance etc) before validation.
	bool DoPedHandholdTest(const Matrix34& testMat, CClimbHandHoldDetected& handHold, sPedTestParams *pPedParams, sPedTestResults *pPedResults);

	// PURPOSE: Set climb height. Returns false if climb height is invalid.
	bool SetClimbHeight(const Matrix34& testMat, CClimbHandHoldDetected& inOutHandHold, sPedTestParams *pPedParams);

	// PURPOSE: Attempt to adjust handhold point.
	void AdjustHandholdPoint(const Matrix34& testMat, CClimbHandHoldDetected& inOutHandHold, const WorldProbe::CShapeTestHitPoint& adjacent1, const WorldProbe::CShapeTestHitPoint& adjacent2);

	// PURPOSE: Set physical entity we are climbing on.
	bool SetClimbPhysical(CClimbHandHoldDetected& inOutHandHold, WorldProbe::CShapeTestHitPoint& mainHandhold);

	// PURPOSE: Update Async Status (lazily allocates results buffer here). Also handles debug switching.
	bool UpdateAsyncStatus();

	// PURPOSE: The matrix that our tests are based around
	void GetMatrix(Matrix34& m) const;

	// PURPOSE: Get ground position of testing ped.
	Vector3 GetPedGroundPosition(const Matrix34& testMat) const;

	// PURPOSE: Get player ped (test ped might be a horse, so this handles that scenario).
	CPed *GetPlayerPed() const;

	// PURPOSE: Determine if this a quadruped test as this affects how we process the climb detection.
	bool IsQuadrupedTest() const;

	// PURPOSE: Process any delay in autovault detection when on/off stairs.
	void ProcessAutoVaultStairDelay();

	// PURPOSE: process any delays with climbing CPhysical's.  
	void ProcessAutoVaultPhysicalDelay(CPhysical *pPhysical);

	// PURPOSE: Set delay timers, so that we can add a cooldown delay to auto-vaulting.
	void SetAutoVaultCoolownDelay(u32 uDelayMS, u32 uDelayEndMs);

	// PURPOSE: Set delay timers, so that add a delay to climbing a particular entity.
	void SetAutoVaultPhysicalDelay(CPhysical *pPhysical, u32 uDelayEndMs);

	// PURPOSE: Returns true if we can't auto-vault this frame due to a self imposed delay.
	bool IsDoingAutoVaultCooldownDelay() const;

	// PURPOSE: Returns true if we can't auto-vault this frame due to a self imposed delay.
	bool IsDoingAutoVaultPhysicalDelay() const;

	// PURPOSE: Select best front material for climb.
	bool SelectBestFrontSurfaceMaterial(CClimbHandHoldDetected& outHandHold, 
		const WorldProbe::CShapeTestHitPoint *pIntersections[CLIMB_DETECTOR_MAX_HORIZ_TESTS],
		const WorldProbe::CShapeTestHitPoint *pStartIntersection);

	// PURPOSE: Do Arch test as we disallow climb in certain cases.
	bool IsGoingUnderArch(const Matrix34& testMat, 
		const CClimbHandHoldDetected& inHandHold, 
		const float (&minIntesectionDistances)[CLIMB_DETECTOR_MAX_HORIZ_TESTS],
		const WorldProbe::CShapeTestHitPoint *pIntersections[CLIMB_DETECTOR_MAX_HORIZ_TESTS]) const;

	// PURPOSE: Calculate the climbing angle (used to pick different anims to prevent penetration).
	bool CalculateClimbAngle(const Matrix34& testMat,
		CClimbHandHoldDetected& inOutHandHold, 
		const float (&minIntesectionDistances)[CLIMB_DETECTOR_MAX_HORIZ_TESTS],
		const WorldProbe::CShapeTestHitPoint *pIntersections[CLIMB_DETECTOR_MAX_HORIZ_TESTS]);

	void DoBlockJumpTest(const Matrix34& testMat, const WorldProbe::CShapeTestHitPoint *pIntersections[CLIMB_DETECTOR_MAX_HORIZ_TESTS]);

	float GetMaxHeightDropAllowance() const;

	float GetMaxClimbHeight(sPedTestParams *pPedParams) const;
	float GetMinClimbHeight(sPedTestParams *pPedParams, bool bClimbingPed) const;

#if __BANK
	// PURPOSE: Is debug draw enabled?
	bool GetDebugEnabled() const;

	// PURPOSE: Control whether debug drawing is enabled, and set the draw colour
	void SetDebugEnabled(const bool bEnabled, const Color32& colour = Color_white) const;

	// PURPOSE: Draw intersections
	void AddDebugIntersections(const WorldProbe::CShapeTestHitPoint* pIntersections, const u32 uNumIntersections, const Color32 debugColour) const;
#endif // __BANK

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Owner CPed
	CPed& m_Ped;

	// PURPOSE: Flags
	enum eDetectorFlags
	{
		DF_DetectedHandHold			= BIT0,
		DF_AutoVault				= BIT1,
		DF_JumpVaultTestOnly		= BIT2,
		DF_OnStairs					= BIT3,
		DF_ForceSynchronous			= BIT4,
		VF_OrientToHandhold			= BIT5,
		VF_DoMaxHeightDropCheck		= BIT6,
		VF_DisableAutoVaultDelay	= BIT7,
	};

	// PURPOSE: Detected hand hold as part of Process
	CClimbHandHoldDetected m_handHoldDetected;

	sShapeTestResult m_HorizontalTestResults[CLIMB_DETECTOR_MAX_HORIZ_TESTS];
	eAsyncTestPhaseState m_eHorizontalTestPhase; 

	RegdPhy m_pLastClimbPhysical;
	u32 m_uAutoVaultPhysicalDelayEndTime;

	u32 m_uLastTimeDetectedHandhold;
	u32 m_uLastOnStairsTime;
	u32 m_uAutoVaultDetectedStartTimeMs;
	u32 m_uAutoVaultCooldownDelayMs;
	u32 m_uAutoVaultCooldownEndTimeMs;

	bool m_bDoHorizontalLineTests : 1;
	bool m_bDoAsyncTests : 1;
	bool m_bBlockJump : 1;

	// PURPOSE: Flags
	fwFlags8 m_flags;

#if __BANK
	// PURPOSE: Debug drawing modes
	static bank_bool ms_bDebugDrawSearch;
	static bank_bool ms_bDebugDrawIntersections;
	static bank_bool ms_bDebugDrawCompute;
	static bank_bool ms_bDebugDrawValidate;

	// PURPOSE: Debug draw
	static bank_bool ms_bDebugDraw;

	// PURPOSE: Debug rendering colour
	static Color32 ms_debugDrawColour;

	// PURPOSE: Debug draw storage
	static CDebugDrawStore* ms_pDebugDrawStore;

	// PURPOSE: Apply a orientation to our detector tests
	static bank_float ms_fHeadingModifier;
#endif // __BANK
};

////////////////////////////////////////////////////////////////////////////////

#if __BANK
inline CDebugDrawStore& CClimbDetector::GetDebugDrawStore()
{
	if(!ms_pDebugDrawStore)
	{
		ms_pDebugDrawStore = rage_new CDebugDrawStore(256);
	}
	return *ms_pDebugDrawStore;
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

#endif // CLIMB_DETECTOR_H
