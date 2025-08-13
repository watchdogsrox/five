///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	Scrollbars.cpp
//	BY	: 	Mark Nicholson (Adapted from original by Obbe)
//	FOR	:	Rockstar North
//	ON	:	01 Aug 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "Scrollbars.h"

// Rage headers

// Framework headers
#include "fwmaths/random.h"
#include "fwscene/stores/txdstore.h"
#include "fwsys/timer.h"
#include "vfx/channel.h"
#include "vfx/vfxwidget.h"

// Game headers
#include "Camera/CamInterface.h"
#include "Core/Game.h"
#include "Game/Clock.h"
#include "Scene/DataFileMgr.h"
#include "Scene/Entity.h"
#include "Shaders/ShaderLib.h"
#include "System/FileMgr.h"
#include "Vfx/Misc/DistantLights.h"
#include "Vfx/VfxHelper.h"
#include "Vfx/VisualEffects.h"
#include "Vfx/misc/DistantLightsVertexBuffer.h"

///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_MISC_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define SCROLLBARS_FONT_HEIGHT			(5)		
#define SCROLLBARS_FONT_SPACE_WIDTH		(2)

dev_float	SCROLLBARS_HDR_MULT			= 12.0f;
dev_float	SCROLLBARS_SIZE_MULT		= 0.15f;
dev_u32	SCROLLBARS_LIGHT_COL_R		= 255;
dev_u32	SCROLLBARS_LIGHT_COL_G		= 128;
dev_u32	SCROLLBARS_LIGHT_COL_B		= 0;
dev_u32	SCROLLBARS_LIGHT_COL_A		= 255;


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CScrollBars		g_scrollbars;

u64			g_scrollbarFont			[64][SCROLLBARS_FONT_HEIGHT] = 
{
	{			// space
		0x00000,
		0x00000,
		0x00000,
		0x00000,
		0x00000,
	},

	{			// !
		0x00100,
		0x00100,
		0x00100,
		0x00000,
		0x00100,
	},

	{			// "
		0x01010,
		0x01010,
		0x00000,
		0x00000,
		0x00000,
	},

	{			// #
		0x01010,
		0x11111,
		0x01010,
		0x11111,
		0x01010,
	},

	{			// $
		0x01110,
		0x10100,
		0x01110,
		0x00101,
		0x01110,
	},

	{			// %
		0x11001,
		0x10010,
		0x00100,
		0x01001,
		0x10011,
	},

	{			// &
		0x01100,
		0x01010,
		0x10100,
		0x10011,
		0x01110,
	},

	{			// '
		0x00100,
		0x00100,
		0x00000,
		0x00000,
		0x00000,
	},

	{			// (
		0x00010,
		0x00100,
		0x00100,
		0x00100,
		0x00010,
	},

	{			// )
		0x01000,
		0x00100,
		0x00100,
		0x00100,
		0x01000,
	},

	{			// *
		0x10101,
		0x00100,
		0x11111,
		0x00100,
		0x10101,
	},

	{			// +
		0x00100,
		0x00100,
		0x11111,
		0x00100,
		0x00100,
	},

	{			// ,
		0x00000,
		0x00000,
		0x00000,
		0x00100,
		0x01000,
	},

	{			// -
		0x00000,
		0x00000,
		0x01110,
		0x00000,
		0x00000,
	},

	{			// .
		0x00000,
		0x00000,
		0x00000,
		0x00000,
		0x00100,
	},

	{			// /
		0x00001,
		0x00010,
		0x00100,
		0x01000,
		0x10000,
	},

	{			// 0
		0x01110,
		0x10001,
		0x10001,
		0x10001,
		0x01110,
	},

	{			// 1
		0x00100,
		0x01100,
		0x00100,
		0x00100,
		0x01110,
	},

	{			// 2
		0x01110,
		0x00001,
		0x01111,
		0x10000,
		0x11111,
	},

	{			// 3
		0x01110,
		0x00001,
		0x00110,
		0x00001,
		0x11110,
	},

	{			// 4
		0x10000,
		0x10010,
		0x11111,
		0x00010,
		0x00010,
	},

	{			// 5
		0x11111,
		0x10000,
		0x11110,
		0x00001,
		0x11110,
	},

	{			// 6
		0x01110,
		0x10000,
		0x11110,
		0x10001,
		0x01110,
	},

	{			// 7
		0x11111,
		0x00010,
		0x00100,
		0x00100,
		0x00100,
	},

	{			// 8
		0x01110,
		0x10001,
		0x01110,
		0x10001,
		0x01110,
	},

	{			// 9
		0x01110,
		0x10001,
		0x01111,
		0x00001,
		0x01110,
	},

	{			// :
		0x00000,
		0x00100,
		0x00000,
		0x00100,
		0x00000,
	},

	{			// ;
		0x00000,
		0x00100,
		0x00000,
		0x00100,
		0x01000,
	},

	{			// <
		0x00010,
		0x00100,
		0x01000,
		0x00100,
		0x00010,
	},

	{			// =
		0x00000,
		0x01110,
		0x00000,
		0x01110,
		0x00000,
	},

	{			// >
		0x01000,
		0x00100,
		0x00010,
		0x00100,
		0x01000,
	},


	{			// ?
		0x01110,
		0x10001,
		0x00110,
		0x00000,
		0x00100,
	},

	{			// @
		0x01110,
		0x10001,
		0x10011,
		0x10000,
		0x01110,
	},

	{			// A
		0x01110,
		0x10001,
		0x11111,
		0x10001,
		0x10001,
	},

	{			// B
		0x11110,
		0x10001,
		0x11110,
		0x10001,
		0x11110,
	},

	{			// C
		0x01110,
		0x10001,
		0x10000,
		0x10001,
		0x01110,
	},

	{			// D
		0x11110,
		0x10001,
		0x10001,
		0x10001,
		0x11110,
	},

	{			// E
		0x11111,
		0x10000,
		0x11100,
		0x10000,
		0x11111,
	},

	{			// F
		0x11111,
		0x10000,
		0x11100,
		0x10000,
		0x10000,
	},


	{			// G
		0x01110,
		0x10000,
		0x10011,
		0x10001,
		0x01110,
	},

	{			// H
		0x10001,
		0x10001,
		0x11111,
		0x10001,
		0x10001,
	},

	{			// I
		0x01110,
		0x00100,
		0x00100,
		0x00100,
		0x01110,
	},

	{			// J
		0x00111,
		0x00010,
		0x00010,
		0x10010,
		0x01100,
	},

	{			// K
		0x10001,
		0x10010,
		0x11110,
		0x10001,
		0x10001,
	},

	{			// L
		0x10000,
		0x10000,
		0x10000,
		0x10000,
		0x11111,
	},

	{			// M
		0x10001,
		0x11011,
		0x10101,
		0x10101,
		0x10001,
	},

	{			// N
		0x10001,
		0x11001,
		0x10101,
		0x10011,
		0x10001,
	},

	{			// O
		0x01110,
		0x10001,
		0x10001,
		0x10001,
		0x01110,
	},

	{			// P
		0x11110,
		0x10001,
		0x10001,
		0x11110,
		0x10000,
	},

	{			// Q
		0x01110,
		0x10001,
		0x10001,
		0x01110,
		0x00011,
	},

	{			// R
		0x11110,
		0x10001,
		0x11110,
		0x10001,
		0x10001,
	},

	{			// S
		0x01110,
		0x10000,
		0x11110,
		0x00001,
		0x11110,
	},

	{			// T
		0x11111,
		0x00100,
		0x00100,
		0x00100,
		0x00100,
	},

	{			// U
		0x10001,
		0x10001,
		0x10001,
		0x10001,
		0x01110,
	},

	{			// V
		0x10001,
		0x10001,
		0x10001,
		0x01010,
		0x00100,
	},

	{			// W
		0x10001,
		0x10101,
		0x10101,
		0x11011,
		0x10001,
	},

	{			// X
		0x10001,
		0x01010,
		0x00100,
		0x01010,
		0x10001,
	},

	{			// Y
		0x10001,
		0x10001,
		0x01110,
		0x00100,
		0x00100,
	},

	{			// Z
		0x11111,
		0x00010,
		0x00100,
		0x01000,
		0x11111,
	},

	{			// [
		0x01110,
		0x01000,
		0x01000,
		0x01000,
		0x01110,
	},

	{			// backslash
		0x10000,
		0x01000,
		0x00100,
		0x00010,
		0x00001,
	},

	{			// ]
		0x01110,
		0x00010,
		0x00010,
		0x00010,
		0x01110,
	},

	{			// ^
		0x00100,
		0x01010,
		0x10001,
		0x00000,
		0x00000,
	},

	{			// -
		0x00000,
		0x00000,
		0x11111,
		0x00000,
		0x00000,
	},
};


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CMarkers
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  InitLevel
///////////////////////////////////////////////////////////////////////////////

void			CScrollBars::Init   			(unsigned initMode)
{	
	if (initMode==INIT_CORE)
	{
#if __BANK
		m_disable = false;
#endif
	}
	else if (initMode==INIT_AFTER_MAP_LOADED)
	{
		// initialise the data
		for (s32 i=0; i<SCROLLBARTYPE_MAX; i++)	
		{
			m_strings[i][0] = 0;
		}

		m_numScrollbars[0] = 0;
		m_numScrollbars[1] = 0;

		// load in the data
		const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::SCROLLBARS_FILE);

		if (DATAFILEMGR.IsValid(pData))
		{
			// load the episode effects
			LoadData(pData->m_filename);
		}
		else
		{
			LoadData("common:/data/scrollbars.dat");
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void			CScrollBars::Shutdown				(unsigned UNUSED_PARAM(shutdownMode))
{	
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CScrollBars::InitWidgets			()
{	
	bkBank* pVfxBank = vfxWidget::GetBank();
	pVfxBank->PushGroup("Scrollbars", false);
	{
		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("DEBUG");
		pVfxBank->AddToggle("Disable", &m_disable);
	}
	pVfxBank->PopGroup();
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  LoadData
///////////////////////////////////////////////////////////////////////////////

void			CScrollBars::LoadData				(const char* filename)
{	
	// open the data file
	FileHandle fileId = CFileMgr::OpenFile(filename, "rb");
	vfxAssertf(CFileMgr::IsValidFileHandle(fileId), "CScrollBars::LoadData - could not load %s file", filename);

	ScrollbarType_e currType = SCROLLBARTYPE_INVALID;
	char* pLine;
	while ((pLine = CFileMgr::ReadLine(fileId)) != NULL)
	{
		if (*pLine != '#' && *pLine != '\0')
		{
			if (strcmp(pLine, "*financial")==0)
			{
				vfxAssertf(currType == SCROLLBARTYPE_INVALID, "CScrollBars::LoadData - scrollbars.dat has unexpected ordering");
				currType = SCROLLBARTYPE_FINANCIAL;
			}
			else if (strcmp(pLine, "*theatre")==0)
			{
				vfxAssertf(currType == SCROLLBARTYPE_FINANCIAL, "CScrollBars::LoadData - scrollbars.dat has unexpected ordering");
				currType = SCROLLBARTYPE_THEATRE;

			}
			else if (strcmp(pLine, "*advertising")==0)
			{
				vfxAssertf(currType == SCROLLBARTYPE_THEATRE, "CScrollBars::LoadData - scrollbars.dat has unexpected ordering");
				currType = SCROLLBARTYPE_ADVERTISING;
			}
			else if (strcmp(pLine, "*clock")==0)
			{
				vfxAssertf(currType == SCROLLBARTYPE_ADVERTISING, "CScrollBars::LoadData - scrollbars.dat has unexpected ordering");
				currType = SCROLLBARTYPE_CLOCK;
			}
			else if (strcmp(pLine, "*urls")==0)
			{
				vfxAssertf(currType == SCROLLBARTYPE_CLOCK, "CScrollBars::LoadData - scrollbars.dat has unexpected ordering");
				currType = SCROLLBARTYPE_URLS;
			}
			else if (strcmp(pLine, "*comedyclub")==0)
			{
				vfxAssertf(currType == SCROLLBARTYPE_URLS, "CScrollBars::LoadData - scrollbars.dat has unexpected ordering");
				currType = SCROLLBARTYPE_COMEDYCLUB;
			}
			else if (strcmp(pLine, "*traffic")==0)
			{
				vfxAssertf(currType == SCROLLBARTYPE_COMEDYCLUB, "CScrollBars::LoadData - scrollbars.dat has unexpected ordering");
				currType = SCROLLBARTYPE_TRAFFIC;
			}
			else if (strcmp(pLine, "*news")==0)
			{
				vfxAssertf(currType == SCROLLBARTYPE_TRAFFIC, "CScrollBars::LoadData - scrollbars.dat has unexpected ordering");
				currType = SCROLLBARTYPE_NEWS;
			}
			else
			{
				vfxAssertf(strlen(m_strings[currType]) + strlen(pLine) + 10 < SCROLLBARS_MAX_STRING_LEN, "CScrollBars::LoadData - too many chars in scrollbars for one type");
				strcat(m_strings[currType], pLine);
				strcat(m_strings[currType], "       ");
			}
		}
	}

	CFileMgr::CloseFile(fileId);
}


///////////////////////////////////////////////////////////////////////////////
//  Render
///////////////////////////////////////////////////////////////////////////////

// TODO: tidy up this function

void			CScrollBars::Render					()
{
#if __BANK
	if (m_disable)
	{
		return;
	}
#endif

	// get the buffer id
	s32 bufferId = gRenderThreadInterface.GetRenderBuffer();

	// return if no scroll bars are active
	if (m_numScrollbars[bufferId] == 0)
	{
		return;
	}

	// set up the colour of the individual light that makes up the scroll text
	Color32 lightCol(SCROLLBARS_LIGHT_COL_R, SCROLLBARS_LIGHT_COL_G, SCROLLBARS_LIGHT_COL_B, SCROLLBARS_LIGHT_COL_A);

	// ...
	bool bBrokenLights = true;

	// set up the render state
	CDistantLightsVertexBuffer::SetStateBlocks(false,false);
	grcViewport::SetCurrentWorldIdentity();
	grcWorldIdentity();

	// set the texture
	rage::grcTexture* pTexture = CShaderLib::LookupTexture("corona");
	vfxAssertf(pTexture, "CScrollBars::Render - corona texture lookup failed");
	grcBindTexture(pTexture);

	// init the render buffers
	// Vec3V	vPosBuffer[DISTANTLIGHTS_BUFFER_SIZE];
	// Color32	colBuffer[DISTANTLIGHTS_BUFFER_SIZE];

	// go through the scroll bars
	for (s32 i=0; i<m_numScrollbars[bufferId]; i++)
	{
		Scrollbar_t& currScrollbar = m_scrollbars[bufferId][i];

		const CScrollBarAttr* pScrollbarAttr = currScrollbar.pScrollbarAttr;

		// set the hdr for this scroll bar
		CShaderLib::SetGlobalEmissiveScale(SCROLLBARS_HDR_MULT * currScrollbar.lightfade);

		Vec3V vPos;
		pScrollbarAttr->GetPos(RC_VECTOR3(vPos));

		Vec3V centrePoint = Transform(currScrollbar.matrix, Vec3V(pScrollbarAttr->m_centrePos.x, pScrollbarAttr->m_centrePos.y, pScrollbarAttr->m_centrePos.z));

		if (grcViewport::GetCurrent()->IsSphereVisible(VEC3V_ARGS(centrePoint), pScrollbarAttr->m_radius))
		{
			char* pString = m_strings[pScrollbarAttr->m_scrollBarType];
			s32 numChars = istrlen(pString);
			vfxAssertf(numChars > 0, "CScrollBars::Render - string with no characters found");

			s32 numColumns = numChars * (5+SCROLLBARS_FONT_SPACE_WIDTH);


			s32 currColumn;
			if (pScrollbarAttr->m_scrollBarType == SCROLLBARTYPE_CLOCK)
			{
				currColumn = 0;
			}
			else
			{
				s32 div = 40 + (((s32)(vPos.GetZf()*1234.5678f)) % 23);
				currColumn = (fwTimer::GetTimeInMilliseconds() / div) % numColumns;
			}

			char tempString[SCROLLBARS_MAX_STRING_LEN];
			strcpy(tempString, pString);

			// if time is required we stick this in
			for (s32 j=0; j<numChars; j++)
			{
				if (tempString[j] == '~')
				{
					BuildClockString(&tempString[j]);
				}
			}

			// go through the segments of the p2dEffect to calculate their coordinates
			Vec3V vPointCoors[MAX_POINTS_IN_SCROLLBAR+1];
			vPointCoors[0] = Transform(currScrollbar.matrix, vPos);

			for (s32 j=0; j<pScrollbarAttr->m_numPoints; j++)
			{
				vPointCoors[j+1] = Transform(currScrollbar.matrix, Vec3V(vPos.GetXf() + pScrollbarAttr->m_pointsX[j]*SCROLLBARS_COMPRESS_MULT, vPos.GetYf() + pScrollbarAttr->m_pointsY[j]*SCROLLBARS_COMPRESS_MULT, vPos.GetZf()));
			} 

			float lightDist;
			if (pScrollbarAttr->m_scrollBarType == SCROLLBARTYPE_CLOCK)
			{		
				// For clocks we need to work out what the full length is of the banner to work out the light distance.
				float bannerLength = 0.0f;
				for (s32 j=0; j<pScrollbarAttr->m_numPoints; j++)
				{
					Vec3V vec = vPointCoors[j] - vPointCoors[j+1];
					vec.SetZ(ScalarV(V_ZERO));
					bannerLength += Mag(vec).Getf();
				}
				vfxAssertf(bannerLength > 0.0f, "CScrollBars::Render - non positive banner length found");
				lightDist = bannerLength / (5 * (5 + SCROLLBARS_FONT_SPACE_WIDTH));	// We need space for the 5 character of the clock 12:34
				bBrokenLights = false;
			}
			else
			{
				lightDist = pScrollbarAttr->m_height / SCROLLBARS_FONT_HEIGHT;
			}


			// ScalarV vSize = ScalarVFromF32(SCROLLBARS_SIZE_MULT * pScrollbarAttr->m_height);
			//Vec3V vRight = (grcViewport::GetCurrent()->GetCameraMtx().GetCol0()) * vSize;
			//Vec3V vUp = -(grcViewport::GetCurrent()->GetCameraMtx().GetCol1()) * vSize;

			// Go through the segments of the p2dEffect
			for (s32 j=0; j<pScrollbarAttr->m_numPoints; j++)
			{
				Vec3V vec = vPointCoors[j] - vPointCoors[j+1];

				float dist = Mag(vec).Getf();
				s32 lightsPerSegment = (s32)(dist/lightDist);

				// Go through the points on the segment of the p2dEffect
				for (s32 k=0; k<lightsPerSegment; k++)
				{
					u32 charToDisplay = tempString[(currColumn/(5+SCROLLBARS_FONT_SPACE_WIDTH)) % numChars]; 
					s32 columnToDisplay = currColumn % (5+SCROLLBARS_FONT_SPACE_WIDTH);

					if (columnToDisplay < 5)	// Skip the spaces.
					{
						// Turn lowercase into uppercase
						if (charToDisplay >= 'a' && charToDisplay <= 'z')
						{
							charToDisplay -= (u32)('a' - 'A');
						}

						charToDisplay = charToDisplay - ' ';

						for (s32 cy=0; cy<SCROLLBARS_FONT_HEIGHT; cy++)		// Go through the 5 points in the column
						{
							if (g_scrollbarFont[charToDisplay][cy] & (((u64)1) << ((4-columnToDisplay)*4)))		// was (4-columnToDisplay)
							{
								Vec3V vSpriteCoors = vPointCoors[j];
								vSpriteCoors += (vPointCoors[j+1] - vPointCoors[j]) * ScalarVFromF32(float(k) / lightsPerSegment);
								vSpriteCoors.SetZf(vSpriteCoors.GetZf() - pScrollbarAttr->m_height * (((float)(cy)) / 5));

								s32 offIndex = (k * 25 + columnToDisplay * 5 + cy)& 127;
								if ((offIndex != 101 && offIndex != 48) || !bBrokenLights)	// Some lights are broken.
								{
//									g_distantLights.AddToRenderBuffer(vSpriteCoors, lightCol);
								}
							}
						}
					}

					currColumn = (currColumn+1) % numColumns;
				}
			}

			CDistantLightsVertexBuffer::RenderBuffer(false);
		}
	}

	// reset the render state
	CShaderLib::SetGlobalEmissiveScale(1.0f);

	// reset the number of scrollbars now they are rendered
	m_numScrollbars[bufferId] = 0;
}


///////////////////////////////////////////////////////////////////////////////
//  Setup
///////////////////////////////////////////////////////////////////////////////

void			CScrollBars::Setup					(CScrollBarAttr& scrollbarAttr)
{
	Vec3V vCentrePos;
	scrollbarAttr.GetPos(RC_VECTOR3(vCentrePos));

	Vec3V vPos = vCentrePos;
	CScrollBarAttr* sc = scrollbarAttr.GetScrollBar();

	for (s32 i=0; i<sc->m_numPoints-1; i++)
	{
		float f = 1.0f / (i+1.0f);
		vCentrePos = vCentrePos * ScalarVFromF32(1.0f-f) + Vec3V(sc->m_pointsX[i]*SCROLLBARS_COMPRESS_MULT, sc->m_pointsY[i]*SCROLLBARS_COMPRESS_MULT, vPos.GetZf()) * ScalarVFromF32(f);
	}

	float radius = Mag(vPos-vCentrePos).Getf();
	for (s32 i=0; i<sc->m_numPoints-1; i++)
	{
		float dist = Mag(Vec3V(sc->m_pointsX[i]*SCROLLBARS_COMPRESS_MULT, sc->m_pointsY[i]*SCROLLBARS_COMPRESS_MULT, vPos.GetZf()) - vCentrePos).Getf();
		radius = Max(radius, dist);
	}

	sc->m_radius = radius;

	sc->m_centrePos.x = vCentrePos.GetXf();
	sc->m_centrePos.y = vCentrePos.GetYf();
	sc->m_centrePos.z = vCentrePos.GetZf();
}



///////////////////////////////////////////////////////////////////////////////
//  Register
///////////////////////////////////////////////////////////////////////////////

void			CScrollBars::Register				(CEntity* const pEntity, const CScrollBarAttr& scrollbarAttr, float lightFade)
{
	s32 bufferId = gRenderThreadInterface.GetUpdateBuffer();
	if (m_numScrollbars[bufferId] < SCROLLBARS_MAX)
	{
		m_scrollbars[bufferId][m_numScrollbars[bufferId]].pScrollbarAttr = &scrollbarAttr;
		m_scrollbars[bufferId][m_numScrollbars[bufferId]].matrix = pEntity->GetMatrix();
		m_scrollbars[bufferId][m_numScrollbars[bufferId]].lightfade = lightFade;
		m_numScrollbars[bufferId]++;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ClearNews
///////////////////////////////////////////////////////////////////////////////

void			CScrollBars::ClearNews				()
{
	m_strings[SCROLLBARTYPE_NEWS][0] = ' ';
	m_strings[SCROLLBARTYPE_NEWS][1] = 0;
}


///////////////////////////////////////////////////////////////////////////////
//  AddNews
///////////////////////////////////////////////////////////////////////////////

void			CScrollBars::AddNews				(const char *pString)
{
	vfxAssertf(strlen(m_strings[SCROLLBARTYPE_NEWS]) + strlen(pString) + 10 < SCROLLBARS_MAX_STRING_LEN, "CScrollBars::AddNews - ADD_STRING_TO_NEWS_SCROLLBAR too much text displayed");
	if (strlen(m_strings[SCROLLBARTYPE_NEWS])+strlen(pString)+10 < SCROLLBARS_MAX_STRING_LEN)
	{
		strcat(m_strings[SCROLLBARTYPE_NEWS], pString);
		strcat(m_strings[SCROLLBARTYPE_NEWS], " ");
	}
}


///////////////////////////////////////////////////////////////////////////////
//  BuildClockString
///////////////////////////////////////////////////////////////////////////////

void			CScrollBars::BuildClockString		(char* pString)
{
	if (CClock::GetHour()>=10)
	{
		pString[0] = '0' + (s8)(CClock::GetHour() / 10);
	}
	else
	{
		pString[0] = ' ';
	}

	pString[1] = '0' + (s8)(CClock::GetHour() % 10);

	if ((fwTimer::GetTimeInMilliseconds() / 500) & 1)
	{
		pString[2] = ' ';
	}
	else
	{
		pString[2] = ':';
	}

	pString[3] = '0' + (s8)(CClock::GetMinute() / 10);
	pString[4] = '0' + (s8)(CClock::GetMinute() % 10);
}