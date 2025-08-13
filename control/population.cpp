/////////////////////////////////////////////////////////////////////////////////
// Title	:	Population.cpp
// Author	:	Adam Croston
// Started	:	08/12/08
//
// To encapsulate population tests done against the keyhole shape used in the
// vehicle and ped population systems.
// It should help with consistency across the pathserver/ped pop/veh pop codebase.
/////////////////////////////////////////////////////////////////////////////////

// Framework Headers
#include "fwmaths/keyholetests.h"

// Game includes
#include "population.h"
#if !__SPU // including this file in VehPopUpdateDensitiesJob.cpp
#include "debug/vectormap.h"
#include "peds/PlayerInfo.h"
#include "peds/Ped.h"
#include "Peds/pedpopulation.h"
#include "vehicleAi/VehicleAILodManager.h"
#include "vehicles/vehiclepopulation.h"
#endif
AI_OPTIMISATIONS()

#if !__SPU
#if __BANK
float CPopulationHelpers::sm_CpuRating = 1.0f;
bool CPopulationHelpers::sm_OverrideCpuRating = false;
#endif // _BANK

s32 CPopulationHelpers::GetConfigValue(const s32& value, const s32& baseValue)
{
	float cpuRating = CSettingsManager::GetInstance().GetSettings().m_graphics.m_CityDensity;

#if __BANK
	if (sm_OverrideCpuRating)
	{
		cpuRating = sm_CpuRating;
	}
#endif // __BANK

	return static_cast<s32>(ceilf(baseValue + ((value - baseValue) * cpuRating)));
}

void CPopulationHelpers::OnSystemCpuRatingChanged()
{
	CPedPopulation::InitValuesFromConfig();
	CVehicleAILodManager::InitValuesFromConfig();
	CVehiclePopulation::InitValuesFromConfig();
}

#if __BANK
void CPopulationHelpers::InitWidgets()
{
	bkBank &bank = BANKMGR.CreateBank("Population Helpers");

	bank.AddToggle("Override CPU rating", &sm_OverrideCpuRating, OnSystemCpuRatingChanged);
	bank.AddSlider("CPU rating", &sm_CpuRating, 0.0f, 1.0f, 0.01f, OnSystemCpuRatingChanged);
}
#endif
#endif // !__SPU

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CPopGenShape
// PURPOSE :	To properly initialize a CPopGenShape object.
// PARAMETERS :	centre - the center of population generation.
//				dir - the heading direction to guide the generation.
//				cosHalfAngle - the cosine of the half view angle.
//				innerBandRadiusMin - the radius of the inner band min.
//				innerBandRadiusMax - the radius of the inner band max.
//				outerBandRadiusMin - the radius of the outer band min.
//				outerBandRadiusMax - the radius of the outer band max.
//				sidewallThickness - the width of the keyhole side walls.
// RETURNS :	Nothing.
/////////////////////////////////////////////////////////////////////////////////
CPopGenShape::CPopGenShape()
{
	m_centre.Zero();
	m_dir.Zero();
	m_sidewallFrustumOffset.Zero();
	m_halfAngle = 0.0f;
	m_cosHalfAngle = 0.0f;
	m_innerBandRadiusMin = 0.0f;
	m_innerBandRadiusMax = 0.0f;
	m_outerBandRadiusMin = 0.0f;
	m_outerBandRadiusMax = 0.0f;
	m_sidewallThickness = 0.0f;
}

void CPopGenShape::Init(
		const Vector3&	centre,
		const Vector2&	dir,
		const float		cosHalfAngle,
		const float		innerBandRadiusMin,
		const float		innerBandRadiusMax,
		const float		outerBandRadiusMin,
		const float		outerBandRadiusMax,
		const float		sidewallThickness)
{
	m_centre = centre;
	m_dir = dir;
	m_cosHalfAngle = cosHalfAngle;
	m_innerBandRadiusMin = innerBandRadiusMin;
	m_innerBandRadiusMax = innerBandRadiusMax;
	m_outerBandRadiusMin = outerBandRadiusMin;
	m_outerBandRadiusMax = outerBandRadiusMax;
	m_sidewallThickness = sidewallThickness;

	m_halfAngle = rage::Acosf(cosHalfAngle);
	float sinHalfAngle = rage::Sinf(m_halfAngle);
	Assert(sinHalfAngle > 0.0f);
	m_sidewallFrustumOffset = m_dir * (m_sidewallThickness / sinHalfAngle);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CategorisePoint
// PURPOSE :	To determine where a point lies relative the the pop gen shape.
// PARAMETERS :	p0 - the point to test.
// RETURNS :	The categorization type.
/////////////////////////////////////////////////////////////////////////////////
CPopGenShape::GenCategory CPopGenShape::CategorisePoint(const Vector2& p0) const
{
	const Vector2 center(m_centre.x, m_centre.y);

	if(!DoesPointTouchBand(center, 0.0f, m_outerBandRadiusMax, p0))
	{
		return GC_Off;
	}

	if(DoesPointTouchBand(center, m_innerBandRadiusMin, m_innerBandRadiusMax, p0))
	{
		const bool doesLineTouchViewFrustum			= DoesPointTouchRayWedge(center, m_dir, m_cosHalfAngle, p0);

		// The point touches the inner band...
		if(doesLineTouchViewFrustum)
		{
			return GC_InFOV_usableIfOccluded;
		}
		else
		{
			return GC_KeyHoleInnerBand_on;
		}
	}
	else if(DoesPointTouchBand(center, m_outerBandRadiusMin, m_outerBandRadiusMax, p0))
	{
		const bool doesLineTouchViewFrustumOffset	= DoesPointTouchRayWedge(center - m_sidewallFrustumOffset, m_dir, m_cosHalfAngle, p0);
		const bool doesLineTouchViewFrustum			= doesLineTouchViewFrustumOffset && DoesPointTouchRayWedge(center, m_dir, m_cosHalfAngle, p0);

		// The point touches the outer band...
		if(doesLineTouchViewFrustum)
		{
			return GC_KeyHoleOuterBand_on;
		}
		else if(doesLineTouchViewFrustumOffset)
		{
			// The point touches the sidewalls, but at the edges of the outer band...
			return GC_KeyHoleSideWall_on;
		}
		else
		{
			return GC_KeyHoleOuterBand_off;
		}
	}
	else if(DoesPointTouchBand(center, m_innerBandRadiusMax, m_outerBandRadiusMin, p0))
	{
		const bool doesLineTouchViewFrustumOffset	= DoesPointTouchRayWedge(center - m_sidewallFrustumOffset, m_dir, m_cosHalfAngle, p0);
		const bool doesLineTouchViewFrustum			= doesLineTouchViewFrustumOffset && DoesPointTouchRayWedge(center, m_dir, m_cosHalfAngle, p0);

		// The point touches the mid-range band...
		if(doesLineTouchViewFrustum )
		{
			return GC_InFOV_usableIfOccluded;
		}
		else if(doesLineTouchViewFrustumOffset)
		{
			// The point touches the mid-range sidewalls...
			return GC_KeyHoleSideWall_on;
		}
		else
		{
			return GC_Off;
		}
	}
	else
	{
		return GC_Off;
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CategoriseLink
// PURPOSE :	To determine how a link lies relative the the pop gen shape.
// PARAMETERS :	p0 - the start point of the link to test.
//				p1 - the end point of the link to test.
// RETURNS :	The categorization type.
/////////////////////////////////////////////////////////////////////////////////
CPopGenShape::GenCategory CPopGenShape::CategoriseLink(const Vector2& p0, const Vector2& p1) const
{
	const Vector2 center(m_centre.x, m_centre.y);

	float radii[4] = { m_innerBandRadiusMin, m_innerBandRadiusMax, m_outerBandRadiusMin, m_outerBandRadiusMax };
	int batchRet = DoesLineTouchDoubleBand(center, radii, p0,  p1);

	if (batchRet == -1)
	{
		return GC_Off;
	}
	
	const bool doesLineTouchViewFrustumOffset	= DoesLineTouchRayWedge(center - m_sidewallFrustumOffset, m_dir, m_cosHalfAngle, p0, p1);
	const bool doesLineTouchViewFrustum			= doesLineTouchViewFrustumOffset && DoesLineTouchRayWedge(center, m_dir, m_cosHalfAngle, p0, p1);

	switch (batchRet)
	{
	case 1:
		if(doesLineTouchViewFrustum)
		{
			return GC_InFOV_usableIfOccluded;
		}
		else
		{
			return GC_KeyHoleInnerBand_on;
		}
	case 2:
		// The link touches the outer band...
		if(doesLineTouchViewFrustum)
		{
			return GC_KeyHoleOuterBand_on;
		}
		else if(doesLineTouchViewFrustumOffset)
		{
			// The link touches the sidewalls, but at the edges of the outer band...
			return GC_KeyHoleSideWall_on;
		}
		else
		{
			return GC_KeyHoleOuterBand_off;
		}
	case 3:
		// The link touches the mid-range band...
		if(doesLineTouchViewFrustum )
		{
			return GC_InFOV_usableIfOccluded;
		}
		else if(doesLineTouchViewFrustumOffset)
		{
			// The link touches the mid-range sidewalls...
			return GC_KeyHoleSideWall_on;
		}
		else
		{
			return  GC_Off;
		}
	default:
		return GC_Off;
	}
}

#if __BANK && !__SPU

void CPopGenShape::DrawTestCategories(float fRange, float fGridSpacing)
{
	Vector2 vMin(m_centre.x, m_centre.y);
	Vector2 vMax(vMin);
	vMin.x -= fRange;
	vMin.y -= fRange;
	vMax.x += fRange;
	vMax.y += fRange;

	Vector2 vPosToSample(vMin);
	while (vPosToSample.y < vMax.y)
	{
		vPosToSample.x = vMin.x;
		while (vPosToSample.x < vMax.x)
		{
			CPopGenShape::GenCategory cat = CategorisePoint(vPosToSample);
			Color32 col;
			const char *pString=0;
			switch(cat)
			{
			case GC_KeyHoleInnerBand_on:
				pString = "IB";
				col = Color_green;
				break;
			case GC_KeyHoleOuterBand_on:
				pString = "OB";
				col = Color_green;
				break;
			case GC_KeyHoleSideWall_on:
				pString = "SW";
				col = Color_green;
				break;
			case GC_InFOV_on:
				pString = "FOV";
				col = Color_green;
				break;
			case GC_InFOV_usableIfOccluded:
				pString = "UIO";
				col = Color_yellow;
				break;
			case GC_Off:
				pString = "Off";
				col = Color_red;
				break;
			case GC_KeyHoleInnerBand_off:
				pString = "IB";
				col = Color_red;
				break;
			case GC_KeyHoleOuterBand_off:
				pString = "OB";
				col = Color_red;
				break;
			case GC_KeyHoleSideWall_off:
				pString = "SW";
				col = Color_red;
				break;
			}
			if (pString)
			{
				Vector3 vPos(m_centre);
				vPos.x = vPosToSample.x;
				vPos.y = vPosToSample.y;
				CVectorMap::DrawString(vPos, pString, col, true);
			}
			vPosToSample.x += fGridSpacing;
		}
		vPosToSample.y += fGridSpacing;
	}
}

void CPopGenShape::Draw(bool bInWorld, bool bOnVectorMap, const Vector3& vPopGenCenter, float fZoneScale)
{
	// Represent the generation ranges.
	{
		//************************************************************
		// Out of view
		// The area behind the player, which is closer to the origin

		float fBaseHeading = fwAngle::GetRadianAngleBetweenPoints(m_dir.x, m_dir.y, 0.0f, 0.0f);
		float fHeading = fBaseHeading + PI;
		float fAngleSwept = TWO_PI - (m_halfAngle * 2.0f);
		float fWedgeRadiusInner = m_innerBandRadiusMin * fZoneScale;
		float fWedgeRadiusOuter = m_innerBandRadiusMax * fZoneScale;
		float fWedgeThetaStart = fHeading - (fAngleSwept / 2.0f);
		float fWedgeThetaEnd = fWedgeThetaStart + fAngleSwept;
		s32 iWedgeNumSegs = 15;
		Color32 iWedgeCol(0x00,0xff,0x00,0x30);

		// Draw it.
		DrawDebugWedge(
			vPopGenCenter,
			fWedgeRadiusInner,
			fWedgeRadiusOuter,
			fWedgeThetaStart,
			fWedgeThetaEnd,
			iWedgeNumSegs,
			iWedgeCol,
			bOnVectorMap,
			bInWorld);
	}
	{
		//*********************************************************************
		// In view
		// The area in front of the player, which is further from the origin

		float fHeading = fwAngle::GetRadianAngleBetweenPoints(m_dir.x, m_dir.y, 0.0f, 0.0f);
		float fAngleSwept = m_halfAngle * 2.0f;
		float fWedgeRadiusInner = m_outerBandRadiusMin * fZoneScale;
		float fWedgeRadiusOuter = m_outerBandRadiusMax * fZoneScale;
		float fWedgeThetaStart = fHeading - (fAngleSwept / 2.0f);
		float fWedgeThetaEnd = fWedgeThetaStart + fAngleSwept;
		s32 iWedgeNumSegs = 15;
		Color32 iWedgeCol(0x00,0xff,0x00,0x30);

		// Draw it.
		DrawDebugWedge(
			vPopGenCenter,
			fWedgeRadiusInner,
			fWedgeRadiusOuter,
			fWedgeThetaStart,
			fWedgeThetaEnd,
			iWedgeNumSegs,
			iWedgeCol,
			bOnVectorMap,
			bInWorld);
	}

	//TMS:	When debugging the original attempt to display the sidewalls which fell 
	//		apart at different fovs I thought it possible that the sidewall wedge
	//		doesn't really centre on m_Centre. Because the outer edge is offset
	//		from m_Centre the origin of the wedge formed is going to move quite 
	//		far away at time.....
	//		Eventually I realized - actually. Its not a wedge...
	//			Its a shape with two parallel sides bounded by the inner and outer radii
	//			Slightly more complicated to display so I've started with a simple quad
	//			I added DrawTestCategories which wont fall apart in the same way.
	//			and left drawing the more complicated shape for the future.
	//Right
	{
		float fHeading = fwAngle::GetRadianAngleBetweenPoints(m_dir.x, m_dir.y, 0.0f, 0.0f);
		float fAngleSwept = m_halfAngle * 2.0f;
		float fWedgeThetaStart = fHeading - (fAngleSwept / 2.0f);

		//const float dirInnerEdge		= rage::Acosf(fWedgeThetaStart);
		const float	sinInnerOffset		= rage::Sinf(fWedgeThetaStart);
		const float	cosInnerOffset		= rage::Cosf(fWedgeThetaStart);
		Vector3 vSegOneInner(-sinInnerOffset, cosInnerOffset, 0.0f);
		vSegOneInner *= m_innerBandRadiusMax * fZoneScale;
		vSegOneInner += vPopGenCenter;
		Vector3 vSegOneOuter(-sinInnerOffset, cosInnerOffset, 0.0f);
		vSegOneOuter *= m_outerBandRadiusMax * fZoneScale;
		vSegOneOuter += vPopGenCenter;
		
		//Offset to side
		Vector3 vPerp(cosInnerOffset, sinInnerOffset, 0.0f);
		vPerp *= m_sidewallThickness;

		Vector3 vSegTwoInner(vSegOneInner);
		vSegTwoInner += vPerp;

		Vector3 vSegTwoOuter(vSegOneOuter);
		vSegTwoOuter += vPerp;

		Color32 iWedgeCol(0xdd,0xdd,0x00,0x30);
		if(bInWorld)
		{
			grcDebugDraw::Poly(RCC_VEC3V(vSegTwoOuter), RCC_VEC3V(vSegOneOuter), RCC_VEC3V(vSegOneInner), iWedgeCol);
			grcDebugDraw::Poly(RCC_VEC3V(vSegOneInner), RCC_VEC3V(vSegTwoInner), RCC_VEC3V(vSegTwoOuter), iWedgeCol);
		}
		if(bOnVectorMap)
		{
			CVectorMap::DrawPoly(vSegOneInner, vSegOneOuter, vSegTwoOuter, iWedgeCol);
			CVectorMap::DrawPoly(vSegTwoOuter, vSegTwoInner, vSegOneInner, iWedgeCol);
		}
	}

	//Left
	{
		float fHeading = fwAngle::GetRadianAngleBetweenPoints(m_dir.x, m_dir.y, 0.0f, 0.0f);
		float fAngleSwept = m_halfAngle * 2.0f;
		float fWedgeThetaStart = fHeading + (fAngleSwept / 2.0f);

		//const float dirInnerEdge		= rage::Acosf(fWedgeThetaStart);
		const float	sinInnerOffset		= rage::Sinf(fWedgeThetaStart);
		const float	cosInnerOffset		= rage::Cosf(fWedgeThetaStart);
		Vector3 vSegOneInner(-sinInnerOffset, cosInnerOffset, 0.0f);
		vSegOneInner *= m_innerBandRadiusMax * fZoneScale;
		vSegOneInner += vPopGenCenter;
		Vector3 vSegOneOuter(-sinInnerOffset, cosInnerOffset, 0.0f);
		vSegOneOuter *= m_outerBandRadiusMax * fZoneScale;
		vSegOneOuter += vPopGenCenter;

		//Offset to side
		Vector3 vPerp(cosInnerOffset, sinInnerOffset, 0.0f);
		vPerp *= -m_sidewallThickness;

		Vector3 vSegTwoInner(vSegOneInner);
		vSegTwoInner += vPerp;

		Vector3 vSegTwoOuter(vSegOneOuter);
		vSegTwoOuter += vPerp;

		Color32 iWedgeCol(0xdd,0xdd,0x00,0x30);
		if(bInWorld)
		{
			grcDebugDraw::Poly(RCC_VEC3V(vSegOneInner), RCC_VEC3V(vSegOneOuter), RCC_VEC3V(vSegTwoOuter), iWedgeCol);
			grcDebugDraw::Poly(RCC_VEC3V(vSegTwoOuter), RCC_VEC3V(vSegTwoInner), RCC_VEC3V(vSegOneInner), iWedgeCol);
		}
		if(bOnVectorMap)
		{
			CVectorMap::DrawPoly(vSegOneInner, vSegOneOuter, vSegTwoOuter, iWedgeCol);
			CVectorMap::DrawPoly(vSegTwoOuter, vSegTwoInner, vSegOneInner, iWedgeCol);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// TODO: Document this function.
/////////////////////////////////////////////////////////////////////////////////
void CPopGenShape::DrawDebugWedge(
					const Vector3&	popCtrlCentre,
					float			radiusInner,
					float			radiusOuter,
					float			thetaStart,
					float			thetaEnd,
					s32			numSegments,
					const Color32	colour,
					bool bVectorMap, bool bWorld)
{
	if(bVectorMap)
	{
		CVectorMap::DrawWedge(
			popCtrlCentre,
			radiusInner,
			radiusOuter,
			thetaStart,
			thetaEnd,
			numSegments,
			colour);
	}
	if(bWorld)
	{
		// Draw the wedge.
		const float		thetaStep	= (thetaEnd - thetaStart) / static_cast<float>(numSegments);
		for(int i = 0; i < numSegments; ++i)
		{
			const float		theta0	= thetaStart + static_cast<float>(i) * thetaStep;
			const float		theta1	=  theta0 + thetaStep;

			const float		st0 = rage::Sinf(theta0);
			const float		ct0 = rage::Cosf(theta0);
			const float		st1 = rage::Sinf(theta1);
			const float		ct1 = rage::Cosf(theta1);

			const float		r0x0	= -radiusInner * st0;
			const float		r0y0	= radiusInner * ct0;
			const Vector3	r0p0	(popCtrlCentre.x + r0x0, popCtrlCentre.y + r0y0, popCtrlCentre.z);

			const float		r0x1	= -radiusInner * st1;
			const float		r0y1	= radiusInner * ct1;
			const Vector3	r0p1	(popCtrlCentre.x + r0x1, popCtrlCentre.y + r0y1, popCtrlCentre.z);

			const float		r1x0	= -radiusOuter * st0;
			const float		r1y0	= radiusOuter * ct0;
			const Vector3	r1p0	(popCtrlCentre.x + r1x0, popCtrlCentre.y + r1y0, popCtrlCentre.z);

			const float		r1x1	= -radiusOuter * st1;
			const float		r1y1	= radiusOuter * ct1;
			const Vector3	r1p1	(popCtrlCentre.x + r1x1, popCtrlCentre.y + r1y1, popCtrlCentre.z);

			grcDebugDraw::Poly(RCC_VEC3V(r1p0), RCC_VEC3V(r0p1), RCC_VEC3V(r0p0), colour);
			grcDebugDraw::Poly(RCC_VEC3V(r1p1), RCC_VEC3V(r0p1), RCC_VEC3V(r1p0), colour);
		}
	}
}

#endif // BANK and !__SPU
