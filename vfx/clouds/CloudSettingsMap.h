// 
// shadercontrollers/CloudSettingsMap.h 
// 
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved. 
// 

#ifndef SHADERCONTROLLERS_CLOUDSETTINGSMAP_H 
#define SHADERCONTROLLERS_CLOUDSETTINGSMAP_H 

#include "atl/array.h"
#include "atl/map.h"
#include "atl/hashstring.h"
#include "parser/manager.h"
#include "CloudHatSettings.h"

namespace rage {

//////////////////////////////////////////////////////////////////////
// PURPOSE
//	If you only have a single global cloud settings structure, or if you
//	have multiple and want to save the them directly into an existing settings
//	file, or just as a number of loose XMLs, you can stop here and use the
//	CloudHatSettings class above directly.
//	
//	However, if you prefer to have one large cloud settings mega file containing
//	all variations of cloud settings, you use the classes below to easily
//	accomplish this. The settings will be represented as a map so you can quickly
//	retrieve a specific set by it's hash name.
//	

struct CloudSettingsNamed
{
	typedef atHashString Key;
	typedef CloudHatSettings Value;

	Key m_Name;
	Value m_Settings;

	CloudSettingsNamed()	{}
	CloudSettingsNamed(Key name, const Value &settings)
	: m_Name(name)
	, m_Settings(settings)
	{
	}

	PAR_SIMPLE_PARSABLE;
};

class CloudSettingsMap : public atMap<CloudSettingsNamed::Key, CloudSettingsNamed::Value>, public datBase
{
public:
	typedef atMap<CloudSettingsNamed::Key, CloudSettingsNamed::Value> base_type;
	typedef base_type::Iterator iterator;
	typedef base_type::ConstIterator const_iterator;

#if __BANK
	void AddWidgets(bkBank &bank);
	void ReloadWidgets();

	const CloudSettingsNamed::Key &BANKONLY_GetCurrentlySelectedSet() const	{ return mBANKONLY_CurrSelectedSet; }
	void CheckForWeatherChange();

	bool Save(const char *filename);
#endif //__BANK
	bool Load(const char *filename, const char *ext = NULL);	//If ext is NULL, uses default XML extension
	void PostMapLoad();
	atArray<float> & GetKeyFrameTimesArray() {return m_KeyframeTimes;};

	PAR_SIMPLE_PARSABLE;
private:
	void PreSave();
	void PostSave(parTreeNode *node);
	//void PreLoad(parTreeNode *node);
	void PostLoad();

#if __BANK
	int GetTimeCycleCloudSet();
#endif

private:
	atArray<float> m_KeyframeTimes; // what times should we allow for editing in the keyframe
	
	//Maps aren't parser friendly yet, so we use an array for saving/loading.
	typedef atArray<CloudSettingsNamed> array_type;
	array_type m_SettingsMap;

private:
	//Necessary elements for BANK ONLY functionality
#if __BANK
	bkBank *mBANKONLY_Bank;
	bkGroup *mBANKONLY_CurrGroup;
	bkWidget *mBANKONLY_ComboWidget;
	int mBANKONLY_ComboSelection;
	CloudSettingsNamed::Key mBANKONLY_CurrSelectedSet;

	void BANKONLY_OnComboChanged();
#endif //__BANK
};

} // namespace rage

#endif // SHADERCONTROLLERS_CLOUDSETTINGSMAP_H 
