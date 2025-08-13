#ifndef TASK_NAVBASE_H
#define TASK_NAVBASE_H

#include "fwmaths\angle.h"
#include "vectormath\mathops.h"
#include "Task\System\TaskMove.h"

//***************************************************************************************
//	Filename : TaskNavBase
//	Author	 : James Broad

namespace rage
{
	struct TRequestPathStruct;
}
class CSimpleFollowPath;
class CLadderClipSetRequestHelper;
class CEntityBoundAI;
class CMoveBlendRatioSpeeds;
class CMoveBlendRatioTurnRates;

#define RUNTIME_PATH_SPLINE_MAX_SEGMENTS	8
#define RUNTIME_PATH_SPLINE_STEPVAL			(1.0f / ((float)RUNTIME_PATH_SPLINE_MAX_SEGMENTS))

struct CNavMeshExpandedRouteVert
{
	Vec3V_ConstRef GetPosRef() const { return *reinterpret_cast<const Vec3V*>(&m_vPosAndSplineT); }
	Vec3V_Out GetPos() const { return m_vPosAndSplineT.GetXYZ(); }

	float GetSplineT() const { return m_vPosAndSplineT.GetWf(); }
	ScalarV_Out GetSplineTV() const { return m_vPosAndSplineT.GetW(); }
	void SetSplineT(float t) { m_vPosAndSplineT.SetWf(t); }

	void SetPosAndSplineT(Vec4V_In posAndSplineT) { m_vPosAndSplineT = posAndSplineT; }

	void SetPosAndSplineT(Vec3V_In posV, ScalarV_In tV)
	{	Vec4V posAndT(posV);
		posAndT.SetW(tV);
		m_vPosAndSplineT = posAndT;
	}

	Vec4V m_vPosAndSplineT;
	s32 m_iProgress;
	TNavMeshWaypointFlag m_iWptFlag;
};

//************************************************************
//	CNavMeshRoute
//	A list of waypoints on the navmesh for a ped to follow

class CNavMeshRoute
{
public:

	inline CNavMeshRoute()
		: m_iRouteSize(0)
		, m_iRouteFlags(0)
		, m_PossiblySetSpecialActionFlags(0)
	{
	}
	inline CNavMeshRoute(const CNavMeshRoute& src)
	{   
		From(src);
	}
	inline ~CNavMeshRoute(){}

	FW_REGISTER_CLASS_POOL(CNavMeshRoute);

	inline CNavMeshRoute & operator=(const CNavMeshRoute& src)
	{
		From(src);
		return *this;
	}

	enum 
	{ 
		MAX_NUM_ROUTE_ELEMENTS = 16
	};

	enum
	{
		FLAG_LAST_ROUTE_POINT_IS_TARGET		=	0x1
	};

	inline u32 GetRouteFlags() const { return (u16)m_iRouteFlags; }
	inline void SetRouteFlags(const u32 iFlags) { m_iRouteFlags = (u16)iFlags; }

	inline void From(const CNavMeshRoute& src)
	{
		m_iRouteSize=src.m_iRouteSize;
		int i;
		for(i=0;i<m_iRouteSize;i++)
		{
			m_routePoints[i]=src.m_routePoints[i];
			m_routeWayPoints[i]=src.m_routeWayPoints[i];
			m_routeSplineEnums[i]=src.m_routeSplineEnums[i];
			m_routeSplineCtrlPt2TVals[i]=src.m_routeSplineCtrlPt2TVals[i];
			m_routeFreeSpaceApproachingWaypoint[i]=src.m_routeFreeSpaceApproachingWaypoint[i];
		}
		Assert(m_iRouteSize<=MAX_NUM_ROUTE_ELEMENTS);
		m_PossiblySetSpecialActionFlags = src.m_PossiblySetSpecialActionFlags;
	}
	inline void Clear() 
	{
		Assert(m_iRouteSize<=MAX_NUM_ROUTE_ELEMENTS);
		m_iRouteSize=0;
		m_PossiblySetSpecialActionFlags = 0;
	}
	inline bool Add(const Vector3& vPos, const TNavMeshWaypointFlag iWaypoint, const int iSplineEnum=-1, const float fTVal=0.0f, const int iFreeSpace=0)
	{
		if(m_iRouteSize<MAX_NUM_ROUTE_ELEMENTS)
		{
			m_routePoints[m_iRouteSize] = vPos;
			m_routeWayPoints[m_iRouteSize] = iWaypoint;
			m_routeSplineEnums[m_iRouteSize] = (s8)iSplineEnum;
			m_routeSplineCtrlPt2TVals[m_iRouteSize] = (u8) (Clamp(fTVal, 0.0f, 1.0f) * 255.0f);
			m_routeFreeSpaceApproachingWaypoint[m_iRouteSize] = (s8)iFreeSpace;
			m_iRouteSize++;
			m_PossiblySetSpecialActionFlags |= iWaypoint.m_iSpecialActionFlags;
			return true;
		}   
		return false;
	}
	inline bool Insert(int index, const Vector3 & vPos, const TNavMeshWaypointFlag iWaypoint, const int iSplineEnum=-1, const float fTVal=0.0f, const int iFreeSpace=0)
	{
		Assert(index >= 0 && index <= m_iRouteSize);

		if(m_iRouteSize+1>=MAX_NUM_ROUTE_ELEMENTS)
		{
			Assert(m_iRouteSize+1<MAX_NUM_ROUTE_ELEMENTS);
			return false;
		}

		for(int i=m_iRouteSize; i>index; i--)
		{
			m_routePoints[i] = m_routePoints[i-1];
			m_routeWayPoints[i] = m_routeWayPoints[i-1];
			m_routeSplineEnums[i] = m_routeSplineEnums[i-1];
			m_routeSplineCtrlPt2TVals[i] = m_routeSplineCtrlPt2TVals[i-1];
			m_routeFreeSpaceApproachingWaypoint[i] = m_routeFreeSpaceApproachingWaypoint[i-1];
		}
		m_routePoints[index] = vPos;
		m_routeWayPoints[index] = iWaypoint;
		m_routeSplineEnums[index] = (s8)iSplineEnum;
		m_routeSplineCtrlPt2TVals[index] = (u8) (Clamp(fTVal, 0.0f, 1.0f) * 255.0f);
		m_routeFreeSpaceApproachingWaypoint[index] = (s8)iFreeSpace;
		m_iRouteSize++;
		m_PossiblySetSpecialActionFlags |= iWaypoint.m_iSpecialActionFlags;

		return true;
	}

	inline const Vector3 & GetPosition(const int i) const
	{
		Assert(m_iRouteSize<=MAX_NUM_ROUTE_ELEMENTS);
		Assert(i<MAX_NUM_ROUTE_ELEMENTS);
		Assert(i<m_iRouteSize);
		Assert(i>=0);
		return m_routePoints[i];
	}
	inline const TNavMeshWaypointFlag GetWaypoint(const int i) const
	{
		Assert(m_iRouteSize<=MAX_NUM_ROUTE_ELEMENTS);
		Assert(i<MAX_NUM_ROUTE_ELEMENTS);
		Assert(i>=0);
		return m_routeWayPoints[i];
	}
	inline int GetSplineEnum(const int i) const
	{
		Assert(m_iRouteSize<=MAX_NUM_ROUTE_ELEMENTS);
		Assert(i<MAX_NUM_ROUTE_ELEMENTS);
		Assert(i<m_iRouteSize);
		Assert(i>=0);
		return m_routeSplineEnums[i];
	}
	inline float GetSplineCtrlPt2TVal(const int i) const
	{
		Assert(m_iRouteSize<=MAX_NUM_ROUTE_ELEMENTS);
		Assert(i<MAX_NUM_ROUTE_ELEMENTS);
		Assert(i<m_iRouteSize);
		Assert(i>=0);
		return (float) (((float)m_routeSplineCtrlPt2TVals[i]) / 255.0f);
	}
	inline ScalarV_Out GetSplineCtrlPt2TValV(const int i) const
	{
		Assert(m_iRouteSize<=MAX_NUM_ROUTE_ELEMENTS);
		Assert(i<MAX_NUM_ROUTE_ELEMENTS);
		Assert(i<m_iRouteSize);
		Assert(i>=0);
		ScalarV tmpV;
		tmpV.Seti(m_routeSplineCtrlPt2TVals[i]);

		// Note: if we were using 128 or 256 as the scaling factor instead
		// of 255, this could be made faster by setting the exponent in IntToFloatRaw()
		// and getting rid of the extra multiplication.
		const ScalarV scalingV(Vec::V4VConstantSplat<FLOAT_TO_INT(1.0f/255.0f)>());
		return Scale(IntToFloatRaw<0>(tmpV), scalingV);
	}
	inline int GetFreeSpaceApproachingWaypoint(const int i) const
	{
		Assert(m_iRouteSize<=MAX_NUM_ROUTE_ELEMENTS);
		Assert(i<MAX_NUM_ROUTE_ELEMENTS);
		Assert(i<m_iRouteSize);
		Assert(i>=0);
		return m_routeFreeSpaceApproachingWaypoint[i];
	}

	bool GetPositionAheadOnSpline(const Vector3 & vPedPos, const int iInitialProgress, float fInitialSplineTVal, const float fInitialDistanceAhead, Vector3 & vOutPos) const;

	inline void Set(const int i, const Vector3 & vPos, const TNavMeshWaypointFlag iWaypoint, const int iSplineEnum=-1, const float fTVal=0.0f, const int iFreeSpace=0)
	{
		Assert(m_iRouteSize<=MAX_NUM_ROUTE_ELEMENTS);
		Assert(i>=0 && i<m_iRouteSize);
		m_routePoints[i] = vPos;
		m_routeWayPoints[i] = iWaypoint;
		m_routeSplineEnums[i] = (s8)iSplineEnum;
		m_routeSplineCtrlPt2TVals[i] = (u8) (Clamp(fTVal, 0.0f, 1.0f) * 255.0f);
		m_routeFreeSpaceApproachingWaypoint[i] = (s8)iFreeSpace;
	}
	inline int GetSize() const 
	{
		Assert(m_iRouteSize<=MAX_NUM_ROUTE_ELEMENTS);
		return m_iRouteSize;
	}
	inline void SetSize(const int iSize)
	{
		Assert(iSize<=MAX_NUM_ROUTE_ELEMENTS);
		Assert(iSize>=0);
		if(iSize>MAX_NUM_ROUTE_ELEMENTS)
		{
			return;
		}
		CompileTimeAssert(MAX_NUM_ROUTE_ELEMENTS <= 0xff);
		m_iRouteSize=(u8)iSize;
	}
	inline void Reverse()
	{
		int i0=0;
		int i1=m_iRouteSize-1;
		while(i0<i1)
		{
			const Vector3 a=m_routePoints[i0];
			const TNavMeshWaypointFlag i=m_routeWayPoints[i0];
			const s8 s=m_routeSplineEnums[i0];
			const u8 t=m_routeSplineCtrlPt2TVals[i0];
			const s8 f=m_routeFreeSpaceApproachingWaypoint[i0];

			m_routePoints[i0]=m_routePoints[i1];
			m_routeWayPoints[i0]=m_routeWayPoints[i1];
			m_routeSplineEnums[i0]=m_routeSplineEnums[i1];
			m_routeSplineCtrlPt2TVals[i0]=m_routeSplineCtrlPt2TVals[i1];
			m_routeFreeSpaceApproachingWaypoint[i0]=m_routeFreeSpaceApproachingWaypoint[i1];

			m_routePoints[i1]=a;
			m_routeWayPoints[i1]=i;
			m_routeSplineEnums[i1]=s;
			m_routeSplineCtrlPt2TVals[i1]=t;
			m_routeFreeSpaceApproachingWaypoint[i1]=f;

			i0++;
			i1--;
		}
	}
	inline void Remove(const int index)
	{
		Assert(index>=0);
		Assert(index<MAX_NUM_ROUTE_ELEMENTS);   	

		int i;
		for(i=index;i<(m_iRouteSize-1);i++)
		{
			m_routePoints[i]=m_routePoints[i+1];
			m_routeWayPoints[i]=m_routeWayPoints[i+1];
			m_routeSplineEnums[i]=m_routeSplineEnums[i+1];
			m_routeSplineCtrlPt2TVals[i]=m_routeSplineCtrlPt2TVals[i+1];
			m_routeFreeSpaceApproachingWaypoint[i]=m_routeFreeSpaceApproachingWaypoint[i+1];
		}
		Assert(m_iRouteSize > 0);
		m_iRouteSize--;

		// Note: we could recompute m_PossiblySetSpecialActionFlags here, to clear out any flags that
		// may no longer be set, but it's probably not worth it.
	}
	inline float GetLengthSqr(void) const
	{
		float fLengthSqr = 0.0f;
		int i;
		for(i=1; i<m_iRouteSize; i++)
		{
			fLengthSqr += (m_routePoints[i] - m_routePoints[i-1]).Mag2();
		}
		return fLengthSqr;
	}
	inline float GetLength(void) const
	{
		return Sqrtf(GetLengthSqr());
	}
	inline bool IsLongerThan(float fLength) const
	{
		float fTotalLength = 0.0f;
		int i;
		for(i=1; i<m_iRouteSize; i++)
		{
			fTotalLength += (m_routePoints[i] - m_routePoints[i-1]).Mag();
			if(fTotalLength > fLength)
			{
				return true;
			}
		}
		return false;
	}
	inline void GetMinMax(Vector3 & vMin, Vector3 & vMax) const
	{
		Assert(m_iRouteSize > 0);
		vMin = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
		vMax = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		int i;
		for(i=0; i<m_iRouteSize; i++)
		{
			if(m_routePoints[i].x < vMin.x) vMin.x = m_routePoints[i].x;
			if(m_routePoints[i].y < vMin.y) vMin.y = m_routePoints[i].y;
			if(m_routePoints[i].z < vMin.z) vMin.z = m_routePoints[i].z;
			if(m_routePoints[i].x > vMax.x) vMax.x = m_routePoints[i].x;
			if(m_routePoints[i].y > vMax.y) vMax.y = m_routePoints[i].y;
			if(m_routePoints[i].z > vMax.z) vMax.z = m_routePoints[i].z;
		}
	}

	int ExpandRoute(CNavMeshExpandedRouteVert * RESTRICT pRoutePoints, float * RESTRICT pRouteLength
		, int startSegmentIndex, int stopSegmentIndex, const float& distanceToExpand=FLT_MAX) const;

	unsigned int GetPossiblySetSpecialActionFlags() const { return m_PossiblySetSpecialActionFlags; }

#if __ASSERT
	void CheckValid() const
	{
		for(s32 i=0; i<m_iRouteSize; i++)
		{
			Assert( rage::FPIsFinite(m_routePoints[i].x) && rage::FPIsFinite(m_routePoints[i].y) && rage::FPIsFinite(m_routePoints[i].z) );
		}
	}
#endif

private:

	// The waypoint positions
	Vector3 m_routePoints[MAX_NUM_ROUTE_ELEMENTS];
	// Flags & heading info associated with each waypoint
	TNavMeshWaypointFlag m_routeWayPoints[MAX_NUM_ROUTE_ELEMENTS];
	// The type of spline associated with a waypoint, or -1 if none
	s8 m_routeSplineEnums[MAX_NUM_ROUTE_ELEMENTS];
	// Control-points for the spline, from a set number of values
	u8 m_routeSplineCtrlPt2TVals[MAX_NUM_ROUTE_ELEMENTS];
	// The minimum amount of free-space in all the polys visited along the line-segment leading up to the waypoint
	u8 m_routeFreeSpaceApproachingWaypoint[MAX_NUM_ROUTE_ELEMENTS];
	// The number of points in the route
	u8 m_iRouteSize;
	// Special action flags (from eWaypointActionFlags) that could possibly be set for any of the waypoints along the route
	u16	m_PossiblySetSpecialActionFlags;
	// Flags associated with this route
	u16 m_iRouteFlags;
};

class CNavBaseConfigFlags
{
public:
	CNavBaseConfigFlags() { m_iFlags = 0; }
	~CNavBaseConfigFlags() { }

	inline bool IsSet(const u64 i) const { return ((m_iFlags & i)!=0); }
	inline void Set(const u64 iFlags) { m_iFlags |= iFlags; }
	inline void Set(const u64 iFlags, const bool bState)
	{
		if(bState) m_iFlags |= iFlags;
		else m_iFlags &= ~iFlags;
	}
	inline void Reset(const u64 iFlags) { m_iFlags &= ~iFlags; }
	inline void ResetAll() { m_iFlags = 0; }

	u64 GetAsPathServerFlags() const;

	// Config flags
	static const u64 NB_KeepMovingWhilstWaitingForPath				= 0x01;
	static const u64 NB_DontStandStillAtEnd							= 0x02;
	static const u64 NB_DontAdjustTargetPosition					= 0x04;
	static const u64 NB_ScriptedRoute								= 0x08;
	static const u64 NB_UseLargerSearchExtents						= 0x10;
	static const u64 NB_AvoidObjects								= 0x20;
	static const u64 NB_UseAdaptiveSetTargetEpsilon					= 0x40;
	static const u64 NB_SlideToCoordAtEnd							= 0x80;
	static const u64 NB_SlideToCoordUsingCurrentHeading				= 0x100;
	static const u64 NB_UseDirectionalCover							= 0x200;
	static const u64 NB_UseFreeSpaceReferenceValue					= 0x400;
	static const u64 NB_FavourGreaterThanFreeSpaceReferenceValue	= 0x800;
	static const u64 NB_DontUseLadders								= 0x1000;
	static const u64 NB_DontUseClimbs								= 0x2000;
	static const u64 NB_DontUseDrops								= 0x4000;
	static const u64 NB_SmoothPathCorners							= 0x8000;
	static const u64 NB_StartAtFirstRoutePoint						= 0x10000;
	static const u64 NB_StopExactly									= 0x20000;
	static const u64 NB_TryToGetOntoMainNavMeshIfStranded			= 0x40000;
	static const u64 NB_AllowToNavigateUpSteepPolygons				= 0x80000;
	static const u64 NB_AllowToPushVehicleDoorsClosed				= 0x100000;
	static const u64 NB_AvoidEntityTypeObject						= 0x200000;
	static const u64 NB_AvoidEntityTypeVehicle						= 0x400000;
	static const u64 NB_ConsiderRunStartForPathLookAhead			= 0x800000;
	static const u64 NB_HighPrioRoute								= 0x1000000;
	static const u64 NB_ScanForObstructions							= 0x2000000;
	static const u64 NB_NeverLeaveWater								= 0x4000000;
	static const u64 NB_SetAllowNavmeshAvoidance					= 0x8000000;
	static const u64 NB_FollowNavMeshAtCurrentHeight				= 0x10000000;	// things like fish which don't want to actually float up to the navmesh.
	static const u64 NB_DontStopForClimbs							= 0x20000000;
	static const u64 NB_AvoidPeds									= 0x40000000;
	static const u64 NB_LenientAchieveHeadingForExactStop			= 0x80000000;
	static const u64 NB_SlowDownInsteadOfUsingRunStops				= 0x100000000;
	static const u64 NB_ExpandStartEndTessellationRadius			= 0x200000000;
	static const u64 NB_DisableCalculatePathOnTheFly				= 0x400000000;
	static const u64 NB_PullFromEdgeExtra							= 0x800000000;
	static const u64 NB_NeverSlowDownForPathLength					= 0x1000000000;
	static const u64 NB_SofterFleeHeuristics						= 0x2000000000;
	static const u64 NB_NeverLeaveDeepWater							= 0x4000000000;

private:

	u64 m_iFlags;
};

class CNavBaseInternalFlags
{
public:
	CNavBaseInternalFlags() { m_iFlags = 0; }
	~CNavBaseInternalFlags() { }

	inline bool IsSet(const u32 i) const { return ((m_iFlags & i)!=0); }
	inline void Set(const u32 iFlags) { m_iFlags |= iFlags; }
	inline void Set(const u32 iFlags, const bool bState)
	{
		if(bState) m_iFlags |= iFlags;
		else m_iFlags &= ~iFlags;
	}
	inline void Reset(const u32 iFlags) { m_iFlags &= ~iFlags; }
	inline void ResetAll() { m_iFlags = 0; }

	void ResetTemporaryFlags();

	// Internal flags
	static const u32 NB_HasBeenInitialised							= 0x01;
	static const u32 NB_TargetPositionWasAdjusted					= 0x02;
	static const u32 NB_HadPreviousGotoTarget						= 0x04;
	static const u32 NB_Unused1										= 0x08;
	static const u32 NB_PathIncludesWater							= 0x10;
	static const u32 NB_NewTarget									= 0x20;
	static const u32 NB_HadNewTarget								= 0x40;
	static const u32 NB_GotoTaskSequencedBefore						= 0x80;
	static const u32 NB_GotoTaskSequencedAfter						= 0x100;
	static const u32 NB_HasSlidTargetAlongSpline					= 0x200;	// NB: can probably get rid of this now
	static const u32 NB_JustDoneClimbResponse						= 0x400;
	static const u32 NB_LastSetTargetWasNotUsed						= 0x800;
	static const u32 NB_RestartNextFrame							= 0x1000;
	static const u32 NB_CheckedStoppingDistance						= 0x2000;
	static const u32 NB_PedUsingPhysicsLodLastFrame					= 0x4000;	// needed to detect when ped has changed state
	static const u32 NB_AbortedForDoorCollision						= 0x8000;	// door collision event
	static const u32 NB_AbortedForCollisionEvent					= 0x10000;	// generic collision event (perhaps can replace door collision flag)
	static const u32 NB_MayUsePreviousTargetInMovingWaitingForPath	= 0x20000;	// required because aiTaskFlags::IsAborted only persists on the first UpdateFSM() of the frame
	static const u32 NB_HasPathed									= 0x40000;	// will be set once the the task has entered state NavBaseState_FollowingPath at least once
	static const u32 NB_TargetIsDestination							= 0x80000;	// true for regular navmesh paths, always false for wander and flee tasks
	static const u32 NB_CalculatingPathOnTheFly						= 0x100000;	// is set if we request a path whilst in NavBaseState_FollowingPath
	static const u32 NB_OnTheFlyPathStartedAtRouteEnd				= 0x200000;
	static const u32 NB_PathStartedInsidePlaneVehicle				= 0x400000;	// path start was inside the bounds of a VEHICLE_TYPE_PLANE vehicle; this special case allows us to trigger a response task to walk around the plane
	static const u32 NB_UsingMaxSlopeNavigableOverride				= 0x800000; // Has the max slope navigable been changed from the default provided by the ped's nav capabilities?
	static const u32 NB_ExceededVerticalDistanceFromPath			= 0x1000000;// The task was restarted due to the ped exceending Z threshold; will enter moving whilst waiting for path state
	static const u32 NB_ForceRestartFollowPathState					= 0x2000000;
	static const u32 NB_StartedFromInVehicle						= 0x4000000;

private:

	u32 m_iFlags;
};


//**************************************************************************************
//	CTaskNavBase
//	A base class from which navigation tasks may inherit, to avoid duplicating complex
//	code across CTaskMoveWander, CTaskNavMesh and other classes.

class CTaskNavBase : public CTaskMove
{
public:

	static const float ms_fHeightAboveReturnedPointsForRouteVerts;
	static bank_float ms_fCorneringLookAheadTime;
	static bank_float ms_fGotoTargetLookAheadDist;
	static const float ms_fTargetRadiusForWaypoints;
	static const float ms_fTargetEqualEpsilon;
	static const float ms_fWaypointSlowdownProximitySqr;
	static const s32 ms_iAttemptToSwimStraightToTargetFreqMs;
	static const float ms_DefaultPathDistanceToExpand;
	static bank_bool ms_bScanForObstructions;
	static bank_s32 ms_iScanForObstructionsFreq_Walk;
	static bank_s32 ms_iScanForObstructionsFreq_Run;
	static bank_s32 ms_iScanForObstructionsFreq_Sprint;
	static bank_float ms_fScanForObstructionsDistance;
	static bank_bool ms_bUseExandRouteOptimisations;
	static bank_float ms_fDistanceFromRouteEndToRequestNextSection;
	static bank_float ms_fDefaultDistanceAheadForSetNewTarget;
	static bank_float ms_fDistanceAheadForSetNewTargetObstruction;
	static bank_float ms_fDistanceFromPathToTurnOnNavMeshAvoidance;
	static bank_float ms_fDistanceFromPathToTurnOnObjectAvoidance;
	static bank_float ms_fDistanceFromPathToTurnOnVehicleAvoidance;

	static const s32 ms_iMaxExpandedVerts = CNavMeshRoute::MAX_NUM_ROUTE_ELEMENTS * RUNTIME_PATH_SPLINE_MAX_SEGMENTS;

#if __BANK
	static float ms_fOverrideRadius;
	static bank_bool ms_bDebugRequests;
#endif

	enum eRouteMethod
	{
		eRouteMethod_Normal,						// Use a normally calculated route with full object avoidance
		eRouteMethod_ReducedObjectBoundingBoxes,	// Reduce the bounding boxes of objects to help ped get past them
		eRouteMethod_OnlyAvoidNonMovableObjects,	// Try the route only avoiding objects which cannot be moved (eg. fences, fire hydrants, locked doors, etc)

		eRouteMethod_NumMethods,
		eRouteMethod_LastMethodWhichAvoidsObjects = eRouteMethod_OnlyAvoidNonMovableObjects	// Used by tasks, etc
	};

public:
	CTaskNavBase(const float fMoveBlendRatio, const Vector3 & vTarget);
	virtual ~CTaskNavBase();

	virtual bool InheritsFromTaskNavBase() const { return true; }

	virtual s32 GetDefaultStateAfterAbort() const;
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return ProcessPostFSM();
	virtual void CleanUp();
	void CopySettings(CTaskNavBase * pClone) const;

	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	enum
	{
		NavBaseState_Initial,
		NavBaseState_WaitingInVehiclePriorToRequest,
		NavBaseState_RequestPath,
		NavBaseState_MovingWhilstWaitingForPath,
		NavBaseState_StationaryWhilstWaitingForPath,
		NavBaseState_WaitingToLeaveVehicle,
		NavBaseState_WaitingAfterFailureToObtainPath,
		NavBaseState_FollowingPath,
		NavBaseState_SlideToCoord,
		NavBaseState_StoppingAtTarget,
		NavBaseState_WaitingForWaypointActionEvent,
		NavBaseState_WaitingAfterFailedWaypointAction,
		NavBaseState_GetOntoMainNavMesh,
		NavBaseState_AchieveHeading,
		NavBaseState_UsingLadder,

		NavBaseState_NumStates	// derived classes should start their FSM state enumerations from this value
	};

#if !__FINAL
	static const char * GetBaseStateName(s32 iState);
	virtual void Debug() const { }
#if DEBUG_DRAW
	virtual const char * GetInfoForWaypoint(const TNavMeshWaypointFlag & wpt, Color32 & col) const;
	virtual void DrawPedText(const CPed * pPed, const Vector3 & vWorldCoords, const Color32 iTxtCol, int & iTextStartLineOffset) const;
	void DebugVisualiserDisplay(const CPed * pPed, const Vector3 & vWorldCoords, s32 iOffsetY) const;
#endif
#endif

	virtual void SetMoveBlendRatio(const float fMoveBlendRatio, const bool bFlagAsChanged = true);
	virtual void SetTarget(const CPed * pPed, Vec3V_In vTarget, const bool bForce=false);
	virtual void SetTargetRadius(const float fTargetRadius);
	virtual void SetTargetStopHeading (const float fTargetHeading){ m_fTargetStopHeading = fTargetHeading; }

	float GetTargetStopHeading() const { return m_fTargetStopHeading; }

	// Function prototype for building a list of influence spheres
	typedef bool SpheresOfInfluenceBuilder( TInfluenceSphere* apSpheres, int &iNumSpheres, const CPed* pPed );

	// Initialise the structure which is passed into the CPathServer::RequestPath() function
	virtual void InitPathRequestPathStruct(CPed * pPed, TRequestPathStruct & reqPathStruct, const float fDistanceAheadOfPed=0.0f) = 0;

	// Allow path to be requested on the fly when this close to route end - regardless of whether the route takes us all the way to the target
	virtual float GetRouteDistanceFromEndForNextRequest() const { return 0.0f; }

	// When handling a new target position (in HandleNewTarget()), the task may use a start-position some distance ahead of the ped
	virtual float GetDistanceAheadForSetNewTarget() const { return ms_fDefaultDistanceAheadForSetNewTarget; }

	inline const CNavMeshRoute * GetRoute() const { return m_pRoute; }
	inline int GetRouteSize() const { return m_pRoute ? m_pRoute->GetSize() : 0; }
	inline int GetProgress() const { return m_iProgress; }
	TNavMeshWaypointFlag GetWaypointFlag(const int i) const;

	inline TPathHandle GetPathHandle() const { return m_iPathHandle; }
	inline TPathHandle GetLastPathHandle() const { return m_iLastPathHandle; }
	
	void SetMaxSlopeNavigable(const float f) { m_fMaxSlopeNavigable = f; m_NavBaseInternalFlags.Set(CNavBaseInternalFlags::NB_UsingMaxSlopeNavigableOverride); }
	float GetMaxSlopeNavigable() const { return m_fMaxSlopeNavigable; }
	float GetTotalRouteLength() const;
	float GetTotalRouteLengthSq() const;
	bool IsTotalRouteLongerThan(float fLength) const;
	float GetDistanceLeftOnCurrentRouteSection(const CPed * pPed) const;
	// returns the start and end point of the current waypoint line being traversed
	bool GetThisWaypointLine(const CPed * pPed, Vector3 & vOutStartPos, Vector3 & vOutEndPos, s32 iProgressAhead = 0 );
	void GetAdjustedTargetPos(Vector3 & vPos) const { vPos.x = m_vAdjustedTarget[0]; vPos.y = m_vAdjustedTarget[1]; vPos.z = m_vAdjustedTarget[2]; }
	bool GetPreviousWaypoint(Vector3 & vPos) const;
	bool GetCurrentWaypoint(Vector3 & vPos) const;

	u32 TestSpheresIntersectingRoute( s32 iNumSpheres, const spdSphere * ppSpheres, const bool bOnlyAheadOfCurrentPosition ) const;
	bool GetPositionAlongRouteWithDistanceFromTarget(Vec3V_InOut vReturnPosition, Vec3V_In vTargetPosition, float &fDesiredDistanceFromTarget);

	bool GetPositionAheadOnRoute(const CPed * pPed, const float fDistanceAhead, Vector3 & vOutPos, const bool bHaltAtClimbsOrDrops=false);

	//**************************************************************************
	// Legacy accessors which should now go via the CNavBaseFlags interface

	void SetDontAdjustTargetPosition(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_DontAdjustTargetPosition, b); }
	bool GetDontAdjustTargetPosition() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_DontAdjustTargetPosition); }
	bool WasTargetPosAdjusted() { return m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_TargetPositionWasAdjusted); }
	void SetForceRestartFollowPathState(bool b) { m_NavBaseConfigFlags.Set(CNavBaseInternalFlags::NB_ForceRestartFollowPathState, b); }
	void SetTargetIsDestination(bool b) { m_NavBaseConfigFlags.Set(CNavBaseInternalFlags::NB_TargetIsDestination, b); }
	//bool GetLastRoutePointIsTarget() const { return m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_LastRoutePointIsTarget); }
	inline bool GetLastRoutePointIsTarget() const { return ((m_pRoute->GetRouteFlags() & CNavMeshRoute::FLAG_LAST_ROUTE_POINT_IS_TARGET)!=0); }
	void SetDontUseLadders(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_DontUseLadders, b); }
	bool GetDontUseLadders() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_DontUseLadders); }
	void SetDontUseClimbs(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_DontUseClimbs, b); }
	bool GetDontUseClimbs() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_DontUseClimbs); }
	void SetDontUseDrops(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_DontUseDrops, b); }
	bool GetDontUseDrops() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_DontUseDrops); }
	void SetUseLargerSearchExtents(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_UseLargerSearchExtents, b); }
	bool GetUseLargerSearchExtents() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_UseLargerSearchExtents); }
	void SetIsScriptedRoute(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_ScriptedRoute, b); }
	bool GetIsScriptedRoute() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_ScriptedRoute); }
	void SetSlideToCoordAtEnd(bool b, float fHeading=-1000.0f, float fSlideSpeed=2.0f, float fHeadingSpeed=TWO_PI)
	{
		m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_SlideToCoordAtEnd, b);

		if(b)
		{
			m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_SlideToCoordUsingCurrentHeading, (fHeading == -1000.0f));
			m_fSlideToCoordHeading = fHeading;
			m_fSlideToCoordSpeed = fSlideSpeed;
			m_fSlideToCoordHeadingChangeSpeed = fHeadingSpeed;
		}
	}
	bool GetSlideToCoordAtEnd() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_SlideToCoordAtEnd); }
	void SetUseDirectionalCover(bool b, const Vector3 * pvThreatPos)
	{
		if(b && pvThreatPos)
		{
			m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_UseDirectionalCover);
			m_vDirectionalCoverThreatPos[0] = pvThreatPos->x;
			m_vDirectionalCoverThreatPos[1] = pvThreatPos->y;
		}
		else
		{
			m_NavBaseConfigFlags.Reset(CNavBaseConfigFlags::NB_UseDirectionalCover);
		}
	}
	void SetUseFreeSpaceReferenceValue(bool b, int iReferenceValue=-1000, bool bFavourGreaterThan=true)
	{
		if(b)
		{
			Assertf(iReferenceValue!=-1000, "SetUseFreeSpaceReferenceValue - you must supply a value!");
			if(iReferenceValue!=-1000)
			{
				m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_UseFreeSpaceReferenceValue);
				m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_FavourGreaterThanFreeSpaceReferenceValue, bFavourGreaterThan);
				m_iFreeSpaceReferenceValue = (s16)iReferenceValue;
			}
			else
			{
				m_NavBaseConfigFlags.Reset(CNavBaseConfigFlags::NB_UseFreeSpaceReferenceValue);
			}
		}
		else
		{
			m_NavBaseConfigFlags.Reset(CNavBaseConfigFlags::NB_UseFreeSpaceReferenceValue);
		}
	}
	void SetClampMaxSearchDistance(float f)
	{
		Assertf(f >= 0.0f && f <= 255.0f, "Clamp search distance must be between 0 and 255 inclusive");
		m_iClampMaxSearchDistance = (u8) Clamp(f, 0.0f, 255.0f);
	}
	float GetClampMaxSearchDistance() const { return (float)m_iClampMaxSearchDistance; }


	void SetSmoothPathCorners(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_SmoothPathCorners, b); }
	bool GetSmoothPathCorners() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_SmoothPathCorners); }
	void SetStartAtFirstRoutePoint(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_StartAtFirstRoutePoint, b); }
	bool GetStartAtFirstRoutePoint() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_StartAtFirstRoutePoint); }
	void SetAllowToNavigateUpSteepPolygons(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_AllowToNavigateUpSteepPolygons, b); }
	bool GetAllowToNavigateUpSteepPolygons() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_AllowToNavigateUpSteepPolygons); }
	void SetAllowToPushVehicleDoorsClosed(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_AllowToPushVehicleDoorsClosed, b); }
	bool GetAllowToPushVehicleDoorsClosed() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_AllowToPushVehicleDoorsClosed); }
	void SetConsiderRunStartForPathLookAhead(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_ConsiderRunStartForPathLookAhead, b); }
	bool GetConsiderRunStartForPathLookAhead() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_ConsiderRunStartForPathLookAhead); }
	void SetHighPrioRoute(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_HighPrioRoute, b); }
	bool GetHighPrioRoute() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_HighPrioRoute); }
	void SetScanForObstructions(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_ScanForObstructions, b); }
	bool GetScanForObstructions() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_ScanForObstructions); }
	void SetNeverLeaveWater(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_NeverLeaveWater, b); }
	bool GetNeverLeaveWater() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_NeverLeaveWater); }
	void SetNeverLeaveDeepWater(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_NeverLeaveDeepWater, b); }
	bool GetNeverLeaveDeepWater() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_NeverLeaveDeepWater); }
	void SetFollowNavMeshAtCurrentHeight(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_FollowNavMeshAtCurrentHeight, b); }
	bool GetFollowNavmeshAtCurrentHeight() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_FollowNavMeshAtCurrentHeight); }
	void SetDontStopForClimbs(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_DontStopForClimbs, b); }
	bool GetDontStopForClimbs() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_DontStopForClimbs); }
	void SetLenientAchieveHeadingForExactStop(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_LenientAchieveHeadingForExactStop, b); }
	bool GetLenientAchieveHeadingForExactStop() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_LenientAchieveHeadingForExactStop); }
	void SetExpandStartEndTessellationRadius(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_ExpandStartEndTessellationRadius, b); }
	bool GetExpandStartEndTessellationRadius() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_ExpandStartEndTessellationRadius); }
	void SetDisableCalculatePathOnTheFly(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_DisableCalculatePathOnTheFly, b); }
	void SetNeverSlowDownForPathLength(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_NeverSlowDownForPathLength, b); }
	bool GetNeverSlowDownForPathLength() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_NeverSlowDownForPathLength); }
	bool GetPathStartedInsidePlaneVehicle() const { return m_NavBaseInternalFlags.IsSet(CNavBaseInternalFlags::NB_PathStartedInsidePlaneVehicle); }
	void SetPullFromEdgeExtra(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_PullFromEdgeExtra, b); }
	bool GetPullFromEdgeExtra() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_PullFromEdgeExtra); }
	void SetSofterFleeHeuristics(bool b) { m_NavBaseConfigFlags.Set(CNavBaseConfigFlags::NB_SofterFleeHeuristics, b); }
	bool GetSofterFleeHeuristics() const { return m_NavBaseConfigFlags.IsSet(CNavBaseConfigFlags::NB_SofterFleeHeuristics); }

	void SetMaxDistanceMayAdjustPathStartPosition(float f)
	{
		Assertf(f >= 0.0f && f <= 10.0f, "Maximum adjust distance must be between 0.0f and 10.0f inclusive");
		const float s = Clamp(f, 0.0f, 10.0f) / 10.0f;
		m_iMaxDistanceToMovePathStart = static_cast<u8>(s * 255.0f);
	}
	float GetMaxDistanceMayAdjustPathStartPosition() const
	{
		return (((float)m_iMaxDistanceToMovePathStart) / 255.0f) * 10.0f;
	}

	void SetMaxDistanceMayAdjustPathEndPosition(float f)
	{
		Assertf(f >= 0.0f && f <= 10.0f, "Maximum adjust distance must be between 0.0f and 10.0f inclusive");
		const float s = Clamp(f, 0.0f, 10.0f) / 10.0f;
		m_iMaxDistanceToMovePathEnd = static_cast<u8>(s * 255.0f);
	}
	float GetMaxDistanceMayAdjustPathEndPosition() const
	{
		return (((float)m_iMaxDistanceToMovePathEnd) / 255.0f) * 10.0f;
	}

	//--------------------------------------------------------------------------------------------------------------------------------

	void SetRecalcPathFrequency(const s32 iFreq, const s32 iInitialTime=-1)
	{
		m_iRecalcPathFreqMs = (s16)iFreq;
		m_iRecalcPathTimerMs = (iInitialTime==-1) ? m_iRecalcPathFreqMs : (s16)iInitialTime;
	}

	void SetScanForObstructionsFrequency(u32 iFrequencyMS);
	void SetScanForObstructionsNextFrame();
	u32 GetTimeSinceLastScanForObstructions();

	bool IsMovingIntoObstacle(const CPed * pPed, bool bVehicles, bool bObjects) const;

	bool GetPathIntersectsRegion(const Vector3 & vMin, const Vector3 & vMax);
	bool IsAvoidingNonMovableObjects() const { return m_iNavMeshRouteMethod == eRouteMethod_OnlyAvoidNonMovableObjects; }

	virtual void Restart();

	static bool TestClearance(const Vector3 & vStartIn, const Vector3 & vEndIn, const CPed * pPed, bool bStartTestAboveKnee);

	// Returns how many millisecs remain until task will warp the ped to their destination, or -1 if no warp will occur
	s32 GetWarpTimer() const { return m_WarpTimer.IsSet() ? m_WarpTimer.GetTimeLeft() : -1; }

	Vector3 PathRequest_DistanceAheadCalculation(const float fDistanceAhead, CPed * pPed);

protected:

	enum EWaypointActionResult
	{
		WaypointAction_EventCreated,		// created the event, eg. CEventClimbLadderOnRoute
		WaypointAction_EventNotNeeded,		// event wasn't required, we can just continue to follow route
		WaypointAction_ActionFailed,		// there was a problem & we should restart task, eg. a failed climb
		WaypointAction_ActionNotFound,		// there wasn't an action applicable at this waypoint
		WaypointAction_StateTransitioned	// instead of creating an event, the task has transitioned its state
	};

	virtual FSM_Return StateInitial_OnEnter(CPed * pPed);
	virtual FSM_Return StateInitial_OnUpdate(CPed * pPed);
	virtual FSM_Return StateWaitingInVehiclePriorToRequest_OnUpdate(CPed * pPed);
	virtual FSM_Return StateRequestPath_OnEnter(CPed * pPed);
	virtual FSM_Return StateRequestPath_OnUpdate(CPed * pPed);
	virtual FSM_Return StateMovingWhilstWaitingForPath_OnEnter(CPed * pPed);
	virtual FSM_Return StateMovingWhilstWaitingForPath_OnUpdate(CPed * pPed);
	virtual FSM_Return StateStationaryWhilstWaitingForPath_OnEnter(CPed * pPed);
	virtual FSM_Return StateStationaryWhilstWaitingForPath_OnUpdate(CPed * pPed);
	virtual FSM_Return StateWaitingToLeaveVehicle_OnUpdate(CPed * pPed);
	virtual FSM_Return StateWaitingAfterFailureToObtainPath_OnEnter(CPed * pPed);
	virtual FSM_Return StateWaitingAfterFailureToObtainPath_OnUpdate(CPed * pPed);
	virtual FSM_Return StateFollowingPath_OnEnter(CPed * pPed);
	virtual FSM_Return StateFollowingPath_OnUpdate(CPed * pPed);
	virtual FSM_Return StateSlideToCoord_OnEnter(CPed * pPed);
	virtual FSM_Return StateSlideToCoord_OnUpdate(CPed * pPed);
	virtual FSM_Return StateStoppingAtTarget_OnEnter(CPed * pPed);
	virtual FSM_Return StateStoppingAtTarget_OnUpdate(CPed * pPed);
	virtual FSM_Return StateWaitingForWaypointActionEvent_OnEnter(CPed * pPed);
	virtual FSM_Return StateWaitingForWaypointActionEvent_OnUpdate(CPed * pPed);
	virtual FSM_Return StateWaitingAfterFailedWaypointAction_OnEnter(CPed * pPed);
	virtual FSM_Return StateWaitingAfterFailedWaypointAction_OnUpdate(CPed * pPed);
	virtual FSM_Return StateGetOntoMainNavMesh_OnEnter(CPed * pPed);
	virtual FSM_Return StateGetOntoMainNavMesh_OnUpdate(CPed * pPed);
	virtual FSM_Return StateAchieveHeading_OnEnter(CPed * pPed);
	virtual FSM_Return StateAchieveHeading_OnUpdate(CPed * pPed);
	virtual FSM_Return StateUsingLadder_OnEnter(CPed * pPed);
	virtual FSM_Return StateUsingLadder_OnUpdate(CPed * pPed);

	virtual bool ShouldAbort(const AbortPriority priority, const aiEvent* pEvent);

	FSM_Return DecideStoppingState(CPed * pPed);
	bool IsNavigatingUnderwater(const CPed * pPed) const;

	bool RequestPath(CPed * pPed, const float fDistanceAheadOfPed=0.0f);
	bool RetrievePath(TPathResultInfo & pathResultInfo, CPed * pPed, CNavMeshRoute * pTargetRoute);
	void InitPathRequest_DistanceAheadCalculation(const float fDistanceAhead, CPed * pPed, TRequestPathStruct & reqPathStruct);	// helper
	bool CreateRouteFollowingTask(CPed * pPed);
	void CheckForInitialDoorObstruction(CPed * pPed, const Vector3 & vTarget);
	u32 CalcScanForObstructionsFreq() const;
	virtual u32 GetInitialScanForObstructionsTime();
	bool ScanForObstructions(CPed * pPed, const float fMaxDistanceAheadToScan, float & fOut_DistanceToObstruction) const;
	bool TestObjectLookAheadLOS(CPed * pPed);
	s32 GetNearbyEntitiesForLineOfSightTest(CPed * pPed, CEntityBoundAI * pEntityBoundsArray, const CEntity ** pEntityArray, const s32 iNumClosest, const s32 iMaxNum, const float fPedRadius, const float fPedZ) const;
	void CalcTimeNeededToCompleteRouteSegment(const CPed * pPed, const s32 iProgress);
	void SmoothPedLeanClipsForNewTarget(CPed * pPed);
	bool CanSwimStraightToNextTarget(CPed * pPed) const;
	void ScanForMovedOutsidePlaneBounds(CPed * pPed);
	float AdjustMoveBlendRatioForRouteLength(const float fMBR, const float fRouteLength) const;
	void ProcessLookAhead(CPed * pPed, const bool bWithinSlowDownDist, Vector3 & vOut_GotoPointTarget, Vector3 & vOut_LookAheadTarget);
	
	float GetLookAheadExtra() const;
	float GetLookAheadDistanceIncrement() const;
	float GetInitialGotoTargetLookAheadDistance() const;
	float GetGotoTargetLookAheadDistance() const;


	float CalculateFinalHeading(CPed * pPed) const;
	float GetPathToleranceForWaypoint(const int iProgress) const;

	virtual bool OnSuccessfulRoute(CPed * pPed);
	virtual FSM_Return OnFailedRoute(CPed * pPed, const TPathResultInfo & pathResultInfo);
	virtual bool IsPedMoving(CPed * pPed) const;	// Maybe this should be in the ped but I want full control here for now
	virtual bool ShouldKeepMovingWhilstWaitingForPath(CPed * pPed, bool & bMayUsePreviousTarget) const;
	virtual bool HandleNewTarget(CPed * pPed);
	virtual EWaypointActionResult CreateWaypointActionEvent(CPed * pPed, const s32 iProgress);
	virtual float GetTargetRadiusForWaypoint(const int iProgress) const;
	virtual bool ShouldSlowDownForWaypoint(const TNavMeshWaypointFlag & wptFlag) const;

	bool CreateClimbOutOfWaterEvent(CPed * pPed, const Vector3 & vPedPos);

	enum
	{
		GPAFlag_HitRouteEnd				= 0x1,
		GPAFlag_HitWaypointFlag			= 0x2,
		GPSFlag_HitWaypointFlagLadder	= 0x4,	// GPAFlag_HitWaypointFlag should also always be set when this is set
		GPAFlag_HitWaypointFlagPullOut	= 0x8,
		GPAFlag_UpcomingClimbOrDrop		= 0x10,	// Set when a climb or drop is ahead, even if we don't specify to stop at such a waypoint

		GPAFlag_MaskForRouteAhead = GPAFlag_HitRouteEnd | GPAFlag_HitWaypointFlag,
	};

	u32 GetPositionAheadOnRoute(const CNavMeshExpandedRouteVert * pPoints, const int iNumPts, Vec3V_In inputPos, 
		ScalarV_In distanceAhead, Vec3V_Ref outPos, ScalarV_Ref outDistanceRemaining, ScalarV* outLengthOfFinalSegment /* optional */,
		const bool bHaltAtClimbs, const bool bHaltAtDrops, ScalarV* pOutDistToClimb = NULL, ScalarV* pOutDistToDrop = NULL) const;

	virtual void ResetForRestart();

	void ModifyPathFlagsForPedLod(CPed * pPed, u64 & iFlags) const;

	bool CheckIfClimbIsPossible(const Vector3 & vClimbPos, float fClimbHeading, CPed * pPed) const;
	bool ScanForMissedClimb(CPed * pPed);
	bool TransformRouteOnDynamicNavmesh(const CPed * pPed);

	bool GetIsTaskSequencedBefore(const CPed * pPed, bool & bIsGotoTask);
	bool GetIsTaskSequencedAfter(const CPed * pPed, bool & bIsGotoTask);
	static bool GetIsTaskSequencedAdjacent(const CPed * pPed, const CTaskMove * pThisTask, bool & bIsGotoTask, const int iDirection);

	int CalculateClosestSegmentOnExpandedRoute(CNavMeshExpandedRouteVert * pExpandedRouteVerts, const int iNumVerts, const Vector3 & vTargetPos, float & fOut_SplineTVal, Vector3 * v_OutClosestPosOnRoute, Vector3 * v_OutClosestSegment);
	bool ProcessCornering(CPed * pPed, Vec3V_In vPosAhead, const float fLookAheadTime, const CMoveBlendRatioSpeeds& moveSpeeds, const CMoveBlendRatioTurnRates& turnRates);

	bool RetransformRouteOnDynamicNavmesh(const CPed * pPed);

	bool IsExitingScenario(Vec3V_InOut vExitPosition) const;
	bool IsExitingVehicle(Vec3V_InOut vExitPosition) const;
	bool ShouldWaitInVehicle() const;
	bool ForceVehicleDoorOpenForNavigation(CPed * pPed);

	void SwitchToNextRoute();

	class COnEntityRouteData
	{
	public:
		COnEntityRouteData()
		{
			m_pLocalSpaceRoute = NULL;
			m_pDynamicEntityRouteIsOn = NULL;
		}
		~COnEntityRouteData()
		{
			if(m_pLocalSpaceRoute)
				delete m_pLocalSpaceRoute;
		}
		Vector3 m_vLocalSpaceTarget;
		CNavMeshRoute * m_pLocalSpaceRoute;
		RegdEnt m_pDynamicEntityRouteIsOn;
	};

	Vector3 m_vTarget;
	Vector3 m_vNewTarget;
	Vector3 m_vClosestPosOnRoute;
	CNavBaseConfigFlags m_NavBaseConfigFlags;
	CNavBaseInternalFlags m_NavBaseInternalFlags;
	COnEntityRouteData * m_pOnEntityRouteData;
	CNavMeshRoute * m_pRoute;
	CNavMeshRoute * m_pNextRouteSection;
	TPathHandle m_iPathHandle;
	TPathHandle m_iLastPathHandle;
	CTaskGameTimer m_WarpTimer;
	CTaskGameTimer m_ScanForObstructionsTimer;
	CLadderClipSetRequestHelper* m_pLadderClipRequestHelper;	// Initiate from pool
	float m_vAdjustedTarget[3];
	float m_vDirectionalCoverThreatPos[2];
	float m_fInitialMoveBlendRatio;
	float m_fTargetRadius;
	float m_fCurrentTargetRadius;
	float m_fTargetStopHeading;
	float m_fSlowDownDistance;
	float m_fDistanceToMoveWhileWaitingForPath;
	float m_fMaxSlopeNavigable;
	float m_fSlideToCoordHeading;
	float m_fSlideToCoordSpeed;
	float m_fSlideToCoordHeadingChangeSpeed;
	float m_fSplineTVal;
	float m_fCurrentLookAheadDistance;
	float m_fAnimatedStoppingDistance;
	s32 m_iWarpTimeMs;
	s32 m_iTimeOfLastSetTargetCall;
	s16 m_iRecalcPathTimerMs;
	s16 m_iRecalcPathFreqMs;
	u16 m_iTimeSpentOnCurrentRouteSegmentMs;
	u16 m_iTimeShouldSpendOnCurrentRouteSegmentMs;
	s16 m_iFreeSpaceReferenceValue;
	s16 m_iClosestSegOnExpandedRoute;
	u8 m_iNumRouteAttempts;
	u8 m_iNavMeshRouteMethod;
	u8 m_iLastRecoveryMode;
	s8 m_iProgress;
	u8 m_iClampMinProgress;
	u8 m_iClampMaxSearchDistance;
	u8 m_iMaxDistanceToMovePathStart;	// fixedpt
	u8 m_iMaxDistanceToMovePathEnd;	// fixedpt

	// Expanded route used internally, in which route is decomposed to linear segments
	static CNavMeshExpandedRouteVert ms_vExpandedRouteVerts[ms_iMaxExpandedVerts];
	static s32 ms_iNumExpandedRouteVerts;
	static Vector3 ms_vClosestSegmentOnRoute;
};



class CTaskMoveGetOntoMainNavMesh : public CTaskMove
{
public:

	enum
	{
		State_Initial,
		State_CalculatingAreaUnderfoot,
		State_FindingReversePath,
		State_MovingTowardsTarget,
		State_MovingToLastNonIsolatedPosition
	};

	enum
	{
		RecoveryMode_GoToLastNonIsolatedPosition	= 0,
		RecoveryMode_ReversePathFind,
		RecoveryMode_NumModes
	};

	static const float ms_fTargetRadius;
	static const float ms_fMaxAreaToAttempt;

	CTaskMoveGetOntoMainNavMesh(const float fMoveBlendRatio, const Vector3 & vTarget, const int iRecoveryMode, const int iAttemptTime=4000);
	virtual ~CTaskMoveGetOntoMainNavMesh();

	virtual aiTask* Copy() const
	{
		return rage_new CTaskMoveGetOntoMainNavMesh(m_fMoveBlendRatio, m_vTarget, m_iRecoveryMode, m_iAttemptTime);
	}

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_GET_ONTO_MAIN_NAVMESH; }

#if !__FINAL
	virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_Initial : return "Initial";
			case State_CalculatingAreaUnderfoot: return "CalculatingAreaUnderfoot";
			case State_FindingReversePath: return "FindingReversePath";
			case State_MovingTowardsTarget: return "MovingTowardsTarget";
			case State_MovingToLastNonIsolatedPosition: return "MovingToLastNonIsolatedPosition";
		}
		Assert(false);
		return NULL;
	}
#endif 

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

	FSM_Return Initial_OnUpdate(CPed * pPed);
	FSM_Return CalculatingAreaUnderfoot_OnUpdate(CPed * pPed);
	FSM_Return FindingReversePath_OnUpdate(CPed * pPed);
	FSM_Return MovingTowardsTarget_OnUpdate(CPed * pPed);
	FSM_Return MovingToLastNonIsolatedPosition_OnUpdate(CPed * pPed);

	virtual bool HasTarget() const { return true; }
	virtual Vector3 GetTarget() const { return m_vTarget; }
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }
	virtual float GetTargetRadius() const { return ms_fTargetRadius; }

protected:

	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	enum eReqResult
	{
		EStillWaiting,
		EContinue,
		EQuit
	};

	Vector3 m_vNavMeshTarget;
	Vector3 m_vTarget;
	s32 m_iAttemptTime;
	s32 m_iRecoveryMode;
	CTaskGameTimer m_AttemptTimer;
	TPathHandle m_hPathHandle;
	TPathHandle m_hCalcAreaUnderfootHandle;
	bool m_bIsOnSmallAreaOfNavMesh;

	eReqResult GetPathRequestResult(const CPed * pPed);
	eReqResult GetAreaRequestResult(const CPed * pPed);

	CTask * CreateSubTask(const int iTaskType, CPed* pPed);
};


#endif

