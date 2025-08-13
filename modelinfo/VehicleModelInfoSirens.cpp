//
// VehicleModelInfoSirens.h
// Data file for vehicle sirens settings
//
// Rockstar Games (c) 2011

#include "modelinfo/VehicleModelInfoSirens.h"
#include "modelinfo/VehicleModelInfoSirens_parser.h"

#include "bank/bank.h"
#include "modelinfo/VehicleModelInfo.h"
#include "Vehicles/vehicle.h"

VEHICLE_OPTIMISATIONS()

sirenLight::sirenLight()
{
	intensity = 1.0f;
	lightGroup = 0;
	rotate = false;
	scale = false;
	scaleFactor = 0;
	light = false;
	spotLight = false;
	castShadows = false;
}

#if __BANK
sirenLight sirenLight::sirenCopy;
bool sirenLight::copyValid = false;

void sirenLight::AddWidget(bkBank* bank)
{
	copyValid = false;
	
	bank->AddButton("Copy Settings",datCallback(CFA1(sirenLight::CopyCB), this));
	bank->AddButton("Paste Settings",datCallback(CFA1(sirenLight::PasteCB), this));
	bank->AddColor("Color",&color);
	
	bank->PushGroup("Corona");
	corona.AddWidget(bank);	
	bank->PopGroup();
	
	bank->PushGroup("Rotation");
	bank->AddToggle("Apply Rotation",&rotate);
	rotation.AddWidget(bank);
	bank->PopGroup();

	bank->PushGroup("Scale");
	bank->AddToggle("Scales",&scale);
	bank->AddSlider("Scale Factor",&scaleFactor,0,100,1);
	bank->PopGroup();
	
	bank->PushGroup("Flash");
	bank->AddToggle("Flashes",&flash);
	flashiness.AddWidget(bank, 60.0f);
	bank->PopGroup();

	bank->PushGroup("Lighting");
	bank->AddToggle("Contribute to lighting",&light);
	bank->AddSlider("Light Type",&lightGroup,0,MAX_LIGHT_GROUPS-1,1);
	bank->AddToggle("Spotlight",&spotLight);
	bank->AddSlider("Intensity", &intensity,0.0f,50.0f,0.1f);
	bank->AddToggle("Cast shadows",&castShadows);
	bank->PopGroup();
}

void sirenLight::CopyCB(sirenLight *src)
{
	sirenCopy = *src;
	copyValid = true;
}

void sirenLight::PasteCB(sirenLight *dst)
{
	if( copyValid )
	{
		*dst = sirenCopy;
	}
}

#endif // __BANK

sirenCorona::sirenCorona()
{
	intensity = 1.0f;
	size = 0.1f;
	pull = 0.1f;
}

#if __BANK	
void sirenCorona::AddWidget(bkBank* bank)
{
	bank->AddSlider("Intensity", &intensity,0.0f,50.0f,0.1f);
	bank->AddSlider("Size", &size,0.0f,50.0f,0.1f);
	bank->AddSlider("Pull", &pull,0.0f,1.0f,0.01f);
	bank->AddToggle("Face Camera", &faceCamera);
}
#endif // __BANK

sirenRotation::sirenRotation()
{
	start = 0.0f;
	delta = 0.0f;
	speed = 1.0f;
	sequencer = 0;
	multiples = 1;
	direction = false;
	syncToBpm = false;

#if __BANK
	safecpy(sequencerString, "11111111111111111111111111111111");
#endif
}

void sirenRotation::PostLoad()
{
#if __BANK
	degDelta = delta * RtoD;
	degStart = start * RtoD;

	for (s32 i = 0; i < MAX_SIREN_BEATS; ++i)
	{
		sequencerString[i] = 48 + (char)((sequencer >> (MAX_SIREN_BEATS - i - 1)) & 0x1);
	}
#endif // __BANK
}

#if __BANK
void sirenRotation::AddWidget(bkBank* bank, float maximumSpeed)
{
	bank->AddSlider("Speed",&speed,0.0f,maximumSpeed,0.1f);
	bank->AddAngle("Start",&degStart,bkAngleType::DEGREES,0.0f,360.0f,datCallback(CFA1(sirenRotation::UpdateAngleCB), this));
	bank->AddAngle("Delta",&degDelta,bkAngleType::DEGREES,0.0f,360.0f,datCallback(CFA1(sirenRotation::UpdateAngleCB), this));
	bank->AddToggle("Direction", &direction);
	bank->AddToggle("Sync to BPM", &syncToBpm);
	bank->AddSlider("Multiples", &multiples, 1, 5, 1);
	bank->AddText("Sequencer", sequencerString, sizeof(sequencerString), datCallback(CFA1(sirenRotation::SequencerStringUpdate), this));
}

void sirenRotation::UpdateAngleCB(sirenRotation *r)
{
	r->delta = r->degDelta * DtoR;
	r->start = r->degStart * DtoR;
}

void sirenRotation::SequencerStringUpdate(sirenRotation *r)
{
	// replace all non 0/1 characters with 0 and encode the bits into the u32
	for (s32 i = 0; i < MAX_SIREN_BEATS; ++i)
	{
		const u32 bit = MAX_SIREN_BEATS - i - 1;
		char chr = r->sequencerString[i];
		if (chr == '1')
		{
			r->sequencer |= (1 << bit);
		}
		else
		{
			r->sequencer &= ~(1 << bit);
			r->sequencerString[i] = '0';
		}
	}
}
#endif // __BANK

sirenSettings::sirenSettings() 
{
	id = INVALID_SIREN_SETTINGS_ID;
	timeMultiplier = 1.0f;

	lightFalloffMax = 12.0f;
	lightFalloffExponent = 8.0f;
	lightInnerConeAngle = 35.0f;
	lightOuterConeAngle = 45.0f;
	lightOffset = 2.0f;
	textureName = "";

	sequencerBpm = 200;
	leftHeadLight.sequencer = 0;
	leftHeadLightMultiples = 1;
	rightHeadLight.sequencer = 0;
	rightHeadLightMultiples = 1;
	leftTailLight.sequencer = 0;
	leftTailLightMultiples = 1;
	rightTailLight.sequencer = 0;
	rightTailLightMultiples = 1;

#if __BANK
	safecpy(leftHeadLight.seqString, "11111111111111111111111111111111");
	safecpy(rightHeadLight.seqString, "11111111111111111111111111111111");
	safecpy(leftTailLight.seqString, "11111111111111111111111111111111");
	safecpy(rightTailLight.seqString, "11111111111111111111111111111111");
#endif
	
	useRealLights = false;
	
	sirens.SetCount(MAX_SIRENS);
	
}

void sirenSettings::PostLoad()
{
#if __BANK
	for (s32 i = 0; i < MAX_SIREN_BEATS; ++i)
	{
		leftHeadLight.seqString[i] = 48 + (char)((leftHeadLight.sequencer >> (MAX_SIREN_BEATS - i - 1)) & 0x1);
		rightHeadLight.seqString[i] = 48 + (char)((rightHeadLight.sequencer >> (MAX_SIREN_BEATS - i - 1)) & 0x1);
		leftTailLight.seqString[i] = 48 + (char)((leftTailLight.sequencer >> (MAX_SIREN_BEATS - i - 1)) & 0x1);
		rightTailLight.seqString[i] = 48 + (char)((rightTailLight.sequencer >> (MAX_SIREN_BEATS - i - 1)) & 0x1);
	}
#endif // __BANK

}
#if __BANK

static char vehicleSirenSettings_tempName[64];			//HACK: stops us from displaying more than one ModelColor widget at a time - but does make it easier to display the name, rework if we need more than one color widget at a time

void sirenSettings::WidgetNameChangedCB(CallbackData obj)
{
	sirenSettings * t = ((sirenSettings*)obj);
	StringFree(t->name);
	t->name = StringDuplicate((const char*)vehicleSirenSettings_tempName);
	
	CVehicleModelInfo::RefreshVehicleWidgets();
}

void sirenSettings::AddWidget(bkBank* bank)
{
	if (name)
	{
		strncpy(vehicleSirenSettings_tempName, name, sizeof(vehicleSirenSettings_tempName));
		vehicleSirenSettings_tempName[sizeof(vehicleSirenSettings_tempName)-1] = 0;
	}
	else
	{
		vehicleSirenSettings_tempName[0] = 0;
	}

	bank->AddText("Name", &vehicleSirenSettings_tempName[0], sizeof(vehicleSirenSettings_tempName)-1, false, datCallback(CFA1(sirenSettings::WidgetNameChangedCB),(CallbackData)this));
	bank->AddSlider("Time Multiplier", &timeMultiplier,0.0f, 1000.0f, 0.1f);
	bank->AddSlider("Sequencer BPM", &sequencerBpm, 0, 1000, 1);
	bank->AddText("Left Head Light Sequencer", leftHeadLight.seqString, sizeof(leftHeadLight.seqString), datCallback(CFA1(sirenSettings::SequencerStringUpdate), &leftHeadLight));
	bank->AddSlider("Left Head Light Multiples", &leftHeadLightMultiples, 1, 5, 1);
	bank->AddText("Right Head Light Sequencer", rightHeadLight.seqString, sizeof(rightHeadLight.seqString), datCallback(CFA1(sirenSettings::SequencerStringUpdate), &rightHeadLight));
	bank->AddSlider("Right Head Light Multiples", &rightHeadLightMultiples, 1, 5, 1);
	bank->AddText("Left Tail Light Sequencer", leftTailLight.seqString, sizeof(leftTailLight.seqString), datCallback(CFA1(sirenSettings::SequencerStringUpdate), &leftTailLight));
	bank->AddSlider("Left Tail Light Multiples", &leftTailLightMultiples, 1, 5, 1);
	bank->AddText("Right Tail Light Sequencer", rightTailLight.seqString, sizeof(rightTailLight.seqString), datCallback(CFA1(sirenSettings::SequencerStringUpdate), &rightTailLight));
	bank->AddSlider("Right Tail Light Multiples", &rightTailLightMultiples, 1, 5, 1);
	bank->AddToggle("Use Real Lights",&useRealLights);
	bank->PushGroup("Global Lights parameters");
			bank->AddSlider("Falloff max", &lightFalloffMax, 0.1f, 15.0f, 0.1f);
			bank->AddSlider("Falloff Exponent", &lightFalloffExponent, 0.0f, 256.0f, 0.1f);
			bank->AddAngle("Cone inner", &lightInnerConeAngle, bkAngleType::DEGREES,0.0f,90.0f);
			bank->AddAngle("Cone outer", &lightOuterConeAngle, bkAngleType::DEGREES,0.0f,90.0f);
			bank->AddSlider("Offset", &lightOffset,0.0f,2.0f,0.1f);
	bank->AddText("Siren Texture", &textureName);
	bank->PopGroup();
	
	char title[256];
	for(int i=0;i<sirens.GetMaxCount();i++)
	{
		sprintf(title,"Siren %d",i);
		bank->PushGroup(title);
		sirens[i].AddWidget(bank);
		bank->PopGroup();
	}
}

void sirenSettings::SequencerStringUpdate(sequencerData* seqData)
{
	// replace all non 0/1 characters with 0 and encode the bits into the u32
	for (s32 i = 0; i < MAX_SIREN_BEATS; ++i)
	{
		const u32 bit = MAX_SIREN_BEATS - i - 1;
		char chr = seqData->seqString[i];
		if (chr == '1')
		{
			seqData->sequencer |= (1 << bit);
		}
		else
		{
			seqData->sequencer &= ~(1 << bit);
			seqData->seqString[i] = '0';
		}
	}
}
#endif

void sirenInstanceData::Reset(const CVehicle* owner)
{
	if (owner)
		sirenOnTime = fwTimer::GetTimeInMilliseconds() + owner->GetRandomSeed();
	else
		sirenOnTime = 0;

	sirenTimeDelta = 0.f;
	lastSirenBeat = -1;

	for (s32 i = 0; i < MAX_SIRENS; ++i)
	{
		lastSirenRotationBit[i] = 0;
		sirenRotationStart[i] = 0;
		lastSirenFlashBit[i] = 0;
		sirenFlashStart[i] = 0;
	}

	for (s32 i = 0; i < MAX_SIREN_LIGHTS; ++i)
	{
		lastSirenLightBit[i] = 0;
		sirenLightStart[i] = 0;
		lightIntensity[i] = 1.f;
	}

	sirenRotating = 0;
	sirenFlashing = 0;
	sirenLightOn = 0;
}

// order is:
// front_left = 0
// front_right
// rear_left
// rear_right = MAX_SIREN_LIGHTS - 1
bool sirenInstanceData::IsSirenLightOn(bool rear, bool right) const
{
	const u32 index = (rear * 2) + right;
	return IsSirenLightOn(index);
}

float sirenInstanceData::GetLightIntensityMutiplier(bool rear, bool right) const
{
	const u32 index = (rear * 2) + right;
	return lightIntensity[index];
}