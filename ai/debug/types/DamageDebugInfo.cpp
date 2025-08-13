#include "ai\debug\types\DamageDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

#include "camera\CamInterface.h"
#include "Peds\Ped.h"
#include "Peds\PedDebugVisualiser.h"
#include "Peds\PlayerInfo.h"

CDamageDebugInfo::CDamageDebugInfo(const CPed* pPed, bool bOnlyDamageRecords, DebugInfoPrintParams printParams)
	: CAIDebugInfo(printParams)
	, m_Ped(pPed)
	, m_OnlyDamageRecords(bOnlyDamageRecords)
{

}

void CDamageDebugInfo::Print()
{
	if (!ValidateInput())
		return;

	WriteLogEvent("DAMAGE");
		PushIndent();
			PushIndent();
				PushIndent();
					PrintDamageDebug();
}

bool CDamageDebugInfo::ValidateInput()
{
	if (!m_Ped)
		return false;

	return true;
}

void CDamageDebugInfo::PrintDamageDebug()
{
	if (!m_OnlyDamageRecords)
	{
		bool bOn = false;

		ColorPrintLn(Color_blue, "%s:%s:%p", CDebugScene::GetEntityDescription(m_Ped), m_Ped->GetModelName(), m_Ped.Get());
		ColorPrintLn(Color_red, "  Health(%.2f/%.2f) Armor(%.2f) Endurance(%.2f/%.2f)", m_Ped->GetHealth(), m_Ped->GetMaxHealth(), m_Ped->GetArmour(), m_Ped->GetEndurance(), m_Ped->GetMaxEndurance());
		ColorPrintLn(Color_red, "  Health Lost(%.2f) Armor Lost(%.2f)",m_Ped->GetHealthLost(), m_Ped->GetArmourLost());
		ColorPrintLn(Color_blue, "Proofs: ");
		bOn = m_Ped->m_nPhysicalFlags.bNotDamagedByBullets;
		ColorPrintLn(bOn?Color_green:Color_red, "  Bullet(%s) ", bOn ? "On" : "Off");
		bOn = m_Ped->m_nPhysicalFlags.bNotDamagedByFlames;
		ColorPrintLn(bOn?Color_green:Color_red, "  Flame(%s) ", bOn ? "On" : "Off");
		bOn = m_Ped->m_nPhysicalFlags.bIgnoresExplosions;
		ColorPrintLn(bOn?Color_green:Color_red, "  Explosion(%s) ", bOn ? "On" : "Off");
		bOn = m_Ped->m_nPhysicalFlags.bNotDamagedByCollisions;
		ColorPrintLn(bOn?Color_green:Color_red, "  Collision(%s) ", bOn ? "On" : "Off");
		bOn = m_Ped->m_nPhysicalFlags.bNotDamagedByMelee;
		ColorPrintLn(bOn?Color_green:Color_red, "  Melee(%s) ", bOn ? "On" : "Off");
		bOn = m_Ped->m_nPhysicalFlags.bNotDamagedBySteam;
		ColorPrintLn(bOn?Color_green:Color_red, "  Steam(%s) ", bOn ? "On" : "Off");
		bOn = m_Ped->m_nPhysicalFlags.bNotDamagedByAnythingButHasReactions;
		ColorPrintLn(bOn?Color_green:Color_red, "  bNotDamagedByAnythingButHasReactions(%s) ", bOn ? "On" : "Off");

		if (m_Ped->IsPlayer() && m_Ped->GetPlayerInfo())
		{
			ColorPrintLn(Color_blue, "Player specific:");

			bOn = m_Ped->IsDebugInvincible();
			bool bInvincibleHealthRestoreOn = m_Ped->IsLocalPlayer() ? CPlayerInfo::IsPlayerInvincibleRestoreHealth() : false;
			bool bRestoreArmorWithHealth = m_Ped->IsLocalPlayer() ? CPlayerInfo::IsPlayerInvincibleRestoreArmorWithHealth() : false;
			ColorPrintLn(bOn?Color_green:Color_red, "  Debug player invincible (V): (%s) Restore health %s (V+ALT): (%s) ", bOn ? "On" : "Off", bRestoreArmorWithHealth ? "and armor" : "", bInvincibleHealthRestoreOn ? "On" : "Off");

			bOn = !m_Ped->GetPlayerInfo()->GetPlayerDataCanBeDamaged();
			ColorPrintLn(bOn?Color_green:Color_red, "  Player control/Marketing(%s) ", bOn ? "On" : "Off");

			bOn = !m_Ped->GetPlayerInfo()->AffectedByWaterCannon();
			ColorPrintLn(bOn?Color_green:Color_red, "  Water cannon(%s) ", bOn ? "On" : "Off");
		}
				
		ColorPrintLn(Color_blue, "Misc:");

		bOn = m_Ped->GetPedConfigFlag(CPED_CONFIG_FLAG_HasBulletProofVest);
		ColorPrintLn(bOn?Color_green:Color_red, "  CPED_CONFIG_FLAG_HasBulletProofVest(%s) ", bOn ? "On" : "Off");

		bOn = m_Ped->GetPedConfigFlag(CPED_CONFIG_FLAG_CanBeShotInVehicle);
		ColorPrintLn(bOn?Color_green:Color_red, "  CPED_CONFIG_FLAG_CanBeShotInVehicle(%s) ", bOn ? "On" : "Off");

		bOn = m_Ped->GetPlayerResetFlag(CPlayerResetFlags::PRF_RUN_AND_GUN); 
		ColorPrintLn(bOn?Color_green:Color_red,  "  PRF_RUN_AND_GUN(%s) ", bOn ? "On" : "Off");

		bOn = m_Ped->GetPlayerResetFlag(CPlayerResetFlags::PRF_NO_RETICULE_AIM_ASSIST_ON);
		ColorPrintLn(bOn?Color_green:Color_red, "  PRF_NO_RETICULE_AIM_ASSIST_ON(%s) ", bOn ? "On" : "Off");

		bOn = m_Ped->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON);
		ColorPrintLn(bOn?Color_green:Color_red, "  PRF_ASSISTED_AIMING_ON(%s) ", bOn ? "On" : "Off");

		bOn = m_Ped->m_nPhysicalFlags.bNotDamagedByAnything;
		ColorPrintLn(bOn?Color_green:Color_red, "  Damage off (e.g. SET_ENTITY_CAN_BE_DAMAGED)(%s) ", bOn ? "On" : "Off");

		bOn = m_Ped->m_nPhysicalFlags.bOnlyDamagedByPlayer;
		ColorPrintLn(bOn?Color_green:Color_red, "  SET_ENTITY_ONLY_DAMAGED_BY_PLAYER(%s) ", bOn ? "On" : "Off");

		bOn = m_Ped->m_nPhysicalFlags.bOnlyDamagedByRelGroup;
		CRelationshipGroup* pRelGroup = CRelationshipManager::FindRelationshipGroup(m_Ped->m_specificRelGroupHash);
		ColorPrintLn(bOn?Color_green:Color_red, "  Only damaged by Rel Group(%s, %s) ", bOn ? "On" : "Off", pRelGroup?pRelGroup->GetName().GetCStr():"");

		bOn = m_Ped->m_nPhysicalFlags.bNotDamagedByRelGroup;
		pRelGroup = CRelationshipManager::FindRelationshipGroup(m_Ped->m_specificRelGroupHash);
		ColorPrintLn(bOn?Color_green:Color_red, "  Not damaged by Rel group(%s, %s) ", bOn ? "On" : "Off", pRelGroup?pRelGroup->GetName().GetCStr():"");

		bOn = m_Ped->m_nPhysicalFlags.bOnlyDamagedWhenRunningScript;
		ColorPrintLn(bOn?Color_green:Color_red, "  Only Damaged When Running Script: %s", bOn ? "On" : "Off");

		ColorPrintLn(Color_red, "  Damaged Body Parts: %s ", m_Ped->GetBodyPartDamageDbgStr().c_str());

		bOn = m_Ped->GetPedConfigFlag( CPED_CONFIG_FLAG_HelmetHasBeenShot );
		ColorPrintLn(bOn?Color_green:Color_red, "  Helmet shot: %s ", bOn ? "On" : "Off");

		const char* zKnockOffVehicleModeNames[4] =
		{
			"KNOCKOFFVEHICLE_DEFAULT",
			"KNOCKOFFVEHICLE_NEVER",
			"KNOCKOFFVEHICLE_EASY",
			"KNOCKOFFVEHICLE_HARD"
		};

		ColorPrintLn(bOn?Color_green:Color_red, "  Knock off vehicle mode: %s ", zKnockOffVehicleModeNames[m_Ped->m_PedConfigFlags.GetCantBeKnockedOffVehicle()]);

		bOn = m_Ped->GetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInWater );
		ColorPrintLn(bOn?Color_green:Color_red, "  Can Drown: %s ", bOn ? "On" : (m_Ped->m_bDrownDisabledByScript ? "Off by SCRIPT" : "Off"));

		bOn = false;
		u32 iBitSet = 0;
		if (NetworkInterface::IsGameInProgress() && m_Ped->GetNetworkObject())
		{
			CNetObjPed* pPedObj = SafeCast(CNetObjPed, m_Ped->GetNetworkObject());
			if (pPedObj && pPedObj->GetArePlayerDamagableFlagsSet())
			{
				bOn = true;
				iBitSet = pPedObj->GetPlayerDamagableFlags();
			}
		}
		ColorPrintLn(bOn?Color_green:Color_red, "  Can Be Damaged By Player bitset: %s , Bitset: %u", bOn ? "On" : "Off", iBitSet);
	}

	if (m_Ped->IsLocalPlayer() && m_Ped->GetPlayerInfo()->GetDamageOverTime().GetCount() > 0)
	{
		ColorPrintLn(Color_blue, "Damage Over Time Effects:");
		for (s32 i = 0; i < m_Ped->GetPlayerInfo()->GetDamageOverTime().GetCount(); i++)
		{
			atString debugString = m_Ped->GetPlayerInfo()->GetDamageOverTime().GetDebugString(i);
			ColorPrintLn(Color_red, debugString.c_str());
		}
	}

	ColorPrintLn(Color_blue,  "Damage Records:");

	for(s32 i = m_Ped->m_damageRecords.GetCount()-1; i >= 0; i--)
	{
		const CPed::sDamageRecord& dr = m_Ped->m_damageRecords[i];
		const float fTimeLapsed = ((float)(fwTimer::GetTimeInMilliseconds() - dr.uTimeInflicted))/-1000.0f;
		const char* entityName = dr.pInflictor ? CDebugScene::GetEntityDescription(dr.pInflictor) : "NULL";
		const char* modelName = dr.pInflictor && dr.pInflictor->IsArchetypeSet() ? dr.pInflictor->GetModelName() : "NULL";
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(dr.uWeaponHash);
		const char* weaponName = pWeaponInfo ? pWeaponInfo->GetName() : "NULL";
		const float fDamageTotal = dr.fArmourLost + dr.fHealthLost;

		const char* boneName = "NULL";
		eAnimBoneTag nBoneTag = BONETAG_INVALID;
		if(dr.iComponent >= 0 && dr.iComponent < HIGH_RAGDOLL_LOD_NUM_COMPONENTS)
		{
			nBoneTag = m_Ped->GetBoneTagFromRagdollComponent(dr.iComponent);
		}

		if(nBoneTag != BONETAG_INVALID)
		{
			int boneIdx = -1;
			if(m_Ped->GetSkeleton() && m_Ped->GetSkeletonData().ConvertBoneIdToIndex((u16)nBoneTag, boneIdx))
			{
				const crBoneData* bd = m_Ped->GetSkeletonData().GetBoneData(boneIdx);
				if(bd)
				{
					boneName = bd->GetName();
				}
			}
		}

		Color32 col = Color_red;
		if(dr.bInstantKill)
			col = Color_green;
		else if(dr.bInvincible)
			col = Color_orange;

		if (CPedDebugVisualiser::ms_bDetailedDamageDisplay)
		{
			ColorPrintLn(col, "  %.2f: 0x%p:%s:%s, %s, DST:%.2f, DH:%.2f, DA:%.2f, DE:%.2f, %s, H:%.2f", fTimeLapsed, dr.pInflictor.Get(), entityName, modelName, weaponName, dr.fDistance, dr.fHealthLost, dr.fArmourLost, dr.fEnduranceLost, CPedDamageCalculator::GetRejectionReasonString(dr.uRejectionReason), dr.fHealth);

			float fPlayerDamageMod = 1.0f;
			if(m_Ped->GetPlayerInfo())
			{
				if( pWeaponInfo && pWeaponInfo->GetIsMelee() )
					fPlayerDamageMod = m_Ped->GetPlayerInfo()->GetPlayerMeleeDamageModifier(pWeaponInfo->GetIsUnarmed());
				else 
					fPlayerDamageMod = m_Ped->GetPlayerInfo()->GetPlayerWeaponDamageModifier();
			}

			float fSpecialAbilityDamageMod = 1.0f;
			if(dr.pInflictor && dr.pInflictor->GetIsTypePed())
			{
				const CPed* pInflictorPed = static_cast<const CPed*>(dr.pInflictor.Get());
				if(pInflictorPed->GetSpecialAbility())
				{
					fSpecialAbilityDamageMod = pInflictorPed->GetSpecialAbility()->GetDamageMultiplier();
				}
			}

			ColorPrintLn(col, "    %s, K:%d, PM:%.2f, LM:%.2f, AM:%d:%.2f, SA:%.2f",  boneName, dr.bInstantKill, fPlayerDamageMod, NetworkInterface::IsGameInProgress() ? pWeaponInfo->GetNetworkHitLimbsDamageModifier() : pWeaponInfo->GetHitLimbsDamageModifier(), m_Ped->IsBoneLightlyArmoured(nBoneTag), pWeaponInfo->GetLightlyArmouredDamageModifier(), fSpecialAbilityDamageMod);
		}
		else if (dr.fEnduranceLost > 0)
		{
			ColorPrintLn(col, "  %.2f: %s, %s, D:%.2f, DE:%.2f, H:%.2f, %s, %s", fTimeLapsed, modelName, weaponName, fDamageTotal, dr.fEnduranceLost, dr.fHealth, CPedDamageCalculator::GetRejectionReasonString(dr.uRejectionReason), boneName);
		}
		else
		{

			ColorPrintLn(col, "  %.2f: %s, %s, D:%.2f, H:%.2f, %s, %s", fTimeLapsed, modelName, weaponName, fDamageTotal, dr.fHealth, CPedDamageCalculator::GetRejectionReasonString(dr.uRejectionReason), boneName);
		}
	}

	if (m_Ped->GetIsDeadOrDying())
	{
		ColorPrintLn(Color_blue, "Death source info:");

		CEntity* pDeathSource = m_Ped->GetSourceOfDeath();
		ColorPrintLn(Color_red, "Source Entity: %s(%p)", pDeathSource ? pDeathSource->GetModelName() : "none", pDeathSource);

		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_Ped->GetCauseOfDeath());
		ColorPrintLn(Color_red, "Cause of death: %s (%u)", pWeaponInfo ? pWeaponInfo->GetName() : "none",  m_Ped->GetCauseOfDeath());

		ColorPrintLn(Color_red, "Time of death: %u", m_Ped->GetTimeOfDeath());
	}
}

#endif // AI_DEBUG_OUTPUT_ENABLED