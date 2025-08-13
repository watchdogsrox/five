#ifndef FOLLOW_NEAREST_PATH_HELPER_H
#define FOLLOW_NEAREST_PATH_HELPER_H

#include "Peds/Horse/horseDefines.h"

#if ENABLE_HORSE

// Rage headers
#include "atl/ptr.h"
#include "fwtl/Pool.h"
#include "vector/vector3.h"
#include "vectormath/vec3v.h"

// Framework headers
#include "fwutil/Flags.h"
#include "grcore/debugdraw.h"

// Game headers
#include "scene/RegdRefTypes.h"
#include "vehicleai/pathfind.h"
#include "Task/System/Tuning.h"

// Forward declarations

/* PURPOSE
 *	A helper class used to request the nearest path (road network)
 *	within a given distance and to be available to query for input
 *	that is used to update a ped to keep them on the road.
 */

class CFollowNearestPathHelper
{
public:
	FW_REGISTER_CLASS_POOL(CFollowNearestPathHelper);

	// Used to setup inputs for CFollowNearestPathHelper
	class HelperSettings
	{
	public:
		HelperSettings()
		: m_vPositionToTestFrom(rage::VEC3_ZERO)
		, m_vHeadingToTestWith(rage::YAXIS)
		, m_vVelocity(rage::YAXIS)
		, m_fCutoffDistForNodeSearch(0.0f)
		, m_fMinDistanceBetweenNodes(0.0f)
		, m_fForwardProjectionInSeconds(0.0f)
		, m_fDistSqBeforeMovingToNextNode(0.0f)
		, m_fDistBeforeMovingToNextNode(0.0f)
		, m_fMaxHeadingAdjustAllowed(0.0f)
		, m_fMaxDistSqFromRoad(0.0f)
		, m_bIgnoreSwitchedOffNodes(false)
		, m_bUseWaterNodes(false)
		, m_bUseOnlyHighwayNodes(false)
		, m_bSearchUpFromPosition(false)
		{
		}

		Vec3V	m_vPositionToTestFrom;				// used to determine the closest path
		Vec3V	m_vHeadingToTestWith;				// used to determine the closest path
		Vec3V	m_vVelocity;
		float	m_fCutoffDistForNodeSearch;			// used to determine the max distance to test for a path segment
		float	m_fMinDistanceBetweenNodes;
		float	m_fForwardProjectionInSeconds;
		float	m_fDistSqBeforeMovingToNextNode;
		float	m_fDistBeforeMovingToNextNode;
		float	m_fMaxHeadingAdjustAllowed;
		float	m_fMaxDistSqFromRoad;
		bool	m_bIgnoreSwitchedOffNodes;
		bool	m_bUseWaterNodes;
		bool	m_bUseOnlyHighwayNodes;
		bool	m_bSearchUpFromPosition;
	};
	
	CFollowNearestPathHelper();
	virtual ~CFollowNearestPathHelper();

	// PURPOSE: This is the update function
	void Process(const CEntity* pEntity);

	// PURPOSE: Sets up helper info for auto-following
	void StartSearch(const HelperSettings& context);

	// PURPOSE: A valid path to travel was found
	bool HasValidRoute() const { return m_FollowNearestPathStatus == FNPS_ValidRoute; }

	// PURPOSE: No valid paths were found
	bool HasNoRoute() const { return m_FollowNearestPathStatus == FNPS_NoRoute; }
	
	// PURPOSE: this is to get the current position to move towards for auto-following
	Vec3V_Out GetPositionToMoveTowards() const { return m_vGoalPosition; }

#if __BANK
	// Debug rendering
	void RenderDebug(const CEntity* pEntity) const;
#endif // __BANK

private:

	// PURPOSE: Find the next node within our forward tolerance, and update our position to move towards
	void RequestNextPathNode(Vec3V_In vEntityPos, Vec3V_In vEntityDir);

	// Is Ped close enough to request the next node
	bool ShouldMoveToNextNode(Vec3V_In vEntityPos, Vec3V_In vEntityHeading) const;

	// Is the address valid and its region loaded
	bool IsNodeValid(const CNodeAddress& nodeAddr);
	
	// Is the address valid, its region loaded, and we can head towards its position
	bool IsNodePosWithinForwardTolerance(const CNodeAddress& nodeAddr, Vec3V_In vEntityPos, Vec3V_In vEntityDir);

	// Is the node valid and the heading change needed to approach within the max heading adjustment
	bool IsNodePosWithinForwardTolerance(const CPathNode* pNode, Vec3V_In vEntityPos, Vec3V_In vEntityDir);

	// Is the heading change needed to approach the position within the max heading adjustment
	bool IsPosWithinForwardTolerance(Vec3V_In vQueryPos, Vec3V_In vEntityPos, Vec3V_In vEntityDir, float fHeadingTolerance);

	// Loop through the current node's links and determine if one of the links are valid
	void FindNextNodeInForwardTolerance(const CPathNode* pNode, CNodeAddress& nodeAddr, Vec3V_In vEntityPos, Vec3V_In vEntityDir);

	// Is the closest point on the road segment to the test position within our max sq distance from the entity
	bool IsCloseEnoughToPath(Vec3V_In vPathStartPosition, Vec3V_In vPathEndPosition, Vec3V_In vTestPosition, float fMaxDistSq);

	// Project the entity's position forward and check if the position is within our max sq distance and the 
	// heading to achieve is within the max heading adjustment
	bool IsEntityProjectionWithinForwardTolerance(Vec3V_In vPathStartPosition, Vec3V_In vPathEndPosition, Vec3V_In vEntityPosition, Vec3V_In vEntityHeading, Vec3V_In vEntityVelocity, float fForwardProjectionInSeconds);

	// Check if either point on the road segment is valid and return pNode1, pNode2 or NULL
	const CPathNode* DetermineBestForwardNode(const CPathNode* pNode1, const CPathNode* pNode2, Vec3V_In vTestPosition, Vec3V_In vTestHeading);

	// Status information
	enum eFollowNearestPathStatus
	{
	 	FNPS_Invalid = 0,
		FNPS_ValidRoute,
		FNPS_NoRoute,
	 	FNPS_MaxStatus,
	};

	eFollowNearestPathStatus m_FollowNearestPathStatus;
	
	Vec3V m_vGoalPosition;
	
	CNodeAddress m_GoalNode;
	
	HelperSettings m_Settings;

#if DEBUG_DRAW
	Vector3	m_vInitialSouthPosition;
	Vector3	m_vInitialNorthPosition;
	Vector3 m_vProjectedPosition;
	Vector3 m_vTestPosition;
	Vector3 m_vClosestPointOnPath;
	bool	m_bCanDrawProjectedPositionTest;
#endif // DEBUG_DRAW

};

#endif

#endif // FOLLOW_NEAREST_PATH_HELPER_H