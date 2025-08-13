
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    junctions.h
// PURPOSE : When cars cross paths on a junction this code will stop one of the two from entering the junction. 
// AUTHOR :  Obbe.
// CREATED : 16/4/07
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef JUNCTIONS_H
#define JUNCTIONS_H

#define __JUNCTION_EDITOR			__DEV

// rage includes
#include "parser/macros.h"

// framework includes
#include "vehicleAi/pathfind.h"

// game includes
#include "scene/RegdRefTypes.h"
#include "script/thread.h"
#include "game_config.h"

//forward declarations gta
class CDoor;

#define	JUNCTIONS_MAX_TEMPLATES						150
#define JUNCTIONS_MAX_AUTO_ADJUSTMENTS				8
#define MAX_ROADS_INTO_JUNCTION						16
#define MAX_TRAFFIC_LIGHT_LOCATIONS					8
#define MAX_JUNCTION_NODES_PER_JUNCTION				8

#define TRAFFIC_LIGHT_POS_INVALID					0x7FFF

enum ETrafficLightCommand
{
	TRAFFICLIGHT_COMMAND_INVALID,
	TRAFFICLIGHT_COMMAND_STOP,
	TRAFFICLIGHT_COMMAND_AMBERLIGHT,
	TRAFFICLIGHT_COMMAND_GO,
	TRAFFICLIGHT_COMMAND_FILTER_LEFT,
	TRAFFICLIGHT_COMMAND_FILTER_RIGHT,
	TRAFFICLIGHT_COMMAND_FILTER_MIDDLE,
	TRAFFICLIGHT_COMMAND_PED_WALK,
	TRAFFICLIGHT_COMMAND_PED_DONTWALK
};

enum ERailwayCrossingLightState
{
	RAILWAY_CROSSING_LIGHT_OFF,
	RAILWAY_CROSSING_LIGHT_PULSE_LEFT,
	RAILWAY_CROSSING_LIGHT_PULSE_RIGHT
};

class CTrainTrackNode;
class CVirtualJunction;

//------------------------------------------------------
// CJunctionEntrance
// Defines the entrance/exit into a junction

class CJunctionEntrance
{
public:

	void Clear();

	// The position of the entrance node
	Vector3 m_vPositionOfNode;

	// The direction of the road into the junction
	Vector2 m_vEntryDir;

	// The node address (found by matching with the position above)
	CNodeAddress m_Node;

	// The distance before the junction at which cars should stop
	float m_EntryStopDistance;

	u32 m_iNumVehicles						: 8;

	// The lights phase which this entrance operates upon
	s32 m_iPhase							: 4;

	// If this is a multi-lane entrance, it can have its own left filter lane
	s32 m_iLeftFilterPhase					: 4;

	// If this is a valid entrance, then cars in the left lane will always filter through to this exit
	bool m_bCanTurnRightOnRedLight			: 1;
	bool m_bLeftLaneIsAheadOnly				: 1;
	bool m_bRightLaneIsRightOnly			: 1;
	bool m_bIsGiveWay						: 1;
	bool m_bIsSwitchedOff						: 1;
	bool m_bIgnoreBackedUpExitsOnStraight	: 1;
	bool m_bHasPlayer						: 1;	// 10 bits available

	// TODO: Add some kind of identified to link to the traffic lights model for this entrance
};

//----------------------------------------------------------------------------------
// CJunctionTemplate
// A junction template is a stored instance of a junction which has been authored
// by hand using the in-game Junction Editor.
// When a junction is created at runtime, a check is first made against a store of
// templates to see whether one exists for that junction position.

class CJunctionTemplate
{
public:

	enum
	{
		Flag_NonEmpty			= 0x01,
		Flag_RailwayCrossing	= 0x02,
		Flag_DisableSkipPedLightPhase = 0x04
	};

	CJunctionTemplate();

	class CEntrance
	{
	public:
		Vector3 m_vNodePosition;
		s32 m_iPhase;
		float m_fStoppingDistance;
		float m_fOrientation;
		float m_fAngleFromCenter;		// Just used for clockwise ordering
		bool m_bLeftLaneIsAheadOnly;
		bool m_bCanTurnRightOnRedLight;
		bool m_bRightLaneIsRightOnly;
		s32 m_iLeftFilterLanePhase;

		PAR_SIMPLE_PARSABLE;
	};
	class CPhaseTiming	// Kept start & end times, in case we want to have periods of no traffic - for peds to cross, etc
	{
	public:
		float m_fStartTime;
		float m_fDuration;

		PAR_SIMPLE_PARSABLE;
	};
	class CTrafficLightLocation
	{
	public:
		s16 m_iPosX;
		s16 m_iPosY;
		s16 m_iPosZ;
		
		inline CTrafficLightLocation() { m_iPosX = m_iPosY = m_iPosZ = TRAFFIC_LIGHT_POS_INVALID; }
		inline void GetAsVec3(Vector3 & v) const { v.x = m_iPosX; v.y = m_iPosY; v.z = m_iPosZ; }

		PAR_SIMPLE_PARSABLE;
	};
	u32 m_iFlags;
	s32 m_iNumJunctionNodes;
	s32 m_iNumEntrances;
	s32 m_iNumPhases;
	s32 m_iNumTrafficLightLocations;
	float m_fSearchDistance;
	float m_fPhaseOffset;
	//s32 m_padFreeForUse;
	//s32 m_padFreeForUse2;
	
	Vector3 m_vJunctionMin;
	Vector3 m_vJunctionMax;

	// The XYZ position of the central node which defines this junction
	atRangeArray<Vector3,MAX_JUNCTION_NODES_PER_JUNCTION> m_vJunctionNodePositions;

	// The entrances into the junction
	atRangeArray<CEntrance,MAX_ROADS_INTO_JUNCTION> m_Entrances;

	// The phases, which define different light phases
	atRangeArray<CPhaseTiming,MAX_ROADS_INTO_JUNCTION> m_PhaseTimings;

	// Locations of traffic lights; these can be defined in junction editor to help in binding
	// traffic-light props to the correct junctions which they are a part of.
	// If not defined then the traffic-lights code will do its best to grab nearby traffic-light props
	atRangeArray<CTrafficLightLocation,MAX_TRAFFIC_LIGHT_LOCATIONS> m_TrafficLightLocations;

	PAR_SIMPLE_PARSABLE;
};

class CAutoJunctionAdjustment
{
public:
	CAutoJunctionAdjustment() : m_fCycleOffset(0.0f), m_fCycleDuration(0.0f), m_vLocation(V_ZERO) { }

	Vec3V m_vLocation;
	float m_fCycleOffset;
	float m_fCycleDuration;

	PAR_SIMPLE_PARSABLE;
};

class CJunctionTemplateArray
{
public:

	inline CJunctionTemplate & Get(s32 i) { return m_Entries[i]; }
	
	atFixedArray<CJunctionTemplate, JUNCTIONS_MAX_TEMPLATES> m_Entries;

	atFixedArray<CAutoJunctionAdjustment, 8> m_AutoJunctionAdjustments;

	PAR_SIMPLE_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////
// CJunction
// Holds the information required for one junction. These are created and destroyed
// as needed (as cars enter them)

class CJunction
{
	//friend class CJunctions;
public:

	// Used when first constructing junctions
	struct sEntryNodeInfo
	{
		Vector3			vNodePos;
		Vector2			EntryDir;	// NB: debug only?
		CNodeAddress	EntryNode;
		float			orientation;
		s32			lanesToJunction;
		s32			lanesFromJunction;
		s32			leftLaneFilterPhase;
		s32			canTurnRightOnRedLight;
		s32			leftLaneIsAheadOnly;
		s32			rightLaneIsRightOnly;
		s32			phase;
	};

	struct CLightTiming
	{
		//float m_fStartTime;
		float m_fDuration;
	};

	enum
	{
		JUNCTION_MAX_VEHICLES = MAX_VEHICLES_AT_JUNCTION
	};
	enum
	{
		JUNCTION_MAX_LIGHT_PHASES = 8
	};
	enum
	{
		JUNCTION_MAX_TRAFFICLIGHTS = 12
	};
	enum
	{
		JUNCTION_MAX_RAILWAY_BARRIERS = 8
	};
	enum
	{
		JUNCTION_MAX_RAILWAY_TRACKS = 4
	};

	void		Clear();
	s32			GetNumberOfCars();

	// PURPOSE:	True if this junction currently has any vehicles in it (not taking into
	//			account that some elements might be temporarily empty).
	bool		MayHaveVehicles() const { return m_HighestVehicleArrayElementUsed >= 0; }

	void		Update(float timeStep);
	ETrafficLightCommand GetTrafficLightCommand(const CVehicle * pVehicle, const int iEntrance, bool& bGoingRightOnRedOut, const bool bAllowGoIfPastLine=true, const CVehicleNodeList* pNodeList = NULL, const bool bAllowReturnAmberLightCycle=true) const;
	bool		VehicleRouteClashesWithOtherCarOnJunction(const s32 vehicleIndex, const CNodeAddress *pEntryNodes, const CNodeAddress *pExitNodes, const float * fDistances, const bool bGoingRightOnRed, s32* iEntryLanes, s32* iExitLanes, const s32* iEntrances, const bool* bRedLights);
	bool		FindEntryAndExitNodes(const CVehicle* const pVeh, CNodeAddress &PrevNode, CNodeAddress &NextNode, s32& iPrevLane, s32& iNextLane) const;
	bool		FindEntryAndExitDirs(const CVehicle* const pVeh, Vector2& vEntryDir, Vector2& vExitDir) const;
	s32			FindEntranceIndexWithNode(CNodeAddress entryNode) const;
	bool		CalculateDistanceToJunction(const CVehicle* const pVeh, s32 * iEntranceIndex, float &dist);
	float		GetTimeToJunction(const CVehicle* const pVeh, const float fDist) const;
	void		RemoveVehicleFromSlot(s32 slot);
	void		RemoveVehicle(CVehicle *pVeh);
	void		AddVehicle(CVehicle *pVeh);
	bool		IsVehicleAddedToJunction(const CVehicle *pVeh) const;
	void		TidyJunction();
	int			AddTrafficLight(CEntity *pTL);
	void		SetTrafficLight(int idx, CEntity *pTL);
	int			FindTrafficLightEntranceIds(const Vector3 &direction, bool isSingleLight, atRangeArray<int,4> &entranceIds);
	int			ConnectLightToJunction77(const Vector3 &vDir,atRangeArray<int,4> &entranceIds);
	int			ConnectLightToJunction109(const Vector3 &vDir, bool isSingleLight, atRangeArray<int,4> &entranceIds);
	void		RemoveTrafficLights();
	void		RemoveTrafficLight(CEntity *pTL);
	bool		Deploy(CNodeAddress node, bool bIsBuildPaths=false, CPathFind& pathData = ThePaths);
	s32			GetTemplateIndex() { return m_iTemplateIndex; }
	void		SetTemplateIndex(s32 i) { m_iTemplateIndex = i; }
	s32			GetNumEntrances() const { return m_iNumEntrances; }

	u32			GetNumActivelyCrossingPeds() const { return m_uNumActivelyCrossingPeds; }
	void		RegisterActivelyCrossingPed() { m_uNumActivelyCrossingPeds++; }
	void		RemoveActivelyCrossingPed();
	u32			GetNumWaitingForLightPed() const { return m_uNumWaitingForLightPeds; }
	void		RegisterWaitingForLightPed() { m_uNumWaitingForLightPeds++; }
	void		RemoveWaitingForLightPed();

	bool		ShouldCarsStopForTrain() const;

	inline CVehicle *	GetVehicle(s32 i)
	{
		Assert(i >= 0 && i < JUNCTION_MAX_VEHICLES);
		return m_pVehicles[i];
	}
	inline const Vector3 & GetJunctionCenter() const { return m_vJunctionCenter; }
	inline const Vector3 & GetJunctionMin() const { return m_vJunctionMin;}
	inline const Vector3 & GetJunctionMax() const { return m_vJunctionMax;}
	inline void SetJunctionCenter(const Vector3 & vCenter) { m_vJunctionCenter = vCenter; }

	inline void SetNumEntrances(const s32 i)
	{
		Assert(i>0 && i<MAX_ROADS_INTO_JUNCTION);
		m_iNumEntrances = i;
	}
	inline const CJunctionEntrance & GetEntrance(s32 i) const
	{
		Assertf(i >= 0 && i < m_iNumEntrances,"Requesting entrance %d out of %d",i,m_iNumEntrances);
		return m_Entrances[i];
	}

	inline CJunctionEntrance & GetEntrance(s32 i)
	{
		Assertf(i >= 0 && i < m_iNumEntrances,"Requesting entrance %d out of %d",i,m_iNumEntrances);
		return m_Entrances[i];
	}

	inline bool ContainsJunctionNode(const CNodeAddress & node) const 
	{
		for(s32 n=0; n<m_iNumJunctionNodes; n++)
		{
			if(m_JunctionNodes[n] == node)
				return true;
		}
		return false;
	}

	inline bool ContainsEntranceNode(const CNodeAddress & node) const 
	{
		if (node.IsEmpty())
		{
			return false;
		}

		for (s32 n=0; n<m_iNumEntrances; n++)
		{
			if (m_Entrances[n].m_Node == node )
			{
				return true;
			}
		}
		return false;
	}

	inline s32 GetNumJunctionNodes() const { return m_iNumJunctionNodes; }
	inline CNodeAddress GetJunctionNode(const s32 i) const 
	{
		Assert(i>=0 && i<m_iNumJunctionNodes);
		return m_JunctionNodes[i];
	}

	inline void SetNumJunctionNodes(const s32 n)
	{
		Assert(n > 0 && n < MAX_JUNCTION_NODES_PER_JUNCTION);
		m_iNumJunctionNodes = n;
	}
	inline void SetJunctionNode(s32 i, CNodeAddress iNodeAddr)
	{
		m_JunctionNodes[i] = iNodeAddr;
	}
	void SetNumLightPhases(s32 i) { m_iNumLightPhases = i; }
	s32 GetNumLightPhases() const { return m_iNumLightPhases; }
	s32 GetLightPhase() const { return m_iLightPhase; }
	float GetLightTimeRemaining() const { return m_fLightTimeRemaining; }
	void SetLightTimeRemaining(const float f) { m_fLightTimeRemaining = f; }
	void SetLightPhase(int i) { m_iLightPhase = i; }
	float GetLightPhaseDuration(int i) const { return m_LightTiming[i].m_fDuration; }

	s32 GetAutoJunctionCycleOffset() const { return m_iCycleOffsetMs; }
	float GetAutoJunctionCycleScale() const { return m_fCycleScale; }

#if __BANK
	float GetLightPhaseDurationMultiplier() { return m_fLightPhaseDurationMultiplier; }
#endif
	u32 GetNumLightPhaseVehicles() { return m_iNumLightPhaseVehicles; }

	s32	GetLightStatusForPedCrossing(bool bConsiderTimeRemaining) const;

	void SetToMalfunction(bool b);
	inline bool GetIsMalfunctioning() const { return m_bMalfunctioning; }

	inline void SetRequiredByScript(scrThreadId iScriptThreadId)
	{
		Assertf(!iScriptThreadId, "Junction already required by script.");
		m_iRequiredByScriptId = iScriptThreadId;
	}
	inline scrThreadId GetRequiredByScript() const { return m_iRequiredByScriptId; }

	inline void SetErrorBindingJunction(const bool b) { m_bErrorBindingJunction = b; }
	inline bool GetErrorBindingJunction() const { return m_bErrorBindingJunction; }

	inline u32 GetLastUsedTime() const { return m_iLRUTime; }

	float GetTrafficLightSearchDistance();
	bool HasTrafficLightLocations() { return GetNumTrafficLightLocations() > 0; }
	s32 GetNumTrafficLightLocations();
	void GetTrafficLightLocation(s32 iIndex, Vector3 & vPosition);
	bool HasTrafficLightNodes() { return m_bHasTrafficLightNodes; }
	bool HasGiveWayNodes() { return m_bHasGiveWayNodes; }
	bool HasFlowControlNodes() { return HasTrafficLightNodes() || HasGiveWayNodes(); }
	s32 GetVirtualJunctionIndex() const {return m_iVirtualJunctionIndex;}

	void ScanForPedCrossing(const CPathFind& pathData = ThePaths);
	
	inline void SetIsRailwayCrossing(const bool b) { m_bIsRailwayCrossing = b; }
	inline bool GetIsRailwayCrossing() const { return m_bIsRailwayCrossing; }
	inline void SetCanSkipPedPhase(const bool b) { m_bCanSkipPedPhase = b;}
	inline bool GetCanSkipPedPhase() const { return m_bCanSkipPedPhase; }
	inline CTrainTrackNode * GetRailwayTrainTrackNode(s32 i) { return m_pTrainTrackNodes[i]; }
	inline CObject * GetRailwayBarrier(const s32 i) { return m_RailwayBarriers[i]; }
	inline bool GetRailwayBarriersShouldBeDown() const { return m_bRailwayBarriersShouldBeDown; }
	inline bool GetRailwayBarriersAreFullyRaised() const { return m_bRailwayBarriersAreFullyRaised; }
	inline ERailwayCrossingLightState GetRailwayCrossingLightState() const { return m_RailwayCrossingLightState; }

	inline bool GetHasPedCrossingPhase() const { return m_HasPedCrossingPhase; }
	inline bool IsOnlyJunctionBecauseHasSwitchedOffEntrances() const {return m_bIsOnlyJunctionBecauseHasSwitchedOffEntrances;}

	bool IsPosWithinBounds(const Vector3& vPos, const float fZTolerance) const;
	bool IsPosWithinBoundsZOnly(const Vector3& vPos, const float fZTolerance) const;
	
	static bank_float ms_fMaxDistanceFromTrainTrack;
	static bank_float ms_fScanForRailwayBarriersRange;
	static bank_s32 ms_iScanForRailwayBarriersFreqMs;
	static bank_s32 ms_iScanForApproachingTrainsFreqMs;
	static bank_float ms_fDurationRatioForSafeCrossing;
	static bank_u32 ms_uRailwayLightPulseDuration;

	static bank_float ms_LightPhaseDurationMultiplierEmpty;
	static bank_float ms_LightPhaseDurationMultiplierLow;
	static bank_float ms_LightPhaseDurationMultiplierHigh;
	static bank_float ms_LightPhaseDurationMultiplierExtreme;
	static bank_u32 ms_LightPhaseDurationMultiplierLowThreshold;
	static bank_u32 ms_LightPhaseDurationMultiplierHighThreshold;
	static bank_u32 ms_LightPhaseDurationMultiplierExtremeThreshold;

	static bank_float ms_fMaxTimeExtensionForCrossingPeds;
	
private:

	void		ScanForRailwayBarriers();
	void		UpdateRailwayCrossing(float timeStep);
	void		UpdateRailwayCrossingLightPulse();
	void		ScanForRailwayNodes();
	void		ScanForApproachingTrains();
	static bool IdentifyRailwayBarrierCB(CEntity * pEntity, void * pData);
	bool		AddRailwayBarrier(CObject * pBarrier);
	bool		LowerRailwayBarrier(CDoor * pBarrier);
	bool		RaiseRailwayBarrier(CDoor * pBarrier);
	bool		CanSkipPedPhase();

	float		HelperCalcDistanceToEntrance(const Vector3& vPos, s32 iEntranceIndex);

	// Entrances/Exits to this junction
	atRangeArray<CJunctionEntrance,MAX_ROADS_INTO_JUNCTION>	m_Entrances;
	// All the vehicles currently using this junction
	atRangeArray<RegdVeh,JUNCTION_MAX_VEHICLES>				m_pVehicles;
	// The timing of traffic lights
	atRangeArray<CLightTiming,JUNCTION_MAX_LIGHT_PHASES>	m_LightTiming;
	// The traffic lights
	atRangeArray<RegdEnt,JUNCTION_MAX_TRAFFICLIGHTS>		m_TrafficLights;

	// Pointer to closest node on any train tracks which is passing through this junction
	CTrainTrackNode * m_pTrainTrackNodes[JUNCTION_MAX_RAILWAY_TRACKS];

	// The center node identifies the junction.
	Vector3			m_vJunctionCenter;
	Vector3			m_vJunctionMin;
	Vector3			m_vJunctionMax;
	s32				m_iVirtualJunctionIndex;
	s32				m_iNumJunctionNodes;
	atRangeArray<CNodeAddress,MAX_JUNCTION_NODES_PER_JUNCTION>	m_JunctionNodes;
	s32				m_iNumEntrances;
	s32				m_HighestVehicleArrayElementUsed;

	s32				m_iNumLightPhases;
	s32				m_iLightPhase;
	float			m_fLightTimeRemaining;

	s32				m_iCycleOffsetMs; // Offset in integer milliseconds
	float			m_fCycleScale;

#if __BANK
	float			m_fLightPhaseDurationMultiplier;
#endif
	u32				m_iNumLightPhaseVehicles;

	u32				m_iLRUTime;
	s32				m_iTemplateIndex;
	u32				m_iLastBarrierScanTime;
	u32				m_iLastApproachingTrainScanTime;
	u32				m_uLastRailwayLightPulseTime;
	
	ERailwayCrossingLightState	m_RailwayCrossingLightState;

	u32				m_uNumActivelyCrossingPeds;
	u32				m_uNumWaitingForLightPeds;

	//Used to delay the opening/closing of railway barriers
	float			m_RailwayTimeSinceSignal; 

	// If this junction was instantiated by script, this records the script ID for mission cleanup purposes
	scrThreadId		m_iRequiredByScriptId;

	// Railway barriers which lower/raise with passing trains
	RegdObj m_RailwayBarriers[JUNCTION_MAX_RAILWAY_BARRIERS];

	bool			m_bMalfunctioning;
	bool			m_bErrorBindingJunction;
	bool			m_bHasTrafficLightNodes;
	bool			m_bHasGiveWayNodes;
	bool			m_bIsRailwayCrossing;
	bool			m_bCanSkipPedPhase;
	bool			m_bRailwayBarriersShouldBeDown;
	bool			m_bRailwayBarriersAreFullyRaised;
	bool			m_HasPedCrossingPhase;
	bool			m_bExtendedTimeForCrossingPeds;
	bool			m_bIsOnlyJunctionBecauseHasSwitchedOffEntrances;

	//used to determine if railway crossings are open or closed
	static const float ms_fRailCrossingCloseRatio;
	static const float ms_fRailCrossingOpenRatio;

};

/////////////////////////////////////////////////////////////////////////////////
// CJunctions
// Manages the active junctions in the games.
// Essentially a little array of them.

class CJunctions
{
	friend class CJunction;
public:

	static	void	Init(unsigned initMode);
	static	void	Shutdown(unsigned shutdownMode);

	static	void	Update();
	static	void	CarIsApproachingJunction(CNodeAddress);
	static	void	RemoveVehicleFromJunction(CVehicle *pVeh, CNodeAddress nodeAddress);
	static	CJunction* AddVehicleToJunction(CVehicle *pVeh, CNodeAddress nodeAddress, CJunction* pJunctionIfKnown);

	static	s32		GetLightStatusForPed(const Vector3 & vJunctionSearchPosition, const Vector3 & vCrossingDir, bool bConsiderTimeRemaining, int& cachedJunctionIndex);
	static	bool	IsPedNearToJunction(const Vector3 & vPedPos);
	static	CJunction * FindJunctionFromNode(const CNodeAddress & junctionNode);

	static	bool	LoadJunctionTemplates();

	static	bool	ScanJunctionNodeFn(CPathNode * pNode, void * pData);
	static  void	ScanAndInstanceJunctionsNearOrigin(const Vector3 & vOrigin);

#if __JUNCTION_EDITOR
	static	void	RefreshAllJunctions();
	static	void	EditorSaveJunctionTemplates();
	static	void	EditorLoadJunctionTemplates();
	static	void	EditorSaveJunctionTemplatesXml();
	static	void	EditorLoadJunctionTemplatesXml();
	static	char	ms_JunctionEditorXmlFilename[RAGE_MAX_PATH];
#endif
	static	s32	BindJunctionTemplate(CJunction & junction, const CNodeAddress & junctionNode, CJunction::sEntryNodeInfo * entryInfos, const s32 iMaxEntryInfos, const CPathFind& pathData = ThePaths);

#if __BANK
	static void		InitWidgets();
	static void		RenderDebug();
	static void		RemoveAllJunctions();
	static CJunction * Debug_GetClosestJunction();
	static void		Debug_AdvanceClosestLights();
	static void		Debug_MalfunctionClosest();

	// A 2nd debug rendering function, to help sync up junctions & traffic-light rendering
	static void		DebugRenderLights();

	static bool		m_bDebug;
	static bool		m_bDebugWaitForTraffic;
	static bool		m_bDebugText;

	// PURPOSE:	True if debug drawing of timesliced junctions is enabled.
	static bool		m_bDebugTimeslicing;

	static bool		m_bDebugLights;
	static bool		m_bDisableProcessing;
#endif

	static s32 GetNumJunctionTemplates();
	static s32 GetMaxNumJunctionTemplates();
	static CJunctionTemplate & GetJunctionTemplate(s32 i) { return m_JunctionTemplates->m_Entries[i]; }

	static s32 GetNumAutoJunctionAdjustments() { return m_JunctionTemplates->m_AutoJunctionAdjustments.GetCount(); }
	static CAutoJunctionAdjustment* FindAutoJunctionAdjustmentData(Vec3V_In vPos);
	static void FindAutoJunctionAdjustments(Vec3V_In vPos, bool hasPedPhase, s32& outOffset, float& outScale);

	static s32 GetJunctionAtPosition(const Vector3 & vPos, float maxDistSqr = 50.0f*50.0f, bool bIgnoreFakeJunctions=false );
	static s32 GetJunctionAtPositionUsingJunctionMinMax(const Vector3& vPos, float fMaxDistSqr, bool bIgnoreFakeJunctions);
	static s32 GetJunctionAtPositionForTrafficLight(const Vector3 & vPos, const Vector3 & vDir, bool isSingleLight);
	static s32 GetJunctionAtPositionForRailwayCrossing(const Vector3 & vPos);
	
	static CJunction *	GetJunctionByIndex(s32 i);

#if __JUNCTION_EDITOR
	static CJunctionTemplate * GetJunctionTemplateAtPosition(const Vector3 & vPos);
	static s32 GetJunctionUsingTemplate(const s32 iTemplate);

	static CAutoJunctionAdjustment& FindOrCreateAutoJunctionAdjustment(CJunction& junction);
	static bool DeleteAutoJunctionAdjustment(CJunction& junction);
#endif

#if __BANK
	static CJunctionTemplate * GetJunctionTemplateContainingEntrance(const Vector3 & vNodePos);
#endif

	static bool CreateTemplatedJunctionForScript(scrThreadId iScriptThreadId, const Vector3 & vJunctionPos);
	static void	OnScriptTerminate(scrThreadId iScriptThreadId);

	static int	CountNumJunctionsInUse();


	// Distance from camera to automatically instance junctions (or zero to disable/use previous behaviour)
	// Previous behaviour was to only instanciate a junction where there was traffic in the vicinity -
	// but since traffic lights are now dependent upon each junction, we need to create them whenever
	// the camera is close by.
	static bank_float ms_fInstanceJunctionDist;
	static bank_float ms_fScanFrequency;

	static const float ms_fJunctionNodePosEps;
	static const float ms_fEntranceNodePosEps;

protected:

	static atRangeArray<CJunction,JUNCTIONS_MAX_NUMBER> m_aJunctions;
	static CJunctionTemplateArray * m_JunctionTemplates;
	static float m_fTimeSinceLastScan;
	static bank_bool ms_bInstanceJunctionsAroundPlayer;
	static const float ms_fScriptCommandJunctionProximitySqr;

	// PURPOSE:	Parallel array to the junction array, accumulating elapsed time since
	//			each one last updated.
	static atRangeArray<float, JUNCTIONS_MAX_NUMBER> m_TimesliceTimeSteps;

	// PURPOSE:	Parallel array to the junction array, keeping track of how many frames
	//			have elapsed since the last update.
	static atRangeArray<u8, JUNCTIONS_MAX_NUMBER> m_TimesliceFramesSinceUpdate;

	// PURPOSE:	The index of the next junction we will do timesliced updates for.
	static int m_TimesliceUpdateIndex;

	// PURPOSE:	The maximum update period for timesliced junctions, in frames.
	static int m_TimesliceUpdatePeriod;

	// PURPOSE:	Estimate of how many junctions are active and have vehicles in them.
	static int m_TimesliceJunctionsInUse;

	// PURPOSE:	Used as a counter for updating m_TimesliceJunctionsInUse.
	static int m_TimesliceJunctionsInUseCounting;

	// PURPOSE:	True if junctions should be allowed to do timesliced updates.
	static bool m_TimesliceEnabled;

	// PURPOSE: Timestep used online to ensure junctions are synced
	static unsigned m_LastNetworkUpdateTime;
};

//------------------------------------------------------------------
// Callback functions used by CPathFind to filter pathnodes search

extern bool PathfindFindJunctionNodeCB(CPathNode * pNode, void * pData);
extern bool PathfindFindNonJunctionNodeCB(CPathNode * pNode, void * pData);

#endif
