///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	DecalDebug.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	13 May 2010
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#if __BANK


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "DecalDebug.h"

// rage
#include "bank/bank.h"
#include "phbound/boundcomposite.h"
#include "physics/gtainst.h"

// framework
#include "fwscene/stores/txdstore.h"
#include "vfx/channel.h"
#include "vfx/vfxutil.h"
#include "vfx/vfxwidget.h"
#include "vfx/decal/decaldebug.h"
#include "vfx/decal/decalmanager.h"

// game
#include "Camera/CamInterface.h"
#include "Network/Live/LiveManager.h"
#include "Network/Live/NetworkClan.h"
#include "Peds/Ped.h"
#include "Physics/Physics.h"
#include "Scene/World/GameWorld.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/VfxHelper.h"
#include "Vfx/Systems/VfxBlood.h"
#include "Vfx/Systems/VfxLiquid.h"
#include "Vfx/Systems/VfxWheel.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_DECAL_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

s32 CDecalDebug::ms_patchDiffuseMapId;
char CDecalDebug::ms_patchDiffuseMapName[64];
int CDecalDebug::ms_decalUnderPlayerId;

int CDecalDebug::ms_maxNonPlayerVehicleBangsScuffsPerFrame;

int CDecalDebug::ms_numberOfBadges;
int CDecalDebug::ms_vehicleBadgeIndex;
//UI values
int CDecalDebug::ms_vehicleBadgeBoneIndex;
float CDecalDebug::ms_vehicleBadgeOffsetX;
float CDecalDebug::ms_vehicleBadgeOffsetY;
float CDecalDebug::ms_vehicleBadgeOffsetZ;
float CDecalDebug::ms_vehicleBadgeDirX;
float CDecalDebug::ms_vehicleBadgeDirY;
float CDecalDebug::ms_vehicleBadgeDirZ;
float CDecalDebug::ms_vehicleBadgeSideX;
float CDecalDebug::ms_vehicleBadgeSideY;
float CDecalDebug::ms_vehicleBadgeSideZ;
float CDecalDebug::ms_vehicleBadgeSize;
u8 CDecalDebug::ms_vehicleBadgeAlpha;
//actual values sent to the decal manager etc
int CDecalDebug::ms_vehicleBadgeBoneIndices[DECAL_NUM_VEHICLE_BADGES];
Vec3V CDecalDebug::ms_vehicleBadgeOffsets[DECAL_NUM_VEHICLE_BADGES];
Vec3V CDecalDebug::ms_vehicleBadgeDirs[DECAL_NUM_VEHICLE_BADGES];
Vec3V CDecalDebug::ms_vehicleBadgeSides[DECAL_NUM_VEHICLE_BADGES];
float CDecalDebug::ms_vehicleBadgeSizes[DECAL_NUM_VEHICLE_BADGES];
u8 CDecalDebug::ms_vehicleBadgeAlphas[DECAL_NUM_VEHICLE_BADGES];
char CDecalDebug::ms_vehicleBadgeTxdName[64];
char CDecalDebug::ms_vehicleBadgeTextureName[64];

fwEntity* CDecalDebug::ms_pEntityA;
fwEntity* CDecalDebug::ms_pEntityB;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Class CDecalDebug
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::Init						()
{
	for (int i=0; i<DECALTYPE_NUM; i++)
	{
		m_disableDecalType[i] = false;
	}

	m_disableDuplicateRejectDistances = false;
	m_disableOverlays = false;

	m_alwaysUseDrawableIfAvailable = false;
	m_forceVehicleLod = false;
	m_disableVehicleShaderTest = false;
	m_disableVehicleBoneTest = false;
	m_useTestDiffuseMap = false;
	m_useTestDiffuseMap2 = false;
	m_overrideDiffuseMap = false;
	m_overrideDiffuseMapId = 9999;
	//m_emitPlayerLaserSight = false;
	m_emitPlayerMarker = false;
	m_playerMarkerSize = 2.0f;
	m_playerMarkerAlpha = 1.0f;
	m_playerMarkerPulse = false;
	m_sizeMult = 1.0f;
	m_reloadSettings = false;

	ms_patchDiffuseMapId = 9100;
	sprintf(ms_patchDiffuseMapName, "%s", "marker_lost_logo_hollow");
	ms_decalUnderPlayerId = 0;

	ms_maxNonPlayerVehicleBangsScuffsPerFrame = 1;

	ms_numberOfBadges = 1;
	ms_vehicleBadgeIndex = 0;
	ms_vehicleBadgeBoneIndex = 0;
	ms_vehicleBadgeOffsetX = 1.0f;
	ms_vehicleBadgeOffsetY = 0.0f;
	ms_vehicleBadgeOffsetZ = 0.0f;
	ms_vehicleBadgeDirX = -1.0f;
	ms_vehicleBadgeDirY = 0.0f;
	ms_vehicleBadgeDirZ = 0.0f;
	ms_vehicleBadgeSideX = 0.0f;
	ms_vehicleBadgeSideY = 1.0f;
	ms_vehicleBadgeSideZ = 0.0f;
	ms_vehicleBadgeSize = 0.25f;
	ms_vehicleBadgeAlpha = 200;
	for (int i=0; i<DECAL_NUM_VEHICLE_BADGES; i++)
	{
		ms_vehicleBadgeBoneIndices[i] = ms_vehicleBadgeBoneIndex;
		ms_vehicleBadgeOffsets[i] = Vec3V(ms_vehicleBadgeOffsetX, ms_vehicleBadgeOffsetY, ms_vehicleBadgeOffsetZ);
		ms_vehicleBadgeDirs[i] = Vec3V(ms_vehicleBadgeDirX,ms_vehicleBadgeDirY,ms_vehicleBadgeDirZ);
		ms_vehicleBadgeSides[i] = Vec3V(ms_vehicleBadgeSideX,ms_vehicleBadgeSideY,ms_vehicleBadgeSideZ);
		ms_vehicleBadgeSizes[i] = ms_vehicleBadgeSize;
		ms_vehicleBadgeAlphas[i] = ms_vehicleBadgeAlpha;
	}
	ms_vehicleBadgeTxdName[0] = '\0';
	ms_vehicleBadgeTextureName[0] = '\0';
	ms_vehicleBadgeProbeDebug = false;
	ms_vehicleBadgeSimulateCloudIssues = false;

	m_underwaterCheckWaterBodyHeightDiffMin = DECAL_UNDERWATER_WATER_BODY_HEIGHT_DIFF_MIN;
	m_enableUnderwaterCheckWaterBody = true;

	ms_pEntityA = NULL;
	ms_pEntityB = NULL;
}

///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::Update						()
{
	if (m_reloadSettings)
	{
		g_decalMan.ReloadSettings();
		m_reloadSettings = false;
	}

// 	if (m_emitPlayerLaserSight)
// 	{
// 		CPed* pPed = CGameWorld::FindLocalPlayer();
// 		if (pPed)
// 		{
// 			// get the component id of the player head
// 			s32 componentId = pPed->GetRagdollComponentFromBoneTag(BONETAG_HEAD);
// 
// 			// get the composite bound and check the composite id validity
// 			phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(pPed->GetRagdollInst()->GetArchetype()->GetBound());
// 			Assertf(componentId > -1 && componentId < pBoundComp->GetNumBounds(),"invalid component id");
// 
// 			Mat34V globalHeadMtx;
// 			Transform(globalHeadMtx, pPed->GetRagdollInst()->GetMatrix(), pBoundComp->GetCurrentMatrix(componentId));
// 			Vec3V vTestVecStart = globalHeadMtx.GetCol3();
// 			Vec3V vTestVecDir = -globalHeadMtx.GetCol2();
// 			//Vec3V vTestVecSide = globalHeadMtx.GetCol0();
// 
// 			// do a line test in the direction of the blood spray
// 			ScalarV vTestVecDist = ScalarVFromF32(30.0f);
// 			Vec3V vTestVecEnd = vTestVecStart + (vTestVecDir * vTestVecDist);
// 			u32 probeFlags = ArchetypeFlags::GTA_MAP_TYPE_WEAPON | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_RAGDOLL_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_PICKUP_TYPE | ArchetypeFlags::GTA_GLASS_TYPE;
// 
// 			WorldProbe::CShapeTestProbeDesc probeDesc;
// 			WorldProbe::CShapeTestHitPoint probeResult;
// 			WorldProbe::CShapeTestResults probeResults(probeResult);
// 			probeDesc.SetResultsStructure(&probeResults);
// 			probeDesc.SetStartAndEnd(RCC_VECTOR3(vTestVecStart), RCC_VECTOR3(vTestVecEnd));
// 			probeDesc.SetIncludeFlags(probeFlags);
// 			if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
// 			{
// 				g_decalMan.RegisterLaserSight(RCC_VEC3V(probeResults[0].GetHitPosition()), vTestVecDir);
// 			}
// 		}
// 	}

	if (m_emitPlayerMarker)
	{
		CPed* pPed = CGameWorld::FindLocalPlayer();
		if (pPed)
		{
			WorldProbe::CShapeTestHitPoint probeResult;
			WorldProbe::CShapeTestResults probeResults(probeResult);
			Vec3V vStartPos = pPed->GetTransform().GetPosition();
			Vec3V vEndPos = vStartPos - Vec3V(0.0f, 0.0f, 2.5f);

			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos));
			probeDesc.SetResultsStructure(&probeResults);
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER);
			if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
			{
				s32 decalRenderSettingIndex;
				s32 decalRenderSettingCount;
				g_decalMan.FindRenderSettingInfo(DECALID_TEST_ROUND, decalRenderSettingIndex, decalRenderSettingCount);

				Mat34V vMtx;
				Vec3V vPos = RCC_VEC3V(probeResults[0].GetHitPosition());
				Vec3V vNormal = -RCC_VEC3V(probeResults[0].GetHitNormal());
				Vec3V vAlign = pPed->GetTransform().GetMatrix().GetCol0();
				CVfxHelper::CreateMatFromVecZAlign(vMtx, vPos, vNormal, vAlign);

				// pulse the size
				float currSize = m_playerMarkerSize;
				if (m_playerMarkerPulse)
				{
					static float currSizeTimer = 0.0f;
					currSizeTimer += 5.0f * fwTimer::GetTimeStep();
					float sizeMult = (rage::Sinf(currSizeTimer)+1.0f) * 0.5f;
					float sizeMax = m_playerMarkerSize;
					float sizeMin = m_playerMarkerSize/2.0f;
					currSize = sizeMin + (sizeMult*(sizeMax-sizeMin));
				}

				Color32 col(255, 255, 255, static_cast<u8>(m_playerMarkerAlpha*255));

				g_decalMan.RegisterMarker(decalRenderSettingIndex, decalRenderSettingCount, RCC_VEC3V(probeResults[0].GetHitPosition()), vMtx.GetCol2(), vMtx.GetCol1(), currSize, currSize, col, false, false, false);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//  WashAroundPlayer
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::WashAroundPlayer					()
{
	CPed* pPed = CGameWorld::FindLocalPlayer();
	if (pPed)
	{
		DECALMGR.WashInRange(0.04f, pPed->GetTransform().GetPosition(), ScalarV(V_FIVE), NULL);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  WashPlayerVehicle
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::WashPlayerVehicle					()
{
	CVehicle* pVehicle = CGameWorld::FindLocalPlayerVehicle();
	if (pVehicle)
	{
		DECALMGR.Wash(0.04f, pVehicle, false);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveAroundPlayer
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::RemoveAroundPlayer					()
{
	CPed* pPed = CGameWorld::FindLocalPlayer();
	if (pPed)
	{
		DECALMGR.RemoveInRange(pPed->GetTransform().GetPosition(), ScalarV(V_FIVE), NULL, NULL);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveFromPlayerVehicle
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::RemoveFromPlayerVehicle			()
{
	CVehicle* pVehicle = CGameWorld::FindLocalPlayerVehicle();
	if (pVehicle)
	{
		DECALMGR.Remove(pVehicle, -1, NULL, 0);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  PlaceUnderPlayer
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::PlaceUnderPlayer					()
{
	CPed* pPed = CGameWorld::FindLocalPlayer();
	if (pPed)
	{
		Vector3 vGroundPos = pPed->GetGroundPos();
		Vec3V vPos = RCC_VEC3V(vGroundPos);
		Vec3V vDir = -Vec3V(V_Z_AXIS_WZERO);

		Color32 col(255, 255, 255, 255);

		s32 decalRenderSettingIndex;
		s32 decalRenderSettingCount;
		g_decalMan.FindRenderSettingInfo(DECALID_TEST_ROUND, decalRenderSettingIndex, decalRenderSettingCount);
		ms_decalUnderPlayerId = g_decalMan.AddGeneric(DECALTYPE_GENERIC_SIMPLE_COLN, decalRenderSettingIndex, decalRenderSettingCount, vPos, vDir, pPed->GetTransform().GetA(), 1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, col, false, false, 0.0f, false, false REPLAY_ONLY(, 0.0f));
	}
}


///////////////////////////////////////////////////////////////////////////////
//  PlaceInfrontOfCamera
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::PlaceInfrontOfCamera				()
{
	u32 probeFlags = ArchetypeFlags::GTA_MAP_TYPE_WEAPON | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_GLASS_TYPE;
	Vec3V_ConstRef vCamPos = RCC_VEC3V(camInterface::GetPos());
	Vec3V_ConstRef vCamForward = RCC_VEC3V(camInterface::GetFront());
	Vec3V_ConstRef vCamSide = RCC_VEC3V(camInterface::GetRight());
	Vec3V vProbeEnd = vCamPos+(vCamForward*ScalarVFromF32(20.0f));
	WorldProbe::CShapeTestHitPoint probeResult;
	WorldProbe::CShapeTestResults probeResults(probeResult);
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetIncludeFlags(probeFlags);
	probeDesc.SetStartAndEnd(RCC_VECTOR3(vCamPos), RCC_VECTOR3(vProbeEnd));
	if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		Vec3V vHitPos = RCC_VEC3V(probeResults[0].GetHitPosition());
		Vec3V vHitNormal = RCC_VEC3V(probeResults[0].GetHitNormal());

		Vec3V vCross = Cross(vHitNormal, vCamSide);
		vCross = Normalize(vCross);
		Vec3V vSide = Cross(vCross, vHitNormal);

		Color32 col(255, 255, 255, 255);

		s32 decalRenderSettingIndex;
		s32 decalRenderSettingCount;
		g_decalMan.FindRenderSettingInfo(DECALID_TEST_ROUND, decalRenderSettingIndex, decalRenderSettingCount);
		g_decalMan.AddGeneric(DECALTYPE_GENERIC_COMPLEX_COLN, decalRenderSettingIndex, decalRenderSettingCount, vHitPos, -vHitNormal, vSide, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, col, false, false, 0.0f, false, false REPLAY_ONLY(, 0.0f));
	}
}


///////////////////////////////////////////////////////////////////////////////
//  PlaceBloodSplatInfrontOfCamera
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::PlaceBloodSplatInfrontOfCamera			()
{
	phMaterialMgr::Id correctedMtlId = PGTAMATERIALMGR->g_idHead - PGTAMATERIALMGR->g_idButtocks;
	s32 infoIndex = 0;
	VfxBloodInfo_s* pVfxBloodInfo = g_vfxBlood.GetEntryDeadInfo(correctedMtlId, WEAPON_EFFECT_GROUP_SMG, infoIndex);
	if (pVfxBloodInfo)
	{
		u32 probeFlags = ArchetypeFlags::GTA_MAP_TYPE_WEAPON | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_GLASS_TYPE;
		Vec3V_ConstRef vCamPos = RCC_VEC3V(camInterface::GetPos());
		Vec3V_ConstRef vCamForward = RCC_VEC3V(camInterface::GetFront());
		Vec3V vProbeEnd = vCamPos+(vCamForward*ScalarVFromF32(20.0f));
		WorldProbe::CShapeTestHitPoint probeResult;
		WorldProbe::CShapeTestResults probeResults(probeResult);
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetResultsStructure(&probeResults);
		probeDesc.SetIncludeFlags(probeFlags);
		probeDesc.SetStartAndEnd(RCC_VECTOR3(vCamPos), RCC_VECTOR3(vProbeEnd));
		if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
		{
			CEntity* pHitEntity = CPhysics::GetEntityFromInst(probeResults[0].GetHitInst());
			if (pHitEntity)
			{
				VfxCollInfo_s vfxCollInfoBlood;
				vfxCollInfoBlood.regdEnt		= pHitEntity;
				vfxCollInfoBlood.vPositionWld	= RCC_VEC3V(probeResults[0].GetHitPosition());
				vfxCollInfoBlood.vNormalWld		= RCC_VEC3V(probeResults[0].GetHitNormal());
				vfxCollInfoBlood.vDirectionWld	= Vec3V(V_ZERO);
				vfxCollInfoBlood.materialId		= probeResults[0].GetHitMaterialId();
				vfxCollInfoBlood.componentId	= probeResults[0].GetHitComponent();
				vfxCollInfoBlood.roomId = -1;
				vfxCollInfoBlood.weaponGroup	= WEAPON_EFFECT_GROUP_PUNCH_KICK;		// shouldn't matter what this is set to at this point
				vfxCollInfoBlood.force			= probeResults[0].GetHitTValue();
				vfxCollInfoBlood.isBloody		= false;
				vfxCollInfoBlood.isWater		= false;
				vfxCollInfoBlood.isExitFx		= false;
				vfxCollInfoBlood.noPtFx			= false;
				vfxCollInfoBlood.noPedDamage   	= false;
				vfxCollInfoBlood.noDecal	   	= false; 
				vfxCollInfoBlood.isSnowball		= false;
				vfxCollInfoBlood.isFMJAmmo		= false;

				g_decalMan.AddBloodSplat(*pVfxBloodInfo, vfxCollInfoBlood, pVfxBloodInfo->sizeMaxAHi, false, false);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  PlaceBloodPoolInfrontOfCamera
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::PlaceBloodPoolInfrontOfCamera			()
{
	u32 probeFlags = ArchetypeFlags::GTA_MAP_TYPE_WEAPON | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_GLASS_TYPE;
	Vec3V_ConstRef vCamPos = RCC_VEC3V(camInterface::GetPos());
	Vec3V_ConstRef vCamForward = RCC_VEC3V(camInterface::GetFront());
	Vec3V vProbeEnd = vCamPos+(vCamForward*ScalarVFromF32(20.0f));
	WorldProbe::CShapeTestHitPoint probeResult;
	WorldProbe::CShapeTestResults probeResults(probeResult);
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetIncludeFlags(probeFlags);
	probeDesc.SetStartAndEnd(RCC_VECTOR3(vCamPos), RCC_VECTOR3(vProbeEnd));
	if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		CEntity* pHitEntity = CPhysics::GetEntityFromInst(probeResults[0].GetHitInst());
		if (pHitEntity)
		{
			g_decalMan.AddPool(VFXLIQUID_TYPE_BLOOD, -1, 0, probeResults[0].GetHitPositionV(), probeResults[0].GetHitNormalV(), 0.1f, 1.0f, 0.1f, (u8)probeResults[0].GetHitMaterialId(), Color32(1.0f, 1.0f, 1.0f, 1.0f), false);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  PlaceBangInfrontOfCamera
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::PlaceBangInfrontOfCamera			()
{
	u32 probeFlags = ArchetypeFlags::GTA_MAP_TYPE_WEAPON | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_GLASS_TYPE;
	Vec3V_ConstRef vCamPos = RCC_VEC3V(camInterface::GetPos());
	Vec3V_ConstRef vCamForward = RCC_VEC3V(camInterface::GetFront());
	Vec3V vProbeEnd = vCamPos+(vCamForward*ScalarVFromF32(20.0f));
	WorldProbe::CShapeTestHitPoint probeResult;
	WorldProbe::CShapeTestResults probeResults(probeResult);
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetIncludeFlags(probeFlags);
	probeDesc.SetStartAndEnd(RCC_VECTOR3(vCamPos), RCC_VECTOR3(vProbeEnd));
	if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		CEntity* pHitEntity = CPhysics::GetEntityFromInst(probeResults[0].GetHitInst());
		if (pHitEntity)
		{
			VfxCollInfo_s vfxCollInfoBang;
			vfxCollInfoBang.regdEnt			= pHitEntity;
			vfxCollInfoBang.vPositionWld	= RCC_VEC3V(probeResults[0].GetHitPosition());
			vfxCollInfoBang.vNormalWld		= RCC_VEC3V(probeResults[0].GetHitNormal());
			vfxCollInfoBang.vDirectionWld	= Vec3V(V_ZERO);
			vfxCollInfoBang.materialId		= probeResults[0].GetHitMaterialId();
			vfxCollInfoBang.componentId		= probeResults[0].GetHitComponent();
			vfxCollInfoBang.roomId			= -1;
			vfxCollInfoBang.weaponGroup		= WEAPON_EFFECT_GROUP_PUNCH_KICK;		// shouldn't matter what this is set to at this point
			vfxCollInfoBang.force			= probeResults[0].GetHitTValue();
			vfxCollInfoBang.isBloody		= false;
			vfxCollInfoBang.isWater			= false;
			vfxCollInfoBang.isExitFx		= false;
			vfxCollInfoBang.noPtFx			= false;
			vfxCollInfoBang.isSnowball		= false;
			vfxCollInfoBang.isFMJAmmo		= false;

			VfxGroup_e vfxGroup = PGTAMATERIALMGR->GetMtlVfxGroup(probeResults[0].GetHitMaterialId());
			VfxMaterialInfo_s* pVfxMtlInfo = g_vfxMaterial.GetBangInfo(vfxGroup, VFXGROUP_CAR_METAL);
			if (pVfxMtlInfo)
			{
				g_decalMan.AddBang(*pVfxMtlInfo, vfxCollInfoBang);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  PlacePlayerVehicleBadge
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::PlacePlayerVehicleBadge	()
{
	CPed* pPed = CGameWorld::FindLocalPlayer();
	if (pPed)
	{
		CVehicle* pVehicle = CGameWorld::FindLocalPlayerVehicle();
		if (pVehicle)
		{
			for (unsigned int i=0;i<ms_numberOfBadges;i++)
			{
				ms_vehicleBadgeDirs[i] = Normalize(ms_vehicleBadgeDirs[i]);
				ms_vehicleBadgeSides[i] = Normalize(ms_vehicleBadgeSides[i]);

				if (ms_vehicleBadgeTextureName[0]!='\0')
				{
					CVehicleBadgeDesc badgeDesc;
					badgeDesc.Set(atHashWithStringNotFinal(ms_vehicleBadgeTxdName), atHashWithStringNotFinal(ms_vehicleBadgeTextureName));

					g_decalMan.ProcessVehicleBadge(pVehicle, badgeDesc, ms_vehicleBadgeBoneIndices[i], ms_vehicleBadgeOffsets[i], ms_vehicleBadgeDirs[i], ms_vehicleBadgeSides[i], ms_vehicleBadgeSizes[i], true, i, ms_vehicleBadgeAlphas[i]);
				}
				else
				{
					CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();
					if (pPlayerInfo)
					{
						NetworkClan& clanMgr = CLiveManager::GetNetworkClan();
						rlGamerHandle gamerHandle = pPlayerInfo->m_GamerInfo.GetGamerHandle();
						const rlClanDesc* pMyClan = clanMgr.GetPrimaryClan(gamerHandle);
						if (pMyClan && pMyClan->IsValid())
						{
							EmblemDescriptor emblemDesc(EmblemDescriptor::EMBLEM_CLAN, (EmblemId)pMyClan->m_Id);

							CVehicleBadgeDesc badgeDesc;
							badgeDesc.Set(emblemDesc);

							g_decalMan.ProcessVehicleBadge(pVehicle, badgeDesc, ms_vehicleBadgeBoneIndices[i], ms_vehicleBadgeOffsets[i], ms_vehicleBadgeDirs[i], ms_vehicleBadgeSides[i], ms_vehicleBadgeSizes[i], true, i, ms_vehicleBadgeAlphas[i]);
						}
					}
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RemovePlayerVehicleBadge
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::RemovePlayerVehicleBadge	()
{
	CPed* pPed = CGameWorld::FindLocalPlayer();
	if (pPed)
	{
		CVehicle* pVehicle = CGameWorld::FindLocalPlayerVehicle();
		if (pVehicle)
		{
			g_decalMan.RemoveAllVehicleBadges(pVehicle);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  AbortPlayerVehicleBadge
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::AbortPlayerVehicleBadge	()
{
	CPed* pPed = CGameWorld::FindLocalPlayer();
	if (pPed)
	{
		CVehicle* pVehicle = CGameWorld::FindLocalPlayerVehicle();
		if (pVehicle)
		{
			g_decalMan.AbortVehicleBadgeRequests(pVehicle);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveThenPlacePlayerVehicleBadge
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::RemoveThenPlacePlayerVehicleBadge	()
{
	RemovePlayerVehicleBadge();
	PlacePlayerVehicleBadge();
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveCompletedVehicleBadgeRequests
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::RemoveCompletedVehicleBadgeRequests()
{
	g_decalMan.RemoveCompletedVehicleBadgeRequests();
}


///////////////////////////////////////////////////////////////////////////////
//  PlaceTestSkidmark
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::PlaceTestSkidmark			()
{
	// start the skid
	static float width = 0.5f;

	VfxWheelInfo_s* pVfxWheelInfo = g_vfxWheel.GetInfo(VFXTYRESTATE_OK_DRY, VFXGROUP_TARMAC);
	s32 renderSettingsIndex = g_decalMan.GetRenderSettingsIndex(pVfxWheelInfo->decal1RenderSettingIndex, pVfxWheelInfo->decal1RenderSettingCount);

	Color32 col(255, 255, 255, 255);

	g_decalMan.StartScriptedTrail(DECALTYPE_TRAIL_SKID, VFXLIQUID_TYPE_NONE, renderSettingsIndex, width, col);

	// add the skid points
	static int numSkidmarksPoints = 16;

	Vec3V vBasePos = Vec3V(60.0f, 0.0f, 6.17f);
	static float radius = 5.0f;

	float currAlpha = 0.0f;
	const s32 NDIV2 = numSkidmarksPoints / 2;
	float alphaStep = 1.0f/NDIV2;

	for (int i=0; i<numSkidmarksPoints; i++)
	{
		float sinVal = sinf(((float)i/(float)numSkidmarksPoints) * PI);
		float cosVal = cosf(((float)i/(float)numSkidmarksPoints) * PI);

		Vec3V vCurrPos = vBasePos;
		vCurrPos += Vec3V(radius*sinVal, radius*cosVal, 0.0f);

		g_decalMan.AddScriptedTrailInfo(vCurrPos, currAlpha, false);

		if (i<NDIV2)
		{
			currAlpha += alphaStep;
		}
		else
		{
			currAlpha -= alphaStep;
		}
	}

	// end the skid
	g_decalMan.EndScriptedTrail();
}


///////////////////////////////////////////////////////////////////////////////
//  PlaceTestPetrolTrail
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::PlaceTestPetrolTrail		()
{
	// start the skid
	static float width = 0.5f;

	s32 renderSettingsIndex = -1;

	VfxLiquidInfo_s& liquidInfo = g_vfxLiquid.GetLiquidInfo(VFXLIQUID_TYPE_PETROL);
	Color32 col(liquidInfo.decalColR, liquidInfo.decalColG, liquidInfo.decalColB, liquidInfo.decalColA);

	g_decalMan.StartScriptedTrail(DECALTYPE_TRAIL_LIQUID, VFXLIQUID_TYPE_PETROL, renderSettingsIndex, width, col);

	// add the skid points
	static int numSkidmarksPoints = 16;

	Vec3V vBasePos = Vec3V(60.0f, 0.0f, 6.17f);
	static float radius = 5.0f;

	float currAlpha = 0.0f;
	const s32 NDIV2 = numSkidmarksPoints / 2;
	float alphaStep = 1.0f/NDIV2;

	for (int i=0; i<numSkidmarksPoints; i++)
	{
		float sinVal = sinf(((float)i/(float)numSkidmarksPoints) * PI);
		float cosVal = cosf(((float)i/(float)numSkidmarksPoints) * PI);

		Vec3V vCurrPos = vBasePos;
		vCurrPos += Vec3V(radius*sinVal, radius*cosVal, 0.0f);

		g_decalMan.AddScriptedTrailInfo(vCurrPos, currAlpha, false);

		if (i<NDIV2)
		{
			currAlpha += alphaStep;
		}
		else
		{
			currAlpha -= alphaStep;
		}
	}

	// end the skid
	g_decalMan.EndScriptedTrail();
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveFromUnderPlayer
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::RemoveFromUnderPlayer				()
{
	if (ms_decalUnderPlayerId>0)
	{
		g_decalMan.Remove(ms_decalUnderPlayerId);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  PatchDiffuseMap
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::PatchDiffuseMap					()
{
	strLocalIndex txdSlot = strLocalIndex(vfxUtil::InitTxd("fxdecal"));

	grcTexture* pDiffuseMap = g_TxdStore.Get(txdSlot)->Lookup(ms_patchDiffuseMapName);
	g_decalMan.PatchDiffuseMap(ms_patchDiffuseMapId, pDiffuseMap);

	vfxUtil::ShutdownTxd("fxdecal");
}


///////////////////////////////////////////////////////////////////////////////
//  UnPatchDiffuseMap
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::UnPatchDiffuseMap					()
{
	grcTexture* pDiffuseMap = const_cast<grcTexture*>(grcTexture::None);
	g_decalMan.PatchDiffuseMap(ms_patchDiffuseMapId, pDiffuseMap);
}


///////////////////////////////////////////////////////////////////////////////
//  StoreEntityA
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::StoreEntityA						()
{
	CVehicle* pVehicle = CGameWorld::FindLocalPlayerVehicle();
	if (pVehicle)
	{
		ms_pEntityA = pVehicle;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  StoreEntityB
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::StoreEntityB						()
{
	CVehicle* pVehicle = CGameWorld::FindLocalPlayerVehicle();
	if (pVehicle)
	{
		ms_pEntityB = pVehicle;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  MoveFromAToB
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::MoveFromAToB						()
{
	if (ms_pEntityA && ms_pEntityB)
	{
		g_decalMan.UpdateAttachEntity(ms_pEntityA, ms_pEntityB);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  MoveFromBToA
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::MoveFromBToA						()
{
	if (ms_pEntityA && ms_pEntityB)
	{
		g_decalMan.UpdateAttachEntity(ms_pEntityB, ms_pEntityA);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ReloadSettings
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::ReloadSettings						()
{
	g_decalMan.GetDebugInterface().m_reloadSettings = true;
}


///////////////////////////////////////////////////////////////////////////////
//  OutputPlayerVehicleWindscreenMatrix
///////////////////////////////////////////////////////////////////////////////

#if DECAL_SHOW_BROKEN_FRAG_MTX_PROBLEM
void			CDecalDebug::OutputPlayerVehicleWindscreenMatrix	()
{
	CVehicle* pVehicle = CGameWorld::FindLocalPlayerVehicle();
	if (pVehicle)
	{
		CVehicleModelInfo* pModelInfo = pVehicle->GetVehicleModelInfo();
		s32 windscreenBoneId = pModelInfo->GetBoneIndex(VEH_WINDSCREEN);
		if (windscreenBoneId>-1)
		{
			Mat34V windscreenMtx;
			CVfxHelper::GetMatrixFromBoneIndex_Skinned(windscreenMtx, pVehicle, windscreenBoneId);
			decalDebugf1("player vehicle windscreen mtx = %.3f %.3f %.3f", windscreenMtx.GetCol0().GetXf(), windscreenMtx.GetCol0().GetYf(), windscreenMtx.GetCol0().GetZf());
		}
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  Vehicle Badge Callbacks
///////////////////////////////////////////////////////////////////////////////

//called when the badge index changes 
void CDecalDebug::BadgeIndexCallback(void)
{
	ms_vehicleBadgeBoneIndex = ms_vehicleBadgeBoneIndices[ms_vehicleBadgeIndex];
	ms_vehicleBadgeOffsetX = ms_vehicleBadgeOffsets[ms_vehicleBadgeIndex].GetXf();
	ms_vehicleBadgeOffsetY = ms_vehicleBadgeOffsets[ms_vehicleBadgeIndex].GetYf();
	ms_vehicleBadgeOffsetZ = ms_vehicleBadgeOffsets[ms_vehicleBadgeIndex].GetZf();
	ms_vehicleBadgeDirX = ms_vehicleBadgeDirs[ms_vehicleBadgeIndex].GetXf();
	ms_vehicleBadgeDirY = ms_vehicleBadgeDirs[ms_vehicleBadgeIndex].GetYf();
	ms_vehicleBadgeDirZ = ms_vehicleBadgeDirs[ms_vehicleBadgeIndex].GetZf();
	ms_vehicleBadgeSideX = ms_vehicleBadgeSides[ms_vehicleBadgeIndex].GetXf();
	ms_vehicleBadgeSideY = ms_vehicleBadgeSides[ms_vehicleBadgeIndex].GetYf();
	ms_vehicleBadgeSideZ = ms_vehicleBadgeSides[ms_vehicleBadgeIndex].GetZf();
	ms_vehicleBadgeSize = ms_vehicleBadgeSizes[ms_vehicleBadgeIndex];
	ms_vehicleBadgeAlpha = ms_vehicleBadgeAlphas[ms_vehicleBadgeIndex];
}

//called when the slider changes 
void CDecalDebug::BadgeSliderCallback(void)
{
	ms_vehicleBadgeBoneIndices[ms_vehicleBadgeIndex] = ms_vehicleBadgeBoneIndex;
	ms_vehicleBadgeOffsets[ms_vehicleBadgeIndex] = Vec3V(ms_vehicleBadgeOffsetX, ms_vehicleBadgeOffsetY, ms_vehicleBadgeOffsetZ);
	ms_vehicleBadgeDirs[ms_vehicleBadgeIndex] = Vec3V(ms_vehicleBadgeDirX,ms_vehicleBadgeDirY,ms_vehicleBadgeDirZ);
	ms_vehicleBadgeSides[ms_vehicleBadgeIndex] = Vec3V(ms_vehicleBadgeSideX,ms_vehicleBadgeSideY,ms_vehicleBadgeSideZ);
	ms_vehicleBadgeSizes[ms_vehicleBadgeIndex] = ms_vehicleBadgeSize;
	ms_vehicleBadgeAlphas[ms_vehicleBadgeIndex] = ms_vehicleBadgeAlpha;
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

void			CDecalDebug::InitWidgets						()
{
	bkBank* pVfxBank = vfxWidget::GetBank();
	bkGroup* pGroup = DECALMGR.DebugGetGameGroup();

	pVfxBank->SetCurrentGroup(*pGroup);
	{
#if __DEV
		pVfxBank->AddTitle("SETTINGS");

		pVfxBank->AddSlider("Vehicle Badge - Tint R", &DECAL_VEHICLE_BADGE_TINT_R, 0, 255, 1);
		pVfxBank->AddSlider("Vehicle Badge - Tint G", &DECAL_VEHICLE_BADGE_TINT_G, 0, 255, 1);
		pVfxBank->AddSlider("Vehicle Badge - Tint B", &DECAL_VEHICLE_BADGE_TINT_B, 0, 255, 1);

		pVfxBank->AddSlider("Blood Splat Duplicate Reject Dist Width Mult", &DECAL_BLOOD_SPLAT_DRD_WIDTH_MULT, 0.0f, 2.0f, 0.01f);
#endif

		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("DEBUG");

		pVfxBank->AddToggle("Disable Bangs", &m_disableDecalType[DECALTYPE_BANG]);
		pVfxBank->AddToggle("Disable Blood Splats", &m_disableDecalType[DECALTYPE_BLOOD_SPLAT]);
		pVfxBank->AddToggle("Disable Footprints", &m_disableDecalType[DECALTYPE_FOOTPRINT]);
		pVfxBank->AddToggle("Disable Generic (Simple)", &m_disableDecalType[DECALTYPE_GENERIC_SIMPLE_COLN]);
		pVfxBank->AddToggle("Disable Generic (Complex)", &m_disableDecalType[DECALTYPE_GENERIC_COMPLEX_COLN]);
		pVfxBank->AddToggle("Disable Generic (Simple - Long Range)", &m_disableDecalType[DECALTYPE_GENERIC_SIMPLE_COLN_LONG_RANGE]);
		pVfxBank->AddToggle("Disable Generic (Complex - Long Range)", &m_disableDecalType[DECALTYPE_GENERIC_COMPLEX_COLN_LONG_RANGE]);
		pVfxBank->AddToggle("Disable Liquid Pools", &m_disableDecalType[DECALTYPE_POOL_LIQUID]);
		pVfxBank->AddToggle("Disable Scuffs", &m_disableDecalType[DECALTYPE_SCUFF]);
		pVfxBank->AddToggle("Disable Scrape Trails", &m_disableDecalType[DECALTYPE_TRAIL_SCRAPE]);
		pVfxBank->AddToggle("Disable Liquid Trails", &m_disableDecalType[DECALTYPE_TRAIL_LIQUID]);
		pVfxBank->AddToggle("Disable Skid Trails", &m_disableDecalType[DECALTYPE_TRAIL_SKID]);
		pVfxBank->AddToggle("Disable Generic Trails", &m_disableDecalType[DECALTYPE_TRAIL_GENERIC]);

		char txt[64];
		for (int i=0; i<DECAL_NUM_VEHICLE_BADGES; i++)
		{
			sprintf(txt, "Disable Vehicle Badge %d", i);
			pVfxBank->AddToggle(txt, &m_disableDecalType[DECALTYPE_VEHICLE_BADGE+i]);
		}

		pVfxBank->AddToggle("Disable Weapon Impacts", &m_disableDecalType[DECALTYPE_WEAPON_IMPACT]);
		pVfxBank->AddToggle("Disable Weapon Impacts (Shotgun)", &m_disableDecalType[DECALTYPE_WEAPON_IMPACT_SHOTGUN]);
		pVfxBank->AddToggle("Disable Weapon Impacts (Smash Group)", &m_disableDecalType[DECALTYPE_WEAPON_IMPACT_SMASHGROUP]);
		pVfxBank->AddToggle("Disable Weapon Impacts (Smash Group Shotgun)", &m_disableDecalType[DECALTYPE_WEAPON_IMPACT_SMASHGROUP_SHOTGUN]);
		pVfxBank->AddToggle("Disable Weapon Impacts (Explosion Ground)", &m_disableDecalType[DECALTYPE_WEAPON_IMPACT_EXPLOSION_GROUND]);
		pVfxBank->AddToggle("Disable Weapon Impacts (Explosion Vehicle)", &m_disableDecalType[DECALTYPE_WEAPON_IMPACT_EXPLOSION_VEHICLE]);
		pVfxBank->AddToggle("Disable Weapon Impacts (Vehicle)", &m_disableDecalType[DECALTYPE_WEAPON_IMPACT_VEHICLE]);
		//pVfxBank->AddToggle("Disable Laser Sight", &m_disableDecalType[DECALTYPE_LASER_SIGHT]);
		pVfxBank->AddToggle("Disable Marker", &m_disableDecalType[DECALTYPE_MARKER]);
		pVfxBank->AddToggle("Disable Marker (Long Range)", &m_disableDecalType[DECALTYPE_MARKER_LONG_RANGE]);

		pVfxBank->AddToggle("Disable Duplicate Reject Distances", &m_disableDuplicateRejectDistances);
		pVfxBank->AddToggle("Disable Overlays", &m_disableOverlays);

		pVfxBank->AddToggle("Always Use Drawable (If Available)", &m_alwaysUseDrawableIfAvailable);
		pVfxBank->AddToggle("Force Vehicle Lod", &m_forceVehicleLod);
		pVfxBank->AddToggle("Disable Vehicle Shader Test", &m_disableVehicleShaderTest);
		pVfxBank->AddToggle("Disable Vehicle Bone Test", &m_disableVehicleBoneTest);
		pVfxBank->AddToggle("Use Test Diffuse Map 2", &m_useTestDiffuseMap2);
		pVfxBank->AddToggle("Override Diffuse Map", &m_overrideDiffuseMap);
		pVfxBank->AddSlider("Override Diffuse Map Id", &m_overrideDiffuseMapId, 0, 9999, 1);
		//pVfxBank->AddToggle("Emit Player Laser Sight", &m_emitPlayerLaserSight);
		pVfxBank->AddToggle("Emit Player Marker", &m_emitPlayerMarker);
		pVfxBank->AddSlider("Player Marker Size", &m_playerMarkerSize, 0.0f, 20.0f, 0.1f);
		pVfxBank->AddSlider("Player Marker Alpha", &m_playerMarkerAlpha, 0.0f, 1.0f, 0.01f);
		pVfxBank->AddToggle("Player Marker Pulse", &m_playerMarkerPulse);
		pVfxBank->AddSlider("Size Multiplier", &m_sizeMult, 0.0f, 20.0f, 0.1f);
		pVfxBank->AddButton("Wash Around Player", WashAroundPlayer);
		pVfxBank->AddButton("Wash Player Vehicle", WashPlayerVehicle);
		pVfxBank->AddButton("Remove Around Player", RemoveAroundPlayer);
		pVfxBank->AddButton("Remove From Player Vehicle", RemoveFromPlayerVehicle);
		pVfxBank->AddButton("Place Under Player", PlaceUnderPlayer);
		pVfxBank->AddButton("Remove From Under Player", RemoveFromUnderPlayer);
		pVfxBank->AddButton("Place Infront Of Camera", PlaceInfrontOfCamera);
		pVfxBank->AddButton("Place Blood Splat Infront Of Camera", PlaceBloodSplatInfrontOfCamera);
		pVfxBank->AddButton("Place Blood Pool Infront Of Camera", PlaceBloodPoolInfrontOfCamera);
		pVfxBank->AddButton("Place Bang Infront Of Camera", PlaceBangInfrontOfCamera);

		pVfxBank->AddToggle("Enable Water Body Check", &m_enableUnderwaterCheckWaterBody);
		pVfxBank->AddSlider("Distance from Water body for Underwater Check", &m_underwaterCheckWaterBodyHeightDiffMin, 0.0f, 100.0f, 0.001f);

		pVfxBank->AddSlider("Max Non Player Vehicle Bangs & Scuffs Per Frame", &ms_maxNonPlayerVehicleBangsScuffsPerFrame, 0, 32, 1);

		pVfxBank->AddSlider("Vehicle Badge - # Badges", &ms_numberOfBadges, 1, DECAL_NUM_VEHICLE_BADGES, 1);
		pVfxBank->AddSlider("Vehicle Badge - Badge Index", &ms_vehicleBadgeIndex, 0, DECAL_NUM_VEHICLE_BADGES-1, 1, datCallback(BadgeIndexCallback));
		pVfxBank->AddSlider("Vehicle Badge - Bone Index", &ms_vehicleBadgeBoneIndex, 0, 128, 1, datCallback(BadgeSliderCallback));
		pVfxBank->AddSlider("Vehicle Badge - OffsetX", &ms_vehicleBadgeOffsetX, -5.0f, 5.0f, 0.01f, datCallback(BadgeSliderCallback));
		pVfxBank->AddSlider("Vehicle Badge - OffsetY", &ms_vehicleBadgeOffsetY, -5.0f, 5.0f, 0.01f, datCallback(BadgeSliderCallback));
		pVfxBank->AddSlider("Vehicle Badge - OffsetZ", &ms_vehicleBadgeOffsetZ, -5.0f, 5.0f, 0.01f, datCallback(BadgeSliderCallback));
		pVfxBank->AddSlider("Vehicle Badge - DirX", &ms_vehicleBadgeDirX, -5.0f, 5.0f, 0.01f, datCallback(BadgeSliderCallback));
		pVfxBank->AddSlider("Vehicle Badge - DirY", &ms_vehicleBadgeDirY, -5.0f, 5.0f, 0.01f, datCallback(BadgeSliderCallback));
		pVfxBank->AddSlider("Vehicle Badge - DirZ", &ms_vehicleBadgeDirZ, -5.0f, 5.0f, 0.01f, datCallback(BadgeSliderCallback));
		pVfxBank->AddSlider("Vehicle Badge - SideX", &ms_vehicleBadgeSideX, -5.0f, 5.0f, 0.01f, datCallback(BadgeSliderCallback));
		pVfxBank->AddSlider("Vehicle Badge - SideY", &ms_vehicleBadgeSideY, -5.0f, 5.0f, 0.01f, datCallback(BadgeSliderCallback));
		pVfxBank->AddSlider("Vehicle Badge - SideZ", &ms_vehicleBadgeSideZ, -5.0f, 5.0f, 0.01f, datCallback(BadgeSliderCallback));
		pVfxBank->AddSlider("Vehicle Badge - Size", &ms_vehicleBadgeSize, 0.01f, 2.0f, 0.01f, datCallback(BadgeSliderCallback));
		pVfxBank->AddSlider("Vehicle Badge - Alpha", &ms_vehicleBadgeAlpha, 0, 255, 1, datCallback(BadgeSliderCallback));
		pVfxBank->AddToggle("Vehicle Badge - Probe Debug ", &ms_vehicleBadgeProbeDebug);
		pVfxBank->AddText("Vehicle Badge - Txd Name", ms_vehicleBadgeTxdName, 64);
		pVfxBank->AddText("Vehicle Badge - Texture Name", ms_vehicleBadgeTextureName, 64);
		pVfxBank->AddToggle("Simulate Badge Cloud Issues ", &ms_vehicleBadgeSimulateCloudIssues);
		pVfxBank->AddButton("Place Badge On Player Vehicle", PlacePlayerVehicleBadge);
		pVfxBank->AddButton("Remove Badge On Player Vehicle", RemovePlayerVehicleBadge);
		pVfxBank->AddButton("Abort Badge On Player Vehicle", AbortPlayerVehicleBadge);
		pVfxBank->AddButton("Remove Then Place Badge On Player Vehicle", RemoveThenPlacePlayerVehicleBadge);
		pVfxBank->AddButton("Remove Completed Vehicle Badge Requests", RemoveCompletedVehicleBadgeRequests);

		pVfxBank->AddButton("Place Test Skidmark", PlaceTestSkidmark);
		pVfxBank->AddButton("Place Test Petrol Trail", PlaceTestPetrolTrail);
		pVfxBank->AddSlider("Patch Diffuse Map Id", &ms_patchDiffuseMapId, 0, 99999, 1);
		pVfxBank->AddText("Patch Diffuse Map Name", ms_patchDiffuseMapName, 64);
		pVfxBank->AddButton("Patch Diffuse Map", PatchDiffuseMap);
		pVfxBank->AddButton("Unpatch Diffuse Map", UnPatchDiffuseMap);

		pVfxBank->AddButton("Set Player Vehicle As Entity A", StoreEntityA);
		pVfxBank->AddButton("Set Player Vehicle As Entity B", StoreEntityB);
		pVfxBank->AddButton("Move Decals From Entity A To B", MoveFromAToB);
		pVfxBank->AddButton("Move Decals From Entity B To A", MoveFromBToA);

		pVfxBank->AddButton("Reload Settings", ReloadSettings);
	}
	pVfxBank->UnSetCurrentGroup(*pGroup);	
}

#endif // __BANK

