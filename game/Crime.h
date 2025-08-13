// Title	:	Crime.h
// Author	:	Gordon Speirs
// Started	:	17/04/00

#ifndef _CRIME_H_
#define _CRIME_H_

class CEntity;
class CPed;
class CVehicle;

// if you alter this enum please also modify the string list in Crime.cpp and the metadata in crimeinformation.meta
enum eCrimeType
{
	CRIME_NONE = 0,
	CRIME_POSSESSION_GUN,
	CRIME_RUN_REDLIGHT,
	CRIME_RECKLESS_DRIVING,
	CRIME_SPEEDING,
	CRIME_DRIVE_AGAINST_TRAFFIC,
	CRIME_RIDING_BIKE_WITHOUT_HELMET,
	CRIME_LAST_MINOR_CRIME = CRIME_RIDING_BIKE_WITHOUT_HELMET,		// Minor crimes will not automatically make the wanted level go up but it will start a parole instead. If the player does anything wrong doing the parole period he will get a wanted level.
	
	CRIME_STEAL_VEHICLE,
	CRIME_STEAL_CAR,
	CRIME_LAST_ONE_NO_REFOCUS = CRIME_STEAL_CAR,					// Some crimes will not refocus the search area.

	CRIME_BLOCK_POLICE_CAR,
	CRIME_STAND_ON_POLICE_CAR,
	CRIME_HIT_PED,
	CRIME_HIT_COP,
	CRIME_SHOOT_PED,
	CRIME_SHOOT_COP,
	CRIME_RUNOVER_PED,
	CRIME_RUNOVER_COP,
	CRIME_DESTROY_HELI,
	CRIME_PED_SET_ON_FIRE,
	CRIME_COP_SET_ON_FIRE,
	CRIME_CAR_SET_ON_FIRE,
	CRIME_DESTROY_PLANE,
	CRIME_CAUSE_EXPLOSION,
	CRIME_STAB_PED,
	CRIME_STAB_COP,
	CRIME_DESTROY_VEHICLE,
	CRIME_DAMAGE_TO_PROPERTY,
	CRIME_TARGET_COP,
	CRIME_FIREARM_DISCHARGE,
	CRIME_RESIST_ARREST,
	CRIME_MOLOTOV,
	CRIME_SHOOT_NONLETHAL_PED,
	CRIME_SHOOT_NONLETHAL_COP,
	CRIME_KILL_COP,
	CRIME_SHOOT_AT_COP,
	CRIME_SHOOT_VEHICLE,
	CRIME_TERRORIST_ACTIVITY,
	CRIME_HASSLE,
	CRIME_THROW_GRENADE,
	CRIME_VEHICLE_EXPLOSION,
	CRIME_KILL_PED,
	CRIME_STEALTH_KILL_COP,
	CRIME_SUICIDE,
	CRIME_DISTURBANCE,
	CRIME_CIVILIAN_NEEDS_ASSISTANCE,
	CRIME_STEALTH_KILL_PED,
	CRIME_SHOOT_PED_SUPPRESSED,
	CRIME_JACK_DEAD_PED,
	CRIME_CHAIN_EXPLOSION,
	MAX_CRIMES
};

// used by the police scanner - must be in sync with the enum above
extern const u32 g_CrimeNamePHashes[MAX_CRIMES];

// used by crime information -- must be in sync with the enum above
extern const u32 g_CrimeNameHashes[MAX_CRIMES];

class CCrime
{
public:

	enum eReportCrimeMethod
	{
		REPORT_CRIME_DEFAULT,
		REPORT_CRIME_ARCADE_CNC
	};

	static eReportCrimeMethod sm_eReportCrimeMethod;

	static void ReportCrime(eCrimeType CrimeType, CEntity* pVictim, CPed* pCommitedby, u32 uTime = MAX_UINT32);
	static void ReportDestroyVehicle(CVehicle* pVehicle, CPed* pCommitedby);
	static bool ShouldIgnoreVehicleCrime(eCrimeType CrimeType, CEntity* pVictim, CPed* pCommitedby);
};







#endif
