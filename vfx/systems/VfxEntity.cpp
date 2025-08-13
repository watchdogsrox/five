///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	FxEntity.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	05 Sept 2006
//	WHAT:	 
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxEntity.h"

// rage
#include "audioengine/engine.h"
#include "pheffects/wind.h"
#include "rmptfx/ptxeffectrule.h"
#include "system/param.h"

// framework
#include "fwsys/timer.h"
#include "grcore/debugdraw.h"
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxhistory.h"
#include "vfx/ptfx/ptfxreg.h"
#include "vfx/ptfx/ptfxwrapper.h"
#include "vfx/vfxwidget.h"

// game
#include "Audio/Ambience/AmbientAudioEntity.h"
#include "Core/Game.h"
#include "Game/Clock.h"
#include "Game/Weather.h"
#include "Game/Wind.h"
#include "Renderer/Lights/Lights.h"
#include "Scene/2dEffect.h"
#include "Scene/DataFileMgr.h"
#include "Scene/Entities/CompEntity.h"
#include "Scene/Physical.h"
#include "Scene/Portals/Portal.h"
#include "Scene/Loader/MapTypes.h"
#include "TimeCycle/TimeCycle.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Particles/PtFxDefines.h"
#include "Vfx/Particles/PtFxCollision.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/VfxHelper.h"
#include "Weapons/Explosion.h"
#include "control/replay/Effects/ParticleEntityFxPacket.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()

///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

// rag tweakable settings
dev_float	ENTITYFX_COLL_HIST_DIST				(0.5f);
dev_float	ENTITYFX_COLL_HIST_TIME				(0.5f);

dev_float	ENTITYFX_SHOT_HIST_DIST				(0.5f);
dev_float	ENTITYFX_SHOT_HIST_TIME				(0.5f);

dev_float	ENTITYFX_INWATER_HIST_DIST			(0.5f);
dev_float	ENTITYFX_INWATER_HIST_TIME			(0.5f);


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////
	
CVfxEntity	g_vfxEntity;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxEntityAsyncProbeManager
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxEntityAsyncProbeManager::Init			()
{
	// init the decal probes
	for (int i=0; i<VFXENTITY_MAX_DECAL_PROBES; i++)
	{
		m_decalProbes[i].m_vfxProbeId= -1;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxEntityAsyncProbeManager::Shutdown		()
{
	// init the decal probes
	for (int i=0; i<VFXENTITY_MAX_DECAL_PROBES; i++)
	{
		if (m_decalProbes[i].m_vfxProbeId>=0)
		{
			CVfxAsyncProbeManager::AbortProbe(m_decalProbes[i].m_vfxProbeId);
			m_decalProbes[i].m_vfxProbeId = -1;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxEntityAsyncProbeManager::Update			()
{
	// update the splat probes
	for (int i=0; i<VFXENTITY_MAX_DECAL_PROBES; i++)
	{
		if (m_decalProbes[i].m_vfxProbeId>=0)
		{
			// query the probe
			VfxAsyncProbeResults_s vfxProbeResults;
			if (CVfxAsyncProbeManager::QueryProbe(m_decalProbes[i].m_vfxProbeId, vfxProbeResults))
			{
				// the probe has finished
				m_decalProbes[i].m_vfxProbeId = -1;

				// check if the probe intersected
				if (vfxProbeResults.hasIntersected)
				{
					// there was an intersection 
					//CEntity* pHitEntity = CVfxAsyncProbeManager::GetEntity(vfxProbeResults);
					//if (pHitEntity)
					{
						Vec3V vDecalDir = -vfxProbeResults.vNormal;

						Vec3V vRandSide;
						CVfxHelper::GetRandomTangent(vRandSide, vDecalDir);

						const VfxEntityGenericDecalInfo_s* pDecalInfo = m_decalProbes[i].m_pDecalInfo;

						float randSize = g_DrawRand.GetRanged(pDecalInfo->sizeMin, pDecalInfo->sizeMax) * m_decalProbes[i].m_scale;
						float randLife = g_DrawRand.GetRanged(pDecalInfo->lifeMin, pDecalInfo->lifeMax);
						float randAlpha = g_DrawRand.GetRanged(pDecalInfo->alphaMin, pDecalInfo->alphaMax);

						Color32 col(pDecalInfo->colR, pDecalInfo->colG, pDecalInfo->colB, static_cast<u8>(randAlpha*255));

						g_decalMan.AddGeneric(DECALTYPE_GENERIC_SIMPLE_COLN, pDecalInfo->renderSettingIndex, pDecalInfo->renderSettingCount, vfxProbeResults.vPos, vDecalDir, vRandSide, randSize, randSize, 0.0f, randLife, pDecalInfo->fadeInTime, pDecalInfo->uvMultStart, pDecalInfo->uvMultEnd, pDecalInfo->uvMultTime, col, false, false, 0.01f, false, false REPLAY_ONLY(, 0.0f));
					}
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerDecalProbe
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxEntityAsyncProbeManager::TriggerDecalProbe(Vec3V_In vStartPos, Vec3V_In vEndPos, const VfxEntityGenericDecalInfo_s* pDecalInfo, float scale)
{
	static u32 probeFlags = ArchetypeFlags::GTA_MAP_TYPE_WEAPON;

	// look for an available probe
	for (int i=0; i<VFXENTITY_MAX_DECAL_PROBES; i++)
	{
		if (m_decalProbes[i].m_vfxProbeId==-1)
		{
			m_decalProbes[i].m_vfxProbeId = CVfxAsyncProbeManager::SubmitWorldProbe(vStartPos, vEndPos, probeFlags);
			m_decalProbes[i].m_pDecalInfo = pDecalInfo;
			m_decalProbes[i].m_scale = scale;

#if __BANK
			if (g_vfxEntity.m_renderEntityDecalProbes)
			{
				grcDebugDraw::Line(vStartPos, vEndPos, Color32(0.0f, 1.0f, 0.0f, 1.0f), -200);
			}
#endif

			return;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxEntity
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxEntity::Init								(unsigned initMode)
{
	if (initMode==INIT_CORE)
	{
		m_inWaterPtFxDisabledScriptThreadId = THREAD_INVALID;
		m_inWaterPtFxDisabled = false;

#if __BANK
		m_bankInitialised = false;
#endif
	}
	else if (initMode==INIT_AFTER_MAP_LOADED)
	{
	}
	else if (initMode==INIT_SESSION)
	{
		m_asyncProbeMgr.Init();
    }
}


///////////////////////////////////////////////////////////////////////////////
//  InitDefinitions
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxEntity::InitDefinitions						()
{
	LoadData();
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void			CVfxEntity::Shutdown							(unsigned shutdownMode)
{	
	if (shutdownMode==SHUTDOWN_WITH_MAP_LOADED)
	{
	}
	else if (shutdownMode==SHUTDOWN_SESSION)
	{
		m_asyncProbeMgr.Shutdown();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxEntity::Update							()
{
	m_asyncProbeMgr.Update();
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveScript
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxEntity::RemoveScript						(scrThreadId scriptThreadId)
{
	if (scriptThreadId==m_inWaterPtFxDisabledScriptThreadId)
	{
		m_inWaterPtFxDisabled = false;
		m_inWaterPtFxDisabledScriptThreadId = THREAD_INVALID;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetInWaterPtFxDisabled
///////////////////////////////////////////////////////////////////////////////

void			CVfxEntity::SetInWaterPtFxDisabled				(bool val, scrThreadId scriptThreadId)
{
	if (vfxVerifyf(m_inWaterPtFxDisabledScriptThreadId==THREAD_INVALID || m_inWaterPtFxDisabledScriptThreadId==scriptThreadId, "trying to disable in-water vfx when this is already in use by another script")) 
	{
		m_inWaterPtFxDisabled = val; 
		m_inWaterPtFxDisabledScriptThreadId = scriptThreadId;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  GetPtFxAssetName
///////////////////////////////////////////////////////////////////////////////

atHashWithStringNotFinal CVfxEntity::GetPtFxAssetName			(eParticleType ptFxType, atHashWithStringNotFinal m_tagHashName)
{
	VfxEntityGenericPtFxInfo_s* pGenericPtFxInfo = NULL;

	if (ptFxType==PT_AMBIENT_FX)
	{
		pGenericPtFxInfo = GetAmbientPtFxInfo(m_tagHashName);
	}
	else if (ptFxType==PT_COLLISION_FX)
	{
		pGenericPtFxInfo = GetCollisionPtFxInfo(m_tagHashName);
	}
	else if (ptFxType==PT_SHOT_FX)
	{
		pGenericPtFxInfo = GetShotPtFxInfo(m_tagHashName);
	}
	else if (ptFxType==PT_BREAK_FX)
	{
		pGenericPtFxInfo = GetBreakPtFxInfo(m_tagHashName);
	}
	else if (ptFxType==PT_DESTROY_FX)
	{
		pGenericPtFxInfo = GetDestroyPtFxInfo(m_tagHashName);
	}
	else if (ptFxType==PT_ANIM_FX)
	{
		pGenericPtFxInfo = GetAnimPtFxInfo(m_tagHashName);
	}
	else if (ptFxType==PT_RAYFIRE_FX)
	{
		pGenericPtFxInfo = GetRayFirePtFxInfo(m_tagHashName);
	}
	else if (ptFxType==PT_INWATER_FX)
	{
		pGenericPtFxInfo = GetInWaterPtFxInfo(m_tagHashName);
	}

	if (pGenericPtFxInfo)
	{
		return pGenericPtFxInfo->ptFxAssetHashName;
	}

	return atHashWithStringNotFinal((u32)0);
}

#if __ASSERT
bool CVfxEntity::GetIsInfinitelyLooped (const CParticleAttr& PtFxAttr)
{
	VfxEntityAnimPtFxInfo_s* pAnimInfo = GetAnimPtFxInfo(PtFxAttr.m_tagHashName);
	if (pAnimInfo)
	{
		ptxEffectRule* pEffectRule = ptfxWrapper::GetEffectRule(pAnimInfo->ptFxHashName);
		if (pEffectRule)
		{
			return pEffectRule->GetIsInfinitelyLooped();
		}
	}

	return false;
}
#endif

///////////////////////////////////////////////////////////////////////////////
//  LoadData
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxEntity::LoadData							()
{
	// initialise the data
	m_ambientPtFxInfoMap.Reset();
	m_collisionPtFxInfoMap.Reset();
	m_shotPtFxInfoMap.Reset();
	m_breakPtFxInfoMap.Reset();
	m_destroyPtFxInfoMap.Reset();
	m_animPtFxInfoMap.Reset();
	m_rayfirePtFxInfoMap.Reset();
	m_inWaterPtFxInfoMap.Reset();

	m_collisionDecalInfoMap.Reset();
	m_shotDecalInfoMap.Reset();
	m_breakDecalInfoMap.Reset();
	m_destroyDecalInfoMap.Reset();

	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::ENTITYFX_FILE);
	while(DATAFILEMGR.IsValid(pData))
	{
		// read in the data
		fiStream* stream = ASSET.Open(pData->m_filename, "dat", true);
		vfxAssertf(stream, "cannot open data file");

		if (stream)
		{
			// initialise the tokens
			fiAsciiTokenizer token;
			token.Init("entityFx", stream);

			// read in the data
			char charBuff[128];
			s32 phase = -1;	

			// read in the version
			m_version = token.GetFloat();
			// m_version = m_version;

			if (m_version<5.0f)
			{
				return;
			}

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
				if (	(stricmp(charBuff, "ENTITYFX_START")==0)					||
						(stricmp(charBuff, "ENTITYFX_AMBIENT_PTFX_END")==0)			||
						(stricmp(charBuff, "ENTITYFX_COLLISION_PTFX_END")==0)		||
						(stricmp(charBuff, "ENTITYFX_SHOT_PTFX_END")==0)			||
						(stricmp(charBuff, "FRAGMENTFX_BREAK_PTFX_END")==0)			||
						(stricmp(charBuff, "FRAGMENTFX_DESTROY_PTFX_END")==0)		||
						(stricmp(charBuff, "ENTITYFX_ANIM_PTFX_END")==0)			||
						(stricmp(charBuff, "ENTITYFX_RAYFIRE_PTFX_END")==0)			||
						(stricmp(charBuff, "ENTITYFX_INWATER_PTFX_END")==0)			||
						(stricmp(charBuff, "ENTITYFX_COLLISION_DECAL_END")==0)		||
						(stricmp(charBuff, "ENTITYFX_SHOT_DECAL_END")==0)			||
						(stricmp(charBuff, "FRAGMENTFX_BREAK_DECAL_END")==0)		||
						(stricmp(charBuff, "FRAGMENTFX_DESTROY_DECAL_END")==0))		{phase = -1; continue;}
				else if (stricmp(charBuff, "ENTITYFX_AMBIENT_PTFX_START")==0)		{phase = 0;	continue;}
				else if (stricmp(charBuff, "ENTITYFX_COLLISION_PTFX_START")==0)		{phase = 1;	continue;}
				else if (stricmp(charBuff, "ENTITYFX_SHOT_PTFX_START")==0)			{phase = 2;	continue;}
				else if (stricmp(charBuff, "FRAGMENTFX_BREAK_PTFX_START")==0)		{phase = 3; continue;}
				else if (stricmp(charBuff, "FRAGMENTFX_DESTROY_PTFX_START")==0)		{phase = 4;	continue;}
				else if (stricmp(charBuff, "ENTITYFX_ANIM_PTFX_START")==0)			{phase = 5;	continue;}
				else if (stricmp(charBuff, "ENTITYFX_RAYFIRE_PTFX_START")==0)		{phase = 6;	continue;}
				else if (stricmp(charBuff, "ENTITYFX_INWATER_PTFX_START")==0)		{phase = 7;	continue;}
				else if (stricmp(charBuff, "ENTITYFX_COLLISION_DECAL_START")==0)	{phase = 8; continue;}
				else if (stricmp(charBuff, "ENTITYFX_SHOT_DECAL_START")==0)			{phase = 9;	continue;}
				else if (stricmp(charBuff, "FRAGMENTFX_BREAK_DECAL_START")==0)		{phase = 10; continue;}
				else if (stricmp(charBuff, "FRAGMENTFX_DESTROY_DECAL_START")==0)	{phase = 11; continue;}
				else if (stricmp(charBuff, "ENTITYFX_END")==0)						{break;}

				if (phase==0)
				{
					VfxEntityAmbientPtFxInfo_s ambientPtFxInfo;

					// generic ptfx data
					LoadGenericPtFxData(token, &ambientPtFxInfo, charBuff);

					// ignore rotation
					s32 ignoreRot = token.GetInt();
					ambientPtFxInfo.ignoreRotation = ignoreRot>0;

					// play always flag
					s32 playAlways = token.GetInt();
					ambientPtFxInfo.alwaysOn = playAlways>0;

					// sun threshold
					if (m_version>=7.0f)
					{
						ambientPtFxInfo.sunThresh = token.GetFloat();
					}
					else
					{
						ambientPtFxInfo.sunThresh = -1.0f;
					}

					// rain threshold
					ambientPtFxInfo.rainThresh = token.GetFloat();

					// wind threshold
					ambientPtFxInfo.windThresh = token.GetFloat();

					// temperature
					ambientPtFxInfo.tempMin = token.GetFloat();
					ambientPtFxInfo.tempMax = token.GetFloat();

					// time of day
					ambientPtFxInfo.timeMin = token.GetInt();
					ambientPtFxInfo.timeMax = token.GetInt();

					vfxAssertf(ambientPtFxInfo.tempMin <= 
						ambientPtFxInfo.tempMax, "min temperature is greater than max");

					// evolution flags
					s32 hasRainEvo = token.GetInt();
					ambientPtFxInfo.hasRainEvo = hasRainEvo>0;

					s32 hasWindEvo = token.GetInt();
					ambientPtFxInfo.hasWindEvo = hasWindEvo>0;
					ambientPtFxInfo.maxWindEvo = token.GetFloat();
					ambientPtFxInfo.tempEvoMin = token.GetFloat();
					ambientPtFxInfo.tempEvoMax = token.GetFloat();

					// misc
					ambientPtFxInfo.nightProbMult = token.GetFloat();

					// triggered
					if (m_version>=8.0f)
					{
						s32 isTriggered = token.GetInt();
						ambientPtFxInfo.isTriggered = isTriggered>0;
						ambientPtFxInfo.triggerTimeMin = token.GetFloat();
						ambientPtFxInfo.triggerTimeMax = token.GetFloat();
					}
					else
					{
						ambientPtFxInfo.isTriggered = false;
						ambientPtFxInfo.triggerTimeMin = 0.0f;
						ambientPtFxInfo.triggerTimeMax = 0.0f;
					}

					m_ambientPtFxInfoMap.Insert(ambientPtFxInfo.tagHashName, ambientPtFxInfo);
				}
				else if (phase==1)
				{
					VfxEntityCollisionPtFxInfo_s collisionPtFxInfo;

					// generic ptfx data
					LoadGenericPtFxData(token, &collisionPtFxInfo, charBuff);

					// play at collision point
					s32 collPt = token.GetInt();
					collisionPtFxInfo.atCollisionPt = collPt>0;

					// collision threshold
					collisionPtFxInfo.collImpThreshMin = token.GetFloat();
					collisionPtFxInfo.collImpThreshMax = token.GetFloat();

					if (collisionPtFxInfo.collImpThreshMin<0.0f)
					{
						collisionPtFxInfo.collImpThreshMin = FLT_MAX;
						collisionPtFxInfo.collImpThreshMax = FLT_MAX;
					}

					vfxAssertf(collisionPtFxInfo.collImpThreshMin <=
						collisionPtFxInfo.collImpThreshMax, "min impulse threshold is greater than max");

					m_collisionPtFxInfoMap.Insert(collisionPtFxInfo.tagHashName, collisionPtFxInfo);
				}
				else if (phase==2)
				{	
					VfxEntityShotPtFxInfo_s shotPtFxInfo;

					// generic ptfx data
					LoadGenericPtFxData(token, &shotPtFxInfo, charBuff);

					// play at collision point
					s32 collPt = token.GetInt();
					shotPtFxInfo.atCollisionPt = collPt>0;

					m_shotPtFxInfoMap.Insert(shotPtFxInfo.tagHashName, shotPtFxInfo);
				}
				else if (phase==3)
				{	
					VfxEntityBreakPtFxInfo_s breakPtFxInfo;

					// generic ptfx data
					LoadGenericPtFxData(token, &breakPtFxInfo, charBuff);

					m_breakPtFxInfoMap.Insert(breakPtFxInfo.tagHashName, breakPtFxInfo);
				}
				else if (phase==4)
				{	
					VfxEntityDestroyPtFxInfo_s destroyPtFxInfo;

					// generic ptfx data
					LoadGenericPtFxData(token, &destroyPtFxInfo, charBuff);

					// no trigger explosion range
					destroyPtFxInfo.noTriggerExpRange = token.GetFloat();

					m_destroyPtFxInfoMap.Insert(destroyPtFxInfo.tagHashName, destroyPtFxInfo);
				}
				else if (phase==5)
				{	
					VfxEntityAnimPtFxInfo_s animPtFxInfo;

					// generic ptfx data
					LoadGenericPtFxData(token, &animPtFxInfo, charBuff);

					m_animPtFxInfoMap.Insert(animPtFxInfo.tagHashName, animPtFxInfo);
				}
				else if (phase==6)
				{	
					VfxEntityRayFirePtFxInfo_s rayfirePtFxInfo;

					// generic ptfx data
					LoadGenericPtFxData(token, &rayfirePtFxInfo, charBuff);

					m_rayfirePtFxInfoMap.Insert(rayfirePtFxInfo.tagHashName, rayfirePtFxInfo);
				}
				else if (phase==7)
				{	
					VfxEntityInWaterPtFxInfo_s inWaterPtFxInfo;

					// generic ptfx data
					LoadGenericPtFxData(token, &inWaterPtFxInfo, charBuff);

					// speed up threshold
					if (m_version>=6.0f)
					{
						inWaterPtFxInfo.speedUpThreshMin = token.GetFloat();
						inWaterPtFxInfo.speedUpThreshMax = token.GetFloat();
					}
					else
					{
						inWaterPtFxInfo.speedUpThreshMin = 0.0f;
						inWaterPtFxInfo.speedUpThreshMax = 0.0f;
					}

					m_inWaterPtFxInfoMap.Insert(inWaterPtFxInfo.tagHashName, inWaterPtFxInfo);
				}
				else if (phase==8)
				{
					VfxEntityCollisionDecalInfo_s collisionDecalInfo;

					// generic decal data
					LoadGenericDecalData(token, &collisionDecalInfo, charBuff);

					// play at collision point
					s32 collPt = token.GetInt();
					collisionDecalInfo.atCollisionPt = collPt>0;

					// collision threshold
					collisionDecalInfo.collImpThreshMin = token.GetFloat();
					collisionDecalInfo.collImpThreshMax = token.GetFloat();

					if (collisionDecalInfo.collImpThreshMin<0.0f)
					{
						collisionDecalInfo.collImpThreshMin = FLT_MAX;
						collisionDecalInfo.collImpThreshMax = FLT_MAX;
					}

					vfxAssertf(collisionDecalInfo.collImpThreshMin <=
						collisionDecalInfo.collImpThreshMax, "min impulse threshold is greater than max");

					m_collisionDecalInfoMap.Insert(collisionDecalInfo.tagHashName, collisionDecalInfo);
				}
				else if (phase==9)
				{
					VfxEntityShotDecalInfo_s shotDecalInfo;
	
					// generic decal data
					LoadGenericDecalData(token, &shotDecalInfo, charBuff);

					// play at collision point
					s32 collPt = token.GetInt();
					shotDecalInfo.atCollisionPt = collPt>0;

					m_shotDecalInfoMap.Insert(shotDecalInfo.tagHashName, shotDecalInfo);
				}
				else if (phase==10)
				{
					VfxEntityBreakDecalInfo_s breakDecalInfo;

					// generic decal data
					LoadGenericDecalData(token, &breakDecalInfo, charBuff);

					m_breakDecalInfoMap.Insert(breakDecalInfo.tagHashName, breakDecalInfo);
				}
				else if (phase==11)
				{
					VfxEntityDestroyDecalInfo_s destroyDecalInfo;

					// generic decal data
					LoadGenericDecalData(token, &destroyDecalInfo, charBuff);

					m_destroyDecalInfoMap.Insert(destroyDecalInfo.tagHashName, destroyDecalInfo);
				}
			}

			stream->Close();
		}

		// get next file
		pData = DATAFILEMGR.GetPrevFile(pData);
	}

	m_ambientPtFxInfoMap.FinishInsertion();
	m_collisionPtFxInfoMap.FinishInsertion();
	m_shotPtFxInfoMap.FinishInsertion();
	m_breakPtFxInfoMap.FinishInsertion();
	m_destroyPtFxInfoMap.FinishInsertion();
	m_animPtFxInfoMap.FinishInsertion();
	m_rayfirePtFxInfoMap.FinishInsertion();
	m_inWaterPtFxInfoMap.FinishInsertion();

	m_collisionDecalInfoMap.FinishInsertion();
	m_shotDecalInfoMap.FinishInsertion();
	m_breakDecalInfoMap.FinishInsertion();
	m_destroyDecalInfoMap.FinishInsertion();
}


///////////////////////////////////////////////////////////////////////////////
// LoadGenericPtFxData
///////////////////////////////////////////////////////////////////////////////

void			CVfxEntity::LoadGenericPtFxData				(fiAsciiTokenizer& token, VfxEntityGenericPtFxInfo_s* pGenericPtFxInfo, char* pTagName)
{
	// tag
	pGenericPtFxInfo->tagHashName = atHashWithStringNotFinal(pTagName);
#if __BANK
	strcpy(pGenericPtFxInfo->tagName, pTagName);
#endif

	// ptfx asset name
	char charBuff[128];
	if (m_version>=4.0f)
	{
		token.GetToken(charBuff, 128);
		pGenericPtFxInfo->ptFxAssetHashName = atHashWithStringNotFinal(charBuff);
	}	

	// particle data
	token.GetToken(charBuff, 128);		
	pGenericPtFxInfo->ptFxHashName = atHashWithStringNotFinal(charBuff);
}


///////////////////////////////////////////////////////////////////////////////
// LoadGenericDecalData
///////////////////////////////////////////////////////////////////////////////

void			CVfxEntity::LoadGenericDecalData			(fiAsciiTokenizer& token, VfxEntityGenericDecalInfo_s* pGenericDecalInfo, char* pTagName)
{
	// tag
	pGenericDecalInfo->tagHashName = atHashWithStringNotFinal(pTagName);
#if __BANK
	strcpy(pGenericDecalInfo->tagName, pTagName);
#endif

	// decal data
	s32 decalDataId = token.GetInt();
	g_decalMan.FindRenderSettingInfo(decalDataId, pGenericDecalInfo->renderSettingIndex, pGenericDecalInfo->renderSettingCount);

	pGenericDecalInfo->sizeMin = token.GetFloat();
	pGenericDecalInfo->sizeMax = token.GetFloat();
	pGenericDecalInfo->lifeMin = token.GetFloat();
	pGenericDecalInfo->lifeMax = token.GetFloat();
	pGenericDecalInfo->fadeInTime = token.GetFloat();
	if (m_version>=3.0f)
	{
		pGenericDecalInfo->colR = (u8)token.GetInt();
		pGenericDecalInfo->colG = (u8)token.GetInt();
		pGenericDecalInfo->colB = (u8)token.GetInt();
	}
	else
	{
		pGenericDecalInfo->colR = 255;
		pGenericDecalInfo->colG = 255;
		pGenericDecalInfo->colB = 255;
	}
	pGenericDecalInfo->alphaMin = token.GetFloat();
	pGenericDecalInfo->alphaMax = token.GetFloat();
	pGenericDecalInfo->uvMultTime = token.GetFloat();
	pGenericDecalInfo->uvMultStart = token.GetFloat();
	pGenericDecalInfo->uvMultEnd = token.GetFloat();
	pGenericDecalInfo->numProbes = token.GetInt();

	float probeDirX = token.GetFloat();
	float probeDirY = token.GetFloat();
	float probeDirZ = token.GetFloat();
	pGenericDecalInfo->vProbeDir = Vec3V(probeDirX, probeDirY, probeDirZ);
	if (IsZeroAll(pGenericDecalInfo->vProbeDir)==false)
	{
		pGenericDecalInfo->vProbeDir = Normalize(pGenericDecalInfo->vProbeDir);
	}

	pGenericDecalInfo->probeRadiusMin = token.GetFloat();
	pGenericDecalInfo->probeRadiusMax = token.GetFloat();
	pGenericDecalInfo->probeDist = token.GetFloat();
}


///////////////////////////////////////////////////////////////////////////////
// InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CVfxEntity::InitWidgets						()
{
	if (m_bankInitialised==false)
	{
		m_renderAmbientDebug = false;
		m_renderEntityDecalProbes = false;
		m_onlyProcessThisTag = false;
		sprintf(m_onlyProcessThisTagName, "%s", "");

		bkBank* pVfxBank = vfxWidget::GetBank();
		pVfxBank->PushGroup("Vfx Entity", false);
		{
			pVfxBank->AddTitle("INFO");

#if __DEV
			pVfxBank->AddTitle("");
			pVfxBank->AddTitle("SETTINGS");
			pVfxBank->AddSlider("Coln History Dist", &ENTITYFX_COLL_HIST_DIST, 0.0f, 100.0f, 0.25f, NullCB, "The distance at which a new collision effect will be rejected if a history exists within this range");
			pVfxBank->AddSlider("Coln History Time", &ENTITYFX_COLL_HIST_TIME, 0.0f, 100.0f, 0.25f, NullCB, "The time that a history will exist for a new collision effect");
			pVfxBank->AddSlider("Shot History Dist", &ENTITYFX_SHOT_HIST_DIST, 0.0f, 100.0f, 0.25f, NullCB, "The distance at which a new shot effect will be rejected if a history exists within this range");
			pVfxBank->AddSlider("Shot History Time", &ENTITYFX_SHOT_HIST_TIME, 0.0f, 100.0f, 0.25f, NullCB, "The time that a history will exist for a new shot effect");
			pVfxBank->AddSlider("In Water History Dist", &ENTITYFX_INWATER_HIST_DIST, 0.0f, 100.0f, 0.25f, NullCB, "The distance at which a new 'in water' effect will be rejected if a history exists within this range");
			pVfxBank->AddSlider("In Water History Time", &ENTITYFX_INWATER_HIST_TIME, 0.0f, 100.0f, 0.25f, NullCB, "The time that a history will exist for a new 'in water' effect");
#endif

			pVfxBank->AddTitle("");
			pVfxBank->AddTitle("DEBUG");
			pVfxBank->AddToggle("Render Ambient Debug", &m_renderAmbientDebug);
			pVfxBank->AddToggle("Render Entity Decal Probes", &m_renderEntityDecalProbes);
			pVfxBank->AddToggle("Only Play This Tag", &m_onlyProcessThisTag);
			pVfxBank->AddText("Only Play This Tag Name", m_onlyProcessThisTagName,64);

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
void			CVfxEntity::Reset							()
{
	g_vfxEntity.InitDefinitions();
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  CalcAmbientProbability
///////////////////////////////////////////////////////////////////////////////

float				CVfxEntity::CalcAmbientProbability(const CParticleAttr* pPtFxAttr, VfxEntityAmbientPtFxInfo_s* pAmbientInfo)
{
	// work out the current probability based on the time of day
	float prob = (float)pPtFxAttr->m_probability;
	if (pAmbientInfo->nightProbMult!=1.0f)
	{
		s32 clockMins = (CClock::GetHour()*60) + CClock::GetMinute();
		if (clockMins<=6*60 || clockMins>=20*60)
		{
			// before 6am and after 8pm we use the night time probability multiplier in full
			prob *= pAmbientInfo->nightProbMult;
		}
		else if (clockMins<8*60)
		{
			vfxAssertf(clockMins>6*60, "clock logic error found");
			// between 6am and 8am - interpolate accordingly
			float d = (clockMins-(6.0f*60.0f))/120.0f;
			float probMult = pAmbientInfo->nightProbMult + d*(1.0f-pAmbientInfo->nightProbMult);
			prob *= probMult;
		}
		else if (clockMins>18*60)
		{
			vfxAssertf(clockMins<20*60, "clock logic error found");
			// between 6pm and 8pm - interpolate accordingly
			float d = (clockMins-(18.0f*60.0f))/120.0f;
			float probMult = 1.0f + d*(pAmbientInfo->nightProbMult-1.0f);
			prob *= probMult;
		}
		else
		{
			// normal prob between 8am and 6pm
		}
	}

	return prob;
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessPtFxEntityAmbient
///////////////////////////////////////////////////////////////////////////////

void				CVfxEntity::ProcessPtFxEntityAmbient	(CEntity* pEntity, const CParticleAttr* pPtFxAttr, s32 effectId)
{
	if (pPtFxAttr->m_boneTag<0)
	{
		vfxAssertf(0, "called with invalid bone tag (%s)", pEntity->GetModelName());
		return;
	}

	if (pEntity->GetDrawable()==NULL)
	{
		return;
	}

	VfxEntityAmbientPtFxInfo_s* pAmbientInfo = GetAmbientPtFxInfo(pPtFxAttr->m_tagHashName);
	if (pAmbientInfo)
	{
		if (pAmbientInfo->isTriggered)
		{
			TriggerPtFxEntityAmbient(pEntity, pPtFxAttr, effectId, pAmbientInfo);
		}
		else
		{
			UpdatePtFxEntityAmbient(pEntity, pPtFxAttr, effectId, pAmbientInfo);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxEntityAmbient
///////////////////////////////////////////////////////////////////////////////

void				CVfxEntity::UpdatePtFxEntityAmbient		(CEntity* pEntity, const CParticleAttr* pPtFxAttr, s32 effectId, VfxEntityAmbientPtFxInfo_s* pAmbientInfo)
{
	float prob = CalcAmbientProbability(pPtFxAttr, pAmbientInfo);

	Vec3V vEffectPos;
	pPtFxAttr->GetPos(RC_VECTOR3(vEffectPos));

	vfxAssertf(IsFiniteAll(vEffectPos), "vEffectPos is not finite");

	if (ShouldUpdate(pPtFxAttr->m_tagHashName, (int)prob, pEntity, vEffectPos, effectId)==false)
	{
		return;
	}

	// check if this ambient effect needs to be created
	float currTemperature = g_weather.GetTemperature(vEffectPos);
	if (pAmbientInfo->alwaysOn || (g_weather.GetSun()>=pAmbientInfo->sunThresh && 
									g_weather.GetTimeCycleAdjustedRain()>=pAmbientInfo->rainThresh && 
									currTemperature>=pAmbientInfo->tempMin && 
									currTemperature<=pAmbientInfo->tempMax && 
									CClock::IsTimeInRange(int(pAmbientInfo->timeMin), int(pAmbientInfo->timeMax))))
	{
		// get the matrix to attach the fx to
		Mat34V boneMtx;
		s32 boneIndex = CVfxHelper::GetMatrixFromBoneTag(boneMtx, pEntity, (eAnimBoneTag)pPtFxAttr->m_boneTag);
		
		vfxAssertf(IsFiniteAll(boneMtx), "boneMtx is not finite (%s - %d - %d)", pEntity->GetDebugName(), pPtFxAttr->m_boneTag, boneIndex);

		// return if we don't have a valid matrix
		if (IsZeroAll(boneMtx.GetCol0()))
		{
			return;
		}

		// get access to the effect rule
		ptxEffectRule* pEffectRule = ptfxWrapper::GetEffectRule(pAmbientInfo->ptFxHashName);
		if (pEffectRule)
		{
			// get the final position where this is going to be 
			Vec3V vFxPos;
			vFxPos = Transform3x3(boneMtx, vEffectPos);
			vFxPos = vFxPos + boneMtx.GetCol3(); 

			// check the wind value is greater than the threshold
			Vec3V vWindVel;
			WIND.GetLocalVelocity(vFxPos, vWindVel);
			float localWindSpeed = CVfxHelper::GetInterpValue(Mag(vWindVel).Getf(), 0.0f, pAmbientInfo->maxWindEvo);
			if (localWindSpeed>=pAmbientInfo->windThresh || g_weather.GetWind()>=pAmbientInfo->windThresh)
			{
				// get closest active camera to fxPos (and the distance to it)
				float fxDistSqr = CVfxHelper::GetDistSqrToCamera(vFxPos);
#if __BANK
				if (m_renderAmbientDebug)
				{
					grcDebugDraw::Sphere(RCC_VECTOR3(vFxPos), 0.20f, Color32(1.0f, 0.0f, 0.0f, 1.0f));
					grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(boneMtx.GetCol3()), 0.5f, Color32(1.0f, 1.0f, 0.0f, 1.0f));
					grcDebugDraw::Line(RCC_VECTOR3(vFxPos), VEC3V_TO_VECTOR3(boneMtx.GetCol3()), Color32(1.0f, 1.0f, 1.0f, 1.0f));
				}
#endif
				// continue if this fx system is in range
				if (IsPtFxInRange(pEffectRule, fxDistSqr))
				{
#if __BANK
					if (m_renderAmbientDebug)
					{
						grcDebugDraw::Sphere(RCC_VECTOR3(vFxPos), 0.25f, Color32(0.0f, 1.0f, 0.0f, 1.0f));
					}
#endif

					// try to create the fx system
					bool justCreated;
					ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pEntity, effectId, true, pAmbientInfo->ptFxHashName, justCreated);
					if (pFxInst)
					{
						ptfxRegInst* pRegFx = ptfxReg::Find(pEntity, effectId);
						vfxAssertf(pRegFx, "cannot find registered effect");

						// fx system created ok - start it up and set its position and orientation
						ptfxAttach::Attach(pFxInst, pEntity, boneIndex);

						float fxLevel = CPortal::GetInteriorFxLevel();

						// set any evolution variables
						if (pAmbientInfo->hasRainEvo)
						{
							float rainEvo = 0.0f;
							if (fxLevel==1.0f)
							{
								// only use rain evo if outside a building
								rainEvo = CVfxHelper::GetInterpValue(g_weather.GetTimeCycleAdjustedRain(), 0.0f, 1.0f);
							}
							pFxInst->SetEvoValueFromHash(ATSTRINGHASH("rain", 0x54A69840), rainEvo);
						}

						if (pAmbientInfo->hasWindEvo)
						{
							float windEvo = 0.0f;
							if (fxLevel==1.0f)
							{
								// only use wind evo if outside a building
								float windSpeed = Max(localWindSpeed, g_weather.GetWind());
								windEvo = CVfxHelper::GetInterpValue(windSpeed, 0.0f, 1.0f);
							}
							pFxInst->SetEvoValueFromHash(ATSTRINGHASH("wind", 0x35369828), windEvo);
						}

						if (pAmbientInfo->tempEvoMin!=0.0f || pAmbientInfo->tempEvoMax!=0.0f)
						{
							float tempEvo = CVfxHelper::GetInterpValue(currTemperature, pAmbientInfo->tempEvoMin, pAmbientInfo->tempEvoMax);
							pFxInst->SetEvoValueFromHash(ATSTRINGHASH("temp", 0x6CE0A51A), tempEvo);
						}

						if (pEntity && pEntity->GetIsPhysical())
						{
							CPhysical* pPhysical = static_cast<CPhysical*>(pEntity);
							ptfxWrapper::SetVelocityAdd(pFxInst, RCC_VEC3V(pPhysical->GetVelocity()));
						}

						if (justCreated)
						{
							// fx system created ok - start it up and set its position and orientation
							pFxInst->SetOffsetPos(vEffectPos);

							if (!pAmbientInfo->ignoreRotation)
							{
								pFxInst->SetOffsetRot(QuatV(pPtFxAttr->qX, pPtFxAttr->qY, pPtFxAttr->qZ, pPtFxAttr->qW));
							}

							if (pPtFxAttr->m_hasTint)
							{
								Vec3V vColourTint(pPtFxAttr->m_tintR/255.0f, pPtFxAttr->m_tintG/255.0f, pPtFxAttr->m_tintB/255.0f);
								ptfxWrapper::SetColourTint(pFxInst, vColourTint);
								ptfxWrapper::SetAlphaTint(pFxInst, pPtFxAttr->m_tintA/255.0f);
							}

							pFxInst->SetUserZoom(pPtFxAttr->m_scale);	
							pFxInst->Start();							

							// only check for audio once when this effect is first created
							pRegFx->m_hasAudio = (SOUNDFACTORY.GetMetadataPtr(pAmbientInfo->ptFxHashName)!=NULL);
						}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
						else
						{
							// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
							vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "effect attachment error");
						}
#endif

						if (pRegFx->m_hasAudio)
						{
							fwUniqueObjId effectUniqueID = fwIdKeyGenerator::Get(pEntity, effectId);
							g_AmbientAudioEntity.RegisterEffectAudio(pAmbientInfo->ptFxHashName, effectUniqueID, RCC_VECTOR3(vFxPos), pEntity);
						}
					}
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxEntityAmbient
///////////////////////////////////////////////////////////////////////////////

void				CVfxEntity::TriggerPtFxEntityAmbient	(CEntity* pEntity, const CParticleAttr* pPtFxAttr, s32 effectId, VfxEntityAmbientPtFxInfo_s* pAmbientInfo)
{
	float prob = CalcAmbientProbability(pPtFxAttr, pAmbientInfo);

	Vec3V vEffectPos;
	pPtFxAttr->GetPos(RC_VECTOR3(vEffectPos));

	if (ShouldUpdate(pPtFxAttr->m_tagHashName, (int)prob, pEntity, vEffectPos, effectId)==false)
	{
		return;
	}

	// check if this ambient effect needs to be created
	float currTemperature = g_weather.GetTemperature(vEffectPos);
	if (pAmbientInfo->alwaysOn || (g_weather.GetSun()>=pAmbientInfo->sunThresh && 
		g_weather.GetTimeCycleAdjustedRain()>=pAmbientInfo->rainThresh && 
		currTemperature>=pAmbientInfo->tempMin && 
		currTemperature<=pAmbientInfo->tempMax && 
		CClock::IsTimeInRange(int(pAmbientInfo->timeMin), int(pAmbientInfo->timeMax))))
	{
		// get the matrix to attach the fx to
		Mat34V boneMtx;
		s32 boneIndex = CVfxHelper::GetMatrixFromBoneTag(boneMtx, pEntity, (eAnimBoneTag)pPtFxAttr->m_boneTag);

		// return if we don't have a valid matrix
		if (IsZeroAll(boneMtx.GetCol0()))
		{
			return;
		}

		// get access to the effect rule
		ptxEffectRule* pEffectRule = ptfxWrapper::GetEffectRule(pAmbientInfo->ptFxHashName);
		if (pEffectRule)
		{
			// get the final position where this is going to be 
			Vec3V vFxPos;
			vFxPos = Transform3x3(boneMtx, vEffectPos);
			vFxPos = vFxPos + boneMtx.GetCol3(); 

			// check the wind value is greater than the threshold
			Vec3V vWindVel;
			WIND.GetLocalVelocity(vFxPos, vWindVel);
			float localWindSpeed = CVfxHelper::GetInterpValue(Mag(vWindVel).Getf(), 0.0f, pAmbientInfo->maxWindEvo);
			if (localWindSpeed>=pAmbientInfo->windThresh || g_weather.GetWind()>=pAmbientInfo->windThresh)
			{
				// get closest active camera to fxPos (and the distance to it)
				float fxDistSqr = CVfxHelper::GetDistSqrToCamera(vFxPos);
#if __BANK
				if (m_renderAmbientDebug)
				{
					grcDebugDraw::Sphere(RCC_VECTOR3(vFxPos), 0.20f, Color32(1.0f, 0.0f, 0.0f, 1.0f));
					grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(boneMtx.GetCol3()), 0.5f, Color32(1.0f, 1.0f, 0.0f, 1.0f));
					grcDebugDraw::Line(RCC_VECTOR3(vFxPos), VEC3V_TO_VECTOR3(boneMtx.GetCol3()), Color32(1.0f, 1.0f, 1.0f, 1.0f));
				}
#endif
				// continue if this fx system is in range
				if (IsPtFxInRange(pEffectRule, fxDistSqr))
				{
					if (ptfxHistory::Check(pAmbientInfo->ptFxHashName, 0, pEntity, vFxPos, 1.0f))
					{
						return;
					}

#if __BANK
					if (m_renderAmbientDebug)
					{
						grcDebugDraw::Sphere(RCC_VECTOR3(vFxPos), 0.25f, Color32(0.0f, 1.0f, 0.0f, 1.0f));
					}
#endif

					// try to create the fx system
					ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pAmbientInfo->ptFxHashName);
					if (pFxInst)
					{
						// fx system created ok - start it up and set its position and orientation
						ptfxAttach::Attach(pFxInst, pEntity, boneIndex);

						float fxLevel = CPortal::GetInteriorFxLevel();

						// set any evolution variables
						if (pAmbientInfo->hasRainEvo)
						{
							float rainEvo = 0.0f;
							if (fxLevel==1.0f)
							{
								// only use rain evo if outside a building
								rainEvo = CVfxHelper::GetInterpValue(g_weather.GetTimeCycleAdjustedRain(), 0.0f, 1.0f);
							}
							pFxInst->SetEvoValueFromHash(ATSTRINGHASH("rain", 0x54A69840), rainEvo);
						}

						if (pAmbientInfo->hasWindEvo)
						{
							float windEvo = 0.0f;
							if (fxLevel==1.0f)
							{
								// only use wind evo if outside a building
								float windSpeed = Max(localWindSpeed, g_weather.GetWind());
								windEvo = CVfxHelper::GetInterpValue(windSpeed, 0.0f, 1.0f);
							}
							pFxInst->SetEvoValueFromHash(ATSTRINGHASH("wind", 0x35369828), windEvo);
						}

						if (pAmbientInfo->tempEvoMin!=0.0f || pAmbientInfo->tempEvoMax!=0.0f)
						{
							float tempEvo = CVfxHelper::GetInterpValue(currTemperature, pAmbientInfo->tempEvoMin, pAmbientInfo->tempEvoMax);
							pFxInst->SetEvoValueFromHash(ATSTRINGHASH("temp", 0x6CE0A51A), tempEvo);
						}

						if (pEntity && pEntity->GetIsPhysical())
						{
							CPhysical* pPhysical = static_cast<CPhysical*>(pEntity);
							ptfxWrapper::SetVelocityAdd(pFxInst, RCC_VEC3V(pPhysical->GetVelocity()));
						}

						// fx system created ok - start it up and set its position and orientation
						pFxInst->SetOffsetPos(vEffectPos);

						if (!pAmbientInfo->ignoreRotation)
						{
							pFxInst->SetOffsetRot(QuatV(pPtFxAttr->qX, pPtFxAttr->qY, pPtFxAttr->qZ, pPtFxAttr->qW));
						}

						if (pPtFxAttr->m_hasTint)
						{
							Vec3V vColourTint(pPtFxAttr->m_tintR/255.0f, pPtFxAttr->m_tintG/255.0f, pPtFxAttr->m_tintB/255.0f);
							ptfxWrapper::SetColourTint(pFxInst, vColourTint);
							ptfxWrapper::SetAlphaTint(pFxInst, pPtFxAttr->m_tintA/255.0f);
						}

						pFxInst->SetUserZoom(pPtFxAttr->m_scale);	
						pFxInst->Start();	

						float historyTime = g_DrawRand.GetRanged(pAmbientInfo->triggerTimeMin, pAmbientInfo->triggerTimeMax);
						ptfxHistory::Add(pAmbientInfo->ptFxHashName, 0, pEntity, vFxPos, historyTime);
					}
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxEntityCollision
///////////////////////////////////////////////////////////////////////////////

void				CVfxEntity::TriggerPtFxEntityCollision		(CEntity* pEntity, const CParticleAttr* pPtFxAttr, Vec3V_In vPos, Vec3V_In vNormal, s32 collidedBoneTag, float accumImpulse)
{
	if (ShouldTrigger(pPtFxAttr->m_tagHashName, pPtFxAttr->m_probability)==false)
	{
		return;
	}

	Vec3V vEffectPos;
	pPtFxAttr->GetPos(RC_VECTOR3(vEffectPos));

	VfxEntityCollisionPtFxInfo_s* pCollisionInfo = GetCollisionPtFxInfo(pPtFxAttr->m_tagHashName);
	if (pCollisionInfo)
	{
		if (accumImpulse>=pCollisionInfo->collImpThreshMin)
		{
			// get the matrix to attach the fx to
			Mat34V boneMtx;
			s32 boneIndex = CVfxHelper::GetMatrixFromBoneTag(boneMtx, pEntity, (eAnimBoneTag)collidedBoneTag);

			// return if we don't have a valid matrix
			if (IsZeroAll(boneMtx.GetCol0()))
			{
				return;
			}

			// calc the evolution time
			float impulseEvo = CVfxHelper::GetInterpValue(accumImpulse, pCollisionInfo->collImpThreshMin, pCollisionInfo->collImpThreshMax);

			// get access to the effect rule
			ptxEffectRule* pEffectRule = ptfxWrapper::GetEffectRule(pCollisionInfo->ptFxHashName);
			if (pEffectRule)
			{
				// get the final position where this is going to be 
				Vec3V vFxPos = vPos;
				if (pCollisionInfo->atCollisionPt==false)
				{
					vFxPos = Transform3x3(boneMtx, vEffectPos);
					vFxPos = vFxPos + boneMtx.GetCol3(); 
				}

				// get closest active camera to pos (and the distance to it)
				float fxDistSqr = CVfxHelper::GetDistSqrToCamera(vFxPos);

				// continue if this fx system is in range
				if (IsPtFxInRange(pEffectRule, fxDistSqr))
				{
					if (ptfxHistory::Check(pCollisionInfo->ptFxHashName, 0, pEntity, vFxPos, ENTITYFX_COLL_HIST_DIST))
					{
						return;
					}

					// try to create the fx system
					ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pCollisionInfo->ptFxHashName);
					if (pFxInst)
					{
						// fx system created ok - start it up and set its matrix
						ptfxAttach::Attach(pFxInst, pEntity, boneIndex);

						if (pCollisionInfo->atCollisionPt)
						{
							Mat34V fxMat;
							Vec3V vNegNormal = -vNormal;
							CVfxHelper::CreateMatFromVecZ(fxMat, vFxPos, vNegNormal);

							// make the fx matrix relative to the parent matrix
							Mat34V relFxMat;
							CVfxHelper::CreateRelativeMat(relFxMat, boneMtx, fxMat);

							pFxInst->SetOffsetMtx(relFxMat);
						}
						else
						{
							pFxInst->SetOffsetPos(vEffectPos);
							pFxInst->SetOffsetRot(QuatV(pPtFxAttr->qX, pPtFxAttr->qY, pPtFxAttr->qZ, pPtFxAttr->qW));
						}

						pFxInst->SetEvoValueFromHash(ATSTRINGHASH("impulse", 0xB05D6C92), impulseEvo);
						pFxInst->SetUserZoom(pPtFxAttr->m_scale);

						if (pPtFxAttr->m_hasTint)
						{
							Vec3V vColourTint(pPtFxAttr->m_tintR/255.0f, pPtFxAttr->m_tintG/255.0f, pPtFxAttr->m_tintB/255.0f);
							ptfxWrapper::SetColourTint(pFxInst, vColourTint);
							ptfxWrapper::SetAlphaTint(pFxInst, pPtFxAttr->m_tintA/255.0f);
						}

						pFxInst->Start();

						ptfxHistory::Add(pCollisionInfo->ptFxHashName, 0, pEntity, vFxPos, ENTITYFX_COLL_HIST_TIME);
					}
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxEntityShot
///////////////////////////////////////////////////////////////////////////////

void				CVfxEntity::TriggerPtFxEntityShot			(CEntity* pEntity, const CParticleAttr* pPtFxAttr, Vec3V_In vPos, Vec3V_In vNormal, s32 shotBoneTag)
{
	if (ShouldTrigger(pPtFxAttr->m_tagHashName, pPtFxAttr->m_probability)==false)
	{
		return;
	}

	Vec3V vEffectPos;
	pPtFxAttr->GetPos(RC_VECTOR3(vEffectPos));

	VfxEntityShotPtFxInfo_s* pShotInfo = GetShotPtFxInfo(pPtFxAttr->m_tagHashName);
	if (pShotInfo)
	{
		// get the matrix to attach the fx to
		Mat34V boneMtx = Mat34V(V_IDENTITY);
		s32 boneIndex = -1;
		if (pEntity)
		{
			boneIndex = CVfxHelper::GetMatrixFromBoneTag(boneMtx, pEntity, (eAnimBoneTag)shotBoneTag);
		}

		// return if we don't have a valid matrix
		if (IsZeroAll(boneMtx.GetCol0()))
		{
			return;
		}

		// get access to the effect rule
		ptxEffectRule* pEffectRule = ptfxWrapper::GetEffectRule(pShotInfo->ptFxHashName);
		if (pEffectRule)
		{
			// get the final position where this is going to be 
			Vec3V vFxPos = vPos;
			if (pShotInfo->atCollisionPt==false)
			{
				vFxPos = Transform3x3(boneMtx, vEffectPos);
				vFxPos = vFxPos + boneMtx.GetCol3(); 
			}

			// get closest active camera to pos (and the distance to it)
			float fxDistSqr = CVfxHelper::GetDistSqrToCamera(vFxPos);

			// continue if this fx system is in range
			if (IsPtFxInRange(pEffectRule, fxDistSqr))
			{
				if (ptfxHistory::Check(pShotInfo->ptFxHashName, 0, pEntity, vFxPos, ENTITYFX_SHOT_HIST_DIST))
				{
					return;
				}

				// try to create the fx system
				ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pShotInfo->ptFxHashName);
				if (pFxInst)
				{
					// fx system created ok - start it up and set its matrix
					if (pEntity)
					{
						ptfxAttach::Attach(pFxInst, pEntity, boneIndex);
					}

					Mat34V relFxMat;
					if (pShotInfo->atCollisionPt)
					{
						Mat34V fxMat;
						CVfxHelper::CreateMatFromVecZ(fxMat, vFxPos, vNormal);

						// make the fx matrix relative to the entity						
						CVfxHelper::CreateRelativeMat(relFxMat, boneMtx, fxMat);

						pFxInst->SetOffsetMtx(relFxMat);
					}
					else
					{
						pFxInst->SetOffsetPos(vEffectPos);
						pFxInst->SetOffsetRot(QuatV(pPtFxAttr->qX, pPtFxAttr->qY, pPtFxAttr->qZ, pPtFxAttr->qW));
					}

					if (pPtFxAttr->m_hasTint)
					{
						Vec3V vColourTint(pPtFxAttr->m_tintR/255.0f, pPtFxAttr->m_tintG/255.0f, pPtFxAttr->m_tintB/255.0f);
						ptfxWrapper::SetColourTint(pFxInst, vColourTint);
						ptfxWrapper::SetAlphaTint(pFxInst, pPtFxAttr->m_tintA/255.0f);
					}

					pFxInst->SetUserZoom(pPtFxAttr->m_scale);	
					pFxInst->Start();

#if GTA_REPLAY
					if(CReplayMgr::ShouldRecord())
					{
						CReplayMgr::RecordFx<CPacketEntityShotFx>(
							CPacketEntityShotFx( pShotInfo->ptFxHashName, boneIndex, pShotInfo->atCollisionPt, relFxMat, vEffectPos, QuatV(pPtFxAttr->qX, pPtFxAttr->qY, pPtFxAttr->qZ, pPtFxAttr->qW)
							,pPtFxAttr->m_hasTint, pPtFxAttr->m_tintR, pPtFxAttr->m_tintG, pPtFxAttr->m_tintB, pPtFxAttr->m_tintA, pPtFxAttr->m_scale ), pEntity);
					}
#endif

					ptfxHistory::Add(pShotInfo->ptFxHashName, 0, pEntity, vFxPos, ENTITYFX_SHOT_HIST_TIME);

					// All one shot sounds triggered from the one shot vfx are tracking entities and invalidating the entity pointer in the sound.
					// By doing that we make sure that if the entity gets deleted and hence its track, the sound will stop. BUG 1450179
					audSoundInitParams initParams;
					initParams.Position = RCC_VECTOR3(vFxPos);
					g_AmbientAudioEntity.CreateDeferredSound(pShotInfo->ptFxHashName, pEntity, &initParams, pEntity != NULL, pEntity != NULL,pEntity != NULL);

					REPLAY_ONLY(CReplayMgr::ReplayRecordSound(pShotInfo->ptFxHashName, &initParams, NULL, pEntity, eAmbientSoundEntity));
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxFragmentBreak
///////////////////////////////////////////////////////////////////////////////

void				CVfxEntity::TriggerPtFxFragmentBreak		(CEntity* pEntity, const CParticleAttr* pPtFxAttr, s32 brokenBoneTag, s32 parentBoneTag)
{
	if (ShouldTrigger(pPtFxAttr->m_tagHashName, pPtFxAttr->m_probability)==false)
	{
		return;
	}

	Vec3V vEffectPos;
	pPtFxAttr->GetPos(RC_VECTOR3(vEffectPos));

	VfxEntityBreakPtFxInfo_s* pBreakInfo = GetBreakPtFxInfo(pPtFxAttr->m_tagHashName);
	if (pBreakInfo)
	{
		// get the matrix of the broken piece
		s32 attachBoneTag = brokenBoneTag;
		if (pPtFxAttr->m_playOnParent)
		{
			// attach to the parent instead
			attachBoneTag = parentBoneTag;
		}

		Mat34V boneMtx;
		s32 boneIndex = CVfxHelper::GetMatrixFromBoneTag(boneMtx, pEntity, (eAnimBoneTag)attachBoneTag);

		// return if we don't have a valid matrix
		if (IsZeroAll(boneMtx.GetCol0()))
		{
			return;
		}

		// get the final position where this is going to be 
		Vec3V vFxPos;
		if (pPtFxAttr->m_playOnParent)
		{
			vFxPos = boneMtx.GetCol3();
		}
		else
		{
			vFxPos = Transform3x3(boneMtx, vEffectPos);
			vFxPos = vFxPos + boneMtx.GetCol3(); 
		}

		// get access to the effect rule
		ptxEffectRule* pEffectRule = ptfxWrapper::GetEffectRule(pBreakInfo->ptFxHashName);
		if (pEffectRule)
		{
			// get closest active camera to fxPos (and the distance to it)
			float fxDistSqr = CVfxHelper::GetDistSqrToCamera(vFxPos);

			// continue if this fx system is in range
			if ((IsPtFxInRange(pEffectRule, fxDistSqr)))
			{
				// try to create the fx system
				ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pBreakInfo->ptFxHashName);
				if (pFxInst)
				{
					// fx system created ok - start it up and set its position and orientation
					ptfxAttach::Attach(pFxInst, pEntity, boneIndex);

					// play offset from the bone (unless we're playing on the parent)
					if (!pPtFxAttr->m_playOnParent)
					{
						pFxInst->SetOffsetPos(vEffectPos);
						pFxInst->SetOffsetRot(QuatV(pPtFxAttr->qX, pPtFxAttr->qY, pPtFxAttr->qZ, pPtFxAttr->qW));
					}

					pFxInst->SetUserZoom(pPtFxAttr->m_scale);	

					if (pPtFxAttr->m_hasTint)
					{
						Vec3V vColourTint(pPtFxAttr->m_tintR/255.0f, pPtFxAttr->m_tintG/255.0f, pPtFxAttr->m_tintB/255.0f);
						ptfxWrapper::SetColourTint(pFxInst, vColourTint);
						ptfxWrapper::SetAlphaTint(pFxInst, pPtFxAttr->m_tintA/255.0f);
					}

					pFxInst->Start();


#if GTA_REPLAY
					// Attach to replay here
					if(CReplayMgr::ShouldRecord())
					{
						CReplayMgr::RecordFx<CPacketEntityFragBreakFx>(
							CPacketEntityFragBreakFx(	pPtFxAttr->m_tagHashName, VEC3V_TO_VECTOR3(vEffectPos), Quaternion(pPtFxAttr->qX, pPtFxAttr->qY, pPtFxAttr->qZ, pPtFxAttr->qW), 
														1.0f, (u16)attachBoneTag, pPtFxAttr->m_playOnParent, pPtFxAttr->m_hasTint, pPtFxAttr->m_tintR, pPtFxAttr->m_tintG, pPtFxAttr->m_tintB), pEntity);
					}
#endif

					// trigger audio
					audSoundInitParams initParams;
					initParams.Position = RCC_VECTOR3(vFxPos);
					g_AmbientAudioEntity.CreateDeferredSound(pBreakInfo->ptFxHashName, pEntity, &initParams, pEntity != NULL, false);
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxFragmentDestroy
///////////////////////////////////////////////////////////////////////////////

void				CVfxEntity::TriggerPtFxFragmentDestroy		(CEntity* pEntity, const CParticleAttr* pPtFxAttr, Mat34V_In fxMat)
{
	if (ShouldTrigger(pPtFxAttr->m_tagHashName, pPtFxAttr->m_probability)==false)
	{
		return;
	}

	Vec3V vEffectPos;
	pPtFxAttr->GetPos(RC_VECTOR3(vEffectPos));

	VfxEntityDestroyPtFxInfo_s* pDestroyInfo = GetDestroyPtFxInfo(pPtFxAttr->m_tagHashName);
	if (pDestroyInfo)
	{
		// return if we don't have a valid matrix
		if (IsZeroAll(fxMat.GetCol0()))
		{
			return;
		}

		// get access to the effect rule
		ptxEffectRule* pEffectRule = ptfxWrapper::GetEffectRule(pDestroyInfo->ptFxHashName);
		if (pEffectRule)
		{
			// get the final position where this is going to be 
			Vec3V vFxPos;
			vFxPos = Transform3x3(fxMat, vEffectPos);
			vFxPos = vFxPos + fxMat.GetCol3(); 

			// get closest active camera to fxPos (and the distance to it)
			float fxDistSqr = CVfxHelper::GetDistSqrToCamera(vFxPos);

			// continue if this fx system is in range
			if (IsPtFxInRange(pEffectRule, fxDistSqr))
			{
				// don't play if too close to an explosion 
				if (pDestroyInfo->noTriggerExpRange>0.0f && CExplosionManager::TestForExplosionsInSphere(EXP_TAG_DONTCARE, RC_VECTOR3(vFxPos), pDestroyInfo->noTriggerExpRange))
				{
					return;
				}

				// try to create the fx system
				ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pDestroyInfo->ptFxHashName);
				if (pFxInst)
				{
					// fx system created ok - start it up and set its position and orientation
					pFxInst->SetBaseMtx(fxMat);

					// play offset from the bone 
					pFxInst->SetOffsetPos(vEffectPos);

					QuatV quat(pPtFxAttr->qX, pPtFxAttr->qY, pPtFxAttr->qZ, pPtFxAttr->qW);
					pFxInst->SetOffsetRot(quat);

					pFxInst->SetUserZoom(pPtFxAttr->m_scale);	

					
					u8 replayR = pPtFxAttr->m_tintR;
					u8 replayG = pPtFxAttr->m_tintG;
					u8 replayB = pPtFxAttr->m_tintB;
					bool replayHasTint = pPtFxAttr->m_hasTint;

					// colour tint
					if (pPtFxAttr->m_tagHashName==ATSTRINGHASH("DST_INFLATABLE", 0x8853B7AF))
					{
						Vec3V vColourTint(1.0f, 1.0f, 1.0f);

						u32 tintIndex = pEntity->GetTintIndex();
						if (tintIndex==0)
							vColourTint = Vec3V(0.77f, 0.58f, 0.00f);	// red 
						else if (tintIndex==1)
							vColourTint = Vec3V(0.90f, 0.14f, 0.18f);	// red 
						else if (tintIndex==2)
							vColourTint = Vec3V(0.12f, 0.59f, 0.85f);	// blue 
						else if (tintIndex==3)
							vColourTint = Vec3V(0.42f, 0.32f, 0.49f);	// purple 
						else if (tintIndex==4)
							vColourTint = Vec3V(0.42f, 0.32f, 0.49f);	// black 
						else if (tintIndex==5)
							vColourTint = Vec3V(1.00f, 1.00f, 1.00f);	// white 
						else if (tintIndex==6)
							vColourTint = Vec3V(0.50f, 0.50f, 0.50f);	// grey 
						else if (tintIndex==7)
							vColourTint = Vec3V(0.70f, 0.59f, 0.29f);	// yellow 
						else if (tintIndex==8)
							vColourTint = Vec3V(1.00f, 0.55f, 0.00f);	// orange 
						else if (tintIndex==9)
							vColourTint = Vec3V(0.31f, 0.58f, 0.24f);	// green 
						else if (tintIndex==10)
							vColourTint = Vec3V(0.93f, 0.02f, 0.55f);	// pink 
						else if (tintIndex==11)
							vColourTint = Vec3V(0.90f, 0.14f, 0.18f);	// blue 
						else if (tintIndex==12)
							vColourTint = Vec3V(0.22f, 0.22f, 0.22f);	// black 
						else if (tintIndex==13)
							vColourTint = Vec3V(0.89f, 0.45f, 0.15f);	// orange 
						else if (tintIndex==14)
							vColourTint = Vec3V(0.02f, 0.29f, 0.18f);	// green 
						else if (tintIndex==15)
							vColourTint = Vec3V(0.93f, 0.02f, 0.55f);	// pink 

						replayR = (u8)(vColourTint.GetXf() * 255.0f);
						replayG = (u8)(vColourTint.GetYf() * 255.0f);
						replayB = (u8)(vColourTint.GetZf() * 255.0f);
						replayHasTint = true;

						ptfxWrapper::SetColourTint(pFxInst, vColourTint);
					}
					else if (pPtFxAttr->m_hasTint)
					{
						Vec3V vColourTint(pPtFxAttr->m_tintR/255.0f, pPtFxAttr->m_tintG/255.0f, pPtFxAttr->m_tintB/255.0f);
						ptfxWrapper::SetColourTint(pFxInst, vColourTint);
						ptfxWrapper::SetAlphaTint(pFxInst, pPtFxAttr->m_tintA/255.0f);
					}

					pFxInst->Start();

#if GTA_REPLAY
					// Attach to replay here
					if(CReplayMgr::ShouldRecord())
					{
						CReplayMgr::RecordFx<CPacketPtFxFragmentDestroy>(
							CPacketPtFxFragmentDestroy(pDestroyInfo->ptFxHashName, fxMat, vEffectPos, quat, pPtFxAttr->m_scale, replayHasTint, replayR, replayG, replayB, pPtFxAttr->m_tintA ) );
					}
#endif

					// trigger audio
					audSoundInitParams initParams;
					initParams.Position = RCC_VECTOR3(vFxPos);
					u32 soundHash = pDestroyInfo->ptFxHashName;

					// HughL - Ugh, super hacky late fix for GTA5. The concrete destruction particle effect triggers very frequently when driving over 
					// small rocks in the countryside, and has a very noticeable bullet impact-y element that really doesn't suit this situation. Its too 
					// late to swap out the particle effect for another just for this one particular instance, so we're simply explicitly checking for the 
					// sound and changing it if you're in a car. This might affect shooting something when doing a drive-by, but thats much better than 
					// the alternative.
					if(soundHash == ATSTRINGHASH("ent_dst_concrete", 0x7F0A819E))
					{
						CPed* playerPed = FindPlayerPed();

						if(playerPed && playerPed->GetIsInVehicle())
						{
							soundHash = ATSTRINGHASH("ent_dst_concrete_vehicle", 0x734DC29);
						}
					}
					
					g_AmbientAudioEntity.CreateDeferredSound(soundHash, pEntity, &initParams, pEntity != NULL, false);
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxEntityAnim
///////////////////////////////////////////////////////////////////////////////

void				CVfxEntity::UpdatePtFxEntityAnim			(CEntity* pEntity, const CParticleAttr* pPtFxAttr, s32 effectId)
{
	if (pPtFxAttr->m_boneTag<0)
	{
		vfxAssertf(0, "called with invalid bone tag (%s)", pEntity->GetModelName());
		return;
	}

	Vec3V vEffectPos;
	pPtFxAttr->GetPos(RC_VECTOR3(vEffectPos));

	if (ShouldUpdate(pPtFxAttr->m_tagHashName, pPtFxAttr->m_probability, pEntity, vEffectPos, effectId)==false)
	{
		return;
	}

	VfxEntityAnimPtFxInfo_s* pAnimInfo = GetAnimPtFxInfo(pPtFxAttr->m_tagHashName);
	if (pAnimInfo)
	{
		// get the matrix to attach the fx to
		Mat34V boneMtx;
		s32 boneIndex = CVfxHelper::GetMatrixFromBoneTag(boneMtx, pEntity, (eAnimBoneTag)pPtFxAttr->m_boneTag);

		// return if we don't have a valid matrix
		if (IsZeroAll(boneMtx.GetCol0()))
		{
			return;
		}

		// get access to the effect rule
		ptxEffectRule* pEffectRule = ptfxWrapper::GetEffectRule(pAnimInfo->ptFxHashName);
		if (pEffectRule)
		{
			// get the final position where this is going to be 
			Vec3V vFxPos;
			vFxPos = Transform3x3(boneMtx, vEffectPos);
			vFxPos = vFxPos + boneMtx.GetCol3(); 

			// get closest active camera to fxPos (and the distance to it)
			float fxDistSqr = CVfxHelper::GetDistSqrToCamera(vFxPos);

			// continue if this fx system is in range
			if (IsPtFxInRange(pEffectRule, fxDistSqr))
			{
				// try to create the fx system
				bool justCreated;
				ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pEntity, effectId, true, pAnimInfo->ptFxHashName, justCreated);
				if (pFxInst)
				{
					ptfxRegInst* pRegFx = ptfxReg::Find(pEntity, effectId);
					vfxAssertf(pRegFx, "cannot find registered effect");

					// fx system created ok - start it up and set its position and orientation
					ptfxAttach::Attach(pFxInst, pEntity, boneIndex);

					if (justCreated)
					{
						pFxInst->SetOffsetPos(vEffectPos);
						pFxInst->SetOffsetRot(QuatV(pPtFxAttr->qX, pPtFxAttr->qY, pPtFxAttr->qZ, pPtFxAttr->qW));

						if (pPtFxAttr->m_hasTint)
						{
							Vec3V vColourTint(pPtFxAttr->m_tintR/255.0f, pPtFxAttr->m_tintG/255.0f, pPtFxAttr->m_tintB/255.0f);
							ptfxWrapper::SetColourTint(pFxInst, vColourTint);
							ptfxWrapper::SetAlphaTint(pFxInst, pPtFxAttr->m_tintA/255.0f);
						}

						pFxInst->SetUserZoom(pPtFxAttr->m_scale);	
						pFxInst->Start();

						pRegFx->m_hasAudio = SOUNDFACTORY.GetMetadataPtr(pAnimInfo->ptFxHashName) != NULL;
					}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
					else
					{
						// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
						vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "effect attachment error");
					}
#endif
					if (pRegFx->m_hasAudio)
					{
						fwUniqueObjId effectUniqueID = fwIdKeyGenerator::Get(pEntity, effectId);
						g_AmbientAudioEntity.RegisterEffectAudio(pAnimInfo->ptFxHashName, effectUniqueID, RCC_VECTOR3(vFxPos), pEntity);
					}

#if GTA_REPLAY
					if(CReplayMgr::ShouldRecord())
					{
						CReplayMgr::RecordFx<CPacketEntityAmbientFx>(CPacketEntityAmbientFx(pPtFxAttr, effectId, false) , pEntity);
					}
#endif
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxEntityAnim
///////////////////////////////////////////////////////////////////////////////

void				CVfxEntity::TriggerPtFxEntityAnim			(CEntity* pEntity, const CParticleAttr* pPtFxAttr)
{
	if (pPtFxAttr->m_boneTag<0)
	{
		vfxAssertf(0, "called with invalid bone tag (%s)", pEntity->GetModelName());
		return;
	}

	if (ShouldTrigger(pPtFxAttr->m_tagHashName, pPtFxAttr->m_probability)==false)
	{
		return;
	}

	Vec3V vEffectPos;
	pPtFxAttr->GetPos(RC_VECTOR3(vEffectPos));

	VfxEntityAnimPtFxInfo_s* pAnimInfo = GetAnimPtFxInfo(pPtFxAttr->m_tagHashName);
	if (pAnimInfo)
	{
		// get the matrix to attach the fx to
		Mat34V boneMtx;
		s32 boneIndex = CVfxHelper::GetMatrixFromBoneTag(boneMtx, pEntity, (eAnimBoneTag)pPtFxAttr->m_boneTag);

		// return if we don't have a valid matrix
		if (IsZeroAll(boneMtx.GetCol0()))
		{
			return;
		}

		// get access to the effect rule
		ptxEffectRule* pEffectRule = ptfxWrapper::GetEffectRule(pAnimInfo->ptFxHashName);
		if (pEffectRule)
		{
			// get the final position where this is going to be 
			Vec3V vFxPos;
			vFxPos = Transform3x3(boneMtx, vEffectPos);
			vFxPos = vFxPos + boneMtx.GetCol3(); 

			// get closest active camera to fxPos (and the distance to it)
			float fxDistSqr = CVfxHelper::GetDistSqrToCamera(vFxPos);

			// continue if this fx system is in range
			if (IsPtFxInRange(pEffectRule, fxDistSqr))
			{
				// try to create the fx system
				ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pAnimInfo->ptFxHashName);
				if (pFxInst)
				{
					// fx system created ok - start it up and set its position and orientation
					ptfxAttach::Attach(pFxInst, pEntity, boneIndex);

					pFxInst->SetOffsetPos(vEffectPos);
					pFxInst->SetOffsetRot(QuatV(pPtFxAttr->qX, pPtFxAttr->qY, pPtFxAttr->qZ, pPtFxAttr->qW));

					if (pPtFxAttr->m_hasTint)
					{
						Vec3V vColourTint(pPtFxAttr->m_tintR/255.0f, pPtFxAttr->m_tintG/255.0f, pPtFxAttr->m_tintB/255.0f);
						ptfxWrapper::SetColourTint(pFxInst, vColourTint);
						ptfxWrapper::SetAlphaTint(pFxInst, pPtFxAttr->m_tintA/255.0f);
					}

					pFxInst->SetUserZoom(pPtFxAttr->m_scale);	
					pFxInst->Start();

					// trigger audio
					audSoundInitParams initParams;
					initParams.Position = RCC_VECTOR3(vFxPos);
					g_AmbientAudioEntity.CreateDeferredSound(pAnimInfo->ptFxHashName, pEntity, &initParams, pEntity != NULL, false);

#if GTA_REPLAY
					if(CReplayMgr::ShouldRecord())
					{
						CReplayMgr::RecordFx<CPacketEntityAmbientFx>(CPacketEntityAmbientFx(pPtFxAttr, 0, true), pEntity);
					}
#endif
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxEntityRayFire
///////////////////////////////////////////////////////////////////////////////

void					CVfxEntity::UpdatePtFxEntityRayFire		(CEntity* pEntity, const CCompEntityEffectsData* pRayFireEffectData, s32 effectId)
{
	if (ShouldUpdate(pRayFireEffectData->GetPtFxTagHash(), (s32)pRayFireEffectData->GetPtFxProbability(), pEntity, pRayFireEffectData->GetFxOffsetPos(), effectId)==false)
	{
		return;
	}

	VfxEntityRayFirePtFxInfo_s* pRayFireInfo = GetRayFirePtFxInfo(pRayFireEffectData->GetPtFxTagHash());
	if (pRayFireInfo)
	{
		// get the matrix to attach the fx to
		Mat34V boneMtx;
		s32 boneIndex = CVfxHelper::GetMatrixFromBoneTag(boneMtx, pEntity, (eAnimBoneTag)pRayFireEffectData->GetBoneTag());

		// return if we don't have a valid matrix
		if (IsZeroAll(boneMtx.GetCol0()))
		{
			return;
		}

		// get access to the effect rule
		ptxEffectRule* pEffectRule = ptfxWrapper::GetEffectRule(pRayFireInfo->ptFxHashName);
		if (pEffectRule)
		{
			// get the final position where this is going to be 
			Vec3V vFxPos;
			vFxPos = Transform3x3(boneMtx, pRayFireEffectData->GetFxOffsetPos());
			vFxPos = vFxPos + boneMtx.GetCol3(); 

			// get closest active camera to fxPos (and the distance to it)
			float fxDistSqr = CVfxHelper::GetDistSqrToCamera(vFxPos);

			// continue if this fx system is in range
			if (IsPtFxInRange(pEffectRule, fxDistSqr))
			{
				// try to create the fx system
				bool justCreated;
				ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pEntity, effectId, true, pRayFireInfo->ptFxHashName, justCreated);
				if (pFxInst)
				{
					ptfxRegInst* pRegFx = ptfxReg::Find(pEntity, effectId);
					vfxAssertf(pRegFx, "cannot find registered effect");

					// fx system created ok - start it up and set its position and orientation
					ptfxAttach::Attach(pFxInst, pEntity, boneIndex);

					if (justCreated)
					{
						pFxInst->SetOffsetPos(pRayFireEffectData->GetFxOffsetPos());
						pFxInst->SetOffsetRot(QuatV(pRayFireEffectData->GetFxOffsetRot().GetIntrin128()));

						if (pRayFireEffectData->GetPtFxHasTint())
						{
							Vec3V vColourTint(pRayFireEffectData->GetPtFxTintR()/255.0f, pRayFireEffectData->GetPtFxTintG()/255.0f, pRayFireEffectData->GetPtFxTintB()/255.0f);
							ptfxWrapper::SetColourTint(pFxInst, vColourTint);
						}

						pFxInst->SetUserZoom(pRayFireEffectData->GetPtFxScale());
						pFxInst->Start();

						pRegFx->m_hasAudio = SOUNDFACTORY.GetMetadataPtr(pRayFireInfo->ptFxHashName) != NULL;
					}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
					else
					{
						// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
						vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "effect attachment error");
					}
#endif
					if (pRegFx->m_hasAudio)
					{
						fwUniqueObjId effectUniqueID = fwIdKeyGenerator::Get(pEntity, effectId);
						g_AmbientAudioEntity.RegisterEffectAudio(pRayFireInfo->ptFxHashName, effectUniqueID, RCC_VECTOR3(vFxPos), pEntity);
					}
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxEntityRayFire
///////////////////////////////////////////////////////////////////////////////

void					CVfxEntity::TriggerPtFxEntityRayFire	(CEntity* pEntity, const CCompEntityEffectsData* pRayFireEffectData)
{
	if (ShouldTrigger(pRayFireEffectData->GetPtFxTagHash(), (int)pRayFireEffectData->GetPtFxProbability())==false)
	{
		return;
	}

	VfxEntityRayFirePtFxInfo_s* pRayFireInfo = GetRayFirePtFxInfo(pRayFireEffectData->GetPtFxTagHash());
	if (pRayFireInfo)
	{
		// get the matrix to attach the fx to
		Mat34V boneMtx;
		s32 boneIndex = CVfxHelper::GetMatrixFromBoneTag(boneMtx, pEntity, (eAnimBoneTag)pRayFireEffectData->GetBoneTag());

		// return if we don't have a valid matrix
		if (IsZeroAll(boneMtx.GetCol0()))
		{
			return;
		}

		// get access to the effect rule
		ptxEffectRule* pEffectRule = ptfxWrapper::GetEffectRule(pRayFireInfo->ptFxHashName);
		if (pEffectRule)
		{
			// get the final position where this is going to be 
			Vec3V vFxPos;
			vFxPos = Transform3x3(boneMtx, pRayFireEffectData->GetFxOffsetPos());
			vFxPos = vFxPos + boneMtx.GetCol3(); 

			// get closest active camera to fxPos (and the distance to it)
			float fxDistSqr = CVfxHelper::GetDistSqrToCamera(vFxPos);

			// continue if this fx system is in range
			if (IsPtFxInRange(pEffectRule, fxDistSqr))
			{
				// try to create the fx system
				ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pRayFireInfo->ptFxHashName);
				if (pFxInst)
				{
					// fx system created ok - start it up and set its position and orientation
					ptfxAttach::Attach(pFxInst, pEntity, boneIndex);

					pFxInst->SetOffsetPos(pRayFireEffectData->GetFxOffsetPos());
					pFxInst->SetOffsetRot(QuatV(pRayFireEffectData->GetFxOffsetRot().GetIntrin128()));

					if (pRayFireEffectData->GetPtFxHasTint())
					{
						Vec3V vColourTint(pRayFireEffectData->GetPtFxTintR()/255.0f, pRayFireEffectData->GetPtFxTintG()/255.0f, pRayFireEffectData->GetPtFxTintB()/255.0f);
						ptfxWrapper::SetColourTint(pFxInst, vColourTint);
					}

					pFxInst->SetUserZoom(pRayFireEffectData->GetPtFxScale());
					pFxInst->Start();

					// trigger audio
					audSoundInitParams initParams;
					initParams.Position = RCC_VECTOR3(vFxPos);
					g_AmbientAudioEntity.CreateDeferredSound(pRayFireInfo->ptFxHashName, pEntity, &initParams, pEntity != NULL, false);
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxEntityInWater
///////////////////////////////////////////////////////////////////////////////

void				CVfxEntity::TriggerPtFxEntityInWater			(CEntity* pEntity, const CParticleAttr* pPtFxAttr)
{
	if (m_inWaterPtFxDisabled)
	{
		return;
	}

	if (pPtFxAttr->m_boneTag<0)
	{
		vfxAssertf(0, "called with invalid bone tag (%s)", pEntity->GetModelName());
		return;
	}

	if (ShouldTrigger(pPtFxAttr->m_tagHashName, pPtFxAttr->m_probability)==false)
	{
		return;
	}

	Vec3V vEffectPos;
	pPtFxAttr->GetPos(RC_VECTOR3(vEffectPos));

	VfxEntityInWaterPtFxInfo_s* pInWaterInfo = GetInWaterPtFxInfo(pPtFxAttr->m_tagHashName);
	if (pInWaterInfo)
	{
		// get the matrix to attach the fx to
		Mat34V boneMtx;
		s32 boneIndex = CVfxHelper::GetMatrixFromBoneTag(boneMtx, pEntity, (eAnimBoneTag)pPtFxAttr->m_boneTag);

		// return if we don't have a valid matrix
		if (IsZeroAll(boneMtx.GetCol0()))
		{
			return;
		}

		// get access to the effect rule
		ptxEffectRule* pEffectRule = ptfxWrapper::GetEffectRule(pInWaterInfo->ptFxHashName);
		if (pEffectRule)
		{
			// get the final position where this is going to be 
			Vec3V vFxPos;
			vFxPos = Transform3x3(boneMtx, vEffectPos);
			vFxPos = vFxPos + boneMtx.GetCol3(); 

			// get closest active camera to fxPos (and the distance to it)
			float fxDistSqr = CVfxHelper::GetDistSqrToCamera(vFxPos);

			// continue if this fx system is in range
			if (IsPtFxInRange(pEffectRule, fxDistSqr))
			{
				// get the current water z and the speed it is rising at this position
				float oceanWaterZ = 0.0f;
				float oceanWaterSpeedUp = 0.0f;
				if (CVfxHelper::GetOceanWaterZAndSpeedUp(vFxPos, oceanWaterZ, oceanWaterSpeedUp)==false)
				{
					return;
				}

				// calc the approximate previous water z at this position
				float distUp = oceanWaterSpeedUp*CVfxHelper::GetPrevTimeStep()*Water::GetWaterTimeStepScale();
				float approxPrevOceanWaterZ = oceanWaterZ - distUp;

				// check if the water has risen over the particle helper this frame
				float ptxZ = vFxPos.GetZf();
				if (approxPrevOceanWaterZ<ptxZ && oceanWaterZ>=ptxZ)
				{
					// return early if the water z isn't rising quickly enough
					if (oceanWaterSpeedUp<pInWaterInfo->speedUpThreshMin)
					{
						//vfxDebugf1("In Water - currWaterZ %.5f - speedUp %.5f - approxPrevWaterZ %.5f - SKIPPED DUE TO SPEED", oceanWaterZ, oceanWaterSpeedUp, approxPrevOceanWaterZ);
						return;
					}

					// return early if there is already a similar effect playing close by
					if (ptfxHistory::Check(pInWaterInfo->ptFxHashName, 0, pEntity, vFxPos, ENTITYFX_INWATER_HIST_DIST))
					{
						//vfxDebugf1("In Water - currWaterZ %.5f - speedUp %.5f - approxPrevWaterZ %.5f - SKIPPED DUE TO HISTORY", oceanWaterZ, oceanWaterSpeedUp, approxPrevOceanWaterZ);
						return;
					}

					//vfxDebugf1("In Water - currWaterZ %.5f - speedUp %.5f - approxPrevWaterZ %.5f - TRIGGERED", oceanWaterZ, oceanWaterSpeedUp, approxPrevOceanWaterZ);

					// try to create the fx system
					ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pInWaterInfo->ptFxHashName);
					if (pFxInst)
					{
						// fx system created ok - start it up and set its position and orientation
						ptfxAttach::Attach(pFxInst, pEntity, boneIndex);

						pFxInst->SetOffsetPos(vEffectPos);
						pFxInst->SetOffsetRot(QuatV(pPtFxAttr->qX, pPtFxAttr->qY, pPtFxAttr->qZ, pPtFxAttr->qW));

						float speedEvo = CVfxHelper::GetInterpValue(oceanWaterSpeedUp, pInWaterInfo->speedUpThreshMin, pInWaterInfo->speedUpThreshMax);
						pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

						if (pPtFxAttr->m_hasTint)
						{
							Vec3V vColourTint(pPtFxAttr->m_tintR/255.0f, pPtFxAttr->m_tintG/255.0f, pPtFxAttr->m_tintB/255.0f);
							ptfxWrapper::SetColourTint(pFxInst, vColourTint);
							ptfxWrapper::SetAlphaTint(pFxInst, pPtFxAttr->m_tintA/255.0f);
						}

						pFxInst->SetUserZoom(pPtFxAttr->m_scale);	
						pFxInst->Start();

						ptfxHistory::Add(pInWaterInfo->ptFxHashName, 0, pEntity, vFxPos, ENTITYFX_INWATER_HIST_TIME);

						// trigger audio
						audSoundInitParams initParams;
						initParams.Position = RCC_VECTOR3(vFxPos);
						initParams.SetVariableValue(ATSTRINGHASH("size", 0xC052DEA7),speedEvo);
						g_AmbientAudioEntity.CreateDeferredSound(pInWaterInfo->ptFxHashName, pEntity, &initParams, pEntity != NULL, false);
					}
				}
// 				else
// 				{
// 					vfxDebugf1("In Water - currWaterZ %.5f - speedUp %.5f - approxPrevWaterZ %.5f", oceanWaterZ, oceanWaterSpeedUp, approxPrevOceanWaterZ);
// 				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxLightShaft
///////////////////////////////////////////////////////////////////////////////

void			CVfxEntity::UpdatePtFxLightShaft						(CEntity* pEntity, const CLightShaft* pLightShaft, s32 effectId)
{
	atHashWithStringNotFinal ptFxHashName("env_dust_motes",0xF49139C0);

	ptxEffectRule* pEffectRule = ptfxWrapper::GetEffectRule(ptFxHashName);
	if (pEffectRule)
	{
		static float SHAFT_MAX_LENGTH = 2.0f;
		ScalarV vShaftLength = ScalarVFromF32(Min(pLightShaft->m_length, SHAFT_MAX_LENGTH));
		Vec3V vQuadCentre = (pLightShaft->m_corners[0]+pLightShaft->m_corners[1]+pLightShaft->m_corners[2]+pLightShaft->m_corners[3]) * Vec3V(V_QUARTER);
		Vec3V vShaftCentre = vQuadCentre + (pLightShaft->m_direction*vShaftLength*ScalarV(V_HALF));

		float fxDistSqr = CVfxHelper::GetDistSqrToCamera(vShaftCentre);
		if (IsPtFxInRange(pEffectRule, fxDistSqr))
		{
			//grcDebugDraw::Sphere(pLightShaft->m_corners[0], 0.1f, Color32(1.0f, 0.0f, 0.0f, 1.0f));
			//grcDebugDraw::Sphere(pLightShaft->m_corners[1], 0.1f, Color32(0.0f, 1.0f, 0.0f, 1.0f));
			//grcDebugDraw::Sphere(pLightShaft->m_corners[2], 0.1f, Color32(0.0f, 0.0f, 1.0f, 1.0f));
			//grcDebugDraw::Sphere(pLightShaft->m_corners[3], 0.1f, Color32(1.0f, 1.0f, 0.0f, 1.0f));

			// it has just been created - set its matrix
			Vec3V vVec12 = pLightShaft->m_corners[2]-pLightShaft->m_corners[1];
			Vec3V vVec23 = pLightShaft->m_corners[3]-pLightShaft->m_corners[2];

			ScalarV vDist12 = Mag(vVec12);
			ScalarV vDist23 = Mag(vVec23);

			vVec12 = Normalize(vVec12);
			vVec23 = Normalize(vVec23);

			Mat34V vMtx;
			CVfxHelper::CreateMatFromVecZAlign(vMtx, vShaftCentre, pLightShaft->m_direction, vVec23);

			// set the override domain size 
			ScalarV vNdotD12 = Dot(vVec12, vMtx.GetCol0());
			ScalarV vNdotD23 = Dot(vVec23, vMtx.GetCol1());

			ScalarV vZero(V_ZERO);
			if (IsGreaterThan(vNdotD12, vZero).Getb() && IsGreaterThan(vNdotD23, vZero).Getb())
			{
				Vec3V vDomainSize;
				vDomainSize.SetX(vDist12*vNdotD12);
				vDomainSize.SetY(vDist23*vNdotD23);
				vDomainSize.SetZ(vShaftLength);

				static float SHAFT_DOMAIN_SIZE_SCALE = 0.8f;
				vDomainSize = Scale(vDomainSize, ScalarV(V_HALF) * ScalarVFromF32(SHAFT_DOMAIN_SIZE_SCALE));

				// register the fx system
				bool justCreated;
				ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pEntity, effectId, false, ptFxHashName, justCreated);
				if (pFxInst)
				{
					pFxInst->SetOverrideCreationDomainSize(vDomainSize);

					pFxInst->SetBaseMtx(vMtx);

					// check if the effect has just been created 
					if (justCreated)
					{
						pFxInst->Start();
					}
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerDecalEntityCollision
///////////////////////////////////////////////////////////////////////////////

void				CVfxEntity::TriggerDecalEntityCollision		(CEntity* pEntity, const CDecalAttr* pDecalAttr, Vec3V_In vPos, Vec3V_In UNUSED_PARAM(vNormal), s32 collidedBoneTag, float accumImpulse)
{
	if (ShouldTrigger(pDecalAttr->m_tagHashName, pDecalAttr->m_probability)==false)
	{
		return;
	}

	VfxEntityCollisionDecalInfo_s* pDecalInfo = GetCollisionDecalInfo(pDecalAttr->m_tagHashName);
	if (pDecalInfo)
	{	
		if (accumImpulse>=pDecalInfo->collImpThreshMin)
		{
			// get the final position where this is going to be 
			Vec3V vDecalPosWld;
			if (pDecalInfo->atCollisionPt)
			{
				vDecalPosWld = vPos;
			}
			else
			{
				// get the matrix of the bone we're attached to
				Mat34V boneMtx;
				CVfxHelper::GetMatrixFromBoneTag(boneMtx, pEntity, (eAnimBoneTag)collidedBoneTag);

				// return if we don't have a valid matrix
				if (IsZeroAll(boneMtx.GetCol0()))
				{
					return;
				}

				Vec3V vDecalPosLcl;
				pDecalAttr->GetPos(RC_VECTOR3(vDecalPosLcl));

				vDecalPosWld = Transform3x3(boneMtx, vDecalPosLcl);
				vDecalPosWld = vDecalPosWld + boneMtx.GetCol3(); 
			}

			TriggerDecalGeneric(pDecalInfo, vDecalPosWld, pDecalAttr->m_scale);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerDecalEntityShot
///////////////////////////////////////////////////////////////////////////////

void				CVfxEntity::TriggerDecalEntityShot			(CEntity* pEntity, const CDecalAttr* pDecalAttr, Vec3V_In vPos, Vec3V_In UNUSED_PARAM(vNormal), s32 shotBoneTag)
{
	if (ShouldTrigger(pDecalAttr->m_tagHashName, pDecalAttr->m_probability)==false)
	{
		return;
	}

	VfxEntityShotDecalInfo_s* pDecalInfo = GetShotDecalInfo(pDecalAttr->m_tagHashName);
	if (pDecalInfo)
	{
		// get the final position where this is going to be 
		Vec3V vDecalPosWld;
		if (pDecalInfo->atCollisionPt)
		{
			vDecalPosWld = vPos;
		}
		else
		{
			// get the matrix of the bone we're attached to
			Mat34V boneMtx;
			CVfxHelper::GetMatrixFromBoneTag(boneMtx, pEntity, (eAnimBoneTag)shotBoneTag);

			// return if we don't have a valid matrix
			if (IsZeroAll(boneMtx.GetCol0()))
			{
				return;
			}

			Vec3V vDecalPosLcl;
			pDecalAttr->GetPos(RC_VECTOR3(vDecalPosLcl));

			vDecalPosWld = Transform3x3(boneMtx, vDecalPosLcl);
			vDecalPosWld = vDecalPosWld + boneMtx.GetCol3(); 
		}

		TriggerDecalGeneric(pDecalInfo, vDecalPosWld, pDecalAttr->m_scale);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerDecalFragmentBreak
///////////////////////////////////////////////////////////////////////////////

void				CVfxEntity::TriggerDecalFragmentBreak		(CEntity* pEntity, const CDecalAttr* pDecalAttr, s32 brokenBoneTag, s32 parentBoneTag)
{
	if (ShouldTrigger(pDecalAttr->m_tagHashName, pDecalAttr->m_probability)==false)
	{
		return;
	}

	VfxEntityShotDecalInfo_s* pDecalInfo = GetShotDecalInfo(pDecalAttr->m_tagHashName);
	if (pDecalInfo)
	{
		// get the matrix of the broken piece
		s32 attachBoneTag = brokenBoneTag;
		if (pDecalAttr->m_playOnParent)
		{
			// attach to the parent instead
			attachBoneTag = parentBoneTag;
		}

		Mat34V boneMtx;
		CVfxHelper::GetMatrixFromBoneTag(boneMtx, pEntity, (eAnimBoneTag)attachBoneTag);

		// return if we don't have a valid matrix
		if (IsZeroAll(boneMtx.GetCol0()))
		{
			return;
		}

		// get the final position where this is going to be 
		Vec3V vDecalPosWld;
		if (pDecalAttr->m_playOnParent)
		{
			vDecalPosWld = boneMtx.GetCol3();
		}
		else
		{
			Vec3V vDecalPosLcl;
			pDecalAttr->GetPos(RC_VECTOR3(vDecalPosLcl));
			vDecalPosWld = Transform3x3(boneMtx, vDecalPosLcl);
			vDecalPosWld = vDecalPosWld + boneMtx.GetCol3(); 
		}

		TriggerDecalGeneric(pDecalInfo, vDecalPosWld, pDecalAttr->m_scale);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerDecalFragmentDestroy
///////////////////////////////////////////////////////////////////////////////

void				CVfxEntity::TriggerDecalFragmentDestroy		(CEntity* UNUSED_PARAM(pEntity), const CDecalAttr* pDecalAttr, Mat34V_In fxMat)
{
	if (ShouldTrigger(pDecalAttr->m_tagHashName, pDecalAttr->m_probability)==false)
	{
		return;
	}

	VfxEntityBreakDecalInfo_s* pDecalInfo = GetBreakDecalInfo(pDecalAttr->m_tagHashName);
	if (pDecalInfo)
	{
		// return if we don't have a valid matrix
		if (IsZeroAll(fxMat.GetCol0()))
		{
			return;
		}

		// get the final position where this is going to be 
		Vec3V vDecalPosLcl;
		pDecalAttr->GetPos(RC_VECTOR3(vDecalPosLcl));

		Vec3V vDecalPosWld;
		vDecalPosWld = Transform3x3(fxMat, vDecalPosLcl);
		vDecalPosWld = vDecalPosWld + fxMat.GetCol3(); 

		TriggerDecalGeneric(pDecalInfo, vDecalPosWld, pDecalAttr->m_scale);

#if GTA_REPLAY
		// Attach to replay here
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketDecalFragmentDestroy>(
				CPacketDecalFragmentDestroy(pDecalAttr->m_tagHashName, vDecalPosWld, pDecalAttr->m_scale ) );
		}
#endif

	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerDecalGeneric
///////////////////////////////////////////////////////////////////////////////

void				CVfxEntity::TriggerDecalGeneric				(const VfxEntityGenericDecalInfo_s* pDecalInfo, Vec3V_In vPosWld, float scale)
{
	// get closest active camera to pos (and the distance to it)
	float fxDistSqr = CVfxHelper::GetDistSqrToCamera(vPosWld);

	// continue if we're in range
	if (fxDistSqr<=30.0f*30.0f)
	{
		// do each probe
		for (s32 i=0; i<pDecalInfo->numProbes; i++)
		{
			Vec3V vStartPos;
			Vec3V vProbeDir;
			if (IsZeroAll(pDecalInfo->vProbeDir))
			{
				// probing radially from a single point
				vStartPos = vPosWld;
				GetRandomVector(vProbeDir);
			}
			else
			{
				// probe in the direction specified within a radius perpendicular to the direction
				Vec3V vRandTangent;
				CVfxHelper::GetRandomTangent(vRandTangent, pDecalInfo->vProbeDir);
				float randProbeRadius = g_DrawRand.GetRanged(pDecalInfo->probeRadiusMin, pDecalInfo->probeRadiusMax);
				vStartPos = vPosWld + (vRandTangent*ScalarVFromF32(randProbeRadius));
				vProbeDir = pDecalInfo->vProbeDir;
			}

			Vec3V vEndPos = vStartPos + (vProbeDir*ScalarVFromF32(pDecalInfo->probeDist));

			m_asyncProbeMgr.TriggerDecalProbe(vStartPos, vEndPos, pDecalInfo, scale);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//	ShouldTrigger
///////////////////////////////////////////////////////////////////////////////

bool				CVfxEntity::ShouldTrigger					(atHashWithStringNotFinal DEV_ONLY(tagHashName), s32 probability)
{
#if __DEV
	if (m_onlyProcessThisTag)
	{
		if (tagHashName.GetHash()!=atHashValue(m_onlyProcessThisTagName))
		{
			return false;
		}
	}
#endif

	// use the probability to see if we need to go further
	if (g_DrawRand.GetFloat()>probability*0.01f)
	{
		return false;
	}

	return true;
}


///////////////////////////////////////////////////////////////////////////////
//	ShouldUpdate
///////////////////////////////////////////////////////////////////////////////

bool				CVfxEntity::ShouldUpdate					(atHashWithStringNotFinal DEV_ONLY(tagHashName), s32 probability, CEntity* pEntity, Vec3V_In vPosLcl, s32 effectId)
{
#if __DEV
	if (m_onlyProcessThisTag)
	{
		if (tagHashName.GetHash()!=atHashValue(m_onlyProcessThisTagName))
		{
			return false;
		}
	}
#endif

	// use the probability to see if we need to go further
	if (GetFxEntitySeed(pEntity, vPosLcl, 0.247f+effectId)>probability*0.01f)
	{
		return false;
	}

	return true;
}


///////////////////////////////////////////////////////////////////////////////
//  GetFxEntitySeed
///////////////////////////////////////////////////////////////////////////////

float				CVfxEntity::GetFxEntitySeed						(CEntity* pEntity, Vec3V_In vPosLcl, float divNumber)
{
	float seed;
	if (pEntity->GetIsDynamic())
	{
		seed = ((CDynamicEntity*)pEntity)->GetRandomSeed() + vPosLcl.GetXf() + vPosLcl.GetYf() + vPosLcl.GetZf();
	}
	else
	{
		const Vector3 vEntityPosition = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
		seed = (vEntityPosition.x+vPosLcl.GetXf()) + (vEntityPosition.y+vPosLcl.GetYf())+ (vEntityPosition.z+vPosLcl.GetZf());
	}

	seed = Abs(seed)/divNumber;
	return seed - static_cast<s32>(seed);
}


///////////////////////////////////////////////////////////////////////////////
//	IsPtFxInRange
///////////////////////////////////////////////////////////////////////////////

bool				CVfxEntity::IsPtFxInRange					(ptxEffectRule* pEffectRule, float fxDistSqr)
{
	float cullDist = pEffectRule->GetDistanceCullingCullDist();
	return pEffectRule->GetDistanceCullingMode()==0 || cullDist==0.0f || fxDistSqr<=cullDist*cullDist;
}


















