// Title	:	DispatchMangaer.h
// Purpose	:	Manages the separate dispatch subsystems
// Started	:	13/05/2010

#ifndef INC_DISPATCH_MANAGER_H_
#define INC_DISPATCH_MANAGER_H_

// rage includes
#include "atl/array.h"
#include "vector/color32.h"

// Game includes
#include "game/Dispatch/DispatchEnums.h"
#include "game/Wanted.h"

class CIncident;
class CDispatchService;
class CPoliceAutomobileDispatch;

class CDispatchManager
{
public:
	CDispatchManager();
	~CDispatchManager();

	void InitSession();
	void ShutdownSession();

	static CDispatchManager& GetInstance() { return ms_instance; }
	static bool ShouldReleaseStreamingRequests() { return m_bShouldReleaseStreamRequests; }
	void Update();
	void Flush();

	CDispatchService* GetPoliceAutomobileDispatch() { return m_apDispatch[DT_PoliceAutomobile]; }
	CDispatchService* GetPoliceVehicleRequestDispatch() { return m_apDispatch[DT_PoliceVehicleRequest]; }
#if __BANK
	void InitWidgets();
	static void AddFireIncident();
	static void AddWantedIncident();
	static void AddScriptIncident();
	static void AddInjuryIncident();
	static bool DebugEnabled(const DispatchType dispatchType);
	void DebugRender();
	void RenderResourceUsageForIncident(s32& iNoTexts, Color32 debugColor, CIncident* pIncident);
	static s32 m_DebugWantedLevel;
	static bool  m_RenderResourceDebug;
	float m_IncidentTime;
	bool m_bDispatchToPlayerEntity;

#endif
	
	// Enables or disables the dispatch service passed
	void BlockDispatchResourceCreation(const DispatchType dispatchType, const bool bBlock);
	void EnableDispatch(const DispatchType dispatchType, const bool bEnable);
	void BlockAllDispatchResourceCreation(const bool bBlock);
	void EnableAllDispatch(const bool bEnable);
	bool IsDispatchEnabled(const DispatchType dispatchType) const;
	bool IsDispatchResourceCreationBlocked(const DispatchType dispatchType) const;

	// We use this to globally delay the police spawning (return the max delay for the desired type)
	float DelayLawSpawn();

			CDispatchService*	GetDispatch(DispatchType nType)			{ Assert(nType > DT_Invalid && nType < DT_Max); return m_apDispatch[nType]; }
	const	CDispatchService*	GetDispatch(DispatchType nType) const	{ Assert(nType > DT_Invalid && nType < DT_Max); return m_apDispatch[nType]; }
	
	static const char* GetDispatchTypeName(DispatchType type) { return ms_DispatchTypeNames[type]; }

	static void SetLawResponseDelayOverride(float fVal) { m_fLawResponseDelayOverride = fVal; }

	void GetNetworkReservations(u32& reservedPeds, u32& reservedVehicles, u32& reservedObjects, u32& createdPeds, u32& createdVehicles, u32& createdObjects);
	void LogNetworkReservations(netLoggingInterface& log);

	static u32 GetNetworkPopulationReservationId() { return m_netPopulationReservationId;}

	static bool ms_SpawnedResourcesThisFrame;

private:
	void AddDispatchService(CDispatchService* pDispatch);
	void UpdateNetworkReservations();

	CDispatchService* m_apDispatch[DT_Max];

	static float m_fLawResponseDelayOverride;	// Used by script to override the law response delay, will use this one time then reset
	static int	m_iCurrZoneVehicleResponseType; // The type of vehicle response our current zone has, i.e. country, default, etc
	static eWantedLevel m_nPlayerWantedLevel;	// The wanted level for the local player
	static bool	m_bShouldReleaseStreamRequests; // Should we release our streaming requests? Used when we want to refresh the vehicle sets being used

	static const char* ms_DispatchTypeNames[DT_Max];

	static CDispatchManager ms_instance;
	
	static float ms_fUpdateTimeStep; // Stored timestep for Update() method - keeps track of how long since last update

	static u32 m_netPopulationReservationId;	// The reservation id given by the network object population manager when we are trying to make space in the ambient population for the dispatch entities
	static u32 m_numNetPedsReserved;			// The number of peds to reserved in the network object population manager
	static u32 m_numNetVehiclesReserved;		// The number of vehicles to reserved in the network object population manager

	static u32 m_networkReservationCounter;		// used to periodically update the network reservations
};

#endif
