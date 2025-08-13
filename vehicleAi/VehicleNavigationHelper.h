#ifndef VEHICLE_NAGIVATION_HELPER_H
#define VEHICLE_NAGIVATION_HELPER_H

#include "vector/vector3.h"
#include "Task/System/TaskHelpers.h"

class CVehicle;
class CEntity;

//////////////////////////////////////////////////////////////////////////
// Class provides several static functions for checking navigation around terrain and objects
// Can also be added to a class to provide functionality to navigate to a target position without using navmesh/routing
//////////////////////////////////////////////////////////////////////////
class VehicleNavigationHelper
{

public:

	enum SearchType
	{
		eSearchStart,
		eSearchBasic,
		eSearchCircle,
		eSearchDetailed,
		eSearchCount
	};

	VehicleNavigationHelper();
	~VehicleNavigationHelper();

	typedef float (*fp_HeightQuery)(float x, float y);
	static float ComputeTargetZToAvoidHeightMap(const Vector3& vPosition, float fSpeed, const Vector3& vTargetPosition, float minHeight, 
											fp_HeightQuery in_fpHeightQuery, bool checkCurrentPosition = true, bool heightAtFullDistance = true);

	static bool CanNavigateOverObject(const Vector3& collisionPoint, const Vector3& end, int numProbes = 10, float depthOffset = -5.0f);

	static bool CheckHasLineOfSight(const Vector3& start, const Vector3& end, const CEntity* pSub, const CEntity* pTargetVehicle, Vector3& collidePoint);
	static bool WorldCollisionTest(Vector3& intersectionPoint_out, const Vector3 &vStart, const Vector3 &vEnd, const CEntity* pException);
	
	static void HelperExcludeCollisionEntities(WorldProbe::CShapeTestDesc& o_ShapeTest, const CEntity& in_Entity);
	
	void Init(const CVehicle* vehicle, const CEntity* pTarget, const Vector3& currentTarget);
	
	bool CanNavigateStraightTo(const Vector3& currentPos, const Vector3& targetPos);

	const Vector3& GetGoToTarget() const { return m_vCurrentGoToTarget; }
	void SetSearchTime(float fTime) { m_fSearchTime = fTime; }
	void SetSightDistance(float fDist) { m_fSightDistance = fDist; }
		 
	void Update(float fTimeStep, const Vector3& targetPos, bool forceUpdate = false);
	void LimitEndByDistance(const Vector3& startPos, Vector3& endPos, float distanceLimit);
	bool UseLastKnownOverSearch(const Vector3& currentPos);
	void CalculateNewIntermediaryTarget(const Vector3& currentPos);
	void UpdateSearchResults();

#if !__FINAL
	void Debug() const;
#endif 

protected:

	Vector3 m_vTargetPosition;
	Vector3 m_vLastValidTarget;
	Vector3 m_vCurrentGoToTarget;
	float m_fSearchTimer;
	float m_fSearchTime;
	float m_fSightDistance;
	float m_fArrivalDistance;
	const CVehicle* m_pVehicle;
	const CEntity* m_pTarget;

	bool m_bIntermediaryGoToValid;
	bool m_bIsPerformingSearch;
	bool m_bStartNewSearch;

	SearchType m_currentSearchType;

	static int s_iMaxNumProbes;
	WorldProbe::CShapeTestSingleResult* m_navigationProbeResults;
	Vector3* m_navigationTargets;
	int m_iNumProbesUsed;
};

#endif