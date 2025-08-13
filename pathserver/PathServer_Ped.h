#ifndef PATHSERVER_PED_H
#define PATHSERVER_PED_H

#include "ai/navmesh/pedgen.h"		// Contains stuff that used to be declared in this header file.
#include "fwmaths/Vector.h"
#include "script/script_areas.h"

// This is the maximum number of ped generation coords which may be found each time the pathserver thread
// looks for pedgen coordinates.

// Note: most of the contents in this file were moved to 'ai/navmesh/pedgen.h'. /FF

// Must be enough to hold the existing peds in the world, plus the new coordinates we find during the algorithm processing
#define MAX_STORED_PED_POSITIONS	MAX_POTENTIAL_PEDGEN_COORDS*2

// CLASS : CPathServerAmbientPedGen
// Variables & methods concerned with generation of spawn-points for ambient ped population
// I moved this from CPathServerGta to its own class to make distinct from CPathServerMultiplayerPedGen

class CPedGenNavMeshIteratorAmbient : public fwPedGenNavMeshIterator
{
public:
	CPedGenNavMeshIteratorAmbient() : fwPedGenNavMeshIterator() { }
	virtual ~CPedGenNavMeshIteratorAmbient() { }

	virtual void AddNavMesh(CNavMesh * pNavMesh);
};

class CPathServerAmbientPedGen
{
	friend class CPathServerThread;

public:

	CPathServerAmbientPedGen();

	static bank_bool ms_bPromoteDensityOneToDensityTwo;
	s32 m_iPedGenPercentagePerNavMesh;
	bool m_bPedGenNeedsProcessing;

	struct TPedGenVars
	{
		TPedGenVars()
		{
		}

		static const float m_fMinCreationZDist;
		static const float m_fMaxCreationZDist;

		static const float m_fNearbyPedsDesiredCount[8];
		static const float m_fNearbyPedsSearchDistances[8];

		int m_iNumPedPositions;
		Vector3 m_PedPositions[MAX_STORED_PED_POSITIONS];
		u32 m_PedFlags[MAX_STORED_PED_POSITIONS];	// members of the enumeration TPedGenParams::Flags

		bool m_bNavMeshesHaveChanged;

		CPopGenShape m_PopGenShape;
		TShortMinMax m_AreaMinMax;
	};

	TPedGenVars m_PedGenVars;
	CPedGenSortedList m_PedGenCoords;
	CPedGenNavMeshIteratorAmbient m_PedGenNavMeshIterator;

	CPedGenBlockedArea m_PedGenBlockedAreas[MAX_PEDGEN_BLOCKED_AREAS];
	Vector3 m_vPedGenConstraintArea;
	float m_fPedGenConstraintAreaRadiusSqr;
	bool m_bUsePedGenConstraintArea;

	atArray<s32> m_PedGenLoadedMeshes;

	bool IsActive();

	struct TPedGenParams
	{
		s32 m_iNumPedPositions;
		Vector3 * m_pPedPositionsArray;
		u32 * m_iPedFlagsArray;				// members of the enumeration TPedGenParams::Flags

		enum Flags
		{
			FLAG_SCENARIO_PED	= 0x1
		};
	};

	void SetPedGenParams(const TPedGenParams & params, const CPopGenShape & popGenShape);

	/*
		const int iNumPedPositions,						// Number of ped positions which are being passed in
		const Vector3 * pPedPositionsArray,				// The array of positions of peds in the world
		const u32 * pPedFlags,							// Flags associated with 
		const CPopGenShape & popGenShape,
		const float * pPedsPerMeterDensityLookupArray,
		const float * pNearbbyPedsSearchDistance,
		const float fMinZDistance,
		const float fMaxZDistance
	);
	*/

	void UnsetPedGenParams() { m_bPedGenNeedsProcessing = false; }
	bool AreCachedPedGenCoordsReady() const;
	bool GetCachedPedGenCoords(CPedGenCoords & destCachedCoords);
	void ClearCachedPedGenCoords();
	bool IsProcessingPedGen() { return m_PedGenCoords.GetInUse(); }

	// Get the potential ped generation coordinates as found by the pathserver worker thread.
	void FindPedGenCoordsBackgroundTask();
	void FindPedGenCoords_StartNewTimeSlice(const atArray<s32> & loadedMeshes);
	void FindPedGenCoords_ContinueTimeSlice();

	float GetNumPedsAroundSpawnPoint(const Vector3 & vPos, const float fAreaAround);
	s32 AddPedGenBlockedArea(const Vector3 & vOrigin, const float fRadius, const u32 iDuration, CPedGenBlockedArea::EOwner owner = CPedGenBlockedArea::EUnspecified/*const bool bCutscene=false*/, const bool bShrink = false);
	s32 AddPedGenBlockedArea(const Vector3 * pPts, const float fTopZ, const float fBottomZ, const u32 iDuration, CPedGenBlockedArea::EOwner owner = CPedGenBlockedArea::EUnspecified);
	void UpdatePedGenBlockedArea(s32 iIndex, const Vector3 & vOrigin, const float fRadius, const u32 iDuration);
	bool IsInPedGenBlockedArea(const Vector3 & vPos);
	void ClearAllPedGenBlockingAreas(const bool bCutsceneOnly, const bool bNonCutsceneOnly);
	void ClearAllPedGenBlockingAreasWithSpecificOwner(CPedGenBlockedArea::EOwner owner);
	void ProcessPedGenBlockedAreas();

#if __ASSERT
	void PrintDebugPedGenAreas();
#endif

	bool IsPositionWithinPedGenRegion(const Vector3 & vPos);

	void DefinePedGenConstraintArea(const Vector3 & vPos, const float fRadius);
	void DestroyPedGenConstraintArea();

	void Reset();

	void NotifyNavmeshesHaveChanged() { m_PedGenVars.m_bNavMeshesHaveChanged = true; }
	const CPedGenBlockedArea* GetPedGenBlockedAreas() { return m_PedGenBlockedAreas; }
};

class CPathServerOpponentPedGen
{
	friend class CPathServerThread;
	friend class ::CPathServer;

public:
	static const s32 ms_iMaxCoords = 256;

	enum
	{
		STATE_IDLE,
		STATE_ACTIVE,
		STATE_COMPLETE,
		STATE_FAILED
	};

	enum
	{
		FLAG_MAY_SPAWN_IN_INTERIOR						= BIT0,		//Allows spawn points in interior spaces.
		FLAG_MAY_SPAWN_IN_EXTERIOR						= BIT1,		//Allows spawn points in exterior spaces.
		FLAG_ALLOW_NOT_NETWORK_SPAWN_CANDIDATE_POLYS	= BIT2,		//Allows spawning on polys which are not marked as "network spawn candidate"
		FLAG_ALLOW_ISOLATED_POLYS						= BIT3,		//Allows spawning on polys marked as "isolated"
		FLAG_ALLOW_ROAD_POLYS							= BIT4,		//Allow spawning on polys which are marked as "road"
		FLAG_ONLY_POINTS_AGAINST_EDGES					= BIT5		//Only generate points which are next to yellow navmesh cover edges
	};

	enum
	{
		SEARCH_VOLUME_SPHERE							= 1,
		SEARCH_VOLUME_ANGLED_AREA						= 2
	};

public:
	CPathServerOpponentPedGen();

	bool StartSearch(const Vector3 & vOrigin, const float fRadius, const float fDistZ, const u32 iFlags, const float fMinimumSpacing, const u32 iTimeOutMs=0);
	bool StartSearch(const Vector3 & vAngledAreaPt1, const Vector3 & vAngledAreaPt2, const float fAngledAreaWidth, const u32 iFlags, const float fMinimumSpacing, const u32 iTimeOutMs=0);

	bool IsSearchActive() const;
	bool IsSearchComplete() const;
	bool IsSearchFailed() const;
	bool CancelSeach();
	s32 GetNumResults() const;
	bool GetResult(const s32 i, Vector3 & vPos) const;
	bool GetResultFlags(const s32 i, u32 & iFlags) const;

	void NotifyNavmeshesHaveChanged() { m_bNavMeshesHaveChanged = true; }

	s32 GetState() const { return m_iState; }

protected:

	class TSpawnPosition
	{
	public:
		enum Flags
		{
			FLAG_PAVEMENT	= 0x1,
			FLAG_INTERIOR	= 0x2
		};

		TSpawnPosition() { }
		TSpawnPosition(const Vector3 & pos, const float fScore, const u32 iFlags) { m_vPos = pos; m_fScore = fScore; m_iFlags = iFlags; }

		Vector3 m_vPos;
		float m_fScore;
		u32 m_iFlags;
	};

	s32 m_iState;
	s32 m_iSearchVolumeType;
	u32 m_iSearchStartTime;
	u32 m_iTimeOutMs;

	CScriptAngledArea m_SearchAngledArea;

	Vector3 m_vSearchOrigin;
	float m_fSearchRadiusXY;
	float m_fDistZ;
	float m_fMinimumSpacing;
	u32 m_iSearchFlags;
	TShortMinMax m_AreaMinMax;

	atArray<s32> m_PedGenLoadedMeshes;
	fwPedGenNavMeshIterator m_PedGenNavMeshIterator;

	TSpawnPosition m_SpawnCoords[ms_iMaxCoords+1];
	s32 m_iNumCoords;

	bool m_bNavMeshesHaveChanged;

	void FindSpawnCoordsBackgroundTask();
	void FindSpawnCoords_StartNewTimeSlice(const atArray<s32> & loadedMeshes);
	void FindSpawnCoords_ContinueTimeSlice();

	bool CheckSpacing(const Vector3 & vPos) const;
	bool InsertSpawnPos(const TSpawnPosition & pos);
};

#endif // PATHSERVER_PED_H

