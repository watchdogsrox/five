//
// VehicleModelInfoLights.h
// Data file for vehicle lights settings
//
// Rockstar Games (c) 2011

#include "modelinfo/VehicleModelInfoLights.h"
#include "modelinfo/VehicleModelInfoLights_parser.h"

#include "bank/bank.h"
#include "fwmaths/angle.h"
#include "modelinfo/VehicleModelInfo.h"
#include "TimeCycle/TimeCycle.h"

#include "data/aes_init.h"
AES_INIT_7;

VEHICLE_OPTIMISATIONS()

#if __BANK
void vehicleLight::AddWidgets(bkBank & bank) 
{ 
	bank.AddSlider("Intensity",&intensity,0.0f,100.0f,0.1f);
	bank.AddSlider("Falloff",&falloffMax,0.0f,100.0f,0.1f);
	bank.AddSlider("Falloff exponent", &falloffExponent, 0.0f, 256.0f, 0.1f);
	bank.AddSlider("Inner Cone Angle",&innerConeAngle,0.0f,90.0f,0.1f);
	bank.AddSlider("Outer Cone Angle",&outerConeAngle,0.0f,90.0f,0.1f);
	bank.AddColor("Color", &color);
	bank.AddToggle("Boost Emissive", &emmissiveBoost);
	bank.AddText("Texture Name", &textureName);
	bank.AddToggle("Mirror Texture", &mirrorTexture);
}

void vehicleCorona::AddWidgets(bkBank & bank, bool multipleCoronas, bool supportPullCoronaIn)
{
	if (supportPullCoronaIn)
	{
		bank.AddSlider("Corona zBias",&zBias,0.001f,0.35f,0.001f);
		bank.AddToggle("Pull Corona In At Distance", &pullCoronaIn);
	}

	bank.AddSlider("Corona Size",&size,0.0f,100.0f,0.1f);
	bank.AddSlider("Corona Size (Far)",&size_far,0.0f,100.0f,0.1f);
	bank.AddSlider("Corona Intensity",&intensity,0.0f,100.0f,0.1f);
	bank.AddSlider("Corona Intensity (Far)",&intensity_far,0.0f,100.0f,0.1f);
	bank.AddColor("Corona Color",&color);

	if (multipleCoronas)
	{
		bank.AddSlider("Corona Count",&numCoronas,1,8,1);
		bank.AddSlider("Corona Seperation",&distBetweenCoronas,1,255,1);
		bank.AddSlider("Corona Seperation (Far)",&distBetweenCoronas_far,1,255,1);
		bank.AddSlider("Corona X-Rotation",&xRotation,0.0f,TWO_PI,0.001f);
		bank.AddSlider("Corona Y-Rotation",&yRotation,0.0f,TWO_PI,0.001f);
		bank.AddSlider("Corona Z-Rotation",&zRotation,0.0f,TWO_PI,0.001f);
	}
}

static char vehicleLightSettings_tempName[64];			//HACK: stops us from displaying more than one ModelColor widget at a time - but does make it easier to display the name, rework if we need more than one color widget at a time

void vehicleLightSettings::WidgetNameChangedCB(CallbackData obj)
{
	vehicleLightSettings * t = ((vehicleLightSettings*)obj);
	StringFree(t->name);
	t->name = StringDuplicate((const char*)vehicleLightSettings_tempName);
	
#if __BANK
	CVehicleModelInfo::RefreshVehicleWidgets();
#endif // 	__BANK
}

void vehicleLightSettings::AddWidgets(bkBank & bank)
{
	if (name)
	{
		strncpy(vehicleLightSettings_tempName, name, sizeof(vehicleLightSettings_tempName));
		vehicleLightSettings_tempName[sizeof(vehicleLightSettings_tempName)-1] = 0;
	}
	else
	{
		vehicleLightSettings_tempName[0] = 0;
	}

	bank.AddText("Name", &vehicleLightSettings_tempName[0], sizeof(vehicleLightSettings_tempName)-1, false, datCallback(CFA1(vehicleLightSettings::WidgetNameChangedCB),(CallbackData)this));

	bank.PushGroup("Headlights");
	headLight.AddWidgets(bank);
	headLightCorona.AddWidgets(bank, true, false);
	bank.PopGroup();

	bank.PushGroup("Indicators");
	{
		indicator.AddWidgets(bank);

		bank.PushGroup("Rear Indicator Coronas");
		rearIndicatorCorona.AddWidgets(bank, true, true);
		bank.PopGroup();

		bank.PushGroup("Front Indicator Coronas");
		frontIndicatorCorona.AddWidgets(bank, true, false);
		bank.PopGroup();
	}
	bank.PopGroup();

	bank.PushGroup("Taillights");
	tailLight.AddWidgets(bank);
	tailLightCorona.AddWidgets(bank, true, true);
	{
		bank.PushGroup("Middle Taillight");
		tailLightMiddleCorona.AddWidgets(bank, true, true);
		bank.PopGroup();
	}
	bank.PopGroup();

	bank.PushGroup("Reversing Lights");
	reversingLight.AddWidgets(bank);
	reversingLightCorona.AddWidgets(bank, false, true);
	bank.PopGroup();
}
#endif // __BANK	

// Vehicle Helper Methods
void PopulateCommonVehicleCoronaProperties(const vehicleCorona & corona, float coronaFade, float alphaFadeLOD, vehicleCoronaRenderProp & properties, float tcSpriteSize, float tcSpriteBrightness)
{
	const float c_intensity =  corona.intensity;
	const float c_intensity_Far =  corona.intensity_far;
	const float c_size =  corona.size;
	const float c_size_Far =  corona.size_far;
	const u8	coronas_Count =  corona.numCoronas;
	const float coronas_DistBetween =  corona.distBetweenCoronas / 256.0f;
	const float coronas_DistBetween_Far =  corona.distBetweenCoronas_far / 256.0f;

	properties.color = corona.color;
	properties.xRotation = corona.xRotation;
	properties.yRotation = corona.yRotation;
	properties.zRotation = corona.zRotation;
	properties.zBias = corona.zBias;

	properties.intensity = Lerp(coronaFade,c_intensity,c_intensity_Far) * tcSpriteBrightness * alphaFadeLOD;
	properties.size = Lerp(coronaFade, c_size, c_size_Far) * tcSpriteSize;

	// variables for multiple coronas (used for LED style lights)
	properties.numCoronas = (u8)((coronas_Count + (1.0f - coronas_Count) * coronaFade) + 0.5f); // lerped and rounded
	properties.distBetween = Lerp(coronaFade,coronas_DistBetween, coronas_DistBetween_Far);
	properties.halfWidth = ((properties.numCoronas-1) * properties.distBetween) * 0.5f;

	properties.pullCoronaIn = corona.pullCoronaIn;
}

void PopulateCommonVehicleCoronaProperties(const vehicleCorona & corona, float coronaFade, float alphaFadeLOD, vehicleCoronaRenderProp & properties)
{
	const tcKeyframeUncompressed& currKeyFrame = g_timeCycle.GetCurrUpdateKeyframe();
	PopulateCommonVehicleCoronaProperties(corona, coronaFade, alphaFadeLOD, properties, currKeyFrame.GetVar(TCVAR_SPRITE_SIZE), currKeyFrame.GetVar(TCVAR_SPRITE_BRIGHTNESS));
}