//
// VehicleModelInfoColors.h
// Data file for vehicle color settings
//
// Rockstar Games (c) 2010

#ifndef INC_VEHICLE_MODELINFO_COLORS_H_
#define INC_VEHICLE_MODELINFO_COLORS_H_

#include "atl/array.h"
#include "atl/map.h"
#include "data/base.h"
#include "parser/macros.h"
#include "renderer/color.h"
#include "renderer/HierarchyIds.h"
#include "system/noncopyable.h"
#include "system/nelem.h"

#include "modelinfo/VehicleModelInfoPlates.h"
#include "modelinfo/VehicleModelInfoLights.h"
#include "modelinfo/VehicleModelInfoSirens.h"
#include "modelinfo/VehicleModelInfoEnums.h"
#include "modelinfo/VehicleModelInfoMods.h"

namespace rage {
	class bkGroup;
	class bkBank;
}

class CVehicleStreamRenderGfx;
class CCustomShaderEffectVehicleType;

// describe a single color
class CVehicleModelColor
{
public:
	CVehicleModelColor();
	~CVehicleModelColor();
	CVehicleModelColor & operator=(const CVehicleModelColor & that);
	CVehicleModelColor(const CVehicleModelColor &that);

	Color32		m_color;
	u8			m_metallicID;						// metallicID (index of metallic setting; -1=default)
	enum EVehicleModelColorMetallicID	{
		EVehicleModelColorMetallic_none		=-1,
		EVehicleModelColorMetallic_normal	= 0,
		EVehicleModelColorMetallic_1,
		EVehicleModelColorMetallic_2, 
		EVehicleModelColorMetallic_3, 
		EVehicleModelColorMetallic_4,
		EVehicleModelColorMetallic_5,
		EVehicleModelColorMetallic_6,
		EVehicleModelColorMetallic_7,
		EVehicleModelColorMetallic_8,
		EVehicleModelColorMetallic_9,
		EVehicleModelColorMetallic_COUNT
	};
	u8			m_audioColor;						// need non hashed (enumerated) values for editing, need to add a bkCombo that works with hashes
	u8			m_audioPrefix;						// need non hashed (enumerated) values for editing, need to add a bkCombo that works with hashes
	enum EVehicleModelAudioColor	{
		EVehicleModelAudioColor_black = 0, 
		EVehicleModelAudioColor_blue, 
		EVehicleModelAudioColor_brown, 
		EVehicleModelAudioColor_beige, 
		EVehicleModelAudioColor_graphite, 
		EVehicleModelAudioColor_green, 
		EVehicleModelAudioColor_grey, 
		EVehicleModelAudioColor_orange, 
		EVehicleModelAudioColor_pink, 
		EVehicleModelAudioColor_red, 
		EVehicleModelAudioColor_silver, 
		EVehicleModelAudioColor_white, 
		EVehicleModelAudioColor_yellow,
		EVehicleModelAudioColor_COUNT
	};
	enum EVehicleModelAudioPrefix	{
		EVehicleModelAudioPrefix_none = 0,
		EVehicleModelAudioPrefix_bright,
		EVehicleModelAudioPrefix_light,
		EVehicleModelAudioPrefix_dark,
		EVehicleModelAudioPrefix_COUNT,
	};
	u32			m_audioColorHash;				// Store the hashed value
	u32			m_audioPrefixHash;				// Store the hashed value
	char *		m_colorName;					// name of the color (not needed in final builds)

	void		Init();

	PAR_SIMPLE_PARSABLE;
};

class CVehicleModelColorGen9
{
public:
	CVehicleModelColorGen9();
	~CVehicleModelColorGen9();
	CVehicleModelColorGen9 & operator=(const CVehicleModelColorGen9 & that);
	CVehicleModelColorGen9(const CVehicleModelColorGen9 &that);

	void Init();
	void InitFromBase(const CVehicleModelColor& baseCol);
	void UpdateValuesFromHashes();		// update the m_audioColor and m_audioPrefix from the hashed versions of these
	void UpdateHashesFromValues();		// update the m_audioColor and m_audioPrefix from the hashed versions of these

#if __BANK
	void AddWidgets(bkBank & bank);
	static void WidgetNameChangedCB(CallbackData obj);
	static void UpdateHashesFromValuesCB(CallbackData obj);
	static void WidgetColorChangedCB(CallbackData obj);
#endif // __BANK

	Color32		m_color;
	u8			m_metallicID;					// metallicID (index of metallic setting; -1=default)
	u8			m_audioColor;					// need non hashed (enumerated) values for editing, need to add a bkCombo that works with hashes
	u8			m_audioPrefix;					// need non hashed (enumerated) values for editing, need to add a bkCombo that works with hashes
	u32			m_audioColorHash;				// Store the hashed value
	u32			m_audioPrefixHash;				// Store the hashed value
	char *		m_colorName;					// name of the color (not needed in final builds)
	char *		m_rampTextureName;				// Optional ramp texture

	PAR_SIMPLE_PARSABLE;
};

class CVehicleModelColorsGen9
{
public:
	rage::atArray<CVehicleModelColorGen9> m_Colors;

	PAR_SIMPLE_PARSABLE;
};

// describes a single metallic setting
class CVehicleMetallicSetting
{
public:
	CVehicleMetallicSetting() : m_specInt(0), m_specFalloff(0), m_specFresnel(0)	{}
	~CVehicleMetallicSetting() {}

public:
	float	m_specInt;
	float	m_specFalloff;
	float	m_specFresnel;

	PAR_SIMPLE_PARSABLE;
};


class CVehicleWindowColor
{
public:
	CVehicleWindowColor();
	~CVehicleWindowColor() {}

	Color32 m_color;
	atHashString m_name;

#if __BANK
	void AddWidgets(bkBank & bank);
	static void WidgetNameChangedCB(CallbackData obj);
#endif // __BANK

	PAR_SIMPLE_PARSABLE;
};

// describe the color set for a single vehicle
class CVehicleModelColorIndices
{
public:
	CVehicleModelColorIndices();

	u8		m_indices[NUM_VEH_BASE_COLOURS];
	bool	m_liveries[MAX_NUM_LIVERIES]; // liveries for which this color is enabled

	PAR_SIMPLE_PARSABLE;
};

class CVehicleVariationGlobalData
{
public:
	CVehicleVariationGlobalData() {}

	Color32 m_xenonLightColor;
	Color32 m_xenonCoronaColor;
	float m_xenonLightIntensityModifier;
	float m_xenonCoronaIntensityModifier;

	PAR_SIMPLE_PARSABLE;
};

class CVehicleXenonLightColor
{
public:
	CVehicleXenonLightColor() {}

	Color32	m_lightColor;
	Color32	m_coronaColor;
	float	m_lightIntensityModifier;
	float   m_coronaIntensityModifier;

	PAR_SIMPLE_PARSABLE;
};


// class that stores all global vehicle variation data
class CVehicleModelInfoVarGlobal : public rage::datBase
{
	NON_COPYABLE(CVehicleModelInfoVarGlobal);
public:
	CVehicleModelInfoVarGlobal();
	~CVehicleModelInfoVarGlobal();

	static void Init();

	CVehicleModelInfoPlates &GetLicensePlateData() { return m_VehiclePlates; }

	atArray<CVehicleModelColorGen9>& GetColors();
	u32 GetColorCount();

	void GetAudioDataForColour(s32 colId, u32 &prefix, u32 &colour)
	{
		CVehicleModelColorGen9 & c = GetColors()[colId];
		prefix = c.m_audioPrefixHash;
		colour = c.m_audioColorHash;
	}

	void GetAudioIndexesForColour(s32 colId, u8 &prefix, u8 &colour)
	{
		CVehicleModelColorGen9 & c = GetColors()[colId];
		prefix = c.m_audioPrefix;
		colour = c.m_audioColor;
	}
	
	const char *GetVehicleColourText(s32 col) {return (col < GetColorCount()) ? GetColors()[col].m_colorName : "";}
	Color32	GetVehicleColour(s32 col) {return (col < GetColorCount()) ? GetColors()[col].m_color : CRGBA(150,150,150,255); }

#if VEHICLE_SUPPORT_PAINT_RAMP
	const char *GetVehicleColourRampTextureName(s32 col) { return (col < GetColorCount()) ? GetColors()[col].m_rampTextureName : ""; }
#endif

	u8							GetVehicleColourMetallicID(s32 col)	{ return (col< GetColorCount()) ? GetColors()[col].m_metallicID : CVehicleModelColor::EVehicleModelColorMetallic_none; }
	CVehicleMetallicSetting*	GetMetallicSetting(u32 i)			{ FastAssert(i<m_MetallicSettings.GetCount()); return &m_MetallicSettings[i]; }

#if !__FINAL
	const char* GetVehicleWindowColorText(s32 col) const { return (col < m_WindowColors.GetCount()) ? m_WindowColors[col].m_name.GetCStr() : ""; }
#endif	//	!__FINAL
	Color32 GetVehicleWindowColor(s32 col) const { return (col < m_WindowColors.GetCount()) ? m_WindowColors[col].m_color : CRGBA(255, 255, 255, 150); }

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif

	u16 GetKitIndexById(VehicleKitId id) const
	{
		if (id == INVALID_VEHICLE_KIT_ID)
			return INVALID_VEHICLE_KIT_INDEX;

		if(AssertVerify(id < NELEM(m_KitsIdMap)))
			return m_KitsIdMap[id];
		return INVALID_VEHICLE_KIT_INDEX;
	}

	static u8 GetLightIndexById(vehicleLightSettingsId id)
	{
		if(AssertVerify(id < NELEM(m_LightsIdMap)))
			return m_LightsIdMap[id];
		return INVALID_VEHICLE_LIGHT_SETTINGS_ID;
	}

	static u8 GetSirenIndexById(sirenSettingsId id)
	{
		if(AssertVerify(id < NELEM(m_SirensIdMap)))
			return m_SirensIdMap[id];
		return INVALID_SIREN_SETTINGS_ID;
	}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

	const CVehicleVariationGlobalData& GetGlobalVariationData() const { return m_GlobalVariationData; }
#if __BANK
	void CreateOwnershipInfos(const char* fileName);
	void AddWidgets(rage::bkBank & bank);
	void RefreshVehicleWidgets();

	const atArray<const char*>& GetVehicleNames() const { return m_VehicleNames; }
	const atArray<const char*>& GetColorNames() const { return m_ColorNames; }
	const atArray<const char*>& GetWindowColorNames() const { return m_WindowColorNames; }
	const atArray<const char*>& GetLightNames() const { return m_LightNames; }
	const atArray<const char*>& GetSirenNames() const { return m_SirenNames; }

	int GetColorEditingIndex() const { return m_VehicleColorEditingIndex; }

	static void AddCarColFile(const char* filename) { if(ms_carcolFiles.Find(filename) < 0) ms_carcolFiles.PushAndGrow(filename); }
	static const char* GetCarColFile(int index){ return ms_carcolFiles[index].TryGetCStr(); }
	static int GetCarColFileCount(){ return ms_carcolFiles.GetCount(); }
#endif // __BANK

	void OnPostLoad(BANK_ONLY(const char* filename));
	void OnPreSave();

	void UpdateKitIds(int start, int count BANK_ONLY(, const char* fileName=NULL));
	void UpdateLightIds(int start, int count BANK_ONLY(, const char* fileName=NULL));
	void UpdateSirenIds(int start, int count BANK_ONLY(, const char* fileName=NULL));
	void UpdateWheelIds(eVehicleWheelType wheelType, int start, int count BANK_ONLY(, char const *filename = NULL));


	rage::atArray<CVehicleModelColor>				m_Colors;
	rage::atArray<CVehicleMetallicSetting>			m_MetallicSettings;
	rage::atArray<CVehicleWindowColor>				m_WindowColors;
	rage::atArray<vehicleLightSettings>				m_Lights;
	rage::atArray<sirenSettings>					m_Sirens;
	rage::atArray<CVehicleKit>						m_Kits;
    rage::atArray<CVehicleWheel>					m_Wheels[VWT_MAX];

	CVehicleVariationGlobalData						m_GlobalVariationData;
	rage::atArray<CVehicleXenonLightColor>			m_XenonLightColors;

	PAR_SIMPLE_PARSABLE;

private:
	CVehicleModelInfoPlates m_VehiclePlates;

	VehicleKitId GenerateKitId(u16 index) const; 
	vehicleLightSettingsId GenerateLightId(u8 index) const; 
	sirenSettingsId GenerateSirenId(u8 index) const; 

	static u8										m_LightsIdMap[256];
	static u8										m_SirensIdMap[256];
	static u16										m_KitsIdMap[1024];
	static atMap<u32, u8>							m_WheelsIdMap[VWT_MAX];

#if __BANK
	void RefreshColorNames();
	void RefreshWindowColorNames();
	void RefreshLightNames();
	void RefreshSirenNames();
	void WidgetSave();
	void WidgetSaveAll();
	void DeleteWidgetNames(atArray<const char*> & nameArray);
	void WidgetChangedColorEditIndex();
	void WidgetChangedWindowColorEditIndex();
	void WidgetChangedLightEditIndex();
	void WidgetChangedSirenEditIndex();
	void WidgetAddColor();
	void WidgetAddWindowColor();
	void WidgetDeleteColor();
	void WidgetDeleteWindowColor();
	void WidgetAddVehiclePossibleColor();
	void WidgetCopyColorsFromVehicle();
	void WidgetAddLights();
	void WidgetDeleteLights();
	void WidgetAddSirens();
	void WidgetCopySirens();
	void WidgetPasteSirens();
	void WidgetDeleteSirens();

	static const int kMAX_NUM_COLORS = 255;					// using an 8 bit index for colors
	static const int kMAX_NUM_LIGHTS = 255;					// using an 8 bit index for lights too
	static const int kMAX_NUM_SIRENS = 127;					// using a signed 8 bit index for sirens too

	static int ms_fileSelection;
	static atArray<atHashString> ms_carcolFiles;

	rage::bkGroup *								m_VehicleModelBankGroup;
	rage::bkGroup *								m_VehicleColorBankGroup;
	rage::bkGroup *								m_VehicleWindowColorBankGroup;
	rage::bkGroup *								m_VehicleLightsBankGroup;
	rage::bkGroup *								m_VehicleSirensBankGroup;
	rage::bkGroup *								m_VehicleCurrentModelBankGroup;
	rage::bkGroup *								m_VehicleCurrentColorBankGroup;
	rage::bkGroup *								m_VehicleCurrentWindowColorBankGroup;
	rage::bkGroup *								m_VehicleCurrentLightsBankGroup;
	rage::bkGroup *								m_VehicleCurrentSirensBankGroup;
	rage::bkCombo *								m_ColorNameCombo;
	rage::bkCombo *								m_WindowColorNameCombo;
	rage::bkCombo *								m_LightNameCombo;
	rage::bkCombo *								m_SirenNameCombo;
	int											m_VehicleModelEditingIndex;
	int											m_VehicleColorEditingIndex;
	int											m_VehicleWindowColorEditingIndex;
	int											m_VehicleLightEditingIndex;
	int											m_VehicleSirenEditingIndex;
	bool										m_VehicleSirenCopyReady;
	sirenSettings								m_VehicleSirenCopyData;
	int											m_VehicleCopyFromIndex;
	atArray<const char*>						m_VehicleNames;						// extracted for widgets
	atArray<const char*>						m_ColorNames;						// extracted for widgets
	atArray<const char*>						m_WindowColorNames;					// extracted for widgets
	atArray<const char*>						m_LightNames;						// extracted for widgets
	atArray<const char*>						m_SirenNames;						// extracted for widgets
#endif
};




#endif // INC_VEHICLE_MODELINFO_COLORS_H_
