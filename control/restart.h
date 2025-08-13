
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    Restart.h
// PURPOSE : Handles restarting the player after he has been arrested or killed
// AUTHOR :  Graeme
// CREATED : 02-08-00
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef RESTART_H
#define RESTART_H

// Rage headers
#include "atl/queue.h"
#include "fwutil/Flags.h"
#include "scene/RegdRefTypes.h"
#include "script/script_areas.h"
#include "vector/vector3.h"
#include "ai/navmesh/navmesh.h"

// Forward declarations
class CNetGamePlayer;

#define MAX_HOSPITAL_RESTARTS	(10)
#define MAX_POLICE_RESTARTS		(20)
#define MAX_NETWORK_RESTARTS	(32)
#define MAX_SAVE_HOUSES			(12)

#define MAX_LENGTH_OF_ROOM_NAME	(32)
struct SaveHouseStruct
{
	Vector3 Coords;
	float Heading;
	char RoomName[MAX_LENGTH_OF_ROOM_NAME];
	s32 WhenToUse;
	u32 m_PlayerModelNameHash;
	bool m_bEnabled;
	bool m_bAvailableForAutosaves;
	//CInventory m_Inventory;
};


struct RestartPoint
{
	Vector3 m_Coords;
	float	m_Heading;
	s32		m_WhenToUse;
	bool	m_bEnabled;
};

class CRestart
{
public :

	static RestartPoint HospitalRestarts[MAX_HOSPITAL_RESTARTS];
	static s32 NumberOfHospitalRestarts;

	static RestartPoint PoliceRestarts[MAX_POLICE_RESTARTS];
	static s32 NumberOfPoliceRestarts;

	static SaveHouseStruct SaveHouses[MAX_SAVE_HOUSES];
	static s32 NumberOfSaveHouses;

	static bool ms_bOverrideSavehouse;
	static Vector3 ms_vCoordsOfSavehouseOverride;
	static float ms_fHeadingOfSavehouseOverride;

	static Vector3 NetworkRestartPoints[MAX_NETWORK_RESTARTS];
	static float NetworkRestartHeadings[MAX_NETWORK_RESTARTS];
	static u32 NetworkRestartTimes[MAX_NETWORK_RESTARTS];
	static s32 NetworkRestartTeams[MAX_NETWORK_RESTARTS];
	static s32 NumberOfNetworkRestarts;

    static bool bOverrideRestart;
	static Vector3 OverridePosition;
	static float OverrideHeading;
	static bool bPausePlayerRespawnAfterDeathArrest;

	static	bool	bSuppressFadeOutAfterDeath;
	static	bool	bSuppressFadeOutAfterArrest;
	static	bool	bSuppressFadeInAfterDeathArrest;

	static	Vector3	ExtraHospitalRestartCoors;
	static	float	ExtraHospitalRestartRadius;
	static	float	ExtraHospitalRestartHeading;

	static	Vector3	ExtraPoliceStationRestartCoors;
	static	float	ExtraPoliceStationRestartRadius;
	static	float	ExtraPoliceStationRestartHeading;

	static	bool	bOverrideRespawnBasePointForMission;
	static	Vector3	OverrideRespawnBasePointForMission;

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static s32 AddHospitalRestartPoint(const Vector3 &NewPoint, const float NewHeading, s32 WhenToUse = 0);
	static void DisableHospitalRestartPoint(const s32 index, const bool bDisable);

	static s32 AddPoliceRestartPoint(const Vector3 &NewPoint, const float NewHeading, s32 WhenToUse = 0);
	static void DisablePoliceRestartPoint(const s32 index, const bool bDisable);

	static int AddSaveHouse(const Vector3 &NewPoint, const float NewHeading, const char *pRoomName, s32 WhenToUse, u32 playerModelNameHash);
	static void SetSaveHouseEnabled(int SaveHouseIndex, bool bNewEnabledFlag, bool bAvailableForAutosaves);

	static void SetSavehouseOverride(bool bOverride, const Vector3 &vecCoords, float fHeading);

	static void AddNetworkRestartPoint(const Vector3 &NewPoint, const float NewHeading, const int Team);
	static void RemoveAllNetworkRestartPoints();
	static bool FindNetworkRestartPoint(Vector3 *pResultCoors, float *pResultHeading, int Team);
	static void FindNetworkRestartPointForScript(Vector3 *pResultCoors, float *pResultHeading, int Team);

	static bool FindClosestHospitalRestartPoint(const Vector3& TestPoint, float fPlayerCurrentHeading, Vector3 *ClosestPoint, float *pHeading);
	static void FindClosestPoliceRestartPoint(const Vector3& TestPoint, float fPlayerCurrentHeading, Vector3 *ClosestPoint, float *pHeading);


	static void OverrideNextRestart(const Vector3 &OverridePoint, const float OverrideHdng);
	static void CancelOverrideRestart(void);

	static void SetIgnoreNextRestart(bool bIgnore);
	static bool GetIgnoreNextRestart() { return ms_bIgnoreNextRestart; }

	static void SetRespawnPointForDurationOfMission(const Vector3& Coors);
	static void ClearRespawnPointForDurationOfMission();

private:

	static bool ms_bIgnoreNextRestart;

	static void ResetData(void);
};

// CLASS : CNetRespawnMgr
// PURPOSE : System to handle automatic generation/selection of respawn points during network game

class CNetRespawnMgr
{

public:

	struct Position
	{
		enum Flags
		{
			IsInAircraft = BIT0,
		};

		Position()
		{ Reset(); }

		void Reset()
		{
			m_vPosition.Zero();
			m_uFlags.ClearAllFlags();
		}

		bool UseFlatDistance() const
		{
			return (m_uFlags.IsFlagSet(IsInAircraft));
		}

		Vector3		m_vPosition;
		fwFlags8	m_uFlags;
	};

public:

	static const int sMaxPositions = 32;
	typedef atFixedArray<Position, sMaxPositions> Positions;

#if __BANK
	static const int sMaxOverridePositions = 12;
	typedef atQueue<Position, sMaxOverridePositions> OverridePositions;
#endif

public:

	struct CachedPositions
	{
		CachedPositions()
		: m_uTimeLastUpdated(0)
		{}

		bool IsValid() const
		{
			return (m_uTimeLastUpdated == fwTimer::GetTimeInMilliseconds());
		}

		Positions	m_EnemyPositions;
		Positions	m_FriendlyPositions;
		u32			m_uTimeLastUpdated;
	};

public:

	enum
	{
		FLAG_SCRIPT_COMMAND					= BIT0,		//Whether the search was launched via script command.
		FLAG_IGNORE_TARGET_HEADING			= BIT1,		//Gives no value to spawn points with better headings.
		FLAG_PREFER_CLOSE_TO_SPAWN_ORIGIN	= BIT2,		//Gives more value to spawn points close to the origin.
		FLAG_MAY_SPAWN_IN_INTERIOR			= BIT3,		//Allows spawn points in interior spaces.
		FLAG_MAY_SPAWN_IN_EXTERIOR			= BIT4,		//Allows spawn points in exterior spaces.
		FLAG_PREFER_WIDE_FOV				= BIT5,		//Gives more value to spawn points with wider FOVs.
		FLAG_PREFER_TEAM_BUNCHING			= BIT6,		//Gives more value to spawn points closer to team bunches and away from enemy bunches.
		FLAG_PREFER_ENEMY_PLAYERS_FARTHER	= BIT7,		//Gives more value to spawn points farther away from enemy players.
		FLAG_PREFER_FRIENDLY_PLAYERS_CLOSER	= BIT8,		//Gives more value to spawn points closer to friendly players.
		FLAG_PREFER_ENEMY_AI_FARTHER		= BIT9,		//Gives more value to spawn points farther away from enemy AI.
		FLAG_PREFER_FRIENDLY_AI_CLOSER		= BIT10,	//Gives more value to spawn points closer to friendly AI.
		FLAG_PREFER_RANDOMNESS				= BIT11,	//Gives more value to spawn points randomly.
		FLAG_IGNORE_Z						= BIT12,	//Ignores height when searching for respawn points.
	};
	enum
	{
		RESPAWN_QUERY_RESULTS_FAILED			= 0,
		RESPAWN_QUERY_RESULTS_SUCCEEDED			= 1,
		RESPAWN_QUERY_RESULTS_STILL_WORKING		= 2,
		RESPAWN_QUERY_RESULTS_NO_SEARCH_ACTIVE	= 3
	};
	enum
	{
		SEARCH_VOLUME_SPHERE							= 1,
		SEARCH_VOLUME_ANGLED_AREA						= 2
	};

	struct TSpawnPoint
	{
		enum Flags
		{
			FLAG_PAVEMENT	= 0x1,
			FLAG_INTERIOR	= 0x2
		};

		TSpawnPoint()
			: m_vPos(VEC3_ZERO)
			, m_fHeading(0.0f)
			, m_Score()
			, m_iFlags(0)
		{

		}

		struct Score
		{
			Score()
				: m_fTotal(0.0f)
#if __BANK
				, m_fFOV(0.0f)
				, m_fProximityToOrigin(0.0f)
				, m_fTeamBunching(0.0f)
				, m_fEnemyPlayersFarther(0.0f)
				, m_fFriendlyPlayersCloser(0.0f)
				, m_fEnemyAiFarther(0.0f)
				, m_fFriendlyAiCloser(0.0f)
				, m_fRandomness(0.0f)
#endif
			{

			}

			float m_fTotal;
#if __BANK
			float m_fFOV;
			float m_fProximityToOrigin;
			float m_fTeamBunching;
			float m_fEnemyPlayersFarther;
			float m_fFriendlyPlayersCloser;
			float m_fEnemyAiFarther;
			float m_fFriendlyAiCloser;
			float m_fRandomness;
#endif
		};

		Vector3 m_vPos;
		float m_fHeading;
		Score m_Score;
		s32 m_iFlags;
	};

#if __BANK
	struct TSpawnRecord
	{
		TSpawnRecord()
			: m_aEnemyPlayerPositions()
			, m_aFriendlyPlayerPositions()
			, m_aEnemyAiPositions()
			, m_aFriendlyAiPositions()
			, m_BestSpawnPoint()
			, m_vOrigin(VEC3_ZERO)
			, m_vTarget(VEC3_ZERO)
			, m_pPed(NULL)
			, m_iTeam(-1)
			, m_fMinDistance(0.0f)
			, m_fMaxDistance(0.0f)
			, m_uFlags(0)
		{

		}

		Positions		m_aEnemyPlayerPositions;
		Positions		m_aFriendlyPlayerPositions;
		Positions		m_aEnemyAiPositions;
		Positions		m_aFriendlyAiPositions;
		TSpawnPoint		m_BestSpawnPoint;
		Vector3			m_vOrigin;
		Vector3			m_vTarget;
		RegdConstPed	m_pPed;
		int				m_iTeam;
		float			m_fMinDistance;
		float			m_fMaxDistance;
		fwFlags16		m_uFlags;
	};
#endif

	static const float ms_fSearchMinDistDefault;
	static const float ms_fSearchMaxDistDefault;
	static const float ms_fSearchMaxDistCutoff;
	static const float ms_fSearchHeadingNone;
	static const Vector3 ms_vSearchTargetPosNone;
	
	static float	ms_fMaxDistanceForBunchingPlayers;
	static float	ms_fMinDistanceForScoringEnemyPlayersFarther;
	static float	ms_fMaxDistanceForScoringEnemyPlayersFarther;
	static float	ms_fMinDistanceForScoringFriendlyPlayersCloser;
	static float	ms_fIdealDistanceForScoringFriendlyPlayersCloser;
	static float	ms_fMaxDistanceForScoringFriendlyPlayersCloser;
	static float	ms_fMinDistanceForScoringEnemyAiFarther;
	static float	ms_fMaxDistanceForScoringEnemyAiFarther;
	static float	ms_fMinDistanceForScoringFriendlyAiCloser;
	static float	ms_fIdealDistanceForScoringFriendlyAiCloser;
	static float	ms_fMaxDistanceForScoringFriendlyAiCloser;
	
	static float	ms_fMinScoreFOV;
	static float	ms_fMinScoreProximityToOrigin;
	static float	ms_fMinScoreTeamBunchingDirection;
	static float	ms_fMinScoreTeamBunchingPosition;
	static float	ms_fMinScoreEnemyPlayersFarther;
	static float	ms_fMinScoreFriendlyPlayersCloser;
	static float	ms_fMinScoreFriendlyPlayersTooClose;
	static float	ms_fMinScoreEnemyAiFarther;
	static float	ms_fMinScoreFriendlyAiCloser;
	static float	ms_fMinScoreFriendlyAiTooClose;
	static float	ms_fMinScoreRandomness;
	static float	ms_fMaxScoreFOV;
	static float	ms_fMaxScoreProximityToOrigin;
	static float	ms_fMaxScoreTeamBunchingDirection;
	static float	ms_fMaxScoreTeamBunchingPosition;
	static float	ms_fMaxScoreEnemyPlayersFarther;
	static float	ms_fMaxScoreFriendlyPlayersCloser;
	static float	ms_fMaxScoreFriendlyPlayersTooClose;
	static float	ms_fMaxScoreEnemyAiFarther;
	static float	ms_fMaxScoreFriendlyAiCloser;
	static float	ms_fMaxScoreFriendlyAiTooClose;
	static float	ms_fMaxScoreRandomness;

	static const s32 ms_iMaxStoredNavmeshes = 16;
	static const s32 ms_iMaxSpawnPoints = 128;
	
#if __BANK
	static bool		ms_bDebugUseAngledArea;
	static bool		ms_bAdvancedUsePlayerAsPed;
	static bool		ms_bAdvancedUsePlayerAsOrigin;
	static bool		ms_bAdvancedUseCameraAsOrigin;
	static bool		ms_bAdvancedUsePlayerAsTarget;
	static bool		ms_bAdvancedUseCameraAsTarget;
	static float	ms_fAdvancedMinSearchDistance;
	static float	ms_fAdvancedMaxSearchDistance;
	static bool		ms_bAdvancedFlagScriptCommand;
	static bool		ms_bAdvancedFlagIgnoreTargetHeading;
	static bool		ms_bAdvancedFlagPreferCloseToSpawnOrigin;
	static bool		ms_bAdvancedFlagMaySpawnInInterior;
	static bool		ms_bAdvancedFlagMaySpawnInExterior;
	static bool		ms_bAdvancedFlagPreferWideFOV;
	static bool		ms_bAdvancedFlagPreferTeamBunching;
	static bool		ms_bAdvancedFlagPreferEnemyPlayersFarther;
	static bool		ms_bAdvancedFlagPreferFriendlyPlayersCloser;
	static bool		ms_bAdvancedFlagPreferEnemyAiFarther;
	static bool		ms_bAdvancedFlagPreferFriendlyAiCloser;
	static bool		ms_bAdvancedFlagPreferRandomness;
	static bool		ms_bDrawResultForPedPositionProjectionTesting;
	static float	ms_fTimeForPedPositionProjectionTesting;
	static Vector3	ms_vResultForPedPositionProjectionTesting;
	
	static OverridePositions ms_aEnemyPlayerPositionsOverride;
	static OverridePositions ms_aFriendlyPlayerPositionsOverride;
	static OverridePositions ms_aEnemyAiPositionsOverride;
	static OverridePositions ms_aFriendlyAiPositionsOverride;
	
	static TSpawnRecord ms_SpawnRecord;
#endif

	CNetRespawnMgr();
	~CNetRespawnMgr();

	static void Init(unsigned int initMode);
	static void Shutdown(unsigned int shutdownMode);
	static void Update();

	// Interface
	struct StartSearchInput
	{
		StartSearchInput(const Vector3& vOrigin)
		: m_iSearchVolumeType(SEARCH_VOLUME_SPHERE)
		, m_pPed(NULL)
		, m_vOrigin(vOrigin)
		, m_vTargetPos(ms_vSearchTargetPosNone)
		, m_fMinDist(ms_fSearchMinDistDefault)
		, m_fMaxDist(ms_fSearchMaxDistDefault)
		, m_iTeam(-1)
		, m_uFlags(0)
		{
		
		}
		
		StartSearchInput(const Vector3& vAngledAreaPt1, const Vector3& vAngledAreaPt2, const float fAngledAreaWidth)
			: m_iSearchVolumeType(SEARCH_VOLUME_ANGLED_AREA)
			, m_pPed(NULL)
			, m_vAngledAreaPt1(vAngledAreaPt1)
			, m_vAngledAreaPt2(vAngledAreaPt2)
			, m_fAngledAreaWidth(fAngledAreaWidth)
			, m_vTargetPos(ms_vSearchTargetPosNone)
			, m_fMinDist(ms_fSearchMinDistDefault)
			, m_fMaxDist(ms_fSearchMaxDistDefault)
			, m_iTeam(-1)
			, m_uFlags(0)
		{

		}

		s32				m_iSearchVolumeType;
		RegdConstPed	m_pPed;
		Vector3			m_vTargetPos;

		Vector3			m_vOrigin;
		Vector3			m_vAngledAreaPt1;
		Vector3			m_vAngledAreaPt2;
		float			m_fAngledAreaWidth;

		
		float			m_fMinDist;
		float			m_fMaxDist;
		int				m_iTeam;
		fwFlags16		m_uFlags;
	};
	static void StartSearch(const Vector3 & vOrigin, const Vector3 & vTargetPos=ms_vSearchTargetPosNone, const float fMinDist=ms_fSearchMinDistDefault, const float fMaxDist=ms_fSearchMaxDistDefault, const fwFlags16 uFlags = 0);
	static void StartSearch(const Vector3 & vAngledAreaPt1, const Vector3 & vAngledAreaPt2, const float fAngledAreaWidth, const Vector3 & vTargetPos=ms_vSearchTargetPosNone, const fwFlags16 uFlags = 0);
	static void StartSearch(const StartSearchInput& rInput);
	
	static bool	FindLargestBunchOfPlayers(bool bFriendly, Vector3& vPosition);
	static bool	IsSearchActive();
	static bool	IsSearchComplete();
	static bool	WasSearchSuccessful();
	static int	GetNumSearchResults();
	static void GetSearchResults(const int iIndex, Vector3 & vSpawnPosOut, float & fSpawnHeadingOut);
	static s32 GetSearchResultFlags(const int iIndex);

	static void CancelSearch();

#if __BANK
	static Vec3V_Out	CalculateProjectedPositionForPlayer(float fTime);
	static void			InitWidgets();
#endif

protected:

	static bool		StreamAroundSearchOrigin();
	static bool		UpdateSearch();
	static bool		EvaluateSpawnPoint(TNavMeshPoly* pPoly, TSpawnPoint& rSpawnPoint);
	static float	FindFieldOfViewAtPosition(const Vector3 & vPos, TNavMeshPoly * pPoly, const float fBaseHeading);
	static bool		InsertSpawnPoint(const TSpawnPoint& rSpawnPoint);
	
	static void				AddPosition(Positions& rPositions, const CPed& rPed, bool bUseProjectedPosition);
	static Vec3V_Out		CalculateProjectedPosition(const CPed& rPed, float fTime, float& fDistanceFromStart);
	static void				EvaluateAiPositions(TSpawnPoint& rSpawnPoint);
	static bool				EvaluateFOVAndHeading(TNavMeshPoly* pPoly, TSpawnPoint& rSpawnPoint);
	static void				EvaluatePlayerPositions(TSpawnPoint& rSpawnPoint);
	static void				EvaluateProximityToOrigin(TSpawnPoint& rSpawnPoint);
	static void				EvaluateRandomness(TSpawnPoint& rSpawnPoint);
	static void				EvaluateTeamBunching(TSpawnPoint& rSpawnPoint);
	static void				FindClosestEnemyAndFriendlyAi(const Vector3& vPos, float& fClosestEnemyDistSq, float& fClosestFriendlyDistSq);
	static void				FindClosestEnemyAndFriendlyPlayers(const Vector3& vPos, float& fClosestEnemyDistSq, float& fClosestFriendlyDistSq);
	static void				FindClosestEnemyAndFriendlyPositions(const Vector3& vPos, const Positions& rEnemyPositions, const Positions& rFriendlyPositions, float& fClosestEnemyDistSq, float& fClosestFriendlyDistSq);
	static void				FindEnemyAndFriendlyAiPositions(Positions& rEnemyAiPositions, Positions& rFriendlyAiPositions);
	static void				FindEnemyAndFriendlyAiPositionsFromWorld(Positions& rEnemyAiPositions, Positions& rFriendlyAiPositions);
	static void				FindEnemyAndFriendlyPlayerPositions(Positions& rEnemyPlayerPositions, Positions& rFriendlyPlayerPositions);
	static void				FindEnemyAndFriendlyPlayerPositionsFromNetwork(Positions& rEnemyPlayerPositions, Positions& rFriendlyPlayerPositions);
	static CNetGamePlayer*	GetNetPlayerForPed(const CPed& rPed);
	static int				GetNetTeamForPed(const CPed& rPed);
	static void				UpdateLargestBunchOfEnemyPlayers();
	static void				UpdateLargestBunchOfFriendlyPlayers();
	static void				UpdateLargestBunches();
	
protected:

	struct EvaluatePositionsInput
	{
		EvaluatePositionsInput()
		: m_bCareAboutEnemies(false)
		, m_bCareAboutFriendlies(false)
		, m_fClosestEnemyDistSq(-1.0f)
		, m_fClosestFriendlyDistSq(-1.0f)
		, m_fMinDistanceForScoringEnemiesFarther(0.0f)
		, m_fMaxDistanceForScoringEnemiesFarther(0.0f)
		, m_fMinDistanceForScoringFriendliesCloser(0.0f)
		, m_fIdealDistanceForScoringFriendliesCloser(0.0f)
		, m_fMaxDistanceForScoringFriendliesCloser(0.0f)
		, m_fMinScoreEnemiesFarther(0.0f)
		, m_fMaxScoreEnemiesFarther(0.0f)
		, m_fMinScoreFriendliesCloser(0.0f)
		, m_fMaxScoreFriendliesCloser(0.0f)
		, m_fMinScoreFriendliesTooClose(0.0f)
		, m_fMaxScoreFriendliesTooClose(0.0f)
		{
		
		}
		
		bool	m_bCareAboutEnemies;
		bool	m_bCareAboutFriendlies;
		float	m_fClosestEnemyDistSq;
		float	m_fClosestFriendlyDistSq;
		float	m_fMinDistanceForScoringEnemiesFarther;
		float	m_fMaxDistanceForScoringEnemiesFarther;
		float	m_fMinDistanceForScoringFriendliesCloser;
		float	m_fIdealDistanceForScoringFriendliesCloser;
		float	m_fMaxDistanceForScoringFriendliesCloser;
		float	m_fMinScoreEnemiesFarther;
		float	m_fMaxScoreEnemiesFarther;
		float	m_fMinScoreFriendliesCloser;
		float	m_fMaxScoreFriendliesCloser;
		float	m_fMinScoreFriendliesTooClose;
		float	m_fMaxScoreFriendliesTooClose;
	};
	struct EvaluatePositionsOutput
	{
		EvaluatePositionsOutput()
		: m_fScoreEnemiesFarther(0.0f)
		, m_fScoreFriendliesCloser(0.0f)
		{
		
		}
		
		float	m_fScoreEnemiesFarther;
		float	m_fScoreFriendliesCloser;
	};

	static void EvaluatePositions(const EvaluatePositionsInput& rInput, EvaluatePositionsOutput& rOutput);
	
#if __BANK
	static void		AddSpawnRecord();
	static void		CopyEnemyAndFriendlyPositionsFromOverrides(const OverridePositions& rEnemyOverridePositions, const OverridePositions& rFriendlyOverridePositions, Positions& rEnemyPositions, Positions& rFriendlyPositions);
	static void		FindEnemyAndFriendlyAiPositionsFromOverrides(Positions& rEnemyAiPositions, Positions& rFriendlyAiPositions);
	static void		FindEnemyAndFriendlyPlayerPositionsFromOverrides(Positions& rEnemyPlayerPositions, Positions& rFriendlyPlayerPositions);
#endif

	static RegdConstPed m_pPed;
	static Vector3 m_vSearchOrigin;
	static Vector3 m_vSearchTargetPos;
	static float m_fSearchOptimalHeading;
	static float m_fSearchMinDist;
	static float m_fSearchMaxDist;
	static CScriptAngledArea m_SearchAngledArea;
	static Vector3 m_vSearchMins;
	static Vector3 m_vSearchMaxs;
	static s32 m_iSearchVolumeType;
	static int m_iTeam;
	static fwFlags16 m_uFlags;
	static u32 m_iSearchStartTimeMs;
	static u32 m_iSeachNumFrames;
	
	static Vector3 m_vLargestBunchOfEnemyPlayersPosition;
	static Vector3 m_vLargestBunchOfFriendlyPlayersPosition;
	
	static bool m_bSearchActive;
	static bool m_bSearchComplete;
	static bool m_bSearchSuccessful;

	static atRangeArray<TNavMeshIndex, ms_iMaxStoredNavmeshes> m_Timeslice_StoredNavmeshes;
	static s32 m_Timeslice_iCurrentNavmesh;
	static s32 m_Timeslice_iCurrentPoly;

	static s32 m_iNumBestSpawnPoints;
	static atRangeArray<TSpawnPoint, ms_iMaxSpawnPoints + 1> m_BestSpawnPoints;

	static float m_fCachedNavmeshLoadDistance;

	static const float ms_fZCutoff;

	static CachedPositions m_PlayerPositionsFromNetwork;
	static CachedPositions m_AiPositionsFromWorld;

#if __BANK
	static bool m_bDebugSearch;
	static bool m_bDebugScores;
	static bool ms_bUsePlayerOverrides;
	static bool ms_bUseAiOverrides;
#endif
};

#endif
