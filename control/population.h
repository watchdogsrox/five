/////////////////////////////////////////////////////////////////////////////////
// Title	:	Population.h
// Author	:	Adam Croston
// Started	:	08/12/08
//
// To encapsulate population tests done against the keyhole shape used in the
// vehicle and ped population systems.
// It should help with consistency across the pathserver/ped pop/veh pop codebase.
/////////////////////////////////////////////////////////////////////////////////
#ifndef POPULATION_H_
#define POPULATION_H_

// rage includes
#include "vector/vector3.h"
#include "vector/color32.h"

// Framework headers
#include "fwmaths/keyholetests.h"

// game includes

using namespace rage;

////////////////////////////////////////////////////////////////////////////////

class CPopulationHelpers
{
public:
	static s32 GetConfigValue(const s32& value, const s32& baseValue);
	static void OnSystemCpuRatingChanged();
#if __BANK
	static void InitWidgets();
#endif // _BANK

private:
#if __BANK
	static float sm_CpuRating;
	static bool sm_OverrideCpuRating;
#endif // _BANK
};

#define GET_POPULATION_VALUE(name) CPopulationHelpers::GetConfigValue(CGameConfig::Get().GetConfigPopulation().m_##name, CGameConfig::Get().GetConfigPopulation().m_##name##_Base)
#define GET_INDEXED_POPULATION_VALUE(name, index) CPopulationHelpers::GetConfigValue(CGameConfig::Get().GetConfigPopulation().m_##name[index], CGameConfig::Get().GetConfigPopulation().m_##name##_Base[index])

////////////////////////////////////////////////////////////////////////////////

enum LocInfo		{ LOCATION_EXTERIOR, LOCATION_SHALLOW_INTERIOR, LOCATION_DEEP_INTERIOR };

class CPopCtrl
{
public:
	CPopCtrl() :
		m_centre			(0.0f, 0.0f, 0.0f),
		m_conversionCentre	(0.0f, 0.0f, 0.0f),
		m_baseHeading		(0.0f),
		m_FOV				(0.0f),
		m_tanHFov			(0.0f),
		m_locationInfo		(LOCATION_EXTERIOR),
		m_centreVelocity	(0.0f, 0.0f, 0.0f),
		m_inCar				(false)
	{

	}

	Vector3	m_centre;
	float	m_baseHeading;
	float	m_FOV;
	float	m_tanHFov;
	LocInfo	m_locationInfo;
	Vector3	m_centreVelocity;
	Vector3	m_conversionCentre;
	bool	m_inCar;
};

/////////////////////////////////////////////////////////////////////////////////
// CPopGenShape
// PURPOSE :	To store the population generation shape and to facilitate a
//				common point for tests against that shape.
/////////////////////////////////////////////////////////////////////////////////
class CPopGenShape
{
public:
	CPopGenShape();

	void Init(
		const Vector3&	centre,
		const Vector2&	dir,
		const float		cosHalfAngle,
		const float		innerBandRadiusMin,
		const float		innerBandRadiusMax,
		const float		outerBandRadiusMin,
		const float		outerBandRadiusMax,
		const float		sidewallThickness);

	enum GenCategory
	{
		GC_KeyHoleInnerBand_on,
		GC_KeyHoleOuterBand_on,
		GC_KeyHoleSideWall_on,
		GC_InFOV_on,
		GC_InFOV_usableIfOccluded,
		GC_Off,
		GC_KeyHoleInnerBand_off,
		GC_KeyHoleOuterBand_off,
		GC_KeyHoleSideWall_off		
	};
	static inline bool IsCategoryUsable(const GenCategory eCat) { return (eCat < GC_Off); }
	GenCategory CategorisePoint(const Vector2& p0) const;
	GenCategory CategoriseLink(const Vector2& p0, const Vector2& p1) const;
	inline void GetBoundingMinMax(Vector2 & vMin, Vector2 & vMax)
	{
		vMin = Vector2(m_centre.x - m_outerBandRadiusMax, m_centre.y - m_outerBandRadiusMax);
		vMax = Vector2(m_centre.x + m_outerBandRadiusMax, m_centre.y + m_outerBandRadiusMax);
	}
	inline Vector3 GetCenter() const { return m_centre; }
	inline Vector2 GetDir() const { return m_dir; }
	inline Vector2 GetSidewallFrustumOffset() const { return m_sidewallFrustumOffset; }
	inline float GetCosHalfAngle() const { return m_cosHalfAngle; }
	inline float GetInnerBandRadiusMin() const { return m_innerBandRadiusMin; }
	inline float GetInnerBandRadiusMax() const { return m_innerBandRadiusMax; }
	inline float GetOuterBandRadiusMin() const { return m_outerBandRadiusMin; }
	inline float GetOuterBandRadiusMax() const { return m_outerBandRadiusMax; }
	inline float GetSidewallThickness() const { return m_sidewallThickness; }
	inline void SetInnerBandRadiusMin(float f) { m_innerBandRadiusMin = f; }
	inline void SetInnerBandRadiusMax(float f) { m_innerBandRadiusMax = f; }
	inline void SetOuterBandRadiusMin(float f) { m_outerBandRadiusMin = f; }
	inline void SetOuterBandRadiusMax(float f) { m_outerBandRadiusMax = f; }

#if __BANK
	void Draw(bool bInWorld, bool bOnVectorMap, const Vector3& vPopGenCenter, float fZoneScale=1.0f);
	void DrawTestCategories(float fRange, float fGridSpacing);
	static void DrawDebugWedge(
		const Vector3&	popCtrlCentre,
		float			radiusInner,
		float			radiusOuter,
		float			thetaStart,
		float			thetaEnd,
		s32			numSegments,
		const Color32	colour,
		bool bVectorMap, bool bWorld);
#endif

protected:
	Vector3	m_centre;
	Vector2	m_dir;
	Vector2	m_sidewallFrustumOffset;
	float	m_halfAngle;
	float	m_cosHalfAngle;
	float	m_innerBandRadiusMin;
	float	m_innerBandRadiusMax;
	float	m_outerBandRadiusMin;
	float	m_outerBandRadiusMax;
	float	m_sidewallThickness;
};


#endif // POPULATION_H_
