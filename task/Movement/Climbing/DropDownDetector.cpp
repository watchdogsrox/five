//
// Task/DropDownDetector.cpp
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

// File header
#include "Task/Movement/Climbing/DropDownDetector.h"

#include "math/angmath.h"

// Game headers
#include "Peds/Ped.h"
#include "peds/PedCapsule.h"
#include "system/control.h"
#include "camera/caminterface.h"
#include "Task/Movement/Climbing/TaskDropDown.h"
#include "Task/Movement/Jumping/TaskJump.h"
#include "Task/Motion/Locomotion/TaskInWater.h"
#include "Task/Movement/TaskParachute.h"
#include "Task/Movement/TaskFall.h"

AI_OPTIMISATIONS()
AI_MOVEMENT_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

PF_PAGE(GTADropDownDetector, "GTA drop down detector");
PF_GROUP(DropDownDetector);
PF_LINK(GTADropDownDetector, DropDownDetector);

PF_TIMER(DropDownProcess, DropDownDetector);
PF_TIMER(DropDownSearch, DropDownDetector);
PF_TIMER(DropDownValidate, DropDownDetector);
PF_TIMER(DropDownCapsuleTest, DropDownDetector);

// Debug initialization
#if __BANK
bank_bool			CDropDownDetector::ms_bDebugDraw			= false;
CDebugDrawStore*	CDropDownDetector::ms_pDebugDrawStore		= NULL;
Color32				CDropDownDetector::ms_debugDrawColour		= Color_green;

dev_u32				DROPDOWN_DETECTOR_DEBUG_EXPIRY_TIME			= 1000;

#endif // __BANK

// Constants
const u32			DROPDOWN_DETECTOR_TEST_FLAGS	= ArchetypeFlags::GTA_MAP_TYPE_MOVER	| 
																ArchetypeFlags::GTA_GLASS_TYPE		| 
																ArchetypeFlags::GTA_VEHICLE_TYPE	| 
																ArchetypeFlags::GTA_OBJECT_TYPE		|
																ArchetypeFlags::GTA_PED_TYPE;

////////////////////////////////////////////////////////////////////////////////

CDropDownDetector::CDropDownDetectorParams::CDropDownDetectorParams()
{
	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownDepthRadius, 0.3f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownMinHeight, 1.25f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(DROPDOWN, fHeightToDropDownCutoff, 0.5f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(DROPDOWN, fSmallDropDownHeight, 0.0f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownStartZOffset, 1.0f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownStartForwardOffset, 0.35f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownSlopeDotMax, 0.4f, -1.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownSlopeDotMinDirectionCutoff, -0.65f, -1.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownPointMinZDot, 0.4f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(DROPDOWN, fTestHeightDifference, 0.25f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(DROPDOWN, fSlantedSlopeDot, 0.875f, 0.0f, 1.0f, 0.01f);

	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownEdgeTestRadius, 0.2f, 0.0f, 1.0f, 0.01f);

	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownBestDistanceDotMax, 0.95f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownBestDistanceDotMin, 0.65f, 0.0f, 10.0f, 0.1f);

	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownRunMaxDistance, 2.0f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownRunMinDistance, 0.75f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownRunBestDistanceMax, 0.25f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownRunBestDistanceMin, 0.125f, 0.0f, 10.0f, 0.1f);

	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownWalkMaxDistance, 0.75f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownWalkMinDistance, 0.2f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownWalkBestDistanceMax, 0.15f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownWalkBestDistanceMin, 0.05f, 0.0f, 10.0f, 0.1f);

	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownSlopeTolerance, 0.3f, 0.0f, 1.0f, 0.01f);

	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownClearanceTestZOffset, 0.35f, 0.0f, 10.0f, 0.01f);
	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownClearanceTestRadius, 0.25f, 0.0f, 1.0f, 0.01f);

	TUNE_GROUP_FLOAT(DROPDOWN, fDropDownEdgeThresholdDot, 0.2f, 0.0f, 1.0f, 0.01f);

	TUNE_GROUP_FLOAT(DROPDOWN, fAutoJumpMaxDistance, 2.0f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(DROPDOWN, fAutoJumpMinDistance, 0.4f, 0.0f, 10.0f, 0.1f);

	TUNE_GROUP_FLOAT(DROPDOWN, fSmallDropDownWalkMaxDistance, 0.75f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(DROPDOWN, fSmallDropDownWalkMinDistance, 0.4f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(DROPDOWN, fSmallDropDownWalkBestDistanceMax, 0.2f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(DROPDOWN, fSmallDropDownWalkBestDistanceMin, 0.1f, 0.0f, 10.0f, 0.1f);

	TUNE_GROUP_FLOAT(DROPDOWN, fSmallDropDownRunMaxDistance, 0.75f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(DROPDOWN, fSmallDropDownRunMinDistance, 0.4f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(DROPDOWN, fSmallDropDownRunBestDistanceMax, 0.2f, 0.0f, 10.0f, 0.1f);
	TUNE_GROUP_FLOAT(DROPDOWN, fSmallDropDownRunBestDistanceMin, 0.1f, 0.0f, 10.0f, 0.1f);

	TUNE_GROUP_FLOAT(DROPDOWN, fMidpointTestRadius, 0.1f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(DROPDOWN, fMidpointTestCutoff, 0.33f, 0.0f, 1.0f, 0.01f);

	TUNE_GROUP_FLOAT(DROPDOWN, fBiasRightSideHeading, -0.1f, -1.0f, 1.0f, 0.01f);

	TUNE_GROUP_FLOAT(DROPDOWN, fGapDetectionDistance, 8.0f, 0.0f, 20.0f, 0.01f);

	m_fDropDownDepthRadius = fDropDownDepthRadius;
	m_fDropDownMinHeight = fDropDownMinHeight;
	m_fHeightToDropDownCutoff = fHeightToDropDownCutoff;
	m_fSmallDropDownHeight = fSmallDropDownHeight;
	m_fDropDownStartZOffset = fDropDownStartZOffset;
	m_fDropDownStartForwardOffset = fDropDownStartForwardOffset;
	m_fDropDownSlopeDotMax = fDropDownSlopeDotMax;
	m_fDropDownSlopeDotMinDirectionCutoff = fDropDownSlopeDotMinDirectionCutoff;
	m_fDropDownPointMinZDot = fDropDownPointMinZDot;
	m_fTestHeightDifference = fTestHeightDifference;
	m_fSlantedSlopeDot = fSlantedSlopeDot;
	m_bPerformControlTest = true;

	m_fDropDownEdgeTestRadius = fDropDownEdgeTestRadius;

	m_fDropDownBestDistanceDotMax = fDropDownBestDistanceDotMax;
	m_fDropDownBestDistanceDotMin = fDropDownBestDistanceDotMin;

	m_fDropDownRunMaxDistance = fDropDownRunMaxDistance;
	m_fDropDownRunMinDistance = fDropDownRunMinDistance;
	m_fDropDownRunBestDistanceMax = fDropDownRunBestDistanceMax;
	m_fDropDownRunBestDistanceMin = fDropDownRunBestDistanceMin;

	m_fDropDownWalkMaxDistance = fDropDownWalkMaxDistance;
	m_fDropDownWalkMinDistance = fDropDownWalkMinDistance;
	m_fDropDownWalkBestDistanceMax = fDropDownWalkBestDistanceMax;
	m_fDropDownWalkBestDistanceMin = fDropDownWalkBestDistanceMin;

	m_fDropDownSlopeTolerance = fDropDownSlopeTolerance;

	m_fDropDownClearanceTestZOffset = fDropDownClearanceTestZOffset;
	m_fDropDownClearanceTestRadius = fDropDownClearanceTestRadius;

	m_fDropDownEdgeThresholdDot = fDropDownEdgeThresholdDot;

	m_fAutoJumpMaxDistance = fAutoJumpMaxDistance;
	m_fAutoJumpMinDistance = fAutoJumpMinDistance;

	m_fSmallDropDownWalkMaxDistance = fSmallDropDownWalkMaxDistance;
	m_fSmallDropDownWalkMinDistance = fSmallDropDownWalkMinDistance;
	m_fSmallDropDownWalkBestDistanceMax = fSmallDropDownWalkBestDistanceMax;
	m_fSmallDropDownWalkBestDistanceMin = fSmallDropDownWalkBestDistanceMin;

	m_fSmallDropDownRunMaxDistance = fSmallDropDownRunMaxDistance;
	m_fSmallDropDownRunMinDistance = fSmallDropDownRunMinDistance;
	m_fSmallDropDownRunBestDistanceMax = fSmallDropDownRunBestDistanceMax;
	m_fSmallDropDownRunBestDistanceMin = fSmallDropDownRunBestDistanceMin;

	m_fMidpointTestRadius = fMidpointTestRadius;
	m_fMidpointTestCutoff = fMidpointTestCutoff;

	m_fBiasRightSideHeading = fBiasRightSideHeading;

	m_fGapDetectionDistance = fGapDetectionDistance;
}

CDropDownDetector::CDropDownDetector(CPed& ped)
: m_Ped(ped)
, m_eTestPhase(eATS_Free)
, m_nSearchFlags(0)
, m_nResultFlags(0)
, m_vCachedTestHeading(Vector3::ZeroType)
{
	for(int i = 0; i < ms_MaxDropDownTests; ++i)
	{
		m_pProbeResult[i]=NULL;

#if __BANK
		m_vDebugAsyncStartPos[i].Zero();
		m_vDebugAsyncEndPos[i].Zero();
#endif
	}
}

////////////////////////////////////////////////////////////////////////////////

CDropDownDetector::~CDropDownDetector()
{
	DeleteProbeResults();
}

////////////////////////////////////////////////////////////////////////////////

void CDropDownDetector::ResetPendingResults()
{
	for(int i = 0; i < ms_MaxDropDownTests; ++i)
	{
		if(m_pProbeResult[i])
			m_pProbeResult[i]->Reset();
	}
	m_eTestPhase = eATS_Free;
	m_nSearchFlags = 0;
	m_nResultFlags = 0;
}

////////////////////////////////////////////////////////////////////////////////

void CDropDownDetector::Process(Matrix34 *overRideMtx, CDropDownDetectorParams *pPedParams, u8 nFlags)
{
	PF_FUNC(DropDownProcess);

	if(m_Ped.GetPedResetFlag(CPED_RESET_FLAG_SearchingForDropDown))
	{
		m_DetectedDropDown.Reset();
		m_nSearchFlags = (u8)nFlags;
		m_nResultFlags = 0;

#if __BANK
		if(ms_pDebugDrawStore)
		{
			ms_pDebugDrawStore->ClearAll();
		}
#endif // __BANK

		Matrix34 testMat;
		if(overRideMtx)
		{
			testMat = *overRideMtx;
		}
		else
		{
			GetMatrix(testMat);
		}

		
		CDropDownDetectorParams dropDownDetectorParams;
		if (pPedParams == NULL)
		{
			pPedParams = &dropDownDetectorParams;
		}

		bool bPassedControlTest = false;
		const CControl* pControl = m_Ped.GetControlFromPlayer();
		if(pControl && pPedParams->m_bPerformControlTest)
		{
			Vector3 vStickInput(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm(), 0.0f);

			float fStickMagSq = vStickInput.Mag2();
			if(fStickMagSq > 0.0f)
			{
				float fCamOrient = camInterface::GetHeading();
				vStickInput.RotateZ(fCamOrient);
				vStickInput.NormalizeFast();
				Vector3 vPlayerForward = testMat.b;
				vPlayerForward.z = 0.0f;
				vPlayerForward.NormalizeFast();

				TUNE_GROUP_FLOAT(DROPDOWN, fDropDownStickDot, 0.5f, 0.0f, 1.0f, 0.0f);
				float fDot = vPlayerForward.Dot(vStickInput);
				if(fDot > fDropDownStickDot)
				{
					bPassedControlTest = true;
				}
			}
		}
		else
		{
			bPassedControlTest = true;
		}
		
		CreateProbeResults();

		eSearchResult eResult = eSR_Failed;

		eResult = SearchForDropDown(testMat, *pPedParams, m_DetectedDropDown);

		// ragdoll searches 
		if (eResult==eSR_Succeeded && !bPassedControlTest && m_DetectedDropDown.m_eDropType!=eRagdollDrop)
		{
			eResult = eSR_Failed;
		}

		bool bDeleteResults = true;
		switch(eResult)
		{
		case(eSR_Succeeded):
			{
				m_nResultFlags.SetFlag(DRF_DropDownDetected);
			}
			break;
		case(eSR_Deferred):
			//! Do nothing if we haven't got a result yet.
			bDeleteResults = false;
			break;
		case(eSR_Failed):
			{
				ResetPendingResults();
			}
			break;	
		}

		//! Don't delete for local player. Just keep valid always.
		if(bDeleteResults && !m_Ped.IsLocalPlayer())
		{
			DeleteProbeResults();
		}

		// Clear the reset flag
		m_Ped.SetPedResetFlag(CPED_RESET_FLAG_SearchingForDropDown, false);
	}
}

void CDropDownDetector::GetMatrix(Matrix34& m) const
{
	m = MAT34V_TO_MATRIX34(m_Ped.GetMatrix());

	if(m_Ped.IsLocalPlayer())
	{
		CControl* pControl = m_Ped.GetControlFromPlayer();
		if(pControl)
		{
			f32 fHeadingDiff = 0.0f;
			Vector3 vStickInput(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm(), 0.0f);
			if(vStickInput.Mag2() > 0.0f)
			{
				float fCamOrient = camInterface::GetPlayerControlCamHeading(m_Ped);
				float fStickAngle = rage::Atan2f(-vStickInput.x, vStickInput.y);
				fStickAngle = fStickAngle + fCamOrient;
				fStickAngle = fwAngle::LimitRadianAngle(fStickAngle);
				fHeadingDiff  = fwAngle::LimitRadianAngle(fStickAngle - m_Ped.GetCurrentHeading());

				TUNE_GROUP_FLOAT(DROPDOWN, fGetMatrixMaxHeadingDiff, 10.0f, 0.0f, 90.0, 1.0f);
				float fMaxHeading = fGetMatrixMaxHeadingDiff * DtoR;
				fHeadingDiff = Clamp(fHeadingDiff, -fMaxHeading, fMaxHeading);
			}

			f32 fNewHeading = fwAngle::LimitRadianAngle(m_Ped.GetCurrentHeading() + fHeadingDiff);

			m.MakeRotateZ(fNewHeading);

			Vec3V v = m_Ped.GetTransform().GetPosition();
			m.MakeTranslate(RCC_VECTOR3(v));
		}
	}
}

void CDropDownDetector::CreateProbeResults()
{
	//! Early out.
	if(m_pProbeResult[0])
		return;

	for(int i = 0; i < ms_MaxDropDownTests; ++i)
	{
		if(!m_pProbeResult[i])
		{	
			m_pProbeResult[i] = rage_new WorldProbe::CShapeTestResults(1);
		}
	}
}

void CDropDownDetector::DeleteProbeResults()
{
	for(int i = 0; i < ms_MaxDropDownTests; ++i)
	{
		delete m_pProbeResult[i];
	}
}

////////////////////////////////////////////////////////////////////////////////

CDropDownDetector::eSearchResult CDropDownDetector::SearchForDropDown(const Matrix34& testMat, CDropDownDetectorParams& dropDownDetectorParams, CDropDown& outDropDown)
{
	PF_START(DropDownSearch);

	//! Need a capsule for tests.
	if(!m_Ped.GetCapsuleInfo() || !m_Ped.GetCapsuleInfo()->IsBiped())
	{
		return eSR_Failed;
	}

	//! If on top of a moving object, don't drop.
	if(m_Ped.GetGroundPhysical())
	{
		static dev_float s_fVelocityThresholdSq = 1.0f;
		if(m_Ped.GetGroundPhysical()->GetVelocity().Mag2() > (s_fVelocityThresholdSq * s_fVelocityThresholdSq))
		{
			return eSR_Failed;
		}
	}

	float fDropDownMaxHeight;
	if(m_Ped.IsLocalPlayer())
	{
		fDropDownMaxHeight = CPlayerInfo::GetFallHeightForPed(m_Ped);
	}
	else
	{
		fDropDownMaxHeight = CTaskFall::GetHighFallHeight();
	}

	Vector3 vTestDirection = m_Ped.GetSlopeNormal();
	vTestDirection.Cross(testMat.a);
	float fSlopeDot = vTestDirection.Dot(ZAXIS);

	//! Don't allow vaulting dropping down from steep slopes.
	if(fSlopeDot > dropDownDetectorParams.m_fDropDownSlopeDotMax)
	{
		return eSR_Failed;
	}

	//! If we are standing on a slanted slope don't do drop downs if it's too severe as out arm & leg IK starts to break.
	const Vector3& vNormal = m_Ped.GetGroundNormal();
	float fSlopeHeading = rage::Atan2f(-vNormal.x, vNormal.y);
	if(abs(fSlopeHeading)>0.001f && VEC3V_TO_VECTOR3(m_Ped.GetMaxGroundNormal()).z < dropDownDetectorParams.m_fSlantedSlopeDot)
	{
		float fHeadingToSlope = m_Ped.GetCurrentHeading() - fSlopeHeading;
		fHeadingToSlope = fwAngle::LimitRadianAngleSafe(fHeadingToSlope);
		if (fHeadingToSlope < -QUARTER_PI || fHeadingToSlope > QUARTER_PI)
		{
			return eSR_Failed;
		}
	}

	//! Reset if performing non-async test.
	if(!m_nSearchFlags & DSF_AsyncTests)
	{
		m_eTestPhase = eATS_Free;
	}

	//! Don't allow massive slopes. This can happen when walking off a square edge, so prevent using it.
	if(fSlopeDot < dropDownDetectorParams.m_fDropDownSlopeDotMinDirectionCutoff)
	{
		vTestDirection = testMat.b;
	}

	Vector3 vDepthTestStart(testMat.d);
	vDepthTestStart.z += dropDownDetectorParams.m_fDropDownStartZOffset;
	vDepthTestStart += (vTestDirection * dropDownDetectorParams.m_fDropDownStartForwardOffset);

	float fDropDownDepthLength = fDropDownMaxHeight + dropDownDetectorParams.m_fDropDownStartZOffset + m_Ped.GetCapsuleInfo()->GetGroundToRootOffset();

#if __BANK
	ms_debugDrawColour = Color_green;
#endif

	Vector3 vStartOffset(vTestDirection * dropDownDetectorParams.m_fDropDownDepthRadius * 2.0f);
	Vector3 vEndOffset(0.f, 0.f, -fDropDownDepthLength);

	if(m_eTestPhase != eATS_InProgress)
	{
		m_vCachedTestHeading = testMat.b;
	}

	bool bAllReady = TestCapsuleAsync(vDepthTestStart, vStartOffset, vEndOffset, dropDownDetectorParams.m_fDropDownDepthRadius, m_nSearchFlags & DSF_AsyncTests);
	PF_STOP(DropDownSearch);

#if __BANK
	ms_debugDrawColour = Color_red;
#endif

	Vector3 vGroundPosition = m_Ped.GetGroundPos();

	if(bAllReady)
	{
		m_eTestPhase = eATS_Free;

		s32 iDropID = -1;
		float fHighestZ = -9999.0f;
		//! Find drop ID.
		for(int i = 0; i < (ms_MaxDropDownTests -1); ++i)
		{
			WorldProbe::CShapeTestHitPoint *pPrevResult = (i > 0) ? &((*m_pProbeResult[i-1])[0]) : NULL;
			WorldProbe::CShapeTestHitPoint &testResult1 = (*m_pProbeResult[i])[0];
			WorldProbe::CShapeTestHitPoint &testResult2 = (*m_pProbeResult[i+1])[0];

			bool bTestResult1Valid = GetIsValidIntersection(testResult1);
			bool bTestResult2Valid = GetIsValidIntersection(testResult2) || IsTestingForAutoJump() || IsTestingForRagdollTeeter();

			float fTestResult1Z = testResult1.GetHitPosition().z;
			if(fTestResult1Z > fHighestZ)
			{
				fHighestZ = fTestResult1Z;
			}

			if(bTestResult1Valid && bTestResult2Valid)
			{
				if(pPrevResult)
				{
					float fHeightDelta = abs(pPrevResult->GetHitPosition().z - testResult1.GetHitPosition().z);
					if(fHeightDelta > dropDownDetectorParams.m_fTestHeightDifference)
					{
						break;
					}
				}

				//! Note: Consider all valid indices. If our inst disappears, we still want to consider the impact as valid.
				bool bTestResult2Hit = testResult2.GetLevelIndex() != phInst::INVALID_INDEX;

				float fHeight1 = testResult1.GetPosition().GetZf();
				float fHeight2;
				//! note: Consider all valid indices. If our inst disappears, we still want to consider the impact as valid.
				if(bTestResult2Hit)
				{
					fHeight2 = testResult2.GetPosition().GetZf();
				}
				else
				{
					fHeight2 = -1000.0f;
				}

				if( ((fHeight1 - fHeight2) > dropDownDetectorParams.m_fDropDownMinHeight) &&					//! Height between 2 points is high enough.
					((vGroundPosition.z - fHeight2) > dropDownDetectorParams.m_fDropDownMinHeight) &&		//! Height from current ped position is high enough.
					((fHighestZ - vGroundPosition.z) < dropDownDetectorParams.m_fHeightToDropDownCutoff) &&	//! Height of drop point is near to ped position.
					(!bTestResult2Hit || (testResult2.GetHitNormal().z > dropDownDetectorParams.m_fDropDownPointMinZDot)) )
				{
					outDropDown.m_fDropHeight = fHeight1 - fHeight2;
					outDropDown.m_bSmallDrop = outDropDown.m_fDropHeight < dropDownDetectorParams.m_fSmallDropDownHeight;
					iDropID = (s32)i;
					break;
				}
			}
		}

		if(iDropID != -1)
		{
			WorldProbe::CShapeTestHitPoint &testResultDrop = (*m_pProbeResult[iDropID])[0];
			WorldProbe::CShapeTestHitPoint &testResultLand = (*m_pProbeResult[iDropID+1])[0];

			//! Use cached heading for dropdown validation as this is the direction our search tests were set.
			Matrix34 mTemp = testMat;
			mTemp.b = m_vCachedTestHeading;
				 
			if(ValidateDropDown(mTemp, testResultDrop, testResultLand, dropDownDetectorParams.m_fDropDownDepthRadius, fDropDownMaxHeight, dropDownDetectorParams, outDropDown))
			{
				outDropDown.m_pPhysical = m_Ped.GetGroundPhysical();
				if(outDropDown.m_pPhysical)
				{
					outDropDown.m_vDropDownStartPoint = VEC3V_TO_VECTOR3(outDropDown.m_pPhysical->GetTransform().UnTransform(VECTOR3_TO_VEC3V(outDropDown.m_vDropDownStartPoint)));
					outDropDown.m_vDropDownEdge = VEC3V_TO_VECTOR3(outDropDown.m_pPhysical->GetTransform().UnTransform(VECTOR3_TO_VEC3V(outDropDown.m_vDropDownEdge)));
					outDropDown.m_vDropDownLand = VEC3V_TO_VECTOR3(outDropDown.m_pPhysical->GetTransform().UnTransform(VECTOR3_TO_VEC3V(outDropDown.m_vDropDownLand)));
					outDropDown.m_vDropDownHeading = VEC3V_TO_VECTOR3(outDropDown.m_pPhysical->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(outDropDown.m_vDropDownHeading)));
				}
				return eSR_Succeeded;
			}
		}

		return eSR_Failed;
	}
	
	if(m_eTestPhase == eATS_Free)
	{
		m_eTestPhase = eATS_InProgress;
	}

	//! If all horizontal tests haven't finished, bail.
	return eSR_Deferred;
}

////////////////////////////////////////////////////////////////////////////////

bool CDropDownDetector::ValidateDropDown(const Matrix34& testMat, const WorldProbe::CShapeTestHitPoint& intersectionDrop, 
										 const WorldProbe::CShapeTestHitPoint& intersectionLand,
										 float fDropDownDepthRadius,
										 float fMaxDropDownHeight,
										 const CDropDownDetectorParams& dropDownDetectorParams,
										 CDropDown& outDropDown)
{
	PF_FUNC(DropDownValidate);

	bool bRunning = m_nSearchFlags.IsFlagSet(DSF_PedRunning);

	bool bTestForAutoJumpOnly = false;
	if(bRunning && !outDropDown.m_bSmallDrop)
	{
		if(m_nSearchFlags.IsFlagSet(DSF_TestForPararachuteJump))
		{
			TUNE_GROUP_FLOAT(DROPDOWN, fParachuteJumpTestDistance, 2.25f, 0.01f, 10.0f, 0.1f);
			TUNE_GROUP_FLOAT(DROPDOWN, fParachuteJumpTestZOffset, 0.75f, 0.01f, 10.0f, 0.1f);
			Vector3 vTestPos = intersectionDrop.GetHitPosition() + (testMat.b * fParachuteJumpTestDistance);
			vTestPos.z += fParachuteJumpTestZOffset;

			if(CTaskParachute::CanPedParachuteFromPosition(m_Ped, VECTOR3_TO_VEC3V(vTestPos), true, NULL, 20.0f))
			{
				m_nResultFlags.SetFlag(DRF_ParachuteDropDetected);	
				bTestForAutoJumpOnly = true;
			}
		}

		if(!m_nResultFlags.IsFlagSet(DRF_ParachuteDropDetected))
		{			
			if (m_nSearchFlags.IsFlagSet(DSF_TestForMount) && m_Ped.IsPlayer())
			{
				Vector3 vSearchDirection = VEC3V_TO_VECTOR3(m_Ped.GetTransform().GetB());
				CPed* pMountableAnimal = const_cast<CPed*>(CPlayerInfo::ScanForAnimalToRide(&m_Ped, vSearchDirection, false, true));
				if (pMountableAnimal!=NULL)
				{
					m_nResultFlags.SetFlag(DRF_MountDetected);
				}
			}
		}

		if(!m_nResultFlags.IsFlagSet(DRF_ParachuteDropDetected))
		{
			TUNE_GROUP_FLOAT(DROPDOWN, fDiveJumpTestDistance1, 1.5f, 0.01f, 10.0f, 0.1f);
			TUNE_GROUP_FLOAT(DROPDOWN, fDiveJumpTestDistance2, 2.25f, 0.01f, 10.0f, 0.1f);
			TUNE_GROUP_FLOAT(DROPDOWN, fDiveJumpTestZOffset, 0.75f, 0.01f, 10.0f, 0.1f);
			Vector3 vTestPos1 = intersectionDrop.GetHitPosition() + (testMat.b * fDiveJumpTestDistance1);
			Vector3 vTestPos2 = intersectionDrop.GetHitPosition() + (testMat.b * fDiveJumpTestDistance2);
			vTestPos1.z += fDiveJumpTestZOffset;
			vTestPos2.z += fDiveJumpTestZOffset;

			if(m_nSearchFlags.IsFlagSet(DSF_TestForDive) && 
				CTaskMotionSwimming::CanDiveFromPosition(m_Ped, VECTOR3_TO_VEC3V(vTestPos1), false) && 
				CTaskMotionSwimming::CanDiveFromPosition(m_Ped, VECTOR3_TO_VEC3V(vTestPos2), false))
			{
				m_nResultFlags.SetFlag(DRF_DiveDetected);
				bTestForAutoJumpOnly = true;
			}
		}
	}

	outDropDown.m_vDropDownLand = intersectionLand.GetHitPosition();

	Vector3 vPedGroundPos = m_Ped.GetGroundPos();
	Vector3 vPedToDrop = intersectionDrop.GetHitPosition() - vPedGroundPos;
	float fDistanceToDrop = vPedToDrop.Mag();
	
	Vector3 vExtrapolatedDropPos = vPedGroundPos + (testMat.b * fDistanceToDrop);
	vExtrapolatedDropPos.z = intersectionDrop.GetHitPosition().z;

	vPedToDrop = vExtrapolatedDropPos - vPedGroundPos;
	vPedToDrop.Normalize();

	Vector3 vEdgeTestEnd;
	if(bTestForAutoJumpOnly)
	{
		vEdgeTestEnd = m_Ped.GetGroundPos();
	}
	else
	{
		vEdgeTestEnd = vExtrapolatedDropPos;
	}

	Vector3 vEdgeTestStart = vExtrapolatedDropPos + (vPedToDrop * (fDropDownDepthRadius * 2.0f) );
	Vector3 vEdgeTestDir = vEdgeTestStart - vEdgeTestEnd;

	//! Edge can't rise too high - shouldn't edge drop up hill.
	float fEdgeDirectionDot = vEdgeTestDir.Dot(ZAXIS);
	if(fEdgeDirectionDot >= dropDownDetectorParams.m_fDropDownEdgeThresholdDot)
	{
		return false;
	}
	
	WorldProbe::CShapeTestHitPoint edgeIntersection;
	if(!TestCapsule(vEdgeTestStart, vEdgeTestEnd, dropDownDetectorParams.m_fDropDownEdgeTestRadius, edgeIntersection, true))
	{
		return false;
	}

	outDropDown.m_vDropDownEdge = edgeIntersection.GetHitPosition();

	Vector3 vPedToEdge = outDropDown.m_vDropDownEdge - vPedGroundPos;

	// Store the initial distance
	float fInitialDistance = vPedToEdge.XYMag();

	//! Check our slope angle to ensure we are able to reach edge.
	if(!bTestForAutoJumpOnly)
	{
		vPedToEdge.Normalize();
		float fDot = vPedToEdge.Dot(ZAXIS);
		if( abs(fDot) >= dropDownDetectorParams.m_fDropDownSlopeTolerance)
		{
			return false;
		}
	}

	//! Depending on edge normal, scale best distance. This allows us to drop off closer to angled edges (so that we
	//! don't penetrate them).
	vEdgeTestDir.Normalize();
	Vector3 vEdgeNormal = edgeIntersection.GetHitPolyNormal().z > 0.9 ? edgeIntersection.GetHitNormal() : edgeIntersection.GetHitPolyNormal();
	float fEdgeDot = vEdgeTestDir.Dot(vEdgeNormal);
	float fEdgeWeight = ClampRange(fEdgeDot, dropDownDetectorParams.m_fDropDownBestDistanceDotMin, dropDownDetectorParams.m_fDropDownBestDistanceDotMax);

	float fDistanceFromEdge = 0.0f;
	//! Do run/walk test.
	if(bTestForAutoJumpOnly)
	{
		if( (fInitialDistance > dropDownDetectorParams.m_fAutoJumpMinDistance) && (fInitialDistance < dropDownDetectorParams.m_fAutoJumpMaxDistance) )
		{
			outDropDown.m_eDropType = eAutoJump;
		}
	}
	else
	{
		float fMinDistance, fMaxDistance, fBestMinDistance, fBestMaxDistance;

		if(bRunning)
		{
			if(outDropDown.m_bSmallDrop)
			{
				fMinDistance = dropDownDetectorParams.m_fSmallDropDownRunMinDistance;
				fMaxDistance = dropDownDetectorParams.m_fSmallDropDownRunMaxDistance;
				fBestMinDistance = dropDownDetectorParams.m_fSmallDropDownRunBestDistanceMin;
				fBestMaxDistance = dropDownDetectorParams.m_fSmallDropDownRunBestDistanceMax;
			}
			else
			{
				fMinDistance = dropDownDetectorParams.m_fDropDownRunMinDistance;
				fMaxDistance = dropDownDetectorParams.m_fDropDownRunMaxDistance;
				fBestMinDistance = dropDownDetectorParams.m_fDropDownRunBestDistanceMin;
				fBestMaxDistance = dropDownDetectorParams.m_fDropDownRunBestDistanceMax;
			}

			if( (fInitialDistance > fMinDistance) && (fInitialDistance < fMaxDistance) )
			{
				fDistanceFromEdge = Lerp(fEdgeWeight, fBestMinDistance, fBestMaxDistance);
				outDropDown.m_vDropDownStartPoint = outDropDown.m_vDropDownEdge + (-testMat.b * fDistanceFromEdge);
				outDropDown.m_eDropType = eRunningDrop;
			}
		}

		if(outDropDown.m_eDropType != eRunningDrop)
		{
			if(outDropDown.m_bSmallDrop)
			{
				fMinDistance = dropDownDetectorParams.m_fSmallDropDownWalkMinDistance;
				fMaxDistance = dropDownDetectorParams.m_fSmallDropDownWalkMaxDistance;
				fBestMinDistance = dropDownDetectorParams.m_fSmallDropDownWalkBestDistanceMin;
				fBestMaxDistance = dropDownDetectorParams.m_fSmallDropDownWalkBestDistanceMax;
			}
			else
			{
				fMinDistance = dropDownDetectorParams.m_fDropDownWalkMinDistance;
				fMaxDistance = dropDownDetectorParams.m_fDropDownWalkMaxDistance;
				fBestMinDistance = dropDownDetectorParams.m_fDropDownWalkBestDistanceMin;
				fBestMaxDistance = dropDownDetectorParams.m_fDropDownWalkBestDistanceMax;
			}

			if( (fInitialDistance > fMinDistance) && (fInitialDistance < fMaxDistance) )
			{
				fDistanceFromEdge = Lerp(fEdgeWeight, fBestMinDistance, fBestMaxDistance);
				outDropDown.m_vDropDownStartPoint = outDropDown.m_vDropDownEdge + (-testMat.b * fDistanceFromEdge);
				outDropDown.m_eDropType = eWalkDrop;
			}
		}
	}

	if(outDropDown.m_eDropType == eDropMax)
	{
		return false;
	}

#if __BANK
	GetDebugDrawStore().AddSphere(RCC_VEC3V(outDropDown.m_vDropDownStartPoint), 0.1f, Color_yellow, DROPDOWN_DETECTOR_DEBUG_EXPIRY_TIME, 0, false);
	GetDebugDrawStore().AddSphere(RCC_VEC3V(outDropDown.m_vDropDownEdge), 0.1f, Color_orange, DROPDOWN_DETECTOR_DEBUG_EXPIRY_TIME, 0, false);
	GetDebugDrawStore().AddSphere(RCC_VEC3V(vPedGroundPos), 0.1f, Color_purple, DROPDOWN_DETECTOR_DEBUG_EXPIRY_TIME, 0, false);
#endif

	if(!bTestForAutoJumpOnly)
	{
		//! Check nothing is in the way of the edge point.
		{
			Vector3 vClearanceStart = VEC3V_TO_VECTOR3(m_Ped.GetTransform().GetPosition());
			Vector3 vClearanceEnd = outDropDown.m_vDropDownEdge + (testMat.b * (fDropDownDepthRadius * 4.0f));
			vClearanceEnd.z = outDropDown.m_vDropDownEdge.z + dropDownDetectorParams.m_fDropDownClearanceTestZOffset;

			WorldProbe::CShapeTestHitPoint clearanceIntersection;
			if(TestCapsule(vClearanceStart, vClearanceEnd, dropDownDetectorParams.m_fDropDownClearanceTestRadius, clearanceIntersection, true))
			{
				return false;
			}
		}

		//! Check the land point is sufficiently large (just check for any intersection in a small +/- range in front of land point).
		//! Note: Do specific tests for auto-jumps.
		{
			Vector3 vLandClearanceStart;
			if(intersectionLand.GetLevelIndex() != phInst::INVALID_INDEX)
			{
				vLandClearanceStart = outDropDown.m_vDropDownLand + (testMat.b * (fDropDownDepthRadius * 2.0f));
			}
			else
			{
				vLandClearanceStart = outDropDown.m_vDropDownEdge + (testMat.b * (fDropDownDepthRadius * 5.0f));
				vLandClearanceStart.z -= fMaxDropDownHeight;
			}

			Vector3 vLandClearanceEnd = vLandClearanceStart;
			vLandClearanceStart.z += 2.0f;
			vLandClearanceEnd.z -= 1.0f;

			WorldProbe::CShapeTestHitPoint landClearanceIntersection;
			bool bLandClearanceHit = TestCapsule(vLandClearanceStart, vLandClearanceEnd, fDropDownDepthRadius, landClearanceIntersection, true);
			if(!bLandClearanceHit || landClearanceIntersection.GetPosition().GetZf() > (outDropDown.m_vDropDownLand.z + 1.0f) )
			{
				// We're at an edge, but there's no floor to land on here!
				// do a ragdoll drop down if possible...
				if ( (outDropDown.m_fDropHeight > fMaxDropDownHeight) &&
					IsTestingForRagdollTeeter() && (!bRunning || fInitialDistance <CTaskNMBehaviour::sm_Tunables.m_PreEmptiveEdgeActivationMaxDistance) && CTaskNMBehaviour::CanUseRagdoll(&m_Ped, RAGDOLL_TRIGGER_FALL))
				{
					Vector3 vel = m_Ped.GetVelocity();

					// block if the character is moving faster than the max allowed speed
					if (vel.Mag()>CTaskNMBehaviour::sm_Tunables.m_PreEmptiveEdgeActivationMaxVel)
						return false;

					// block if the desired move blend ratio is less than a certain amount.
					// this allows the player to creep slowly towards edges without falling
					float pedDesiredMBR2 = m_Ped.GetMotionData()->GetDesiredMoveBlendRatio().Mag2();
					if (pedDesiredMBR2<CTaskNMBehaviour::sm_Tunables.m_PreEmptiveEdgeActivationMinDesiredMBR2)
						return false;

					Vector3 vToEdge = m_DetectedDropDown.m_vDropDownEdge - (VEC3V_TO_VECTOR3(m_Ped.GetMatrix().d()) - Vector3(0.0f, 0.0f, m_Ped.GetCapsuleInfo()->GetGroundToRootOffset()));
					vToEdge.NormalizeSafe();

					// block if the desired heading isn't pointing at the edge
					float angleToEdge = fwAngle::LimitRadianAngleSafe(fwAngle::GetATanOfXY(vToEdge.x, vToEdge.y)-(PI*0.5f));
					float pedHeading = fwAngle::LimitRadianAngleSafe(m_Ped.GetDesiredHeading());
					float angleDiff = Abs(SubtractAngleShorter(angleToEdge, pedHeading));
					if (angleDiff>CTaskNMBehaviour::sm_Tunables.m_PreEmptiveEdgeActivationMaxHeadingDiff)
						return false;

					if (vel.Dot(vToEdge)>CTaskNMBehaviour::sm_Tunables.m_PreEmptiveEdgeActivationMinDotVel)
					{
						//! Check water height at drop point. Don't want to ragdoll into water.
						Vector3 vWaterTestStart = intersectionDrop.GetHitPosition() + (testMat.b * 0.25f);

						float fWaterHeight = 0.0f;
						bool bHitWater = CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(vWaterTestStart, &fWaterHeight, true, fMaxDropDownHeight);

						if(!bHitWater || (testMat.d.z - fWaterHeight) > fMaxDropDownHeight)
						{
							outDropDown.m_eDropType = eRagdollDrop;
							return true;
						}
					}
				}

				return false;
			}
		}
	}

	//! if we think we can jump a gap, don't do drop down. Note: Do this only when running. Note: Allow ped to drop down
	//! if currently in vault task.
	if(bRunning && !m_nSearchFlags.IsFlagSet(DSF_FromVault))
	{
		CTaskJump::DoJumpGapTest(&m_Ped, g_JumpHelper, false, dropDownDetectorParams.m_fGapDetectionDistance);
		if(g_JumpHelper.WasLandingFound())
		{
			return false;
		}
	}

	//! Test mid point between drop and current position to see if there is a hole here.
	if(outDropDown.m_eDropType == eRunningDrop)
	{
		Vector3 vMidpointTestPosStart = (outDropDown.m_vDropDownEdge + vPedGroundPos) * 0.5f;
		Vector3 vMidpointTestPosEnd = vMidpointTestPosStart;
		vMidpointTestPosStart.z += 0.5f;
		vMidpointTestPosEnd.z -= 0.5f;
		WorldProbe::CShapeTestHitPoint midPointIntersection;
		if(TestCapsule(vMidpointTestPosStart, vMidpointTestPosEnd, dropDownDetectorParams.m_fMidpointTestRadius, midPointIntersection, true))
		{
			float fHeightDelta = abs(midPointIntersection.GetHitPosition().z - intersectionDrop.GetHitPosition().z);
			if(fHeightDelta > dropDownDetectorParams.m_fMidpointTestCutoff)
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	outDropDown.m_vDropDownHeading = testMat.b;

	vEdgeNormal.z = 0.0f;
	if(vEdgeNormal.Mag2() > 0.0f)
	{
		vEdgeNormal.NormalizeFast();
		float fEdgeHeading = fwAngle::LimitRadianAngle(rage::Atan2f(vEdgeNormal.x, -vEdgeNormal.y));

		//! Is this a left handed drop?
		float fHeadingDelta = SubtractAngleShorter(m_Ped.GetCurrentHeading(), fEdgeHeading);
		
		//! If we are biasing right side, adjust heading to accommodate.
		if(m_nSearchFlags.IsFlagSet(DSF_BiasRightSide))
		{
			fHeadingDelta = fwAngle::LimitRadianAngle(fHeadingDelta + dropDownDetectorParams.m_fBiasRightSideHeading);
		}
		
		outDropDown.m_bLeftHandDrop = fHeadingDelta < 0.0f;
	}

	return true;
}

bool CDropDownDetector::IsTestingForAutoJump() const
{
	return m_nSearchFlags.IsFlagSet(DSF_TestForPararachuteJump) || m_nSearchFlags.IsFlagSet(DSF_TestForDive);
}

bool CDropDownDetector::IsTestingForRagdollTeeter() const
{
	if(!m_nSearchFlags.IsFlagSet(DSF_TestForRagdollTeeter))
	{
		return false;
	}

	return (NetworkInterface::IsGameInProgress() ? CTaskNMBehaviour::sm_Tunables.m_UsePreEmptiveEdgeActivationMp==true : CTaskNMBehaviour::sm_Tunables.m_UsePreEmptiveEdgeActivation==true) && !m_Ped.GetPedResetFlag(CPED_RESET_FLAG_IsNearLaddder);
}


////////////////////////////////////////////////////////////////////////////////

bool CDropDownDetector::TestCapsuleAsync(const Vector3& vStart, const Vector3& vStartOffset, const Vector3& vEndOffset, const f32 fRadius, bool bAsync) const
{
	PF_FUNC(DropDownCapsuleTest);

	// Send shape tests if we are not already in progress
	if(m_eTestPhase != eATS_InProgress)
	{	
		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		capsuleDesc.SetIsDirected(true);
		capsuleDesc.SetIncludeFlags(DROPDOWN_DETECTOR_TEST_FLAGS);
		capsuleDesc.SetTypeFlags(TYPE_FLAGS_ALL);
		capsuleDesc.SetStateIncludeFlags(phLevelBase::STATE_FLAGS_ALL);
		capsuleDesc.SetExcludeInstance(m_Ped.GetAnimatedInst());
		capsuleDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_PICKUP_TYPE);
		capsuleDesc.SetContext(WorldProbe::LOS_GeneralAI);
		
		Vector3 vP0(vStart);
		for(int i = 0; i < ms_MaxDropDownTests; ++i)
		{
			WorldProbe::CShapeTestResults* pResult = m_pProbeResult[i];
			Assert(!pResult->GetWaitingOnResults());
			m_pProbeResult[i]->Reset();

			capsuleDesc.SetResultsStructure(pResult);
			capsuleDesc.SetCapsule(vP0, vP0 + vEndOffset, fRadius);
			WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc, bAsync ? WorldProbe::PERFORM_ASYNCHRONOUS_TEST : WorldProbe::PERFORM_SYNCHRONOUS_TEST);

#if __BANK
			const_cast<CDropDownDetector*>(this)->m_vDebugAsyncStartPos[i] = vP0;
			const_cast<CDropDownDetector*>(this)->m_vDebugAsyncEndPos[i] = vP0 + vEndOffset;
#endif
			vP0 += vStartOffset;
		}
	}

	// Check if all results are ready
	for(int i = 0; i < ms_MaxDropDownTests; ++i)
	{
		WorldProbe::CShapeTestResults* pResult = m_pProbeResult[i];
		if(!pResult->GetResultsReady())
			return false;
	}

	// Display debug shapes
#if __BANK
	for(int i = 0; i < ms_MaxDropDownTests; ++i)
	{
		WorldProbe::CShapeTestResults* pResult = m_pProbeResult[i];
		for(s32 j = 0; j < pResult->GetSize(); ++j)
		{
#if __ASSERT
			WorldProbe::CShapeTestHitPoint &testResult = (*pResult)[j];
			aiAssertf(IsEqualAll(testResult.GetPosition(), testResult.GetPosition()), "NaN detected from collision at (%.2f,%.2f,%.2f)", testResult.GetPosition().GetXf(), testResult.GetPosition().GetYf(),testResult.GetPosition().GetZf());
			aiAssertf(IsEqualAll(testResult.GetNormal(), testResult.GetNormal()), "NaN detected from collision at (%.2f,%.2f,%.2f)", testResult.GetPosition().GetXf(), testResult.GetPosition().GetYf(), testResult.GetPosition().GetZf());
			aiAssertf(IsEqualAll(testResult.GetIntersectedPolyNormal(), testResult.GetIntersectedPolyNormal()), "NaN detected from collision at (%.2f,%.2f,%.2f)", testResult.GetPosition().GetXf(), testResult.GetPosition().GetYf(), testResult.GetPosition().GetZf());
#endif
		}

		Vector3 vP0 = m_vDebugAsyncStartPos[i];
		Vector3 vP1 = m_vDebugAsyncEndPos[i];
		if(GetDebugEnabled())
		{
			GetDebugDrawStore().AddLine(RCC_VEC3V(vP0), RCC_VEC3V(vP1), ms_debugDrawColour, DROPDOWN_DETECTOR_DEBUG_EXPIRY_TIME);
			GetDebugDrawStore().AddSphere(RCC_VEC3V(vP0), fRadius, ms_debugDrawColour, DROPDOWN_DETECTOR_DEBUG_EXPIRY_TIME, 0, false);
			GetDebugDrawStore().AddSphere(RCC_VEC3V(vP1), fRadius, ms_debugDrawColour, DROPDOWN_DETECTOR_DEBUG_EXPIRY_TIME, 0, false);
			AddDebugIntersections(&((*pResult)[0]), pResult->GetNumHits(), ms_debugDrawColour);
		}
	}
#endif // __BANK

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CDropDownDetector::TestCapsule(const Vector3& vP0, const Vector3& vP1, const f32 fRadius, WorldProbe::CShapeTestHitPoint &hitPoint, const bool bDoInitialSphereTest) const
{
	PF_FUNC(DropDownCapsuleTest);

	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	WorldProbe::CShapeTestResults probeResults(hitPoint);
	capsuleDesc.SetResultsStructure(&probeResults);
	capsuleDesc.SetCapsule(vP0, vP1, fRadius);
	capsuleDesc.SetIsDirected(true);
	capsuleDesc.SetDoInitialSphereCheck(bDoInitialSphereTest);
	capsuleDesc.SetIncludeFlags(DROPDOWN_DETECTOR_TEST_FLAGS);
	capsuleDesc.SetTypeFlags(TYPE_FLAGS_ALL);
	capsuleDesc.SetStateIncludeFlags(phLevelBase::STATE_FLAGS_ALL);
	capsuleDesc.SetExcludeEntity(&m_Ped);
	capsuleDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_PICKUP_TYPE);
	capsuleDesc.SetContext(WorldProbe::LOS_GeneralAI);

	WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST);

#if __BANK
	for(s32 i = 0; i < probeResults.GetNumHits(); i++)
	{
#if __ASSERT
		WorldProbe::CShapeTestHitPoint &testResult = probeResults[i];
		aiAssertf(IsEqualAll(testResult.GetPosition(), testResult.GetPosition()), "NaN detected from collision at (%.2f,%.2f,%.2f)", testResult.GetPosition().GetXf(), testResult.GetPosition().GetYf(),testResult.GetPosition().GetZf());
		aiAssertf(IsEqualAll(testResult.GetNormal(), testResult.GetNormal()), "NaN detected from collision at (%.2f,%.2f,%.2f)", testResult.GetPosition().GetXf(), testResult.GetPosition().GetYf(), testResult.GetPosition().GetZf());
		aiAssertf(IsEqualAll(testResult.GetIntersectedPolyNormal(), testResult.GetIntersectedPolyNormal()), "NaN detected from collision at (%.2f,%.2f,%.2f)", testResult.GetPosition().GetXf(), testResult.GetPosition().GetYf(), testResult.GetPosition().GetZf());
#endif
	}

	if(GetDebugEnabled())
	{
		GetDebugDrawStore().AddLine(RCC_VEC3V(vP0), RCC_VEC3V(vP1), ms_debugDrawColour, DROPDOWN_DETECTOR_DEBUG_EXPIRY_TIME);
		if(bDoInitialSphereTest)
		{
			GetDebugDrawStore().AddSphere(RCC_VEC3V(vP0), fRadius, ms_debugDrawColour, DROPDOWN_DETECTOR_DEBUG_EXPIRY_TIME, 0, false);
		}
		GetDebugDrawStore().AddSphere(RCC_VEC3V(vP1), fRadius, ms_debugDrawColour, DROPDOWN_DETECTOR_DEBUG_EXPIRY_TIME, 0, false);
		AddDebugIntersections(&probeResults[0], probeResults.GetNumHits(), ms_debugDrawColour);
	}
#endif // __BANK

	return probeResults.GetNumHits() > 0;
}

////////////////////////////////////////////////////////////////////////////////

bool CDropDownDetector::GetIsValidIntersection(const WorldProbe::CShapeTestHitPoint& intersection) const
{
	if(intersection.GetHitInst() && !PGTAMATERIALMGR->GetPolyFlagNotClimbable(intersection.GetMaterialId()))
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CDropDownDetector::AddDebugIntersections(const WorldProbe::CShapeTestHitPoint* pIntersections, const u32 uNumIntersections, const Color32 debugColour) const
{
	if(pIntersections)
	{
		for(u32 i = 0; i < uNumIntersections; i++)
		{
			if(pIntersections[i].IsAHit())
			{
				Color32 rotatedColour(debugColour.GetBlue(), debugColour.GetRed(), debugColour.GetGreen());

				static dev_float RADIUS = 0.0125f;
				GetDebugDrawStore().AddSphere(pIntersections[i].GetPosition(), RADIUS, debugColour, DROPDOWN_DETECTOR_DEBUG_EXPIRY_TIME);
				GetDebugDrawStore().AddLine(pIntersections[i].GetPosition(), pIntersections[i].GetPosition() + pIntersections[i].GetIntersectedPolyNormal() * ScalarVFromF32(0.1f), rotatedColour, DROPDOWN_DETECTOR_DEBUG_EXPIRY_TIME);

				if(!GetIsValidIntersection(pIntersections[i]))
				{
					// Render something extra if not valid
					GetDebugDrawStore().AddSphere(pIntersections[i].GetPosition(), RADIUS * 2.0f, rotatedColour, DROPDOWN_DETECTOR_DEBUG_EXPIRY_TIME, 0, false);
				}
			}
		}
	}
}

void CDropDownDetector::RenderDebug()
{
	if(ms_bDebugDraw && ms_pDebugDrawStore)
	{
		ms_pDebugDrawStore->Render();
	}
}

void CDropDownDetector::SetupWidgets(bkBank& bank)
{
	bank.PushGroup("DropDown", false);
	bank.AddToggle("Display Debug",                    &ms_bDebugDraw);

	CTaskDropDown::AddWidgets(bank);

	bank.PopGroup(); // "DropDown"
}

#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

Vector3 CDropDown::GetDropDownStartPoint() const
{
	return m_pPhysical ? VEC3V_TO_VECTOR3(m_pPhysical->GetTransform().Transform(VECTOR3_TO_VEC3V(m_vDropDownStartPoint))) : m_vDropDownStartPoint;
}

Vector3 CDropDown::GetDropDownEdge() const
{
	return m_pPhysical ? VEC3V_TO_VECTOR3(m_pPhysical->GetTransform().Transform(VECTOR3_TO_VEC3V(m_vDropDownEdge))) : m_vDropDownEdge;
}

Vector3 CDropDown::GetDropDownLand() const
{
	return m_pPhysical ? VEC3V_TO_VECTOR3(m_pPhysical->GetTransform().Transform(VECTOR3_TO_VEC3V(m_vDropDownLand))) : m_vDropDownLand;
}

Vector3 CDropDown::GetDropDownHeading() const
{
	return m_pPhysical ? VEC3V_TO_VECTOR3(m_pPhysical->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(m_vDropDownHeading))) : m_vDropDownHeading;
}


//Helper for serialising if attached object can't be network
void  CDropDown::ClearPhysicalAndConvertToAbsolute()
{
	if(m_pPhysical)
	{
		m_vDropDownStartPoint	= GetDropDownStartPoint();
		m_vDropDownHeading		= GetDropDownHeading();
		m_vDropDownLand			= GetDropDownLand();
		m_vDropDownEdge			= GetDropDownEdge();
		m_pPhysical = NULL;
	}
}
