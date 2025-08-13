#if 0
/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : FontDef.cpp
// PURPOSE : one resourced font definition
// AUTHOR  : Derek Payne
// STARTED : 28/05/2009
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////

#define __USE_RESOURCED_DATA	(0)  // flag to switch between reading resourced data or the old files

#include "Text/FontDef.h"

#include "parser/manager.h"
#include "parser/tree.h"
#include "streaming/streaming.h"

#if !__USE_RESOURCED_DATA
#include "Scene/stores/TxdStore.h"
#endif

#define WHITESPACE_CHAR (32)  // space is ascii 32
#define NUM_FONTS		(16)



CFontDef::~CFontDef()
{
#if !__USE_RESOURCED_DATA
	s32 iFontTxdId = g_TxdStore.FindSlot(m_cTextureFilename);
	if(iFontTxdId != -1)
	{
		g_TxdStore.RemoveRef(iFontTxdId);
		g_TxdStore.RemoveSlot(iFontTxdId);
	}
#endif
}



////////////////////////////////////////////////////////////////////////////
// name:	CFontDef::Load
//
// purpose: loads font data from original xml file - one for each font
////////////////////////////////////////////////////////////////////////////
bool CFontDef::Load(const char *pName)
{
	for (s32 iClearCount = 0; iClearCount < MAX_CHARACTER_SUPPORTED; iClearCount++)
	{
		m_iCharPosInTexture[iClearCount] = 63;  // so anything not found appears as a '?'
		m_iCharacterWidth[iClearCount] = 0;
	}
#if __USE_RESOURCED_DATA
	// not done yet!

#else
	//
	// Load the data from the XML file:
	//
	INIT_PARSER;

	char cfilename[100];
	sprintf(cfilename, "common:/data/%s.xml", pName);

	parTree* pTree = PARSER.LoadTree(cfilename, "xml");

	Assertf(pTree, "Font - failed to load the text definition xml file!");

	if (pTree)
	{
		parTreeNode* pRootNode= pTree->GetRoot();

		rage::parTreeNode::ChildNodeIterator it = pRootNode->BeginChildren();
		for(; it != pRootNode->EndChildren(); ++it)
		{
			//
			// <texture>
			//
			if(stricmp((*it)->GetElement().GetName(), "texture") == 0)
			{
				m_cTextureFilename.Set( (*it)->GetData() );
			}
			else
			//
			// <map>
			//
			if(stricmp((*it)->GetElement().GetName(), "map") == 0)
			{
				s32 iCharCount = 0;
				for (s32 iLineNum = 1; iLineNum < 14; iLineNum++)
				{
					s32 iNum[16];
					char cLineAttName[10];
					sprintf(cLineAttName, "line%d", iLineNum);  // create the attribute we want to find

					rage::parTreeNode::ChildNodeIterator wit = (*it)->BeginChildren();
					for(; wit != (*it)->EndChildren(); ++wit)
					{
						if(stricmp((*wit)->GetElement().GetName(), cLineAttName) == 0)
						{
							const char *pLine = (*wit)->GetData();

							sscanf(pLine, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
											&iNum[0], &iNum[1], &iNum[2], &iNum[3], &iNum[4], &iNum[5], &iNum[6], &iNum[7],
											&iNum[8], &iNum[9], &iNum[10], &iNum[11], &iNum[12], &iNum[13], &iNum[14], &iNum[15]);

							for(s32 iCount = 0; iCount < 16; iCount++)
							{
								m_iCharPosInTexture[(u8)iNum[iCount]] = (u8)iCharCount;
								iNum[iCount] = 0;  // reset the value for next pass
								iCharCount++;
							}
						}
					}
				}
			}
			else
			//
			// <prop>
			//
			if(stricmp((*it)->GetElement().GetName(), "prop") == 0)
			{
				m_iCharacterWidth[32] = 0;

				s32 iCharCount = 0;
				for (s32 iLineNum = 1; iLineNum < 14; iLineNum++)
				{
					s32 iNum[16];
					char cLineAttName[10];
					sprintf(cLineAttName, "line%d", iLineNum);  // create the attribute we want to find

					rage::parTreeNode::ChildNodeIterator wit = (*it)->BeginChildren();
					for(; wit != (*it)->EndChildren(); ++wit)
					{
						if(stricmp((*wit)->GetElement().GetName(), cLineAttName) == 0)
						{
							const char *pLine = (*wit)->GetData();

							sscanf(pLine, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
											&iNum[0], &iNum[1], &iNum[2], &iNum[3], &iNum[4], &iNum[5], &iNum[6], &iNum[7],
											&iNum[8], &iNum[9], &iNum[10], &iNum[11], &iNum[12], &iNum[13], &iNum[14], &iNum[15]);

							for(s32 iCount = 0; iCount < 16; iCount++)
							{
								for (s32 iMapCount = 0; iMapCount < MAX_CHARACTER_SUPPORTED; iMapCount++)
								{
									if (iCharCount == m_iCharPosInTexture[iMapCount])
									{
										m_iCharacterWidth[iMapCount] = 32-(u8)iNum[iCount];  // store this prop value
									}
								}

								iCharCount++;
							}
						}
					}
				}
			}
			else
			//
			// <unprop>
			//
			if(stricmp((*it)->GetElement().GetName(), "unprop") == 0)
			{
				m_iNonProportionalWidth = (u8)(atoi)((*it)->GetData());
			}
			else
			//
			// <spacing>
			//
			if(stricmp((*it)->GetElement().GetName(), "spacing") == 0)
			{
				m_iSpacing = (u8)(atoi)((*it)->GetData());
			}
			else
			//
			// <whitespace>
			//
			if(stricmp((*it)->GetElement().GetName(), "whitespace") == 0)
			{
				m_iCharacterWidth[WHITESPACE_CHAR] = 32-(u8)(atoi)((*it)->GetData());
			}
		}

		delete pTree;
	}

	SHUTDOWN_PARSER;

	//
	// load the texture from the texture dictionary
	//

	s32 fontTxdId = g_TxdStore.FindSlot(m_cTextureFilename);
	if(fontTxdId == -1)
		fontTxdId = g_TxdStore.AddSlot(m_cTextureFilename);

	if(CStreaming::IsObjectInImage(fontTxdId, g_TxdStore.GetStreamingModuleId()))
	{
		CStreaming::RequestObject(fontTxdId, g_TxdStore.GetStreamingModuleId(),  (STRFLAG_DONTDELETE | STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD) );
		CStreaming::LoadAllRequestedObjects();

		// yep get it from the img
		g_TxdStore.PushCurrentTxd();
		g_TxdStore.SetCurrentTxd(fontTxdId);
	}
	else
	{
		// temp stuff - still get it from the old place for now
		char cfilename[100];
		sprintf(cfilename, "platform:/textures/%s", pName);

		g_TxdStore.LoadFile(fontTxdId, cfilename);
		g_TxdStore.AddRef(fontTxdId);
		g_TxdStore.SetCurrentTxd(fontTxdId);
	}

	if (fontTxdId != -1)
	{
		m_pTexture = Txd::GetCurrent()->Lookup(pName);
		Assert(m_pTexture);

		g_TxdStore.PopCurrentTxd();
	}
#endif

	return true;
}



////////////////////////////////////////////////////////////////////////////
// name:	CTextFontStore::GetTexture
//
// purpose: returns a pointer to the texture
////////////////////////////////////////////////////////////////////////////
grcTexture *CFontDef::GetTexture()
{
	if(m_pTexture)
	{
		return m_pTexture;  // return the texture
	}

	Assertf(0, "Font doesnt contain a texture!");

	return NULL;  // no texture
}



////////////////////////////////////////////////////////////////////////////
// name:	CTextFontStore::GetCharacterWidth
//
// purpose: returns the width of the passed unicode value
////////////////////////////////////////////////////////////////////////////
s32 CFontDef::GetCharacterWidth(s32 iUnicodeValue)
{
	Assertf(iUnicodeValue < MAX_CHARACTER_SUPPORTED, "Unicode value (%d) over supported char!", iUnicodeValue);

	return m_iCharacterWidth[iUnicodeValue];
}



////////////////////////////////////////////////////////////////////////////
// name:	CTextFontStore::GetNonProportionalCharacterWidth
//
// purpose: returns the non-proportional width
////////////////////////////////////////////////////////////////////////////
s32 CFontDef::GetNonProportionalCharacterWidth()
{
	return m_iNonProportionalWidth;
}



////////////////////////////////////////////////////////////////////////////
// name:	CTextFontStore::GetCharacterSpacing
//
// purpose: returns the spacing between each character
////////////////////////////////////////////////////////////////////////////
s32 CFontDef::GetCharacterSpacing()
{
	return m_iSpacing;
}



////////////////////////////////////////////////////////////////////////////
// name:	CTextFontStore::GetPositionInTexture
//
// purpose: returns the map between ascii and the placement in the texture
//			note this will be changed soon
////////////////////////////////////////////////////////////////////////////
s32 CFontDef::GetPositionInTexture(s32 iUnicodeValue)
{
	Assertf(iUnicodeValue < MAX_CHARACTER_SUPPORTED, "Unicode value (%d) over supported char!", iUnicodeValue);

	return m_iCharPosInTexture[iUnicodeValue];
}





// eof

#endif  // 0