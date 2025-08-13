#ifndef INC_SCENARIO_TYPES_H
#define INC_SCENARIO_TYPES_H

// Game headers
#include "audioengine/widgets.h"

// Constant definitions for code that needs to reference scenarios directly by name
static const u32 SCENARIOTYPE_WORLD_HUMAN_HANGOUT_STREET = ATSTRINGHASH("WORLD_HUMAN_HANG_OUT_STREET", 0x0b5ef66f7);

typedef enum
{
	ST_Invalid = -1,
	ST_Stationary = 0,
	ST_MoveBetween,
	ST_Group,
	ST_SeatedGroup,
	ST_Seated,
	ST_Wandering,
	ST_Jogging,
	ST_Sniper,
	ST_Skiing,
	ST_Control,
	ST_Couple
} ScenarioTaskType;

typedef enum
{
	PT_ParkedOnStreet,
	PT_ParkedAtPetrolPumps,
	//PT_ParkedOutsideMechanics
} ParkedType;

enum eVehicleScenarioType
{
	VSCENARIO_TYPE_UNKNOWN = 0,
	VSCENARIO_TYPE_BROKEN_DOWN_VEHICLE,
	VSCENARIO_TYPE_LOOKING_IN_BOOT,
	VSCENARIO_TYPE_DELIVERY_DRIVER,
	VSCENARIO_TYPE_SMOKE_THEN_DRIVE_AWAY,
	VSCENARIO_TYPE_LEANING,
	NUM_EVSCENARIO,
	EVSCENARIO_MAX = NUM_EVSCENARIO,
};

// This enum has a corresponding definition in ScenarioTypes.psc
enum eLookAtImportance 
{
	//Ordered this way so default value is Medium
	LookAtImportanceMedium,
	LookAtImportanceHigh,
	LookAtImportanceLow
};

enum eVehicleScenarioPedLayoutOrigin
{
	// Note: keep up to date with parser declaration in 'ScenarioInfo.psc'.
	kLayoutOriginVehicle,
	kLayoutOriginVehicleFront,
	kLayoutOriginVehicleBack
};

#define Scenario_Invalid -1

#endif	// INC_SCENARIO_TYPES_H
