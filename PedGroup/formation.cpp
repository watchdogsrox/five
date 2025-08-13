// C++ headers
#include <limits>

// Framework headers
#include "grcore/debugdraw.h"
#include "fwmaths\angle.h"

// Game headers
#include "formation.h"
#include "peds\pedIntelligence.h"
#include "Task\System\Task.h"
#include "peds/Ped.h"
#include "modelinfo\vehiclemodelinfo.h"
#include "game\modelindices.h"
#include "Task\General\TaskBasic.h"
#include "objects\object.h"

#include "data/aes_init.h"
AES_INIT_8;

AI_OPTIMISATIONS()

#define PED_FORMATION_RADIUS 0.35f		// equal to what PED_RADIUS was before it became data driven

//
// Name			:	Empty
// Purpose		:	Gets rid of all the points.
//

void CPointList::Empty()
{
	m_nPoints = 0;
	for (s32 C = 0; C < MAX_NUM_POINTS; C++)
	{
		m_aPointHasBeenClaimed[C] = false;
	}
}

//
// Name			:	AddPoint
// Purpose		:	Adds a point to a PointList.
//

void CPointList::AddPoint(const Vector3& NewPoint)
{
//	Assert(m_nPoints < MAX_NUM_POINTS);
	if (m_nPoints < MAX_NUM_POINTS)
	{
		m_aPoints[m_nPoints++] = NewPoint;
	}
}


//
// Name			:	MergeListsRemovingDoubles
// Purpose		:	Adds a second pointlist to the first. Points that are too close only get one copy in the final list.
//

void CPointList::MergeListsRemovingDoubles(CPointList *pMainList, CPointList *pToBeMergedList)
{
	s32	PointToBeMergedIn = 0;
	Vector3 TestPoint;

	while (PointToBeMergedIn < pToBeMergedList->m_nPoints && pMainList->m_nPoints < CPointList::MAX_NUM_POINTS)
	{
		if (pMainList->m_nPoints < CPointList::MAX_NUM_POINTS)	// Make sure we have enough space in the result list.
		{	
			TestPoint = pToBeMergedList->m_aPoints[PointToBeMergedIn];
	
			// Test whether this point already exists in the resulting list.
			bool	bFoundPoint = false;
			for (s32 C = 0; C < pMainList->m_nPoints; C++)
			{
				if ((pMainList->m_aPoints[C] - TestPoint).Mag() < 1.5f)
				{		// Point is already there. Don't add it again.
					bFoundPoint = true;
					break;
				}
			}
			if (!bFoundPoint)
			{	// Add the point to the resulting list.
				pMainList->m_aPoints[pMainList->m_nPoints++] = TestPoint;
			}
		}

		PointToBeMergedIn++;
	}
}


//
// Name			:	PrintDebug
// Purpose		:	
//

#if DEBUG_DRAW
void CPointList::PrintDebug()
{
	for (s32 C = 0; C < m_nPoints; C++)
	{
		grcDebugDraw::Line(m_aPoints[C],
							m_aPoints[C] + Vector3(0.0f,0.0f,10.0f), Color32(0xff, 0xff, 0xff, 0xff));
	}
}
#endif



#if !__FINAL
bool CPedFormation::ms_bDebugPedFormations = false;
const char * CPedFormation::sz_PedFormationTypes[CPedFormationTypes::NUM_FORMATION_TYPES] =
{
	"Loose",
	"Surround Facing Inwards",
	"Surround Facing Ahead",
	"Line Abreast",
	"Arrowhead",
	"V",
	"Follow In Line",
	"Single",
	"Pair"

//	"Arc In Front",
//	"Surround Facing In",
//	"Surround Facing Out",
//	"Surround Facing Out",
};

#if DEBUG_DRAW
void
CPedFormation::Debug(CPedGroup * pPedGroup)
{
	CPedGroupMembership * pMembership = pPedGroup->GetGroupMembership();
	for(s32 m=0; m<MAX_FORMATION_SIZE; m++)
	{
		CPed * pPed = pMembership->GetMember(m);
		if(!pPed)
			continue;

		Vector3 vPos = m_PedPositions[m].GetPosition();
		float fHeading = m_PedPositions[m].GetHeading();
		float fTargetRadius = m_PedPositions[m].GetTargetRadius();

		Color32 iMarkerCol = (m==CPedGroupMembership::LEADER) ? Color32(255,128,128,255) : Color32(128,64,64,255);
		Color32 iHeadingCol = (m==CPedGroupMembership::LEADER) ? Color32(128,255,128,255) : Color32(64,128,64,255);

		grcDebugDraw::Line(vPos - Vector3(fTargetRadius,0.0f,0.0f), vPos + Vector3(fTargetRadius,0.0f,0.0f), iMarkerCol);
		grcDebugDraw::Line(vPos - Vector3(0.0f,fTargetRadius,0.0f), vPos + Vector3(0.0f,fTargetRadius,0.0f), iMarkerCol);

		Vector3 vHeading(-rage::Sinf(fHeading), rage::Cosf(fHeading), 0.0f);

		grcDebugDraw::Line(vPos, vPos + vHeading, iHeadingCol);

		bool bDebugDrawFormationPositions = true;
		if (bDebugDrawFormationPositions)
		{
			grcDebugDraw::Sphere(vPos, CPedFormation_Loose::ms_fPerMemberSpacing, Color_orchid4, false);
		}
	}
}
#endif // DEBUG_DRAW
#endif // __FINAL

//***********************************************************************************
//	CPedFormation
//***********************************************************************************

const float CPedFormation::ms_fDefaultTargetRadius	= 1.0f;
dev_float		CPedFormation::ms_fMinSpacing						=	1.5f;


s32 MapPedFormationToScriptableFormation(s32 iPedFormationType)
{
	switch(iPedFormationType)
	{
		case CPedFormationTypes::FORMATION_LOOSE:
			return CScriptablePedFormationTypes::SCRIPT_FORMATION_LOOSE;
		case CPedFormationTypes::FORMATION_SURROUND_FACING_INWARDS:
			return CScriptablePedFormationTypes::SCRIPT_FORMATION_SURROUND_FACING_INWARDS;
		case CPedFormationTypes::FORMATION_SURROUND_FACING_AHEAD:
			return CScriptablePedFormationTypes::SCRIPT_FORMATION_SURROUND_FACING_AHEAD;
		case CPedFormationTypes::FORMATION_LINE_ABREAST:
			return CScriptablePedFormationTypes::SCRIPT_FORMATION_LINE_ABREAST;
		case CPedFormationTypes::FORMATION_FOLLOW_IN_LINE:
			return CScriptablePedFormationTypes::SCRIPT_FORMATION_FOLLOW_IN_LINE;
	}
	Assert(0);
	return -1;
}

s32 MapScriptableFormationToPedFormation(s32 iScriptableFormationType)
{
	switch(iScriptableFormationType)
	{
		case CScriptablePedFormationTypes::SCRIPT_FORMATION_LOOSE:
			return CPedFormationTypes::FORMATION_LOOSE;
		case CScriptablePedFormationTypes::SCRIPT_FORMATION_SURROUND_FACING_INWARDS:
			return CPedFormationTypes::FORMATION_SURROUND_FACING_INWARDS;
		case CScriptablePedFormationTypes::SCRIPT_FORMATION_SURROUND_FACING_AHEAD:
			return CPedFormationTypes::FORMATION_SURROUND_FACING_AHEAD;
		case CScriptablePedFormationTypes::SCRIPT_FORMATION_LINE_ABREAST:
			return CPedFormationTypes::FORMATION_LINE_ABREAST;
		case CScriptablePedFormationTypes::SCRIPT_FORMATION_FOLLOW_IN_LINE:
			return CPedFormationTypes::FORMATION_FOLLOW_IN_LINE;
	}
	Assert(0);
	return -1;
}


CPedFormation *
CPedFormation::NewFormation(s32 iType)
{
	Assert(iType >= 0 && iType < CPedFormationTypes::NUM_FORMATION_TYPES);

	switch(iType) {
		case CPedFormationTypes::FORMATION_LOOSE:
			return rage_new CPedFormation_Loose;
		case CPedFormationTypes::FORMATION_SURROUND_FACING_INWARDS:
			return rage_new CPedFormation_SurroundFacingInwards;
		case CPedFormationTypes::FORMATION_SURROUND_FACING_AHEAD:
			return rage_new CPedFormation_SurroundFacingAhead;
		case CPedFormationTypes::FORMATION_LINE_ABREAST:
			return rage_new CPedFormation_LineAbreast;
		case CPedFormationTypes::FORMATION_ARROWHEAD:
			return rage_new CPedFormation_Arrowhead;
		case CPedFormationTypes::FORMATION_V:
			return rage_new CPedFormation_V;
		case CPedFormationTypes::FORMATION_FOLLOW_IN_LINE:
			return rage_new CPedFormation_FollowInLine;
		case CPedFormationTypes::FORMATION_SINGLE:
			return rage_new CPedFormation_Single;
		case CPedFormationTypes::FORMATION_IN_PAIR:
			return rage_new CPedFormation_Pair;
	}
	Assert(0);
	return NULL;
}

bool
CPedFormation::Process()
{
	if (m_pPedGroup)
	{
		CPed * pLeader = m_pPedGroup->GetGroupMembership()->GetLeader();
		if(!pLeader)
			return true;
	}

	bool bOk = CalculatePositions(m_pPedGroup);

#if __DEV
	if(m_pPedGroup && ms_bDebugPedFormations)
		Debug(m_pPedGroup);
#endif

	return bOk;
}

void CPedFormation::SetSpacing(const float fSpacing, const float fAdjustSpeedMinDist, const float fAdjustSpeedMaxDist, const scrThreadId iThreadId)
{ 
	m_fSpacing = fSpacing; 

	if(fAdjustSpeedMinDist >= 0.0f)
		m_fAdjustSpeedMinDist = fAdjustSpeedMinDist;
	if(fAdjustSpeedMaxDist >= 0.0f)
		m_fAdjustSpeedMaxDist = fAdjustSpeedMaxDist;

	if(m_eFormationType == CPedFormationTypes::FORMATION_LOOSE)
	{
		// If no values were supplied for FORMATION_LOOSE, we calculate them automatically
		if(fAdjustSpeedMinDist < 0.0f && fAdjustSpeedMaxDist < 0.0f)
		{
			m_fAdjustSpeedMinDist = Max(m_fSpacing - 0.5f, 0.0f);
			m_fAdjustSpeedMaxDist = m_fSpacing * 2.0f;
		}
	}

	if(iThreadId != THREAD_INVALID)
	{
		m_iScriptModified = iThreadId;
	}

	if (m_pPedGroup && m_pPedGroup->IsLocallyControlled())
		m_pPedGroup->SetDirty();
}

//***********************************************************************************
//	CPedFormation_Loose
//***********************************************************************************
dev_float CPedFormation_Loose::ms_fPerMemberSpacing = 2.5f;

CPedFormation_Loose::CPedFormation_Loose() : CPedFormation()
{
	m_eFormationType = CPedFormationTypes::FORMATION_LOOSE;
	
	SetPositionsRotateWithLeader(false);
	SetFaceTowardsLeader(true);
	SetCrowdRoundLeaderWhenStationary(false);
	SetAddLeadersVelocityOntoGotoTarget(false);
	SetWalkAlongsideLeaderWhenClose(true);

	SetDefaultSpacing();
}

float CPedFormation_Loose::GetAdaptiveMBRAccel(float fDistance, float fMinAdjustSpeedDistance, float fMaxAdjustSpeedDistance)
{
	static dev_float sf_CloseFactor = 1.5f;
	static dev_float sf_SlowAccel = 0.3f;
	static dev_float sf_FastAccel = 1.0f;

	if (fDistance > fMaxAdjustSpeedDistance * sf_CloseFactor || fDistance < fMinAdjustSpeedDistance)
	{
		return sf_FastAccel;
	}
	else
	{
		return sf_SlowAccel;
	}
}

void CPedFormation_Loose::SetDefaultSpacing()
{
	static dev_float fDefault = 4.75f;
	SetSpacing(fDefault, Max(fDefault - 0.5f, 0.0f), fDefault * 2.0f);
}

float CPedFormation_Loose::GetMBRAccelForDistance(float fDistance, bool bUseAdaptiveMbr) const
{
	static dev_float sf_DefaultMbrAccel = 1.0f;

	if (!bUseAdaptiveMbr)
	{
		return sf_DefaultMbrAccel;
	}
	else
	{
		return GetAdaptiveMBRAccel(fDistance, m_fAdjustSpeedMinDist, m_fAdjustSpeedMaxDist);
	}
}

bool
CPedFormation_Loose::CalculatePositions(CPedGroup * pPedGroup)
{
	CPedGroupMembership * pMembership = pPedGroup->GetGroupMembership();
	CPed * pLeader = pMembership->GetLeader();
	Assert(pLeader);
	Vector3 vLeaderPosition = VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition());

	for(s32 iMemberIndex=0; iMemberIndex<CPedGroupMembership::LEADER; iMemberIndex++)
	{
		CPed * pMember = pMembership->GetMember(iMemberIndex);
		if(!pMember)
		{
			continue;
		}

		Vector3 vMemberPosition = VEC3V_TO_VECTOR3(pMember->GetTransform().GetPosition());
		float fPedSpacing = m_fSpacing < CPedFormation_Loose::ms_fPerMemberSpacing ? m_fSpacing : CPedFormation_Loose::ms_fPerMemberSpacing; // Minimum spacing between peds.

		// For some peds, space them out a little more if the leader is stationary.
		float fSpacingEpsilon = pMember->GetPedIntelligence()->GetNavCapabilities().GetStillLeaderSpacingEpsilon();
		if (fSpacingEpsilon > VERY_SMALL_FLOAT && pLeader->GetMotionData()->GetIsStill())
		{
			fPedSpacing += fSpacingEpsilon;
			// If running or sprinting then tack a little extra.
			if (!pMember->GetMotionData()->GetIsLessThanRun())
			{
				fPedSpacing += 0.4f;
			}
		}

		// Calculate a position for this formation member. 
		// Start by pushing out the desired point fPedSpacing from the leader. [3/9/2013 mdawe]
		Vector3 vLeaderToMember = vMemberPosition - vLeaderPosition;
		if(vLeaderToMember.Mag2() > SMALL_FLOAT)
			vLeaderToMember.NormalizeFast();
		vLeaderToMember *= fPedSpacing;
		Vector3 vMemberDesiredPosition = vLeaderPosition + vLeaderToMember;

		// For every other member, make sure we're not too close to them. [3/9/2013 mdawe]
		const ScalarV vMinDist2 = ScalarVFromF32(rage::square(fPedSpacing));
		for (s32 iPreviousMemberIndex = 0; iPreviousMemberIndex < iMemberIndex; ++iPreviousMemberIndex)
		{
			CPed* pPreviousMember = pMembership->GetMember(iPreviousMemberIndex);
			if (!pPreviousMember)
			{
				continue;
			}

			// Check to see if the last location this other member wanted is within fPedSpacing of our desired position.
			Vector3 vPreviousMemberDesiredPosition = m_PedPositions[iPreviousMemberIndex].GetPosition();
			ScalarV vDist2BetweenPositions = ScalarVFromF32((vPreviousMemberDesiredPosition - vMemberDesiredPosition).Mag2());
			if (IsLessThanAll(vDist2BetweenPositions, vMinDist2))
			{
				// Find a location off to the side of this previous member:
				
				Vector3 vLeaderToPreviousMember = vLeaderPosition - vPreviousMemberDesiredPosition;
				if(vLeaderToPreviousMember.Mag2() > SMALL_FLOAT)
					vLeaderToPreviousMember.NormalizeFast();

				Vector3 vLeaderToPreviousRight = vLeaderToPreviousMember;
				vLeaderToPreviousRight.RotateZ(DEG2RAD(90.f));
				float fDot = vLeaderToPreviousRight.Dot(vLeaderToMember);

				vLeaderToPreviousMember.RotateZ(DEG2RAD(90.f) * (fDot > 0.f ? 1.f : -1.f));
				vLeaderToPreviousMember *= fPedSpacing;

				vMemberDesiredPosition = vPreviousMemberDesiredPosition + vLeaderToPreviousMember;
			}
		}

		Assertf(rage::FPIsFinite(vMemberDesiredPosition.x) && rage::FPIsFinite(vMemberDesiredPosition.y) && rage::FPIsFinite(vMemberDesiredPosition.z), "vMemberDesiredPosition is not numerically valid.");
		ASSERT_ONLY(const float fLargeValue = 10000.0f;)
		Assertf(vMemberDesiredPosition.x > -fLargeValue && vMemberDesiredPosition.y > -fLargeValue && vMemberDesiredPosition.z > -fLargeValue && vMemberDesiredPosition.x < fLargeValue && vMemberDesiredPosition.y < fLargeValue && vMemberDesiredPosition.z < fLargeValue, "vMemberDesiredPosition (%.2f, %.2f, %.2f) is outside of the world", vMemberDesiredPosition.x, vMemberDesiredPosition.y, vMemberDesiredPosition.z);

		// Set our position and heading. [3/12/2013 mdawe]
		m_PedPositions[iMemberIndex].SetPosition(vMemberDesiredPosition);

		float fHeading = fwAngle::GetRadianAngleBetweenPoints(
			vLeaderPosition.x, vLeaderPosition.y,
			vMemberDesiredPosition.x, vMemberDesiredPosition.y
		);

		m_PedPositions[iMemberIndex].SetHeading(fHeading);
		m_PedPositions[iMemberIndex].SetTargetRadius(m_fSpacing);
	}

	return true;
}


//***********************************************************************************
//	CPedFormation_SurroundFacingInwards
//***********************************************************************************

CPedFormation_SurroundFacingInwards::CPedFormation_SurroundFacingInwards() : CPedFormation()
{
	m_eFormationType = CPedFormationTypes::FORMATION_SURROUND_FACING_INWARDS;
	SetPositionsRotateWithLeader(false);
	SetFaceTowardsLeader(true);
	SetCrowdRoundLeaderWhenStationary(false);
	SetAddLeadersVelocityOntoGotoTarget(true);
	SetWalkAlongsideLeaderWhenClose(false);

	SetDefaultSpacing();
}

void CPedFormation_SurroundFacingInwards::SetDefaultSpacing()
{
	SetSpacing(PED_FORMATION_RADIUS * 12.0f, 3.0f, 16.0f);
}

bool
CPedFormation_SurroundFacingInwards::CalculatePositions(CPedGroup * pPedGroup)
{
	// Just crap to get something working..  Place the group-members in a circle,
	// evenly spaced around the leader.

	CPedGroupMembership * pMembership = pPedGroup->GetGroupMembership();
	CPed * pLeader = pMembership->GetLeader();
	Assert(pLeader);

	Vector3 vLeaderPos = VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition());

	s32 iNumFollowers = pMembership->CountMembersExcludingLeader();
	float fAngleInc = TWO_PI / (float)iNumFollowers;
	float fAngle = 0.0f;
	float fHeading;

	for(s32 m=0; m<CPedGroupMembership::LEADER; m++)
	{
		if(pMembership->GetMember(m))
		{
			float fRotAngle = GetPositionsRotateWithLeader() ? fwAngle::LimitRadianAngle(fAngle + pLeader->GetTransform().GetHeading()) : fAngle;
			Vector3 vPosition(-rage::Sinf(fRotAngle) * m_fSpacing, rage::Cosf(fRotAngle) * m_fSpacing, 0.0f);
			vPosition += vLeaderPos;
			m_PedPositions[m].SetPosition(vPosition);
			m_PedPositions[m].SetTargetRadius(ms_fDefaultTargetRadius);
			
			if(GetFaceTowardsLeader()) fHeading = fwAngle::LimitRadianAngle(fAngle+PI);
			else if(GetFaceAwayFromLeader()) fHeading = fwAngle::LimitRadianAngle(fAngle);
			else fHeading = pLeader->GetTransform().GetHeading();

			m_PedPositions[m].SetHeading(fHeading);
			
			fAngle += fAngleInc;
		}
	}

	// Any need to set the leader's position, etc.. He will be in charge of it himself.
	m_PedPositions[CPedGroupMembership::LEADER].SetPosition(vLeaderPos);
	m_PedPositions[CPedGroupMembership::LEADER].SetHeading(pLeader->GetTransform().GetHeading());
	m_PedPositions[CPedGroupMembership::LEADER].SetTargetRadius(ms_fDefaultTargetRadius);

	return true;
}




//***********************************************************************************
//	CPedFormation_SurroundFacingAhead
//***********************************************************************************

CPedFormation_SurroundFacingAhead::CPedFormation_SurroundFacingAhead() : CPedFormation()
{
	m_eFormationType = CPedFormationTypes::FORMATION_SURROUND_FACING_AHEAD;
	SetPositionsRotateWithLeader(false);
	SetFaceSameWayAsLeader(true);
	SetCrowdRoundLeaderWhenStationary(false);
	SetAddLeadersVelocityOntoGotoTarget(true);
	SetWalkAlongsideLeaderWhenClose(false);

	SetDefaultSpacing();
}

void CPedFormation_SurroundFacingAhead::SetDefaultSpacing()
{
	SetSpacing(PED_FORMATION_RADIUS * 12.0f, 3.0f, 16.0f);
}

bool
CPedFormation_SurroundFacingAhead::CalculatePositions(CPedGroup * pPedGroup)
{
	CPedGroupMembership * pMembership = pPedGroup->GetGroupMembership();

	CPed * pLeader = pMembership->GetLeader();
	Assert(pLeader);

	Vector3 vLeaderPos = VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition());
	float fLeaderHeading = pLeader->GetDesiredHeading();	//pLeader->GetTransform().GetHeading()

	s32 iNumFollowers = pMembership->CountMembersExcludingLeader();
	float fAngleInc = TWO_PI / (float)iNumFollowers;
	float fAngle = 0.0f;

	for(s32 m=0; m<CPedGroupMembership::LEADER; m++)
	{
		if(pMembership->GetMember(m))
		{
			float fRotAngle = GetPositionsRotateWithLeader() ? fwAngle::LimitRadianAngle(fAngle + pLeader->GetTransform().GetHeading()) : fAngle;
			Vector3 vPosition(-rage::Sinf(fRotAngle) * m_fSpacing, rage::Cosf(fRotAngle) * m_fSpacing, 0.0f);
			vPosition += vLeaderPos;
			m_PedPositions[m].SetPosition(vPosition);
			m_PedPositions[m].SetTargetRadius(ms_fDefaultTargetRadius);

			m_PedPositions[m].SetTargetRadius(ms_fDefaultTargetRadius);

			fAngle += fAngleInc;
		}
	}

	// Any need to set the leader's position, etc.. He will be in charge of it himself.
	m_PedPositions[CPedGroupMembership::LEADER].SetPosition(vLeaderPos);
	m_PedPositions[CPedGroupMembership::LEADER].SetHeading(fLeaderHeading);
	m_PedPositions[CPedGroupMembership::LEADER].SetTargetRadius(ms_fDefaultTargetRadius);

	return true;
}




//***********************************************************************************
//	CPedFormation_LineAbreast
//***********************************************************************************

CPedFormation_LineAbreast::CPedFormation_LineAbreast() : CPedFormation()
{
	m_eFormationType = CPedFormationTypes::FORMATION_LINE_ABREAST;
	SetPositionsRotateWithLeader(true);
	SetFaceSameWayAsLeader(true);
	SetCrowdRoundLeaderWhenStationary(false);
	SetAddLeadersVelocityOntoGotoTarget(true);
	SetWalkAlongsideLeaderWhenClose(false);

	SetDefaultSpacing();
}

void CPedFormation_LineAbreast::SetDefaultSpacing()
{
	SetSpacing(PED_FORMATION_RADIUS * 8.0f, 3.0f, 16.0f);
}

bool
CPedFormation_LineAbreast::CalculatePositions(CPedGroup * pPedGroup)
{
	CPedGroupMembership * pMembership = pPedGroup->GetGroupMembership();

	CPed * pLeader = pMembership->GetLeader();
	Assert(pLeader);

	Vector3 vLeaderPos = VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition());
	float fLeaderHeading = pLeader->GetDesiredHeading();	//pLeader->GetTransform().GetHeading()

	Matrix34 rotMat;
	rotMat.Identity();
	if(GetPositionsRotateWithLeader())
		rotMat.MakeRotateZ(fLeaderHeading);
	Vector3 vLeaderRight = rotMat.a;

	// Any need to set the leader's position, etc.. He will be in charge of it himself.
	m_PedPositions[CPedGroupMembership::LEADER].SetPosition(vLeaderPos);
	m_PedPositions[CPedGroupMembership::LEADER].SetHeading(fLeaderHeading);
	m_PedPositions[CPedGroupMembership::LEADER].SetTargetRadius(ms_fDefaultTargetRadius);

	float fSide = -1.0f;
	float fCurrentSpacing = m_fSpacing;
	float fHeading;

	for(s32 m=0; m<CPedGroupMembership::LEADER; m++)
	{
		if(pMembership->GetMember(m))
		{
			Vector3 vPosition = vLeaderPos + (fCurrentSpacing * (vLeaderRight * fSide));
			m_PedPositions[m].SetPosition(vPosition);
			m_PedPositions[m].SetTargetRadius(ms_fDefaultTargetRadius);

			if(GetFaceTowardsLeader()) fHeading = fwAngle::GetRadianAngleBetweenPoints(vLeaderPos.x, vLeaderPos.y, vPosition.x, vPosition.y);
			else if(GetFaceAwayFromLeader()) fHeading = fwAngle::GetRadianAngleBetweenPoints(vPosition.x, vPosition.y, vLeaderPos.x, vLeaderPos.y);
			else fHeading = pLeader->GetTransform().GetHeading();

			m_PedPositions[m].SetHeading( fwAngle::LimitRadianAngle(fHeading) );

			if(fSide > 0.0f)
				fCurrentSpacing += m_fSpacing;

			fSide *= -1.0f;
		}
	}

	return true;
}



//***********************************************************************************
//	CPedFormation_Arrowhead
//***********************************************************************************

CPedFormation_Arrowhead::CPedFormation_Arrowhead() : CPedFormation()
{
	m_eFormationType = CPedFormationTypes::FORMATION_ARROWHEAD;
	SetPositionsRotateWithLeader(true);
	SetFaceSameWayAsLeader(true);
	SetCrowdRoundLeaderWhenStationary(false);
	SetAddLeadersVelocityOntoGotoTarget(true);
	SetWalkAlongsideLeaderWhenClose(false);

	SetDefaultSpacing();
}

void CPedFormation_Arrowhead::SetDefaultSpacing()
{
	SetSpacing(PED_FORMATION_RADIUS * 8.0f, 3.0f, 16.0f);
}

bool
CPedFormation_Arrowhead::CalculatePositions(CPedGroup * pPedGroup)
{
	CPedGroupMembership * pMembership = pPedGroup->GetGroupMembership();

	CPed * pLeader = pMembership->GetLeader();
	Assert(pLeader);

	Vector3 vLeaderPos = VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition());
	float fLeaderHeading = pLeader->GetDesiredHeading();	//pLeader->GetTransform().GetHeading()

	Matrix34 rotMat;
	rotMat.Identity();
	if(GetPositionsRotateWithLeader())
		rotMat.MakeRotateZ(fLeaderHeading);

	Vector3 vLeaderRight = rotMat.a;
	Vector3 vLeaderForwards = rotMat.b;

	// Any need to set the leader's position, etc.. He will be in charge of it himself.
	m_PedPositions[CPedGroupMembership::LEADER].SetPosition(vLeaderPos);
	m_PedPositions[CPedGroupMembership::LEADER].SetHeading(fLeaderHeading);
	m_PedPositions[CPedGroupMembership::LEADER].SetTargetRadius(ms_fDefaultTargetRadius);

	static const float fRowSpacing = m_fSpacing;
	static const float fSpacing = m_fSpacing;
	s32 iNumPerRow = 3;
	s32 iRowNumber = 1;
	s32 iNumThisRow = 0;
	float fHeading;
	Vector3 vHalfSpacingOffset = vLeaderRight * (fSpacing*0.5f);

	for(s32 m=0; m<CPedGroupMembership::LEADER; m++)
	{
		if(pMembership->GetMember(m))
		{
			// Move position back by the row index
			Vector3 vPosition = vLeaderPos;
			vPosition -= (vLeaderForwards * ((float)iRowNumber*fRowSpacing));
			// Move position back along to start of row
			Vector3 vRowStartOffset = ((float)iNumPerRow * 0.5f) * (vLeaderRight * fRowSpacing);
			vPosition -= vRowStartOffset;
			vPosition += vHalfSpacingOffset;
			// Move position along laterally by the index of this ped in the row
			vPosition += ((float)iNumThisRow) * (vLeaderRight * fSpacing);
			
			m_PedPositions[m].SetPosition(vPosition);
			m_PedPositions[m].SetTargetRadius(ms_fDefaultTargetRadius);

			if(GetFaceTowardsLeader()) fHeading = fwAngle::GetRadianAngleBetweenPoints(vLeaderPos.x, vLeaderPos.y, vPosition.x, vPosition.y);
			else if(GetFaceAwayFromLeader()) fHeading = fwAngle::GetRadianAngleBetweenPoints(vPosition.x, vPosition.y, vLeaderPos.x, vLeaderPos.y);
			else fHeading = pLeader->GetTransform().GetHeading();

			m_PedPositions[m].SetHeading( fwAngle::LimitRadianAngle(fHeading) );

			iNumThisRow++;
			if(iNumThisRow==iNumPerRow)
			{
				iRowNumber++;
				iNumPerRow+=2;
				iNumThisRow=0;
			}
		}
	}

	return true;
}








//***********************************************************************************
//	CPedFormation_V
//***********************************************************************************

CPedFormation_V::CPedFormation_V() : CPedFormation()
{
	m_eFormationType = CPedFormationTypes::FORMATION_V;
	SetPositionsRotateWithLeader(true);
	SetFaceSameWayAsLeader(true);
	SetCrowdRoundLeaderWhenStationary(false);
	SetAddLeadersVelocityOntoGotoTarget(true);
	SetWalkAlongsideLeaderWhenClose(false);
	
	SetDefaultSpacing();
}

void CPedFormation_V::SetDefaultSpacing()
{
	SetSpacing(PED_FORMATION_RADIUS * 8.0f, 3.0f, 16.0f);
}

bool
CPedFormation_V::CalculatePositions(CPedGroup * pPedGroup)
{
	CPedGroupMembership * pMembership = pPedGroup->GetGroupMembership();

	CPed * pLeader = pMembership->GetLeader();
	Assert(pLeader);

	Vector3 vLeaderPos = VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition());
	float fLeaderHeading = pLeader->GetDesiredHeading();

	Matrix34 rotMat;
	rotMat.Identity();
	if(GetPositionsRotateWithLeader())
		rotMat.MakeRotateZ(fLeaderHeading);

	Vector3 vLeaderRight = rotMat.a;
	Vector3 vLeaderForwards = rotMat.b;

	// Any need to set the leader's position, etc.. He will be in charge of it himself.
	m_PedPositions[CPedGroupMembership::LEADER].SetPosition(vLeaderPos);
	m_PedPositions[CPedGroupMembership::LEADER].SetHeading(fLeaderHeading);
	m_PedPositions[CPedGroupMembership::LEADER].SetTargetRadius(ms_fDefaultTargetRadius);

	static const float fRowSpacing = m_fSpacing;
	static const float fSpacing = m_fSpacing;
	s32 iNumPerRow = 3;
	s32 iRowNumber = 1;
	s32 iNumThisRow = 0;
	float fHeading;
	Vector3 vHalfSpacingOffset = vLeaderRight * (fSpacing*0.5f);

	for(s32 m=0; m<CPedGroupMembership::LEADER; m++)
	{
		if(pMembership->GetMember(m))
		{
			// Move position back by the row index
			Vector3 vPosition = vLeaderPos;
			vPosition -= (vLeaderForwards * ((float)iRowNumber*fRowSpacing));
			// Move position back along to start of row
			Vector3 vRowStartOffset = ((float)iNumPerRow * 0.5f) * (vLeaderRight * fRowSpacing);
			vPosition -= vRowStartOffset;
			vPosition += vHalfSpacingOffset;
			// Move position along laterally by the index of this ped in the row
			vPosition += ((float)iNumThisRow) * (vLeaderRight * fSpacing);

			m_PedPositions[m].SetPosition(vPosition);
			m_PedPositions[m].SetTargetRadius(ms_fDefaultTargetRadius);

			if(GetFaceTowardsLeader()) fHeading = fwAngle::GetRadianAngleBetweenPoints(vLeaderPos.x, vLeaderPos.y, vPosition.x, vPosition.y);
			else if(GetFaceAwayFromLeader()) fHeading = fwAngle::GetRadianAngleBetweenPoints(vPosition.x, vPosition.y, vLeaderPos.x, vLeaderPos.y);
			else fHeading = pLeader->GetTransform().GetHeading();

			m_PedPositions[m].SetHeading( fwAngle::LimitRadianAngle(fHeading) );

			iNumThisRow++;
			if(iNumThisRow==iNumPerRow)
			{
				iRowNumber++;
				iNumPerRow+=2;
				iNumThisRow=0;
			}
		}
	}

	return true;
}







//***********************************************************************************
//	CPedFormation_FollowInLine
//***********************************************************************************

static bank_float s_fAdditionalSpacing = 0.2f;

CPedFormation_FollowInLine::CPedFormation_FollowInLine() : CPedFormation()
{
	m_eFormationType = CPedFormationTypes::FORMATION_FOLLOW_IN_LINE;
	SetPositionsRotateWithLeader(true);
	SetFaceSameWayAsLeader(true);
	SetCrowdRoundLeaderWhenStationary(false);
	SetAddLeadersVelocityOntoGotoTarget(false);
	SetWalkAlongsideLeaderWhenClose(false);
	m_bResetPositionHistory = false;

	SetDefaultSpacing();

	m_iLastNumMembersExclLeader = 0;
	m_vLeaderLastPosition = Vector3(9999.9f, 9999.9f, 9999.9f);
	
	for(s32 i = 0; i < MAX_FORMATION_SIZE; i++)
	{
		m_fRandomSpacing[i] = fwRandom::GetRandomNumberInRange(0.0f, s_fAdditionalSpacing);
	}
}

void CPedFormation_FollowInLine::SetDefaultSpacing()
{
	SetSpacing(2.0f, 1.0f, 5.0f);
}

#if __DEV
void
CPedFormation_FollowInLine::Debug(CPedGroup * pPedGroup)
{
	CPedFormation::Debug(pPedGroup);
	
	static const float fSize=0.5f;
	static const Color32 iCol(255,0,0,255);

	for(s32 p=0; p<MAX_FORMATION_SIZE; p++)
	{
		Vector3 vPos = m_vPositionHistory[p];
		grcDebugDraw::Line(Vector3(vPos.x - fSize, vPos.y, vPos.z), Vector3(vPos.x + fSize, vPos.y, vPos.z), iCol);
		grcDebugDraw::Line(Vector3(vPos.x, vPos.y - fSize, vPos.z), Vector3(vPos.x, vPos.y + fSize, vPos.z), iCol);
	}
}
#endif

bool CPedFormation_FollowInLine::HasEnoughPositionsForFollowers() const
{
	if(m_bResetPositionHistory)
	{
		return false;
	}

	unsigned int i = 0;
	for(i = 0; i < MAX_FORMATION_SIZE; i++)
	{
		if(i == MAX_FORMATION_SIZE - 1)
		{
			continue;
		}

		if(m_vPositionHistory[i] == m_vPositionHistory[i + 1])
		{
			break;
		}
	}

	return i >= m_pPedGroup->GetGroupMembership()->CountMembers();
}

bool
CPedFormation_FollowInLine::CalculatePositions(CPedGroup * pPedGroup)
{
	//m_fSpacing = 20.0f;
	const float fDeltaSqr = m_fSpacing*m_fSpacing;		// how far leader moves before another waypoint added
	static const float fMaxDeltaSqr = 10.0f*10.0f;		// how far before we restart positions, eg teleport
	static const float fSurroundPlayerTargetRadius = 6.0f;
	Assert(fDeltaSqr < fMaxDeltaSqr);

	CPedGroupMembership * pMembership = pPedGroup->GetGroupMembership();

	s32 iNumMembersExclLeader = pMembership->CountMembersExcludingLeader();
	CPed * pLeader = pMembership->GetLeader();
	Assert(pLeader);

	Vector3 vLeaderPos = VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition());
	float fLeaderHeading = pLeader->GetDesiredHeading();

	Vector3 vDiff = vLeaderPos - m_vLeaderLastPosition;
	float fDistSqr = vDiff.Mag2();

	// Leader has moved too far, reset all positions
	if(fDistSqr > fMaxDeltaSqr || m_bResetPositionHistory)
	{
		for(s32 i=0; i<MAX_FORMATION_SIZE; i++)
		{
			m_vPositionHistory[i] = vLeaderPos;
			m_fHeadingHistory[i] = fLeaderHeading;
			m_fTargetRadiusHistory[i] = fSurroundPlayerTargetRadius;
			m_fRandomSpacing[i] = fwRandom::GetRandomNumberInRange(0.0f, s_fAdditionalSpacing);
		}
		m_vLeaderLastPosition = vLeaderPos;
		vDiff = Vector3(0.0f,0.0f,0.0f);
		fDistSqr = 0.001f;	// force a small update

		m_bResetPositionHistory = false;
	}

	// If leader has moved over a certain distance, then create a new point
	if(fDistSqr > fDeltaSqr)
	{
		// Shuffle all the positions along.  We use all the slots including LEADER
		for(s32 i=CPedGroupMembership::LEADER; i>0; i--)
		{
			Vector3 vPos = m_vPositionHistory[i-1];
			float fHeading = m_fHeadingHistory[i-1];
			float fTargetRadius = m_fTargetRadiusHistory[i-1];
			m_vPositionHistory[i] = vPos;
			m_fHeadingHistory[i] = fHeading;
			m_fTargetRadiusHistory[i] = fTargetRadius;
		}

		// Add the new position at start of the array
		m_vPositionHistory[0] = vLeaderPos;	//m_vLeaderLastPosition;
		m_fTargetRadiusHistory[0] = ms_fDefaultTargetRadius;

		// Add the heading to the leader's current pos
		Vector3 vVecToLeader = vLeaderPos - m_vLeaderLastPosition;
		vVecToLeader.z = 0.0f;
		if(vVecToLeader.Mag2() > 0.05f)
		{
			float fHeadingToLeader = fwAngle::GetRadianAngleBetweenPoints(vVecToLeader.x, vVecToLeader.y, 0.0f,0.0f);
			m_fHeadingHistory[0] = fHeadingToLeader;
		}
		else
		{
			m_fHeadingHistory[0] = fLeaderHeading;
		}

		m_vLeaderLastPosition = vLeaderPos;

		vDiff = Vector3(0.0f,0.0f,0.0f);
		fDistSqr = 0.001f;	// force a small update

		pMembership->SortByDistanceFromLeader();
	}

	//****************************************************************************
	// If we've moved, or the number of group members has changed - then fill out
	// the ped positions from our history store of leader's positions.
	//****************************************************************************

	if(fDistSqr != 0.0f || iNumMembersExclLeader != m_iLastNumMembersExclLeader)
	{
		// Any need to set the leader's position, etc ? He will be in charge of it himself..
		m_PedPositions[CPedGroupMembership::LEADER].SetPosition(vLeaderPos);
		m_PedPositions[CPedGroupMembership::LEADER].SetHeading(fLeaderHeading);
		m_PedPositions[CPedGroupMembership::LEADER].SetTargetRadius(ms_fDefaultTargetRadius);

		// The distance that the leader has moved
		float fDistMoved = vDiff.XYMag();
		if(fDistMoved > m_fSpacing)
			fDistMoved = m_fSpacing;
		float t = fDistMoved / m_fSpacing;

		s32 iHistoryIndex = 1;
		s32 iAheadIndexHistory = 0;

		for(s32 i=0; i<CPedGroupMembership::LEADER; i++)
		{
			CPed * pMember = pMembership->GetMember(i);
			if(pMember)
			{
				Vector3 vVecToNext, vNewPos;
				vVecToNext = m_vPositionHistory[iAheadIndexHistory] - m_vPositionHistory[iHistoryIndex];
				vNewPos = m_vPositionHistory[iHistoryIndex] + (vVecToNext * t);

				vNewPos += vVecToNext * m_fRandomSpacing[i];

				float fHeadingToNext = fwAngle::GetRadianAngleBetweenPoints(vVecToNext.x, vVecToNext.y, 0.0f,0.0f);

				m_PedPositions[i].SetPosition(vNewPos);	//m_vPositionHistory[iHistoryIndex]);
				m_PedPositions[i].SetHeading(fHeadingToNext);	//m_fHeadingHistory[iHistoryIndex]);
				m_PedPositions[i].SetTargetRadius(m_fTargetRadiusHistory[iHistoryIndex]);
				
				iAheadIndexHistory = iHistoryIndex;
				iHistoryIndex++;
			}
		}
	}

	m_iLastNumMembersExclLeader = iNumMembersExclLeader;

	return true;
}

//***********************************************************************************
//	CPedFormation_Single
//***********************************************************************************

CPedFormation_Single::CPedFormation_Single() : CPedFormation()
{
	m_eFormationType = CPedFormationTypes::FORMATION_SINGLE;
	SetPositionsRotateWithLeader(true);
	SetFaceSameWayAsLeader(true);
	SetCrowdRoundLeaderWhenStationary(false);
	SetAddLeadersVelocityOntoGotoTarget(false);
	SetWalkAlongsideLeaderWhenClose(false);

	SetDefaultSpacing();
}

void CPedFormation_Single::SetDefaultSpacing()
{
	SetSpacing(2.0f, 1.0f, 5.0f);
}

#if __DEV
void
CPedFormation_Single::Debug(CPedGroup * pPedGroup)
{
	CPedFormation::Debug(pPedGroup);

}
#endif

bool
CPedFormation_Single::CalculatePositions(CPedGroup* pPedGroup)
{
	CPedGroupMembership * pMembership = pPedGroup->GetGroupMembership();

	for (s32 i=0; i<CPedGroupMembership::LEADER; i++)
	{
		CPed* pMember = pMembership->GetMember(i);
		if (pMember)
		{
			CPed* pLeader = pMembership->GetMember(CPedGroupMembership::LEADER);
			
			if (pLeader)
			{
				Vector3 vLeaderForward = VEC3V_TO_VECTOR3(pLeader->GetTransform().GetB());
				vLeaderForward.Scale(m_fSpacing);

				Vector3 vNewPos = VEC3V_TO_VECTOR3(pLeader->GetTransform().GetPosition()) - float(i) * vLeaderForward;

				m_PedPositions[i].SetPosition(vNewPos);
				m_PedPositions[i].SetHeading(pLeader->GetTransform().GetHeading());	
				m_PedPositions[i].SetTargetRadius(1.0f);
			}
		}
	}
	return true;
}

//***********************************************************************************
//	CPedFormation_Pair
//***********************************************************************************

CPedFormation_Pair::CPedFormation_Pair() : CPedFormation()
{
	m_eFormationType = CPedFormationTypes::FORMATION_IN_PAIR;
	SetPositionsRotateWithLeader(true);
	SetFaceSameWayAsLeader(true);
	SetCrowdRoundLeaderWhenStationary(false);
	SetAddLeadersVelocityOntoGotoTarget(false);
	SetWalkAlongsideLeaderWhenClose(false);

	SetDefaultSpacing();

	m_iLeaderState = LeaderIdle;
	m_bLeaderStateAcknowledged = false;
}

void CPedFormation_Pair::SetDefaultSpacing()
{
	SetSpacing(2.0f, 1.0f, 5.0f);
}

#if __DEV
void
CPedFormation_Pair::Debug(CPedGroup * pPedGroup)
{
	CPedFormation::Debug(pPedGroup);
	
// 	static const float fSize=0.5f;
// 	static const Color32 iCol(255,0,0,255);
// 
// 	for(s32 p=0; p<MAX_FORMATION_SIZE; p++)
// 	{
// 		Vector3 vPos = m_vPositionHistory[p];
// 		grcDebugDraw::Line(Vector3(vPos.x - fSize, vPos.y, vPos.z), Vector3(vPos.x + fSize, vPos.y, vPos.z), iCol);
// 		grcDebugDraw::Line(Vector3(vPos.x, vPos.y - fSize, vPos.z), Vector3(vPos.x, vPos.y + fSize, vPos.z), iCol);
// 	}
}
#endif

bool
CPedFormation_Pair::CalculatePositions(CPedGroup* UNUSED_PARAM(pPedGroup))
{
// 	CPedGroupMembership* pMembership = pPedGroup->GetGroupMembership();
// 
// 	aiAssertf(pMembership->CountMembers() <= 2, "Trying to use formation pair with more than 2 members");
// 
// 	CPed* pLeader = pMembership->GetMember(CPedGroupMembership::LEADER);
// 	
// 	if (pLeader)
// 	{
// 		for (s32 i=0; i<CPedGroupMembership::LEADER; i++)
// 		{
// 			CPed* pMember = pMembership->GetMember(i);
// 			if (pMember)
// 			{
// 				Vector3 vLeaderRight = pLeader->GetA();
// 				Vector3 vNewPos = pLeader->GetPosition() - vLeaderRight;
// 
// 				m_PedPositions[i].SetPosition(vNewPos);
// 				m_PedPositions[i].SetHeading(pLeader->GetTransform().GetHeading());	
// 				m_PedPositions[i].SetTargetRadius(1.0f);
// 			}
// 		}
// 	}

	return true;
}


