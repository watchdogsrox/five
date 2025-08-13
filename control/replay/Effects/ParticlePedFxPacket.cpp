//=====================================================================================================
// name:		ParticlePedFxPacket.cpp
//=====================================================================================================

#include "ParticlePedFxPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/ReplayInterfacePed.h"
#include "control/replay/ReplayInternal.h"
#include "scene/world/GameWorld.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "vfx/ptfx/ptfxattach.h"


//========================================================================================================================
CPacketPedSmokeFx::CPacketPedSmokeFx(u32 uPfxHash)
	: CPacketEvent(PACKETID_PEDSMOKEFX, sizeof(CPacketPedSmokeFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{}

//========================================================================================================================
void CPacketPedSmokeFx::Extract(const CEventInfo<CPed>& eventInfo) const
{
	CPed* pPed = eventInfo.GetEntity();
	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		s32 sBoneIndex = CPedBoneTagConvertor::GetBoneIndexFromBoneTag(pPed->GetSkeletonData(), (BONETAG_HEAD));
		if (sBoneIndex != -1)
		{
			{
				ptfxAttach::Attach(pFxInst, pPed, sBoneIndex);
				////g_ptFx.SetMatrixPointerOwner(pFxInst, pPed);

				pFxInst->Start();

#if DEBUG_REPLAY_PFX
				replayDebugf1("REPLAY: PED SMOKE FX: %0X: ID: %0X\n", (u32)pPed, pPed->GetReplayID());
#endif
			}
		}
	}
}


//========================================================================================================================
CPacketPedBreathFx::CPacketPedBreathFx(u32 uPfxHash, float fSpeed, float fTemp, float fRate)
	: CPacketEvent(PACKETID_PEDBREATHFX, sizeof(CPacketPedBreathFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{
	m_fSpeed = fSpeed;
	m_fTemp = fTemp;
	m_fRate = fRate;
}

//========================================================================================================================
void CPacketPedBreathFx::Extract(const CEventInfo<CPed>& eventInfo) const
{
	CPed* pPed = eventInfo.GetEntity();
	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		s32 sBoneIndex = CPedBoneTagConvertor::GetBoneIndexFromBoneTag(pPed->GetSkeletonData(), (BONETAG_HEAD));
		if (sBoneIndex != -1)
		{
			{
				ptfxAttach::Attach(pFxInst, pPed, sBoneIndex);
				////g_ptFx.SetMatrixPointerOwner(pFxInst, pPed);

				// speed evo
				replayAssert(m_fSpeed>=0.0f);
				replayAssert(m_fSpeed<=1.0f);
				pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed",0xf997622b), m_fSpeed);

				// temperature evo - 9C=1.0 and 2C=0.0
				pFxInst->SetEvoValueFromHash(ATSTRINGHASH("temp",0x6ce0a51a), m_fTemp);

				// rate evo
				replayAssert(m_fRate>=0.0f);
				replayAssert(m_fRate<=1.0f);
				pFxInst->SetEvoValueFromHash(ATSTRINGHASH("rate",0x7e68c088), m_fRate);

				pFxInst->Start();

#if DEBUG_REPLAY_PFX
				replayDebugf1("REPLAY: PED BREATH FX: %0X: ID: %0X\n", (u32)pPed, pPed->GetReplayID());
#endif
			}
		}
	}
}



//////////////////////////////////////////////////////////////////////////
CPacketPedFootFx::CPacketPedFootFx(u32 uPfxHash, float depth, float speed, float scale, u32 foot)
	: CPacketEvent(PACKETID_PEDFOOTFX, sizeof(CPacketPedFootFx))
	, CPacketEntityInfo()
	, m_pfxHash(uPfxHash)
{
	REPLAY_UNUSEAD_VAR(m_fSize);
	REPLAY_UNUSEAD_VAR(m_bIsMale);
	REPLAY_UNUSEAD_VAR(m_bIsLeft);
	REPLAY_UNUSEAD_VAR(m_Pad);

	m_depthEvo = depth;
	m_speedEvo = speed;
	m_ptFxScale = scale;
	m_foot = foot;
}


//////////////////////////////////////////////////////////////////////////
void CPacketPedFootFx::Extract(const CEventInfo<CPed>& eventInfo) const
{
	CPed* pPed = eventInfo.GetEntity();
	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(m_pfxHash);
	if (pFxInst)
	{
		// get the vfx ped info
		CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);
		if (pVfxPedInfo==NULL)
		{
			return;
		}
		VfxGroup_e colnVfxGroup = PGTAMATERIALMGR->GetMtlVfxGroup(pPed->GetPackedGroundMaterialId());

		Matrix34 footMatrix;
		footMatrix.Identity();
		switch(m_foot)
		{
		case LeftFoot :
			pPed->GetBoneMatrixCached(footMatrix,BONETAG_L_PH_FOOT);
			break;
		case RightFoot:
			pPed->GetBoneMatrixCached(footMatrix,BONETAG_R_PH_FOOT);
			break;
		default:
			break;
		}
		Mat34V footMat = MATRIX34_TO_MAT34V(footMatrix);

		Vector3 groundNormal = pPed->GetGroundNormal();

		Vec3V vHeelPos =  pPed->GetFootstepHelper().GetHeelPosition(FeetTags(m_foot));
		Vec3V vFootForward = -footMat.GetCol1();

		// calculate the footstep matrix using the heel position and the ground normal (and aligning to the forward vector)
		Mat34V vFootStepMtx;
		CVfxHelper::CreateMatFromVecZAlign(vFootStepMtx, vHeelPos,  RCC_VEC3V(groundNormal), vFootForward);

		// adjust the footstep position to be in the centre of the shoe
		const VfxPedFootInfo* pVfxPedFootInfo_DecalVfxGroup = pVfxPedInfo->GetFootVfxInfo(colnVfxGroup);
		const VfxPedFootDecalInfo* pVfxPedFootDecalInfo = pVfxPedFootInfo_DecalVfxGroup->GetDecalInfo();
		ScalarV vHalfShoeLength = ScalarVFromF32(pVfxPedFootDecalInfo->m_decalLength*0.5f);
		vFootForward = vFootStepMtx.GetCol1();
		Vec3V vFootCentrePos = vHeelPos + (vFootForward*vHalfShoeLength);

		// reposition the matrix at the centre of the foot
		vFootStepMtx.SetCol3(vFootCentrePos);

		pFxInst->SetBaseMtx(vFootStepMtx);

		// depth evo
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("depth",0xbca972d6), m_depthEvo);

		// speed evo
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed",0xf997622b), m_speedEvo);

		pFxInst->SetUserZoom(m_ptFxScale);

		pFxInst->Start();

#if DEBUG_REPLAY_PFX
		replayDebugf1("REPLAY: PED FOOT FX: %0X: ID: %0X\n", (u32)pPed, pPed->GetReplayID());
#endif
	}
}


//////////////////////////////////////////////////////////////////////////
CPacketPlayerSwitch::CPacketPlayerSwitch()
	: CPacketEvent(PACKETID_PLAYERSWITCH, sizeof(CPacketPlayerSwitch))
	, CPacketEntityInfo()
{}


//////////////////////////////////////////////////////////////////////////
void CPacketPlayerSwitch::Extract(const CEventInfo<CPed>& eventInfo) const
{
	CPed* pPed1		= eventInfo.GetEntity(0);	// Playing forward this one should be the new player...
	CPed* pPed2		= eventInfo.GetEntity(1);

	replayFatalAssertf(pPed1, "Failed to find new Player to change (replayID: %d/0x%08X)", GetReplayID(0).ToInt(), GetReplayID(0).ToInt());
	replayFatalAssertf(pPed2, "Failed to find old Player to change (replayID: %d/0x%08X)", GetReplayID(1).ToInt(), GetReplayID(1).ToInt());

	if(eventInfo.GetIsFirstFrame() && pPed1 && pPed1->GetPlayerInfo())
	{
		replayDebugf1("Not playing the player switch as it's the first frame");
		return;
	}

	if(pPed1 && pPed2)
	{
		CPed* pCurrPed = NULL;
		CPed* pNextPed = NULL;

		if(pPed1->IsAPlayerPed())
		{
			replayFatalAssertf(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK, "Inconsistency in CPacketPlayerSwitch::Extract");
			pCurrPed = pPed1;
			pNextPed = pPed2;
		}
		else
		{
			replayFatalAssertf(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_FWD, "Inconsistency in CPacketPlayerSwitch::Extract");
			pCurrPed = pPed2;
			pNextPed = pPed1;
		}

		if(pCurrPed->GetPlayerInfo() == NULL)
		{
			replayDebugf1("Can't change player ped here as the 'current' one has no player info (network player?)");
			return;
		}

		replayDebugf1("Switching player from 0x%08X to 0x%08X...", pCurrPed->GetReplayID().ToInt(), pNextPed->GetReplayID().ToInt());
		CGameWorld::ChangePlayerPed(*pCurrPed, *pNextPed);
		CReplayMgr::SetMainPlayerPtr(pNextPed);
	}
}

#endif // GTA_REPLAY
