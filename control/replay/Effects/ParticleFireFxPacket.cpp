//=====================================================================================================
// name:		ParticleFireFxPacket.cpp
//=====================================================================================================

#include "ParticleFireFxPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/ReplayInternal.h"
#include "control/replay/Misc/ReplayPacket.h"
#include "control/replay/replayinterfaceped.h"

#include "vfx/channel.h"
#include "vfx/systems/VfxFire.h"
#include "vfx/misc/Fire.h"
#include "vfx/particles/PtFxDefines.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxconfig.h"
#include "vfx/ptfx/ptfxwrapper.h"
#include "Vfx/Systems/VfxProjectile.h"
#include "vfx/systems/VfxWheel.h"
#include "vfx/systems/VfxVehicle.h"
#include "control/replay/Compression/Quantization.h"

CompileTimeAssert(sizeof(size_t) >= sizeof(void*)); // If this fails redo the ptxFireRef stuff below as we're storing a pointer in a size_t type

//========================================================================================================================
CPacketFireSmokeFx::CPacketFireSmokeFx(u32 uPfxHash)
	: CPacketEvent(PACKETID_FIRESMOKEPEDFX, sizeof(CPacketFireSmokeFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{}

//========================================================================================================================
void CPacketFireSmokeFx::Extract(const CEventInfo<CPed>& eventInfo) const
{
	CPed* pPed = eventInfo.GetEntity();
	if (pPed == NULL)
		return;

	if (m_pfxHash != ATSTRINGHASH("fire_ped_smoke", 0x64C1DA06))
	{
		replayAssertf(false, "Hash is not fire_ped_smoke in CPacketFireSmokeFx::Extract");
	}

	// register the fx system
	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		// set position of effect
		s32 boneIndex1 = CPedBoneTagConvertor::GetBoneIndexFromBoneTag(pPed->GetSkeletonData(), BONETAG_SPINE1);
		s32 boneIndex2 = CPedBoneTagConvertor::GetBoneIndexFromBoneTag(pPed->GetSkeletonData(), BONETAG_SPINE3);
		Matrix34 pBoneMatrix1, pBoneMatrix2;
		pPed->GetGlobalMtx(boneIndex1, pBoneMatrix1);
		pPed->GetGlobalMtx(boneIndex2, pBoneMatrix2);

		Vector3 dir = pBoneMatrix2.d - pBoneMatrix1.d;
		Vector3 pos = pBoneMatrix1.d + (dir*0.5f);
		dir.Normalize();

		Mat34V fxMat;
		CVfxHelper::CreateMatFromVecY(fxMat, VECTOR3_TO_VEC3V(pos), VECTOR3_TO_VEC3V(dir));

		// make the fx matrix relative to the bone 1
		Mat34V invEntityMat = MATRIX34_TO_MAT34V(pBoneMatrix1);
		Invert3x3Full(invEntityMat, invEntityMat);
		
		Mat34V relFxMat = fxMat;
		Transform(relFxMat, invEntityMat);
		
		pFxInst->SetOffsetMtx(relFxMat);
		ptfxAttach::Attach(pFxInst, pPed, -1);

		pFxInst->Start();
#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: FIRE SMOKE FX: %0X: ID: %0X\n", (u32)pPed, pPed->GetReplayID());
#endif
	}
}


//========================================================================================================================
CPacketFireVehWreckedFx::CPacketFireVehWreckedFx(u32 uPfxHash, float fStrength)
	: CPacketEventInterp(PACKETID_FIREVEHWRECKEDFX, sizeof(CPacketFireVehWreckedFx))
	, m_pfxHash(uPfxHash)
{
	replayAssert(false && "There is no PTFXOFFSET_FIRE_WRECKED");
	/*TODO4FIVE d.m_uPfxKey = (u8)FXOFFSET_FIRE_WRECKED;*/

	m_fStrength = fStrength;
}

//========================================================================================================================
void CPacketFireVehWreckedFx::Extract(const CInterpEventInfo<CPacketFireVehWreckedFx, CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	if (pVehicle == NULL)
		return;

	if (m_pfxHash != ATSTRINGHASH("veh_wrecked_truck", 0x7C74A91C) &&
		m_pfxHash != ATSTRINGHASH("veh_wrecked_car", 0xB8F8B818) &&
		m_pfxHash != ATSTRINGHASH("veh_wrecked_bike", 0xA5809AE6) &&
		m_pfxHash != ATSTRINGHASH("veh_wrecked_boat", 0xB6197010) &&
		m_pfxHash != ATSTRINGHASH("veh_wrecked_heli", 0xFE2C3029))
	{
		replayAssertf(false, "Hash is not valid in CPacketFireVehWreckedFx::Extract");
	}
	replayAssert(false && "There is no PTFXOFFSET_FIRE_WRECKED");

	/*TODO4FIVE s32 boneIndex = pVeh->GetBoneIndex(VEH_SEAT_DSIDE_F);
	if (boneIndex==-1)
	{
		return;
	}

	// TODO: check local matrices and if they are updated?
	Vector3 bonePosLcl = pVeh->GetLocalMtx(boneIndex).d;
	bonePosLcl.x = 0.0f;

	bool justCreated = false;
	ptxEffectInst* pFxInst = NULL;

	pFxInst = ptfxManager::GetRegisteredInst(pVeh, FXOFFSET_FIRE_WRECKED, true, m_PfxHash, justCreated);

	if (pFxInst)
	{
		float fStrength = m_fStrength;
		if (fInterp == 0.0f)
		{
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("fade",0x6e9c2bb6), fStrength);
		}
		else if (!justCreated && HasNextOffset())
		{
			CPacketParticleInterp* pNext;
			CReplayMgr::GetNextAddress(&pNext, (CPacketParticleInterp*)this);

			replayAssert(pNext);
			replayAssert(pNext->IsInterpPacket());

			fStrength = ExtractInterpolateableValues(pFxInst, (CPacketFireVehWreckedFx*)pNext, fInterp);
		}

		pFxInst->SetVelocityAdd(pVeh->GetVelocity());

		ptfxAttach::Attach(pFxInst, pVeh, -1);

		// check if the effect has just been created 
		if (justCreated)
		{
			pFxInst->SetOffsetPos(bonePosLcl);

			pFxInst->Start();
#if DEBUG_REPLAY_PFX
			replayDebugf1("REPLAY: FIRE WRECKED FX: %0X: ID: %0X\n", (u32)pVeh, pVeh->GetReplayID());
#endif
		}
		else
		{
#if DEBUG_REPLAY_PFX
			replayDebugf1("REPLAY: FIRE WRECKED UPD FX: %0X: ID: %0X\n", (u32)pVeh, pVeh->GetReplayID());
#endif
		}

		// add light
		Vector3 lightDir(0.0f, 0.0f, 1.0f);
		Vector3 lightTan(0.0f, 1.0f, 0.0f);
		Vector3 lightCol(WRECKED_LIGHT_R, WRECKED_LIGHT_G, WRECKED_LIGHT_B);
		float lightInt = g_DrawRand.GetRanged(WRECKED_LIGHT_INT_MIN, WRECKED_LIGHT_INT_MAX);

		if (fwTimer::IsGamePaused() || CReplayMgr::IsReplayPaused())
		{
			lightInt = WRECKED_LIGHT_INT_MIN;
		}

		Vector3 wreckedPosWld = bonePosLcl;
		pVeh->GetMatrix().Transform(wreckedPosWld);
		CLights::AddSceneLight(NULL, LT_POINT, LT_FLAG_STATIC_POS|LT_FLAG_FX_LIGHT, lightDir, lightTan, wreckedPosWld, lightCol, lightInt*fStrength, 0, CLights::m_defaultTxdID, WRECKED_LIGHT_RANGE, 0, 0, -1, 0, 0);
	}*/
}

//========================================================================================================================
float CPacketFireVehWreckedFx::ExtractInterpolateableValues(ptxEffectInst* pFxInst, CPacketFireVehWreckedFx const* pNextPacket, float fInterp) const
{
	float fStrength = Lerp(fInterp, m_fStrength, pNextPacket->GetStrength());
	pFxInst->SetEvoValueFromHash(ATSTRINGHASH("fade",0x6e9c2bb6), fStrength);

	return fStrength;
}


//========================================================================================================================
CPacketFireSteamFx::CPacketFireSteamFx(u32 uPfxHash, float fStrength, Vector3& rVec)
	: CPacketFireVehWreckedFx(uPfxHash, fStrength)
{
	// Overwrite packetId
	CPacketBase::Store(PACKETID_FIRESTEAMFX, sizeof(CPacketFireSteamFx));

	m_Position[0] = rVec.x;
	m_Position[1] = rVec.y;
	m_Position[2] = rVec.z;

	SetPfxKey((u8)PTFXOFFSET_FIRE_STEAM);
}

//========================================================================================================================
void CPacketFireSteamFx::Extract(const CInterpEventInfo<CPacketFireSteamFx, CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity();
	if (pEntity == NULL)
	{
		// TODO: This could be possible! Fire on the ground, etc, find a way to fix this
		return;
	}

	if (m_pfxHash != ATSTRINGHASH("fire_steam", 0x15FE896))
	{
		replayAssertf(false, "Hash is not valid in CPacketFireSteamFx::Extract");
	}

	bool justCreated = false;
	ptxEffectInst* pFxInst = NULL;

	pFxInst = ptfxManager::GetRegisteredInst(pEntity, PTFXOFFSET_FIRE_STEAM, true, m_pfxHash, justCreated);

	if (pFxInst)
	{
		if (eventInfo.GetInterp() == 0.0f)
		{
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("strength",0xff1c4627), m_fStrength);
		}
		else if (!justCreated && HasNextOffset())
		{
			CPacketFireSteamFx const* pNext = eventInfo.GetNextPacket();
			ExtractInterpolateableValues(pFxInst, pNext, eventInfo.GetInterp());
		}

		if (justCreated)
		{
			Vec3V tempVec(m_Position[0], m_Position[1], m_Position[2]);
			pFxInst->SetBasePos(tempVec);
			pFxInst->Start();

#if DEBUG_REPLAY_PFX
			replayDebugf1("REPLAY: FIRE STEAM FX: %0X:\n", (u32)pEntity);
#endif
		}
		else
		{
#if DEBUG_REPLAY_PFX
			replayDebugf1("REPLAY: FIRE STEAM UPD FX: %0X:\n", (u32)pEntity);
#endif
		}
	}
}

//========================================================================================================================
void CPacketFireSteamFx::ExtractInterpolateableValues(ptxEffectInst* pFxInst, CPacketFireSteamFx const* pNextPacket, float fInterp) const
{
	float fStrength = Lerp(fInterp, m_fStrength, pNextPacket->GetStrength());
	pFxInst->SetEvoValueFromHash(ATSTRINGHASH("strength",0xff1c4627), fStrength);
}


//========================================================================================================================
CPacketFirePedBoneFx::CPacketFirePedBoneFx(u32 uPfxHash, u8 uBodyID, float fStrength, float fSpeed, u8 uBoneIdx1, u8 uBoneIdx2, bool bContainsLight)
	: CPacketFireVehWreckedFx(uPfxHash, fStrength)
{
	// Overwrite packetId
	CPacketBase::Store(PACKETID_FIREPEDFX, sizeof(CPacketFirePedBoneFx));

	SetPfxKey(uBodyID + (u8)PTFXOFFSET_FIRE_FLAMES);

	m_fSpeed = fSpeed;
	m_uBoneIdx1 = uBoneIdx1;
	m_uBoneIdx2 = uBoneIdx2;
	m_uBodyID = uBodyID;
	m_bContainsLight = bContainsLight;
}

//========================================================================================================================
void CPacketFirePedBoneFx::Extract(const CInterpEventInfo<CPacketFirePedBoneFx, CPed>& eventInfo) const
{
	CPed* pPed = eventInfo.GetEntity();
	if (pPed == NULL)
		return;

	if (m_pfxHash != ATSTRINGHASH("fire_ped_body", 0xE18C194B) &&
		m_pfxHash != ATSTRINGHASH("fire_ped_limb", 0xBDDFEE5E))
	{
		replayAssertf(false, "Hash is not valid in CPacketFirePedBoneFx::Extract");
	}

	bool justCreated = false;
	ptxEffectInst* pFxInst = NULL;

	pFxInst = ptfxManager::GetRegisteredInst(pPed, PTFXOFFSET_FIRE_FLAMES+m_uBodyID, true, m_pfxHash, justCreated);

	if (pFxInst)
	{
		Matrix34 pBoneMatrix1, pBoneMatrix2;
		pPed->GetGlobalMtx(m_uBoneIdx1, pBoneMatrix1);
		pPed->GetGlobalMtx(m_uBoneIdx2, pBoneMatrix2);

		Vector3 dir = pBoneMatrix2.d - pBoneMatrix1.d;
		Vector3 pos = pBoneMatrix1.d + (dir*0.5f);
		dir.Normalize();

		Mat34V fxMat;
		CVfxHelper::CreateMatFromVecY(fxMat, VECTOR3_TO_VEC3V(pos), VECTOR3_TO_VEC3V(dir));

		Mat34V invEntityMat = MATRIX34_TO_MAT34V(pBoneMatrix1);
		Invert3x3Full(invEntityMat, invEntityMat);
		
		Mat34V relFxMat = fxMat;
		Transform(relFxMat, invEntityMat);
		
		pFxInst->SetOffsetMtx(relFxMat);

		if (eventInfo.GetInterp() == 0.0f)
		{
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("strength",0xff1c4627), m_fStrength);

			replayAssert(m_fSpeed>=0.0f);
			replayAssert(m_fSpeed<=1.0f);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed",0xf997622b), m_fSpeed);
		}
		else if (!justCreated && HasNextOffset())
		{
			CPacketFirePedBoneFx const* pNext = eventInfo.GetNextPacket();
			ExtractInterpolateableValues(pFxInst, pNext, eventInfo.GetInterp());
		}

		// Update the position every single time
		pFxInst->SetBaseMtx(MATRIX34_TO_MAT34V(pBoneMatrix1));

		pFxInst->SetLODScalar(1.0f);
		
		if (justCreated)
		{
			////g_ptFx.SetMatrixPointerOwner(pFxInst, pPed);

			// it has just been created - start it and set it's position
			pFxInst->Start();

#if DEBUG_REPLAY_PFX
			replayDebugf1("REPLAY: FIRE PED BONE FX: %0X: ID: %0X\n", (u32)pPed, pPed->GetReplayID());
#endif
		}
		else
		{
#if DEBUG_REPLAY_PFX
			replayDebugf1("REPLAY: FIRE PED BONE UPD FX: %0X: ID: %0X\n", (u32)pPed, pPed->GetReplayID());
#endif
		}

		// attach light to fx
		/*TODO4FIVE if (m_bContainsLight && m_uBodyID == 0)
		{
			Vector3 lightDir(0.0f, 0.0f, 1.0f);
			Vector3 lightTan(0.0f, 1.0f, 0.0f);
			Vector3 lightCol(FIRE_LIGHT_R, FIRE_LIGHT_G, FIRE_LIGHT_B);
			float lightInt = g_DrawRand.GetRanged(FIRE_LIGHT_I_MIN, FIRE_LIGHT_I_MAX);

			if (fwTimer::IsGamePaused() || CReplayMgr::IsReplayPaused())
			{
				lightInt = FIRE_LIGHT_I_MIN;
			}

			CLights::AddSceneLight(NULL, LT_POINT, LT_FLAG_STATIC_POS|LT_FLAG_FX_LIGHT, lightDir, lightTan, fxMat.d, lightCol, lightInt, 0, CLights::m_defaultTxdID, FIRE_LIGHT_RANGE, 0, 0, -1, 0, 0);
		}*/
	}
}

//========================================================================================================================
void CPacketFirePedBoneFx::ExtractInterpolateableValues(ptxEffectInst* pFxInst, CPacketFirePedBoneFx const* pNextPacket, float fInterp) const
{
	float fSpeed = Lerp(fInterp, m_fSpeed, pNextPacket->GetSpeed());
	pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed",0xf997622b), fSpeed);

	float fStrength = Lerp(fInterp, m_fStrength, pNextPacket->GetStrength());
	pFxInst->SetEvoValueFromHash(ATSTRINGHASH("strength",0xff1c4627), fStrength);
}

//========================================================================================================================
CPacketFireFx_OLD::CPacketFireFx_OLD(const CFireSettings& fireSettings, s32 fireIndex)
	: CPacketEvent(PACKETID_FIREFX, sizeof(CPacketFireFx_OLD))
	, CPacketEntityInfo()
{
	if(fireSettings.pCulprit)
		m_culpritID = fireSettings.pCulprit->GetReplayID();
	else
		m_culpritID = -1;

	m_PosLcl		= fireSettings.vPosLcl;
	m_NormLcl		= fireSettings.vNormLcl;

	m_fireType		= static_cast<u8>(fireSettings.fireType);
	m_mtlId			= fireSettings.mtlId;
	m_burnTime		= fireSettings.burnTime;
	m_burnStrength	= fireSettings.burnStrength;
	m_peakTime		= fireSettings.peakTime;
	m_numGenerations= fireSettings.numGenerations;
	m_fuelLevel		= fireSettings.fuelLevel;
	m_fuelBurnRate	= fireSettings.fuelBurnRate;
	m_ParentPos		= fireSettings.vParentPos;
	m_parentFireType =  static_cast<u8>(fireSettings.parentFireType);
	m_isFromScript = g_fireMan.ReplayIsScriptedFire();
	m_isPetrolFire = g_fireMan.ReplayIsPetrolFire();

	m_FireIndex = fireIndex;
}

bool CPacketFireFx_OLD::HasExpired(const CEventInfo<CEntity>&) const
{
	if( m_FireIndex >= 0 )
	{
		CFire pFire = g_fireMan.GetFire(m_FireIndex);
		return !pFire.GetFlag(FIRE_FLAG_IS_ACTIVE);
	}
	return true;
}


//========================================================================================================================
void CPacketFireFx_OLD::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK && !eventInfo.GetIsFirstFrame())
	{
		return;
	}

	CFireSettings fireSettings;
	fireSettings.vPosLcl		= m_PosLcl;
	fireSettings.vNormLcl		= m_NormLcl;
	fireSettings.fireType		= static_cast<FireType_e>(m_fireType);	 
	fireSettings.mtlId			= m_mtlId;	 
	fireSettings.burnTime		= m_burnTime;
	fireSettings.burnStrength	= m_burnStrength;
	fireSettings.peakTime		= m_peakTime;
	fireSettings.numGenerations = m_numGenerations;
	fireSettings.fuelLevel		= m_fuelLevel;
	fireSettings.fuelBurnRate	= m_fuelBurnRate;
	fireSettings.pEntity		= eventInfo.GetEntity();
	fireSettings.vParentPos		= m_ParentPos;
	fireSettings.parentFireType = static_cast<FireType_e>(m_parentFireType);

	// 	if(m_culpritID != -1)
	// 	{
	// 		CPed* pCulprit = CReplayMgr::GetEntity<CPed>(REPLAYIDTYPE_PLAYBACK, m_culpritID);
	// 		fireSettings.pCulprit = pCulprit;
	// 		replayAssert(pCulprit != NULL && "Failed to get culprit for fire");
	// 	}

	CFire* pFire = g_fireMan.StartFire(fireSettings);

	if (pFire && fireSettings.fireType == FIRETYPE_TIMED_MAP)
	{
		s32 gridX, gridY;
		g_fireMan.GetMapStatusGridIndices(fireSettings.vPosLcl, gridX, gridY);

		g_fireMan.SetMapStatusGrid(gridX, gridY, pFire->GetBurnTime()*5.0f, fireSettings.vPosLcl.GetZf()+pFire->GetMaxRadius());
	}

	if( pFire && m_isFromScript )
	{
		pFire->SetFlag(FIRE_FLAG_IS_SCRIPTED);
		pFire->SetFlag(FIRE_FLAG_IS_REPLAY);

		if( m_isPetrolFire )
			pFire->SetFlag(FIRE_FLAG_IS_SCRIPTED_PETROL_FIRE);
	}
}

const tPacketVersion CPacketFireFx_Version1 = 1;
//========================================================================================================================
CPacketFireFx::CPacketFireFx(const CFireSettings& fireSettings, s32 fireIndex, Vec3V_In worldPos)
	: CPacketEventTracked(PACKETID_TRACKED_FIREFX, sizeof(CPacketFireFx), CPacketFireFx_Version1)
	, CPacketEntityInfoStaticAware()
{
	if(fireSettings.pCulprit)
		m_culpritID = fireSettings.pCulprit->GetReplayID();
	else
		m_culpritID = -1;

	m_PosLcl		= fireSettings.vPosLcl;
	m_NormLcl		= fireSettings.vNormLcl;

	m_fireType		= static_cast<u8>(fireSettings.fireType);
	m_mtlId			= fireSettings.mtlId;
	m_burnTime		= fireSettings.burnTime;
	m_burnStrength	= fireSettings.burnStrength;
	m_peakTime		= fireSettings.peakTime;
	m_numGenerations= fireSettings.numGenerations;
	m_fuelLevel		= fireSettings.fuelLevel;
	m_fuelBurnRate	= fireSettings.fuelBurnRate;
	m_ParentPos		= fireSettings.vParentPos;
	m_parentFireType =  static_cast<u8>(fireSettings.parentFireType);
	m_isFromScript = g_fireMan.ReplayIsScriptedFire();
	m_isPetrolFire = g_fireMan.ReplayIsPetrolFire();

	m_FireIndex = fireIndex;

	m_WorldPos.Store(worldPos);
}

bool CPacketFireFx::HasExpired(const CTrackedEventInfo<ptxFireRef>& /*trackingInfo*/) const
{
	bool expired = false;

	if( m_FireIndex >= 0 )
	{
		CFire pFire = g_fireMan.GetFire(m_FireIndex);

		if( !pFire.GetFlag(FIRE_FLAG_IS_ACTIVE) )
		{			
			expired = true;
		}
	}

	return expired;
}

//========================================================================================================================
void CPacketFireFx::Extract(CTrackedEventInfo<ptxFireRef>& eventInfo) const
{
	bool playingBackwards = eventInfo.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_BACK);

	//get the estimated elapsed time of the fire
	replayAssert(eventInfo.GetReplayTime() >= GetGameTime());
	float length = (float(eventInfo.GetReplayTime()) - float(GetGameTime())) / 1000.0f;

	//work out the new duration
	float duration = m_burnTime > length ? m_burnTime - length : 0.0f;
	float peakTime = m_peakTime > length ? m_peakTime - length : 0.0f;

	float fadeTime = length < FIRE_PETROL_BURN_TIME ? FIRE_PETROL_BURN_TIME - length : 0.0f;

	if( ( playingBackwards && !eventInfo.GetIsFirstFrame() ) || eventInfo.m_pEffect[0] != 0 )
	{
		if( playingBackwards )
		{
			//reset the effect when we are playing back so we can start again.
			eventInfo.m_pEffect[0] = 0;
		}
		else
		{
			if( GetPacketVersion() >= CPacketFireFx_Version1 )
			{
				// check for petrol pools around the fire 
				u32 foundDecalTypeFlag = 0;
				Vec3V vPetrolDecalPos;
				fwEntity* pPetrolDecalEntity = NULL;
				g_decalMan.FindClosest((1<<DECALTYPE_POOL_LIQUID | 1<<DECALTYPE_TRAIL_LIQUID), VFXLIQUID_TYPE_PETROL, m_WorldPos, FIRE_PETROL_SEARCH_DIST, 0.0f, false, true, fadeTime, vPetrolDecalPos, &pPetrolDecalEntity, foundDecalTypeFlag);
			}
		}

		return;
	}

	CFireSettings fireSettings;
	fireSettings.vPosLcl		= m_PosLcl;
	fireSettings.vNormLcl		= m_NormLcl;
	fireSettings.fireType		= static_cast<FireType_e>(m_fireType);	 
	fireSettings.mtlId			= m_mtlId;	 
	fireSettings.burnTime		= duration;
	fireSettings.burnStrength	= m_burnStrength;
	fireSettings.peakTime		= peakTime;
	fireSettings.numGenerations = m_numGenerations;
	fireSettings.fuelLevel		= m_fuelLevel;
	fireSettings.fuelBurnRate	= m_fuelBurnRate;
	fireSettings.pEntity		= eventInfo.pEntity[0];
	fireSettings.vParentPos		= m_ParentPos;
	fireSettings.parentFireType = static_cast<FireType_e>(m_parentFireType);

// 	if(m_culpritID != -1)
// 	{
// 		CPed* pCulprit = CReplayMgr::GetEntity<CPed>(REPLAYIDTYPE_PLAYBACK, m_culpritID);
// 		fireSettings.pCulprit = pCulprit;
// 		replayAssert(pCulprit != NULL && "Failed to get culprit for fire");
// 	}

	CFire* pFire = g_fireMan.StartFire(fireSettings);
	eventInfo.m_pEffect[0] = reinterpret_cast<size_t>(pFire);

	if (pFire && fireSettings.fireType == FIRETYPE_TIMED_MAP)
	{
		s32 gridX, gridY;
		g_fireMan.GetMapStatusGridIndices(fireSettings.vPosLcl, gridX, gridY);

		g_fireMan.SetMapStatusGrid(gridX, gridY, pFire->GetBurnTime()*5.0f, fireSettings.vPosLcl.GetZf()+pFire->GetMaxRadius());
	}

	if( pFire && m_isFromScript )
	{
		pFire->SetFlag(FIRE_FLAG_IS_SCRIPTED);
		pFire->SetFlag(FIRE_FLAG_IS_REPLAY);

		if( m_isPetrolFire )
			pFire->SetFlag(FIRE_FLAG_IS_SCRIPTED_PETROL_FIRE);
	}
}

//========================================================================================================================
CPacketStopFireFx::CPacketStopFireFx()
	: CPacketEventTracked(PACKETID_TRACKED_STOPFIREFX, sizeof(CPacketStopFireFx))
	, CPacketEntityInfoStaticAware()
{
}


//========================================================================================================================
void CPacketStopFireFx::Extract(CTrackedEventInfo<ptxFireRef>& eventInfo) const
{
	if(eventInfo.m_pEffect[0] != 0)
	{
		CFire* pFire = reinterpret_cast<CFire*>(eventInfo.m_pEffect[0].m_Id);
		if(pFire->GetFlag(FIRE_FLAG_IS_ACTIVE))
		{
			pFire->Shutdown();
		}
	}
	eventInfo.m_pEffect[0] = 0;
}


//========================================================================================================================
CPacketFireEntityFx::CPacketFireEntityFx(size_t id, const Matrix34& matrix, float strength, int boneIndex)
	: CPacketEvent(PACKETID_FIREENTITYFX, sizeof(CPacketFireEntityFx))
	, CPacketEntityInfo()
{
	m_Id = (u64)id;
	m_Matrix.StoreMatrix(matrix);
	m_Strength = strength;
	m_BoneIndex = boneIndex;
}


//========================================================================================================================
void CPacketFireEntityFx::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK && !eventInfo.GetIsFirstFrame())
	{
		return;
	}

	CEntity* pEntity = eventInfo.GetEntity();

	if (pEntity == NULL)
		return;

	// register the fx system
	bool justCreated = false;
	ptxEffectInst* pFxInst = NULL;

	if (pEntity->GetIsTypeVehicle())
	{
		pFxInst = ptfxManager::GetRegisteredInst((void*)m_Id, PTFXOFFSET_FIRE_FLAMES, false, atHashWithStringNotFinal("fire_vehicle",0xB54FD790), justCreated);
	}
	else if (pEntity->GetIsTypeObject())
	{
		pFxInst = ptfxManager::GetRegisteredInst((void*)m_Id, PTFXOFFSET_FIRE_FLAMES, false, atHashWithStringNotFinal("fire_object",0x83CC94D0), justCreated);
	}
	else
	{
		vfxAssertf(0, "unsupported entity is on fire");
	}

	if (pFxInst)
	{
		// set position of effect
		ptfxAttach::Attach(pFxInst, pEntity, m_BoneIndex);

		// set the time of the effect based on the fire strength
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("strength", 0xFF1C4627), m_Strength);

		// set velocity of entity attached to
		if (pEntity->GetIsPhysical())
		{
			CPhysical* pPhysical = static_cast<CPhysical*>(pEntity);
			ptfxWrapper::SetVelocityAdd(pFxInst, RCC_VEC3V(pPhysical->GetVelocity()));
		}

		// check if the effect has just been created 
		if (justCreated)
		{
			Matrix34 m_Mat;
			m_Matrix.LoadMatrix(m_Mat);
			pFxInst->SetOffsetMtx(MATRIX34_TO_MAT34V(m_Mat));

			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
		else
		{
			// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
			vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "effect attachment error");
		}
#endif
	}
}

//========================================================================================================================
CPacketFireProjectileFx::CPacketFireProjectileFx(bool isPrime, bool isFirstPersonActive, bool isFirstPersonPass, u32 effectHashName)
: CPacketEvent(PACKETID_PROJECTILEFX, sizeof(CPacketFireProjectileFx))
, CPacketEntityInfo()
{
	m_IsPrime			= isPrime;
	m_FirstPersonActive = isFirstPersonActive;
	m_IsFirstPersonPass = isFirstPersonPass;
	m_EffectHashName	= effectHashName;
}

//========================================================================================================================
void CPacketFireProjectileFx::Extract(const CEventInfo<CObject>& eventInfo) const
{
	CObject* pObject = eventInfo.GetEntity();
	
	if( pObject == NULL )
	{
		return;
	}

	// get the effect offset
	int classFxOffset = PTFXOFFSET_PROJECTILE_FUSE;
	if (m_FirstPersonActive)
	{
		if (m_IsFirstPersonPass)
		{
			classFxOffset = PTFXOFFSET_PROJECTILE_FUSE_ALT_FP;
		}
		else
		{
			classFxOffset = PTFXOFFSET_PROJECTILE_FUSE_ALT_NORM;
		}
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pObject, classFxOffset, true, m_EffectHashName, justCreated);
	if (pFxInst)
	{
		CWeaponModelInfo* pModelInfo = static_cast<CWeaponModelInfo*>(pObject->GetBaseModelInfo());
		s32 trailBoneId = pModelInfo->GetBoneIndex(WEAPON_VFX_PROJTRAIL);

		if (m_FirstPersonActive)
		{
			// the first person pass plays the alternative effect, attached to the normal skeleton bone, only rendered in the first person view
			ptfxAttach::Attach(pFxInst, pObject, trailBoneId);
			pFxInst->SetFlag(PTFX_EFFECTINST_RENDER_ONLY_FIRST_PERSON_VIEW);
		}
		else 
		{
			// the normal (non-first person) pass plays the normal effect
			if (m_FirstPersonActive)
			{
				// if the first person camera is active we want to attach to the alternative skeleton and only render in the non first person view 
				ptfxAttach::Attach(pFxInst, pObject, trailBoneId, PTFXATTACHMODE_FULL, true);
				pFxInst->SetFlag(PTFX_EFFECTINST_RENDER_ONLY_NON_FIRST_PERSON_VIEW);
			}
			else
			{
				// if the first person camera isn't active we want to attach to the normal skeleton and render at all times
				ptfxAttach::Attach(pFxInst, pObject, trailBoneId);
				pFxInst->ClearFlag(PTFX_EFFECTINST_RENDER_ONLY_NON_FIRST_PERSON_VIEW);
			}
		}

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set its position
			pFxInst->Start();
		}
	}
}

//========================================================================================================================
CPacketExplosionFx::CPacketExplosionFx(size_t id, u32 hashName, const Matrix34& matrix, float radius, int boneIndex, float scale)
	: CPacketEvent(PACKETID_EXPLOSIONFX, sizeof(CPacketExplosionFx))
	, CPacketEntityInfo()
{
	m_Id = (u64)id;
	m_Matrix.StoreMatrix(matrix);
	m_Scale = scale;
	m_BoneIndex = boneIndex;
	m_Radius = radius;
	m_HashName = hashName;
}


//========================================================================================================================
void CPacketExplosionFx::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	CEntity* pEntity = eventInfo.GetEntity();

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst((void*)m_Id, 0, false, m_HashName, justCreated);

	// check the effect exists
	if (pFxInst)
	{
		Matrix34 m_Mat;
		m_Matrix.LoadMatrix(m_Mat);

		if (pEntity)
		{
			ptfxAttach::Attach(pFxInst, pEntity, m_BoneIndex);
			pFxInst->SetOffsetMtx(MATRIX34_TO_MAT34V(m_Mat));
		}
		else
		{
			pFxInst->SetBaseMtx(MATRIX34_TO_MAT34V(m_Mat));
		}
		
		// set any evolutions
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("radius", 0x4FBB9CF3), m_Radius);
		pFxInst->SetUserZoom(m_Scale);

		// check if the effect has just been created 
		if (justCreated)
		{		
			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
CPacketRegisterVehicleFire::CPacketRegisterVehicleFire(int boneIndex, const Vec3V& localPos, u8 wheelIndex, float evo, u8 fireType)
	: CPacketEvent(PACKETID_REGVEHICLEFIRE, sizeof(CPacketRegisterVehicleFire))
	, CPacketEntityInfo()
	, m_boneIndex(boneIndex)
	, m_wheelIndex(wheelIndex)
	, m_evo(evo)
	, m_fireType(fireType)
{
	m_localPos.Store(localPos);
	m_unused[0] = 0;
	m_unused[1] = 0;
}


//////////////////////////////////////////////////////////////////////////
void CPacketRegisterVehicleFire::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	if(pVehicle)
	{
		Vec3V localPos;
		m_localPos.Load(localPos);

		if(m_fireType == TYRE_FIRE)
		{
			CWheel* pWheel = pVehicle->GetWheel(m_wheelIndex);
			g_fireMan.RegisterVehicleTyreFire(pVehicle, m_boneIndex, localPos, pWheel, PTFXOFFSET_WHEEL_FIRE, m_evo, true);
		}
		else if(m_fireType == TANK_FIRE_0 || m_fireType == TANK_FIRE_1)
		{
			g_fireMan.RegisterVehicleTankFire(pVehicle, -1, localPos, pVehicle, PTFXOFFSET_VEHICLE_FIRE_PETROL_TANK+m_fireType, m_evo, nullptr);
		}
	}
}


#endif // GTA_REPLAY
