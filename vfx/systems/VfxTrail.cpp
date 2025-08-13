///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxTrail.cpp
//	BY	: 	Mark Nicholson 
//	FOR	:	Rockstar North
//	ON	:	12 July 2006
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxTrail.h"

// rage

// framework 
#include "grcore/debugdraw.h"
#include "fwscene/world/WorldLimits.h"
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxreg.h"
#include "vfx/ptfx/ptfxwrapper.h"
#include "vfx/vfxwidget.h"

// game
#include "Camera/CamInterface.h"
#include "Camera/Gameplay/Aim/FirstPersonShooterCamera.h"
#include "Core/Game.h"
#include "Scene/DynamicEntity.h"
#include "Scene/World/GameWorld.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Metadata/VfxVehicleInfo.h"
#include "Vfx/Particles/PtFxDefines.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/Systems/VfxMaterial.h"
#include "Vfx/Systems/VfxPed.h"
#include "Vfx/Systems/VfxVehicle.h"
#include "Vfx/VfxHelper.h"

// includes (debug)
#if 0
#include "system/findsize.h"
FindSize(CVfxTrail);
#endif


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

// vfxtrail ptfx offsets for registered systems
enum PtFxTrailOffsets_e
{
	PTFXOFFSET_TRAILGROUP_SCRAPE				= 0,
	PTFXOFFSET_TRAILGROUP_LIQUID,
};

// rag tweakable settings
dev_float		VFXTRAIL_NEW_PT_DIST_MIN		= 0.1f;
dev_float		VFXTRAIL_NEW_PT_DIST_MAX		= 10.0f;
dev_float		VFXTRAIL_NEARBY_RANGE			= 0.2f;
dev_u32			VFXTRAIL_STAYALIVETIME_DEFAULT	= 0;
dev_u32			VFXTRAIL_STAYALIVETIME_PTFX		= 100;


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CVfxTrail		g_vfxTrail;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CTrailGroup
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void		 	CTrailGroup::Init							(VfxTrailType_e type, fwUniqueObjId id)
{
	m_type = type;
	m_id = id;
	m_vCurrDir = Vec3V(V_ZERO);
	m_isActive = true;
	m_registeredThisFrame = false;
	m_isFirstTrailDecal = true;
	m_timeLastRegistered = fwTimer::GetTimeInMilliseconds();
	m_timeLastTrailInfoAdded = fwTimer::GetTimeInMilliseconds();
	m_lastTrailInfoValid = false;

	m_currLength = 0.0f;

#if __BANK
	m_debugRenderLife = 5.0f;
	vfxAssertf(m_debugTrailPointList.GetNumItems()==0, "debug point list is not empty");
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void		 	CTrailGroup::Shutdown						()
{
	m_lastTrailInfo.regdEnt = NULL;

#if __BANK
	// return all of the debug trail points to the pool
	CDebugTrailPoint* pDebugTrailPoint = static_cast<CDebugTrailPoint*>(m_debugTrailPointList.GetHead());
	while (pDebugTrailPoint)
	{
		CDebugTrailPoint* pNextDebugTrailPoint = static_cast<CDebugTrailPoint*>(m_debugTrailPointList.GetNext(pDebugTrailPoint));

		g_vfxTrail.ReturnDebugTrailPointToPool(pDebugTrailPoint, &m_debugTrailPointList);

		pDebugTrailPoint = pNextDebugTrailPoint;
	}

	vfxAssertf(m_debugTrailPointList.GetNumItems()==0, "debug point list is not empty");
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  AddPoint
///////////////////////////////////////////////////////////////////////////////

void		 	CTrailGroup::AddPoint						(VfxTrailInfo_t& vfxTrailInfo, bool isLeadOut)
{
	// don't process if we've already been registered this frame
	// this stops particle effects with mutliple collisions causing problems
//	if (m_registeredThisFrame)
//	{
//		vfxAssertf(0, "trying to register a trail more than once in a frame");
//		return;
//	}

	BANK_ONLY(bool isOnMovable = false);
	if (vfxTrailInfo.regdEnt && !vfxTrailInfo.regdEnt->IsBaseFlagSet(fwEntity::IS_FIXED))
	{
		BANK_ONLY(isOnMovable = true);
		m_lastTrailInfoValid = false;
	}
// 	else
// 	{
// 		// temp - reject points on fixed entities
// 		return;
// 	}
	
	// store the position of the previous trail info (for particle effects)
	bool prevTrailPosValid = false;
	Vec3V vPrevTrailPos = Vec3V(V_ZERO);
	if (m_lastTrailInfoValid)
	{
		prevTrailPosValid = true;
		vPrevTrailPos = m_lastTrailInfo.vWorldPos;
	}

	// check if too close to last point to add
	bool rejectPointTooClose = false;
	bool rejectPointTooFar = false;
	bool rejectPointDir = false;
	if (!isLeadOut && m_lastTrailInfoValid && !IsZeroAll(m_lastTrailInfo.vWorldPos))
	{
		// calc the vector and the square distance from the last to the new point
		Vec3V vVec = vfxTrailInfo.vWorldPos - m_lastTrailInfo.vWorldPos;
		float distSqr = MagSquared(vVec).Getf();

		// reject this point if it's too close to the trail
		if (distSqr<VFXTRAIL_NEW_PT_DIST_MIN*VFXTRAIL_NEW_PT_DIST_MIN)
		{
			rejectPointTooClose = true;
		}
		// reject this point if it's too far away from the trail
		else if (distSqr>VFXTRAIL_NEW_PT_DIST_MAX*VFXTRAIL_NEW_PT_DIST_MAX)
		{
			rejectPointTooFar = true;
		}
		// now check if the trail direction is in the direction of the forward check
		else if (!IsZeroAll(vfxTrailInfo.vForwardCheck))
		{
			float dotProd = Dot(vVec, vfxTrailInfo.vForwardCheck).Getf();
			if (dotProd<0.0f)
			{
				rejectPointDir = true;
				
				//grcDebugDraw::Line(m_lastTrailInfo.worldPos, vfxTrailInfo.worldPos, Color32(0.0f, 1.0f, 0.0f, 1.0f));
			}
		}
	}

	if (!rejectPointTooClose && !rejectPointTooFar && !rejectPointDir)
	{
		vfxTrailInfo.decalRenderSettingsIndex = g_decalMan.GetRenderSettingsIndex(vfxTrailInfo.decalRenderSettingIndex, vfxTrailInfo.decalRenderSettingCount);

		if (m_lastTrailInfoValid)
		{
			// make sure we use the same decal setting for the entire trail 
			if (vfxTrailInfo.decalRenderSettingIndex == m_lastTrailInfo.decalRenderSettingIndex)
			{
				vfxTrailInfo.decalRenderSettingsIndex = m_lastTrailInfo.decalRenderSettingsIndex;
			}

			// copy over the returned info from the previous trail point as input for this one
			vfxTrailInfo.vPrevProjVerts[0] = m_lastTrailInfo.vPrevProjVerts[0];
			vfxTrailInfo.vPrevProjVerts[1] = m_lastTrailInfo.vPrevProjVerts[1];
			vfxTrailInfo.vPrevProjVerts[2] = m_lastTrailInfo.vPrevProjVerts[2];
			vfxTrailInfo.vPrevProjVerts[3] = m_lastTrailInfo.vPrevProjVerts[3];
		}
		else
		{
			vfxTrailInfo.vPrevProjVerts[0] = Vec3V(V_ZERO);
		}

		// do any required visual effects
		int decalId = 0;
		if (m_type==VFXTRAIL_TYPE_SKIDMARK)
		{
			if (m_lastTrailInfoValid && !isLeadOut)
			{
				decalId = g_decalMan.AddTrail(DECALTYPE_TRAIL_SKID, VFXLIQUID_TYPE_NONE, vfxTrailInfo.decalRenderSettingsIndex, m_lastTrailInfo.vWorldPos, vfxTrailInfo.vWorldPos, vfxTrailInfo.vNormal, 
					vfxTrailInfo.width, vfxTrailInfo.col, m_lastTrailInfo.col.GetAlpha(), vfxTrailInfo.decalLife, vfxTrailInfo.mtlId, m_currLength, &vfxTrailInfo.vPrevProjVerts[0], false);
			}
		}
		else if (m_type==VFXTRAIL_TYPE_SCRAPE)
		{
#if __BANK
			if (g_vfxMaterial.GetScrapeDecalsDisabled()==false)
#endif
			{
				if (!vfxTrailInfo.dontApplyDecal)
				{
					// add a projected texture	
// 					if (m_lastTrailInfoValid && !isLeadOut)
// 					{
// 						decalId = g_decalMan.AddTrail(DECALTYPE_TRAIL_SCRAPE, VFXLIQUID_TYPE_NONE, vfxTrailInfo.decalRenderSettingsIndex, m_lastTrailInfo.vWorldPos, vfxTrailInfo.vWorldPos, vfxTrailInfo.vNormal, 
// 							vfxTrailInfo.width, vfxTrailInfo.col, m_lastTrailInfo.col.GetAlpha(), vfxTrailInfo.decalLife, vfxTrailInfo.mtlId, m_currLength, &vfxTrailInfo.vPrevProjVerts[0], false);
// 					}
// 					else
					{
#if __BANK
						if (!g_vfxMaterial.GetScrapeDecalsDisableSingle())
#endif
						{
							// static float MAX_LENGTH_MULT = 4.0f;

							Vec3V vDir = vfxTrailInfo.vDir;
							vDir = NormalizeSafe(vDir, Vec3V(V_ZERO));

							float velocityT = CVfxHelper::GetInterpValue(vfxTrailInfo.scrapeMag, vfxTrailInfo.pVfxMaterialInfo->velThreshMin, vfxTrailInfo.pVfxMaterialInfo->velThreshMax);
							float scrapeLengthMult = vfxTrailInfo.pVfxMaterialInfo->texLengthMultMin + (velocityT*(vfxTrailInfo.pVfxMaterialInfo->texLengthMultMax-vfxTrailInfo.pVfxMaterialInfo->texLengthMultMin));
							float halfScrapeLength = vfxTrailInfo.width*scrapeLengthMult*0.5f;

							Vec3V vPosA = vfxTrailInfo.vWorldPos - vDir*ScalarVFromF32(halfScrapeLength);
							Vec3V vPosB = vfxTrailInfo.vWorldPos + vDir*ScalarVFromF32(halfScrapeLength);

							//decalId = g_decalMan.AddTrail(DECALTYPE_TRAIL_SCRAPE_FIRST, VFXLIQUID_TYPE_NONE, vfxTrailInfo.decalRenderSettingsIndex, vPosA, vPosB, vfxTrailInfo.vNormal, 
							//	vfxTrailInfo.width, vfxTrailInfo.col, m_lastTrailInfo.col.GetAlpha(), vfxTrailInfo.decalLife, vfxTrailInfo.mtlId, m_currLength, &vfxTrailInfo.vPrevProjVerts[0], false);						

							decalId = g_decalMan.AddScuff(vfxTrailInfo.regdEnt, vfxTrailInfo.decalRenderSettingsIndex, vPosA, vPosB, vfxTrailInfo.vNormal, 
								vfxTrailInfo.width, vfxTrailInfo.col, vfxTrailInfo.decalLife, vfxTrailInfo.mtlId, 
								vfxTrailInfo.pVfxMaterialInfo->maxOverlayRadius, vfxTrailInfo.pVfxMaterialInfo->duplicateRejectDist, false);

#if GTA_REPLAY
							if(decalId > 0 && CReplayMgr::ShouldRecord())
							{
								bool playerVeh = false;
								if( vfxTrailInfo.regdEnt->GetIsTypeVehicle())
								{
									CVehicle* pVeh = static_cast<CVehicle*>(vfxTrailInfo.regdEnt.Get());
									playerVeh = pVeh->IsDriverAPlayer();
								}

								if( playerVeh )
								{
									CReplayMgr::RecordFx<CPacketPTexMtlScrape_PlayerVeh>(CPacketPTexMtlScrape_PlayerVeh(vfxTrailInfo.regdEnt, vfxTrailInfo.decalRenderSettingsIndex, vPosA, vPosB, vfxTrailInfo.vNormal, 
										vfxTrailInfo.width, vfxTrailInfo.col, vfxTrailInfo.decalLife, vfxTrailInfo.mtlId, 
										vfxTrailInfo.pVfxMaterialInfo->maxOverlayRadius, vfxTrailInfo.pVfxMaterialInfo->duplicateRejectDist, decalId), vfxTrailInfo.regdEnt);
								}
								else
								{
									CReplayMgr::RecordFx<CPacketPTexMtlScrape>(CPacketPTexMtlScrape(vfxTrailInfo.regdEnt, vfxTrailInfo.decalRenderSettingsIndex, vPosA, vPosB, vfxTrailInfo.vNormal, 
										vfxTrailInfo.width, vfxTrailInfo.col, vfxTrailInfo.decalLife, vfxTrailInfo.mtlId, 
										vfxTrailInfo.pVfxMaterialInfo->maxOverlayRadius, vfxTrailInfo.pVfxMaterialInfo->duplicateRejectDist, decalId), vfxTrailInfo.regdEnt);
								}

							}
#endif

							vfxTrailInfo.vPrevProjVerts[0] = Vec3V(V_ZERO);
						}
					}
				}
			}
		}
		else if (m_type==VFXTRAIL_TYPE_WADE)
		{
#if __BANK
			if (g_vfxPed.GetDisableWadeTextures()==false)
#endif
			{
				if (m_lastTrailInfoValid && !isLeadOut)
				{
					decalId = g_decalMan.AddTrail(DECALTYPE_TRAIL_GENERIC, VFXLIQUID_TYPE_NONE, vfxTrailInfo.decalRenderSettingsIndex, m_lastTrailInfo.vWorldPos, vfxTrailInfo.vWorldPos, vfxTrailInfo.vNormal, 
						vfxTrailInfo.width, vfxTrailInfo.col, m_lastTrailInfo.col.GetAlpha(), vfxTrailInfo.decalLife, vfxTrailInfo.mtlId, m_currLength, &vfxTrailInfo.vPrevProjVerts[0], false);
				}
			}
		}
//		else if (m_type==VFXTRAIL_TYPE_SKI)
//		{
//			if (m_lastTrailInfoValid)
//			{
//			}
//		}
		else if (m_type==VFXTRAIL_TYPE_GENERIC)
		{
			if (m_lastTrailInfoValid && !isLeadOut)
			{
				decalId = g_decalMan.AddTrail(DECALTYPE_TRAIL_GENERIC, VFXLIQUID_TYPE_NONE, vfxTrailInfo.decalRenderSettingsIndex, m_lastTrailInfo.vWorldPos, vfxTrailInfo.vWorldPos, vfxTrailInfo.vNormal, 
					vfxTrailInfo.width, vfxTrailInfo.col, m_lastTrailInfo.col.GetAlpha(), vfxTrailInfo.decalLife, vfxTrailInfo.mtlId, m_currLength, &vfxTrailInfo.vPrevProjVerts[0], false);
			}
		}
		else if (m_type>=VFXTRAIL_TYPE_LIQUID)
		{
			if (m_lastTrailInfoValid)
			{
				VfxLiquidType_e liquidType = (VfxLiquidType_e)(m_type-VFXTRAIL_TYPE_LIQUID);
				VfxLiquidInfo_s& liquidInfo = g_vfxLiquid.GetLiquidInfo(liquidType);

				Vec3V vPrevPosWld = m_lastTrailInfo.vWorldPos;
				Vec3V vCurrPosWld = vfxTrailInfo.vWorldPos;
				DecalTrailPiece_e trailPiece = DECALTRAIL_MID;
				if (m_isFirstTrailDecal)
				{
					trailPiece = DECALTRAIL_IN;

					Vec3V vCurrToPrevDir = m_lastTrailInfo.vWorldPos - vCurrPosWld;

					if (IsZeroAll(vCurrToPrevDir)==false)
					{
						vCurrToPrevDir = Normalize(vCurrToPrevDir);
						vPrevPosWld = vCurrPosWld + (vCurrToPrevDir*ScalarVFromF32(liquidInfo.inOutDecalSize));
					}
				}
				else if (isLeadOut)
				{
					trailPiece = DECALTRAIL_OUT;

					if (IsZeroAll(m_vCurrDir)==false)
					{
						vCurrPosWld = vCurrPosWld + (m_vCurrDir*ScalarVFromF32(liquidInfo.inOutDecalSize));
					}
				}
				//Dont add the decals on replay playback as they're recorded seperately
				REPLAY_ONLY(if( !CReplayMgr::IsEditModeActive() ))
				{
					decalId = g_decalMan.AddTrail(DECALTYPE_TRAIL_LIQUID, liquidType, -1, vPrevPosWld, vCurrPosWld, vfxTrailInfo.vNormal, 
						vfxTrailInfo.width, vfxTrailInfo.col, m_lastTrailInfo.col.GetAlpha(), vfxTrailInfo.decalLife, vfxTrailInfo.mtlId, m_currLength, &vfxTrailInfo.vPrevProjVerts[0], false, trailPiece);
				}
				
			}
		}

		if (decalId==0)
		{
			// adding the new decal failed - make sure the join verts get zeroed
			vfxTrailInfo.vPrevProjVerts[0] = Vec3V(V_ZERO);
		}

		// 
		if (m_lastTrailInfoValid)
		{
			m_isFirstTrailDecal = false;
			m_vCurrDir = vfxTrailInfo.vWorldPos - m_lastTrailInfo.vWorldPos;
			m_vCurrDir = NormalizeSafe(m_vCurrDir, Vec3V(V_X_AXIS_WZERO));
		}

		// set the last trail point
		m_lastTrailInfoValid = true;
		m_lastTrailInfo = vfxTrailInfo;
		m_timeLastTrailInfoAdded = fwTimer::GetTimeInMilliseconds();
		
#if __BANK
		if (isOnMovable && m_debugTrailPointList.GetNumItems()==1)
		{
			// just update the current info
			CDebugTrailPoint* pDebugTrailPoint = static_cast<CDebugTrailPoint*>(m_debugTrailPointList.GetHead());
			vfxAssertf(pDebugTrailPoint, "debug point list error");
			pDebugTrailPoint->m_info = vfxTrailInfo;
		}
		else
		{
			// try to get a point from the pool
			CDebugTrailPoint* pDebugTrailPoint = g_vfxTrail.GetDebugTrailPointFromPool(&m_debugTrailPointList);
			if (pDebugTrailPoint)
			{
				// we got a debug point - copy over the info
				pDebugTrailPoint->m_info = vfxTrailInfo;
			}
		}
#endif
	}
	else
	{
		if (m_type>=VFXTRAIL_TYPE_LIQUID)
		{
			if (vfxTrailInfo.createLiquidPools && m_timeLastTrailInfoAdded+500<fwTimer::GetTimeInMilliseconds())
			{
				g_decalMan.AddPool((VfxLiquidType_e)(m_type-VFXTRAIL_TYPE_LIQUID), -1, 0, vfxTrailInfo.vWorldPos, vfxTrailInfo.vNormal, vfxTrailInfo.liquidPoolStartSize, vfxTrailInfo.liquidPoolEndSize, vfxTrailInfo.liquidPoolGrowthRate, vfxTrailInfo.mtlId, vfxTrailInfo.col, false);
			}
		}
	}

	// update any particle effect
	if (!rejectPointTooFar)
	{
		if (m_type==VFXTRAIL_TYPE_SCRAPE)
		{
			UpdatePtFxMtlScrape(vfxTrailInfo);
		}
		else if (m_type>=VFXTRAIL_TYPE_LIQUID)
		{
			UpdatePtFxLiquidTrail(vfxTrailInfo, vPrevTrailPos, prevTrailPosValid);
		}

		// mark as registered this frame
		m_registeredThisFrame = true;
		m_timeLastRegistered = fwTimer::GetTimeInMilliseconds();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxMtlScrape
///////////////////////////////////////////////////////////////////////////////

void		 	CTrailGroup::UpdatePtFxMtlScrape				(VfxTrailInfo_t& vfxTrailInfo)
{
#if __BANK
	if (g_vfxMaterial.GetScrapePtFxDisabled()==false)
#endif
	{
		vfxAssertf(vfxTrailInfo.regdEnt, "entity not valid");

		// check that we have an effect to play
		if (vfxTrailInfo.pVfxMaterialInfo->ptFxHashName.GetHash()==0)
		{
			return;
		}

		// set up the effect matrix
		Vec3V vPos, vRight, vForward, vUp;
		vPos = vfxTrailInfo.vWorldPos;
		vRight = Normalize(vfxTrailInfo.vCollVel);
		vForward = Normalize(vfxTrailInfo.vCollNormal);
		vUp = Cross(vRight, vForward);
		vUp = Normalize(vUp);
		vRight = Cross(vForward, vUp);
	
		Mat34V fxMat = Mat34V(V_IDENTITY);

		fxMat.SetCol0(vRight);
		fxMat.SetCol1(vForward);
		fxMat.SetCol2(vUp);
		fxMat.SetCol3(vPos);

		// register the fx system
		bool justCreated;
		ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(this, PTFXOFFSET_TRAILGROUP_SCRAPE, false, vfxTrailInfo.pVfxMaterialInfo->ptFxHashName, justCreated);
		if (pFxInst)
		{
			// set the position of the effect
			if (!vfxTrailInfo.regdEnt->IsBaseFlagSet(fwEntity::IS_FIXED))
			{
				ptfxAttach::Attach(pFxInst, vfxTrailInfo.regdEnt, -1);

				// make the fx matrix relative to the entity
				Mat34V relFxMat;
				CVfxHelper::CreateRelativeMat(relFxMat, vfxTrailInfo.regdEnt->GetMatrix(), fxMat);

				pFxInst->SetOffsetMtx(relFxMat);
			}
			else
			{
				pFxInst->SetBaseMtx(fxMat);
			}

			float velocityEvo = CVfxHelper::GetInterpValue(vfxTrailInfo.scrapeMag, vfxTrailInfo.pVfxMaterialInfo->velThreshMin, vfxTrailInfo.pVfxMaterialInfo->velThreshMax);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("velocity", 0x8FF7E5A7), velocityEvo);

			float impulseEvo = CVfxHelper::GetInterpValue(vfxTrailInfo.accumImpulse, vfxTrailInfo.pVfxMaterialInfo->impThreshMin, vfxTrailInfo.pVfxMaterialInfo->impThreshMax);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("impulse", 0xB05D6C92), impulseEvo);

			// set the vehicle evo
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("vehicle", 0xDD245B9C), vfxTrailInfo.vehicleEvo);

			// if the z component is pointing down then flip it
			if (fxMat.GetCol2().GetZf()<0.0f)
			{
				pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_Z);
			}

			// 
			if (vfxTrailInfo.regdEnt->GetIsPhysical())
			{
				CPhysical* pPhysical = static_cast<CPhysical*>(vfxTrailInfo.regdEnt.Get());
				ptfxWrapper::SetVelocityAdd(pFxInst, RCC_VEC3V(pPhysical->GetVelocity()));
			}

			// check if the effect has just been created 
			if (justCreated)
			{
				// check for scrapes coming from the heli rotor system
				if (vfxTrailInfo.accumImpulse==100000.0f && vfxTrailInfo.scrapeMag==100000.0f)
				{
					pFxInst->SetUserZoom(VFXMATERIAL_ROTOR_SIZE_SCALE);
					pFxInst->SetOverrideDistTravelled(VFXMATERIAL_ROTOR_SPAWN_DIST);
				}
				else
				{
					pFxInst->SetUserZoom(vfxTrailInfo.zoom);
				}

				pFxInst->SetLODScalar(g_vfxMaterial.GetBangScrapePtFxLodRangeScale()*vfxTrailInfo.lodRangeScale);

				// it has just been created - start it and set it's position
				pFxInst->Start();
			}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
			else
			{
				if (!vfxTrailInfo.regdEnt->GetIsFixed())
				{
					// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
					vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "effect attachment error");
				}
			}
#endif

#if __BANK
			if (g_vfxMaterial.GetScrapePtFxOutputDebug())
			{
				vfxDebugf1("********************************************\n");
				vfxDebugf1("ScrapeVelMag %.2f (EvoVal %.2f)\n", vfxTrailInfo.scrapeMag, velocityEvo);
				vfxDebugf1("ScrapeImpMag %.2f (EvoVal %.2f)\n", vfxTrailInfo.accumImpulse, impulseEvo);
				vfxDebugf1("********************************************\n");
			}
#endif

#if __BANK
			if (g_vfxMaterial.GetScrapePtFxRenderDebug())
			{
				float colR = velocityEvo;
				float colG = impulseEvo;
				float colB = 0.0f;
				float colA = 1.0f;
				Color32 col = Color32(colR, colG, colB, colA);
				grcDebugDraw::Sphere(vPos, g_vfxMaterial.GetScrapePtFxRenderDebugSize(), col, true, g_vfxMaterial.GetScrapePtFxRenderDebugTimer());
			}
#endif
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxLiquidTrail
///////////////////////////////////////////////////////////////////////////////

void		 	CTrailGroup::UpdatePtFxLiquidTrail				(VfxTrailInfo_t& vfxTrailInfo, Vec3V_In vPrevTrailPos, bool prevTrailPosValid)
{
	// check if the first person camera is active for the ped holding this projectile
	bool isFirstPersonCameraActive = false;
#if FPS_MODE_SUPPORTED
	const camBaseCamera* pCamera = camInterface::GetDominantRenderedCamera();
	isFirstPersonCameraActive = pCamera && pCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId());
#endif

	// get the fx name
	VfxLiquidType_e liquidType = (VfxLiquidType_e)(m_type-VFXTRAIL_TYPE_LIQUID);
	atHashWithStringNotFinal ptFxSplashHashName = g_vfxLiquid.GetLiquidInfo(liquidType).ptFxSplashHashName;
	if (ptFxSplashHashName.GetHash()==0)
	{
		return;
	}

	// get the splash direction and distance evo
//	Vec3V vDir = vfxTrailInfo.vDir;
	Vec3V vDir = Vec3V(V_X_AXIS_WZERO);
	float distEvo = 0.0f;
	if (prevTrailPosValid)
	{
		Vec3V vDeltaPos = vPrevTrailPos - vfxTrailInfo.vWorldPos;
		distEvo = CVfxHelper::GetInterpValue(Mag(vDeltaPos).Getf(), 0.0f, 2.0f);
		if (distEvo==0.0f)
		{
			vDir = Vec3V(V_X_AXIS_WZERO);
		}
		else
		{
			vDir = Normalize(vDeltaPos);
		}
	}

	Mat34V vMtx;
	CVfxHelper::CreateMatFromVecZAlign(vMtx, vfxTrailInfo.vWorldPos, vfxTrailInfo.vNormal, vDir);;

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(this, PTFXOFFSET_TRAILGROUP_LIQUID, false, ptFxSplashHashName, justCreated);
	if (pFxInst)
	{
		// set the position of the effect
		pFxInst->SetBaseMtx(vMtx);

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("dist", 0xD27AC355), distEvo);

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
	}
}





///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxTrail
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxTrail::Init								(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
		// initialise the store trail fx infos
//		m_numStoredTrailInfos = 0;

		// put all of the trail groups into the pool
		vfxAssertf(m_trailGroupList.GetNumItems()==0, "trail group list not empty");
		vfxAssertf(m_trailGroupPool.GetNumItems()==0, "trail group pool not empty");
		for (s32 i=0; i<VFXTRAIL_MAX_GROUPS; i++)
		{
#if __BANK
			vfxAssertf(m_trailGroups[i].m_debugTrailPointList.GetNumItems()==0, "trail point list not empty");
#endif
			m_trailGroupPool.AddItem(&m_trailGroups[i]);
		}
		vfxAssertf(m_trailGroupPool.GetNumItems()==VFXTRAIL_MAX_GROUPS, "trail group pool not full");

#if __BANK
		// put all of the trail points into the pool
		vfxAssertf(m_debugTrailPointPool.GetNumItems()==0, "debug trail point pool not empty");
		for (s32 i=0; i<VFXTRAIL_MAX_DEBUG_POINTS; i++)
		{
			m_debugTrailPointPool.AddItem(&m_debugTrailPoints[i]);
		}
		vfxAssertf(m_debugTrailPointPool.GetNumItems()==VFXTRAIL_MAX_DEBUG_POINTS, "debug trail point pool not full");

		m_debugRenderLifeSingle = 5.0f;
		m_debugRenderLifeMultiple = 5.0f;

		m_debugRenderSkidmarkInfo = false;
		m_debugRenderScrapeInfo = false;
		m_debugRenderWadeInfo = false;
		//m_debugRenderSkiInfo = false;
		m_debugRenderGenericInfo = false;
		m_debugRenderLiquidInfo = false;

		m_disableSkidmarkTrails = false;
		m_disableScrapeTrails = false;
		m_disableWadeTrails = false;
		m_disableGenericTrails = false;
		m_disableLiquidTrails = false;
#endif
	}

// #if __BANK
// 	size_t trailGroupSize = sizeof(CTrailGroup);
// 	size_t trailInfoSize = sizeof(VfxTrailInfo_t);
// 	size_t trailPointSize = sizeof(CDebugTrailPoint);
// 
// 	int totalTrailGroupSize = trailGroupSize * VFXTRAIL_MAX_GROUPS;
// 	int totalTrailInfoSize = trailInfoSize * VFXTRAIL_MAX_INFOS;
// 	int totalTrailPointSize = trailPointSize * VFXTRAIL_MAX_DEBUG_POINTS;
// 
// 	int totalMem = totalTrailGroupSize + totalTrailInfoSize + totalTrailPointSize;
// 	totalMem = totalMem;
// #endif
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxTrail::Shutdown							(unsigned shutdownMode)
{
	if (shutdownMode==SHUTDOWN_CORE)
	{
		// remove any active trail groups
		CTrailGroup* pTrailGroup = static_cast<CTrailGroup*>(m_trailGroupList.GetHead());
		while (pTrailGroup)
		{
			CTrailGroup* pNextTrailGroup = static_cast<CTrailGroup*>(m_trailGroupList.GetNext(pTrailGroup));

			pTrailGroup->Shutdown();
			ReturnTrailGroupToPool(pTrailGroup);

			pTrailGroup = pNextTrailGroup;
		}
		vfxAssertf(m_trailGroupList.GetNumItems()==0, "trail group list not empty");

		vfxAssertf(m_trailGroupPool.GetNumItems()==VFXTRAIL_MAX_GROUPS, "trail group pool not full");
		m_trailGroupPool.RemoveAll();
		vfxAssertf(m_trailGroupPool.GetNumItems()==0, "trail group pool not empty");

#if __BANK
		vfxAssertf(m_debugTrailPointPool.GetNumItems()==VFXTRAIL_MAX_DEBUG_POINTS, "debug trail point pool not full");
		m_debugTrailPointPool.RemoveAll();
		vfxAssertf(m_debugTrailPointPool.GetNumItems()==0, "debug trail point pool not empty");
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxTrail::Update								(float BANK_ONLY(deltaTime))
{
	// update the world position of each 
	CTrailGroup* pTrailGroup = static_cast<CTrailGroup*>(m_trailGroupList.GetHead());
	while (pTrailGroup)
	{
		CTrailGroup* pNextTrailGroup = static_cast<CTrailGroup*>(m_trailGroupList.GetNext(pTrailGroup));

		if (pTrailGroup->m_lastTrailInfo.regdEnt)
		{
			if (pTrailGroup->m_lastTrailInfo.regdEnt->GetCurrentPhysicsInst()==NULL)
			{
				// the entity pointer is valid but the physics instance has been deleted 
				pTrailGroup->Shutdown();
				ReturnTrailGroupToPool(pTrailGroup);
			}
			else
			{
				Mat34V mat;
				CVfxHelper::GetMatrixFromBoneIndex(mat, pTrailGroup->m_lastTrailInfo.regdEnt, pTrailGroup->m_lastTrailInfo.matrixIndex);

				// don't process any matrices that are now zero - i.e. the bone has broken off
				if (IsZeroAll(mat.GetCol0()))
				{
					pTrailGroup->Shutdown();
					ReturnTrailGroupToPool(pTrailGroup);
					pTrailGroup = pNextTrailGroup;
					continue;
				}

				pTrailGroup->m_lastTrailInfo.vWorldPos = Transform(mat, pTrailGroup->m_lastTrailInfo.vLocalPos);

#if __BANK
				CDebugTrailPoint* pDebugTrailPoint = static_cast<CDebugTrailPoint*>(pTrailGroup->m_debugTrailPointList.GetHead());
				while (pDebugTrailPoint)
				{	
					pDebugTrailPoint->m_info.vWorldPos = Transform(mat, pDebugTrailPoint->m_info.vLocalPos);

					pDebugTrailPoint = static_cast<CDebugTrailPoint*>(pTrailGroup->m_debugTrailPointList.GetNext(pDebugTrailPoint));
				}
#endif
			}
		}
		else if (pTrailGroup->m_lastTrailInfo.hasEntityPtr)
		{
			// the entity pointer was valid but has been deleted - shutdown this group 
			pTrailGroup->Shutdown();
			ReturnTrailGroupToPool(pTrailGroup);
		}

		pTrailGroup = pNextTrailGroup;
	}

	//ProcessStoredTrailInfos();

	// reset the trail infos as this frame's have been processed now
//	m_numStoredTrailInfos = 0;

	// go through the trail group list and update
	pTrailGroup = static_cast<CTrailGroup*>(m_trailGroupList.GetHead());
	while (pTrailGroup)
	{
		// store the next trail group in the list
		CTrailGroup* pNextTrailGroup = static_cast<CTrailGroup*>(m_trailGroupList.GetNext(pTrailGroup));

		// update active trail groups
		if (pTrailGroup->m_isActive)
		{
			// calc the stay alive time for this group type
			u32 stayAliveTimeMs = VFXTRAIL_STAYALIVETIME_DEFAULT;
			if (pTrailGroup->m_type>=VFXTRAIL_TYPE_GENERIC || pTrailGroup->m_type>=VFXTRAIL_TYPE_LIQUID)
			{
				// generic and liquid trails come from particle system collisions
				// we can't guarantee registering one per frame so we use a 'stay alive time' instead
				stayAliveTimeMs = VFXTRAIL_STAYALIVETIME_PTFX;
			}

			// MN - replacing m_registeredThisFrame with m_timeLastRegistered so that trails will stay alive for a while
//			if (!pTrailGroup->m_registeredThisFrame)
			if (pTrailGroup->m_timeLastRegistered+stayAliveTimeMs<fwTimer::GetTimeInMilliseconds())
			{
				pTrailGroup->m_isActive = false;

				// add a lead out decal
				if (pTrailGroup->m_lastTrailInfoValid)
				{
					pTrailGroup->AddPoint(pTrailGroup->m_lastTrailInfo, true);
				}
			}

			// reset the registered this frame flag 
			pTrailGroup->m_registeredThisFrame = false;

#if __BANK
			// reset the current time left for this group
			if (pTrailGroup->m_debugTrailPointList.GetNumItems()==1)
			{
				pTrailGroup->m_debugRenderLife = m_debugRenderLifeSingle;
			}
			else
			{
				pTrailGroup->m_debugRenderLife = m_debugRenderLifeMultiple;
			}
#endif
		}
		else
		{
#if __BANK
			// update the non-active group
			pTrailGroup->m_debugRenderLife -= deltaTime;

			if (pTrailGroup->m_debugRenderLife<=0.0f)
#endif
			{
				// shutdown this group 
				pTrailGroup->Shutdown();
				ReturnTrailGroupToPool(pTrailGroup);
			}
		}

		// set the next trail group
		pTrailGroup = pNextTrailGroup;
	}

#if __BANK
	m_numTrailGroups = m_trailGroupList.GetNumItems();
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  GetLastTrailInfo
///////////////////////////////////////////////////////////////////////////////

const 
VfxTrailInfo_t*	CVfxTrail::GetLastTrailInfo					(fwUniqueObjId id)
{
	// we have an id - try to find an active trail group with the correct id
	CTrailGroup* pTrailGroup = FindActiveTrailGroup(id);
	if (pTrailGroup && pTrailGroup->m_lastTrailInfoValid)
	{
		return &pTrailGroup->m_lastTrailInfo;
	}

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  RegisterPoint
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxTrail::RegisterPoint					(VfxTrailInfo_t& vfxTrailInfo)
{
#if __BANK
	if ((vfxTrailInfo.type==VFXTRAIL_TYPE_SKIDMARK && m_disableSkidmarkTrails) ||
		(vfxTrailInfo.type==VFXTRAIL_TYPE_SCRAPE && m_disableScrapeTrails) ||
		(vfxTrailInfo.type==VFXTRAIL_TYPE_WADE && m_disableWadeTrails) ||
		(vfxTrailInfo.type==VFXTRAIL_TYPE_GENERIC && m_disableGenericTrails) ||
		(vfxTrailInfo.type==VFXTRAIL_TYPE_LIQUID && m_disableLiquidTrails))
	{
		return;
	}
#endif

#if __BANK
	vfxTrailInfo.vOrigWorldPos = vfxTrailInfo.vWorldPos;
	vfxTrailInfo.origHasEntityPtr = false;
#endif

	vfxAssertf(IsFiniteAll(vfxTrailInfo.vNormal), "trying to register a trail point with an invalid normal");

	// set the matrix index and the local position
	vfxTrailInfo.matrixIndex = -1;
	vfxTrailInfo.hasEntityPtr = false;
	if (vfxTrailInfo.regdEnt)
	{
		// don't bother processing off screen entities
// 		if (vfxTrailInfo.regdEnt->m_nFlags.bOffscreen)
// 		{
// 			return;
// 		}

		// don't process if this is a ped
		if (vfxTrailInfo.regdEnt->GetIsTypePed())
		{
			return;
		}

		// if using a skeleton bone then make sure our skeleton is ok to use - return if not
		if (vfxTrailInfo.matrixIndex>0)
		{
			if (!vfxTrailInfo.regdEnt->GetIsDynamic())
			{
				return;
			}
		}

		vfxTrailInfo.hasEntityPtr = true;

		fragInst* pFragInst = static_cast<fragInst*>(vfxTrailInfo.regdEnt->GetFragInst());
		if (pFragInst)
		{
			fragTypeChild** ppChildren = pFragInst->GetTypePhysics()->GetAllChildren();
			if (ppChildren)
			{
				if (vfxTrailInfo.componentId<0 || vfxTrailInfo.componentId>=pFragInst->GetTypePhysics()->GetNumChildren())
				{
					return;
				}

				vfxTrailInfo.matrixIndex = pFragInst->GetType()->GetBoneIndexFromID(ppChildren[vfxTrailInfo.componentId]->GetBoneID());
			}
		}

		// transform the position into entity space
		Mat34V mat;
		CVfxHelper::GetMatrixFromBoneIndex(mat, vfxTrailInfo.regdEnt, vfxTrailInfo.matrixIndex);
		
		// don't process any matrices that are now zero - i.e. the bone has broken off
		if (IsZeroAll(mat.GetCol0()))
		{
			return;
		}

		vfxTrailInfo.vLocalPos = UnTransformOrtho(mat, vfxTrailInfo.vWorldPos);

#if __BANK
		vfxTrailInfo.origHasEntityPtr = true;
		vfxTrailInfo.vOrigLocalPos = vfxTrailInfo.vLocalPos;
		vfxTrailInfo.origEntityMatrix = mat;
#endif

#if __ASSERT
		ProcessDataValidity(mat, vfxTrailInfo, 0);
#endif

/*
#if __ASSERT
		phInst* pInst = vfxTrailInfo.regdEnt->GetCurrentPhysicsInst();
		if (pInst)
		{
			Assert(pInst->GetArchetype());
			if (pInst->GetArchetype())
			{
				phBound* pBound = pInst->GetArchetype()->GetBound();
				Assert(pBound);
				if (pBound)
				{
					Assertf(vfxTrailInfo.localPos.Mag()<=pBound->GetRadiusAroundLocalOrigin()*2.0f, "FxTrail local pos calculation has gone wrong (localPos: %.2f %.2f %.2f", vfxTrailInfo.localPos.x, vfxTrailInfo.localPos.y, vfxTrailInfo.localPos.z);
				}
			}
		}
#endif
*/

	}

	// add the point 
//	if (vfxTrailInfo.id==0)
//	{
//		// no id - store the info and process later
//		StoreTrailInfo(vfxTrailInfo);
//	}
//	else
	{
		vfxAssertf(vfxTrailInfo.id!=0, "trying to register a trail point with an invalid id - this is no longer supported");

		// we have an id - try to find an active trail group with the correct id
		CTrailGroup* pTrailGroup = FindActiveTrailGroup(vfxTrailInfo.id);

		// if one wasn't found - create a new one
		if (pTrailGroup==NULL)
		{
			// create a new trail group
			pTrailGroup = GetTrailGroupFromPool();

			// initialise it if we created one ok
			if (pTrailGroup)
			{
				pTrailGroup->Init(vfxTrailInfo.type, vfxTrailInfo.id);
			}
		}

		// add the point to the group
		if (pTrailGroup)
		{
			pTrailGroup->AddPoint(vfxTrailInfo);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  StoreTrailInfo
///////////////////////////////////////////////////////////////////////////////

// void		 	CVfxTrail::StoreTrailInfo					(VfxTrailInfo_t& vfxTrailInfo)
// {
// 	// go through the current stored trail infos looking for a 'similar' one
// 	for (s32 i=0; i<(s32)m_numStoredTrailInfos; i++)
// 	{
// 		if (m_storedTrailInfos[i].regdEnt==vfxTrailInfo.regdEnt && m_storedTrailInfos[i].matrixIndex==vfxTrailInfo.matrixIndex)
// 		{
// 			Vec3V vVec = m_storedTrailInfos[i].vWorldPos-vfxTrailInfo.vWorldPos;
// 			if (MagSquared(vVec).Getf()<VFXTRAIL_NEARBY_RANGE*VFXTRAIL_NEARBY_RANGE)
// 			{
// 				//m_storedTrailInfos[i].accumImpulse += vfxTrailInfo.accumImpulse;
// 				return;
// 			}
// 		}
// 	}
// 
// 	// hasn't found a 'similar' one - add this to the end
// 	if (m_numStoredTrailInfos<VFXTRAIL_MAX_INFOS)
// 	{
// 		m_storedTrailInfos[m_numStoredTrailInfos] = vfxTrailInfo;
// 		m_numStoredTrailInfos++;
// 	}
// }


///////////////////////////////////////////////////////////////////////////////
//  ProcessStoredTrailInfos
///////////////////////////////////////////////////////////////////////////////

// void		 	CVfxTrail::ProcessStoredTrailInfos				()
// {
// 	// process the stored trail infos without an id and try to assign them to a group
// 	for (s32 i=0; i<(s32)m_numStoredTrailInfos; i++)
// 	{
// 		// skip any stored infos whose entity has been deleted
// 		if (m_storedTrailInfos[i].hasEntityPtr && m_storedTrailInfos[i].regdEnt.Get()==NULL)
// 		{
// 			continue;
// 		}
// 
// 		// skip any stored infos who have an entity but no physics instance
// 		if (m_storedTrailInfos[i].regdEnt && m_storedTrailInfos[i].regdEnt->GetCurrentPhysicsInst()==NULL && m_storedTrailInfos[i].regdEnt->GetFragInst()==NULL)
// 		{	
// 			// tidy up reference
// 			m_storedTrailInfos[i].regdEnt = NULL;
// 			continue;
// 		}
// 
// #if __ASSERT
// 		Mat34V tempMat;
// 		ProcessDataValidity(tempMat, m_storedTrailInfos[i], 1);
// #endif
// 
// 		// calc this frame's world position
// 		if (m_storedTrailInfos[i].regdEnt)
// 		{
// 			vfxAssertf(m_storedTrailInfos[i].hasEntityPtr, "trail info entity pointer flag error");
// 			Mat34V mat;
// 			CVfxHelper::GetMatrixFromBoneIndex(mat, m_storedTrailInfos[i].regdEnt, m_storedTrailInfos[i].matrixIndex);
// 
// 			// don't process any matrices that are now zero - i.e. the bone has broken off
// 			if (IsZeroAll(mat.GetCol0()))
// 			{
// 				// tidy up reference
// 				m_storedTrailInfos[i].regdEnt = NULL;
// 				continue;
// 			}
// 
// 			m_storedTrailInfos[i].vWorldPos = Transform(mat, m_storedTrailInfos[i].vLocalPos);
// 
// #if __ASSERT
// 			ProcessDataValidity(mat, m_storedTrailInfos[i], 2);
// #endif
// 		}
// 
// 		// check if this attached to a moving entity
// 		CTrailGroup* pTrailGroup = NULL;
// 		if (m_storedTrailInfos[i].regdEnt && !m_storedTrailInfos[i].regdEnt->IsBaseFlagSet(fwEntity::IS_FIXED))
// 		{
// 			// we're dealing with a moving entity 
// 			pTrailGroup = FindActiveTrailGroupMovable(m_storedTrailInfos[i].vWorldPos, m_storedTrailInfos[i].regdEnt, m_storedTrailInfos[i].matrixIndex);
// 		}
// 		else
// 		{
// 			// this is a fixed entity (or NULL entity) - try to find an active trail group with the correct id
// 			pTrailGroup = FindActiveTrailGroup(m_storedTrailInfos[i].vWorldPos, m_storedTrailInfos[i].vDir);
// 		}
// 
// 		// if one wasn't found - create a new one
// 		if (pTrailGroup==NULL)
// 		{
// 			// create a new trail group
// 			pTrailGroup = GetTrailGroupFromPool();
// 
// 			// initialise it if we created one ok
// 			if (pTrailGroup)
// 			{
// 				pTrailGroup->Init(m_storedTrailInfos[i].type, 0);
// 			}
// 		}
// 
// 		// if we have a trail group and it hasn't had a point added so far this frame - add this point to it
// 		if (pTrailGroup && pTrailGroup->m_registeredThisFrame==false)
// 		{
// 			pTrailGroup->AddPoint(m_storedTrailInfos[i]);
// 		}	
// 
// 		// tidy up reference
// 		m_storedTrailInfos[i].regdEnt = NULL;
// 	}
// }


///////////////////////////////////////////////////////////////////////////////
//  FindActiveTrailGroup
///////////////////////////////////////////////////////////////////////////////

CTrailGroup*	 	CVfxTrail::FindActiveTrailGroup				(fwUniqueObjId id)
{
	CTrailGroup* pTrailGroup = static_cast<CTrailGroup*>(m_trailGroupList.GetHead());
	while (pTrailGroup)
	{
		if (pTrailGroup->m_isActive && pTrailGroup->m_id==id)
		{
			return pTrailGroup;
		}

		pTrailGroup = static_cast<CTrailGroup*>(m_trailGroupList.GetNext(pTrailGroup));
	}

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  FindActiveTrailGroup
///////////////////////////////////////////////////////////////////////////////

// CTrailGroup*	 	CVfxTrail::FindActiveTrailGroup				(Vec3V_In vPos, Vec3V_In vDir)
// {
// 	Vec3V vLastApproxPos = vPos - vDir;
// 
// 	static float DISTANCE_THRESH = 0.075f;
// 	float closestGroupDistSqr = DISTANCE_THRESH*DISTANCE_THRESH;
// 	CTrailGroup* pClosestTrailGroup = NULL;
// 
// 	CTrailGroup* pTrailGroup = static_cast<CTrailGroup*>(m_trailGroupList.GetHead());
// 	while (pTrailGroup)
// 	{
// 		// look for an active group which has a zero id and hadn't been registered this frame (i.e. another point hasn't been added to it this frame)
// 		if (pTrailGroup->m_isActive && pTrailGroup->m_id==0 && pTrailGroup->m_registeredThisFrame==false)
// 		{
// 			// we have found an active group with a zero id - test how close it is
// 			Vec3V vVec = vLastApproxPos - pTrailGroup->m_lastTrailInfo.vWorldPos;
// 			float distSqr = MagSquared(vVec).Getf();
// 			if (distSqr<closestGroupDistSqr)
// 			{
// 				closestGroupDistSqr = distSqr;
// 				pClosestTrailGroup = pTrailGroup;
// 			}
// 		}
// 
// 		pTrailGroup = static_cast<CTrailGroup*>(m_trailGroupList.GetNext(pTrailGroup));
// 	}
// 
// 	return pClosestTrailGroup;
// }


///////////////////////////////////////////////////////////////////////////////
//  FindActiveTrailGroupMovable
///////////////////////////////////////////////////////////////////////////////

// CTrailGroup*	 	CVfxTrail::FindActiveTrailGroupMovable		(Vec3V_In vPos, CEntity* pEntity, s32 matrixIndex)
// {
// 	// go through the active groups looking for one similar
// 	CTrailGroup* pTrailGroup = static_cast<CTrailGroup*>(m_trailGroupList.GetHead());
// 	while (pTrailGroup)
// 	{
// 		// check if the entity and matrix index match
// 		if (pTrailGroup->m_isActive && pTrailGroup->m_lastTrailInfo.regdEnt==pEntity && pTrailGroup->m_lastTrailInfo.matrixIndex==matrixIndex)
// 		{
// 			// check if 'nearby'
// 			Vec3V vVec = pTrailGroup->m_lastTrailInfo.vWorldPos-vPos;
// 			if (MagSquared(vVec).Getf()<VFXTRAIL_NEARBY_RANGE*VFXTRAIL_NEARBY_RANGE)
// 			{
// 				return pTrailGroup;
// 			}
// 		}
// 
// 		pTrailGroup = static_cast<CTrailGroup*>(m_trailGroupList.GetNext(pTrailGroup));
// 	}
// 
// 	return NULL;
// }


///////////////////////////////////////////////////////////////////////////////
//  GetTrailGroupFromPool
///////////////////////////////////////////////////////////////////////////////

CTrailGroup*	 	CVfxTrail::GetTrailGroupFromPool				()
{
	// try to get a trail group from the pool
	CTrailGroup* pTrailGroup = static_cast<CTrailGroup*>(m_trailGroupPool.RemoveHead());
	if (pTrailGroup)
	{
#if __BANK
		vfxAssertf(pTrailGroup->m_debugTrailPointList.GetNumItems()==0, "Debug Trail Point list is not empty");
#endif

		// we've got one - add it to the list
		m_trailGroupList.AddItem(pTrailGroup);
	}

	return pTrailGroup;
}


///////////////////////////////////////////////////////////////////////////////
//  ReturnTrailGroupToPool
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxTrail::ReturnTrailGroupToPool				(CTrailGroup* pTrailGroup)
{
	// 
	ptfxReg::UnRegister(pTrailGroup);

	// remove the item from the active list
	m_trailGroupList.RemoveItem(pTrailGroup);

	// return the item to the pool
	m_trailGroupPool.AddItem(pTrailGroup);

#if __BANK
	vfxAssertf(pTrailGroup->m_debugTrailPointList.GetNumItems()==0, "debug trail point list not empty");
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  GetDebugTrailPointFromPool
///////////////////////////////////////////////////////////////////////////////

#if __BANK
CDebugTrailPoint*	CVfxTrail::GetDebugTrailPointFromPool		(vfxList* pDebugListToAddTo)
{
	// try to get a trail point from the pool
	CDebugTrailPoint* pDebugTrailPoint = static_cast<CDebugTrailPoint*>(m_debugTrailPointPool.RemoveHead());
	if (pDebugTrailPoint)
	{
		// we've got one - add it to the list
		pDebugListToAddTo->AddItem(pDebugTrailPoint);
		pDebugTrailPoint->m_info.vPrevProjVerts[0] = Vec3V(V_ZERO);
		pDebugTrailPoint->m_info.vPrevProjVerts[1] = Vec3V(V_ZERO);
		pDebugTrailPoint->m_info.vPrevProjVerts[2] = Vec3V(V_ZERO);
		pDebugTrailPoint->m_info.vPrevProjVerts[3] = Vec3V(V_ZERO);
	}

	return pDebugTrailPoint;
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  ReturnDebugTrailPointToPool
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void		 	CVfxTrail::ReturnDebugTrailPointToPool			(CDebugTrailPoint* pDebugTrailPoint, vfxList* pDebugListToRemoveFrom)
{
	// remove the item from the active list
	pDebugListToRemoveFrom->RemoveItem(pDebugTrailPoint);

	// return the item to the pool
	m_debugTrailPointPool.AddItem(pDebugTrailPoint);
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  RenderDebug
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void		 	CVfxTrail::RenderDebug						()
{
	Color32 colRed(1.0f, 0.0f, 0.0f, 1.0f);
	Color32 colYellow(1.0f, 1.0f, 0.0f, 1.0f);
	Color32 colGreen(0.0f, 1.0f, 0.0f, 1.0f);

	// go through the trail group list and render the points
	CTrailGroup* pTrailGroup = static_cast<CTrailGroup*>(m_trailGroupList.GetHead());
	while (pTrailGroup)
	{
		if ((pTrailGroup->m_type==VFXTRAIL_TYPE_SKIDMARK && m_debugRenderSkidmarkInfo) ||
			(pTrailGroup->m_type==VFXTRAIL_TYPE_SCRAPE && m_debugRenderScrapeInfo) ||
			(pTrailGroup->m_type==VFXTRAIL_TYPE_WADE && m_debugRenderWadeInfo) ||
			//(pTrailGroup->m_type==VFXTRAIL_TYPE_SKI && m_debugRenderSkiInfo) ||
			(pTrailGroup->m_type==VFXTRAIL_TYPE_GENERIC && m_debugRenderGenericInfo) ||
			(pTrailGroup->m_type>=VFXTRAIL_TYPE_LIQUID && m_debugRenderLiquidInfo))
		{
			// go through the trail points and render them
			CDebugTrailPoint* pDebugTrailPoint = static_cast<CDebugTrailPoint*>(pTrailGroup->m_debugTrailPointList.GetHead());
			while (pDebugTrailPoint)
			{	
				CDebugTrailPoint* pNextDebugTrailPoint = static_cast<CDebugTrailPoint*>(pTrailGroup->m_debugTrailPointList.GetNext(pDebugTrailPoint));
				if (pNextDebugTrailPoint)
				{
					grcDebugDraw::Line(RCC_VECTOR3(pDebugTrailPoint->m_info.vWorldPos), RCC_VECTOR3(pNextDebugTrailPoint->m_info.vWorldPos), colGreen);
				}

				Color32 col = colYellow;
				if (pTrailGroup->m_debugTrailPointList.GetNumItems()==1)
				{
					col = colRed;
				}

				Vec3V vStartPos = pDebugTrailPoint->m_info.vWorldPos - Vec3V(0.0f, 0.0f, 0.05f);
				Vec3V vEndPos = pDebugTrailPoint->m_info.vWorldPos + Vec3V(0.0f, 0.0f, 0.05f);
				grcDebugDraw::Line(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos), col);

				vStartPos = pDebugTrailPoint->m_info.vWorldPos - Vec3V(0.0f, 0.05f, 0.0f);
				vEndPos = pDebugTrailPoint->m_info.vWorldPos + Vec3V(0.0f, 0.05f, 0.0f);
				grcDebugDraw::Line(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos), col);

				vStartPos = pDebugTrailPoint->m_info.vWorldPos - Vec3V(0.05f, 0.0f, 0.0f);
				vEndPos = pDebugTrailPoint->m_info.vWorldPos + Vec3V(0.05f, 0.0f, 0.0f);
				grcDebugDraw::Line(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos), col);

				// decal verts
				if (!IsZeroAll(pDebugTrailPoint->m_info.vPrevProjVerts[0]))
				{
					grcDebugDraw::Line(RCC_VECTOR3(pDebugTrailPoint->m_info.vPrevProjVerts[0]), RCC_VECTOR3(pDebugTrailPoint->m_info.vPrevProjVerts[1]), col);
					grcDebugDraw::Line(RCC_VECTOR3(pDebugTrailPoint->m_info.vPrevProjVerts[1]), RCC_VECTOR3(pDebugTrailPoint->m_info.vPrevProjVerts[2]), col);
					grcDebugDraw::Line(RCC_VECTOR3(pDebugTrailPoint->m_info.vPrevProjVerts[2]), RCC_VECTOR3(pDebugTrailPoint->m_info.vPrevProjVerts[3]), col);
					grcDebugDraw::Line(RCC_VECTOR3(pDebugTrailPoint->m_info.vPrevProjVerts[3]), RCC_VECTOR3(pDebugTrailPoint->m_info.vPrevProjVerts[0]), col);
				}


				pDebugTrailPoint = pNextDebugTrailPoint;
			}
		}

		pTrailGroup = static_cast<CTrailGroup*>(m_trailGroupList.GetNext(pTrailGroup));
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  ProcessDataValidity
///////////////////////////////////////////////////////////////////////////////

#if __ASSERT
void			CVfxTrail::ProcessDataValidity				(Mat34V_In mat, VfxTrailInfo_t& vfxTrailInfo, s32 originId)
{
	char errorString[256];
	bool foundError = false;

	if (!IsEqualAll(vfxTrailInfo.vLocalPos, vfxTrailInfo.vLocalPos) || 
		vfxTrailInfo.vLocalPos.GetXf() < WORLDLIMITS_REP_XMIN || 
		vfxTrailInfo.vLocalPos.GetXf() > WORLDLIMITS_REP_XMAX || 
		vfxTrailInfo.vLocalPos.GetYf() < WORLDLIMITS_REP_YMIN || 
		vfxTrailInfo.vLocalPos.GetYf() > WORLDLIMITS_REP_YMAX || 
		vfxTrailInfo.vLocalPos.GetZf() < WORLDLIMITS_ZMIN || 
		vfxTrailInfo.vLocalPos.GetZf() > WORLDLIMITS_ZMAX)
	{
		// local pos isn't valid
		foundError = true;
		sprintf(errorString, "local pos is not valid (%d)", originId);
	}

	if (foundError)
	{
		vfxErrorf("FxTrail Info:");
		vfxErrorf("World Pos: %.3f %.3f %.3f", VEC3V_ARGS(vfxTrailInfo.vWorldPos));
		vfxErrorf("Local Pos: %.3f %.3f %.3f", VEC3V_ARGS(vfxTrailInfo.vLocalPos));
		vfxErrorf("Matrix a: %.3f %.3f %.3f", VEC3V_ARGS(mat.GetCol0()));
		vfxErrorf("Matrix b: %.3f %.3f %.3f", VEC3V_ARGS(mat.GetCol1()));
		vfxErrorf("Matrix c: %.3f %.3f %.3f", VEC3V_ARGS(mat.GetCol2()));
		vfxErrorf("Matrix d: %.3f %.3f %.3f", VEC3V_ARGS(mat.GetCol3()));
		vfxErrorf("Matrix Index: %d", vfxTrailInfo.matrixIndex);

#if __BANK
		vfxErrorf("Orig Local Pos: %.3f %.3f %.3f", VEC3V_ARGS(vfxTrailInfo.vOrigLocalPos));
		vfxErrorf("Orig Has Entity Ptr: %d", vfxTrailInfo.origHasEntityPtr);
		if (vfxTrailInfo.hasEntityPtr && vfxTrailInfo.regdEnt)
		{
			vfxErrorf("Entity Name: %s", vfxTrailInfo.regdEnt->GetModelName());
		}
		vfxErrorf("Orig World Pos: %.3f %.3f %.3f", VEC3V_ARGS(vfxTrailInfo.vOrigWorldPos));
		vfxErrorf("Orig Matrix a: %.3f %.3f %.3f", VEC3V_ARGS(vfxTrailInfo.origEntityMatrix.GetCol0()));
		vfxErrorf("Orig Matrix b: %.3f %.3f %.3f", VEC3V_ARGS(vfxTrailInfo.origEntityMatrix.GetCol1()));
		vfxErrorf("Orig Matrix c: %.3f %.3f %.3f", VEC3V_ARGS(vfxTrailInfo.origEntityMatrix.GetCol2()));
		vfxErrorf("Orig Matrix d: %.3f %.3f %.3f", VEC3V_ARGS(vfxTrailInfo.origEntityMatrix.GetCol3()));
#endif

		vfxAssertf(0, "%s", errorString);
	}
}
#endif // __ASSERT


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void		 	CVfxTrail::InitWidgets					()
{
	u32 trailGroupSize = sizeof(CTrailGroup);
	float trailGroupPoolSize = (trailGroupSize * VFXTRAIL_MAX_GROUPS) / 1024.0f;

	char txt[128];
	sprintf(txt, "Num Groups (%d - %.2fK)", VFXTRAIL_MAX_GROUPS, trailGroupPoolSize);

	bkBank* pVfxBank = vfxWidget::GetBank();
	pVfxBank->PushGroup("Vfx Trail", false);
	{
		pVfxBank->AddTitle("INFO");
		pVfxBank->AddSlider(txt, &m_numTrailGroups, 0, VFXTRAIL_MAX_GROUPS, 0);

#if __DEV
		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("SETTINGS");
		pVfxBank->AddSlider("New Point Distance Min", &VFXTRAIL_NEW_PT_DIST_MIN, 0.0f, 50.0f, 0.01f, NullCB, "The minimum distance at which a new trail point is placed");
		pVfxBank->AddSlider("New Point Distance Max", &VFXTRAIL_NEW_PT_DIST_MAX, 0.0f, 50.0f, 0.01f, NullCB, "The maximum distance at which a new trail point is placed");
		pVfxBank->AddSlider("Nearby Range", &VFXTRAIL_NEARBY_RANGE, 0.0f, 5.0f, 0.01f, NullCB, "");
		pVfxBank->AddSlider("Default Stay Alive Time", &VFXTRAIL_STAYALIVETIME_DEFAULT, 0, 10000, 100, NullCB, "The time that a default trail point will stay active for");
		pVfxBank->AddSlider("PtFx Stay Alive Time", &VFXTRAIL_STAYALIVETIME_PTFX, 0, 10000, 100, NullCB, "The time that a particle trail point will stay active for");
#endif

		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("DEBUG");
		pVfxBank->AddSlider("Render Life (Single)", &m_debugRenderLifeSingle, 0.0f, 120.0f, 0.5f);
		pVfxBank->AddSlider("Render Life (Multiple)", &m_debugRenderLifeMultiple, 0.0f, 120.0f, 0.5f);

		pVfxBank->AddToggle("Render Skidmark Debug", &m_debugRenderSkidmarkInfo);
		pVfxBank->AddToggle("Render Scrape Debug", &m_debugRenderScrapeInfo);
		pVfxBank->AddToggle("Render Wade Debug", &m_debugRenderWadeInfo);
		//pVfxBank->AddToggle("Render Ski Debug", &m_debugRenderSkiInfo);
		pVfxBank->AddToggle("Render Generic Debug", &m_debugRenderGenericInfo);
		pVfxBank->AddToggle("Render Liquid Debug", &m_debugRenderLiquidInfo);

		pVfxBank->AddToggle("Disable Skidmark Trails", &m_disableSkidmarkTrails);
		pVfxBank->AddToggle("Disable Scrape Trails", &m_disableScrapeTrails);
		pVfxBank->AddToggle("Disable Wade Trails", &m_disableWadeTrails);
		pVfxBank->AddToggle("Disable Generic Trails", &m_disableGenericTrails);
		pVfxBank->AddToggle("Disable Liquid Trails", &m_disableLiquidTrails);
	}
	pVfxBank->PopGroup();
}
#endif

