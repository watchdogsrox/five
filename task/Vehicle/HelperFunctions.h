#ifndef HELPER_FUNC_H
#define HELPER_FUNC_H

#include "Peds/Horse/horseDefines.h"

#if ENABLE_HORSE

#include "grcore/debugdraw.h"
#include "vector/matrix34.h"

class CPed;
class CPhysical;

#include "grcore/debugdraw.h"
#include "vector/matrix34.h"

namespace HelperFunctions
{
	struct bhCreatureInfo
	{	
		float Dist2;
		CPhysical *Actor;
		const Matrix34 *Matrix;

		bhCreatureInfo() : Dist2(-1),Actor(NULL), Matrix(NULL)
		{	}
	};

	enum bhSpatialArrayActorTypes
	{
		kTypeFlagCreature,
		kTypeFlagHorse,
		kTypeFlagVehicle
	};

	//PURPOSE:  do the newer, simpler speed matching
	//	returns ABSOLUTE output speed
	//	Give maxSpeedAboveTarget or maxSpeedAbs a negative value for no limit
	float UpdateSpeedMatchSimple(float distToTargetAhead, const CPed* pPed, const CPhysical* pTarget);
	float ComputeNormalizedSpeed(const CPed* pPed, float fSpeed);
	float ClampNormalizedSpeed(float fNormalizedSpeed, const CPed* pPed, const CPhysical* pTarget);
	const char* GetGaitName(float fNormalizedSpeed);

	void AddPlayerHorseFollowTarget(int playerIndex, CPhysical* followTgt, float offsX, float offsZ, int followMode, int followPriority, bool disableWhenNotMounted, bool isRidingInPosse = false);
	void RemPlayerHorseFollowTarget(int playerIndex, CPhysical* followTgt);

	void Find3ActorsWithinRange(const CPed& actor, const Vector3 &position, float range, bhSpatialArrayActorTypes actorTypes, bhCreatureInfo* creaturesOut);
	void Find4ActorsWithinRange(const CPed& actor, const Vector3 &position, float range, bhSpatialArrayActorTypes actorTypes, bhCreatureInfo* creaturesOut);

	bool TestCoarseMoverBound(const Vector3& vCentroid, Vector3 verts[3], float fCoarseMoverBoundsShift = 0.4f );
}

#endif // ENABLE_HORSE

#endif	// #ifndef HELPER_FUNC_H
