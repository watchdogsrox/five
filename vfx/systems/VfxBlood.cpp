///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxBlood.cpp 
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	29 Jan 2007
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxBlood.h"

// rage
#include "file/asset.h"
#include "file/token.h"
#include "system/param.h"

// framework
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxhistory.h"
#include "vfx/ptfx/ptfxwrapper.h"
#include "vfx/vfxwidget.h"

// game
#include "Camera/CamInterface.h"
#include "camera/base/BaseCamera.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "control/replay/replay.h"
#include "control/replay/effects/Particlebloodfxpacket.h"
#include "Core/Game.h"
#include "Debug/MarketingTools.h"
#include "Network/NetworkInterface.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedHelmetComponent.h"
#include "Peds/Ped.h"
#include "Peds/PlayerSpecialAbility.h"
#include "Peds/rendering/PedDamage.h"
#include "Physics/GtaMaterialManager.h"
#include "Physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Scene/DataFileMgr.h"
#include "Scene/World/GameWorld.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/System/TaskHelpers.h"
#include "Vehicles/Vehicle.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Metadata/VfxPedInfo.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/Systems/VfxLens.h"
#include "Vfx/Systems/VfxPed.h"
#include "Vfx/VehicleGlass/VehicleGlass.h" // for IsEntitySmashable
#include "Vfx/VfxHelper.h"
#include "Weapons/Info/WeaponInfoManager.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

// rag tweakable settings
dev_float	VFXBLOOD_HIST_DIST							= 0.15f;
dev_float	VFXBLOOD_HIST_TIME							= 0.2f;
//dev_float		VFXBLOOD_CAMSPLASH_DOT_PROD				= 0.3f;
//dev_float		VFXBLOOD_CAMSPLASH_DIST					= 2.5f;

dev_float	VFXBLOOD_PTFX_ZOOM_TREVOR_RAGE				= 3.1f;

// blood pool settings
dev_float VFXBLOOD_POOL_TIMER_MIN						= 12.0f;
dev_float VFXBLOOD_POOL_TIMER_MAX						= 20.0f;
dev_float VFXBLOOD_POOL_STARTSIZE_MIN					= 0.05f;
dev_float VFXBLOOD_POOL_STARTSIZE_MAX					= 0.10f;
dev_float VFXBLOOD_POOL_ENDSIZE_MIN						= 0.90f;
dev_float VFXBLOOD_POOL_ENDSIZE_MAX						= 1.30f;
dev_float VFXBLOOD_POOL_GROWRATE_MIN					= 0.005f;
dev_float VFXBLOOD_POOL_GROWRATE_MAX					= 0.010f;
dev_float VFXBLOOD_POOL_LOD_DIST						= 30.0f;
dev_float VFXBLOOD_POOL_IN_WATER_DEPTH					= 2.0f;


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CVfxBlood		g_vfxBlood;

class CVFXBloodFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile& file)
	{
		g_vfxBlood.PatchData(file.m_filename);
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile& file)
	{
		g_vfxBlood.UnPatchData(file.m_filename);
	}

} g_vfxBloodFileMounter;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxBloodAsyncProbeManager
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxBloodAsyncProbeManager::Init			()
{
	// init the splat probes
	for (int i=0; i<VFXBLOOD_MAX_SPLAT_PROBES; i++)
	{
		m_splatProbes[i].m_vfxProbeId = -1;
	}

	// init the pool probes
	for (int i=0; i<VFXBLOOD_MAX_POOL_PROBES; i++)
	{
		m_poolProbes[i].m_vfxProbeId = -1;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxBloodAsyncProbeManager::Shutdown		()
{
	// init the splat probes
	for (int i=0; i<VFXBLOOD_MAX_SPLAT_PROBES; i++)
	{
		if (m_splatProbes[i].m_vfxProbeId>=0)
		{
			CVfxAsyncProbeManager::AbortProbe(m_splatProbes[i].m_vfxProbeId);
			m_splatProbes[i].m_vfxProbeId = -1;
		}
	}

	// init the pool probes
	for (int i=0; i<VFXBLOOD_MAX_POOL_PROBES; i++)
	{
		if (m_poolProbes[i].m_vfxProbeId>=0)
		{
			CVfxAsyncProbeManager::AbortProbe(m_poolProbes[i].m_vfxProbeId);
			m_poolProbes[i].m_vfxProbeId = -1;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxBloodAsyncProbeManager::Update			()
{
	// update the splat probes
	for (int i=0; i<VFXBLOOD_MAX_SPLAT_PROBES; i++)
	{
		if (m_splatProbes[i].m_vfxProbeId>=0)
		{
			// query the probe
			VfxAsyncProbeResults_s vfxProbeResults;
			if (CVfxAsyncProbeManager::QueryProbe(m_splatProbes[i].m_vfxProbeId, vfxProbeResults))
			{
				// the probe has finished
				m_splatProbes[i].m_vfxProbeId = -1;

				const VfxBloodInfo_s* pVfxBloodInfo = m_splatProbes[i].m_pVfxBloodInfo;

				// check if the probe intersected
				if (vfxProbeResults.hasIntersected)
				{
					// there was an intersection 
					CEntity* pHitEntity = CVfxAsyncProbeManager::GetEntity(vfxProbeResults);
					if (pHitEntity)
					{
						if( pHitEntity->GetIsTypePed() )
						{
							CPed * pPed = static_cast<CPed*>(pHitEntity);
							// ignore player->player splats to retain more blits
							if( !(pPed->IsLocalPlayer() && m_splatProbes[i].m_isFromPlayer) )
							{
								PEDDAMAGEMANAGER.AddPedBlood(pPed, vfxProbeResults.componentId, VEC3V_TO_VECTOR3(vfxProbeResults.vPos), ATSTRINGHASH("BackSplash",0xdde56f4));
							}
						}
						else if (PGTAMATERIALMGR->GetIsSmashableGlass(vfxProbeResults.mtlId) && IsEntitySmashable(pHitEntity))
						{
							// to deal with...
						}
						else if (PGTAMATERIALMGR->GetIsShootThrough(vfxProbeResults.mtlId) && !PGTAMATERIALMGR->GetIsGlass(vfxProbeResults.mtlId))
						{
							// don't place textures on non-glass shoot thru materials 
						}
						else
						{
							float hiToLoLodRatio = m_splatProbes[i].m_hiToLoLodRatio;

							VfxCollInfo_s vfxCollInfoBlood;
							vfxCollInfoBlood.regdEnt		= pHitEntity;
							vfxCollInfoBlood.vPositionWld	= vfxProbeResults.vPos;
							vfxCollInfoBlood.vNormalWld		= vfxProbeResults.vNormal;
							vfxCollInfoBlood.vDirectionWld	= m_splatProbes[i].m_vProbeDir;
							vfxCollInfoBlood.materialId		= PGTAMATERIALMGR->UnpackMtlId(vfxProbeResults.mtlId);
							vfxCollInfoBlood.roomId			= PGTAMATERIALMGR->UnpackRoomId(vfxProbeResults.mtlId);
							vfxCollInfoBlood.componentId	= vfxProbeResults.componentId;
							vfxCollInfoBlood.weaponGroup	= WEAPON_EFFECT_GROUP_PUNCH_KICK;		// shouldn't matter what this is set to at this point
							vfxCollInfoBlood.force			= vfxProbeResults.tValue;
							vfxCollInfoBlood.isBloody		= false;
							vfxCollInfoBlood.isWater		= false;
							vfxCollInfoBlood.isExitFx		= false;
							vfxCollInfoBlood.noPtFx			= false;
							vfxCollInfoBlood.noPedDamage   	= false;
							vfxCollInfoBlood.noDecal	   	= false; 
							vfxCollInfoBlood.isSnowball		= false;
							vfxCollInfoBlood.isFMJAmmo		= false;

							// calc the settings based on lod
							float sizeMin;
							float sizeMax;
							if (m_splatProbes[i].m_isProbeA)
							{
								sizeMin = pVfxBloodInfo->sizeMinAHi + (hiToLoLodRatio * (pVfxBloodInfo->sizeMinALo-pVfxBloodInfo->sizeMinAHi));
								sizeMax = pVfxBloodInfo->sizeMaxAHi + (hiToLoLodRatio * (pVfxBloodInfo->sizeMaxALo-pVfxBloodInfo->sizeMaxAHi));
							}
							else
							{
								sizeMin = pVfxBloodInfo->sizeMinBHi + (hiToLoLodRatio * (pVfxBloodInfo->sizeMinBLo-pVfxBloodInfo->sizeMinBHi));
								sizeMax = pVfxBloodInfo->sizeMaxBHi + (hiToLoLodRatio * (pVfxBloodInfo->sizeMaxBLo-pVfxBloodInfo->sizeMaxBHi));
							}

							float width = sizeMin + vfxCollInfoBlood.force*(sizeMax-sizeMin);

							bool doSprayDecal = Abs(Dot(m_splatProbes[i].m_vProbeDir, vfxCollInfoBlood.vNormalWld).Getf())<pVfxBloodInfo->sprayDotProductThresh;
							bool doMistDecal = vfxProbeResults.tValue>pVfxBloodInfo->mistTValueThresh;

							g_decalMan.AddBloodSplat(*pVfxBloodInfo, vfxCollInfoBlood, width, doSprayDecal, doMistDecal);
						}
					}
				}
				else
				{
					// no intersection - if this was probe A then trigger probe B
					if (m_splatProbes[i].m_isProbeA && pVfxBloodInfo->probeDistB>0.0f)
					{
						// probe B starts at the midpoint of the first probe and probes downwards looking for ground
						Vec3V vProbeDir = -Vec3V(V_Z_AXIS_WZERO);
						Vec3V vRand;
						vRand.SetXf(g_DrawRand.GetRanged(-pVfxBloodInfo->probeVariation, pVfxBloodInfo->probeVariation));
						vRand.SetYf(g_DrawRand.GetRanged(-pVfxBloodInfo->probeVariation, pVfxBloodInfo->probeVariation));
						vRand.SetZf(g_DrawRand.GetRanged(-pVfxBloodInfo->probeVariation, pVfxBloodInfo->probeVariation));
						vProbeDir = vProbeDir + vRand;
						vProbeDir = Normalize(vProbeDir);

						Vec3V vStartPos = m_splatProbes[i].m_vMidPoint;
						Vec3V vEndPos = vStartPos+(vProbeDir*ScalarVFromF32(pVfxBloodInfo->probeDistB));

						TriggerSplatProbe(vStartPos, vEndPos, vProbeDir, pVfxBloodInfo, m_splatProbes[i].m_hiToLoLodRatio, false, m_splatProbes[i].m_isFromPlayer);
					}
				}
			}
		}
	}

	// update the pool probes
	for (int i=0; i<VFXBLOOD_MAX_POOL_PROBES; i++)
	{
		if (m_poolProbes[i].m_vfxProbeId>=0)
		{
			// query the probe
			VfxAsyncProbeResults_s vfxProbeResults;
			if (CVfxAsyncProbeManager::QueryProbe(m_poolProbes[i].m_vfxProbeId, vfxProbeResults))
			{
				// the probe has finished
				m_poolProbes[i].m_vfxProbeId = -1;

				// check if the probe intersected
				if (vfxProbeResults.hasIntersected)
				{
					// there was an intersection - check that the ped is still valid
					if (m_poolProbes[i].m_regdPed)
					{
						if(PGTAMATERIALMGR->GetIsShootThrough(vfxProbeResults.mtlId)==false || PGTAMATERIALMGR->GetIsGlass(vfxProbeResults.mtlId))
						{
							u8 mtlId = (u8)PGTAMATERIALMGR->UnpackMtlId(vfxProbeResults.mtlId);

							Color32 col(255, 255, 255, 255);
							g_decalMan.AddPool(VFXLIQUID_TYPE_BLOOD, -1, 0, vfxProbeResults.vPos, vfxProbeResults.vNormal, m_poolProbes[i].m_poolStartSize, m_poolProbes[i].m_poolEndSize, m_poolProbes[i].m_poolGrowRate, mtlId, col, false);
						}
					}
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerSplatProbe
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxBloodAsyncProbeManager::TriggerSplatProbe(Vec3V_In vStartPos, Vec3V_In vEndPos, Vec3V_In vProbeDir, const VfxBloodInfo_s* pVfxBloodInfo, float hiToLoLodRatio, bool isProbeA, bool isFromPlayerPed)
{
	static u32 probeFlags = ArchetypeFlags::GTA_MAP_TYPE_WEAPON | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_GLASS_TYPE | ArchetypeFlags::GTA_PED_TYPE;

	// look for an available probe
	for (int i=0; i<VFXBLOOD_MAX_SPLAT_PROBES; i++)
	{
		if (m_splatProbes[i].m_vfxProbeId==-1)
		{
			m_splatProbes[i].m_vfxProbeId = CVfxAsyncProbeManager::SubmitWorldProbe(vStartPos, vEndPos, probeFlags);
			m_splatProbes[i].m_vProbeDir = vProbeDir;
			m_splatProbes[i].m_vMidPoint = (vStartPos+vEndPos)*Vec3V(V_HALF);
			m_splatProbes[i].m_pVfxBloodInfo = pVfxBloodInfo;
			m_splatProbes[i].m_hiToLoLodRatio= hiToLoLodRatio;
			m_splatProbes[i].m_isProbeA = isProbeA;
			m_splatProbes[i].m_isFromPlayer = isFromPlayerPed;

#if __BANK
			if (g_vfxBlood.m_renderBloodSplatVfxProbes)
			{
				if (isProbeA)
				{
					grcDebugDraw::Line(vStartPos, vEndPos, Color32(0.0f, 1.0f, 0.0f, 1.0f), -200);
				}
				else
				{
					grcDebugDraw::Line(vStartPos, vEndPos, Color32(1.0f, 1.0f, 0.0f, 1.0f), -200);
				}
			}
#endif

			return;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPoolProbe
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxBloodAsyncProbeManager::TriggerPoolProbe(Vec3V_In vStartPos, Vec3V_In vEndPos, CPed* pPed, float poolStartSize, float poolEndSize, float poolGrowRate)
{
	static u32 probeFlags = ArchetypeFlags::GTA_ALL_MAP_TYPES;

	// look for an available probe
	for (int i=0; i<VFXBLOOD_MAX_POOL_PROBES; i++)
	{
		if (m_poolProbes[i].m_vfxProbeId==-1)
		{
			m_poolProbes[i].m_vfxProbeId = CVfxAsyncProbeManager::SubmitWorldProbe(vStartPos, vEndPos, probeFlags);
			m_poolProbes[i].m_regdPed = pPed;
			m_poolProbes[i].m_poolStartSize = poolStartSize;
			m_poolProbes[i].m_poolEndSize = poolEndSize;
			m_poolProbes[i].m_poolGrowRate = poolGrowRate;

#if __BANK
			if (g_vfxBlood.m_renderBloodPoolVfxProbes)
			{
				grcDebugDraw::Line(vStartPos, vEndPos, Color32(1.0f, 0.0f, 0.0f, 1.0f), -200);
			}
#endif

			return;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxBlood
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxBlood::Init								(unsigned initMode)
{
	if (initMode==INIT_CORE)
	{
		m_bloodVfxDisabledScriptThreadId = THREAD_INVALID;
		m_bloodVfxDisabled = false;

		m_bloodPtFxUserZoomScaleScriptThreadId = THREAD_INVALID;
		m_bloodPtFxUserZoomScale = 1.0f;

		m_overrideWeaponGroupIdScriptThreadId = THREAD_INVALID;
		m_overrideWeaponGroupId = -1;

#if __BANK
		m_bankInitialised = false;

		m_disableEntryVfx = false;
		m_disableExitVfx = false;
		m_forceMPData = false;

		m_renderBloodVfxInfo = false;
		m_renderBloodVfxInfoTimer = 30;
		m_renderBloodSplatVfxProbes = false;
		m_renderBloodPoolVfxProbes = false;

#endif
	}
    else if (initMode==INIT_AFTER_MAP_LOADED)
    {
		// load in the data from the file
		CDataFileMount::RegisterMountInterface(CDataFileMgr::BLOODFX_FILE_PATCH, &g_vfxBloodFileMounter);
	    LoadData();
	}
	else if (initMode==INIT_SESSION)
	{
		m_asyncProbeMgr.Init();
    }
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxBlood::Shutdown							(unsigned shutdownMode)
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

void		 	CVfxBlood::Update							()
{
	m_asyncProbeMgr.Update();
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveScript
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxBlood::RemoveScript							(scrThreadId scriptThreadId)
{
	if (scriptThreadId==m_bloodVfxDisabledScriptThreadId)
	{
		m_bloodVfxDisabled = false;
		m_bloodVfxDisabledScriptThreadId = THREAD_INVALID;
	}

	if (scriptThreadId==m_bloodPtFxUserZoomScaleScriptThreadId)
	{
		m_bloodPtFxUserZoomScale = 1.0f;
		m_bloodPtFxUserZoomScaleScriptThreadId = THREAD_INVALID;
	}

	if (scriptThreadId==m_overrideWeaponGroupIdScriptThreadId)
	{
		m_overrideWeaponGroupId = -1;
		m_overrideWeaponGroupIdScriptThreadId = THREAD_INVALID;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetDisableBloodVfx
///////////////////////////////////////////////////////////////////////////////

void			CVfxBlood::SetBloodVfxDisabled		(bool val, scrThreadId scriptThreadId) 
{
	if (vfxVerifyf(m_bloodVfxDisabledScriptThreadId==THREAD_INVALID || m_bloodVfxDisabledScriptThreadId==scriptThreadId, "trying to disable blood vfx when this is already in use by another script")) 
	{
		m_bloodVfxDisabled = val; 
		m_bloodVfxDisabledScriptThreadId = scriptThreadId;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetBloodPtFxUserZoomScale
///////////////////////////////////////////////////////////////////////////////

void			CVfxBlood::SetBloodPtFxUserZoomScale		(float val, scrThreadId scriptThreadId) 
{
	if (vfxVerifyf(m_bloodPtFxUserZoomScaleScriptThreadId==THREAD_INVALID || m_bloodPtFxUserZoomScaleScriptThreadId==scriptThreadId, "trying to set blood user zoom when this is already in use by another script")) 
	{
		m_bloodPtFxUserZoomScale = val; 
		m_bloodPtFxUserZoomScaleScriptThreadId = scriptThreadId;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetOverrideWeaponGroupId
///////////////////////////////////////////////////////////////////////////////

void			CVfxBlood::SetOverrideWeaponGroupId			(s32 val, scrThreadId scriptThreadId) 
{
	if (vfxVerifyf(m_overrideWeaponGroupIdScriptThreadId==THREAD_INVALID || m_overrideWeaponGroupIdScriptThreadId==scriptThreadId, "trying to override blood weapon group when this is already in use by another script")) 
	{
		m_overrideWeaponGroupId = val; 
		m_overrideWeaponGroupIdScriptThreadId = scriptThreadId;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  LoadData
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxBlood::LoadData							()
{
	// initialise the data
	for (s32 i=0; i<VFXBLOOD_NUM_PED_MATERIALS; i++)
	{
		for (s32 j=0; j<VFXBLOOD_NUM_WEAPON_GROUPS; j++)
		{
			m_vfxBloodTableSPAlive[i][j].id = 0;
			m_vfxBloodTableSPAlive[i][j].offset[0] = -1;
			m_vfxBloodTableSPAlive[i][j].count[0] = -1;
			m_vfxBloodTableSPAlive[i][j].offset[1] = -1;
			m_vfxBloodTableSPAlive[i][j].count[1] = -1;

			m_vfxBloodTableSPDead[i][j].id = 0;
			m_vfxBloodTableSPDead[i][j].offset[0] = -1;
			m_vfxBloodTableSPDead[i][j].count[0] = -1;
			m_vfxBloodTableSPDead[i][j].offset[1] = -1;
			m_vfxBloodTableSPDead[i][j].count[1] = -1;

			m_vfxBloodTableMPAlive[i][j].id = 0;
			m_vfxBloodTableMPAlive[i][j].offset[0] = -1;
			m_vfxBloodTableMPAlive[i][j].count[0] = -1;
			m_vfxBloodTableMPAlive[i][j].offset[1] = -1;
			m_vfxBloodTableMPAlive[i][j].count[1] = -1;

			m_vfxBloodTableMPDead[i][j].id = 0;
			m_vfxBloodTableMPDead[i][j].offset[0] = -1;
			m_vfxBloodTableMPDead[i][j].count[0] = -1;
			m_vfxBloodTableMPDead[i][j].offset[1] = -1;
			m_vfxBloodTableMPDead[i][j].count[1] = -1;
		}
	}

	m_numBloodEntryVfxInfos = 0;
	m_numBloodExitVfxInfos = 0;

	// read in the data
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::BLOODFX_FILE);
	while(DATAFILEMGR.IsValid(pData))
	{
		fiStream* stream = ASSET.Open(pData->m_filename, "dat", true);
		vfxAssertf(stream, "cannot open data file");

		if (stream)
		{		
			// initialise the tokens
			fiAsciiTokenizer token;
			token.Init("bloodFx", stream);

			// count the number of materials
			s32 phase = -1;	
			s32 currBloodTableId = 0;
			s32* pNumVfxBloodInfos = NULL;
			VfxBloodInfo_s* pVfxBloodInfo = NULL;
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
				if (stricmp(charBuff, "BLOODFX_TABLE_START")==0)
				{
					currBloodTableId = 0;
					phase = 0;
					continue;
				}
				else if (stricmp(charBuff, "BLOODFX_TABLE_END")==0)
				{
					vfxAssertf(currBloodTableId==VFXBLOOD_NUM_PED_MATERIALS, "wrong number of ped materials in data file");
					phase = -1;

					for (int i=0; i<VFXBLOOD_NUM_PED_MATERIALS; i++)
					{
						for (int j=0; j<VFXBLOOD_NUM_WEAPON_GROUPS; j++)
						{
							m_vfxBloodTableSPDead[i][j] = m_vfxBloodTableSPAlive[i][j];
							m_vfxBloodTableMPAlive[i][j] = m_vfxBloodTableSPAlive[i][j];
							m_vfxBloodTableMPDead[i][j] = m_vfxBloodTableSPAlive[i][j];
						}
					}

					continue;
				}
				else if (stricmp(charBuff, "BLOODFX_TABLE_ALIVE_START")==0)
				{
					currBloodTableId = 0;
					phase = 0;
					continue;
				}
				else if (stricmp(charBuff, "BLOODFX_TABLE_ALIVE_END")==0)
				{
					vfxAssertf(currBloodTableId==VFXBLOOD_NUM_PED_MATERIALS, "wrong number of ped materials in data file");
					phase = -1;
					continue;
				}
				else if (stricmp(charBuff, "BLOODFX_TABLE_DEAD_START")==0)
				{
					currBloodTableId = 0;
					phase = 1;
					continue;
				}
				else if (stricmp(charBuff, "BLOODFX_TABLE_DEAD_END")==0)
				{
					vfxAssertf(currBloodTableId==VFXBLOOD_NUM_PED_MATERIALS, "wrong number of ped materials in data file");
					phase = -1;
					continue;
				}
				else if (stricmp(charBuff, "BLOODFX_TABLE_ALIVE_MP_START")==0)
				{
					currBloodTableId = 0;
					phase = 2;
					continue;
				}
				else if (stricmp(charBuff, "BLOODFX_TABLE_ALIVE_MP_END")==0)
				{
					vfxAssertf(currBloodTableId==VFXBLOOD_NUM_PED_MATERIALS, "wrong number of ped materials in data file");
					phase = -1;
					continue;
				}
				else if (stricmp(charBuff, "BLOODFX_TABLE_DEAD_MP_START")==0)
				{
					currBloodTableId = 0;
					phase = 3;
					continue;
				}
				else if (stricmp(charBuff, "BLOODFX_TABLE_DEAD_MP_END")==0)
				{
					vfxAssertf(currBloodTableId==VFXBLOOD_NUM_PED_MATERIALS, "wrong number of ped materials in data file");
					phase = -1;
					continue;
				}
				else if (stricmp(charBuff, "BLOODFX_ENTRY_INFO_START")==0)
				{
					pNumVfxBloodInfos = &m_numBloodEntryVfxInfos;
					pVfxBloodInfo = m_bloodEntryFxInfo;
					phase = 4;
					continue;
				}
				else if (stricmp(charBuff, "BLOODFX_ENTRY_INFO_END")==0)
				{
					phase = -1;
					continue;
				}
				else if (stricmp(charBuff, "BLOODFX_EXIT_INFO_START")==0)
				{
					pNumVfxBloodInfos = &m_numBloodExitVfxInfos;
					pVfxBloodInfo = m_bloodExitFxInfo;
					phase = 5;
					continue;
				}
				else if (stricmp(charBuff, "BLOODFX_EXIT_INFO_END")==0)
				{
					break;
				}

				// phase 0 - material effect table SP (alive)
				if (phase==0)
				{
#if __ASSERT
					phMaterialMgr::Id mtlId = PGTAMATERIALMGR->GetMaterialId(PGTAMATERIALMGR->FindMaterial(charBuff));
					phMaterialMgr::Id basePedMtlId = PGTAMATERIALMGR->g_idButtocks;
					vfxAssertf(mtlId==currBloodTableId+basePedMtlId, "materials don't match in data file");
#endif	
					for (s32 i=0; i<VFXBLOOD_NUM_WEAPON_GROUPS; i++)
					{
						m_vfxBloodTableSPAlive[currBloodTableId][i].id = token.GetInt();
					}
					token.SkipToEndOfLine();

					currBloodTableId++;
				}
				// phase 1 - material effect table SP (dead)
				else if (phase==1)
				{
#if __ASSERT
					phMaterialMgr::Id mtlId = PGTAMATERIALMGR->GetMaterialId(PGTAMATERIALMGR->FindMaterial(charBuff));
					phMaterialMgr::Id basePedMtlId = PGTAMATERIALMGR->g_idButtocks;
					vfxAssertf(mtlId==currBloodTableId+basePedMtlId, "materials don't match in data file");
#endif	
					for (s32 i=0; i<VFXBLOOD_NUM_WEAPON_GROUPS; i++)
					{
						m_vfxBloodTableSPDead[currBloodTableId][i].id = token.GetInt();
					}
					token.SkipToEndOfLine();

					currBloodTableId++;
				}
				// phase 2 - material effect table MP (alive)
				else if (phase==2)
				{
#if __ASSERT
					phMaterialMgr::Id mtlId = PGTAMATERIALMGR->GetMaterialId(PGTAMATERIALMGR->FindMaterial(charBuff));
					phMaterialMgr::Id basePedMtlId = PGTAMATERIALMGR->g_idButtocks;
					vfxAssertf(mtlId==currBloodTableId+basePedMtlId, "materials don't match in data file");
#endif	
					for (s32 i=0; i<VFXBLOOD_NUM_WEAPON_GROUPS; i++)
					{
						m_vfxBloodTableMPAlive[currBloodTableId][i].id = token.GetInt();
					}
					token.SkipToEndOfLine();

					currBloodTableId++;
				}
				// phase 3 - material effect table MP (dead)
				else if (phase==3)
				{
#if __ASSERT
					phMaterialMgr::Id mtlId = PGTAMATERIALMGR->GetMaterialId(PGTAMATERIALMGR->FindMaterial(charBuff));
					phMaterialMgr::Id basePedMtlId = PGTAMATERIALMGR->g_idButtocks;
					vfxAssertf(mtlId==currBloodTableId+basePedMtlId, "materials don't match in data file");
#endif	
					for (s32 i=0; i<VFXBLOOD_NUM_WEAPON_GROUPS; i++)
					{
						m_vfxBloodTableMPDead[currBloodTableId][i].id = token.GetInt();
					}
					token.SkipToEndOfLine();

					currBloodTableId++;
				}
				// phase 4, 5- effect info
				else if (phase==4 || phase==5)
				{
					vfxAssertf(*pNumVfxBloodInfos<VFXBLOOD_MAX_INFOS, "no room to add new blood info");
					if (*pNumVfxBloodInfos<VFXBLOOD_MAX_INFOS)
					{
						// id
						pVfxBloodInfo[*pNumVfxBloodInfos].id = atoi(&charBuff[0]);

						// blood probability
						if (m_version>=9.0f)
						{
							pVfxBloodInfo[*pNumVfxBloodInfos].probabilityBlood = token.GetFloat();
						}
						else
						{
							pVfxBloodInfo[*pNumVfxBloodInfos].probabilityBlood = 1.0f;
						}

						// splat decal data
						s32 decalDataId = token.GetInt();
						if (decalDataId>0)
						{
							g_decalMan.FindRenderSettingInfo(decalDataId, pVfxBloodInfo[*pNumVfxBloodInfos].splatNormRenderSettingIndex, pVfxBloodInfo[*pNumVfxBloodInfos].splatNormRenderSettingCount);					
						}
						else
						{
							pVfxBloodInfo[*pNumVfxBloodInfos].splatNormRenderSettingIndex = -1;
							pVfxBloodInfo[*pNumVfxBloodInfos].splatNormRenderSettingCount = 0;
						}

						decalDataId = token.GetInt();
						if (decalDataId>0)
						{
							g_decalMan.FindRenderSettingInfo(decalDataId, pVfxBloodInfo[*pNumVfxBloodInfos].splatSoakRenderSettingIndex, pVfxBloodInfo[*pNumVfxBloodInfos].splatSoakRenderSettingCount);					
						}
						else
						{
							pVfxBloodInfo[*pNumVfxBloodInfos].splatSoakRenderSettingIndex = -1;
							pVfxBloodInfo[*pNumVfxBloodInfos].splatSoakRenderSettingCount = 0;
						}

						// spray decal data
						if (m_version>=7.0f)
						{
							s32 decalDataId = token.GetInt();
							if (decalDataId>0)
							{
								g_decalMan.FindRenderSettingInfo(decalDataId, pVfxBloodInfo[*pNumVfxBloodInfos].sprayNormRenderSettingIndex, pVfxBloodInfo[*pNumVfxBloodInfos].sprayNormRenderSettingCount);					
							}
							else
							{
								pVfxBloodInfo[*pNumVfxBloodInfos].sprayNormRenderSettingIndex = -1;
								pVfxBloodInfo[*pNumVfxBloodInfos].sprayNormRenderSettingCount = 0;
							}

							decalDataId = token.GetInt();
							if (decalDataId>0)
							{
								g_decalMan.FindRenderSettingInfo(decalDataId, pVfxBloodInfo[*pNumVfxBloodInfos].spraySoakRenderSettingIndex, pVfxBloodInfo[*pNumVfxBloodInfos].spraySoakRenderSettingCount);					
							}
							else
							{
								pVfxBloodInfo[*pNumVfxBloodInfos].spraySoakRenderSettingIndex = -1;
								pVfxBloodInfo[*pNumVfxBloodInfos].spraySoakRenderSettingCount = 0;
							}
						}
						else
						{
							pVfxBloodInfo[*pNumVfxBloodInfos].sprayNormRenderSettingIndex = -1;
							pVfxBloodInfo[*pNumVfxBloodInfos].sprayNormRenderSettingCount = 0;
							pVfxBloodInfo[*pNumVfxBloodInfos].spraySoakRenderSettingIndex = -1;
							pVfxBloodInfo[*pNumVfxBloodInfos].spraySoakRenderSettingCount = 0;
						}

						// mist decal data
						if (m_version>=7.0f)
						{
							s32 decalDataId = token.GetInt();
							if (decalDataId>0)
							{
								g_decalMan.FindRenderSettingInfo(decalDataId, pVfxBloodInfo[*pNumVfxBloodInfos].mistNormRenderSettingIndex, pVfxBloodInfo[*pNumVfxBloodInfos].mistNormRenderSettingCount);					
							}
							else
							{
								pVfxBloodInfo[*pNumVfxBloodInfos].mistNormRenderSettingIndex = -1;
								pVfxBloodInfo[*pNumVfxBloodInfos].mistNormRenderSettingCount = 0;
							}

							decalDataId = token.GetInt();
							if (decalDataId>0)
							{
								g_decalMan.FindRenderSettingInfo(decalDataId, pVfxBloodInfo[*pNumVfxBloodInfos].mistSoakRenderSettingIndex, pVfxBloodInfo[*pNumVfxBloodInfos].mistSoakRenderSettingCount);					
							}
							else
							{
								pVfxBloodInfo[*pNumVfxBloodInfos].mistSoakRenderSettingIndex = -1;
								pVfxBloodInfo[*pNumVfxBloodInfos].mistSoakRenderSettingCount = 0;
							}
						}
						else
						{
							pVfxBloodInfo[*pNumVfxBloodInfos].mistNormRenderSettingIndex = -1;
							pVfxBloodInfo[*pNumVfxBloodInfos].mistNormRenderSettingCount = 0;
							pVfxBloodInfo[*pNumVfxBloodInfos].mistSoakRenderSettingIndex = -1;
							pVfxBloodInfo[*pNumVfxBloodInfos].mistSoakRenderSettingCount = 0;
						}

						// colour tint
						pVfxBloodInfo[*pNumVfxBloodInfos].decalColR = (u8)token.GetInt();
						pVfxBloodInfo[*pNumVfxBloodInfos].decalColG = (u8)token.GetInt();
						pVfxBloodInfo[*pNumVfxBloodInfos].decalColB = (u8)token.GetInt();

						if (m_version>=7.0f)
						{
							// spray dot product threshold
							pVfxBloodInfo[*pNumVfxBloodInfos].sprayDotProductThresh = token.GetFloat();

							// mist t value threshold
							pVfxBloodInfo[*pNumVfxBloodInfos].mistTValueThresh = token.GetFloat();
							
							// lod ranges
							pVfxBloodInfo[*pNumVfxBloodInfos].lodRangeHi = token.GetFloat();
							pVfxBloodInfo[*pNumVfxBloodInfos].lodRangeLo = token.GetFloat();

							// num probes
							pVfxBloodInfo[*pNumVfxBloodInfos].numProbesHi = (u8)token.GetInt();
							pVfxBloodInfo[*pNumVfxBloodInfos].numProbesLo = (u8)token.GetInt();
							
							// probe variation
							pVfxBloodInfo[*pNumVfxBloodInfos].probeVariation = token.GetFloat();
						}
						else
						{
							// lod ranges
							pVfxBloodInfo[*pNumVfxBloodInfos].lodRangeHi = 0.0f;
							pVfxBloodInfo[*pNumVfxBloodInfos].lodRangeLo = 0.0f;

							// num probes
							pVfxBloodInfo[*pNumVfxBloodInfos].numProbesHi = 1;
							pVfxBloodInfo[*pNumVfxBloodInfos].numProbesLo = 1;

							// probe variation
							pVfxBloodInfo[*pNumVfxBloodInfos].probeVariation = 0.0f;
						}
	
						// texture settings
						pVfxBloodInfo[*pNumVfxBloodInfos].probeDistA = token.GetFloat();
						pVfxBloodInfo[*pNumVfxBloodInfos].probeDistB = token.GetFloat();

						if (m_version>=7.0f)
						{
							pVfxBloodInfo[*pNumVfxBloodInfos].sizeMinAHi = token.GetFloat();
							pVfxBloodInfo[*pNumVfxBloodInfos].sizeMaxAHi = token.GetFloat();
							pVfxBloodInfo[*pNumVfxBloodInfos].sizeMinALo = token.GetFloat();
							pVfxBloodInfo[*pNumVfxBloodInfos].sizeMaxALo = token.GetFloat();

							pVfxBloodInfo[*pNumVfxBloodInfos].sizeMinBHi = token.GetFloat();
							pVfxBloodInfo[*pNumVfxBloodInfos].sizeMaxBHi = token.GetFloat();
							pVfxBloodInfo[*pNumVfxBloodInfos].sizeMinBLo = token.GetFloat();
							pVfxBloodInfo[*pNumVfxBloodInfos].sizeMaxBLo = token.GetFloat();
						}
						else
						{
							pVfxBloodInfo[*pNumVfxBloodInfos].sizeMinAHi = token.GetFloat();
							pVfxBloodInfo[*pNumVfxBloodInfos].sizeMaxAHi = token.GetFloat();
							pVfxBloodInfo[*pNumVfxBloodInfos].sizeMinALo = pVfxBloodInfo[*pNumVfxBloodInfos].sizeMinAHi;
							pVfxBloodInfo[*pNumVfxBloodInfos].sizeMaxALo = pVfxBloodInfo[*pNumVfxBloodInfos].sizeMaxAHi;

							pVfxBloodInfo[*pNumVfxBloodInfos].sizeMinBHi = token.GetFloat();
							pVfxBloodInfo[*pNumVfxBloodInfos].sizeMaxBHi = token.GetFloat();
							pVfxBloodInfo[*pNumVfxBloodInfos].sizeMinBLo = pVfxBloodInfo[*pNumVfxBloodInfos].sizeMinBHi;
							pVfxBloodInfo[*pNumVfxBloodInfos].sizeMaxBLo = pVfxBloodInfo[*pNumVfxBloodInfos].sizeMaxBHi;
						}

						pVfxBloodInfo[*pNumVfxBloodInfos].lifeMin = token.GetFloat();
						pVfxBloodInfo[*pNumVfxBloodInfos].lifeMax = token.GetFloat();
						pVfxBloodInfo[*pNumVfxBloodInfos].fadeInTime = token.GetFloat();
						pVfxBloodInfo[*pNumVfxBloodInfos].growthTime = token.GetFloat();
						pVfxBloodInfo[*pNumVfxBloodInfos].growthMultMin = token.GetFloat();
						pVfxBloodInfo[*pNumVfxBloodInfos].growthMultMax = token.GetFloat();

						// fx system
						token.GetToken(charBuff, 128);		
						if (stricmp(charBuff, "-")==0)
						{	
							pVfxBloodInfo[*pNumVfxBloodInfos].ptFxBloodHashName = (u32)0;					
						}
						else
						{
							pVfxBloodInfo[*pNumVfxBloodInfos].ptFxBloodHashName = atHashWithStringNotFinal(charBuff);
						}

						token.GetToken(charBuff, 128);		
						if (stricmp(charBuff, "-")==0)
						{	
							pVfxBloodInfo[*pNumVfxBloodInfos].ptFxBloodBodyArmourHashName = (u32)0;					
						}
						else
						{
							pVfxBloodInfo[*pNumVfxBloodInfos].ptFxBloodBodyArmourHashName = atHashWithStringNotFinal(charBuff);
						}

						token.GetToken(charBuff, 128);		
						if (stricmp(charBuff, "-")==0)
						{	
							pVfxBloodInfo[*pNumVfxBloodInfos].ptFxBloodHeavyArmourHashName = (u32)0;					
						}
						else
						{
							pVfxBloodInfo[*pNumVfxBloodInfos].ptFxBloodHeavyArmourHashName = atHashWithStringNotFinal(charBuff);
						}

						if (m_version>=10.0f)
						{
							token.GetToken(charBuff, 128);		
							if (stricmp(charBuff, "-")==0)
							{	
								pVfxBloodInfo[*pNumVfxBloodInfos].ptFxBloodUnderwaterHashName = (u32)0;	
							}
							else
							{
								pVfxBloodInfo[*pNumVfxBloodInfos].ptFxBloodUnderwaterHashName = atHashWithStringNotFinal(charBuff);
							}
						}
						else
						{
							pVfxBloodInfo[*pNumVfxBloodInfos].ptFxBloodUnderwaterHashName = (u32)0;	
						}

						// fx sys evolution times
						pVfxBloodInfo[*pNumVfxBloodInfos].ptFxSizeEvo = token.GetFloat();

						vfxAssertf(pVfxBloodInfo[*pNumVfxBloodInfos].ptFxSizeEvo>=0.0f, "size evo out of range");
						vfxAssertf(pVfxBloodInfo[*pNumVfxBloodInfos].ptFxSizeEvo<=1.0f, "size evo out of range");
						
						// ped damage info
						if (m_version>=9.0f)
						{
							pVfxBloodInfo[*pNumVfxBloodInfos].probabilityDamage = token.GetFloat();
						}
						else
						{
							pVfxBloodInfo[*pNumVfxBloodInfos].probabilityDamage = 1.0f;
						}

						pVfxBloodInfo[*pNumVfxBloodInfos].pedDamageHashName = (u32)0;
						pVfxBloodInfo[*pNumVfxBloodInfos].pedDamageBodyArmourHashName = (u32)0;
						pVfxBloodInfo[*pNumVfxBloodInfos].pedDamageHeavyArmourHashName = (u32)0;
						if (m_version>=8.0f)
						{
							token.GetToken(charBuff, 128);		
							if (stricmp(charBuff, "-")!=0)
								pVfxBloodInfo[*pNumVfxBloodInfos].pedDamageHashName = atHashWithStringNotFinal(charBuff);

							token.GetToken(charBuff, 128);		
							if (stricmp(charBuff, "-")!=0)
								pVfxBloodInfo[*pNumVfxBloodInfos].pedDamageBodyArmourHashName = atHashWithStringNotFinal(charBuff);

							token.GetToken(charBuff, 128);		
							if (stricmp(charBuff, "-")!=0)
								pVfxBloodInfo[*pNumVfxBloodInfos].pedDamageHeavyArmourHashName = atHashWithStringNotFinal(charBuff);
						}

						// patch data
						pVfxBloodInfo[*pNumVfxBloodInfos].ptFxBloodSloMoHashName = pVfxBloodInfo[*pNumVfxBloodInfos].ptFxBloodHashName;

						// increment number of infos
						(*pNumVfxBloodInfos)++;
					}
				}
			}

			stream->Close();
		}

		// Get next file
		pData = DATAFILEMGR.GetNextFile(pData);
	}

	ProcessLookUpData();

	// check the data's integrity
	for (s32 i=0; i<VFXBLOOD_NUM_PED_MATERIALS; i++)
	{
		for (s32 j=0; j<VFXBLOOD_NUM_WEAPON_GROUPS; j++)
		{
			s32 infoIndex = 0;
			GetEntryAliveInfo(i, j, infoIndex);
			GetExitAliveInfo(i, j, infoIndex);
			GetEntryDeadInfo(i, j, infoIndex);
			GetExitDeadInfo(i, j, infoIndex);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  PatchData
///////////////////////////////////////////////////////////////////////////////

void			CVfxBlood::PatchData							(const char* pFilename)
{
	fiStream* stream = ASSET.Open(pFilename, "dat", true);
	vfxAssertf(stream, "cannot open data file");

	if (stream)
	{		
		// initialise the tokens
		fiAsciiTokenizer token;
		token.Init("bloodFxPatch", stream);

		// count the number of materials
		s32 phase = -1;	
		char charBuff[128];

		// read in the version
		token.GetFloat();

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
			if (stricmp(charBuff, "BLOODFX_ENTRY_PATCH_INFO_START")==0)
			{
				phase = 0;
				continue;
			}
			else if (stricmp(charBuff, "BLOODFX_ENTRY_PATCH_INFO_END")==0)
			{
				phase = -1;
				continue;
			}
			else if (stricmp(charBuff, "BLOODFX_EXIT_PATCH_INFO_START")==0)
			{
				phase = 1;
				continue;
			}
			else if (stricmp(charBuff, "BLOODFX_EXIT_PATCH_INFO_END")==0)
			{
				break;
			}

			// phase 0 
			if (phase==0)
			{
				s32 id = atoi(&charBuff[0]);
				token.GetToken(charBuff, 128);	

				for (int i=0; i<m_numBloodEntryVfxInfos; i++)
				{
					if (m_bloodEntryFxInfo[i].id==id)
					{
						if (stricmp(charBuff, "-")==0)
						{	
							m_bloodEntryFxInfo[i].ptFxBloodSloMoHashName = (u32)0;					
						}
						else
						{
							m_bloodEntryFxInfo[i].ptFxBloodSloMoHashName = atHashWithStringNotFinal(charBuff);
						}
					}
				}
			}
			else if (phase==1)
			{
				s32 id = atoi(&charBuff[0]);
				token.GetToken(charBuff, 128);	

				for (int i=0; i<m_numBloodExitVfxInfos; i++)
				{
					if (m_bloodExitFxInfo[i].id==id)
					{
						if (stricmp(charBuff, "-")==0)
						{	
							m_bloodExitFxInfo[i].ptFxBloodSloMoHashName = (u32)0;					
						}
						else
						{
							m_bloodExitFxInfo[i].ptFxBloodSloMoHashName = atHashWithStringNotFinal(charBuff);
						}
					}
				}
			}
		}

		stream->Close();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UnPatchData
///////////////////////////////////////////////////////////////////////////////

void			CVfxBlood::UnPatchData							(const char* UNUSED_PARAM(pFilename))
{
	for (int i=0; i<m_numBloodEntryVfxInfos; i++)
	{
		m_bloodEntryFxInfo[i].ptFxBloodSloMoHashName = m_bloodEntryFxInfo[i].ptFxBloodHashName;
	}

	for (int i=0; i<m_numBloodExitVfxInfos; i++)
	{
		m_bloodExitFxInfo[i].ptFxBloodSloMoHashName = m_bloodExitFxInfo[i].ptFxBloodHashName;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessLookUpData
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxBlood::ProcessLookUpData					()
{
	// PROCESS ENTRY DATA (index 0)
	// fill in the offset and count data in the lookup table
	s32 currId = -1;
	s32 currOffset = 0;
	s32 currCount = 0;
	for (s32 i=0; i<m_numBloodEntryVfxInfos; i++)
	{
		// check if the id is the same
		if (m_bloodEntryFxInfo[i].id == currId)
		{
			// same id - update the info
			currCount++;
		}

		// check if the id has changed 
		if (m_bloodEntryFxInfo[i].id!=currId)
		{
			// new id - store the old info
			StoreLookUpData(0, currId, currOffset, currCount);

			// set up the new info
			currId = m_bloodEntryFxInfo[i].id;
			currOffset = i;
			currCount = 1;
		}
	}

	// store the final info
	StoreLookUpData(0, currId, currOffset, currCount);

	// PROCESS EXIT DATA (index 1)
	// fill in the offset and count data in the lookup table
	currId = -1;
	currOffset = 0;
	currCount = 0;
	for (s32 i=0; i<m_numBloodExitVfxInfos; i++)
	{
		// check if the id is the same
		if (m_bloodExitFxInfo[i].id == currId)
		{
			// same id - update the info
			currCount++;
		}

		// check if the id has changed 
		if (m_bloodExitFxInfo[i].id!=currId)
		{
			// new id - store the old info
			StoreLookUpData(1, currId, currOffset, currCount);

			// set up the new info
			currId = m_bloodExitFxInfo[i].id;
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

void		 	CVfxBlood::StoreLookUpData					(s32 index, s32 currId, s32 currOffset, s32 currCount)
{
	if (currId>-1)
	{
		for (s32 j=0; j<VFXBLOOD_NUM_PED_MATERIALS; j++)
		{
			for (s32 k=0; k<VFXBLOOD_NUM_WEAPON_GROUPS; k++)
			{
				if (m_vfxBloodTableSPAlive[j][k].id==currId)
				{
					vfxAssertf(m_vfxBloodTableSPAlive[j][k].offset[index]==-1, "blood table offset entry already set");
					vfxAssertf(m_vfxBloodTableSPAlive[j][k].count[index]==-1, "blood table count entry already set");
					m_vfxBloodTableSPAlive[j][k].offset[index] = currOffset;
					m_vfxBloodTableSPAlive[j][k].count[index] = currCount;
				}

				if (m_vfxBloodTableSPDead[j][k].id==currId)
				{
					vfxAssertf(m_vfxBloodTableSPDead[j][k].offset[index]==-1, "blood table offset entry already set");
					vfxAssertf(m_vfxBloodTableSPDead[j][k].count[index]==-1, "blood table count entry already set");
					m_vfxBloodTableSPDead[j][k].offset[index] = currOffset;
					m_vfxBloodTableSPDead[j][k].count[index] = currCount;
				}

				if (m_vfxBloodTableMPAlive[j][k].id==currId)
				{
					vfxAssertf(m_vfxBloodTableMPAlive[j][k].offset[index]==-1, "blood table offset entry already set");
					vfxAssertf(m_vfxBloodTableMPAlive[j][k].count[index]==-1, "blood table count entry already set");
					m_vfxBloodTableMPAlive[j][k].offset[index] = currOffset;
					m_vfxBloodTableMPAlive[j][k].count[index] = currCount;
				}

				if (m_vfxBloodTableMPDead[j][k].id==currId)
				{
					vfxAssertf(m_vfxBloodTableMPDead[j][k].offset[index]==-1, "blood table offset entry already set");
					vfxAssertf(m_vfxBloodTableMPDead[j][k].count[index]==-1, "blood table count entry already set");
					m_vfxBloodTableMPDead[j][k].offset[index] = currOffset;
					m_vfxBloodTableMPDead[j][k].count[index] = currCount;
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
// GetEntryAliveInfo
///////////////////////////////////////////////////////////////////////////////

VfxBloodInfo_s*	CVfxBlood::GetEntryAliveInfo						(phMaterialMgr::Id correctedMtlId, s32 weaponGroupId, s32& infoIndex)
{
	if (m_overrideWeaponGroupId>-1)
	{
		weaponGroupId = m_overrideWeaponGroupId;
	}

	// get the id from the table
	vfxAssertf(correctedMtlId<VFXBLOOD_NUM_PED_MATERIALS, "material id out of range");
	vfxAssertf(weaponGroupId<VFXBLOOD_NUM_WEAPON_GROUPS, "weapon group id out of range");
	s32 offset = m_vfxBloodTableSPAlive[correctedMtlId][weaponGroupId].offset[0];
	if (NetworkInterface::IsGameInProgress() BANK_ONLY(|| m_forceMPData))
	{
		offset = m_vfxBloodTableMPAlive[correctedMtlId][weaponGroupId].offset[0];
	}

	// check for no infos
	if (offset==-1)
	{
		return NULL;
	}

	s32 count = m_vfxBloodTableSPAlive[correctedMtlId][weaponGroupId].count[0];
	if (NetworkInterface::IsGameInProgress() BANK_ONLY(|| m_forceMPData))
	{
		count = m_vfxBloodTableMPAlive[correctedMtlId][weaponGroupId].count[0];
	}
	s32 rand = g_DrawRand.GetRanged(offset, offset+count-1);

	infoIndex = rand;
	return &m_bloodEntryFxInfo[rand];
}


///////////////////////////////////////////////////////////////////////////////
// GetExitAliveInfo
///////////////////////////////////////////////////////////////////////////////

VfxBloodInfo_s*	CVfxBlood::GetExitAliveInfo						(phMaterialMgr::Id correctedMtlId, s32 weaponGroupId, s32& infoIndex)
{
	if (m_overrideWeaponGroupId>-1)
	{
		weaponGroupId = m_overrideWeaponGroupId;
	}

	// get the id from the table
	vfxAssertf(correctedMtlId<VFXBLOOD_NUM_PED_MATERIALS, "material id out of range");
	vfxAssertf(weaponGroupId<VFXBLOOD_NUM_WEAPON_GROUPS, "weapon group id out of range");
	s32 offset = m_vfxBloodTableSPAlive[correctedMtlId][weaponGroupId].offset[1];
	if (NetworkInterface::IsGameInProgress() BANK_ONLY(|| m_forceMPData))
	{
		offset = m_vfxBloodTableMPAlive[correctedMtlId][weaponGroupId].offset[1];
	}

	// check for no infos
	if (offset==-1)
	{
		return NULL;
	}

	s32 count = m_vfxBloodTableSPAlive[correctedMtlId][weaponGroupId].count[1];
	if (NetworkInterface::IsGameInProgress() BANK_ONLY(|| m_forceMPData))
	{
		count = m_vfxBloodTableMPAlive[correctedMtlId][weaponGroupId].count[1];
	}
	s32 rand = g_DrawRand.GetRanged(offset, offset+count-1);

	infoIndex = rand;
	return &m_bloodExitFxInfo[rand];
}


///////////////////////////////////////////////////////////////////////////////
// GetEntryDeadInfo
///////////////////////////////////////////////////////////////////////////////

VfxBloodInfo_s*	CVfxBlood::GetEntryDeadInfo						(phMaterialMgr::Id correctedMtlId, s32 weaponGroupId, s32& infoIndex)
{
	if (m_overrideWeaponGroupId>-1)
	{
		weaponGroupId = m_overrideWeaponGroupId;
	}

	// get the id from the table
	vfxAssertf(correctedMtlId<VFXBLOOD_NUM_PED_MATERIALS, "material id out of range");
	vfxAssertf(weaponGroupId<VFXBLOOD_NUM_WEAPON_GROUPS, "weapon group id out of range");
	s32 offset = m_vfxBloodTableSPDead[correctedMtlId][weaponGroupId].offset[0];
	if (NetworkInterface::IsGameInProgress() BANK_ONLY(|| m_forceMPData))
	{
		offset = m_vfxBloodTableMPDead[correctedMtlId][weaponGroupId].offset[0];
	}

	// check for no infos
	if (offset==-1)
	{
		return NULL;
	}

	s32 count = m_vfxBloodTableSPDead[correctedMtlId][weaponGroupId].count[0];
	if (NetworkInterface::IsGameInProgress() BANK_ONLY(|| m_forceMPData))
	{
		count = m_vfxBloodTableMPDead[correctedMtlId][weaponGroupId].count[0];
	}
	s32 rand = g_DrawRand.GetRanged(offset, offset+count-1);

	infoIndex = rand;
	return &m_bloodEntryFxInfo[rand];
}


///////////////////////////////////////////////////////////////////////////////
// GetExitDeadInfo
///////////////////////////////////////////////////////////////////////////////

VfxBloodInfo_s*	CVfxBlood::GetExitDeadInfo						(phMaterialMgr::Id correctedMtlId, s32 weaponGroupId, s32& infoIndex)
{
	if (m_overrideWeaponGroupId>-1)
	{
		weaponGroupId = m_overrideWeaponGroupId;
	}

	// get the id from the table
	vfxAssertf(correctedMtlId<VFXBLOOD_NUM_PED_MATERIALS, "material id out of range");
	vfxAssertf(weaponGroupId<VFXBLOOD_NUM_WEAPON_GROUPS, "weapon group id out of range");
	s32 offset = m_vfxBloodTableSPDead[correctedMtlId][weaponGroupId].offset[1];
	if (NetworkInterface::IsGameInProgress() BANK_ONLY(|| m_forceMPData))
	{
		offset = m_vfxBloodTableMPDead[correctedMtlId][weaponGroupId].offset[1];
	}

	// check for no infos
	if (offset==-1)
	{
		return NULL;
	}

	s32 count = m_vfxBloodTableSPDead[correctedMtlId][weaponGroupId].count[1];
	if (NetworkInterface::IsGameInProgress() BANK_ONLY(|| m_forceMPData))
	{
		count = m_vfxBloodTableMPDead[correctedMtlId][weaponGroupId].count[1];
	}
	s32 rand = g_DrawRand.GetRanged(offset, offset+count-1);

	infoIndex = rand;
	return &m_bloodExitFxInfo[rand];
}


///////////////////////////////////////////////////////////////////////////////
//  InitBloodPool
///////////////////////////////////////////////////////////////////////////////

bool			CVfxBlood::InitBloodPool						(CPed* pPed, CTaskTimer& timer, float& startSize, float& endSize, float& growRate)
{
	timer = g_DrawRand.GetRanged(VFXBLOOD_POOL_TIMER_MIN, VFXBLOOD_POOL_TIMER_MAX);
	startSize = g_DrawRand.GetRanged(VFXBLOOD_POOL_STARTSIZE_MIN, VFXBLOOD_POOL_STARTSIZE_MAX);
	endSize = g_DrawRand.GetRanged(VFXBLOOD_POOL_ENDSIZE_MIN, VFXBLOOD_POOL_ENDSIZE_MAX);
	growRate = g_DrawRand.GetRanged(VFXBLOOD_POOL_GROWRATE_MIN, VFXBLOOD_POOL_GROWRATE_MAX);

	bool pedWasShot = pPed->GetLastSignificantShotBoneTag()>-1;

	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pPed->GetCauseOfDeath());

	bool pedWasClubbed = pWeaponInfo && pWeaponInfo->GetIsClub();

	bool pedWasStabbed = pWeaponInfo && (pWeaponInfo->GetIsBlade() || pWeaponInfo->GetIsHatchet() || pWeaponInfo->GetIsMachete());

	bool pedWasMauled = false;

	CEntity* pDeathSource = pPed->GetSourceOfDeath();
	if (pDeathSource && pDeathSource->GetIsTypePed())
	{
		CPed* pDeathPed = static_cast<CPed*>(pDeathSource);
		if (pDeathPed)
		{
			pedWasMauled = pDeathPed->GetPedType()==PEDTYPE_ANIMAL;
		}
	}

	return pedWasShot || pedWasStabbed || pedWasMauled || (pedWasClubbed && g_DrawRand.GetFloat()<0.6f);
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessBloodPool
///////////////////////////////////////////////////////////////////////////////

void			CVfxBlood::ProcessBloodPool						(CPed* pPed, float poolStartSize, float poolEndSize, float poolGrowRate)
{
	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);
	if (pVfxPedInfo==NULL || !pVfxPedInfo->GetBloodPoolVfxEnabled())
	{
		return;
	}

	if (!pPed->GetIsInVehicle() && 
		pPed->GetPedVfx()!=NULL && 
		!pPed->GetPedVfx()->GetBloodVfxDisabled())
	{
		if (pPed->GetIsInWater())
		{
			UpdatePtFxBloodPoolInWater(pPed);
		}
		else
		{
			// do a line test straight down from the ped to get the ground position
			if (pPed->GetSkeleton())
			{
				s32 boneTag = BONETAG_SPINE0;
				if (pPed->GetLastSignificantShotBoneTag()>-1)
				{
					boneTag = pPed->GetLastSignificantShotBoneTag();
				}
				Assert((unsigned)boneTag < 65536);

				int boneIndex = -1;
				pPed->GetSkeletonData().ConvertBoneIdToIndex((rage::u16)boneTag, boneIndex);

				Mat34V vBoneMtx;
				CVfxHelper::GetMatrixFromBoneIndex(vBoneMtx, pPed, boneIndex);

				Vec3V vStartPos = vBoneMtx.GetCol3();
				Vec3V vEndPos = vStartPos - Vec3V(0.0f, 0.0f, 2.0f);

				float bloodPoolSizeMult = pVfxPedInfo->GetBloodPoolVfxSizeMultiplier();
				m_asyncProbeMgr.TriggerPoolProbe(vStartPos, vEndPos, pPed, poolStartSize*bloodPoolSizeMult, poolEndSize*bloodPoolSizeMult, poolGrowRate);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  DoVfxBlood
///////////////////////////////////////////////////////////////////////////////

void			CVfxBlood::DoVfxBlood							(const VfxCollInfo_s& vfxCollInfo, const CEntity* pFiringEntity)
{
	if (m_bloodVfxDisabled)
	{
		return;
	}

	CPed* pPedShot = NULL;
	eAnimBoneTag boneTagShot = BONETAG_INVALID;
	if (vfxVerifyf(vfxCollInfo.regdEnt->GetIsTypePed(), "trying to play blood vfx on a non ped"))
	{
		pPedShot = static_cast<CPed*>(const_cast<CEntity*>(vfxCollInfo.regdEnt.Get()));
		boneTagShot = pPedShot->GetBoneTagFromRagdollComponent(vfxCollInfo.componentId);
	}

	if (pPedShot==NULL)
	{
		return;
	}

	// get the firing ped
	const CPed* pFiringPed = NULL;
	bool firingPedIsPlayer = false;
	if (pFiringEntity && pFiringEntity->GetIsTypePed())
	{
		pFiringPed = static_cast<const CPed*>(pFiringEntity);	
		if (pFiringPed && pFiringPed->IsPlayer())
		{
			firingPedIsPlayer = true;
		}
	}

	bool bIsMPGamePlayer = NetworkInterface::IsGameInProgress() ? pPedShot->IsPlayer() : false;

	if (pPedShot->GetPedVfx())
	{
		// don't process blood for invisible peds (non-players) or peds that have their blood disabled - players should always have blood applied - otherwise damage is incorrect when meeting up
		if ((!bIsMPGamePlayer && !pPedShot->GetIsVisible()) || pPedShot->GetPedVfx()->GetBloodVfxDisabled())
		{
			return;
		}
		
		// don't process blood for melee hits when the victim is blocking 
		// unless the hit is to the head
		if (vfxCollInfo.weaponGroup <= WEAPON_EFFECT_GROUP_MELEE_GENERIC)
		{
			if (boneTagShot!=BONETAG_HEAD)
			{
				const CTaskMeleeActionResult* pHitPedCurrentSimpleMeleeActionResultTask = pPedShot->GetPedIntelligence()->GetTaskMeleeActionResult();
				if(pHitPedCurrentSimpleMeleeActionResultTask && pHitPedCurrentSimpleMeleeActionResultTask->IsDoingBlock())
				{
					return;
				}
			}
		}
	}

#if __BANK
	if (m_renderBloodVfxInfo)
	{
		char materialIdTxt[64] = "";
		PGTAMATERIALMGR->GetMaterialName(PGTAMATERIALMGR->UnpackMtlId(vfxCollInfo.materialId), materialIdTxt, sizeof(materialIdTxt));
		const char* pWeaponGroupTxt = GetWeaponEffectGroupName(vfxCollInfo.weaponGroup);
		char debugTxt[128];
		formatf(debugTxt, 128, "%s v %s", materialIdTxt, pWeaponGroupTxt);
		grcDebugDraw::Sphere(vfxCollInfo.vPositionWld, 0.05f, Color32(1.0f, 0.0f, 0.0f, 1.0f), true, -m_renderBloodVfxInfoTimer);
		grcDebugDraw::Text(vfxCollInfo.vPositionWld, Color32(1.0f, 1.0f, 0.0f, 1.0f), 0, 10, debugTxt, false, -m_renderBloodVfxInfoTimer);
	}
#endif

	phMaterialMgr::Id correctedMtlId = vfxCollInfo.materialId - PGTAMATERIALMGR->g_idButtocks;
	if (correctedMtlId>=VFXBLOOD_NUM_PED_MATERIALS)
	{
		return;
	}
	
	// get the distance to the closest camera
	float fxDistSqr = CVfxHelper::GetDistSqrToCamera(vfxCollInfo.vPositionWld);

	// check if this has hit body armour
	bool hitBodyArmour = false;
	if (pPedShot->GetArmour()>0.0f)
	{
		// check for spine0-3 or either clavicle
		if (vfxCollInfo.materialId==PGTAMATERIALMGR->g_idSpine0 || 
			vfxCollInfo.materialId==PGTAMATERIALMGR->g_idSpine1 ||
			vfxCollInfo.materialId==PGTAMATERIALMGR->g_idSpine2 ||
			vfxCollInfo.materialId==PGTAMATERIALMGR->g_idSpine3 ||
			vfxCollInfo.materialId==PGTAMATERIALMGR->g_idClavicleLeft ||
			vfxCollInfo.materialId==PGTAMATERIALMGR->g_idClavicleRight)
		{
			hitBodyArmour = true;
		}
	}

	// check if this has hit heavy armour
	bool hitHeavyArmour = pPedShot->IsBoneArmoured(boneTagShot) || pPedShot->IsBoneHelmeted(boneTagShot);
	bool hitLightArmour = pPedShot->IsBoneLightlyArmoured(boneTagShot);

	// check if underwater
	bool isUnderwater = false;
	float waterDepth;
	if (CVfxHelper::GetWaterDepth(vfxCollInfo.vPositionWld, waterDepth))
	{
		isUnderwater = waterDepth>0.0f;
	}

	// if the ped is respawning then override with body armour
	bool isRespawning = pPedShot->GetNetworkObject() && (static_cast<CNetObjPlayer*>(pPedShot->GetNetworkObject()))->GetRespawnInvincibilityTimer()>0;
	if (isRespawning)
	{
		hitBodyArmour = true;
	}

// 	// no blood on certain bones when wearing a helmet
// 	// MN - this shouldn't be required once the peds are set up correctly as IsBoneArmoured (above) should cover this
// 	if (pPedShot->GetHelmetComponent() && pPedShot->GetHelmetComponent()->IsHelmetEnabled())
// 	{
// 		// check for neck and head
// 		if (vfxCollInfo.materialId==PGTAMATERIALMGR->g_idNeck || 
// 			vfxCollInfo.materialId==PGTAMATERIALMGR->g_idHead)
// 		{
// 			hitHeavyArmour = true;
// 		}
// 	}

	bool hitAnyArmour = hitBodyArmour || hitHeavyArmour || hitLightArmour;

	// get the vfx ped info
	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPedShot);

	// process bullet entry blood - in direction of the negative collision direction
#if __BANK
	if (m_disableEntryVfx==false)
#endif
	{
		if (pVfxPedInfo && (pVfxPedInfo->GetBloodEntryPtFxEnabled() || pVfxPedInfo->GetBloodEntryDecalEnabled()))
		{
			s32 infoIndex = 0;
			VfxBloodInfo_s* pVfxBloodInfo = NULL;
			if (pPedShot->ShouldBeDead())
			{
				pVfxBloodInfo = GetEntryDeadInfo(correctedMtlId, vfxCollInfo.weaponGroup, infoIndex);
			}
			else
			{
				pVfxBloodInfo = GetEntryAliveInfo(correctedMtlId, vfxCollInfo.weaponGroup, infoIndex);
			}

			if (pVfxBloodInfo)
			{
				// reduce the chance of blood vfx if a player isn't involved
				float probBlood = pVfxBloodInfo->probabilityBlood;
				float probDamage = pVfxBloodInfo->probabilityDamage;
				float rangeSqrPtFx = pVfxPedInfo->GetBloodEntryPtFxRangeSqr();
				float rangeSqrDecal = pVfxPedInfo->GetBloodEntryDecalRangeSqr();
				if (pPedShot->IsPlayer()==false && firingPedIsPlayer==false)
				{
					// halve the ranges and probabilities
					rangeSqrPtFx *= 0.25f;
					rangeSqrDecal *= 0.25f;
					probBlood *= 0.5f;
					probDamage *= 0.5f;
				}

				// check which vfx systems are active 
				bool doPtFx = bIsMPGamePlayer || g_DrawRand.GetFloat()<=probBlood;
				bool doDecal = doPtFx;
				bool doDamage = bIsMPGamePlayer || g_DrawRand.GetFloat()<=probDamage;

				doPtFx = doPtFx && pVfxPedInfo->GetBloodEntryPtFxEnabled() && fxDistSqr<rangeSqrPtFx;
				doDecal = doDecal && !hitAnyArmour && pVfxPedInfo->GetBloodEntryDecalEnabled() && (bIsMPGamePlayer || fxDistSqr<rangeSqrDecal);
				doDamage = doDamage && !vfxCollInfo.noPedDamage && !isRespawning;

				// reduce the chance of blood decals when the ped is offscreen
				if (doDecal && !bIsMPGamePlayer && pPedShot->GetIsVisibleInSomeViewportThisFrame()==false)
				{
					doDecal = g_DrawRand.GetRanged(0, 10)<3;
				}

				// disable any damage on the hands
				if (vfxCollInfo.materialId==PGTAMATERIALMGR->g_idHandRight || 
					vfxCollInfo.materialId==PGTAMATERIALMGR->g_idHandLeft)
				{
					doDamage = false;
				}

				bool replayPtFx = false;
				bool replayDamage = false;
				if (doPtFx || doDamage)
				{
					TriggerPtFxBlood(pVfxPedInfo, pVfxBloodInfo, vfxCollInfo, fxDistSqr, false, hitBodyArmour, hitHeavyArmour || hitLightArmour, isUnderwater, pPedShot->IsLocalPlayer(), pFiringPed, doPtFx, doDamage, replayPtFx, replayDamage);
#if GTA_REPLAY
					if(CReplayMgr::ShouldRecord())
					{
						CReplayMgr::RecordFx<CPacketBloodFx>(
							CPacketBloodFx(vfxCollInfo, 
							false, 
							false, 
							isUnderwater,
							pPedShot->IsLocalPlayer(),
							infoIndex, 
							fxDistSqr,
							replayPtFx, 
							replayDamage,
							true),
							vfxCollInfo.regdEnt.Get(),
							(CEntity*)pFiringPed);
					}
#endif // GTA_REPLAY
				}

				if (doDecal)
				{
					TriggerDecalBlood(pVfxBloodInfo, vfxCollInfo.vPositionWld, -vfxCollInfo.vDirectionWld, vfxCollInfo.regdEnt, fxDistSqr);
				}


			}
		}
	}

	// process bullet exit blood - in direction of the shot direction 
#if __BANK
	if (m_disableExitVfx==false)
#endif
	{
		if (pVfxPedInfo && (pVfxPedInfo->GetBloodExitPtFxEnabled() || pVfxPedInfo->GetBloodExitDecalEnabled()))
		{
			if (!hitAnyArmour)
			{
				s32 infoIndex = 0;
				VfxBloodInfo_s* pVfxBloodInfo = NULL;
				if (pPedShot->ShouldBeDead())
				{
					pVfxBloodInfo = GetExitDeadInfo(correctedMtlId, vfxCollInfo.weaponGroup, infoIndex);
				}
				else
				{
					pVfxBloodInfo = GetExitAliveInfo(correctedMtlId, vfxCollInfo.weaponGroup, infoIndex);
				}

				if (pVfxBloodInfo)
				{
					// reduce the chance of blood vfx if a player isn't involved
					float probBlood = pVfxBloodInfo->probabilityBlood;
					float probDamage = pVfxBloodInfo->probabilityDamage;
					float rangeSqrPtFx = pVfxPedInfo->GetBloodEntryPtFxRangeSqr();
					float rangeSqrDecal = pVfxPedInfo->GetBloodEntryDecalRangeSqr();
					if (pPedShot->IsPlayer()==false && firingPedIsPlayer==false)
					{
						// halve the ranges and probabilities
						rangeSqrPtFx *= 0.25f;
						rangeSqrDecal *= 0.25f;
						probBlood *= 0.5f;
						probDamage *= 0.5f;
					}

					// check which vfx systems are active 
					bool doPtFx = bIsMPGamePlayer || g_DrawRand.GetFloat()<=probBlood;
					bool doDecal = doPtFx;
					bool doDamage = bIsMPGamePlayer || g_DrawRand.GetFloat()<=probDamage;

					doPtFx = doPtFx && pVfxPedInfo->GetBloodEntryPtFxEnabled() && fxDistSqr<rangeSqrPtFx;
					doDecal = doDecal && !hitAnyArmour && pVfxPedInfo->GetBloodEntryDecalEnabled() && (bIsMPGamePlayer || fxDistSqr<rangeSqrDecal);
					doDamage = doDamage && !vfxCollInfo.noPedDamage;

					bool replayPtFx = false;
					bool replayDamage = false;
					if (doPtFx || doDamage)
					{
						TriggerPtFxBlood(pVfxPedInfo, pVfxBloodInfo, vfxCollInfo, fxDistSqr, true, false, false, isUnderwater, pPedShot->IsLocalPlayer(), pFiringPed, doPtFx, doDamage, replayPtFx, replayDamage);
					
#if GTA_REPLAY
						if(CReplayMgr::ShouldRecord())
						{
							CReplayMgr::RecordFx<CPacketBloodFx>(
								CPacketBloodFx(vfxCollInfo, 
								false, 
								false, 
								isUnderwater,
								pPedShot->IsLocalPlayer(),
								infoIndex, 
								fxDistSqr,
								replayPtFx, 
								replayDamage,
								false),
								vfxCollInfo.regdEnt.Get(),
								(CEntity*)pFiringPed);
						}
#endif // GTA_REPLAY
					}
					
					if (doDecal)
					{
						TriggerDecalBlood(pVfxBloodInfo, vfxCollInfo.vPositionWld, vfxCollInfo.vDirectionWld, vfxCollInfo.regdEnt, fxDistSqr);
					}
				}
			}
		}
	}

	// process blood from the mouth 
	if (!hitAnyArmour)
	{
		// bullets
		if(vfxCollInfo.weaponGroup>=WEAPON_EFFECT_GROUP_PISTOL_SMALL && vfxCollInfo.weaponGroup<=WEAPON_EFFECT_GROUP_RIFLE_SNIPER)
		{
			// spine2, spine3, neck, head
			if (vfxCollInfo.materialId==PGTAMATERIALMGR->g_idSpine2 ||
				vfxCollInfo.materialId==PGTAMATERIALMGR->g_idSpine3 ||
				vfxCollInfo.materialId==PGTAMATERIALMGR->g_idNeck ||
				vfxCollInfo.materialId==PGTAMATERIALMGR->g_idHead)
			{
				if (g_DrawRand.GetFloat()<0.4f)
				{
					TriggerPtFxMouthBlood(pPedShot);
				}
			}
		}
		// melee
		else if (vfxCollInfo.weaponGroup<=WEAPON_EFFECT_GROUP_MELEE_GENERIC)
		{
			// spine1, spine2, head
			if (vfxCollInfo.materialId==PGTAMATERIALMGR->g_idSpine1 ||
				vfxCollInfo.materialId==PGTAMATERIALMGR->g_idSpine2 ||
				vfxCollInfo.materialId==PGTAMATERIALMGR->g_idHead)
			{
				TriggerPtFxMouthBlood(pPedShot);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxFallToDeath
///////////////////////////////////////////////////////////////////////////////

void			CVfxBlood::TriggerPtFxFallToDeath				(CPed* pPed)
{
	if (pPed->GetPedVfx() == NULL || (pPed->GetPedVfx() && pPed->GetPedVfx()->GetBloodVfxDisabled()))
	{
		return;
	}

	// check that there is a camera in range
	if (CVfxHelper::GetDistSqrToCamera(pPed->GetTransform().GetPosition())<VFXRANGE_BLOOD_FALL_TO_DEATH_SQR)
	{
		int boneIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_ROOT);
		if (ptxVerifyf(boneIndex!=BONETAG_INVALID, "invalid bone index generated from bone tag (%s - BONETAG_ROOT)", pPed->GetDebugName()))
		{
			// register the fx system
			ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(atHashWithStringNotFinal("blood_fall",0xE9062420));
			if (pFxInst)
			{
				ptfxAttach::Attach(pFxInst, pPed, boneIndex);	

				// don't cull if this is a player effect
				CPlayerInfo* pPlayerInfo = CGameWorld::GetMainPlayerInfo();
				if (pPlayerInfo)
				{
					if (pPlayerInfo->GetPlayerPed()==pPed)
					{
						pFxInst->SetCanBeCulled(false);
					}
				}

				pFxInst->Start();

#if GTA_REPLAY
				if(CReplayMgr::ShouldRecord())
				{
					CReplayMgr::RecordFx<CPacketBloodFallDeathFx>(
						CPacketBloodFallDeathFx(atHashWithStringNotFinal("blood_fall",0xE9062420)),
						pPed);
				}
#endif // GTA_REPLAY
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxWheelSquash
///////////////////////////////////////////////////////////////////////////////

void			CVfxBlood::TriggerPtFxWheelSquash				(CWheel* UNUSED_PARAM(pWheel), CVehicle* pVehicle, Vec3V_In vFxPos)
{
	// check that there is a camera in range
	if (CVfxHelper::GetDistSqrToCamera(vFxPos)<VFXRANGE_BLOOD_WHEEL_SQUASH_SQR)
	{
		// register the fx system
		ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(atHashWithStringNotFinal("blood_wheel",0xF2C2930A));
		if (pFxInst)
		{
			Mat34V fxMat = pVehicle->GetMatrix();
			fxMat.SetCol3(vFxPos);

			pFxInst->SetBaseMtx(fxMat);

			float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocity().Mag(), 0.0f, 10.0f);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

			pFxInst->Start();
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxBlood
///////////////////////////////////////////////////////////////////////////////

void			CVfxBlood::TriggerPtFxBlood					(const CVfxPedInfo* pVfxPedInfo, const VfxBloodInfo_s* pVfxBloodInfo, const VfxCollInfo_s& vfxCollInfo, float fxDistSqr, bool isExitWound, bool hitBodyArmour, bool hitHeavyArmour, bool isUnderwater, bool isPlayerPed, const CPed* pFiringPed, bool doPtFx, bool doDamage, bool& producedPtFx, bool& producedDamage)
{
	producedPtFx = false;
	producedDamage = false;
	
	// don't play blood if we don't have a dynamic entity to attach to
	if (vfxCollInfo.regdEnt.Get()==NULL)
	{
		vfxAssertf(0, "has been called with a NULL entity");
		return;
	}
	else if (vfxCollInfo.regdEnt->GetIsDynamic()==false)
	{
		vfxAssertf(0, "has been called with a non-dynamic entity");
		return;
	}

	// cast to a dynamic entity
	CDynamicEntity* pDynamicEntity = static_cast<CDynamicEntity*>(vfxCollInfo.regdEnt.Get());
	if (pDynamicEntity==NULL)
	{
		return;
	}

	// no blood particles if not visible in any viewport
	if (!pDynamicEntity->GetIsVisibleInSomeViewportThisFrame())
	{
		bool bReturn = true;
		if (NetworkInterface::IsGameInProgress())
		{
			if (pDynamicEntity->GetIsTypePed())
			{
				CPed* pPed = static_cast<CPed*>(pDynamicEntity);
				if (pPed && pPed->IsPlayer())
					bReturn = false;
			}
		}

		if (bReturn)
			return;
	}

	// play any required effect
	atHashWithStringNotFinal effectRuleHashName = pVfxBloodInfo->ptFxBloodHashName;
	atHashWithStringNotFinal pedDamageHashName = pVfxBloodInfo->pedDamageHashName;
	if (isUnderwater)
	{
		if (hitBodyArmour || hitHeavyArmour)
		{
			return;
		}
		else
		{
			effectRuleHashName = pVfxBloodInfo->ptFxBloodUnderwaterHashName;
		}
	}
	else
	{
		if (hitBodyArmour)
		{
			effectRuleHashName = pVfxBloodInfo->ptFxBloodBodyArmourHashName;
			if (!NetworkInterface::IsGameInProgress())
				pedDamageHashName = pVfxBloodInfo->pedDamageBodyArmourHashName;
		}
		else if (hitHeavyArmour)
		{
			effectRuleHashName = pVfxBloodInfo->ptFxBloodHeavyArmourHashName;
			if (!NetworkInterface::IsGameInProgress())
				pedDamageHashName = pVfxBloodInfo->pedDamageHeavyArmourHashName;
		}
		else 
		{
			if (vfxVerifyf(vfxCollInfo.regdEnt->GetIsTypePed(), "trying to play blood vfx on a non ped"))
			{
				CPed* pPed = static_cast<CPed*>(const_cast<CEntity*>(vfxCollInfo.regdEnt.Get()));
				if (pPed && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UseSloMoBloodVfx))
				{
					effectRuleHashName = pVfxBloodInfo->ptFxBloodSloMoHashName;
				}
			}
		}
	}

	if (effectRuleHashName || pedDamageHashName)
	{
		Vec3V vBloodDir = -vfxCollInfo.vDirectionWld;
		if (isExitWound)
		{
			vBloodDir = -vBloodDir;
		}

		Vec3V vBloodPos = vfxCollInfo.vPositionWld;
		if (isExitWound && vfxCollInfo.regdEnt)
		{
			if (isPlayerPed || fxDistSqr<pVfxPedInfo->GetBloodExitProbeRangeSqr())
			{
				// at close range we do a probe to get the correct exit position
				Vec3V vProbeStart = vfxCollInfo.vPositionWld + (ScalarV(V_TWO)*vfxCollInfo.vDirectionWld);
				WorldProbe::CShapeTestHitPoint probeResult;
				WorldProbe::CShapeTestResults probeResults(probeResult);
				WorldProbe::CShapeTestProbeDesc probeDesc;
				probeDesc.SetResultsStructure(&probeResults);
				probeDesc.SetStartAndEnd(RCC_VECTOR3(vProbeStart), RCC_VECTOR3(vfxCollInfo.vPositionWld));
				probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
				probeDesc.SetIncludeEntity(vfxCollInfo.regdEnt);
				if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
				{
					vBloodPos = RCC_VEC3V(probeResults[0].GetHitPosition());
				}
			}
			else
			{
				// otherwise we estimate the exit position based on the body part hit
				float exitOffsetDist = 0.0f;
				VfxGroup_e vfxGroup = PGTAMATERIALMGR->GetMtlVfxGroup(vfxCollInfo.materialId);
				if (vfxGroup==VFXGROUP_PED_HEAD)		exitOffsetDist = pVfxPedInfo->GetBloodExitPosDistHead();
				else if (vfxGroup==VFXGROUP_PED_TORSO)	exitOffsetDist = pVfxPedInfo->GetBloodExitPosDistTorso();
				else if (vfxGroup==VFXGROUP_PED_LIMB)	exitOffsetDist = pVfxPedInfo->GetBloodExitPosDistLimb();
				else if (vfxGroup==VFXGROUP_PED_FOOT)	exitOffsetDist = pVfxPedInfo->GetBloodExitPosDistFoot();
				vBloodPos = vfxCollInfo.vPositionWld + (ScalarVFromF32(exitOffsetDist)*vfxCollInfo.vDirectionWld);
			}
		}

		bool bDuplicate = false;
		// check for nearby effects of this type - don't play this one if another is being played too close by
		if (ptfxHistory::Check(effectRuleHashName?effectRuleHashName:pedDamageHashName, 0, vfxCollInfo.regdEnt, vBloodPos, VFXBLOOD_HIST_DIST))
		{
			bDuplicate = true;
//			extern const char* parser_RagdollComponent_Strings[];
//			Displayf("duplicate - component %d (%s)",vfxCollInfo.componentId, vfxCollInfo.componentId<RAGDOLL_NUM_COMPONENTS ? parser_RagdollComponent_Strings[vfxCollInfo.componentId] : "out of range");
		}
		
		bool bShotGun = false;
		if (pedDamageHashName==ATSTRINGHASH("ShotgunLarge",0x68f7998b) || pedDamageHashName==ATSTRINGHASH("ShotgunSmall",0xbdf00aa))
			bShotGun = true;


		// PedDamage effect setup moved here from CWeaponDamage::DoWeaponImpactVfx
		if (doDamage && pedDamageHashName.IsNotNull() && (!bDuplicate || bShotGun))
		{
			if (vfxVerifyf(vfxCollInfo.regdEnt->GetIsTypePed(), "trying to play blood vfx on a non ped"))
			{
				CPed * pPed = static_cast<CPed*>(const_cast<CEntity*>(vfxCollInfo.regdEnt.Get()));

				static const u32 kDeadTooLongForBlood = 60;				// the number of seconds after death when we can still bleed
				static const u32 kDeadLongEnoughForReducedBlood = 20;	// the number of seconds after death which blood soak is reduced

				u32 timeSinceDead = fwTimer::GetTimeInMilliseconds() - pPed->GetTimeOfDeath();
				bool noBleeding = pPed->IsDead() && timeSinceDead > kDeadTooLongForBlood*1000;
				bool reduceBleeding  = pPed->IsDead() && timeSinceDead > kDeadLongEnoughForReducedBlood*1000;
				
				if (noBleeding)	// if they have been dead too long, we don't put the soaking/bleeding damage on
				{
					//  (we should have a special dead damage blit type here)
				}
				else
				{
					// if it's a non fatal head shot, override the head shot damage
					if((vfxCollInfo.componentId==RAGDOLL_HEAD || vfxCollInfo.componentId==RAGDOLL_NECK) && pPed->GetHealth()>0) 
					{
						// this need to take into account melee hits as well. don't swap out a bruise for a head shot...
						//pedDamageHashName = ATSTRINGHASH("NonFatalHeadshot",0x57ed7007); 
					}
					if (isPlayerPed)
					{
						// we might want to skip exit wound? or make other changes for the player ped
					}
					PEDDAMAGEMANAGER.AddPedDamageVfx(pPed, (u16) vfxCollInfo.componentId, VEC3V_TO_VECTOR3(vBloodPos), pedDamageHashName, reduceBleeding);
					producedDamage = true;
				}
			}
		}

		if (bDuplicate)
			return;

		if (doPtFx && effectRuleHashName)
		{
			if (vfxCollInfo.regdEnt && vfxCollInfo.regdEnt->GetIsDynamic())
			{
				// try to get a valid matrix index
				vfxAssertf(vfxCollInfo.regdEnt->GetFragInst(), "can't get fragment instance from entity");
				fragInst* pFragInst = static_cast<fragInst*>(vfxCollInfo.regdEnt->GetFragInst());
				fragTypeChild** ppChildren = pFragInst->GetTypePhysics()->GetAllChildren();

				if (vfxCollInfo.componentId>=0 && vfxCollInfo.componentId<pFragInst->GetTypePhysics()->GetNumChildren())
				{
					s32 matrixIndex = pFragInst->GetType()->GetBoneIndexFromID(ppChildren[vfxCollInfo.componentId]->GetBoneID());

					ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(effectRuleHashName);
					if (pFxInst)
					{
						// calc the world matrix
						Mat34V fxMat;
						CVfxHelper::CreateMatFromVecZAlign(fxMat, vBloodPos, vBloodDir, Vec3V(V_Z_AXIS_WZERO));

						// get the entity that this should be attached to
						vfxAssertf(vfxCollInfo.regdEnt, "invalid entity found");
						vfxAssertf(vfxCollInfo.regdEnt && vfxCollInfo.regdEnt->GetIsDynamic(), "entity is not dynamic");

						// set the position of the effect
						Mat34V boneMtx;
						CVfxHelper::GetMatrixFromBoneIndex(boneMtx, vfxCollInfo.regdEnt, matrixIndex);

						// make the fx matrix relative to the entity
						Mat34V relFxMat;
						CVfxHelper::CreateRelativeMat(relFxMat, boneMtx, fxMat);

						ptfxAttach::Attach(pFxInst, vfxCollInfo.regdEnt, matrixIndex);
						pFxInst->SetOffsetMtx(relFxMat);

						// set evolution variables
						if (!hitBodyArmour && !hitHeavyArmour)
						{
							pFxInst->SetEvoValueFromHash(ATSTRINGHASH("size", 0xC052DEA7), pVfxBloodInfo->ptFxSizeEvo);
						}

						Vec3V_ConstRef vCamForward = RCC_VEC3V(camInterface::GetFront());
						float angleEvo = Abs(Dot(vCamForward, vfxCollInfo.vDirectionWld).Getf());
						pFxInst->SetEvoValueFromHash(ATSTRINGHASH("angle", 0xF3805B5), angleEvo);

						// user zoom
						float userZoom = pVfxPedInfo->GetBloodEntryPtFxScale();
						if (pFiringPed && pFiringPed->GetSpecialAbility() && pFiringPed->GetSpecialAbility()->IsActive() && pFiringPed->GetSpecialAbility()->GetType()==SAT_RAGE)
						{
							// special ability override
							userZoom *= VFXBLOOD_PTFX_ZOOM_TREVOR_RAGE;
						}
						else
						{
							userZoom *= m_bloodPtFxUserZoomScale;
						}
						pFxInst->SetUserZoom(userZoom);

						// start the effect
						pFxInst->Start();

						ptfxHistory::Add(effectRuleHashName, 0, vfxCollInfo.regdEnt, vBloodPos, VFXBLOOD_HIST_TIME);

						// should we spray the camera with blood?
						//ProcessCamBlood(vBloodPos, vBloodDir, vfxCollInfo.weaponGroup==WEAPON_EFFECT_GROUP_SHOTGUN);

						producedPtFx = true;  
					}
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxMouthBlood
///////////////////////////////////////////////////////////////////////////////

void			CVfxBlood::TriggerPtFxMouthBlood				(CPed* pPed)
{
	if (pPed->GetPedVfx() == NULL || (pPed->GetPedVfx() && pPed->GetPedVfx()->GetBloodVfxDisabled()))
	{
		return;
	}

	int boneIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_HEAD);
	if (ptxVerifyf(boneIndex!=BONETAG_INVALID, "invalid bone index generated from bone tag (%s - BONETAG_ROOT)", pPed->GetDebugName()))
	{
		Mat34V boneMtx;
		CVfxHelper::GetMatrixFromBoneIndex(boneMtx, pPed, boneIndex);

		// check that there is a camera in range
		Vec3V vFxPos = boneMtx.GetCol3();
		if (CVfxHelper::GetDistSqrToCamera(vFxPos)<VFXRANGE_BLOOD_MOUTH_SQR)
		{
			// register the fx system
			ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(atHashWithStringNotFinal("blood_mouth",0xC5947257));
			if (pFxInst)
			{
				ptfxAttach::Attach(pFxInst, pPed, boneIndex);	

				pFxInst->Start();
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxBloodSharkAttack
///////////////////////////////////////////////////////////////////////////////

void			CVfxBlood::TriggerPtFxBloodSharkAttack			(CPed* pPed)
{
	if (pPed->GetPedVfx() == NULL || (pPed->GetPedVfx() && pPed->GetPedVfx()->GetBloodVfxDisabled()))
	{
		return;
	}

	// check that there is a camera in range
	Vec3V vFxPos = pPed->GetTransform().GetPosition();
	if (CVfxHelper::GetDistSqrToCamera(vFxPos)<VFXRANGE_BLOOD_SHARK_ATTACK_SQR)
	{
		int boneIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_ROOT);
		if (ptxVerifyf(boneIndex!=BONETAG_INVALID, "invalid bone index generated from bone tag (%s - BONETAG_ROOT)", pPed->GetDebugName()))
		{
			// register the fx system
			ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(atHashWithStringNotFinal("blood_shark_attack", 0x0ab548fe));
			if (pFxInst)
			{
				ptfxAttach::Attach(pFxInst, pPed, boneIndex);	

				pFxInst->Start();
			}

#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				CReplayMgr::RecordFx<CPacketBloodSharkBiteFx>(
					CPacketBloodSharkBiteFx(atHashWithStringNotFinal("blood_shark_attack",0x0ab548fe), boneIndex),
					pPed);
			}
#endif // GTA_REPLAY
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxBloodMist
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxBlood::UpdatePtFxBloodMist					(CPed* pPed)
{
	if (!pPed->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	if (pPed->GetIsInVehicle())
	{
		return;
	}

	if (pPed->GetIsInWater())
	{
		return;
	}

	if (pPed->GetPedVfx() == NULL || (pPed->GetPedVfx() && pPed->GetPedVfx()->GetBloodVfxDisabled()))
	{
		return;
	}

	// check if the ped is within fx creation range
	if (CVfxHelper::GetDistSqrToCamera(pPed->GetTransform().GetPosition())>VFXRANGE_BLOOD_MIST_SQR)
	{
		return;
	}

	int boneIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_ROOT);
	if (ptxVerifyf(boneIndex!=BONETAG_INVALID, "invalid bone index generated from bone tag (%s - BONETAG_ROOT)", pPed->GetDebugName()))
	{
		// register the fx system
		bool justCreated = false;
		ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pPed, PTFXOFFSET_PED_BLOOD_MIST, true, atHashWithStringNotFinal("blood_mist_prop", 0xa2657aa3), justCreated);
		if (pFxInst)
		{
			ptfxAttach::Attach(pFxInst, pPed, boneIndex);

			if (justCreated)
			{
				pFxInst->Start();
			}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
			else
			{
				// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
				vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "Effect attachment error");
			}
#endif
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxBloodDrips
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxBlood::UpdatePtFxBloodDrips					(CPed* pPed)
{
	if (!pPed->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	if (pPed->GetIsInVehicle())
	{
		return;
	}

	if (pPed->GetIsInWater())
	{
		return;
	}

	// check if the ped is within fx creation range
	if (CVfxHelper::GetDistSqrToCamera(pPed->GetTransform().GetPosition())>VFXRANGE_BLOOD_DRIPS_SQR)
	{
		return;
	}

	int boneIndex = pPed->GetBoneIndexFromBoneTag(BONETAG_ROOT);
	if (ptxVerifyf(boneIndex!=BONETAG_INVALID, "invalid bone index generated from bone tag (%s - BONETAG_ROOT)", pPed->GetDebugName()))
	{
		// register the fx system
		bool justCreated = false;
		ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pPed, PTFXOFFSET_PED_BLOOD_DRIPS, true, atHashWithStringNotFinal("ped_blood_drips",0xEF3053DE), justCreated);
		if (pFxInst)
		{
			ptfxAttach::Attach(pFxInst, pPed, boneIndex);

			if (justCreated)
			{
				pFxInst->Start();
			}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
			else
			{
				// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
				vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "Effect attachment error");
			}
#endif
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxBloodPoolInWater
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxBlood::UpdatePtFxBloodPoolInWater			(CPed* pPed)
{
	if (!pPed->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	// check if the ped is within fx creation range
	if (CVfxHelper::GetDistSqrToCamera(pPed->GetTransform().GetPosition())>VFXRANGE_BLOOD_POOL_IN_WATER_SQR)
	{
		return;
	}

	// get the water depth at the ped position
	float waterZ;
	Vec3V vPedPos = pPed->GetTransform().GetPosition();
	CVfxHelper::GetWaterZ(vPedPos, waterZ, pPed);

	if (vPedPos.GetZf()<waterZ-VFXBLOOD_POOL_IN_WATER_DEPTH)
	{
		return;
	}

	Vec3V vPedWaterPos = vPedPos;
	vPedWaterPos.SetZf(waterZ);

	// register the fx system
	bool justCreated = false;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pPed, PTFXOFFSET_PED_BLOOD_DRIPS, true, atHashWithStringNotFinal("ped_blood_pool_water", 0x3d2c1891), justCreated);
	if (pFxInst)
	{
		pFxInst->SetBasePos(vPedWaterPos);

		if (justCreated)
		{
			pFxInst->Start();
		}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
		else
		{
			// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
			vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "Effect attachment error");
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessCamBlood
///////////////////////////////////////////////////////////////////////////////

// void			CVfxBlood::ProcessCamBlood				(Vec3V_In vPos, Vec3V_In vDir, bool UNUSED_PARAM(isShotgunWound))
// {
// 	float bloodEvo = 2.0f;
// 	Mat34V_ConstRef camMat = RCC_MAT34V(camInterface::GetGameplayDirector().GetFrame().GetWorldMatrix());
// 	
// 	Vec3V vCamPos = camMat.GetCol3();
// 	float dist = Mag(vPos-vCamPos).Getf();
// 	if (dist>VFXBLOOD_CAMSPLASH_DIST)
// 	{
// 		return;
// 	}
// 
// 	bloodEvo *= 1.0f-(dist/VFXBLOOD_CAMSPLASH_DIST);
// 
// 	Vec3V vCamForward = camMat.GetCol1();
// 	float dotProd = Dot(-vCamForward, vDir).Getf();
// 	if (dotProd<VFXBLOOD_CAMSPLASH_DOT_PROD)
// 	{
// 		return;
// 	}
// 
// 	bloodEvo *= (dotProd-VFXBLOOD_CAMSPLASH_DOT_PROD)/(1.0f-VFXBLOOD_CAMSPLASH_DOT_PROD);
// 	bloodEvo = Max(bloodEvo, 0.0f);
// 	bloodEvo = Min(bloodEvo, 1.0f);
// 
// // 	float shotgunEvo = 0.0f;
// // 	if (isShotgunWound)
// // 	{
// // 		shotgunEvo = 1.0f;
// // 	}
// 
// 	g_vfxLens.Register(VFXLENSTYPE_BLOOD, bloodEvo, 0.0f, 0.0f, 1.0f);
// }


///////////////////////////////////////////////////////////////////////////////
//  TriggerDecalBlood
///////////////////////////////////////////////////////////////////////////////

void			CVfxBlood::TriggerDecalBlood					(const VfxBloodInfo_s* pVfxBloodInfo, Vec3V_In vPos, Vec3V_In vDir, CEntity* pEntity, float fxDistSqr)
{
	bool isPlayer = false;
	// don't project textures if the ped is in a vehicle
	if (pEntity->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pEntity);
		if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			return;
		}
		isPlayer = pPed->IsLocalPlayer();
	}

	// return if no probes are specified
	if (pVfxBloodInfo->probeDistA<=0.0f)
	{
		return;
	}

	// calc the lod ratio
	float fxDist = Sqrtf(fxDistSqr);
	float hiToLoLodRatio = CVfxHelper::GetInterpValue(fxDist, pVfxBloodInfo->lodRangeHi, pVfxBloodInfo->lodRangeLo, true);

	// calc the settings based on lod
	s32 numProbes = (s32)(pVfxBloodInfo->numProbesHi + (hiToLoLodRatio * (pVfxBloodInfo->numProbesLo-pVfxBloodInfo->numProbesHi)));
	for (int i=0; i<numProbes; i++)
	{
		// probe A starts at the collision position and probes along the direction looking for walls etc
		Vec3V vProbeDir = vDir;
		Vec3V vRand;
		vRand.SetXf(g_DrawRand.GetRanged(-pVfxBloodInfo->probeVariation, pVfxBloodInfo->probeVariation));
		vRand.SetYf(g_DrawRand.GetRanged(-pVfxBloodInfo->probeVariation, pVfxBloodInfo->probeVariation));
		vRand.SetZf(g_DrawRand.GetRanged(-pVfxBloodInfo->probeVariation, pVfxBloodInfo->probeVariation));
		vProbeDir = vProbeDir + vRand;
		vProbeDir = Normalize(vProbeDir);

		Vec3V vStartPos = vPos;
		Vec3V vEndPos = vStartPos+(vProbeDir*ScalarVFromF32(pVfxBloodInfo->probeDistA));

		m_asyncProbeMgr.TriggerSplatProbe(vStartPos, vEndPos, vProbeDir, pVfxBloodInfo, hiToLoLodRatio, true, isPlayer);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void		 	CVfxBlood::InitWidgets						()
{
	if (m_bankInitialised==false)
	{
		char txt[128];
		u32 bloodInfoSize = sizeof(VfxBloodInfo_s);
		float entryInfoPoolSize = (bloodInfoSize * VFXBLOOD_MAX_INFOS) / 1024.0f;
		float exitInfoPoolSize = (bloodInfoSize * VFXBLOOD_MAX_INFOS) / 1024.0f;

		bkBank* pVfxBank = vfxWidget::GetBank();
		pVfxBank->PushGroup("Vfx Blood", false);
		{
			pVfxBank->AddTitle("INFO");
			sprintf(txt, "Num Entry Infos (%d - %.2fK)", VFXBLOOD_MAX_INFOS, entryInfoPoolSize);
			pVfxBank->AddSlider(txt, &m_numBloodEntryVfxInfos, 0, VFXBLOOD_MAX_INFOS, 0);
			sprintf(txt, "Num Exit Infos (%d - %.2fK)", VFXBLOOD_MAX_INFOS, exitInfoPoolSize);
			pVfxBank->AddSlider(txt, &m_numBloodExitVfxInfos, 0, VFXBLOOD_MAX_INFOS, 0);

#if __DEV
			pVfxBank->AddTitle("");
			pVfxBank->AddTitle("SETTINGS");

			pVfxBank->AddSlider("Blood History Dist", &VFXBLOOD_HIST_DIST, 0.0f, 100.0f, 0.25f, NullCB, "The distance at which a new blood effect will be rejected if a history exists within this range");
			pVfxBank->AddSlider("Blood History Time", &VFXBLOOD_HIST_TIME, 0.0f, 100.0f, 0.25f, NullCB, "The time that a history will exist for a new blood effect");
//			pVfxBank->AddSlider("Cam Blood Angle", &VFXBLOOD_CAMSPLASH_DOT_PROD, 0.0f, 1.0f, 0.05f, NullCB, "The dot product value at which camera blood gets processed - closer to 1.0 results in a smaller angle");
//			pVfxBank->AddSlider("Cam Blood Distance", &VFXBLOOD_CAMSPLASH_DIST, 0.0f, 50.0f, 0.5f, NullCB, "The distance within which camera blood gets processed");

			pVfxBank->AddSlider("Blood PtFx Zoom - Trevor Rage", &VFXBLOOD_PTFX_ZOOM_TREVOR_RAGE, 0.0f, 10.0f, 0.1f);
			
			pVfxBank->AddSlider("Blood Pool Timer Min", &VFXBLOOD_POOL_TIMER_MIN, 0.0f, 50.0f, 0.5f);
			pVfxBank->AddSlider("Blood Pool Timer Max", &VFXBLOOD_POOL_TIMER_MAX, 0.0f, 50.0f, 0.5f);
			pVfxBank->AddSlider("Blood Pool Start Size Min", &VFXBLOOD_POOL_STARTSIZE_MIN, 0.0f, 1.0f, 0.01f);
			pVfxBank->AddSlider("Blood Pool Start Size Max", &VFXBLOOD_POOL_STARTSIZE_MAX, 0.0f, 1.0f, 0.01f);
			pVfxBank->AddSlider("Blood Pool End Size Min", &VFXBLOOD_POOL_ENDSIZE_MIN, 0.0f, 5.0f, 0.01f);
			pVfxBank->AddSlider("Blood Pool End Size Max", &VFXBLOOD_POOL_ENDSIZE_MAX, 0.0f, 5.0f, 0.01f);
			pVfxBank->AddSlider("Blood Pool Grow Rate Min", &VFXBLOOD_POOL_GROWRATE_MIN, 0.0f, 1.0f, 0.001f);
			pVfxBank->AddSlider("Blood Pool Grow Rate Max", &VFXBLOOD_POOL_GROWRATE_MAX, 0.0f, 1.0f, 0.001f);
			pVfxBank->AddSlider("Blood Pool Lod Dist", &VFXBLOOD_POOL_LOD_DIST, 0.0f, 100.0f, 0.25f);
			pVfxBank->AddSlider("Blood Pool In Water Depth", &VFXBLOOD_POOL_IN_WATER_DEPTH, 0.0f, 20.0f, 0.1f);
#endif

			pVfxBank->AddTitle("");
			pVfxBank->AddTitle("DEBUG");
			pVfxBank->AddToggle("Disable Entry Vfx", &m_disableEntryVfx);
			pVfxBank->AddToggle("Disable Exit Vfx", &m_disableExitVfx);
			pVfxBank->AddToggle("Force MP Data", &m_forceMPData);
			pVfxBank->AddToggle("Render Blood Vfx Info", &m_renderBloodVfxInfo);
			pVfxBank->AddSlider("Render Blood Vfx Info Timer", &m_renderBloodVfxInfoTimer, 0, 1000, 1);
			pVfxBank->AddToggle("Render Blood Splat Vfx Probes", &m_renderBloodSplatVfxProbes);
			pVfxBank->AddToggle("Render Blood Pool Vfx Probes", &m_renderBloodPoolVfxProbes);
			pVfxBank->AddSlider("Blood User Zoom Scale", &m_bloodPtFxUserZoomScale, 0.0f, 20.0f, 0.05f);
			pVfxBank->AddSlider("Override Weapon Group Id", &m_overrideWeaponGroupId, -1, VFXBLOOD_NUM_WEAPON_GROUPS-1, 1);

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
void			CVfxBlood::Reset								()
{
	g_vfxBlood.Shutdown(SHUTDOWN_WITH_MAP_LOADED);
	g_vfxBlood.Init(INIT_AFTER_MAP_LOADED);
}
#endif // __BANK





