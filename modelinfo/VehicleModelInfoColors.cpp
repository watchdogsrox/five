//
// VehicleModelInfoColors.cpp
// Data file for vehicle color settings
//
// Rockstar Games (c) 2010

#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/combo.h"
#include "modelinfo/VehicleModelInfoColors.h"
#include "modelinfo/VehicleModelInfoColors_parser.h"
#include "modelinfo/VehicleModelInfoVariation.h"
#include "parser/visitorwidgets.h"
#include "scene/Entity.h"
#include "scene/EntityIterator.h"
#include "vehicles/Vehicle.h"

#if __BANK
#include "modelinfo/VehicleModelInfo.h"
#include "scene/ExtraContent.h"
#include "vehicles/VehicleFactory.h"
#include "shaders/CustomShaderEffectVehicle.h"
#endif

using namespace rage;

u8 CVehicleModelInfoVarGlobal::m_LightsIdMap[256];
u8 CVehicleModelInfoVarGlobal::m_SirensIdMap[256];
u16 CVehicleModelInfoVarGlobal::m_KitsIdMap[1024];
atMap<u32, u8> CVehicleModelInfoVarGlobal::m_WheelsIdMap[VWT_MAX];

#if __BANK
int CVehicleModelInfoVarGlobal::ms_fileSelection = 0;
atArray<atHashString> CVehicleModelInfoVarGlobal::ms_carcolFiles;
#endif // __BANK

static atArray<CVehicleModelColorGen9> s_VehicleModelColorsAll;
static u32 s_VehicleModelColorBaseNum;

atArray<CVehicleModelColorGen9>& CVehicleModelInfoVarGlobal::GetColors()
{
	return s_VehicleModelColorsAll;
}

u32 CVehicleModelInfoVarGlobal::GetColorCount()
{
	return s_VehicleModelColorsAll.GetCount();
}

CVehicleModelColor::CVehicleModelColor()
{
	Init();
}

void CVehicleModelColor::Init()
{
	m_color.Set(0);
	m_metallicID = (u8)EVehicleModelColorMetallic_none;
	m_audioColor = EVehicleModelAudioColor_black;
	m_audioPrefix = EVehicleModelAudioPrefix_none;
	m_audioColorHash = 0;
	m_audioPrefixHash = 0;
	m_colorName = NULL;
}

CVehicleModelColor & CVehicleModelColor::operator=(const CVehicleModelColor & that)
{
	if (this != &that)
	{
		m_color = that.m_color;
		m_metallicID = that.m_metallicID;
		m_audioColor = that.m_audioColor;
		m_audioPrefix = that.m_audioPrefix;
		m_audioColorHash = that.m_audioColorHash;
		m_audioPrefixHash = that.m_audioPrefixHash;
		StringFree(m_colorName);
		m_colorName = StringDuplicate(that.m_colorName);
	}
	return *this;
}

CVehicleModelColor::CVehicleModelColor(const CVehicleModelColor &that)
{
	m_color = that.m_color;
	m_metallicID = that.m_metallicID;
	m_audioColor = that.m_audioColor;
	m_audioPrefix = that.m_audioPrefix;
	m_audioColorHash = that.m_audioColorHash;
	m_audioPrefixHash = that.m_audioPrefixHash;
	m_colorName = StringDuplicate(that.m_colorName);
}

CVehicleModelColor::~CVehicleModelColor()
{
	StringFree(m_colorName);
}

CVehicleModelColorGen9::CVehicleModelColorGen9()
{
	Init();
}

void CVehicleModelColorGen9::Init()
{
	m_color.Set(0);
	m_metallicID = (u8)CVehicleModelColor::EVehicleModelColorMetallic_none;
	m_audioColor = CVehicleModelColor::EVehicleModelAudioColor_black;
	m_audioPrefix = CVehicleModelColor::EVehicleModelAudioPrefix_none;
	m_audioColorHash = 0;
	m_audioPrefixHash = 0;
	m_colorName = NULL;
	m_rampTextureName = NULL;
}

void CVehicleModelColorGen9::InitFromBase(const CVehicleModelColor& baseCol)
{
	m_color = baseCol.m_color;
	m_metallicID = baseCol.m_metallicID;
	m_audioColor = baseCol.m_audioColor;
	m_audioPrefix = baseCol.m_audioPrefix;
	m_audioColorHash = baseCol.m_audioColorHash;
	m_audioPrefixHash = baseCol.m_audioPrefixHash;
	m_colorName = StringDuplicate(baseCol.m_colorName);
	m_rampTextureName = NULL;
}

CVehicleModelColorGen9 & CVehicleModelColorGen9::operator=(const CVehicleModelColorGen9 & that)
{
	if (this != &that)
	{
		m_color = that.m_color;
		m_metallicID = that.m_metallicID;
		m_audioColor = that.m_audioColor;
		m_audioPrefix = that.m_audioPrefix;
		m_audioColorHash = that.m_audioColorHash;
		m_audioPrefixHash = that.m_audioPrefixHash;
		StringFree(m_colorName);
		m_colorName = StringDuplicate(that.m_colorName);
		StringFree(m_rampTextureName);
		m_rampTextureName = StringDuplicate(that.m_rampTextureName);
	}
	return *this;
}

CVehicleModelColorGen9::CVehicleModelColorGen9(const CVehicleModelColorGen9 &that)
{
	m_color = that.m_color;
	m_metallicID = that.m_metallicID;
	m_audioColor = that.m_audioColor;
	m_audioPrefix = that.m_audioPrefix;
	m_audioColorHash = that.m_audioColorHash;
	m_audioPrefixHash = that.m_audioPrefixHash;
	m_colorName = StringDuplicate(that.m_colorName);
	m_rampTextureName = StringDuplicate(that.m_rampTextureName);
}

CVehicleModelColorGen9::~CVehicleModelColorGen9()
{
	StringFree(m_colorName);
	StringFree(m_rampTextureName);
}

void CVehicleModelColorGen9::UpdateValuesFromHashes()
{
#if 0	// use this version if we change the hashing of our parser strings to use the normalized hash (that the audio system uses)
	parMemberEnum audioColorEnum = parMemberEnum(parser_CVehicleModelColor_audioColor);
	int audioColor = audioColorEnum.ValueFromName(m_audioColorHash);
	m_audioColor = (audioColor>=0) ? u8(audioColor) : CVehicleModelColor::EVehicleModelAudioColor_none;

	parMemberEnum audioPrefixEnum = parMemberEnum(parser_CVehicleModelColor_audioPrefix);
	int audioPrefix = (u8)audioPrefixEnum.ValueFromName(m_audioPrefixHash);
	m_audioPrefix = (audioPrefix>=0) ? u8(audioPrefix) : CVehicleModelColor::EVehicleModelAudioPrefix_none;
#else
	m_audioColor = CVehicleModelColor::EVehicleModelAudioColor_black;
	if (m_audioColorHash != 0)
	{
		for(int i=0;i<parser_CVehicleModelColor_audioColor.m_EnumData->m_NumEnums;i++)
		{
			const u32 h = atStringHash(parser_CVehicleModelColor_audioColor.m_EnumData->m_Names[i]);
			if (h==m_audioColorHash)
			{
				m_audioColor = (u8) i;
				break;
			}
		}
	}
	m_audioPrefix = CVehicleModelColor::EVehicleModelAudioPrefix_none;
	if (m_audioPrefixHash != 0)
	{
		for(int i=0;i<parser_CVehicleModelColor_audioPrefix.m_EnumData->m_NumEnums;i++)
		{
			const u32 h = atStringHash(parser_CVehicleModelColor_audioPrefix.m_EnumData->m_Names[i]);
			if (h==m_audioPrefixHash)
			{
				m_audioPrefix = (u8) i;
				break;
			}
		}
	}
#endif
}


void CVehicleModelColorGen9::UpdateHashesFromValues()
{
#if 0	// use this version if we change the hashing of our parser strings to use the normalized hash (that the audio system uses)
	parMemberEnum audioColorEnum = parMemberEnum(parser_CVehicleModelColor_audioColor);
	m_audioColorHash = audioColorEnum.HashFromValue(m_audioColor);
	Assert(m_audioColorHash!=0);

	if (m_audioPrefix== CVehicleModelColor::EVehicleModelAudioPrefix_none)
		m_audioPrefixHash = 0;
	else
	{
		parMemberEnum audioPrefixEnum = parMemberEnum(parser_CVehicleModelColor_audioPrefix);
		m_audioPrefixHash = audioPrefixEnum.HashFromValue(m_audioPrefix );
		Assert(m_audioPrefixHash!=0);
	}
#else
	if (AssertVerify(m_audioColor<parser_CVehicleModelColor_audioColor.m_EnumData->m_NumEnums))
		m_audioColorHash = atStringHash(parser_CVehicleModelColor_audioColor.m_EnumData->m_Names[m_audioColor]);
	else
		m_audioColorHash = 0;

	if (m_audioPrefix== CVehicleModelColor::EVehicleModelAudioPrefix_none)
		// the 'none' prefix has a 0 hash value
		m_audioPrefixHash = 0;
	else if (AssertVerify(m_audioPrefix<parser_CVehicleModelColor_audioPrefix.m_EnumData->m_NumEnums))
		m_audioPrefixHash = atStringHash(parser_CVehicleModelColor_audioPrefix.m_EnumData->m_Names[m_audioPrefix]);
	else
		m_audioPrefixHash = 0;
#endif
}

#if __BANK

#if VEHICLE_SUPPORT_PAINT_RAMP
static int CVehicleModelColor_tempRampIdx;
static void WidgetRampTextureChanged(CallbackData obj)
{
	CVehicleModelColorGen9 *t = (CVehicleModelColorGen9 *)obj;

	fwTxd *rampTxd = CCustomShaderEffectVehicle::GetRampTxd();
	if (Verifyf(rampTxd, "Missing ramp txd"))
	{
		StringFree(t->m_rampTextureName);
		t->m_rampTextureName = 0;

		if (CVehicleModelColor_tempRampIdx)
		{
			grcTexture *rampTex = rampTxd->GetEntry(CVehicleModelColor_tempRampIdx - 1);
			t->m_rampTextureName = StringDuplicate(rampTex->GetName());
		}
	}

	CVehicleModelInfo::RefreshAllVehicleBodyColors();
}
#endif

static char CVehicleModelColor_tempName[64];			//HACK: stops us from displaying more than one ModelColor widget at a time - but does make it easier to display the name, rework if we need more than one color widget at a time
void CVehicleModelColorGen9::AddWidgets(bkBank & bank)
{
	if (m_colorName)
	{
		strncpy(CVehicleModelColor_tempName, m_colorName, sizeof(CVehicleModelColor_tempName));
		CVehicleModelColor_tempName[sizeof(CVehicleModelColor_tempName)-1] = 0;
	}
	else
		CVehicleModelColor_tempName[0] = 0;
		
	bank.AddColor("Colour", &m_color, datCallback(CFA1(CVehicleModelColorGen9::WidgetColorChangedCB),(CallbackData)this));
	bank.AddText("Name", &CVehicleModelColor_tempName[0], sizeof(CVehicleModelColor_tempName)-1, false, datCallback(CFA1(CVehicleModelColorGen9::WidgetNameChangedCB),(CallbackData)this));
	bank.AddCombo("Metallic Setting", &m_metallicID, parser_EVehicleModelColorMetallicID_Count, parser_EVehicleModelColorMetallicID_Strings, /*offset*/-1, datCallback(CVehicleModelInfo::RefreshAllVehicleBodyColors));
	bank.AddCombo("Audio Color", &m_audioColor, parser_EVehicleModelAudioColor_Count, parser_EVehicleModelAudioColor_Strings, 0, datCallback(CFA1(CVehicleModelColorGen9::UpdateHashesFromValuesCB),(CallbackData)this));
	bank.AddCombo("Audio Prefix", &m_audioPrefix, parser_EVehicleModelAudioPrefix_Count, parser_EVehicleModelAudioPrefix_Strings, 0, datCallback(CFA1(CVehicleModelColorGen9::UpdateHashesFromValuesCB),(CallbackData)this));

#if VEHICLE_SUPPORT_PAINT_RAMP
	fwTxd *rampTxd = CCustomShaderEffectVehicle::GetRampTxd();
	if (Verifyf(rampTxd, "Missing ramp txd"))
	{
		const char *rampNames[256];
		int rampNum = rampTxd->GetCount() + 1;
		Assert(rampNum <= NELEM(rampNames));

		rampNames[0] = "none";
		CVehicleModelColor_tempRampIdx = 0;

		for (int i = 1; i < rampNum && i < NELEM(rampNames); ++i)
		{
			grcTexture *tex = rampTxd->GetEntry(i - 1);
			rampNames[i] = tex->GetName();

			if (m_rampTextureName && !strcmp(m_rampTextureName, rampNames[i]))
			{
				Assert(!CVehicleModelColor_tempRampIdx);
				CVehicleModelColor_tempRampIdx = i;
			}
		}

		bank.AddCombo("Ramp Texture", &CVehicleModelColor_tempRampIdx, rampNum, rampNames, datCallback(CFA1(WidgetRampTextureChanged), (CallbackData)this));
	}
#endif
}

void CVehicleModelColorGen9::WidgetNameChangedCB(CallbackData obj)
{
	CVehicleModelColorGen9 * t = ((CVehicleModelColorGen9*)obj);
	StringFree(t->m_colorName);
	t->m_colorName = StringDuplicate((const char*)CVehicleModelColor_tempName);

	CVehicleModelInfo::RefreshVehicleWidgets();
}

void CVehicleModelColorGen9::UpdateHashesFromValuesCB(CallbackData obj)
{
	((CVehicleModelColorGen9*)obj)->UpdateHashesFromValues();
}

void CVehicleModelColorGen9::WidgetColorChangedCB(CallbackData )
{
	CVehicleModelInfo::RefreshAllVehicleBodyColors();
}


#endif // __BANK

CVehicleWindowColor::CVehicleWindowColor()
{
	m_color.Set(0);
	m_name = atHashString("none",0x1D632BA1);
}

#if __BANK
static char CVehicleWindowColor_tempName[64];			//HACK: stops us from displaying more than one WindowColor widget at a time - but does make it easier to display the name, rework if we need more than one color widget at a time
void CVehicleWindowColor::AddWidgets(bkBank& bank)
{
	if (m_name.GetCStr())
	{
		strncpy(CVehicleWindowColor_tempName, m_name.GetCStr(), sizeof(CVehicleWindowColor_tempName));
		CVehicleWindowColor_tempName[sizeof(CVehicleWindowColor_tempName)-1] = 0;
	}
	else
		CVehicleWindowColor_tempName[0] = 0;
	bank.AddColor("Color", &m_color);
	bank.AddText("Name", &CVehicleWindowColor_tempName[0], sizeof(CVehicleWindowColor_tempName)-1, false, datCallback(CFA1(CVehicleWindowColor::WidgetNameChangedCB),(CallbackData)this));
}

void CVehicleWindowColor::WidgetNameChangedCB(CallbackData obj)
{
	CVehicleWindowColor* t = ((CVehicleWindowColor*)obj);
	t->m_name = atHashString(CVehicleWindowColor_tempName);

	CVehicleModelInfo::RefreshVehicleWidgets();
}
#endif // __BANK

CVehicleModelColorIndices::CVehicleModelColorIndices()
{
	for(u32 i=0; i<NUM_VEH_BASE_COLOURS; i++)
	{
		m_indices[i] = 0;
	}

	for(u32 i=0; i<MAX_NUM_LIVERIES; i++)
	{
		m_liveries[i] = false;
	}
}

void CVehicleModVisible::PostLoad()
{
	for (s32 i = 0; i < m_turnOffBones.GetCount(); ++i)
		if (m_turnOffBones[i] == chassis)
			m_turnOffBones[i] = none;
}

CVehicleModelInfoVarGlobal::CVehicleModelInfoVarGlobal()
{
	// Reserve enough for DLC kits here as kit pointers are cached around the code, resizing can lead to crashes...
	m_Kits.Reserve(160);
	
	// Ditto for DLC wheels. 160 wheels for each wheel **seems** enough...
	for (int wheelType = 0; wheelType < VWT_MAX; ++wheelType) {
		m_Wheels[wheelType].Reserve(160);
	}

#if __BANK
	m_VehicleColorEditingIndex = 0;
	m_VehicleWindowColorEditingIndex = 0;
	m_VehicleModelEditingIndex = 0;
	m_VehicleLightEditingIndex = 0;
	m_VehicleSirenEditingIndex = 0;
	m_VehicleSirenCopyReady = false;
	m_VehicleCopyFromIndex = 0;
	m_VehicleColorBankGroup = NULL;
	m_VehicleWindowColorBankGroup = NULL;
	m_VehicleModelBankGroup = NULL;
	m_VehicleLightsBankGroup = NULL;
	m_VehicleSirensBankGroup = NULL;
	m_VehicleCurrentColorBankGroup = NULL;
	m_VehicleCurrentWindowColorBankGroup = NULL;
	m_VehicleCurrentModelBankGroup = NULL;
	m_VehicleCurrentLightsBankGroup = NULL;
	m_VehicleCurrentSirensBankGroup = NULL;
	m_ColorNameCombo = NULL;
	m_WindowColorNameCombo = NULL;
	m_LightNameCombo = NULL;
	m_SirenNameCombo = NULL;
#endif // __BANK
};

CVehicleModelInfoVarGlobal::~CVehicleModelInfoVarGlobal()
{
#if __BANK
	bkBank * rootBank = BANKMGR.FindBank("Vehicles");
	if (rootBank && m_VehicleCurrentColorBankGroup)
		rootBank->DeleteGroup(*m_VehicleCurrentColorBankGroup);
	if (rootBank && m_VehicleCurrentWindowColorBankGroup)
		rootBank->DeleteGroup(*m_VehicleCurrentWindowColorBankGroup);
	if (rootBank && m_VehicleCurrentModelBankGroup)
		rootBank->DeleteGroup(*m_VehicleCurrentModelBankGroup);
	if (rootBank && m_VehicleColorBankGroup)
		rootBank->DeleteGroup(*m_VehicleColorBankGroup);
	if (rootBank && m_VehicleModelBankGroup)
		rootBank->DeleteGroup(*m_VehicleModelBankGroup);
	DeleteWidgetNames(m_VehicleNames);
	DeleteWidgetNames(m_ColorNames);
	DeleteWidgetNames(m_WindowColorNames);
	DeleteWidgetNames(m_LightNames);
	DeleteWidgetNames(m_SirenNames);
#endif
}

void CVehicleModelInfoVarGlobal::Init()
{
	for(int id = 0; id < NELEM(m_KitsIdMap); id++)
	{
		m_KitsIdMap[id] = INVALID_VEHICLE_KIT_ID;
	}

	for(int id = 0; id < NELEM(m_LightsIdMap); id++)
	{
		m_LightsIdMap[id] = INVALID_VEHICLE_LIGHT_SETTINGS_ID;		
	}

	for(int id = 0; id < NELEM(m_SirensIdMap); id++)
	{
		m_SirensIdMap[id] = INVALID_SIREN_SETTINGS_ID;
	}
}

VehicleKitId CVehicleModelInfoVarGlobal::GenerateKitId(u16 index) const
{
	Assert(index != INVALID_VEHICLE_KIT_ID);

	for(int id = 0; id < NELEM(m_KitsIdMap); id++)
	{
		if(m_KitsIdMap[id] == INVALID_VEHICLE_KIT_ID)
		{
			m_KitsIdMap[id] = index;
			return (VehicleKitId)id;	
		}
	}

	return INVALID_VEHICLE_KIT_ID;
}

vehicleLightSettingsId CVehicleModelInfoVarGlobal::GenerateLightId(u8 index) const
{
	Assert(index != INVALID_VEHICLE_LIGHT_SETTINGS_ID);

	for(int id = 0; id < NELEM(m_LightsIdMap); id++)
	{
		if(m_LightsIdMap[id] == INVALID_VEHICLE_LIGHT_SETTINGS_ID)
		{
			m_LightsIdMap[id] = index;
			return (vehicleLightSettingsId)id;	
		}
	}

	return INVALID_VEHICLE_LIGHT_SETTINGS_ID;
}

sirenSettingsId CVehicleModelInfoVarGlobal::GenerateSirenId(u8 index) const
{
	Assert(index != INVALID_SIREN_SETTINGS_ID);

	for(int id = 0; id < NELEM(m_SirensIdMap); id++)
	{
		if(m_SirensIdMap[id] == INVALID_SIREN_SETTINGS_ID)
		{
			m_SirensIdMap[id] = (sirenSettingsId)index;
			return (sirenSettingsId)id;	
		}
	}

	return INVALID_SIREN_SETTINGS_ID;
}

void CVehicleModelInfoVarGlobal::OnPostLoad(BANK_ONLY(const char* filename))
{
	s_VehicleModelColorsAll.Reset();

	s_VehicleModelColorBaseNum = m_Colors.GetCount();
	for (u32 i = 0; i < s_VehicleModelColorBaseNum; ++i)
	{
		CVehicleModelColorGen9& c = s_VehicleModelColorsAll.Grow();
		c.InitFromBase(m_Colors[i]);
	}

	UpdateKitIds(0, m_Kits.GetCount() BANK_ONLY(, filename));
	UpdateLightIds(0, m_Lights.GetCount() BANK_ONLY(, filename));
	UpdateSirenIds(0, m_Sirens.GetCount() BANK_ONLY(, filename));
}

void CVehicleModelInfoVarGlobal::UpdateKitIds(int start, int count BANK_ONLY(, const char* fileName))
{
	Assert((start + count) <= m_Kits.GetCount());

	for(int i = start; i < (start + count); i++)
	{
		if(m_Kits[i].GetId() == INVALID_VEHICLE_KIT_ID)
		{
			if(start > 0) // DLC
			{
				Assertf(m_Kits[i].GetId() != INVALID_VEHICLE_KIT_ID, "DLC vehicle kit '%s' doesn't have unique id", m_Kits[i].GetNameHashString().TryGetCStr());
			}

			m_Kits[i].SetId(GenerateKitId((u16)i));
		}
		else if(!AssertVerify(m_KitsIdMap[m_Kits[i].GetId()] == INVALID_VEHICLE_KIT_ID))
		{
			Warningf("CVehicleModelInfoVarGlobal found duplicate Kit id = %d!", m_Kits[i].GetId());
			m_Kits[i].SetId(GenerateKitId((u16)i));
			Warningf("CVehicleModelInfoVarGlobal found duplicate Kit id = %d!", m_Kits[i].GetId());
		}

		Assert(m_Kits[i].GetId() != INVALID_VEHICLE_KIT_ID);
		m_KitsIdMap[m_Kits[i].GetId()] = (u16)i;

#if __BANK
		if(fileName)
			COwnershipInfo<CVehicleKit, VehicleKitId>::Add(m_Kits[i].GetId(), fileName);
#endif // __BANK
	}
}

void CVehicleModelInfoVarGlobal::UpdateLightIds(int start, int count BANK_ONLY(, const char* fileName))
{
	Assert((start + count) <= m_Lights.GetCount());

	for(int i = start; i < (start + count); i++)
	{
		if(m_Lights[i].id == INVALID_VEHICLE_LIGHT_SETTINGS_ID)
		{
			if(start > 0) // DLC
			{
				Assertf(m_Lights[i].id != INVALID_VEHICLE_LIGHT_SETTINGS_ID, "DLC vehicle light '%s' doesn't have unique id", m_Lights[i].name);
			}

			m_Lights[i].id = GenerateLightId((u8)i);
		}
		else if(!AssertVerify(m_LightsIdMap[m_Lights[i].id] == INVALID_VEHICLE_LIGHT_SETTINGS_ID))
		{
			Warningf("CVehicleModelInfoVarGlobal found duplicate Light id = %d!", m_Lights[i].id);
			m_Lights[i].id = GenerateLightId((u8)i);
			Warningf("CVehicleModelInfoVarGlobal found duplicate Light id = %d!", m_Lights[i].id);
		}

		Assert(m_Lights[i].id != INVALID_VEHICLE_LIGHT_SETTINGS_ID);
		m_LightsIdMap[m_Lights[i].id] = (u8)i;

#if __BANK
		if(fileName)
			COwnershipInfo<vehicleLightSettings, vehicleLightSettingsId>::Add(m_Lights[i].GetId(), fileName);
#endif // __BANK
	}
}

void CVehicleModelInfoVarGlobal::UpdateSirenIds(int start, int count BANK_ONLY(, const char* fileName))
{
	Assert((start + count) <= m_Sirens.GetCount());

	for(int i = start; i < (start + count); i++)
	{
		if(m_Sirens[i].id == INVALID_SIREN_SETTINGS_ID)
		{
			if(start > 0) // DLC
			{
				Assertf(m_Sirens[i].id != INVALID_SIREN_SETTINGS_ID, "DLC vehicle siren '%s' doesn't have unique id", m_Sirens[i].name);
			}

			m_Sirens[i].id = GenerateSirenId((u8)i);
		}
		else if(!AssertVerify(m_SirensIdMap[m_Sirens[i].id] == INVALID_SIREN_SETTINGS_ID))
		{
			Warningf("CVehicleModelInfoVarGlobal found duplicate Siren id = %d!", m_Sirens[i].id);
			m_Sirens[i].id = GenerateSirenId((u8)i);
			Warningf("CVehicleModelInfoVarGlobal found duplicate Siren id = %d!", m_Sirens[i].id);
		}

		Assert(m_Sirens[i].id != INVALID_SIREN_SETTINGS_ID);
		m_SirensIdMap[m_Sirens[i].id] = (u8)i;

#if __BANK
		if(fileName)
			COwnershipInfo<sirenSettings, sirenSettingsId>::Add(m_Sirens[i].GetId(), fileName);
#endif // __BANK
	}
}

void CVehicleModelInfoVarGlobal::UpdateWheelIds(eVehicleWheelType wheelType, int start, int count BANK_ONLY(, char const *filename)) {
	Assert((start + count) <= m_Wheels[wheelType].GetCount());

	for(int i = start; i < (start + count); i++)
	{
		if(!AssertVerify(m_WheelsIdMap[wheelType].Access(m_Wheels[wheelType][i].GetId()) == NULL))
		{
			Warningf("CVehicleModelInfoVarGlobal found duplicate Wheels id = %d!", m_Wheels[wheelType][i].GetId());
		}

		Assert(m_Wheels[wheelType][i].GetId() != INVALID_VEHICLE_WHEEL_ID);
		m_WheelsIdMap[wheelType].Insert(m_Wheels[wheelType][i].GetId(), (u8)i);

#if __BANK
		if(filename)
			COwnershipInfo<CVehicleWheel, vehicleWheelId>::Add(m_Wheels[wheelType][i].GetId(), filename);
#endif // __BANK
	}
}

void CVehicleModelInfoVarGlobal::OnPreSave()
{
}

#if __BANK
static void SaveVehicleColorsGen9()
{
	CVehicleModelColorsGen9 cols;
	u32 colNum = s_VehicleModelColorsAll.GetCount();
	for (u32 i = s_VehicleModelColorBaseNum; i < colNum; ++i)
	{
		CVehicleModelColorGen9& c = cols.m_Colors.Grow();
		c = s_VehicleModelColorsAll[i];
	}

	const char *filename = "update:/common/data/carcols_gen9.meta";
	const fiDevice *device = fiDevice::GetDevice(filename);
	if(Verifyf(device, "Couldn't get device for %s", filename))
	{
		char path[RAGE_MAX_PATH];
		device->FixRelativeName(path, RAGE_MAX_PATH, filename);
		Verifyf(PARSER.SaveObject(path, "meta", &cols, parManager::XML), "Failed to save carcols_gen9.meta");
	}
}

void CVehicleModelInfoVarGlobal::AddWidgets(bkBank & bank)
{
	// copy out the colour names for widgets
	DeleteWidgetNames(m_ColorNames);
	m_ColorNames.Reserve(kMAX_NUM_COLORS);		// reserve the maximum possible number of colors
	if (m_ColorNameCombo)
		m_ColorNameCombo->Destroy();
	RefreshColorNames();

	DeleteWidgetNames(m_WindowColorNames);
	m_WindowColorNames.Reserve(kMAX_NUM_COLORS);		// reserve the maximum possible number of colors
	if (m_WindowColorNameCombo)
		m_WindowColorNameCombo->Destroy();
	RefreshWindowColorNames();

	DeleteWidgetNames(m_LightNames);
	m_LightNames.Reserve(kMAX_NUM_LIGHTS);		// reserve the maximum possible number of lights
	if (m_LightNameCombo)
		m_LightNameCombo->Destroy();
	RefreshLightNames();

	DeleteWidgetNames(m_SirenNames);
	m_SirenNames.Reserve(kMAX_NUM_SIRENS);		// reserve the maximum possible number of lights
	if (m_SirenNameCombo)
		m_SirenNameCombo->Destroy();
	RefreshSirenNames();
	
	// Combo box + Save
	const char* stringArray[64];
	for(int i=0; i<ms_carcolFiles.GetCount(); ++i)
	{
		stringArray[i] = ms_carcolFiles[i].TryGetCStr();
	}
	bank.AddCombo("Car colours file", &ms_fileSelection, ms_carcolFiles.GetCount(), stringArray);
	bank.AddButton("Save", datCallback(MFA(CVehicleModelInfoVarGlobal::WidgetSave),this));
	bank.AddButton("Save Gen9 colours", datCallback(SaveVehicleColorsGen9));
	bank.AddSeparator();
	bank.AddButton("Save All", datCallback(MFA(CVehicleModelInfoVarGlobal::WidgetSaveAll),this));

	bank.AddSeparator();
	m_VehicleColorBankGroup = bank.PushGroup("Colours", false);
		m_ColorNameCombo = bank.AddCombo("Current Colour", &m_VehicleColorEditingIndex, m_ColorNames.GetCount(), m_ColorNames.GetElements(), datCallback(MFA(CVehicleModelInfoVarGlobal::WidgetChangedColorEditIndex),this));
		bank.AddButton("Add Colour", datCallback(MFA(CVehicleModelInfoVarGlobal::WidgetAddColor), this));
		bank.AddButton("Delete Colour", datCallback(MFA(CVehicleModelInfoVarGlobal::WidgetDeleteColor), this));
	bank.PopGroup();

	bank.AddSeparator();
	m_VehicleWindowColorBankGroup = bank.PushGroup("Window Colors", false);
	m_WindowColorNameCombo = bank.AddCombo("Current Window Colour", &m_VehicleWindowColorEditingIndex, m_WindowColorNames.GetCount(), m_WindowColorNames.GetElements(), datCallback(MFA(CVehicleModelInfoVarGlobal::WidgetChangedWindowColorEditIndex),this));
	bank.AddButton("Add Window Colour", datCallback(MFA(CVehicleModelInfoVarGlobal::WidgetAddWindowColor), this));
	bank.AddButton("Delete Window Colour", datCallback(MFA(CVehicleModelInfoVarGlobal::WidgetDeleteWindowColor), this));
	bank.PopGroup();

	m_VehicleLightsBankGroup = bank.PushGroup("Lights", false);
		m_LightNameCombo = bank.AddCombo("Current Light", &m_VehicleLightEditingIndex, m_LightNames.GetCount(), m_LightNames.GetElements(), datCallback(MFA(CVehicleModelInfoVarGlobal::WidgetChangedLightEditIndex),this));
		bank.AddButton("Add Light Settings", datCallback(MFA(CVehicleModelInfoVarGlobal::WidgetAddLights), this));
		bank.AddButton("Delete Light Settings", datCallback(MFA(CVehicleModelInfoVarGlobal::WidgetDeleteLights), this));
	bank.PopGroup();

	m_VehicleSirensBankGroup = bank.PushGroup("Sirens", false);
		m_SirenNameCombo = bank.AddCombo("Current Siren", &m_VehicleSirenEditingIndex, m_SirenNames.GetCount(), m_SirenNames.GetElements(), datCallback(MFA(CVehicleModelInfoVarGlobal::WidgetChangedSirenEditIndex),this));
		bank.AddButton("Add Siren Settings", datCallback(MFA(CVehicleModelInfoVarGlobal::WidgetAddSirens), this));
		bank.AddButton("Delete Siren Settings", datCallback(MFA(CVehicleModelInfoVarGlobal::WidgetDeleteSirens), this));
		bank.AddButton("Copy Siren Settings", datCallback(MFA(CVehicleModelInfoVarGlobal::WidgetCopySirens), this));
		bank.AddButton("Paste Siren Settings", datCallback(MFA(CVehicleModelInfoVarGlobal::WidgetPasteSirens), this));
	bank.PopGroup();
}

void CVehicleModelInfoVarGlobal::WidgetSave()
{
	CVehicleModelInfo::SaveVehicleColours(ms_carcolFiles[ms_fileSelection].TryGetCStr());
}

void CVehicleModelInfoVarGlobal::WidgetSaveAll()
{
	for(int i=0; i<ms_carcolFiles.GetCount(); ++i)
		CVehicleModelInfo::SaveVehicleColours(ms_carcolFiles[i].TryGetCStr());
}

void CVehicleModelInfoVarGlobal::DeleteWidgetNames(atArray<const char*> & nameArray)
{
	for(int i=0;i<nameArray.GetCount();i++)
	{
		if (nameArray[i])
		{
			delete [] nameArray[i];
			nameArray[i] = NULL;
		}
	}
	nameArray.Reset();
}


void CVehicleModelInfoVarGlobal::WidgetChangedColorEditIndex()
{
	if (m_VehicleColorEditingIndex >= GetColorCount())
		m_VehicleColorEditingIndex = GetColorCount()-1;

	// delete the old widgets
	bkBank * rootBank = BANKMGR.FindBank("Vehicles");
	if (rootBank && m_VehicleCurrentColorBankGroup)
		rootBank->DeleteGroup(*m_VehicleCurrentColorBankGroup);
	m_VehicleCurrentColorBankGroup = NULL;

	if (GetColorCount() > 0)
	{
		if (rootBank && m_VehicleColorBankGroup)
		{
			m_VehicleCurrentColorBankGroup = m_VehicleColorBankGroup->AddGroup("Colour Data", true);
			rootBank->SetCurrentGroup(*m_VehicleCurrentColorBankGroup);
			GetColors()[m_VehicleColorEditingIndex].AddWidgets(*rootBank);
			rootBank->UnSetCurrentGroup(*m_VehicleCurrentColorBankGroup);
		}
	}
	else
		m_VehicleColorEditingIndex = 0;
}

void CVehicleModelInfoVarGlobal::WidgetChangedWindowColorEditIndex()
{
	if (m_VehicleWindowColorEditingIndex >= m_WindowColors.GetCount())
		m_VehicleWindowColorEditingIndex = m_WindowColors.GetCount()-1;

	// delete the old widgets
	bkBank * rootBank = BANKMGR.FindBank("Vehicles");
	if (rootBank && m_VehicleCurrentWindowColorBankGroup)
		rootBank->DeleteGroup(*m_VehicleCurrentWindowColorBankGroup);
	m_VehicleCurrentWindowColorBankGroup = NULL;

	if (m_WindowColors.GetCount() > 0)
	{
		if (rootBank && m_VehicleWindowColorBankGroup)
		{
			m_VehicleCurrentWindowColorBankGroup = m_VehicleWindowColorBankGroup->AddGroup("Colour Data", true);
			rootBank->SetCurrentGroup(*m_VehicleCurrentWindowColorBankGroup);
			m_WindowColors[m_VehicleWindowColorEditingIndex].AddWidgets(*rootBank);
			rootBank->UnSetCurrentGroup(*m_VehicleCurrentWindowColorBankGroup);
		}
	}
	else
		m_VehicleWindowColorEditingIndex = 0;
}

void CVehicleModelInfoVarGlobal::WidgetAddColor()
{
	// delete the old widgets
	bkBank * rootBank = BANKMGR.FindBank("Vehicles");
	if (rootBank && m_VehicleCurrentColorBankGroup)
		rootBank->DeleteGroup(*m_VehicleCurrentColorBankGroup);
	m_VehicleCurrentColorBankGroup = NULL;
	if (rootBank && m_VehicleCurrentModelBankGroup)
		rootBank->DeleteGroup(*m_VehicleCurrentModelBankGroup);
	m_VehicleCurrentModelBankGroup = NULL;

	if (GetColorCount()>kMAX_NUM_COLORS)	// 8bit values holding color index
		return;
	CVehicleModelColorGen9 & c = GetColors().Grow();
	char temp[16];
	formatf(temp,15," %d ", GetColorCount()-1);
	c.m_colorName = StringDuplicate(temp);
	m_VehicleColorEditingIndex = GetColorCount()-1;

	WidgetChangedColorEditIndex();
	RefreshVehicleWidgets();
}

void CVehicleModelInfoVarGlobal::WidgetAddWindowColor()
{
	// delete the old widgets
	bkBank * rootBank = BANKMGR.FindBank("Vehicles");
	if (rootBank && m_VehicleCurrentWindowColorBankGroup)
		rootBank->DeleteGroup(*m_VehicleCurrentWindowColorBankGroup);
	m_VehicleCurrentWindowColorBankGroup = NULL;

	if (m_WindowColors.GetCount()>kMAX_NUM_COLORS)	// 8bit values holding color index
		return;
	CVehicleWindowColor & c = m_WindowColors.Grow();
	char temp[16];
	formatf(temp,15," %d ", m_WindowColors.GetCount()-1);
	c.m_name = atHashString(temp);
	m_VehicleWindowColorEditingIndex = m_WindowColors.GetCount()-1;

	WidgetChangedWindowColorEditIndex();
	RefreshVehicleWidgets();
}

void CVehicleModelInfoVarGlobal::WidgetDeleteColor()
{
	if (GetColorCount()<=0 || m_VehicleColorEditingIndex >= GetColorCount())
		return;

	// reshuffle all the possible colors
	CVehicleModelInfo::GetVehicleVariations()->ReshuffleColors(m_VehicleColorEditingIndex);

	// change the names
	for(int i=0;i<GetColorCount();i++)
	{
		bool hasNumberAtStart = false;
		char * c = GetColors()[i].m_colorName;
		while (isspace(*c))
			c++;
		char * numStart = c;
		while (isdigit(*c))
		{
			c++;
			hasNumberAtStart = true;
		}
		if (hasNumberAtStart)
		{
			char tmp = *c;
			*c = 0;	// terminate string ready for atoi
			int index = atoi(numStart);
			*c = tmp;
			if (index != i)
				Printf("Invalid index %d on color %s at position %d\n", index, GetColors()[i].m_colorName, i);
			if (i > m_VehicleColorEditingIndex)
			{
				Printf("Changing index of color %s to %d\n", GetColors()[i].m_colorName, i-1);
				char temp[512];
				formatf(temp,511," %d%s", i-1,c);	temp[511]=0;
				StringFree(GetColors()[i].m_colorName);
				GetColors()[i].m_colorName = StringDuplicate(temp);
			}
		}
	}
	GetColors().Delete(m_VehicleColorEditingIndex);
	RefreshVehicleWidgets();
	CVehicleModelInfo::RefreshAllVehicleBodyColors();
}

void CVehicleModelInfoVarGlobal::WidgetDeleteWindowColor()
{
	if (m_WindowColors.GetCount()<=0 || m_VehicleWindowColorEditingIndex >= m_WindowColors.GetCount())
		return;

	m_WindowColors.Delete(m_VehicleWindowColorEditingIndex);
	RefreshVehicleWidgets();
}


void CVehicleModelInfoVarGlobal::WidgetChangedLightEditIndex()
{
	if (m_VehicleLightEditingIndex >= m_Lights.GetCount())
		m_VehicleLightEditingIndex = m_Lights.GetCount()-1;

	// delete the old widgets
	bkBank * rootBank = BANKMGR.FindBank("Vehicles");
	if (rootBank && m_VehicleCurrentLightsBankGroup)
		rootBank->DeleteGroup(*m_VehicleCurrentLightsBankGroup);
	m_VehicleCurrentLightsBankGroup = NULL;

	if (m_Lights.GetCount() > 0)
	{
		if (rootBank && m_VehicleLightsBankGroup)
		{
			m_VehicleCurrentLightsBankGroup = m_VehicleLightsBankGroup->AddGroup("Light Data", true);
			rootBank->SetCurrentGroup(*m_VehicleCurrentLightsBankGroup);
			m_Lights[m_VehicleLightEditingIndex].AddWidgets(*rootBank);
			rootBank->UnSetCurrentGroup(*m_VehicleCurrentLightsBankGroup);
		}
	}
	else
		m_VehicleLightEditingIndex = 0;
}

void CVehicleModelInfoVarGlobal::WidgetAddLights()
{
	// delete the old widgets
	bkBank * rootBank = BANKMGR.FindBank("Vehicles");
	if (rootBank && m_VehicleCurrentLightsBankGroup)
		rootBank->DeleteGroup(*m_VehicleCurrentLightsBankGroup);
	m_VehicleCurrentLightsBankGroup = NULL;
	if (rootBank && m_VehicleCurrentModelBankGroup)
		rootBank->DeleteGroup(*m_VehicleCurrentModelBankGroup);
	m_VehicleCurrentModelBankGroup = NULL;

	if (m_Lights.GetCount()>kMAX_NUM_LIGHTS)	// 8bit values holding color index
		return;
	vehicleLightSettings & light = m_Lights.Grow();
	light.id = GenerateLightId(u8(m_Lights.GetCount() - 1));
	Assert(light.id != INVALID_VEHICLE_LIGHT_SETTINGS_ID);

	char temp[16];
	formatf(temp,15,"%d ", m_Lights.GetCount()-1);
	light.name = StringDuplicate(temp);

#if __BANK
	COwnershipInfo<vehicleLightSettings, vehicleLightSettingsId>::Add(light.GetId(), ms_carcolFiles[ms_fileSelection].TryGetCStr());
#endif // __BANK

	WidgetChangedLightEditIndex();
	RefreshVehicleWidgets();
}

void CVehicleModelInfoVarGlobal::WidgetDeleteLights()
{
	if (m_Lights.GetCount()<=0 || m_VehicleLightEditingIndex >= m_Lights.GetCount())
		return;

	// reshuffle all the possible lights
	CVehicleModelInfo::GetVehicleVariations()->ReshuffleLights(m_VehicleLightEditingIndex);

	// change the names
	for(int i=0;i<m_Lights.GetCount();i++)
	{
		bool hasNumberAtStart = false;
		char * c = m_Lights[i].name;
		while (isspace(*c))
			c++;
		char * numStart = c;
		while (isdigit(*c))
		{
			c++;
			hasNumberAtStart = true;
		}
		if (hasNumberAtStart)
		{
			char tmp = *c;
			*c = 0;	// terminate string ready for atoi
			int index = atoi(numStart);
			*c = tmp;
			if (index != i)
				Printf("Invalid index %d on light %s at position %d\n", index, m_Lights[i].name, i);
			if (i > m_VehicleColorEditingIndex)
			{
				Printf("Changing index of light %s to %d\n", m_Lights[i].name, i-1);
				char temp[512];
				formatf(temp,511," %d%s", i-1,c);	temp[511]=0;
				StringFree(m_Lights[i].name);
				m_Lights[i].name = StringDuplicate(temp);
			}
		}
	}

#if __BANK
	COwnershipInfo<vehicleLightSettings, vehicleLightSettingsId>::Remove(m_Lights[m_VehicleLightEditingIndex].GetId());
#endif // __BANK

	Assert(m_Lights[m_VehicleLightEditingIndex].id != INVALID_VEHICLE_LIGHT_SETTINGS_ID);
	m_LightsIdMap[m_Lights[m_VehicleLightEditingIndex].id] = INVALID_VEHICLE_LIGHT_SETTINGS_ID;
	m_Lights.Delete(m_VehicleLightEditingIndex);
	RefreshVehicleWidgets();
	CVehicleModelInfo::RefreshAllVehicleLightSettings();
}

void CVehicleModelInfoVarGlobal::WidgetChangedSirenEditIndex()
{
	if (m_VehicleSirenEditingIndex >= m_Sirens.GetCount())
		m_VehicleSirenEditingIndex = m_Sirens.GetCount()-1;

	// delete the old widgets
	bkBank * rootBank = BANKMGR.FindBank("Vehicles");
	if (rootBank && m_VehicleCurrentSirensBankGroup)
		rootBank->DeleteGroup(*m_VehicleCurrentSirensBankGroup);
	m_VehicleCurrentSirensBankGroup = NULL;

	if (m_Sirens.GetCount() > 0)
	{
		if (rootBank && m_VehicleSirensBankGroup)
		{
			m_VehicleCurrentSirensBankGroup = m_VehicleSirensBankGroup->AddGroup("Siren Data", true);
			rootBank->SetCurrentGroup(*m_VehicleCurrentSirensBankGroup);
			m_Sirens[m_VehicleSirenEditingIndex].AddWidget(rootBank);
			rootBank->UnSetCurrentGroup(*m_VehicleCurrentSirensBankGroup);
		}
	}
	else
		m_VehicleSirenEditingIndex = 0;
}

void CVehicleModelInfoVarGlobal::WidgetAddSirens()
{
	// delete the old widgets
	bkBank * rootBank = BANKMGR.FindBank("Vehicles");
	if (rootBank && m_VehicleCurrentSirensBankGroup)
		rootBank->DeleteGroup(*m_VehicleCurrentSirensBankGroup);
	m_VehicleCurrentSirensBankGroup = NULL;
	if (rootBank && m_VehicleCurrentModelBankGroup)
		rootBank->DeleteGroup(*m_VehicleCurrentModelBankGroup);
	m_VehicleCurrentModelBankGroup = NULL;

	if (m_Sirens.GetCount()>kMAX_NUM_SIRENS)	// 8bit values holding color index
		return;
	sirenSettings & Siren = m_Sirens.Grow();
	Siren.id = GenerateSirenId((u8)(m_Sirens.GetCount()-1));
	Assert(Siren.id != INVALID_SIREN_SETTINGS_ID);
	char temp[16];
	formatf(temp,15,"%d ", m_Sirens.GetCount()-1);
	Siren.name = StringDuplicate(temp);
	m_VehicleSirenEditingIndex = m_Sirens.GetCount()-1;
	
#if __BANK
	COwnershipInfo<sirenSettings, sirenSettingsId>::Add(Siren.GetId(), ms_carcolFiles[ms_fileSelection].TryGetCStr());
#endif // __BANK

	WidgetChangedSirenEditIndex();
	RefreshVehicleWidgets();
}

void CVehicleModelInfoVarGlobal::WidgetCopySirens()
{
	if (m_Sirens.GetCount()<=0 || m_VehicleSirenEditingIndex >= m_Sirens.GetCount())
		return;

	m_VehicleSirenCopyData = m_Sirens[m_VehicleSirenEditingIndex];
	m_VehicleSirenCopyReady = true;
}

void CVehicleModelInfoVarGlobal::WidgetPasteSirens()
{
	if (m_Sirens.GetCount()<=0 || m_VehicleSirenEditingIndex >= m_Sirens.GetCount() || m_VehicleSirenCopyReady == false)
		return;
		
	sirenSettingsId id = m_Sirens[m_VehicleSirenEditingIndex].id;
	m_Sirens[m_VehicleSirenEditingIndex] = m_VehicleSirenCopyData;
	m_Sirens[m_VehicleSirenEditingIndex].id = id;
}

void CVehicleModelInfoVarGlobal::WidgetDeleteSirens()
{
	if (m_Sirens.GetCount()<=0 || m_VehicleSirenEditingIndex >= m_Sirens.GetCount())
		return;

	// reshuffle all the possible Sirens
	CVehicleModelInfo::GetVehicleVariations()->ReshuffleLights(m_VehicleLightEditingIndex);

	// change the names
	for(int i=0;i<m_Sirens.GetCount();i++)
	{
		bool hasNumberAtStart = false;
		char * c = m_Sirens[i].name;
		while (isspace(*c))
			c++;
		char * numStart = c;
		while (isdigit(*c))
		{
			c++;
			hasNumberAtStart = true;
		}
		if (hasNumberAtStart)
		{
			char tmp = *c;
			*c = 0;	// terminate string ready for atoi
			int index = atoi(numStart);
			*c = tmp;
			if (index != i)
				Printf("Invalid index %d on siren %s at position %d\n", index, m_Sirens[i].name, i);
			if (i > m_VehicleSirenEditingIndex)
			{
				Printf("Changing index of siren %s to %d\n", m_Sirens[i].name, i-1);
				char temp[512];
				formatf(temp,511," %d%s", i-1,c);	temp[511]=0;
				StringFree(m_Sirens[i].name);
				m_Sirens[i].name = StringDuplicate(temp);
			}
		}
	}

	Assert(m_Sirens[m_VehicleSirenEditingIndex].id != INVALID_SIREN_SETTINGS_ID);
	m_SirensIdMap[m_Sirens[m_VehicleSirenEditingIndex].id] = INVALID_SIREN_SETTINGS_ID;

#if __BANK
	COwnershipInfo<sirenSettings, sirenSettingsId>::Remove(m_Sirens[m_VehicleSirenEditingIndex].GetId());
#endif // __BANK

	m_Sirens.Delete(m_VehicleSirenEditingIndex);

	RefreshVehicleWidgets();
	CVehicleModelInfo::RefreshAllVehicleLightSettings();
}

void CVehicleModelInfoVarGlobal::RefreshColorNames()
{
	int i;
	for(i=0;i<GetColorCount();i++)
	{
		if (i < m_ColorNames.GetCount())
		{
			if (strcmp(m_ColorNames[i], GetColors()[i].m_colorName)!=0)
			{
				StringFree(m_ColorNames[i]);
				m_ColorNames[i] = StringDuplicate(GetColors()[i].m_colorName);
			}
		}
		else
		{
			// need to add new elements to m_ColorNames, this may delete m_ColorNames so beware of pointers to this in widgets
			m_ColorNames.Push(StringDuplicate(GetColors()[i].m_colorName));
		}
	}
	if (i<m_ColorNames.GetCount())
	{
		// need to remove elements from m_ColorNames
		while(i < m_ColorNames.GetCount())
		{
			const int top = m_ColorNames.GetCount()-1;
			StringFree(m_ColorNames[top]);
			m_ColorNames[top] = NULL;
			m_ColorNames.Delete(top);
		}
	}
}

void CVehicleModelInfoVarGlobal::RefreshWindowColorNames()
{
	int i;
	for(i=0;i<m_WindowColors.GetCount();i++)
	{
		if (i < m_WindowColorNames.GetCount())
		{
			if (strcmp(m_WindowColorNames[i], m_WindowColors[i].m_name.GetCStr())!=0)
			{
				StringFree(m_WindowColorNames[i]);
				m_WindowColorNames[i] = StringDuplicate(m_WindowColors[i].m_name.GetCStr());
			}
		}
		else
		{
			// need to add new elements to m_WindowColorNames, this may delete m_WindowColorNames so beware of pointers to this in widgets
			m_WindowColorNames.Push(StringDuplicate(m_WindowColors[i].m_name.GetCStr()));
		}
	}
	if (i<m_WindowColorNames.GetCount())
	{
		// need to remove elements from m_WindowColorNames
		while(i < m_WindowColorNames.GetCount())
		{
			const int top = m_WindowColorNames.GetCount()-1;
			StringFree(m_WindowColorNames[top]);
			m_WindowColorNames[top] = NULL;
			m_WindowColorNames.Delete(top);
		}
	}
}

void CVehicleModelInfoVarGlobal::RefreshLightNames()
{
	int i;
	for(i=0;i<m_Lights.GetCount();i++)
	{
		if (i < m_LightNames.GetCount())
		{
			if (strcmp(m_LightNames[i], m_Lights[i].name)!=0)
			{
				StringFree(m_LightNames[i]);
				m_LightNames[i] = StringDuplicate(m_Lights[i].name);
			}
		}
		else
		{
			// need to add new elements to m_LightNames, this may delete m_LightNames so beware of pointers to this in widgets
			m_LightNames.Push(StringDuplicate(m_Lights[i].name));
		}
	}
	if (i<m_LightNames.GetCount())
	{
		// need to remove elements from m_LightNames
		while(i < m_LightNames.GetCount())
		{
			const int top = m_LightNames.GetCount()-1;
			StringFree(m_LightNames[top]);
			m_LightNames[top] = NULL;
			m_LightNames.Delete(top);
		}
	}
}

void CVehicleModelInfoVarGlobal::RefreshSirenNames()
{
	int i;
	for(i=0;i<m_Sirens.GetCount();i++)
	{
		if (i < m_SirenNames.GetCount())
		{
			if (strcmp(m_SirenNames[i], m_Sirens[i].name)!=0)
			{
				StringFree(m_SirenNames[i]);
				m_SirenNames[i] = StringDuplicate(m_Sirens[i].name);
			}
		}
		else
		{
			// need to add new elements to m_SirenNames, this may delete m_SirenNames so beware of pointers to this in widgets
			m_SirenNames.Push(StringDuplicate(m_Sirens[i].name));
		}
	}
	if (i<m_SirenNames.GetCount())
	{
		// need to remove elements from m_SirenNames
		while(i < m_SirenNames.GetCount())
		{
			const int top = m_SirenNames.GetCount()-1;
			StringFree(m_SirenNames[top]);
			m_SirenNames[top] = NULL;
			m_SirenNames.Delete(top);
		}
	}
}
void CVehicleModelInfoVarGlobal::RefreshVehicleWidgets()
{
	// delete the old widgets
	bkBank * rootBank = BANKMGR.FindBank("Vehicles");
	if (rootBank && m_VehicleCurrentModelBankGroup)
		rootBank->DeleteGroup(*m_VehicleCurrentModelBankGroup);
	m_VehicleCurrentModelBankGroup = NULL;

	RefreshColorNames();
	if (m_ColorNameCombo)
		m_ColorNameCombo->UpdateCombo("Current Colour", &m_VehicleColorEditingIndex, m_ColorNames.GetCount(), m_ColorNames.GetElements(), datCallback(MFA(CVehicleModelInfoVarGlobal::WidgetChangedColorEditIndex),this));

	RefreshWindowColorNames();
	if (m_WindowColorNameCombo)
		m_WindowColorNameCombo->UpdateCombo("Current Window Colour", &m_VehicleWindowColorEditingIndex, m_WindowColorNames.GetCount(), m_WindowColorNames.GetElements(), datCallback(MFA(CVehicleModelInfoVarGlobal::WidgetChangedWindowColorEditIndex),this));

	RefreshLightNames();
	if (m_LightNameCombo)
		m_LightNameCombo->UpdateCombo("Current Light", &m_VehicleLightEditingIndex, m_LightNames.GetCount(), m_LightNames.GetElements(), datCallback(MFA(CVehicleModelInfoVarGlobal::WidgetChangedLightEditIndex),this));

	RefreshSirenNames();
	if (m_SirenNameCombo)
		m_SirenNameCombo->UpdateCombo("Current Siren", &m_VehicleSirenEditingIndex, m_SirenNames.GetCount(), m_SirenNames.GetElements(), datCallback(MFA(CVehicleModelInfoVarGlobal::WidgetChangedSirenEditIndex),this));

	CVehicleFactory::UpdateWindowTintSlider();
}

void CVehicleModelInfoVarGlobal::CreateOwnershipInfos(const char* fileName)
{
	for(int i=0; i<m_Lights.GetCount(); ++i)
	{
		COwnershipInfo<vehicleLightSettings, vehicleLightSettingsId>::Add(m_Lights[i].GetId(), fileName);
	}
	for(int i=0; i<m_Sirens.GetCount(); ++i)
	{
		COwnershipInfo<sirenSettings, sirenSettingsId>::Add(m_Sirens[i].GetId(), fileName);
	}
	for(int i=0; i<m_Kits.GetCount(); ++i)
	{
		COwnershipInfo<CVehicleKit, VehicleKitId>::Add(m_Kits[i].GetId(), fileName);
	}
}

#endif // __BANK
