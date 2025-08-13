//
// VehicleModelInfoPlates.h
// Data file for vehicle license plate settings
//
// Rockstar Games (c) 2010

#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/combo.h"
#include "fwscene/stores/txdstore.h"
#include "modelinfo/VehicleModelInfoColors.h"
#include "modelinfo/VehicleModelInfoPlates.h"
#include "modelinfo/VehicleModelInfoPlates_parser.h"
#include "modelinfo/VehicleModelInfo.h"
#include "parser/manager.h"
#include "math/random.h"
#include <numeric>

VEHICLE_OPTIMISATIONS()

////////////////////////////////
// CVehicleModelInfoPlates Class Definition
// 

CVehicleModelInfoPlateTextureSet::CVehicleModelInfoPlateTextureSet()
: m_FontExtents(0.043f, 0.38f, 0.945f, 0.841f)
, m_MaxLettersOnPlate(7.0f, 1.0f)
, m_FontOutlineMinMaxDepth(0.475f, 0.5f)
, m_FontColor(0x00FFFFFFU)
, m_FontOutlineColor(0x00FFFFFFU)
, m_IsFontOutlineEnabled(false)
{
}

void CVehicleModelInfoPlateTextureSet::Reset(const char *diffuseMap, const char *normalMap)
{
	m_DiffuseMapName = diffuseMap;
	m_NormalMapName = normalMap;
}


////////////////////////////////
// CVehicleModelInfoPlates Class Definition
// 

CVehicleModelInfoPlates::CVehicleModelInfoPlates()
: m_DefaultTexureIndex(-1)
, m_NumericOffset(0)
, m_AlphabeticOffset(10)
, m_SpaceOffset(63)
, m_RandomCharOffset(36)
, m_NumRandomChar(4)
{
}

namespace
{
	static const char *sDefaultConfigFileExtention = "xml";
}

#if __BANK
namespace
{
	static const u32 sBANKONLY_MaxPathLen = 256;
	static const u32 sBANKONLY_MaxTexNameLen = 64;
	char sBANKONLY_CurrConfigFilename[sBANKONLY_MaxPathLen]		= {'\0'};
	char sBANKONLY_NewDiffuseMapName[sBANKONLY_MaxTexNameLen]	= {'\0'};
	char sBANKONLY_NewNormalMapName[sBANKONLY_MaxTexNameLen]	= {'\0'};
	static const char sHashedOnlyStr[] = "<Unknown Hash>";

	void BANKONLY_SetCurrentConfigFilename(const char *filename, const char *ext = NULL)
	{
		char buff[sBANKONLY_MaxPathLen]; //Use temp buffer in case filename or ext alias path var
		const char *extInFilename = fiAssetManager::FindExtensionInPath(filename);
		if(ext && !extInFilename)
			formatf(buff, "%s.%s", filename, ext);
		else
			formatf(buff, "%s", filename);

		formatf(sBANKONLY_CurrConfigFilename, "%s", buff);
	}

// 	bool BANKONLY_ShowSelectFileDialog(bool isSaving)
// 	{
// 		//Parse the file extension.
// 		char extFilter[32] = {'\0'};
// 		const char *ext = fiAssetManager::FindExtensionInPath(sBANKONLY_CurrConfigFilename);
// 		formatf(extFilter, "*.%s", (ext != NULL ? ++ext : sDefaultConfigFileExtention));
// 
// 		char filename[sBANKONLY_MaxPathLen];
// 		bool fileSelected = BANKMGR.OpenFile(filename, 256, extFilter, isSaving, "Vehicle License Plate Config File");
// 
// 		//Make this file our new config file if selected
// 		if(fileSelected && filename)
// 		{
// 			formatf(sBANKONLY_CurrConfigFilename, "%s", filename);
// 		}
// 
// 		//return sBANKONLY_CurrConfigFilename;
// 		return fileSelected;
// 	}

	////////////////
	//Support for reloading rag widgets
	bkGroup *sBANKONLY_MainConfigUiGroup = NULL;

	//Support for saving/loading/reloading config files
// 	void sBANKONLY_ReloadSettings(CVehicleModelInfoPlates *obj)
// 	{
// 		if(obj)
// 		{
// 			//Parse the filename into name and extension.
// 			char configName[sBANKONLY_MaxPathLen] = {'\0'};
// 			fiAssetManager::RemoveExtensionFromPath(configName, sBANKONLY_MaxPathLen, sBANKONLY_CurrConfigFilename);
// 			const char *ext = fiAssetManager::FindExtensionInPath(sBANKONLY_CurrConfigFilename);
// 			if(ext) ++ext;	//FindExtensionInPath returns a pointer to the file extension including the '.' character.
// 
// 			//Load the settings
// 			/*bool success =*/ obj->Load(configName, ext);
// 
// 			//Need to reload bank widgets to reflect newly loaded settings
// 			obj->ReloadWidgets();
// 		}
// 	}
// 
// 	void sBANKONLY_LoadNewSettings(CVehicleModelInfoPlates *obj)
// 	{
// 		if(obj)
// 		{
// 			if(BANKONLY_ShowSelectFileDialog(false))
// 				sBANKONLY_ReloadSettings(obj);
// 		}
// 	}
// 
// 	void sBANKONLY_SaveNewSettings(CVehicleModelInfoPlates *obj)
// 	{
// 		if(obj)
// 		{
// 			if(BANKONLY_ShowSelectFileDialog(true))
// 			{
// 				BANKONLY_SetCurrentConfigFilename(sBANKONLY_CurrConfigFilename);
// 				obj->Save(sBANKONLY_CurrConfigFilename);
// 			}
// 		}
// 	}
}

void CVehicleModelInfoPlates::AddWidgets(rage::bkBank &bank)
{
	//Cache off main group ptr
	sBANKONLY_MainConfigUiGroup = static_cast<bkGroup *>(bank.GetCurrentGroup());

	//Add Save/Load Widgets
// 	static const bool sIsConfigFileReadOnly = false;
// 	bank.AddText("Config File:", sBANKONLY_CurrConfigFilename, sBANKONLY_MaxPathLen, sIsConfigFileReadOnly);
// 	bank.AddButton("Reload", datCallback(CFA1(sBANKONLY_ReloadSettings), reinterpret_cast<CallbackData>(this)));
// 	bank.AddButton("Save", datCallback(MFA1(CVehicleModelInfoPlates::Save), this, reinterpret_cast<CallbackData>(sBANKONLY_CurrConfigFilename)));
// 	bank.AddButton("Save As...", datCallback(CFA1(sBANKONLY_SaveNewSettings), reinterpret_cast<CallbackData>(this)));
// 	bank.AddButton("Load New File...", datCallback(CFA1(sBANKONLY_LoadNewSettings), reinterpret_cast<CallbackData>(this)));
	bank.AddSeparator();

	//Texture Sets
	bank.PushGroup("Texture Set Settings");
	{
		bank.PushGroup("Textures");
		{
			//Compute string padding
			char title[256];
			formatf(title, "%d", m_Textures.size() - 1);
			size_t padWidth = strlen(title);

			TextureArray::iterator begin = m_Textures.begin();
			TextureArray::iterator end = m_Textures.end();
			for(TextureArray::iterator iter = begin; iter != end; ++iter)
			{
				TextureArray::difference_type index = ptrdiff_t_to_int(iter - begin);
				const rage::atHashString &texSetName = iter->GetTextureSetName();
				const char *texSetNameStr = texSetName.GetCStr();
				formatf(title, "Texture Set %*d: \t%s\t\t(Hash: %#-10x)", padWidth, index, (texSetNameStr ? texSetNameStr : sHashedOnlyStr), texSetName.GetHash());
				bank.PushGroup(title);
				{
					PARSER.AddWidgets(bank, &(*iter));
					bank.AddSeparator();
					bank.AddButton("Delete This Texture Set", datCallback(MFA1(CVehicleModelInfoPlates::DeleteTextureSet), this, reinterpret_cast<CallbackData>(index)));
				}
				bank.PopGroup();
			}
		}
		bank.PopGroup();
		bank.AddSlider("Default Texture Set:", &m_DefaultTexureIndex, -1, m_Textures.size() - 1, 1);
		bank.AddSeparator();

		//Add widgets to allow adding new texture sets
		bank.AddTitle("Add a new Texture Set:");
		bank.AddText("Diffuse Map Name:", sBANKONLY_NewDiffuseMapName, sBANKONLY_MaxTexNameLen);
		bank.AddText("Normal Map Name:", sBANKONLY_NewNormalMapName, sBANKONLY_MaxTexNameLen);
		bank.AddButton("Add this Texture Set", datCallback(MFA(CVehicleModelInfoPlates::AddTextureSet), this));
	}
	bank.PopGroup();

	//Texture Atlas Config
	bank.PushGroup("Texture Atlas Settings");
	{
		static const u8 sMinVal = 0x0;
		static const u8 sMaxVal = 0xFF;
		bank.AddSlider("Numeric Offset",		&m_NumericOffset,		sMinVal, sMaxVal, 1);
		bank.AddSlider("Alphabetic Offset",		&m_AlphabeticOffset,	sMinVal, sMaxVal, 1);
		bank.AddSlider("Space Offset",			&m_SpaceOffset,			sMinVal, sMaxVal, 1);
		bank.AddSlider("Random Char Offset",	&m_RandomCharOffset,	sMinVal, sMaxVal, 1);
		bank.AddSlider("Num Random Char",		&m_NumRandomChar,		sMinVal, sMaxVal, 1);
	}
	bank.PopGroup();
}

void CVehicleModelInfoPlates::ReloadWidgets()
{
	if(sBANKONLY_MainConfigUiGroup)
	{
		if(bkBank *bank = BANKMGR.FindBank("Vehicles"))
		{
			//Destroy all widgets in the group
			while(bkWidget *child = sBANKONLY_MainConfigUiGroup->GetChild())
				child->Destroy();

			//Reset bank context and re-add all the widgets
			bank->SetCurrentGroup(*sBANKONLY_MainConfigUiGroup);
			AddWidgets(*bank);
			bank->UnSetCurrentGroup(*sBANKONLY_MainConfigUiGroup);
		}
	}
}

void CVehicleModelInfoPlates::AddTextureSet()
{
	//Add a new element to the array and set the texture names to the ones from Rag
	m_Textures.Grow().Reset(sBANKONLY_NewDiffuseMapName, sBANKONLY_NewNormalMapName);

	//Array has changed now, so the widgets need to be updated.
	ReloadWidgets();
}

void CVehicleModelInfoPlates::DeleteTextureSet(int index)
{
	if(Verifyf(index >= 0 && index < m_Textures.size(),
		"WARNING: DeleteTextureSet seems to have received an invalid index! Delete call will fail. (index = %d, Valid Range = [0, %d])", index, m_Textures.size()))
	{
		TextureArray::iterator iter = m_Textures.begin() + index;

		//Lets change the default index so that it's still points where it should. If it's pointing to the current texture, invalidate it.
		if(m_DefaultTexureIndex > index)
			--m_DefaultTexureIndex;
		else if(m_DefaultTexureIndex == index)
			m_DefaultTexureIndex = -1;
		//else, do nothing since the element being erased is after the default element

		//Remove the requested element from the array.
		m_Textures.erase(iter);

		//Widgets need to be reloaded now that array has changed.
		ReloadWidgets();
	}
}

bool CVehicleModelInfoPlates::Save(const char *filename)
{
	const char *ext = fiAssetManager::FindExtensionInPath(filename);
	ext = (ext != NULL) ? ++ext : sDefaultConfigFileExtention;
	bool success = PARSER.SaveObject(filename, ext, this);

	return success;
}
#endif // __BANK

bool CVehicleModelInfoPlates::Load(const char *filename, const char *ext)
{
	//Need to reset the array. If you don't it's possible that PARSER.LoadObject() may crash if the array is already
	//initialized with at least 1 element and the load causes the array to try to resize to greater than the current capacity.
	//This will eventually be fixed in Rage.
	m_Textures.Reset();

	ext = (ext != NULL) ? ext : sDefaultConfigFileExtention;
	bool success = PARSER.LoadObject(filename, ext, *this);
	BANK_ONLY(BANKONLY_SetCurrentConfigFilename(filename, ext));

	return success;
}

void CVehicleModelInfoPlates::PostLoad()
{
	//Make sure the default index is within the array bounds. (Or -1 for no default)
	if(m_DefaultTexureIndex < -1 || m_DefaultTexureIndex >= m_Textures.size())
	{
		Assertf(false, "Warning: Default vehicle license plate texture index was out of bounds! Resetting to -1. (Default index = %d, Bounds = [-1, %d])",
				m_DefaultTexureIndex, m_Textures.size());
		m_DefaultTexureIndex = -1;
	}
}

////////////////////////////////
// CVehicleModelPlateProbabilities Class Definition
// 

CVehicleModelPlateProbabilities::CVehicleModelPlateProbabilities()
#if __BANK
: m_Group(NULL)
, m_Combo(NULL)
, m_ComboSelection(0)
#endif //__BANK
{
}

CVehicleModelPlateProbabilities::name_type CVehicleModelPlateProbabilities::SelectPlateTextureSet() const
{
	return SelectPlateTextureSet(m_Probabilities);
}

CVehicleModelPlateProbabilities::name_type CVehicleModelPlateProbabilities::SelectPlateTextureSet(const array_type& plateProbs)
{
	value_type sumProbability = std::accumulate(plateProbs.begin(), plateProbs.end(), 0U);
	if(sumProbability > 0)
	{
		int randSelector = g_DrawRand.GetInt() % sumProbability;
		array_type::const_iterator end = plateProbs.end();
		for(array_type::const_iterator iter = plateProbs.begin(); iter != end; ++iter)
		{
			if(randSelector < iter->GetValue())
				return iter->GetName();

			randSelector -= iter->GetValue();
		}
	}

	return name_type();
}

#if __BANK
namespace
{
	void BANKONLY_SetupPlateSetNames(atArray<const char *> &names, const CVehicleModelInfoPlates &plateInfo, const CVehicleModelPlateProbabilities::array_type &probabilities)
	{
		const CVehicleModelInfoPlates::TextureArray &texSets = plateInfo.GetTextureArray();

		//Resize name array if necessary, if not just reset the count so we can rebuild the array
		if(names.GetCapacity() <= texSets.GetCount())
		{
			names.Reset();
			names.Reserve(texSets.GetCount());
		}
		else
			names.ResetCount();

		//Iterate through all the texture set names, but only add the ones that aren't already in the probability list.
		for (int i=0; i<texSets.GetCount(); i++) {
			const atHashString &name = texSets[i].GetTextureSetName();
			int j;
			for (j=0; j<probabilities.GetCount(); j++)
				if (name == probabilities[j].GetName())
					break;
			if (j == probabilities.GetCount()) {
				const char *nameStr = name.GetCStr();
				names.Push(nameStr ? nameStr : sHashedOnlyStr);
			}
		}
	}
}

void CVehicleModelPlateProbabilities::AddWidgets(rage::bkBank &bank, const CVehicleModelInfoPlates &plateInfo)
{
	m_Group = static_cast<bkGroup *>(bank.GetCurrentGroup());

	BANKONLY_SetupPlateSetNames(m_PlateTexSetNames, plateInfo, m_Probabilities);
	if(m_PlateTexSetNames.size() > 0)
	{
		m_ComboSelection = rage::Clamp(m_ComboSelection, 0, m_PlateTexSetNames.size() - 1);
		m_Combo = bank.AddCombo("Texture Set To Attach a Probability to:", &m_ComboSelection, m_PlateTexSetNames.size(), &(m_PlateTexSetNames[0]));
		bank.AddButton("Add Probability for Selected Texture Set", datCallback(MFA(CVehicleModelPlateProbabilities::AddSelectedEntry), this));
		bank.AddSeparator();
	}
	else
	{
		m_Combo = NULL;
	}

	//Add widgets for each probability entry.
	array_type::iterator begin = m_Probabilities.begin();
	array_type::iterator end = m_Probabilities.end();
	for(array_type::iterator iter = begin; iter != end; ++iter)
	{
		PARSER.AddWidgets(bank, &(*iter));
		bank.AddButton("Delete This Entry", datCallback(MFA1(CVehicleModelPlateProbabilities::DeleteEntry), this, reinterpret_cast<CallbackData>(iter - begin)));
		bank.AddSeparator();
	}
}

void CVehicleModelPlateProbabilities::ReloadWidgets()
{
	if(m_Group)
	{
		if(bkBank *bank = BANKMGR.FindBank("Vehicles"))
		{
			//Destroy all widgets in the group
			while(bkWidget *child = m_Group->GetChild())
				child->Destroy();

			//Reset bank context and re-add all the widgets
			bank->SetCurrentGroup(*m_Group);
			AddWidgets(*bank, CVehicleModelInfo::GetVehicleColours()->GetLicensePlateData());
			bank->UnSetCurrentGroup(*m_Group);
		}
	}
}

void CVehicleModelPlateProbabilities::AddSelectedEntry()
{
	if(m_Combo)
	{
		atHashString selected(m_Combo->GetString(m_ComboSelection));
		m_Probabilities.PushAndGrow(array_type::value_type(selected, 1));
		ReloadWidgets();
	}
}

void CVehicleModelPlateProbabilities::DeleteEntry(int index)
{
	if(index < m_Probabilities.size())
	{
		m_Probabilities.erase(m_Probabilities.begin() + index);
		ReloadWidgets();
	}
}
#endif
