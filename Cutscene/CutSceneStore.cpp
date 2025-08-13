/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutSceneStore.cpp
// PURPOSE : manages the store for the cutscenes
// AUTHOR  : Thomans French
// STARTED : 17/03/2009
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////

#include "cutscene/CutSceneStore.h"
#include "fwsys/fileExts.h"
#include "parser/psofile.h"
// --- Globals ------------------------------------------------------------------

CCutsceneStore g_CutSceneStore;

// --- Code ------------------------------------------------------------------

ANIM_OPTIMISATIONS ()


CCutsceneStore::CCutsceneStore() : fwAssetStore<cutfCutsceneFile2,cutfCutsceneFile2Def>("CutSceneStore", PI_CUTSCENE_FILE_ID, CONFIGURED_FROM_FILE, 585, true)
{
}

void cutfCutsceneFile2Def::Init( const strStreamingObjectNameString name )
{
	fwAssetNameDef<cutfCutsceneFile2>::Init(name);
}


//
// name:		CStore<char, SizedObjectDef<char> >::Load
// description:	allocate space for buffer and copy
bool CCutsceneStore::LoadFile(strLocalIndex index, const char* pFilename)
{
	//Make a store of cutfCutsceneFile2 objects
	cutfCutsceneFile2Def* pDef = GetSlot(index);
	Assertf(pDef, "No object at this slot");
	//pDef->Init(pFilename);
	Assertf(pDef->m_pObject == NULL, "%s:Object is in memory already", pDef->m_name.GetCStr());

	pDef->m_pObject = rage_new cutfCutsceneFile2;

	//Fill the store object with parsed data.
	bool rt = false; 
	psoFile* cutscenePsoFile = psoLoadFile(pFilename, PSOLOAD_PREP_FOR_PARSER_LOADING, atFixedBitSet32().Set(psoFile::IGNORE_CHECKSUM));
	
	if(Verifyf(cutscenePsoFile,"CutsceneStore: %s scene does not exist in the store", pFilename))
	{
		rt = psoInitAndLoadObject(*cutscenePsoFile, *pDef->m_pObject);
		pDef->m_pObject->ConvertArgIndicesToPointers(); 
		delete cutscenePsoFile;
	}

	if(!rt)
	{
		delete pDef->m_pObject; 
		pDef->m_pObject = NULL;
	}
	else
	{
		pDef->m_pObject->InitExternallyLoadedFile(pDef->m_name.GetCStr(), true);
	}
	return rt;
}

//
// name:		CStore<char, SizedObjectDef<char> >::Remove
// description:	
void CCutsceneStore::Remove(strLocalIndex index)
{
	ASSERT_ONLY(cutfCutsceneFile2Def* pDef = GetSlot(index);)
	Assertf(pDef, "No cutscene in slot");
	Assertf(pDef->m_pObject!=NULL, "Cutscene not in memory");

	fwAssetStore<cutfCutsceneFile2,cutfCutsceneFile2Def>::Remove(index);
}

