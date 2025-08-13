///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxLiquid.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	19 August 2011
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxLiquid.h"

// rage

// framework
#include "fwsys/gameskeleton.h"
#include "vfx/channel.h"
#include "vfx/vfxwidget.h"

// game
#include "Scene/DataFileMgr.h"
#include "Vfx/Decals/DecalManager.h"


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
	
CVfxLiquid		g_vfxLiquid;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxLiquid
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxLiquid::Init							(unsigned initMode)
{
	if (initMode==INIT_CORE)
	{
#if __BANK
		m_bankInitialised = false;
#endif
	}
	else if (initMode == INIT_AFTER_MAP_LOADED)
    {
	    // load in the data from the file
	    LoadData();
    }
}

///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxLiquid::Shutdown						(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
    {
    }
}

///////////////////////////////////////////////////////////////////////////////
//  LoadData
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxLiquid::LoadData						()
{
	// initialise the data
	m_numVfxLiquidInfos = 0;

	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::LIQUIDFX_FILE);
	while(DATAFILEMGR.IsValid(pData))
	{
		// read in the data
		fiStream* stream = ASSET.Open(pData->m_filename, "dat", true);
		vfxAssertf(stream, "cannot open data file");

		if (stream)
		{
			// initialise the tokens
			fiAsciiTokenizer token;
			token.Init("liquidFx", stream);

			// count the number of liquids
			s32 phase = -1;	
			char charBuff[128];

			// read in the version
			m_version = token.GetFloat();
			// m_version = m_version;

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
				if (stricmp(charBuff, "LIQUIDFX_START")==0)
				{
					phase = 0;
					continue;
				}
				if (stricmp(charBuff, "LIQUIDFX_END")==0)
				{
					break;
				}

				// phase 0 
				if (phase==0)
				{
					// ignore the liquid name, for now

					// read in the vfx group that this liquid uses
					token.GetToken(charBuff, 128);
					m_vfxLiquidInfo[m_numVfxLiquidInfos].vfxGroup = phMaterialGta::FindVfxGroup(charBuff);
					if (m_vfxLiquidInfo[m_numVfxLiquidInfos].vfxGroup==VFXGROUP_UNDEFINED)
					{
#if CHECK_VFXGROUP_DATA
						vfxAssertf(0, "invalid vfx group found in liquidfx.dat %s", charBuff);
#endif
					}

					// read in the durations
					m_vfxLiquidInfo[m_numVfxLiquidInfos].durationFoot = token.GetFloat();
					m_vfxLiquidInfo[m_numVfxLiquidInfos].durationWheel = token.GetFloat();

					// read in the decal lives
					m_vfxLiquidInfo[m_numVfxLiquidInfos].decalLifeFoot = token.GetFloat();
					m_vfxLiquidInfo[m_numVfxLiquidInfos].decalLifeWheel = token.GetFloat();

					// normal decal pool data
					s32 decalDataId = token.GetInt();
					if (decalDataId>0)
					{
						g_decalMan.FindRenderSettingInfo(decalDataId, m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalPoolRenderSettingIndex, m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalPoolRenderSettingCount);
					}
					else
					{
						m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalPoolRenderSettingIndex = -1;
						m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalPoolRenderSettingCount = 0;
					}

					// normal decal trail data
					decalDataId = token.GetInt();
					if (decalDataId>0)
					{
						g_decalMan.FindRenderSettingInfo(decalDataId, m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalTrailInRenderSettingIndex, m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalTrailInRenderSettingCount);
					}
					else
					{
						m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalTrailInRenderSettingIndex = -1;
						m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalTrailInRenderSettingCount = 0;
					}

					if (m_version>=6.0)
					{
						decalDataId = token.GetInt();
						if (decalDataId>0)
						{
							g_decalMan.FindRenderSettingInfo(decalDataId, m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalTrailMidRenderSettingIndex, m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalTrailMidRenderSettingCount);
						}
						else
						{
							m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalTrailMidRenderSettingIndex = -1;
							m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalTrailMidRenderSettingCount = 0;
						}

						decalDataId = token.GetInt();
						if (decalDataId>0)
						{
							g_decalMan.FindRenderSettingInfo(decalDataId, m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalTrailOutRenderSettingIndex, m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalTrailOutRenderSettingCount);
						}
						else
						{
							m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalTrailOutRenderSettingIndex = -1;
							m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalTrailOutRenderSettingCount = 0;
						}
					}
					else
					{
						m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalTrailMidRenderSettingIndex = m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalTrailInRenderSettingIndex;
						m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalTrailMidRenderSettingCount = m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalTrailInRenderSettingCount;

						m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalTrailOutRenderSettingIndex = m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalTrailInRenderSettingIndex;
						m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalTrailOutRenderSettingCount = m_vfxLiquidInfo[m_numVfxLiquidInfos].normDecalTrailInRenderSettingCount;
					}

					if (m_version>=2.0f)
					{
						// soak decal pool data
						s32 decalDataId = token.GetInt();
						if (decalDataId>0)
						{
							g_decalMan.FindRenderSettingInfo(decalDataId, m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalPoolRenderSettingIndex, m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalPoolRenderSettingCount);
						}
						else
						{
							m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalPoolRenderSettingIndex = -1;
							m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalPoolRenderSettingCount = 0;
						}

						// soak decal trail data
						decalDataId = token.GetInt();
						if (decalDataId>0)
						{
							g_decalMan.FindRenderSettingInfo(decalDataId, m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailInRenderSettingIndex, m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailInRenderSettingCount);
						}
						else
						{
							m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailInRenderSettingIndex = -1;
							m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailInRenderSettingCount = 0;
						}

						if (m_version>=6.0)
						{
							decalDataId = token.GetInt();
							if (decalDataId>0)
							{
								g_decalMan.FindRenderSettingInfo(decalDataId, m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailMidRenderSettingIndex, m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailMidRenderSettingCount);
							}
							else
							{
								m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailMidRenderSettingIndex = -1;
								m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailMidRenderSettingCount = 0;
							}

							decalDataId = token.GetInt();
							if (decalDataId>0)
							{
								g_decalMan.FindRenderSettingInfo(decalDataId, m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailOutRenderSettingIndex, m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailOutRenderSettingCount);
							}
							else
							{
								m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailOutRenderSettingIndex = -1;
								m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailOutRenderSettingCount = 0;
							}
						}
						else
						{
							m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailMidRenderSettingIndex = m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailInRenderSettingIndex;
							m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailMidRenderSettingCount = m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailInRenderSettingCount;

							m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailOutRenderSettingIndex = m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailInRenderSettingIndex;
							m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailOutRenderSettingCount = m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailInRenderSettingCount;
						}
					}
					else
					{
						m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalPoolRenderSettingIndex = -1;
						m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalPoolRenderSettingCount = 0;
						m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailInRenderSettingIndex = -1;
						m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailInRenderSettingCount = 0;
						m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailMidRenderSettingIndex = -1;
						m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailMidRenderSettingCount = 0;
						m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailOutRenderSettingIndex = -1;
						m_vfxLiquidInfo[m_numVfxLiquidInfos].soakDecalTrailOutRenderSettingCount = 0;
					}

					if (m_version>=6.0f)
					{
						m_vfxLiquidInfo[m_numVfxLiquidInfos].inOutDecalSize = token.GetFloat();
					}
					else
					{
						m_vfxLiquidInfo[m_numVfxLiquidInfos].inOutDecalSize = 0.0f;
					}

					if (m_version>=4.0f)
					{
						m_vfxLiquidInfo[m_numVfxLiquidInfos].decalColR = (u8)token.GetInt();
						m_vfxLiquidInfo[m_numVfxLiquidInfos].decalColG = (u8)token.GetInt();
						m_vfxLiquidInfo[m_numVfxLiquidInfos].decalColB = (u8)token.GetInt();

						if (m_version>=5.0f)
						{
							m_vfxLiquidInfo[m_numVfxLiquidInfos].decalColA = (u8)token.GetInt();
						}
						else
						{
							m_vfxLiquidInfo[m_numVfxLiquidInfos].decalColA = 255;
						}
					}
					else
					{
						m_vfxLiquidInfo[m_numVfxLiquidInfos].decalColR = 255;
						m_vfxLiquidInfo[m_numVfxLiquidInfos].decalColG = 255;
						m_vfxLiquidInfo[m_numVfxLiquidInfos].decalColB = 255;
						m_vfxLiquidInfo[m_numVfxLiquidInfos].decalColA = 255;
					}

					// splash fx system
					if (m_version>=3.0f)
					{
						token.GetToken(charBuff, 128);		
						if (stricmp(charBuff, "-")==0)
						{	
							m_vfxLiquidInfo[m_numVfxLiquidInfos].ptFxSplashHashName = (u32)0;					
						}
						else
						{
							m_vfxLiquidInfo[m_numVfxLiquidInfos].ptFxSplashHashName = atHashWithStringNotFinal(charBuff);
						}
					}
					else
					{
						m_vfxLiquidInfo[m_numVfxLiquidInfos].ptFxSplashHashName = (u32)0;		
					}

					// increment number of infos
					m_numVfxLiquidInfos++;
				}
			}

			stream->Close();
		}

		// get next file
		pData = DATAFILEMGR.GetPrevFile(pData);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void		 	CVfxLiquid::InitWidgets					()
{	
	if (m_bankInitialised==false)
	{
//		char txt[128];
//		u32 materialInfoSize = sizeof(VfxMaterialInfo_s);
//		float materialInfoPoolSize = (materialInfoSize * VFXMATERIAL_MAX_INFOS) / 1024.0f;

		bkBank* pVfxBank = vfxWidget::GetBank();
		pVfxBank->PushGroup("Vfx Liquid", false);
		{
// 			pVfxBank->AddTitle("INFO");
// 			sprintf(txt, "Num Bang Infos (%d - %.2fK)", VFXMATERIAL_MAX_INFOS, materialInfoPoolSize);
// 			pVfxBank->AddSlider(txt, &m_numVfxMtlBangInfos, 0, VFXMATERIAL_MAX_INFOS, 0);
// 			sprintf(txt, "Num Scrape Infos (%d - %.2fK)", VFXMATERIAL_MAX_INFOS, materialInfoPoolSize);
// 			pVfxBank->AddSlider(txt, &m_numVfxMtlScrapeInfos, 0, VFXMATERIAL_MAX_INFOS, 0);
// 
// #if __DEV
// 			pVfxBank->AddTitle("");
// 			pVfxBank->AddTitle("SETTINGS");
// 			pVfxBank->AddSlider("Bang History Dist", &VFXMATERIAL_BANG_HIST_DIST, 0.0f, 10.0f, 0.25f, NullCB, "The distance at which a new bang effect will be rejected if a history exists within this range");
// 			pVfxBank->AddSlider("Bang History Time", &VFXMATERIAL_BANG_HIST_TIME, 0.0f, 10.0f, 0.25f, NullCB, "The time that a history will exist for a new bang effect");
// 			pVfxBank->AddSlider("Heli Rotor Spawn Dist", &VFXMATERIAL_ROTOR_SPAWN_DIST, 0.0f, 5.0f, 0.05f, NullCB, "The spawn distance that is fed into the material effect for heli rotor blades");
// 			pVfxBank->AddSlider("Heli Rotor Size Scale", &VFXMATERIAL_ROTOR_SIZE_SCALE, 0.0f, 10.0f, 0.25f, NullCB, "The scale that is applied to the material effect for heli rotor blades");
// 			pVfxBank->AddSlider("Wheel Coln Dot Thresh", &VFXMATERIAL_WHEEL_COLN_DOT_THRESH, 0.0f, 10.0f, 0.25f, NullCB, "The dot product threshold at which wheel collisions are rejected (vehicle up with coln normal)");
// #endif
// 
// 			pVfxBank->AddTitle("");
// 			pVfxBank->AddTitle("DEBUG");
// 			pVfxBank->AddToggle("Disable Bang Fx", &m_disbaleVfxBang);
// 			pVfxBank->AddToggle("Disable Bang Marks", &m_disableBangMarks);
// 			pVfxBank->AddToggle("Disable Scrape Fx", &m_disableVfxScrape);
// 			pVfxBank->AddToggle("Disable Scrape Trails", &m_disableScrapeTrails);
// 			pVfxBank->AddToggle("Disable Mtl Mtl Fx A", &m_disableFxMtlMtlA);
// 			pVfxBank->AddToggle("Disable Mtl Mtl Fx B", &m_disableFxMtlMtlB);
// 			pVfxBank->AddToggle("Disable Wheel Collisions", &m_disableWheelCollisions);
// 			
// 
// 			pVfxBank->AddToggle("Output Bang Debug", &m_outputBangDebug);
// 			pVfxBank->AddToggle("Output Scrape Debug", &m_outputScrapeDebug);
// 
// 			pVfxBank->AddToggle("Render Mtl Mtl Debug", &m_renderMtlMtlDebug);

			pVfxBank->AddTitle("");
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
void			CVfxLiquid::Reset							()
{
	g_vfxLiquid.Shutdown(SHUTDOWN_WITH_MAP_LOADED);
	g_vfxLiquid.Init(INIT_AFTER_MAP_LOADED);
}
#endif




///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxLiquidAttachInfo
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Init
///////////////////////////////////////////////////////////////////////////////

void CVfxLiquidAttachInfo::Init() 
{
	m_liquidAttachType = VFXMTL_WET_MODE_WHEEL; 
	m_liquidType = VFXLIQUID_TYPE_WATER; 
	m_timer = 0.0f;
};


///////////////////////////////////////////////////////////////////////////////
// Update
///////////////////////////////////////////////////////////////////////////////

void CVfxLiquidAttachInfo::Update(float dt) 
{
	m_timer = MAX(0.0f, m_timer-dt); 
	if (m_timer==0.0f) 
	{
		m_liquidType = VFXLIQUID_TYPE_WATER;
	}
}


///////////////////////////////////////////////////////////////////////////////
// Set
///////////////////////////////////////////////////////////////////////////////

void CVfxLiquidAttachInfo::Set(VfxLiquidAttachType_e liquidAttachType, VfxLiquidType_e liquidType) 
{
	m_liquidAttachType = liquidAttachType; 
	m_liquidType = liquidType;
	
	if (m_liquidAttachType==VFXMTL_WET_MODE_FOOT)
	{
		m_timer = g_vfxLiquid.GetLiquidInfo(m_liquidType).durationFoot;
	}
	else
	{
		m_timer = g_vfxLiquid.GetLiquidInfo(m_liquidType).durationWheel;
	}
}


///////////////////////////////////////////////////////////////////////////////
// GetCurrAlpha
///////////////////////////////////////////////////////////////////////////////

float CVfxLiquidAttachInfo::GetCurrAlpha() const 
{
	if (m_liquidAttachType==VFXMTL_WET_MODE_FOOT)
	{
		return m_timer/g_vfxLiquid.GetLiquidInfo(m_liquidType).durationFoot;
	}
	else
	{
		return m_timer/g_vfxLiquid.GetLiquidInfo(m_liquidType).durationWheel;
	}
}


///////////////////////////////////////////////////////////////////////////////
// GetDecalLife
///////////////////////////////////////////////////////////////////////////////

float CVfxLiquidAttachInfo::GetDecalLife() const 
{
	if (m_liquidAttachType==VFXMTL_WET_MODE_FOOT)
	{
		return g_vfxLiquid.GetLiquidInfo(m_liquidType).decalLifeFoot;
	}
	else
	{
		return g_vfxLiquid.GetLiquidInfo(m_liquidType).decalLifeWheel;
	}
}