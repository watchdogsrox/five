#ifndef DISPATCH_DATA_H_
#define DISPATCH_DATA_H_

// rage headers
#include "atl/array.h"
#include "atl/hashstring.h"
#include "parser/macros.h"

// Game headers
#include "game/Dispatch/DispatchEnums.h"
#include "Peds/rendering/PedVariationDS.h"

// Forward decleration
class CPed;

enum eLawResponseTime
{
	LAW_RESPONSE_DELAY_SLOW = 0,
	LAW_RESPONSE_DELAY_MEDIUM,
	LAW_RESPONSE_DELAY_FAST,

	NUM_LAW_RESPONSE_DELAY_TYPES
};

//Make sure these match the law enum.
enum
{
	AMBULANCE_RESPONSE_DELAY_SLOW = 0,
	AMBULANCE_RESPONSE_DELAY_MEDIUM,
	AMBULANCE_RESPONSE_DELAY_FAST,

	NUM_AMBULANCE_RESPONSE_DELAY_TYPES
};

enum
{
	FIRE_RESPONSE_DELAY_SLOW = 0,
	FIRE_RESPONSE_DELAY_MEDIUM,
	FIRE_RESPONSE_DELAY_FAST,

	NUM_FIRE_RESPONSE_DELAY_TYPES
};

enum eZoneVehicleResponseType
{
	VEHICLE_RESPONSE_DEFAULT = 0,
	VEHICLE_RESPONSE_COUNTRYSIDE,
	VEHICLE_RESPONSE_ARMY_BASE,

	NUM_VEHICLE_RESPONSE_TYPES
};

enum eGameVehicleResponseType
{
	VEHICLE_RESPONSE_GAMETYPE_SP,
	VEHICLE_RESPONSE_GAMETYPE_MP,
	VEHICLE_RESPONSE_GAMETYPE_ALL,
};

class CConditionalVehicleSet
{
public:

	eZoneVehicleResponseType GetZoneType() const { return m_ZoneType; }
	eGameVehicleResponseType GetGameType() const { return m_GameType; }
	bool IsMutuallyExclusive() const { return m_IsMutuallyExclusive; }
	const atArray<atHashWithStringNotFinal>& GetVehicleModelHashes() const { return m_VehicleModels; }
	const atArray<atHashWithStringNotFinal>& GetPedModelHashes() const { return m_PedModels; }
	const atArray<atHashWithStringNotFinal>& GetObjectModelHashes() const { return m_ObjectModels; }
	const atArray<atHashWithStringNotFinal>& GetClipSetHashes() const { return m_ClipSets; }

private:

	// This is the zone type that the vehicle can spawn in
	eZoneVehicleResponseType m_ZoneType;
	// This is the game type for the set
	eGameVehicleResponseType m_GameType;
	bool m_IsMutuallyExclusive;
	// This is an array of possible vehicle model hashes
	atArray<atHashWithStringNotFinal> m_VehicleModels;
	// This is an array of possible ped model hashes
	atArray<atHashWithStringNotFinal> m_PedModels;
	// This is an array of possible object model hashes
	atArray<atHashWithStringNotFinal> m_ObjectModels;
	// This is an array of possible clip sets
	atArray<atHashWithStringNotFinal> m_ClipSets;

	PAR_SIMPLE_PARSABLE;
};

class CVehicleSet
{
public:

    u32 GetNameHash() { return m_Name.GetHash(); }
	atArray<CConditionalVehicleSet>& GetConditionalVehicleSets() { return m_ConditionalVehicleSets; }

private:

    // This is the name of our vehicle SET, i.e. what we can reference it by when looking for it
	atHashWithStringNotFinal m_Name;
	// This is an array of possible clip sets
	atArray<CConditionalVehicleSet> m_ConditionalVehicleSets;

    PAR_SIMPLE_PARSABLE;
};

struct DispatchModelVariation
{
	DispatchModelVariation()
		: m_Component(PV_COMP_INVALID)
		, m_DrawableId(0)
		, m_MinWantedLevel(0)
		, m_Armour(0)
	{}

	~DispatchModelVariation()
	{}

	ePedVarComp		m_Component;
	u32				m_DrawableId;
	u8				m_MinWantedLevel;
	float			m_Armour;

	PAR_SIMPLE_PARSABLE;
};

struct sDispatchModelVariations
{
	// The list of vehicle sets associated with this dispatch service/response
	atArray<DispatchModelVariation> m_ModelVariations;

	PAR_SIMPLE_PARSABLE;
};

// A dispatch response/service contains information about what to spawn for a certain dispatch type
class CDispatchResponse
{
public:

    DispatchType GetType() const { return m_DispatchType; }
    s8 GetNumPedsToSpawn() const { return m_NumPedsToSpawn; }
	bool GetUseOneVehicleSet() const { return m_UseOneVehicleSet; }
    atArray<atHashWithStringNotFinal>* GetDispatchVehicleSets() { return &m_DispatchVehicleSets; }

private:

    // This is the type of dispatch (swat, police auto, police heli, fire dept, medic, etc)
    DispatchType	m_DispatchType;
    // This is the number of peds we want to spawn (in DispatchServices.cpp it's referenced as needed resources)
    s8				m_NumPedsToSpawn;
	// This tells the dispatch to only ever stream/spawn one vehicle set at a time
	bool			m_UseOneVehicleSet;
    // The list of vehicle sets associated with this dispatch service/response
    atArray<atHashWithStringNotFinal> m_DispatchVehicleSets;

    PAR_SIMPLE_PARSABLE;
};

// Wanted response if just a contained for an array of dispatch services
// A wanted response is not required to include all dispatch services (i.e. WL1 does not have SWAT)
class CWantedResponse
{
public:

	s8 CalculateNumPedsToSpawn(DispatchType dispatchType);

    // Get dispatch services for the given dispatch type. 
    CDispatchResponse* GetDispatchServices(DispatchType dispatchType);

private:

    // Our list of dispatch services
    atArray<CDispatchResponse> m_DispatchServices;

    PAR_SIMPLE_PARSABLE;
};

class CWantedResponseOverrides
{

private:

	CWantedResponseOverrides();
	~CWantedResponseOverrides();

public:

	static CWantedResponseOverrides& GetInstance();

public:

	s8		GetNumPedsToSpawn(DispatchType nDispatchType) const;
	bool	HasNumPedsToSpawn(DispatchType nDispatchType) const;
	void	Reset();
	void	ResetNumPedsToSpawn();
	void	SetNumPedsToSpawn(DispatchType nDispatchType, s8 iNumPedsToSpawn);

private:

	atRangeArray<s8, DT_Max>	m_aNumPedsToSpawn;

};

class CDispatchData
{
public:
	CDispatchData();

    // The maximum distance orders are valid
    static const u32 NUM_DISPATCH_SERVICES = DT_Max -1; // -1 because enum 0 is invalid type

    struct sDispatchServiceOrderDistances
    {
        DispatchType			m_DispatchType;
        float					m_OnFootOrderDistance;
        float					m_MinInVehicleOrderDistance;
        float					m_MaxInVehicleOrderDistance;
        PAR_SIMPLE_PARSABLE
    };

    static void Init(unsigned initMode);
	static u32  GetParoleDuration() { return m_DispatchDataInstance.m_ParoleDuration; }
	static float GetInHeliRadiusMultiplier() { return m_DispatchDataInstance.m_InHeliRadiusMultiplier; }
	static float GetImmediateDetectionRange() { return m_DispatchDataInstance.m_ImmediateDetectionRange; }
	static float GetOnScreenImmediateDetectionRange() { return m_DispatchDataInstance.m_OnScreenImmediateDetectionRange; }
	static int GetLawResponseDelayTypeFromString(char* pString);
	static int GetVehicleResponseTypeFromString(char* pString);
	static float GetMaxLawSpawnDelay(int iLawResponseDelayType);
    static float GetRandomLawSpawnDelay(int iLawResponseDelayType);
	static float GetRandomAmbulanceSpawnDelay(int iAmbulanceResponseDelayType);
	static float GetRandomFireSpawnDelay(int iFireResponseDelayType);
    static float GetOnFootOrderDistance(DispatchType eDispatchType);
    static bool GetMinMaxInVehicleOrderDistances(DispatchType eDispatchType, float& fMinDistance, float& fMaxDistance);
    static s32  GetWantedLevelThreshold(s32 iWantedLevel);
    static float GetWantedZoneRadius(s32 iWantedLevel, const CPed* pPlayerPed = NULL);
	static u32 GetHiddenEvasionTime(s32 iWantedLevel);
    // PURPOSE: Get a wanted response for a specific wanted level
    static CWantedResponse* GetWantedResponse(s32 iWantedLevel);
    // PURPOSE: Get a emergency response for a specific dispatch type
    static CDispatchResponse* GetEmergencyResponse(DispatchType eDispatchType);
    // PURPOSE: To find a vehicle set based on a hash
    static CConditionalVehicleSet* FindConditionalVehicleSet(u32 name);
	static sDispatchModelVariations* FindDispatchModelVariations(CPed& rPed);

private:

    static bool LoadWantedData();

    // The instance of this class we'll use to access data
    static CDispatchData m_DispatchDataInstance;

    // The array of vehicle sets defined in data
    atArray<CVehicleSet> m_VehicleSets;
	// Possible dispatch ped variations
	atBinaryMap<sDispatchModelVariations, atHashString> m_PedVariations;
    // The thresholds (point values) for each wanted level
    atArray<int> m_WantedLevelThresholds;
    // The radius of each wanted level in SP
    atArray<float> m_SingleplayerWantedLevelRadius;
    // The radius of each wanted level in MP
    atArray<float> m_MultiplayerWantedLevelRadius;
    // The time it takes for the WL to go away when the ped is hidden
    atArray<u32> m_HiddenEvasionTimes;
    // This response data for each wanted level, included what dispatch types to use, how many, and vehicle sets
    atArray<CWantedResponse> m_WantedResponses;
    // This response data for each emergency service, included what dispatch types to use, how many, and vehicle sets
    atArray<CDispatchResponse> m_EmergencyResponses;

    u32 m_ParoleDuration;

    // This is the min and max possible delay we use when we get a wanted level (indexed according to the LAW_RESPONSE_DELAY_* enum)
    atArray<float> m_LawSpawnDelayMin;
    atArray<float> m_LawSpawnDelayMax;

	// This is the min and max possible delay we use when spawn an ambulance 
	atArray<float> m_AmbulanceSpawnDelayMin;
	atArray<float> m_AmbulanceSpawnDelayMax;

	// This is the min and max possible delay we use when spawn an firetruck 
	atArray<float> m_FireSpawnDelayMin;
	atArray<float> m_FireSpawnDelayMax;

	// This says how much to increase the wanted radius by when the player is in a helicopter
	float m_InHeliRadiusMultiplier;

	// The default distance for police immediate detection of a crime
	float m_ImmediateDetectionRange;
	float m_OnScreenImmediateDetectionRange;

    // Array of order distances for each dispatch service
    atFixedArray<sDispatchServiceOrderDistances, NUM_DISPATCH_SERVICES> m_DispatchOrderDistances;	

	// The time it takes for the WL to go away when the ped is hidden
	atArray<u32> m_MultiplayerHiddenEvasionTimes;

    PAR_SIMPLE_PARSABLE;
};

#endif //DISPATCH_DATA_H_
