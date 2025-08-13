#ifndef ASSISTEDMOVEMENTSTORE_H
#define ASSISTEDMOVEMENTSTORE_H

//Rage headers.
#include "Script/thread.h"
#include "Vector/Vector3.h"
#include "fwvehicleai/pathfindtypes.h"
#include "system/memops.h"

class CEntity;
class CPed;
class CObject;

//****************************************************************************
//	CAssistedMovementRoute
//	A route which the player will snap to when moving through the world.
//	This is intended to help guide the player through awkward areas - such
//	as across narrow walkways, stairwells, etc.
//	We might want to extend this class to contain more info about each point,
//	such as a snap-strength, or area of effect.

class CAssistedMovementRoute
{
	friend class CAssistedMovementRouteStore;
	friend class CPedDebugVisualiserMenu;

public:

	enum eRouteFlags
	{
		RF_HasBeenPostProcessed				= 0x01,
		RF_IsActiveWhenStrafing				= 0x02,
		RF_DisableInForwardsDirection		= 0x04,
		RF_DisableInReverseDirection		= 0x08
	};
	enum eRoutePointFlags	// Be sure to keep these in sync with script enum in "commands_player.sch"
	{
		RPF_ReduceSpeedForCorners	= 0x01,
		RPF_IsUnderwater			= 0x02
	};
	struct TRoutePoint
	{
		Vector3 m_vPos;			// also contains 'm_fWidth' in W component
		Vector3 m_vTangent;		// also contains 'm_fTension' in W component
	};

	CAssistedMovementRoute();
	inline ~CAssistedMovementRoute() { }

	enum eRouteType
	{
		ROUTETYPE_NONE				= 0,
		ROUTETYPE_MAP				= 1,
		ROUTETYPE_DOOR				= 2,
		ROUTETYPE_SCRIPT			= 4,
		ROUTETYPE_WAYPOINTRECORDING	= 8,
		ROUTETYPE_DEBUG				= 16,
		ROUTEYYPE_ANY				= ROUTETYPE_MAP + ROUTETYPE_DOOR + ROUTETYPE_SCRIPT + ROUTETYPE_WAYPOINTRECORDING + ROUTETYPE_DEBUG
	};
	enum
	{ 
		MAX_NUM_ROUTE_ELEMENTS	= 64
	};

	void Init();
	inline void Clear() { Init(); }

	inline const Vector3 & GetMin() const { return m_vMin; }
	inline const Vector3 & GetMax() const { return m_vMax; }

	inline s32 GetSize() const { return m_iSize; }
	inline float GetLength() const { return m_fLength; }

	inline u32 GetFlags() const { return m_iFlags; }
	inline void SetFlags(const u32 f) { m_iFlags = f; }

	Vector3 GetPathBoundaryPoint(const s32 p, const float fPathWidth);
	bool GetClosestPos(const Vector3 & vSrcPos, int & iOutRouteSegment, float & fOutRouteSegmentProgress, Vector3 & vOutPosOnRoute, Vector3 & vOutRouteNormal, Vector3 & vOutTangent, u32 & iOutFlags) const;
	float GetClosestPos(const Vector3 & vSrcPos, Vector3 * vOutPosOnRoute=NULL, Vector3 * vOutRouteNormal=NULL, Vector3 * vOutTangent=NULL, float * fOutTension=NULL, u32 * iOutFlags=0) const;

	bool GetAtDistAlong(float fInputDist, Vector3 * vOutPosOnRoute=NULL, Vector3 * vOutRouteNormal=NULL, Vector3 * vOutTangent=NULL, float * fOutTension=NULL) const;

	inline u32 GetPathStreetNameHash() const { return m_iPathStreetNameHash; }
	inline void SetPathStreetNameHash(const u32 i) { m_iPathStreetNameHash = i; }

	inline void SetRouteType(const s32 r) { m_iRouteType = r; }
	inline s32 GetRouteType() const { return m_iRouteType; }

	// Calculate total length, tangents & min/max
	void PostProcessRoute();

	inline void CopyFrom(const CAssistedMovementRoute & src)
	{
		sysMemCpy(this, &src, sizeof(CAssistedMovementRoute));
	}


	inline void AddPoint(const Vector3 & vPos, const float fWidth=ms_fDefaultPathWidth, const u32 iFlags=0, const float fTension=ms_fDefaultPathTension)
	{
		Assert(m_iSize < MAX_NUM_ROUTE_ELEMENTS);
		m_Points[m_iSize].m_vPos = vPos;
		m_Points[m_iSize].m_vPos.w = fWidth;
		m_Points[m_iSize].m_vTangent = VEC3_ZERO;
		m_Points[m_iSize].m_vTangent.w = fTension;

		m_PointFlags[m_iSize] = iFlags;

		m_iSize++;
	}
	// Element access functions
	inline const Vector3 & GetPoint(const s32 p) const
	{
		Assert(p>=0 && p<MAX_NUM_ROUTE_ELEMENTS);
		return m_Points[p].m_vPos;
	}
	inline float GetPointWidth(const s32 p) const
	{
		Assert(p>=0 && p<MAX_NUM_ROUTE_ELEMENTS);
		return m_Points[p].m_vPos.w;
	}
	inline u32 GetPointFlags(const s32 p) const
	{
		Assert(p>=0 && p<MAX_NUM_ROUTE_ELEMENTS);
		return m_PointFlags[p];
	}
	inline float GetPointTension(const s32 p) const
	{
		Assert(p>=0 && p<MAX_NUM_ROUTE_ELEMENTS);
		return m_Points[p].m_vTangent.w;
	}
	inline const Vector3 & GetPointTangent(const s32 p) const
	{
		Assert(p>=0 && p<MAX_NUM_ROUTE_ELEMENTS);
		return m_Points[p].m_vTangent;
	}

	static const float ms_fDefaultPathWidth;
	static const float ms_fDefaultPathTension;
	static const float ms_fTangentLerpMaxDist;

private:

	// The min/max of all the points
	Vector3 m_vMin;
	Vector3 m_vMax;

	TRoutePoint m_Points[MAX_NUM_ROUTE_ELEMENTS];
	u32 m_PointFlags[MAX_NUM_ROUTE_ELEMENTS];	// moved out of TRoutePoint so that memory may be packed better

	u32 m_iFlags;
	s32 m_iSize;
	float m_fLength;
	// The unique index of this route within the CPathFind system (see "VehicleAI/pathfind.h")
	u32 m_iPathStreetNameHash;
	// Used to distinguish between routes which came from the map, were auto-generated from doors, or came from a script.
	s32 m_iRouteType;
};


class CAssistedMovementToggles
{
public:

	enum { MAX_NUM_TOGGLES = 32 };

	CAssistedMovementToggles();

	void Init();

	bool TurnOnThisRoute(const u32 iRouteNameHash, const scrThreadId iScriptThreadId);
	bool TurnOffThisRoute(const u32 iRouteNameHash, const scrThreadId iScriptThreadId);

	inline bool GetHasChanged() const { return m_bHasChanged; }
	inline void SetHasChanged(const bool b) { m_bHasChanged = b; }

	bool IsThisRouteRequested(const u32 iRouteNameHash, CPathNode * pPathNode=NULL) const;

	void ResetAll(const scrThreadId iThreadId);

private:

	class CToggle
	{
	public:	
		// Which script does this toggle belong to
		scrThreadId m_iScriptThreadId;
		// Hashname of the route	
		u32 m_iRouteNameHash;

		CToggle() { Reset(); }
		void Reset()
		{
			m_iRouteNameHash = 0;
			m_iScriptThreadId = THREAD_INVALID;
		}
	};

	bool m_bHasChanged;
	CToggle m_Toggles[MAX_NUM_TOGGLES];
};

//**************************************************************
//	CAssistedMovementRouteStore
//	A storage class for the routes.

class CAssistedMovementRouteStore
{
	friend class CPedDebugVisualiserMenu;

public:
	CAssistedMovementRouteStore() { }
	~CAssistedMovementRouteStore() { }

	enum
	{
		MAX_MAP_ROUTES			= 72,
		MAX_SCRIPTED_ROUTES		= 8,
		MAX_WAYPOINT_ROUTES		= 8,
		MAX_ROUTES				= MAX_MAP_ROUTES + MAX_SCRIPTED_ROUTES + MAX_WAYPOINT_ROUTES,
		FIRST_MAP_ROUTE			= 0,
		FIRST_SCRIPTED_ROUTE	= FIRST_MAP_ROUTE + MAX_MAP_ROUTES,
		FIRST_WAYPOINT_ROUTE	= FIRST_SCRIPTED_ROUTE + MAX_SCRIPTED_ROUTES
	};

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static CAssistedMovementToggles & GetRouteToggles() { return m_RouteToggles; }

	static inline CAssistedMovementRoute * GetRoute(const int r)
	{
		Assert(r >=0 && r < MAX_ROUTES);
		return &ms_RouteStore[r];
	}
	static inline CAssistedMovementRoute * GetRouteByNameHash(const u32 iHash)
	{
		Assert(iHash);
		if(!iHash)
			return NULL;
		for(int i=0; i<MAX_ROUTES; i++)
		{
			if( ms_RouteStore[i].GetPathStreetNameHash()==iHash )
			{
				return &ms_RouteStore[i];
			}
		}
		return NULL;
	}
	static inline CAssistedMovementRoute * GetEmptyRoute(const bool bMapPool, const bool bScriptPool, const bool bWaypointRecordingPool)
	{
		Assert(!(bMapPool&&bScriptPool));
		Assert(!(bMapPool&&bWaypointRecordingPool));
		Assert(!(bScriptPool&&bWaypointRecordingPool));

		int r;
		if(bMapPool)
		{
			for(r=FIRST_MAP_ROUTE; r<MAX_MAP_ROUTES; r++)
				if(ms_RouteStore[r].m_iSize==0 && ms_RouteStore[r].m_iPathStreetNameHash==0)
					return &ms_RouteStore[r];
		}
		else if(bScriptPool)
		{
			for(r=FIRST_SCRIPTED_ROUTE; r<FIRST_SCRIPTED_ROUTE+MAX_SCRIPTED_ROUTES; r++)
				if(ms_RouteStore[r].m_iSize==0 && ms_RouteStore[r].m_iPathStreetNameHash==0)
					return &ms_RouteStore[r];
		}
		else if(bWaypointRecordingPool)
		{
			for(r=FIRST_WAYPOINT_ROUTE; r<FIRST_WAYPOINT_ROUTE+MAX_WAYPOINT_ROUTES; r++)
				if(ms_RouteStore[r].m_iSize==0 && ms_RouteStore[r].m_iPathStreetNameHash==0)
					return &ms_RouteStore[r];
		}
		return NULL;
	}
	static inline void ClearAll(const s32 iTypeMask)
	{
		for(int r=0; r<MAX_ROUTES; r++)
		{
			if(ms_RouteStore[r].GetRouteType() & iTypeMask)
				ms_RouteStore[r].Clear();
		}
	}

	static void Process(CPed * pPed, const bool bForceUpdate);

	static void MaybeAddRouteForDoor(CEntity * pEntity);
	static void MaybeRemoveRouteForDoor(CEntity * pEntity);

	static inline int GetScriptRouteEditIndex() { return ms_iScriptRouteEditIndex; }
	static inline void SetScriptRouteEditIndex(const int i) { ms_iScriptRouteEditIndex = i; }

	static u32 CalcDoorHash(CObject * pDoor);
	static u32 CalcScriptedRouteHash(const int iRouteIndex);

	static void SetStreamingLoadDistance(float f) { ms_fStreamingLoadDistance = f; }

	static bool ShouldProcessNode(CPathNode * pNode);

#if !__FINAL
	static void Debug();
#if __BANK
	static void RescanNow();
#endif
#endif

private:

	static CAssistedMovementRoute * ms_RouteStore;

	//***************************************************************************
	// The assisted movement routes are stored in the CPathFind node network,
	// which handles their streaming, etc.  Every so often this store is updated
	// from the node system - far away routes are cleared out, and new ones are
	// added.  We must also make sure that existing routes are updates because
	// they may not always be all loaded in if they straddle a region boundary.

	static void UpdateRoutesFromNodesSystem(const Vector3 & vOrigin);

	static void UpdateRoutesFromWaypointRecordings(const Vector3 & vOrigin);

	static bool IsPermanentRoute(const CPathNode * pNode);

	static Vector3 m_vLastOrigin;

	// The index of the script route being created via script commands - or '-1' if none.
	static int ms_iScriptRouteEditIndex;

	static float ms_fStreamingLoadDistance;
	static float ms_fStreamingUpdatePosDeltaSqr;
	static const float ms_fDefaultStreamingLoadDistance;
	static bank_float ms_fDistOutFromDoor;
	static bank_bool ms_bAutoGenerateRoutesInDoorways;

	static CAssistedMovementToggles m_RouteToggles;
};

#endif // ASSISTEDMOVEMENTSTORE_H
