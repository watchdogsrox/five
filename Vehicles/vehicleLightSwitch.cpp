//
// filename:	VehicleLightSwitch.cpp
// description: Light dimmer controller for vehicle emissive bits
//

// C headers

// Rage headers
#include "grcore\vertexbuffer.h"
#include "grcore\vertexbuffereditor.h"
#include "grmodel\geometry.h"

// Microsoft headers

// Rage headers

// Game headers
#include "modelInfo\vehicleModelInfo.h"
#include "vehicles\vehicle.h"
#include "vehicles\vehicleLightSwitch.h"
#include "shaders\CustomShaderEffectVehicle.h"


// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------
void CVehicleLightSwitch::Init(CVehicle* pVehicle)
{
	m_pParentVehicle = pVehicle;
}

void CVehicleLightSwitch::LightRemapFixup()
{
	const CVehicleModelInfo* pModelInfo = m_pParentVehicle->GetVehicleModelInfo();
	Assert(true == pModelInfo->HasLightRemap());

	const bool gotTail = pModelInfo->HasTailLight();
	const bool gotBrake = pModelInfo->HasBrakeLight();
	const bool gotIndicator = pModelInfo->HasIndicatorLight();

	float newIntensityL = -10.0f;
	float newIntensityR = -10.0f;

	CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(m_pParentVehicle->GetDrawHandler().GetShaderEffect());
	Assert(pShader);

	// something's missing, we need to remap
	// Select where to remap
	if( false == gotTail )
	{
		newIntensityL = rage::Max(newIntensityL, pShader->GetLightValue(CVehicleLightSwitch::LW_TAILLIGHT_L));
		newIntensityR = rage::Max(newIntensityR, pShader->GetLightValue(CVehicleLightSwitch::LW_TAILLIGHT_R));
	}
	
	if( false == gotBrake )
	{
		newIntensityL = rage::Max(newIntensityL, pShader->GetLightValue(CVehicleLightSwitch::LW_BRAKELIGHT_L));
		newIntensityR = rage::Max(newIntensityR, pShader->GetLightValue(CVehicleLightSwitch::LW_BRAKELIGHT_R));
	}
	
	if( false == gotIndicator )
	{
		newIntensityL = rage::Max(newIntensityL, pShader->GetLightValue(CVehicleLightSwitch::LW_INDICATOR_RL));
		newIntensityR = rage::Max(newIntensityR, pShader->GetLightValue(CVehicleLightSwitch::LW_INDICATOR_RR));
	}

	if( gotTail )
	{
		// missing light uses tailLight
		pShader->SetLightValueMax(CVehicleLightSwitch::LW_TAILLIGHT_L,newIntensityL);
		pShader->SetLightValueMax(CVehicleLightSwitch::LW_TAILLIGHT_R,newIntensityR);
	}
	else if ( gotBrake )
	{
		// missing light uses brake light
		pShader->SetLightValueMax(CVehicleLightSwitch::LW_BRAKELIGHT_L,newIntensityL);
		pShader->SetLightValueMax(CVehicleLightSwitch::LW_BRAKELIGHT_R,newIntensityR);
	}
	else if ( gotIndicator )
	{
		// missing light uses indicator
		pShader->SetLightValueMax(CVehicleLightSwitch::LW_TAILLIGHT_L,newIntensityL);
		pShader->SetLightValueMax(CVehicleLightSwitch::LW_TAILLIGHT_R,newIntensityR);
	}
}
