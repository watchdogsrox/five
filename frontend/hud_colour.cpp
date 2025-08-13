/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : hud_colour.cpp
// PURPOSE : lists all the frontend & hud colours in 1 file for easy adapting
//			 in array so that level designers can retrieve the RGBA values
// AUTHOR  : Derek Payne
// STARTED : 07/06/2004
//
/////////////////////////////////////////////////////////////////////////////////

// c headers:
#include "stdio.h"

// rage headers:
#include "grcore/device.h"
#include "string/stringhash.h"

// game headers:
#include "frontend/hud_colour.h"
#include "frontend/ui_channel.h"
#include "scene/datafilemgr.h"
#include "system/FileMgr.h"
#include "text/TextFile.h"

atRangeArray<Color32, HUD_COLOUR_MAX_COLOURS>	CHudColour::m_values;
atRangeArray<u32, HUD_COLOUR_MAX_COLOURS>		CHudColour::m_NameRef;
bool	CHudColour::bHudColoursInitialised = false;


////////////////////////////////////////////////////////////////////////////
// name:	CHudColour::SetValuesFromDataFile
//
// purpose: reads in the colours from the data file
////////////////////////////////////////////////////////////////////////////
void CHudColour::SetValuesFromDataFile()
{
	char*		pLine;
	s32		red = 0, green = 0, blue = 0, alpha = 0;

	uiDebugf3("Intialising Hud Color...\n");

	// new episodic code to read in a new hud_colour.dat file for the episodes instead of the main game one:
	FileHandle fid = INVALID_FILE_HANDLE;

	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::HUDCOLOR_FILE);
	while(DATAFILEMGR.IsValid(pData))
	{
		fid = CFileMgr::OpenFile(pData->m_filename, "rb");
		pData = DATAFILEMGR.GetPrevFile(pData);
	}

	if (!CFileMgr::IsValidFileHandle(fid))  // if no episodic content, then use standard credits dat file:
	{
		CFileMgr::SetDir("");

		fid = CFileMgr::OpenFile("common:/DATA/UI/HUDCOLOR.DAT", "rb");
	}

	CFileMgr::SetDir("");

	Assertf( CFileMgr::IsValidFileHandle(fid), "Could not load hud color data file" );

	bool bReadInThisData = false;
	char text[200];
	char text2[200];

	s32 i = 0;
	while( ((pLine = CFileMgr::ReadLine(fid)) != NULL) && (i < HUD_COLOUR_MAX_COLOURS) )
	{
		if(*pLine == '#' || *pLine == '\0')
			continue;

		if (!bReadInThisData)
		{
			sscanf(pLine, "%s", text);

#if __WIN32PC
			if (!stricmp(text, "[HD]"))  // PC uses HD settings always
			{
				bReadInThisData = true;
			}
#else
			if ((GRCDEVICE.GetHiDef()) && (!stricmp(text, "[HD]")))
			{
				bReadInThisData = true;
			}

			if ((!GRCDEVICE.GetHiDef()) && (!stricmp(text, "[CRT]")))
			{
				bReadInThisData = true;
			}
#endif
			continue;
		}

		if (bReadInThisData)
		{
			sscanf(pLine, "%s %s %d %d %d %d", &text[0], &text2[0], &red, &green, &blue, &alpha);

			SetNameRef((eHUD_COLOURS)i, atStringHash(&text[0]));

			if (!stricmp("RGBA", text2))
			{
				SetRGBAValue((eHUD_COLOURS)i, red, green, blue, alpha);
			}
			else
			{
				bool bFound = false;
				for (s32 iExistingCols = 0; ((!bFound) && iExistingCols < i); iExistingCols++)
				{
					if (i == iExistingCols)
						continue;

					if (m_NameRef[(eHUD_COLOURS)iExistingCols] == atStringHash(&text2[0]))
					{
						bHudColoursInitialised = true;  // set the flag to true temporarily so we can get the colour
						CRGBA col = GetRGBA((eHUD_COLOURS)iExistingCols);
						bHudColoursInitialised = false;
						SetRGBAValue((eHUD_COLOURS)i, col.GetRed(), col.GetGreen(), col.GetBlue(), col.GetAlpha());
						bFound = true;
					}
				}

				if (!bFound)
				{
					Assertf(0, "HUDCOLOUR - Could not find '%s' in previous hud colour in DAT file", text2);
					SetRGBAValue((eHUD_COLOURS)i, 0, 0, 0, 0);
				}
			}

			i++;
		}
	}

	CFileMgr::CloseFile(fid);

	bHudColoursInitialised = true;
}


atHashString CHudColour::GetKeyFromColour(eHUD_COLOURS const c_colorId)
{
	if ((s32)c_colorId < HUD_COLOUR_MAX_COLOURS)
	{
		return m_NameRef[c_colorId];
	}

	return atHashString::Null();
}



eHUD_COLOURS CHudColour::GetColourFromKey(atHashString ColourName)
{
	for (s32 i = 0; i < HUD_COLOUR_MAX_COLOURS; i++)
	{
		if (ColourName.GetHash() == m_NameRef[i])
		{
			return (eHUD_COLOURS)i;
		}
	}

	Assertf(0, "HUD - Could not find colour '%s' [0x%08x], returning HUD_COLOUR_WHITE", ColourName.TryGetCStr(), ColourName.GetHash());

	return HUD_COLOUR_WHITE;  // return white if we cannot find a colour but it asserts first
}


eHUD_COLOURS CHudColour::GetColourFromRGBA(Color32 const c_rgba)
{
	for (s32 i = 0; i < HUD_COLOUR_MAX_COLOURS; i++)
	{
		if (m_values[i] == c_rgba)
		{
			return (eHUD_COLOURS)i;
		}
	}

	Assertf(0, "HUD - Could not find a matching hud colour for RGBA(%d,%d,%d,%d), returning HUD_COLOUR_WHITE", c_rgba.GetRed(), c_rgba.GetGreen(), c_rgba.GetBlue(), c_rgba.GetAlpha());

	return HUD_COLOUR_WHITE;  // return white if we cannot find a colour but it asserts first
}



char *CHudColour::GetNameString(eHUD_COLOURS const c_colourId)
{
	if ((s32)c_colourId < HUD_COLOUR_MAX_COLOURS)
	{
		return TheText.Get(m_NameRef[c_colourId], "");
	}

	return NULL;
}



////////////////////////////////////////////////////////////////////////////
// name:	SetRGBAValue
//
// purpose: set the RGBA value of this colour
////////////////////////////////////////////////////////////////////////////
void CHudColour::SetRGBAValue(eHUD_COLOURS colour_id, s32 r, s32 g, s32 b, s32 a)
{
	Assertf (colour_id < HUD_COLOUR_MAX_COLOURS, "Warning: incorrect hud colour used!");

	m_values[colour_id].Set(r,g,b,a);
}


////////////////////////////////////////////////////////////////////////////
// name:	SetIntColour
//
// purpose: set the RGBA value of this colour from a 32bit integer
////////////////////////////////////////////////////////////////////////////
void CHudColour::SetIntColour(eHUD_COLOURS colour_id, u32 colour)
{
	Assertf (colour_id < HUD_COLOUR_MAX_COLOURS, "Warning: incorrect hud colour used!");

	CRGBA col(colour);
	SetRGBAValue(colour_id, col.GetRed(), col.GetGreen(), col.GetBlue(), col.GetAlpha());
}


////////////////////////////////////////////////////////////////////////////
// name:	GetRGBA
//
// purpose: get the RGBA value of this colour
////////////////////////////////////////////////////////////////////////////
CRGBA CHudColour::GetRGBA(eHUD_COLOURS colour_id)
{
#if __DEV
	// we can now just check against whether its been initialised once as we can now reload the hud colours, but
	// the data array will remain intact.
	Assertf(bHudColoursInitialised, "One or more hud colours have not been set!");
#endif

	if( !Verifyf(colour_id>=0 && colour_id<m_values.GetMaxCount(), "Can't access HUD_COLOUR #%d. Chances are your scaleform is newer than your code! Returning HUD_COLOUR_PURPLE", colour_id) )
		colour_id = HUD_COLOUR_PURPLE;

	return CRGBA(m_values[colour_id]);

}

void CHudColour::GetRGBValues(eHUD_COLOURS colour_id, int& r, int& g, int& b)
{
#if __DEV
	Assertf(bHudColoursInitialised, "One or more hud colours have not been set!");
#endif

	if( !Verifyf(colour_id>=0 && colour_id<m_values.GetMaxCount(), "Can't access HUD_COLOUR #%d. Chances are your scaleform is newer than your code! Returning HUD_COLOUR_PURPLE", colour_id) )
		colour_id = HUD_COLOUR_PURPLE;

	r = m_values[colour_id].GetRed();
	g = m_values[colour_id].GetGreen();
	b = m_values[colour_id].GetBlue();
}


////////////////////////////////////////////////////////////////////////////
// name:	GetIntColour
//
// purpose: returns the colour as a 32bit integer
////////////////////////////////////////////////////////////////////////////

u32 CHudColour::GetIntColour(eHUD_COLOURS colour_id)
{
#if __DEV
	// we can now just check against whether its been initialised once as we can now reload the hud colours, but
	// the data array will remain intact.
	Assertf(bHudColoursInitialised, "One or more hud colours have not been set!");
#endif

	if( !Verifyf(colour_id>=0 && colour_id<m_values.GetMaxCount(), "Can't access HUD_COLOUR #%d. Chances are your scaleform is newer than your code! Returning HUD_COLOUR_PURPLE", colour_id) )
		colour_id = HUD_COLOUR_PURPLE;

	return m_values[colour_id].GetColor();
}


////////////////////////////////////////////////////////////////////////////
// name:	GetRGB
//
// purpose: get the RGB value of this colour and use the passed alpha
////////////////////////////////////////////////////////////////////////////
CRGBA CHudColour::GetRGB(eHUD_COLOURS colour_id, u8 alpha)
{
#if __DEV
	// we can now just check against whether its been initialised once as we can now reload the hud colours, but
	// the data array will remain intact.
	Assertf(bHudColoursInitialised, "One or more hud colours have not been set!");
#endif

	if( !Verifyf(colour_id>=0 && colour_id<m_values.GetMaxCount(), "Can't access HUD_COLOUR #%d. Chances are your scaleform is newer than your code! Returning HUD_COLOUR_PURPLE", colour_id) )
		colour_id = HUD_COLOUR_PURPLE;

	CRGBA tempCol(m_values[colour_id]);
	tempCol.SetAlpha(alpha);
	return tempCol;
}

////////////////////////////////////////////////////////////////////////////
// name:	GetRGBF
//
// purpose: gets the RGBA value of this colour with its alpha scaled by a value [0-1]
////////////////////////////////////////////////////////////////////////////
CRGBA CHudColour::GetRGBF(eHUD_COLOURS colour_id, float fAlphaFader)
{
	CRGBA color( GetRGBA(colour_id) );
	u8 alpha( u8( Clamp( int(color.GetAlpha()*fAlphaFader), 0, 255) ) );
	color.SetAlpha( alpha );

	return color;
}

// eof
