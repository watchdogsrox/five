#include "Peds/Ped.h"
#include "debug/DebugScene.h"
#include "modelinfo/PedModelInfo.h"

#include "GapHelper.h"
#include "math/angmath.h"

#include "system/control.h"
#include "Camera/CamInterface.h"

AI_OPTIMISATIONS()
AI_MOVEMENT_OPTIMISATIONS()

JumpAnalysis		g_JumpHelper;

#define	PED_VERTICAL_OFFSET		(PED_HUMAN_GROUNDTOROOTOFFSET)

JumpAnalysis::JumpAnalysis()
: m_LandFound(false)
, m_bNonAssistedJumpLandFound(false)
, m_bNonAssistedJumpSlopeFound(false)
, m_vNonAssistedJumpLandPos(Vector3::ZeroType)
{
}

void JumpAnalysis::GetMatrix(const CPed* pPed, Matrix34& m) const
{
	m = MAT34V_TO_MATRIX34(pPed->GetMatrix());

	if(pPed->IsLocalPlayer())
	{
		//! Local players will attempt to use stick direction to give a more accurate jump direction).
		const CControl* pControl = pPed->GetControlFromPlayer();
		if(pControl)
		{
			Vector3 vStickInput(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm(), 0.0f);
			if(vStickInput.Mag2() > 0.f)
			{
				float fCamOrient = camInterface::GetPlayerControlCamHeading(*pPed);
				float fStickAngle = rage::Atan2f(-vStickInput.x, vStickInput.y);
				fStickAngle = fStickAngle + fCamOrient;
				fStickAngle = fwAngle::LimitRadianAngle(fStickAngle);

				f32 fHeadingDiff  = fwAngle::LimitRadianAngle(fStickAngle - pPed->GetCurrentHeading());

				TUNE_GROUP_FLOAT(PED_JUMPING, fJumpHeadingAllowance, 10.0f, 0.0f, 90.0, 1.0f);
				float fMaxHeading = fJumpHeadingAllowance * DtoR;
				fHeadingDiff = Clamp(fHeadingDiff, -fMaxHeading, fMaxHeading);

				f32 fNewHeading = fwAngle::LimitRadianAngle(pPed->GetCurrentHeading() + fHeadingDiff);

				m.MakeRotateZ(fNewHeading);
			}
		}
	}
	else
	{
		//! For non-players, try to incorporate the turn as the jump fires off.
		float fDesiredHeadingChange =  SubtractAngleShorter(
			fwAngle::LimitRadianAngleSafe(pPed->GetMotionData()->GetDesiredHeading()), 
			fwAngle::LimitRadianAngleSafe(pPed->GetMotionData()->GetCurrentHeading())
			);

		fDesiredHeadingChange = Clamp( fDesiredHeadingChange, -DtoR*10.0f, +DtoR*10.0f );
		f32 fNewHeading = fwAngle::LimitRadianAngle(pPed->GetCurrentHeading() + fDesiredHeadingChange);

		m.MakeRotateZ(fNewHeading);
	}

	Vec3V v = pPed->GetTransform().GetPosition();
	m.MakeTranslate(RCC_VECTOR3(v));
}

// Returns true if the analysis found something
bool	JumpAnalysis::AnalyseTerrain(const CPed* pPed, float fMaxHorizJumpDistance, float fMinHorizJumpDistance, int iNumberOfProbes )
{
#if __BANK
	TUNE_GROUP_BOOL(PED_JUMPING, bRenderTerrainAnalysis, false);	
	TUNE_GROUP_INT(PED_JUMPING, nRenderTerrainAnalysisDebugTime, 1, 1, 10000, 1);
#endif	//__BANK

	m_LandFound = false;

	TUNE_GROUP_FLOAT(PED_JUMPING, fProbeDistance, 2.0f, 0.0f, 100.0f, 1.0f);
	TUNE_GROUP_FLOAT(PED_JUMPING, fHighJumpTestDistanceMin, 0.5f, 0.0f, 100.0f, 1.0f);
	TUNE_GROUP_FLOAT(PED_JUMPING, fHighJumpTestDistanceMax, 1.75f, 0.0f, 100.0f, 1.0f);

	Assert(iNumberOfProbes <= JUMP_ANALYSIS_MAX_PROBES);

	ProbeStore	probeStore[JUMP_ANALYSIS_MAX_PROBES];

	float	horizDistIncrement = (fMaxHorizJumpDistance / (float)iNumberOfProbes);
	float	horizDist = fMaxHorizJumpDistance;

	Matrix34 pedMatrix;
	GetMatrix(pPed, pedMatrix);
	Vector3	curPos = pedMatrix.d;
	m_CurDir = pedMatrix.b;

	// Check the horizontal distance
	static dev_float s_fHorTestClearanceDist = 1.0f;
	Vector3 endPos = curPos + (m_CurDir * (fMaxHorizJumpDistance+s_fHorTestClearanceDist));
	WorldProbe::CShapeTestHitPoint hitPoint;
	if( DoProbe(pPed, curPos, endPos, hitPoint ) )
	{
		// We did, calculate the length
		horizDist = (hitPoint.GetHitPosition() - curPos).XYMag();

#if __BANK
		if(bRenderTerrainAnalysis)
		{
			grcDebugDraw::Line(curPos, hitPoint.GetHitPosition(), Color32(255, 0, 0, 255), Color32(255, 0, 0, 255), nRenderTerrainAnalysisDebugTime );
		}
#endif	//__BANK
		}

#if __BANK
	else if(bRenderTerrainAnalysis)
	{
		grcDebugDraw::Line(curPos, endPos, Color32(255, 255, 0, 255), Color32(255, 0, 0, 255), nRenderTerrainAnalysisDebugTime );
	}

#endif	//__BANK

	const float fProbeLength = PED_VERTICAL_OFFSET + fProbeDistance;

	float distance = 0.0f;
	int	j=0;

	u32 nNonAssistedJumpHits = 0;
	u32 nNonAssistedJumpSlopeHits = 0;
	Vector3 vLastNonAssistedJumpPosition(0.0f, 0.0f, -9999.0f);
	Vector3 vLastNonAssistedJumpSlopePosition(0.0f, 0.0f, -9999.0f);
	float fNonAssistedJumpAvgZ = 0.0f;

	bool bFoundNonAssistedJumpNormal = false;
	bool bFoundNonAssistedSlope = false;

	while( distance <= horizDist )
	{
		bool	probeHit;
		Vector3 downPos = curPos - ( ZAXIS * (fProbeLength) );
		probeHit = DoProbe(pPed, curPos, downPos, hitPoint);

		probeStore[j].dx = distance;
		probeStore[j].pPhysical = NULL;
		if(probeHit)
		{
			// We hit something, store
			probeStore[j].dy = hitPoint.GetHitPosition().z - curPos.z;
			
			if(!PGTAMATERIALMGR->GetPolyFlagNotClimbable(hitPoint.GetMaterialId()))
			{
				probeStore[j].validHit = true;
			}
			if(hitPoint.GetHitEntity() && hitPoint.GetHitEntity()->GetIsPhysical())
			{
				probeStore[j].pPhysical = static_cast<CPhysical*>(hitPoint.GetHitEntity());
			}

			//! If test position is before jump distance, cache off some information that we can use. Note: we take
			//! highest position found.
			float fIntersectionDistFromPed = (hitPoint.GetHitPosition() - pedMatrix.d).XYMag();
			if(fIntersectionDistFromPed < fHighJumpTestDistanceMax)
			{
				nNonAssistedJumpSlopeHits++;

				static dev_float fMinJumpAngleRequired = 10.0f;
				static dev_float fMaxJumpAngleRequired = 45.0f;
				const float cosSlopeAngleMin = rage::Cosf( DtoR * fMinJumpAngleRequired );
				const float cosSlopeAngleMax = rage::Cosf( DtoR * fMaxJumpAngleRequired );

				if(!bFoundNonAssistedJumpNormal)
				{
					if(PGTAMATERIALMGR->GetPolyFlagStairs(hitPoint.GetMaterialId()))
					{
						bFoundNonAssistedJumpNormal = true;
					}
					else
					{
						bool bSteepSlope = (abs(hitPoint.GetHitPolyNormal().z) < cosSlopeAngleMin);
						if(bSteepSlope)
						{
							float fSlopeHeading = rage::Atan2f(-hitPoint.GetHitPolyNormal().x, hitPoint.GetHitPolyNormal().y);
							if(abs(fSlopeHeading)>0.001f)
							{
								float fHeadingToSlope = pPed->GetCurrentHeading() - fSlopeHeading;
								fHeadingToSlope = fwAngle::LimitRadianAngleSafe(fHeadingToSlope);

								if (fHeadingToSlope < -HALF_PI || fHeadingToSlope > HALF_PI)
								{
									bFoundNonAssistedJumpNormal = true;
								}
							}
						}
					}
				}
				if(!bFoundNonAssistedSlope && nNonAssistedJumpSlopeHits > 1 &&
					hitPoint.GetHitPosition().z > vLastNonAssistedJumpSlopePosition.z)
				{
					Vector3 vSlope(hitPoint.GetHitPosition() - vLastNonAssistedJumpSlopePosition);
					vSlope.NormalizeFast();

					Vector3 vSlopeNormal;
					Vector3 vTempRight;
					vTempRight.Cross(vSlope, ZAXIS);
					vSlopeNormal.Cross(vTempRight, vSlope);
					vSlopeNormal.NormalizeFast();

					bFoundNonAssistedSlope = (abs(vSlopeNormal.z) > cosSlopeAngleMax) && (abs(vSlopeNormal.z) < cosSlopeAngleMin);
				}

				vLastNonAssistedJumpSlopePosition = hitPoint.GetHitPosition();

				static dev_float s_fTolerance = 0.01f;

				bool bWithinZTolerance = (abs(hitPoint.GetHitPosition().z - vLastNonAssistedJumpPosition.z) < s_fTolerance) || 
					(hitPoint.GetHitPosition().z > vLastNonAssistedJumpPosition.z);

				if(!probeStore[j].pPhysical && (fIntersectionDistFromPed > fHighJumpTestDistanceMin) && bWithinZTolerance)
				{
					nNonAssistedJumpHits++;
					vLastNonAssistedJumpPosition = hitPoint.GetHitPosition();
					fNonAssistedJumpAvgZ += vLastNonAssistedJumpPosition.z;
				}
			}
		}
		else
		{
			probeStore[j].dy = downPos.z - curPos.z;
		}

#if __BANK
		if(bRenderTerrainAnalysis)
		{
			if(probeHit)
			{
				grcDebugDraw::Line(curPos, hitPoint.GetHitPosition(), Color32(0, 255, 0, 255), Color32(0, 255, 0, 255), nRenderTerrainAnalysisDebugTime );
			}
			else
			{
				// Probes that missed in red
				grcDebugDraw::Line(curPos, downPos, Color32(255, 0, 0, 255), Color32(255, 0, 0, 255), nRenderTerrainAnalysisDebugTime );
			}
		}
#endif	//__BANK

		j++;

		// Update probe position
		curPos += (m_CurDir * horizDistIncrement);
		distance += horizDistIncrement;
	}

	if(nNonAssistedJumpHits > 0)
	{
		if(nNonAssistedJumpHits == 1)
		{
			m_bNonAssistedJumpSlopeFound = bFoundNonAssistedJumpNormal;
		}
		else
		{
			static dev_bool bCheckNormal = true;
			if(bCheckNormal)
			{
				m_bNonAssistedJumpSlopeFound = bFoundNonAssistedSlope && bFoundNonAssistedJumpNormal;
			}
			else
			{	
				m_bNonAssistedJumpSlopeFound = bFoundNonAssistedSlope;
			}
		}

		m_bNonAssistedJumpLandFound = true;
		m_vNonAssistedJumpLandPos = vLastNonAssistedJumpPosition; 
		fNonAssistedJumpAvgZ /= nNonAssistedJumpHits;
		m_vNonAssistedJumpLandPos.z = fNonAssistedJumpAvgZ;
	}	

	// If distance is less than some distance, there is no gap. Note: Do this after non assisted jump calculation.
	if(horizDist < fMinHorizJumpDistance )
	{
		return false;
	}

	// Set number of probes to the actual number cast.
	iNumberOfProbes = j;

	int	currentLevelIdx = 0;
	ProbeLevel	levelData[JUMP_ANALYSIS_MAX_PROBES];


	for(int i=0;i<iNumberOfProbes;i++)
	{
		ProbeLevel *pLevelData = &levelData[currentLevelIdx];
		float		totalY;
		int			numProbes;

		// Start the current level
		pLevelData->dMinX = probeStore[i].dx;
		pLevelData->dMaxX = pLevelData->dMinX;
		if( probeStore[i].validHit )
			pLevelData->bValid = true;
		else
			pLevelData->bValid = false;
		totalY = probeStore[i].dy;
		
		// level physical is 1st physical that makes up this level (even if subsequent probes change physicals).
		pLevelData->pPhysical = probeStore[i].pPhysical;
		
		numProbes = 1;

		// Find any probes after this that are at the same level
		for(int j=i+1; j<iNumberOfProbes; j++)
		{
			if(IsProbeAdjacent(probeStore[i], probeStore[j]))
			{
				// We're adjacent, add to the current level
				totalY += probeStore[j].dy;
				pLevelData->dMaxX = probeStore[j].dx;
				numProbes++;
				if( !probeStore[j].validHit )
					pLevelData->bValid = false;
				i=j;
			}
			else
			{
				// Move on
				break;
			}
		}
		// Terminate the current level & move on
		pLevelData->averageY = totalY / (float)numProbes;		// Average out the Y

		currentLevelIdx++;
	}

#if __BANK
	if(bRenderTerrainAnalysis)
	{
		for(int i=0;i<currentLevelIdx;i++)
		{
			ProbeLevel *pLevel = &levelData[i];
			// Draw it
			Vector3	curPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			Vector3 start = curPos + (m_CurDir * pLevel->dMinX);
			Vector3 end = curPos + (m_CurDir * pLevel->dMaxX);

			start.z += pLevel->averageY;
			end.z += pLevel->averageY;

			// Draw a sphere for the landing spot, and a line for the area/length of any ledge
			grcDebugDraw::Sphere( start, 0.1f, Color32(0, 0, 255, 255), true, nRenderTerrainAnalysisDebugTime);

			if(pLevel->bValid)
				grcDebugDraw::Line(start, end, Color32(0, 0, 255, 255), Color32(0, 0, 255, 255), nRenderTerrainAnalysisDebugTime );
			else
				grcDebugDraw::Line(start, end, Color32(255, 0, 0, 255), Color32(255, 0, 0, 255), nRenderTerrainAnalysisDebugTime );
		}
	}
#endif	//__BANK

	//! Need to have at least 3 levels (level, we are on, a gap, then another level).
	int	levelID = 0;
	if(currentLevelIdx >= 2)
	{
		levelID = FindGap( 0, currentLevelIdx, levelData);
		if(levelID > 0 && CanWeMakeIt(pPed, levelID, levelData) )
		{
			// Copy the resultant landing data
			m_LandData = levelData[levelID];

			if( (m_LandData.dMinX + s_fHorTestClearanceDist) > horizDist )
			{
				return false;
			}

			// See if we can continue beyond our current level?
			m_LandFound = true;
		}
	}

	if( m_LandFound )
	{
		// Calculate the landing position from it
		CalculateLandingPosition( pPed, &m_LandData );

#if __BANK
		if(bRenderTerrainAnalysis)
		{
			grcDebugDraw::Sphere( GetLandingPosition(), 0.12f, Color32(255, 0, 0, 255), true, nRenderTerrainAnalysisDebugTime);
		}
#endif	//__BANK
	}

	return m_LandFound;
}

// Check if a probe is within a specified angle to the next probe
bool	JumpAnalysis::IsProbeAdjacent(ProbeStore &thisProbe, ProbeStore &nextProbe)
{
	static dev_float s_fDistance = 0.25f;
	float	deltaY = abs( nextProbe.dy - thisProbe.dy );
	if(deltaY <= s_fDistance)
	{
		return true;
	}

	return false;
}

void JumpAnalysis::CalculateLandingPosition( const CPed *pPed, ProbeLevel *pLevel )
{
	m_LandPos = m_CurDir * pLevel->dMinX + VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	m_LandPos.z += pLevel->averageY;

	if(pLevel->pPhysical)
	{
		// Get handhold in local space relative to physical handhold object.
		m_LandPosPhysical = VEC3V_TO_VECTOR3(pLevel->pPhysical->GetTransform().UnTransform(VECTOR3_TO_VEC3V(m_LandPos)));
	}
}

// Looks for gaps that are where there was no probe result, and this no level data
// Returns the ID of the other side of the gap (or zero)
int JumpAnalysis::FindGap(int startID, int maxID, ProbeLevel *pLevelData)
{
	TUNE_GROUP_FLOAT(PED_JUMPING, fGapInvalidThreshhold, 1.5f, 0.0f, 100.0f, 0.1f);
	TUNE_GROUP_FLOAT(PED_JUMPING, fGapMinDistance, 1.0f, 0.0f, 100.0f, 0.1f);
	TUNE_GROUP_FLOAT(PED_JUMPING, fHeightScoreFactor, 1.0f, 0.0f, 100.0f, 0.1f);
	TUNE_GROUP_FLOAT(PED_JUMPING, fDepthScoreFactor, 1.0f, 0.0f, 100.0f, 0.1f);

	int bestLevel = 0;
	float fBestScore = -1000.0f;

	ProbeLevel *pStartLevel = &pLevelData[startID];
	for(int i = (maxID-1); i > (startID+1); i-- )
	{
		ProbeLevel *pLandLevel = &pLevelData[i];
		ProbeLevel *pPrevLevel = &pLevelData[i-1];

		bool bPrevValid = (pPrevLevel->averageY < -fGapInvalidThreshhold);
		bool bLandValid = pLandLevel->bValid && 
			((pLandLevel->dMinX - pStartLevel->dMaxX) > fGapMinDistance) &&
			(pLandLevel->dMaxX - pLandLevel->dMinX > 0.01f);

		if( bPrevValid && bLandValid )
		{
			//! if level long enough to be considered a gap?
			bool bDetectedJumpableGap = (((pPrevLevel->dMaxX - pPrevLevel->dMinX) > 0.01f) && (pPrevLevel->averageY < pLandLevel->averageY));
			//! If not, check we have at least one other gap.
			if(!bDetectedJumpableGap)
			{
				for(int j = (startID + 1); j < (i-1); j++ )
				{
					if((pLevelData[j].averageY < -fGapInvalidThreshhold) && (pLevelData[j].averageY < pLandLevel->averageY))
					{
						bDetectedJumpableGap = true;
						break;
					}
				}
			}

			if(bDetectedJumpableGap)
			{
				float fScore = (pLandLevel->averageY * fHeightScoreFactor) + ((pLandLevel->dMaxX - pLandLevel->dMinX) * fDepthScoreFactor) ;
				if(fScore > fBestScore)
				{
					fBestScore = fScore;
					bestLevel = i;
				}
			}
		}
	}

	return bestLevel;
}

// JAY, maybe this should return the level we can make?
bool	JumpAnalysis::CanWeMakeIt(const CPed *pPed, int levelID, ProbeLevel *pLevelData )
{
	Vector3	vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

	// Check the heights of the levels up to this index and see if we can make the landing spot.
	for(int i = 0; i < levelID; i++)
	{
		float fDelta = (pLevelData[i].averageY + PED_HUMAN_GROUNDTOROOTOFFSET) - vPedPos.z;
		if(fDelta > 0)
		{
			// This level is higher than our launch height
			return false;
		}
	}
	return true;
}

bool JumpAnalysis::DoProbe(const CPed* pPed, Vector3 &start, Vector3 &end, WorldProbe::CShapeTestHitPoint &hitPoint)
{
	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestResults probeResults(hitPoint);
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetStartAndEnd(start, end);
	probeDesc.SetExcludeEntity(pPed);

	u32 nFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE;

	probeDesc.SetIncludeFlags(nFlags);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		return true;
	}
	return false;
}

float JumpAnalysis::GetHorizontalJumpDistance() const
{
	return m_LandFound ? m_LandData.dMinX : 0.0f;
}

void JumpAnalysis::ForceLandData(bool bLandFound, const Vector3 &vLandPosition, CPhysical *pPhysical, float fDistance)
{
	m_LandFound = bLandFound;
	m_LandData.dMinX = fDistance;
	if(pPhysical)
	{
		m_LandData.pPhysical = pPhysical;
		m_LandPosPhysical = vLandPosition;
		m_LandPos = VEC3V_TO_VECTOR3(GetLandingPhysical()->GetTransform().Transform(VECTOR3_TO_VEC3V(m_LandPosPhysical)));
	}
	else
	{
		m_LandPos = vLandPosition;
	}
}

const Vector3 &JumpAnalysis::GetLandingPositionForSync() const
{
	if(GetLandingPhysical())
	{
		return m_LandPosPhysical;
	}
	else
	{
		return m_LandPos;
	}
}

Vector3 JumpAnalysis::GetLandingPosition(bool bAsAttachedPos) const
{
	if(GetLandingPhysical() && bAsAttachedPos)
	{
		return VEC3V_TO_VECTOR3(GetLandingPhysical()->GetTransform().Transform(VECTOR3_TO_VEC3V(m_LandPosPhysical)));
	}
	else
	{
		return m_LandPos;
	}
}

const Vector3 &JumpAnalysis::GetNonAssistedLandingPosition() const
{
	return m_vNonAssistedJumpLandPos;
}
