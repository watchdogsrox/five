/////////////////////////////////////////////////////////////////////////////////
// FILE :    train.h
// PURPOSE : Those big metal cars on fixed paths that everybody loves.
// AUTHOR :  Obbe, Adam Croston
// CREATED : 21-8-00
/////////////////////////////////////////////////////////////////////////////////
#ifndef _TRAIN_H_
#define _TRAIN_H_

#include "grcore/debugdraw.h"
#include "Network\Cloud\CloudManager.h"
#include "network\objects\entities\netObjTrain.h"
#include "renderer\hierarchyIds.h"
#include "scene\RegdRefTypes.h"
#include "timecycle\TimeCycleConfig.h"
#include "vehicles\door.h"
#include "vehicles\vehicle.h"
#include "vehicles\automobile.h"

class CCarDoor;


#define MAX_TRAIN_DOORS				(6)
#define MAX_ON_DISK_TRAIN_TRACKS_PER_LEVEL	(12)
#define MAX_DLC_TRAIN_TRACKS_PER_LEVEL	(15)
#define MAX_ON_DISK_TRAIN_CONFIGS (15)
#define MAX_DLC_TRAIN_CONFIGS (15)
#define MAX_TRAIN_TRACKS_PER_LEVEL	MAX_ON_DISK_TRAIN_TRACKS_PER_LEVEL+MAX_DLC_TRAIN_TRACKS_PER_LEVEL

// This structure contains the required information for a single node
// on a train track.
class CTrainTrackNode
{
public:
	enum eStationType
	{
		ST_NONE,		// No station
		ST_LEFT,		// Station with platform on left side of track
		ST_RIGHT		// Station with platform on right side of track
	};

	enum 
	{
		TUNNEL_MASK = (1<<2)
	};

	CTrainTrackNode() : 
	m_nStation(ST_NONE),
	m_isTunnel(false)
	{ }

	void	SetCoorsX(float Coor) { m_fCoorX = Coor; }
	void	SetCoorsY(float Coor) { m_fCoorY = Coor; }
	void	SetCoorsZ(float Coor) { m_fCoorZ = Coor; }	// Packed into 14 bits
	void	SetCoors(const Vector3 &vCoor) { m_fCoorX = vCoor.x; m_fCoorY = vCoor.y; m_fCoorZ = vCoor.z; }

	float	GetCoorsX() const { return m_fCoorX; }
	float	GetCoorsY() const { return m_fCoorY; }
	float	GetCoorsZ() const { return m_fCoorZ; }
	
	Vector3 GetCoors() const { return Vector3(m_fCoorX, m_fCoorY, m_fCoorZ); }
	Vector2 GetCoorsXY() const { return Vector2(m_fCoorX, m_fCoorY); }
	Vec3V GetCoorsV() const { return Vec3V(m_fCoorX, m_fCoorY, m_fCoorZ); }
	
	// JB: Why are these functions multiplying/dividing by 3??
	void	SetLengthFromStart(const float fLength) { m_fLengthFromStart = fLength; }
	float	GetLengthFromStart() const { return m_fLengthFromStart; }

	void	SetStationType(eStationType nType)	{ m_nStation = nType; }
	eStationType GetStationType() const			{ return m_nStation; }

	void	SetIsTunnel(bool bIsTunnel)			{ m_isTunnel = bIsTunnel; }
	bool    GetIsTunnel() const					{ return(m_isTunnel); }

protected:

	float	m_fCoorX, m_fCoorY, m_fCoorZ;
	float	m_fLengthFromStart;			// In meters

	u8				m_isTunnel : 1;

public:
	eStationType	m_nStation : 2;					// 0 = no station. 1 = station with platform on left side of track. 2 = station with platform on right side of track.

};

#if !__FINAL

class CTrainCachedNode
{
public:
	CTrainTrackNode * pNode;
	s32 iTrack;
	s32 iNode;	
};
#endif

typedef bool (*ForAllTrackNodesCB) (s32 iTrackIndex, s32 iNodeIndex, void * data);

#define MAX_NUM_STATIONS (20) // The maximum number of stations we can have for a particular track.

class CTrainTrack
{
public:

	CTrainTrack();

	enum PlatformSide
	{
		None, Left, Right, Both
	};

	u32					m_trainConfigGroupNameHash;					// Is this track a cable-car?
	bool				m_isEnabled;
	bool				m_isLooped;
	bool				m_stopAtStations;
	bool				m_MPstopAtStations;
	u32					m_maxSpeed;
	u32					m_breakDist;

	s32	 				m_numNodes;									// The number of nodes on this track
	CTrainTrackNode*	m_pNodes;									// Pointer to the actual track nodes
	float				m_totalLengthIfLooped;						// In meters  (total length linear is just GetLengthFromStart() of final node).

	int					m_iLinkedTrackIndex;

	bool				m_bInitiallySwitchedOff;					// Tracks default as off, and have to be enabled by script
	scrThreadId			m_iTurnedOffByScriptThreadID;				// If non-zero then this track has been disabled by the specified script

#if __BANK
	char				m_TurnedOffByScriptName[64];
#endif

	u32					m_iLastTrainSpawnTimeMS;					// The last time that a train was spawned on this track
	s32					m_iScriptTrainSpawnFreqMS;					// Lower bound frequency for trains to be spawned on this track, or -1 if none
	scrThreadId			m_iSpawnFreqThreadID;						// ID of the thread which modified this track's spawn frequency

	u32					m_iLastTrainReportingTimeMS;				// The last time this train reported in

	bool				m_bTrainActive;
	bool				m_bTrainProcessNetworkCreation;
	bool				m_bTrainNetworkCreationApproval;

	s32					m_numStations;								// The number of stations for each track type.
	Vector3				m_aStationCoors[MAX_NUM_STATIONS];			// The coordinates of the stations
	s32					m_aStationNodes[MAX_NUM_STATIONS];			// The node Id for each station, so that I can search quickly
	float				m_aStationDistFromStart[MAX_NUM_STATIONS];	// How far along the tracks are the stations located.
	s8					m_aStationSide[MAX_NUM_STATIONS];			// Which side the platform is on  (see the PlatformSide enum)

	inline CTrainTrackNode * GetNode(const s32 iNode)
	{
		Assert(iNode >= 0 && iNode < m_numNodes);
		return &m_pNodes[iNode];
	}

	inline bool IsSwitchedOff() { return (m_bInitiallySwitchedOff || (m_iTurnedOffByScriptThreadID != THREAD_INVALID)); }

	float GetTotalTrackLength()
	{
		if(m_isLooped)
		{
			const float totalTrackLengthIfLooped = m_totalLengthIfLooped;

			return totalTrackLengthIfLooped;
		}
		else
		{
			const float totalTrackLengthLinear = m_pNodes[m_numNodes - 1].GetLengthFromStart();
			return totalTrackLengthLinear;
		}
	}

	static int GetTrackIndex(CTrainTrack * pTrack);

#if __DEV
	char*				m_trainConfigGroupName;
#endif
};
extern CTrainTrack gTrainTracks[MAX_TRAIN_TRACKS_PER_LEVEL];

// for audio
namespace rage
{
	struct TrainStation;
}

class CTrainCloudListener : public CloudListener
{
public:
	virtual void OnCloudEvent(const CloudEvent* pEvent);
};

class CTrain : public CVehicle
{

public:

	enum TrainState
	{
		TS_Moving,
		TS_ArrivingAtStation,
		TS_OpeningDoors,
		TS_WaitingAtStation,
		TS_ClosingDoors,
		TS_LeavingStation,
		TS_Destroyed
	};
	
	enum PassengersState
	{
		PS_None,
		PS_GetOff,
		PS_WaitingForGetOff,
		PS_GetOn,
		PS_WaitingForGetOn,
	};

	enum SideFlags
	{
		SF_Left		= BIT0,
		SF_Right	= BIT1,
	};

	enum ReleaseTrainFlags
	{
		RTF_KeepOldSpeed	= BIT0
	};

public:
	CTrain*			GetEngine() { return m_pEngine; }
	const CTrain*	GetEngine() const { return m_pEngine; }
	void			SetEngine(CTrain* engine, bool bValidate = true)
	{
#if ENABLE_NETWORK_LOGGING
		if(NetworkInterface::IsGameInProgress())
		{
			if(engine)
			{
				if(GetNetworkObject())
					NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "TRAIN_SET_ENGINE", "%s", GetLogName());
				else
					NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "TRAIN_SET_ENGINE", "0x%p", this);

				if(m_pEngine)
				{
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Previous Engine", "0x%p", m_pEngine.Get());
				}
				NetworkInterface::GetObjectManagerLog().WriteDataValue("New Engine", "0x%p", engine);
			}
			else
			{
				if(GetNetworkObject())
					NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "TRAIN_REMOVE_ENGINE", "%s", GetLogName());
				else
					NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "TRAIN_REMOVE_ENGINE", "0x%p", this);

				if(m_pEngine)
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Previous Engine", "0x%p", m_pEngine.Get());
			}
		}
#endif // ENABLE_NETWORK_LOGGING
		m_pEngine = engine;
		if (bValidate) 
			ValidateLinkedLists( m_pEngine );
	}

	CTrain*	GetLinkedToBackward() const 
	{
		if(m_pLinkedToBackward)
		{
			bool validPtr = !sysThreadType::IsUpdateThread() || CVehicle::GetPool()->IsValidPtr( m_pLinkedToBackward);
			bool inheritsFromTrain = m_pLinkedToBackward->InheritsFromTrain();
			bool linkedForwardIsNotThis = m_pLinkedToBackward->m_pLinkedToForward == this;
			bool notEngine = !m_pLinkedToBackward->IsEngine();
			bool linkedBackIsNotThis = m_pLinkedToBackward != this;

#if ENABLE_NETWORK_LOGGING
			if(NetworkInterface::IsGameInProgress())
			{
				NETWORK_QUITF(validPtr, "Corrupted train car detected %s(0x%p), the train car linked to forward is %s(0x%p)", m_pLinkedToBackward->GetDebugName(), m_pLinkedToBackward.Get(), GetDebugName(), this);
				NETWORK_QUITF(inheritsFromTrain, "m_pLinkedToBackward does not inherit from train! Corrupted train car detected %s(0x%p), the train car linked to forward is %s(0x%p)", m_pLinkedToBackward->GetDebugName(), m_pLinkedToBackward.Get(), GetDebugName(), this);
				NETWORK_QUITF(linkedForwardIsNotThis, "m_pLinkedToBackward's linked to forward is not 'this'! Corrupted train car detected %s(0x%p), the train car linked to forward is %s(0x%p)", m_pLinkedToBackward->GetDebugName(), m_pLinkedToBackward.Get(), GetDebugName(), this);
				NETWORK_QUITF(notEngine, "m_pLinkedToBackward is an engine! Corrupted train car detected %s(0x%p), the train car linked to forward is %s(0x%p)", m_pLinkedToBackward->GetDebugName(), m_pLinkedToBackward.Get(), GetDebugName(), this);
				NETWORK_QUITF(linkedBackIsNotThis, "m_pLinkedToBackward is 'this'! Corrupted train car detected %s(0x%p), the train car linked to forward is %s(0x%p)", m_pLinkedToBackward->GetDebugName(), m_pLinkedToBackward.Get(), GetDebugName(), this);
			}
#endif // ENABLE_NETWORK_LOGGING

			if(validPtr && inheritsFromTrain && linkedForwardIsNotThis && notEngine && linkedBackIsNotThis)
			{
				return m_pLinkedToBackward;
			}
		}

		return NULL;
	}
	
	CTrain*	GetLinkedToForward() const 
	{ 	
		if(m_pLinkedToForward)
		{
			bool validPtr = !sysThreadType::IsUpdateThread() || CVehicle::GetPool()->IsValidPtr( m_pLinkedToForward );
			bool inheritsFromTrain = m_pLinkedToForward->InheritsFromTrain();
			bool linkedBackwardIsNotThis = m_pLinkedToForward->m_pLinkedToBackward == this;
			bool notEngine = !IsEngine();
			bool linkedForwardIsNotThis = m_pLinkedToForward != this;

#if ENABLE_NETWORK_LOGGING
			if(NetworkInterface::IsGameInProgress())
			{
				NETWORK_QUITF(validPtr, "Corrupted train car detected %s(0x%p), the train car linked to backward is %s(0x%p)", m_pLinkedToForward->GetDebugName(), m_pLinkedToForward.Get(), GetDebugName(), this);
				NETWORK_QUITF(inheritsFromTrain, "m_pLinkedToForward does not inherit from train! Corrupted train car detected %s(0x%p), the train car linked to backward is %s(0x%p)", m_pLinkedToForward->GetDebugName(), m_pLinkedToForward.Get(), GetDebugName(), this);
				NETWORK_QUITF(linkedBackwardIsNotThis, "m_pLinkedToForward's linked to forward is not 'this'! Corrupted train car detected %s(0x%p), the train car linked to backward is %s(0x%p)", m_pLinkedToForward->GetDebugName(), m_pLinkedToForward.Get(), GetDebugName(), this);
				NETWORK_QUITF(notEngine, "m_pLinkedToForward is an engine! Corrupted train car detected %s(0x%p), the train car linked to backward is %s(0x%p)", m_pLinkedToForward->GetDebugName(), m_pLinkedToForward.Get(), GetDebugName(), this);
				NETWORK_QUITF(linkedForwardIsNotThis, "m_pLinkedToForward is 'this'! Corrupted train car detected %s(0x%p), the train car linked to backward is %s(0x%p)", m_pLinkedToForward->GetDebugName(), m_pLinkedToForward.Get(), GetDebugName(), this);
			}
#endif // ENABLE_NETWORK_LOGGING

			if(validPtr && inheritsFromTrain && linkedBackwardIsNotThis && notEngine && linkedForwardIsNotThis)
			{
				return m_pLinkedToForward;
			}
		}

		return NULL;
	}

	void SetLinkedToBackward( CTrain* linked, bool bValidate = true)
	{
#if ENABLE_NETWORK_LOGGING
		if(NetworkInterface::IsGameInProgress())
		{
			if(linked)
			{
				if(GetNetworkObject())
					NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "TRAIN_SET_LINK_BACKWARD", "%s", GetLogName());
				else
					NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "TRAIN_SET_LINK_BACKWARD", "0x%p", this);

				if(m_pLinkedToBackward)
				{
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Previous Linked Backward", "0x%p", m_pLinkedToBackward.Get());
				}
				NetworkInterface::GetObjectManagerLog().WriteDataValue("New Linked Backward", "0x%p", linked);
			}
			else
			{
				if(GetNetworkObject())
					NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "TRAIN_REMOVE_LINK_BACKWARD", "%s", GetLogName());
				else
					NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "TRAIN_REMOVE_LINK_BACKWARD", "0x%p", this);

				if(m_pLinkedToBackward)
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Previous Linked Backward", "0x%p", m_pLinkedToBackward.Get());
			}
		}
#endif // ENABLE_NETWORK_LOGGING
		m_pLinkedToBackward = linked;
		if (bValidate)
			ValidateLinkedLists( m_pLinkedToBackward );
	}
	
	void SetLinkedToForward( CTrain* linked, bool bValidate = true)
	{
#if ENABLE_NETWORK_LOGGING
		if(NetworkInterface::IsGameInProgress())
		{
			if(linked)
			{
				if(GetNetworkObject())
					NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "TRAIN_SET_LINK_FORWARD", "%s", GetLogName());
				else
					NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "TRAIN_SET_LINK_FORWARD", "0x%p", this);

				if(m_pLinkedToForward)
				{
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Previous Linked Forward", "0x%p", m_pLinkedToForward.Get());
				}
				NetworkInterface::GetObjectManagerLog().WriteDataValue("New Linked Forward", "0x%p", linked);
			}
			else
			{
				if(GetNetworkObject())
					NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "TRAIN_REMOVE_LINK_FORWARD", "%s", GetLogName());
				else
					NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "TRAIN_REMOVE_LINK_FORWARD", "0x%p", this);

				if(m_pLinkedToForward)
				{
					NetworkInterface::GetObjectManagerLog().WriteDataValue("Previous Linked Forward", "0x%p", m_pLinkedToForward.Get());
				}
			}
		}
#endif // ENABLE_NETWORK_LOGGING
		m_pLinkedToForward = linked;
		if (bValidate)
			ValidateLinkedLists( m_pLinkedToForward );
	}

public:

	bool CanGetOff() const;
	bool CanGetOn() const;
	bool CanGetOnOrOff() const;
	bool IsMoving() const;

	bool AreDoorsMoving() const;

	void RemoveAllOccupants();

	bool SetUpDriverOnEngine(bool bUseExistingNodes);

public:	// Static members
	static void		Init(unsigned initMode);
	static void		Shutdown(unsigned shutdownMode);

	static void		SetTunablesValues();
	static void		UpdateVisualDataSettings(const CVisualSettings& visualSettings);

#if __BANK
	static void		InitWidgets(bkBank& bank);
#endif
	static s32		GetTrainConfigIndex(u32 nameHash);
	static void		LoadTrainConfigs(const char *pFileName);
	static void		TrainConfigsPostLoad();
	static void		LoadTrackXml(const char *pXmlFilename);
	static void		TrainTracksPostLoad();

	static void		ProcessTrainNetworkPending(int trackindex);

	static void		DoTrainGenerationAndRemoval();
	static bool		FindTrackGenerationPos(const int iTrackIndex, const Vector3 & vOrigin, const float fMinCreationDist, const float fMaxCreationDist, Vector3 & vOut_CreationPos, int & iOut_PrevNodeIndex, float & fOut_CreationTVal);
	static void		RenderTrainDebug();

	static void		SetTrackActive(int trackindex, bool trainactive);
	static bool		GetTrackActive(int trackindex);
	static bool		DetermineHostApproval(int trackindex);
	static void		ReceivedHostApprovalData(int trackindex, bool trainapproval);

	// The functions the level designers use to control trains.
	static void		DisableRandomTrains(bool bDisable);
	static void		CreateTrain(s8 trackIndex, s32 trackNodeForEngine, bool bDirection, u8 trainConfigIndex, CTrain** ppEngine_out = NULL, CTrain** ppLastCarriage_out = NULL, scrThreadId iMissionScriptId = THREAD_INVALID);
	static void		RemoveMissionTrains(scrThreadId iCurrentThreadId);
	static void		ReleaseMissionTrains(scrThreadId iCurrentThreadId, const u32 iReleaseTrainFlags=0);
	static void		ReleaseOneMissionTrain(GtaThread& iCurrentThread, CTrain* pEngine, const u32 iReleaseTrainFlags=0);
	static void		RemoveOneMissionTrain(GtaThread& iCurrentThreadId, CTrain* pEngine);
	static void		RemoveTrainsFromArea(const Vector3 &Centre, float Radius);
	static void		RemoveAllTrains();	

	static void		RestoreScriptModificationsToTracks(const scrThreadId iThreadId); // called from mission cleanup
	static void		SwitchTrainTrackFromScript(const s32 iTrackIndex, const bool bSwitchOn, const scrThreadId iThreadId);
	static void		SetTrainTrackSpawnFrequencyFromScript(const s32 iTrackIndex, const s32 iSpawnFreqMS, const scrThreadId iThreadId);

	static void		SetCarriageSpeed(CTrain* pCarriage, float NewSpeed);
	static float	GetCarriageSpeed(CTrain* pCarriage);
	static void		SetCarriageCruiseSpeed(CTrain* pCarriage, float NewSpeed);
	static float	GetCarriageCruiseSpeed(CTrain* pCarriage);
	static CTrain*	FindEngine(const CTrain* pCarriage);
	static CTrain*	FindCaboose(const CTrain* pCarriage);
	static CTrain*	FindCarriage(const CTrain* pEngine, u8 CarriageNumber);
	static s32		FindClosestNodeOnTrack(const Vector3 &pos, s32 trackIndex, const float * fIn_ThisHeading=NULL, float * fOut_Distance=NULL, float * fOut_Heading=NULL, Vector3 * vOut_Position=NULL);
    static s32		FindClosestNodeOnTrack(const Vector3 &pos, s32 trackIndex, s32 startNode, f32 maxDistance, u32& numNodesSearched);
    static s32		FindClosestTrackNode(const Vector3& pos, s8& trackIndex_out);
	static CTrain*	FindNearestTrainCarriage(const Vector3& Coors, bool bOnlyEngine);
	static void		SetNewTrainPosition(CTrain* pEngine, const Vector3& NewCoors);
	static void		SkipToNextAllowedStation(CTrain* pCarriage);

	static CTrainTrack * GetTrainTrack(const s32 iTrackIndex);
	static s32			GetTrackIndexFromNode(CTrainTrackNode * pNode);
	static CTrain*		IsTrainApproachingNode(CTrainTrackNode * pNode);
	

	// Useful queries.
	static bool		IsLoopedTrack(s8 trackIndex);
	static bool		StopsAtStations(s8 trackIndex);
	static void		FixupTrackNode(s8 trackIndex, s32& nodeIndex_inOut);
	static bool		FixupTrackPos(s8 trackIndex, float& trackPos_inOut);

	static void		CalcWorldPosAndForward(Vector3& worldPos_out, Vector3& forward_out, s8 trackIndex, float trackPosFront, bool bDirectionTrackForward, s32 initialNodeToSearchFrom, float carriageLength, bool carriagesAreSuspended, bool flipModelDir, bool& bIsTunnel);
	static s32	FindCurrentNode(s8 trackIndex, float trackPos, bool bDirectionTrackForward);
	static s32	FindCurrentNodeFromInitialNode(s8 trackIndex, float trackPos, bool bDirectionTrackForward, s32 initialNodeToSearchFrom);
	static bool     GetWorldPosFromTrackNode(Vector3& worldPos, s8 trackIndex, s32 trackNode);
    static void		GetWorldPosFromTrackPos(Vector3& worldPos_out, s32& currentNode, s32& nextNode, s8 trackIndex, float trackPos, float preferredSmoothLength, bool& bIsTunnel);
	static bool		GetWorldPosFromTrackSegmentPos(Vector3& worldPos_out, float segmentPos, s32 PreviousNode, s32 Node, s32 NextNode, const CTrainTrackNode *pTrackNodes, float PreferredSmoothLength);
	static	bool	sm_bDisplayTrainAndTrackDebug;
#if __BANK
    static	bool	sm_bProcessWithPhysics;
#endif
	static	bool	sm_bDisplayTrainAndTrackDebugOnVMap;
	static	bool	sm_bDisableRandomTrains;

	static float	sm_stationRadius;
	static float	ms_fCloseStationDistEps;	// Used for linking tracks with adjacent stations, if "link_tracks_with_adjacent_stations" is set in config
	static float	sm_speedAtWhichConsideredStopped;
	static float	sm_maxDeceleration;
	static float	sm_maxAcceleration;
	static	bool	sm_swingCarriages;
	static	bool	sm_swingCarriagesSideToSide;
	static float	sm_swingSideToSideAngMax;
	static float	sm_swingSideToSideRate;
	static float	sm_swingSideToSideNoiseFrequency;
	static float	sm_swingSideToSideNoiseStrength;
	static	bool	sm_swingCarriagesFrontToBack;
	static float	sm_swingFrontToBackAngMax;
	static float	sm_swingFrontToBackAngDeltaMaxPerSec;

	enum GenTrainStatus		{ TGEN_NOTRAINPENDING, TGEN_TRAINPENDING, TGEN_TRAINNETWORKPENDING };
	static	GenTrainStatus	sm_genTrain_status;
	static	s8		sm_genTrain_trackIndex;


	static unsigned	sm_networkTimeCreationRequest;

#if __BANK
	static void		PerformDebugTeleportEngineCB();
	static void		PerformDebugTeleportCB();
	static void		PerformDebugTeleportBaseCB(bool bTeleportToEngine = false);
	static void		PerformDebugTeleportIntoSeatCB();
	static void		PerformDebugTeleportToRoof();
#endif

#if !__FINAL
	static void		CullTrackNodesToArea(const Vector3 & vMin, const Vector3 & vMax);
	static void		ForAllTrackNodesIntersectingArea(const Vector3 & vMin, const Vector3 & vMax, ForAllTrackNodesCB callbackFunction, void * pData);
	static bool		CollisionTriangleIntersectsTrainTracks(Vector3 * pTriPts);
#endif

public:
	NETWORK_OBJECT_TYPE_DECL( CNetObjTrain, NET_OBJ_TYPE_TRAIN );

	// CTrain() is protected... see below...
	~CTrain();

	virtual void	SetModelId(fwModelId modelId);
	virtual int 	InitPhys();
	virtual void	InitDoors();	// virtual door stuff
	virtual void	InitCompositeBound();
	virtual void	PlayCarHorn(bool bOneShot, u32 hornTypeHash);

	virtual bool	ShouldDoFullUpdate();
	virtual void	UpdateLODAndFadeState(const Vector3& worldPosNew);
	virtual void	DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep);
	bool	        ProcessTrainControl(float fTimeStep);
	// physics
	virtual ePhysicsResult ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice);
	bool UpdatePosition(float fTimeStep);

	void			ProcessControlInputs(CPed *UNUSED_PARAM(pPlayerPed)){};
	bool			ProcessDoors(float fTargetRatio);
	void			ProcessPassengers();
	void			ProcessVfx();

	virtual ePrerenderStatus PreRender(const bool bIsVisibleInMainViewport = true);
	virtual void	PreRender2(const bool bIsVisibleInMainViewport = true);

	float			PopulateTrainDist() const;
	void			PopulateCarriage(s32& maxPedsToCreateForTrain_inOut);

	bool			IsLocalPlayerSeatedInThisCarriage() const;
	bool			IsLocalPlayerSeatedInAnyCarriage() const;
	bool			IsLocalPlayerSeatedOnThisEnginesTrain() const;
	bool			IsLocalPlayerStandingInThisTrain() const;
	bool			DoesLocalPlayerDisableAmbientTrainStops() const;

	virtual const Vector3& GetVelocity() const;
	virtual const Vector3& GetAngVelocity() const;
	
	virtual Mat34V_Out GetMatrixPrePhysicsUpdate() const;

	inline u32	GetCurrentNode() { return m_currentNode; }
	inline u32	GetPreviousNode() { return m_oldCurrentNode; }

	s32			StopAtStationDurationNormal() const;
	s32			StopAtStationDurationPlayer() const;
	s32			StopAtStationDurationPlayerWithGang() const;
	bool			AnnouncesStations() const;
	bool			DoorsGiveWarningBeep() const;
	bool			CarriagesAreSuspended() const;
	bool			CarriagesSwing() const;
	bool			IsEngine() const;
	float			GetCarriageVerticalOffsetFromTrack() const; 
	int				GetNumCarriages() const;

#if __ASSERT
	bool AllowedToActivate() const { return m_trackIndex == -1 || m_nTrainFlags.bCreatedAsScriptVehicle; }
#endif // __ASSERT

	virtual void BlowUpCar( CEntity *pCulprit, bool bInACutscene = false, bool bAddExplosion = true, bool bNetCall = false, u32 weaponHash = WEAPONTYPE_EXPLOSION, bool bDelayedExplosion = false);

	// Flag this the train/driver has been threaten and should respond accordingly
	void			SetThreatened();

// DEBUG!! -AC, These are here just for audio support.
	int				GetCurrentStation() const;
	int				GetNextStation() const;
	const char*		GetStationName(int stationId) const;
	int				GetTimeTilNextStation() const;
// END DEBUG!!

	void			SignalTrainPadShake(u32 iTimeToSecondaryShake = 0);

	void			ScanForEntitiesBlockingProgress(float & fInOut_dirForwardSpeedDesired);
	bool			IsEntityBlockingProgress(CEntity * pEntity, const Vector3 & vTrainFrontPos, const float fScanDist, float & fOut_DistToEntity);

	void			FindNextStationPositionInDirection(bool bDir, float PosOnTrack, float *pResultPosOnTrack, s32 *pStation) const;
	void			SetTrainStopsForStations(bool bStops);
	void			SetTrainIsStoppedAtStation();
	void			SetTrainToLeaveStation();
	bool			FindNearestStationSide(s8 trackIndex, float PositionOnTrack) const;

	void 			GetStationRelativeInfo(s32& currentStation_out, s32& nextStation_out, float& travelDistToNextStation_out, float& maxSpeedToApproachStation_out, int & iPlatformSideForNextStation_out) const;

	void			SetStationSideForEngineAndCarriages(int iSides);	// 0:left, 1:right, 2:both
	void			SetTrackPosFromWorldPos(s32 currentNode = -1, s32 nextNode = -1);
	bool			CanPedGetOnTrain(const CPed& rPed) const;
	
	bool			AddPassenger(CPed* pPed);
	void			ClearInvalidPassengers();
	u32				GetEmptyPassengerSlots() const;
	bool			HasPassengers() const;
	bool			RemovePassenger(CPed* pPed);

	bool			HasNetworkClonedPassenger() const;

	inline bool			GetIsMissionTrain() const { return m_iMissionScriptId!=THREAD_INVALID || (NetworkInterface::IsGameInProgress() && GetScriptThatCreatedMe() != nullptr); }
	inline scrThreadId	GetMissionScriptId() const { return m_iMissionScriptId; }

	int				GetTrackIndex() const { return m_trackIndex; }

	TrainState		GetTrainState() const { return m_nTrainState; }

	void			InitiateTrainReportOnJoin(u8 playerIndex);

	void			SetDoorsForcedOpen(bool bValue) { m_doorsForcedOpen = bValue; }
	const bool		GetDoorsForcedOpen() { return m_doorsForcedOpen; }

	bool			IsDoorBlocked(const CCarDoor& rDoor) const;

	struct CTrainFlags
	{
		u8	bAtStation:1;
		u8	bEngine:1;					// The one that controls the speed etc.
		u8	bCaboose:1;					// The one with the tail lights
//		u8	bMissionTrain:1;
		u8	bDirectionTrackForward:1;
		u8	bStopForStations:1;			// Is this a passenger train that stops at stations?

		u8	bIsForcedToSlowDown:1;
		u8	bHasPassengerCarriages:1;
		u8	bIsCarriageTurningRight:1;

		u8	iStationPlatformSides:2;	// 0:left (along positive 'A' vector), 1:right, 2:both
		u8	bRenderAsDerailed:1;
		u8	bStartedOpeningDoors:1;
		u8	bStartedClosingDoors:1;

		u8	bDisableNextStationAnnouncements : 1;
		u8	bCreatedAsScriptVehicle : 1;	// don't treat this like a train, it is just a script vehicle
		u8	bIsCutsceneControlled : 1;
		u8	bIsSyncedSceneControlled : 1;
		u8  bAllowRemovalByPopulation : 1;
	};

	enum UpdateLODState	{ FADING_IN, NORMAL, FADING_OUT, SIMPLIFIED };

	UpdateLODState	m_updateLODState;
	u32			m_updateLODFadeStartTimeMS;
	float			m_updateLODCurrentAlpha;

	Matrix34		m_inactiveLastMatrix;
	Vector3			m_inactiveMoveSpeed;
	Vector3			m_inactiveTurnSpeed;	

	float			m_desiredCruiseSpeed;	//decided to put this here rather then in a vehicle task, as this is all the vehicle task would store.
	float			m_trackForwardSpeed;
	float			m_trackForwardAccPortion;
	float			m_frontBackSwingAngPrev;
	float			m_trackPosFront;
	float			m_trackSteepness;		// The angle of the track with the horizon. Used by the cablecar to place the hanger thingy.

	float			m_wheelSquealIntensity;

	u32			m_currentNode;		// We can speed stuff up by not always looking from 0
	u32			m_oldCurrentNode;		// Old position data

	private:
	RegdTrain		m_pEngine;				// Pointer to the carriage that is at the head of the train.
	RegdTrain		m_pLinkedToForward;		// Pointer to the carriage we're linked to in the forward direction.
	RegdTrain		m_pLinkedToBackward;	// Pointer to the carriage we're linked to in the rear direction.

	public:
	s32			m_currentStation;
	s32			m_nextStation;
	float			m_travelDistToNextStation;
	CTrainFlags		m_nTrainFlags;

	s8			m_trackIndex;				// What track are we on?
	s8			m_trainConfigIndex;
	s8			m_trainConfigCarriageIndex;
	float			m_trackDistFromEngineFrontForCarriageFront;

	u32			m_TimeSliceLastProcessed;

	u32			m_iTimeOfSecondaryPadShake;
	
	static const int sMaxPassengers = 8;
	atFixedArray<RegdPed, sMaxPassengers>	m_aPassengers;
	
	TrainState		m_nTrainState;
	float			m_fTimeInTrainState;
	PassengersState	m_nPassengersState;

	s32				m_iLastTimeScannedForVehicles;
	RegdVeh			m_pVehicleBlockingProgress;

	s32				m_iLastTimeScannedForPeds;
	RegdPed			m_pPedBlockingProgress;

	u32				m_iLastHornTime;
	u32				m_iTimeWaitingForBlockage;

	static const float sm_fMinSecondsToConsiderBeingThreatened;
	float			m_fTimeSinceThreatened;

	u32				m_requestInteriorRescanTime;

	bool			m_bMetroTrain;
	bool			m_bArchetypeSet;

	// Script ID which owns this train
	// This replaces the previous 'm_nTrainFlags.bMissionTrain'
	scrThreadId		m_iMissionScriptId;

protected:
	// This is PROTECTED as only CVEHICLEFACTORY is allowed to call this - make sure you use CVehicleFactory!
	// DONT make this public!
	CTrain(const eEntityOwnedBy ownedBy, u32 popType);
	friend class CVehicleFactory;

#if __ASSERT
	u32			m_uFrameCreated; // keep track of the frame we were created on, for assert purposes
#endif

	
public:

	bool FindClosestBoardingPoint(Vec3V_In vPos, CBoardingPoint& rBoardingPoint) const;
	
	void ChangeTrainState(TrainState nTrainState);

	static void		ValidateLinkedLists( CTrain* pCarriage );

private:
	
	void		CorrectLocalPlayerInsideTrain();

	bool		WasProcessedThisTimeSlice() const;
	void		SetWasProcessedThisTimeSlice();

	bool		ArePassengersGettingOff() const;
	bool		ArePassengersGettingOn() const;
	bool		GetFlipModelDir() const;
	fwFlags8	GetSideFlagsClosestToPlatform() const;
	void		MakeNearbyPedsGetOn();
	void		MakePassengersGetOff();

#if !__FINAL
	const char*	GetTrainStateString();
	const char* GetPassengerStateString();
	const int	GetFullTrainPassengerCount();
#endif

private:
	void DoInteriorLights();

	static bool		CheckTrainSpawnTrackInView(s32 generationNode, bool bDirection, u8 trainConfigIndex, const Vector3& vCamCenter);

//	static void		LoadTrack(char *pFileName, char *pParamString0, char *pParamString1, char *pParamString2, char *pParamString3, char *pParamString4);

	static void		ReadAndInterpretTrackFile(const char * pFileName, CTrainTrack & track, TrainStation ** stations);
	static void		PostProcessTrack(CTrainTrack & track, const float fMaxSpacingBetweenNodes=50.0f);

	static void     InitLoadConfig();

#if DEBUG_DRAW
	static void		SceneUpdateDebugDisplayTrainAndTrack(fwEntity &entity, void *userData);
#endif // DEBUG_DRAW


private:
	static CTrainCloudListener* msp_trainCloudListener;

	//Need to make door array private to make sure that doors are accessed by function by referencing either the 
	//eDoors enum or the hierarchyId enum.  We need to get the door from a function so that we can readily 
	//switch from left-hand drive right-hand drive.
	CCarDoor m_aTrainDoors[MAX_TRAIN_DOORS];

	bool m_doorsForcedOpen;

public:
	static void DumpTrainInformation();
	static u32  ms_iTrainNetworkHearbeatInterval;
	static bool ms_bEnableTrainsInMP;
	static bool ms_bEnableTrainPassengersInMP;
	static bool ms_bEnableTrainLinkLoopForceCrash;

#if __BANK
	static bool sm_bDebugTrain;
	static bool ms_bForceSpawn;
#endif
};


#endif // _TRAIN_H_
