#ifndef INC_BLOCKING_BOUNDS_H
#define INC_BLOCKING_BOUNDS_H

// Rage headers
#include "vector/vector3.h"
#include "script/thread.h"

////////////////////////////////////////////////////////////////////////////////
// CScenarioBlockingArea
////////////////////////////////////////////////////////////////////////////////

class CScenarioBlockingArea
{
public:
	CScenarioBlockingArea() : m_bValid(false), m_bShouldCancelActiveScenarios(true), m_bBlocksPeds(true), m_bBlocksVehicles(true) {}
	~CScenarioBlockingArea() {}

	// Initialises the bounding box with the center and extents specified
	void InitFromMinMax(Vector3& vMin, Vector3& vMax );

	// Returns true if the box contains the point
	bool ContainsPoint(const Vector3& vPoint) const;

	void SetShouldCancelActiveScenarios(bool bShouldCancel)		{ m_bShouldCancelActiveScenarios = bShouldCancel; }

	// Returns true if the point should cause peds already using scenarios in its area to exit.
	bool ShouldCancelActiveScenarios() const					{ return m_bShouldCancelActiveScenarios; }

	inline void GetMin(Vector3& vMin) const						{ vMin = m_vBoundingMin; }
	inline void GetMax(Vector3& vMax) const						{ vMax = m_vBoundingMax; }
	inline void SetMin(const Vector3& vMin)						{ m_vBoundingMin = vMin; }
	inline void SetMax(const Vector3& vMax)						{ m_vBoundingMax = vMax; }
	inline void Invalidate()									{ m_bValid = false; }
	inline bool IsValid() const									{ return m_bValid; }

	bool AffectsEntities(bool peds, bool vehicles) const		{ return (peds && m_bBlocksPeds) || (vehicles && m_bBlocksVehicles); }

	bool GetBlocksPeds() const									{ return m_bBlocksPeds; }
	bool GetBlocksVehicles() const								{ return m_bBlocksVehicles; }

	void SetBlocksPeds(bool b)									{ m_bBlocksPeds = b; }
	void SetBlocksVehicles(bool b)								{ m_bBlocksVehicles = b; }

private:

	Vector3 m_vBoundingMin;
	Vector3 m_vBoundingMax;
	bool	m_bValid;
	bool	m_bShouldCancelActiveScenarios;
	bool	m_bBlocksPeds;
	bool	m_bBlocksVehicles;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioBlockingAreas
////////////////////////////////////////////////////////////////////////////////

class CScenarioBlockingAreas
{
public:
	enum BlockingAreaUserType
	{
		kUserTypeNone,
		kUserTypeCutscene,
		kUserTypeScript,
		kUserTypeNetScript,
		kUserTypeDebug,

		kNumUserTypes
	};

#if __BANK
	struct DebugInfo
	{
		ConstString				m_UserName;
		BlockingAreaUserType	m_UserType;
	};
#endif	// __BANK

	static const int kScenarioBlockingAreaIdInvalid = 0;

	static int AddScenarioBlockingArea(Vector3& vMin, Vector3& vMax, bool bShouldCancelActiveScenarios, bool bBlocksPeds, bool bBlocksVehicles, BlockingAreaUserType debugUserType, const char* debugUserName = NULL);
	static void RemoveScenarioBlockingArea( int id );
    static bool IsScenarioBlockingAreaValid( int id );
	static bool IsPointInsideBlockingAreas(const Vector3& vPoint, bool forPed, bool forVehicle);
	static bool ShouldScenarioQuitFromBlockingAreas(const Vector3& vPoint, bool forPed, bool forVehicle);
	static bool IsScenarioBlockingAreaExists(Vector3& vMin, Vector3& vMax);

	static int IdToIndex(int id) { return id - 1; }
	static int IndexToId(int index) { return index + 1; }

#if __BANK
	static void RenderScenarioBlockingAreas();
	static bool	ms_bRenderScenarioBlockingAreas;

	static void DumpDebugInfo();
	static void AddAroundPlayer();
	static void ClearAll();
#endif // __BANK

private:

	static const s32 MAX_SCENARIO_BLOCKING_AREAS = 45;
	static CScenarioBlockingArea ms_aBlockingAreas[MAX_SCENARIO_BLOCKING_AREAS];

#if __BANK
	static DebugInfo ms_aBlockingAreaDebugInfo[MAX_SCENARIO_BLOCKING_AREAS];
#endif	// __BANK
};

#endif // INC_BLOCKING_BOUNDS_H
