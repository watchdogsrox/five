///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxExplosion.cpp
//	BY	: 	Mark Nicholson 
//	FOR	:	Rockstar North
//	ON	:	24 February 2010
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxExplosion.h"

// rage
#include "system/param.h"
#include "rmptfx/ptxeffectinst.h"

// framework
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "vfx/ptfx/ptfxhistory.h"
#include "vfx/vfxwidget.h"

// game
#include "Camera/CamInterface.h"
#include "control/replay/Replay.h"
#include "control/replay/effects/ParticleWeaponFxPacket.h"
#include "control/replay/effects/ParticleFireFxPacket.h"
#include "Core/Game.h"
#include "Peds/PlayerInfo.h"
#include "Peds/Ped.h"
#include "Physics/Physics.h"
#include "Renderer/Water.h"
#include "Scene/DataFileMgr.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Misc/Fire.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/Systems/VfxBlood.h"
#include "Vfx/Systems/VfxWeapon.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "Vfx/VfxHelper.h"
#include "Weapons/ExplosionInst.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  ENUMERATIONS
///////////////////////////////////////////////////////////////////////////////

// explosion ptfx offsets for registered systems 
enum PtFxExplosionOffsets_e
{
	PTFXOFFSET_EXPINST_EXPLOSION			= 0,
	PTFXOFFSET_EXPINST_EXPLOSION_2
};


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

dev_float	VFXEXPLOSION_HIST_DIST[3]				= { 5.0f, 10.0f, 20.0f};
dev_float	VFXEXPLOSION_HIST_TIME[3]				= { 1.0f,  2.0f,  5.0f};


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CVfxExplosion		g_vfxExplosion;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxExplosionAsyncProbeManager
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxExplosionAsyncProbeManager::Init			()
{
	// init the fire probes
	for (int i=0; i<VFXEXPLOSION_MAX_FIRE_PROBES; i++)
	{
		m_fireProbes[i].m_vfxProbeId = -1;
	}

	// init the timed scorch probes
	for (int i=0; i<VFXEXPLOSION_MAX_TIMED_SCORCH_PROBES; i++)
	{
		m_timedScorchProbes[i].m_vfxProbeId = -1;
	}

	// init the non-timed scorch probes
	for (int i=0; i<VFXEXPLOSION_MAX_NONTIMED_SCORCH_PROBES; i++)
	{
		m_nonTimedScorchProbes[i].m_isActive = false;
		for (int j=0; j<5; j++)
		{
			m_nonTimedScorchProbes[i].m_vfxProbeIds[j] = -1;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxExplosionAsyncProbeManager::Shutdown		()
{
	// shutdown the pool probes
	for (int i=0; i<VFXEXPLOSION_MAX_FIRE_PROBES; i++)
	{
		if (m_fireProbes[i].m_vfxProbeId>=0)
		{
			CVfxAsyncProbeManager::AbortProbe(m_fireProbes[i].m_vfxProbeId);
			m_fireProbes[i].m_vfxProbeId = -1;
		}
	}

	// shutdown the timed scorch probes
	for (int i=0; i<VFXEXPLOSION_MAX_TIMED_SCORCH_PROBES; i++)
	{
		if (m_timedScorchProbes[i].m_vfxProbeId>=0)
		{
			CVfxAsyncProbeManager::AbortProbe(m_timedScorchProbes[i].m_vfxProbeId);
			m_timedScorchProbes[i].m_vfxProbeId = -1;
		}
	}

	// shutdown the non-timed scorch probes
	for (int i=0; i<VFXEXPLOSION_MAX_NONTIMED_SCORCH_PROBES; i++)
	{
		m_nonTimedScorchProbes[i].m_isActive = false;
		for (int j=0; j<5; j++)
		{
			if (m_nonTimedScorchProbes[i].m_vfxProbeIds[j]>=0)
			{
				CVfxAsyncProbeManager::AbortProbe(m_nonTimedScorchProbes[i].m_vfxProbeIds[j]);
				m_nonTimedScorchProbes[i].m_vfxProbeIds[j] = -1;
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxExplosionAsyncProbeManager::Update			()
{
	// update the fire probes
	for (int i=0; i<VFXEXPLOSION_MAX_FIRE_PROBES; i++)
	{
		if (m_fireProbes[i].m_vfxProbeId>=0)
		{
			// query the probe
			VfxAsyncProbeResults_s vfxProbeResults;
			if (CVfxAsyncProbeManager::QueryProbe(m_fireProbes[i].m_vfxProbeId, vfxProbeResults))
			{
				// the probe has finished
				m_fireProbes[i].m_vfxProbeId = -1;

				// check if the probe intersected
				if (vfxProbeResults.hasIntersected)
				{
					// only process entity fires on the main component if this is a frag (as fires can't be attached to bones - only entities)
					CEntity* pHitEntity = CVfxAsyncProbeManager::GetEntity(vfxProbeResults);
					if (pHitEntity)
					{
						if (pHitEntity->GetFragInst())
						{
							if (pHitEntity->GetFragInst()->GetTypePhysics()->GetAllChildren()[vfxProbeResults.componentId]->GetOwnerGroupPointerIndex() != 0)
							{
								continue;
							}
						}

						float zDist = rage::Abs(vfxProbeResults.vPos.GetZf() - m_fireProbes[i].m_vExplosionPos.GetZf());

						if (zDist<10.0f) 
						{
							CPed* pFireCulprit = NULL;
							if (m_fireProbes[i].m_regdExplosionOwner)
							{
								if (m_fireProbes[i].m_regdExplosionOwner->GetIsTypePed())
								{
									CEntity* pExplosionOwner = m_fireProbes[i].m_regdExplosionOwner;
									pFireCulprit = static_cast<CPed*>(pExplosionOwner);
								}
								else if (m_fireProbes[i].m_regdExplosionOwner->GetIsTypeVehicle())
								{
									CEntity* pExplosionOwner = m_fireProbes[i].m_regdExplosionOwner;
									CVehicle* pExplosionOwnerVehicle = static_cast<CVehicle*>(pExplosionOwner);
									pFireCulprit = static_cast<CPed*>(pExplosionOwnerVehicle->GetDriver());
								}
							}

							if (pHitEntity->GetIsTypeVehicle())
							{
								CVehicle* pHitVehicle = static_cast<CVehicle*>(pHitEntity);
								fragInstGta* pVehicleFragInst = pHitVehicle->GetVehicleFragInst();
								if (pVehicleFragInst)
								{
									fragPhysicsLOD* pPhysicsLod = pVehicleFragInst->GetTypePhysics();
									if (pPhysicsLod)
									{
										fragTypeChild* pFragTypeChild = pPhysicsLod->GetChild(vfxProbeResults.componentId);
										if (pFragTypeChild)
										{
											u16 boneId = pFragTypeChild->GetBoneID();

											CVehicleModelInfo* pModelInfo = pHitVehicle->GetVehicleModelInfo(); 
											if (pModelInfo)
											{
												gtaFragType* pFragType = pModelInfo->GetFragType();
												if (pFragType)
												{
													s32 boneIndex = pFragType->GetBoneIndexFromID(boneId);

													Vec3V vPosLcl;
													Vec3V vNormLcl;
													if (CVfxHelper::GetLocalEntityBonePosDir(*pHitVehicle, boneIndex, vfxProbeResults.vPos, vfxProbeResults.vNormal, vPosLcl, vNormLcl))
													{
														g_fireMan.StartVehicleFire(pHitVehicle, boneIndex, vPosLcl, vNormLcl, pFireCulprit, m_fireProbes[i].m_burnTime, m_fireProbes[i].m_burnStrength, m_fireProbes[i].m_peakTime, FIRE_DEFAULT_NUM_GENERATIONS, m_fireProbes[i].m_vExplosionPos, BANK_ONLY(m_fireProbes[i].m_vExplosionPos,) false, m_fireProbes[i].m_weaponHash);
													}
												}
											}
										}
									}
								}
							}
							else if (pHitEntity->GetIsTypeObject())
							{
								CObject* pHitObject = static_cast<CObject*>(pHitEntity);
								
								s32 boneIndex = -1;
								fragInst* pFragInst = pHitObject->GetFragInst();
								if (pFragInst)
								{
									fragPhysicsLOD* pPhysicsLod = pFragInst->GetTypePhysics();
									if (pPhysicsLod)
									{
										fragTypeChild* pFragTypeChild = pPhysicsLod->GetChild(vfxProbeResults.componentId);
										if (pFragTypeChild)
										{
											u16 boneId = pFragTypeChild->GetBoneID();

											CBaseModelInfo* pModelInfo = pHitObject->GetBaseModelInfo(); 
											if (pModelInfo)
											{
												gtaFragType* pFragType = pModelInfo->GetFragType();
												if (pFragType)
												{
													boneIndex = pFragType->GetBoneIndexFromID(boneId);
												}
											}
										}
									}
								}

								Vec3V vPosLcl;
								Vec3V vNormLcl;
								if (CVfxHelper::GetLocalEntityBonePosDir(*pHitObject, boneIndex, vfxProbeResults.vPos, vfxProbeResults.vNormal, vPosLcl, vNormLcl))
								{
									g_fireMan.StartObjectFire(pHitObject, boneIndex, vPosLcl, vNormLcl, pFireCulprit, m_fireProbes[i].m_burnTime, m_fireProbes[i].m_burnStrength, m_fireProbes[i].m_peakTime, FIRE_DEFAULT_NUM_GENERATIONS, m_fireProbes[i].m_vExplosionPos, BANK_ONLY(m_fireProbes[i].m_vExplosionPos,) false, m_fireProbes[i].m_weaponHash);
								}
							}
							else
							{
								g_fireMan.StartMapFire(vfxProbeResults.vPos, vfxProbeResults.vNormal, vfxProbeResults.mtlId, pFireCulprit, m_fireProbes[i].m_burnTime, m_fireProbes[i].m_burnStrength, m_fireProbes[i].m_peakTime, m_fireProbes[i].m_fuelLevel, m_fireProbes[i].m_fuelBurnRate, FIRE_DEFAULT_NUM_GENERATIONS, m_fireProbes[i].m_vExplosionPos, BANK_ONLY(m_fireProbes[i].m_vExplosionPos,) false, m_fireProbes[i].m_weaponHash);
							}
						}
					}
				}
			}
		}
	}

	// update the timed scorch probes
	for (int i=0; i<VFXEXPLOSION_MAX_TIMED_SCORCH_PROBES; i++)
	{
		if (m_timedScorchProbes[i].m_vfxProbeId>=0)
		{
			// query the probe
			VfxAsyncProbeResults_s vfxProbeResults;
			if (CVfxAsyncProbeManager::QueryProbe(m_timedScorchProbes[i].m_vfxProbeId, vfxProbeResults))
			{
				// the probe has finished
				m_timedScorchProbes[i].m_vfxProbeId = -1;

				// check if the probe intersected
				if (vfxProbeResults.hasIntersected)
				{
					CEntity* pHitEntity = CVfxAsyncProbeManager::GetEntity(vfxProbeResults);
					if (pHitEntity)
					{
						// set up the collision info structure
						VfxCollInfo_s vfxCollInfo;
						vfxCollInfo.regdEnt = pHitEntity;
						vfxCollInfo.vPositionWld = vfxProbeResults.vPos;
						vfxCollInfo.vNormalWld = vfxProbeResults.vNormal;
						vfxCollInfo.vDirectionWld = -vfxCollInfo.vNormalWld;
						vfxCollInfo.materialId = PGTAMATERIALMGR->UnpackMtlId(vfxProbeResults.mtlId);
						vfxCollInfo.roomId = PGTAMATERIALMGR->UnpackRoomId(vfxProbeResults.mtlId);
						vfxCollInfo.componentId = vfxProbeResults.componentId;
						vfxCollInfo.weaponGroup = m_timedScorchProbes[i].m_weaponEffectGroup;
						vfxCollInfo.force = VEHICLEGLASSFORCE_NOT_USED;
						vfxCollInfo.isBloody = false;
						vfxCollInfo.isWater = false;
						vfxCollInfo.isExitFx = false;
						vfxCollInfo.noPtFx = false;
						vfxCollInfo.noPedDamage = false;
						vfxCollInfo.noDecal = false;
						vfxCollInfo.isSnowball = false;
						vfxCollInfo.isFMJAmmo = false;

						// play any weapon impact effects
						g_vfxWeapon.DoWeaponImpactVfx(vfxCollInfo, DAMAGE_TYPE_EXPLOSIVE, m_timedScorchProbes[i].m_regdExplosionOwner);
					}
				}
			}
		}
	}

	// update the non-timed scorch probes
	for (int i=0; i<VFXEXPLOSION_MAX_NONTIMED_SCORCH_PROBES; i++)
	{
		if (m_nonTimedScorchProbes[i].m_isActive)
		{
			// query the probes
			int numFinishedProbes = 0;
			for (int j=0; j<5; j++)
			{
				// check if this probe is still active
				if (m_nonTimedScorchProbes[i].m_vfxProbeIds[j] > -1)
				{
					// it is - check if it's finished
					VfxAsyncProbeResults_s vfxProbeResults;
					if (CVfxAsyncProbeManager::QueryProbe(m_nonTimedScorchProbes[i].m_vfxProbeIds[j], vfxProbeResults))
					{
						// the probe has finished
						numFinishedProbes++;

						m_nonTimedScorchProbes[i].m_vfxProbeIds[j] = -1;

						// check if the probe intersected
						if (vfxProbeResults.hasIntersected)
						{
							CEntity* pHitEntity = CVfxAsyncProbeManager::GetEntity(vfxProbeResults);
							if (pHitEntity)
							{
								// set up the collision info structure
								VfxCollInfo_s vfxCollInfo;
								vfxCollInfo.regdEnt = pHitEntity;
								vfxCollInfo.vPositionWld = vfxProbeResults.vPos;
								vfxCollInfo.vNormalWld = vfxProbeResults.vNormal;
								vfxCollInfo.vDirectionWld = -vfxCollInfo.vNormalWld;
								vfxCollInfo.materialId = PGTAMATERIALMGR->UnpackMtlId(vfxProbeResults.mtlId);
								vfxCollInfo.roomId = PGTAMATERIALMGR->UnpackRoomId(vfxProbeResults.mtlId);
								vfxCollInfo.componentId = vfxProbeResults.componentId;
								vfxCollInfo.weaponGroup = m_nonTimedScorchProbes[i].m_expInstData.m_weaponEffectGroup;
								vfxCollInfo.force = VEHICLEGLASSFORCE_NOT_USED;
								vfxCollInfo.isBloody = false;
								vfxCollInfo.isWater = false;
								vfxCollInfo.isExitFx = false;
								vfxCollInfo.noPtFx = false;
								vfxCollInfo.noPedDamage = false;
								vfxCollInfo.noDecal = false;
								vfxCollInfo.isSnowball = false;
								vfxCollInfo.isFMJAmmo = false;

								// play any weapon impact effects
								VfxExplosionInfo_s* pExpInfo = m_nonTimedScorchProbes[i].m_expInstData.m_pVfxExpInfo;
								if (pExpInfo && pExpInfo->createsScorchDecals)
								{
									g_vfxWeapon.DoWeaponImpactVfx(vfxCollInfo, DAMAGE_TYPE_EXPLOSIVE, m_nonTimedScorchProbes[i].m_expInstData.m_regdOwner);
								}

								// update the global data
								float distSqr = DistSquared(vfxProbeResults.vPos, m_nonTimedScorchProbes[i].m_expInstData.m_vPosWld).Getf();
								if (distSqr<m_nonTimedScorchProbes[i].m_closestDistSqr)
								{
									m_nonTimedScorchProbes[i].m_vClosestColnPos = vfxProbeResults.vPos;
									m_nonTimedScorchProbes[i].m_vClosestColnNormal = vfxProbeResults.vNormal;
									m_nonTimedScorchProbes[i].m_closestDistSqr = distSqr;
								}

#if __BANK
								if (g_vfxExplosion.m_renderExplosionNonTimedScorchProbeResults)
								{
									grcDebugDraw::Line(vfxProbeResults.vPos, vfxProbeResults.vPos+vfxProbeResults.vNormal, Color32(0.0f, 1.0f, 0.0f, 1.0f), Color32(1.0f, 1.0f, 1.0f, 1.0f), -200);
								}
#endif
							}
						}
					}
				}
				else
				{
					// probe has already finished
					numFinishedProbes++;
				}
			}

			// check if all probes have finished
			if (numFinishedProbes==5)
			{
				// they have - trigger the explosion and fires
				VfxExplosionInfo_s* pExpInfo = m_nonTimedScorchProbes[i].m_expInstData.m_pVfxExpInfo;
				if (pExpInfo)
				{
#if __BANK
					if (g_vfxExplosion.m_renderExplosionNonTimedScorchProbeResults)
					{
						grcDebugDraw::Line(m_nonTimedScorchProbes[i].m_vClosestColnPos, m_nonTimedScorchProbes[i].m_vClosestColnPos+m_nonTimedScorchProbes[i].m_vClosestColnNormal, Color32(1.0f, 0.0f, 0.0f, 1.0f), Color32(1.0f, 1.0f, 1.0f, 1.0f), -200);
					}
#endif
					g_vfxExplosion.TriggerPtFxExplosion(m_nonTimedScorchProbes[i].m_expInstData, pExpInfo, m_nonTimedScorchProbes[i].m_vClosestColnPos, m_nonTimedScorchProbes[i].m_vClosestColnNormal);
				}

				m_nonTimedScorchProbes[i].m_isActive = false;
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerFireProbe
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxExplosionAsyncProbeManager::TriggerFireProbe(Vec3V_In vStartPos, Vec3V_In vEndPos, Vec3V_In vExplosionPos, CEntity* pExplosionOwner, float burnTime, float burnStrength, float peakTime, float fuelLevel, float fuelBurnRate, u32 weaponHash)
{
	static u32 probeFlags = ArchetypeFlags::GTA_MAP_TYPE_WEAPON | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE;

	// look for an available probe
	for (int i=0; i<VFXEXPLOSION_MAX_FIRE_PROBES; i++)
	{
		if (m_fireProbes[i].m_vfxProbeId==-1)
		{
			m_fireProbes[i].m_vfxProbeId = CVfxAsyncProbeManager::SubmitWorldProbe(vStartPos, vEndPos, probeFlags);
			m_fireProbes[i].m_vExplosionPos = vExplosionPos;
			m_fireProbes[i].m_regdExplosionOwner = pExplosionOwner;
			m_fireProbes[i].m_burnTime = burnTime;
			m_fireProbes[i].m_burnStrength = burnStrength;
			m_fireProbes[i].m_peakTime = peakTime;
			m_fireProbes[i].m_fuelLevel = fuelLevel;
			m_fireProbes[i].m_fuelBurnRate = fuelBurnRate;
			m_fireProbes[i].m_weaponHash = weaponHash;
							
#if __BANK
			if (g_vfxExplosion.m_renderExplosionFireProbes)
			{
				grcDebugDraw::Line(vStartPos, vEndPos, Color32(1.0f, 0.5f, 0.0f, 1.0f), Color32(1.0f, 1.0f, 1.0f, 1.0f), -200);
			}
#endif

			return;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerTimedScorchProbe
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxExplosionAsyncProbeManager::TriggerTimedScorchProbe(Vec3V_In vStartPos, Vec3V_In vEndPos, CEntity* pExplosionOwner, eWeaponEffectGroup weaponEffectGroup)
{
	static u32 probeFlags = ArchetypeFlags::GTA_MAP_TYPE_WEAPON;

	// look for an available probe
	for (int i=0; i<VFXEXPLOSION_MAX_TIMED_SCORCH_PROBES; i++)
	{
		if (m_timedScorchProbes[i].m_vfxProbeId==-1)
		{
			m_timedScorchProbes[i].m_vfxProbeId = CVfxAsyncProbeManager::SubmitWorldProbe(vStartPos, vEndPos, probeFlags);
			m_timedScorchProbes[i].m_regdExplosionOwner = pExplosionOwner;
			m_timedScorchProbes[i].m_weaponEffectGroup = weaponEffectGroup;

#if __BANK
			if (g_vfxExplosion.m_renderExplosionTimedScorchProbes)
			{
				grcDebugDraw::Line(vStartPos, vEndPos, Color32(1.0f, 0.5f, 0.0f, 1.0f), Color32(1.0f, 1.0f, 1.0f, 1.0f), -200);
			}
#endif

			return;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerNonTimedScorchProbes
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxExplosionAsyncProbeManager::TriggerNonTimedScorchProbes(phGtaExplosionInst* pExpInst, VfxExplosionInfo_s* pVfxExpInfo)
{
	static u32 probeFlags = ArchetypeFlags::GTA_MAP_TYPE_WEAPON;

	// look for an available probe
	for (int i=0; i<VFXEXPLOSION_MAX_NONTIMED_SCORCH_PROBES; i++)
	{
		if (m_nonTimedScorchProbes[i].m_isActive==false)
		{
			// cache some data
			Vec3V vExpPosWld = pExpInst->GetPosWld();
			Vec3V vExpDirWld = pExpInst->GetDirWld();

			// initialise the global data
			m_nonTimedScorchProbes[i].m_isActive = true;

			m_nonTimedScorchProbes[i].m_vClosestColnPos = vExpPosWld;
			m_nonTimedScorchProbes[i].m_vClosestColnNormal = vExpDirWld;
			m_nonTimedScorchProbes[i].m_closestDistSqr = FLT_MAX;

			m_nonTimedScorchProbes[i].m_expInstData.m_vPosWld = pExpInst->GetPosWld();
			m_nonTimedScorchProbes[i].m_expInstData.m_vPosLcl = pExpInst->GetPosLcl();
			m_nonTimedScorchProbes[i].m_expInstData.m_vDirLcl = pExpInst->GetDirLcl();
			m_nonTimedScorchProbes[i].m_expInstData.m_regdOwner = pExpInst->GetExplosionOwner();
			m_nonTimedScorchProbes[i].m_expInstData.m_regdAttachEntity = pExpInst->GetAttachEntity();
			if (m_nonTimedScorchProbes[i].m_expInstData.m_regdAttachEntity)
			{
				m_nonTimedScorchProbes[i].m_expInstData.m_attachBoneIndex = pExpInst->GetAttachBoneIndex();
			}
			else
			{
				m_nonTimedScorchProbes[i].m_expInstData.m_attachBoneIndex = -1;
			}
			m_nonTimedScorchProbes[i].m_expInstData.m_pVfxExpInfo = pVfxExpInfo;
			m_nonTimedScorchProbes[i].m_expInstData.m_sizeScale = pExpInst->GetSizeScale();
			m_nonTimedScorchProbes[i].m_expInstData.m_physicalType = pExpInst->GetPhysicalType();
			m_nonTimedScorchProbes[i].m_expInstData.m_weaponEffectGroup = pExpInst->GetWeaponGroup();
			m_nonTimedScorchProbes[i].m_expInstData.m_expInstInAir = pExpInst->GetInAir();
			m_nonTimedScorchProbes[i].m_expInstData.m_expTag = pExpInst->GetExplosionTag();

			// submit the ground probe
			const float PROBE_DIST = 2.5f;
			Vec3V vStartPos = vExpPosWld;
			vStartPos.SetZf(vStartPos.GetZf()+0.05f);
			Vec3V vEndPos = vStartPos - Vec3V(0.0f, 0.0f, PROBE_DIST);

			m_nonTimedScorchProbes[i].m_vfxProbeIds[0] = CVfxAsyncProbeManager::SubmitWorldProbe(vStartPos, vEndPos, probeFlags);

#if __BANK
			if (g_vfxExplosion.m_renderExplosionNonTimedScorchProbes)
			{
				grcDebugDraw::Line(vStartPos, vEndPos, Color32(1.0f, 0.5f, 0.0f, 1.0f), Color32(1.0f, 1.0f, 1.0f, 1.0f), -200);
			}
#endif

			// submit the wall probes
			const Vec3V PROBE_DIRS[4] = 
			{
				 Vec3V(V_X_AXIS_WZERO), 
				-Vec3V(V_X_AXIS_WZERO), 
				 Vec3V(V_Y_AXIS_WZERO), 
				-Vec3V(V_Y_AXIS_WZERO)
			};

			for (s32 j=0; j<4; j++)
			{
				vStartPos = vExpPosWld;
				vEndPos = vStartPos + (PROBE_DIRS[j]*ScalarVFromF32(PROBE_DIST));
				vStartPos -= (PROBE_DIRS[j]*ScalarVFromF32(0.05f));

				m_nonTimedScorchProbes[i].m_vfxProbeIds[j+1] = CVfxAsyncProbeManager::SubmitWorldProbe(vStartPos, vEndPos, probeFlags);

#if __BANK
				if (g_vfxExplosion.m_renderExplosionNonTimedScorchProbes)
				{
					grcDebugDraw::Line(vStartPos, vEndPos, Color32(1.0f, 0.5f, 0.0f, 1.0f), Color32(1.0f, 1.0f, 1.0f, 1.0f), -200);
				}
#endif
			}

			return;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxExplosion
///////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////
//  Append DLC data
//////////////////////////////////////////////////////////////////////////
class CVfxExplosionFileMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile& file)
	{
		g_vfxExplosion.Append(&file);
		return true;
	}
	virtual void UnloadDataFile(const CDataFileMgr::DataFile & /*file*/)
	{
	}
} g_vfxExplosionFileMounter;




///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxExplosion::Init								(unsigned initMode)
{
	if (initMode==INIT_CORE)
	{
		CDataFileMount::RegisterMountInterface(CDataFileMgr::EXPLOSIONFX_FILE, &g_vfxExplosionFileMounter);	

#if __BANK
		m_bankInitialised = false;
		m_renderExplosionInfo = false;
		m_renderExplosionFireProbes = false;
		m_renderExplosionTimedScorchProbes = false;
		m_renderExplosionNonTimedScorchProbes = false;
		m_renderExplosionNonTimedScorchProbeResults = false;
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
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxExplosion::Shutdown							(unsigned shutdownMode)
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

void		 	CVfxExplosion::Update							()
{
	m_asyncProbeMgr.Update();
}



///////////////////////////////////////////////////////////////////////////////
//  LoadData
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxExplosion::LoadData							()
{
	// initialise the data
	m_explosionInfoMap.Reset();

	// skip reading in the data if required
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::EXPLOSIONFX_FILE);
	while (DATAFILEMGR.IsValid(pData))
	{
		ReadData(pData);
		// get next file
		pData = DATAFILEMGR.GetPrevFile(pData);
	}

	m_explosionInfoMap.FinishInsertion();
}

void			CVfxExplosion::Append(const CDataFileMgr::DataFile* pData)
{
	if(DATAFILEMGR.IsValid(pData))
	{
		ReadData(pData,true);
	}
	m_explosionInfoMap.FinishInsertion();
}

//////////////////////////////////////////////////////////////////////////
// Read Data
//////////////////////////////////////////////////////////////////////////
void			CVfxExplosion::ReadData(const CDataFileMgr::DataFile* pData, bool checkForDupes)
{
	// read in the data
	fiStream* stream = ASSET.Open(pData->m_filename, "dat", true);
	vfxAssertf(stream, "cannot open data file");

	if (stream)
	{
		// initialise the tokens
		fiAsciiTokenizer token;
		token.Init("explosionFx", stream);

		// read in the version
		m_version = token.GetFloat();
		// m_version = m_version;

		s32 phase = -1;	
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
			if (stricmp(charBuff, "EXPLOSIONFX_TABLE_START")==0)
			{
				phase = 0;
				continue;
			}
			if (stricmp(charBuff, "EXPLOSIONFX_TABLE_END")==0)
			{
				break;
			}

			// phase 0 - explosion info
			if (phase==0)
			{
				VfxExplosionInfo_s explosionInfo;

				atHashWithStringNotFinal explosionNameHash = atHashWithStringNotFinal(charBuff);

				// fx data
				token.GetToken(charBuff, 128);		
				if (stricmp(charBuff, "-")==0)
				{	
					explosionInfo.fxHashName[0] = (u32)0;					
				}
				else
				{
					explosionInfo.fxHashName[0] = atHashWithStringNotFinal(charBuff);
				}

				if (m_version>=2.0f)
				{
					token.GetToken(charBuff, 128);		
					if (stricmp(charBuff, "-")==0)
					{	
						explosionInfo.fxHashName[1] = (u32)0;					
					}
					else
					{
						explosionInfo.fxHashName[1] = atHashWithStringNotFinal(charBuff);
					}
				}
				else
				{
					explosionInfo.fxHashName[1] = (u32)0;	
				}

				if (m_version>=3.0f)
				{
					token.GetToken(charBuff, 128);		
					if (stricmp(charBuff, "-")==0)
					{	
						explosionInfo.fxHashName[2] = (u32)0;					
					}
					else
					{
						explosionInfo.fxHashName[2] = atHashWithStringNotFinal(charBuff);
					}
				}
				else
				{
					explosionInfo.fxHashName[2] = (u32)0;	
				}

				if (m_version>=5.0f)
				{
					token.GetToken(charBuff, 128);	
					if (stricmp(charBuff, "-")==0)
					{	
						explosionInfo.fxLodHashName = (u32)0;					
					}
					else
					{
						explosionInfo.fxLodHashName = atHashWithStringNotFinal(charBuff);
					}
				}
				else
				{
					explosionInfo.fxLodHashName = (u32)0;	
				}

				token.GetToken(charBuff, 128);		
				if (stricmp(charBuff, "-")==0)
				{	
					explosionInfo.fxAirHashName[0] = (u32)0;					
				}
				else
				{
					explosionInfo.fxAirHashName[0] = atHashWithStringNotFinal(charBuff);
				}

				if (m_version>=2.0f)
				{
					token.GetToken(charBuff, 128);		
					if (stricmp(charBuff, "-")==0)
					{	
						explosionInfo.fxAirHashName[1] = (u32)0;					
					}
					else
					{
						explosionInfo.fxAirHashName[1] = atHashWithStringNotFinal(charBuff);
					}
				}
				else
				{
					explosionInfo.fxAirHashName[1] = (u32)0;	
				}

				if (m_version>=3.0f)
				{
					token.GetToken(charBuff, 128);		
					if (stricmp(charBuff, "-")==0)
					{	
						explosionInfo.fxAirHashName[2] = (u32)0;					
					}
					else
					{
						explosionInfo.fxAirHashName[2] = atHashWithStringNotFinal(charBuff);
					}
				}
				else
				{
					explosionInfo.fxAirHashName[2] = (u32)0;	
				}

				if (m_version>=5.0f)
				{
					token.GetToken(charBuff, 128);	
					if (stricmp(charBuff, "-")==0)
					{	
						explosionInfo.fxAirLodHashName = (u32)0;					
					}
					else
					{
						explosionInfo.fxAirLodHashName = atHashWithStringNotFinal(charBuff);
					}
				}
				else
				{
					explosionInfo.fxAirLodHashName = (u32)0;	
				}

				explosionInfo.fxScale = token.GetFloat();

				if (m_version>=4.0f)
				{
					explosionInfo.createsScorchDecals = token.GetInt()>0;
				}
				else
				{
					explosionInfo.createsScorchDecals = true;
				}

				// fires
				explosionInfo.minFires = token.GetInt();
				explosionInfo.maxFires = token.GetInt();
				explosionInfo.minFireRange = token.GetFloat();
				explosionInfo.maxFireRange = token.GetFloat();
				explosionInfo.burnTimeMin = token.GetFloat();
				explosionInfo.burnTimeMax = token.GetFloat();
				explosionInfo.burnStrengthMin = token.GetFloat();
				explosionInfo.burnStrengthMax = token.GetFloat();
				explosionInfo.peakTimeMin = token.GetFloat();
				explosionInfo.peakTimeMax = token.GetFloat();
				explosionInfo.fuelLevel = token.GetFloat();
				explosionInfo.fuelBurnRate = token.GetFloat();

				if (!vfxVerifyf(explosionInfo.minFires<=explosionInfo.maxFires, "explosionfx.dat has a minFires value greather than maxFires - this will be clamped"))
				{
					explosionInfo.minFires = explosionInfo.maxFires;
				}

				// check if this is an alternative entry of an existing explosion info
				explosionInfo.numAltEntries = 0;
				VfxExplosionInfo_s* pOrigVfxExpInfo = m_explosionInfoMap.SafeGet(explosionNameHash);
				if (pOrigVfxExpInfo)
				{
					pOrigVfxExpInfo->numAltEntries++;	
					explosionNameHash.SetHash(explosionNameHash.GetHash()+pOrigVfxExpInfo->numAltEntries);
				}

				if(checkForDupes)
				{
					if(!m_explosionInfoMap.SafeGet(explosionNameHash))
					{
						m_explosionInfoMap.Insert(explosionNameHash, explosionInfo);
						m_explosionInfoMap.FinishInsertion();						
					}
				}
				else
				{
					m_explosionInfoMap.Insert(explosionNameHash, explosionInfo);
					m_explosionInfoMap.FinishInsertion();							
				}
			}
		}

		stream->Close();
	}
}

///////////////////////////////////////////////////////////////////////////////
// GetInfo
///////////////////////////////////////////////////////////////////////////////

VfxExplosionInfo_s*	CVfxExplosion::GetInfo						(atHashWithStringNotFinal hashName)
{
	VfxExplosionInfo_s* pVfxExpInfo = m_explosionInfoMap.SafeGet(hashName);
	if (pVfxExpInfo && pVfxExpInfo->numAltEntries>0)
	{
		s32 id = g_DrawRand.GetRanged(0, pVfxExpInfo->numAltEntries);
		if (id>0)
		{
			pVfxExpInfo = m_explosionInfoMap.SafeGet(hashName.GetHash()+id);
		}
	}

	return pVfxExpInfo;
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessVfxExplosion
///////////////////////////////////////////////////////////////////////////////

void			CVfxExplosion::ProcessVfxExplosion			(phGtaExplosionInst* pExpInst, bool isTimedExplosion, float currRadius)
{
	// check if the owner is inside an interior
// 	bool insideInterior = false;
// 	if (pExpInst->GetExplosionOwner() && pExpInst->GetExplosionOwner()->GetIsDynamic())
// 	{
// 		CDynamicEntity* pDynamicEntity = static_cast<CDynamicEntity*>(pExpInst->GetExplosionOwner());
// 		if (pDynamicEntity->GetPortalTracker() && pDynamicEntity->GetPortalTracker()->m_pIntInst && pDynamicEntity->GetPortalTracker()->m_roomIdx>-1)
// 		{
// 			insideInterior = true;
// 		}
// 	}

	Vec3V vExpPosWld = pExpInst->GetPosWld();
	Vec3V vExpDirWld = pExpInst->GetDirWld();

#if __BANK
	if (g_vfxExplosion.m_renderExplosionInfo)
	{
		grcDebugDraw::Line(vExpPosWld, vExpPosWld+vExpDirWld, Color32(1.0f, 0.0f, 0.0f, 1.0f), Color32(1.0f, 1.0f, 1.0f, 1.0f), -200);
	}
#endif

	// check if this explosion is underwater
	float underwaterDepth = pExpInst->GetUnderwaterDepth();
	WaterTestResultType underwaterType = pExpInst->GetUnderwaterType();

	// make vehicle explosions have to be a little distance underwater before creating underwater explosions
	bool isUnderwater = underwaterDepth>0.0f;
	if (pExpInst->GetAttachEntity() && pExpInst->GetAttachEntity()->GetIsTypeVehicle())
	{
		isUnderwater = underwaterDepth>1.0f;
	}
	
	if (isUnderwater)
	{
		// explosion is underwater
		// non-timed explosions should trigger an underwater explosion if less than 10m in water
		CExplosionTagData& explosionTagData = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pExpInst->GetExplosionTag());
		if (isTimedExplosion==false && underwaterDepth<=10.0f && pExpInst->GetDamageType()!=DAMAGE_TYPE_FIRE && !explosionTagData.bPreventWaterExplosionVFX)
		{
			const bool isTorpedoFromKosatka = (pExpInst->GetExplosionTag() == EXP_TAG_TORPEDO_UNDERWATER || pExpInst->GetExplosionTag() == EXP_TAG_TORPEDO) &&
											  pExpInst->GetExplosionOwner() &&
											  MI_SUB_KOSATKA.IsValid() &&
											  pExpInst->GetExplosionOwner()->GetModelIndex() == MI_SUB_KOSATKA;
			const float effectScale = isTorpedoFromKosatka ? 3.5f : 1.0f;

			Vec3V vFxPos = vExpPosWld;
			vFxPos.SetZf(vFxPos.GetZf()+underwaterDepth);
			g_vfxExplosion.TriggerPtFxExplosionWater(vFxPos, underwaterDepth, underwaterType, effectScale);

			// disturb the water surface
			static float disturbVal = 100.0f;
			float worldX = vFxPos.GetXf();
			float worldY = vFxPos.GetYf();
			Water::ModifyDynamicWaterSpeed(worldX,			worldY,			disturbVal, 1.0f);
			Water::ModifyDynamicWaterSpeed(worldX,			worldY + 2.0f,	disturbVal, 1.0f);
			Water::ModifyDynamicWaterSpeed(worldX,			worldY - 2.0f,	disturbVal, 1.0f);
			Water::ModifyDynamicWaterSpeed(worldX + 2.0f,	worldY,			disturbVal, 1.0f);
			Water::ModifyDynamicWaterSpeed(worldX - 2.0f,	worldY,			disturbVal, 1.0f);
		}

		if (!explosionTagData.bAllowUnderwaterExplosion)
		{
			return;
		}
	}

	// get the vfx tag from the explosion inst
	u32 vfxTagHash = pExpInst->GetVfxTagHash();
	if (vfxTagHash==0)
	{
		// use the default vfx tag if this is zero
		vfxTagHash = CExplosionInfoManager::m_ExplosionInfoManagerInstance.GetExplosionTagData(pExpInst->GetExplosionTag()).GetVFXTagHash();
	}

	// search for any ground below the explosion
	if (isTimedExplosion)
	{
		VfxExplosionInfo_s* pExpInfo = GetInfo(vfxTagHash);
		if (pExpInfo)
		{
			if (pExpInfo->createsScorchDecals)
			{
				// note that it is possible for some timed explosions to be non directed - e.g. smoke grenades
				if (pExpInst->GetPhysicalType()==PT_DIRECTED)
				{
					Vec3V vStartPos = vExpPosWld;
					Vec3V vEndPos = vStartPos + vExpDirWld*ScalarVFromF32(currRadius);

					m_asyncProbeMgr.TriggerTimedScorchProbe(vStartPos, vEndPos, pExpInst->GetExplosionOwner(), pExpInst->GetWeaponGroup());
				}
			}

			UpdatePtFxExplosion(pExpInst, pExpInfo, currRadius);
		}
	}
	else
	{
		if (vfxVerifyf(pExpInst->GetPhysicalType()==PT_SPHERE, "non-spherical, non-timed explosion being processed"))
		{
			VfxExplosionInfo_s* pExpInfo = GetInfo(vfxTagHash);
			if (pExpInfo)
			{
				// play any weapon impact effects
				if (vfxTagHash==ATSTRINGHASH("EXP_VFXTAG_SNOWBALL", 0x4B0245BA))
				{
					// snowball specific hack
					s32 decalRenderSettingIndex;
					s32 decalRenderSettingCount;
					g_decalMan.FindRenderSettingInfo(DECALID_SNOWBALL, decalRenderSettingIndex, decalRenderSettingCount);

					Vec3V vSide = Vec3V(1.0f, 0.0f, 0.0f);
					CVfxHelper::GetRandomTangent(vSide, -vExpDirWld);

 					REPLAY_ONLY(int decalID = )g_decalMan.AddGeneric(DECALTYPE_GENERIC_COMPLEX_COLN, 
 										 decalRenderSettingIndex, decalRenderSettingCount, 
 										 vExpPosWld, -vExpDirWld, vSide, 
 										 1.0f, 1.0f, 0.3f, 
 										 -1.0f, 0.0f, 
 										 1.0f, 1.0f, 0.0f, 
 										 Color32(1.0f, 1.0f, 1.0f, 1.0f), 
 										 false, false, 
 										 0.0f, 
 										 false, false
 										 REPLAY_ONLY(, 0.0f));

#if GTA_REPLAY
					if(CReplayMgr::ShouldRecord())
						CReplayMgr::RecordFx(CPacketAddSnowballDecal(vExpPosWld, vExpDirWld, vSide, decalID));
#endif // GTA_REPLAY

					VfxExplosionInstData_s expInstData;
					expInstData.m_vPosWld = pExpInst->GetPosWld();
					expInstData.m_vPosLcl = pExpInst->GetPosLcl();
					expInstData.m_vDirLcl = pExpInst->GetDirLcl();
					expInstData.m_regdOwner = pExpInst->GetExplosionOwner();
					expInstData.m_regdAttachEntity = pExpInst->GetAttachEntity();
					expInstData.m_attachBoneIndex = pExpInst->GetAttachEntity() ? pExpInst->GetAttachBoneIndex() : -1;
					expInstData.m_pVfxExpInfo = pExpInfo;
					expInstData.m_sizeScale = pExpInst->GetSizeScale();
					expInstData.m_physicalType = pExpInst->GetPhysicalType();
					expInstData.m_weaponEffectGroup = pExpInst->GetWeaponGroup();
					expInstData.m_expInstInAir = pExpInst->GetInAir();
					expInstData.m_expTag = pExpInst->GetExplosionTag();

					g_vfxExplosion.TriggerPtFxExplosion(expInstData, pExpInfo, vExpPosWld, vExpDirWld);
				}
				else
				{
					m_asyncProbeMgr.TriggerNonTimedScorchProbes(pExpInst, pExpInfo);

					CreateFires(pExpInst, pExpInfo);

					// check if the explosion is close enough to a petrol pool
					u32 foundDecalTypeFlag = 0;
					Vec3V vClosestPos = Vec3V(V_ZERO);
					fwEntity* pClosestEntity = NULL;
					if (g_decalMan.GetDisablePetrolDecalsIgniting()==false && g_decalMan.FindClosest((1<<DECALTYPE_POOL_LIQUID | 1<<DECALTYPE_TRAIL_LIQUID), VFXLIQUID_TYPE_PETROL, vExpPosWld, 5.0f, 0.0f, false, true, 2.5f, vClosestPos, &pClosestEntity, foundDecalTypeFlag))
					{
						CEntity* pClosestEntityActual = NULL;
						if (pClosestEntity)
						{
							pClosestEntityActual = static_cast<CEntity*>(pClosestEntity);
						}

						CPed* pCulprit = (pExpInst->GetExplosionOwner() && pExpInst->GetExplosionOwner()->GetIsTypePed()) ? static_cast<CPed*>(pExpInst->GetExplosionOwner()) : NULL;

						g_fireMan.StartPetrolFire(pClosestEntityActual, vClosestPos, Vec3V(V_Z_AXIS_WZERO), pCulprit, foundDecalTypeFlag==(1<<DECALTYPE_POOL_LIQUID), vExpPosWld, BANK_ONLY(vExpPosWld,) false);
					}
				}
			}
		}
	}

}


///////////////////////////////////////////////////////////////////////////////
//  CreateFires
///////////////////////////////////////////////////////////////////////////////

void			CVfxExplosion::CreateFires				(phGtaExplosionInst* pExpInst, VfxExplosionInfo_s* pExpInfo)
{
	CEntity* pExplosionOwner = pExpInst->GetExplosionOwner();

	// cloned explosions cannot create fires
	if (pExpInst->GetNetworkIdentifier().IsClone())
	{
		return;
	}

	// create generic fires
	s32 numFiresToCreate = g_DrawRand.GetRanged(pExpInfo->minFires, pExpInfo->maxFires);
	float numFireToCreateF = static_cast<float>(numFiresToCreate);

	if (numFiresToCreate)
	{
		float burnTime = g_DrawRand.GetRanged(pExpInfo->burnTimeMin, pExpInfo->burnTimeMax);
		float burnStrength = g_DrawRand.GetRanged(pExpInfo->burnStrengthMin, pExpInfo->burnStrengthMax);
		float peakTime = g_DrawRand.GetRanged(pExpInfo->peakTimeMin, pExpInfo->peakTimeMax);
		float fuelLevel = pExpInfo->fuelLevel;
		float fuelBurnRate = pExpInfo->fuelBurnRate;

		for (s32 i=0; i<numFiresToCreate; i++)
		{
			Vec3V vFirePos = pExpInst->GetPosWld();
			vFirePos.SetZf(vFirePos.GetZf()+0.05f);

			float radius = 0.0f;
			if (i==0)
			{
				burnStrength = 1.0f;
				burnTime = g_DrawRand.GetRanged(20.0f, 25.0f);
			}
			else
			{			
				radius = g_DrawRand.GetRanged(pExpInfo->minFireRange, pExpInfo->maxFireRange);
			}

			if (pExpInst->GetDamageType()==DAMAGE_TYPE_FIRE)
			{
				float angle = -PI + (2.0f*PI*(i/numFireToCreateF));
				vFirePos.SetZf(vFirePos.GetZf()+(radius*rage::Cosf(angle)));
				vFirePos.SetYf(vFirePos.GetYf()+(radius*rage::Sinf(angle)));
			}
			else if (pExpInst->GetExplosionTag()==EXP_TAG_CAR && !pExpInst->GetInAir() && pExpInst->GetExplodingEntity())
			{
				float angle = g_DrawRand.GetRanged(-PI, PI);
				vFirePos = pExpInst->GetExplodingEntity()->GetTransform().GetPosition();
				if (i%2==0)
				{
					vFirePos.SetZf(vFirePos.GetZf()+1.0f);
				}
				vFirePos.SetXf(vFirePos.GetXf() + (radius*pExpInst->GetExplodingEntity()->GetBoundRadius()*rage::Cosf(angle)));
				vFirePos.SetYf(vFirePos.GetYf() + (radius*pExpInst->GetExplodingEntity()->GetBoundRadius()*rage::Sinf(angle)));
			}
			else
			{
				if (i>0)
				{
					float angle = g_DrawRand.GetRanged(-PI, PI);
					float radius = g_DrawRand.GetRanged(pExpInfo->minFireRange, pExpInfo->maxFireRange);
					vFirePos.SetXf(vFirePos.GetXf() + (radius*rage::Cosf(angle)));
					vFirePos.SetYf(vFirePos.GetYf() + (radius*rage::Sinf(angle)));
				}
			}

			Vec3V vStartPos = vFirePos;
			Vec3V vEndPos = vStartPos - Vec3V(0.0f, 0.0f, 2.0f);

			m_asyncProbeMgr.TriggerFireProbe(vStartPos, vEndPos, pExpInst->GetPosWld(), pExplosionOwner, burnTime, burnStrength, peakTime, fuelLevel, fuelBurnRate, pExpInst->GetWeaponHash());
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxExplosion
///////////////////////////////////////////////////////////////////////////////

void			CVfxExplosion::UpdatePtFxExplosion			(phGtaExplosionInst* pExpInst, VfxExplosionInfo_s* pExpInfo, float currRadius)
{
	bool isAirExplosion = pExpInst->GetInAir();

	Vec3V vPos = pExpInst->GetPosLcl();
	Vec3V vDir = pExpInst->GetDirLcl();
	Vec3V vCamForward = RCC_VEC3V(camInterface::GetMat().b);

	for (int i=0; i<VFXEXPLOSION_NUM_SUB_EFFECTS; i++)
	{
		atHashWithStringNotFinal fxHashName = (u32)0;
		if (isAirExplosion)
		{
			fxHashName = pExpInfo->fxAirHashName[i];
		}
		else
		{
			fxHashName = pExpInfo->fxHashName[i];
		}

		// play the explosion effect
		if (fxHashName)
		{
			// register the fx system
			bool justCreated;
			ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pExpInst, PTFXOFFSET_EXPINST_EXPLOSION+i, false, fxHashName, justCreated);

			// check the effect exists
			if (pFxInst)
			{
				Mat34V vFxMat;

				if (pExpInst->GetAttachEntity())
				{
					// push the subsequent effects behind the first
					if (i>0)
					{
						// bring the camera forward vector into bone space
						Mat34V vAttachMtx;
						CVfxHelper::GetMatrixFromBoneIndex(vAttachMtx, pExpInst->GetAttachEntity(), pExpInst->GetAttachBoneIndex());
						vCamForward = UnTransform3x3Ortho(vAttachMtx, vCamForward);

						// adjust the local position
						for (int k=1; k<VFXEXPLOSION_NUM_SUB_EFFECTS; k++)
						{
							vPos += vCamForward*ScalarV(V_FLT_SMALL_1);
						}
					}

					ptfxAttach::Attach(pFxInst, pExpInst->GetAttachEntity(), pExpInst->GetAttachBoneIndex());

					CVfxHelper::CreateMatFromVecZAlign(vFxMat, vPos, vDir, vCamForward);
					pFxInst->SetOffsetMtx(vFxMat);
				}
				else
				{
					// push the subsequent effects behind the first
					if (i>0)
					{
						for (int k=1; k<VFXEXPLOSION_NUM_SUB_EFFECTS; k++)
						{
							vPos += vCamForward*ScalarV(V_FLT_SMALL_1);
						}
					}

					CVfxHelper::CreateMatFromVecZAlign(vFxMat, vPos, vDir, vCamForward);
					pFxInst->SetBaseMtx(vFxMat);
				}

				// set any evolutions
				float radiusEvo = CVfxHelper::GetInterpValue(currRadius, 0.0f, 10.0f);
				pFxInst->SetEvoValueFromHash(ATSTRINGHASH("radius", 0x4FBB9CF3), radiusEvo);

				float scale = pExpInfo->fxScale*pExpInst->GetSizeScale();
				pFxInst->SetUserZoom(scale);

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
					CReplayMgr::RecordFx<CPacketExplosionFx>(CPacketExplosionFx((size_t)pExpInst, fxHashName, MAT34V_TO_MATRIX34(vFxMat), radiusEvo, pExpInst->GetAttachBoneIndex(), scale), pExpInst->GetAttachEntity());
				}
#endif // GTA_REPLAY

			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxExplosion
///////////////////////////////////////////////////////////////////////////////

void			CVfxExplosion::TriggerPtFxExplosion			(VfxExplosionInstData_s& expInstData, VfxExplosionInfo_s* pExpInfo, Vec3V_In vColnPos, Vec3V_In vColnNormal)
{
	bool isAirExplosion = expInstData.m_expInstInAir;

	// calc the main explosion position and direction
	Vec3V vPos;
	Vec3V vDir;
	Vec3V vPosWld;
	Mat34V vAttachMtx;
	if (expInstData.m_regdAttachEntity)
	{
		// attached explosion data is in local space using the explosion data
		// we know it's already hit an entity so we can just use the data directly
		vPos = expInstData.m_vPosLcl;
		vDir = expInstData.m_vDirLcl;

		CVfxHelper::GetMatrixFromBoneIndex(vAttachMtx, expInstData.m_regdAttachEntity, expInstData.m_attachBoneIndex);
		vPosWld = Transform(vAttachMtx, vPos);
	}
	else
	{
		// non attached explosion data is in world space and may want to use collision data if it is to be relocated
		// default to use the collision position
		vPos = vColnPos;
		if (isAirExplosion)
		{
			// if this is an air explosion use the explosion position (local and world are the same as we aren't attached)
			vPos = expInstData.m_vPosLcl;
		}

		// default to use the collision normal
		vDir = vColnNormal;
		if (expInstData.m_physicalType==PT_DIRECTED)
		{
			// if this is a directed explosion use the explosion direction  (local and world are the same as we aren't attached)
			vDir = expInstData.m_vDirLcl;
		}
		
		vPosWld = vPos;
	}

	Vec3V vCamForward = RCC_VEC3V(camInterface::GetMat().b);

	for (int i=0; i<VFXEXPLOSION_NUM_SUB_EFFECTS; i++)
	{
		atHashWithStringNotFinal fxHashName = (u32)0;
		atHashWithStringNotFinal fxLodHashName = (u32)0;
		if (isAirExplosion)
		{
			fxHashName = pExpInfo->fxAirHashName[i];
			fxLodHashName = pExpInfo->fxAirLodHashName;
		}
		else
		{
			fxHashName = pExpInfo->fxHashName[i];
			fxLodHashName = pExpInfo->fxLodHashName;
		}

		if (g_vfxBlood.IsOverridingWithClownBlood())
		{
			fxHashName = atHashWithStringNotFinal("scr_exp_clown", 0x44d211ff);
		}

		// blimp
		if (i==0 && expInstData.m_expTag==EXP_TAG_BLIMP && expInstData.m_regdAttachEntity && expInstData.m_regdAttachEntity->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(expInstData.m_regdAttachEntity.Get());
			if (pVehicle)
			{
				if (MI_BLIMP_3.IsValid() && pVehicle->GetModelIndex()==MI_BLIMP_3)
				{
					s32 liveryId = pVehicle->GetVariationInstance().GetModIndex(VMT_LIVERY_MOD);
					if (liveryId==0)
					{
						fxHashName = atHashWithStringNotFinal("exp_blimp3_0", 0xF02A504B);
					}
					else if (liveryId==1)
					{
						fxHashName = atHashWithStringNotFinal("exp_blimp3_1", 0x277F4E6);
					}
					else if (liveryId==2)
					{
						fxHashName = atHashWithStringNotFinal("exp_blimp3_2", 0x146918C4);
					}
					else if (liveryId==3)
					{
						fxHashName = atHashWithStringNotFinal("exp_blimp3_3", 0x97EA9FCD);
					}
					else if (liveryId==4)
					{
						fxHashName = atHashWithStringNotFinal("exp_blimp3_4", 0xA942C27D);
					}
					else if (liveryId==5)
					{
						fxHashName = atHashWithStringNotFinal("exp_blimp3_5", 0xBB4C6690);
					}
					else if (liveryId==6)
					{
						fxHashName = atHashWithStringNotFinal("exp_blimp3_6", 0xC87680E4);
					}
					else if (liveryId==7)
					{
						fxHashName = atHashWithStringNotFinal("exp_blimp3_7", 0x4A44047D);
					}
					else if (liveryId==8)
					{
						fxHashName = atHashWithStringNotFinal("exp_blimp3_8", 0x5C8F2913);
					}
					else if (liveryId==9)
					{
						fxHashName = atHashWithStringNotFinal("exp_blimp3_9", 0x71485285);
					}
					else if (liveryId==10)
					{
						fxHashName = atHashWithStringNotFinal("exp_blimp3_10", 0x13694E9E);
					}
					else if (liveryId==11)
					{
						fxHashName = atHashWithStringNotFinal("exp_blimp3_11", 0x127AA1B);
					}
					else if (liveryId==12)
					{
						fxHashName = atHashWithStringNotFinal("exp_blimp3_12", 0xED1C8205);
					}
					else if (liveryId==13)
					{
						fxHashName = atHashWithStringNotFinal("exp_blimp3_13", 0x5BE65F97);
					}
					else if (liveryId==14)
					{
						fxHashName = atHashWithStringNotFinal("exp_blimp3_14", 0x4A8CBCE4);
					}
					else if (liveryId==15)
					{
						fxHashName = atHashWithStringNotFinal("exp_blimp3_15", 0x384F1869);
					}
					else if (liveryId==16)
					{
						fxHashName = atHashWithStringNotFinal("exp_blimp3_16", 0x44F031B7);
					}
					else if (liveryId==17)
					{
						fxHashName = atHashWithStringNotFinal("exp_blimp3_17", 0x32310C39);
					}
					else if (liveryId==18)
					{
						fxHashName = atHashWithStringNotFinal("exp_blimp3_18", 0xA1AE6B32);
					}
					else if (liveryId==19)
					{
						fxHashName = atHashWithStringNotFinal("exp_blimp3_19", 0x8E5C448E);
					}
				}
			}
		}

		// play the explosion effect
		if (fxHashName)
		{
			if (ptfxHistory::Check(fxHashName, 0, NULL, vPosWld, VFXEXPLOSION_HIST_DIST[i]))
			{
				if (i>0)
				{
					continue;
				}
				else if (fxLodHashName)
				{
					// use the lod effect instead
					fxHashName = fxLodHashName;
				}
			}

			ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(fxHashName);
			if (pFxInst)
			{
				Mat34V vFxMat;
				if (expInstData.m_regdAttachEntity)
				{
					// push the subsequent effects behind the first
					if (i>0)
					{
						// bring the camera forward vector into bone space
						vCamForward = UnTransform3x3Ortho(vAttachMtx, vCamForward);

						// adjust the local position
						for (int k=1; k<VFXEXPLOSION_NUM_SUB_EFFECTS; k++)
						{
							vPos += vCamForward*ScalarV(V_FLT_SMALL_1);
						}
					}

					// the explosion is attached - just attach using the attach info and local position and direction direct from the explosion
					ptfxAttach::Attach(pFxInst, expInstData.m_regdAttachEntity, expInstData.m_attachBoneIndex);

					// create and set the offset matrix
					CVfxHelper::CreateMatFromVecZ(vFxMat, vPos, vDir);
					pFxInst->SetOffsetMtx(vFxMat);
				}
				else
				{
					// push the subsequent effects behind the first
					if (i>0)
					{
						for (int k=1; k<VFXEXPLOSION_NUM_SUB_EFFECTS; k++)
						{
							vPos += vCamForward*ScalarV(V_FLT_SMALL_1);
						}
					}

					// create and set the base matrix
					CVfxHelper::CreateMatFromVecZAlign(vFxMat, vPos, vDir, vCamForward);
					pFxInst->SetBaseMtx(vFxMat);
				}

				// apply any user zoom
				pFxInst->SetUserZoom(pExpInfo->fxScale*expInstData.m_sizeScale);

				// start the effect
				pFxInst->Start();

				ptfxHistory::Add(fxHashName, 0, NULL, vPosWld, VFXEXPLOSION_HIST_TIME[i]);

#if GTA_REPLAY
				if(CReplayMgr::ShouldRecord())
				{
					CReplayMgr::RecordFx<CPacketWeaponExplosionFx>(
						CPacketWeaponExplosionFx(fxHashName,
							RCC_MATRIX34(vFxMat),
							expInstData.m_regdAttachEntity ? expInstData.m_attachBoneIndex : 0,
							pExpInfo->fxScale),
						expInstData.m_regdAttachEntity);
				}
#endif
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxExplosionWater
///////////////////////////////////////////////////////////////////////////////

void			CVfxExplosion::TriggerPtFxExplosionWater	(Vec3V_In vPos, float depth, WaterTestResultType underwaterType, float scale)
{
	// play any required effect
	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(atHashWithStringNotFinal("exp_water",0x4F45B437));
	if (pFxInst)
	{
		// calc the world matrix
		Mat34V vFxMtx;
		Vec3V vNormal(V_Z_AXIS_WZERO);
		CVfxHelper::CreateMatFromVecZ(vFxMtx, vPos, vNormal);

		// set the position
		pFxInst->SetBaseMtx(vFxMtx);

		// set the scale
		pFxInst->SetUserZoom(scale);

		// set any evolutions
		float depthEvo = CVfxHelper::GetInterpValue(depth, 0.0f, 10.0f);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("depth", 0xBCA972D6), depthEvo);

		float riverEvo = 0.0f;
		if (underwaterType==WATERTEST_TYPE_RIVER)
		{
			riverEvo = 1.0f;
		}
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("river", 0xBA8B3109), riverEvo);

		// start the effect
		pFxInst->Start();

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketWeaponExplosionWaterFx>(
				CPacketWeaponExplosionWaterFx(atHashWithStringNotFinal("exp_water",0x4F45B437),
					RCC_VECTOR3(vPos),
					depthEvo, 
					riverEvo));
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void		 	CVfxExplosion::InitWidgets					()
{
	if (m_bankInitialised==false)
	{
		bkBank* pVfxBank = vfxWidget::GetBank();
		pVfxBank->PushGroup("Vfx Explosion", false);
		{
			pVfxBank->AddTitle("INFO");

#if __DEV
			pVfxBank->AddTitle("");
			pVfxBank->AddTitle("SETTINGS");
			pVfxBank->AddSlider("Explosion History Dist 1", &VFXEXPLOSION_HIST_DIST[0], 0.0f, 100.0f, 0.25f, NullCB, "");
			pVfxBank->AddSlider("Explosion History Dist 2", &VFXEXPLOSION_HIST_DIST[1], 0.0f, 100.0f, 0.25f, NullCB, "");
			pVfxBank->AddSlider("Explosion History Dist 3", &VFXEXPLOSION_HIST_DIST[2], 0.0f, 100.0f, 0.25f, NullCB, "");
			pVfxBank->AddSlider("Explosion History Time 1", &VFXEXPLOSION_HIST_TIME[0], 0.0f, 100.0f, 0.25f, NullCB, "");
			pVfxBank->AddSlider("Explosion History Time 2", &VFXEXPLOSION_HIST_TIME[1], 0.0f, 100.0f, 0.25f, NullCB, "");
			pVfxBank->AddSlider("Explosion History Time 3", &VFXEXPLOSION_HIST_TIME[2], 0.0f, 100.0f, 0.25f, NullCB, "");
#endif

			pVfxBank->AddTitle("");
			pVfxBank->AddTitle("DEBUG");
			pVfxBank->AddToggle("Render Explosion Info", &m_renderExplosionInfo);
			pVfxBank->AddToggle("Render Explosion Fire Probes", &m_renderExplosionFireProbes);
			pVfxBank->AddToggle("Render Explosion Timed Scorch Probes", &m_renderExplosionTimedScorchProbes);
			pVfxBank->AddToggle("Render Explosion Non-Timed Scorch Probes", &m_renderExplosionNonTimedScorchProbes);
			pVfxBank->AddToggle("Render Explosion Non-Timed Scorch Probe Results", &m_renderExplosionNonTimedScorchProbeResults);
			
			
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
void			CVfxExplosion::Reset						()
{
	g_vfxExplosion.Shutdown(SHUTDOWN_WITH_MAP_LOADED);
	g_vfxExplosion.Init(INIT_AFTER_MAP_LOADED);
}
#endif // __BANK




