//
// Task/DropDownDetector.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef DROPDOWN_DETECTOR_H
#define DROPDOWN_DETECTOR_H

#include "debug/debugdrawstore.h"
#include "physics/WorldProbe/worldprobe.h"

////////////////////////////////////////////////////////////////////////////////

class CPed;

enum eDropType
{
	eWalkDrop = 0,
	eRunningDrop,
	eAutoJump,
	eRagdollDrop,
	eDropMax,
};

class CDropDown
{
public:
	CDropDown() { Reset(); }
	
	void Reset()
	{
		m_vDropDownStartPoint.Zero();
		m_vDropDownEdge.Zero();
		m_vDropDownLand.Zero();
		m_vDropDownHeading.Zero();
		m_eDropType = eDropMax;
		m_bSmallDrop = false;
		m_bLeftHandDrop = false;
		m_fDropHeight = 0.0f;
		m_pPhysical = NULL;
	}

	//Helper for serialising if attached object can't be network
	void ClearPhysicalAndConvertToAbsolute();

	Vector3 GetDropDownStartPoint() const;
	Vector3 GetDropDownEdge() const;
	Vector3 GetDropDownLand() const;
	Vector3 GetDropDownHeading() const;

	CPhysical *GetPhysical() const { return m_pPhysical; }

	Vector3 m_vDropDownStartPoint;
	Vector3 m_vDropDownEdge;
	Vector3 m_vDropDownLand;
	Vector3 m_vDropDownHeading;
	RegdPhy m_pPhysical;
	float m_fDropHeight;
	bool m_bSmallDrop;
	bool m_bLeftHandDrop;
	eDropType m_eDropType;
};

////////////////////////////////////////////////////////////////////////////////

class CDropDownDetector
{
public:

	struct CDropDownDetectorParams
	{
		float m_fDropDownDepthRadius;
		float m_fDropDownMinHeight;
		float m_fHeightToDropDownCutoff;
		float m_fSmallDropDownHeight;
		float m_fDropDownStartZOffset;
		float m_fDropDownStartForwardOffset;
		float m_fDropDownSlopeDotMax;
		float m_fDropDownSlopeDotMinDirectionCutoff;
		float m_fDropDownPointMinZDot;
		float m_fTestHeightDifference;
		float m_fSlantedSlopeDot;
		bool m_bPerformControlTest;

		float m_fDropDownEdgeTestRadius;

		float m_fDropDownBestDistanceDotMax;
		float m_fDropDownBestDistanceDotMin;

		float m_fDropDownRunMaxDistance;
		float m_fDropDownRunMinDistance;
		float m_fDropDownRunBestDistanceMax;
		float m_fDropDownRunBestDistanceMin;

		float m_fDropDownWalkMaxDistance;
		float m_fDropDownWalkMinDistance;
		float m_fDropDownWalkBestDistanceMax;
		float m_fDropDownWalkBestDistanceMin;

		float m_fDropDownSlopeTolerance;

		float m_fDropDownClearanceTestZOffset;
		float m_fDropDownClearanceTestRadius;

		float m_fDropDownEdgeThresholdDot;

		float m_fAutoJumpMaxDistance;
		float m_fAutoJumpMinDistance;

		float m_fSmallDropDownWalkMaxDistance;
		float m_fSmallDropDownWalkMinDistance;
		float m_fSmallDropDownWalkBestDistanceMax;
		float m_fSmallDropDownWalkBestDistanceMin;

		float m_fSmallDropDownRunMaxDistance;
		float m_fSmallDropDownRunMinDistance;
		float m_fSmallDropDownRunBestDistanceMax;
		float m_fSmallDropDownRunBestDistanceMin;

		float m_fMidpointTestRadius;
		float m_fMidpointTestCutoff;

		float m_fBiasRightSideHeading;

		float m_fGapDetectionDistance;

		CDropDownDetectorParams();
	};

	enum eSearchResult
	{
		eSR_Failed = 0,
		eSR_Deferred,
		eSR_Succeeded,
	};

	enum eAsyncTestPhaseState
	{
		eATS_InProgress = 0,
		eATS_Free,
	};

	// PURPOSE: Search Flags
	enum eDetectorSearchFlags
	{
		DSF_AsyncTests				= BIT0,
		DSF_TestForPararachuteJump	= BIT1,
		DSF_TestForDive				= BIT2,
		DSF_PedRunning				= BIT3,
		DSF_FromVault				= BIT4,
		DSF_BiasRightSide			= BIT5,
		DSF_TestForMount			= BIT6,
		DSF_TestForRagdollTeeter	= BIT7,
	};

	// PURPOSE: Result Flags
	enum eDetectorResultFlags
	{
		DRF_DropDownDetected		= BIT0,
		DRF_ParachuteDropDetected   = BIT1,	
		DRF_DiveDetected			= BIT2,	
		DRF_MountDetected			= BIT3
	};

	CDropDownDetector(CPed& ped);
	~CDropDownDetector();

	// PURPOSE: Run drop detector in full.
	void Process(Matrix34 *overRideMtx = NULL, CDropDownDetectorParams *pPedParams = NULL, u8 nFlags = DSF_AsyncTests);

	// PURPOSE: Scan for vertical edges in front of us - this is an on demand scan
	eSearchResult SearchForDropDown(const Matrix34& testMat, CDropDownDetectorParams& dropDownDetectorParams, CDropDown& outDropDown);

	// PURPOSE: The matrix that our tests are based around
	void GetMatrix(Matrix34& m) const;

	// PURPOSE: Reset results. Necessary for async probes.
	void ResetPendingResults();

	// PURPOSE: Get results of dropdown detector.
	bool HasDetectedDropDown() const { return m_nResultFlags.IsFlagSet(DRF_DropDownDetected); }
	bool HasDetectedParachuteDrop() const { return m_nResultFlags.IsFlagSet(DRF_ParachuteDropDetected); }
	bool HasDetectedDive() const { return m_nResultFlags.IsFlagSet(DRF_DiveDetected); }
	bool HasDetectedMount() const { return m_nResultFlags.IsFlagSet(DRF_MountDetected); }
	const CDropDown &GetDetectedDropDown() { return m_DetectedDropDown; }

#if __BANK
	
	// PURPOSE: Debug rendering
	static void RenderDebug();

	// PURPOSE: Access to debug draw store
	static CDebugDrawStore& GetDebugDrawStore();

	static bool GetDebugEnabled() { return ms_bDebugDraw; }

	// Setup the RAG widgets for climbing
	static void SetupWidgets(bkBank& bank);

	// PURPOSE: Draw intersections
	void AddDebugIntersections(const WorldProbe::CShapeTestHitPoint* pIntersections, const u32 uNumIntersections, const Color32 debugColour) const;

#endif // __BANK

private:

	// PURPOSE: Is Intersection valid?
	bool GetIsValidIntersection(const WorldProbe::CShapeTestHitPoint& intersection) const;

	// PURPOSE: Do an async capsule test.
	bool TestCapsuleAsync(const Vector3& vStart, const Vector3& vStartOffset, const Vector3& vEndOffset, const f32 fRadius, bool bAsync) const;
	bool TestCapsule(const Vector3& vP0, const Vector3& vP1, const f32 fRadius, WorldProbe::CShapeTestHitPoint &hitPoint, const bool bDoInitialSphereTest) const;

	// PURPOSE: Create/Destroy probe results.
	void CreateProbeResults();
	void DeleteProbeResults();

	// PURPOSE: Validate the drop down.
	bool ValidateDropDown(const Matrix34& testMat, 
		const WorldProbe::CShapeTestHitPoint& intersectionDrop, 
		const WorldProbe::CShapeTestHitPoint& intersectionLand, 
		float fDropDownDepthRadius,
		float fMaxDropDownHeight,
		const CDropDownDetectorParams& dropDownDetectorParams,
		CDropDown& outDropDown);

	bool IsTestingForAutoJump() const;

	bool IsTestingForRagdollTeeter() const;

	// PURPOSE: Owner CPed
	CPed& m_Ped;

	// PURPOSE: position of drop down.
	CDropDown m_DetectedDropDown;

	// For Async purposes, we cache heading we used for async tests, so that we can re-use it for further tests afterwards.
	Vector3 m_vCachedTestHeading;

	// PURPOSE: Async probes for drop down test.
	static const u32 ms_MaxDropDownTests = 4;
	WorldProbe::CShapeTestResults *m_pProbeResult[ms_MaxDropDownTests];
	eAsyncTestPhaseState m_eTestPhase; 
	
	// PURPOSE: Flags
	fwFlags8 m_nSearchFlags;
	fwFlags8 m_nResultFlags;

#if __BANK

	Vector3 m_vDebugAsyncStartPos[ms_MaxDropDownTests];
	Vector3 m_vDebugAsyncEndPos[ms_MaxDropDownTests];

	// PURPOSE: Debug draw storage
	static bank_bool ms_bDebugDraw;
	static Color32 ms_debugDrawColour;
	static CDebugDrawStore* ms_pDebugDrawStore;
#endif
};

////////////////////////////////////////////////////////////////////////////////

#if __BANK
inline CDebugDrawStore& CDropDownDetector::GetDebugDrawStore()
{
	if(!ms_pDebugDrawStore)
	{
		ms_pDebugDrawStore = rage_new CDebugDrawStore(256);
	}
	return *ms_pDebugDrawStore;
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

#endif // DROPDOWN_DETECTOR_H
