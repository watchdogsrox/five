#if 0
/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : FontDef.h
// PURPOSE : one font definition
// AUTHOR  : Derek Payne
// STARTED : 25/05/2009
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef FONTDEF_H
#define FONTDEF_H

// Rage headers
#include "fwtl/assetstore.h"
#include "paging/base.h"

#define MAX_CHARACTER_SUPPORTED	(255)  // max ascii value supported - will increase when unicode is supported

namespace rage
{
	class grcTexture;
}

//
// Font definition - everything for 1 font here:
//
class CFontDef : public pgBase
{
public:

	CFontDef() { };
	~CFontDef();

	grcTexture *GetTexture();
	s32 GetCharacterWidth(s32 iUnicodeValue);
	s32 GetNonProportionalCharacterWidth();
	s32 GetCharacterSpacing();
	s32 GetPositionInTexture(s32 iUnicodeValue);

	bool Load(const char *pName);

private:

	u8 m_iSpacing;

	u8 m_iCharacterWidth[MAX_CHARACTER_SUPPORTED];
	u8 m_iNonProportionalWidth;

	grcTexture *m_pTexture;

	strStreamingObjectName m_cTextureFilename;

	// old values that need to be changed/removed:
	u8 m_iCharPosInTexture[MAX_CHARACTER_SUPPORTED];

	float m_width; 
	float m_height;
	float m_charHeight;
	float m_charYStartOffset;
	float m_charYEndOffset;
};	

#endif // FONTDEF_H

// eof
#endif