/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    gps.h
// PURPOSE : Everything having to do with gps. This code will store a list of nodes that
//			 would take the player to a particular destination.
//			 Also generates appropriates sound triggers; turn left after 400 yards.
// AUTHOR :  Matthew Smith, Obbe Vermeij.
// CREATED : 11/04/07
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef GPS_H
#define GPS_H

// rage includes
#include "script/thread.h"
#include "vector/vector3.h"
#include "fwControl/WaypointRecordingRoute.h"

// game includes
#include "scene/RegdRefTypes.h"
#include "templates/DblBuf.h"
#include "vehicleAi/pathfind.h"


#define GPS_SLOT_USED_FOR_RACE_TRACK		(GPS_SLOT_RADAR_BLIP)		// One of the 2 slots is also used to display race tracks.
#define GPS_SLOT_USED_FOR_WAYPOINTS_GPS		(GPS_SLOT_RADAR_BLIP)		// One of the 2 slots is also used to display waypoints GPS.
#define GPS_SLOT_USED_FOR_CUSTOM_ROUTE		(GPS_SLOT_RADAR_BLIP)		// One of the 2 slots is also used to display designer-specified routes.
#define GPS_SLOT_USED_FOR_DISCRETE_ROUTE	(GPS_SLOT_DISCRETE)			// Used for haxxor

#define NODEINFO_LINKS_MASK		(15)
#define NODEINFO_HIGHWAY_BIT	(1<<7)
#define NODEINFO_JUNCTION_BIT	(1<<8)

#define PARTIAL_DESTINATION_PATH_LENGTH (1000)
#define PARTIAL_DESTINATION_PATH_DIST_FROM_PLAYER_SQR (122500.f) // 350 * 350
#define PARTIAL_DESTINATION_RECALCULATE_LENGTH (750)

// We store important information in the w component in our node position list
enum eGpsNodePosInfo
{
	GNI_IGNORE_FOR_NAV = 0x00008000,	// Just some cool bit
	GNI_PLAYER_TRAIL_POS = 0x00004000,
};

/////////////////////////////////////////////////////////////////////
// CGpsSlot
// One unit that can perform gps type routefinding. It stores a bunch of nodes
// for the player to follow.
/////////////////////////////////////////////////////////////////////
#define INVALID_GPS_INSTRUCTION (-1)

class CGpsSlot
{
public:
	CGpsSlot():m_NodeCoordinates(NULL),m_NodeAdress(NULL), m_NodeInfo(NULL), m_NodeDistanceToTarget(NULL){}

	static bank_s32 m_iMinDurationBeforeProgressingRoute;
	static bank_s32 m_iMinDurationBetweenRecalculatingRoute;
	static bank_s32 m_iMaxDurationBeforeRedrawingRoute;
	static bank_s32 m_iMaxZDistanceBeforeRecalculatingRoute;
	static bank_s32 m_iTargetEntityMaxDistMayMoveBeforeRecalculatingRoute;
	static bank_s32 m_iPlayerMaxDistMayMoveBeforeRecalculatingRoute;
	static bank_s32 m_iCheckNumSegments;
	static bank_s32 m_iScanNodesFreqMs;

public:

	enum
	{
		GPS_NUM_NODES_STORED = 1024
	};

	enum
	{
		GPS_ROUTE_FLAG_NONE = 0,
		GPS_ROUTE_FLAG_ON_FOOT = 1,
		GPS_ROUTE_FLAG_IN_VEHICLE = 2
	};

	enum eStatus
	{
		STATUS_OFF,

		STATUS_START_CALCULATING_RANGE,
		STATUS_CALCULATING_ROUTE = STATUS_START_CALCULATING_RANGE,
		STATUS_CALCULATING_ROUTE_QUIET,
		STATUS_CALCULATING_ROUTE_RACE_TRACK,
		STATUS_CALCULATING_ROUTE_MULTI,
		STATUS_END_CALCULATING_RANGE = STATUS_CALCULATING_ROUTE_MULTI,

		STATUS_START_WORKING_RANGE,
		STATUS_WORKING = STATUS_START_WORKING_RANGE,
		STATUS_WORKING_RACE_TRACK,
		STATUS_WORKING_ROUTE_MULTI,
		STATUS_WORKING_CUSTOM_ROUTE,
		STATUS_END_WORKING_RANGE = STATUS_WORKING_CUSTOM_ROUTE,
	};

	enum GpsInstruction
	{	// Be careful when adding new instructions, make sure to keep ConvertPathDirectionToInstruction() in sync
		GPS_INSTRUCTION_UNKNOWN,
		GPS_INSTRUCTION_TURN_LEFT,
		GPS_INSTRUCTION_TURN_RIGHT,
		GPS_INSTRUCTION_KEEP_LEFT,
		GPS_INSTRUCTION_KEEP_RIGHT,
		GPS_INSTRUCTION_STRAIGHT_AHEAD,
		GPS_INSTRUCTION_UTURN,
		GPS_INSTRUCTION_CALCULATING_ROUTE,
		GPS_INSTRUCTION_HIGHLIGHTED_ROUTE,
		GPS_INSTRUCTION_YOU_HAVE_ARRIVED,
		GPS_INSTRUCTION_BONG,
		GPS_INSTRUCTION_MAX,
	};

	Vector3	GetNextNodePositionAlongRoute(bool bStartAtPlayerPosition = true, float fDistanceAlongRoute = 0.0f);

	void	Init(s32 slotIndex, unsigned initmode);
	void	Shutdown(unsigned shutdownmode);
	void	Clear(s32 slotIndex, bool bClearGpsOnMiniMap);
	void	Start(Vector3 &newDestination, CEntity *pEntity, s32 identifier, bool bFastCalculatingPhase, s32 slotIndex, Color32 colour);
	void	Update(s32 slotIndex, bool bTalk);
	void	UpdateRoute();
	s32		CalcClosestSegment(s32 iStartFromSeg, float* pfOutSmallestDistSqr = NULL, bool bWholeRoute = false);
	void	ShiftRoute(s32 number);
	char	*ThinkOfSomethingToSay();
	char	*FindDistanceStringWithDistance(float distInMeters, s32* audioID = NULL);

	void	Render(s32 iRouteNum, float offsetX, float offsetY, bool bClipFirstLine, float clipX, float clipY);
	void	RenderRaceTrack(s32 iRouteNum, float offsetX, float offsetY);
	void	RenderMultiGPS(s32 iRouteNum, float offsetX, float offsetY);
	void	RenderCustomRoute(s32 iRouteNum, float offsetX, float offsetY);
	bool	CheckForReachingDestination();
	s32		ConvertPathDirectionToInstruction(s32 PathFindDirection);
	s32		CalcRouteInstruction(float* pfOutDistance = NULL);

	void	SetColor(const Color32& color) {m_Colour = color;}
	const Color32& GetColor() const {return m_Colour;}

	void	SetCurrentRenderStatus(bool value);
	bool	GetCurrentRenderStatus() const {return m_bCurrentRenderStatus;}

	void	SetRenderedLastFrame() {m_bRenderedLastFrame = true;}
	bool	GetRenderedLastFrame() const {return m_bRenderedLastFrame;}

	s16		GetDistanceToTarget();//gets the distance along the path to the target
	void	CalcDistanceToTarget();

	CEntity * GetTargetEntity() { return m_pTargetEntity; }
	const Vector3 & GetDestination() const { return m_Destination; }
	Vector3 GetRouteEnd() const { return (m_NumNodes > 0 ? m_NodeCoordinates[m_NumNodes-1] : m_Destination); }

#if __BANK
	void DebugRender();
#endif

	void SetStatus(eStatus newStatus) {m_Status = newStatus;}
	eStatus GetStatus() const {return m_Status;}
	bool IsOn() const {return m_Status != STATUS_OFF;}
	bool IsCalculating() const {return STATUS_START_CALCULATING_RANGE <= m_Status  && m_Status <= STATUS_END_CALCULATING_RANGE;}
	bool IsWorking() const {return STATUS_START_WORKING_RANGE <= m_Status  && m_Status <= STATUS_END_WORKING_RANGE;}
	bool IsRaceTrackRoute() const {return m_Status == STATUS_CALCULATING_ROUTE_RACE_TRACK || m_Status == STATUS_WORKING_RACE_TRACK;}
	bool IsMultiRoute() const {return m_Status == STATUS_CALCULATING_ROUTE_MULTI || m_Status == STATUS_WORKING_ROUTE_MULTI;}

private:

	bool Announce(s32 instruction, f32 distance = -1.0f, s32 nodeId = -1, u32 timeToDestination = 0, f32 speed = -1.0f, s32 immediatelyInstruction = -1);
	void SayTurn(bool rightTurn, f32 runningDist, u32 timeToDestination, s32 immediatelyInstruction);
	void SaySingleLine(s32 phraseId);

	void SetCalculatePartialRouteAndDestination();

public:
	Vector3*		m_NodeCoordinates;		// We need to store the coordinates as the path nodes may have to be streamed out.
	CNodeAddress*	m_NodeAdress;
	u16*			m_NodeInfo;
	s16*			m_NodeDistanceToTarget;
	Vector3		m_Destination;									// Position of the target we are aiming for
	Vector3		m_PartialDestination;
	scrThreadId m_iScriptIdSetGpsFlags;
	s32			m_NumNodes;
	u32			m_LastRecalcRouteTime;
	u32			m_LastPartialRouteTime;
	u32			m_LastShiftRouteTime;
	u32			m_iScanNodesTimer;
	u32			m_RedrawTimer;
	s32			m_Identifier;									// This can be used to make sure it gets removed when the blip that triggered it gets removed (and not another one)
	s32			m_SlotIndex;									// Which slot is this? (0 or 1)
	float		m_fShowRouteProximity;							// When within this distance of the target entity, no route will be drawn
	float		m_LastRouteDistance;
	u16			m_iGpsFlags;									// Bitfield with members of the GPSFlags enum
	bool		m_bClearRender;
	u8			m_uCustomRouteFlags;
	u8			m_fastCalculatingPhase:1;						// Calculate new route as fast as possible
	u8			m_targetEntitySpecified:1;						// Was this gps route started with a target entity?
	u8			m_bUsePartialDestination:1;
	u8			m_bLastRouteSearchWasIgnoreOneWay:1;

private:	
	bool m_bCurrentRenderStatus:1;
	bool m_bRenderedLastFrame:1;
	bool m_AlreadyAnnouncedCalculatingRoute:1;
	bool m_AlreadyAnnouncedHighlighted:1;
	bool m_InCarLastUpdate:1;
	bool m_LastAnnouncedWasADouble:1;

	eStatus	m_Status;
	s32 m_ConsecutiveUTurnRequests;
	s32  m_LastAnnouncedNode;
	f32 m_LastAnnouncedDistance;
	u32 m_LastAnnouncedTime;
	s32 m_LastBongedNode;
	f32 m_DistanceFromRoute;
	u32 m_DontSayAnythingBeforeTime;
	float m_DistanceOfRoute;
	s32 m_iClosestRaceTrackSegment;
	Color32		m_Colour;										// RGBA value held in u32
	RegdEnt	m_pTargetEntity;
};



/////////////////////////////////////////////////////////////////////
// CGps
// One unit that can perform gps type routefinding. It stores a bunch of nodes
// for the player to follow.
/////////////////////////////////////////////////////////////////////

#define MAX_CUSTOM_ADDED_POINTS (fwWaypointRecordingRoute::ms_iMaxNumWaypoints)

class CGps
{
public:

	static	CGpsSlot	m_Slots[GPS_NUM_SLOTS];
	static	s32			ms_GpsNumNodesStored;
	static	s32			m_EnableAudio;
	static	bool		m_AudioCurrentlyDisabled;
	static  bool		m_bIsASyncSearchBlockedThisFrame;
	static	bool		m_bASyncStatusSearchSlot[CPathFind::GPS_ASYNC_SEARCH_NUM_SLOTS];

	// The level designers can specify a racetrack that is displayed just like a gps trail.
	// The points for this track are stored here.
	static	s32	m_iNumCustomAddedPoints;
	static	Vector3 m_aCustomAddedPoints[MAX_CUSTOM_ADDED_POINTS];
	static	int m_iMultiRouteStartNodeProgress[MAX_CUSTOM_ADDED_POINTS];
	static	CNodeAddress m_aCustomAddedNodes[MAX_CUSTOM_ADDED_POINTS];	// In theory, we could replace this with the m_aCustomAddedPoints
	static	Color32	m_CustomRouteColour;
	static	float m_iCustomRouteMiniMapWidth;
	static	float m_iCustomRoutePauseMapWidth;
	static	bool m_bMultiGpsTracksPlayer;
	static  bool m_bIsFlashingGPS;
	static  bool m_bShowGPSWhileFlashing;
	static  u32 m_bFlashGPSTime;

	static bank_float ms_fNodeRequestMarginMin;
	static bank_float ms_fNodeRequestMarginMax;
	static bank_float ms_iRoadWidthMiniMap;
	static bank_float ms_iRoadWidthPauseMap;

	static float	CalcRequiredMarginX(const Vector3& vStart, const Vector3& vDestination);

	static	void	StartRaceTrack(s32 colour);
	static	void	AddPointToRaceTrack(Vector3 &pnt);
	static	void	RenderRaceTrack(bool bOn);

	static	void	StartMultiGPS(s32 colour, bool bTrackPlayer, bool bOnFoot);
	static	void	AddPointToMultiGPS(const Vector3 &pnt);
	static	void	RenderMultiGPS(bool bOn);

	static	void	StartCustomRoute(s32 colour, bool bOnFoot, bool bInVehicle);
	static	void	AddPointToCustomRoute(Vector3 &pnt);
	static	void	InitCustomRouteFromWaypointRecordingRoute(const char * pName, int iStartIndex, int iNumPts, s32 colour, bool bOnFoot, bool bInVehicle);
	static	void	InitCustomRouteFromAssistedMovementRoute(const char * pName, int iDirection, int iStartIndex, int iNumPts, s32 colour, bool bOnFoot, bool bInVehicle);
	static	void	RenderCustomRoute(bool bOn, int iMiniMapWidth, int iPauseMapWidth);

	static	void	Init(unsigned initMode);
	static	void	Shutdown(unsigned shutdownMode);
	static	void	Update();
	static	void	ClearGpsOnMiniMap(s32 iSlot);
	static	void	ForceUpdateOfGpsOnMiniMap();
	static	void	UpdateGpsOnMiniMapOnRT();
	static  void	SetGpsFlashes(bool bFlash);
	static	bool	ShouldGpsBeVisible(s32 iSlot, bool checkMiniMap = true);
	static	void	StartRadarBlipGps(Vector3 &newDestination, CEntity *pEntity, s32 identifier, Color32 colour);
	static	void	StopRadarBlipGps(s32 identifier);
	static	void	ChangeRadarBlipGpsColour(s32 identifier, Color32 colour);
	static	void	EnableAudio(s32 enableAudio){m_EnableAudio = enableAudio;};
	static	void	TemporarilyDisableAudio(bool disableAudio){m_AudioCurrentlyDisabled = disableAudio;};

	static CPed* GetRelevantPlayer();
	static CVehicle* GetRelevantPlayerVehicle();
	static Vector3 GetRelevantPlayerCoords();

	static CGpsSlot & GetSlot(s32 iSlot) { Assert(iSlot>=0 && iSlot<GPS_NUM_SLOTS); return m_Slots[iSlot]; }

	static	s32		CalcRouteInstruction(s32 iSlot, float* pfOutDistance = 0);
	static	s16		GetRouteLength(s32 iSlot);
	static  Color32 GetRouteColour(s32 iSlot);
	static	bool	GetRouteFound(s32 iSlot);

	static void SetGpsFlagsForScript(const scrThreadId iScriptId, const u32 iFlags, const float fBlipRouteDisplayDistance=0.0f);
	static void ClearGpsFlagsOnScriptExit(const scrThreadId iScriptId);

	static bool FindClosestNodeLinkCB(CPathNode * pNode, void * pData);

#if __BANK
	static	bool	m_bDebug;
	static  void	InitWidgets();
#endif
	
private:
	static CDblBuf<Vector3> ms_relevantPlayerCoords;
};

#endif
