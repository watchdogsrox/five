//
// VehicleModelInfoLights.h
// Data file for vehicle lights settings
//
// Rockstar Games (c) 2011

#ifndef INC_VEHICLE_MODELINFO_LIGHTS_H_
#define INC_VEHICLE_MODELINFO_LIGHTS_H_

#include "atl/array.h"
#include "atl/hashstring.h"
#include "data/base.h"
#include "data/callback.h"
#include "parser/macros.h"
#include "vector/color32.h"

namespace rage {
	class bkBank;
}


struct vehicleLight
{
	vehicleLight()
	{
		intensity = 2.1f;
		falloffMax = 20.0f;
		falloffExponent = 8.0f;
		innerConeAngle = 20.0f;
		outerConeAngle = 50.0f;
		color.Set(0xFFFFFFFF);
		emmissiveBoost = false;
		textureName = "";
		mirrorTexture = false;
	}
	
	float				intensity;
	float				falloffMax;
	float				falloffExponent;
	float				innerConeAngle;
	float				outerConeAngle;
	Color32				color;
	bool				emmissiveBoost;
	atHashWithStringNotFinal textureName;
	bool				mirrorTexture;
	
#if __BANK
	void AddWidgets(bkBank & bank);
#endif // __BANK	

	PAR_SIMPLE_PARSABLE;
};

struct vehicleCorona
{
	vehicleCorona()
	{
		size = 0.4f;
		size_far = 0.75f;
		intensity = 2.0f;
		intensity_far = 5.0f;
		color.Set(0xFFFFFFFF);
		numCoronas = 1;
		distBetweenCoronas = 128;
		distBetweenCoronas_far = 255;
		zBias = 0.08f;
		pullCoronaIn = false;
	}
	float				size;
	float				size_far;
	float				intensity;
	float				intensity_far;
	Color32				color;

	// for wide LED style lights
	u8					numCoronas;
	u8					distBetweenCoronas;		// 0-255, scaled to 0-1
	u8					distBetweenCoronas_far;	// 0-255, scaled to 0-1
	float				zBias;

	float				xRotation;
	float				yRotation;
	float				zRotation;

	// pull the corona in the "backwards" direction as it gets farther away from the camera
	bool				pullCoronaIn;
	
#if __BANK
	void AddWidgets(bkBank & bank, bool multipleCoronas, bool supportPullCoronaIn);
#endif // __BANK	

	PAR_SIMPLE_PARSABLE;
};

typedef u8 vehicleLightSettingsId;

enum { INVALID_VEHICLE_LIGHT_SETTINGS_ID = 0xFF };

struct vehicleLightSettings 
{
	vehicleLightSettings() :
		id(INVALID_VEHICLE_LIGHT_SETTINGS_ID)
	{
	}

	vehicleLightSettingsId GetId() const { return id; };

	vehicleLightSettingsId	id;
	vehicleLight			indicator;
	vehicleCorona			rearIndicatorCorona;
	vehicleCorona			frontIndicatorCorona;
	vehicleLight			tailLight;
	vehicleCorona			tailLightCorona;
	vehicleCorona			tailLightMiddleCorona;
	vehicleLight			headLight;
	vehicleCorona			headLightCorona;
	vehicleLight			reversingLight;
	vehicleCorona			reversingLightCorona;
	char 					*name;

#if __BANK
	void AddWidgets(bkBank & bank);
	static void WidgetNameChangedCB(CallbackData obj);
#endif // __BANK	

	PAR_SIMPLE_PARSABLE;
private:
};

struct vehicleCoronaRenderProp
{
	float	size;
	float	intensity;
	float	xRotation;
	float	yRotation;
	float	zRotation;

	float	distBetween;
	float	halfWidth;

	float	zBias;
	bool	pullCoronaIn;

	Color32 color;
	u8		numCoronas;
};

void PopulateCommonVehicleCoronaProperties(const vehicleCorona & corona, float coronaFade, float alphaFadeLOD, vehicleCoronaRenderProp & properties );
void PopulateCommonVehicleCoronaProperties(const vehicleCorona & corona, float coronaFade, float alphaFadeLOD, vehicleCoronaRenderProp & properties, float tcSpriteSize, float tcSpriteBrightness);

#endif // INC_VEHICLE_MODELINFO_LIGHTS_H_
