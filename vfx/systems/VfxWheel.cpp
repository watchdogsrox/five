///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxWheel.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	07 May 2010
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxWheel.h"

// rage
#include "rmptfx/ptxeffectinst.h"
#include "system/param.h"

// framework
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "vfx/ptfx/ptfxwrapper.h"
#include "vfx/vfxwidget.h"

// game
#include "Camera/CamInterface.h"
#include "Core/Game.h"
#include "Control/Replay/Replay.h"
#include "Control/Replay/Effects/ParticleVehicleFxPacket.h"
#include "Peds/Ped.h"
#include "Scene/DataFileMgr.h"
#include "Scene/World/GameWorld.h"
#include "Vehicles/Vehicle.h"
#include "Vehicles/Wheel.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/Systems/VfxVehicle.h"
#include "Vfx/VfxHelper.h"


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
	
CVfxWheel		g_vfxWheel;

// rag tweakable settings 
dev_float		VFXWHEEL_SKID_WIDTH_MULT						= 1.4f;
dev_float		VFXWHEEL_SKID_WIDTH_MULT_BICYCLE				= 0.7f;
dev_float		VFXWHEEL_SKID_MAX_ALPHA							= 0.7f;

dev_float		VFXWHEEL_FRICTION_GROUND_VEL_MULT				= 1.0f;
dev_float		VFXWHEEL_FRICTION_VEL_MULT						= 1.0f;

dev_float		VFXWHEEL_FRICTION_SPEED_EVO_MIN					= 2.0f;
dev_float		VFXWHEEL_FRICTION_SPEED_EVO_MAX					= 20.0f;

dev_float		VFXWHEEL_FRICTION_SPIN_VEL_THRESH				= 15.0f;
dev_float		VFXWHEEL_FRICTION_SPIN_THROTTLE_THRESH			= 0.5f;
dev_float		VFXWHEEL_FRICTION_SPIN_GROUND_SPEED_EVO_MIN		= 0.001f;
dev_float		VFXWHEEL_FRICTION_SPIN_GROUND_SPEED_EVO_MAX		= 5.0f;

dev_float		VFXWHEEL_BURNOUT_SPEED_EVO_MIN					= 0.0f;
dev_float		VFXWHEEL_BURNOUT_SPEED_EVO_MAX					= 3.0f;

dev_float		VFXWHEEL_WETROAD_DRY_MAX_THRESH					= 0.8f;
dev_float		VFXWHEEL_WETROAD_WET_MIN_THRESH					= 0.6f;

dev_float		VFXWHEEL_WET_REDUCE_TEMP_MIN					= 30.0f;
dev_float		VFXWHEEL_WET_REDUCE_TEMP_MAX					= 50.0f;

dev_float		VFXWHEEL_COOL_RATE								= 9.0f;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxWheel
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWheel::Init						(unsigned initMode)
{
	if (initMode==INIT_CORE)
	{
		m_wheelPtFxLodRangeScaleScriptThreadId = THREAD_INVALID;
		m_wheelPtFxLodRangeScale = 1.0f;

		m_wheelSkidmarkRangeScaleScriptThreadId = THREAD_INVALID;
		m_wheelSkidmarkRangeScale = 1.0f;

		m_useSnowWheelVfxWhenUnsheltered = false;

#if __BANK
		m_bankInitialised = false;

		m_disableSkidmarks = false;
		m_disableWheelFricFx = false;
		m_disableWheelDispFx = false;
		m_disableWheelBurnoutFx = false;
		m_disableWheelFL = false;
		m_disableWheelFR = false;
		m_disableWheelML = false;
		m_disableWheelMR = false;
		m_disableWheelRL = false;
		m_disableWheelRR = false;

		m_skidmarkOverlapPerc = 0.0f;

		m_wetRoadOverride = 0.0f;

		m_smokeOverrideTintR = 1.0f;
		m_smokeOverrideTintG = 1.0f;
		m_smokeOverrideTintB = 1.0f;
		m_smokeOverrideTintA = 1.0f;
		m_smokeOverrideTintActive = false;

		m_outputWheelVfxValues = false;
		m_outputWheelFricDebug = false;
		m_outputWheelDispDebug = false;
		m_outputWheelBurnoutDebug = false;

		m_renderWheelBoneMatrices = false;
		m_renderWheelDrawMatrices = false;
		m_disableNoMovementOpt = false;
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

void		 	CVfxWheel::Shutdown					(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
    {
    }
}


///////////////////////////////////////////////////////////////////////////////
//  SetWheelPtFxLodRangeScale
///////////////////////////////////////////////////////////////////////////////

void			CVfxWheel::SetWheelPtFxLodRangeScale		(float val, scrThreadId scriptThreadId) 
{
	if (vfxVerifyf(m_wheelPtFxLodRangeScaleScriptThreadId==THREAD_INVALID || m_wheelPtFxLodRangeScaleScriptThreadId==scriptThreadId, "trying to set bullet impact user zoom when this is already in use by another script")) 
	{
		m_wheelPtFxLodRangeScale = val; 
		m_wheelPtFxLodRangeScaleScriptThreadId = scriptThreadId;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetWheelSkidmarkRangeScale
///////////////////////////////////////////////////////////////////////////////

void			CVfxWheel::SetWheelSkidmarkRangeScale		(float val, scrThreadId scriptThreadId) 
{
	if (vfxVerifyf(m_wheelSkidmarkRangeScaleScriptThreadId==THREAD_INVALID || m_wheelSkidmarkRangeScaleScriptThreadId==scriptThreadId, "trying to set wheel skidmark range scale when this is already in use by another script")) 
	{
		m_wheelSkidmarkRangeScale = val; 
		m_wheelSkidmarkRangeScaleScriptThreadId = scriptThreadId;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveScript
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWheel::RemoveScript							(scrThreadId scriptThreadId)
{
	if (scriptThreadId==m_wheelPtFxLodRangeScaleScriptThreadId)
	{
		m_wheelPtFxLodRangeScale = 1.0f;
		m_wheelPtFxLodRangeScaleScriptThreadId = THREAD_INVALID;
	}

	if (scriptThreadId==m_wheelSkidmarkRangeScaleScriptThreadId)
	{
		m_wheelSkidmarkRangeScale = 1.0f;
		m_wheelSkidmarkRangeScaleScriptThreadId = THREAD_INVALID;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  LoadData
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWheel::LoadData					()
{
	// initialise the data
	for (s32 i=0; i<VFXTYRESTATE_NUM; i++)
	{
		for (s32 j=0; j<NUM_VFX_GROUPS; j++)
		{
			m_vfxWheelInfos[i][j].ptFxWheelFricThreshMin = FLT_MAX;
			m_vfxWheelInfos[i][j].ptFxWheelFricThreshMax = FLT_MAX;
			m_vfxWheelInfos[i][j].ptFxWheelFric1HashName = (u32)0;
			m_vfxWheelInfos[i][j].ptFxWheelFric2HashName = (u32)0;
			m_vfxWheelInfos[i][j].ptFxWheelFric3HashName = (u32)0;

			m_vfxWheelInfos[i][j].ptFxWheelDispThreshMin = FLT_MAX;
			m_vfxWheelInfos[i][j].ptFxWheelDispThreshMax = FLT_MAX;
			m_vfxWheelInfos[i][j].ptFxWheelDisp1HashName = (u32)0;
			m_vfxWheelInfos[i][j].ptFxWheelDisp2HashName = (u32)0;
			m_vfxWheelInfos[i][j].ptFxWheelDisp3HashName = (u32)0;
			m_vfxWheelInfos[i][j].ptFxWheelDispLodHashName = (u32)0;

			m_vfxWheelInfos[i][j].ptFxWheelBurnout1HashName = (u32)0;
			m_vfxWheelInfos[i][j].ptFxWheelBurnout2HashName = (u32)0;
			m_vfxWheelInfos[i][j].ptFxWheelBurnout3HashName = (u32)0;
		}
	}

	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::WHEELFX_FILE);
	while (DATAFILEMGR.IsValid(pData))
	{
		// read in the data
		fiStream* stream = ASSET.Open(pData->m_filename, "dat", true);
		vfxAssertf(stream, "Cannot open data file");

		if (stream)
		{
#if CHECK_VFXGROUP_DATA
			bool vfxGroupVerify[VFXTYRESTATE_NUM][NUM_VFX_GROUPS];

			for (s32 i=0; i<VFXTYRESTATE_NUM; i++)
			{
				for (s32 j=0; j<NUM_VFX_GROUPS; j++)
				{
					vfxGroupVerify[i][j] = false;
				}
			}
#endif
			// initialise the tokens
			fiAsciiTokenizer token;
			token.Init("vehicleFx", stream);

			// read in the data
			char charBuff[128];
			s32 phase = -1;	
			VfxTyreState_e currTyreState = VFXTYRESTATE_OK_DRY;

			// read in the version
			m_version = token.GetFloat();
			// m_version = m_version;

			if (m_version>=3.0f)
			{
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
					else if (stricmp(charBuff, "VEHFX_INFO_START")==0)
					{
						phase = 0;
						continue;
					}
					else if (stricmp(charBuff, "VEHFX_INFO_END")==0)
					{
						phase = -1;
						continue;
					}
					else if (stricmp(charBuff, "VEHFX_INFO_BURST_START")==0)
					{
						currTyreState = VFXTYRESTATE_BURST_DRY;
						phase = 0;
						continue;
					}
					else if (stricmp(charBuff, "VEHFX_INFO_BURST_END")==0)
					{
						phase = -1;
						continue;
					}
					else if (stricmp(charBuff, "VEHFX_INFO_WET_START")==0)
					{
						currTyreState = VFXTYRESTATE_OK_WET;
						phase = 0;
						continue;
					}
					else if (stricmp(charBuff, "VEHFX_INFO_WET_END")==0)
					{
						phase = -1;
						continue;
					}
					else if (stricmp(charBuff, "VEHFX_INFO_BURST_WET_START")==0)
					{
						currTyreState = VFXTYRESTATE_BURST_WET;
						phase = 0;
						continue;
					}
					else if (stricmp(charBuff, "VEHFX_INFO_BURST_WET_END")==0)
					{
						break;
					}

					if (phase==0)
					{
						VfxGroup_e vfxGroup = phMaterialGta::FindVfxGroup(charBuff);
						if (vfxGroup==VFXGROUP_UNDEFINED)
						{
#if CHECK_VFXGROUP_DATA
							vfxAssertf(0, "Invalid vfx group found in vehiclefx.dat %s", charBuff);
#endif
							token.SkipToEndOfLine();
						}	
						else
						{
#if CHECK_VFXGROUP_DATA
							vfxAssertf(vfxGroupVerify[currTyreState][vfxGroup]==false, "Duplicate vfxgroup data found in vehiclefx.dat for %s (state=%d)", charBuff, currTyreState);
							vfxGroupVerify[currTyreState][vfxGroup]=true;
#endif

							// decal range
							if (m_version>=4.0f && m_version<=12.0f)
							{
								token.GetFloat();
							}

							// skidmark slip thresholds
							m_vfxWheelInfos[currTyreState][vfxGroup].skidmarkSlipThreshMin = token.GetFloat();
							m_vfxWheelInfos[currTyreState][vfxGroup].skidmarkSlipThreshMax = token.GetFloat();

							if (m_vfxWheelInfos[currTyreState][vfxGroup].skidmarkSlipThreshMin<0.0f)
							{
								m_vfxWheelInfos[currTyreState][vfxGroup].skidmarkSlipThreshMin = FLT_MAX;
								m_vfxWheelInfos[currTyreState][vfxGroup].skidmarkSlipThreshMax = FLT_MAX;
							}

							vfxAssertf(m_vfxWheelInfos[currTyreState][vfxGroup].skidmarkSlipThreshMin <= 
								m_vfxWheelInfos[currTyreState][vfxGroup].skidmarkSlipThreshMax, "skidmark slip min threshold is greater than max");

							// skidmark pressure thresholds
							if (m_version>=6.0f)
							{
								m_vfxWheelInfos[currTyreState][vfxGroup].skidmarkPressureThreshMin = token.GetFloat();
								m_vfxWheelInfos[currTyreState][vfxGroup].skidmarkPressureThreshMax = token.GetFloat();

								if (m_vfxWheelInfos[currTyreState][vfxGroup].skidmarkPressureThreshMin<0.0f)
								{
									m_vfxWheelInfos[currTyreState][vfxGroup].skidmarkPressureThreshMin = FLT_MAX;
									m_vfxWheelInfos[currTyreState][vfxGroup].skidmarkPressureThreshMax = FLT_MAX;
								}

								vfxAssertf(m_vfxWheelInfos[currTyreState][vfxGroup].skidmarkPressureThreshMin <= 
									m_vfxWheelInfos[currTyreState][vfxGroup].skidmarkPressureThreshMax, "skidmark pressure min threshold is greater than max");
							}
							else
							{
								m_vfxWheelInfos[currTyreState][vfxGroup].skidmarkPressureThreshMin = FLT_MAX;
								m_vfxWheelInfos[currTyreState][vfxGroup].skidmarkPressureThreshMax = FLT_MAX;
							}

							// decal data
							s32 decalDataId = token.GetInt();
							if (decalDataId>0)
							{
								g_decalMan.FindRenderSettingInfo(decalDataId, m_vfxWheelInfos[currTyreState][vfxGroup].decal1RenderSettingIndex, m_vfxWheelInfos[currTyreState][vfxGroup].decal1RenderSettingCount);

								// hack for formula vehicle
								if (currTyreState == VFXTYRESTATE_OK_DRY || currTyreState == VFXTYRESTATE_OK_WET)
								{
									g_decalMan.FindRenderSettingInfo(decalDataId+10000, m_vfxWheelInfos[currTyreState][vfxGroup].decalSlickRenderSettingIndex, m_vfxWheelInfos[currTyreState][vfxGroup].decalSlickRenderSettingCount);
								}
								else
								{
									m_vfxWheelInfos[currTyreState][vfxGroup].decalSlickRenderSettingIndex = m_vfxWheelInfos[currTyreState][vfxGroup].decal1RenderSettingIndex;
									m_vfxWheelInfos[currTyreState][vfxGroup].decalSlickRenderSettingCount = m_vfxWheelInfos[currTyreState][vfxGroup].decal1RenderSettingCount;
								}
							}
							else
							{
								m_vfxWheelInfos[currTyreState][vfxGroup].decal1RenderSettingIndex = -1;
								m_vfxWheelInfos[currTyreState][vfxGroup].decal1RenderSettingCount = 0;

								m_vfxWheelInfos[currTyreState][vfxGroup].decalSlickRenderSettingIndex = -1;
								m_vfxWheelInfos[currTyreState][vfxGroup].decalSlickRenderSettingCount = 0;
							}

							if (m_version>=10.0f)
							{
								s32 decalDataId = token.GetInt();
								if (decalDataId>0)
								{
									g_decalMan.FindRenderSettingInfo(decalDataId, m_vfxWheelInfos[currTyreState][vfxGroup].decal2RenderSettingIndex, m_vfxWheelInfos[currTyreState][vfxGroup].decal2RenderSettingCount);
								}
								else
								{
									m_vfxWheelInfos[currTyreState][vfxGroup].decal2RenderSettingIndex = -1;
									m_vfxWheelInfos[currTyreState][vfxGroup].decal2RenderSettingCount = 0;
								}
							}
							else
							{
								m_vfxWheelInfos[currTyreState][vfxGroup].decal2RenderSettingIndex = m_vfxWheelInfos[currTyreState][vfxGroup].decal1RenderSettingIndex;
								m_vfxWheelInfos[currTyreState][vfxGroup].decal2RenderSettingCount = m_vfxWheelInfos[currTyreState][vfxGroup].decal1RenderSettingCount;
							}

							if (m_version>=12.0f)
							{
								s32 decalDataId = token.GetInt();
								if (decalDataId>0)
								{
									g_decalMan.FindRenderSettingInfo(decalDataId, m_vfxWheelInfos[currTyreState][vfxGroup].decal3RenderSettingIndex, m_vfxWheelInfos[currTyreState][vfxGroup].decal3RenderSettingCount);
								}
								else
								{
									m_vfxWheelInfos[currTyreState][vfxGroup].decal3RenderSettingIndex = -1;
									m_vfxWheelInfos[currTyreState][vfxGroup].decal3RenderSettingCount = 0;
								}
							}
							else
							{
								m_vfxWheelInfos[currTyreState][vfxGroup].decal3RenderSettingIndex = m_vfxWheelInfos[currTyreState][vfxGroup].decal1RenderSettingIndex;
								m_vfxWheelInfos[currTyreState][vfxGroup].decal3RenderSettingCount = m_vfxWheelInfos[currTyreState][vfxGroup].decal1RenderSettingCount;
							}

							if (m_version>=7.0f)
							{
								m_vfxWheelInfos[currTyreState][vfxGroup].decalColR = (u8)token.GetInt();
								m_vfxWheelInfos[currTyreState][vfxGroup].decalColG = (u8)token.GetInt();
								m_vfxWheelInfos[currTyreState][vfxGroup].decalColB = (u8)token.GetInt();
							}
							else
							{
								m_vfxWheelInfos[currTyreState][vfxGroup].decalColR = 255;
								m_vfxWheelInfos[currTyreState][vfxGroup].decalColG = 255;
								m_vfxWheelInfos[currTyreState][vfxGroup].decalColB = 255;
							}

							// wheel friction 
							if (m_version>=11.0f && m_version<=12.0f)
							{
								token.GetFloat();
							}

							m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelFricThreshMin = token.GetFloat();
							m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelFricThreshMax = token.GetFloat();

							if (m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelFricThreshMin<0.0f)
							{
								m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelFricThreshMin = FLT_MAX;
								m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelFricThreshMax = FLT_MAX;
							}

							vfxAssertf(m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelFricThreshMin <= 
								m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelFricThreshMax, "Wheel friction min threshold is greater than max");

							// wheel friction fx system
							token.GetToken(charBuff, 128);		
							if (stricmp(charBuff, "-")==0)
							{	
								m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelFric1HashName = (u32)0;					
							}
							else
							{
								m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelFric1HashName = atHashWithStringNotFinal(charBuff);
							}

							if (m_version>=8.0f)
							{
								token.GetToken(charBuff, 128);		
								if (stricmp(charBuff, "-")==0)
								{	
									m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelFric2HashName = (u32)0;					
								}
								else
								{
									m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelFric2HashName = atHashWithStringNotFinal(charBuff);
								}
							}

							if (m_version>=12.0f)
							{
								token.GetToken(charBuff, 128);		
								if (stricmp(charBuff, "-")==0)
								{	
									m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelFric3HashName = (u32)0;					
								}
								else
								{
									m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelFric3HashName = atHashWithStringNotFinal(charBuff);
								}
							}

							// wheel displacement
							if (m_version>=11.0f && m_version<=12.0f)
							{
								token.GetFloat();
								token.GetFloat();
							}

							m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelDispThreshMin = token.GetFloat();
							m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelDispThreshMax = token.GetFloat();

							if (m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelDispThreshMin<0.0f)
							{
								m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelDispThreshMin = FLT_MAX;
								m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelDispThreshMax = FLT_MAX;
							}

							vfxAssertf(m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelDispThreshMin <=
								m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelDispThreshMax, "Wheel displacement min threshold is greater than max");

							// displacement fx system
							token.GetToken(charBuff, 128);		
							if (stricmp(charBuff, "-")==0)
							{	
								m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelDisp1HashName = (u32)0;					
							}
							else
							{
								m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelDisp1HashName = atHashWithStringNotFinal(charBuff);
							}

							if (m_version>=8.0f)
							{
								token.GetToken(charBuff, 128);		
								if (stricmp(charBuff, "-")==0)
								{	
									m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelDisp2HashName = (u32)0;					
								}
								else
								{
									m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelDisp2HashName = atHashWithStringNotFinal(charBuff);
								}
							}

							if (m_version>=12.0f)
							{
								token.GetToken(charBuff, 128);		
								if (stricmp(charBuff, "-")==0)
								{	
									m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelDisp3HashName = (u32)0;					
								}
								else
								{
									m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelDisp3HashName = atHashWithStringNotFinal(charBuff);
								}
							}

							if (m_version>=11.0f)
							{
								token.GetToken(charBuff, 128);		
								if (stricmp(charBuff, "-")==0)
								{	
									m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelDispLodHashName = (u32)0;					
								}
								else
								{
									m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelDispLodHashName = atHashWithStringNotFinal(charBuff);
								}
							}

							// wheel burnout
							if (m_version>=11.0f && m_version<=12.0f)
							{
								token.GetFloat();
							}

							m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelBurnoutFricMin = token.GetFloat();
							m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelBurnoutFricMax = token.GetFloat();
					
							if (m_version>=9.0f)
							{
								m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelBurnoutTempMin = token.GetFloat();
								m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelBurnoutTempMax = token.GetFloat();
							}
							else
							{
								m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelBurnoutTempMin = 12.0f;
								m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelBurnoutTempMax = 50.0f;
							}

							// burnout fx system
							token.GetToken(charBuff, 128);		
							if (stricmp(charBuff, "-")==0)
							{	
								m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelBurnout1HashName = (u32)0;					
							}
							else
							{
								m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelBurnout1HashName = atHashWithStringNotFinal(charBuff);
							}

							if (m_version>=8.0f)
							{
								token.GetToken(charBuff, 128);		
								if (stricmp(charBuff, "-")==0)
								{	
									m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelBurnout2HashName = (u32)0;					
								}
								else
								{
									m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelBurnout2HashName = atHashWithStringNotFinal(charBuff);
								}
							}

							if (m_version>=12.0f)
							{
								token.GetToken(charBuff, 128);		
								if (stricmp(charBuff, "-")==0)
								{	
									m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelBurnout3HashName = (u32)0;					
								}
								else
								{
									m_vfxWheelInfos[currTyreState][vfxGroup].ptFxWheelBurnout3HashName = atHashWithStringNotFinal(charBuff);
								}
							}

							// lights
							if (m_version>=14.0f)
							{
								int lightEnabled = token.GetInt();

								m_vfxWheelInfos[currTyreState][vfxGroup].lightEnabled = lightEnabled>0;
								m_vfxWheelInfos[currTyreState][vfxGroup].lightColRMin = (u8)token.GetInt();
								m_vfxWheelInfos[currTyreState][vfxGroup].lightColGMin = (u8)token.GetInt();
								m_vfxWheelInfos[currTyreState][vfxGroup].lightColBMin = (u8)token.GetInt();
								m_vfxWheelInfos[currTyreState][vfxGroup].lightColRMax = (u8)token.GetInt();
								m_vfxWheelInfos[currTyreState][vfxGroup].lightColGMax = (u8)token.GetInt();
								m_vfxWheelInfos[currTyreState][vfxGroup].lightColBMax = (u8)token.GetInt();
								m_vfxWheelInfos[currTyreState][vfxGroup].lightIntensityMin = token.GetFloat();
								m_vfxWheelInfos[currTyreState][vfxGroup].lightIntensityMax = token.GetFloat();
								m_vfxWheelInfos[currTyreState][vfxGroup].lightRangeMin = token.GetFloat();
								m_vfxWheelInfos[currTyreState][vfxGroup].lightRangeMax = token.GetFloat();
								m_vfxWheelInfos[currTyreState][vfxGroup].lightFalloffMin = token.GetFloat();
								m_vfxWheelInfos[currTyreState][vfxGroup].lightFalloffMax = token.GetFloat();
							}
						}
					}
				}

#if CHECK_VFXGROUP_DATA
				for (s32 i=0; i<NUM_VFX_GROUPS; i++)
				{
					vfxAssertf(vfxGroupVerify[currTyreState][i]==true, "Missing vfxgroup data found in vehiclefx.dat for %s (state=%d)", g_fxGroupsList[i], currTyreState);
				}
#endif
			}

			stream->Close();
		}

		// get next file
		pData = DATAFILEMGR.GetPrevFile(pData);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CVfxWheel::RenderDebugWheelMatrices			(CWheel* pWheel, CVehicle* pVehicle)
{
	if (m_renderWheelBoneMatrices)
	{
		// render the wheel bone matrix
		Mat34V wheelBoneMtx;
		s32 wheelBoneIndex = pVehicle->GetBoneIndex(pWheel->m_WheelId);
		CVfxHelper::GetMatrixFromBoneIndex(wheelBoneMtx, pVehicle, wheelBoneIndex);
		grcDebugDraw::Axis(wheelBoneMtx, 1.0f);
	}

	if (m_renderWheelDrawMatrices)
	{
		// render the wheel draw matrix
		// MN - this currently appears to give the same results as the bone matrix above
		Mat34V wheelDrawMtx;
		Vec3V wheelBoundBox;
		pWheel->GetWheelMatrixAndBBox(RC_MATRIX34(wheelDrawMtx), RC_VECTOR3(wheelBoundBox));
		grcDebugDraw::Axis(wheelDrawMtx, 1.0f);
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  ProcessWheelLights
///////////////////////////////////////////////////////////////////////////////

void			CVfxWheel::ProcessWheelLights				(CWheel* pWheel, CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, VfxWheelInfo_s* pVfxWheelInfo, Vec3V_In vWheelPos, float UNUSED_PARAM(distSqrToCam), float frictionVal)
{
	if (pVfxWheelInfo->lightEnabled)
	{
		frictionVal *= pVfxVehicleInfo->GetWheelFrictionPtFxFricMult();

		// don't do anything below certain slip angle (needs tweaking)
		if (frictionVal < pVfxWheelInfo->ptFxWheelFricThreshMin)
		{
			return;
		}

		// subtract velocity along the hit normal
		Vec3V vFwd = -(RCC_VEC3V(pWheel->m_vecGroundContactVelocity)*ScalarVFromF32(VFXWHEEL_FRICTION_GROUND_VEL_MULT)) + (RCC_VEC3V(pWheel->m_vecTyreContactVelocity)*ScalarVFromF32(VFXWHEEL_FRICTION_VEL_MULT));
		Vec3V vUp = pVehicle->GetTransform().GetC();
		ScalarV sFwdDotUp = Dot(vFwd, vUp);
		vFwd = vFwd - (sFwdDotUp * vUp);

		if (MagSquared(vFwd).Getf()<0.01f)
		{
			return;
		}

		float frictionEvo = CVfxHelper::GetInterpValue(frictionVal, pVfxWheelInfo->ptFxWheelFricThreshMin, pVfxWheelInfo->ptFxWheelFricThreshMax);

		// light settings
		Vec3V vLightDir(V_Z_AXIS_WZERO);
		Vec3V vLightTan(V_Y_AXIS_WZERO);

		Vec3V vLightPos = vWheelPos + Vec3V(0.0f, 0.0f, 0.02f); 
		Vec3V vLightCol = Vec3V(CVfxHelper::GetRatioValue(frictionEvo, pVfxWheelInfo->lightColRMin/255.0f, pVfxWheelInfo->lightColRMax/255.0f),
								CVfxHelper::GetRatioValue(frictionEvo, pVfxWheelInfo->lightColGMin/255.0f, pVfxWheelInfo->lightColGMax/255.0f),
								CVfxHelper::GetRatioValue(frictionEvo, pVfxWheelInfo->lightColBMin/255.0f, pVfxWheelInfo->lightColBMax/255.0f));
		float lightIntensity = CVfxHelper::GetRatioValue(frictionEvo, pVfxWheelInfo->lightIntensityMin, pVfxWheelInfo->lightIntensityMax);
		float lightRange = CVfxHelper::GetRatioValue(frictionEvo, pVfxWheelInfo->lightRangeMin, pVfxWheelInfo->lightRangeMax);
		float lightFalloff = CVfxHelper::GetRatioValue(frictionEvo, pVfxWheelInfo->lightFalloffMin, pVfxWheelInfo->lightFalloffMax);

		// set up flags
		u32 lightFlags = LIGHTFLAG_FX;

		// set up light
		CLightSource lightSource(LIGHT_TYPE_POINT, lightFlags, RCC_VECTOR3(vLightPos), RCC_VECTOR3(vLightCol), lightIntensity, LIGHT_ALWAYS_ON);
		lightSource.SetDirTangent(RCC_VECTOR3(vLightDir), RCC_VECTOR3(vLightTan));
		lightSource.SetRadius(lightRange);
		lightSource.SetFalloffExponent(lightFalloff);

		CEntity* pAttachEntity = static_cast<CEntity*>(pVehicle);
		fwInteriorLocation interiorLocation = pAttachEntity->GetInteriorLocation();
		lightSource.SetInInterior(interiorLocation);
		Lights::AddSceneLight(lightSource);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxWheelFriction
///////////////////////////////////////////////////////////////////////////////

void			CVfxWheel::UpdatePtFxWheelFriction			(CWheel* pWheel, CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, VfxWheelInfo_s* pVfxWheelInfo, Vec3V_In vWheelPos, float distSqrToCam, float frictionVal, float wetEvo, bool isWet)
{
#if __BANK
	if (m_disableWheelFricFx)
	{
		return;
	}
#endif

	float rangeSqr = VFXRANGE_WHEEL_FRICTION * pVfxVehicleInfo->GetWheelGenericRangeMult() * m_wheelPtFxLodRangeScale;
	rangeSqr *= rangeSqr;
	if (distSqrToCam>rangeSqr)
	{
		return;
	}

	// no effects for flat tyres - only inflated and burst ones
//	if (m_fTyreHealth>=TYRE_HEALTH_FLAT && m_fTyreHealth<=TYRE_HEALTH_FLAT+0.0001f)
//	{
//		return;
//	}

	// don't update effect if the vehicle is invisible
	if (!pVehicle->GetIsVisible())
	{
		return;
	}

// 	// return if the player is in this vehicle and the camera is inside the vehicle
// 	if (pVehicle->ContainsLocalPlayer() && (camInterface::IsRenderedCameraInsideVehicle() || CVfxHelper::GetCamInVehicleFlag()))
// 	{
// 		return;
// 	}

	frictionVal *= pVfxVehicleInfo->GetWheelFrictionPtFxFricMult();

	// don't do anything below certain slip angle (needs tweaking)
	if (frictionVal < pVfxWheelInfo->ptFxWheelFricThreshMin)
	{
		return;
	}

	// check that we have an effect to play
	atHashWithStringNotFinal ptFxHashName;
	if (pVfxVehicleInfo->GetWheelGenericPtFxSet()==2)
	{
		ptFxHashName = pVfxWheelInfo->ptFxWheelFric2HashName;
	}
	else if (pVfxVehicleInfo->GetWheelGenericPtFxSet()==3)
	{
		ptFxHashName = pVfxWheelInfo->ptFxWheelFric3HashName;
	}
	else
	{
		vfxAssertf(pVfxVehicleInfo->GetWheelGenericPtFxSet()==1, "invalid wheel generic ptfx set specified for %s", pVehicle->GetDebugName());
		ptFxHashName = pVfxWheelInfo->ptFxWheelFric1HashName;
	}

	if (ptFxHashName.GetHash()==0)
	{
		return;
	}

	// 
	bool noColourTint = true;
	u8 colR = 255;
	u8 colG = 255;
	u8 colB = 255;

	if (pVehicle->GetVariationInstance().IsToggleModOn(VMT_TYRE_SMOKE))
	{
		// wheel smoke mod active - we need to tint with the smoke colour
		colR = pVehicle->GetVariationInstance().GetSmokeColorR();
		colG = pVehicle->GetVariationInstance().GetSmokeColorG();
		colB = pVehicle->GetVariationInstance().GetSmokeColorB();
		noColourTint = false;
		
		// if the smoke colour is black we swap effects instead (and don't tint)
		if (colR==0 && colG==0 && colB==0)
		{
			if (ptFxHashName==atHashWithStringNotFinal("wheel_fric_hard"))
			{
				ptFxHashName = atHashWithStringNotFinal("wheel_fric_hard_indep");
			}
			else if (ptFxHashName==atHashWithStringNotFinal("wheel_fric_hard_Bike"))
			{
				ptFxHashName = atHashWithStringNotFinal("wheel_fric_hard_Bike_indep");
			}
			else if (ptFxHashName==atHashWithStringNotFinal("wheel_fric_hard_tank"))
			{
				ptFxHashName = atHashWithStringNotFinal("wheel_fric_hard_tank_indep");
			}
			noColourTint = true;
		}
	}

	// get the matrix of the fx system
	Vec3V_ConstRef vPos = vWheelPos;
	Vec3V vFwd = -(RCC_VEC3V(pWheel->m_vecGroundContactVelocity)*ScalarVFromF32(VFXWHEEL_FRICTION_GROUND_VEL_MULT)) + (RCC_VEC3V(pWheel->m_vecTyreContactVelocity)*ScalarVFromF32(VFXWHEEL_FRICTION_VEL_MULT));
	Vec3V vUp = pVehicle->GetTransform().GetC();

	// subtract velocity along the hit normal
	ScalarV sFwdDotUp = Dot(vFwd, vUp);
	vFwd = vFwd - (sFwdDotUp * vUp);

 	if (MagSquared(vFwd).Getf()<0.01f)
 	{
 		return;
 	}

	Vec3V vCross = Cross(vFwd, vUp);
	vCross = Normalize(vCross);
	vFwd = Cross(vUp, vCross);
	vFwd = Normalize(vFwd);

	Mat34V wheelFxMat;
	wheelFxMat.SetCol0(vCross);
	wheelFxMat.SetCol1(vFwd);
	wheelFxMat.SetCol2(vUp);
	wheelFxMat.SetCol3(vPos);

	Vec3V vWheelMatA, vWheelMatB, vWheelMatC, vWheelMatD;
	vWheelMatA = vCross;
	vWheelMatB = vFwd;
	vWheelMatC = vUp;
	vWheelMatD = vPos;

#if __BANK
	if (CVehicle::ms_nVehicleDebug==VEH_DEBUG_FX && pVehicle->GetStatus()==STATUS_PLAYER)
	{
		Vec3V vParentMatC = pVehicle->GetTransform().GetC();

		// draw the velocity difference that was used to build the matrix
		Vec3V vVelUsed = RCC_VEC3V(pWheel->m_vecGroundContactVelocity) + RCC_VEC3V(pWheel->m_vecTyreContactVelocity);
		Vec3V vDrawAboveWheel = vWheelMatD + vParentMatC;
		grcDebugDraw::Line(RCC_VECTOR3(vDrawAboveWheel), RCC_VECTOR3(vDrawAboveWheel) + RCC_VECTOR3(vVelUsed), Color32(1.0f, 1.0f, 1.0f));

		grcDebugDraw::Line(RCC_VECTOR3(vWheelMatD), RCC_VECTOR3(vWheelMatD) + RCC_VECTOR3(vWheelMatA), Color32(1.0f, 0.0f, 0.0f));
		grcDebugDraw::Line(RCC_VECTOR3(vWheelMatD), RCC_VECTOR3(vWheelMatD) + RCC_VECTOR3(vWheelMatB), Color32(0.0f, 1.0f, 0.0f));
		grcDebugDraw::Line(RCC_VECTOR3(vWheelMatD), RCC_VECTOR3(vWheelMatD) + RCC_VECTOR3(vWheelMatC), Color32(0.0f, 0.0f, 1.0f));
	}
#endif

	// register the fx system
	int classFxOffset;
	if (isWet)
	{
		classFxOffset = PTFXOFFSET_WHEEL_FRIC_WET;
	}
	else
	{
		classFxOffset = PTFXOFFSET_WHEEL_FRIC_DRY;
	}

	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pWheel, classFxOffset, false, ptFxHashName, justCreated);

	// check the effect exists
	if (pFxInst)
	{
		// calc the offset matrix
		Mat34V relFxMat;
		CVfxHelper::CreateRelativeMat(relFxMat, pVehicle->GetMatrix(), wheelFxMat);

		pFxInst->SetOffsetMtx(relFxMat);

		// set evolutions
		float frictionEvo = CVfxHelper::GetInterpValue(frictionVal, pVfxWheelInfo->ptFxWheelFricThreshMin, pVfxWheelInfo->ptFxWheelFricThreshMax);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("friction", 0xF2F1048B), frictionEvo);

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("wet", 0x4CF2C242), wetEvo);

		float vehicleSpeed = pVehicle->GetVelocity().Mag();
		float speedEvo = CVfxHelper::GetInterpValue(vehicleSpeed, VFXWHEEL_FRICTION_SPEED_EVO_MIN, VFXWHEEL_FRICTION_SPEED_EVO_MAX);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		float spinEvo = 0.0f;
		bool isFWD = pWheel->GetHandlingData()->DriveFrontWheels();
		bool isRWD = pWheel->GetHandlingData()->DriveRearWheels();

		bool processSpinEvo = false;
		if (isRWD)
		{
			// AWD and RWD vehicles should only process spin evos on the rear wheels
			processSpinEvo = pWheel->GetConfigFlags().IsFlagSet(WCF_REARWHEEL);
		}
		else if (isFWD)
		{
			// FWD vehicles should only process spin evos on the non-rear wheels
			processSpinEvo = !pWheel->GetConfigFlags().IsFlagSet(WCF_REARWHEEL);
		}

		if (processSpinEvo)
		{
			if (vehicleSpeed<VFXWHEEL_FRICTION_SPIN_VEL_THRESH && Abs(pVehicle->GetThrottle())>VFXWHEEL_FRICTION_SPIN_THROTTLE_THRESH)
			{	
				float wheelGroundSpeed = Abs(pWheel->GetGroundSpeed());
				if (wheelGroundSpeed>VFXWHEEL_FRICTION_SPIN_GROUND_SPEED_EVO_MIN)
				{
					spinEvo = CVfxHelper::GetInterpValue(wheelGroundSpeed, VFXWHEEL_FRICTION_SPIN_GROUND_SPEED_EVO_MIN, VFXWHEEL_FRICTION_SPIN_GROUND_SPEED_EVO_MAX);
				}
			}
		}
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("spin", 0xCCE74BD9), spinEvo);

		float depthEvo = CVfxHelper::GetInterpValue(pWheel->GetDeepSurfaceInfo().GetDepth(), pWheel->m_fWheelRadius, 0.0f);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("depth", 0xBCA972D6), depthEvo);

		// override the draw distance to stop effect popping when rotating around the vehicle
//		float fxDistSqr;
//		CViewport* pViewport = gVpMan.GetClosestEffectiveActive3DSceneViewport(pVehicle->GetPosition(), &fxDistSqr);
//		if (pViewport!=NULL) 
//		{
//			s32 wheelIndex = pWheel->m_WheelId - VEH_WHEEL_LF;
//			pFxInst->SetOverrideDistSqrToCamera(fxDistSqr+FXVEHICLE_RENDER_ORDER_OFFSET_WHEEL+(wheelIndex/100.0f));
//		}

		// make sure the x axis is always pointing out of the side of the vehicle
		Vec3V vParentMatA = pVehicle->GetTransform().GetA();

		pFxInst->SetInvertAxes(0);
		float sideDot = Dot(vParentMatA, vWheelMatA).Getf();
		if (sideDot>=0.0f)
		{
			if (pWheel->GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL))
			{
				pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
			}
		}
		else
		{
			if (!pWheel->GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL))
			{
				pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
			}
		}

		ptfxAttach::Attach(pFxInst, pVehicle, -1);

		// set colour tint
		if (!noColourTint)
		{
#if __BANK
			if (m_smokeOverrideTintActive)
			{
				ptfxWrapper::SetColourTint(pFxInst, Vec3V(m_smokeOverrideTintR, m_smokeOverrideTintG, m_smokeOverrideTintB));
				ptfxWrapper::SetAlphaTint(pFxInst, m_smokeOverrideTintA);
			}
			else
#endif
			{
				if (colR!=255 || colG!=255 || colB!=255)
				{
					// the wheel smoke is modded (i.e. it isn't white) - set the colour tint and boost the alpha
					ptfxWrapper::SetColourTint(pFxInst, Vec3V(colR/255.0f, colG/255.0f, colB/255.0f));
					ptfxWrapper::SetAlphaTint(pFxInst, 1.5f);
				}
			}
		}

		// check if the effect has just been created 
		if (justCreated)
		{
// 			CPlayerInfo* pPlayerInfo = CGameWorld::GetMainPlayerInfo();
// 			if (pPlayerInfo)
// 			{
// 				CVehicle* pPlayerVehicle = pPlayerInfo->GetPlayerPed()->GetMyVehicle();
// 				if (pVehicle==pPlayerVehicle)
// 				{
// 					pFxInst->SetLODScalar(2.0f);
// 				}
// 			}

			pFxInst->SetFlag(PTFX_EFFECTINST_VEHICLE_WHEEL_ATTACHED_TRAILING);
			pFxInst->SetLODScalar(m_wheelPtFxLodRangeScale*pVfxVehicleInfo->GetWheelGenericRangeMult());

			// set zoom
			SetWheelFxZoom(pWheel, pFxInst);

			// it has just been created - start it and set its position
			pFxInst->Start();
		}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
		else
		{
			// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
			vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "Effect attachment error");
		}
#endif

#if __BANK
		if (m_outputWheelFricDebug)
		{
			vfxDebugf1("Wheel Friction Fx - friction:%.2f evo:%.2f\n", frictionVal, frictionEvo);
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxWheelDisplacement
///////////////////////////////////////////////////////////////////////////////

void			CVfxWheel::UpdatePtFxWheelDisplacement		(CWheel* pWheel, CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, VfxWheelInfo_s* pVfxWheelInfo, Vec3V_In vWheelPos, Vec3V_In vWheelNormal, float distSqrToCam, float displacementVal, float wetEvo, bool isWet)
{
#if __BANK
	if (m_disableWheelDispFx)
	{
		return;
	}
#endif

	float rangeSqr = VFXRANGE_WHEEL_DISPLACEMENT_HI * pVfxVehicleInfo->GetWheelGenericRangeMult() * m_wheelPtFxLodRangeScale;
	rangeSqr *= rangeSqr;
	if (distSqrToCam>rangeSqr)
	{
		return;
	}

	// don't update effect if the vehicle is invisible
	if (!pVehicle->GetIsVisible())
	{
		return;
	}

// 	// return if the player is in this vehicle and the camera is inside the vehicle
// 	if (pVehicle->ContainsLocalPlayer() && (camInterface::IsRenderedCameraInsideVehicle() || CVfxHelper::GetCamInVehicleFlag()))
// 	{
// 		return;
// 	}

	displacementVal *= pVfxVehicleInfo->GetWheelDisplacementPtFxDispMult();

	if (displacementVal < pVfxWheelInfo->ptFxWheelDispThreshMin)
	{
		return;
	}

	// check that we have an effect to play
	atHashWithStringNotFinal ptFxHashName;
	if (pVfxVehicleInfo->GetWheelGenericPtFxSet()==2)
	{
		ptFxHashName = pVfxWheelInfo->ptFxWheelDisp2HashName;
	}
	else if (pVfxVehicleInfo->GetWheelGenericPtFxSet()==3)
	{
		ptFxHashName = pVfxWheelInfo->ptFxWheelDisp3HashName;
	}
	else
	{
		vfxAssertf(pVfxVehicleInfo->GetWheelGenericPtFxSet()==1, "invalid wheel generic ptfx set specified for %s", pVehicle->GetDebugName());
		ptFxHashName = pVfxWheelInfo->ptFxWheelDisp1HashName;
	}

	if (ptFxHashName.GetHash()==0)
	{
		return;
	}

	// get the matrix of the fx system
	Vec3V_ConstRef vPos = vWheelPos;
	Vec3V vUp = vWheelNormal;
	Vec3V vFwd = RCC_VEC3V(pWheel->m_vecGroundContactVelocity);

	// subtract velocity along the hit normal
	ScalarV sFwdDotUp = Dot(vFwd, vUp);
	vFwd = vFwd - (sFwdDotUp * vUp);

	if (MagSquared(vFwd).Getf()<0.01f)
	{
		return;
	}

	vFwd = Normalize(vFwd);
	Vec3V vCross = Cross(vFwd, vUp);
	vCross = Normalize(vCross);
	vFwd = Cross(vUp, vCross);
	vUp = Normalize(vUp);

	Mat34V wheelFxMat;
	wheelFxMat.SetCol0(vCross);
	wheelFxMat.SetCol1(vFwd);
	wheelFxMat.SetCol2(vUp);
	wheelFxMat.SetCol3(vPos);

	Vec3V vWheelMatA, vWheelMatB, vWheelMatC, vWheelMatD;
	vWheelMatA = vCross;
	vWheelMatB = vFwd;
	vWheelMatC = vUp;
	vWheelMatD = vPos;

#if __BANK
	if (CVehicle::ms_nVehicleDebug==VEH_DEBUG_FX && pVehicle->GetStatus()==STATUS_PLAYER)
	{
		grcDebugDraw::Line(RCC_VECTOR3(vWheelMatD), RCC_VECTOR3(vWheelMatD) + RCC_VECTOR3(vWheelMatA), Color32(1.0f, 0.0f, 0.0f));
		grcDebugDraw::Line(RCC_VECTOR3(vWheelMatD), RCC_VECTOR3(vWheelMatD) + RCC_VECTOR3(vWheelMatB), Color32(0.0f, 1.0f, 0.0f));
		grcDebugDraw::Line(RCC_VECTOR3(vWheelMatD), RCC_VECTOR3(vWheelMatD) + RCC_VECTOR3(vWheelMatC), Color32(0.0f, 0.0f, 1.0f));
	}
#endif

	// register the fx system
	int classFxOffset;
	if (isWet)
	{
		classFxOffset = PTFXOFFSET_WHEEL_DISP_WET;
	}
	else
	{
		classFxOffset = PTFXOFFSET_WHEEL_DISP_DRY;
	}

	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pWheel, classFxOffset, false, ptFxHashName, justCreated);

	// check the effect exists
	if (pFxInst)
	{
		// calc the offset matrix
		Mat34V relFxMat;
		CVfxHelper::CreateRelativeMat(relFxMat, pVehicle->GetMatrix(), wheelFxMat);

		pFxInst->SetOffsetMtx(relFxMat);

		// calculate the angle of the wheel to the direction of the vehicle
		Mat34V wheelMtx;
		Vec3V vWheelBB;
		pWheel->GetWheelMatrixAndBBox(RC_MATRIX34(wheelMtx), RC_VECTOR3(vWheelBB));
		Vec3V vGroundContactVelNorm = RCC_VEC3V(pWheel->m_vecGroundContactVelocity);
		vGroundContactVelNorm = Normalize(vGroundContactVelNorm);
		float angleEvo = Abs(Dot(vWheelMatA, vGroundContactVelNorm)).Getf();
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("wheelangle", 0xAFA3EEE3), angleEvo);

		float dispEvo = CVfxHelper::GetInterpValue(displacementVal, pVfxWheelInfo->ptFxWheelDispThreshMin, pVfxWheelInfo->ptFxWheelDispThreshMax);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("groundspeed", 0x89A858E5), dispEvo);

		float depthEvo = CVfxHelper::GetInterpValue(pWheel->GetDeepSurfaceInfo().GetDepth(), pWheel->m_fWheelRadius, 0.0f);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("depth", 0xBCA972D6), depthEvo);

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("wet", 0x4CF2C242), wetEvo);

		// make sure the x axis is always pointing out of the side of the vehicle
		Vec3V vParentMatA = pVehicle->GetTransform().GetA();

		pFxInst->SetInvertAxes(0);
		float sideDot = Dot(vParentMatA, vWheelMatA).Getf();
		if (sideDot>=0.0f)
		{
			if (pWheel->GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL))
			{
				pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
			}
		}
		else
		{
			if (!pWheel->GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL))
			{
				pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
			}
		}

		// set velocity
		ptfxWrapper::SetVelocityAdd(pFxInst, RCC_VEC3V(pWheel->m_vecGroundContactVelocity));

		// set colour tint
#if __BANK
		if (m_smokeOverrideTintActive)
		{
			ptfxWrapper::SetColourTint(pFxInst, Vec3V(m_smokeOverrideTintR, m_smokeOverrideTintG, m_smokeOverrideTintB));
			ptfxWrapper::SetAlphaTint(pFxInst, m_smokeOverrideTintA);
		}
		else
#endif
		{
			u8 colR = pVehicle->GetVariationInstance().GetSmokeColorR();
			u8 colG = pVehicle->GetVariationInstance().GetSmokeColorG();
			u8 colB = pVehicle->GetVariationInstance().GetSmokeColorB();

			if (colR!=255 || colG!=255 || colB!=255)
			{
				// the wheel smoke is modded (i.e. it isn't white) - set the colour tint and boost the alpha
				ptfxWrapper::SetColourTint(pFxInst, Vec3V(colR/255.0f, colG/255.0f, colB/255.0f));
				ptfxWrapper::SetAlphaTint(pFxInst, 1.5f);
			}
		}

		// check if the effect has just been created 
		if (justCreated)
		{		
			ptfxAttach::Attach(pFxInst, pVehicle, -1);

// 			CPlayerInfo* pPlayerInfo = CGameWorld::GetMainPlayerInfo();
// 			if (pPlayerInfo)
// 			{
// 				CVehicle* pPlayerVehicle = pPlayerInfo->GetPlayerPed()->GetMyVehicle();
// 				if (pVehicle==pPlayerVehicle)
// 				{
// 					pFxInst->SetLODScalar(2.0f);
// 				}
// 			}

			pFxInst->SetLODScalar(m_wheelPtFxLodRangeScale*pVfxVehicleInfo->GetWheelGenericRangeMult());

//			if (pVehicle->GetVehicleType()==VEHICLE_TYPE_BIKE)
//			{
//				pFxInst->SetEvoValue("bike", 1.0f);
//			}
//			else
//			{
//				pFxInst->SetEvoValue("bike", 0.0f);
//			}

			// set zoom
			SetWheelFxZoom(pWheel, pFxInst);

			// it has just been created - start it and set its position
			pFxInst->Start();
		}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
		else
		{
			// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
			vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "Effect attachment error");
		}
#endif

#if __BANK
		if (m_outputWheelDispDebug)
		{
			vfxDebugf1("Wheel Displacement Fx - disp:%.2f evo:%.2f\n", displacementVal, dispEvo);
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxWheelBurnout
///////////////////////////////////////////////////////////////////////////////

void			CVfxWheel::UpdatePtFxWheelBurnout				(CWheel* pWheel, CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, VfxWheelInfo_s* pVfxWheelInfo, Vec3V_In vWheelPos, float distSqrToCam, float frictionVal, float UNUSED_PARAM(wetEvo), bool isWet)
{
#if __BANK
	if (m_disableWheelBurnoutFx)
	{
		return;
	}
#endif

	float rangeSqr = VFXRANGE_WHEEL_BURNOUT * pVfxVehicleInfo->GetWheelGenericRangeMult() * m_wheelPtFxLodRangeScale;
	rangeSqr *= rangeSqr;
	if (distSqrToCam>rangeSqr)
	{
		return;
	}

	// don't update effect if the vehicle is invisible
	if (!pVehicle->GetIsVisible())
	{
		return;
	}

// 	// return if the player is in this vehicle and the camera is inside the vehicle
// 	if (pVehicle->ContainsLocalPlayer() && (camInterface::IsRenderedCameraInsideVehicle() || CVfxHelper::GetCamInVehicleFlag()))
// 	{
// 		return;
// 	}

	float tyreTemp = pWheel->m_fTyreTemp * pVfxVehicleInfo->GetWheelBurnoutPtFxTempMult();
	frictionVal *= pVfxVehicleInfo->GetWheelBurnoutPtFxFricMult();

	// don't do anything below min temperature
	if (tyreTemp < pVfxWheelInfo->ptFxWheelBurnoutTempMin)
	{
		return;
	}

	// check that we have an effect to play
	atHashWithStringNotFinal ptFxHashName;
	if (pVfxVehicleInfo->GetWheelGenericPtFxSet()==2)
	{
		ptFxHashName = pVfxWheelInfo->ptFxWheelBurnout2HashName;
	}
	else if (pVfxVehicleInfo->GetWheelGenericPtFxSet()==3)
	{
		ptFxHashName = pVfxWheelInfo->ptFxWheelBurnout3HashName;
	}
	else
	{
		vfxAssertf(pVfxVehicleInfo->GetWheelGenericPtFxSet()==1, "invalid wheel generic ptfx set specified for %s", pVehicle->GetDebugName());
		ptFxHashName = pVfxWheelInfo->ptFxWheelBurnout1HashName;
	}

	if (ptFxHashName.GetHash()==0)
	{
		return;
	}

	// 
	bool noColourTint = true;
	u8 colR = 255;
	u8 colG = 255;
	u8 colB = 255;

	if (pVehicle->GetVariationInstance().IsToggleModOn(VMT_TYRE_SMOKE))
	{
		// wheel smoke mod active - we need to tint with the smoke colour
		colR = pVehicle->GetVariationInstance().GetSmokeColorR();
		colG = pVehicle->GetVariationInstance().GetSmokeColorG();
		colB = pVehicle->GetVariationInstance().GetSmokeColorB();
		noColourTint = false;

		// if the smoke colour is black we swap effects instead (and don't tint)
		if (colR==0 && colG==0 && colB==0)
		{
			if (ptFxHashName==atHashWithStringNotFinal("wheel_burnout"))
			{
				ptFxHashName = atHashWithStringNotFinal("wheel_burnout_indep");
			}
			noColourTint = true;
		}
	}

	// register the fx system
	int classFxOffset;
	if (isWet)
	{
		classFxOffset = PTFXOFFSET_WHEEL_BURNOUT_WET;
	}
	else
	{
		classFxOffset = PTFXOFFSET_WHEEL_BURNOUT_DRY;
	}

	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pWheel, classFxOffset, false, ptFxHashName, justCreated);

	// check the effect exists
	if (pFxInst)
	{
		// set the matrix
		Mat34V wheelBoneMtx;
		s32 wheelBoneIndex = pVehicle->GetBoneIndex(pWheel->m_WheelId);
		CVfxHelper::GetMatrixFromBoneIndex(wheelBoneMtx, pVehicle, wheelBoneIndex);

		Vec3V_ConstRef vPos = wheelBoneMtx.GetCol3();
		Vec3V vUp = vPos - vWheelPos;
#if GTA_REPLAY
		if (IsZeroAll(vUp))
			vUp = Vec3V(0.0f,0.0f,1.0f); //Bad data from replay B*1870373.
#endif
		vUp = Normalize(vUp);
		Vec3V vSide = -wheelBoneMtx.GetCol0();
		vSide = Normalize(vSide);
		Vec3V vCross = Cross(vUp, vSide);
		vCross = Normalize(vCross);
		vUp = Cross(vSide, vCross);
		vUp = Normalize(vUp);

		Mat34V wldFxMat;
		wldFxMat.SetCol0(vSide);
		wldFxMat.SetCol1(vCross);
		wldFxMat.SetCol2(vUp);
		wldFxMat.SetCol3(vPos);

		ptfxAttach::Attach(pFxInst, pVehicle, -1);
		Mat34V relFxMat;
		CVfxHelper::CreateRelativeMat(relFxMat, pVehicle->GetMatrix(), wldFxMat);
		pFxInst->SetOffsetMtx(relFxMat);

		pFxInst->SetLODScalar(m_wheelPtFxLodRangeScale*pVfxVehicleInfo->GetWheelGenericRangeMult());

		// set evolutions
		float tempEvo = CVfxHelper::GetInterpValue(tyreTemp, pVfxWheelInfo->ptFxWheelBurnoutTempMin, pVfxWheelInfo->ptFxWheelBurnoutTempMax);
// 		if (pWheel->GetHandlingData()->DriveFrontWheels() && pWheel->GetHandlingData()->DriveRearWheels())
// 		{
// 			// half the temp evo for 4 wheel drive vehicles
// 			tempEvo *= 0.5f;
// 		}
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("temp", 0x6CE0A51A), tempEvo);

		float frictionEvo = CVfxHelper::GetInterpValue(frictionVal, pVfxWheelInfo->ptFxWheelBurnoutFricMin, pVfxWheelInfo->ptFxWheelBurnoutFricMax);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("friction", 0xF2F1048B), frictionEvo);

		float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocity().Mag(), VFXWHEEL_BURNOUT_SPEED_EVO_MIN, VFXWHEEL_BURNOUT_SPEED_EVO_MAX);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

//		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("wet", 0x4CF2C242), wetEvo);

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("bike", 0xE77840DC), pVehicle->GetVehicleType()==VEHICLE_TYPE_BIKE ? 1.0f : 0.0f);

		// flip the axes to get the matrix how we need it
		pFxInst->SetInvertAxes(0);
		if (!pWheel->GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL) && pVehicle->GetVehicleType()!=VEHICLE_TYPE_BIKE)
		{
			pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_Y);
		}

		// set zoom
		SetWheelFxZoom(pWheel, pFxInst);

		// set colour tint
		if (!noColourTint)
		{
#if __BANK
			if (m_smokeOverrideTintActive)
			{
				ptfxWrapper::SetColourTint(pFxInst, Vec3V(m_smokeOverrideTintR, m_smokeOverrideTintG, m_smokeOverrideTintB));
				ptfxWrapper::SetAlphaTint(pFxInst, m_smokeOverrideTintA);
			}
			else
#endif
			{
				if (colR!=255 || colG!=255 || colB!=255)
				{
					// the wheel smoke is modded (i.e. it isn't white) - set the colour tint and boost the alpha
					ptfxWrapper::SetColourTint(pFxInst, Vec3V(colR/255.0f, colG/255.0f, colB/255.0f));
					ptfxWrapper::SetAlphaTint(pFxInst, 1.5f);
				}
			}
		}

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set its position
			pFxInst->SetFlag(PTFX_EFFECTINST_VEHICLE_WHEEL_ATTACHED_TRAILING);
			pFxInst->Start();
		}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
		else
		{
			// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
			vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "Effect attachment error");
		}
#endif

#if __BANK
		if (m_outputWheelBurnoutDebug)
		{
			vfxDebugf1("Wheel Burnout Fx - temp:%.2f evo:%.2f fric:%.2f evo:%.2f\n", tyreTemp, tempEvo, frictionVal, frictionEvo);
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxWheelPuncture
///////////////////////////////////////////////////////////////////////////////

void			CVfxWheel::TriggerPtFxWheelPuncture			(CWheel* pWheel, CVehicle* pVehicle, Vec3V_In UNUSED_PARAM(vPos), Vec3V_In UNUSED_PARAM(vNormal), s32 boneIndex)
{
	// don't update effect if the vehicle is off screen
	if (!pVehicle->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	// check if we're in range
	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>pVfxVehicleInfo->GetWheelPuncturePtFxRangeSqr())
	{
		return;
	}

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pVfxVehicleInfo->GetWheelPuncturePtFxName());
	if (pFxInst)
	{
		// get the parent matrix (from the component hit)
		Mat34V boneMtx;
		CVfxHelper::GetMatrixFromBoneIndex(boneMtx, pVehicle, (eAnimBoneTag)boneIndex);

		// calc the offset matrix (using the impact position and normal)
// 		// get the world matrix of the effect
// 		Mat34V wldMat;
// 		CVfxHelper::CreateMatFromVecZ(wldMat, vPos, vNormal);
// 		Transform(wldMat, pVehicle->GetMatrix(), wldMat);
// 
// 		Mat34V relFxMat;
// 		CVfxHelper::CreateRelativeMat(relFxMat, boneMtx, wldMat);

		// if the local wheel pos is within the rim then move out to the tyre
// 		Vec3V vVec(0.0f, relFxMat.GetCol3().GetYf(), relFxMat.GetCol3().GetZf());
// 		if (Mag(vVec).Getf()<pWheel->m_fWheelRimRadius)
// 		{
// 			vVec = Normalize(vVec);
// 			vVec *= ScalarVFromF32(pWheel->m_fWheelRimRadius);
// 			relFxMat.SetCol3(Vec3V(relFxMat.GetCol3().GetXf(), vVec.GetYf(), vVec.GetZf()));
// 		}

		// calc the offset matrix (play at the outside centre of the wheel now instead of the impact position and normal)
		Vec3V vOffsetPos(V_ZERO);
		if (pVehicle->GetVehicleType()!=VEHICLE_TYPE_BIKE && pVehicle->GetVehicleType()!=VEHICLE_TYPE_BICYCLE)
		{
			vOffsetPos.SetXf(-pWheel->GetWheelWidth()*0.5f);
		}

		// set up the fx inst
		ptfxAttach::Attach(pFxInst, pVehicle, boneIndex);

		pFxInst->SetOffsetPos(vOffsetPos);

		pFxInst->SetUserZoom(pWheel->GetWheelRadius());

		float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocity().Mag(), 0.0f, 20.0f);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		// start the fx
		pFxInst->Start();

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			u8 wheelIndex = 0;
			for (; wheelIndex < pVehicle->GetNumWheels(); wheelIndex++)
			{
				if(pVehicle->GetWheel(wheelIndex) == pWheel)
					break;
			}

			CReplayMgr::RecordFx<CPacketVehTyrePunctureFx>(
				CPacketVehTyrePunctureFx((u8)boneIndex,
					wheelIndex,
					speedEvo),
				pVehicle);
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxWheelBurst
///////////////////////////////////////////////////////////////////////////////

void			CVfxWheel::TriggerPtFxWheelBurst			(CWheel* pWheel, CVehicle* pVehicle, Vec3V_In vPos, s32 UNUSED_PARAM(boneIndex), bool dueToWheelSpin, bool dueToFire)
{
	// don't update effect if the vehicle is off screen
	if (!pVehicle->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	// check if we're in range
	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>pVfxVehicleInfo->GetWheelBurstPtFxRangeSqr())
	{
		return;
	}

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pVfxVehicleInfo->GetWheelBurstPtFxName());
	if (pFxInst)
	{
		// get the world matrix of the effect
		Mat34V fxMat = pVehicle->GetMatrix();
		fxMat.SetCol3(vPos);

		// calc the offset matrix
		Mat34V relFxMat;
		CVfxHelper::CreateRelativeMat(relFxMat, pVehicle->GetMatrix(), fxMat);

		ptfxAttach::Attach(pFxInst, pVehicle, -1);

		pFxInst->SetOffsetMtx(relFxMat);

		float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocity().Mag(), 0.0f, 20.0f);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		float spinEvo = 0.0f;
		if (dueToWheelSpin)
		{
			spinEvo = 1.0f;
		}
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("spin", 0xCCE74BD9), spinEvo);

		float fireEvo = 0.0f;
		if (dueToFire)
		{
			fireEvo = 1.0f;
		}
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("fire", 0xD30A036B), fireEvo);

		// flip the x axis for left hand effects
		if (pWheel->GetConfigFlags().IsFlagSet(WCF_LEFTWHEEL))
		{
			pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
		}

		pFxInst->SetUserZoom(pWheel->GetWheelRadius());

		pFxInst->Start();

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			u8 wheelIndex = 0;
			for (; wheelIndex < pVehicle->GetNumWheels(); wheelIndex++)
			{
				if(pVehicle->GetWheel(wheelIndex) == pWheel)
					break;
			}

			CReplayMgr::RecordFx<CPacketVehTyreBurstFx>(
				CPacketVehTyreBurstFx(VEC3V_TO_VECTOR3(vPos),
					wheelIndex,
					dueToWheelSpin, 
					dueToFire),
				pVehicle);
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetWheelFxZoom
///////////////////////////////////////////////////////////////////////////////

void			CVfxWheel::SetWheelFxZoom						(CWheel* pWheel, ptxEffectInst* pFxInst)
{
	float userZoom = 0.0f;
	if (pWheel->GetTyreHealth()==0.0f)
	{
		userZoom = Min(pWheel->GetWheelWidth(), 0.4f);
	}
	else
	{
		userZoom = Min(pWheel->GetWheelRadius(), 0.5f);
		if (pWheel->GetTyreHealth()>=TYRE_HEALTH_FLAT && pWheel->GetTyreHealth()<=TYRE_HEALTH_FLAT+0.0001f)
		{
			userZoom *= 0.8f;
		}
	}

	pFxInst->SetUserZoom(userZoom);
}



///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void		 	CVfxWheel::InitWidgets				()
{
	if (m_bankInitialised==false)
	{	
		//char txt[128];
		//u32 vehicleInfoSize = sizeof(VfxWheelInfo_s);
		//float vehicleInfoPoolSize = (vehicleInfoSize * NUM_VFX_GROUPS) / 1024.0f;

		bkBank* pVfxBank = vfxWidget::GetBank();
		pVfxBank->PushGroup("Vfx Wheel", false);
		{
			//pVfxBank->AddTitle("INFO");
			//sprintf(txt, "Num Vehicle Infos (%d - %.2fK)", NUM_VFX_GROUPS, vehicleInfoPoolSize);
			//pVfxBank->AddSlider(txt, &m_numVfxWheelInfos, 0, NUM_VFX_GROUPS, 0);

#if __DEV
			pVfxBank->AddTitle("");
			pVfxBank->AddTitle("SETTINGS");
			pVfxBank->AddSlider("Skidmark Width Mult", &VFXWHEEL_SKID_WIDTH_MULT, 0.5f, 2.0f, 0.02f, NullCB, "The value that skidmark width is multiplied by to get the decal width");
			pVfxBank->AddSlider("Skidmark Width Mult (Bicycles)", &VFXWHEEL_SKID_WIDTH_MULT_BICYCLE, 0.5f, 2.0f, 0.02f, NullCB, "The value that skidmark width is multiplied by to get the decal width for bicycles");
			
			pVfxBank->AddSlider("Skidmark Max Alpha", &VFXWHEEL_SKID_MAX_ALPHA, 0.0f, 1.0f, 0.01f);

			pVfxBank->AddSlider("Friction Ground Vel Mult", &VFXWHEEL_FRICTION_GROUND_VEL_MULT, -10.0f, 10.0f, 1.0f, NullCB, "");
			pVfxBank->AddSlider("Friction Tyre Vel Mult", &VFXWHEEL_FRICTION_VEL_MULT, -10.0f, 10.0f, 1.0f, NullCB, "");
	
			pVfxBank->AddSlider("Friction Speed Evo Min", &VFXWHEEL_FRICTION_SPEED_EVO_MIN, 0.0f, 50.0f, 1.0f, NullCB, "The speed value at which the speed evolution will start to rise above 0.0");
			pVfxBank->AddSlider("Friction Speed Evo Max", &VFXWHEEL_FRICTION_SPEED_EVO_MAX, 0.0f, 50.0f, 1.0f, NullCB, "The speed value at which the speed evolution will max out at 1.0");
		
			pVfxBank->AddSlider("Friction Spin Vel Thresh", &VFXWHEEL_FRICTION_SPIN_VEL_THRESH, 0.0f, 50.0f, 1.0f, NullCB, "The vehicle speed value below which the spin evolution will be valid");
			pVfxBank->AddSlider("Friction Spin Throttle Thresh", &VFXWHEEL_FRICTION_SPIN_THROTTLE_THRESH, 0.0f, 1.0f, 1.0f, NullCB, "The vehicle throttle value above which the spin evolution will be valid");
			pVfxBank->AddSlider("Friction Spin Ground Speed Evo Min", &VFXWHEEL_FRICTION_SPIN_GROUND_SPEED_EVO_MIN, 0.0f, 50.0f, 1.0f, NullCB, "The wheel ground speed value at which the spin evolution will start to rise above 0.0");
			pVfxBank->AddSlider("Friction Spin Ground Speed Evo Max", &VFXWHEEL_FRICTION_SPIN_GROUND_SPEED_EVO_MAX, 0.0f, 50.0f, 1.0f, NullCB, "The wheel ground speed value at which the spin evolution will max out at 1.0");

			pVfxBank->AddSlider("Burnout Speed Evo Min", &VFXWHEEL_BURNOUT_SPEED_EVO_MIN, 0.0f, 50.0f, 1.0f, NullCB, "The speed value at which the speed evolution will start to rise above 0.0");
			pVfxBank->AddSlider("Burnout Speed Evo Max", &VFXWHEEL_BURNOUT_SPEED_EVO_MAX, 0.0f, 50.0f, 1.0f, NullCB, "The speed value at which the speed evolution will max out at 1.0");

			pVfxBank->AddSlider("Wet Road Dry Max Thresh", &VFXWHEEL_WETROAD_DRY_MAX_THRESH, 0.0f, 1.0f, 0.025f, NullCB, "The maximum wet road value at which it will be considered dry");
			pVfxBank->AddSlider("Wet Road Wet Min Thresh", &VFXWHEEL_WETROAD_WET_MIN_THRESH, 0.0f, 1.0f, 0.025f, NullCB, "The minimum wet road value at which it will be considered wet");

			pVfxBank->AddSlider("Wet Reduce Temp Min", &VFXWHEEL_WET_REDUCE_TEMP_MIN, 0.0f, 200.0f, 1.0f, NullCB, "The minimum wheel temp that will start to reduce the wet road value");
			pVfxBank->AddSlider("Wet Reduce Temp Max", &VFXWHEEL_WET_REDUCE_TEMP_MAX, 0.0f, 200.0f, 1.0f, NullCB, "The maximum wheel temp that will start to reduce the wet road value");

			pVfxBank->AddSlider("Cool Rate", &VFXWHEEL_COOL_RATE, 0.0f, 20.0f, 1.0f, NullCB, "");
#endif

			pVfxBank->AddTitle("");
			pVfxBank->AddTitle("DEBUG");
			pVfxBank->AddToggle("Disable Skidmarks", &m_disableSkidmarks);
			pVfxBank->AddToggle("Disable Wheel Friction Fx", &m_disableWheelFricFx);
			pVfxBank->AddToggle("Disable Wheel Displacement Fx", &m_disableWheelDispFx);
			pVfxBank->AddToggle("Disable Wheel Burnout Fx", &m_disableWheelBurnoutFx);
			pVfxBank->AddToggle("Disable Wheel FL ", &m_disableWheelFL);
			pVfxBank->AddToggle("Disable Wheel FR ", &m_disableWheelFR);
			pVfxBank->AddToggle("Disable Wheel ML ", &m_disableWheelML);
			pVfxBank->AddToggle("Disable Wheel MR ", &m_disableWheelMR);
			pVfxBank->AddToggle("Disable Wheel RL ", &m_disableWheelRL);
			pVfxBank->AddToggle("Disable Wheel RR ", &m_disableWheelRR);

			pVfxBank->AddSlider("Skidmark Overlap %", &m_skidmarkOverlapPerc, -100.0f, 100.0f, 1.0f);

			pVfxBank->AddSlider("Override WetRoad Val", &m_wetRoadOverride, 0.0f, 1.0f, 0.005f);

			pVfxBank->AddToggle("Smoke Override Tint Active ", &m_smokeOverrideTintActive);
			pVfxBank->AddSlider("Smoke Override Tint R", &m_smokeOverrideTintR, 0.0f, 5.0f, 0.1f);
			pVfxBank->AddSlider("Smoke Override Tint G", &m_smokeOverrideTintG, 0.0f, 5.0f, 0.1f);
			pVfxBank->AddSlider("Smoke Override Tint B", &m_smokeOverrideTintB, 0.0f, 5.0f, 0.1f);
			pVfxBank->AddSlider("Smoke Override Tint A", &m_smokeOverrideTintA, 0.0f, 5.0f, 0.1f);
			
			pVfxBank->AddSlider("Wheel Lod/Range Scale", &m_wheelPtFxLodRangeScale, 0.0f, 20.0f, 0.5f);
			pVfxBank->AddSlider("Wheel Skidmark Range Scale", &m_wheelSkidmarkRangeScale, 0.0f, 20.0f, 0.5f);
			
			pVfxBank->AddToggle("Use Snow Wheel Vfx When Unsheltered", &m_useSnowWheelVfxWhenUnsheltered);

			pVfxBank->AddToggle("Output Wheel Vfx Values", &m_outputWheelVfxValues);
			pVfxBank->AddToggle("Output Wheel Friction", &m_outputWheelFricDebug);
			pVfxBank->AddToggle("Output Wheel Displacement", &m_outputWheelDispDebug);
			pVfxBank->AddToggle("Output Wheel Burnout", &m_outputWheelBurnoutDebug);

			pVfxBank->AddToggle("Render Wheel Bone Matrices", &m_renderWheelBoneMatrices);
			pVfxBank->AddToggle("Render Wheel Draw Matrices", &m_renderWheelDrawMatrices);

			pVfxBank->AddToggle("Disable No Movement Optimisation", &m_disableNoMovementOpt);

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
void			CVfxWheel::Reset							()
{
	g_vfxWheel.Shutdown(SHUTDOWN_WITH_MAP_LOADED);
	g_vfxWheel.Init(INIT_AFTER_MAP_LOADED);

}
#endif // __BANK
