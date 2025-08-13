///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxScript.cpp 
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	24 Nov 2011
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxScript.h"

// rage
#include "file/asset.h"
#include "file/token.h"
#include "system/param.h"

// framework
#include "fwsys/gameskeleton.h"
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxwrapper.h"
#include "vfx/vfxwidget.h"

// game 
#include "Scene/DataFileMgr.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CVfxScript		g_vfxScript;

class CVFXScriptFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile& file)
	{
		g_vfxScript.AppendData(file.m_filename);
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile & /*file*/)
	{

	}

} g_vfxScriptFileMounter;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxScript
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void CVfxScript::AppendData(const char* file)
{
	// initialise the data
	s32 phase = -1;	

	// read in the data
	fiStream* stream = ASSET.Open(file, "dat", true);
	if (stream)
	{
		vfxAssertf(stream, "cannot open data file");
		// initialise the tokens
		fiAsciiTokenizer token;
		token.Init("scriptFx", stream);

		// count the number of materials
		char charBuff[128];

		while (1)
		{
			token.GetToken(charBuff, 128);

			// check for commented lines
			if (charBuff[0]=='#')
			{
				token.SkipToEndOfLine();
				continue;
			}

			// check for change of phase
			if (stricmp(charBuff, "SCRIPTFX_INFO_START")==0)
			{
				phase = 0;
				continue;
			}
			else if (stricmp(charBuff, "SCRIPTFX_INFO_END")==0)
			{
				phase = -1;
				continue;
			}
			else if (stricmp(charBuff, "CUTSCENEFX_INFO_START")==0)
			{
				phase = 1;
				continue;
			}
			else if (stricmp(charBuff, "CUTSCENEFX_INFO_END")==0)
			{
				phase = -1;
				break;
			}

			// phase 0
			if (phase==0)
			{
				atHashWithStringNotFinal scriptNameHash = atHashWithStringNotFinal(charBuff);

				token.GetToken(charBuff, 128);

				VfxScriptInfo_s scriptInfo;
				scriptInfo.ptfxAssetName = charBuff;

				m_scriptInfoMap.SafeInsert(scriptNameHash, scriptInfo);
			}
			// phase 1
			else if (phase==1)
			{
				atHashWithStringNotFinal cutsceneNameHash = atHashWithStringNotFinal(charBuff);

				token.GetToken(charBuff, 128);

				VfxScriptInfo_s scriptInfo;
				scriptInfo.ptfxAssetName = charBuff;

				m_cutsceneInfoMap.SafeInsert(cutsceneNameHash, scriptInfo);
			}
		}

		stream->Close();
	}
	m_scriptInfoMap.FinishInsertion();
	m_cutsceneInfoMap.FinishInsertion();
}

void		 	CVfxScript::Init							(unsigned initMode)
{
	if (initMode==INIT_CORE)
	{
#if __BANK
		m_bankInitialised = false;
#endif
	}
    else if (initMode==INIT_AFTER_MAP_LOADED)
    {
	    // load in the data from the file
		CDataFileMount::RegisterMountInterface(CDataFileMgr::SCRIPTFX_FILE, &g_vfxScriptFileMounter);
	    LoadData();
    }
}

///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxScript::Shutdown						(unsigned shutdownMode)
{
    if (shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
    {
    }
}

///////////////////////////////////////////////////////////////////////////////
//  LoadData
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxScript::LoadData						()
{
	// initialise the data
	s32 phase = -1;	
	m_scriptInfoMap.Reset();
	m_cutsceneInfoMap.Reset();

	// read in the data
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::SCRIPTFX_FILE);
	while (DATAFILEMGR.IsValid(pData))
	{
		fiStream* stream = ASSET.Open(pData->m_filename, "dat", true);
		vfxAssertf(stream, "cannot open data file");

		if (stream)
		{		
			// initialise the tokens
			fiAsciiTokenizer token;
			token.Init("scriptFx", stream);

			// count the number of materials
			char charBuff[128];

			// read in the version
			m_version = token.GetFloat();

			while (1)
			{
				token.GetToken(charBuff, 128);

				// check for commented lines
				if (charBuff[0]=='#')
				{
					token.SkipToEndOfLine();
					continue;
				}

				// check for change of phase
				if (stricmp(charBuff, "SCRIPTFX_INFO_START")==0)
				{
					phase = 0;
					continue;
				}
				else if (stricmp(charBuff, "SCRIPTFX_INFO_END")==0)
				{
					phase = -1;
					if (m_version<2.0f)
					{
						break;
					}
					else
					{
						continue;
					}
				}
				else if (stricmp(charBuff, "CUTSCENEFX_INFO_START")==0)
				{
					phase = 1;
					continue;
				}
				else if (stricmp(charBuff, "CUTSCENEFX_INFO_END")==0)
				{
					phase = -1;
					break;
				}

				// phase 0
				if (phase==0)
				{
					atHashWithStringNotFinal scriptNameHash = atHashWithStringNotFinal(charBuff);

					token.GetToken(charBuff, 128);

					VfxScriptInfo_s scriptInfo;
					scriptInfo.ptfxAssetName = charBuff;

					m_scriptInfoMap.Insert(scriptNameHash, scriptInfo);
				}
				// phase 1
				else if (phase==1)
				{
					atHashWithStringNotFinal cutsceneNameHash = atHashWithStringNotFinal(charBuff);

					token.GetToken(charBuff, 128);

					VfxScriptInfo_s scriptInfo;
					scriptInfo.ptfxAssetName = charBuff;

					m_cutsceneInfoMap.Insert(cutsceneNameHash, scriptInfo);
				}
			}

			stream->Close();
		}

		// Get next file
		pData = DATAFILEMGR.GetPrevFile(pData);
	}

	m_scriptInfoMap.FinishInsertion();
	m_cutsceneInfoMap.FinishInsertion();
}


///////////////////////////////////////////////////////////////////////////////
//  GetScriptPtFxAssetName
///////////////////////////////////////////////////////////////////////////////

atHashWithStringNotFinal CVfxScript::GetScriptPtFxAssetName	(const char* pScriptName)
{
	atHashWithStringNotFinal scriptHashName = atHashWithStringNotFinal(pScriptName);
	return GetScriptPtFxAssetName(scriptHashName);
}

///////////////////////////////////////////////////////////////////////////////
//  GetScriptPtFxAssetName
///////////////////////////////////////////////////////////////////////////////

atHashWithStringNotFinal CVfxScript::GetScriptPtFxAssetName	(atHashWithStringNotFinal pScriptHashName)
{
	VfxScriptInfo_s* pScriptInfo = m_scriptInfoMap.SafeGet(pScriptHashName);
	if (pScriptInfo)
	{
		return pScriptInfo->ptfxAssetName;
	}

	return atHashWithStringNotFinal(u32(0));
}


///////////////////////////////////////////////////////////////////////////////
//  GetCutscenePtFxAssetName
///////////////////////////////////////////////////////////////////////////////

atHashWithStringNotFinal CVfxScript::GetCutscenePtFxAssetName(const char* pCutsceneName)
{
	atHashWithStringNotFinal scriptHashName = atHashWithStringNotFinal(pCutsceneName);
	VfxScriptInfo_s* pScriptInfo = m_cutsceneInfoMap.SafeGet(scriptHashName);
	if (pScriptInfo)
	{
		return pScriptInfo->ptfxAssetName;
	}

	return atHashWithStringNotFinal(u32(0));
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void		 	CVfxScript::InitWidgets						()
{
	if (m_bankInitialised==false)
 	{
		bkBank* pVfxBank = vfxWidget::GetBank();
		pVfxBank->PushGroup("Vfx Script", false);
		{
			pVfxBank->AddTitle("DATA FILE");
			pVfxBank->AddButton("Reload", Reset);
		}
		pVfxBank->PopGroup();

 		m_bankInitialised = true;
 	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
// Reset
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CVfxScript::Reset							()
{
	g_vfxScript.Shutdown(SHUTDOWN_WITH_MAP_LOADED);
	g_vfxScript.Init(INIT_AFTER_MAP_LOADED);
}
#endif // __BANK





