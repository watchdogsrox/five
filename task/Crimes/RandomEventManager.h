#ifndef INC_RANDOM_CRIMES_MANAGER_H
#define INC_RANDOM_CRIMES_MANAGER_H

// Rage headers
#include "atl/array.h"
#include "atl/hashstring.h"
#include "atl/singleton.h"
#include "atl/string.h"
#include "fwanimation/animdefines.h"
#include "fwtl/pool.h"
#include "fwutil/rtti.h"
#include "parser/macros.h"
#include "streaming/streamingrequest.h"
#include "vector/quaternion.h"
#include "vector/vector3.h"

// Required headers
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

// Forward declarations
class CPed;
class CTask;
class CVehicle;
class CDefaultCrimeInfo;

////////////////////////////////////////////////////////////////////////////////
// eCrimes
////////////////////////////////////////////////////////////////////////////////

//If you add to this then make sure you update the enum in commands_misc.sch
enum eRandomEvent
{
	RC_INVALID = -1,
	RC_PED_STEAL_VEHICLE = 0,
	RC_PED_JAY_WALK_LIGHTS,
	RC_COP_PURSUE,
	RC_COP_PURSUE_VEHICLE_FLEE_SPAWNED,
	RC_COP_VEHICLE_DRIVING_FAST,
	RC_DRIVER_SLOW,
	RC_DRIVER_RECKLESS,
	RC_DRIVER_PRO,
	RC_PED_PURSUE_WHEN_HIT_BY_CAR,
	RC_MAX
};

//Note: If you update this enum, please update the metadata declaration in game\task\crimes\metadata.psc.
enum eRandomEventType
{
	ET_INVALID = -1,
	ET_CRIME = 0,
	ET_JAYWALKING,
	ET_COP_PURSUIT,
	ET_SPAWNED_COP_PURSUIT,
	ET_AMBIENT_COP,
	ET_INTERESTING_DRIVER,
	ET_AGGRESSIVE_DRIVER,
	ET_MAX
};

//////////////////////////////////////////////////////////////////////////
// CRandomEventManager
//////////////////////////////////////////////////////////////////////////

class CRandomEventManager
{
public:

	//Data stored for each eRandomEventType
	struct RandomEventType
	{
		RandomEventType()
			: m_RandomEventTypeName()
			, m_RandomEventTimer(0)
			, m_RandomEventTimeIntervalMin(0)
			, m_RandomEventTimeIntervalMax(0)
			, m_DeltaScaleWhenPlayerStationary(100.0f)
		{}

		~RandomEventType()
		{}

		atHashString		m_RandomEventTypeName;				//Name
		float				m_RandomEventTimer;					//Timer to control how often the event will happen  - NOT PARGEN
		float				m_RandomEventTimeIntervalMin;		//Time between events min
		float				m_RandomEventTimeIntervalMax;		//Time between events max
		float				m_DeltaScaleWhenPlayerStationary;	//Scale timer dt when the player is stationary

		PAR_SIMPLE_PARSABLE;
	};

	//Data stored for each eRandomEvent
	struct RandomEventData
	{
		RandomEventData()
			: m_RandomEventName()
			, m_RandomEventType(ET_INVALID)
			, m_RandomEventHistory(0)
			, m_RandomEventPossibility(true)
			, m_Enabled(true)
		{}

		~RandomEventData()
		{}

		atHashString	 m_RandomEventName;			//Name
		eRandomEventType m_RandomEventType;			//Event type
		float			 m_RandomEventHistory;		//How many times the event has happened. - NOT PARGEN
		bool			 m_RandomEventPossibility;	//Is the current event possible - NOT PARGEN
		bool			 m_Enabled;					//Is the current event enabled- reset flag!!!! - NOT PARGEN

		PAR_SIMPLE_PARSABLE;
	};

	struct Tunables : CTuning
	{
		Tunables();

		// Distance 2d text debug on screen
		bool m_RenderDebug;
		bool m_Enabled;							// disable crime		
		bool m_ForceCrime;						// Ignore crime flags
		float m_EventInterval;					// Time between crimes		
		float m_EventInitInterval;				// Time the timer starts at when manager is init	
		bool m_SpawningChasesEnabled;			// Are we allow to spawn police vehicle chases
		int	 m_MaxNumberCopVehiclesInChase;		// Number of vehicles to spawn with CreatePoliceChase()
		int  m_ProbSpawnHeli;
		int	 m_MaxAmbientVehiclesToSpawnChase;  // Max number of ambient vehicles in the world to spawn a cop chase.
		int  m_MinPlayerMoveDistanceToSpawnChase; // The min distance the player has to move before spawning another chase.

		//Vehicle and ped spawn hashs
		atHashWithStringNotFinal m_HeliVehicleModelId;
		atHashWithStringNotFinal m_HeliPedModelId;

		atArray<RandomEventType> m_RandomEventType;	//Random event type
		atArray<RandomEventData> m_RandomEventData;	//Random event data

		PAR_PARSABLE;
	};

	CRandomEventManager();
	~CRandomEventManager();

	// Initialisation
	static void Init(unsigned initMode);

	// Shutdown
	static void Shutdown(unsigned shutdownMode);

	// Gets the CrimeInfo from the metadata
	static CDefaultCrimeInfo* GetCrimeInfo(s32 crimeType);

	//Update
	static void Update();

	// Can update the timers for each event
	static bool CanTickTimers(RandomEventType iEventType, float& fScaleTimer);

	//Can a crime of this type be committed
	bool CanCreateEventOfType(eRandomEvent aType, bool bIgnoreGlobalTimer = false, bool bIgnoreIndividualTimer = false);
	
	//Let the manager know a crime has been started
	void SetEventStarted( eRandomEvent aType );

	static void Enable( bool enable ) { sm_Tunables.m_Enabled = enable; }

	static CRandomEventManager& GetInstance() { return ms_Instance; }
	
	void ResetPossibilities();

	void UpdatePossibilities();

	float GetRandomEventTimerInterval(eRandomEventType eventType);
	float& GetRandomEventTimer(eRandomEventType eventType) const;
	eRandomEventType GetRandomEventType(s32 index) const;

	void SetRandomEventPossibility(s32 index, bool bPossible);
	bool& GetRandomEventPossibility(s32 index) const;

	void ResetRandomEvent(s32 index);
	void SetRandomEventHistory(s32 index, float history);
	float& GetRandomEventHistory(s32 index) const;

	void SetEventTypeEnabled(s32 index, bool var);
	bool GetEventTypeEnabled(s32 index) const;

	void ResetGlobalTimerTicking();

	static Tunables& GetTunables() { return sm_Tunables; }

	static bool CanCreatePoliceChase();
	static void CreatePoliceChase();

	static void FlushCopAsserts();

	//Debug
#if __BANK

	static void UpdatePossibilitiesCB( CallbackData cbdata);

	static void SetForceCrime();

	static void DebugRender();

	static void EnableDebugMode();

	void AddWidgets(bkBank& bank);

	const char * GetEventName(eRandomEvent aEvent) const;
	const char * GetEventTypeName(eRandomEventType aEventType) const;
#endif // __BANK

	//Debug mode
	static bool m_DebugMode;

	//Global event timer
	static float m_GlobalTimer;
	static bool m_GlobalTimerTicking;

	static float m_SpawnPoliceChaseTimer;
	static bool m_bSpawnHeli;

	static Vector3 m_LastPoliceChasePlayerLocation;

private:

	static bool SetUpCopPedsInVehicle(CVehicle* pVehicle, bool bCanDoDriveBys);
	
	static void SetUpCopPursuitTasks(CVehicle* pVehicle, CVehicle* pCriminalVehicle, bool bLeader);
	
	static strRequestArray<4> m_streamingRequests;

	// Tunables
	static Tunables	sm_Tunables;

	// Static manager object
	static CRandomEventManager ms_Instance;
};

////////////////////////////////////////////////////////////////////////////////
// eDefaultCrimeType
////////////////////////////////////////////////////////////////////////////////

enum eDefaultCrimeType
{
    DCT_Invalid = -1,
    DCT_Jacking = 0
};


////////////////////////////////////////////////////////////////////////////////
// CDefaultCrimeInfo
////////////////////////////////////////////////////////////////////////////////

class CDefaultCrimeInfo
{
    DECLARE_RTTI_BASE_CLASS(CDefaultCrimeInfo);
public:
    FW_REGISTER_CLASS_POOL(CDefaultCrimeInfo);

    // Construction
    CDefaultCrimeInfo()
		: m_timeBetweenCrimes(0)
    {
    }

    // Destruction
    virtual ~CDefaultCrimeInfo(){};

	// Crime hash
	u32 GetHash() const { return m_Name.GetHash(); }

#if !__FINAL
		// Crime name
		const char* GetName() const { return m_Name.GetCStr(); }

#endif // !__FINAL
    
    // Get the crime type
    //CDefaultCrimeType	GetType() const { return m_defaultCrime; }

    // Is this crime available 
	//virtual bool        IsApplicable( CPed* UNUSED_PARAM(pPed) ) { return false; }

    // Get the primary task to be used
    //CTask*      GetPrimaryTask(CVehicle*UNUSED_PARAM(pVehicle)) { return NULL; }

private:

    //
    // Members
    //

    // The hash of the name
    atHashWithStringNotFinal m_Name;

    //
    // Creation data
    //

    // Crime category 
    //u32 m_defaultCrime;

    // Allow crime timer
    float        m_timeBetweenCrimes;

    PAR_PARSABLE;
};


////////////////////////////////////////////////////////////////////////////////
// CCrimeInfoManager
////////////////////////////////////////////////////////////////////////////////

class CCrimeInfoManager : public datBase
{
public:

	CCrimeInfoManager();
	~CCrimeInfoManager();

	// Access a scenario info
	const CDefaultCrimeInfo* GetCrimeInfo(atHashWithStringNotFinal hash) const
	{
		for(s32 i = 0; i < m_Crimes.GetCount(); i++)
		{
			if(m_Crimes[i]->GetHash() == hash.GetHash())
			{
				return m_Crimes[i];
			}
		}
		return NULL;

	}
	// Access a scenario info
	CDefaultCrimeInfo* GetCrimeInfoByIndex(s32 iIndex)
	{
		if(iIndex != DCT_Invalid)
		{
			if(Verifyf(iIndex >= 0 && iIndex < m_Crimes.GetCount(), "Index [%d] out of range [0..%d]", iIndex, m_Crimes.GetCount()))
			{
				return m_Crimes[iIndex];
			}
		}
		return NULL;
	}

	// Get the index from the hash
	s32 GetCrimeIndex(atHashWithStringNotFinal hash) const
	{
		for(s32 i = 0; i < m_Crimes.GetCount(); i++)
		{
			if(m_Crimes[i]->GetHash() == hash.GetHash())
			{
				return i;
			}
		}
		Assertf(0, "CrimeInfo with hash [%s][%d] not found", hash.GetCStr(), hash.GetHash());
		return -1;
	}

#if __BANK
	// Add widgets
	void AddWidgets(bkBank& bank);
#endif // __BANK

private:

	// Delete the data
	void Reset();

	// Load the data
	void Load();

#if __BANK
	// Save the data
	void Save();

	// Add widgets
	void AddParserWidgets(bkBank* pBank);
#endif // __BANK

	//
	// Members
	//

	// The scenario info storage
	atArray<CDefaultCrimeInfo*> m_Crimes;

	PAR_SIMPLE_PARSABLE;
};

typedef atSingleton<CCrimeInfoManager> CCrimeInfoManagerSingleton;
#define CRIMEINFOMGR CCrimeInfoManagerSingleton::InstanceRef()
#define INIT_CRIMEINFOMGR											\
	do {																\
	if(!CCrimeInfoManagerSingleton::IsInstantiated()) {			\
	CCrimeInfoManagerSingleton::Instantiate();				\
	}																\
	} while(0)															\
	//END
#define SHUTDOWN_CRIMEINFOMGR	 CCrimeInfoManagerSingleton::Destroy()


#endif // INC_RANDOM_CRIMES_MANAGER_H