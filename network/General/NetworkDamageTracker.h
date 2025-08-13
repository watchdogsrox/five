//
// name:        NetworkDamageTracker.h
// description: Keeps track of damage dealt to a network object by participating players
// written by:  Daniel Yelland
//

#ifndef NETWORK_DAMAGE_TRACKER_H
#define NETWORK_DAMAGE_TRACKER_H

#include "network/NetworkInterface.h"
#include "fwtl/pool.h"

// Game headers
#include "peds/pedtype.h"
#include "peds/pedDefines.h"
#include "network/Objects/Entities/NetObjPhysical.h"


#define MAX_NUM_DAMAGE_TRACKERS 100

//PEDTYPE_LAST_PEDTYPE + 1 an additional element is added to the arrays for tracking "All Peds"
#define ALL_KILL_PED_TYPES				(PEDTYPE_LAST_PEDTYPE)
#define NUM_KILL_PED_CLASSIFICATIONS	(ALL_KILL_PED_TYPES + 1)

namespace rage
{
    class netObject;
    class netPlayer;
}

class CNetworkDamageTracker
{
public:

    FW_REGISTER_CLASS_POOL(CNetworkDamageTracker);

     CNetworkDamageTracker(netObject *networkObject);
    ~CNetworkDamageTracker();

    void  AddDamageDealt  (const netPlayer& player, float damage);
    float GetDamageDealt  (const netPlayer& playe);
    void  ResetDamageDealt(const netPlayer& playe);
	void  ResetAll();

#if __BANK
    static void AddDebugWidgets();
#endif // __BANK

private:

    CNetworkDamageTracker();
    CNetworkDamageTracker(const CNetworkDamageTracker &);

    CNetworkDamageTracker &operator=(const CNetworkDamageTracker &);

    netObject *m_networkObject; // network object to track damage for
    float           m_damageDealt[MAX_NUM_PHYSICAL_PLAYERS];
};

class CNetworkKillTracker
{
public:

    CNetworkKillTracker();
    ~CNetworkKillTracker();

    static void Init();

    static void RegisterKill(u32 modelIndex, u32 weaponTypeHash, bool awardRankPoints);

    // kill tracking for one-off missions
    static bool StartTracking(u32 modelIndex, u32 weaponTypeHash = 0);
    static bool StopTracking(u32 modelIndex, u32 weaponTypeHash = 0);
    static bool IsKillTracking(u32 modelIndex, u32 weaponTypeHash = 0);
    static u32 GetTrackingResults(u32 modelIndex, u32 weaponTypeHash = 0);
    static void ResetTrackingResults(u32 modelIndex, u32 weaponTypeHash = 0);

    // kill tracking for rank points - persistent kill tracking for entire session
    static bool RegisterModelIndexForRankPoints(u32 modelIndex);
    static bool IsTrackingForRankPoints(u32 modelIndex);
    static u32 GetNumKillsForRankPoints(u32 modelIndex);
    static void ResetNumKillsForRankPoints(u32 modelIndex);

    // kill tracking for civilians
    static void StartTrackingCivilianKills();
    static void StopTrackingCivilianKills();
    static bool IsTrackingCivilianKills();
    static u32 GetNumCivilianKills();
    static void ResetNumCivilianKills();

	//Kill tracking for specified type of ped 
	static void StartTrackingPedKills(s32 pedClassification);
	static void StopTrackingPedKills(s32 pedClassification);
	static bool IsTrackingPedKills(s32 pedClassification);
	static u32 GetNumPedKills(s32 pedClassification);
	static void ResetNumPedKills(s32 pedClassification);

    bool IsValid();
    bool IsTracking(s32 modelIndex, u32 weaponTypeHash = 0);

    void   IncrementNumKills();
    void   Reset();
    u32 GetNumKills();

#if __BANK && __DEV
    static void DisplayDebugInfo();
#endif // __BANK && __DEV

private:

    static const unsigned int MAX_NUM_KILL_TRACKERS = 10;
    static const unsigned int MAX_NUM_KILL_TRACKERS_FOR_RANK_POINTS = 25;
    static const unsigned int KILL_TRACKER_FOR_NPC_RANK_POINTS = 0;
    static const unsigned int KILL_TRACKER_FOR_MISSION_RANK_POINTS_START = 1;

    strLocalIndex  m_modelIndex;     // model index to track
    u32 m_weaponTypeHash; // weapon type to track
    u32 m_numKills;       // number of entities with the specified model index killed by the specified weapon type

	static bool ms_killTrackCivilians;
	static u32  ms_numCiviliansKilled;
	
	// Track how many Peds of each type have been killed - 
	static atFixedArray<u32, NUM_KILL_PED_CLASSIFICATIONS> sm_NumPedsKilledOfThisTypeArray;
	static atFixedArray <bool, NUM_KILL_PED_CLASSIFICATIONS> sm_IsKillTrackingPedsOfThisTypeArray;


    static CNetworkKillTracker ms_killTrackers[MAX_NUM_KILL_TRACKERS];
    static CNetworkKillTracker ms_killTrackersForRankPoints[MAX_NUM_KILL_TRACKERS_FOR_RANK_POINTS];
};

class CNetworkHeadShotTracker
{
public:

    CNetworkHeadShotTracker();
    ~CNetworkHeadShotTracker();

    static void Init();

    static void RegisterHeadShot(u32 modelIndex, u32 weaponTypeHash );

    // kill tracking for one-off missions
    static bool StartTracking(s32 modelIndex, u32 weaponTypeHash = 0);
    static bool StopTracking(s32 modelIndex, u32 weaponTypeHash = 0);
    static bool IsHeadShotTracking(s32 modelIndex, u32 weaponTypeHash = 0);
    static u32 GetTrackingResults(s32 modelIndex, u32 weaponTypeHash = 0);
    static void ResetTrackingResults(s32 modelIndex, u32 weaponTypeHash = 0);

    bool IsValid();
    bool IsTracking(s32 modelIndex, u32 weaponTypeHash = 0);

    void   IncrementNumHeadShots();
    void   Reset();
    u32	   GetNumHeadShots();

#if __BANK && __DEV
    static void DisplayDebugInfo();
#endif // __BANK && __DEV

private:

    static const unsigned int MAX_NUM_HEADSHOT_TRACKERS = 10;

    strLocalIndex m_modelIndex;		// model index to track
    u32 m_weaponTypeHash;	// weapon type to track
	u32 m_numHeadShots;		// number of entities with the specified model index killed by the specified weapon type by headshot
	
	// Script have specifically requested to have headshots be independant from kills....
	static CNetworkHeadShotTracker ms_headShotTrackers[MAX_NUM_HEADSHOT_TRACKERS];
};


//Used by the scripts to determine which body parts of the player were damaged.
class CNetworkBodyDamageTracker
{
public:

	//We track the information for the last 10 shots
	static const unsigned NUM_SHOTS_TRACKED = 10;

	//Used to signal damage for components that kill the player but arent set, ie, 
	static const int COMPONENT_NOT_SET  = RAGDOLL_NUM_COMPONENTS;
	static const int MAX_NUM_COMPONENTS = COMPONENT_NOT_SET+1;

public:
	struct ShotInfo
	{
	public:
		rlGamerHandle  m_Player;
		u32            m_DamageComponents;
		int            m_Fatalcomponent;

		ShotInfo() : m_DamageComponents(0), m_Fatalcomponent(-1)
		{
		}

		void Reset()
		{
			m_Player.Clear();
			m_DamageComponents = 0;
			m_Fatalcomponent   = -1;
		}

		bool GetComponentHit(const int component);
	};

	struct ShotHistory
	{
	public:
		rlGamerHandle  m_Player;
		int            m_ComponentHit;
		bool           m_WasFatal;

	public:
		ShotHistory() : m_ComponentHit(-1), m_WasFatal(false) { }

		void Reset()
		{
			m_ComponentHit = -1;
			m_WasFatal     = false;
			m_Player.Clear();
		}

		bool IsValid()  const { return (-1 < m_ComponentHit); }
		bool IsPlayer() const { return (m_Player.IsValid()); }
		bool IsAiPed()  const { return (!m_Player.IsValid()); }
	};

public:
	CNetworkBodyDamageTracker();
	~CNetworkBodyDamageTracker();

	static void  Reset();
	static void  SetComponent(const CNetObjPhysical::NetworkDamageInfo& damageInfo);
	static void  UnSetComponent(const int component);
	static bool  GetComponentHit(const int component);
	static u32   GetNumComponents();
	static int   GetFatalComponent();
	static void  GetFatalShotHistory(ShotInfo& info);

private:
	static u32  m_NumComponentsHit;
	static u32  m_DamageComponents;
	static u32  m_HistoryIndex;
	static ShotHistory m_ShotsHistory[NUM_SHOTS_TRACKED];
};

#endif  // #NETWORK_DAMAGE_TRACKER_H
