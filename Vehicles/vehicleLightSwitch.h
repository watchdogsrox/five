//
// filename:	vehicleLightSwitch.h
// description:	Light dimmer controller for vehicle emissive bits
//

#ifndef INC_VEHICLELIGHTSWITCH_H_
#define INC_VEHICLELIGHTSWITCH_H_

// --- Include Files ------------------------------------------------------------
#include "renderer/HierarchyIds.h"
// --- Rage Forward Declarations ------------------------------------------------

namespace rage
{
	class fragType;
}

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------
class CVehicle;
class CVehicleModelInfo;

class CVehicleLightSwitch
{
public:
	CVehicleLightSwitch(): m_pParentVehicle(NULL) {}
	~CVehicleLightSwitch() {}

	void Init(CVehicle* pVehicle);
	void Shutdown() { /*NoOp*/ } 

	// --- Emissive Light ----------------------------------------
	enum LightId { 
			LW_DEFAULT = 0,

			LW_HEADLIGHT_L,	
			LW_HEADLIGHT_R,	
			LW_TAILLIGHT_L,	
			LW_TAILLIGHT_R,	
			LW_INDICATOR_FL,
			LW_INDICATOR_FR,
			LW_INDICATOR_RL,
			LW_INDICATOR_RR,
			LW_BRAKELIGHT_L,
			LW_BRAKELIGHT_R,
			LW_BRAKELIGHT_M,
			LW_REVERSINGLIGHT_L,
			LW_REVERSINGLIGHT_R,
			LW_EXTRALIGHT_1,
			LW_EXTRALIGHT_2,
			LW_EXTRALIGHT_3,
			LW_EXTRALIGHT_4,
			LW_EXTRALIGHT,
			
			LW_SIRENLIGHT,			// 19: special case for vehicle_ligtsemissive_siren shader (see BS#6131299 "Emissive values for sirens in visualsettings.dat");

			LW_LIGHTCOUNT,
			LW_LIGHTVECTORS = (LW_LIGHTCOUNT+3)/4
			};

	void LightRemapFixup();

public:
	// --- Static -----------------------------------------------

private:
	// --- General ----------------------------------------------
	CVehicle *m_pParentVehicle;
};

// !!! To use with bone id BEFORE translation in the vehicle skeleton !!!
inline CVehicleLightSwitch::LightId BONEID_TO_LIGHTID(int boneId)
{
	switch(boneId)
	{
	case VEH_HEADLIGHT_L: return CVehicleLightSwitch::LW_HEADLIGHT_L;
	case VEH_HEADLIGHT_R: return CVehicleLightSwitch::LW_HEADLIGHT_R;
	case VEH_TAILLIGHT_L: return CVehicleLightSwitch::LW_TAILLIGHT_L;
	case VEH_TAILLIGHT_R: return CVehicleLightSwitch::LW_TAILLIGHT_R;
	case VEH_INDICATOR_LF: return CVehicleLightSwitch::LW_INDICATOR_FL;
	case VEH_INDICATOR_RF: return CVehicleLightSwitch::LW_INDICATOR_FR;
	case VEH_INDICATOR_LR: return CVehicleLightSwitch::LW_INDICATOR_RL;
	case VEH_INDICATOR_RR: return CVehicleLightSwitch::LW_INDICATOR_RR;
	case VEH_BRAKELIGHT_L: return CVehicleLightSwitch::LW_BRAKELIGHT_L;
	case VEH_BRAKELIGHT_R: return CVehicleLightSwitch::LW_BRAKELIGHT_R;
	case VEH_BRAKELIGHT_M: return CVehicleLightSwitch::LW_BRAKELIGHT_M;
	case VEH_REVERSINGLIGHT_L: return CVehicleLightSwitch::LW_REVERSINGLIGHT_L;
	case VEH_REVERSINGLIGHT_R: return CVehicleLightSwitch::LW_REVERSINGLIGHT_R;
	case VEH_EXTRALIGHT_1: return CVehicleLightSwitch::LW_EXTRALIGHT_1;
	case VEH_EXTRALIGHT_2: return CVehicleLightSwitch::LW_EXTRALIGHT_2;
	case VEH_EXTRALIGHT_3: return CVehicleLightSwitch::LW_EXTRALIGHT_3;
	case VEH_EXTRALIGHT_4: return CVehicleLightSwitch::LW_EXTRALIGHT_4;
	default: return CVehicleLightSwitch::LW_LIGHTCOUNT;		// invalid index
	}
}

inline int LIGHTID_TO_BONEID(CVehicleLightSwitch::LightId lightId)
{
	switch(lightId)
	{
	case CVehicleLightSwitch::LW_HEADLIGHT_L: return VEH_HEADLIGHT_L;
	case CVehicleLightSwitch::LW_HEADLIGHT_R: return VEH_HEADLIGHT_R;
	case CVehicleLightSwitch::LW_TAILLIGHT_L: return VEH_TAILLIGHT_L;
	case CVehicleLightSwitch::LW_TAILLIGHT_R: return VEH_TAILLIGHT_R;
	case CVehicleLightSwitch::LW_INDICATOR_FL: return VEH_INDICATOR_LF;
	case CVehicleLightSwitch::LW_INDICATOR_FR: return VEH_INDICATOR_RF;
	case CVehicleLightSwitch::LW_INDICATOR_RL: return VEH_INDICATOR_LR;
	case CVehicleLightSwitch::LW_INDICATOR_RR: return VEH_INDICATOR_RR;
	case CVehicleLightSwitch::LW_BRAKELIGHT_L: return VEH_BRAKELIGHT_L;
	case CVehicleLightSwitch::LW_BRAKELIGHT_R: return VEH_BRAKELIGHT_R;
	case CVehicleLightSwitch::LW_BRAKELIGHT_M: return VEH_BRAKELIGHT_M;
	case CVehicleLightSwitch::LW_REVERSINGLIGHT_L: return VEH_REVERSINGLIGHT_L;
	case CVehicleLightSwitch::LW_REVERSINGLIGHT_R: return VEH_REVERSINGLIGHT_R;
	case CVehicleLightSwitch::LW_EXTRALIGHT_1: return VEH_EXTRALIGHT_1;
	case CVehicleLightSwitch::LW_EXTRALIGHT_2: return VEH_EXTRALIGHT_2;
	case CVehicleLightSwitch::LW_EXTRALIGHT_3: return VEH_EXTRALIGHT_3;
	case CVehicleLightSwitch::LW_EXTRALIGHT_4: return VEH_EXTRALIGHT_4;
	default: return -1;
	}
}

#endif // !INC_VEHICLELIGHTSWITCH_H_
