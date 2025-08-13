///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxWeapon.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	01 June 2006
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxWeapon.h"

// rage
#include "file/asset.h"
#include "file/stream.h"
#include "file/token.h"
#include "grcore/texture.h"
#include "math/random.h"
#include "physics/intersection.h"
#include "rmptfx/ptxmanager.h"
#include "rmcore/drawable.h"

// rage (reliant on previous includes)
#include "grcore/viewport_inline.h"


// framework
#include "fwscene/stores/txdstore.h"
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxflags.h"
#include "vfx/ptfx/ptfxhistory.h"
#include "vfx/ptfx/ptfxreg.h"
#include "vfx/ptfx/ptfxwrapper.h"
#include "vfx/vfxwidget.h"

// game
#include "Camera/CamInterface.h"
#include "camera/base/BaseCamera.h"
#include "Camera/Gameplay/Aim/FirstPersonShooterCamera.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Camera/Viewports/ViewportManager.h"
#include "Control/Replay/Replay.h"
#include "Control/Replay/Effects/ParticleWeaponFxPacket.h"
#include "Control/Replay/Effects/ProjectedTexturePacket.h"
#include "Core/Game.h"
#include "Game/Localisation.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/VehicleModelInfoColors.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "ModelInfo/WeaponModelInfo.h"
#include "Objects/Object.h"
#include "Peds/Ped.h"
#include "Peds/PlayerSpecialAbility.h"
#include "Physics/GtaMaterialManager.h"
#include "Physics/Physics.h"
#include "renderer/lights/lights.h"
#include "renderer/lights/AsyncLightOcclusionMgr.h"
#include "renderer/RenderPhases/RenderPhaseFX.h"
#include "Scene/DataFileMgr.h"
#include "Scene/Physical.h"
#include "Scene/Portals/Portal.h"
#include "Scene/World/GameWorld.h"
#include "Shaders/ShaderLib.h"
#include "Vehicles/Vehicle.h"
#include "Vehicles/VehicleGadgets.h"
#include "Vfx/Decals/DecalCallbacks.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Metadata/VfxInteriorInfo.h"
#include "Vfx/Metadata/VfxWeaponInfo.h"
#include "Vfx/Misc/Fire.h"
#include "Vfx/Particles/PtFxCollision.h"
#include "Vfx/Particles/PtFxDefines.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/Systems/VfxBlood.h"
#include "Vfx/Systems/VfxEntity.h"
#include "Vfx/Systems/VfxExplosion.h"
#include "Vfx/Systems/VfxLens.h"
#include "Vfx/Systems/VfxProjectile.h"
#include "Vfx/Systems/VfxWeather.h"
#include "Vfx/VfxHelper.h"
#include "Weapons/Info/WeaponInfo.h"
#include "Weapons/Projectiles/Projectile.h"
#include "Weapons/Weapon.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  ENUMERATIONS
///////////////////////////////////////////////////////////////////////////////

// weapon ptfx offsets for registered systems 
enum PtFxVfxWeaponOffsets_e
{
	PTFXOFFSET_VFXWEAPON_INTERIOR_SMOKE		= 0,
	PTFXOFFSET_VFXWEAPON_INTERIOR_DUST,
};

// weapon ptfx offsets for registered systems 
enum PtFxWeaponOffsets_e
{
	PTFXOFFSET_WEAPON_GUNSHELL				= 0,
	PTFXOFFSET_WEAPON_GUNSHELL_LAST			= PTFXOFFSET_WEAPON_GUNSHELL + MAX_NUM_BATTERY_WEAPONS,
	PTFXOFFSET_WEAPON_GUNSHELL_ALT,
	PTFXOFFSET_WEAPON_GUNSHELL_ALT_LAST		= PTFXOFFSET_WEAPON_GUNSHELL_ALT + MAX_NUM_BATTERY_WEAPONS,
	PTFXOFFSET_WEAPON_VOLUMETRIC,
	PTFXOFFSET_WEAPON_VOLUMETRIC_2,
	PTFXOFFSET_WEAPON_VOLUMETRIC_ALT,
	PTFXOFFSET_WEAPON_VOLUMETRIC_ALT_2,
	PTFXOFFSET_WEAPON_MUZZLE_SMOKE,
	//PTFXOFFSET_WEAPON_LASER_SIGHT,
	PTFXOFFSET_WEAPON_GROUND_DISTURB,
};		


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

// rag tweakable settings
dev_float		VFXWEAPON_IMPACT_HIST_DIST					= 0.25f;
dev_float		VFXWEAPON_IMPACT_HIST_TIME					= 0.2f;

dev_float		VFXWEAPON_INTSMOKE_INC_PER_SHOT				= 0.08f;
dev_float		VFXWEAPON_INTSMOKE_INC_PER_EXP				= 1.0f;
dev_float		VFXWEAPON_INTSMOKE_DEC_PER_UPDATE			= 0.002f;
dev_float		VFXWEAPON_INTSMOKE_MAX_LEVEL				= 1.0f;
dev_float		VFXWEAPON_INTSMOKE_INTERP_SPEED				= 0.02f;
dev_float		VFXWEAPON_INTSMOKE_MIN_GRID_SIZE			= 10.0f;

dev_float		VFXWEAPON_IMPACT_PTFX_ZOOM_TREVOR_RAGE		= 2.0f;

dev_float		VFXWEAPON_TRACE_ANGLE_REJECT_THRESH			= 0.985f;
dev_float		VFXWEAPON_TRACE_ANGLE_REJECT_THRESH_FPS		= 0.95f;
dev_float		VFXWEAPON_TRACE_CAM_REJECT_DIST				= 0.75f;


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CVfxWeapon		g_vfxWeapon;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxWeapon
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxWeaponAsyncProbeManager
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWeaponAsyncProbeManager::Init			()
{
	// init the async probes
	for (int i=0; i<VFXWEAPON_MAX_GROUND_DISTURB_PROBES; i++)
	{
		m_groundDisturbProbes[i].m_vfxProbeId = -1;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWeaponAsyncProbeManager::Shutdown		()
{
	// init the async probes
	for (int i=0; i<VFXWEAPON_MAX_GROUND_DISTURB_PROBES; i++)
	{
		if (m_groundDisturbProbes[i].m_vfxProbeId>=0)
		{
			CVfxAsyncProbeManager::AbortProbe(m_groundDisturbProbes[i].m_vfxProbeId);
			m_groundDisturbProbes[i].m_vfxProbeId = -1;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWeaponAsyncProbeManager::Update			()
{
	// update the ground disturbance probes
	for (int i=0; i<VFXWEAPON_MAX_GROUND_DISTURB_PROBES; i++)
	{
		if (m_groundDisturbProbes[i].m_vfxProbeId>=0)
		{
			// query the probe
			VfxAsyncProbeResults_s vfxProbeResults;
			if (CVfxAsyncProbeManager::QueryProbe(m_groundDisturbProbes[i].m_vfxProbeId, vfxProbeResults))
			{
				// the probe has finished
				m_groundDisturbProbes[i].m_vfxProbeId = -1;

				if (m_groundDisturbProbes[i].m_pWeapon)
				{
					VfxDisturbanceType_e vfxDisturbanceType = VFX_DISTURBANCE_DEFAULT;
					Vec3V vColnPos = m_groundDisturbProbes[i].m_vEndPos;
					Vec3V vColnNormal;
					float intersectRatio = 1.0f;
					float groundZ = -1000000.0f;

					float dist = m_groundDisturbProbes[i].m_pWeapon->GetWeaponInfo()->GetGroundDisturbFxDist();

					// check for ground
					bool foundGround = vfxProbeResults.hasIntersected;
					if (foundGround)
					{
						vfxDisturbanceType = PGTAMATERIALMGR->GetMtlVfxDisturbanceType(vfxProbeResults.mtlId);
						vColnPos = vfxProbeResults.vPos;
						vColnNormal = vfxProbeResults.vNormal;
						foundGround = true;
						intersectRatio = vfxProbeResults.tValue;
						groundZ = vColnPos.GetZf();
					}

					// check for water
					Vec3V vWaterLevel;
					WaterTestResultType waterType = CVfxHelper::TestLineAgainstWater(m_groundDisturbProbes[i].m_vStartPos, vColnPos, vWaterLevel);
					bool foundWater = waterType>WATERTEST_TYPE_NONE;
					if (foundWater)
					{
						vColnPos = vWaterLevel;
						vColnNormal = Vec3V(V_Z_AXIS_WZERO);
						vfxDisturbanceType = VFX_DISTURBANCE_WATER;

						Vec3V vVec = m_groundDisturbProbes[i].m_vStartPos - vColnPos;
						intersectRatio = CVfxHelper::GetInterpValue(Mag(vVec).Getf(), 0.0f, dist);
						groundZ = vColnPos.GetZf();
					}

					if (foundGround || foundWater)
					{
						g_vfxWeapon.UpdatePtFxWeaponGroundDisturb(m_groundDisturbProbes[i].m_pWeapon, vColnPos, vColnNormal, m_groundDisturbProbes[i].m_vDir, intersectRatio, vfxDisturbanceType);
					}
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerGroundDisturbProbe
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWeaponAsyncProbeManager::TriggerGroundDisturbProbe(Vec3V_In vStartPos, Vec3V_In vEndPos, CWeapon* pWeapon, Vec3V_In vDir)
{
	static u32 probeFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER;

	// look for an available probe
	for (int i=0; i<VFXWEAPON_MAX_GROUND_DISTURB_PROBES; i++)
	{
		if (m_groundDisturbProbes[i].m_vfxProbeId==-1)
		{
			m_groundDisturbProbes[i].m_vfxProbeId = CVfxAsyncProbeManager::SubmitWorldProbe(vStartPos, vEndPos, probeFlags);
			m_groundDisturbProbes[i].m_vStartPos = vStartPos;
			m_groundDisturbProbes[i].m_vEndPos = vEndPos;
			m_groundDisturbProbes[i].m_pWeapon = pWeapon;
			m_groundDisturbProbes[i].m_vDir = vDir;

#if __BANK
			if (g_vfxWeapon.GetRenderGroundDisturbVfxProbes())
			{
 				grcDebugDraw::Line(vStartPos, vEndPos, Color32(1.0f, 0.0f, 1.0f, 1.0f), -200);
			}
#endif

			return;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWeapon::Init								(unsigned initMode)
{
	if (initMode==INIT_CORE)
	{
		m_bulletImpactPtFxUserZoomScaleScriptThreadId = THREAD_INVALID;
		m_bulletImpactPtFxUserZoomScale = 1.0f;
		m_bulletImpactPtFxLodRangeScaleScriptThreadId = THREAD_INVALID;
		m_bulletImpactPtFxLodRangeScale = 1.0f;
		m_bulletImpactDecalRangeScaleScriptThreadId = THREAD_INVALID;
		m_bulletImpactDecalRangeScale = 1.0f;
		m_bulletTraceNoAngleRejectScriptThreadId = THREAD_INVALID;
		m_bulletTraceNoAngleReject = false;

#if __BANK
		m_bankInitialised = false;

		sprintf(m_intSmokeOverrideName, "%s", "");
		m_disableInteriorSmoke = false;
		m_disableInteriorDust = false;

		m_forceBulletTraces = false;
		m_renderDebugBulletTraces = false;
		m_attachBulletTraces = false;

		m_bulletImpactDecalOverridesActive = false;
		m_bulletImpactDecalOverrideMaxOverlayRadius = 0.0f;
		m_bulletImpactDecalOverrideDuplicateRejectDist = 0.0f;

		m_renderGroundDisturbVfxProbes = false;
#endif
	}
	else if (initMode == INIT_AFTER_MAP_LOADED)
    {
	    // load in the data from the file
	    LoadData();
	}
	else if (initMode==INIT_SESSION)
	{
		m_asyncProbeMgr.Init();
	}
	
	// init the overrides
	ResetIntSmokeOverrides();
}

///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWeapon::Shutdown							(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
    {
    }
    else if(shutdownMode==SHUTDOWN_SESSION)
	{
		m_asyncProbeMgr.Shutdown();
        ptfxReg::UnRegister(this);
	}
}

///////////////////////////////////////////////////////////////////////////////
//  ResetIntSmokeOverrides
///////////////////////////////////////////////////////////////////////////////

void CVfxWeapon::ResetIntSmokeOverrides()
{
	m_intSmokeOverrideHashName = (u32)0;
	m_intSmokeOverrideLevel = 0.0f;
}


///////////////////////////////////////////////////////////////////////////////
//  SetBulletImpactPtFxUserZoomScale
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeapon::SetBulletImpactPtFxUserZoomScale		(float val, scrThreadId scriptThreadId) 
{
	if (vfxVerifyf(m_bulletImpactPtFxUserZoomScaleScriptThreadId==THREAD_INVALID || m_bulletImpactPtFxUserZoomScaleScriptThreadId==scriptThreadId, "trying to set bullet impact user zoom when this is already in use by another script")) 
	{
		m_bulletImpactPtFxUserZoomScale = val; 
		m_bulletImpactPtFxUserZoomScaleScriptThreadId = scriptThreadId;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetBulletImpactPtFxLodRangeScale
///////////////////////////////////////////////////////////////////////////////


void			CVfxWeapon::SetBulletImpactPtFxLodRangeScale		(float val, scrThreadId scriptThreadId) 
{
	if (vfxVerifyf(m_bulletImpactPtFxLodRangeScaleScriptThreadId==THREAD_INVALID || m_bulletImpactPtFxLodRangeScaleScriptThreadId==scriptThreadId, "trying to set bullet impact user zoom when this is already in use by another script")) 
	{
		m_bulletImpactPtFxLodRangeScale = val; 
		m_bulletImpactPtFxLodRangeScaleScriptThreadId = scriptThreadId;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetBulletImpactDecalRangeScale
///////////////////////////////////////////////////////////////////////////////


void			CVfxWeapon::SetBulletImpactDecalRangeScale		(float val, scrThreadId scriptThreadId) 
{
	if (vfxVerifyf(m_bulletImpactDecalRangeScaleScriptThreadId==THREAD_INVALID || m_bulletImpactDecalRangeScaleScriptThreadId==scriptThreadId, "trying to set bullet impact user zoom when this is already in use by another script")) 
	{
		m_bulletImpactDecalRangeScale = val; 
		m_bulletImpactDecalRangeScaleScriptThreadId = scriptThreadId;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetBulletTraceNoAngleReject
///////////////////////////////////////////////////////////////////////////////


void			CVfxWeapon::SetBulletTraceNoAngleReject				(bool val, scrThreadId scriptThreadId) 
{
	if (vfxVerifyf(m_bulletTraceNoAngleRejectScriptThreadId==THREAD_INVALID || m_bulletTraceNoAngleRejectScriptThreadId==scriptThreadId, "trying to set bullet impact user zoom when this is already in use by another script")) 
	{
		m_bulletTraceNoAngleReject = val; 
		m_bulletTraceNoAngleRejectScriptThreadId = scriptThreadId;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  LoadData
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWeapon::LoadData							()
{
	// initialise the data
	for (s32 i=0; i<NUM_VFX_GROUPS; i++)
	{
		for (s32 j=0; j<EWEAPONEFFECTGROUP_MAX; j++)
		{
			m_vfxWeaponTable[i][j].id = 0;
			m_vfxWeaponTable[i][j].offset = -1;
			m_vfxWeaponTable[i][j].count = -1;
		}
	}

	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::WEAPONFX_FILE);
	while (DATAFILEMGR.IsValid(pData))
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
			token.Init("weaponFx", stream);

			// count the number of materials
			s32 phase = -1;	
			m_numVfxWeaponInfos = 0;
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
				if (stricmp(charBuff, "WEAPONFX_TABLE_START")==0)
				{
					phase = 0;
					continue;
				}
				if (stricmp(charBuff, "WEAPONFX_TABLE_END")==0)
				{
					phase = -1;
					continue;
				}
				else if (stricmp(charBuff, "WEAPONFX_INFO_START")==0)
				{
					phase = 1;
					continue;
				}
				else if (stricmp(charBuff, "WEAPONFX_INFO_END")==0)
				{
					break;
				}

				// phase 0 - material effect table
				if (phase==0)
				{
					if (stricmp(charBuff, "BRKGLASS_LOOKUP")==0)
					{ 
						// Breakable glass look up table
						for (s32 i=0; i<EWEAPONEFFECTGROUP_MAX; i++)
						{
							m_vfxBreakableGlass[i] = token.GetInt();
						}
						token.SkipToEndOfLine();
					}
					else
					{
						VfxGroup_e vfxGroup = phMaterialGta::FindVfxGroup(charBuff);
						if (vfxGroup==VFXGROUP_UNDEFINED)
						{
#if CHECK_VFXGROUP_DATA
							vfxAssertf(0, "invalid vfxgroup found in weaponFx.dat %s", charBuff);
#endif
							token.SkipToEndOfLine();
						}	
						else
						{
#if CHECK_VFXGROUP_DATA
							vfxAssertf(vfxGroupVerify[vfxGroup]==false, "duplicate vfxrroup data found in weaponFx.dat for %s", charBuff);
							vfxGroupVerify[vfxGroup]=true;
#endif
							for (s32 i=0; i<EWEAPONEFFECTGROUP_MAX; i++)
							{
								m_vfxWeaponTable[vfxGroup][i].id = token.GetInt();
							}
							token.SkipToEndOfLine();
						}
					}
				}
				// phase 1 - effect info
				else if (phase==1)
				{
					vfxAssertf(m_numVfxWeaponInfos<VFXWEAPON_MAX_INFOS, "not enough space to store weapon info");
					if (m_numVfxWeaponInfos<VFXWEAPON_MAX_INFOS)
					{
						// id
						m_vfxWeaponInfos[m_numVfxWeaponInfos].id = atoi(&charBuff[0]);

						// decal data
						s32 decalDataId = token.GetInt();
						if (decalDataId>0)
						{
							g_decalMan.FindRenderSettingInfo(decalDataId, m_vfxWeaponInfos[m_numVfxWeaponInfos].decalRenderSettingIndex, m_vfxWeaponInfos[m_numVfxWeaponInfos].decalRenderSettingCount);
						}
						else
						{
							m_vfxWeaponInfos[m_numVfxWeaponInfos].decalRenderSettingIndex = -1;
							m_vfxWeaponInfos[m_numVfxWeaponInfos].decalRenderSettingCount = 0;
						}

						if (m_version>=9.0f)
						{
							decalDataId = token.GetInt();
							if (decalDataId>0)
							{
								g_decalMan.FindRenderSettingInfo(decalDataId, m_vfxWeaponInfos[m_numVfxWeaponInfos].stretchRenderSettingIndex, m_vfxWeaponInfos[m_numVfxWeaponInfos].stretchRenderSettingCount);
							}
							else
							{
								m_vfxWeaponInfos[m_numVfxWeaponInfos].stretchRenderSettingIndex = -1;
								m_vfxWeaponInfos[m_numVfxWeaponInfos].stretchRenderSettingCount = 0;
							}

							m_vfxWeaponInfos[m_numVfxWeaponInfos].stretchDot = token.GetFloat();
							m_vfxWeaponInfos[m_numVfxWeaponInfos].stretchMult = token.GetFloat();
						}
						else
						{
							m_vfxWeaponInfos[m_numVfxWeaponInfos].stretchRenderSettingIndex = m_vfxWeaponInfos[m_numVfxWeaponInfos].decalRenderSettingIndex;
							m_vfxWeaponInfos[m_numVfxWeaponInfos].stretchRenderSettingCount = m_vfxWeaponInfos[m_numVfxWeaponInfos].decalRenderSettingCount;
							m_vfxWeaponInfos[m_numVfxWeaponInfos].stretchDot = 0.0f;
							m_vfxWeaponInfos[m_numVfxWeaponInfos].stretchMult = 1.0f;
						}

						if (m_version>=7.0f)
						{
							m_vfxWeaponInfos[m_numVfxWeaponInfos].decalRange = token.GetFloat();
						}
						else
						{
							m_vfxWeaponInfos[m_numVfxWeaponInfos].decalRange = 20.0f;
						}

						if (m_version>=4.0f)
						{
							m_vfxWeaponInfos[m_numVfxWeaponInfos].decalColR = (u8)token.GetInt();
							m_vfxWeaponInfos[m_numVfxWeaponInfos].decalColG = (u8)token.GetInt();
							m_vfxWeaponInfos[m_numVfxWeaponInfos].decalColB = (u8)token.GetInt();
						}
						else
						{
							m_vfxWeaponInfos[m_numVfxWeaponInfos].decalColR = 255;
							m_vfxWeaponInfos[m_numVfxWeaponInfos].decalColG = 255;
							m_vfxWeaponInfos[m_numVfxWeaponInfos].decalColB = 255;
						}

						if (m_version>=8.0f)
						{
							m_vfxWeaponInfos[m_numVfxWeaponInfos].decalColA = (u8)token.GetInt();
						}
						else
						{
							m_vfxWeaponInfos[m_numVfxWeaponInfos].decalColA = 255;
						}
						
						// sizes
						m_vfxWeaponInfos[m_numVfxWeaponInfos].minTexSize = token.GetFloat();
						m_vfxWeaponInfos[m_numVfxWeaponInfos].maxTexSize = token.GetFloat();

						vfxAssertf(m_vfxWeaponInfos[m_numVfxWeaponInfos].minTexSize<=m_vfxWeaponInfos[m_numVfxWeaponInfos].maxTexSize, "min texture size is greater than the max");

						// overlays / rejection
						if (m_version>=5.0f)
						{
							m_vfxWeaponInfos[m_numVfxWeaponInfos].maxOverlayRadius = token.GetFloat();
							m_vfxWeaponInfos[m_numVfxWeaponInfos].duplicateRejectDist = token.GetFloat();

							vfxAssertf(m_vfxWeaponInfos[m_numVfxWeaponInfos].maxOverlayRadius==0.0f || m_vfxWeaponInfos[m_numVfxWeaponInfos].duplicateRejectDist==0.0f, "max overlay radius and duplicate reject dist are both specified - these are mutually exclusive");
						}
						else
						{
							m_vfxWeaponInfos[m_numVfxWeaponInfos].maxOverlayRadius = 0.2f;
							m_vfxWeaponInfos[m_numVfxWeaponInfos].duplicateRejectDist = 0.0f;
						}
						
						// lifetimes
						m_vfxWeaponInfos[m_numVfxWeaponInfos].minLifeTime = token.GetFloat();
						m_vfxWeaponInfos[m_numVfxWeaponInfos].maxLifeTime = token.GetFloat();

						vfxAssertf(m_vfxWeaponInfos[m_numVfxWeaponInfos].minLifeTime<=m_vfxWeaponInfos[m_numVfxWeaponInfos].maxLifeTime, "min lifetime is greater than the max");

						// fade-in time
						m_vfxWeaponInfos[m_numVfxWeaponInfos].fadeInTime = token.GetFloat();

						if (m_version>=6.0f)
						{
							s32 exclusiveMtl = token.GetInt();
							m_vfxWeaponInfos[m_numVfxWeaponInfos].useExclusiveMtl = exclusiveMtl>0;
						}
						else
						{
							m_vfxWeaponInfos[m_numVfxWeaponInfos].useExclusiveMtl = false;
						}

						// shotgun v object override texture info
						if (m_version>=3.0f)
						{
							s32 shotgunDecalDataId = token.GetInt();
							if (shotgunDecalDataId>0)
							{
								g_decalMan.FindRenderSettingInfo(shotgunDecalDataId, m_vfxWeaponInfos[m_numVfxWeaponInfos].shotgunDecalRenderSettingIndex, m_vfxWeaponInfos[m_numVfxWeaponInfos].shotgunDecalRenderSettingCount);
							}
							else
							{
								m_vfxWeaponInfos[m_numVfxWeaponInfos].shotgunDecalRenderSettingIndex = -1;
								m_vfxWeaponInfos[m_numVfxWeaponInfos].shotgunDecalRenderSettingCount = 0;
							}

							m_vfxWeaponInfos[m_numVfxWeaponInfos].shotgunTexScale = token.GetFloat();
						}
						else
						{
							token.GetInt();
							token.GetInt();
							token.GetInt();
							token.GetInt();
							token.GetFloat();
							m_vfxWeaponInfos[m_numVfxWeaponInfos].shotgunDecalRenderSettingIndex = -1;
							m_vfxWeaponInfos[m_numVfxWeaponInfos].shotgunDecalRenderSettingCount = 0;
							m_vfxWeaponInfos[m_numVfxWeaponInfos].shotgunTexScale = 1.0f;
						}

						// fx system
						token.GetToken(charBuff, 128);		
						if (stricmp(charBuff, "-")==0)
						{	
							m_vfxWeaponInfos[m_numVfxWeaponInfos].ptFxWeaponHashName = (u32)0;					
						}
						else
						{
							m_vfxWeaponInfos[m_numVfxWeaponInfos].ptFxWeaponHashName = atHashWithStringNotFinal(charBuff);
						}

						if (m_version>=7.0f)
						{
							m_vfxWeaponInfos[m_numVfxWeaponInfos].ptFxRange = token.GetFloat();
						}
						else
						{
							m_vfxWeaponInfos[m_numVfxWeaponInfos].ptFxRange = 60.0f;
						}

						m_vfxWeaponInfos[m_numVfxWeaponInfos].ptFxScale = token.GetFloat();

						if (m_version>=7.0f)
						{
							m_vfxWeaponInfos[m_numVfxWeaponInfos].ptFxLodScale = token.GetFloat();
						}
						else
						{
							m_vfxWeaponInfos[m_numVfxWeaponInfos].ptFxLodScale = 1.0f;
						}

						// increment number of infos
						m_numVfxWeaponInfos++;
					}
				}
			}

#if CHECK_VFXGROUP_DATA
			for (s32 i=0; i<NUM_VFX_GROUPS; i++)
			{
				vfxAssertf(vfxGroupVerify[i]==true, "missing vfxgroup data found in weaponfx.dat for %s", g_fxGroupsList[i]);
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
		for (s32 j=0; j<EWEAPONEFFECTGROUP_MAX; j++)
		{
			GetInfo((VfxGroup_e)i, j);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessLookUpData
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWeapon::ProcessLookUpData				()
{
	// fill in the offset and count data in the lookup table
	s32 currId = -1;
	s32 currOffset = 0;
	s32 currCount = 0;
	for (s32 i=0; i<m_numVfxWeaponInfos; i++)
	{
		// check if the id is the same
		if (m_vfxWeaponInfos[i].id == currId)
		{
			// same id - update the info
			currCount++;
		}

		// check if the id has changed 
		if (m_vfxWeaponInfos[i].id!=currId)
		{
			// new id - store the old info
			StoreLookUpData(currId, currOffset, currCount);

			// set up the new info
			currId = m_vfxWeaponInfos[i].id;
			currOffset = i;
			currCount = 1;
		}
	}

	// store the final info
	StoreLookUpData(currId, currOffset, currCount);
}


///////////////////////////////////////////////////////////////////////////////
//  StoreLookUpData
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWeapon::StoreLookUpData					(s32 currId, s32 currOffset, s32 currCount)
{
	if (currId>-1)
	{
		for (s32 j=0; j<NUM_VFX_GROUPS; j++)
		{
			for (s32 k=0; k<EWEAPONEFFECTGROUP_MAX; k++)
			{
				if (m_vfxWeaponTable[j][k].id==currId)
				{
					vfxAssertf(m_vfxWeaponTable[j][k].offset==-1, "weapon table offset entry already set");
					vfxAssertf(m_vfxWeaponTable[j][k].count==-1, "weapon table count entry already set");
					m_vfxWeaponTable[j][k].offset = currOffset;
					m_vfxWeaponTable[j][k].count = currCount;
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWeapon::Update							()
{
	const camFrame& gameFrame = camInterface::GetGameplayDirector().GetFrame();
	Mat34V_ConstRef camMat = RCC_MAT34V(gameFrame.GetWorldMatrix());
	float fxLevel = 1.0f-CPortal::GetInteriorFxLevel();

	// interior smoke
	float targetSmokeLevel = 0.0f;
	CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();
	if (pIntInst && CPortalVisTracker::GetPrimaryRoomIdx()>0)
	{
		Vector3 vCamPos = gameFrame.GetPosition();
		Vector3 vCamForward = gameFrame.GetFront();
		targetSmokeLevel = pIntInst->GetRoomSmokeLevel(CPortalVisTracker::GetPrimaryRoomIdx(), vCamPos, vCamForward);
	}

	float smokeLevel = targetSmokeLevel*fxLevel;
	vfxAssertf(smokeLevel<=1.0f, "smoke level out of range");
	if (m_intSmokeOverrideLevel>0.0f)
	{
		smokeLevel = m_intSmokeOverrideLevel;
//		fxLevel = 1.0f;
	}

	UpdatePtFxInteriorSmoke(camMat, fxLevel, smokeLevel);
	//g_vfxLens.Register(VFXLENSTYPE_SMOKE, smokeLevel, 0.0f, 0.0f, 1.0f);

	// interior dust
	UpdatePtFxInteriorDust(camMat, fxLevel);

	// async probes
	m_asyncProbeMgr.Update();

#if __BANK && __DEV
	m_intSmokeTargetLevel = 0.0f;
	if (pIntInst && CPortalVisTracker::GetPrimaryRoomIdx()>0)
	{
		Vector3 vCamPos = gameFrame.GetPosition();
		Vector3 vCamForward = gameFrame.GetFront();
		m_intSmokeTargetLevel = pIntInst->GetRoomSmokeLevel(CPortalVisTracker::GetPrimaryRoomIdx(), vCamPos, vCamForward);

		const char* pInteriorName = pIntInst->GetMloModelInfo()->GetModelName();
		sprintf(m_intSmokeInteriorName, "%s", pInteriorName);

		const char* pRoomName = pIntInst->GetRoomName(CPortalVisTracker::GetPrimaryRoomIdx());
		sprintf(m_intSmokeRoomName, "%s", pRoomName);

		m_intSmokeRoomId = CPortalVisTracker::GetPrimaryRoomIdx();
	}
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveScript
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWeapon::RemoveScript							(scrThreadId scriptThreadId)
{
	if (scriptThreadId==m_bulletImpactPtFxUserZoomScaleScriptThreadId)
	{
		m_bulletImpactPtFxUserZoomScale = 1.0f;
		m_bulletImpactPtFxUserZoomScaleScriptThreadId = THREAD_INVALID;
	}

	if (scriptThreadId==m_bulletImpactPtFxLodRangeScaleScriptThreadId)
	{
		m_bulletImpactPtFxLodRangeScale = 1.0f;
		m_bulletImpactPtFxLodRangeScaleScriptThreadId = THREAD_INVALID;
	}

	if (scriptThreadId==m_bulletImpactDecalRangeScaleScriptThreadId)
	{
		m_bulletImpactDecalRangeScale = 1.0f;
		m_bulletImpactDecalRangeScaleScriptThreadId = THREAD_INVALID;
	}

	if (scriptThreadId==m_bulletTraceNoAngleRejectScriptThreadId)
	{
		m_bulletTraceNoAngleReject = false;
		m_bulletTraceNoAngleRejectScriptThreadId = THREAD_INVALID;
	}
}


///////////////////////////////////////////////////////////////////////////////
// GetInfo
///////////////////////////////////////////////////////////////////////////////

VfxWeaponInfo_s*	CVfxWeapon::GetInfo							(VfxGroup_e vfxGroup, s32 weaponGroupId, bool isBloody)
{
	if (CLocalisation::Blood()==false)
	{
		isBloody = false;
	}

	// get the id from the table
	vfxAssertf(vfxGroup<NUM_VFX_GROUPS, "vfx group out of range");
	vfxAssertf(vfxGroup!=VFXGROUP_UNDEFINED, "undefined vfx group found");
	vfxAssertf(weaponGroupId<EWEAPONEFFECTGROUP_MAX, "weapon group id out of range");
	s32 offset = m_vfxWeaponTable[vfxGroup][weaponGroupId].offset;

	// check for no infos
	if (offset==-1)
	{
		return NULL;
	}

	s32 count = m_vfxWeaponTable[vfxGroup][weaponGroupId].count;

	if (m_vfxWeaponTable[vfxGroup][weaponGroupId].id<100)
	{
		// special blood entries (must be an even number of entries - the first half are the non blood entries - the second half are the blood ones)
		vfxAssertf(count%2==0, "must be an even number of blood entries");
		count = count/2;
		if (isBloody)
		{
			offset = offset + count;
		}
	}

	s32 rand = g_DrawRand.GetRanged(offset, offset+count-1);

	return &m_vfxWeaponInfos[rand];
}


///////////////////////////////////////////////////////////////////////////////
//  GetBreakableGlassId
///////////////////////////////////////////////////////////////////////////////

int				CVfxWeapon::GetBreakableGlassId				(s32 weaponGroupId)
{
	vfxAssertf(weaponGroupId<EWEAPONEFFECTGROUP_MAX, "weapon group id out of range");
	return m_vfxBreakableGlass[weaponGroupId];
}


///////////////////////////////////////////////////////////////////////////////
//  DoWeaponFiredVfx
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeapon::DoWeaponFiredVfx				(CWeapon* pWeapon)
{
	// ground disturbance probe
	if (pWeapon->GetWeaponInfo()->GetGroundDisturbFxEnabled())
	{
		const CEntity* pMuzzleEntity = pWeapon->GetMuzzleEntity();
		s32 muzzleBoneIndex = pWeapon->GetMuzzleBoneIndex();

		if (pMuzzleEntity==NULL || muzzleBoneIndex==-1)
		{
			return;
		}

		Mat34V vMuzzleMtx;
		CVfxHelper::GetMatrixFromBoneIndex(vMuzzleMtx, pMuzzleEntity, muzzleBoneIndex);

		Vec3V vStartPos = vMuzzleMtx.GetCol3();
		Vec3V vEndPos = vStartPos-Vec3V(0.0f, 0.0f, pWeapon->GetWeaponInfo()->GetGroundDisturbFxDist());

		m_asyncProbeMgr.TriggerGroundDisturbProbe(vStartPos, vEndPos, pWeapon, vMuzzleMtx.GetCol0());
	}
}


///////////////////////////////////////////////////////////////////////////////
//  DoWeaponImpactVfx
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeapon::DoWeaponImpactVfx				(const VfxCollInfo_s& vfxCollInfo, eDamageType damageType, CEntity* pFiringEntity)
{
// 	static phMaterialMgr::Id onlyProcessThisMtldId = (phMaterialMgr::Id)103;
// 	if ((PGTAMATERIALMGR->UnpackMtlId(vfxCollInfo.materialId)) != onlyProcessThisMtldId)
// 	{
// 		return;
// 	}

	if (NetworkInterface::IsSessionLeaving())   // don't do weapon effect when transitioning out, since the peds will not react.
		return;

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

	// get the distance to the closest camera
	float fxDistSqr = CVfxHelper::GetDistSqrToCamera(vfxCollInfo.vPositionWld);

	// get the vfx group and weapon info
	VfxGroup_e vfxGroup = PGTAMATERIALMGR->GetMtlVfxGroup(vfxCollInfo.materialId);
	VfxWeaponInfo_s* pFxWeaponInfo = g_vfxWeapon.GetInfo(vfxGroup, vfxCollInfo.weaponGroup, vfxCollInfo.isBloody);

	// check for hitting any liquids
	s8 liquidType = -1;
	u32 foundDecalTypeFlag = 0;

	bool checkForLiquid = damageType==DAMAGE_TYPE_BULLET || damageType==DAMAGE_TYPE_BULLET_RUBBER;
	if (firingPedIsPlayer==false)
	{	
		checkForLiquid = g_DrawRand.GetRanged(0, 10)==5;
	}

	if (checkForLiquid)
	{
		// normally we have a small distance threshold for the liquid decal test
		// however, liquid decals on vehicles project onto the drawable which could be some distance from the weapon collision
		// so we increase the threshold in these cases
		float liquidDistThresh = 0.01f;
		if (vfxCollInfo.regdEnt && vfxCollInfo.regdEnt->GetIsTypeVehicle())
		{
			liquidDistThresh = 0.1f;
		}

		VfxWeaponInfo_s fxWeaponInfoMerged;
		if (g_decalMan.IsPointInLiquid(1<<DECALTYPE_POOL_LIQUID | 1<<DECALTYPE_TRAIL_LIQUID, liquidType, vfxCollInfo.vPositionWld, 0.2f, false, false, DECAL_FADE_OUT_TIME, foundDecalTypeFlag, liquidDistThresh))
		{
			VfxGroup_e vfxGroupLiquid = g_vfxLiquid.GetLiquidInfo((VfxLiquidType_e)liquidType).vfxGroup;
			VfxWeaponInfo_s* pFxWeaponInfoLiquid = g_vfxWeapon.GetInfo(vfxGroupLiquid, vfxCollInfo.weaponGroup, vfxCollInfo.isBloody);
			if (pFxWeaponInfoLiquid)
			{
				// merge the liquid and non-liquid data to form a new 'merged' weapon info
				// default to the non-liquid data and add the liquid data particle effects data
				if (pFxWeaponInfo)
				{
					fxWeaponInfoMerged = *pFxWeaponInfo;
					fxWeaponInfoMerged.ptFxWeaponHashName = pFxWeaponInfoLiquid->ptFxWeaponHashName;
					fxWeaponInfoMerged.ptFxScale = pFxWeaponInfoLiquid->ptFxScale;
				}
				else
				{
					fxWeaponInfoMerged = *pFxWeaponInfoLiquid;
				}

				// now set up out pointer to point to this 'merged' weapon info
				pFxWeaponInfo = &fxWeaponInfoMerged;
			}
		}
	}

	// check for igniting petrol
	if ((VfxLiquidType_e)liquidType==VFXLIQUID_TYPE_PETROL &&
		damageType==DAMAGE_TYPE_BULLET &&
		g_decalMan.GetDisablePetrolDecalsIgniting()==false)
	{
		// reject if too close to existing fire
		bool tooClose = false;
		Vec3V vClosestFirePos;
		if (g_fireMan.GetClosestFirePos(vClosestFirePos, vfxCollInfo.vPositionWld))
		{
			Vec3V vDecalToFire = vClosestFirePos-vfxCollInfo.vPositionWld;
			if (MagSquared(vDecalToFire).Getf()<1.0f)
			{
				tooClose = true;
			}
		}

		// create a fire
		if (!tooClose)
		{
			CPed* pCulprit = (pFiringEntity && pFiringEntity->GetIsTypePed() ? static_cast<CPed*>(pFiringEntity) : NULL);
			g_fireMan.StartPetrolFire(vfxCollInfo.regdEnt, vfxCollInfo.vPositionWld, vfxCollInfo.vNormalWld, pCulprit, foundDecalTypeFlag==(1<<DECALTYPE_POOL_LIQUID), vfxCollInfo.vPositionWld, BANK_ONLY(vfxCollInfo.vPositionWld,) false);
		}
	}

	if (vfxCollInfo.isWater)
	{
		// override with the water fx group
		pFxWeaponInfo = g_vfxWeapon.GetInfo(VFXGROUP_LIQUID_WATER, vfxCollInfo.weaponGroup);
	}

	// if this is a vehicle with rubber collision material then force the material to be car_metal
	// this is because wheels stick out and can cause rubber textures to be applied to the car body
	VfxWeaponInfo_s* pFxWeaponInfo_Decal = pFxWeaponInfo;
	if (vfxCollInfo.regdEnt && vfxCollInfo.regdEnt->GetIsTypeVehicle() && vfxCollInfo.materialId==PGTAMATERIALMGR->g_idRubber)
	{
		pFxWeaponInfo_Decal = g_vfxWeapon.GetInfo(VFXGROUP_CAR_METAL, vfxCollInfo.weaponGroup);
	}


	if (pFxWeaponInfo)
	{
		bool doImpactFx = true;
		if (!vfxCollInfo.noDecal && vfxCollInfo.regdEnt && PGTAMATERIALMGR->GetIsNoDecal(vfxCollInfo.materialId)==false)
		{
			if (pFxWeaponInfo_Decal)
			{
				float decalRange = pFxWeaponInfo_Decal->decalRange;
				if (CFileLoader::GetPtfxSetting()>=CSettings::High)
				{
					decalRange *= DECAL_HIGH_QUALITY_RANGE_MULT;
				}

				float impactDecalRangeSqr = decalRange*decalRange;
				impactDecalRangeSqr *= m_bulletImpactDecalRangeScale*m_bulletImpactDecalRangeScale;

				//MP Only: if we are impacting a vehicle and the firing ped is player then allow the decal regardless of distance
				if (fxDistSqr<impactDecalRangeSqr || (NetworkInterface::IsGameInProgress() && firingPedIsPlayer && vfxCollInfo.regdEnt.Get() && vfxCollInfo.regdEnt.Get()->GetIsTypeVehicle()))
				{
					// reduce the chance of creating a bullet decal if this isn't the player and 
					// the bullet is either further than half the range away or is behind the camera
					bool addDecal = true;
					if (firingPedIsPlayer==false)
					{
						if (fxDistSqr>impactDecalRangeSqr*0.25f || CVfxHelper::IsBehindCamera(vfxCollInfo.vPositionWld))
						{
							addDecal = g_DrawRand.GetRanged(0, 10)<3;
						}
					}

					if( vfxCollInfo.regdEnt )
					{
						addDecal &= !vfxCollInfo.regdEnt->GetNoWeaponDecals();
					}

					int decalID = 0;
					if (addDecal)
					{
						decalID = g_decalMan.AddWeaponImpact(*pFxWeaponInfo_Decal, vfxCollInfo);
						doImpactFx = decalID != 0;
					}

	#if GTA_REPLAY
					if(decalID != 0)
					{
						CReplayMgr::RecordFx<CPacketPTexWeaponShot>(
							CPacketPTexWeaponShot(	vfxCollInfo,
													vfxCollInfo.regdEnt && vfxCollInfo.regdEnt->GetIsTypeVehicle() && vfxCollInfo.materialId==PGTAMATERIALMGR->g_idRubber,
													vfxCollInfo.isWater,
													false, 
													false,
													decalID,
													vfxCollInfo.isSnowball),
							vfxCollInfo.regdEnt.Get());
					}
	#endif

					// MN - overriding this for now to always try to play the particle effect
					doImpactFx = true;
				}
			}
		}

		if (doImpactFx)
		{
			float impactPtFxRangeSqr = pFxWeaponInfo->ptFxRange*pFxWeaponInfo->ptFxRange;
			impactPtFxRangeSqr *= m_bulletImpactPtFxLodRangeScale*m_bulletImpactPtFxLodRangeScale;
			if (firingPedIsPlayer)
			{
				impactPtFxRangeSqr *= 4.0f;
			}

			if (fxDistSqr<impactPtFxRangeSqr)
			{
				// Only cache interior location if our test can produce a valid interior location
				// or if the impact definitely didn't happen in an interior.
				fwInteriorLocation interiorLocation;
				VfxInteriorTest_e interiorTestResult = CVfxHelper::GetInteriorLocationFromStaticBounds(vfxCollInfo.regdEnt, vfxCollInfo.roomId, interiorLocation);

				// Adding this extra check to make sure that we invalidate the interior location if its unknown.
				if(interiorTestResult == ITR_UNKNOWN_INTERIOR)
				{
					interiorLocation.MakeInvalid();
				}
				bool bCacheInteriorLocation =  (interiorTestResult == ITR_KNOWN_INTERIOR || interiorTestResult == ITR_NOT_IN_INTERIOR);

				TriggerPtFxBulletImpact(pFxWeaponInfo, vfxCollInfo.regdEnt, vfxCollInfo.vPositionWld, vfxCollInfo.vNormalWld, vfxCollInfo.vDirectionWld, vfxCollInfo.isExitFx, pFiringPed, vfxCollInfo.weaponGroup==WEAPON_EFFECT_GROUP_VEHICLE_MG, interiorLocation, bCacheInteriorLocation);
			}
		}
	}

	// process any special materials
	ProcessSpecialMaterial(vfxCollInfo);
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessSpecialMaterial
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeapon::ProcessSpecialMaterial			(const VfxCollInfo_s& vfxCollInfo)
{
	// only process for weapons that have bullets
	if (vfxCollInfo.weaponGroup==WEAPON_EFFECT_GROUP_PISTOL_SMALL || 
		vfxCollInfo.weaponGroup==WEAPON_EFFECT_GROUP_PISTOL_LARGE || 
		vfxCollInfo.weaponGroup==WEAPON_EFFECT_GROUP_PISTOL_SILENCED || 
		vfxCollInfo.weaponGroup==WEAPON_EFFECT_GROUP_SMG || 
		vfxCollInfo.weaponGroup==WEAPON_EFFECT_GROUP_SHOTGUN || 
		vfxCollInfo.weaponGroup==WEAPON_EFFECT_GROUP_RIFLE_ASSAULT || 
		vfxCollInfo.weaponGroup==WEAPON_EFFECT_GROUP_RIFLE_SNIPER)
	{
		const char* pMtlName = PGTAMATERIALMGR->GetMaterial(vfxCollInfo.materialId).GetName();

		atHashWithStringNotFinal mtlHashName = atHashWithStringNotFinal(pMtlName);

		// particle effects
		VfxWeaponSpecialMtlPtFxInfo* pWeaponSpecialMtlPtFxInfo = g_vfxWeaponInfoMgr.GetWeaponSpecialMtlPtFxInfo(mtlHashName);
		if (pWeaponSpecialMtlPtFxInfo)
		{
			CParticleAttr ptxAttr;
			ptxAttr.SetPos(VEC3_ZERO);
			ptxAttr.qX = ptxAttr.qY = ptxAttr.qZ = 0.0f;
			ptxAttr.qW = 1.0f;
			ptxAttr.m_tagHashName = pWeaponSpecialMtlPtFxInfo->m_shotPtFxTagName;
			ptxAttr.m_fxType = PT_SHOT_FX;
			ptxAttr.m_boneTag = 0;
			ptxAttr.m_scale = pWeaponSpecialMtlPtFxInfo->m_scale;
			ptxAttr.m_probability = (s32)(pWeaponSpecialMtlPtFxInfo->m_probability*100);
			ptxAttr.m_hasTint = true;
			ptxAttr.m_tintR = pWeaponSpecialMtlPtFxInfo->m_colTint.GetRed();
			ptxAttr.m_tintG = pWeaponSpecialMtlPtFxInfo->m_colTint.GetGreen();
			ptxAttr.m_tintB = pWeaponSpecialMtlPtFxInfo->m_colTint.GetBlue();
			ptxAttr.m_tintA = pWeaponSpecialMtlPtFxInfo->m_colTint.GetAlpha();
			ptxAttr.m_ignoreDamageModel = false;
			ptxAttr.m_playOnParent = false;
			ptxAttr.m_onlyOnDamageModel = false;
			ptxAttr.m_allowRubberBulletShotFx = false;
			ptxAttr.m_allowElectricBulletShotFx = false;

			g_vfxEntity.TriggerPtFxEntityShot(NULL, &ptxAttr, vfxCollInfo.vPositionWld, vfxCollInfo.vNormalWld, 0);
		}

		// explosions
		VfxWeaponSpecialMtlExpInfo* pWeaponSpecialMtlExpInfo = g_vfxWeaponInfoMgr.GetWeaponSpecialMtlExpInfo(mtlHashName);
		if (pWeaponSpecialMtlExpInfo)
		{
			// use the probability to see if we need to go further
			if (g_DrawRand.GetFloat()<=pWeaponSpecialMtlExpInfo->m_probability)
			{
				if (ptfxHistory::Check(pWeaponSpecialMtlExpInfo->m_shotPtExpTagName, 0, NULL, vfxCollInfo.vPositionWld, pWeaponSpecialMtlExpInfo->m_historyCheckDist))
				{
					return;
				}

				CExplosionAttr expAttr;
				expAttr.SetPos(VEC3_ZERO);
				expAttr.qX = expAttr.qY = expAttr.qZ = 0.0f;
				expAttr.qW = 1.0f;
				expAttr.m_tagHashName = pWeaponSpecialMtlExpInfo->m_shotPtExpTagName;
				expAttr.m_explosionType = XT_SHOT_PT_FX;
				expAttr.m_boneTag = 0;
				expAttr.m_ignoreDamageModel = false;
				expAttr.m_playOnParent = false;
				expAttr.m_onlyOnDamageModel = false;
				expAttr.m_allowRubberBulletShotFx = false;
				expAttr.m_allowElectricBulletShotFx = false;

				eExplosionTag expTag = eExplosionTag_Parse(expAttr.m_tagHashName);
				CExplosionManager::CExplosionArgs explosionArgs(expTag, RCC_VECTOR3(vfxCollInfo.vPositionWld));
				explosionArgs.m_vDirection = RCC_VECTOR3(vfxCollInfo.vNormalWld);
				explosionArgs.m_pAttachEntity = NULL;
				explosionArgs.m_attachBoneTag = -1;
				explosionArgs.m_pRelatedDummyForNetwork = NULL;
				CExplosionManager::AddExplosion(explosionArgs);

				ptfxHistory::Add(pWeaponSpecialMtlExpInfo->m_shotPtExpTagName, 0, NULL, vfxCollInfo.vPositionWld, pWeaponSpecialMtlExpInfo->m_historyCheckTime);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxBulletImpact
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeapon::TriggerPtFxBulletImpact		(const VfxWeaponInfo_s* pVfxWeaponInfo, CEntity* pEntity, Vec3V_In vPos, Vec3V_In vNormal, Vec3V_In vDirection, bool isExitFx, const CPed* pFiringPed, bool UNUSED_PARAM(isVehicleMG), fwInteriorLocation hitPtInteriorLocation, bool useHitPtInteriorLocation)
{
	// play any required effect
	if (pVfxWeaponInfo->ptFxWeaponHashName)
	{
		// check for nearby effects of this type - don't play this one if another is being played too close by, allow this for clones
		bool bIsNetworkCloneAndFirerPlayer = false;
		if (NetworkInterface::IsGameInProgress() && pFiringPed && pFiringPed->IsPlayer())
		{
			bIsNetworkCloneAndFirerPlayer = (pEntity && pEntity->GetIsDynamic()) ? static_cast<CDynamicEntity*>(pEntity)->IsNetworkClone() : false;
		}
		if (!bIsNetworkCloneAndFirerPlayer && ptfxHistory::Check(pVfxWeaponInfo->ptFxWeaponHashName, (u32)isExitFx, pEntity, vPos, VFXWEAPON_IMPACT_HIST_DIST))
		{
			return;
		}

		// return if not on screen
		if (pEntity && pEntity->GetIsDynamic())
		{
			CDynamicEntity* pDynamicEntity = static_cast<CDynamicEntity*>(pEntity);
			// allow for clones if off screen because they are likely off screen
			if (!bIsNetworkCloneAndFirerPlayer && pDynamicEntity->GetIsVisibleInSomeViewportThisFrame()==false)
			{
				return;
			}
		}
		else
		{
			if (gVpMan.GetGameViewport()->GetGrcViewport().IsSphereVisibleInline(Vec4V(vPos, ScalarV(V_QUARTER)))==0) 
			{
				return;
			}
		}

		ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pVfxWeaponInfo->ptFxWeaponHashName);
		if (pFxInst)
		{
			// calc the world matrix
			Mat34V fxMat;
			CVfxHelper::CreateMatFromVecZAlign(fxMat, vPos, vNormal, vDirection);

			// set the position of the effect
			if (pEntity && pEntity->GetIsDynamic())
			{
				// make the fx matrix relative to the entity
				Mat34V relFxMat;
				CVfxHelper::CreateRelativeMat(relFxMat, pEntity->GetMatrix(), fxMat);

				ptfxAttach::Attach(pFxInst, pEntity, -1);

				pFxInst->SetOffsetMtx(relFxMat);
			}
			else
			{
				pFxInst->SetBaseMtx(fxMat);
			}

			// if this is a vehicle then override the colour of the effect with that of the vehicle
			if (pEntity && pEntity->GetIsTypeVehicle())
			{
				CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
				CRGBA rgba = CVehicleModelInfo::GetVehicleColours()->GetVehicleColour(pVehicle->GetBodyColour1());
				ptfxWrapper::SetColourTint(pFxInst, rgba.GetRGBA().GetXYZ());
			}

			// evo
			float dot = 1.0f-Dot(-vDirection, vNormal).Getf();
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("angle", 0x0f3805b5), dot);

			// user zoom
			float userZoom = pVfxWeaponInfo->ptFxScale;
			if (pFiringPed && pFiringPed->GetSpecialAbility() && pFiringPed->GetSpecialAbility()->IsActive() && pFiringPed->GetSpecialAbility()->GetType()==SAT_RAGE)
			{
				// special ability override
				userZoom *= VFXWEAPON_IMPACT_PTFX_ZOOM_TREVOR_RAGE;
			}
			else
			{
				userZoom *= m_bulletImpactPtFxUserZoomScale;
			}
			pFxInst->SetUserZoom(userZoom);

			// modify the lod values if this is a player fx
			float lodScalar = pVfxWeaponInfo->ptFxLodScale * m_bulletImpactPtFxLodRangeScale;
			if (pFiringPed && pFiringPed->IsPlayer())
			{
				lodScalar *= 2.0f;
			}

			pFxInst->SetLODScalar(lodScalar);

			// only cache the passed interior location if told to do so (the caller might not be able to provide one)
			if (useHitPtInteriorLocation)
			{
				pFxInst->SetInInterior(hitPtInteriorLocation.IsValid());
				pFxInst->SetInteriorLocation(hitPtInteriorLocation.GetAsUint32());
				pFxInst->SetIsInteriorLocationCached(true);
			}

			// start the effect
			pFxInst->Start();

			ptfxHistory::Add(pVfxWeaponInfo->ptFxWeaponHashName, (u32)isExitFx, pEntity, vPos, VFXWEAPON_IMPACT_HIST_TIME);

#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				CReplayMgr::RecordFx<CPacketWeaponBulletImpactFx>(
					CPacketWeaponBulletImpactFx(pVfxWeaponInfo->ptFxWeaponHashName, 
					vPos,
					Normalize(vNormal),
					lodScalar,
					pVfxWeaponInfo->ptFxScale),
					pEntity);
			}
#endif

		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessPtFxMuzzleFlash
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeapon::ProcessPtFxMuzzleFlash			(NOT_REPLAY(const) CEntity* pFiringEntity, const CWeapon* pWeapon, CEntity* pMuzzleEntity, s32 muzzleBoneIndex, bool useAlternateFx, Vec3V_In muzzleOffset)
{
	TriggerPtFxMuzzleFlash(pFiringEntity, pWeapon, pMuzzleEntity, muzzleBoneIndex, useAlternateFx, muzzleOffset, false);
	TriggerPtFxMuzzleFlash(pFiringEntity, pWeapon, pMuzzleEntity, muzzleBoneIndex, useAlternateFx, muzzleOffset, true);
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxMuzzleFlash
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeapon::TriggerPtFxMuzzleFlash			(NOT_REPLAY(const) CEntity* pFiringEntity, const CWeapon* pWeapon, CEntity* pMuzzleEntity, s32 muzzleBoneIndex, bool useAlternateFx, Vec3V_In muzzleOffset, bool isFirstPersonPass)
{
	if (pMuzzleEntity==NULL || muzzleBoneIndex==-1)
	{
		return;
	}

	if (pFiringEntity && camInterface::GetGameplayDirector().IsFirstPersonAiming(pFiringEntity))
	{
		return;
	}

	// don't process if the firing entity is invisible
	if (pMuzzleEntity && pMuzzleEntity->GetIsTypeVehicle())
	{
		if (!pMuzzleEntity->GetIsVisible())
		{
			return;
		}
	}
	else
	{
		if (pFiringEntity && !pFiringEntity->GetIsVisible())
		{

			return;
		}
	}

	// check if the first person camera is active for the ped holding this projectile
	bool isFirstPersonCameraActive = false;
#if FPS_MODE_SUPPORTED
	const camBaseCamera* pCamera = camInterface::GetDominantRenderedCamera();
	isFirstPersonCameraActive = pCamera && pCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) && 
								pFiringEntity && ((pFiringEntity->GetIsTypePed() && static_cast<const CPed*>(pFiringEntity)->IsLocalPlayer()) || 
												  (pFiringEntity->GetIsTypeVehicle() && static_cast<const CVehicle*>(pFiringEntity)->ContainsLocalPlayer()));
#endif

	bool bAllowEarlyReturn = true;
	if (NetworkInterface::IsGameInProgress())
	{
		bAllowEarlyReturn = pFiringEntity && !NetworkUtils::IsNetworkClone(pFiringEntity) ? true : false;
	}

	// return if this is the first person pass and the first person camera isn't active
	if (bAllowEarlyReturn && isFirstPersonPass && !isFirstPersonCameraActive)
	{
		return;
	}

	const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();

	atHashWithStringNotFinal effectRuleHashName;
	if (pWeapon->GetSuppressorComponent() && pWeapon->GetSuppressorComponent()->GetInfo()->GetMuzzleFlashPtFxHashName() != 0)
	{
		effectRuleHashName = pWeapon->GetSuppressorComponent()->GetInfo()->GetMuzzleFlashPtFxHashName();
	}
	else
	{
		if (isFirstPersonPass)
		{
			if (useAlternateFx)
			{
				effectRuleHashName = pWeaponInfo->GetMuzzleFlashFirstPersonAlternatePtFxHashName();	
				if (effectRuleHashName.GetHash()==0)
				{
					effectRuleHashName = pWeaponInfo->GetMuzzleFlashAlternatePtFxHashName();	
				}
			}
			else
			{
				effectRuleHashName = pWeaponInfo->GetMuzzleFlashFirstPersonPtFxHashName();	
				if (effectRuleHashName.GetHash()==0)
				{
					effectRuleHashName = pWeaponInfo->GetMuzzleFlashPtFxHashName();	
				}
			}
		}
		else
		{
			if (useAlternateFx)
			{
				effectRuleHashName = pWeaponInfo->GetMuzzleFlashAlternatePtFxHashName();	
			}
			else
			{
				effectRuleHashName = pWeaponInfo->GetMuzzleFlashPtFxHashName();	
			}
		}
	}

	if (g_vfxBlood.IsOverridingWithClownBlood())
	{
		effectRuleHashName = atHashWithStringNotFinal("muz_clown", 0xf949b111);
	}

	float rangeSqr = VFXRANGE_WPN_MUZZLE_FLASH_SQR;
	if ((pFiringEntity && pFiringEntity->GetIsTypeVehicle()) || 
		(pMuzzleEntity && pMuzzleEntity->GetIsTypeVehicle()))
	{
		rangeSqr = VFXRANGE_WPN_MUZZLE_FLASH_VEH_SQR;
	}

	if (effectRuleHashName.GetHash()>0)
	{
		// check that there is a camera in range
		Mat34V vMuzzleBoneMtx;
		g_pPtfxCallbacks->GetMatrixFromBoneIndex(vMuzzleBoneMtx, pMuzzleEntity, muzzleBoneIndex);
		Vec3V vMuzzleBonePos = vMuzzleBoneMtx.GetCol3();

		if (CVfxHelper::GetDistSqrToCamera(vMuzzleBonePos)<rangeSqr)
		{
			// only play if on screen
			if (gVpMan.GetGameViewport()->GetGrcViewport().IsSphereVisibleInline(Vec4V(vMuzzleBonePos, ScalarV(V_HALF)))>0) 
			{
				bool driveBy = false;
				if (pFiringEntity->GetIsTypePed())
				{
					const CPed* pFiringPed = static_cast<const CPed*>(pFiringEntity);
					if (pFiringPed && pFiringPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
					{
						driveBy = true;
					}
				}

				// muzzle flash effects	
				ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(effectRuleHashName);
				if (pFxInst)
				{
					if (isFirstPersonPass)
					{
						// the first person pass plays the alternative effect, attached to the normal skeleton bone, only rendered in the first person view
						ptfxAttach::Attach(pFxInst, pMuzzleEntity, muzzleBoneIndex, PTFXATTACHMODE_FULL);
						pFxInst->SetFlag(PTFX_EFFECTINST_RENDER_ONLY_FIRST_PERSON_VIEW);
					}
					else
					{
						// the normal (non-first person) pass plays the normal effect
						if (isFirstPersonCameraActive)
						{
							// if the first person camera is active we want to attach to the alternative skeleton and only render in the non first person view 
							ptfxAttach::Attach(pFxInst, pMuzzleEntity, muzzleBoneIndex, PTFXATTACHMODE_FULL, true);
							pFxInst->SetFlag(PTFX_EFFECTINST_RENDER_ONLY_NON_FIRST_PERSON_VIEW);
						}
						else
						{
							// if the first person camera isn't active we want to attach to the normal skeleton and render at all times
							ptfxAttach::Attach(pFxInst, pMuzzleEntity, muzzleBoneIndex, PTFXATTACHMODE_FULL);
							pFxInst->ClearFlag(PTFX_EFFECTINST_RENDER_ONLY_NON_FIRST_PERSON_VIEW);
						}
					}

					// colour tint
					if (pWeaponInfo && pWeaponInfo->GetMuzzleUseProjTintColour())
					{
						if (pWeaponInfo->GetAmmoInfo() && pWeaponInfo->GetAmmoInfo()->GetIsClassId(CAmmoProjectileInfo::GetStaticClassId()))
						{
							const CAmmoProjectileInfo* pAmmoProjInfo = static_cast<const CAmmoProjectileInfo*>(pWeaponInfo->GetAmmoInfo());
							u8 uTintIdx = pWeapon->GetTintIndex();
							Vec3V vTintColor = pAmmoProjInfo->GetFxUseAlternateTintColor() ? g_vfxProjectile.GetAltWeaponProjTintColour(uTintIdx) : g_vfxProjectile.GetWeaponProjTintColour(uTintIdx);

							pFxInst->SetColourTint(vTintColor);
						}
					}

					pFxInst->SetOffsetPos(muzzleOffset);

					pFxInst->SetEvoValueFromHash(ATSTRINGHASH("driveby", 0xC4F83FA), driveBy ? 1.0f : 0.0f);

					pFxInst->SetUserZoom(pWeaponInfo->GetMuzzleFlashPtFxScale());

					pFxInst->Start();

#if GTA_REPLAY
					if(CReplayMgr::ShouldRecord())
					{
						CReplayMgr::RecordFx<CPacketWeaponMuzzleFlashFx>(
							CPacketWeaponMuzzleFlashFx(	effectRuleHashName,
							muzzleBoneIndex,
							pWeaponInfo->GetMuzzleFlashPtFxScale(),
							muzzleOffset),
							pMuzzleEntity);
					}
#endif

					// lights
					if (pWeaponInfo->GetMuzzleFlashLightEnabled())
					{
						// light settings
						Vec3V vLightDir(V_Z_AXIS_WZERO);
						Vec3V vLightTan(V_Y_AXIS_WZERO);

						// offset light slightly so that we don't get a particle vertex exactly at the same position
						// as the light source: otherwise there will be a divide by zero in the forward lighting code.
						Vec3V vLightPos = pFxInst->GetCurrPos() + (pFxInst->GetMatrix().GetCol0()*(Vec3VFromF32(pWeaponInfo->GetMuzzleFlashLightOffsetDist())+Vec3V(V_FLT_SMALL_4))); 
						Vec3V vLightCol = Vec3V(g_DrawRand.GetRanged(pWeaponInfo->GetMuzzleFlashLightRGBAMin().x, pWeaponInfo->GetMuzzleFlashLightRGBAMax().x),
												g_DrawRand.GetRanged(pWeaponInfo->GetMuzzleFlashLightRGBAMin().y, pWeaponInfo->GetMuzzleFlashLightRGBAMax().y),
												g_DrawRand.GetRanged(pWeaponInfo->GetMuzzleFlashLightRGBAMin().z, pWeaponInfo->GetMuzzleFlashLightRGBAMax().z));
						vLightCol *= ScalarV(1.0f/255.0f);
						float lightIntensity = g_DrawRand.GetRanged(pWeaponInfo->GetMuzzleFlashLightIntensityMinMax().x, pWeaponInfo->GetMuzzleFlashLightIntensityMinMax().y);
						float lightRange = g_DrawRand.GetRanged(pWeaponInfo->GetMuzzleFlashLightRangeMinMax().x, pWeaponInfo->GetMuzzleFlashLightRangeMinMax().y);
						float lightFalloff = g_DrawRand.GetRanged(pWeaponInfo->GetMuzzleFlashLightFalloffMinMax().x, pWeaponInfo->GetMuzzleFlashLightFalloffMinMax().y);
						bool castsShadows = pWeaponInfo->GetMuzzleFlashLightCastsShadows();

						// set up flags
						u32 lightFlags = LIGHTFLAG_FX;
						if (castsShadows)
						{
							lightFlags |= LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS | LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS;
						}

						// set up light
						CLightSource lightSource(LIGHT_TYPE_POINT, lightFlags, RCC_VECTOR3(vLightPos), RCC_VECTOR3(vLightCol), lightIntensity, LIGHT_ALWAYS_ON);
						lightSource.SetDirTangent(RCC_VECTOR3(vLightDir), RCC_VECTOR3(vLightTan));
						lightSource.SetRadius(lightRange);
						lightSource.SetFalloffExponent(lightFalloff);

						if (castsShadows)
						{
							lightSource.SetShadowTrackingId(fwIdKeyGenerator::Get(pWeapon, 0));
						}

						CEntity* pAttachEntity = static_cast<CEntity*>(pMuzzleEntity);
						fwInteriorLocation interiorLocation = pAttachEntity->GetInteriorLocation();
						lightSource.SetInInterior(interiorLocation);
						Lights::AddSceneLight(lightSource);
					}
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxMuzzleFlash_AirDefence
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeapon::TriggerPtFxMuzzleFlash_AirDefence(const CWeapon *pWeapon, const Mat34V mMat, const bool useAlternateFx)
{
	if (pWeapon)
	{
		const CWeaponInfo* pWeaponInfo = pWeapon->GetWeaponInfo();
		if (pWeaponInfo)
		{
			atHashWithStringNotFinal effectRuleHashName;
			if (useAlternateFx)
			{
				effectRuleHashName = pWeaponInfo->GetMuzzleFlashAlternatePtFxHashName();	
			}
			else
			{
				effectRuleHashName = pWeaponInfo->GetMuzzleFlashPtFxHashName();	
			}

			ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(effectRuleHashName);

			if (pFxInst)
			{
				pFxInst->SetBaseMtx(mMat);
				pFxInst->SetUserZoom(pWeaponInfo->GetMuzzleFlashPtFxScale());
				pFxInst->Start();
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//  ProcessPtFxGunshell
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeapon::ProcessPtFxGunshell			(const CEntity* pFiringEntity, const CWeapon* pWeapon, CEntity* pGunshellEntity, s32 gunshellBoneIndex, s32 weaponIndex)
{
	TriggerPtFxGunshell(pFiringEntity, pWeapon, pGunshellEntity, gunshellBoneIndex, weaponIndex, false);
	TriggerPtFxGunshell(pFiringEntity, pWeapon, pGunshellEntity, gunshellBoneIndex, weaponIndex, true);
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxGunshell
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeapon::TriggerPtFxGunshell			(const CEntity* pFiringEntity, const CWeapon* pWeapon, CEntity* pGunshellEntity, s32 gunshellBoneIndex, s32 weaponIndex, bool isFirstPersonPass)
{	
	if (pGunshellEntity==NULL || gunshellBoneIndex==-1)
	{
		return;
	}

	if (pFiringEntity && camInterface::GetGameplayDirector().IsFirstPersonAiming(pFiringEntity))
	{
		return;
	}

	// don't process if the firing entity is invisible
	if (pGunshellEntity && pGunshellEntity->GetIsTypeVehicle())
	{
		if (!pGunshellEntity->GetIsVisible())
		{
			return;
		}
	}
	else
	{
		if (pFiringEntity && !pFiringEntity->GetIsVisible())
		{

			return;
		}
	}

	// check clip component for override shell effect
	atHashWithStringNotFinal effectRuleHashName = pWeapon->GetWeaponInfo()->GetGunShellPtFxHashName();
	if (pWeapon && pWeapon->GetClipComponent())
	{
		atHashWithStringNotFinal shellOverrideHashName = pWeapon->GetClipComponent()->GetInfo()->GetShellFxHashName();
		if (shellOverrideHashName.IsNotNull())
		{
			effectRuleHashName = shellOverrideHashName;
		}
	}	

	// check if the first person camera is active for the ped holding this projectile
	bool isFirstPersonCameraActive = false;
#if FPS_MODE_SUPPORTED
	const camBaseCamera* pCamera = camInterface::GetDominantRenderedCamera();
	isFirstPersonCameraActive = pCamera && pCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) && 
								pFiringEntity && ((pFiringEntity->GetIsTypePed() && static_cast<const CPed*>(pFiringEntity)->IsLocalPlayer()) || 
												  (pFiringEntity->GetIsTypeVehicle() && static_cast<const CVehicle*>(pFiringEntity)->ContainsLocalPlayer()));
#endif

	// return if this is the first person pass and the first person camera isn't active
	if (isFirstPersonPass && !isFirstPersonCameraActive)
	{
		return;
	}

	if (isFirstPersonPass)
	{
		// check clip component for override first person shell effect
		atHashWithStringNotFinal shellFPHashName = pWeapon->GetWeaponInfo()->GetGunShellFirstPersonPtFxHashName();
		if (pWeapon && pWeapon->GetClipComponent())
		{
			atHashWithStringNotFinal shellFPOverrideHashName = pWeapon->GetClipComponent()->GetInfo()->GetShellFxFPHashName();
			if (shellFPOverrideHashName.IsNotNull())
			{
				shellFPHashName = shellFPOverrideHashName;
			}
		}	

		// if first person effect doesn't exist, fall back to third person
		if (shellFPHashName.IsNull())
		{
			effectRuleHashName = shellFPHashName;	
		}
	}

	if (g_vfxBlood.IsOverridingWithClownBlood())
	{
		effectRuleHashName = atHashWithStringNotFinal("eject_clown", 0x22a12c73);
	}

	if (effectRuleHashName.GetHash()>0)
	{
		// check that there is a camera in range
		Mat34V vGunshellBoneMtx;
		g_pPtfxCallbacks->GetMatrixFromBoneIndex(vGunshellBoneMtx, pGunshellEntity, gunshellBoneIndex);
		Vec3V vGunshellBonePos = vGunshellBoneMtx.GetCol3();

		float rangeSqr = VFXRANGE_WPN_GUNSHELL_SQR;
		if (pGunshellEntity && pGunshellEntity->GetIsTypeVehicle())
		{
			rangeSqr = VFXRANGE_WPN_GUNSHELL_VEH_SQR;
		}

		if (CVfxHelper::GetDistSqrToCamera(vGunshellBonePos)<rangeSqr)
		{
			// only play if on screen
			if (gVpMan.GetGameViewport()->GetGrcViewport().IsSphereVisibleInline(Vec4V(vGunshellBonePos, ScalarV(V_HALF)))>0) 
			{
				// register the fx system
				bool justCreated;
				ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pWeapon, isFirstPersonPass ? PTFXOFFSET_WEAPON_GUNSHELL_ALT+weaponIndex : PTFXOFFSET_WEAPON_GUNSHELL+weaponIndex, false, effectRuleHashName, justCreated, true);
				if (pFxInst)
				{
					if (isFirstPersonPass)
					{
						// the first person pass plays the alternative effect, attached to the normal skeleton bone, only rendered in the first person view
						ptfxAttach::Attach(pFxInst, pGunshellEntity, gunshellBoneIndex);
						pFxInst->SetFlag(PTFX_EFFECTINST_RENDER_ONLY_FIRST_PERSON_VIEW);
					}
					else
					{
						// the normal (non-first person) pass plays the normal effect
						if (isFirstPersonCameraActive)
						{
							// if the first person camera is active we want to attach to the alternative skeleton and only render in the non first person view 
							ptfxAttach::Attach(pFxInst, pGunshellEntity, gunshellBoneIndex, PTFXATTACHMODE_FULL, true);
							pFxInst->SetFlag(PTFX_EFFECTINST_RENDER_ONLY_NON_FIRST_PERSON_VIEW);
						}
						else
						{
							// if the first person camera isn't active we want to attach to the normal skeleton and render at all times
							ptfxAttach::Attach(pFxInst, pGunshellEntity, gunshellBoneIndex);
							pFxInst->ClearFlag(PTFX_EFFECTINST_RENDER_ONLY_NON_FIRST_PERSON_VIEW);
						}
					}

					// hack - flip some gunshell x axes for certain vehicle guns
					if (pGunshellEntity && pGunshellEntity->GetIsTypeVehicle())
					{
						CVehicle* pVehicle = static_cast<CVehicle*>(pGunshellEntity);
						if (pVehicle)
						{
							if (pVehicle->InheritsFromTrailer() && MI_TRAILER_TRAILERSMALL2.IsValid() && pVehicle->GetModelIndex()==MI_TRAILER_TRAILERSMALL2) 
							{
								if (gunshellBoneIndex==23 || gunshellBoneIndex==27)
								{
									pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
								}
							}
							else if (MI_CAR_HALFTRACK.IsValid() && pVehicle->GetModelIndex()==MI_CAR_HALFTRACK) 
							{
								if (gunshellBoneIndex==79)
								{
									pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
								}
							}
							else if (MI_CAR_TAMPA3.IsValid() && pVehicle->GetModelIndex()==MI_CAR_TAMPA3) 
							{
								if (gunshellBoneIndex==94)
								{
									pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
								}
							}
						}
					}

					pFxInst->Start();

#if GTA_REPLAY
					if(CReplayMgr::ShouldRecord())
					{
						CReplayMgr::RecordFx<CPacketWeaponGunShellFx>(
							CPacketWeaponGunShellFx(effectRuleHashName, 
							PTFXOFFSET_WEAPON_GUNSHELL+weaponIndex,
							gunshellBoneIndex),
							pGunshellEntity);
					}
#endif
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxBulletTrace
///////////////////////////////////////////////////////////////////////////////

bool			CVfxWeapon::TriggerPtFxBulletTrace			(const CEntity* pFiringEntity, const CWeapon* pWeapon, const CWeaponInfo* pWeaponInfo, Vec3V_In vStartPos, Vec3V_In vEndPos)
{
	// don't process if the firing entity is invisible
	if (pFiringEntity)
	{
		// check if the firing entity is invisible
		bool isInvisible = !pFiringEntity->GetIsVisible();

		// override with the vehicle invisibility if the ped is in a vehicle
		// (when in a minitank the ped is the firing entity and they are invisible so we want to check the vehicle visibility instead)
		if (pFiringEntity->GetIsTypePed())
		{
			const CPed* pFiringPed = static_cast<const CPed*>(pFiringEntity);
			if (pFiringPed->GetIsInVehicle())
			{
				const CVehicle* pFiringVehicle = pFiringPed->GetVehiclePedInside();
				if (pFiringVehicle)
				{
					isInvisible = !pFiringVehicle->GetIsVisible();
				}
			}
		}

		if (isInvisible)
		{
			return false;
		}
	}

	// check if the first person camera is active for the ped holding this projectile
	bool isFirstPersonCameraActive = false;
#if FPS_MODE_SUPPORTED
	const camBaseCamera* pCamera = camInterface::GetDominantRenderedCamera();
	isFirstPersonCameraActive = pCamera && pCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) && 
		pFiringEntity && ((pFiringEntity->GetIsTypePed() && static_cast<const CPed*>(pFiringEntity)->IsLocalPlayer()) || 
		(pFiringEntity->GetIsTypeVehicle() && static_cast<const CVehicle*>(pFiringEntity)->ContainsLocalPlayer()));
#endif

	atHashWithStringNotFinal effectRuleHashName = pWeaponInfo->GetBulletTracerPtFxHashName();
	if (pWeapon && pWeapon->GetClipComponent())
	{
		atHashWithStringNotFinal tracerOverrideHashName = pWeapon->GetClipComponent()->GetInfo()->GetTracerFxHashName();
		if (tracerOverrideHashName.IsNotNull())
		{
			effectRuleHashName = tracerOverrideHashName;
		}
	}

	if (effectRuleHashName.GetHash()>0)
	{
		// check if the entire trace is behind the camera
		bool startPosBehindCam = CVfxHelper::IsBehindCamera(vStartPos);
		bool endPosBehindCam = CVfxHelper::IsBehindCamera(vEndPos);
		if (startPosBehindCam && endPosBehindCam)
		{
			return false;
		}


		// check if the trace is close enough to the camera
		float distSqrToCam = CVfxHelper::GetDistSqrToCamera(vStartPos, vEndPos);
		if (pFiringEntity && ((pFiringEntity->GetIsTypePed() && static_cast<const CPed*>(pFiringEntity)->IsPlayer()) || 
							  (pFiringEntity->GetIsTypeVehicle() && static_cast<const CVehicle*>(pFiringEntity)->ContainsPlayer())))
		{
		}
		else
		{
			if (distSqrToCam>VFXRANGE_WPN_BULLET_TRACE_SQR)
			{
				return false;
			}
		}

		Vec3V vTraceDir = vEndPos-vStartPos;
		float traceLen = Mag(vTraceDir).Getf();
		if (traceLen<0.1f)
		{
			return false;
		}
		vTraceDir = Normalize(vTraceDir);

#if __BANK
		if (m_renderDebugBulletTraces)
		{
			grcDebugDraw::Line(vStartPos, vEndPos, Color32(1.0f, 1.0f, 0.0f, 1.0f), -300);
		}
#endif
		bool bIgnoreCamIntersectionTest =	NetworkInterface::IsGameInProgress() && 
											pFiringEntity && 
											pFiringEntity->GetIsTypePed() && 
											static_cast<const CPed*>(pFiringEntity)->IsPlayer() &&
											pWeaponInfo->GetBulletTracerPtFxIgnoreCameraIntersection();

		// reject if too close to the camera and intersecting the camera plane
		if (!bIgnoreCamIntersectionTest && startPosBehindCam!=endPosBehindCam && distSqrToCam<VFXWEAPON_TRACE_CAM_REJECT_DIST*VFXWEAPON_TRACE_CAM_REJECT_DIST)
		{
			return false;
		}

		// play any required effect
		ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(effectRuleHashName);
		if (pFxInst)
		{
			// calc the world matrix
			Mat34V fxMat;
			CVfxHelper::CreateMatFromVecZ(fxMat, vStartPos, vTraceDir);

#if __BANK
			CObject* pWeaponObj = NULL;

			if (m_attachBulletTraces)
			{
				if (pWeapon)
				{
					CEntity* pWeaponEntity = const_cast<CEntity*>(pWeapon->GetEntity());
					if (pWeaponEntity)
					{
						if (pWeaponEntity->GetIsTypeObject())
						{
							pWeaponObj = static_cast<CObject*>(pWeaponEntity);
						}
					}
				}
			}

			if (pWeaponObj)
			{
				// make the fx matrix relative to the entity
				Mat34V relFxMat;
				CVfxHelper::CreateRelativeMat(relFxMat, pWeaponObj->GetMatrix(), fxMat);

				ptfxAttach::Attach(pFxInst, pWeaponObj, -1);

				pFxInst->SetOffsetMtx(relFxMat);
			}
			else
#endif
			{
				// set the position
				pFxInst->SetBaseMtx(fxMat);
			}

			// set the far clamp
			pFxInst->SetOverrideFarClipDist(traceLen);

			// set the render flags - we don't want to see this effect in mirrors when in first person view
			if (isFirstPersonCameraActive)
			{
				pFxInst->SetFlag(PTFX_EFFECTINST_RENDER_ONLY_FIRST_PERSON_VIEW);
			}
			else
			{
				pFxInst->ClearFlag(PTFX_EFFECTINST_RENDER_ONLY_FIRST_PERSON_VIEW);
			}

			if (pWeapon)
			{
				Vec3V colour = GetWeaponTintColour(pWeapon->GetTintIndex(), pWeapon->GetAmmoInClip());

				if(RenderPhaseSeeThrough::GetState())
					colour = Vec3V(V_ONE);

				pFxInst->SetColourTint(colour);
			}

			// start the effect
			pFxInst->Start();

			return true;
		}
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  GetWeaponTintColour
///////////////////////////////////////////////////////////////////////////////

Vec3V_Out		CVfxWeapon::GetWeaponTintColour(u8 weaponTintIndex, s32 UNUSED_PARAM(numBulletsLeft))
{
	Vec3V vTintRGB(1.0f, 1.0f, 1.0f);

	if (weaponTintIndex==0)
	{
		// Normal
		vTintRGB = Vec3V(1.0f, 0.75f, 0.25f);
	}
	else if (weaponTintIndex==1)
	{
		// Normal
		vTintRGB = Vec3V(1.0f, 0.75f, 0.25f);
	}
		// White
	else if (weaponTintIndex==2)
	{
		// White
		vTintRGB = Vec3V(0.5f, 0.5f, 0.5f);
	}
	else if (weaponTintIndex==3)
	{
		// White
		vTintRGB = Vec3V(0.5f, 0.5f, 0.5f);
	}
	else if (weaponTintIndex==4)
	{
		// Olive Grey
		vTintRGB = Vec3V(0.5f, 0.6f, 0.4f);
	}
	else if (weaponTintIndex==5)
	{
		// Khaki
		vTintRGB = Vec3V(0.5f, 0.6f, 0.46f);
	}
	else if (weaponTintIndex==6)
	{
		// Blue Slate
		vTintRGB = Vec3V(0.4f, 0.6f, 1.0f);
	}
	else if (weaponTintIndex==7)
	{
		// Olive Grey
		vTintRGB = Vec3V(0.5f, 0.6f, 0.4f);
	}
	else if (weaponTintIndex==8)
	{
		// Olive Grey
		vTintRGB = Vec3V(0.5f, 0.6f, 0.4f);
	}
	else if (weaponTintIndex==9)
	{
		// Red
		vTintRGB = Vec3V(1.0f, 0.25f, 0.25f);
	}
	else if (weaponTintIndex==10)
	{
		// Blue
		vTintRGB = Vec3V(0.25f, 0.25f, 1.0f);
	}
	else if (weaponTintIndex==11)
	{
		// Gold
		vTintRGB = Vec3V(0.75f, 0.5f, 0.125f);
	}
	else if (weaponTintIndex==12)
	{
		// Orange
		vTintRGB = Vec3V(0.75f, 0.25f, 0.125f);
	}
	else if (weaponTintIndex==13)
	{
		// Pink
		vTintRGB = Vec3V(1.0f, 0.25f, 0.8f);
	}
	else if (weaponTintIndex==14)
	{
		// Purple
		vTintRGB = Vec3V(0.5f, 0.25f, 1.0f);
	}
	else if (weaponTintIndex==15)
	{
		// Orange
		vTintRGB = Vec3V(0.75f, 0.25f, 0.125f);
	}
	else if (weaponTintIndex==16)
	{
		// Green
		vTintRGB = Vec3V(0.125f, 0.5f, 0.125f);
	}
	else if (weaponTintIndex==17)
	{
		// Red
		vTintRGB = Vec3V(1.0f, 0.25f, 0.25f);
	}
	else if (weaponTintIndex==18)
	{
		// Neon Green
		vTintRGB = Vec3V(0.5f, 0.75f, 0.125f);
	}
	else if (weaponTintIndex==19)
	{
		// Cyan
		vTintRGB = Vec3V(0.25f, 0.5f, 1.0f);
	}
	else if (weaponTintIndex==20)
	{
		// Gold
		vTintRGB = Vec3V(0.75f, 0.5f, 0.125f);
	}
	else if (weaponTintIndex==21)
	{
		// Red
		vTintRGB = Vec3V(1.0f, 0.25f, 0.25f);
	}
	else if (weaponTintIndex==22)
	{
		// Blue
		vTintRGB = Vec3V(0.25f, 0.25f, 1.0f);
	}
	else if (weaponTintIndex==23)
	{
		// Gold
		vTintRGB = Vec3V(0.75f, 0.5f, 0.125f);
	}
	else if (weaponTintIndex==24)
	{
		// White
		vTintRGB = Vec3V(0.5f, 0.5f, 0.5f);
	}
	else if (weaponTintIndex==25)
	{
		// Beige
		vTintRGB = Vec3V(0.6f, 0.5f, 0.4f);
	}
	else if (weaponTintIndex==26)
	{
		// Neon Green
		vTintRGB = Vec3V(0.5f, 0.75f, 0.125f);
	}
	else if (weaponTintIndex==27)
	{
		// Red
		vTintRGB = Vec3V(1.0f, 0.25f, 0.25f);
	}
	else if (weaponTintIndex==28)
	{
		// Cyan
		vTintRGB = Vec3V(0.25f, 0.5f, 1.0f);
	}
	else if (weaponTintIndex==29)
	{
		// Cyan
		vTintRGB = Vec3V(0.25f, 0.5f, 1.0f);
	}
	else if (weaponTintIndex==30)
	{
		// Icy Blue
		vTintRGB = Vec3V(0.45f, 0.6f, 1.0f);
	}
	else if (weaponTintIndex==31)
	{
		// Orange
		vTintRGB = Vec3V(0.75f, 0.25f, 0.125f);
	}

/*	{
		// american
		if ((numBulletsLeft%2)==1)
		{
			// white
			vTintRGB = Vec3V(1.0f, 1.0f, 1.0f);
		}
		else if ((numBulletsLeft%4)==0)
		{
			// red
			vTintRGB = Vec3V(1.0f, 0.25f, 0.25f);
		}
		else 
		{
			// blue
			vTintRGB = Vec3V(0.25f, 0.25f, 1.0f);
		}
	}
*/
	return vTintRGB;
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxMtlReactWeapon
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWeapon::TriggerPtFxMtlReactWeapon			(const CWeaponInfo* pWeaponInfo, Vec3V_In vPos)
{
	// play any required effect (using the muzzle flash entry for the moment)
	atHashWithStringNotFinal effectRuleHashName = pWeaponInfo->GetMuzzleFlashPtFxHashName();
	if (effectRuleHashName.GetHash()>0)
	{
		ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(effectRuleHashName);
		if (pFxInst)
		{
			// calc the world matrix
			Mat34V fxMat;
			Vec3V vNormal(V_Z_AXIS_WZERO);
			CVfxHelper::CreateMatFromVecZ(fxMat, vPos, vNormal);

			// set the position
			pFxInst->SetBaseMtx(fxMat);

			// start the effect
			pFxInst->Start();
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxEMP
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWeapon::TriggerPtFxEMP			(CVehicle* pVehicle, float scale)
{
	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(ATSTRINGHASH("veh_stromberg_emp", 0xdc79e150));
	if (pFxInst)
	{
		// cache some vehicle matrix data
		Mat34V vVehMtx = pVehicle->GetMatrix();
		Vec3V vVehPos = vVehMtx.GetCol3();
		Vec3V vVehUp = vVehMtx.GetCol2();

		// calc the world matrix
		Mat34V vFxMat;
		CVfxHelper::CreateMatFromVecZ(vFxMat, vVehPos, vVehUp);

		Mat34V vRelFxMat;
		CVfxHelper::CreateRelativeMat(vRelFxMat, vVehMtx, vFxMat);

		ptfxAttach::Attach(pFxInst, pVehicle, -1);

		// set the position
		pFxInst->SetBaseMtx(vRelFxMat);

		pFxInst->SetUserZoom(scale);

		// start the effect
		pFxInst->Start();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxMuzzleSmoke
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWeapon::UpdatePtFxMuzzleSmoke				(const CEntity* FPS_MODE_SUPPORTED_ONLY(pFiringEntity), const CWeapon* pWeapon, CEntity* pMuzzleEntity, s32 muzzleBoneIndex, float smokeLevel, Vec3V_In muzzleOffset)
{
	if (pMuzzleEntity==NULL || muzzleBoneIndex==-1)
	{
		return;
	}

	atHashWithStringNotFinal effectRuleHashName = pWeapon->GetWeaponInfo()->GetMuzzleSmokePtFxHashName();


		// check if the first person camera is active for the ped holding this projectile
	bool isFirstPersonCameraActive = false;
#if FPS_MODE_SUPPORTED
	const camBaseCamera* pCamera = camInterface::GetDominantRenderedCamera();
	isFirstPersonCameraActive = pCamera && pCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) && 
		pFiringEntity && ((pFiringEntity->GetIsTypePed() && static_cast<const CPed*>(pFiringEntity)->IsLocalPlayer()) || 
		(pFiringEntity->GetIsTypeVehicle() && static_cast<const CVehicle*>(pFiringEntity)->ContainsLocalPlayer()));
#endif

	if (isFirstPersonCameraActive)
	{
		effectRuleHashName = pWeapon->GetWeaponInfo()->GetMuzzleSmokeFirstPersonPtFxHashName();	
		if (effectRuleHashName.GetHash()==0)
		{
			effectRuleHashName = pWeapon->GetWeaponInfo()->GetMuzzleSmokePtFxHashName();	
		}
	}

	if (effectRuleHashName.GetHash()>0)
	{
		// check that there is a camera in range
		if (CVfxHelper::GetDistSqrToCamera(pMuzzleEntity->GetTransform().GetPosition())<VFXRANGE_WPN_MUZZLE_SMOKE_SQR)
		{
			// register the fx system
			bool justCreated;
			ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pWeapon, PTFXOFFSET_WEAPON_MUZZLE_SMOKE, false, effectRuleHashName, justCreated);

			// check the effect exists
			if (pFxInst)
			{
				// set position
				ptfxAttach::Attach(pFxInst, pMuzzleEntity, muzzleBoneIndex);

				// set offset
				pFxInst->SetOffsetPos(muzzleOffset);

				// set evos
				pFxInst->SetEvoValueFromHash(ATSTRINGHASH("smoke", 0xB19506B0), smokeLevel);

				// set the render flags - we don't want to see this effect in mirrors when in first person view
				if (isFirstPersonCameraActive)
				{
					pFxInst->SetFlag(PTFX_EFFECTINST_RENDER_ONLY_FIRST_PERSON_VIEW);
				}
				else
				{
					pFxInst->ClearFlag(PTFX_EFFECTINST_RENDER_ONLY_FIRST_PERSON_VIEW);
				}

				// check if the effect has just been created 
				if (justCreated)
				{		
					// it has just been created - start it and set it's position
					pFxInst->Start();
				}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
				else
				{
					// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
					vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "Effect attachment error");
				}
#endif

#if GTA_REPLAY
				if(CReplayMgr::ShouldRecord())
				{
					CReplayMgr::RecordFx<CPacketWeaponMuzzleSmokeFx>(
						CPacketWeaponMuzzleSmokeFx(effectRuleHashName, muzzleBoneIndex, smokeLevel, PTFXOFFSET_WEAPON_MUZZLE_SMOKE),
						pMuzzleEntity);
				}
#endif
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessPtFxVolumetric
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWeapon::ProcessPtFxVolumetric				(const CWeapon* pWeapon, CEntity* pMuzzleEntity, s32 muzzleBoneIndex)
{
	UpdatePtFxVolumetric(pWeapon, pMuzzleEntity, muzzleBoneIndex, false);
	UpdatePtFxVolumetric(pWeapon, pMuzzleEntity, muzzleBoneIndex, true);
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxVolumetric
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxWeapon::UpdatePtFxVolumetric				(const CWeapon* pWeapon, CEntity* pMuzzleEntity, s32 muzzleBoneIndex, bool isFirstPersonPass)
{
	if (pMuzzleEntity==NULL || muzzleBoneIndex==-1)
	{
		return;
	}

	// check if the first person camera is active for the ped holding this projectile
	bool isFirstPersonCameraActive = false;
#if FPS_MODE_SUPPORTED
	const camBaseCamera* pCamera = camInterface::GetDominantRenderedCamera();
	const CPed* pWeaponOwner = pWeapon->GetOwner();
	isFirstPersonCameraActive = pCamera && pCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) && 
		pWeaponOwner && pWeaponOwner->IsLocalPlayer();
#endif

	// if this is a vehicle weapon, determine which turret we're using so we can attach multiple volumetric ptfx to the same entity
	s32 iTurretIndex = 0;
	if (pMuzzleEntity->GetIsTypeVehicle())
	{
		CVehicle* pVehicleMuzzleEntity = static_cast<CVehicle*>(pMuzzleEntity);
		const CVehicleWeaponMgr* pVehicleWeaponMgr = pVehicleMuzzleEntity->GetVehicleWeaponMgr();
		if (pVehicleWeaponMgr)
		{
			// loop through all turrets on vehicle
			for (int i = 0; i < pVehicleWeaponMgr->GetNumTurrets(); i++)
			{
				// get the fixed weapon associated to that turret index
				const CTurret* pTurret = pVehicleWeaponMgr->GetTurret(i);
				if (pTurret)
				{
					const s32 iWeaponIndex = pVehicleWeaponMgr->GetWeaponIndexForTurret(*pTurret);
					const CVehicleWeapon* pVehicleWeapon = pVehicleWeaponMgr->GetVehicleWeapon(iWeaponIndex);
					if (pVehicleWeapon && pVehicleWeapon->GetType() == VGT_FIXED_VEHICLE_WEAPON)
					{
						const CFixedVehicleWeapon* pFixedWeapon = static_cast<const CFixedVehicleWeapon*>(pVehicleWeapon);
						if (pFixedWeapon)
						{
							// see if that weapon bone matches the one we're using
							const s32 iFixedWeaponBoneIndex = pVehicleMuzzleEntity->GetBoneIndex(pFixedWeapon->GetWeaponBone());
							if (iFixedWeaponBoneIndex == muzzleBoneIndex)
							{
								iTurretIndex = i;
								break;
							}
						}
					}
				}
			}

			vfxAssertf(iTurretIndex < 2, "CVfxWeapon::UpdatePtFxVolumetric only supports two turret ptfxoffsets on a single entity, expand enum");
		}
	}

	// deal with the first person camera being inactive
	if (!isFirstPersonCameraActive)
	{
		// remove any first person specific effects
		ptfxRegInst* pRegFxInst = ptfxReg::Find(pWeapon, PTFXOFFSET_WEAPON_VOLUMETRIC_ALT + iTurretIndex);
		if (pRegFxInst)
		{
			if (pRegFxInst->m_pFxInst)
			{
				pRegFxInst->m_pFxInst->Finish(false);
			}
		}

		// return if this is the first person pass
		if (isFirstPersonPass)
		{
			return;
		}
	}

	atHashWithStringNotFinal effectRuleHashName = pWeapon->GetWeaponInfo()->GetMuzzleFlashPtFxHashName();
	if (effectRuleHashName.GetHash()>0)
	{
		// check that there is a camera in range
		float distanceToCameraSquared = CVfxHelper::GetDistSqrToCamera(pMuzzleEntity->GetTransform().GetPosition());
		if	(distanceToCameraSquared<VFXRANGE_WPN_VOLUMETRIC_SQR ||
			(pMuzzleEntity->GetIsTypeVehicle() && distanceToCameraSquared<VFXRANGE_WPN_VOLUMETRIC_VEHICLE_SQR))
		{
			// register the fx system
			bool justCreated;
			ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pWeapon, isFirstPersonPass ? PTFXOFFSET_WEAPON_VOLUMETRIC_ALT + iTurretIndex : PTFXOFFSET_WEAPON_VOLUMETRIC + iTurretIndex, false, effectRuleHashName, justCreated);

			// check the effect exists
			if (pFxInst)
			{
				// set position
				if (isFirstPersonPass)
				{
					// the first person pass plays the alternative effect, attached to the normal skeleton bone, only rendered in the first person view
					ptfxAttach::Attach(pFxInst, pMuzzleEntity, muzzleBoneIndex);
					pFxInst->SetFlag(PTFX_EFFECTINST_RENDER_ONLY_FIRST_PERSON_VIEW);
				}
				else 
				{
					// the normal (non-first person) pass plays the normal effect
					if (isFirstPersonCameraActive)
					{
						// if the first person camera is active we want to attach to the alternative skeleton and only render in the non first person view 
						ptfxAttach::Attach(pFxInst, pMuzzleEntity, muzzleBoneIndex, PTFXATTACHMODE_FULL, true);
						pFxInst->SetFlag(PTFX_EFFECTINST_RENDER_ONLY_NON_FIRST_PERSON_VIEW);
					}
					else
					{
						// if the first person camera isn't active we want to attach to the normal skeleton and render at all times
						ptfxAttach::Attach(pFxInst, pMuzzleEntity, muzzleBoneIndex);
						pFxInst->ClearFlag(PTFX_EFFECTINST_RENDER_ONLY_NON_FIRST_PERSON_VIEW);
					}
				}

				// check if the effect has just been created 
				if (justCreated)
				{		
					// it has just been created - start it and set it's position
					pFxInst->Start();
				}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
				else
				{
					// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
					vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "Effect attachment error");
				}
#endif

#if GTA_REPLAY
				if(CReplayMgr::ShouldRecord())
				{
					CReplayMgr::RecordFx<CPacketVolumetricFx>(
						CPacketVolumetricFx(effectRuleHashName, muzzleBoneIndex, PTFXOFFSET_WEAPON_VOLUMETRIC + iTurretIndex),
						pMuzzleEntity);
				}
#endif
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxLaserSight
///////////////////////////////////////////////////////////////////////////////
/*
void			CVfxWeapon::UpdatePtFxLaserSight			(const CWeapon* pWeapon, Vec3V_In vStartPos, Vec3V_In vEndPos)
{
	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pWeapon, PTFXOFFSET_WEAPON_LASER_SIGHT, false, atHashWithStringNotFinal("bullet_laser_sight",0x90585004), justCreated);

	// check the effect exists
	if (pFxInst)
	{
		Vec3V vTraceDir = vEndPos-vStartPos;
		float traceLen = Mag(vTraceDir).Getf();
		//if (traceLen<1.0f)
		//{
		//	return;
		//}
		vTraceDir = Normalize(vTraceDir);

		// calc the world matrix
		Mat34V fxMat;
		CVfxHelper::CreateMatFromVecZ(fxMat, vStartPos, vTraceDir);

		// set the position
		pFxInst->SetBaseMtx(fxMat);

		// set the far clamp
		pFxInst->SetOverrideFarClipDist(traceLen);

		// evolutions
		Vec3V_ConstRef vCamForward = RCC_VEC3V(camInterface::GetFront());
		float angleEvo = 1.0f - Abs(Dot(vCamForward, vTraceDir).Getf());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("angle", 0xF3805B5), angleEvo);

		// check if the effect has just been created 
		if (justCreated)
		{		
			// it has just been created - start it and set it's position
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
*/




///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxWeaponGroundDisturb
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeapon::UpdatePtFxWeaponGroundDisturb			(const CWeapon* pWeapon, Vec3V_In vPos, Vec3V_In vNormal, Vec3V_In vDir, float distEvo, VfxDisturbanceType_e vfxDisturbanceType)
{
	atHashWithStringNotFinal fxHashName = u32(0);
	if (vfxDisturbanceType==VFX_DISTURBANCE_DEFAULT)
	{
		fxHashName = pWeapon->GetWeaponInfo()->GetGroundDisturbFxNameDefault();
	}
	else if (vfxDisturbanceType==VFX_DISTURBANCE_SAND)
	{
		fxHashName = pWeapon->GetWeaponInfo()->GetGroundDisturbFxNameSand();
	}
	else if (vfxDisturbanceType==VFX_DISTURBANCE_DIRT)
	{
		fxHashName = pWeapon->GetWeaponInfo()->GetGroundDisturbFxNameDirt();
	}
	else if (vfxDisturbanceType==VFX_DISTURBANCE_WATER)
	{
		fxHashName = pWeapon->GetWeaponInfo()->GetGroundDisturbFxNameWater();
	}
	else if (vfxDisturbanceType==VFX_DISTURBANCE_FOLIAGE)
	{
		fxHashName = pWeapon->GetWeaponInfo()->GetGroundDisturbFxNameFoliage();
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pWeapon, PTFXOFFSET_WEAPON_GROUND_DISTURB, true, fxHashName, justCreated);

	if (pFxInst)
	{
		// check that the effects hasn't jumped a large distance this frame
		// the position is determined by a probe downwards looking for ground 
		// so if the ground suddenly jumps in height (walls, cliffs etc) then we may emit a large amount of particles over this distance jump
		if (!justCreated)
		{
			Vec3V vPrevPos = pFxInst->GetCurrPos();
			if (Abs(vPrevPos.GetZf()-vPos.GetZf())>5.0f)
			{
				pFxInst->SetDontEmitThisFrame(true);
			}
		}

		Mat34V vMtx;
		CVfxHelper::CreateMatFromVecZAlign(vMtx, vPos, vNormal, vDir);
		pFxInst->SetBaseMtx(vMtx);

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("distance", 0xD6FF9582), distEvo);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set its position
			pFxInst->Start();
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxInteriorSmoke
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeapon::UpdatePtFxInteriorSmoke		(Mat34V_In camMat, float fxAlpha, float smokeEvo)
{
#if __BANK
	if (m_disableInteriorSmoke)
	{
		return;
	}
#endif

	// get the interior and room names
	atHashValue interiorHashValue = atHashValue(u32(0));
	atHashValue roomHashValue = atHashValue(u32(0));
	CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();
	if (pIntInst && CPortalVisTracker::GetPrimaryRoomIdx()>0)
	{
		interiorHashValue = pIntInst->GetMloModelInfo()->GetModelNameHash();
		roomHashValue = pIntInst->GetRoomHashcode(CPortalVisTracker::GetPrimaryRoomIdx());
	}
	else
	{
		return;
	}

	if (interiorHashValue.GetHash()==0 || roomHashValue.GetHash()==0)
	{
		return;
	}

	// get the vfx interior info
	CVfxInteriorInfo* pVfxInteriorInfo = g_vfxInteriorInfoMgr.GetInfo(interiorHashValue, roomHashValue);

	// return if smoke isn't enabled
	if (pVfxInteriorInfo==NULL || pVfxInteriorInfo->GetSmokePtFxEnabled()==false)
	{
		return;
	}
 
	// calculate the effect matrix
	Vec3V vMatC(V_Z_AXIS_WZERO);
	Vec3V vMatB = camMat.GetCol1();
	vMatB.SetZ(ScalarV(V_ZERO));
	vMatB = Normalize(vMatB);
	Vec3V vMatA = Cross(vMatB, vMatC);
	Vec3V vMatD = camMat.GetCol3();
	//vMatD += vMatB * ScalarV(V_FIVE);

	// place at player height
	CPed* pPed = CGameWorld::FindLocalPlayer();
	if (pPed)
	{
		vMatD.SetZ(pPed->GetTransform().GetPosition().GetZ());
	}

	Mat34V fxMat;
	fxMat.SetCol0(vMatA);
	fxMat.SetCol1(vMatB);
	fxMat.SetCol2(vMatC);
	fxMat.SetCol3(vMatD);

	// get the name of the effect inst
	atHashWithStringNotFinal effectRuleHashName = pVfxInteriorInfo->GetSmokePtFxName();
	if (m_intSmokeOverrideHashName!=0)
	{
		effectRuleHashName = m_intSmokeOverrideHashName;
	}
#if __BANK
	if (m_intSmokeOverrideName[0]!='\0')
	{
		effectRuleHashName = atHashWithStringNotFinal(m_intSmokeOverrideName);
	}
#endif

	// register the effect inst
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(this, PTFXOFFSET_VFXWEAPON_INTERIOR_SMOKE, false, effectRuleHashName, justCreated);
	if (pFxInst)
	{
		pFxInst->SetBaseMtx(fxMat);

		float overrideLevel = pVfxInteriorInfo->GetSmokePtFxLevelOverride();
		if (overrideLevel>0.0f)
		{
			smokeEvo = overrideLevel;
		}

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("smoke", 0xB19506B0), smokeEvo);

		ptfxWrapper::SetAlphaTint(pFxInst, fxAlpha);

		// don't apply any natural ambient light - just the artificial light from the interior
		// this stops the effect from becoming very bright as you approach the door
		pFxInst->SetNaturalAmbientMult(0.0f);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set its position
			pFxInst->Start();
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxInteriorDust
///////////////////////////////////////////////////////////////////////////////

void			CVfxWeapon::UpdatePtFxInteriorDust		(Mat34V_In camMat, float fxAlpha)
{
#if __BANK
	if (m_disableInteriorDust)
	{
		return;
	}
#endif

	// get the interior and room names
	atHashValue interiorHashValue = atHashValue(u32(0));
	atHashValue roomHashValue = atHashValue(u32(0));
	CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();
	if (pIntInst && CPortalVisTracker::GetPrimaryRoomIdx()>0)
	{
		interiorHashValue = pIntInst->GetMloModelInfo()->GetModelNameHash();
		roomHashValue = pIntInst->GetRoomHashcode(CPortalVisTracker::GetPrimaryRoomIdx());
	}
	else
	{
		return;
	}

	if (interiorHashValue.GetHash()==0 || roomHashValue.GetHash()==0)
	{
		return;
	}

	// get the vfx interior info
	CVfxInteriorInfo* pVfxInteriorInfo = g_vfxInteriorInfoMgr.GetInfo(interiorHashValue, roomHashValue);

	// return if dust isn't enabled
	if (pVfxInteriorInfo==NULL || pVfxInteriorInfo->GetDustPtFxEnabled()==false)
	{
		return;
	}

	// calculate the effect matrix
	Vec3V vMatC(V_Z_AXIS_WZERO);
	Vec3V vMatB = camMat.GetCol1();
	vMatB.SetZ(ScalarV(V_ZERO));
	vMatB = Normalize(vMatB);
	Vec3V vMatA = Cross(vMatB, vMatC);
	Vec3V vMatD = camMat.GetCol3();
	//vMatD += vMatB * ScalarV(V_FIVE);

	// place at player height
	CPed* pPed = CGameWorld::FindLocalPlayer();
	if (pPed)
	{
		vMatD.SetZ(pPed->GetTransform().GetPosition().GetZ());
	}

	Mat34V fxMat;
	fxMat.SetCol0(vMatA);
	fxMat.SetCol1(vMatB);
	fxMat.SetCol2(vMatC);
	fxMat.SetCol3(vMatD);

	// register the effect inst
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(this, PTFXOFFSET_VFXWEAPON_INTERIOR_DUST, false, pVfxInteriorInfo->GetDustPtFxName(), justCreated);
	if (pFxInst)
	{
		pFxInst->SetBaseMtx(fxMat);

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("dust", 0x0d9bfc05), pVfxInteriorInfo->GetDustPtFxEvo());

		ptfxWrapper::SetAlphaTint(pFxInst, fxAlpha);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set its position
			pFxInst->Start();
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void		 	CVfxWeapon::InitWidgets						()
{
	if (m_bankInitialised==false)
	{
		char txt[128];
		u32 weaponInfoSize = sizeof(VfxWeaponInfo_s);
		float weaponInfoPoolSize = (weaponInfoSize * VFXWEAPON_MAX_INFOS) / 1024.0f;

		bkBank* pVfxBank = vfxWidget::GetBank();
		pVfxBank->PushGroup("Vfx Weapon", false);
		{
			pVfxBank->AddTitle("INFO");
			sprintf(txt, "Num Weapon Infos (%d - %.2fK)", VFXWEAPON_MAX_INFOS, weaponInfoPoolSize);
			pVfxBank->AddSlider(txt, &m_numVfxWeaponInfos, 0, VFXWEAPON_MAX_INFOS, 0);
			pVfxBank->AddSlider("IntSmoke Room Id", &m_intSmokeRoomId, 0, 256, 0);
			pVfxBank->AddText("IntSmoke Interior Name", m_intSmokeInteriorName, 128);
			pVfxBank->AddText("IntSmoke Room Name", m_intSmokeRoomName, 128);
			pVfxBank->AddSlider("IntSmoke Target Smoke Level", &m_intSmokeTargetLevel, 0.0f, 10.0f, 0.0f);
#if __DEV
			pVfxBank->AddTitle("");
			pVfxBank->AddTitle("SETTINGS");
			pVfxBank->AddSlider("Impact History Dist", &VFXWEAPON_IMPACT_HIST_DIST, 0.0f, 100.0f, 0.25f, NullCB, "The distance at which a new impact effect will be rejected if a history exists within this range");
			pVfxBank->AddSlider("Impact History Time", &VFXWEAPON_IMPACT_HIST_TIME, 0.0f, 100.0f, 0.25f, NullCB, "The time that a history will exist for a new impact effect");
			pVfxBank->AddSlider("Impact PtFx Zoom - Trevor Rage", &VFXWEAPON_IMPACT_PTFX_ZOOM_TREVOR_RAGE, 0.0f, 10.0f, 0.1f, NullCB, "");
			pVfxBank->AddSlider("Bullet Trace Angle Reject Thresh", &VFXWEAPON_TRACE_ANGLE_REJECT_THRESH, 0.0f, 1.0f, 0.01f, NullCB, "");
			pVfxBank->AddSlider("Bullet Trace Cam Reject Dist", &VFXWEAPON_TRACE_CAM_REJECT_DIST, 0.0f, 10.0f, 0.1f, NullCB, "");
			pVfxBank->PushGroup("Interior Smoke", false);
			{
				pVfxBank->AddSlider("Inc Per Shot", &VFXWEAPON_INTSMOKE_INC_PER_SHOT, 0.0f, 1.0f, 0.01f, NullCB, "How much the interior smoke value increases per shot");
				pVfxBank->AddSlider("Inc Per Explosion", &VFXWEAPON_INTSMOKE_INC_PER_EXP, 0.0f, 1.0f, 0.01f, NullCB, "How much the interior smoke value increases per explosion");
				pVfxBank->AddSlider("Dec Per Update", &VFXWEAPON_INTSMOKE_DEC_PER_UPDATE, 0.0f, 1.0f, 0.005f, NullCB, "How much the interior smoke value decreases per update");
				pVfxBank->AddSlider("Max Level", &VFXWEAPON_INTSMOKE_MAX_LEVEL, 0.0f, 1.0f, 0.005f, NullCB, "The maximum value of the interior smoke value");
				pVfxBank->AddSlider("Interp Speed", &VFXWEAPON_INTSMOKE_INTERP_SPEED, 0.0f, 1.0f, 0.005f, NullCB, "The speed at which the interior smoke interpolates between rooms");
				pVfxBank->AddSlider("Min Grid Size", &VFXWEAPON_INTSMOKE_MIN_GRID_SIZE, 0.0f, 50.0f, 0.5f, NullCB, "The minimum grid size of the interior smoke per room");
			}
			pVfxBank->PopGroup();
#endif // __DEV

			pVfxBank->AddTitle("");
			pVfxBank->AddTitle("DEBUG");
			pVfxBank->AddSlider("IntSmoke Override Smoke Level", &m_intSmokeOverrideLevel, 0.0f, 1.0f, 0.01f);
			pVfxBank->AddText("IntSmoke Override Name", m_intSmokeOverrideName, 64);
			pVfxBank->AddToggle("Disable Interior Smoke Fx", &m_disableInteriorSmoke);
			pVfxBank->AddToggle("Disable Interior Dust Fx", &m_disableInteriorDust);
			pVfxBank->AddToggle("Force Bullet Traces", &m_forceBulletTraces);
			pVfxBank->AddToggle("Render Debug Bullet Traces", &m_renderDebugBulletTraces);
			pVfxBank->AddToggle("Attach Bullet Traces", &m_attachBulletTraces);
			pVfxBank->AddSlider("Bullet Impact PtFx User Zoom Scale", &m_bulletImpactPtFxUserZoomScale, 0.0f, 20.0f, 0.05f);
			pVfxBank->AddSlider("Bullet Impact PtFx Lod/Range Scale", &m_bulletImpactPtFxLodRangeScale, 0.0f, 20.0f, 0.5f);
			pVfxBank->AddSlider("Bullet Impact Decal Range Scale", &m_bulletImpactDecalRangeScale, 0.0f, 20.0f, 0.5f);
			pVfxBank->AddToggle("Bullet Impact Decal Overrides Active", &m_bulletImpactDecalOverridesActive);
			pVfxBank->AddSlider("Bullet Impact Decal Max Overlay Radius Override", &m_bulletImpactDecalOverrideMaxOverlayRadius, 0.0f, 2.0f, 0.01f);
			pVfxBank->AddSlider("Bullet Impact Decal Duplicate Reject Dist Override", &m_bulletImpactDecalOverrideDuplicateRejectDist, 0.0f, 2.0f, 0.01f);
			pVfxBank->AddButton("Trigger Player Vehicle EMP", TriggerPlayerVehicleEMP);
			pVfxBank->AddToggle("Render Ground Disturbance Probes", &m_renderGroundDisturbVfxProbes);

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
void			CVfxWeapon::Reset							()
{
	g_vfxWeapon.Shutdown(SHUTDOWN_WITH_MAP_LOADED);
	g_vfxWeapon.Init(INIT_AFTER_MAP_LOADED);
}
#endif // __BANK


///////////////////////////////////////////////////////////////////////////////
// Reset
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CVfxWeapon::TriggerPlayerVehicleEMP			()
{
	CVehicle* pPlayerVehicle = CGameWorld::GetMainPlayerInfo()->GetPlayerPed()->GetMyVehicle();
	if (pPlayerVehicle)
	{
		g_vfxWeapon.TriggerPtFxEMP(pPlayerVehicle);
	}
}
#endif // __BANK




