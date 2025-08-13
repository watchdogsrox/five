//
// TaskGotoPointAvoidance
// 

#pragma once

#include "ai/EntityScanner.h"

// forward declare
struct sAvoidanceSettings;

namespace TaskGotoPointAvoidance
{
	struct sAvoidanceResults
	{
		Vector3 vAvoidanceVec;
		float fOutTimeToCollide;
		float fOutCollisionSpeed;
	};

	struct sAvoidanceParams
	{
		sAvoidanceParams(const sAvoidanceSettings& in_AvoidanceSettings)
			: m_AvoidanceSettings(in_AvoidanceSettings)
		{
		}

		const sAvoidanceSettings& m_AvoidanceSettings;

		Vec3V vPedPos3d;
		Vec2V vToTarget;
		Vec2V vUnitSegment;
		Vec2V vUnitSegmentRight;
		Vec2V vSegmentStart;
		
		ScalarV fDistanceFromPath;
		ScalarV fScanDistance;
		const CPed* pPed;
		const CEntity* pDontAvoidEntity;

		bool bWandering;
		bool bShouldAvoidDeadPeds;
	};
	
	struct sPedAvoidanceArrays
	{
		sPedAvoidanceArrays()
			: iNumEntities(0)
		{}

		ScalarV fOtherEntRadii[CEntityScanner::MAX_NUM_ENTITIES];
		Vec2V vOtherEntPos[CEntityScanner::MAX_NUM_ENTITIES];
		Vec2V vOtherEntVel[CEntityScanner::MAX_NUM_ENTITIES];
		int iNumEntities;
	};

	struct sObjAvoidanceArrays
	{
		sObjAvoidanceArrays()
			: iNumEntities(0)
		{}
		Vec2V vOtherEntBoundVerts[CEntityScanner::MAX_NUM_ENTITIES][4];
		Vec2V vOtherEntVel[CEntityScanner::MAX_NUM_ENTITIES];
		int iNumEntities;
	};



	enum eAvoidanceCollisionType
	{
		eAvoidanceCollisionType_None,
		eAvoidanceCollisionType_Ped,
		eAvoidanceCollisionType_Obj,
		eAvoidanceCollisionType_Veh,
		eAvoidanceCollisionType_NavMesh,
		eNumAvoidanceCollisionType
	};



	bool ProcessAvoidance(sAvoidanceResults& o_Results, const sAvoidanceParams& in_Parameters );
}