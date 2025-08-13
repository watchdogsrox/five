///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxMaterial.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	01 June 2006
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxMaterial.h"

// rage
#if __BANK
#include "input/mouse.h"
#endif

// framework
#include "fwscene/stores/txdstore.h"
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxhistory.h"
#include "vfx/ptfx/ptfxwrapper.h"
#include "vfx/vfxwidget.h"

// game
#include "Control/Replay/Replay.h"
#include "Control/Replay/Effects/ParticleMaterialFxPacket.h"
#include "Control/Replay/Effects/ProjectedTexturePacket.h"
#include "Core/Game.h"
#include "Debug/Debug.h"
#include "modelinfo/VehicleModelInfoColors.h"
#include "Peds/Ped.h"
#include "Physics/GtaMaterialManager.h"
#include "Scene/DataFileMgr.h"
#include "Scene/Entity.h"
#include "Vehicles/Automobile.h"
#include "Vehicles/Vehicle.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Metadata/VfxVehicleInfo.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/Systems/VfxTrail.h"
#include "Vfx/VfxHelper.h"

#if __BANK
#include "Debug/DebugScene.h"
#endif


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

// rag tweakable settings
dev_float	VFXMATERIAL_ANGLE_RATIO_BIAS		= 0.8f;
dev_float	VFXMATERIAL_BANG_HIST_DIST			= 0.5f;
dev_float	VFXMATERIAL_BANG_HIST_TIME			= 0.5f;
dev_float	VFXMATERIAL_ROTOR_SPAWN_DIST		= 0.3f;
dev_float	VFXMATERIAL_ROTOR_SIZE_SCALE		= 1.2f;
dev_float	VFXMATERIAL_WHEEL_COLN_DOT_THRESH	= 0.7f;
dev_float	VFXMATERIAL_LOD_RANGE_SCALE_VEHICLE = 3.0f;
dev_float	VFXMATERIAL_LOD_RANGE_SCALE_HELI	= 5.0f;


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////
	
CVfxMaterial	g_vfxMaterial;

#if __BANK
bool	CVfxMaterial::ms_debugScrapeRecording = false;
bool	CVfxMaterial::ms_debugScrapeApplying = false;
int		CVfxMaterial::ms_debugScrapeRecordCount = 0;
int		CVfxMaterial::ms_debugScrapeApplyCount = 0;
VfxDebugScrapeInfo_s CVfxMaterial::ms_debugScrapeInfos  [VFXMATERIAL_MAX_DEBUG_SCRAPES];
#endif


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxMaterial
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxMaterial::Init							(unsigned initMode)
{
	if (initMode==INIT_CORE)
	{
		m_bangScrapePtFxLodRangeScaleScriptThreadId = THREAD_INVALID;
		m_bangScrapePtFxLodRangeScale = 1.0f;

#if __BANK
		m_bankInitialised = false;

		m_colnVfxProcessPerContact = false;
		m_colnVfxDisableTimeSlice1 = false;
		m_colnVfxDisableTimeSlice2 = false;
		m_colnVfxDisableWheels = false;
		m_colnVfxDisableEntityA = false;
		m_colnVfxDisableEntityB = false;
		m_colnVfxRecomputePositions = false;
		m_colnVfxOutputDebug = false;
		m_colnVfxRenderDebugVectors = false;
		m_colnVfxRenderDebugImpulses = false;

		m_bangVfxAlwaysPassVelocityThreshold = false;
		m_bangVfxAlwaysPassImpulseThreshold = false;
		m_bangDecalsDisabled = false;
		m_bangPtFxDisabled = false;
		m_bangPtFxOutputDebug = false;
		m_bangPtFxRenderDebug = false;
		m_bangPtFxRenderDebugSize = 1.0f;
		m_bangPtFxRenderDebugTimer = -1;

		m_scrapeVfxAlwaysPassVelocityThreshold = false;
		m_scrapeVfxAlwaysPassImpulseThreshold = false;
		m_scrapeDecalsDisabled = false;
		m_scrapeDecalsDisableSingle = false;
		//m_scrapeTrailsUseId = false;
		m_scrapePtFxDisabled = false;
		m_scrapePtFxOutputDebug = false;
		m_scrapePtFxRenderDebug = false;
		m_scrapePtFxRenderDebugSize = 1.0f;
		m_scrapePtFxRenderDebugTimer = -1;
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

void		 	CVfxMaterial::Shutdown						(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
    {
    }
}

///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxMaterial::Update						()
{
#if __BANK
	UpdateDebugScrapeRecording();
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxMaterial::RemoveScript							(scrThreadId scriptThreadId)
{
	if (scriptThreadId==m_bangScrapePtFxLodRangeScaleScriptThreadId)
	{
		m_bangScrapePtFxLodRangeScale = 1.0f;
		m_bangScrapePtFxLodRangeScaleScriptThreadId = THREAD_INVALID;
	}
}

///////////////////////////////////////////////////////////////////////////////
//  LoadData
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxMaterial::LoadData						()
{
	// initialise the data
	for (s32 i=0; i<NUM_VFX_GROUPS; i++)
	{
		for (s32 j=0; j<NUM_VFX_GROUPS; j++)
		{
			m_vfxMaterialTable[i][j].id = 0;
			m_vfxMaterialTable[i][j].offset[0] = -1;
			m_vfxMaterialTable[i][j].count[0] = -1;
			m_vfxMaterialTable[i][j].offset[1] = -1;
			m_vfxMaterialTable[i][j].count[1] = -1;
		}
	}

	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::MATERIALFX_FILE);
	while(DATAFILEMGR.IsValid(pData))
	{
		// read in the data
		fiStream* stream = ASSET.Open(pData->m_filename, "dat", true);
		vfxAssertf(stream, "cannot open data file");

		if (stream)
		{
#if CHECK_VFXGROUP_DATA
			bool vfxGroupVerify[NUM_VFX_GROUPS];
			for (s32 i=0; i<NUM_VFX_GROUPS; i++)
			{
				vfxGroupVerify[i] = false;
			}
#endif
			// initialise the tokens
			fiAsciiTokenizer token;
			token.Init("materialFx", stream);

			// count the number of materials
			s32 phase = -1;	
			m_numVfxMtlBangInfos = 0;
			m_numVfxMtlScrapeInfos = 0;
			char charBuff[128];
			s32* pNumMtlInfos = NULL;
			VfxMaterialInfo_s* pVfxMtlInfo = NULL;

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
				if (stricmp(charBuff, "MTLFX_TABLE_START")==0)
				{
					phase = 0;
					continue;
				}
				if (stricmp(charBuff, "MTLFX_TABLE_END")==0)
				{
					phase = -1;
					continue;
				}
				else if (stricmp(charBuff, "MTLFX_BANG_INFO_START")==0)
				{
					phase = 1;
					pNumMtlInfos = &m_numVfxMtlBangInfos;
					pVfxMtlInfo = m_vfxMtlBangInfo;
					continue;
				}
				else if (stricmp(charBuff, "MTLFX_BANG_INFO_END")==0)
				{
					phase = -1;
					continue;
				}
				else if (stricmp(charBuff, "MTLFX_SCRAPE_INFO_START")==0)
				{
					phase = 1;
					pNumMtlInfos = &m_numVfxMtlScrapeInfos;
					pVfxMtlInfo = m_vfxMtlScrapeInfo;
					continue;
				}
				else if (stricmp(charBuff, "MTLFX_SCRAPE_INFO_END")==0)
				{
					break;
				}

				// phase 0 - material effect table
				if (phase==0)
				{
					VfxGroup_e vfxGroup = phMaterialGta::FindVfxGroup(charBuff);
					if (vfxGroup==VFXGROUP_UNDEFINED)
					{
#if CHECK_VFXGROUP_DATA
						vfxAssertf(0, "invalid vfx group found in materialfx.dat %s", charBuff);
#endif
						token.SkipToEndOfLine();
					}	
					else
					{
#if CHECK_VFXGROUP_DATA
						vfxAssertf(vfxGroupVerify[vfxGroup]==false, "duplicate vfx group data found in materialfx.dat for %s", charBuff);
						vfxGroupVerify[vfxGroup]=true;
#endif

						// debug colours
						if (m_version>=4.0f)
						{
							BANK_ONLY(s32 debugColR =) token.GetInt();
							BANK_ONLY(s32 debugColG =) token.GetInt();
							BANK_ONLY(s32 debugColB =) token.GetInt();
#if __BANK
							m_vfxGroupDebugCol[vfxGroup].SetRed(debugColR);
							m_vfxGroupDebugCol[vfxGroup].SetGreen(debugColG);
							m_vfxGroupDebugCol[vfxGroup].SetBlue(debugColB);
							m_vfxGroupDebugCol[vfxGroup].SetAlpha(255);
#endif
						}
#if __BANK
						else
						{
							m_vfxGroupDebugCol[vfxGroup].SetRed(255);
							m_vfxGroupDebugCol[vfxGroup].SetGreen(255);
							m_vfxGroupDebugCol[vfxGroup].SetBlue(255);
							m_vfxGroupDebugCol[vfxGroup].SetAlpha(255);
						}
#endif

						// read in the material info across all other vfx groups
						char lineTxt[1024];
						s32 numRead = token.GetLine(lineTxt, 1024, false);
						if (numRead==1024)
						{
							vfxAssertf(0, "not enough space allocated to read line fully");
						}

						char* val;
						const char* delimit = " ,\t";

						s32 index = 0;
						val = strtok(lineTxt, delimit);
						while (val != NULL)
						{
							if (index<NUM_VFX_GROUPS)
							{
								m_vfxMaterialTable[vfxGroup][index++].id = atoi(val);
							}
#if CHECK_VFXGROUP_DATA
							else
							{
								vfxAssertf(0, "too many vfx group columns in materialfx.dat for %s", g_fxGroupsList[vfxGroup]);
							}
#endif

							val = strtok(NULL, delimit);
						}

#if CHECK_VFXGROUP_DATA
						vfxAssertf(index==NUM_VFX_GROUPS, "not enough vfx group columns in materialfx.dat for %s", g_fxGroupsList[vfxGroup]);
#endif
					}
				}
				// phase 1 - effect info
				else if (phase==1)
				{
					vfxAssertf(*pNumMtlInfos<VFXMATERIAL_MAX_INFOS, "not enough space for new material info");
					if (*pNumMtlInfos<VFXMATERIAL_MAX_INFOS)
					{
						// id
						pVfxMtlInfo[*pNumMtlInfos].id = atoi(&charBuff[0]);

						// velocity thresh
						pVfxMtlInfo[*pNumMtlInfos].velThreshMin = token.GetFloat();
						pVfxMtlInfo[*pNumMtlInfos].velThreshMax = token.GetFloat();

						if (pVfxMtlInfo[*pNumMtlInfos].velThreshMin<0.0f)
						{
							pVfxMtlInfo[*pNumMtlInfos].velThreshMin = FLT_MAX;
							pVfxMtlInfo[*pNumMtlInfos].velThreshMax = FLT_MAX;
						}

						vfxAssertf(pVfxMtlInfo[*pNumMtlInfos].velThreshMin <=
							pVfxMtlInfo[*pNumMtlInfos].velThreshMax, "min velocity threshold is greater than max");

						// impulse thresh
						pVfxMtlInfo[*pNumMtlInfos].impThreshMin = token.GetFloat();
						pVfxMtlInfo[*pNumMtlInfos].impThreshMax = token.GetFloat();

						if (pVfxMtlInfo[*pNumMtlInfos].impThreshMin<0.0f)
						{
							pVfxMtlInfo[*pNumMtlInfos].impThreshMin = FLT_MAX;
							pVfxMtlInfo[*pNumMtlInfos].impThreshMax = FLT_MAX;
						}

						vfxAssertf(pVfxMtlInfo[*pNumMtlInfos].impThreshMin <=
							pVfxMtlInfo[*pNumMtlInfos].impThreshMax, "min impulse threshold is greater than max");
	
						// skip underwater
						if (m_version>=6.0f)
						{
							s32 ptFxSkipUnderwater = token.GetInt();
							pVfxMtlInfo[*pNumMtlInfos].ptFxSkipUnderwater = ptFxSkipUnderwater>0;
						}
						else
						{
							pVfxMtlInfo[*pNumMtlInfos].ptFxSkipUnderwater = false;
						}

						// fx system
						token.GetToken(charBuff, 128);		
						if (stricmp(charBuff, "-")==0)
						{	
							pVfxMtlInfo[*pNumMtlInfos].ptFxHashName = (u32)0;					
						}
						else
						{
							pVfxMtlInfo[*pNumMtlInfos].ptFxHashName = atHashWithStringNotFinal(charBuff);
						}

						// overlays / rejection
						if (m_version>=3.0f)
						{
							// decal id
							s32 decalDataId = token.GetInt();
							if (decalDataId>0)
							{
								g_decalMan.FindRenderSettingInfo(decalDataId, pVfxMtlInfo[*pNumMtlInfos].decalRenderSettingIndex, pVfxMtlInfo[*pNumMtlInfos].decalRenderSettingCount);
							}
							else
							{
								pVfxMtlInfo[*pNumMtlInfos].decalRenderSettingIndex = -1;
								pVfxMtlInfo[*pNumMtlInfos].decalRenderSettingCount = 0;
							}

							// rgb
							pVfxMtlInfo[*pNumMtlInfos].decalColR = (u8)token.GetInt();
							pVfxMtlInfo[*pNumMtlInfos].decalColG = (u8)token.GetInt();
							pVfxMtlInfo[*pNumMtlInfos].decalColB = (u8)token.GetInt();

							// tex size
							pVfxMtlInfo[*pNumMtlInfos].texWidthMin = token.GetFloat();
							pVfxMtlInfo[*pNumMtlInfos].texWidthMax = token.GetFloat();

							vfxAssertf(pVfxMtlInfo[*pNumMtlInfos].texWidthMin <=
								pVfxMtlInfo[*pNumMtlInfos].texWidthMax, "min texture width is greater than max");

							if (m_version>=5.0f)
							{
								pVfxMtlInfo[*pNumMtlInfos].texLengthMultMin = token.GetFloat();
								pVfxMtlInfo[*pNumMtlInfos].texLengthMultMax = token.GetFloat();
							}
							else
							{
								pVfxMtlInfo[*pNumMtlInfos].texLengthMultMin = 1.0f;
								pVfxMtlInfo[*pNumMtlInfos].texLengthMultMax = 1.0f;
							}

							vfxAssertf(pVfxMtlInfo[*pNumMtlInfos].texLengthMultMin <=
								pVfxMtlInfo[*pNumMtlInfos].texLengthMultMax, "min texture length multiplier is greater than max");

							// overlays / rejection 
							pVfxMtlInfo[*pNumMtlInfos].maxOverlayRadius = token.GetFloat();
							pVfxMtlInfo[*pNumMtlInfos].duplicateRejectDist = token.GetFloat();

							vfxAssertf(pVfxMtlInfo[*pNumMtlInfos].maxOverlayRadius==0.0f || pVfxMtlInfo[*pNumMtlInfos].duplicateRejectDist==0.0f, "max overlay radius and duplicate reject dist are both specified - these are mutually exclusive");
						}
						else
						{
							// tex size
							pVfxMtlInfo[*pNumMtlInfos].texWidthMin = token.GetFloat();
							pVfxMtlInfo[*pNumMtlInfos].texWidthMax = token.GetFloat();

							vfxAssertf(pVfxMtlInfo[*pNumMtlInfos].texWidthMin <=
								pVfxMtlInfo[*pNumMtlInfos].texWidthMax, "min texture width is greater than max");

							pVfxMtlInfo[*pNumMtlInfos].texLengthMultMin = 1.0f;
							pVfxMtlInfo[*pNumMtlInfos].texLengthMultMax = 1.0f;

							// decal id
							s32 decalDataId = token.GetInt();
							if (decalDataId>0)
							{
								g_decalMan.FindRenderSettingInfo(decalDataId, pVfxMtlInfo[*pNumMtlInfos].decalRenderSettingIndex, pVfxMtlInfo[*pNumMtlInfos].decalRenderSettingCount);
							}
							else
							{
								pVfxMtlInfo[*pNumMtlInfos].decalRenderSettingIndex = -1;
								pVfxMtlInfo[*pNumMtlInfos].decalRenderSettingCount = 0;
							}

							// rgb
							if (m_version>=2.0f)
							{
								pVfxMtlInfo[*pNumMtlInfos].decalColR = (u8)token.GetInt();
								pVfxMtlInfo[*pNumMtlInfos].decalColG = (u8)token.GetInt();
								pVfxMtlInfo[*pNumMtlInfos].decalColB = (u8)token.GetInt();
							}
							else
							{
								pVfxMtlInfo[*pNumMtlInfos].decalColR = 255;
								pVfxMtlInfo[*pNumMtlInfos].decalColG = 255;
								pVfxMtlInfo[*pNumMtlInfos].decalColB = 255;
							}

							// overlays / rejection 
							pVfxMtlInfo[*pNumMtlInfos].maxOverlayRadius = 0.2f;
							pVfxMtlInfo[*pNumMtlInfos].duplicateRejectDist = 0.0f;
						}
						
						// increment number of infos
						(*pNumMtlInfos)++;
					}
				}
			}

#if CHECK_VFXGROUP_DATA
			for (s32 i=0; i<NUM_VFX_GROUPS; i++)
			{
				vfxAssertf(vfxGroupVerify[i]==true, "missing vfx group data found in materialfx.dat for %s", g_fxGroupsList[i]);
			}
#endif

			stream->Close();
		}


		// get next file
		pData = DATAFILEMGR.GetPrevFile(pData);
	}

	ProcessLookUpData();

	// check the data's integrity
	for (s32 i=0; i<NUM_VFX_GROUPS; i++)
	{
		for (s32 j=0; j<NUM_VFX_GROUPS; j++)
		{
			GetBangInfo((VfxGroup_e)i, (VfxGroup_e)j);
			GetScrapeInfo((VfxGroup_e)i, (VfxGroup_e)j);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessLookUpData
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxMaterial::ProcessLookUpData				()
{
	// fill in the offset and count data in the lookup table
	s32 currId = -1;
	s32 currOffset = 0;
	s32 currCount = 0;
	for (s32 i=0; i<m_numVfxMtlBangInfos; i++)
	{
		// check if the id is the same
		if (m_vfxMtlBangInfo[i].id == currId)
		{
			// same id - update the info
			currCount++;
		}

		// check if the id has changed 
		if (m_vfxMtlBangInfo[i].id!=currId)
		{
			// new id - store the old info
			StoreLookUpData(0, currId, currOffset, currCount);

			// set up the new info
			currId = m_vfxMtlBangInfo[i].id;
			currOffset = i;
			currCount = 1;
		}
	}

	// store the final info
	StoreLookUpData(0, currId, currOffset, currCount);

	// fill in the offset and count data in the lookup table
	currId = -1;
	currOffset = 0;
	currCount = 0;
	for (s32 i=0; i<m_numVfxMtlScrapeInfos; i++)
	{
		// check if the id is the same
		if (m_vfxMtlScrapeInfo[i].id == currId)
		{
			// assert no longer valid as we now do scuffs instead of scrape trails
			//vfxAssertf(0, "multiple scrape entries have the same id (%d) in materialfx.dat - this will cause inconsisent scrapes", m_vfxMtlScrapeInfo[i].id);
			
			// same id - update the info
			currCount++;
		}

		// check if the id has changed 
		if (m_vfxMtlScrapeInfo[i].id!=currId)
		{
			// new id - store the old info
			StoreLookUpData(1, currId, currOffset, currCount);

			// set up the new info
			currId = m_vfxMtlScrapeInfo[i].id;
			currOffset = i;
			currCount = 1;
		}
	}

	// store the final info
	StoreLookUpData(1, currId, currOffset, currCount);
}


///////////////////////////////////////////////////////////////////////////////
//  StoreLookUpData
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxMaterial::StoreLookUpData				(s32 index, s32 currId, s32 currOffset, s32 currCount)
{
	if (currId>-1)
	{
		for (s32 j=0; j<NUM_VFX_GROUPS; j++)
		{
			for (s32 k=0; k<NUM_VFX_GROUPS; k++)
			{
				if (m_vfxMaterialTable[j][k].id==currId)
				{
					vfxAssertf(m_vfxMaterialTable[j][k].offset[index]==-1, "material table offset entry already set");
					vfxAssertf(m_vfxMaterialTable[j][k].count[index]==-1, "material table offset count already set");
					m_vfxMaterialTable[j][k].offset[index] = currOffset;
					m_vfxMaterialTable[j][k].count[index] = currCount;
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// GetBangInfo
///////////////////////////////////////////////////////////////////////////////

VfxMaterialInfo_s* 	CVfxMaterial::GetBangInfo				(VfxGroup_e vfxGroupA, VfxGroup_e vfxGroupB)
{
	if (vfxVerifyf(vfxGroupA>=0 && vfxGroupA<NUM_VFX_GROUPS && 
				   vfxGroupB>=0 && vfxGroupB<NUM_VFX_GROUPS, "invalid vfx group pass in %d %d", vfxGroupA, vfxGroupB))
	{
		// get the id from the table
		s32 offset = m_vfxMaterialTable[vfxGroupA][vfxGroupB].offset[0];

		// check for no infos
		if (offset==-1)
		{
			return NULL;
		}

		s32 count = m_vfxMaterialTable[vfxGroupA][vfxGroupB].count[0];
		s32 rand = g_DrawRand.GetRanged(offset, offset+count-1);

		return &m_vfxMtlBangInfo[rand];
	}

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
// GetScrapeInfo
///////////////////////////////////////////////////////////////////////////////

VfxMaterialInfo_s* 	CVfxMaterial::GetScrapeInfo				(VfxGroup_e vfxGroupA, VfxGroup_e vfxGroupB)
{
	if (vfxVerifyf(vfxGroupA>=0 && vfxGroupA<NUM_VFX_GROUPS && 
				   vfxGroupB>=0 && vfxGroupB<NUM_VFX_GROUPS, "invalid vfx group pass in %d %d", vfxGroupA, vfxGroupB))
	{
		// get the id from the table
		s32 offset = m_vfxMaterialTable[vfxGroupA][vfxGroupB].offset[1];

		// check for no infos
		if (offset==-1)
		{
			return NULL;
		}

		s32 count = m_vfxMaterialTable[vfxGroupA][vfxGroupB].count[1];
		s32 rand = g_DrawRand.GetRanged(offset, offset+count-1);

		return &m_vfxMtlScrapeInfo[rand];
	}

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessCollisionVfx
///////////////////////////////////////////////////////////////////////////////

bool			CVfxMaterial::ProcessCollisionVfx			(VfxCollisionInfo_s& vfxColnInfo)
{
	// do distance check
	float distSqrThresh = VFXRANGE_MTL_SCRAPE_BANG_SQR;
	distSqrThresh *= m_bangScrapePtFxLodRangeScale*m_bangScrapePtFxLodRangeScale;

	float bangVehicleEvo = 0.0f;
	float scrapeVehicleEvo = 0.0f;
	float bangZoom = 1.0f;
	float scrapeZoom = 1.0f;
	float lodRangeScale = 1.0f;
	if (vfxColnInfo.pEntityA->GetIsTypeVehicle())
	{
		lodRangeScale = VFXMATERIAL_LOD_RANGE_SCALE_VEHICLE;
		CVehicle* pVehicle = static_cast<CVehicle*>(vfxColnInfo.pEntityA);
		if (pVehicle->GetVehicleType()==VEHICLE_TYPE_HELI || 
			pVehicle->GetVehicleType()==VEHICLE_TYPE_PLANE ||
			pVehicle->GetVehicleType()==VEHICLE_TYPE_BLIMP)
		{
			lodRangeScale = VFXMATERIAL_LOD_RANGE_SCALE_HELI;
		}

		CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
		if (pVfxVehicleInfo)
		{
			bangVehicleEvo = pVfxVehicleInfo->GetMtlBangPtFxVehicleEvo();
			scrapeVehicleEvo = pVfxVehicleInfo->GetMtlScrapePtFxVehicleEvo();

			bangZoom = pVfxVehicleInfo->GetMtlBangPtFxVehicleScale();
			scrapeZoom = pVfxVehicleInfo->GetMtlScrapePtFxVehicleScale();
		}
	}

	if (vfxColnInfo.pEntityB->GetIsTypeVehicle())
	{
		lodRangeScale = Max(lodRangeScale, VFXMATERIAL_LOD_RANGE_SCALE_VEHICLE);
		CVehicle* pVehicle = static_cast<CVehicle*>(vfxColnInfo.pEntityB);
		if (pVehicle->GetVehicleType()==VEHICLE_TYPE_HELI || 
			pVehicle->GetVehicleType()==VEHICLE_TYPE_PLANE ||
			pVehicle->GetVehicleType()==VEHICLE_TYPE_BLIMP)
		{
			lodRangeScale = Max(lodRangeScale, VFXMATERIAL_LOD_RANGE_SCALE_HELI);
		}

		CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
		if (pVfxVehicleInfo)
		{
			bangVehicleEvo = Max(bangVehicleEvo, pVfxVehicleInfo->GetMtlBangPtFxVehicleEvo());
			scrapeVehicleEvo = Max(scrapeVehicleEvo, pVfxVehicleInfo->GetMtlScrapePtFxVehicleEvo());
		}
	}

	distSqrThresh *= lodRangeScale*lodRangeScale;
	if (CVfxHelper::GetDistSqrToCamera(vfxColnInfo.vWorldPosA)>distSqrThresh)
	{
		return false;
	}

	// check for 'deep' materials (e.g. snow)
	Vec3V vColnPosAdjusted = vfxColnInfo.vWorldPosA;
	Vec3V vColnNormalAdjusted = vfxColnInfo.vWorldNormalA;
	VfxGroup_e vfxGroupMtlB = PGTAMATERIALMGR->GetMtlVfxGroup(vfxColnInfo.mtlIdB);
	if (vfxGroupMtlB==VFXGROUP_SNOW_LOOSE || vfxGroupMtlB==VFXGROUP_SNOW_COMPACT)
	{
		CVfxDeepSurfaceInfo vfxDeepSurfaceInfo;
		vfxDeepSurfaceInfo.Process(vfxColnInfo.vWorldPosA, vfxColnInfo.vWorldNormalA, vfxGroupMtlB, VFXRANGE_DEEP_SURFACE_MTL_SQR);

		if (vfxDeepSurfaceInfo.IsActive())
		{
			vColnPosAdjusted += Vec3V(0.0f, 0.0f, vfxDeepSurfaceInfo.GetDepth());
		}
	}

	// calc the bang and scrape magnitude
	ScalarV dotNV = Dot(vfxColnInfo.vWorldNormalA, vfxColnInfo.vColnVel);
	Vec3V vColnVelInN = vfxColnInfo.vWorldNormalA * dotNV;
	Vec3V vColnVelPerpToN = vfxColnInfo.vColnVel-vColnVelInN;
	float bangMag = Mag(vColnVelInN).Getf();
	float scrapeMag = Mag(vColnVelPerpToN).Getf();

#if __BANK
	// debug stuff
	if (g_vfxMaterial.m_colnVfxRenderDebugVectors)
	{
		Color32 colCyan(0.0f, 1.0f, 1.0f, 1.0f);
		Color32 colMagneta(1.0f, 0.0f, 1.0f, 1.0f);
		grcDebugDraw::Line(RCC_VECTOR3(vfxColnInfo.vWorldPosA), VEC3V_TO_VECTOR3(vfxColnInfo.vWorldPosA+vColnVelInN), colCyan, -1);
		grcDebugDraw::Line(RCC_VECTOR3(vfxColnInfo.vWorldPosA), VEC3V_TO_VECTOR3(vfxColnInfo.vWorldPosA+vColnVelPerpToN), colMagneta, -1);
	}
#endif

	// find out whether this predominantly a bang or a scrape and re-adjust the data accordingly
	float bangRatio = bangMag / (bangMag+scrapeMag);
	float scrapeRatio = scrapeMag / (bangMag+scrapeMag);
	if (scrapeRatio>=bangRatio+VFXMATERIAL_ANGLE_RATIO_BIAS)
	{
		DoMtlScrapeFx(vfxColnInfo.pEntityA, vfxColnInfo.componentIdA, vfxColnInfo.pEntityB, vfxColnInfo.componentIdB, vColnPosAdjusted, -vColnNormalAdjusted, vfxColnInfo.vColnVel, vfxColnInfo.mtlIdA, vfxColnInfo.mtlIdB, vfxColnInfo.vColnDir, scrapeMag, vfxColnInfo.vAccumImpulse.Getf(), lodRangeScale, scrapeVehicleEvo, scrapeZoom);
	}
	else
	{		
		DoMtlBangFx(vfxColnInfo.pEntityA, vfxColnInfo.componentIdA, vColnPosAdjusted, -vColnNormalAdjusted, vfxColnInfo.vColnVel, vfxColnInfo.mtlIdA, vfxColnInfo.mtlIdB, bangMag, vfxColnInfo.vAccumImpulse.Getf(), lodRangeScale, bangVehicleEvo, bangZoom);
	}

	return true;
}


///////////////////////////////////////////////////////////////////////////////
//  DoMtlBangFx
///////////////////////////////////////////////////////////////////////////////

void			CVfxMaterial::DoMtlBangFx					(CEntity* pEntity, s32 componentId, Vec3V_In vCollPos, Vec3V_In vCollNormal, Vec3V_In vCollVel, phMaterialMgr::Id mtlIdA, phMaterialMgr::Id mtlIdB, float bangMag, float accumImpulse, float lodRangeScale, float vehicleEvo, float zoom)
{
	// set up the collision matrix
	Vec3V vRight = vCollVel;
	vRight = Normalize(vRight);
	Vec3V vForward = vCollNormal;
	vForward = Normalize(vForward);
	Vec3V vUp = Cross(vRight, vForward);

	// check for a zero up vector being calculated - we can't get a valid matrix from this
	if (IsZeroAll(vUp))
	{
		return;
	}

	vUp = Normalize(vUp);
	vRight = Cross(vForward, vUp);

	Mat34V vColnMtx;
	vColnMtx.SetCol0(vRight);
	vColnMtx.SetCol1(vForward);
	vColnMtx.SetCol2(vUp);
	vColnMtx.SetCol3(vCollPos);

	vfxAssertf(vColnMtx.IsOrthonormal3x3(ScalarV(V_ONE)), "collision vfx matrix isn't orthonormal (vRight:%.3f,%.3f,%.3f - vForward:%.3f,%.3f,%.3f - vUp:%.3f,%.3f,%.3f - vColnVel:%.3f,%.3f,%.3f - vWorldNormalA:%.3f,%.3f,%.3f - vAccumImpulse:%.3f)", 
		vRight.GetXf(), vRight.GetYf(), vRight.GetZf(), 
		vForward.GetXf(), vForward.GetYf(), vForward.GetZf(), 
		vUp.GetXf(), vUp.GetYf(), vUp.GetZf(),
		vCollVel.GetXf(), vCollVel.GetYf(), vCollVel.GetZf(),
		vCollNormal.GetXf(), vCollNormal.GetYf(), vCollNormal.GetZf(),
		accumImpulse);

#if __BANK
	if (m_bangPtFxRenderDebug)
	{
		grcDebugDraw::Sphere(vCollPos, m_bangPtFxRenderDebugSize, Color32(1.0f, 0.0f, 0.0f, 1.0f), false, m_bangPtFxRenderDebugTimer);

		char mtlIdAName[64] = "";
		PGTAMATERIALMGR->GetMaterialName(PGTAMATERIALMGR->UnpackMtlId(mtlIdA), mtlIdAName, sizeof(mtlIdAName));

		char mtlIdBName[64] = "";
		PGTAMATERIALMGR->GetMaterialName(PGTAMATERIALMGR->UnpackMtlId(mtlIdB), mtlIdBName, sizeof(mtlIdBName));

		char displayTxt[1024] = "";
		char tempTxt[1024];

		formatf(tempTxt, 256, "%s v %s - vel:%.3f - imp:%.3f\n", mtlIdAName, mtlIdBName, bangMag, accumImpulse);
		formatf(displayTxt, 256, "%s", tempTxt);

		grcDebugDraw::Text(vCollPos, Color32(1.0f, 0.0f, 1.0f, 1.0f), 0, 0, displayTxt, false, m_bangPtFxRenderDebugTimer);
	}
#endif

	if (pEntity==NULL)
	{
		return;
	}

	if (pEntity->IsArchetypeSet() && pEntity->GetBoundRadius()<0.2f)
	{
		return;
	}

	if (pEntity->GetIsTypeVehicle() && PGTAMATERIALMGR->UnpackMtlId(mtlIdA)==PGTAMATERIALMGR->g_idCarVoid)
	{
		mtlIdA = PGTAMATERIALMGR->g_idCarMetal;
	}

	VfxMaterialInfo_s* pFxMaterialInfo = g_vfxMaterial.GetBangInfo(PGTAMATERIALMGR->GetMtlVfxGroup(mtlIdA), PGTAMATERIALMGR->GetMtlVfxGroup(mtlIdB));
	if (pFxMaterialInfo)
	{
		bool hydraulicsHack = false;
		if (pEntity->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
			if (pVehicle->InheritsFromAutomobile())
			{
				CAutomobile* pAutomobile = static_cast<CAutomobile*>(pVehicle);
				if (pAutomobile->m_nAutomobileFlags.bHydraulicsBounceRaising || pAutomobile->m_nAutomobileFlags.bHydraulicsBounceLanding)
				{
					static float bangMagRatio = 0.0f;
					static float accumImpulseRatio = 0.0f;
					bangMag = pFxMaterialInfo->velThreshMin + (bangMagRatio*(pFxMaterialInfo->velThreshMax-pFxMaterialInfo->velThreshMin));
					accumImpulse = pFxMaterialInfo->impThreshMin + (accumImpulseRatio*(pFxMaterialInfo->impThreshMax-pFxMaterialInfo->impThreshMin));
					hydraulicsHack = true;
				}
			}
		}

#if __BANK
		if (m_bangVfxAlwaysPassVelocityThreshold)
		{
			bangMag = pFxMaterialInfo->velThreshMax;
		}

		if (m_bangVfxAlwaysPassImpulseThreshold)
		{
			accumImpulse = pFxMaterialInfo->impThreshMax;
		}
#endif

		if (bangMag>=pFxMaterialInfo->velThreshMin && accumImpulse>=pFxMaterialInfo->impThreshMin)
		{	
#if __BANK
			// bang textures
			if (m_bangDecalsDisabled==false)
#endif
			{
				// only place textures on materials that aren't shoot thru (unless it's glass)
				if (PGTAMATERIALMGR->GetIsShootThrough(mtlIdA)==false || PGTAMATERIALMGR->GetIsGlass(mtlIdA))
				{
					// check if underwater
					if (pFxMaterialInfo->ptFxSkipUnderwater)
					{
						float waterDepth;
						if (CVfxHelper::GetWaterDepth(vCollPos, waterDepth, pEntity))
						{
							if (waterDepth>0.0f)
							{
								return;
							}
						}
					}

					// calc distance between min and max threshold
					float d = (accumImpulse-pFxMaterialInfo->impThreshMin)/(pFxMaterialInfo->impThreshMax-pFxMaterialInfo->impThreshMin);
					d = Min(d, 1.0f);

					VfxCollInfo_s vfxCollInfo;
					vfxCollInfo.regdEnt			= pEntity;
					vfxCollInfo.vPositionWld	= vCollPos;
					vfxCollInfo.vNormalWld		= vCollNormal;
					vfxCollInfo.vDirectionWld	= Vec3V(V_Z_AXIS_WZERO);
					vfxCollInfo.materialId		= mtlIdA;
					vfxCollInfo.roomId = -1;
					vfxCollInfo.componentId		= componentId;
					vfxCollInfo.weaponGroup		= WEAPON_EFFECT_GROUP_PUNCH_KICK;		// shouldn't matter what this is set to at this point
					vfxCollInfo.force			= d;
					vfxCollInfo.isBloody		= false;
					vfxCollInfo.isWater			= false;
					vfxCollInfo.isExitFx		= false;
					vfxCollInfo.noPtFx			= false;
					vfxCollInfo.noPedDamage		= false;
					vfxCollInfo.noDecal			= false;
					vfxCollInfo.isSnowball		= false;
					vfxCollInfo.isFMJAmmo		= false;

					// apply bang texture
					REPLAY_ONLY(int decalId =) g_decalMan.AddBang(*pFxMaterialInfo, vfxCollInfo);

#if GTA_REPLAY
					if(CReplayMgr::ShouldRecord())
					{
						bool playerVeh = false;
						if( pEntity->GetIsTypeVehicle())
						{
							CVehicle* pVeh = static_cast<CVehicle*>(pEntity);
							playerVeh = pVeh->IsDriverAPlayer();
						}

						if( playerVeh )
						{
							CReplayMgr::RecordFx<CPacketPTexMtlBang_PlayerVeh>(CPacketPTexMtlBang_PlayerVeh(vfxCollInfo, mtlIdB, pEntity, decalId), pEntity);
						}
						else
						{
							CReplayMgr::RecordFx<CPacketPTexMtlBang>(CPacketPTexMtlBang(vfxCollInfo, mtlIdB, pEntity, decalId), pEntity);
						}

					}
#endif
				}
			}

#if __BANK
			// bang effects
			if (m_bangPtFxDisabled==false)
#endif
			{
				// particle effect
				g_vfxMaterial.TriggerPtFxMtlBang(pFxMaterialInfo, pEntity, vColnMtx, bangMag, accumImpulse, lodRangeScale, vehicleEvo, zoom, hydraulicsHack);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  DoMtlScrapeFx
///////////////////////////////////////////////////////////////////////////////

void			CVfxMaterial::DoMtlScrapeFx					(CEntity* pEntityA, s32 componentIdA, CEntity* pEntityB, s32 componentIdB, Vec3V_In vCollPos, Vec3V_In vCollNormal, Vec3V_In vCollVel, phMaterialMgr::Id mtlIdA, phMaterialMgr::Id mtlIdB, Vec3V_In vCollDir, float scrapeMag, float accumImpulse, float lodRangeScale, float vehicleEvo, float zoom)
{
#if __BANK
	if (m_scrapePtFxRenderDebug)
	{
		grcDebugDraw::Sphere(vCollPos, m_scrapePtFxRenderDebugSize, Color32(1.0f, 1.0f, 0.0f, 1.0f), false, m_scrapePtFxRenderDebugTimer);

		char mtlIdAName[64] = "";
		PGTAMATERIALMGR->GetMaterialName(PGTAMATERIALMGR->UnpackMtlId(mtlIdA), mtlIdAName, sizeof(mtlIdAName));

		char mtlIdBName[64] = "";
		PGTAMATERIALMGR->GetMaterialName(PGTAMATERIALMGR->UnpackMtlId(mtlIdB), mtlIdBName, sizeof(mtlIdBName));

		char displayTxt[1024] = "";
		char tempTxt[1024];

		formatf(tempTxt, 256, "%s v %s - vel:%.3f - imp:%.3f\n", mtlIdAName, mtlIdBName, scrapeMag, accumImpulse);
		formatf(displayTxt, 256, "%s", tempTxt);

		grcDebugDraw::Text(vCollPos, Color32(1.0f, 0.0f, 1.0f, 1.0f), 0, 0, displayTxt, false, m_scrapePtFxRenderDebugTimer);
	}
#endif

	if (pEntityA==NULL)
	{
		return;
	}

	if (pEntityA->IsArchetypeSet() && pEntityA->GetBoundRadius()<0.2f)
	{
		return;
	}

	if (pEntityA->GetIsTypeVehicle() && PGTAMATERIALMGR->UnpackMtlId(mtlIdA)==PGTAMATERIALMGR->g_idCarVoid)
	{
		mtlIdA = PGTAMATERIALMGR->g_idCarMetal;
	}

	VfxMaterialInfo_s* pFxMaterialInfo = g_vfxMaterial.GetScrapeInfo(PGTAMATERIALMGR->GetMtlVfxGroup(mtlIdA), PGTAMATERIALMGR->GetMtlVfxGroup(mtlIdB));
	if (pFxMaterialInfo)
	{
		// register the point with the trail effects
#if __BANK
		if (m_scrapeDecalsDisabled==false && m_scrapePtFxDisabled==false)
#endif
		{
#if __BANK
			if (m_scrapeVfxAlwaysPassVelocityThreshold)
			{
				scrapeMag = pFxMaterialInfo->velThreshMax;
			}

			if (m_scrapeVfxAlwaysPassImpulseThreshold)
			{
				accumImpulse = pFxMaterialInfo->impThreshMax;
			}
#endif

			if (scrapeMag>=pFxMaterialInfo->velThreshMin && accumImpulse>=pFxMaterialInfo->impThreshMin)
			{
				// check if underwater
				if (pFxMaterialInfo->ptFxSkipUnderwater)
				{
					float waterDepth;
					if (CVfxHelper::GetWaterDepth(vCollPos, waterDepth, pEntityA))
					{
						if (waterDepth>0.0f)
						{
							return;
						}
					}
				}

				float d = (accumImpulse-pFxMaterialInfo->impThreshMin)/(pFxMaterialInfo->impThreshMax-pFxMaterialInfo->impThreshMin);
				d = Min(d, 1.0f);

				VfxTrailInfo_t vfxTrailInfo;
//#if __BANK
//				if (m_scrapeTrailsUseId)
//				{
//					vfxTrailInfo.id						= fwIdKeyGenerator::Get(pEntityA, componentIdA);
//
//				}
//				else
//#endif
//				{
//					vfxTrailInfo.id						= 0;
//				}

				size_t hash[2];
				hash[0] = fwIdKeyGenerator::Get(pEntityA, componentIdA);
				hash[1] = fwIdKeyGenerator::Get(pEntityB, componentIdB);
				u32 scrapeId = atDataHash((const char*)&(hash[0]), sizeof(hash));

				vfxTrailInfo.id							= scrapeId;
				vfxTrailInfo.type						= VFXTRAIL_TYPE_SCRAPE;
				vfxTrailInfo.regdEnt					= pEntityA;
				vfxTrailInfo.componentId				= componentIdA;
				vfxTrailInfo.vWorldPos					= vCollPos;
				vfxTrailInfo.vCollNormal   				= vCollNormal;
				vfxTrailInfo.vCollVel					= vCollVel;
				vfxTrailInfo.vDir						= vCollDir;
				vfxTrailInfo.vForwardCheck				= Vec3V(V_ZERO);
				vfxTrailInfo.decalRenderSettingIndex	= pFxMaterialInfo->decalRenderSettingIndex;
				vfxTrailInfo.decalRenderSettingCount	= pFxMaterialInfo->decalRenderSettingCount;
				vfxTrailInfo.decalLife					= -1.0f;
				vfxTrailInfo.width						= pFxMaterialInfo->texWidthMin + d*(pFxMaterialInfo->texWidthMax-pFxMaterialInfo->texWidthMin);
				vfxTrailInfo.col						= Color32(pFxMaterialInfo->decalColR, pFxMaterialInfo->decalColG, pFxMaterialInfo->decalColB, 255);
				vfxTrailInfo.pVfxMaterialInfo			= pFxMaterialInfo;
				vfxTrailInfo.scrapeMag					= scrapeMag;
				vfxTrailInfo.accumImpulse				= accumImpulse;
				vfxTrailInfo.vNormal					= vCollNormal;
				vfxTrailInfo.mtlId			 			= (u8)PGTAMATERIALMGR->UnpackMtlId(mtlIdA);
				vfxTrailInfo.liquidPoolStartSize		= 0.0f;
				vfxTrailInfo.liquidPoolEndSize			= 0.0f;
				vfxTrailInfo.liquidPoolGrowthRate		= 0.0f;
				vfxTrailInfo.createLiquidPools			= false;
				vfxTrailInfo.dontApplyDecal				= PGTAMATERIALMGR->GetIsShootThrough(mtlIdA) && !PGTAMATERIALMGR->GetIsGlass(mtlIdA);
				vfxTrailInfo.lodRangeScale				= lodRangeScale;
				vfxTrailInfo.vehicleEvo					= vehicleEvo;
				vfxTrailInfo.zoom						= zoom;

				g_vfxTrail.RegisterPoint(vfxTrailInfo);

			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxMtlBang
///////////////////////////////////////////////////////////////////////////////

void			CVfxMaterial::TriggerPtFxMtlBang			(const VfxMaterialInfo_s* pFxMaterialInfo, CEntity* pEntity, Mat34V_In mat, float bangMag, float accumImpulse, float lodRangeScale, float vehicleEvo, float zoom, bool hydraulicsHack)
{
#if __BANK
	if (m_bangPtFxDisabled==false)
#endif
	{
		// play any required effect
		if (pFxMaterialInfo->ptFxHashName)
		{
			// check for nearby effects of this type - don't play this one if another is being played too close by
			Vec3V vPos = mat.GetCol3();
			if (ptfxHistory::Check(pFxMaterialInfo->ptFxHashName, 0, pEntity, vPos, VFXMATERIAL_BANG_HIST_DIST))
			{
				return;
			}

			ptxEffectInst* pFxInst = NULL;
			if (hydraulicsHack)
			{
				pFxInst = ptfxManager::GetTriggeredInst(ATSTRINGHASH("bang_hydraulics", 0xcdb3c6d0));
			}
			else
			{
				pFxInst = ptfxManager::GetTriggeredInst(pFxMaterialInfo->ptFxHashName);
			}

			if (pFxInst)
			{
				// set the position of the effect
				if (pEntity->GetIsDynamic())
				{
					// make the fx matrix relative to the entity
					Mat34V relFxMat;
					CVfxHelper::CreateRelativeMat(relFxMat, pEntity->GetMatrix(), mat);

					ptfxAttach::Attach(pFxInst, pEntity, -1);

					pFxInst->SetOffsetMtx(relFxMat);
				}
				else
				{
					pFxInst->SetBaseMtx(mat);
				}

				float velocityEvo = CVfxHelper::GetInterpValue(bangMag, pFxMaterialInfo->velThreshMin, pFxMaterialInfo->velThreshMax);
				pFxInst->SetEvoValueFromHash(ATSTRINGHASH("velocity", 0x8FF7E5A7), velocityEvo);

				float impulseEvo = CVfxHelper::GetInterpValue(accumImpulse, pFxMaterialInfo->impThreshMin, pFxMaterialInfo->impThreshMax);
				pFxInst->SetEvoValueFromHash(ATSTRINGHASH("impulse", 0xB05D6C92), impulseEvo);

				pFxInst->SetEvoValueFromHash(ATSTRINGHASH("vehicle", 0xDD245B9C), vehicleEvo);

				// if this is a vehicle then override the colour of the effect with that of the vehicle
				if (pEntity->GetIsTypeVehicle())
				{
					CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
					CRGBA rgba = CVehicleModelInfo::GetVehicleColours()->GetVehicleColour(pVehicle->GetBodyColour1());
					ptfxWrapper::SetColourTint(pFxInst, rgba.GetRGBA().GetXYZ());
				}

				pFxInst->SetLODScalar(m_bangScrapePtFxLodRangeScale*lodRangeScale);

				pFxInst->SetUserZoom(zoom);

				// if the z component is pointing down then flip it
				if (mat.GetCol2().GetZf()<0.0f)
				{
					pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_Z);
				}

				if (pEntity && pEntity->GetIsPhysical())
				{
					CPhysical* pPhysical = static_cast<CPhysical*>(pEntity);
					ptfxWrapper::SetVelocityAdd(pFxInst, RCC_VEC3V(pPhysical->GetVelocity()));
				}

				pFxInst->Start();

#if GTA_REPLAY
				if(CReplayMgr::ShouldRecord())
				{
					CReplayMgr::RecordFx<CPacketMaterialBangFx>(
						CPacketMaterialBangFx(pFxMaterialInfo->ptFxHashName,
							RCC_MATRIX34(mat),
							velocityEvo,
							impulseEvo,
							m_bangScrapePtFxLodRangeScale),
						pEntity);
				}
#endif

#if __BANK
				if (m_bangPtFxOutputDebug)
				{
					vfxDebugf1("********************************************\n");
					vfxDebugf1("BangVelMag %.2f (EvoVal %.2f)\n", bangMag, velocityEvo);
					vfxDebugf1("BangImpMag %.2f (EvoVal %.2f)\n", accumImpulse, impulseEvo);
					vfxDebugf1("********************************************\n");
				}
#endif

#if __BANK
				if (m_bangPtFxRenderDebug)
				{
					float colR = velocityEvo;
					float colG = impulseEvo;
					float colB = 0.0f;
					float colA = 1.0f;
					Color32 col = Color32(colR, colG, colB, colA);
					grcDebugDraw::Sphere(mat.GetCol3(), m_bangPtFxRenderDebugSize, col, true, m_bangPtFxRenderDebugTimer);
				}
#endif

				// store the bang data in the history
				ptfxHistory::Add(pFxMaterialInfo->ptFxHashName, 0, pEntity, vPos, VFXMATERIAL_BANG_HIST_TIME);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetBangScrapePtFxLodRangeScale
///////////////////////////////////////////////////////////////////////////////


void			CVfxMaterial::SetBangScrapePtFxLodRangeScale	(float val, scrThreadId scriptThreadId) 
{
	if (vfxVerifyf(m_bangScrapePtFxLodRangeScaleScriptThreadId==THREAD_INVALID || m_bangScrapePtFxLodRangeScaleScriptThreadId==scriptThreadId, "trying to set bullet impact user zoom when this is already in use by another script")) 
	{
		m_bangScrapePtFxLodRangeScale = val; 
		m_bangScrapePtFxLodRangeScaleScriptThreadId = scriptThreadId;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void		 	CVfxMaterial::InitWidgets					()
{	
	ms_debugScrapeRecording = false;
	ms_debugScrapeApplying = false;
	ms_debugScrapeRecordCount = 0; 
	ms_debugScrapeApplyCount = 0;

	if (m_bankInitialised==false)
	{
		char txt[128];
		u32 materialInfoSize = sizeof(VfxMaterialInfo_s);
		float materialInfoPoolSize = (materialInfoSize * VFXMATERIAL_MAX_INFOS) / 1024.0f;

		bkBank* pVfxBank = vfxWidget::GetBank();
		pVfxBank->PushGroup("Vfx Material", false);
		{
			pVfxBank->AddTitle("INFO");
			sprintf(txt, "Num Bang Infos (%d - %.2fK)", VFXMATERIAL_MAX_INFOS, materialInfoPoolSize);
			pVfxBank->AddSlider(txt, &m_numVfxMtlBangInfos, 0, VFXMATERIAL_MAX_INFOS, 0);
			sprintf(txt, "Num Scrape Infos (%d - %.2fK)", VFXMATERIAL_MAX_INFOS, materialInfoPoolSize);
			pVfxBank->AddSlider(txt, &m_numVfxMtlScrapeInfos, 0, VFXMATERIAL_MAX_INFOS, 0);

#if __DEV
			pVfxBank->AddTitle("");
			pVfxBank->AddTitle("SETTINGS");
			pVfxBank->AddSlider("Bang Angle Ratio Bias", &VFXMATERIAL_ANGLE_RATIO_BIAS, -1.0f, 1.0f, 0.01f);
			pVfxBank->AddSlider("Bang History Dist", &VFXMATERIAL_BANG_HIST_DIST, 0.0f, 10.0f, 0.25f, NullCB, "The distance at which a new bang effect will be rejected if a history exists within this range");
			pVfxBank->AddSlider("Bang History Time", &VFXMATERIAL_BANG_HIST_TIME, 0.0f, 10.0f, 0.25f, NullCB, "The time that a history will exist for a new bang effect");
			pVfxBank->AddSlider("Heli Rotor Spawn Dist", &VFXMATERIAL_ROTOR_SPAWN_DIST, 0.0f, 5.0f, 0.05f, NullCB, "The spawn distance that is fed into the material effect for heli rotor blades");
			pVfxBank->AddSlider("Heli Rotor Size Scale", &VFXMATERIAL_ROTOR_SIZE_SCALE, 0.0f, 10.0f, 0.25f, NullCB, "The scale that is applied to the material effect for heli rotor blades");
			pVfxBank->AddSlider("Wheel Coln Dot Thresh", &VFXMATERIAL_WHEEL_COLN_DOT_THRESH, 0.0f, 10.0f, 0.25f, NullCB, "The dot product threshold at which wheel collisions are rejected (vehicle up with coln normal)");
			pVfxBank->AddSlider("Bang/Scrape Lod/Range Scale (Vehicle)", &VFXMATERIAL_LOD_RANGE_SCALE_VEHICLE, 0.0f, 20.0f, 0.5f);
			pVfxBank->AddSlider("Bang/Scrape Lod/Range Scale (Heli)", &VFXMATERIAL_LOD_RANGE_SCALE_HELI, 0.0f, 20.0f, 0.5f);
#endif

			pVfxBank->AddTitle("");
			pVfxBank->AddTitle("DEBUG");

			pVfxBank->AddSlider("Bang/Scrape Lod/Range Scale (Global)", &m_bangScrapePtFxLodRangeScale, 0.0f, 20.0f, 0.5f);
			
			pVfxBank->AddToggle("Coln Vfx - Process Per Contact", &m_colnVfxProcessPerContact);
			pVfxBank->AddToggle("Coln Vfx - Disable Time Slice 1", &m_colnVfxDisableTimeSlice1);
			pVfxBank->AddToggle("Coln Vfx - Disable Time Slice 2", &m_colnVfxDisableTimeSlice2);
			pVfxBank->AddToggle("Coln Vfx - Disable Wheels", &m_colnVfxDisableWheels);
			pVfxBank->AddToggle("Coln Vfx - Disable Entity A", &m_colnVfxDisableEntityA);
			pVfxBank->AddToggle("Coln Vfx - Disable Entity B", &m_colnVfxDisableEntityB);
			pVfxBank->AddToggle("Coln Vfx - Recompute Positions", &m_colnVfxRecomputePositions);
			pVfxBank->AddToggle("Coln Vfx - Output Debug", &m_colnVfxOutputDebug);
			pVfxBank->AddToggle("Coln Vfx - Render Debug Vectors", &m_colnVfxRenderDebugVectors);
			pVfxBank->AddToggle("Coln Vfx - Render Debug Impulses", &m_colnVfxRenderDebugImpulses);

			pVfxBank->AddToggle("Bang Vfx - Always Pass Velocity Threshold", &m_bangVfxAlwaysPassVelocityThreshold);
			pVfxBank->AddToggle("Bang Vfx - Always Pass Impulse Threshold", &m_bangVfxAlwaysPassImpulseThreshold);
			pVfxBank->AddToggle("Bang Decals - Disable", &m_bangDecalsDisabled);
			pVfxBank->AddToggle("Bang PtFx - Disable", &m_bangPtFxDisabled);
			pVfxBank->AddToggle("Bang PtFx - Output Debug", &m_bangPtFxOutputDebug);
			pVfxBank->AddToggle("Bang PtFx - Render Debug", &m_bangPtFxRenderDebug);
			pVfxBank->AddSlider("Bang PtFx - Render Debug Size", &m_bangPtFxRenderDebugSize, 0.0f, 10.0f, 0.05f);
			pVfxBank->AddSlider("Bang PtFx - Render Debug Timer", &m_bangPtFxRenderDebugTimer, -1000, 1000, 1);
			
			pVfxBank->AddToggle("Scrape Vfx - Always Pass Velocity Threshold", &m_scrapeVfxAlwaysPassVelocityThreshold);
			pVfxBank->AddToggle("Scrape Vfx - Always Pass Impulse Threshold", &m_scrapeVfxAlwaysPassImpulseThreshold);
			pVfxBank->AddToggle("Scrape Decals - Disable All", &m_scrapeDecalsDisabled);
			pVfxBank->AddToggle("Scrape Decals - Disable Single", &m_scrapeDecalsDisableSingle);
			//pVfxBank->AddToggle("Scrape Trails - Use Id", &m_scrapeTrailsUseId);
			pVfxBank->AddToggle("Scrape PtFx - DisableFx", &m_scrapePtFxDisabled);
			pVfxBank->AddToggle("Scrape PtFx - Output Debug", &m_scrapePtFxOutputDebug);
			pVfxBank->AddToggle("Scrape PtFx - Render Debug", &m_scrapePtFxRenderDebug);
			pVfxBank->AddSlider("Scrape PtFx - Render Debug Size", &m_scrapePtFxRenderDebugSize, 0.0f, 10.0f, 0.05f);
			pVfxBank->AddSlider("Scrape PtFx - Render Debug Timer", &m_scrapePtFxRenderDebugTimer, -1000, 1000, 1);

			//pVfxBank->AddToggle("Turn on Measuring Tool", &CPhysics::ms_bDebugMeasuringTool);
			pVfxBank->AddButton("Start Debug Scrape Recording", StartDebugScrapeRecording);
			pVfxBank->AddButton("Stop Debug Scrape Recording", StopDebugScrapeRecording);
			pVfxBank->AddButton("Apply Debug Scrape", ApplyDebugScrape);

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
void			CVfxMaterial::Reset							()
{
	g_vfxMaterial.Shutdown(SHUTDOWN_WITH_MAP_LOADED);
	g_vfxMaterial.Init(INIT_AFTER_MAP_LOADED);
}
#endif


///////////////////////////////////////////////////////////////////////////////
// StartDebugScrapeRecording
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CVfxMaterial::StartDebugScrapeRecording		()
{
	ms_debugScrapeRecording = true;
	ms_debugScrapeRecordCount = 0; 
}
#endif


///////////////////////////////////////////////////////////////////////////////
// StopDebugScrapeRecording
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CVfxMaterial::StopDebugScrapeRecording		()
{
	ms_debugScrapeRecording = false;
}
#endif


///////////////////////////////////////////////////////////////////////////////
// UpdateDebugScrapeRecording
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CVfxMaterial::UpdateDebugScrapeRecording	()
{
	if (ms_debugScrapeRecording)
	{
		Vector3 vPos;
		Vector3 vNormal;
		CEntity* pEntity;
		bool bHasPosition = CDebugScene::GetWorldPositionUnderMouse(vPos, ArchetypeFlags::GTA_ALL_TYPES_MOVER, &vNormal, (void**)&pEntity);
		if (bHasPosition)
		{
			if ((ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT) ||
				(ioMouse::GetButtons() & ioMouse::MOUSE_RIGHT))
			{
				if (vfxVerifyf(ms_debugScrapeRecordCount<VFXMATERIAL_MAX_DEBUG_SCRAPES, "ran out of debug scrape recording entries"))
				{
					ms_debugScrapeInfos[ms_debugScrapeRecordCount].vPosition = RCC_VEC3V(vPos);
					ms_debugScrapeInfos[ms_debugScrapeRecordCount].vNormal = RCC_VEC3V(vNormal);
					ms_debugScrapeInfos[ms_debugScrapeRecordCount].pEntity = pEntity;
					ms_debugScrapeRecordCount++;
				}
			}
		}
	}

	if (ms_debugScrapeApplying)
	{
		if (ms_debugScrapeApplyCount>=ms_debugScrapeRecordCount)
		{
			// the scrape is finished
			ms_debugScrapeApplying = false;
			ms_debugScrapeApplyCount = 0;
			ms_debugScrapeRecordCount = 0;
		}
		else
		{
			// apply this frame's scrape
			VfxMaterialInfo_s* pFxMaterialInfo = g_vfxMaterial.GetScrapeInfo(VFXGROUP_CAR_METAL, VFXGROUP_CONCRETE);
			if (pFxMaterialInfo)
			{
				Vec3V vDir;
				if (ms_debugScrapeApplyCount+1<ms_debugScrapeRecordCount)
				{
					vDir = ms_debugScrapeInfos[ms_debugScrapeApplyCount+1].vPosition - ms_debugScrapeInfos[ms_debugScrapeApplyCount].vPosition;
				}
				else
				{
					vDir = ms_debugScrapeInfos[ms_debugScrapeApplyCount].vPosition - ms_debugScrapeInfos[ms_debugScrapeApplyCount-1].vPosition;
				}

				vDir = Normalize(vDir);

				VfxTrailInfo_t vfxTrailInfo;
				vfxTrailInfo.id							= 9999;
				vfxTrailInfo.type						= VFXTRAIL_TYPE_SCRAPE;
				vfxTrailInfo.regdEnt					= ms_debugScrapeInfos[ms_debugScrapeApplyCount].pEntity;
				vfxTrailInfo.componentId				= 0;
				vfxTrailInfo.vWorldPos					= ms_debugScrapeInfos[ms_debugScrapeApplyCount].vPosition;
				vfxTrailInfo.vCollNormal   				= ms_debugScrapeInfos[ms_debugScrapeApplyCount].vNormal;
				vfxTrailInfo.vCollVel					= vDir;
				vfxTrailInfo.vDir						= vDir;
				vfxTrailInfo.vForwardCheck				= Vec3V(V_ZERO);
				vfxTrailInfo.decalRenderSettingIndex	= pFxMaterialInfo->decalRenderSettingIndex;
				vfxTrailInfo.decalRenderSettingCount	= pFxMaterialInfo->decalRenderSettingCount;
				vfxTrailInfo.decalLife					= -1.0f;
				vfxTrailInfo.width						= pFxMaterialInfo->texWidthMax;
				vfxTrailInfo.col						= Color32(pFxMaterialInfo->decalColR, pFxMaterialInfo->decalColG, pFxMaterialInfo->decalColB, 255);
				vfxTrailInfo.pVfxMaterialInfo			= pFxMaterialInfo;
				vfxTrailInfo.scrapeMag					= 100.0f;
				vfxTrailInfo.accumImpulse				= 100.0f;
				vfxTrailInfo.vNormal					= ms_debugScrapeInfos[ms_debugScrapeApplyCount].vNormal;
				vfxTrailInfo.mtlId			 			= (u8)PGTAMATERIALMGR->g_idCarMetal;
				vfxTrailInfo.liquidPoolStartSize		= 0.0f;
				vfxTrailInfo.liquidPoolEndSize			= 0.0f;
				vfxTrailInfo.liquidPoolGrowthRate		= 0.0f;
				vfxTrailInfo.createLiquidPools			= false;
				vfxTrailInfo.dontApplyDecal				= false;

				g_vfxTrail.RegisterPoint(vfxTrailInfo);
			}

			ms_debugScrapeApplyCount++;
		}
	}
	else
	{
		if (ms_debugScrapeRecordCount>0)
		{
			for (int i=0; i<ms_debugScrapeRecordCount; i++)
			{
				if (ms_debugScrapeRecording)
				{
					grcDebugDraw::Sphere(ms_debugScrapeInfos[i].vPosition, 0.1f, Color32(0.0f, 1.0f, 0.0f, 1.0f));
				}
				else
				{
					grcDebugDraw::Sphere(ms_debugScrapeInfos[i].vPosition, 0.1f, Color32(1.0f, 0.0f, 0.0f, 1.0f));
				}

				if (i>0)
				{
					grcDebugDraw::Line(ms_debugScrapeInfos[i-1].vPosition, ms_debugScrapeInfos[i].vPosition, Color32(1.0f, 1.0f, 0.0f, 1.0f), Color32(1.0f, 1.0f, 0.0f, 1.0f));
				}
			}
		}
	}

}
#endif


///////////////////////////////////////////////////////////////////////////////
// ApplyDebugScrape
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CVfxMaterial::ApplyDebugScrape				()
{
	if (vfxVerifyf(ms_debugScrapeRecording==false, "can't apply debug scrape until recording is stopped"))
	{
		ms_debugScrapeApplying = true;
		ms_debugScrapeApplyCount = 1;
	}
}
#endif











