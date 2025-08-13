/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutSceneStore.h
// PURPOSE : manages the formatting of the text
// AUTHOR  : Thomas French
// STARTED : 10/08/2009
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CUTSCENESTORE_H
#define CUTSCENESTORE_H

// game headers:

#include "cutfile/cutfile2.h"
#include "fwtl/assetstore.h"

class cutfCutsceneFile2Def : public fwAssetNameDef<cutfCutsceneFile2>
{
public:
	void Init( const strStreamingObjectNameString name );
};

class CCutsceneStore : public fwAssetStore<cutfCutsceneFile2, cutfCutsceneFile2Def>
{
public:
	CCutsceneStore();
	
	virtual bool LoadFile(strLocalIndex index, const char* pFilename);
	virtual void Remove(strLocalIndex index);
};



extern CCutsceneStore g_CutSceneStore;

#endif