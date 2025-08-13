//
// script/ScriptMetadataManager.h
//
// Copyright (C) 1999-2015 Rockstar Games. All Rights Reserved.
//

#ifndef SCRIPT_SCRIPTMETADATAMANAGER_H
#define SCRIPT_SCRIPTMETADATAMANAGER_H

#include "grcore/device.h"
#include "atl/array.h"
#include "data/base.h"
#include "script/ScriptMetadata.h"


class CScriptMetadataManager
{

public:
	CScriptMetadataManager();
	~CScriptMetadataManager();
	static void Init();
	static void Shutdown();
	static bool GetMPOutfitData(CMPOutfitsData** outfit, int id, bool bMale);
	static bool GetBaseElementLocation(CBaseElementLocation** outLocation, int element, bool bHighApt);
	static bool GetBaseElementLocationFromBlock(CBaseElementLocation** outLocation, int element, int dataBlock);
	static int GetMPApparelMale(u32 hash);
	static int GetMPApparelFemale(u32 hash);

private:
	static CScriptMetadata* m_ScriptMetadata;
	static const char* sm_scriptMetadataPath;

};


#endif
