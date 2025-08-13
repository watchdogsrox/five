#include "control/WaypointRecordingRoute.h"

// Rage headers
#include "file\stream.h"

// Framework headers
#include "fwmaths\Angle.h"

// Game headers
#if __BANK && !defined(RAGEBUILDER)	// Ragebuilder doesn't know about bank_float, et al
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "peds/Ped.h"
#endif

AI_OPTIMISATIONS()

#if __BANK

bool ::CWaypointRecordingRoute::AddWaypoint(const Vector3 & vPos, const float fSpeed, const float fHeading, const u16 iFlags)
{
	if(m_iNumWaypoints < ms_iMaxNumWaypoints)
	{
		m_pWaypoints[m_iNumWaypoints].m_fXYZ[0] = vPos.x;
		m_pWaypoints[m_iNumWaypoints].m_fXYZ[1] = vPos.y;
		m_pWaypoints[m_iNumWaypoints].m_fXYZ[2] = vPos.z;
		m_pWaypoints[m_iNumWaypoints].m_PackedStruct1.m_iMoveBlendRatio = iFlags & WptFlag_Vehicle ? CruiseSpeedtoUINT8(fSpeed) : MBRtoUINT8(fSpeed);
		m_pWaypoints[m_iNumWaypoints].m_PackedStruct1.m_iHeading = HDGtoUINT8(fHeading);
		m_pWaypoints[m_iNumWaypoints].m_PackedStruct1.m_iFlags = iFlags;
		m_pWaypoints[m_iNumWaypoints].m_PackedStruct2.m_iFreeSpaceOnLeft = 0;
		m_pWaypoints[m_iNumWaypoints].m_PackedStruct2.m_iFreeSpaceOnRight = 0;
		m_pWaypoints[m_iNumWaypoints].m_PackedStruct2.m_iUnused = 0;
		m_iNumWaypoints++;
		return true;
	}
	return false;
}

bool ::CWaypointRecordingRoute::InsertWaypoint(const int iIndex, const Vector3 & vPos, const float fSpeed, const float fHeading, const u16 iFlags)
{
	if(m_iNumWaypoints < ms_iMaxNumWaypoints)
	{
		memmove(&m_pWaypoints[iIndex+1], &m_pWaypoints[iIndex], sizeof(RouteData) * (m_iNumWaypoints - iIndex));

		m_pWaypoints[iIndex].m_fXYZ[0] = vPos.x;
		m_pWaypoints[iIndex].m_fXYZ[1] = vPos.y;
		m_pWaypoints[iIndex].m_fXYZ[2] = vPos.z;
		m_pWaypoints[iIndex].m_PackedStruct1.m_iMoveBlendRatio = iFlags & WptFlag_Vehicle ? CruiseSpeedtoUINT8(fSpeed) : MBRtoUINT8(fSpeed);
		m_pWaypoints[iIndex].m_PackedStruct1.m_iHeading = HDGtoUINT8(fHeading);
		m_pWaypoints[iIndex].m_PackedStruct1.m_iFlags = iFlags;
		m_pWaypoints[iIndex].m_PackedStruct2.m_iFreeSpaceOnLeft = 0;
		m_pWaypoints[iIndex].m_PackedStruct2.m_iFreeSpaceOnRight = 0;
		m_pWaypoints[iIndex].m_PackedStruct2.m_iUnused = 0;
		m_iNumWaypoints++;
		return true;
	}
	return false;
}

void ::CWaypointRecordingRoute::RemoveWaypoint(const int iWpt)
{
	if(iWpt < m_iNumWaypoints && m_iNumWaypoints > 0)
	{
		for(int w=iWpt; w<m_iNumWaypoints; w++)
		{
			sysMemCpy(&m_pWaypoints[w], &m_pWaypoints[w+1], sizeof(RouteData));
		}
	}
	m_iNumWaypoints--;
}

// Purely for recording routes into the CWaypointRecording::ms_RecordingRoute variable
void ::CWaypointRecordingRoute::AllocForRecording()
{
	if(m_pWaypoints) delete[] m_pWaypoints;
	m_pWaypoints = rage_new RouteData[ms_iMaxNumWaypoints];
}

// Check that this route is sound.  Certain configurations & ordering of flags are not ok.
bool ::CWaypointRecordingRoute::SanityCheck()
{
	bool bJumping = false;
	bool bFlipped = false;

	for(int w=0; w<m_iNumWaypoints; w++)
	{
		RouteData & wpt = m_pWaypoints[w];
		const u32 iFlags = wpt.GetFlags();

		//********************
		// Jumping & landing

		if((iFlags & WptFlag_Jump) && (iFlags & WptFlag_Land))
		{
			Assertf(false, "SanityCheck FAIL @ waypoint [%i] : cannot jump & land at the same position!", w);
			return false;
		}
		if(iFlags & WptFlag_Jump)
		{
			if(bJumping)
			{
				Assertf(false, "SanityCheck FAIL @ waypoint [%i] : cannot jump if not yet landed!", w);
				return false;
			}
			bJumping = true;
		}
		if(iFlags & WptFlag_Land)
		{
			if(!bJumping)
			{
				Assertf(false, "SanityCheck FAIL @ waypoint [%i] : cannot land if we haven't jumped!", w);
				return false;
			}
			bJumping = false;
		}

		//***************************
		// Flipping back & forwards

		if((iFlags & WptFlag_SkiFlipBackwards) && (iFlags & WptFlag_SkiFlipForwards))
		{
			Assertf(false, "SanityCheck FAIL @ waypoint [%i] : cannot flip forwards & backwards at the same position!", w);
			return false;
		}
		if(iFlags & WptFlag_SkiFlipBackwards)
		{
			if(bFlipped)
			{
				Assertf(false, "SanityCheck FAIL @ waypoint [%i] : cannot flip backwards if already skiing backwards!", w);
				return false;
			}
			bFlipped = true;
		}
		if(iFlags & WptFlag_SkiFlipForwards)
		{
			if(!bFlipped)
			{
				Assertf(false, "SanityCheck FAIL @ waypoint [%i] : cannot flip forwards if not skiing backwards!", w);
				return false;
			}
			bFlipped = false;
		}
	}

	return true;
}

#ifndef RAGEBUILDER
void ::CWaypointRecordingRoute::Draw(const bool bRecordingRoute)
{
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	const Color32 iRouteCol = bRecordingRoute ? Color_yellow : Color_red;
	const Color32 iWptCol = bRecordingRoute ? Color_cyan : Color_blue;
	const float fRouteLinesHeight = bRecordingRoute ? 0.75f : 1.0f;

	char tmp[64];
	int p;

	const float fMaxDist = 60.0f;

	Vector3 vLastPos = m_iNumWaypoints > 0 ? GetWaypoint(0).GetPosition() : VEC3_ZERO;
	float fDistanceFromStart = 0.0f;

	for(p=0; p<m_iNumWaypoints; p++)
	{
		const ::CWaypointRecordingRoute::RouteData & waypointA = GetWaypoint(p);
		const Vector3 vA = waypointA.GetPosition();

		fDistanceFromStart += (vA - vLastPos).Mag();

		grcDebugDraw::Line(vA, vA + Vector3(0.0f,0.0f,fRouteLinesHeight), iWptCol, iWptCol);

		const float fHdg = fwAngle::LimitRadianAngle(waypointA.GetHeading());
		const Vector3 vOffset(-rage::Sinf(fHdg) * 0.25f, rage::Cosf(fHdg) * 0.25f, 0.25f);
		grcDebugDraw::Line(vA + Vector3(0.0f,0.0,0.25f), vA + vOffset, iRouteCol, Color_blue);

		const float fDistAway = (vA - vOrigin).Mag();
		if(fDistAway < fMaxDist)
		{
			const float fAlpha = 1.0f - (fDistAway / fMaxDist);
			Color32 iCol;
			iCol.Set(255, 255, 255, (int)(fAlpha*255.0f));

			if (waypointA.GetFlags() && ::CWaypointRecordingRoute::WptFlag_Vehicle)
			{
				sprintf(tmp, "[%i] Spd: %.2f", p, waypointA.GetSpeedAsCruiseSpeed());
			}
			else
			{
				sprintf(tmp, "[%i] MBR: %.2f", p, waypointA.GetSpeedAsMBR());
			}
			
			grcDebugDraw::Text(vA, iCol, tmp);

			sprintf(tmp, "Dist: %.1f", fDistanceFromStart);
			grcDebugDraw::Text(vA, iCol, 0, grcDebugDraw::GetScreenSpaceTextHeight(), tmp);

			tmp[0] = 0;
			if(waypointA.GetFlags() & ::CWaypointRecordingRoute::WptFlag_Jump)
				strcat(tmp, "(Jump)");
			if(waypointA.GetFlags() & ::CWaypointRecordingRoute::WptFlag_ApplyJumpForce)
				strcat(tmp, "(ApplyJumpForce)");
			if(waypointA.GetFlags() & ::CWaypointRecordingRoute::WptFlag_Land)
				strcat(tmp, "(Land)");
			if(waypointA.GetFlags() & ::CWaypointRecordingRoute::WptFlag_SkiFlipBackwards)
				strcat(tmp, "(SkiFlipBackwards)");
			if(waypointA.GetFlags() & ::CWaypointRecordingRoute::WptFlag_SkiFlipForwards)
				strcat(tmp, "(SkiFlipForwards)");
			if(waypointA.GetFlags() & ::CWaypointRecordingRoute::WptFlag_DropDown)
				strcat(tmp, "(DropDown)");
			if(waypointA.GetFlags() & ::CWaypointRecordingRoute::WptFlag_SurfacingFromUnderwater)
				strcat(tmp, "(Surfacing)");
			if(waypointA.GetFlags() & ::CWaypointRecordingRoute::WptFlag_NonInterrupt)
				strcat(tmp, "(NonInterrupt)");
			if(waypointA.GetFlags() & ::CWaypointRecordingRoute::WptFlag_Vehicle)
				strcat(tmp, "(Vehicle)");

			grcDebugDraw::Text(vA, iCol, 0, grcDebugDraw::GetScreenSpaceTextHeight()*2, tmp);
		}

		vLastPos = vA;
	}
	for(p=0; p<m_iNumWaypoints-1; p++)
	{
		const ::CWaypointRecordingRoute::RouteData & waypointA = GetWaypoint(p);
		const ::CWaypointRecordingRoute::RouteData & waypointB = GetWaypoint(p+1);
		const Vector3 vA = waypointA.GetPosition();
		const Vector3 vB = waypointB.GetPosition();

		grcDebugDraw::Line(vA + Vector3(0.0f,0.0f,fRouteLinesHeight), vB + Vector3(0.0f,0.0f,fRouteLinesHeight), iRouteCol, iRouteCol);

		const Color32 iSideColor = bRecordingRoute ? Color_DarkGreen : Color_DarkGreen;
		Vector3 vNormal, vDelta;
		vDelta = vB - vA;
		vDelta.NormalizeSafe();
		vNormal.Cross(vDelta, Vector3(0.0f, 0.0f, 1.0f));
		if (waypointA.GetFreeSpaceToLeft() > 0.0f)
		{
			grcDebugDraw::Line(vA + Vector3(0.0f,0.0f,fRouteLinesHeight) + -vNormal*waypointA.GetFreeSpaceToLeft(), vB + Vector3(0.0f,0.0f,fRouteLinesHeight)+ -vNormal*waypointA.GetFreeSpaceToLeft(), iSideColor, iSideColor);
		}
		if (waypointA.GetFreeSpaceToRight() > 0.0f)
		{
			grcDebugDraw::Line(vA + Vector3(0.0f,0.0f,fRouteLinesHeight) + vNormal*waypointA.GetFreeSpaceToRight(), vB + Vector3(0.0f,0.0f,fRouteLinesHeight)+ vNormal*waypointA.GetFreeSpaceToRight(), iSideColor, iSideColor);
		}
	}
}

#endif
#endif