//
// VehicleModelInfoSirens.h
// Data file for vehicle sirens settings
//
// Rockstar Games (c) 2011

#ifndef INC_VEHICLE_MODELINFO_SIRENS_H_
#define INC_VEHICLE_MODELINFO_SIRENS_H_

#include "atl/array.h"
#include "atl/hashstring.h"
#include "data/base.h"
#include "data/callback.h"
#include "parser/macros.h"
#include "vector/color32.h"

#include "renderer/HierarchyIds.h"

#define MAX_SIRENS ((VEH_SIREN_MAX - VEH_SIREN_1)+1)
#define MAX_LIGHT_GROUPS 4
#define MAX_SIREN_LIGHTS (4) // two head and two tail lights
#define MAX_SIREN_BEATS (32)

namespace rage {
	class bkBank;
}

class CVehicle;

struct sirenCorona
{
	sirenCorona();
	
#if __BANK
	void AddWidget(bkBank* bank);
#endif // __BANK
	float intensity;
	float size;
	float pull;
	
	bool faceCamera;
	
	PAR_SIMPLE_PARSABLE;
};

struct sirenRotation
{
	sirenRotation();
	
	void PostLoad();
	
#if __BANK
	void AddWidget(bkBank* bank, float maximumSpeed = 50.0f);
	static void UpdateAngleCB(sirenRotation *r);
	static void SequencerStringUpdate(sirenRotation *r);
	
	float degDelta;
	float degStart;

	char sequencerString[MAX_SIREN_BEATS + 1];
#endif // __BANK
	
	float delta;
	float start;
	float speed;
	u32 sequencer;
	u8 multiples;
	bool direction;
	bool syncToBpm;
	
	PAR_SIMPLE_PARSABLE;
};


struct sirenLight 
{
	sirenLight();
	
#if __BANK
	void AddWidget(bkBank* bank);
	
	static sirenLight sirenCopy;
	static bool copyValid;
	static void CopyCB(sirenLight *src);
	static void PasteCB(sirenLight *dst);
#endif // __BANK

	sirenRotation rotation;
	sirenRotation flashiness;
	sirenCorona corona;
	Color32 color;
	float intensity;
	u8 lightGroup;
	u8 scaleFactor;
	bool scale;
	bool rotate;
	bool flash;
	bool light;
	bool spotLight;
	bool castShadows;

	PAR_SIMPLE_PARSABLE;
};

typedef u8 sirenSettingsId;

enum { INVALID_SIREN_SETTINGS_ID = 0xFF };

struct sirenSettings
{
	sirenSettings();

	void PostLoad();
	
	struct sequencerData
	{
		u32 sequencer;
#if __BANK
		char seqString[MAX_SIREN_BEATS + 1];
#endif
		PAR_SIMPLE_PARSABLE;
	};

#if __BANK
	void AddWidget(bkBank* bank);
	static void WidgetNameChangedCB(CallbackData obj);
	static void SequencerStringUpdate(sequencerData* seq);
#endif // __BANK

	sirenSettingsId GetId() const { return id; };

	sirenSettingsId id;

	float timeMultiplier;
	
	float lightFalloffMax;
	float lightFalloffExponent;
	float lightInnerConeAngle;
	float lightOuterConeAngle;
	float lightOffset;
	atHashWithStringNotFinal textureName;

	u32 sequencerBpm;
	sequencerData leftHeadLight;
	sequencerData rightHeadLight;
	sequencerData leftTailLight;
	sequencerData rightTailLight;

	u8 leftHeadLightMultiples;
	u8 rightHeadLightMultiples;
	u8 leftTailLightMultiples;
	u8 rightTailLightMultiples;
	
	bool useRealLights;
	// padding
	// padding
	// padding
	
	atFixedArray<sirenLight,MAX_SIRENS> sirens;
	
	char *name;

	PAR_SIMPLE_PARSABLE;
};

CompileTimeAssert(MAX_SIRENS <= 20);
CompileTimeAssert(MAX_SIREN_LIGHTS <= 20);
struct sirenInstanceData
{
	sirenInstanceData() { Reset(NULL); }

	void Reset(const CVehicle* owner);

	void SetIsRotating(u32 index, bool val) { if (val) sirenRotating |= (1 << index); else sirenRotating &= ~(1 << index);  } 
	void SetIsFlashing(u32 index, bool val) { if (val) sirenFlashing |= (1 << index); else sirenFlashing &= ~(1 << index);  }
	void SetIsLightOn(u32 index, bool val) { if (val) sirenLightOn |= (1 << index); else sirenLightOn &= ~(1 << index);  }

	bool IsSirenRotating(u32 index) const { return (sirenRotating & (1 << index)) != 0; }
	bool IsSirenFlashing(u32 index) const { return (sirenFlashing & (1 << index)) != 0; }
	bool IsSirenLightOn(u32 index) const { return (sirenLightOn & (1 << index)) != 0; }
	bool IsSirenLightOn(bool rear, bool right) const;
	float GetLightIntensityMutiplier(bool rear, bool right) const;

	void SetLightIntensityMultiplier(u32 index, float multiplier) { lightIntensity[index] = multiplier; }

	u32		sirenOnTime;
	float	sirenTimeDelta;
	s32		lastSirenBeat;

	u32		lastSirenRotationBit[MAX_SIRENS];
	u32		sirenRotationStart[MAX_SIRENS];
	u32		lastSirenFlashBit[MAX_SIRENS];
	u32		sirenFlashStart[MAX_SIRENS];
	u32		lastSirenLightBit[MAX_SIREN_LIGHTS];
	u32		sirenLightStart[MAX_SIREN_LIGHTS];
	float	lightIntensity[MAX_SIREN_LIGHTS];

	u32		sirenRotating; // bit fields holding the state for each siren/light
	u32		sirenFlashing;
	u32		sirenLightOn;
};
#endif // INC_VEHICLE_MODELINFO_SIRENS_H_
