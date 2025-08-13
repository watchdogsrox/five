/************************************************
 * PopMultiplierAreas.h                         *
 * ----------									*
 *											    *
 * declarations for					            *
 * scriptable pop multpliers control for coop	*
 *												*
 * ADW 06/06/11 (Modified from CarGen.cpp / .h	*
 *												*
 * (c)2011 Rockstar North						*
 ************************************************/

#ifndef POPMULTIPLIERAREAS_H
#define POPMULTIPLIERAREAS_H

// system headers
#include "system/spinlock.h"

//---------------------------------
//
//---------------------------------

#define MAX_POP_MULTIPLIER_AREAS (15)

//---------------------------------
//
//---------------------------------

struct PopMultiplierArea
{
	friend class CThePopMultiplierAreas;

public:

	static const float MINIMUM_AREA_WIDTH;
	static const float MINIMUM_AREA_HEIGHT;
	static const float MINIMUM_AREA_DEPTH;
	static const float MINIMUM_AREA_RADIUS;

	PopMultiplierArea()
	:
		m_init(false),
        m_isSphere(false),
		m_bCameraGlobalMultiplier(false),
		m_pedDensityMultiplier(1.0f),
		m_trafficDensityMultiplier(1.0f),
		m_minWS(FLT_MAX, FLT_MAX, FLT_MAX),
		m_maxWS(-FLT_MAX, -FLT_MAX, -FLT_MAX)
	{}

	PopMultiplierArea(Vector3 const& _minWS, Vector3 const& _maxWS, float _pedMultiplier, float _trafficMultiplier, bool bCameraGlobalMultiplier)
	:
		m_init(true),
		m_isSphere(false),
		m_minWS(_minWS),
		m_maxWS(_maxWS),
		m_pedDensityMultiplier(_pedMultiplier),
		m_trafficDensityMultiplier(_trafficMultiplier),
		m_bCameraGlobalMultiplier(bCameraGlobalMultiplier)
	{}

	PopMultiplierArea(const Vector3& center, float radius, float pedMultiplier, float trafficMultiplier, bool bCameraGlobalMultiplier)
	:
		m_init(true),
		m_isSphere(true),
		m_minWS(center),
		m_pedDensityMultiplier(pedMultiplier),
		m_trafficDensityMultiplier(trafficMultiplier),
		m_bCameraGlobalMultiplier(bCameraGlobalMultiplier)
	{ m_maxWS.x = radius; }

	void DebugRender();

	bool operator==(PopMultiplierArea const& rhs) const;

	inline bool IsInit(void) const { return m_init; }

	void Reset(void);

	void Init(Vector3 const& _minWS, Vector3 const& _maxWS, float _pedMultiplier, float _trafficMultiplier, bool bCameraGlobalMultiplier);
	void Init(const Vector3& center, float radius, float pedMultiplier, float trafficMultiplier, bool bCameraGlobalMultiplier);

	Vector3 GetCentreWS(void) const;
	inline Vector3 const & GetMinWS() const { return m_minWS; }
	inline Vector3 const & GetMaxWS() const { return m_maxWS; }
	float GetRadius() const { return m_maxWS.x; }

	bool IsSphere() const { return m_isSphere; }

	Vector3 m_minWS;	// used as center for spheres
	Vector3 m_maxWS;	// x is used as radius for spheres

	float m_pedDensityMultiplier;
	float m_trafficDensityMultiplier;

	bool m_init;
	bool m_isSphere;
	bool m_bCameraGlobalMultiplier;
};

//---------------------------------
//
//---------------------------------

class CThePopMultiplierAreas
{
public:

	static const s32 INVALID_AREA_INDEX;
	static PopMultiplierArea* GetPopMultiplierAreaArray() { return PopMultiplierAreaArray; }

private:

	static PopMultiplierArea PopMultiplierAreaArray[MAX_POP_MULTIPLIER_AREAS];

	static s32 ms_iNumAreas;
	static s32 ms_iNumPedAreas;
	static s32 ms_iNumTrafficAreas;

public :
#if !__FINAL
	static bool gs_bDisplayPopMultiplierAreas;
#endif

	static void Init(unsigned int InitMode);
	static void InitLevelWithMapLoaded(void);
	static void Update(void);
	static void Reset();
	
	static s32 CreatePopMultiplierArea(const Vector3 & _min, const Vector3 & _max, float const _pedDensityMultipier, float const _trafficDensityMultiplier, const bool bCameraGlobalMultiplier = true);
	static s32 CreatePopMultiplierArea(const Vector3& center, float radius, float pedDensityMultipier, float trafficDensityMultiplier, const bool bCameraGlobalMultiplier = true);
	static void RemovePopMultiplierArea(const Vector3 & _min, const Vector3 & _max, bool const isSphere, float const _pedDensityMultipier, float const _trafficDensityMultiplier, const bool bCameraGlobalMultiplier = true);
	static void SetAllPopMultiplierAreasBackToActive();

	static float GetTrafficDensityMultiplier(Vector3 const & vPos, const bool bCameraGlobalMultiplier = true);
	static float GetPedDensityMultiplier(Vector3 const & vPos, const bool bCameraGlobalMultiplier = true);

	static int GetLastActiveAreaIndex_DebugOnly();
	static const PopMultiplierArea * GetActiveArea(u32 const _index);
	static s32 GetNumAreas(void) { return ms_iNumAreas; } 
	static s32 GetNumPedAreas(void) { return ms_iNumPedAreas; }
	static s32 GetNumTrafficAreas(void) { return ms_iNumTrafficAreas; }
};

//---------------------------------
//
//---------------------------------

#endif /* POPMULTIPLIERAREAS_H */