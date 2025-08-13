// 
// shadercontrollers/CloudSettingsMap.cpp 
// 
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved. 
// 

#include "CloudSettingsMap.h"
#include "CloudSettingsMap_parser.h"

#include "bank\bank.h"
#include "bank\bkmgr.h"
#include "bank\title.h"
#include "bank\combo.h"

#if __BANK
#include "game\weather.h"
#include "vfx\clouds\CloudHat.h"
#endif

///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_MISC_OPTIMISATIONS()


namespace rage {

namespace
{
}

///////////////////////////////////////////////////////////////////////////////
//  CloudSettingsMap Definition

void CloudSettingsMap::PreSave()
{
	int numElem = GetNumUsed(); //int numSlots = GetNumSlots();

	//Resize array to correct size. (Should already be reset)
	m_SettingsMap.Reserve(numElem);

	//Convert map into the atArray used by the parser.
	iterator iter = CreateIterator();
	for(iter.Start(); !iter.AtEnd(); iter.Next())
	{
		m_SettingsMap.Push(CloudSettingsNamed(iter.GetKey(), iter.GetData()));
	}
}

void CloudSettingsMap::PostSave(parTreeNode * /*node*/)
{
	//After save, free any memory used from atArray, since it's not used by game.
	m_SettingsMap.Reset();
}

void CloudSettingsMap::PostLoad()
{
	//Reset Map
	Reset();

	//Initialize map from array.
	array_type::iterator end = m_SettingsMap.end();
	for(array_type::iterator iter = m_SettingsMap.begin(); iter != end; ++iter)
	{
		//Need to make sure there are no collisions when adding keys to the array.
		const DataType *data = Access(iter->m_Name);
		if(Verifyf(data == NULL, "WARNING! Found a collision while trying to add named cloud settings set \"%s\" (hash = %d)." 
			"Check the names of the cloud settings and make sure there are no duplicates!",
			(iter->m_Name.GetCStr() ? iter->m_Name.GetCStr() : "<Unknown>"), iter->m_Name.GetHash()) )
		{
			//No collision found, add entry to map
			Insert(iter->m_Name, iter->m_Settings);
		}
	}

	//Free parser-only array memory
	m_SettingsMap.Reset();
}

namespace
{
	static const char *sDefaultConfigFileExtention = "xml";
}

#if __BANK
namespace CloudSettingsMapBank
{
	static const u32 sBANKONLY_MaxPathLen = 256;
	static const u32 sBANKONLY_MaxNameLen = 64;
	char sBANKONLY_CurrConfigFilename[sBANKONLY_MaxPathLen]		= {'\0'};
	char sBANKONLY_NewEntryName[sBANKONLY_MaxNameLen]			= {'\0'};
	bool sBANKONLY_ShadowTimeCycleWeather = true;

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

	bool BANKONLY_ShowSelectFileDialog(bool isSaving)
	{
		//Parse the file extension.
		char extFilter[32] = {'\0'};
		const char *ext = fiAssetManager::FindExtensionInPath(sBANKONLY_CurrConfigFilename);
		formatf(extFilter, "*.%s", (ext != NULL ? ++ext : sDefaultConfigFileExtention));

		char filename[sBANKONLY_MaxPathLen];
		bool fileSelected = BANKMGR.OpenFile(filename, 256, extFilter, isSaving, "Vehicle License Plate Config File");

		//Make this file our new config file if selected
		if(fileSelected && filename)
		{
			formatf(sBANKONLY_CurrConfigFilename, "%s", filename);
		}

		//return sBANKONLY_CurrConfigFilename;
		return fileSelected;
	}

	////////////////
	//Support for reloading rag widgets
	//bkGroup *sBANKONLY_MainConfigUiGroup = NULL;

	//Support for saving/loading/reloading config files
	void sBANKONLY_ReloadSettings(CloudSettingsMap *obj)
	{
		if(obj)
		{
			//Parse the filename into name and extension.
			char configName[sBANKONLY_MaxPathLen] = {'\0'};
			fiAssetManager::RemoveExtensionFromPath(configName, sBANKONLY_MaxPathLen, sBANKONLY_CurrConfigFilename);
			const char *ext = fiAssetManager::FindExtensionInPath(sBANKONLY_CurrConfigFilename);
			if(ext) ++ext;	//FindExtensionInPath returns a pointer to the file extension including the '.' character.

			//Load the settings
			/*bool success =*/ obj->Load(configName, ext);

			//Need to reload bank widgets to reflect newly loaded settings
			obj->ReloadWidgets();
		}
	}

	void sBANKONLY_SaveCurrentSettings(CloudSettingsMap *obj)
	{
		obj->Save(sBANKONLY_CurrConfigFilename);
	}

	void sBANKONLY_LoadNewSettings(CloudSettingsMap *obj)
	{
		if(obj)
		{
			if(BANKONLY_ShowSelectFileDialog(false))
				sBANKONLY_ReloadSettings(obj);
		}
	}

	void sBANKONLY_SaveNewSettings(CloudSettingsMap *obj)
	{
		if(obj)
		{
			if(BANKONLY_ShowSelectFileDialog(true))
			{
				BANKONLY_SetCurrentConfigFilename(sBANKONLY_CurrConfigFilename);
				obj->Save(sBANKONLY_CurrConfigFilename);
			}
		}
	}

	//Specific for CloudSettingsMap

	//Support for adding a new entry
	void sBANKONLY_AddNewEntry(CloudSettingsMap *obj)
	{
		if(obj)
		{
			//Need to make sure there are no collisions when adding keys to the array.
			rage::atHashString name(sBANKONLY_NewEntryName);
			CloudSettingsMap::DataType defaultData;

			const CloudSettingsMap::DataType *data = obj->Access(name);
			if(Verifyf(data == NULL, "WARNING! Found a collision while trying to add named cloud settings set \"%s\" (hash = %d)." 
				"You must make sure not to add duplicate names! Add request ignored!", (name.GetCStr() ? name.GetCStr() : "<Unknown>"), name.GetHash()) )
			{
				//No collision found, add entry to map
				obj->Insert(name, defaultData);

				//After inserting, widgets need to be reloaded!
				obj->ReloadWidgets();
			}
		}
	}

	template<class T> const char *GetCStr(const T &str);

	template<> const char *GetCStr(const rage::atHashString &str)
	{
		const char *retStr = str.GetCStr();
		
		if(!retStr)
		{
			//static char sHashedOnlyStr[32] = {'\0'};
			//formatf(sHashedOnlyStr, "Hash = 0x%x", str.GetHash());

			static const char sHashedOnlyStr[] = "<Unknown Hash>";
			retStr = sHashedOnlyStr;
		}

		return retStr;
	}

	template<class _Key, class _Data, class _Hash, class _Equals, class _MemoryPolicy, class T>
	bkCombo *AddMapEntriesCombo(rage::atMap<_Key, _Data, _Hash, _Equals, _MemoryPolicy> &map, bkBank &bank, const char *title, T *value, datCallback callback = NullCB, const char *memo = 0, const char* fillColor = NULL)
	{
		typedef T value_type;
		typedef T* pointer;
		typedef rage::atMap<_Key, _Data, _Hash, _Equals, _MemoryPolicy> map_type;
		typedef rage::atArray<const char *> name_array;

		bkCombo *comboWidget = NULL;
		int numElem = map.GetNumUsed();

		if(numElem > 0)
		{
			//Create names array
			name_array names(0, numElem);

			typename map_type::Iterator iter = map.CreateIterator();
			for(iter.Start(); !iter.AtEnd(); iter.Next())
			{
				names.Push(GetCStr(iter.GetKey()));
			}

			//Now, add the combo.
			comboWidget = bank.AddCombo(title, value, names.size(), &(names[0]), callback, memo, fillColor);
		}

		return comboWidget;
	}
}

int CloudSettingsMap::GetTimeCycleCloudSet()
{
	if( g_weather.GetNumTypes() == 0 )
		return 0;

	// should we check if we are closer to next? or just use previous?
	const char * weatherName = g_weather.GetPrevType().m_CloudSettingsName.GetCStr();
	int selection = 0;

	if(Access(weatherName))
	{
		iterator iter = CreateIterator();
		for(iter.Start(); !iter.AtEnd(); iter.Next())
		{
			if (strcmp(weatherName,iter.GetKey().GetCStr())==0)
				break; 
			selection++;
		}
	}

	return selection;
}


void CloudSettingsMap::CheckForWeatherChange()
{
	if (mBANKONLY_Bank && mBANKONLY_CurrGroup && CloudSettingsMapBank::sBANKONLY_ShadowTimeCycleWeather)
	{
		int selection = GetTimeCycleCloudSet();
		if (selection!= mBANKONLY_ComboSelection)
		{
			mBANKONLY_ComboSelection = selection;
			BANKONLY_OnComboChanged();
		}
	}
}

void CloudSettingsMap::AddWidgets(bkBank &bank)
{
	//Cache off main group ptr
	mBANKONLY_Bank = &bank;
	mBANKONLY_CurrGroup = static_cast<bkGroup *>(bank.GetCurrentGroup());

	static const bool sIsConfigFileReadOnly = false;
	bank.AddText("Config File:", CloudSettingsMapBank::sBANKONLY_CurrConfigFilename, CloudSettingsMapBank::sBANKONLY_MaxPathLen, sIsConfigFileReadOnly);
	bank.AddButton("Reload", datCallback(CFA1(CloudSettingsMapBank::sBANKONLY_ReloadSettings), reinterpret_cast<CallbackData>(this)));
	//bank.AddButton("Save", datCallback(MFA1(CloudSettingsMap::Save), this, reinterpret_cast<CallbackData>(sBANKONLY_CurrConfigFilename)));
	bank.AddButton("Save", datCallback(CFA1(CloudSettingsMapBank::sBANKONLY_SaveCurrentSettings), reinterpret_cast<CallbackData>(this))); //Not sure why compiler complains here and not elsewhere
	bank.AddButton("Save As...", datCallback(CFA1(CloudSettingsMapBank::sBANKONLY_SaveNewSettings), reinterpret_cast<CallbackData>(this)));
	bank.AddButton("Load New File...", datCallback(CFA1(CloudSettingsMapBank::sBANKONLY_LoadNewSettings), reinterpret_cast<CallbackData>(this)));
	bank.AddSeparator();

	//Add widgets to allow adding new cloud settings entry
	bank.AddTitle("Add a new Cloud Settings Entry:");
	bank.AddText("Cloud Settings Entry Name:", CloudSettingsMapBank::sBANKONLY_NewEntryName, CloudSettingsMapBank::sBANKONLY_MaxNameLen);
	bank.AddButton("Add this Entry!", datCallback(CFA1(CloudSettingsMapBank::sBANKONLY_AddNewEntry), reinterpret_cast<CallbackData>(this)));
	bank.AddSeparator();

	bank.AddToggle("Get 'Cloud Settings Set' from current timecycle weather", &CloudSettingsMapBank::sBANKONLY_ShadowTimeCycleWeather,datCallback(MFA(CloudSettingsMap::BANKONLY_OnComboChanged), this));

	mBANKONLY_CurrSelectedSet.Clear();
	if (CloudSettingsMapBank::sBANKONLY_ShadowTimeCycleWeather)
		mBANKONLY_ComboSelection = GetTimeCycleCloudSet();

	bkCombo *comboWidget = CloudSettingsMapBank::AddMapEntriesCombo(*this, bank, "Cloud Settings Set:", &mBANKONLY_ComboSelection, datCallback(MFA(CloudSettingsMap::BANKONLY_OnComboChanged), this));
	
	bank.AddSeparator();

	// add the debug setting widget here
	CLOUDHATMGR->AddDebugWidgets(bank);

	//Need to create a list of all types
	if(comboWidget)
	{
		mBANKONLY_ComboWidget = comboWidget;

		//Add default selected widgets.
		atHashString selected(comboWidget->GetString(mBANKONLY_ComboSelection));
		DataType *data = Access(selected);
		if(Verifyf(data, "WARNING: Unable to find data for settings named \"%s\"! Widgets not added.", comboWidget->GetString(mBANKONLY_ComboSelection)))
		{
			mBANKONLY_CurrSelectedSet = selected;
			data->AddWidgets(*mBANKONLY_Bank);
		}
	}
	else
		mBANKONLY_ComboWidget = bank.AddTitle("No Entries in Settings Map!");
}

void CloudSettingsMap::ReloadWidgets()
{
	if(mBANKONLY_Bank && mBANKONLY_CurrGroup)
	{
		//Destroy all widgets in the group
		while(bkWidget *child = mBANKONLY_CurrGroup->GetChild())
			child->Destroy();

		//Reset bank context and re-add all the widgets
		mBANKONLY_Bank->SetCurrentGroup(*mBANKONLY_CurrGroup);
		AddWidgets(*mBANKONLY_Bank);
		mBANKONLY_Bank->UnSetCurrentGroup(*mBANKONLY_CurrGroup);
	}
}

void CloudSettingsMap::BANKONLY_OnComboChanged()
{
	if( CLOUDHATMGR->GetDebugGroup()->GetNext())
	{
		//Delete all widgets after debug group 
		while(bkWidget *next = CLOUDHATMGR->GetDebugGroup()->GetNext())
			next->Destroy();
	}
	
	mBANKONLY_CurrSelectedSet.Clear();
	
	if (CloudSettingsMapBank::sBANKONLY_ShadowTimeCycleWeather)
		mBANKONLY_ComboSelection = GetTimeCycleCloudSet();

	if(mBANKONLY_Bank && mBANKONLY_CurrGroup && mBANKONLY_ComboWidget && GetNumUsed() > 0)
	{
		bkCombo *comboWidget = reinterpret_cast<bkCombo *>(mBANKONLY_ComboWidget);	//This widget can also be a bkText, but only if there's 0 elements in map.

		atHashString selected(comboWidget->GetString(mBANKONLY_ComboSelection));
		DataType *data = Access(selected);
		if(Verifyf(data, "WARNING: Unable to find data for settings named \"%s\"! Widgets not added.", comboWidget->GetString(mBANKONLY_ComboSelection)))
		{
			mBANKONLY_CurrSelectedSet = selected;
			mBANKONLY_Bank->SetCurrentGroup(*mBANKONLY_CurrGroup);
			data->AddWidgets(*mBANKONLY_Bank);
			mBANKONLY_Bank->UnSetCurrentGroup(*mBANKONLY_CurrGroup);
		}
	}
}

bool CloudSettingsMap::Save(const char *filename)
{
	const char *ext = fiAssetManager::FindExtensionInPath(filename);
	ext = (ext != NULL) ? ++ext : sDefaultConfigFileExtention;
	bool success = PARSER.SaveObject(filename, ext, this);

	return success;
}
#endif //__BANK

bool CloudSettingsMap::Load(const char *filename, const char *ext)
{
	ext = (ext != NULL) ? ext : sDefaultConfigFileExtention;
	bool success = PARSER.LoadObject(filename, ext, *this);
	BANK_ONLY(CloudSettingsMapBank::BANKONLY_SetCurrentConfigFilename(filename, ext));

	return success;
}

void  CloudSettingsMap::PostMapLoad()
{
#if SYNC_TO_TIMECYCLE_TIMES
	iterator iter = CreateIterator();
	for(iter.Start(); !iter.AtEnd(); iter.Next())
		iter.GetData().SyncToTimeCycleTimes();
#endif
}


} // namespace rage
