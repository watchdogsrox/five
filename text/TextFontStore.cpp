#if 0
/*
/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : TextFormat.cpp
// PURPOSE : manages the formatting of the text
// AUTHOR  : Derek Payne
// STARTED : 17/03/2009
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////

#include "Text\TextFormat.h"
#include "Text\Text.h"
#include "text\FontDef.h"

#include "scene/stores/TxdStore.h"
#include "fwsys/fileExts.h"

#define NUM_FONTS	16

CTextFontStore g_TextFontStore;

CTextFontStore::CTextFontStore() : fwAssetStore<CFontDef>("Font", FONT_FILE_EXT_PATTERN, NUM_FONTS, true, false, 0)
{
}

////////////////////////////////////////////////////////////////////////////
// name:	Load
//
// purpose: loads font data from original txd and xml files
////////////////////////////////////////////////////////////////////////////
bool CTextFontStore::LoadFile(s32 index, const char* pFilename)
{
	fwAssetDef<CFontDef>* pDef = m_pool.GetSlot(index);

	Assertf(pDef, "No font at this slot");

	bool bSuccessful = false;
	pDef->m_pObject = rage_new CFontDef;		
	if (pDef->m_pObject)
	{
		bSuccessful = pDef->m_pObject->Load(pFilename);
	}

	return (bSuccessful);
}

// eof
*/

#endif