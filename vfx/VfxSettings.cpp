///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxHelper.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	12 Jun 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxSettings.h"

// rage
#include "file/token.h"

// framework
#include "vfx/channel.h"

// game
#include "Core/Game.h"
#include "Scene/DataFileMgr.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_PTFX_OPTIMISATIONS()
VFX_DECAL_OPTIMISATIONS()
VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CVfxSettings	g_vfxSettings;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxSettings
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

bool			CVfxSettings::Init  							(unsigned initMode)
{
	if (initMode==INIT_AFTER_MAP_LOADED)
	{
		// init the settings variables
		//	strcpy(m_ptfxNameWindDebris, "");
		m_ptfxAirResistance = 1.0f;
		m_genWaterLevel = 0.0f;
		m_genHorizonHeight = 0.0f;

		// get the settings file name
		const char* pFilename = NULL;
		const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::VFX_SETTINGS_FILE);
		while (DATAFILEMGR.IsValid(pData))
		{
			pFilename = pData->m_filename;
			pData = DATAFILEMGR.GetNextFile(pData);
		}

		if (pFilename==NULL)
		{
			return false;
		}

		// open the settings file
		fiStream* stream = fiStream::Open(pFilename, true);
		if (stream)
		{		
			// initialise the tokens
			fiAsciiTokenizer token;
			token.Init("vfxsettings", stream);

			char settingName[128];
			//		char settingVal[128];

			while (token.GetToken(settingName, 128)>0)
			{	
				if(strcmp(settingName, "PTFX_AIR_RESISTANCE")==0)
				{
					m_ptfxAirResistance = token.GetFloat();
				}
				else if(strcmp(settingName, "GEN_WATERLEVEL")==0)
				{
					m_genWaterLevel = token.GetFloat();
				}
				else if(strcmp(settingName, "GEN_HORIZONHEIGHT")==0)
				{
					m_genHorizonHeight = token.GetFloat();
				}
				//			else if (strcmp(settingName, "PTFXNAME_WINDDEBRIS")==0)
				//			{
				//				token.GetToken(settingVal, 32);
				//				strcpy(m_ptfxNameWindDebris, settingVal);
				//			}
				else
				{
					vfxAssertf(0, "invalid setting name (%s) found in %s", settingName, pFilename);
				}
			}

			stream->Close();
		}
	}

	return true;
}







