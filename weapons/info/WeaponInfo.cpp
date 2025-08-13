//
// weapons/weaponinfo.cpp
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

// File header
#include "Weapons/Info/WeaponInfo.h"

// Game headers
#include "Debug/DebugScene.h"
#include "ModelInfo/PedModelInfo.h"
#include "ModelInfo/WeaponModelInfo.h"
#include "Network/NetworkInterface.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Pickups/PickupManager.h"
#include "Stats/Stats_Channel.h"
#include "Stats/StatsInterface.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/General/Phone/TaskMobilePhone.h"
#include "Task/Motion/Locomotion/TaskMotionAiming.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Weapons/Gun/TaskAimGunOnFoot.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vehicles/Metadata/VehicleMetadataManager.h"
#include "Weapons/Components/WeaponComponentManager.h"
#include "Weapons/Info/AmmoInfo.h"
#include "Weapons/Info/WeaponAnimationsManager.h"
#include "Weapons/Info/WeaponSwapData.h"

// Parser files
#include "WeaponInfo_parser.h"

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()
AI_COVER_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////

const char* CWeaponTintSpecValues::GetNameFromPointer(const CWeaponTintSpecValues* NOTFINAL_ONLY(p))
{
#if !__FINAL
	if(p)
	{
		return p->GetName();
	}
#endif // !__FINAL
	return "";
}

////////////////////////////////////////////////////////////////////////////////

const CWeaponTintSpecValues* CWeaponTintSpecValues::GetPointerFromName(const char* name)
{
	const CWeaponTintSpecValues* p = CWeaponInfoManager::GetWeaponTintSpecValue(atStringHash(name));
	weaponAssertf(p, "CWeaponTintSpecValues [%s] doesn't exist in data", name);
	return p;
}

////////////////////////////////////////////////////////////////////////////////

const char* CWeaponFiringPatternAliases::GetNameFromPointer(const CWeaponFiringPatternAliases* NOTFINAL_ONLY(p))
{
#if !__FINAL
	if(p)
	{
		return p->GetName();
	}
#endif // !__FINAL
	return "";
}

////////////////////////////////////////////////////////////////////////////////

const CWeaponFiringPatternAliases* CWeaponFiringPatternAliases::GetPointerFromName(const char* name)
{
	const CWeaponFiringPatternAliases* p = CWeaponInfoManager::GetWeaponFiringPatternAlias(atStringHash(name));
	weaponAssertf(p, "CWeaponFiringPatternAliases [%s] doesn't exist in data", name);
	return p;
}

////////////////////////////////////////////////////////////////////////////////

float CWeaponUpperBodyFixupExpressionData::Data::GetFixupWeight(const CPed& ped) const
{
	if(ped.GetMotionData()->GetIsStill())
		return Idle;
	else if(ped.GetMotionData()->GetIsWalking())
		return Walk;
	else
		return Run;
}

////////////////////////////////////////////////////////////////////////////////

float CWeaponUpperBodyFixupExpressionData::GetFixupWeight(const CPed& ped) const
{
	const CTaskMotionBase* pMotionTask = ped.GetPrimaryMotionTaskIfExists();
	if(pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_PED)
	{
		const CTaskMotionPed* pMotionPedTask = static_cast<const CTaskMotionPed*>(pMotionTask);
		switch(pMotionPedTask->GetState())
		{
		case CTaskMotionPed::State_ActionMode:
			return m_Data[Action].GetFixupWeight(ped);
		case CTaskMotionPed::State_Aiming:
			return m_Data[Aiming].GetFixupWeight(ped);
		case CTaskMotionPed::State_Stealth:
			return m_Data[Stealth].GetFixupWeight(ped);
		default:
			return m_Data[Normal].GetFixupWeight(ped);
		}
	}
	return 1.0f;
}

////////////////////////////////////////////////////////////////////////////////

const char* CWeaponUpperBodyFixupExpressionData::GetNameFromPointer(const CWeaponUpperBodyFixupExpressionData* NOTFINAL_ONLY(p))
{
#if !__FINAL
	if(p)
	{
		return p->GetName();
	}
#endif // !__FINAL
	return "";
}

////////////////////////////////////////////////////////////////////////////////

const CWeaponUpperBodyFixupExpressionData* CWeaponUpperBodyFixupExpressionData::GetPointerFromName(const char* name)
{
	const CWeaponUpperBodyFixupExpressionData* p = CWeaponInfoManager::GetWeaponUpperBodyFixupExpressionData(atStringHash(name));
	weaponAssertf(p, "CWeaponUpperBodyFixupExpressionData [%s] doesn't exist in data", name);
	return p;
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CWeaponUpperBodyFixupExpressionData::AddWidgets(bkBank& bank)
{
	static const char* typeNames[CWeaponUpperBodyFixupExpressionData::Max] = { "Normal", "Action", "Stealth", "Aiming" };

	bank.PushGroup(GetName());
	for(s32 i = 0; i < CWeaponUpperBodyFixupExpressionData::Max; i++)
	{
		bank.PushGroup(typeNames[i]);
		PARSER.AddWidgets(bank, &m_Data[i]);
		bank.PopGroup();
	}
	bank.PopGroup();
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

CAimingInfo::CAimingInfo()
{
}

////////////////////////////////////////////////////////////////////////////////

CAimingInfo::~CAimingInfo()
{
}

////////////////////////////////////////////////////////////////////////////////

const char* CAimingInfo::GetNameFromInfo(const CAimingInfo* NOTFINAL_ONLY(pInfo))
{
#if !__FINAL
	if(pInfo)
	{
		return pInfo->GetName();
	}
#endif // !__FINAL
	return "";
}

////////////////////////////////////////////////////////////////////////////////

const CAimingInfo* CAimingInfo::GetInfoFromName(const char* name)
{
	const CAimingInfo* pInfo = CWeaponInfoManager::GetAimingInfo(atStringHash(name));
	weaponAssertf(pInfo, "AimGunStrafingInfo [%s] doesn't exist in data", name);
	return pInfo;
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
bool  CAimingInfo::Validate() const
{
	return true;
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

CWeaponComponentPoint::CWeaponComponentPoint()
: m_AttachBoneId(WEAPON_ROOT)
{
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponComponentPoint::OnPostLoad()
{
// 	// Look up the bone id for the attach bone
// 	const CBaseModelInfo* pBaseModelInfo = GetModelInfo();
// 	if(pBaseModelInfo && pBaseModelInfo->GetModelType() == MI_TYPE_WEAPON)
// 	{
// 		const CWeaponModelInfo* pWeaponModelInfo = static_cast<const CWeaponModelInfo*>(pBaseModelInfo);
// 		m_AttachBoneId = pWeaponModelInfo->GetBoneHierarchyId(m_AttachBone.GetHash());
// 	}
}

////////////////////////////////////////////////////////////////////////////////

INSTANTIATE_RTTI_CLASS(CWeaponInfo,0x861905B4);

////////////////////////////////////////////////////////////////////////////////

bool CWeaponInfo::ShouldPedUseCoverReloads(const CPed& ped, const CWeaponInfo& weaponInfo) 
{
	return ped.GetIsInCover() && weaponInfo.GetHasLowCoverReloads() && ped.GetPedResetFlag(CPED_RESET_FLAG_IsInLowCover) && CTaskCover::GetShouldUseCustomLowCoverAnims(ped);
}

////////////////////////////////////////////////////////////////////////////////

CWeaponInfo::CWeaponInfo()
: m_AimingInfo(NULL)
, m_WeaponDamageModifier(1.0f)
, m_DamageFallOffRangeModifier(1.0f)
, m_DamageFallOffTransientModifier(1.0f)
, m_AoEModifier(1.0f)
, m_EffectDurationModifier(1.0f)
, m_RecoilAccuracyToAllowHeadShotPlayerModifier(1.0f)
, m_MinHeadShotDistancePlayerModifier(1.0f)
, m_MaxHeadShotDistancePlayerModifier(1.0f)
, m_HeadShotDamageModifierPlayerModifier(1.0f)
{
}

////////////////////////////////////////////////////////////////////////////////

bool CWeaponInfo::ShouldPedUseAlternateCoverClipSet(const CPed& ped) const
{
	if (ped.IsPlayer() || (ped.GetEquippedWeaponInfo() && ped.GetEquippedWeaponInfo()->GetIsRpg()))
	{
		return false;
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetCoverMovementClipSetHashForPed(const CPed& ped, const FPSStreamingFlags fpsStreamingFlags) const
{
	if (!ShouldPedUseAlternateCoverClipSet(ped))
	{
		return GetPedCoverMovementClipSetId(ped, fpsStreamingFlags);
	}
	return GetPedCoverAlternateMovementClipSetId(ped, fpsStreamingFlags);
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetAimTurnCrouchingClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetAimTurnCrouchingClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetAimTurnStandingClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetAimTurnStandingClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetAlternateAimingCrouchingClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetAlternateAimingCrouchingClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetAlternateAimingStandingClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetAlternateAimingStandingClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetFiringVariationsCrouchingClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetFiringVariationsCrouchingClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetFiringVariationsStandingClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetFiringVariationsStandingClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetInjuredWeaponClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetWeaponClipSetHashInjured(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetInjuredWeaponClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetWeaponClipSetHashInjured(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetStealthWeaponClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetWeaponClipSetHashStealth(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetStealthWeaponClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetWeaponClipSetHashStealth(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetHiCoverWeaponClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetWeaponClipSetHashHiCover(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetHiCoverWeaponClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetWeaponClipSetHashHiCover(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetMeleeBaseClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMeleeBaseClipSetHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetMeleeBaseClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMeleeBaseClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetMeleeClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMeleeClipSetHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetMeleeClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMeleeClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetMeleeSupportTauntClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMeleeSupportTauntClipSetHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetMeleeSupportTauntClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMeleeSupportTauntClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetMeleeStealthClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMeleeStealthClipSetHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetMeleeStealthClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMeleeStealthClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetMeleeTauntClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMeleeTauntClipSetHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetMeleeTauntClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMeleeTauntClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetMeleeVariationClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMeleeVariationClipSetHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetMeleeVariationClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMeleeVariationClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetJumpUpperbodyClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetJumpUpperbodyClipSetHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetJumpUpperbodyClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetJumpUpperbodyClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetFallUpperbodyClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetFallUpperbodyClipSetHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetFallUpperbodyClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetFallUpperbodyClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetFromStrafeTransitionUpperBodyClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetFromStrafeTransitionUpperBodyClipSetHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetFromStrafeTransitionUpperBodyClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetFromStrafeTransitionUpperBodyClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetPedCoverAlternateMovementClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetCoverAlternateMovementClipSetHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetPedCoverAlternateMovementClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetCoverAlternateMovementClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetPedCoverMovementClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetCoverMovementClipSetHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetPedCoverMovementClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetCoverMovementClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetPedCoverMovementExtraClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetCoverMovementExtraClipSetHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetPedCoverMovementExtraClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetCoverMovementExtraClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetPedCoverWeaponClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetCoverWeaponClipSetHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetPedCoverWeaponClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetCoverWeaponClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetPedMotionClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMotionClipSetHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetPedMotionClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	TUNE_GROUP_BOOL(FIRST_PERSON_TUNE, ForcePedMotionCloneAnims, false);
	if(rPed.IsNetworkClone() || ForcePedMotionCloneAnims)
	{
		fwMvClipSetId clipSetId = GetPedMotionClipSetIdForClone(rPed, fpsStreamingFlags);
		if(clipSetId != CLIP_SET_ID_INVALID)
			return clipSetId;
	}
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMotionClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetPedMotionCrouchClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMotionCrouchClipSetHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetPedMotionCrouchClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMotionCrouchClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvFilterId CWeaponInfo::GetPedMotionFilterId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvFilterId(CWeaponAnimationsManager::GetInstance().GetMotionFilterHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetPedMotionStrafingClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMotionStrafingClipSetHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetPedMotionStrafingClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMotionStrafingClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetPedMotionStrafingStealthClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMotionStrafingStealthClipSetHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetPedMotionStrafingStealthClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMotionStrafingStealthClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetPedMotionStrafingUpperBodyClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMotionStrafingUpperBodyClipSetHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetPedMotionStrafingUpperBodyClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMotionStrafingUpperBodyClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetShellShockedClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetShellShockedClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetWeaponClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetWeaponClipSetHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetWeaponClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	u32 weaponHash = GetHash();
#if FPS_MODE_SUPPORTED
	// In FPS Mode: if we're holding an object (ie a phone), use the unarmed weapon clipset
	if (rPed.IsFirstPersonShooterModeEnabledForPlayer(false, true) && (weaponHash == ATSTRINGHASH("OBJECT", 0x39958261) || CTaskMobilePhone::IsRunningMobilePhoneTask(rPed)))
	{
		const CWeaponInfo *pUnarmedInfo = CWeaponInfoManager::GetInfo<const CWeaponInfo>(WEAPONTYPE_UNARMED);
		if (pUnarmedInfo)
		{
			weaponHash = pUnarmedInfo->GetHash();
		}
	}
#endif	//FPS_MODE_SUPPORTED
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetWeaponClipSetHash(rPed, atHashWithStringNotFinal(weaponHash), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetWeaponClipSetStreamedId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetWeaponClipSetStreamedHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetWeaponClipSetStreamedId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetWeaponClipSetStreamedHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetAlternativeClipSetWhenBlockedId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetAlternativeClipSetWhenBlocked(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetAlternativeClipSetWhenBlockedId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetAlternativeClipSetWhenBlocked(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetScopeWeaponClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetScopeWeaponClipSet(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetScopeWeaponClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetScopeWeaponClipSet(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvFilterId CWeaponInfo::GetSwapWeaponFilterId() const
{
	return fwMvFilterId(CWeaponAnimationsManager::GetInstance().GetSwapWeaponFilterHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvFilterId CWeaponInfo::GetSwapWeaponFilterId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvFilterId(CWeaponAnimationsManager::GetInstance().GetSwapWeaponFilterHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvFilterId CWeaponInfo::GetSwapWeaponInLowCoverFilterId() const
{
	return fwMvFilterId(CWeaponAnimationsManager::GetInstance().GetSwapWeaponInLowCoverFilterHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvFilterId CWeaponInfo::GetSwapWeaponInLowCoverFilterId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvFilterId(CWeaponAnimationsManager::GetInstance().GetSwapWeaponInLowCoverFilterHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

const CWeaponSwapData* CWeaponInfo::GetSwapWeaponData() const
{
	return CWeaponAnimationsManager::GetInstance().GetSwapWeaponData(atHashWithStringNotFinal(GetHash()));
}

////////////////////////////////////////////////////////////////////////////////

const CWeaponSwapData* CWeaponInfo::GetSwapWeaponData(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return CWeaponAnimationsManager::GetInstance().GetSwapWeaponData(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags);
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetSwapWeaponClipSetId() const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetSwapWeaponClipSetHash(atHashWithStringNotFinal(GetHash())));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetSwapWeaponClipSetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetSwapWeaponClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetAimGrenadeThrowClipsetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	// Use the alternate clipset if we have a scope attachment and a valid alternate clipset defined in weaponanimations.meta (useful for combat_mg etc)
	CTask *pTask = rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_ON_FOOT);
	if (pTask)
	{
		CTaskAimGunOnFoot *pTaskAimGunOnFoot = static_cast<CTaskAimGunOnFoot*>(pTask);
		if (pTaskAimGunOnFoot && pTaskAimGunOnFoot->GetTempWeaponObjectHasScope())
		{
			fwMvClipSetId altClipsetId = fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetAimGrenadeThrowAlternateClipsetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
			if (altClipsetId != CLIP_SET_ID_INVALID)
			{
				return altClipsetId;
			}
		}
	}

	fwMvClipSetId clipsetId = fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetAimGrenadeThrowClipsetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));

	weaponAssertf(clipsetId != CLIP_SET_ID_INVALID, "CWeaponInfo::GetAimGrenadeThrowClipsetId: Clipset invalid! Ensure it has been set in weaponanimations.meta. Using rifle throw anim as fallback.");
	// Use rifle clipset as a fallback if we haven't got a defined clip
	if (clipsetId == CLIP_SET_ID_INVALID)
	{
		const fwMvClipSetId backupClipsetId("Wpn_Thrown_Grenade_Aiming_Rifle", 0xe7a75138);
		return backupClipsetId;
	}

	return clipsetId;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetGestureBeckonOverride(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetGestureBeckonOverrideClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetGestureOverThereOverride(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetGestureOverThereOverrideClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetGestureHaltOverride(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetGestureHaltOverrideClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetGestureGlancesOverride(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetGestureGlancesOverrideClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetCombatReactionOverride(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetCombatReactionOverrideClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetAppropriateFPSTransitionClipsetId(const CPed& rPed, CPedMotionData::eFPSState fpsPrevStateOverride, FPSStreamingFlags streamFromFPSSet) const
{
	fwMvClipSetId transitionClipsetId = CLIP_SET_ID_INVALID;

	bool bDontPlayIntro = false;

	// Don't assert/select intro anim if states are the same (ie equipping a weapon / ie scoped -> reload -> scoped).
	if(fpsPrevStateOverride == CPedMotionData::FPS_MAX)
	{
		if(rPed.GetMotionData()->GetFPSState() == rPed.GetMotionData()->GetPreviousFPSState() && !rPed.GetMotionData()->GetToggledStealthWhileFPSIdle())
		{
			bDontPlayIntro = true;
		}
	}

	//MN: Handle Special case transitions here
	
	bool bSelectCustomTransition = fpsPrevStateOverride != CPedMotionData::FPS_MAX;
	if(bSelectCustomTransition)
	{
		switch (fpsPrevStateOverride)
		{
			case CPedMotionData::FPS_RNG:
				transitionClipsetId = GetFPSTransitionFromRNGClipsetId(rPed, streamFromFPSSet);
				break;
			case CPedMotionData::FPS_LT:
				transitionClipsetId = GetFPSTransitionFromLTClipsetId(rPed, streamFromFPSSet);
				break;
			case CPedMotionData::FPS_SCOPE:
				transitionClipsetId = GetFPSTransitionFromScopeClipsetId(rPed, streamFromFPSSet);
				break;
			case CPedMotionData::FPS_UNHOLSTER:
				transitionClipsetId = GetFPSTransitionFromUnholsterClipsetId(rPed, streamFromFPSSet);
				break;
			default:
				transitionClipsetId = GetFPSTransitionFromIdleClipsetId(rPed, streamFromFPSSet);
				break;
		}
	}

	// End Handling special case transitions

	if (!bDontPlayIntro && !bSelectCustomTransition)
	{
		// From stealth:
		bool bFromStealth = false;
		bool bToStealth = false;
		FPSStreamingFlags fpsStreamingFlags = FPS_StreamDefault;

		bool bSkipStealthTransitions = GetIsThrownWeapon() || (GetIsMelee() && !GetIsUnarmed());

		// For stealth_to_idle/idle_to_stealth transitions
		if(rPed.GetMotionData()->GetToggledStealthWhileFPSIdle() && !bSkipStealthTransitions)
		{
			if(rPed.GetMotionData()->GetSwitchedFromStealthInFPS())
			{
				bFromStealth = true;
			}
			else if(rPed.GetMotionData()->GetSwitchedToStealthInFPS())
			{
				bToStealth = true;
			}
		} 
		// For transitions to and from other states while using stealth
		else if(rPed.GetMotionData()->GetIsUsingStealthInFPS() && !bSkipStealthTransitions)
		{
			if(rPed.GetMotionData()->GetWasFPSIdle())
			{
				bFromStealth = true;
			}
			else if(rPed.GetMotionData()->GetIsFPSIdle())
			{
				bToStealth = true;

				if(rPed.GetMotionData()->GetWasFPSLT())
				{
					fpsStreamingFlags = FPS_StreamLT;
				}
				else if(rPed.GetMotionData()->GetWasFPSRNG())
				{
					fpsStreamingFlags = FPS_StreamRNG;
				}
				else if(rPed.GetMotionData()->GetWasFPSScope())
				{
					fpsStreamingFlags = FPS_StreamScope;
				}
			}
		}

		// From Stealth
		if(bFromStealth)
		{
			transitionClipsetId = GetFPSTransitionFromStealthClipsetId(rPed, fpsStreamingFlags);
		}
		// To Stealth
		else if (bToStealth)
		{
			if(rPed.GetMotionData()->GetWasFPSUnholster())
			{
				transitionClipsetId = GetFPSTransitionToStealthFromUnholsterClipsetId(rPed);
			}
			else
			{
				transitionClipsetId = GetFPSTransitionToStealthClipsetId(rPed, fpsStreamingFlags);
			}
		}
		// From Unholster:
		else if (rPed.GetMotionData()->GetWasFPSUnholster())
		{
			transitionClipsetId = GetFPSTransitionFromUnholsterClipsetId(rPed);
		}
		// From Idle:
		else if (rPed.GetMotionData()->GetWasFPSIdle())
		{
			transitionClipsetId = GetFPSTransitionFromIdleClipsetId(rPed);
		}
		// From RNG:
		else if (rPed.GetMotionData()->GetWasFPSRNG())
		{
			transitionClipsetId = GetFPSTransitionFromRNGClipsetId(rPed);
		}
		// From Full:
		else if (rPed.GetMotionData()->GetWasFPSLT())
		{
			transitionClipsetId = GetFPSTransitionFromLTClipsetId(rPed);
		}
		// From Scope:
		else if (rPed.GetMotionData()->GetWasFPSScope())
		{
			transitionClipsetId = GetFPSTransitionFromScopeClipsetId(rPed);
		}
	}

#if __BANK
	if (!bDontPlayIntro)
	{
		static const char* s_debugNames[CPedMotionData::FPS_MAX] = 
		{
			"IDLE",
			"RNG",
			"LT",
			"SCOPE",
			"UNHOLSTER"
		};

		if(!fwClipSetManager::GetClipSet(transitionClipsetId))
		{
			Warningf("CWeaponInfo::GetAppropriateFPSTransitionClipsetId: transitionClipsetId for %s is invalid! Ensure FPS transitions are set up in weaponanimations.meta. Current State: %s Prev State: %s transitionClipsetId: %s",
				this->GetName(),
				s_debugNames[rPed.GetMotionData()->GetFPSState()], 
				s_debugNames[rPed.GetMotionData()->GetPreviousFPSState()],
				transitionClipsetId.TryGetCStr() != NULL ? transitionClipsetId.TryGetCStr() : "Invalid Clipset - probably a code selection issue!");
		}

	}
#endif

	return transitionClipsetId;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetFPSTransitionFromIdleClipsetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetFPSTransitionFromIdleClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetFPSTransitionFromRNGClipsetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetFPSTransitionFromRNGClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetFPSTransitionFromLTClipsetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetFPSTransitionFromLTClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetFPSTransitionFromScopeClipsetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetFPSTransitionFromScopeClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetFPSTransitionFromUnholsterClipsetId(const CPed &rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetFPSTransitionFromUnholsterClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetFPSTransitionFromStealthClipsetId(const CPed &rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetFPSTransitionFromStealthClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetFPSTransitionToStealthClipsetId(const CPed &rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetFPSTransitionToStealthClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetFPSTransitionToStealthFromUnholsterClipsetId(const CPed &rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetFPSTransitionToStealthFromUnholsterClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}


////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetAppropriateFPSFidgetClipsetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	fwMvClipSetId fidgetClipsetId = CLIP_SET_ID_INVALID;
	CTaskMotionBase* pMotionTask = rPed.GetPrimaryMotionTaskIfExists();
	if(pMotionTask && pMotionTask->GetTaskType() == CTaskTypes::TASK_MOTION_PED)
	{
		CTaskMotionPed* pMotionPedTask = static_cast<CTaskMotionPed*>(pMotionTask);
		if(pMotionPedTask)
		{
			int iFidgetVariation = pMotionPedTask->GetFPSFidgetVariation();
			int iMaxNumVariations = CWeaponAnimationsManager::GetInstance().GetNumFPSFidgets(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags);
			if (iFidgetVariation >= 0 && iFidgetVariation < iMaxNumVariations)
			{
				fidgetClipsetId = GetFPSFidgetClipsetId(rPed, fpsStreamingFlags, iFidgetVariation);
			}
		}
	}

	return fidgetClipsetId;
}


////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetFPSFidgetClipsetId(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags, int iIndex) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetFPSFidgetClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags, iIndex));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetMovementClipSetOverride(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMovementOverrideClipSetHash(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetWeaponClipSetIdForClone(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetWeaponClipSetHashForClone(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetPedMotionClipSetIdForClone(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return fwMvClipSetId(CWeaponAnimationsManager::GetInstance().GetMotionClipSetHashForClone(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags));
}

////////////////////////////////////////////////////////////////////////////////

float CWeaponInfo::GetBlindFireRateModifier() const
{
	return CWeaponAnimationsManager::GetInstance().GetAnimBlindFireRateModifier(atHashWithStringNotFinal(GetHash()));
}

////////////////////////////////////////////////////////////////////////////////

float CWeaponInfo::GetFireRateModifier() const
{
	return CWeaponAnimationsManager::GetInstance().GetAnimFireRateModifier(atHashWithStringNotFinal(GetHash()));
}

////////////////////////////////////////////////////////////////////////////////

float CWeaponInfo::GetWantingToShootFireRateModifier() const
{
	return CWeaponAnimationsManager::GetInstance().GetAnimWantingToShootFireRateModifier(atHashWithStringNotFinal(GetHash()));
}

////////////////////////////////////////////////////////////////////////////////

bool CWeaponInfo::GetUseFromStrafeUpperBodyAimNetwork() const
{
	return CWeaponAnimationsManager::GetInstance().GetUseFromStrafeUpperBodyAimNetwork(atHashWithStringNotFinal(GetHash()));
}

////////////////////////////////////////////////////////////////////////////////

bool CWeaponInfo::GetAimingDownTheBarrel(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return CWeaponAnimationsManager::GetInstance().GetAimingDownTheBarrel(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags);
}

////////////////////////////////////////////////////////////////////////////////

bool CWeaponInfo::GetUseLeftHandIKAllowTags(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return CWeaponAnimationsManager::GetInstance().GetUseLeftHandIKAllowTags(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags);
}

////////////////////////////////////////////////////////////////////////////////

bool CWeaponInfo::GetBlockLeftHandIKWhileAiming(const CPed& rPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	return CWeaponAnimationsManager::GetInstance().GetBlockLeftHandIKWhileAiming(rPed, atHashWithStringNotFinal(GetHash()), fpsStreamingFlags);
}

////////////////////////////////////////////////////////////////////////////////

const CAmmoInfo* CWeaponInfo::GetAmmoInfo(const CPed* pPed) const
{
	if( pPed && pPed->IsPlayer() && pPed->GetInventory() )
	{
		const u32 uWeaponHash = GetHash();

		if( uWeaponHash != 0 )
		{
			if( const CWeaponItem* pWeaponItem = pPed->GetInventory()->GetWeapon(uWeaponHash) )
			{
				if( const CAmmoInfo* pAmmoInfoOverride = pWeaponItem->GetAmmoInfoOverride() )
					return pAmmoInfoOverride;
			}
		}
	}

	return m_AmmoInfo;
}

////////////////////////////////////////////////////////////////////////////////

u32 CWeaponInfo::GetAmmoMax(const CPed* pPed) const
{
	const CAmmoInfo* pInfo = GetAmmoInfo(pPed);
	if(pInfo)
	{
		return pInfo->GetAmmoMax(pPed);
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

float CWeaponInfo::GetProjectileFuseTime(bool bInVehicle) const
{
	const CAmmoInfo* pInfo = GetAmmoInfo();
	if(pInfo && pInfo->GetIsClassId(CAmmoProjectileInfo::GetStaticClassId()))
	{
		const CAmmoProjectileInfo* pAmmoProjectileInfo = static_cast<const CAmmoProjectileInfo*>(pInfo);
		if(pAmmoProjectileInfo->GetHasFuse())
		{
			return bInVehicle ? pAmmoProjectileInfo->GetFromVehicleLifeTime() : pAmmoProjectileInfo->GetLifeTime();
		}
	}

	return 0.0f;
}

////////////////////////////////////////////////////////////////////////////////

float CWeaponInfo::GetProjectileImpactThreshold() const
{
	const CAmmoInfo* pInfo = GetAmmoInfo();
	if(pInfo && pInfo->GetIsClassId(CAmmoProjectileInfo::GetStaticClassId()))
	{
		const CAmmoProjectileInfo* pAmmoProjectileInfo = static_cast<const CAmmoProjectileInfo*>(pInfo);
		return pAmmoProjectileInfo->GetRicochetTolerance();
	}

	return 0.0f;
}

////////////////////////////////////////////////////////////////////////////////

float CWeaponInfo::GetProjectileImpactVehicleThreshold() const
{
	const CAmmoInfo* pInfo = GetAmmoInfo();
	if(pInfo && pInfo->GetIsClassId(CAmmoProjectileInfo::GetStaticClassId()))
	{
		const CAmmoProjectileInfo* pAmmoProjectileInfo = static_cast<const CAmmoProjectileInfo*>(pInfo);
		return pAmmoProjectileInfo->GetVehicleRicochetTolerance();
	}

	return 0.0f;
}

////////////////////////////////////////////////////////////////////////////////

eExplosionTag CWeaponInfo::GetProjectileExplosionTag() const
{
	const CAmmoInfo* pInfo = GetAmmoInfo();
	if(pInfo && pInfo->GetIsClassId(CAmmoProjectileInfo::GetStaticClassId()))
	{
		const CAmmoProjectileInfo* pAmmoProjectileInfo = static_cast<const CAmmoProjectileInfo*>(pInfo);
		return pAmmoProjectileInfo->GetExplosionTag();
	}

	return EXP_TAG_DONTCARE;
}

////////////////////////////////////////////////////////////////////////////////

bool CWeaponInfo::GetProjectileAddSmokeOnExplosion() const
{
	const CAmmoInfo* pInfo = GetAmmoInfo();
	if(pInfo)
	{
		return pInfo->GetAddSmokeOnExplosion();
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CWeaponInfo::GetProjectileFixedAfterExplosion() const
{
	const CAmmoInfo* pInfo = GetAmmoInfo();
	if(pInfo)
	{
		return pInfo->GetFixedAfterExplosion();
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////

bool CWeaponInfo::GetProjectileCanBePlaced() const
{
	const CAmmoInfo* pInfo = GetAmmoInfo();
	if(pInfo && pInfo->GetIsClassId(CAmmoProjectileInfo::GetStaticClassId()))
	{
		const CAmmoProjectileInfo* pAmmoProjectileInfo = static_cast<const CAmmoProjectileInfo*>(pInfo);
		return pAmmoProjectileInfo->GetCanBePlaced();
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

eExplosionTag CWeaponInfo::GetExplosionTag(const CEntity* pHitEntity) const
{
	eExplosionTag expTag = EXP_TAG_DONTCARE;

	if(pHitEntity && pHitEntity->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(pHitEntity);
		if(pVehicle->m_nVehicleFlags.bIsBig)
		{
			expTag = m_Explosion.HitTruck;
		}
		else if(pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || pVehicle->GetVehicleType()==VEHICLE_TYPE_TRAILER)
		{
			expTag = m_Explosion.HitCar;
		}
		else if(pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE || pVehicle->GetIsRotaryAircraft())
		{
			expTag = m_Explosion.HitPlane;
		}
		else if(pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE)
		{
			expTag = m_Explosion.HitBike;
		}
		else if(pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT)
		{
			expTag = m_Explosion.HitBoat;
		}
	}

	// If we are set to don't care, set it to the default explosion tag
	if(expTag == EXP_TAG_DONTCARE)
	{
		expTag = GetExplosionTag();
	}

	return expTag;
}


////////////////////////////////////////////////////////////////////////////////

f32 CWeaponInfo::GetReloadTime() const
{
	return NetworkInterface::IsGameInProgress() ? m_ReloadTimeMP : m_ReloadTimeSP;
}

////////////////////////////////////////////////////////////////////////////////

bool CWeaponInfo::CanBeUsedAsDriveBy(const CPed* pPed) const
{
	u32 uWeaponGroupHash = GetGroup();

	CVehicle* myVehicle = pPed->GetMyVehicle();
	CPed* myMount = pPed->GetMyMount();
	const CVehicleSeatAnimInfo* pSeatAnimInfo = NULL;

	if( (myVehicle && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle )) ||
		(myMount && pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_OnMount )) ||
		pPed->GetIsParachuting() ||
		pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack))
	{
		if (GetIsUnarmed() && (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableUnarmedDrivebys) || pPed->GetPedResetFlag(CPED_RESET_FLAG_DisableUnarmedDrivebys)))
		{
			return false;
		}

		// Don't allow drivebys if ped is in an air defence sphere
		if (pPed->GetPedResetFlag(CPED_RESET_FLAG_InAirDefenceSphere))
		{
			return false;
		}

		const CVehicleDriveByInfo* pDrivebyInfo = (pPed->GetIsParachuting() || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsUsingJetpack) ? CVehicleMetadataMgr::GetVehicleDriveByInfoFromPed(pPed) : NULL);
		if(!pDrivebyInfo)
		{
			if (myMount) 
			{		
				pSeatAnimInfo = myMount->GetPedModelInfo()->GetLayoutInfo()->GetSeatAnimationInfo(myMount->GetSeatManager()->GetPedsSeatIndex(pPed));
			} 
			else if(myVehicle)
			{
				s32 seatPedOccupies = myVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);

				if(seatPedOccupies == -1)
				{
					return false;
				}

				// I don't know why we're only allowing combination driveby / vehicle weapons for the driver seat...
				if(seatPedOccupies != 0 && myVehicle->GetSeatHasWeaponsOrTurrets(seatPedOccupies))
				{
					bool bIsMulePassenger = MI_CAR_MULE4.IsValid() && myVehicle->GetModelIndex() == MI_CAR_MULE4 && seatPedOccupies == 1;
					bool bIsPounderPassenger = MI_CAR_POUNDER2.IsValid() && myVehicle->GetModelIndex() == MI_CAR_POUNDER2 && seatPedOccupies == 1;
					if (!bIsMulePassenger && !bIsPounderPassenger)
					{
						return false;
					}
				}
				pSeatAnimInfo = myVehicle->GetSeatAnimationInfo(seatPedOccupies);
			}

			// Look up vehicle seat info
			if(aiVerifyf(pSeatAnimInfo, "Expected pSeatAnimInfo!"))
			{
				pDrivebyInfo = pSeatAnimInfo->GetDriveByInfo();
			}
		}

		if(pDrivebyInfo)
		{
			return pDrivebyInfo->CanUseWeaponGroup(uWeaponGroupHash) || pDrivebyInfo->CanUseWeaponType(GetHash());
		}

		return false;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

const CVehicleWeaponInfo* CWeaponInfo::GetVehicleWeaponInfo() const
{
	return CWeaponInfoManager::GetVehicleWeaponInfo(m_VehicleWeaponHash.GetHash());
}

////////////////////////////////////////////////////////////////////////////////

bool CWeaponInfo::GetDoesRecoilAccuracyAllowHeadShotModifier(const CPed* pHitPed, const float fRecoilAccuracyWhenFired, const float fDistance) const
{
	weaponAssert(pHitPed);

	// Make weapons more easily tunable by normalizing the spread at the same distance
	float fSpread = GetAccuracySpread();
	const float fScaledRange = fDistance / CWeapon::STANDARD_WEAPON_ACCURACY_RANGE;
	fSpread *= fScaledRange;

	float fAccuracy = (1.f - fRecoilAccuracyWhenFired) * fSpread;
	if(pHitPed->IsPlayer())
	{
		return fAccuracy < GetRecoilAccuracyToAllowHeadShotPlayer();
	}
	else
	{
		return fAccuracy < GetRecoilAccuracyToAllowHeadShotAI();
	}
}

////////////////////////////////////////////////////////////////////////////////

const CWeaponComponentPoint* CWeaponInfo::GetAttachPoint(const CWeaponComponentInfo* pComponentInfo) const
{
	weaponAssertf(pComponentInfo, "pComponentInfo is NULL");

	const CWeaponInfo::AttachPoints& attachPoints = GetAttachPoints();
	for(s32 i = 0; i < attachPoints.GetCount(); i++)
	{
		const CWeaponComponentPoint::Components& components = attachPoints[i].GetComponents();
		for(s32 j = 0; j < components.GetCount(); j++)
		{
			if(components[j].m_Name.GetHash() == pComponentInfo->GetHash())
			{
				return &attachPoints[i];
			}
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////

const bool CWeaponInfo::GetIsWeaponComponentDefault(atHashString weaponComponentName) const
{
	for(int i=0;i<m_AttachPoints.GetCount();++i)
	{
		const CWeaponComponentPoint::Components& components = m_AttachPoints[i].GetComponents();
		for(int j=0;j<components.GetCount();j++)
		{
			if(weaponComponentName==components[j].m_Name.GetHash())
			{
				return components[j].m_Default;
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

Vector3 CWeaponInfo::GetAimOffset( float fPitchRatio, CPedMotionData::eFPSState FPS_MODE_SUPPORTED_ONLY(fpsState) ) const
{
	Vector3 vAimOffsetMin = GetAimOffsetMin();
	Vector3 vAimOffsetMax = GetAimOffsetMax();
#if FPS_MODE_SUPPORTED
	switch(fpsState)
	{
	case CPedMotionData::FPS_IDLE:
		{
			vAimOffsetMin = GetAimOffsetMinFPSIdle();
			Vector3 vAimOffsetMed = GetAimOffsetMedFPSIdle();
			vAimOffsetMax = GetAimOffsetMaxFPSIdle();
			Vector3 vAimOffset( Vector3::ZeroType );
			if(fPitchRatio < 0.5f)
			{
				vAimOffset.Lerp( fPitchRatio / 0.5f, vAimOffsetMin, vAimOffsetMed );
			}
			else
			{
				vAimOffset.Lerp( (fPitchRatio - 0.5f) / 0.5f, vAimOffsetMed, vAimOffsetMax );
			}

			return vAimOffset;
		}
	case CPedMotionData::FPS_LT:
		vAimOffsetMin = GetAimOffsetMinFPSLT();
		vAimOffsetMax = GetAimOffsetMaxFPSLT();
		break;
	case CPedMotionData::FPS_RNG:
		vAimOffsetMin = GetAimOffsetMinFPSRNG();
		vAimOffsetMax = GetAimOffsetMaxFPSRNG();
		break;
	case CPedMotionData::FPS_SCOPE:
		vAimOffsetMin = GetAimOffsetMinFPSScope();
		vAimOffsetMax = GetAimOffsetMaxFPSScope();
		break;
	default:
		// Do nothing
		break;
	}

	// Temp: Ignore the FPS values if they haven't been tuned.
	if(vAimOffsetMin.Mag2() == 0.0f && vAimOffsetMax.Mag2() == 0.0f)
	{
		vAimOffsetMin = GetAimOffsetMin();
		vAimOffsetMax = GetAimOffsetMax();
	}
#endif	// FPS_MODE_SUPPORTED
	Vector3 vAimOffset( Vector3::ZeroType );
	vAimOffset.Lerp( fPitchRatio, vAimOffsetMin, vAimOffsetMax );
	return vAimOffset;
}

////////////////////////////////////////////////////////////////////////////////

Vector3 CWeaponInfo::GetAimOffsetEndPosFPSIdle( float fPitchRatio ) const
{
	Vector3 vAimOffsetEndPos( Vector3::ZeroType );
	if(fPitchRatio < 0.5f)
	{
		vAimOffsetEndPos.Lerp( fPitchRatio / 0.5f, GetAimOffsetEndPosMinFPSIdle(), GetAimOffsetEndPosMedFPSIdle() );
	}
	else
	{
		vAimOffsetEndPos.Lerp( (fPitchRatio - 0.5f) / 0.5f, GetAimOffsetEndPosMedFPSIdle(), GetAimOffsetEndPosMaxFPSIdle() );
	}
	return vAimOffsetEndPos;
}

////////////////////////////////////////////////////////////////////////////////

Vector3 CWeaponInfo::GetAimOffsetEndPosFPSLT( float fPitchRatio ) const
{
	Vector3 vAimOffsetEndPos( Vector3::ZeroType );
	if(fPitchRatio < 0.5f)
	{
		vAimOffsetEndPos.Lerp( fPitchRatio / 0.5f, GetAimOffsetEndPosMinFPSLT(), GetAimOffsetEndPosMedFPSLT() );
	}
	else
	{
		vAimOffsetEndPos.Lerp( (fPitchRatio - 0.5f) / 0.5f, GetAimOffsetEndPosMedFPSLT(), GetAimOffsetEndPosMaxFPSLT() );
	}
	return vAimOffsetEndPos;
}

////////////////////////////////////////////////////////////////////////////////

float CWeaponInfo::GetAimProbeRadiusOverride(CPedMotionData::eFPSState FPS_MODE_SUPPORTED_ONLY(fpsState), bool bUseFPSStealth ) const
{
#if FPS_MODE_SUPPORTED
	switch(fpsState)
	{
	case CPedMotionData::FPS_IDLE:
		if(bUseFPSStealth)
			return GetAimProbeRadiusOverrideFPSIdleStealth();
		else
			return GetAimProbeRadiusOverrideFPSIdle();
		break;
	case CPedMotionData::FPS_LT:
		return GetAimProbeRadiusOverrideFPSLT();
		break;
	case CPedMotionData::FPS_RNG:
		return GetAimProbeRadiusOverrideFPSRNG();
		break;
	case CPedMotionData::FPS_SCOPE:
		return GetAimProbeRadiusOverrideFPSScope();
		break;
	default:
		// Do nothing
		break;
	}
#endif	// FPS_MODE_SUPPORTED

	return 0.0f;
}

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfo::GetTorsoAimOffsetForPed(Vector3& vAimOffset, const CPed& ped) const
{
#if FPS_MODE_SUPPORTED
	const bool bIsFirstPersonShooting = ped.IsFirstPersonShooterModeEnabledForPlayer(false);
	if (bIsFirstPersonShooting)
	{
		TUNE_GROUP_BOOL(FIRST_PERSON_COVER_TUNE, DISABLE_TORSO_AIM_OFFSET_GLOBAL, false);
		if (DISABLE_TORSO_AIM_OFFSET_GLOBAL)
			return;
	}
	TUNE_GROUP_BOOL(FIRST_PERSON_COVER_TUNE, DISABLE_TORSO_AIM_OFFSET_IN_LEFT_COVER, true);
#endif // FPS_MODE_SUPPORTED

	if (ped.GetIsCrouching())
	{
		vAimOffset += GetTorsoCrouchedAimOffset().x*VEC3V_TO_VECTOR3(ped.GetTransform().GetA()); // GetRight
		vAimOffset += GetTorsoCrouchedAimOffset().y*VEC3V_TO_VECTOR3(ped.GetTransform().GetC());
	}
	else
	{
#if FPS_MODE_SUPPORTED
		const bool bDisableLeftCoverIkOffsets = bIsFirstPersonShooting ? DISABLE_TORSO_AIM_OFFSET_IN_LEFT_COVER : false;
#endif // FPS_MODE_SUPPORTED
		if (ped.GetPedResetFlag(CPED_RESET_FLAG_InCoverFacingLeft) 
#if FPS_MODE_SUPPORTED
			&& !bDisableLeftCoverIkOffsets
#endif // FPS_MODE_SUPPORTED
		)
		{
			const float fLateralTorsoOffset = GetTorsoAimOffset().x + CTaskInCover::ms_Tunables.m_GlobalLateralTorsoOffsetInLeftCover;
			vAimOffset += fLateralTorsoOffset*VEC3V_TO_VECTOR3(ped.GetTransform().GetA()); // GetRight
			vAimOffset += GetTorsoAimOffset().y*VEC3V_TO_VECTOR3(ped.GetTransform().GetC());
		}
		else if(GetHash() == ATSTRINGHASH("WEAPON_RAYMINIGUN", 0xB62D1F67))
		{
			// url:bugstar:5518472 - In order to get better results for laser bullet tracers to appear in the right direction, use a blend of high/med/low torso aim values
			const CTaskMotionAiming* pMotionAimingTask = static_cast<CTaskMotionAiming*>(ped.GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_AIMING));
			if (pMotionAimingTask)
			{
				float fCurrentPitch = pMotionAimingTask->GetCurrentPitch();
				float fTorsoAimOffset = 0.0f;

				TUNE_GROUP_FLOAT(TORSO_AIM_OVERRIDE, RAYMINI_HIGH_X, 0.5f, -10.0f, 10.0f, 0.01f);
				TUNE_GROUP_FLOAT(TORSO_AIM_OVERRIDE, RAYMINI_MEDIUM_X, -0.7f, -10.0f, 10.0f, 0.01f);
				TUNE_GROUP_FLOAT(TORSO_AIM_OVERRIDE, RAYMINI_LOW_X, -1.75f, -10.0f, 10.0f, 0.01f);

				if (fCurrentPitch <= 0.5)
				{
					fTorsoAimOffset = Lerp(fCurrentPitch * 2.0f, RAYMINI_LOW_X, RAYMINI_MEDIUM_X);
				}
				else
				{
					fTorsoAimOffset = Lerp((fCurrentPitch - 0.5f) * 2.0f, RAYMINI_MEDIUM_X, RAYMINI_HIGH_X);
				}
				
				vAimOffset += fTorsoAimOffset*VEC3V_TO_VECTOR3(ped.GetTransform().GetA()); // GetRight
			}
		}
		else
		{
			TUNE_GROUP_FLOAT(ASSISTED_AIM_TUNE, fAssistedAimTorseAimOffsetMultiplierX, 0.0f, -2.0f, 2.0f, 0.01f);
			TUNE_GROUP_FLOAT(ASSISTED_AIM_TUNE, fAssistedAimTorseAimOffsetMultiplierY, 1.0f, -2.0f, 2.0f, 0.01f);
			float fMultiplierX = ped.GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON) ? fAssistedAimTorseAimOffsetMultiplierX : 1.0f;
			float fMultiplierY = ped.GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON) ? fAssistedAimTorseAimOffsetMultiplierY : 1.0f;
			vAimOffset += GetTorsoAimOffset().x*fMultiplierX*VEC3V_TO_VECTOR3(ped.GetTransform().GetA()); // GetRight
			vAimOffset += GetTorsoAimOffset().y*fMultiplierY*VEC3V_TO_VECTOR3(ped.GetTransform().GetC()); // GetUp
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

u32 CWeaponInfo::GetFiringPatternAlias(u32 uFiringPattern) const
{
	if(m_FiringPatternAliases)
	{
		u32 uAlias = m_FiringPatternAliases->GetAlias(uFiringPattern);
		if(uAlias != 0)
		{
			return uAlias;
		}
	}
	return uFiringPattern;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetAppropriateWeaponClipSetId(const CPed* pPed, const FPSStreamingFlags fpsStreamingFlags) const 
{
	if(pPed)
	{
		bool bUsingStealth =  pPed->GetMotionData()->GetUsingStealth();

		bool bInFPSMode = false;
#if FPS_MODE_SUPPORTED
		bInFPSMode = pPed->IsFirstPersonShooterModeEnabledForPlayer(false, true); 		
		if(bInFPSMode && !pPed->IsNetworkClone())
		{
			bUsingStealth = pPed->GetMotionData()->GetIsUsingStealthInFPS();
		}
#endif

		// B*2114283: Don't use melee stealth weapon anim for local players in stealth (we use normal weapon anims + IK).
		bool bPedUsingMeleeWeaponInStealthInFPS = pPed->GetEquippedWeaponInfo() && pPed->GetEquippedWeaponInfo()->GetIsMelee() && !pPed->GetEquippedWeaponInfo()->GetIsUnarmed() && !pPed->GetEquippedWeaponInfo()->GetIsMeleeFist()
												&& bUsingStealth && bInFPSMode;

		TUNE_GROUP_BOOL(FIRST_PERSON_TUNE, ForceWeaponCloneAnims, false);
		if(pPed->IsNetworkClone() || ForceWeaponCloneAnims)
		{
			// Use 3rd person stealth aiming anims on clone peds.
			if(bUsingStealth && bInFPSMode)
			{
				// Use FPS-specific stealth anims on clone peds
				FPSStreamingFlags eFpsStreamingFlags = fpsStreamingFlags;
				if (!bPedUsingMeleeWeaponInStealthInFPS)
				{
					eFpsStreamingFlags = FPS_StreamThirdPerson;
				}

				const fwMvClipSetId stealthClip = GetStealthWeaponClipSetId(*pPed, eFpsStreamingFlags);
				if(stealthClip != CLIP_SET_ID_INVALID)
				{
					return stealthClip;
				}
			}

			const fwMvClipSetId cloneClipSetId = GetWeaponClipSetIdForClone(*pPed, fpsStreamingFlags);
			if(cloneClipSetId != CLIP_SET_ID_INVALID)
			{
				return cloneClipSetId;
			}
		}

		if(pPed->GetPedResetFlag(CPED_RESET_FLAG_UseAlternativeWhenBlock))
		{
			const fwMvClipSetId clipWhenBlocked = GetAlternativeClipSetWhenBlockedId(*pPed, fpsStreamingFlags);
			if(clipWhenBlocked != CLIP_SET_ID_INVALID)
			{
				return clipWhenBlocked;
			}
		}
		if(pPed->HasHurtStarted())
		{
			const fwMvClipSetId clipWhenInjured = GetInjuredWeaponClipSetId(*pPed, fpsStreamingFlags);
			if(clipWhenInjured != CLIP_SET_ID_INVALID)
			{
				return clipWhenInjured;
			}
		}

		if(bUsingStealth)
		{
			bool bUseStealthClip = true;
			if (bPedUsingMeleeWeaponInStealthInFPS && !pPed->IsNetworkClone())
			{
				bUseStealthClip = false;
			}

			const fwMvClipSetId stealthClip = GetStealthWeaponClipSetId(*pPed, fpsStreamingFlags);
			if(stealthClip != CLIP_SET_ID_INVALID && bUseStealthClip)
			{
				return stealthClip;
			}
		}
		else if(pPed->GetPedResetFlag(CPED_RESET_FLAG_IsAimingFromCover))
		{
			CCoverPoint::eCoverHeight coverPointHeight;
			if (CCover::GetPedsCoverHeight(*pPed, coverPointHeight) && coverPointHeight != CCoverPoint::COVHEIGHT_LOW)
			{
				const fwMvClipSetId hiCoverClip = GetHiCoverWeaponClipSetId(*pPed, fpsStreamingFlags);
				if(hiCoverClip != CLIP_SET_ID_INVALID)
				{
					return hiCoverClip;
				}
			}
		}
		else if ((pPed->GetPedResetFlag(CPED_RESET_FLAG_PlayFPSIdleFidgets) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_PlayFPSIdleFidgetsForProjectile))
					&& !pPed->GetPedResetFlag(CPED_RESET_FLAG_EquippedWeaponChanged))
		{
			const fwMvClipSetId fpsFidgetclip = GetAppropriateFPSFidgetClipsetId(*pPed, fpsStreamingFlags);
			if(fpsFidgetclip != CLIP_SET_ID_INVALID)
			{
				return fpsFidgetclip;
			}
		}
		const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();
		if(pWeapon && pWeapon->GetScopeComponent() && !pPed->GetIsInVehicle())
		{
			const fwMvClipSetId clipWhenBlocked = GetScopeWeaponClipSetId(*pPed, fpsStreamingFlags);
			if(clipWhenBlocked != CLIP_SET_ID_INVALID)
			{
				return clipWhenBlocked;
			}
		}

		return GetWeaponClipSetId(*pPed, fpsStreamingFlags);
	}
	
	return GetWeaponClipSetId();
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetAppropriateReloadWeaponClipSetId(const CPed* pPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	if (pPed)
	{
		if (ShouldPedUseCoverReloads(*pPed, *this))
		{
			return GetPedCoverWeaponClipSetId(*pPed, fpsStreamingFlags);
		}
		else
		{
			return (pPed->HasHurtStarted() && GetInjuredWeaponClipSetId(*pPed, fpsStreamingFlags) != CLIP_SET_ID_INVALID) ? GetInjuredWeaponClipSetId(*pPed, fpsStreamingFlags) : (GetWeaponClipSetStreamedId(*pPed, fpsStreamingFlags) != CLIP_SET_ID_INVALID ? GetWeaponClipSetStreamedId(*pPed, fpsStreamingFlags) : GetWeaponClipSetId(*pPed, fpsStreamingFlags));
		}
	}
	return CLIP_SET_ID_INVALID;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetAppropriateSwapWeaponClipSetId(const CPed* pPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	if (pPed)
	{
		if (pPed->GetIsInCover() && pPed->GetPedResetFlag(CPED_RESET_FLAG_IsInLowCover) && GetHasLowCoverSwaps())
		{
			return GetPedCoverWeaponClipSetId(*pPed, fpsStreamingFlags);
		}
		else if((pPed->WantsToUseActionMode() || pPed->GetMotionData()->GetUsingStealth()) && pPed->GetMovementModeDataForWeaponHash(GetHash()).m_UnholsterClipSetId != CLIP_SET_ID_INVALID)
		{
			return pPed->GetMovementModeDataForWeaponHash(GetHash()).m_UnholsterClipSetId;
		}
		else if (GetSwapWeaponClipSetId(*pPed, fpsStreamingFlags) != CLIP_SET_ID_INVALID)
		{
			return GetSwapWeaponClipSetId(*pPed, fpsStreamingFlags);
		}
		else
		{
			return GetAppropriateWeaponClipSetId(pPed, fpsStreamingFlags);
		}
	}
	return CLIP_SET_ID_INVALID;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CWeaponInfo::GetAppropriateFromStrafeWeaponClipSetId(const CPed* pPed, const FPSStreamingFlags fpsStreamingFlags) const
{
	fwMvClipSetId clipSetId = CLIP_SET_ID_INVALID;
	if (pPed)
	{
		if(pPed->GetMotionData()->GetUsingStealth())
		{
			clipSetId = GetStealthWeaponClipSetId(*pPed, fpsStreamingFlags);
		}

		if(clipSetId == CLIP_SET_ID_INVALID)
		{
			clipSetId = GetFromStrafeTransitionUpperBodyClipSetId(*pPed, fpsStreamingFlags);
		}
	}
	return clipSetId;
}

////////////////////////////////////////////////////////////////////////////////

#if !__FINAL
bool CWeaponInfo::Validate() const
{
	// Vehicle weapon component
	if(!weaponVerifyf(m_VehicleWeaponHash.GetHash() == 0 || CWeaponInfoManager::GetVehicleWeaponInfo(m_VehicleWeaponHash.GetHash()), "%s: Vehicle weapon component [%s] doesn't exist in data", GetName(), m_VehicleWeaponHash.GetCStr()))
	{
		return false;
	}

	// Pickup
	if(!weaponVerifyf(m_PickupHash.GetHash() == 0 || m_PickupHash.GetHash() != PICKUP_HEALTH_STANDARD, "%s: Pickup [%s] doesn't exist in data", GetName(), m_PickupHash.GetCStr()))
	{
		return false;
	}

	// Allowed components
	for(s32 i = 0; i < m_AttachPoints.GetCount(); i++)
	{
		const CWeaponComponentPoint::Components& components = m_AttachPoints[i].GetComponents();
		for(s32 j = 0; j < components.GetCount(); j++)
		{
			if(!weaponVerifyf(components[j].m_Name.GetHash() == 0 || CWeaponComponentManager::GetInfo(components[j].m_Name.GetHash()), "%s: Weapon component [%s] doesn't exist in data", GetName(), components[j].m_Name.GetCStr()))
			{
				return false;
			}

			// Validate the override ammo info
			const CWeaponComponentInfo* pComponentInfo = CWeaponComponentManager::GetInfo(components[j].m_Name.GetHash());
			if (pComponentInfo->GetIsClass<CWeaponComponentClipInfo>())
			{
				u32 uAmmoOverrideHash = static_cast<const CWeaponComponentClipInfo*>(pComponentInfo)->GetAmmoInfo();
				if (uAmmoOverrideHash != 0)
				{
					// Don't allow ammo overriding with vehicle weapons
					if (GetIsVehicleWeapon())
					{
						weaponAssertf(0, "Overriding AmmoInfo is not supported on vehicle weapons (%s)", GetName());
					}

					// Don't allow ammo overriding with projectile weapons or ones that create visible ordnance
					if (GetIsProjectile() || GetCreateVisibleOrdnance())
					{
						weaponAssertf(0, "Overriding AmmoInfo is not supported on projectile weapons (%s)", GetName());
					}

					// Don't allow ammo overriding with non-base ammo types
					const CAmmoInfo* pAmmoInfo = CWeaponInfoManager::GetInfo<CAmmoInfo>(uAmmoOverrideHash);
					if (pAmmoInfo && (pAmmoInfo->GetIsClassId(CAmmoProjectileInfo::GetStaticClassId()) || pAmmoInfo->GetIsClassId(CAmmoRocketInfo::GetStaticClassId()) || pAmmoInfo->GetIsClassId(CAmmoThrownInfo::GetStaticClassId())))
					{
						weaponAssertf(0, "Unable to use non-base AmmoInfo %s defined in %s, only CAmmoInfo type is supported", pAmmoInfo->GetName(), components[j].m_Name.GetCStr());
					}		
				}
			}
		}			
	}

	// Base class
	return CItemInfo::Validate();
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CWeaponInfo::AddTuningWidgets(bkBank& bank)
{
	bank.AddSlider("SP:Damage", &m_Damage, bkSlider::FLOAT_MIN_VALUE, bkSlider::FLOAT_MAX_VALUE, 1.0f);
	bank.AddSlider("SP:EnduranceDamage", &m_EnduranceDamage, bkSlider::FLOAT_MIN_VALUE, bkSlider::FLOAT_MAX_VALUE, 1.0f);
	bank.AddSlider("SP:HitLimbsDamageModifier", &m_HitLimbsDamageModifier, 0.0f, 1.0f, 0.1f);
	bank.AddSlider("MP:PlayerDamageModifier", &m_NetworkPlayerDamageModifier, bkSlider::FLOAT_MIN_VALUE, bkSlider::FLOAT_MAX_VALUE, 1.0f);
	bank.AddSlider("MP:PedDamageModifier", &m_NetworkPedDamageModifier, bkSlider::FLOAT_MIN_VALUE, bkSlider::FLOAT_MAX_VALUE, 1.0f);
	bank.AddSlider("MP:HeadShotPlayerDamageModifier", &m_NetworkHeadShotPlayerDamageModifier, bkSlider::FLOAT_MIN_VALUE, bkSlider::FLOAT_MAX_VALUE, 1.0f);
	bank.AddSlider("MP:HitLimbsDamageModifier", &m_NetworkHitLimbsDamageModifier, 0.0f, 1.0f, 0.1f);
	bank.AddSlider("Both:LightlyArmouredDamageModifier", &m_LightlyArmouredDamageModifier, 0.0f, 1.0f, 0.1f);
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

void CWeaponInfo::OnPostLoad()
{
	// Look up the bone id for the attach bone
	const CBaseModelInfo* pBaseModelInfo = GetModelInfo();
	if(pBaseModelInfo && pBaseModelInfo->GetModelType() == MI_TYPE_WEAPON)
	{
		const CWeaponModelInfo* pWeaponModelInfo = static_cast<const CWeaponModelInfo*>(pBaseModelInfo);

		for(s32 i = 0; i < m_AttachPoints.GetCount(); i++)
		{
			m_AttachPoints[i].SetAttachBoneId(pWeaponModelInfo->GetBoneHierarchyId(m_AttachPoints[i].GetAttachBoneHash()));
		}
	}

#define ROUND(a,precision) (Floorf(0.5f+(a)/(precision))*precision)

	float fAvgFallOffRange = (GetDamageFallOffRangeMin()+GetDamageFallOffRangeMax())*0.5f;
	// set up default HUD values based on real values (which admittedly are probably rubbish when compared to each other)
	if( m_HudDamage		== -1 )	m_HudDamage		= s8(Clamp(		int(GetDamage()),							0, 100));
	if( m_HudSpeed		== -1 )	m_HudSpeed		= s8(Clamp( 100-int(GetTimeBetweenShots() * 100.0f),		0, 100));
	if( m_HudCapacity	== -1 )	m_HudCapacity	= s8(Clamp(		int(GetClipSize()),							0, 100));
	if( m_HudAccuracy	== -1 )	m_HudAccuracy	= s8(Clamp(		int(GetAccuracySpread() * 10.0f),			0, 100));
	if( m_HudRange		== -1 )	m_HudRange		= s8(Clamp( int(ROUND(fAvgFallOffRange / 1.50f,5.0f)),0, 100)); // 1.5 is 150m with the *100 factored in
}

////////////////////////////////////////////////////////////////////////////////

f32 CWeaponInfo::GetForceHitPed(eAnimBoneTag boneTagHit, bool front, float distance) const
{
	// no specific override for this bone - use the ped default
	float fForce = m_ForceHitPed;

	// look for a bone tag specific override
	for (s32 i=0; i<m_OverrideForces.GetCount(); i++)
	{
		if (m_OverrideForces[i].BoneTag == boneTagHit)
		{
			fForce = front ? m_OverrideForces[i].ForceFront : m_OverrideForces[i].ForceBack;
			break;
		}
	}

	fForce *= GetForceDistanceModifier(distance);

	return fForce;
}

/////////////////////////////////////////////////////////////////////////////////

f32 CWeaponInfo::GetForceDistanceModifier(float fDistance) const
{
	// The weapon's range should never be less than the start of the falloff range
	weaponAssert(m_ForceFalloffRangeEnd >= m_ForceFalloffRangeStart);

	float fForceModifier = 1.0f;

	// If projectile has traveled to or past the range of the weapon, then shot force is just modified by the
	// minimum falloff amount
	if (fDistance >= m_ForceFalloffRangeEnd)
	{
		fForceModifier = m_ForceFalloffMin;
	}
	// Otherwise, if projectile has traveled farther than the falloff range, we need force to start falling off
	else if (fDistance >= m_ForceFalloffRangeStart)
	{
		// The weapon falloff range is the start range to the total range of the weapon
		float fWeaponFalloffRange = m_ForceFalloffRangeEnd - m_ForceFalloffRangeStart;
		// Determine how far within the falloff range the projectile has traveled
		float fAdjustedDistanceProjectileTraveled = fDistance - m_ForceFalloffRangeStart;

		weaponAssert(fAdjustedDistanceProjectileTraveled >= 0.0f);

		// This ensures that we falloff to the minimum falloff amount (which might not be 0.0f)
		fAdjustedDistanceProjectileTraveled *= 1.0f - m_ForceFalloffMin;

		fForceModifier = Clamp(1.0f - (fAdjustedDistanceProjectileTraveled / fWeaponFalloffRange), 0.0f, 1.0f);
	}

	return fForceModifier;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetNextDiffuseCamoTexture
// PURPOSE	:	
/////////////////////////////////////////////////////////////////////////////////
bool CWeaponInfo::GetNextCamoDiffuseTexIdx( const CWeapon& rWeapon, u32& uNext ) const
{
	const CWeaponModelInfo* pWeaponModelInfo = rWeapon.GetWeaponModelInfo();
	if( pWeaponModelInfo )
	{
		const atBinaryMap<u32, u32>* pMap = m_CamoDiffuseTexIdxs.SafeGet( pWeaponModelInfo->GetModelNameHash() );
		if( pMap )
		{
			const u32* pNext = pMap->SafeGet( rWeapon.GetCamoDiffuseTexIdx() );
			if( pNext )
			{
				uNext = *pNext;

				return true;
			}
		}
	}

	return false;
}
