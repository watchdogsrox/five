///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxVehicle.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	01 June 2006
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxVehicle.h"

// rage
#include "phcore/materialmgr.h"
#include "rmptfx/ptxeffectinst.h"

// framework
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "vfx/ptfx/ptfxwrapper.h"
#include "vfx/ptfx/ptfxreg.h"
#include "vfx/vfxwidget.h"

// game
#include "Audio/HeliAudioEntity.h"
#include "Audio/planeaudioentity.h"
#include "Core/Game.h"
#include "Control/Replay/Replay.h"
#include "Control/Replay/Effects/ParticleVehicleFxPacket.h"
#include "Game/ModelIndices.h"
#include "Game/Weather.h"
#include "ModelInfo/VehicleModelInfoColors.h"
#include "Peds/Ped.h"
#include "Renderer/Water.h"
#include "Scene/Portals/Portal.h"
#include "Scene/World/GameWorld.h"
#include "TimeCycle/TimeCycle.h"
#include "VehicleAI/VehicleIntelligence.h"
#include "Vehicles/Boat.h"
#include "Vehicles/Heli.h"
#include "Vehicles/Planes.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Metadata/VfxVehicleInfo.h"
#include "Vfx/Misc/Fire.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/Systems/VfxWater.h"
#include "Vfx/Systems/VfxWheel.h"
#include "Vfx/VfxHelper.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define VFXVEHICLE_MAX_RECENTLY_DAMAGED_BY_PLAYER_INFOS		(8)
#define VFXVEHICLE_MAX_RECENTLY_CRASHED_INFOS				(8)


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////
	
CVfxVehicle g_vfxVehicle;

// rag tweakable settings 
dev_s32	VFXVEHICLE_DUMMY_MTL_ID_PROBE_FRAME_DELAY			= 0;
dev_float VFXVEHICLE_LIGHT_TRAIL_SPEED_EVO_MIN				= 0.5f;
dev_float VFXVEHICLE_LIGHT_TRAIL_SPEED_EVO_MAX				= 20.0f;

dev_float VFXVEHICLE_RECENTLY_CRASHED_DAMAGE_THRESH			= 40.0f;

dev_s32 VFXVEHICLE_PETROL_IGNITE_CRASH_TIMER				= 3000;
dev_float VFXVEHICLE_PETROL_IGNITE_DAMAGE_THRESH			= 40.0f;
dev_float VFXVEHICLE_PETROL_IGNITE_SPEED_THRESH				= 0.2f;
dev_float VFXVEHICLE_PETROL_IGNITE_RANGE					= 10.0f;
dev_float VFXVEHICLE_PETROL_IGNITE_PROBABILITY				= 0.1f;

dev_u32 VFXVEHICLE_NUM_WRECKED_FIRES_FADE_THRESH			= 4;
dev_u32 VFXVEHICLE_NUM_WRECKED_FIRES_REJECT_THRESH			= 5;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxVehicleAsyncProbeManager
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxVehicleAsyncProbeManager::Init			()
{
	// init the async probes
	for (int i=0; i<VFXVEHICLE_MAX_DOWNWASH_PROBES; i++)
	{
		m_downwashProbes[i].m_vfxProbeId = -1;
	}

	for (int i=0; i<VFXVEHICLE_MAX_GROUND_DISTURB_PROBES; i++)
	{
		m_groundDisturbProbes[i].m_vfxProbeId = -1;
	}

	m_dummyMtlIdProbe.m_vfxProbeId = -1;
	m_dummyMtlIdProbeCurrPoolIdx = 0;
	m_dummyMtlIdProbeFrameDelay = VFXVEHICLE_DUMMY_MTL_ID_PROBE_FRAME_DELAY;
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxVehicleAsyncProbeManager::Shutdown		()
{
	// init the async probes
	for (int i=0; i<VFXVEHICLE_MAX_DOWNWASH_PROBES; i++)
	{
		if (m_downwashProbes[i].m_vfxProbeId>=0)
		{
			CVfxAsyncProbeManager::AbortProbe(m_downwashProbes[i].m_vfxProbeId);
			m_downwashProbes[i].m_vfxProbeId = -1;
		}
	}

	for (int i=0; i<VFXVEHICLE_MAX_GROUND_DISTURB_PROBES; i++)
	{
		if (m_groundDisturbProbes[i].m_vfxProbeId>=0)
		{
			CVfxAsyncProbeManager::AbortProbe(m_groundDisturbProbes[i].m_vfxProbeId);
			m_groundDisturbProbes[i].m_vfxProbeId = -1;
		}
	}

	if (m_dummyMtlIdProbe.m_vfxProbeId>=0)
	{
		CVfxAsyncProbeManager::AbortProbe(m_dummyMtlIdProbe.m_vfxProbeId);
		m_dummyMtlIdProbe.m_vfxProbeId = -1;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxVehicleAsyncProbeManager::Update			()
{
	// update the downwash probes
	for (int i=0; i<VFXVEHICLE_MAX_DOWNWASH_PROBES; i++)
	{
		if (m_downwashProbes[i].m_vfxProbeId>=0)
		{
			// query the probe
			VfxAsyncProbeResults_s vfxProbeResults;
			if (CVfxAsyncProbeManager::QueryProbe(m_downwashProbes[i].m_vfxProbeId, vfxProbeResults))
			{
				// the probe has finished
				m_downwashProbes[i].m_vfxProbeId = -1;

				if (m_downwashProbes[i].m_regdVeh)
				{
					VfxDisturbanceType_e vfxDisturbanceType = VFX_DISTURBANCE_DEFAULT;
					Vec3V vColnPos = m_downwashProbes[i].m_vEndPos;
					Vec3V vColnNormal;
					float intersectRatio = 1.0f;
					float groundZ = -1000000.0f;

					float dist = m_downwashProbes[i].m_pVfxVehicleInfo->GetAircraftDownwashPtFxDist();

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
					WaterTestResultType waterType = CVfxHelper::TestLineAgainstWater(m_downwashProbes[i].m_vStartPos, vColnPos, vWaterLevel);
					bool foundWater = waterType>WATERTEST_TYPE_NONE;
					if (foundWater)
					{
						vColnPos = vWaterLevel;
						vColnNormal = Vec3V(V_Z_AXIS_WZERO);
						vfxDisturbanceType = VFX_DISTURBANCE_WATER;

						Vec3V vVec = m_downwashProbes[i].m_vStartPos - vColnPos;
						intersectRatio = CVfxHelper::GetInterpValue(Mag(vVec).Getf(), 0.0f, dist);
						groundZ = vColnPos.GetZf();
					}

					// add wind disturbance
					g_weather.AddWindDownwash(m_downwashProbes[i].m_vStartPos, m_downwashProbes[i].m_vDir, dist, WIND_HELI_WIND_DIST_FORCE, groundZ, WIND_HELI_WIND_DIST_Z_FADE_THRESH_MIN, WIND_HELI_WIND_DIST_Z_FADE_THRESH_MAX);

					if (foundGround || foundWater)
					{
						g_vfxVehicle.UpdatePtFxDownwash(m_downwashProbes[i].m_regdVeh, m_downwashProbes[i].m_pVfxVehicleInfo, vColnPos, vColnNormal, m_downwashProbes[i].m_downforceEvo, intersectRatio, vfxDisturbanceType, m_downwashProbes[i].m_ptFxId);

						// disturb the water surface
						if (vfxDisturbanceType==VFX_DISTURBANCE_WATER)
						{
							static dev_float variance	= 4.0f;
							float offsetX				= fwRandom::GetRandomNumberInRange(-variance, variance);
							float offsetY				= fwRandom::GetRandomNumberInRange(-variance, variance);
							static dev_float upBias		= 1.0f;
							float signVal				= fwRandom::GetRandomNumberInRange(0, 2)*2.0f - upBias;
							static dev_float intensity	= 1.0f;
							float disturbVal			= signVal*intensity*(1.0f-intersectRatio);
							float size					= 4.0f;
							fwEntity* pEntity = m_downwashProbes[i].m_regdVeh;
							CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
							if (MI_JETPACK_THRUSTER.IsValid() && pVehicle->GetModelIndex()==MI_JETPACK_THRUSTER)
							{
								static dev_float intensity2	= 0.25f;
								disturbVal = signVal*intensity2*(1.0f-intersectRatio);
								size = 1.0f;
							}

							float worldX				= vColnPos.GetXf() + offsetX;
							float worldY				= vColnPos.GetYf() + offsetY;
							Water::ModifyDynamicWaterSpeed(worldX,			worldY,			disturbVal, 1.0f);
							Water::ModifyDynamicWaterSpeed(worldX,			worldY + size,	disturbVal, 1.0f);
							Water::ModifyDynamicWaterSpeed(worldX,			worldY - size,	disturbVal, 1.0f);
							Water::ModifyDynamicWaterSpeed(worldX + size,	worldY,			disturbVal, 1.0f);
							Water::ModifyDynamicWaterSpeed(worldX - size,	worldY,			disturbVal, 1.0f);
						}
					}
				}
			}
		}
	}

	// update the ground disturbance probes
	for (int i=0; i<VFXVEHICLE_MAX_GROUND_DISTURB_PROBES; i++)
	{
		if (m_groundDisturbProbes[i].m_vfxProbeId>=0)
		{
			// query the probe
			VfxAsyncProbeResults_s vfxProbeResults;
			if (CVfxAsyncProbeManager::QueryProbe(m_groundDisturbProbes[i].m_vfxProbeId, vfxProbeResults))
			{
				// the probe has finished
				m_groundDisturbProbes[i].m_vfxProbeId = -1;

				if (m_groundDisturbProbes[i].m_regdVeh)
				{
					VfxDisturbanceType_e vfxDisturbanceType = VFX_DISTURBANCE_DEFAULT;
					Vec3V vColnPos = m_groundDisturbProbes[i].m_vEndPos;
					Vec3V vColnNormal;
					float intersectRatio = 1.0f;
					float groundZ = -1000000.0f;

					float dist = m_groundDisturbProbes[i].m_pVfxVehicleInfo->GetAircraftDownwashPtFxDist();

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
						g_vfxVehicle.UpdatePtFxVehicleGroundDisturb(m_groundDisturbProbes[i].m_regdVeh, m_groundDisturbProbes[i].m_pVfxVehicleInfo, vColnPos, vColnNormal, intersectRatio, vfxDisturbanceType);
					}
				}
			}
		}
	}

	// update the dummy mtl id probe
	if (m_dummyMtlIdProbe.m_vfxProbeId>=0)
	{
		// query the probe
		VfxAsyncProbeResults_s vfxProbeResults;
		if (CVfxAsyncProbeManager::QueryProbe(m_dummyMtlIdProbe.m_vfxProbeId, vfxProbeResults))
		{
			// the probe has finished
			m_dummyMtlIdProbe.m_vfxProbeId = -1;

			if (vfxProbeResults.hasIntersected)
			{
				CEntity* pHitEntity = CVfxAsyncProbeManager::GetEntity(vfxProbeResults);
				if (pHitEntity)
				{
					if (m_dummyMtlIdProbe.m_regdVeh)
					{
						phMaterialMgr::Id mtlId = vfxProbeResults.mtlId;
						phMaterialMgr::Id unpackedMtlId = PGTAMATERIALMGR->UnpackMtlId(mtlId);
						if (Verifyf(unpackedMtlId<PGTAMATERIALMGR->GetNumMaterials(), "probe returned an invalid material id (%" I64FMT "u - %" I64FMT "u) on %s at %.3f %.3f %.3f (probed from %.3f %.3f %.3f to %.3f %.3f %.3f)", 
							mtlId, unpackedMtlId, pHitEntity->GetDebugName(), 
							vfxProbeResults.vPos.GetXf(), vfxProbeResults.vPos.GetYf(), vfxProbeResults.vPos.GetZf(), 
							m_dummyMtlIdProbe.m_vStartPos.GetXf(), m_dummyMtlIdProbe.m_vStartPos.GetYf(), m_dummyMtlIdProbe.m_vStartPos.GetZf(),
							m_dummyMtlIdProbe.m_vEndPos.GetXf(), m_dummyMtlIdProbe.m_vEndPos.GetYf(), m_dummyMtlIdProbe.m_vEndPos.GetZf()))
						{
							m_dummyMtlIdProbe.m_regdVeh->SetDummyMtlId(unpackedMtlId);
						}
					}
				}
			}

			// reset the frame delay now that the probe has finished
			m_dummyMtlIdProbeFrameDelay = VFXVEHICLE_DUMMY_MTL_ID_PROBE_FRAME_DELAY;
		}
	}

	if (m_dummyMtlIdProbe.m_vfxProbeId==-1)
	{
		// check that the frame delay has counted down
		if (m_dummyMtlIdProbeFrameDelay==0)
		{
			CVehicle::Pool* pVehiclePool = CVehicle::GetPool();
			int numVehiclesInPool = (int) pVehiclePool->GetSize();

			// search for the next dummy vehicle
			for (int i=0; i<numVehiclesInPool; i++)
			{
				int currPoolIdx = (m_dummyMtlIdProbeCurrPoolIdx+i)%numVehiclesInPool;

				CVehicle* pVehicle = pVehiclePool->GetSlot(currPoolIdx);
				if (pVehicle && pVehicle->IsDummy())
				{
					// found one

					// get the vehicle bounding box info
					Vec3V vBBMin = Vec3V(V_ZERO);
					Vec3V vBBMax = Vec3V(V_ZERO);
					phArchetypeDamp* pArch = pVehicle->GetPhysArch();
					if (pArch)
					{
						phBound* pBound = pArch->GetBound();
						if (pBound)
						{
							vBBMin = pBound->GetBoundingBoxMin();
							vBBMax = pBound->GetBoundingBoxMax();
						}
					}

					Vec3V vVehiclePos = pVehicle->GetTransform().GetPosition();
					Vec3V vStartPos = vVehiclePos;
					Vec3V vEndPos = vVehiclePos + Vec3V(0.0f, 0.0f, vBBMin.GetZf()-1.0f);

					TriggerDummyMtlIdProbe(vStartPos, vEndPos, pVehicle);

					m_dummyMtlIdProbeCurrPoolIdx = currPoolIdx+1;

					break;
				}
			}
		}
		else
		{
			// count down the frame delay
			m_dummyMtlIdProbeFrameDelay--;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerDownwashProbe
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxVehicleAsyncProbeManager::TriggerDownwashProbe(Vec3V_In vStartPos, Vec3V_In vEndPos, Vec3V_In vDir, CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, float downforceEvo, u32 ptFxId)
{
	static u32 probeFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER;
	static u32 stateIncludeFlags = phLevelBase::STATE_FLAG_FIXED;

	// look for an available probe
	for (int i=0; i<VFXVEHICLE_MAX_DOWNWASH_PROBES; i++)
	{
		if (m_downwashProbes[i].m_vfxProbeId==-1)
		{
			m_downwashProbes[i].m_vfxProbeId = CVfxAsyncProbeManager::SubmitWorldProbe(vStartPos, vEndPos, probeFlags, stateIncludeFlags);
			m_downwashProbes[i].m_vStartPos = vStartPos;
			m_downwashProbes[i].m_vEndPos = vEndPos;
			m_downwashProbes[i].m_vDir = vDir;
			m_downwashProbes[i].m_regdVeh = pVehicle;
			m_downwashProbes[i].m_pVfxVehicleInfo = pVfxVehicleInfo;
			m_downwashProbes[i].m_downforceEvo = downforceEvo;
			m_downwashProbes[i].m_ptFxId = ptFxId;

#if __BANK
			if (g_vfxVehicle.m_renderDownwashVfxProbes)
			{
				grcDebugDraw::Line(vStartPos, vEndPos, Color32(0.0f, 0.0f, 1.0f, 1.0f), -200);
			}
#endif

			return;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerGroundDisturbProbe
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxVehicleAsyncProbeManager::TriggerGroundDisturbProbe(Vec3V_In vStartPos, Vec3V_In vEndPos, CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo)
{
	static u32 probeFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER;

	// look for an available probe
	for (int i=0; i<VFXVEHICLE_MAX_GROUND_DISTURB_PROBES; i++)
	{
		if (m_groundDisturbProbes[i].m_vfxProbeId==-1)
		{
			m_groundDisturbProbes[i].m_vfxProbeId = CVfxAsyncProbeManager::SubmitWorldProbe(vStartPos, vEndPos, probeFlags);
			m_groundDisturbProbes[i].m_vStartPos = vStartPos;
			m_groundDisturbProbes[i].m_vEndPos = vEndPos;
			m_groundDisturbProbes[i].m_regdVeh = pVehicle;
			m_groundDisturbProbes[i].m_pVfxVehicleInfo = pVfxVehicleInfo;

#if __BANK
			if (g_vfxVehicle.m_renderPlaneGroundDisturbVfxProbes)
			{
				grcDebugDraw::Line(vStartPos, vEndPos, Color32(1.0f, 0.0f, 1.0f, 1.0f), -200);
			}
#endif

			return;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerDummyMtlIdProbe
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxVehicleAsyncProbeManager::TriggerDummyMtlIdProbe(Vec3V_In vStartPos, Vec3V_In vEndPos, CVehicle* pVehicle)
{
	static u32 probeFlags = ArchetypeFlags::GTA_MAP_TYPE_MOVER;

	// look for an available probe
	if (m_dummyMtlIdProbe.m_vfxProbeId==-1)
	{
		m_dummyMtlIdProbe.m_vfxProbeId = CVfxAsyncProbeManager::SubmitWorldProbe(vStartPos, vEndPos, probeFlags);
		m_dummyMtlIdProbe.m_vStartPos = vStartPos;
		m_dummyMtlIdProbe.m_vEndPos = vEndPos;
		m_dummyMtlIdProbe.m_regdVeh = pVehicle;

#if __BANK
		if (g_vfxVehicle.m_renderDummyMtlIdVfxProbes)
		{
			grcDebugDraw::Line(vStartPos, vEndPos, Color32(1.0f, 0.0f, 0.0f, 1.0f), -200);
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxVehicle
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxVehicle::Init						(unsigned initMode)
{
	if (initMode==INIT_CORE)
	{
		m_downwashPtFxDisabledScriptThreadId = THREAD_INVALID;
		m_downwashPtFxDisabled = false;

		m_slipstreamPtFxLodRangeScaleScriptThreadId = THREAD_INVALID;
		m_slipstreamPtFxLodRangeScale = 1.0f;

#if __BANK
		m_bankInitialised = false;
		m_renderDownwashVfxProbes = false;
		m_renderPlaneGroundDisturbVfxProbes = false;
		m_renderDummyMtlIdVfxProbes = false;
		m_trainSquealOverride = 0.0f;
#endif
	}
	else if (initMode == INIT_AFTER_MAP_LOADED)
	{
	}
	else if (initMode==INIT_SESSION)
	{
		m_asyncProbeMgr.Init();

		// initialise the 'recently damaged by player' infos
		m_recentlyDamagedByPlayerInfos.Reset();
		m_recentlyDamagedByPlayerInfos.Reserve(VFXVEHICLE_MAX_RECENTLY_DAMAGED_BY_PLAYER_INFOS);
		m_recentlyDamagedByPlayerInfos.Resize(VFXVEHICLE_MAX_RECENTLY_DAMAGED_BY_PLAYER_INFOS);

		for (int i=0; i<VFXVEHICLE_MAX_RECENTLY_DAMAGED_BY_PLAYER_INFOS; i++)
		{
			m_recentlyDamagedByPlayerInfos[i].m_regdVeh = NULL;
			m_recentlyDamagedByPlayerInfos[i].m_timeLastDamagedMs = 0;
		}

		// initialise the 'recently crashed' infos
		m_recentlyCrashedInfos.Reset();
		m_recentlyCrashedInfos.Reserve(VFXVEHICLE_MAX_RECENTLY_CRASHED_INFOS);
		m_recentlyCrashedInfos.Resize(VFXVEHICLE_MAX_RECENTLY_CRASHED_INFOS);

		for (int i=0; i<VFXVEHICLE_MAX_RECENTLY_CRASHED_INFOS; i++)
		{
			m_recentlyCrashedInfos[i].m_regdVeh = NULL;
			m_recentlyCrashedInfos[i].m_timeLastCrashedMs = 0;
			m_recentlyCrashedInfos[i].m_damageApplied = 0.0f;
			m_recentlyCrashedInfos[i].m_petrolIgniteTestDone = false;
		}
    }
}

///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxVehicle::Shutdown					(unsigned shutdownMode)
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

void		 	CVfxVehicle::Update						()
{
	m_asyncProbeMgr.Update();
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveScript
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxVehicle::RemoveScript				(scrThreadId scriptThreadId)
{
	if (scriptThreadId==m_downwashPtFxDisabledScriptThreadId)
	{
		m_downwashPtFxDisabled = false;
		m_downwashPtFxDisabledScriptThreadId = THREAD_INVALID;
	}

	if (scriptThreadId==m_slipstreamPtFxLodRangeScaleScriptThreadId)
	{
		m_slipstreamPtFxLodRangeScale = 1.0f;
		m_slipstreamPtFxLodRangeScaleScriptThreadId = THREAD_INVALID;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetDownwashPtFxDisabled
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::SetDownwashPtFxDisabled	(bool val, scrThreadId scriptThreadId) 
{
	if (vfxVerifyf(m_downwashPtFxDisabledScriptThreadId==THREAD_INVALID || m_downwashPtFxDisabledScriptThreadId==scriptThreadId, "trying to disable downwash ptfx when this is already in use by another script")) 
	{
		m_downwashPtFxDisabled = val; 
		m_downwashPtFxDisabledScriptThreadId = scriptThreadId;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetSlipstreamPtFxLodRangeScale
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::SetSlipstreamPtFxLodRangeScale		(float val, scrThreadId scriptThreadId) 
{
	if (vfxVerifyf(m_slipstreamPtFxLodRangeScaleScriptThreadId==THREAD_INVALID || m_slipstreamPtFxLodRangeScaleScriptThreadId==scriptThreadId, "trying to set slipstream lod/range scale when this is already in use by another script")) 
	{
		m_slipstreamPtFxLodRangeScale = val; 
		m_slipstreamPtFxLodRangeScaleScriptThreadId = scriptThreadId;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  GetEngineDamageHealthThresh
///////////////////////////////////////////////////////////////////////////////

float			CVfxVehicle::GetEngineDamageHealthThresh	(CVehicle* pVehicle)
{
	if (pVehicle->InheritsFromPlane() || pVehicle->InheritsFromHeli())
	{
		float planeDamageThresholdOverride = pVehicle->m_Transmission.GetPlaneDamageThresholdOverride();
		if (planeDamageThresholdOverride>0.0f)
		{
			return planeDamageThresholdOverride;
		}
		else
		{
			return ENGINE_DAMAGE_PLANE_DAMAGE_START;
		}
	}

	return ENGINE_DAMAGE_RADBURST;
}
 

///////////////////////////////////////////////////////////////////////////////
//  ProcessEngineDamage
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::ProcessEngineDamage		(CVehicle* pVehicle)
{
	pVehicle->m_Transmission.SetFireEvo(0.0f);

	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	// do any leaking oil decals on the ground
	ProcessOilLeak(pVehicle, pVfxVehicleInfo);

	// do any leaking petrol decals on the ground
	ProcessPetrolLeak(pVehicle, pVfxVehicleInfo);

	// do any overturned effects
	ProcessOverturnedSmoke(pVehicle, pVfxVehicleInfo);

	// do overheating effects when this vehicle's health gets too low
	float engineHealth = pVehicle->GetVehicleDamage()->GetEngineHealth();
	if ((engineHealth<GetEngineDamageHealthThresh(pVehicle)) && pVehicle->GetStatus()!=STATUS_WRECKED)
	{
		if (pVfxVehicleInfo->GetEngineDamagePtFxEnabled())
		{
			if (pVfxVehicleInfo->GetEngineDamagePtFxHasPanel())
			{
				ProcessEngineDamagePanel(pVehicle, pVfxVehicleInfo);
			}
			else
			{
				ProcessEngineDamageNoPanel(pVehicle, pVfxVehicleInfo);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessEngineDamageNoPanel
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::ProcessEngineDamageNoPanel	(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo)
{
	for (int i=0; i<2; i++)
	{
		// get overheat matrix
		Matrix34 overheatMat;
		s32 overheatBoneIndex = pVehicle->GetBoneIndex((eHierarchyId)(VEH_OVERHEAT+i));
		if (overheatBoneIndex==-1)
		{
			continue;
		}

		CVfxHelper::GetMatrixFromBoneIndex(RC_MAT34V(overheatMat), pVehicle, overheatBoneIndex);
		if (overheatMat.a.IsZero())
		{
			continue;
		}

		fragInst* pFragInst = pVehicle->GetFragInst();
		if (pFragInst)
		{
			int component = pFragInst->GetControllingComponentFromBoneIndex(overheatBoneIndex);
			if (component>=0 && component<pFragInst->GetType()->GetPhysics(0)->GetNumChildren())
			{
				s32 nGroup = pFragInst->GetType()->GetPhysics(0)->GetAllChildren()[component]->GetOwnerGroupPointerIndex();
				if (pFragInst->GetCacheEntry() && !pFragInst->GetCacheEntry()->GetHierInst()->groupBroken->IsClear(nGroup))
				{
					continue;
				}
			}
		}

		// get the engine health
		float engineHealth = pVehicle->GetVehicleDamage()->GetEngineHealth();
		if (pVehicle->GetVehicleType()==VEHICLE_TYPE_PLANE)
		{
			CPlane* pPlane = static_cast<CPlane*>(pVehicle);
			engineHealth = pPlane->GetAircraftDamage().GetSectionHealth((CAircraftDamage::eAircraftSection)(CAircraftDamage::ENGINE_L+i));

			// check the engine health should trigger effects again as this is now a specific (left or right) engine
			// previous checks were done with the main engine health
			if (engineHealth>=GetEngineDamageHealthThresh(pVehicle))
			{
				continue;
			}
		}

		// check if the engine is underwater
		if (pVehicle->GetIsInWater())
		{
			float waterDepth;
			CVfxHelper::GetWaterDepth(RCC_VEC3V(overheatMat.d), waterDepth);
			if (waterDepth>0.0f)
			{
				// put out any engine fires if the engine is underwater
				if (engineHealth<=ENGINE_DAMAGE_ONFIRE)
				{
					engineHealth = ENGINE_DAMAGE_ONFIRE + 0.1f;
					return;
				}
				// otherwise skip emitting smoke this frame
				else
				{
					return;
				}
			}
		}

		// calc evolution variables
		float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocity().Mag(), pVfxVehicleInfo->GetEngineDamagePtFxSpeedEvoMin(), pVfxVehicleInfo->GetEngineDamagePtFxSpeedEvoMax());

		float damageEvo, fireEvo;
		CalcDamageAndFireEvos(pVehicle, engineHealth, damageEvo, fireEvo);

		// play the overheating
		g_vfxVehicle.UpdatePtFxEngineNoPanel(pVehicle, pVfxVehicleInfo, overheatBoneIndex, i, speedEvo, damageEvo, fireEvo);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessEngineDamagePanel
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::ProcessEngineDamagePanel	(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo)
{
	// get engine matrix 
	Matrix34 engineMat;
	const Matrix34* pEngineLclMat = NULL;
	s32 engineBoneIndex = pVehicle->GetBoneIndex(VEH_ENGINE);
	if (engineBoneIndex>-1)
	{
		CVfxHelper::GetMatrixFromBoneIndex(RC_MAT34V(engineMat), pVehicle, engineBoneIndex);
		pEngineLclMat = &pVehicle->GetLocalMtx(engineBoneIndex);
	}

	// check if we've got an engine node
	if (engineBoneIndex>-1 && pEngineLclMat)
	{
		// check if the engine is in the boot or the bonnet - get the component index
		eHierarchyId panelComponentId = VEH_BONNET;
		if (pEngineLclMat->d.y<0.0f)
		{
			// use boot component instead
			panelComponentId = VEH_BOOT;
		}

		// get some data about the vehicle
		bool isPanelDefined = pVehicle->HasComponent(panelComponentId);
		bool isPanelAttached = false;
		float panelOpenRatio = 1.0f;
		if (isPanelDefined)
		{
			s32 panelBoneIndex = pVehicle->GetBoneIndex(panelComponentId);
			Assert(panelBoneIndex);

			Matrix34 panelMat;
			CVfxHelper::GetMatrixFromBoneIndex(RC_MAT34V(panelMat), pVehicle, panelBoneIndex);

			if (!panelMat.a.IsZero())
			{
				isPanelAttached = true;
				Matrix34 m = MAT34V_TO_MATRIX34(pVehicle->GetMatrix());
				panelMat.DotTranspose(m);
				panelOpenRatio = rage::Abs(panelMat.b.z);
			}
		}

		// find out which effects to play
		bool doEngineFx = false;
		bool doVentFx = false;
		if (isPanelDefined)
		{
			// has a panel
			if (isPanelAttached && panelOpenRatio<0.15f)
			{
				// the panel is still attached and is shut
				doVentFx = true;
			}
			else
			{
				// the panel is either detached or open
				doEngineFx = true;
			}
		}
		else
		{
			doVentFx = true;
		}

		// calc evolution variables
		float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocity().Mag(), pVfxVehicleInfo->GetEngineDamagePtFxSpeedEvoMin(), pVfxVehicleInfo->GetEngineDamagePtFxSpeedEvoMax());

		float speedForwardEvo = 0.0f;
		float speedBackwardEvo = 0.0f;

		if (DotProduct(pVehicle->GetVelocity(), VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()))>=0.0f)
		{
			speedForwardEvo = speedEvo;
		}
		else
		{
			speedBackwardEvo = speedEvo;
		}

		// get the engine health
		float engineHealth = pVehicle->GetVehicleDamage()->GetEngineHealth();

		// play the overheating effects
		if (doEngineFx)
		{
			if (engineMat.a.IsZero())
			{
				return;
			}

			fragInst* pFragInst = pVehicle->GetFragInst();
			if (pFragInst)
			{
				int component = pFragInst->GetControllingComponentFromBoneIndex(engineBoneIndex);
				if (component>=0 && component<pFragInst->GetType()->GetPhysics(0)->GetNumChildren())
				{
					s32 nGroup = pFragInst->GetType()->GetPhysics(0)->GetAllChildren()[component]->GetOwnerGroupPointerIndex();
					if (pFragInst->GetCacheEntry() && !pFragInst->GetCacheEntry()->GetHierInst()->groupBroken->IsClear(nGroup))
					{
						return;
					}
				}
			}

			// check if the engine is underwater
			if (pVehicle->GetIsInWater())
			{
				if (engineBoneIndex>-1)
				{
					float waterDepth;
					CVfxHelper::GetWaterDepth(RCC_VEC3V(engineMat.d), waterDepth);
					if (waterDepth>0.0f)
					{
						// put out any engine fires if the engine is underwater
						if (engineHealth<=ENGINE_DAMAGE_ONFIRE)
						{
							engineHealth = ENGINE_DAMAGE_ONFIRE + 0.1f;
							return;
						}
						// otherwise skip emitting smoke this frame
						else
						{
							return;
						}
					}
				}
			}

			float damageEvo, fireEvo;
			CalcDamageAndFireEvos(pVehicle, engineHealth, damageEvo, fireEvo);

			g_vfxVehicle.UpdatePtFxEnginePanelOpen(pVehicle, pVfxVehicleInfo, engineBoneIndex, speedEvo, /*speedBackwardEvo, */damageEvo, panelOpenRatio, fireEvo);
		}
		else if (doVentFx)
		{
			for (int i=0; i<2; i++)
			{
				// get overheat matrix
				Matrix34 overheatMat;
				s32 overheatBoneIndex = pVehicle->GetBoneIndex((eHierarchyId)(VEH_OVERHEAT+i));
				if (overheatBoneIndex==-1)
				{
					continue;
				}
				
				CVfxHelper::GetMatrixFromBoneIndex(RC_MAT34V(overheatMat), pVehicle, overheatBoneIndex);
				if (overheatMat.a.IsZero())
				{
					continue;
				}

				fragInst* pFragInst = pVehicle->GetFragInst();
				if (pFragInst)
				{
					int component = pFragInst->GetControllingComponentFromBoneIndex(overheatBoneIndex);
					if (component>=0 && component<pFragInst->GetType()->GetPhysics(0)->GetNumChildren())
					{
						s32 nGroup = pFragInst->GetType()->GetPhysics(0)->GetAllChildren()[component]->GetOwnerGroupPointerIndex();
						if (pFragInst->GetCacheEntry() && !pFragInst->GetCacheEntry()->GetHierInst()->groupBroken->IsClear(nGroup))
						{
							continue;
						}
					}
				}

				// override the engine health for planes
				if (pVehicle->GetVehicleType()==VEHICLE_TYPE_PLANE)
				{
					CPlane* pPlane = static_cast<CPlane*>(pVehicle);
					engineHealth = pPlane->GetAircraftDamage().GetSectionHealth((CAircraftDamage::eAircraftSection)(CAircraftDamage::ENGINE_L+i));

					// check the engine health should trigger effects again as this is now a specific (left or right) engine
					// previous checks were done with the main engine health
					if (engineHealth>=GetEngineDamageHealthThresh(pVehicle))
					{
						continue;
					}
				}

				// check if the engine is underwater
				if (pVehicle->GetIsInWater())
				{
					float waterDepth;
					CVfxHelper::GetWaterDepth(RCC_VEC3V(overheatMat.d), waterDepth);
					if (waterDepth>0.0f)
					{
						// put out any engine fires if the engine is underwater
						if (engineHealth<=ENGINE_DAMAGE_ONFIRE)
						{
							engineHealth = ENGINE_DAMAGE_ONFIRE + 0.1f;
							return;
						}
						// otherwise skip emitting smoke this frame
						else
						{
							return;
						}
					}
				}

				// calc evolution variables
				float damageEvo, fireEvo;
				CalcDamageAndFireEvos(pVehicle, engineHealth, damageEvo, fireEvo);

				// play the overheating
				g_vfxVehicle.UpdatePtFxEnginePanelShut(pVehicle, pVfxVehicleInfo, overheatBoneIndex, i, speedForwardEvo, speedBackwardEvo, damageEvo, fireEvo);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  CalcDamageAndFireEvos
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::CalcDamageAndFireEvos		(CVehicle* pVehicle, float engineHealth, float& damageEvo, float& fireEvo)
{
	// calc damage evolution
	damageEvo = 1.0f;
	if (engineHealth>ENGINE_DAMAGE_ONFIRE)
	{
		// fading in
		damageEvo = 1.0f - ((engineHealth - ENGINE_DAMAGE_ONFIRE) / (GetEngineDamageHealthThresh(pVehicle) - ENGINE_DAMAGE_ONFIRE));
	}
	//	else if (engineHealth<ENGINE_DAMAGE_FX_FADE)
	//	{
	//		// fading out
	//		damageEvo = 1.0f - (engineHealth-ENGINE_DAMAGE_FX_FADE)/(ENGINE_DAMAGE_FINSHED-ENGINE_DAMAGE_FX_FADE);
	//	}
	Assert(damageEvo>=0.0f);
	Assert(damageEvo<=1.0f);

	// calc fire evo
	fireEvo = 0.0f;
	if (engineHealth<ENGINE_DAMAGE_ONFIRE)
	{
		// we're on fire - are we fading in or out or on full evo?
		if (engineHealth>ENGINE_DAMAGE_FIRE_FULL)
		{
			// fading in
			fireEvo = engineHealth/ENGINE_DAMAGE_FIRE_FULL;
		}
		else if (engineHealth<ENGINE_DAMAGE_FIRE_FINISH)
		{
			// no fire
			fireEvo = 0.0f;
		}
		else if (engineHealth<ENGINE_DAMAGE_FX_FADE)
		{
			// fading out
			fireEvo = 1.0f - (engineHealth-ENGINE_DAMAGE_FIRE_FINISH)/(ENGINE_DAMAGE_FX_FADE-ENGINE_DAMAGE_FINSHED);
		}
		else
		{
			// full
			fireEvo = 1.0f;
		}
	}
	Assert(fireEvo>=0.0f);
	Assert(fireEvo<=1.0f);

	pVehicle->m_Transmission.SetFireEvo(fireEvo);
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessOverturnedSmoke
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::ProcessOverturnedSmoke	(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo)
{
	// don't process if the vehicle is invisible
	if (!pVehicle->GetIsVisible())
	{
		return;
	}

	// don't process if the overturned smoke effect isn't enabled
	if (pVfxVehicleInfo->GetOverturnedSmokePtFxEnabled()==false)
	{
		return;
	}

	if (pVehicle->GetStatus()==STATUS_WRECKED)
	{
		return;
	}

	float engineHealth = pVehicle->GetVehicleDamage()->GetEngineHealth();
	if (engineHealth>pVfxVehicleInfo->GetOverturnedSmokePtFxEngineHealthThresh())
	{
		return;
	}

	// don't process if the vehicle is moving too quickly
	float vehSpeed = pVehicle->GetVelocity().Mag();
	if (vehSpeed>pVfxVehicleInfo->GetOverturnedSmokePtFxSpeedThresh())
	{
		return;
	}

	// don't process if the vehicle is not overturned
	Mat34V vVehicleMtx = pVehicle->GetMatrix();
	Vec3V vVehicleUp = vVehicleMtx.GetCol2();

	if (vVehicleUp.GetZf()>pVfxVehicleInfo->GetOverturnedSmokePtFxAngleThresh())
	{
		return;
	}

	UpdatePtFxOverturnedSmoke(pVehicle, pVfxVehicleInfo);
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessOilLeak
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::ProcessOilLeak					(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo)
{
	if (pVfxVehicleInfo->GetLeakPtFxEnabled()==false)
	{
		return;
	}

	if (NetworkInterface::IsGameInProgress())
	{
		// no vehicle oil leaks in a network game
		return;
	}

	if (pVehicle && pVehicle->IsLeakingOil())
	{
	    // only leak oil under a certain speed
	    float vehSpeed = pVehicle->GetVelocityIncludingReferenceFrame().Mag();
	    if (vehSpeed<1.0f)
	    {
		    // calc oil damage (0.0 at ENGINE_DAMAGE_OIL_LEAKING, 1.0 at ENGINE_DAMAGE_ONFIRE)
			float oilDamage = 1.0f;
			float engineHealth = pVehicle->GetVehicleDamage()->GetEngineHealth();
		    if (engineHealth>ENGINE_DAMAGE_ONFIRE)
		    {
			    oilDamage = CVfxHelper::GetInterpValue(engineHealth, ENGINE_DAMAGE_OIL_LEAKING, ENGINE_DAMAGE_ONFIRE, true);
		    }

		    UpdatePtFxEngineOilLeak(pVehicle, pVfxVehicleInfo, oilDamage);
	    }
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessPetrolLeak
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::ProcessPetrolLeak				(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo)
{
	if (pVfxVehicleInfo->GetLeakPtFxEnabled()==false)
	{
		return;
	}

	if (pVehicle && pVehicle->IsLeakingPetrol())
	{
	    // only leak petrol under a certain speed
//		float vehSpeed = pVehicle->GetVelocity().Mag();
//		if (vehSpeed<25.0f)
	    {
		    // calc petrol tank damage (0.0 at VEH_DAMAGE_HEALTH_PTANK_LEAKING, 1.0 at VEH_DAMAGE_HEALTH_PTANK_ONFIRE)
			float tankDamage = 1.0f;
			float petrolTankHealth = pVehicle->GetVehicleDamage()->GetPetrolTankHealth();
		    if (petrolTankHealth>VEH_DAMAGE_HEALTH_PTANK_ONFIRE)
		    {
			    tankDamage = CVfxHelper::GetInterpValue(petrolTankHealth, VEH_DAMAGE_HEALTH_PTANK_LEAKING, VEH_DAMAGE_HEALTH_PTANK_ONFIRE, true);
		    }

			if(pVehicle->SprayPetrolBeforeExplosion())
			{
				UpdatePtFxPetrolSpray(pVehicle, pVfxVehicleInfo, tankDamage);
			}
			else
			{
				UpdatePtFxEnginePetrolLeak(pVehicle, pVfxVehicleInfo, tankDamage);
			}

			// try to ignite petrol trails shortly after a crash
			if (NetworkInterface::IsGameInProgress()==false)
			{
				if (g_decalMan.GetDisablePetrolDecalsIgniting()==false && g_DrawRand.GetFloat()<=VFXVEHICLE_PETROL_IGNITE_PROBABILITY)
				{
					float vehSpeed = pVehicle->GetVelocity().Mag();
					if (vehSpeed<=VFXVEHICLE_PETROL_IGNITE_SPEED_THRESH)
					{
						if (HasRecentlyCrashed(pVehicle, VFXVEHICLE_PETROL_IGNITE_CRASH_TIMER, VFXVEHICLE_PETROL_IGNITE_DAMAGE_THRESH))
						{
							u32 foundDecalTypeFlag = 0;
							Vec3V vFarthestPos;
							fwEntity* pFarthestEntity = NULL;
							if (g_decalMan.FindFarthest((1<<DECALTYPE_POOL_LIQUID | 1<<DECALTYPE_TRAIL_LIQUID), VFXLIQUID_TYPE_PETROL, pVehicle->GetTransform().GetPosition(), VFXVEHICLE_PETROL_IGNITE_RANGE, 0.0f, false, true, 2.5f, vFarthestPos, &pFarthestEntity, foundDecalTypeFlag))
							{
								CEntity* pFarthestEntityActual = NULL;
								if (pFarthestEntity)
								{
									pFarthestEntityActual = static_cast<CEntity*>(pFarthestEntity);
								}

								g_fireMan.StartPetrolFire(pFarthestEntityActual, vFarthestPos, Vec3V(V_Z_AXIS_WZERO), NULL, foundDecalTypeFlag==(1<<DECALTYPE_POOL_LIQUID), vFarthestPos, BANK_ONLY(vFarthestPos,) false);
							}
						}
					}
				}
			}
	    }
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessPetrolTankDamage
///////////////////////////////////////////////////////////////////////////////

void 			CVfxVehicle::ProcessPetrolTankDamage			(CVehicle* pVehicle, CPed* pCullprit)
{
	float petrolTankHealth = pVehicle->GetVehicleDamage()->GetPetrolTankHealth();
	if (petrolTankHealth>VEH_DAMAGE_HEALTH_PTANK_FINISHED && petrolTankHealth<VEH_DAMAGE_HEALTH_PTANK_ONFIRE && pVehicle->GetStatus()!=STATUS_WRECKED)
	{
		CVehicleModelInfo* pModelInfo = pVehicle->GetVehicleModelInfo();
		bool isElectric = pModelInfo && pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_IS_ELECTRIC);

		// Only burn petrol if there's some left in the tank and its not electric
        if(pVehicle->GetVehicleDamage()->GetPetrolTankLevel() > 0.0f && !isElectric)
        {
		    // do petrol tank on fire ptfx
		    Vec3V vPetrolTankWldPosL, vPetrolTankWldPosR;
		    if (pVehicle->GetVehicleDamage()->GetPetrolTankPosWld(&RC_VECTOR3(vPetrolTankWldPosL), &RC_VECTOR3(vPetrolTankWldPosR)))
		    {
			    // check if the fuel tank is underwater
			    bool isUnderwater = false;
			    if (pVehicle->GetIsInWater())
			    {
					float waterDepth;
					CVfxHelper::GetWaterDepth(vPetrolTankWldPosL, waterDepth);
					if (waterDepth>0.0f)
				    {
					    pVehicle->GetVehicleDamage()->SetPetrolTankHealth(VEH_DAMAGE_HEALTH_PTANK_ONFIRE+0.1f);
					    isUnderwater = true;
				    }
			    }

			    if (!isUnderwater)
			    {
				    // calc the petrol tank positions local to the vehicle matrix (note: not using the bone matrix here)
					if(!IsZeroAll(vPetrolTankWldPosL))
					{
						Vec3V vPetrolTankLclPosL = CVfxHelper::GetLocalEntityPos(*pVehicle, vPetrolTankWldPosL);
					    g_fireMan.RegisterVehicleTankFire(pVehicle, -1, vPetrolTankLclPosL, pVehicle, PTFXOFFSET_VEHICLE_FIRE_PETROL_TANK+0, 1.0f, pCullprit);
					}
						
					if(!IsZeroAll(vPetrolTankWldPosR))
					{
						Vec3V vPetrolTankLclPosR = CVfxHelper::GetLocalEntityPos(*pVehicle, vPetrolTankWldPosR);
						g_fireMan.RegisterVehicleTankFire(pVehicle, -1, vPetrolTankLclPosR, pVehicle, PTFXOFFSET_VEHICLE_FIRE_PETROL_TANK+1, 1.0f, pCullprit);
					}
				}
		    }
        }
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxExhaust
///////////////////////////////////////////////////////////////////////////////
		
void			CVfxVehicle::UpdatePtFxExhaust					(CVehicle* pVehicle)
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

	// return if exhaust ptfx aren't enabled
	if (pVfxVehicleInfo->GetExhaustPtFxEnabled()==false)
	{
		return;
	}

	// return if the vehicle is underwater is sub mode
	if (pVehicle->IsInSubmarineMode())
	{
		return;
	}

	// check if we're in range
	Vec3V vVehiclePos = pVehicle->GetTransform().GetPosition();
	if (CVfxHelper::GetDistSqrToCamera(vVehiclePos)>pVfxVehicleInfo->GetExhaustPtFxRangeSqr())
	{
		return;
	}

	// check if we're above the cut off speed
	float vehicleForwardSpeed;
	vehicleForwardSpeed = DotProduct(pVehicle->GetVelocityIncludingReferenceFrame(), VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()));
	if (vehicleForwardSpeed>pVfxVehicleInfo->GetExhaustPtFxCutOffSpeed())
	{
		return;
	}

	// check that we have a valid exhaust matrix
	for (s32 exhaustId=VEH_FIRST_EXHAUST; exhaustId<=VEH_LAST_EXHAUST; exhaustId++)
	{
		Mat34V boneMtx;
		s32 exhaustBoneId;
		pVehicle->GetExhaustMatrix((eHierarchyId)exhaustId, boneMtx, exhaustBoneId);

		if (IsZeroAll(boneMtx.GetCol0()))
		{
			ptfxRegInst* pRegFx = ptfxReg::Find(pVehicle, PTFXOFFSET_VEHICLE_EXHAUST+(exhaustId-VEH_FIRST_EXHAUST));
			if (pRegFx && pRegFx->m_pFxInst)
			{
				pRegFx->m_pFxInst->Finish(false);
			}

			continue;
		}

		if ((MI_CAR_STROMBERG.IsValid() && pVehicle->GetModelIndex()==MI_CAR_STROMBERG) || (MI_CAR_TOREADOR.IsValid() && pVehicle->GetModelIndex()==MI_CAR_TOREADOR))
		{
			float waterDepth;
			CVfxHelper::GetWaterDepth(boneMtx.GetCol3(), waterDepth);
			if (waterDepth > 0.0f)
			{
				continue;
			}
		}

		// register the fx system
		bool justCreated;
		ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_EXHAUST+(exhaustId-VEH_FIRST_EXHAUST), true, pVfxVehicleInfo->GetExhaustPtFxName(), justCreated);

		// check the effect exists
		if (pFxInst)
		{
			// speed evo
			float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocityIncludingReferenceFrame().Mag(), pVfxVehicleInfo->GetExhaustPtFxSpeedEvoMin(), pVfxVehicleInfo->GetExhaustPtFxSpeedEvoMax());
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

			// damage evo
			float damageEvo = 1.0f - CVfxHelper::GetInterpValue(pVehicle->GetVehicleDamage()->GetEngineHealth(), 0.0f, ENGINE_HEALTH_MAX);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("damage", 0x431375E1), damageEvo);

			// temp evo 
			float tempEvo = CVfxHelper::GetInterpValue(g_weather.GetTemperature(vVehiclePos), pVfxVehicleInfo->GetExhaustPtFxTempEvoMin(), pVfxVehicleInfo->GetExhaustPtFxTempEvoMax());
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("temp", 0x6CE0A51A), tempEvo);

			// throttle evo
			float throttleEvo = 0.0f;
			if (pVfxVehicleInfo->GetExhaustPtFxThrottleEvoOnGearChange())
			{
				if (Abs(pVehicle->GetThrottle())>0.5f && pVehicle->m_Transmission.GetClutchRatio()>0.5f && pVehicle->m_Transmission.GetClutchRatio()<1.0f)
				{
					throttleEvo = 1.0f;
				}
			}
			else
			{
				throttleEvo = CVfxHelper::GetInterpValue(Abs(pVehicle->GetThrottle()), 0.0f, 1.0f);

				if (pVehicle->GetVehicleType()==VEHICLE_TYPE_PLANE)
				{
					CPlane* pPlane = static_cast<CPlane*>(pVehicle);
					if (pPlane->IsInVerticalFlightMode())
					{
						throttleEvo = CVfxHelper::GetInterpValue(Abs(pPlane->GetThrottleControl()), 0.0f, 1.0f);
					}
				}
			}
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("throttle", 0xEA0151DC), throttleEvo);

			ptfxAttach::Attach(pFxInst, pVehicle, exhaustBoneId);

			// check if the effect has just been created 
			if (justCreated)
			{
				// invert the axes depending on which side of the vehicle this exhaust is on
				Vec3V vPosLcl;
				if (CVfxHelper::GetLocalEntityBonePos(*pVehicle, -1, boneMtx.GetCol3(), vPosLcl))
				{
					if (vPosLcl.GetXf()>0.0f)
					{
						pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
					}
				}

				pFxInst->SetUserZoom(pVfxVehicleInfo->GetExhaustPtFxScale());

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
} 


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxNitrous
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxNitrous					(CVehicle* pVehicle)
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

	// return if exhaust ptfx aren't enabled
	if (pVfxVehicleInfo->GetExhaustPtFxEnabled()==false)
	{
		return;
	}

	// check if we're in range
	Vec3V vVehiclePos = pVehicle->GetTransform().GetPosition();
	if (CVfxHelper::GetDistSqrToCamera(vVehiclePos)>VFXRANGE_VEH_NITROUS_SQR)
	{
		return;
	}

	if (NetworkInterface::IsInCopsAndCrooks())
	{
	 	// check if we're below the cut off speed
		float vehicleForwardSpeed;
		vehicleForwardSpeed = DotProduct(pVehicle->GetVelocityIncludingReferenceFrame(), VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()));
		dev_float fNitrousExhaustFlameMinimumSpeed = 10.f;
		if (vehicleForwardSpeed < fNitrousExhaustFlameMinimumSpeed)
		{
	 		return;
		}
	}

	// check that we have a valid exhaust matrix
	for (s32 exhaustId=VEH_FIRST_EXHAUST; exhaustId<=VEH_LAST_EXHAUST; exhaustId++)
	{
		Mat34V boneMtx;
		s32 exhaustBoneId;
		pVehicle->GetExhaustMatrix((eHierarchyId)exhaustId, boneMtx, exhaustBoneId);

		if (IsZeroAll(boneMtx.GetCol0()))
		{
			ptfxRegInst* pRegFx = ptfxReg::Find(pVehicle, PTFXOFFSET_VEHICLE_EXHAUST+(exhaustId-VEH_FIRST_EXHAUST));
			if (pRegFx && pRegFx->m_pFxInst)
			{
				pRegFx->m_pFxInst->Finish(false);
			}

			continue;
		}

		// register the fx system
		atHashWithStringNotFinal ptfxName = atHashWithStringNotFinal("veh_nitrous");
		if (NetworkInterface::IsInCopsAndCrooks())
		{
			CPed* pDriver = pVehicle->GetDriver();
			if (pDriver && pDriver->IsLocalPlayer())
			{
				CPlayerInfo* pDriverInfo = pDriver->GetPlayerInfo();
				if (pDriverInfo->AccessArcadeInformation().GetTeam() == eArcadeTeam::AT_CNC_COP)
				{
					ptfxName = atHashWithStringNotFinal("scr_arc_cc_veh_nitro_cop");
				}
				else if (pDriverInfo->AccessArcadeInformation().GetTeam() == eArcadeTeam::AT_CNC_CROOK)
				{
					ptfxName = atHashWithStringNotFinal("scr_arc_cc_veh_nitro_crook");
				}
			}	
		}
		bool justCreated;
		ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_EXHAUST+(exhaustId-VEH_FIRST_EXHAUST), true, ptfxName, justCreated);

		// check the effect exists
		if (pFxInst)
		{
			// speed evo
//			float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocityIncludingReferenceFrame().Mag(), pVfxVehicleInfo->GetExhaustPtFxSpeedEvoMin(), pVfxVehicleInfo->GetExhaustPtFxSpeedEvoMax());
//			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

			// damage evo
//			float damageEvo = 1.0f - CVfxHelper::GetInterpValue(pVehicle->GetVehicleDamage()->GetEngineHealth(), 0.0f, ENGINE_HEALTH_MAX);
//			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("damage", 0x431375E1), damageEvo);

			// temp evo 
//			float tempEvo = CVfxHelper::GetInterpValue(g_weather.GetTemperature(vVehiclePos), pVfxVehicleInfo->GetExhaustPtFxTempEvoMin(), pVfxVehicleInfo->GetExhaustPtFxTempEvoMax());
//			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("temp", 0x6CE0A51A), tempEvo);

			// throttle evo
// 			float throttleEvo = 0.0f;
// 			if (pVfxVehicleInfo->GetExhaustPtFxThrottleEvoOnGearChange())
// 			{
// 				if (Abs(pVehicle->GetThrottle())>0.5f && pVehicle->m_Transmission.GetClutchRatio()>0.5f && pVehicle->m_Transmission.GetClutchRatio()<1.0f)
// 				{
// 					throttleEvo = 1.0f;
// 				}
// 			}
// 			else
// 			{
// 				throttleEvo = CVfxHelper::GetInterpValue(Abs(pVehicle->GetThrottle()), 0.0f, 1.0f);
// 			}
// 			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("throttle", 0xEA0151DC), throttleEvo);

			ptfxAttach::Attach(pFxInst, pVehicle, exhaustBoneId);

			// check if the effect has just been created 
			if (justCreated)
			{
				// invert the axes depending on which side of the vehicle this exhaust is on
				Vec3V vPosLcl;
				if (CVfxHelper::GetLocalEntityBonePos(*pVehicle, -1, boneMtx.GetCol3(), vPosLcl))
				{
					if (vPosLcl.GetXf()>0.0f)
					{
						pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
					}
				}

				pFxInst->SetUserZoom(pVfxVehicleInfo->GetExhaustPtFxScale());

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
} 


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxEngineStartup
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxEngineStartup		(CVehicle* pVehicle)
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

	// return if engine startup ptfx aren't enabled
	if (pVfxVehicleInfo->GetEngineStartupPtFxEnabled()==false)
	{
		return;
	}

	// check if we're in range
	Vec3V vVehiclePos = pVehicle->GetTransform().GetPosition();
	if (CVfxHelper::GetDistSqrToCamera(vVehiclePos)>pVfxVehicleInfo->GetEngineStartupPtFxRangeSqr())
	{
		return;
	}

	// check that we have a valid exhaust matrix
	for (s32 exhaustId=VEH_FIRST_EXHAUST; exhaustId<=VEH_LAST_EXHAUST; exhaustId++)
	{
		Mat34V boneMtx;
		s32 exhaustBoneId;
		pVehicle->GetExhaustMatrix((eHierarchyId)exhaustId, boneMtx, exhaustBoneId);

		if (IsZeroAll(boneMtx.GetCol0()))
		{
			ptfxRegInst* pRegFx = ptfxReg::Find(pVehicle, PTFXOFFSET_VEHICLE_ENGINE_STARTUP+(exhaustId-VEH_FIRST_EXHAUST));
			if (pRegFx && pRegFx->m_pFxInst)
			{
				pRegFx->m_pFxInst->Finish(false);
			}

			continue;
		}

		// register the fx system
		bool justCreated;
		ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_ENGINE_STARTUP+(exhaustId-VEH_FIRST_EXHAUST), true, pVfxVehicleInfo->GetEngineStartupPtFxName(), justCreated);

		// check the effect exists
		if (pFxInst)
		{
			ptfxAttach::Attach(pFxInst, pVehicle, exhaustBoneId);

			// check if the effect has just been created 
			if (justCreated)
			{
				// invert the axes depending on which side of the vehicle this exhaust is on
				Vec3V vPosLcl;
				if (CVfxHelper::GetLocalEntityBonePos(*pVehicle, -1, boneMtx.GetCol3(), vPosLcl))
				{
					if (vPosLcl.GetXf()>0.0f)
					{
						pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
					}
				}

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
} 


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxMisfire
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxMisfire					(CVehicle* pVehicle)
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

	// return if misfire ptfx aren't enabled
	if (pVfxVehicleInfo->GetMisfirePtFxEnabled()==false)
	{
		return;
	}


	// check if we're in range
	Vec3V vVehiclePos = pVehicle->GetTransform().GetPosition();
	if (CVfxHelper::GetDistSqrToCamera(vVehiclePos)>pVfxVehicleInfo->GetMisfirePtFxRangeSqr())
	{
		return;
	}

	// we try to trigger the misfire effects on the afterburner bones if the vehicle has any
	bool effectAttachedToAfterburner = false;
	CVehicleModelInfo* pModelInfo = pVehicle->GetVehicleModelInfo();
	for (s32 i = 0; i < PLANE_NUM_AFTERBURNERS; ++i)
	{
		// get the matrix of the bone to attach to
		Mat34V boneMtx;
		s32 afterburnerBoneId = pModelInfo->GetBoneIndex(PLANE_AFTERBURNER + i);
		if (afterburnerBoneId > -1)
		{
			CVfxHelper::GetMatrixFromBoneIndex(boneMtx, pVehicle, afterburnerBoneId);
		}
		else
		{
			continue;
		}

		// calculate the offset to use for registration (we make sure that the number of possible bones fits in the number of misfires we can register)
		CompileTimeAssert(PLANE_NUM_AFTERBURNERS <= (PTFXOFFSET_VEHICLE_MISFIRE_LAST - PTFXOFFSET_VEHICLE_MISFIRE + 1));
		int classFxOffset = PTFXOFFSET_VEHICLE_MISFIRE + i;

		// check that we have a valid matrix
		if (IsZeroAll(boneMtx.GetCol0()))
		{
			ptfxRegInst* pRegFx = ptfxReg::Find(pVehicle, classFxOffset);
			if (pRegFx && pRegFx->m_pFxInst)
			{
				pRegFx->m_pFxInst->Finish(false);
			}
			continue;
		}

		// register the fx system
		bool justCreated;
		ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, classFxOffset, true, pVfxVehicleInfo->GetMisfirePtFxName(), justCreated);

		// check the effect exists
		if (pFxInst)
		{
			ptfxAttach::Attach(pFxInst, pVehicle, afterburnerBoneId);

			// check if the effect has just been created 
			if (justCreated)
			{
				// invert the axes depending on which side of the vehicle this exhaust is on
				Vec3V vPosLcl;
				if (CVfxHelper::GetLocalEntityBonePos(*pVehicle, -1, boneMtx.GetCol3(), vPosLcl))
				{
					if (vPosLcl.GetXf() > 0.0f)
					{
						pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
					}
				}

				// it has just been created - start it
				pFxInst->Start();
			}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
			else
			{
				// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
				vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "Effect attachment error");
			}
#endif
			effectAttachedToAfterburner = true;
		}
	}


	// if we didn't have any afterburners to fire an effect on we trigger the misfire effect
	// on the normal exhausts
	if (!effectAttachedToAfterburner)
	{
		for (s32 exhaustId = VEH_FIRST_EXHAUST; exhaustId <= VEH_LAST_EXHAUST; exhaustId++)
		{
			// get the matrix of the bone to attach to
			Mat34V boneMtx;
			s32 exhaustBoneId;
			pVehicle->GetExhaustMatrix((eHierarchyId)exhaustId, boneMtx, exhaustBoneId);

			// calculate the offset to use for registration (we make sure that the number of possible bones fits in the number of misfires we can register)
			CompileTimeAssert((VEH_LAST_EXHAUST - VEH_FIRST_EXHAUST + 1) <= (PTFXOFFSET_VEHICLE_MISFIRE_LAST - PTFXOFFSET_VEHICLE_MISFIRE + 1));
			int classFxOffset = PTFXOFFSET_VEHICLE_MISFIRE + (exhaustId - VEH_FIRST_EXHAUST);

			// check that we have a valid matrix
			if (IsZeroAll(boneMtx.GetCol0()))
			{
				ptfxRegInst* pRegFx = ptfxReg::Find(pVehicle, classFxOffset);
				if (pRegFx && pRegFx->m_pFxInst)
				{
					pRegFx->m_pFxInst->Finish(false);
				}
				continue;
			}

			// register the fx system
			bool justCreated;
			ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, classFxOffset, true, pVfxVehicleInfo->GetMisfirePtFxName(), justCreated);

			// check the effect exists
			if (pFxInst)
			{
				ptfxAttach::Attach(pFxInst, pVehicle, exhaustBoneId);

				// check if the effect has just been created 
				if (justCreated)
				{
					// invert the axes depending on which side of the vehicle this exhaust is on
					Vec3V vPosLcl;
					if (CVfxHelper::GetLocalEntityBonePos(*pVehicle, -1, boneMtx.GetCol3(), vPosLcl))
					{
						if (vPosLcl.GetXf() > 0.0f)
						{
							pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
						}
					}

					// it has just been created - start it
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
} 


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxRocketBoost
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxRocketBoost					(CVehicle* pVehicle)
{
	// don't update effect if the vehicle is off screen
	if (!pVehicle->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	// check if we're in range
	Vec3V vVehiclePos = pVehicle->GetTransform().GetPosition();
	if (CVfxHelper::GetDistSqrToCamera(vVehiclePos)>200.0f*200.0f)
	{
		return;
	}

	const bool isToreador = pVehicle->GetModelIndex()==MI_CAR_TOREADOR;
	if (isToreador && pVehicle->IsInSubmarineMode())
	{
		return;
	}

	const bool allowRocketBoostOnAllExhausts = isToreador;
	for (int i=0; i<VFXVEHICLE_NUM_ROCKET_BOOSTS; i++)
	{
 		// get the rocket boost bone
		Mat34V boneMtx;
 		CVehicleModelInfo* pModelInfo = pVehicle->GetVehicleModelInfo();
 		s32 rocketBoostBoneId = pModelInfo->GetBoneIndex(VEH_ROCKET_BOOST+i);
		if (rocketBoostBoneId == -1 && (i==0 || allowRocketBoostOnAllExhausts))
		{
			CompileTimeAssert(VFXVEHICLE_NUM_ROCKET_BOOSTS <= (VEH_LAST_EXHAUST - VEH_FIRST_EXHAUST + 1));
			rocketBoostBoneId = pModelInfo->GetBoneIndex(VEH_EXHAUST+i);
		}

		if( rocketBoostBoneId == -1 &&
			pVehicle->GetModelIndex() == MI_PLANE_BOMBUSHKA )
		{
			rocketBoostBoneId = pModelInfo->GetBoneIndex(VEH_ROCKET_BOOST);
			rocketBoostBoneId = rocketBoostBoneId | ((i) << 28);
		}

		if (rocketBoostBoneId>-1)
		{
			CVfxHelper::GetMatrixFromBoneIndex(boneMtx, pVehicle, rocketBoostBoneId);

			if (!IsZeroAll(boneMtx.GetCol0()))
			{
				if (isToreador)
				{
					float waterDepth;
					CVfxHelper::GetWaterDepth(boneMtx.GetCol3(), waterDepth);
					if (waterDepth > 0.0f)
					{
						continue;
					}
				}

				// register the fx system
				bool justCreated;
				ptxEffectInst* pFxInst = nullptr;
				
				if (pVehicle->GetModelIndex() == MI_CAR_VIGILANTE)
				{
					pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_ROCKET_BOOST+i, true, ATSTRINGHASH("veh_rocket_boost_vig", 0x610AC773), justCreated);
				}
				else if (pVehicle->GetModelIndex() == MI_CAR_DELUXO)
				{
					pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_ROCKET_BOOST+i, true, ATSTRINGHASH("veh_rocket_boost_deluxo", 0xE0E3C37E), justCreated);
				}
				else
				{
					pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_ROCKET_BOOST+i, true, ATSTRINGHASH("veh_rocket_boost", 0xd6a3641f), justCreated);
				}

				// check the effect exists
				if (pFxInst)
				{
					ptfxAttach::Attach(pFxInst, pVehicle, rocketBoostBoneId);

					float boostEvo = pVehicle->IsRocketBoosting() ? 1.0f : 0.0f;
					pFxInst->SetEvoValueFromHash(ATSTRINGHASH("boost", 0xcf237743), boostEvo);

					float chargeEvo = pVehicle->GetRocketBoostRemaining()/pVehicle->GetRocketBoostCapacity();
					chargeEvo = chargeEvo*chargeEvo*chargeEvo;
					pFxInst->SetEvoValueFromHash(ATSTRINGHASH("charge", 0x355c4f3a), chargeEvo);

					// check if the effect has just been created 
					if (justCreated)
					{
						// it has just been created - start it and set it's position
						pFxInst->Start();

						if (pVehicle->GetModelIndex()==MI_BIKE_OPPRESSOR2)
						{
							pFxInst->SetUserZoom(0.3f);
						}
						else if (pVehicle->GetVehicleType()==VEHICLE_TYPE_BIKE)
						{
							pFxInst->SetUserZoom(0.5f);
						}
						else if (pVehicle->GetModelIndex()==MI_PLANE_TULA || pVehicle->GetModelIndex()==MI_PLANE_MOGUL)
						{
							pFxInst->SetUserZoom(0.21f);
						}
						else if (pVehicle->GetModelIndex()==MI_PLANE_BOMBUSHKA)
						{
							pFxInst->SetUserZoom(0.6f);
						}
						else if (pVehicle->GetModelIndex()==MI_JETPACK_THRUSTER)
						{
							pFxInst->SetUserZoom(0.18f);
						}
						else if (pVehicle->GetModelIndex()==MI_CAR_SCRAMJET)
						{
							pFxInst->SetUserZoom(0.45f);
						}
						else if (isToreador)
						{
							pFxInst->SetUserZoom(0.2f);
							pFxInst->SetOffsetPos(Vec3V(0.0f,0.05f,0.0f));
						}

						// remove any fire data capsules from planes - we don't want them damaging the plane itself
						if (pVehicle->GetVehicleType()==VEHICLE_TYPE_PLANE)
						{
							pFxInst->SetIgnoreDataCapsuleTests(true);
						}
					}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
					else
					{
						// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
						vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "Effect attachment error");
					}
#endif

					// hack - check for petrol decals to ignite
					if (pVehicle->IsRocketBoosting())
					{
						Vec3V vFlamePos = boneMtx.GetCol3();
						float flameRadius = 2.0f;

						u32 foundDecalTypeFlag = 0;
						Vec3V vClosestPos;
						fwEntity* pClosestEntity = NULL;
						if (g_decalMan.GetDisablePetrolDecalsIgniting()==false && g_decalMan.FindClosest((1<<DECALTYPE_POOL_LIQUID | 1<<DECALTYPE_TRAIL_LIQUID), VFXLIQUID_TYPE_PETROL, vFlamePos, flameRadius, 0.0f, false, true, 2.5f, vClosestPos, &pClosestEntity, foundDecalTypeFlag))
						{
							CEntity* pClosestEntityActual = NULL;
							if (pClosestEntity)
							{
								pClosestEntityActual = static_cast<CEntity*>(pClosestEntity);
							}

							g_fireMan.StartPetrolFire(pClosestEntityActual, vClosestPos, Vec3V(V_Z_AXIS_WZERO), NULL, foundDecalTypeFlag==(1<<DECALTYPE_POOL_LIQUID), vFlamePos, BANK_ONLY(vFlamePos,) false);
						}
					}
				}
			}
		}
	}
} 


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxBackfire
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::TriggerPtFxBackfire				(CVehicle* pVehicle, float bangerEvo)
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

	// return if backfire vfx aren't enabled
	if (pVfxVehicleInfo->GetBackfirePtFxEnabled()==false)
	{
		return;
	}

	// check if we're in range
	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>pVfxVehicleInfo->GetBackfirePtFxRangeSqr())
	{
		return;
	}

	for (s32 exhaustId=VEH_FIRST_EXHAUST; exhaustId<=VEH_LAST_EXHAUST; exhaustId++)
	{
		Mat34V boneMtx;
		s32 exhaustBoneId;
		pVehicle->GetExhaustMatrix((eHierarchyId)exhaustId, boneMtx, exhaustBoneId);

		if (IsZeroAll(boneMtx.GetCol0()))
		{
			continue;
		}

		Vec3V vBackfirePos = boneMtx.GetCol3();

		ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pVfxVehicleInfo->GetBackfirePtFxName());
		if (pFxInst)
		{
			ptfxAttach::Attach(pFxInst, pVehicle, exhaustBoneId);

			float damageEvo = 1.0f - CVfxHelper::GetInterpValue(pVehicle->GetVehicleDamage()->GetEngineHealth(), 0.0f, ENGINE_HEALTH_MAX);

			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("damage", 0x431375E1), damageEvo);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("banger", 0x895EC1F8), bangerEvo);

//			pFxInst->SetOverrideDistSqrToCamera(fxDistSqr+FXVEHICLE_RENDER_ORDER_OFFSET_BACKFIRE);

			// invert the axes depending on which side of the vehicle this exhaust is on
			Vec3V vPosLcl;
			if (CVfxHelper::GetLocalEntityBonePos(*pVehicle, -1, boneMtx.GetCol3(), vPosLcl))
			{
				if (vPosLcl.GetXf()>0.0f)
				{
					pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
				}
			}

			// start the fx
			pFxInst->Start();

#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				CReplayMgr::RecordFx<CPacketVehBackFireFx>(
					CPacketVehBackFireFx(pVfxVehicleInfo->GetBackfirePtFxName(), 
						damageEvo,
						bangerEvo,
						exhaustId-VEH_FIRST_EXHAUST),
					pVehicle);
			}
#endif // GTA_REPLAY

			// test for any petrol trails to ignite
			CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
			bool isPlayerCar = pPlayerPed && pPlayerPed->GetMyVehicle()==pVehicle;
			if (isPlayerCar)
			{
				u32 foundDecalTypeFlag = 0;
				Vec3V vClosestPos;
				fwEntity* pClosestEntity = NULL;
				if (g_decalMan.GetDisablePetrolDecalsIgniting()==false && g_decalMan.FindClosest((1<<DECALTYPE_POOL_LIQUID | 1<<DECALTYPE_TRAIL_LIQUID), VFXLIQUID_TYPE_PETROL, vBackfirePos, 1.5f, 0.0f, false, true, 2.5f, vClosestPos, &pClosestEntity, foundDecalTypeFlag))
				{
					CEntity* pClosestEntityActual = NULL;
					if (pClosestEntity)
					{
						pClosestEntityActual = static_cast<CEntity*>(pClosestEntity);
					}

					g_fireMan.StartPetrolFire(pClosestEntityActual, vClosestPos, Vec3V(V_Z_AXIS_WZERO), NULL, foundDecalTypeFlag==(1<<DECALTYPE_POOL_LIQUID), vBackfirePos, BANK_ONLY(vBackfirePos,) false);
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxSlipstream
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxSlipstream				(CVehicle* pVehicle, float slipstreamEvo)
{
	float distSqrThresh = VFXRANGE_VEH_SLIPSTREAM_SQR;
	distSqrThresh *= m_slipstreamPtFxLodRangeScale*m_slipstreamPtFxLodRangeScale;

	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>distSqrThresh)
	{
		return;
	}

	// check if the effect exists - it hasn't been authored yet
	// once it has been this code can be removed
	atHashWithStringNotFinal ptFxName("veh_slipstream",0x7C1BAEA3);
	if (RMPTFXMGR.GetEffectRule(ptFxName)==false)
	{
		return;
	}

	for (int i=0; i<2; i++)
	{
		s32	boneIdx = pVehicle->GetBoneIndex((eHierarchyId)(VEH_SLIPSTREAM_L+i));
		if (boneIdx==-1)
		{
			boneIdx = pVehicle->GetBoneIndex((eHierarchyId)(VEH_TAILLIGHT_L+i));
		}

		//url:bugstar:3790609 - we forgot to add slipstream or taillight bones to oppressor.
		if (boneIdx==-1 && MI_BIKE_OPPRESSOR.IsValid() && pVehicle->GetModelIndex() == MI_BIKE_OPPRESSOR && i==0)
		{
			boneIdx = pVehicle->GetBoneIndex((eHierarchyId)(VEH_ROCKET_BOOST));
		}

		if( boneIdx == -1 && MI_PLANE_VOLATOL.IsValid() && pVehicle->GetModelIndex() == MI_PLANE_VOLATOL )
		{
			boneIdx = pVehicle->GetBoneIndex( (eHierarchyId)( VEH_SIREN_1 + i ) );
		}


		if (boneIdx>-1)
		{
			// register the fx system
			bool justCreated;
			ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_SLIPSTREAM+i, true, ptFxName, justCreated);

			// check the effect exists
			if (pFxInst)
			{
				ptfxAttach::Attach(pFxInst, pVehicle, boneIdx);

				pFxInst->SetEvoValueFromHash(ATSTRINGHASH("slipstream", 0x8AC10C7A), slipstreamEvo);

				pFxInst->SetLODScalar(m_slipstreamPtFxLodRangeScale);

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
					CReplayMgr::RecordFx<CPacketVehicleSlipstreamFx>(CPacketVehicleSlipstreamFx(i, ptFxName, slipstreamEvo), pVehicle);
				}
#endif // GTA_REPLAY

			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxExtra
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxExtra					(CVehicle* pVehicle, const CVfxExtraInfo* pVfxExtraInfo, u32 id)
{
	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>pVfxExtraInfo->GetPtFxRangeSqr())
	{
		return;
	}

	s32 extraBoneIndex = pVehicle->GetBoneIndex((eHierarchyId)(VEH_EXTRA_1+id));
	if (extraBoneIndex<=-1)
	{
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_EXTRA+id, true, pVfxExtraInfo->GetPtFxName(), justCreated);

	// check the effect exists
	if (pFxInst)
	{
		ptfxAttach::Attach(pFxInst, pVehicle, extraBoneIndex);

		Mat34V vFxOffsetMtx;
		vFxOffsetMtx.SetIdentity3x3();
		vFxOffsetMtx.SetCol3(pVfxExtraInfo->GetPtFxOffset());
		pFxInst->SetOffsetMtx(vFxOffsetMtx);

		// set evolutions
		float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocity().Mag(), pVfxExtraInfo->GetPtFxSpeedEvoMin(), pVfxExtraInfo->GetPtFxSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

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


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxThrusterJet
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxThrusterJet				(CVehicle* pVehicle, s32 boneIndex, s32 fxId, float thrustEvo)
{
	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>VFXRANGE_VEH_THRUSTER_JET_SQR)
	{
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_THRUSTER+fxId, true, ATSTRINGHASH("veh_thruster_jet", 0x89E462AC), justCreated);

	// check the effect exists
	if (pFxInst)
	{
		ptfxAttach::Attach(pFxInst, pVehicle, boneIndex);;

		// set evolutions
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("thrust", 0x6D6F8F43), thrustEvo);

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


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxBikeGlideHaze
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxGlideHaze				(CVehicle* pVehicle, atHashWithStringNotFinal ptFxHashName)
{
	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>VFXRANGE_VEH_GLIDE_HAZE_SQR)
	{
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_GLIDE_HAZE, true, ptFxHashName, justCreated);

	// check the effect exists
	if (pFxInst)
	{
		ptfxAttach::Attach(pFxInst, pVehicle, -1);

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


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxElectricRamBar
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxElectricRamBar				(CVehicle* pVehicle)
{
	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>VFXRANGE_VEH_ELECTRIC_RAM_BAR_SQR)
	{
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_ELECTRIC_RAM_BAR, true, ATSTRINGHASH("veh_xs_electrified_rambar", 0xB746F36C), justCreated);

	// check the effect exists
	if (pFxInst)
	{
		ptfxAttach::Attach(pFxInst, pVehicle, -1);

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


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxJump
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::TriggerPtFxJump					(CVehicle* pVehicle)
{
	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>VFXRANGE_VEH_JUMP_SQR)
	{
		return;
	}

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(atHashWithStringNotFinal("veh_ruiner_hop",0x4c71f601));
	if (pFxInst)
	{
		ptfxAttach::Attach(pFxInst, pVehicle, -1);

		// start the fx
		pFxInst->Start();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxLightTrail
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxLightTrail				(CVehicle* pVehicle, const int boneIndex, s32 id)
{
	if (boneIndex==-1)
	{
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_LIGHT_TRAIL+id, true, atHashWithStringNotFinal("veh_light_red_trail",0x99084CF1), justCreated);

	// check the effect exists
	if (pFxInst)
	{
		ptfxAttach::Attach(pFxInst, pVehicle, boneIndex);

		// set evolutions
		float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocity().Mag(), VFXVEHICLE_LIGHT_TRAIL_SPEED_EVO_MIN, VFXVEHICLE_LIGHT_TRAIL_SPEED_EVO_MAX);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

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


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxLightSmash
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::TriggerPtFxLightSmash				(CVehicle* pVehicle, const int boneId, Vec3V_In vPos, Vec3V_In vNormalIn)
{
	// don't update effect if the vehicle is off screen
	if (!pVehicle->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>VFXRANGE_VEH_LIGHT_SMASH_SQR)
	{
		return;
	}

	vfxAssertf(pVehicle, "Called with a NULL vehicle pointer");

	ptxEffectInst* pFxInst = NULL;
	atHashWithStringNotFinal hashName;
	switch (boneId)
	{
	case VEH_HEADLIGHT_L:
	case VEH_HEADLIGHT_R:
	case VEH_REVERSINGLIGHT_L:
	case VEH_REVERSINGLIGHT_R:
	case VEH_NEON_L:
	case VEH_NEON_R:
	case VEH_NEON_F:
	case VEH_NEON_B:
	case VEH_EXTRALIGHT_1:
	case VEH_EXTRALIGHT_2:
	case VEH_EXTRALIGHT_3:
	case VEH_EXTRALIGHT_4:
		hashName = atHashWithStringNotFinal("veh_light_clear",0xC1F2D517);
		pFxInst = ptfxManager::GetTriggeredInst(hashName);
		break;

	case VEH_TAILLIGHT_L:
	case VEH_TAILLIGHT_R:
	case VEH_BRAKELIGHT_L:
	case VEH_BRAKELIGHT_R:
	case VEH_BRAKELIGHT_M:
		hashName = atHashWithStringNotFinal("veh_light_red",0x35B434D6);
		pFxInst = ptfxManager::GetTriggeredInst(hashName);
		break;

	case VEH_INDICATOR_LF:
	case VEH_INDICATOR_RF:
	case VEH_INDICATOR_LR:
	case VEH_INDICATOR_RR:
		hashName = atHashWithStringNotFinal("veh_light_amber",0x60AA9FD8);
		pFxInst = ptfxManager::GetTriggeredInst(hashName);
		break;

	case VEH_EXTRA_5:
	case VEH_EXTRA_6:
	case VEH_EXTRA_9:	
		// Those are only valid for taxis, but they don't have an effect...
		/* No Op */
		break;
	default:
		vfxAssertf(0, "Unsupported bone found");
	}

	if (pFxInst)
	{
		Vec3V vNormal = vNormalIn;	// Assign from Vec3V_In to Vec3V so we can change it (Vec3V_In is Vec3V_ConstRef on PC).

		// override the normal passed in with one that points out from the centre of the vehicle
		vNormal = Vec3V(vPos.GetXY(), ScalarV(V_ZERO));
		if (IsZeroAll(vNormal))
		{
			vNormal = Vec3V(V_Z_AXIS_WZERO);
		}
		else
		{
			vNormal = Normalize(vNormal);
		}

		Mat34V vFxOffsetMtx;
		CVfxHelper::CreateMatFromVecZ(vFxOffsetMtx, vPos, vNormal);

		ptfxAttach::Attach(pFxInst, pVehicle, -1);

		pFxInst->SetOffsetMtx(vFxOffsetMtx);

		// add collision set
		// Vec3V vColnPos = VECTOR3_TO_VEC3V(pVehicle->TransformIntoWorldSpace(RCC_VECTOR3(vPos)));

		ptfxWrapper::SetVelocityAdd(pFxInst, RCC_VEC3V(pVehicle->GetVelocity()));

		pFxInst->Start();

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketVehHeadlightSmashFx>(
				CPacketVehHeadlightSmashFx(hashName,
					(u8)boneId,
					VEC3V_TO_VECTOR3(vPos), 
					VEC3V_TO_VECTOR3(vNormal)),
				pVehicle);
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxRespray
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::TriggerPtFxRespray				(CVehicle* pVehicle, Vec3V_In vFxPos, Vec3V_In vFxDir)
{
	if (CVfxHelper::GetDistSqrToCamera(vFxPos)>VFXRANGE_VEH_RESPRAY_SQR)
	{
		return;
	}

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(atHashWithStringNotFinal("veh_respray_smoke",0x22DF303A));
	if (pFxInst)
	{
		// set up the fx inst
		Mat34V fxMat;
		CVfxHelper::CreateMatFromVecYAlign(fxMat, vFxPos, vFxDir, Vec3V(V_Z_AXIS_WZERO));

		pFxInst->SetBaseMtx(fxMat);

		// set the colour tint
		CRGBA rgba = CVehicleModelInfo::GetVehicleColours()->GetVehicleColour(pVehicle->GetBodyColour1());
		ptfxWrapper::SetColourTint(pFxInst, rgba.GetRGBA().GetXYZ());

		// start the fx
		pFxInst->Start();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxEngineNoPanel
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxEngineNoPanel			(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, s32 boneIndex, s32 fxId, float speedEvo, float damageEvo, float fireEvo)
{
	// don't update effect if the vehicle is invisible
	if (!pVehicle->GetIsVisible())
	{
		return;
	}

	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>pVfxVehicleInfo->GetEngineDamagePtFxRangeSqr())
	{
		return;
	}

	// register the fx system
	bool justCreated = false;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_ENGINE_DAMAGE_PANEL_SHUT_OR_NO_PANEL+fxId, true, pVfxVehicleInfo->GetEngineDamagePtFxNoPanelName(), justCreated);

	// check the effect exists
	if (pFxInst)
	{
		// set evolutions
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("damage", 0x431375E1), damageEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("fire", 0xD30A036B), fireEvo);

		if (pVfxVehicleInfo->GetEngineDamagePtFxHasRotorEvo())
		{
			if (vfxVerifyf(pVehicle->InheritsFromHeli(), "trying to set a rotor evo on a non heli vehicle"))
			{
				CHeli* pHeli = static_cast<CHeli*>(pVehicle);

				// set the rotor speed evolution
				float rotorEvo = CVfxHelper::GetInterpValue(pHeli->GetMainRotorSpeed(), 0.0f, 1.0f);
				pFxInst->SetEvoValueFromHash(ATSTRINGHASH("rotor", 0xBEAA5FD7), rotorEvo);
			}
		}

		ptfxWrapper::SetVelocityAdd(pFxInst, RCC_VEC3V(pVehicle->GetVelocity()));

		if (fxId==1)
		{
			pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
		}

		//		pFxInst->SetOverrideDistSqrToCamera(fxDistSqr+FXVEHICLE_RENDER_ORDER_OFFSET_OVERHEAT);

		// set position
		ptfxAttach::Attach(pFxInst, pVehicle, boneIndex);

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


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxEnginePanelOpen
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxEnginePanelOpen		(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, s32 boneIndex, float speedEvo, float damageEvo, float panelEvo, float fireEvo)
{
	// don't update effect if the vehicle is invisible
	if (!pVehicle->GetIsVisible())
	{
		return;
	}

	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>pVfxVehicleInfo->GetEngineDamagePtFxRangeSqr())
	{
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_ENGINE_DAMAGE_PANEL_OPEN, true, pVfxVehicleInfo->GetEngineDamagePtFxPanelOpenName(), justCreated);

	// check the effect exists
	if (pFxInst)
	{
		// set evolutions
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("damage", 0x431375E1), damageEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("panel", 0x10C1C125), panelEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("fire", 0xD30A036B), fireEvo);

		ptfxWrapper::SetVelocityAdd(pFxInst, RCC_VEC3V(pVehicle->GetVelocity()));

//		pFxInst->SetOverrideDistSqrToCamera(fxDistSqr+FXVEHICLE_RENDER_ORDER_OFFSET_OVERHEAT);

		// set position
		ptfxAttach::Attach(pFxInst, pVehicle, boneIndex);

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


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxEnginePanelShut
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxEnginePanelShut			(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, s32 boneIndex, s32 fxId, float speedForwardEvo, float speedBackwardEvo, float damageEvo, float fireEvo)
{
	// don't update effect if the vehicle is invisible
	if (!pVehicle->GetIsVisible())
	{
		return;
	}

	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>pVfxVehicleInfo->GetEngineDamagePtFxRangeSqr())
	{
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_ENGINE_DAMAGE_PANEL_SHUT_OR_NO_PANEL+fxId, true, pVfxVehicleInfo->GetEngineDamagePtFxPanelShutName(), justCreated);

	// check the effect exists
	if (pFxInst)
	{
		// set evolutions
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedForwardEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("reverse", 0xAE3930D7), speedBackwardEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("damage", 0x431375E1), damageEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("fire", 0xD30A036B), fireEvo);

		ptfxWrapper::SetVelocityAdd(pFxInst, RCC_VEC3V(pVehicle->GetVelocity()));
		
		if (fxId==1)
		{
			pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
		}

//		pFxInst->SetOverrideDistSqrToCamera(fxDistSqr+FXVEHICLE_RENDER_ORDER_OFFSET_OVERHEAT+fxId/1000.0f);

		// set position
		ptfxAttach::Attach(pFxInst, pVehicle, boneIndex);

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


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxOverturnedSmoke
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxOverturnedSmoke		(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo)
{
	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>pVfxVehicleInfo->GetOverturnedSmokePtFxRangeSqr())
	{
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_OVERTURNED_SMOKE, true, pVfxVehicleInfo->GetOverturnedSmokePtFxName(), justCreated);

	// check the effect exists
	if (pFxInst)
	{
		// calc the final matrix
		Mat34V vVehicleMtx = pVehicle->GetMatrix();
		Vec3V vVehicleRight = vVehicleMtx.GetCol0();
		Vec3V vVehicleUp = vVehicleMtx.GetCol2();
		Vec3V vVehiclePos = vVehicleMtx.GetCol3();

		Mat34V vMtxWld;
		CVfxHelper::CreateMatFromVecYAlign(vMtxWld, vVehiclePos, -vVehicleUp, vVehicleRight);

		// calc the offset matrix
		Mat34V relFxMat;
		CVfxHelper::CreateRelativeMat(relFxMat, pVehicle->GetMatrix(), vMtxWld);

		ptfxAttach::Attach(pFxInst, pVehicle, -1);

		pFxInst->SetOffsetMtx(relFxMat);

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


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxEngineOilLeak
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxEngineOilLeak		(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, float oilDamage)
{
	// don't update effect if the vehicle is invisible
	if (!pVehicle->GetIsVisible())
	{
		return;
	}

	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>pVfxVehicleInfo->GetLeakPtFxRangeSqr())
	{
		return;
	}

	s32 engineBoneIndex = pVehicle->GetBoneIndex(VEH_ENGINE);
	if (engineBoneIndex>-1)
	{
		// register the fx system
		bool justCreated;
		ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_OIL_LEAK, true, pVfxVehicleInfo->GetLeakPtFxOilName(), justCreated);

		// check the effect exists
		if (pFxInst)
		{
			// set evolutions
			float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocityIncludingReferenceFrame().Mag(), pVfxVehicleInfo->GetLeakPtFxSpeedEvoMin(), pVfxVehicleInfo->GetLeakPtFxSpeedEvoMax());
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("damage", 0x431375E1), oilDamage);

			// set position
			ptfxAttach::Attach(pFxInst, pVehicle, engineBoneIndex);

			// don't cull these or they don't get processed in first person vehicle view
			pFxInst->SetCanBeCulled(false);

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
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxEnginePetrolLeak
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxEnginePetrolLeak		(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, float petrolTankDamage)
{
	// don't update effect if the vehicle is invisible
	if (!pVehicle->GetIsVisible())
	{
		return;
	}

	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>pVfxVehicleInfo->GetLeakPtFxRangeSqr())
	{
		return;
	}

	// get petrol tank bone matrix
	Vec3V vPetrolTankLclPosL, vPetrolTankLclPosR;
	if (pVehicle->GetVehicleDamage()->GetPetrolTankPosLcl(&RC_VECTOR3(vPetrolTankLclPosL), &RC_VECTOR3(vPetrolTankLclPosR)))
	{
		// register the fx system
		bool justCreated;
		ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_PETROL_LEAK, true, pVfxVehicleInfo->GetLeakPtFxPetrolName(), justCreated);

		// check the effect exists
		if (pFxInst)
		{
			// set evolutions
			float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocityIncludingReferenceFrame().Mag(), pVfxVehicleInfo->GetLeakPtFxSpeedEvoMin(), pVfxVehicleInfo->GetLeakPtFxSpeedEvoMax());
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("damage", 0x431375E1), petrolTankDamage);

			// calc the leak position
			Vec3V vPetrolLeakLclPos;
			if (IsZeroAll(vPetrolTankLclPosL))
			{
				vPetrolLeakLclPos = vPetrolTankLclPosR;
				ScalarV vPosX = vPetrolLeakLclPos.GetX();
				vPetrolLeakLclPos.SetX(vPosX*ScalarV(V_HALF));
			}
			else if (IsZeroAll(vPetrolTankLclPosR))
			{
				vPetrolLeakLclPos = vPetrolTankLclPosL;
				ScalarV vPosX = vPetrolLeakLclPos.GetX();
				vPetrolLeakLclPos.SetX(vPosX*ScalarV(V_HALF));
			}
			else
			{ 
				vPetrolLeakLclPos = vPetrolTankLclPosL + vPetrolTankLclPosR;
				vPetrolLeakLclPos = Scale(vPetrolLeakLclPos, ScalarV(V_HALF));
			}

			Vec3V vPetrolLeakWldPos = pVehicle->GetTransform().Transform(vPetrolLeakLclPos);

			// calc the petrol tank matrix
			Mat34V tankMat;
			tankMat.Set3x3((pVehicle->GetMatrix()));
			tankMat.SetCol3(vPetrolLeakWldPos);

			// calc the offset matrix
			Mat34V relFxMat;
			CVfxHelper::CreateRelativeMat(relFxMat, pVehicle->GetMatrix(), tankMat);

			// set position
			ptfxAttach::Attach(pFxInst, pVehicle, -1);

			pFxInst->SetOffsetPos(relFxMat.GetCol3());

			// don't cull these or they don't get processed in first person vehicle view
			pFxInst->SetCanBeCulled(false);

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
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxPetrolSpray
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxPetrolSpray		(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, float petrolTankDamage)
{
	// don't update effect if the vehicle is invisible
	if (!pVehicle->GetIsVisible())
	{
		return;
	}

	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>pVfxVehicleInfo->GetLeakPtFxRangeSqr())
	{
		return;
	}

	Vec3V vSprayPosLcl = RCC_VEC3V(pVehicle->GetVehicleDamage()->GetPetrolSprayPosLocal());
	Vec3V vSprayNrmLcl = RCC_VEC3V(pVehicle->GetVehicleDamage()->GetPetrolSprayNrmLocal());
	if (!IsZeroAll(vSprayPosLcl))
	{
		// register the fx system
		bool justCreated;
		ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_PETROL_LEAK, true, atHashWithStringNotFinal("veh_trailer_petrol_spray"), justCreated);

		// check the effect exists
		if (pFxInst)
		{
			// set evolutions
			float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocityIncludingReferenceFrame().Mag(), pVfxVehicleInfo->GetLeakPtFxSpeedEvoMin(), pVfxVehicleInfo->GetLeakPtFxSpeedEvoMax(), true);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("damage", 0x431375E1), petrolTankDamage);

			// attach to the vehicle
			ptfxAttach::Attach(pFxInst, pVehicle, -1);

			// calc the offset matrix
			Mat34V vOffsetMtx;
			CVfxHelper::CreateMatFromVecYAlign(vOffsetMtx, vSprayPosLcl, vSprayNrmLcl, Vec3V(V_Z_AXIS_WZERO));
			pFxInst->SetOffsetMtx(vOffsetMtx);

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
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxPlaneAfterburner
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxPlaneAfterburner			(CVehicle* pVehicle, eHierarchyId boneTag, int ptFxId)
{
	// don't update effect if the vehicle is invisible
	if (!pVehicle->GetIsVisible())
	{
		return;
	}

	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	// return if afterburner ptfx aren't enabled
	if (pVfxVehicleInfo->GetPlaneAfterburnerPtFxEnabled()==false)
	{
		return;
	}

	// check if we're in range
	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>pVfxVehicleInfo->GetPlaneAfterburnerPtFxRangeSqr())
	{
		return;
	}

	Mat34V boneMtx;
	CVehicleModelInfo* pModelInfo = pVehicle->GetVehicleModelInfo();
	s32 afterburnerBoneId = pModelInfo->GetBoneIndex(boneTag);
	if (afterburnerBoneId==-1)
	{
		return;
	}

	CVfxHelper::GetMatrixFromBoneIndex(boneMtx, pVehicle, afterburnerBoneId);
	if (IsZeroAll(boneMtx.GetCol0()))
	{
		return;
	}

	// register the fx system
	bool justCreated;
	int classFxOffset = PTFXOFFSET_VEHICLE_PLANE_AFTERBURNER + ptFxId;
	vfxAssert(classFxOffset >= PTFXOFFSET_VEHICLE_PLANE_AFTERBURNER && classFxOffset <= PTFXOFFSET_VEHICLE_PLANE_AFTERBURNER_LAST);
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, classFxOffset, true, pVfxVehicleInfo->GetPlaneAfterburnerPtFxName(), justCreated, false, true);

	// check the effect exists
	if (pFxInst)
	{
		// throttle evo
		float throttleEvo = 0.0f;
		if (pVehicle->m_Transmission.GetCurrentlyMissFiring()==false)
		{
			throttleEvo = CVfxHelper::GetInterpValue(pVehicle->GetThrottle(), 0.0f, 1.0f);
		}

		if (pVehicle->GetVehicleType()==VEHICLE_TYPE_PLANE)
		{
			CPlane* pPlane = static_cast<CPlane*>(pVehicle);
			if (pPlane && pPlane->GetVerticalFlightModeAvaliable())
			{
				throttleEvo *= (1.0f-pPlane->GetVerticalFlightModeRatio());
			}
		}
		else if (pVehicle->GetVehicleType()==VEHICLE_TYPE_HELI)
		{
			CHeli* pHeli = static_cast<CHeli*>(pVehicle);
			if (pHeli)
			{
				throttleEvo = 0.5f;

				CControl *pControl = NULL;
				CPed* pDriver = pHeli->GetDriver();
				bool bDrivenByPlayer = pDriver && pDriver->IsPlayer();
				if (bDrivenByPlayer && pHeli->GetStatus()==STATUS_PLAYER)
				{
#if GTA_REPLAY
					if(CReplayMgr::IsReplayInControlOfWorld())
					{
						throttleEvo = pHeli->GetJetPackThrusterThrotle();
					}
					else
#endif // GTA_REPLAY
					{
						pControl = pDriver->GetControlFromPlayer();
						if (pControl)
						{
							if(!pHeli->IsNetworkClone())
							{
								float throttleUp = pControl->GetVehicleFlyThrottleUp().GetNorm01();
								float throttleDown = pControl->GetVehicleFlyThrottleDown().GetNorm01();

								throttleEvo = 0.5f + (0.5f*throttleUp) - (0.5f*throttleDown);

								pHeli->SetJetPackThrusterThrotle(throttleEvo);
							}					
						}
						else if (pHeli->IsNetworkClone())
						{
							throttleEvo = pHeli->GetJetPackThrusterThrotle();
						}
					}
				}
			}
		}

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("throttle", 0xEA0151DC), throttleEvo);

		ptfxAttach::Attach(pFxInst, pVehicle, afterburnerBoneId);

		pFxInst->SetUserZoom(pVfxVehicleInfo->GetPlaneAfterburnerPtFxScale());

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


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxPlaneWingTips
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxPlaneWingTips			(CPlane* pPlane)
{
	// don't update effect if the vehicle is invisible
	if (!pPlane->GetIsVisible())
	{
		return;
	}

	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pPlane);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	// return if wingtip ptfx aren't enabled
	if (pVfxVehicleInfo->GetPlaneWingTipPtFxEnabled()==false)
	{
		return;
	}

	// check if we're in range
	if (CVfxHelper::GetDistSqrToCamera(pPlane->GetTransform().GetPosition())>pVfxVehicleInfo->GetPlaneWingTipPtFxRangeSqr())
	{
		return;
	}

	// check we're going fast enough
	float planeSpeed = pPlane->GetVelocity().Mag();
	if (planeSpeed<pVfxVehicleInfo->GetPlaneWingTipPtFxSpeedEvoMin())
	{
		return;
	}

	// check that we have a valid wingtip matrix
	for (s32 i=0; i<2; i++)
	{
		CVehicleModelInfo* pModelInfo = pPlane->GetVehicleModelInfo();
		s32 wingTipBoneId = pModelInfo->GetBoneIndex(PLANE_WINGTIP_1+i);
		if (wingTipBoneId==-1)
		{
			continue;
		}

		// check if this section has broken off
		if ((i==0 && pPlane->GetAircraftDamage().HasSectionBrokenOff(pPlane, CAircraftDamage::WING_L)) ||
			(i==1 && pPlane->GetAircraftDamage().HasSectionBrokenOff(pPlane, CAircraftDamage::WING_R)))
		{
			continue;
		}

		Mat34V boneMtx;
		CVfxHelper::GetMatrixFromBoneIndex(boneMtx, pPlane, wingTipBoneId);
		if (IsZeroAll(boneMtx.GetCol0()))
		{
			continue;
		}

		// check which side of the plane this is on
		Mat34V relFxMat;
		CVfxHelper::CreateRelativeMat(relFxMat, pPlane->GetMatrix(), boneMtx);
		bool isLeft = relFxMat.GetCol3().GetXf()<0.0f;

		// register the fx system
		bool justCreated;
		ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pPlane, PTFXOFFSET_VEHICLE_PLANE_WINGTIP+i, true, pVfxVehicleInfo->GetPlaneWingTipPtFxName(), justCreated);

		// check the effect exists
		if (pFxInst)
		{
			// speed evo
			float speedEvo = CVfxHelper::GetInterpValue(planeSpeed, pVfxVehicleInfo->GetPlaneWingTipPtFxSpeedEvoMin(), pVfxVehicleInfo->GetPlaneWingTipPtFxSpeedEvoMax());
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);
			
			// force evo
// 			Vec3V vVel = RCC_VEC3V(pPlane->GetVelocity());
// 			vVel = Normalize(vVel);
// 			float dot = Dot(pPlane->GetMatrix().GetCol1(), vVel).Getf();
// 			float forceEvo = CVfxHelper::GetInterpValue(dot, 1.0f, 0.99f);
// 			pFxInst->SetEvoValue("force", forceEvo);

			ptfxAttach::Attach(pFxInst, pPlane, wingTipBoneId);

			// check if the effect has just been created 
			if (justCreated)
			{
				// flip the x axis for left hand effects
				if (isLeft)
				{
					pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
				}

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
				CReplayMgr::RecordFx<CPacketPlaneWingtipFx>(CPacketPlaneWingtipFx((u32)i, pVfxVehicleInfo->GetPlaneWingTipPtFxName(), speedEvo), pPlane);
			}
#endif


		}
	}
}  


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxPlaneDamageFire
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxPlaneDamageFire			(CPlane* pPlane, Vec3V_In vPosLcl, float UNUSED_PARAM(currTime), float UNUSED_PARAM(maxTime), s32 id)
{
	// don't update effect if the vehicle is invisible
	if (!pPlane->GetIsVisible())
	{
		return;
	}

	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pPlane);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	// return if damage fire ptfx aren't enabled
	if (pVfxVehicleInfo->GetPlaneDamageFirePtFxEnabled()==false)
	{
		return;
	}

	// check if we're in range
	if (CVfxHelper::GetDistSqrToCamera(pPlane->GetTransform().GetPosition())>pVfxVehicleInfo->GetPlaneDamageFirePtFxRangeSqr())
	{
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pPlane, PTFXOFFSET_VEHICLE_PLANE_DAMAGE_FIRE+id, true, pVfxVehicleInfo->GetPlaneDamageFirePtFxName(), justCreated);

//	vfxDebugf1("Registering PlaneDamageFire %d %x, %d, %.3f, %.3f, %.3f", fwTimer::GetFrameCount(), pPlane, id, VEC3V_ARGS(vPosLcl));

	// check the effect exists
	if (pFxInst)
	{
		ptfxAttach::Attach(pFxInst, pPlane, -1);

		Mat34V vFxOffsetMtx;
		CVfxHelper::CreateMatFromVecZAlign(vFxOffsetMtx, vPosLcl, pPlane->GetMatrix().GetCol2(), pPlane->GetMatrix().GetCol1());
		pFxInst->SetOffsetMtx(vFxOffsetMtx);

		float speedEvo = CVfxHelper::GetInterpValue(pPlane->GetVelocity().Mag(), pVfxVehicleInfo->GetPlaneDamageFirePtFxSpeedEvoMin(), pVfxVehicleInfo->GetPlaneDamageFirePtFxSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		// check if the effect has just been created 
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
//  UpdatePtFxAircraftSectionDamageSmoke
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxAircraftSectionDamageSmoke		(CVehicle* pVehicle, Vec3V_In vPosLcl, s32 id)
{
	// don't update effect if the vehicle is invisible
	if (!pVehicle->GetIsVisible())
	{
		return;
	}

	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	// return if section damage ptfx aren't enabled
	if (pVfxVehicleInfo->GetAircraftSectionDamageSmokePtFxEnabled()==false)
	{
		return;
	}

	// check if we're in range
	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>pVfxVehicleInfo->GetAircraftSectionDamageSmokePtFxRangeSqr())
	{
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_AIRCRAFT_SECTION_DAMAGE_SMOKE+id, true, pVfxVehicleInfo->GetAircraftSectionDamageSmokePtFxName(), justCreated);

//	vfxDebugf1("Registering AircraftSectionDamageSmoke %d %x, %d, %.3f, %.3f, %.3f", fwTimer::GetFrameCount(), pVehicle, id, VEC3V_ARGS(vPosLcl));

	// check the effect exists
	if (pFxInst)
	{
		ptfxAttach::Attach(pFxInst, pVehicle, -1);

		Mat34V vFxOffsetMtx;
		CVfxHelper::CreateMatFromVecZAlign(vFxOffsetMtx, vPosLcl, pVehicle->GetMatrix().GetCol2(), pVehicle->GetMatrix().GetCol1());
		pFxInst->SetOffsetMtx(vFxOffsetMtx);

		float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocity().Mag(), pVfxVehicleInfo->GetAircraftSectionDamageSmokePtFxSpeedEvoMin(), pVfxVehicleInfo->GetAircraftSectionDamageSmokePtFxSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		// check if the effect has just been created 
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
//  ProcessGroundDisturb
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::ProcessGroundDisturb		(CVehicle* pVehicle)
{
	// don't update effect if the vehicle is invisible
	if (!pVehicle->GetIsVisible())
	{
		return;
	}
 
 	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}
 
 	// return if ground disturbance ptfx aren't enabled
 	if (pVfxVehicleInfo->GetPlaneGroundDisturbPtFxEnabled()==false)
 	{
 		return;
 	}
 
	// return if we're not in range
	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>pVfxVehicleInfo->GetPlaneGroundDisturbPtFxRangeSqr())
	{
		return;
	}

	// return if we're not moving fast enough.
	// For VTOL aircraft, we always process the ground probes regardless of speed, as the audio makes use of the distance-to-ground value to 
	// modulate vertical thrust audio
	if (pVehicle->GetVehicleType()==VEHICLE_TYPE_PLANE)
	{
		CPlane* pPlane = static_cast<CPlane*>(pVehicle);
		if(pPlane && !pPlane->GetVerticalFlightModeAvaliable())
		{
			Vec3V vPlaneLateralVel = RCC_VEC3V(pPlane->GetVelocity());
			vPlaneLateralVel.SetZ(ScalarV(V_ZERO));
			float planeLateralSpeed = Mag(vPlaneLateralVel).Getf();
			if (planeLateralSpeed<pVfxVehicleInfo->GetPlaneGroundDisturbPtFxSpeedEvoMin())
			{
				return;
			}
		}	
	}

	// trigger the probe
	Vec3V vStartPos = pVehicle->GetTransform().GetPosition();
	Vec3V vEndPos = vStartPos-Vec3V(0.0f, 0.0f, pVfxVehicleInfo->GetPlaneGroundDisturbPtFxDist());
	m_asyncProbeMgr.TriggerGroundDisturbProbe(vStartPos, vEndPos, pVehicle, pVfxVehicleInfo);
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxVehicleGroundDisturb
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxVehicleGroundDisturb			(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, Vec3V_In vPos, Vec3V_In vNormal, float distEvo, VfxDisturbanceType_e vfxDisturbanceType)
{
	// If we're in a VTOL aircraft, we do our speed test *after* the ground probes so that audio can be updated with the distEvo value even while hovering stationary
	if(pVehicle->InheritsFromPlane())
	{
		CPlane* pPlane = static_cast<CPlane*>(pVehicle);

		if(pPlane->GetVerticalFlightModeAvaliable())
		{
			Vec3V vPlaneLateralVel = RCC_VEC3V(pPlane->GetVelocity());
			vPlaneLateralVel.SetZ(ScalarV(V_ZERO));
			float planeLateralSpeed = Mag(vPlaneLateralVel).Getf();
			if (planeLateralSpeed<pVfxVehicleInfo->GetPlaneGroundDisturbPtFxSpeedEvoMin())
			{
				if (pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_PLANE)
				{
					static_cast<audPlaneAudioEntity*>(pVehicle->GetVehicleAudioEntity())->SetDownwashHeightFactor(distEvo);
				}

				return;
			}
		}	
	}

	atHashWithStringNotFinal fxHashName = u32(0);
	if (vfxDisturbanceType==VFX_DISTURBANCE_DEFAULT)
	{
		fxHashName = pVfxVehicleInfo->GetPlaneGroundDisturbPtFxNameDefault();
	}
	else if (vfxDisturbanceType==VFX_DISTURBANCE_SAND)
	{
		fxHashName = pVfxVehicleInfo->GetPlaneGroundDisturbPtFxNameSand();
	}
	else if (vfxDisturbanceType==VFX_DISTURBANCE_DIRT)
	{
		fxHashName = pVfxVehicleInfo->GetPlaneGroundDisturbPtFxNameDirt();
	}
	else if (vfxDisturbanceType==VFX_DISTURBANCE_WATER)
	{
		fxHashName = pVfxVehicleInfo->GetPlaneGroundDisturbPtFxNameWater();
	}
	else if (vfxDisturbanceType==VFX_DISTURBANCE_FOLIAGE)
	{
		fxHashName = pVfxVehicleInfo->GetPlaneGroundDisturbPtFxNameFoliage();
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_PLANE_GROUND_DISTURB, true, fxHashName, justCreated);

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
		CVfxHelper::CreateMatFromVecZAlign(vMtx, vPos, vNormal, pVehicle->GetTransform().GetB());
		pFxInst->SetBaseMtx(vMtx);

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("distance", 0xD6FF9582), distEvo);

		float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocityIncludingReferenceFrame().Mag(), pVfxVehicleInfo->GetPlaneGroundDisturbPtFxSpeedEvoMin(), pVfxVehicleInfo->GetPlaneGroundDisturbPtFxSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set its position
			pFxInst->Start();
		}

		if (pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_PLANE)
		{
			static_cast<audPlaneAudioEntity*>(pVehicle->GetVehicleAudioEntity())->TriggerDownwash(fxHashName, RCC_VECTOR3(vPos), distEvo, speedEvo);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessPlaneDownwash
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::ProcessPlaneDownwash			(CPlane* pPlane)
{
	// don't update effect if the vehicle is invisible
	if (!pPlane->GetIsVisible())
	{
		return;
	}

	// return if vehicle is not upright
	if (pPlane->GetTransform().GetC().GetZf() < 0.0f)
	{
		return;
	}

	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pPlane);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	// return if downwash ptfx aren't enabled
	if (pVfxVehicleInfo->GetAircraftDownwashPtFxEnabled()==false)
	{
		return;
	}

	// check if we're in range
	if (CVfxHelper::GetDistSqrToCamera(pPlane->GetTransform().GetPosition())>pVfxVehicleInfo->GetAircraftDownwashPtFxRangeSqr())
	{
		return;
	}

	float downforceEvo = 0.5f + (0.5f*CVfxHelper::GetInterpValue(pPlane->GetThrottle(), 0.0f, 1.0f));
	downforceEvo *= pPlane->GetVerticalFlightModeRatio();
	if (downforceEvo==0.0f)
	{
		return;
	}

	if (MI_PLANE_AVENGER.IsValid() && pPlane->GetModelIndex()==MI_PLANE_AVENGER)
	{
		CVehicleModelInfo* pModelInfo = pPlane->GetVehicleModelInfo();
		s32 propBoneLId = pModelInfo->GetBoneIndex(PLANE_PROP_1);
		s32 propBoneRId = pModelInfo->GetBoneIndex(PLANE_PROP_2);
		if (propBoneLId>-1 && propBoneRId>-1)
		{
			Mat34V vPropLMtx;
			Mat34V vPropRMtx;
			CVfxHelper::GetMatrixFromBoneIndex(vPropLMtx, pPlane, propBoneLId);
			CVfxHelper::GetMatrixFromBoneIndex(vPropRMtx, pPlane, propBoneRId);

			// left
			float waterDepth;
			CVfxHelper::GetWaterDepth(vPropLMtx.GetCol3(), waterDepth);
			if (waterDepth<=0.0f)
			{
				Vec3V vPropDown = -vPropLMtx.GetCol2();
				ProcessDownwash(pPlane, pVfxVehicleInfo, vPropLMtx.GetCol3(), vPropDown, downforceEvo, 0);
			}


			// right
			CVfxHelper::GetWaterDepth(vPropRMtx.GetCol3(), waterDepth);
			if (waterDepth<=0.0f)
			{
				Vec3V vPropDown = -vPropRMtx.GetCol2();
				ProcessDownwash(pPlane, pVfxVehicleInfo, vPropRMtx.GetCol3(), vPropDown, downforceEvo, 1);
			}

		}
	}
	else
	{
		CVehicleModelInfo* pModelInfo = pPlane->GetVehicleModelInfo();
		s32 nozzleBoneFId = pModelInfo->GetBoneIndex(PLANE_NOZZLE_F);
		s32 nozzleBoneRId = pModelInfo->GetBoneIndex(PLANE_NOZZLE_R);
		if (nozzleBoneFId==-1 || nozzleBoneRId==-1)
		{
			// nozzles doesn't exist - no downwash effect
			return;
		}

		Mat34V vNozzleFMtx;
		Mat34V vNozzleRMtx;
		CVfxHelper::GetMatrixFromBoneIndex(vNozzleFMtx, pPlane, nozzleBoneFId);
		CVfxHelper::GetMatrixFromBoneIndex(vNozzleRMtx, pPlane, nozzleBoneRId);

		Vec3V vNozzleCentrePos = (vNozzleFMtx.GetCol3()+vNozzleRMtx.GetCol3()) * ScalarV(V_HALF);
		Mat34V vNozzleMtx = vNozzleFMtx;
		vNozzleMtx.SetCol3(vNozzleCentrePos);

		// check if the rotor is in water
		float waterDepth;
		CVfxHelper::GetWaterDepth(vNozzleMtx.GetCol3(), waterDepth);
		if (waterDepth>0.0f)
		{
			return;
		}

		Vec3V vNozzleDown = -vNozzleMtx.GetCol2();
		ProcessDownwash(pPlane, pVfxVehicleInfo, vNozzleCentrePos, vNozzleDown, downforceEvo, 0);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessHeliDownwash
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::ProcessHeliDownwash			(CRotaryWingAircraft* pAircraft)
{
	// don't update effect if the vehicle is invisible
	if (!pAircraft->GetIsVisible())
	{
		return;
	}

	// return if vehicle is not upright
	if (pAircraft->GetTransform().GetC().GetZf() < 0.0f)
	{
		return;
	}

	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pAircraft);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	// return if downwash ptfx aren't enabled
	if (pVfxVehicleInfo->GetAircraftDownwashPtFxEnabled()==false)
	{
		return;
	}

	// check if we're in range
	if (CVfxHelper::GetDistSqrToCamera(pAircraft->GetTransform().GetPosition())>pVfxVehicleInfo->GetAircraftDownwashPtFxRangeSqr())
	{
		return;
	}

	if (MI_JETPACK_THRUSTER.IsValid() && pAircraft->GetModelIndex()==MI_JETPACK_THRUSTER)
	{
		// check if the rotor is in water
		CVehicleModelInfo* pModelInfo = pAircraft->GetVehicleModelInfo();
		s32 rotorBone1Id = pModelInfo->GetBoneIndex(HELI_ROTOR_MAIN);
		s32 rotorBone2Id = pModelInfo->GetBoneIndex(HELI_ROTOR_REAR);
		if (rotorBone1Id>-1 && rotorBone2Id>-1)
		{
			Mat34V vRotor1Mtx;
			Mat34V vRotor2Mtx;
			CVfxHelper::GetMatrixFromBoneIndex(vRotor1Mtx, pAircraft, rotorBone1Id);
			CVfxHelper::GetMatrixFromBoneIndex(vRotor2Mtx, pAircraft, rotorBone2Id);

			// main
			float waterDepth;
			CVfxHelper::GetWaterDepth(vRotor1Mtx.GetCol3(), waterDepth);
			if (waterDepth<=0.0f)
			{
				Vec3V vRotorPos = vRotor1Mtx.GetCol3();
				Vec3V vAircraftDown = -vRotor1Mtx.GetCol2();
				float downforceEvo = CVfxHelper::GetInterpValue(pAircraft->GetMainRotorSpeed(), 0.0f, 1.0f);

				ProcessDownwash(pAircraft, pVfxVehicleInfo, vRotorPos, vAircraftDown, downforceEvo, 0);
			}

			// rear
			CVfxHelper::GetWaterDepth(vRotor2Mtx.GetCol3(), waterDepth);
			if (waterDepth<=0.0f)
			{
				Vec3V vRotorPos = vRotor2Mtx.GetCol3();
				Vec3V vAircraftDown = -vRotor2Mtx.GetCol2();
				float downforceEvo = CVfxHelper::GetInterpValue(pAircraft->GetMainRotorSpeed(), 0.0f, 1.0f);

				ProcessDownwash(pAircraft, pVfxVehicleInfo, vRotorPos, vAircraftDown, downforceEvo, 1);
			}
		}
	}
	else
	{
		// check if the rotor is in water
		CVehicleModelInfo* pModelInfo = pAircraft->GetVehicleModelInfo();
		s32 rotorBoneId = pModelInfo->GetBoneIndex(HELI_ROTOR_MAIN);
		if (rotorBoneId==-1)
		{
			// rotor doesn't exist - no downwash effect
			return;
		}

		Mat34V vRotorMtx;
		CVfxHelper::GetMatrixFromBoneIndex(vRotorMtx, pAircraft, rotorBoneId);

		float waterDepth;
		CVfxHelper::GetWaterDepth(vRotorMtx.GetCol3(), waterDepth);
		if (waterDepth>0.0f)
		{
			return;
		}

		Vec3V vRotorPos = vRotorMtx.GetCol3();
		Vec3V vAircraftDown = -pAircraft->GetTransform().GetC();
		float downforceEvo = CVfxHelper::GetInterpValue(pAircraft->GetMainRotorSpeed(), 0.0f, 1.0f);

		ProcessDownwash(pAircraft, pVfxVehicleInfo, vRotorPos, vAircraftDown, downforceEvo, 0);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessDownwash
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::ProcessDownwash				(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, Vec3V_In vPos, Vec3V_In vDir, float downforceEvo, u32 ptFxId)
{
	if (m_downwashPtFxDisabled)
	{
		return;
	}

	float dist = pVfxVehicleInfo->GetAircraftDownwashPtFxDist();

	Vec3V vStartPos = vPos;
	Vec3V vEndPos = vStartPos + (ScalarVFromF32(dist)*vDir);

	m_asyncProbeMgr.TriggerDownwashProbe(vStartPos, vEndPos, vDir, pVehicle, pVfxVehicleInfo, downforceEvo, ptFxId);
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxDownwash
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxDownwash				(CVehicle* pVehicle, CVfxVehicleInfo* pVfxVehicleInfo, Vec3V_In vPos, Vec3V_In vNormal, float downforceEvo, float distEvo, VfxDisturbanceType_e vfxDisturbanceType, u32 ptFxId)
{
	bool justCreated;
	atHashWithStringNotFinal hashName = (u32)0;
	if (vfxDisturbanceType==VFX_DISTURBANCE_DEFAULT)
	{
		hashName = pVfxVehicleInfo->GetAircraftDownwashPtFxNameDefault();
	}
	else if (vfxDisturbanceType==VFX_DISTURBANCE_SAND)
	{
		hashName = pVfxVehicleInfo->GetAircraftDownwashPtFxNameSand();
	}
	else if (vfxDisturbanceType==VFX_DISTURBANCE_DIRT)
	{
		hashName = pVfxVehicleInfo->GetAircraftDownwashPtFxNameDirt();
	}
	else if (vfxDisturbanceType==VFX_DISTURBANCE_WATER)
	{
		hashName = pVfxVehicleInfo->GetAircraftDownwashPtFxNameWater();
	}
	else if (vfxDisturbanceType==VFX_DISTURBANCE_FOLIAGE)
	{
		hashName = pVfxVehicleInfo->GetAircraftDownwashPtFxNameFoliage();
	}

	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_DOWNWASH+ptFxId, true, hashName, justCreated);
	if (pFxInst)
	{
		Mat34V vMtx;
		CVfxHelper::CreateMatFromVecZAlign(vMtx, vPos, vNormal, pVehicle->GetTransform().GetB());
		pFxInst->SetBaseMtx(vMtx);

		// set the distance evolution 
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("distance", 0xD6FF9582), distEvo);

		float speedEvo = CVfxHelper::GetInterpValue(pVehicle->GetVelocity().Mag(), pVfxVehicleInfo->GetAircraftDownwashPtFxSpeedEvoMin(), pVfxVehicleInfo->GetAircraftDownwashPtFxSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		float angleEvo = vNormal.GetZf();
		angleEvo = Max(angleEvo, 0.0f);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("angle", 0xF3805B5), 1.0f-angleEvo);

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("downforce", 0xB7445D32), downforceEvo);

		// fade out if the camera is inside an interior
		ptfxWrapper::SetAlphaTint(pFxInst, CPortal::GetInteriorFxLevel());

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created
			pFxInst->Start();
		}

		if (pVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_HELI)
		{
			static_cast<audHeliAudioEntity*>(pVehicle->GetVehicleAudioEntity())->TriggerDownwash(hashName, RCC_VECTOR3(vPos), distEvo, speedEvo, angleEvo, downforceEvo);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxPlaneSmoke
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxVehicle::UpdatePtFxPlaneSmoke					(CVehicle* pVehicle, Color32 colour)
{
	if (!pVehicle->GetIsVisible())
	{
		return;
	}

	// check if the vehicle is within fx creation range
	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>200.0f*200.0f)
	{
		return;
	}

	// register the fx system
	bool justCreated = false;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_PLANE_SMOKE, true, ATSTRINGHASH("mp_parachute_smoke", 0xba1c6afa), justCreated);
	if (pFxInst)
	{
		ptfxAttach::Attach(pFxInst, pVehicle, -1);

		static Vec3V vOffsetPos(-0.75f, -5.0f, 0.0f);
		pFxInst->SetOffsetPos(vOffsetPos);

		if (justCreated)
		{
			ptfxWrapper::SetColourTint(pFxInst, colour.GetRGB());

			static float userZoom = 2.5f;
			pFxInst->SetUserZoom(userZoom);

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
//  TriggerPtFxPropellerDestroy
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxVehicle::TriggerPtFxPropellerDestroy			(CPlane* pPlane, s32 boneIndex)
{
	// don't update effect if the vehicle is off screen
	if (!pPlane->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	if (CVfxHelper::GetDistSqrToCamera(pPlane->GetTransform().GetPosition())>VFXRANGE_VEH_PLANE_PROP_DESTROY_SQR)
	{
		return;
	}

	if (boneIndex>-1)
	{
		Mat34V vMtxPropeller;
		CVfxHelper::GetMatrixFromBoneIndex(vMtxPropeller, pPlane, boneIndex);

		// calc the offset matrix
		Mat34V relFxMat;
		CVfxHelper::CreateRelativeMat(relFxMat, pPlane->GetMatrix(), vMtxPropeller);

		ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(atHashWithStringNotFinal("veh_plane_propeller_destroy",0x8AA67C5C));
		if (pFxInst)
		{
			ptfxAttach::Attach(pFxInst, pPlane, -1);

			pFxInst->SetOffsetPos(relFxMat.GetCol3());

			// scale effect
			if (pPlane->GetModelIndex()==MI_PLANE_BOMBUSHKA)
			{
				pFxInst->SetUserZoom(2.0f);
			}

			pFxInst->Start();
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxHeliRotorDestroy
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::TriggerPtFxHeliRotorDestroy		(CRotaryWingAircraft* pAircraft, bool isTailRotor)
{
	// don't update effect if the vehicle is off screen
	if (!pAircraft->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	if (CVfxHelper::GetDistSqrToCamera(pAircraft->GetTransform().GetPosition())>VFXRANGE_VEH_HELI_ROTOR_DESTROY_SQR)
	{
		return;
	}

	// get the global matrix of the rotor
	CVehicleModelInfo* pModelInfo = static_cast<CVehicleModelInfo*>(pAircraft->GetBaseModelInfo());

	s32 rotorBoneId = -1;
	if (isTailRotor)
	{
		rotorBoneId = pModelInfo->GetBoneIndex(HELI_ROTOR_REAR);
	}
	else
	{
		rotorBoneId = pModelInfo->GetBoneIndex(HELI_ROTOR_MAIN);
	}

	if (rotorBoneId>-1)
	{
		Mat34V rotorMtx;
		CVfxHelper::GetMatrixFromBoneIndex(rotorMtx, pAircraft, rotorBoneId);

		// calc the offset matrix
		Mat34V relFxMat;
		CVfxHelper::CreateRelativeMat(relFxMat, pAircraft->GetMatrix(), rotorMtx);

		ptxEffectInst* pFxInst = NULL;
		if (isTailRotor)
		{
			pFxInst = ptfxManager::GetTriggeredInst(atHashWithStringNotFinal("veh_rotor_break_tail",0xE3635FE6));
		}
		else
		{
			pFxInst = ptfxManager::GetTriggeredInst(atHashWithStringNotFinal("veh_rotor_break",0xFFCEBAD5));
		}

		if (pFxInst)
		{
			ptfxAttach::Attach(pFxInst, pAircraft, -1);

			pFxInst->SetOffsetPos(relFxMat.GetCol3());

			// scale effect
			if (pAircraft->GetModelIndex()==MI_HELI_POLICE_2)
			{
				pFxInst->SetUserZoom(1.4f);
			}

			pFxInst->Start();
		}
	}

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketHeliPartsDestroyFx>(CPacketHeliPartsDestroyFx(isTailRotor), pAircraft);
	}
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxTrainWheelSparks
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxTrainWheelSparks		(CVehicle* pVehicle, s32 boneIndex, Vec3V_In vWheelOffset, const float squealEvo, s32 wheelId, bool isLeft)
{
	// don't update effect if the vehicle is off screen
	if (!pVehicle->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	Mat34V boneMtx;
	CVfxHelper::GetMatrixFromBoneIndex(boneMtx, pVehicle, boneIndex);

	Vec3V vWheelPos = boneMtx.GetCol3();
	if (CVfxHelper::GetDistSqrToCamera(vWheelPos)>VFXRANGE_VEH_TRAIN_SPARKS_SQR)
	{
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_TRAIN_SPARKS+wheelId, true, atHashWithStringNotFinal("veh_train_sparks"), justCreated);

	// check the effect exists
	if (pFxInst)
	{
		// set evolutions
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("squeal", 0xB5448572), squealEvo);

		// set position
		ptfxAttach::Attach(pFxInst, pVehicle, boneIndex);

		// check if the effect has just been created 
		if (justCreated)
		{		
			// flip the x axis for left hand effects
			if (isLeft)
			{
				pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
			}

			ptfxWrapper::SetVelocityAdd(pFxInst, RCC_VEC3V(pVehicle->GetVelocity()));

			pFxInst->SetOffsetPos(vWheelOffset);

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



///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxVehicleWeaponCharge
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::UpdatePtFxVehicleWeaponCharge		(CVehicle* pVehicle, s32 boneIndex, float chargeEvo)
{
	// don't update effect if the vehicle is off screen
	if (!pVehicle->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	Mat34V boneMtx;
	CVfxHelper::GetMatrixFromBoneIndex(boneMtx, pVehicle, boneIndex);

	if (CVfxHelper::GetDistSqrToCamera(boneMtx.GetCol3())>VFXRANGE_VEH_WEAPON_CHARGE_SQR)
	{
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, PTFXOFFSET_VEHICLE_WEAPON_CHARGE, true, atHashWithStringNotFinal("muz_xm_khanjali_railgun_charge"), justCreated);

	// check the effect exists
	if (pFxInst)
	{
		// set evolutions
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("charge", 0x355C4F3A), chargeEvo);

		// set position
		ptfxAttach::Attach(pFxInst, pVehicle, boneIndex);

		// check if the effect has just been created 
		if (justCreated)
		{		
			ptfxAttach::Attach(pFxInst, pVehicle, boneIndex);

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


///////////////////////////////////////////////////////////////////////////////
//  ProcessWreckedFires
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::ProcessWreckedFires					(CVehicle* pVehicle, CPed* pCulprit, s32 numGenerations)
{
	// return if this vehicle cannot be damaged by flames
	// MN - removed this - if the vehicle blows up we want flames - these tests should stop things much earlier in proceedings if required
// 	if (pVehicle->m_nPhysicalFlags.bNotDamagedByFlames || pVehicle->m_nPhysicalFlags.bNotDamagedByAnything)
// 	{
// 		return;
// 	}
	
	if (pVehicle->IsNetworkClone())
	{
		return;
	}

	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	// create the main wrecked fire
	if (pVfxVehicleInfo->GetWreckedFirePtFxEnabled())
	{
		float burnTime = pVfxVehicleInfo->GetWreckedFirePtFxDurationMin()+(((pVehicle->GetRandomSeed()&0xff)/255.0f)*(pVfxVehicleInfo->GetWreckedFirePtFxDurationMax()-pVfxVehicleInfo->GetWreckedFirePtFxDurationMin()));
		float maxRadius = pVfxVehicleInfo->GetWreckedFirePtFxRadius();

		// calculate the local position of the wrecked fire
		Vec3V vBonePosLcl(V_ZERO);
		if (pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetNumSeats() > 0)
		{
			int iDriverSeatIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetDriverSeat();

			int iDriverSeatBoneIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromSeat(iDriverSeatIndex);
			if (iDriverSeatBoneIndex>-1)
			{
				vBonePosLcl = RCC_MAT34V(pVehicle->GetLocalMtx(iDriverSeatBoneIndex)).GetCol3();
				vBonePosLcl.SetX(ScalarV(V_FLT_SMALL_1)); // using zero causes the sorting to keep thinking it's swapping closest side

				g_fireMan.StartVehicleWreckedFire(pVehicle, vBonePosLcl, 0, pCulprit, burnTime, maxRadius, numGenerations);
			}
		}
	}

	// only create the main fire if we have too many wrecked fires already
	u32 numActiveWreckedFires = g_fireMan.GetNumWreckedFires();
	if (numActiveWreckedFires>VFXVEHICLE_NUM_WRECKED_FIRES_REJECT_THRESH)
	{
		return;
	}

	// create the first additional wrecked fires
	if (pVfxVehicleInfo->GetWreckedFire2PtFxEnabled())
	{
		float burnTime = pVfxVehicleInfo->GetWreckedFire2PtFxDurationMin()+(((pVehicle->GetRandomSeed()&0xff)/255.0f)*(pVfxVehicleInfo->GetWreckedFire2PtFxDurationMax()-pVfxVehicleInfo->GetWreckedFire2PtFxDurationMin()));
		float maxRadius = pVfxVehicleInfo->GetWreckedFire2PtFxRadius();

		Vec3V vBonePosLcl(V_ZERO);
		if (pVfxVehicleInfo->GetWreckedFire2PtFxUseOverheatBone())
		{
			s32 overheatBoneIndex = pVehicle->GetBoneIndex((eHierarchyId)(VEH_OVERHEAT));
			if (overheatBoneIndex>-1)
			{
				Mat34V vMtxWld;
				CVfxHelper::GetMatrixFromBoneIndex(vMtxWld, pVehicle, overheatBoneIndex);

				if (IsZeroAll(vMtxWld.GetCol0())==false)
				{
					Mat34V vMtxLcl;
					CVfxHelper::CreateRelativeMat(vMtxLcl, pVehicle->GetTransform().GetMatrix(), vMtxWld);

					vBonePosLcl = vMtxLcl.GetCol3();

					g_fireMan.StartVehicleWreckedFire(pVehicle, vBonePosLcl, 1, pCulprit, burnTime, maxRadius, numGenerations);
				}
			}
		}
		else
		{
			vBonePosLcl = pVfxVehicleInfo->GetWreckedFire2PtFxOffsetPos();
			g_fireMan.StartVehicleWreckedFire(pVehicle, vBonePosLcl, 1, pCulprit, burnTime, maxRadius, numGenerations);
		}
	}

	// create the second additional wrecked fires
	if (pVfxVehicleInfo->GetWreckedFire3PtFxEnabled())
	{
		float burnTime = pVfxVehicleInfo->GetWreckedFire3PtFxDurationMin()+(((pVehicle->GetRandomSeed()&0xff)/255.0f)*(pVfxVehicleInfo->GetWreckedFire3PtFxDurationMax()-pVfxVehicleInfo->GetWreckedFire3PtFxDurationMin()));
		float maxRadius = pVfxVehicleInfo->GetWreckedFire3PtFxRadius();

		Vec3V vBonePosLcl(V_ZERO);
		if (pVfxVehicleInfo->GetWreckedFire2PtFxUseOverheatBone())
		{
			s32 overheatBoneIndex = pVehicle->GetBoneIndex((eHierarchyId)(VEH_OVERHEAT_2));
			if (overheatBoneIndex>-1)
			{
				Mat34V vMtxWld;
				CVfxHelper::GetMatrixFromBoneIndex(vMtxWld, pVehicle, overheatBoneIndex);

				if (IsZeroAll(vMtxWld.GetCol0())==false)
				{
					Mat34V vMtxLcl;
					CVfxHelper::CreateRelativeMat(vMtxLcl, pVehicle->GetTransform().GetMatrix(), vMtxWld);

					vBonePosLcl = vMtxLcl.GetCol3();

					g_fireMan.StartVehicleWreckedFire(pVehicle, vBonePosLcl, 2, pCulprit, burnTime, maxRadius, numGenerations);
				}
			}
		}
		else
		{
			vBonePosLcl = pVfxVehicleInfo->GetWreckedFire3PtFxOffsetPos();
			g_fireMan.StartVehicleWreckedFire(pVehicle, vBonePosLcl, 2, pCulprit, burnTime, maxRadius, numGenerations);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxVehicleDebris
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::TriggerPtFxVehicleDebris				(CEntity* pEntity)
{
	// don't update effect if the vehicle is invisible
	if (!pEntity->GetIsVisible())
	{
		return;
	}

	Vec3V vFxPos = pEntity->GetTransform().GetPosition();
	if (CVfxHelper::GetDistSqrToCamera(vFxPos)>VFXRANGE_VEH_FLAMING_PART_SQR)
	{
		return;
	}

	if (pEntity->GetIsPhysical())
	{
		CPhysical* pPhysical = static_cast<CPhysical*>(pEntity);
		if (pPhysical && pPhysical->GetIsInWater())
		{
			return;
		}
	}

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(atHashWithStringNotFinal("veh_debris_trail",0xDA01E79C));
	if (pFxInst)
	{
		// calc where to play the effect
		phArchetypeDamp* pArch = pEntity->GetPhysArch();
		if (pArch)
		{
			phBound* pBound = pArch->GetBound();
			if (pBound)
			{
				Vec3V_ConstRef vBBMin = pBound->GetBoundingBoxMin();
				Vec3V_ConstRef bBBMax = pBound->GetBoundingBoxMax();

				vFxPos = vBBMin + ((bBBMax-vBBMin)*ScalarV(V_HALF));

				ptfxAttach::Attach(pFxInst, pEntity, -1);

				pFxInst->SetOffsetPos(vFxPos);

				// get the average size of the 2 largest dimensions of the bound box
				Vec3V vBoundVec = pBound->GetBoundingBoxSize();
				float smallest = Min(Min(vBoundVec.GetXf(), vBoundVec.GetYf()), vBoundVec.GetZf());
				float boundSize = vBoundVec.GetXf() + vBoundVec.GetYf() + vBoundVec.GetZf() - smallest;
				boundSize *=0.5f;

				float sizeEvo = CVfxHelper::GetInterpValue(boundSize, 0.0f, 4.0f);
				pFxInst->SetEvoValueFromHash(ATSTRINGHASH("size", 0xC052DEA7), sizeEvo);

				pFxInst->Start();

#if GTA_REPLAY
				if(CReplayMgr::ShouldRecord())
				{
					CReplayMgr::RecordFx<CPacketVehPartFx>(
						CPacketVehPartFx(atHashWithStringNotFinal("veh_debris_trail",0xDA01E79C),
							VEC3V_TO_VECTOR3(vFxPos),
							sizeEvo),
						pEntity);
				}
#endif
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxEjectorSeat
///////////////////////////////////////////////////////////////////////////////

void			CVfxVehicle::TriggerPtFxEjectorSeat				(CVehicle* pVehicle)
{
	// don't update effect if the vehicle is off screen
	if (!pVehicle->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	if (CVfxHelper::GetDistSqrToCamera(pVehicle->GetTransform().GetPosition())>VFXRANGE_VEH_EJECTOR_SEAT_SQR)
	{
		return;
	}

	// get the global matrix of the seat
	int driverSeatIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetDriverSeat();
	int driverSeatBoneIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromSeat(driverSeatIndex);
	if (driverSeatBoneIndex>-1)
	{
		Mat34V seatMtx;
		CVfxHelper::GetMatrixFromBoneIndex(seatMtx, pVehicle, driverSeatBoneIndex);

		// calc the offset matrix
		Mat34V relFxMat;
		CVfxHelper::CreateRelativeMat(relFxMat, pVehicle->GetMatrix(), seatMtx);

		ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(atHashWithStringNotFinal("veh_plane_eject", 0x3e6a17c8));

		if (pFxInst)
		{
			ptfxAttach::Attach(pFxInst, pVehicle, -1);

			pFxInst->SetOffsetPos(relFxMat.GetCol3());

			pFxInst->Start();
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessLowLodWheels
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxVehicle::ProcessLowLodWheels		(CVehicle* pVehicle)
{
	if (pVehicle->GetVehicleType()==VEHICLE_TYPE_BOAT || pVehicle->GetIsRotaryAircraft())
	{
		return;
	}

	// get the speed of the vehicle and return if we're going too slow
	float vehSpeedSqr = pVehicle->GetVelocityIncludingReferenceFrame().Mag2();
	if (vehSpeedSqr<0.01f)
	{
		return;
	}
   
	// check that this vehicle has wheels
	if (pVehicle->GetNumWheels()==0)
	{
		return;
	}

	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
	if (pVfxVehicleInfo==NULL)
	{
		return;
	}

	// get the vfx group we're on
	VfxGroup_e vfxGroup = VFXGROUP_VOID;
	if (pVehicle->IsDummy())
	{
		phMaterialMgr::Id mtlId = pVehicle->GetDummyMtlId();
		phMaterialMgr::Id unpackedMtlId = PGTAMATERIALMGR->UnpackMtlId(mtlId);
		if (vfxVerifyf(unpackedMtlId<PGTAMATERIALMGR->GetNumMaterials(), "vehicle has an invalid dummy material id (%" I64FMT "u - %" I64FMT "u)", mtlId, unpackedMtlId))
		{
			vfxGroup = PGTAMATERIALMGR->GetMtlVfxGroup(mtlId);
		}
	}
	else
	{
		// use data from the first wheel)
		CWheel* pWheel = pVehicle->GetWheel(0);
		if (pWheel==NULL)
		{
			return;
		}

		if (pWheel->GetDynamicFlags().IsFlagSet(WF_HIT_PREV)==false)
		{
			return;
		}

		phMaterialMgr::Id mtlId = pWheel->GetMaterialId();
		phMaterialMgr::Id unpackedMtlId = PGTAMATERIALMGR->UnpackMtlId(mtlId);
		if (vfxVerifyf(unpackedMtlId<PGTAMATERIALMGR->GetNumMaterials(), "vehicle has an invalid wheel material id (%" I64FMT "u - %" I64FMT "u)", mtlId, unpackedMtlId))
		{
			vfxGroup = PGTAMATERIALMGR->GetMtlVfxGroup(mtlId);
		}
	}

	// get the wheel info for this vfx group
	VfxWheelInfo_s* pVfxWheelInfo = g_vfxWheel.GetInfo(VFXTYRESTATE_OK_DRY, vfxGroup);

	// check if we're in range
	float wheelPtFxLodRangeScale = g_vfxWheel.GetWheelPtFxLodRangeScale();
	float rangeSqrHi = VFXRANGE_WHEEL_DISPLACEMENT_HI * pVfxVehicleInfo->GetWheelGenericRangeMult() * wheelPtFxLodRangeScale;
	rangeSqrHi *= rangeSqrHi;
	float rangeSqrLo = VFXRANGE_WHEEL_DISPLACEMENT_LO * pVfxVehicleInfo->GetWheelGenericRangeMult() * wheelPtFxLodRangeScale;
	rangeSqrLo *= rangeSqrLo;
	Vec3V vVehiclePos = pVehicle->GetTransform().GetPosition();
	float distSqrToCam = CVfxHelper::GetDistSqrToCamera(vVehiclePos);
	if (distSqrToCam<rangeSqrHi || 
		distSqrToCam>rangeSqrLo)
	{
		return;
	}

	// calc if we're going forwards or backwards
	bool travellingForwards = DotProduct(pVehicle->GetVelocityIncludingReferenceFrame(), VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()))>0.0f;

	// get the vehicle bounding box info
	Vec3V vBBMin = Vec3V(V_ZERO);
	Vec3V vBBMax = Vec3V(V_ZERO);
	phArchetypeDamp* pArch = pVehicle->GetPhysArch();
	if (pArch)
	{
		phBound* pBound = pArch->GetBound();
		if (pBound)
		{
			vBBMin = pBound->GetBoundingBoxMin();
			vBBMax = pBound->GetBoundingBoxMax();
		}
	}

	// get the position of the effect
	Vec3V vPosLcl = Vec3V(V_ZERO);
	if (travellingForwards)
	{
		// forwards
		vPosLcl.SetY(vBBMin.GetY());
		vPosLcl.SetZ(vBBMin.GetZ());
	}
	else
	{
		// reverse
		vPosLcl.SetY(vBBMax.GetY());
		vPosLcl.SetZ(vBBMin.GetZ());
	}

	UpdatePtFxLowLodWheels(pVehicle, pVfxWheelInfo, vPosLcl, Sqrtf(vehSpeedSqr), travellingForwards);
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxLowLodWheels
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxVehicle::UpdatePtFxLowLodWheels		(CVehicle* pVehicle, VfxWheelInfo_s* pVfxWheelInfo, Vec3V_In vPosLcl, float vehSpeed, bool travellingForwards)
{
	// check that we have an effect to play
	atHashWithStringNotFinal ptFxHashName = pVfxWheelInfo->ptFxWheelDispLodHashName;
	if (ptFxHashName.GetHash()==0)
	{
		return;
	}

	int classFxOffset;
	if (travellingForwards)
	{
		classFxOffset = PTFXOFFSET_VEHICLE_WHEEL_DISP_LOD_REAR;
	}
	else
	{
		classFxOffset = PTFXOFFSET_VEHICLE_WHEEL_DISP_LOD_FRONT;
	}

	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pVehicle, classFxOffset, true, ptFxHashName, justCreated);

	// check the effect exists
	if (pFxInst)
	{
		// set the offset matrix
		pFxInst->SetOffsetPos(vPosLcl);

		float dispEvo = CVfxHelper::GetInterpValue(vehSpeed, pVfxWheelInfo->ptFxWheelDispThreshMin, pVfxWheelInfo->ptFxWheelDispThreshMax);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("groundspeed", 0x89A858E5), dispEvo);

		// check if the effect has just been created 
		if (justCreated)
		{		
			ptfxAttach::Attach(pFxInst, pVehicle, -1);

			// flip the y axis when traveling backwards
			if (travellingForwards==false)
			{
				pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_Y);
			}

			// set zoom
			CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
			if (pVfxVehicleInfo)
			{
				pFxInst->SetUserZoom(pVfxVehicleInfo->GetWheelLowLodPtFxScale());
			}

			// set colour tint
			u8 colR = pVehicle->GetVariationInstance().GetSmokeColorR();
			u8 colG = pVehicle->GetVariationInstance().GetSmokeColorG();
			u8 colB = pVehicle->GetVariationInstance().GetSmokeColorB();
			ptfxWrapper::SetColourTint(pFxInst, Vec3V(colR/255.0f, colG/255.0f, colB/255.0f));

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
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessLowLodBoatWake
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxVehicle::ProcessLowLodBoatWake		(CBoat* pBoat)
{
	// get the speed of the boat and return if we're going too slow
	float vehSpeedSqr = pBoat->GetVelocity().Mag2();
	if (vehSpeedSqr<0.01f)
	{
		return;
	}

	// get the vfx vehicle info
	CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pBoat);
	if (pVfxVehicleInfo==NULL || pVfxVehicleInfo->GetBoatLowLodWakePtFxEnabled()==false)
	{
		return;
	}

	// check if we're in range
	Vec3V vVehiclePos = pBoat->GetTransform().GetPosition();
	float distSqrToCam = CVfxHelper::GetDistSqrToCamera(vVehiclePos);
	if (distSqrToCam<pVfxVehicleInfo->GetBoatLowLodWakePtFxRangeMinSqr() || 
		distSqrToCam>pVfxVehicleInfo->GetBoatLowLodWakePtFxRangeMaxSqr())
	{
		return;
	}

	// calc if we're going forwards or backwards
	bool travellingForwards = DotProduct(pBoat->GetVelocity(), VEC3V_TO_VECTOR3(pBoat->GetTransform().GetB()))>0.0f;

	// get the vehicle bounding box info
	Vec3V vBBMin = Vec3V(V_ZERO);
	Vec3V vBBMax = Vec3V(V_ZERO);
	phArchetypeDamp* pArch = pBoat->GetPhysArch();
	if (pArch)
	{
		phBound* pBound = pArch->GetBound();
		if (pBound)
		{
			vBBMin = pBound->GetBoundingBoxMin();
			vBBMax = pBound->GetBoundingBoxMax();
		}
	}

	// get the position of the effect
	Vec3V vPosLcl = Vec3V(V_ZERO);
	if (travellingForwards)
	{
		// forwards
		vPosLcl.SetY(vBBMin.GetY());
		vPosLcl.SetZ(vBBMin.GetZ());
	}
	else
	{
		// reverse
		vPosLcl.SetY(vBBMax.GetY());
		vPosLcl.SetZ(vBBMin.GetZ());
	}

	UpdatePtFxLowLodBoatWake(pBoat, pVfxVehicleInfo, vPosLcl, Sqrtf(vehSpeedSqr), travellingForwards);
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxLowLodBoatWake
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxVehicle::UpdatePtFxLowLodBoatWake		(CBoat* pBoat, CVfxVehicleInfo* pVfxVehicleInfo, Vec3V_In vPosLcl, float vehSpeed, bool travellingForwards)
{
	// check that we have an effect to play
	atHashWithStringNotFinal ptFxHashName = pVfxVehicleInfo->GetBoatLowLodWakePtFxName();
	if (ptFxHashName.GetHash()==0)
	{
		return;
	}

	int classFxOffset;
	if (travellingForwards)
	{
		classFxOffset = PTFXOFFSET_VEHICLE_BOAT_WAKE_LOD_REAR;
	}
	else
	{
		classFxOffset = PTFXOFFSET_VEHICLE_BOAT_WAKE_LOD_FRONT;
	}

	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pBoat, classFxOffset, true, ptFxHashName, justCreated);

	// check the effect exists
	if (pFxInst)
	{
		// set the offset matrix
		pFxInst->SetOffsetPos(vPosLcl);

		float speedEvo = CVfxHelper::GetInterpValue(vehSpeed, pVfxVehicleInfo->GetBoatLowLodWakePtFxForwardSpeedEvoMin(), pVfxVehicleInfo->GetBoatLowLodWakePtFxForwardSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		// check if the effect has just been created 
		if (justCreated)
		{		
			ptfxAttach::Attach(pFxInst, pBoat, -1);

			// flip the y axis when traveling backwards
			if (travellingForwards==false)
			{
				pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_Y);
			}

			// set zoom
			CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pBoat);
			if (pVfxVehicleInfo)
			{
				pFxInst->SetUserZoom(pVfxVehicleInfo->GetBoatLowLodWakePtFxScale());
			}

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
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetRecentlyDamagedByPlayer
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxVehicle::SetRecentlyDamagedByPlayer	(CVehicle* pVehicle)
{
	// find where to add this vehicle
	int index = -1;
	bool foundEmptySlot = false;
	u32 currTimeMs = fwTimer::GetTimeInMilliseconds();
	u32 lowestTimeFound = currTimeMs;
	for (int i=0; i<VFXVEHICLE_MAX_RECENTLY_DAMAGED_BY_PLAYER_INFOS; i++)
	{
		// if the vehicle is already stored then we just need to update it
		if (m_recentlyDamagedByPlayerInfos[i].m_regdVeh==pVehicle)
		{
			index = i; 
			break;
		}
		else
		{
			// otherwise we need to search for an empty slot of the slot used least recently
			if (!foundEmptySlot)
			{
				if (m_recentlyDamagedByPlayerInfos[i].m_regdVeh==NULL)
				{
					index = i;
					foundEmptySlot = true;
				}
				else
				{
					if (m_recentlyDamagedByPlayerInfos[i].m_timeLastDamagedMs<lowestTimeFound)
					{
						index = i;
						lowestTimeFound = m_recentlyDamagedByPlayerInfos[i].m_timeLastDamagedMs;
					}
				}
			}
		}
	}

	if (vfxVerifyf(index<VFXVEHICLE_MAX_RECENTLY_DAMAGED_BY_PLAYER_INFOS, "invalid recently damaged by player index calculated"))
	{
		// the index may be -1 if we've filled this entire array up this frame and then try to add another
		// this is because we will never find an entry with a lower time than the current time
		if (index>-1)
		{
			m_recentlyDamagedByPlayerInfos[index].m_regdVeh = pVehicle;
			m_recentlyDamagedByPlayerInfos[index].m_timeLastDamagedMs = currTimeMs;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  HasRecentlyBeenDamagedByPlayer
///////////////////////////////////////////////////////////////////////////////

bool		 	CVfxVehicle::HasRecentlyBeenDamagedByPlayer	(const CVehicle* pVehicle)
{
	for (int i=0; i<VFXVEHICLE_MAX_RECENTLY_DAMAGED_BY_PLAYER_INFOS; i++)
	{
		if (m_recentlyDamagedByPlayerInfos[i].m_regdVeh==pVehicle)
		{
			return true;
		}
	}
	
	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  SetRecentlyCrashed
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxVehicle::SetRecentlyCrashed				(CVehicle* pVehicle, float damageApplied)
{
	// find where to add this vehicle
	int index = -1;
	bool foundEmptySlot = false;
	u32 currTimeMs = fwTimer::GetTimeInMilliseconds();
	u32 lowestTimeFound = currTimeMs;
	for (int i=0; i<VFXVEHICLE_MAX_RECENTLY_CRASHED_INFOS; i++)
	{
		// if the vehicle is already stored then we just need to update it
		if (m_recentlyCrashedInfos[i].m_regdVeh==pVehicle)
		{
			index = i; 
			break;
		}
		else
		{
			// otherwise we need to search for an empty slot of the slot used least recently
			if (!foundEmptySlot)
			{
				if (m_recentlyCrashedInfos[i].m_regdVeh==NULL)
				{
					index = i;
					foundEmptySlot = true;
				}
				else
				{
					if (m_recentlyCrashedInfos[i].m_timeLastCrashedMs<lowestTimeFound)
					{
						index = i;
						lowestTimeFound = m_recentlyCrashedInfos[i].m_timeLastCrashedMs;
					}
				}
			}
		}
	}

	if (vfxVerifyf(index<VFXVEHICLE_MAX_RECENTLY_CRASHED_INFOS, "invalid recently damaged by player index calculated"))
	{
		// the index may be -1 if we've filled this entire array up this frame and then try to add another
		// this is because we will never find an entry with a lower time than the current time
		if (index>-1)
		{
			m_recentlyCrashedInfos[index].m_regdVeh = pVehicle;
			m_recentlyCrashedInfos[index].m_timeLastCrashedMs = currTimeMs;
			m_recentlyCrashedInfos[index].m_damageApplied = damageApplied;
			m_recentlyCrashedInfos[index].m_petrolIgniteTestDone = false;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  HasRecentlyCrashed
///////////////////////////////////////////////////////////////////////////////

bool		 	CVfxVehicle::HasRecentlyCrashed				(const CVehicle* pVehicle, u32 timeMs, float damage)
{
	for (int i=0; i<VFXVEHICLE_MAX_RECENTLY_CRASHED_INFOS; i++)
	{
		if (m_recentlyCrashedInfos[i].m_regdVeh==pVehicle)
		{
			u32 currTimeMs = fwTimer::GetTimeInMilliseconds();
			if (m_recentlyCrashedInfos[i].m_petrolIgniteTestDone==false && 
				m_recentlyCrashedInfos[i].m_damageApplied>=damage &&
				m_recentlyCrashedInfos[i].m_timeLastCrashedMs+timeMs>currTimeMs)
			{
				m_recentlyCrashedInfos[i].m_petrolIgniteTestDone = true;
				return true;
			}
		}
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void		 	CVfxVehicle::InitWidgets				()
{
	if (m_bankInitialised==false)
	{	

		bkBank* pVfxBank = vfxWidget::GetBank();
		pVfxBank->PushGroup("Vfx Vehicle", false);
		{
#if __DEV
			pVfxBank->AddTitle("SETTINGS");
			pVfxBank->AddSlider("Dummy Mtl Id Probe Frame Delay", &VFXVEHICLE_DUMMY_MTL_ID_PROBE_FRAME_DELAY, 0, 30, 1, NullCB, "The number of frames delay between a dummy mtl probe finishing and the next one starting");
			pVfxBank->AddSlider("Light Trail Speed Evo Min", &VFXVEHICLE_LIGHT_TRAIL_SPEED_EVO_MIN, 0.0f, 50.0f, 1, NullCB, "");
			pVfxBank->AddSlider("Light Trail Speed Evo Max", &VFXVEHICLE_LIGHT_TRAIL_SPEED_EVO_MAX, 0.0f, 50.0f, 1, NullCB, "");

			pVfxBank->AddSlider("Recently Crashed - Damage Thresh", &VFXVEHICLE_RECENTLY_CRASHED_DAMAGE_THRESH, 0.0f, 200.0f, 0.5f, NullCB, "");
			
			pVfxBank->AddSlider("Petrol Ignite - Crash Timer", &VFXVEHICLE_PETROL_IGNITE_CRASH_TIMER, 0, 10000, 1, NullCB, "");
			pVfxBank->AddSlider("Petrol Ignite - Damage Thresh", &VFXVEHICLE_PETROL_IGNITE_DAMAGE_THRESH, 0.0f, 200.0f, 0.5f, NullCB, "");
			pVfxBank->AddSlider("Petrol Ignite - Speed Thresh", &VFXVEHICLE_PETROL_IGNITE_SPEED_THRESH, 0.0f, 10.0f, 0.05f, NullCB, "");
			pVfxBank->AddSlider("Petrol Ignite - Range A", &VFXVEHICLE_PETROL_IGNITE_RANGE, 0.0f, 30.0f, 0.1f, NullCB, "");
			pVfxBank->AddSlider("Petrol Ignite - Probability", &VFXVEHICLE_PETROL_IGNITE_PROBABILITY, 0.0f, 1.0f, 0.005f, NullCB, "");

			pVfxBank->AddSlider("Wrecked Fires - Fade Thresh", &VFXVEHICLE_NUM_WRECKED_FIRES_FADE_THRESH, 0, 10, 1, NullCB, "");
			pVfxBank->AddSlider("Wrecked Fires - Reject Thresh", &VFXVEHICLE_NUM_WRECKED_FIRES_REJECT_THRESH, 0, 10, 1, NullCB, "");
#endif

			pVfxBank->AddSlider("Slipstream Lod/Range Scale", &m_slipstreamPtFxLodRangeScale, 0.0f, 20.0f, 0.5f);

			pVfxBank->AddTitle("");
			pVfxBank->AddTitle("DEBUG");
			pVfxBank->AddToggle("Render Downwash Vfx Probes", &m_renderDownwashVfxProbes);
			pVfxBank->AddToggle("Render Plane Ground Disturbance Vfx Probes", &m_renderPlaneGroundDisturbVfxProbes);
			pVfxBank->AddToggle("Render Dummy Mtl Id Vfx Probes", &m_renderDummyMtlIdVfxProbes);
			pVfxBank->AddSlider("Override Train Squeal", &m_trainSquealOverride, 0.0f, 1.0f, 0.005f);
		}
		pVfxBank->PopGroup();

		m_bankInitialised = true;
	}
}
#endif




