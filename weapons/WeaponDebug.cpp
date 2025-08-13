// File header
#include "Weapons/WeaponDebug.h"

// Rage includes
#include "Bank/BkMgr.h"

// Game headers
#include "Debug/DebugScene.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Scene/World/GameWorld.h"
#include "Shaders/CustomShaderEffectWeapon.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Weapons/Bullet.h"
#include "Weapons/Components/WeaponComponentManager.h"
#include "Weapons/Info/WeaponAnimationsManager.h"
#include "Weapons/Inventory/PedInventoryLoadOut.h"
#include "Weapons/Projectiles/ProjectileManager.h"
#include "Weapons/ThrownWeaponInfo.h"
#include "Weapons/Weapon.h"

#if __BANK // Only included in __BANK builds

PARAM(rag_weapon,"rag_weapon");
PARAM(debugrenderweapon,"debug render weapon");
PARAM(debugrenderbullets,"debug render bullets");

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CWeaponDebug
//////////////////////////////////////////////////////////////////////////

// Statics
#if DEBUG_DRAW
CDebugDrawStore CWeaponDebug::ms_debugStore(100);
#endif // DEBUG_DRAW

bkBank* CWeaponDebug::ms_pBank                                       = NULL;
bool    CWeaponDebug::ms_bRenderDebug                                = false;
bool    CWeaponDebug::ms_bRenderDebugBullets                         = false;
bool    CWeaponDebug::ms_bUseInfiniteAmmo                            = false;
bool    CWeaponDebug::ms_bUseRagdollForStungunAndRubbergun           = true;
char    CWeaponDebug::ms_focusPedAddress[MAX_STRING_LENGTH]          = { "None" };
char    CWeaponDebug::ms_focusPedPoolIndex[MAX_STRING_LENGTH]        = { "None" };
char    CWeaponDebug::ms_focusPedModelName[MAX_STRING_LENGTH]        = { "None" };
char    CWeaponDebug::ms_hashString[MAX_STRING_LENGTH]               = "";
char    CWeaponDebug::ms_hashStringResultUnsigned[MAX_STRING_LENGTH] = "";
char    CWeaponDebug::ms_hashStringResultSigned[MAX_STRING_LENGTH]   = "";

void CWeaponDebug::InitBank()
{
	if(ms_pBank)
	{
		ShutdownBank();
	}

	if (PARAM_debugrenderweapon.Get())
	{
		ms_bRenderDebug = true;
	}

	if (PARAM_debugrenderbullets.Get())
	{
		ms_bRenderDebugBullets = true;
	}

	// Create the weapons bank
	ms_pBank = &BANKMGR.CreateBank("Weapons", 0, 0, false); 
	if(weaponVerifyf(ms_pBank, "Failed to create Weapons bank"))
	{
		if (PARAM_rag_weapon.Get())
		{
			CWeaponDebug::AddWidgets(*ms_pBank);
		}
		else
		{
			ms_pBank->AddButton("Create weapon widgets", datCallback(CFA1(CWeaponDebug::AddWidgets), ms_pBank));
		}
	}
}

void CWeaponDebug::AddWidgets(bkBank& bank)
{
	// destroy first widget which is the create button
	bkWidget* pWidget = BANKMGR.FindWidget("Weapons/Create weapon widgets");
	if(pWidget)
	{
		pWidget->Destroy();
	}

	// General widgets
	InitWidgets(bank);

	// Inventory widgets
	CPedInventory::AddWidgets(bank);

	// Weapon info widgets
	CWeaponInfoManager::AddWidgetButton(bank);

	// Weapon component widgets
	CWeaponComponentManager::AddWidgetButton(bank);

	// Load outs
	CPedInventoryLoadOutManager::AddWidgets(bank);

	// Firing pattern widgets
	CFiringPatternInfoManager::AddWidgets(bank);

	// Thrown weapon widgets
	CThrownWeaponInfo::InitWidgets();

    // Accuracy modifier widgets
    sPedAccuracyModifiers::InitWidgets();

	// Weapon animations
	CWeaponAnimationsManager::AddWidgetButton(bank);

#if CSE_WEAPON_EDITABLEVALUES
	CCustomShaderEffectWeapon::InitWidgets(bank);
#endif

#if __BANK
	CWeapon::AddWidgets(bank);
#endif	//__BANK
}

void CWeaponDebug::ShutdownBank()
{
	if(ms_pBank)
	{
		BANKMGR.DestroyBank(*ms_pBank);
	}

	ms_pBank = NULL;
}

void CWeaponDebug::RenderDebug()
{
#if DEBUG_DRAW
	ms_debugStore.Render();
#endif // DEBUG_DRAW

	// Update the focus ped display
	UpdateFocusPedText();

	if(ms_bRenderDebug)
	{
		// Highlight the focus ped
		HighlightFocusPed();

		// Projectile rendering
		CProjectileManager::RenderDebug();

#if __BANK
		CPed* pFocusPed = GetFocusPed();
		if(pFocusPed)
		{
			const CWeapon* pWeapon = pFocusPed->GetWeaponManager() ? pFocusPed->GetWeaponManager()->GetEquippedWeapon() : NULL;
			if(pWeapon)
			{
				pWeapon->RenderDebug(pFocusPed);
			}
		}
#endif // __BANK
	}

	// Inventory rendering
	CPedInventory::RenderDebug();

	if(ms_bRenderDebugBullets)
	{
		CBulletManager::RenderDebug();
	}
}

CPed* CWeaponDebug::GetFocusPed()
{
	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
	if(pFocusEntity && pFocusEntity->GetIsTypePed())
	{
		return static_cast<CPed*>(pFocusEntity);
	}
	else
	{
		return CGameWorld::FindLocalPlayer();
	}
}

void CWeaponDebug::InitWidgets(bkBank& bank)
{
	// Focus ped display
	bank.AddText("Focus Ped Address", ms_focusPedAddress,   MAX_STRING_LENGTH, true);
	bank.AddText("Focus Ped Index", ms_focusPedPoolIndex, MAX_STRING_LENGTH, true);
	bank.AddText("Focus Ped Model", ms_focusPedModelName, MAX_STRING_LENGTH, true);

	// Debug rendering
	bank.AddSeparator();
	bank.AddToggle("Render", &ms_bRenderDebug);
	bank.AddToggle("Render Bullets", &ms_bRenderDebugBullets);
	bank.AddToggle("Use Infinite Ammo", &ms_bUseInfiniteAmmo, UpdateInfiniteAmmo);
	bank.AddToggle("Use Ragdoll for Stungun and Rubbergun", &ms_bUseRagdollForStungunAndRubbergun);
	bank.AddToggle("Batch weapon probes", &CBulletManager::ms_bUseBatchTestsForSingleShotWeapons);
	bank.AddToggle("Use undirected batch tests", &CBulletManager::ms_bUseUndirectedBatchTests);
	bank.AddSeparator();

	// Helpers
	bank.PushGroup("Helpers", false);
	bank.PushGroup("Hasher");
	bank.AddText("String", ms_hashString, MAX_STRING_LENGTH, false, UpdateHasherStrings);
	bank.AddText("Unsigned Hash", ms_hashStringResultUnsigned, MAX_STRING_LENGTH, true);
	bank.AddText("Signed Hash", ms_hashStringResultSigned, MAX_STRING_LENGTH, true);
	bank.PopGroup();
	bank.AddButton("Print Pool Usage", PrintPoolUsage);
	bank.AddButton("Remove All Projectiles",ClearAllProjectiles);
	bank.PopGroup();
}

void CWeaponDebug::UpdateInfiniteAmmo()
{
	CPed* pFocusPed = GetFocusPed();
	if(pFocusPed)
	{
		if(pFocusPed->GetInventory())
		{
			pFocusPed->GetInventory()->GetAmmoRepository().SetUsingInfiniteClips(ms_bUseInfiniteAmmo);
		}
	}
}

void CWeaponDebug::UpdateFocusPedText()
{
	const CPed* pFocusPed = GetFocusPed();
	if(pFocusPed)
	{
		formatf(ms_focusPedAddress,   MAX_STRING_LENGTH, "0x%X", pFocusPed);
		formatf(ms_focusPedPoolIndex, MAX_STRING_LENGTH, "%d",   CPed::GetPool()->GetIndex(pFocusPed));

		const CBaseModelInfo* pModelInfo = pFocusPed->GetBaseModelInfo();
		if(pModelInfo)
		{
			formatf(ms_focusPedModelName, MAX_STRING_LENGTH, "%s", pModelInfo->GetModelName());
		}
		else
		{
			formatf(ms_focusPedModelName, MAX_STRING_LENGTH, "%s", "None");
		}
	}
	else
	{
		formatf(ms_focusPedAddress,   MAX_STRING_LENGTH, "%s", "None");
		formatf(ms_focusPedPoolIndex, MAX_STRING_LENGTH, "%s", "None");
		formatf(ms_focusPedModelName, MAX_STRING_LENGTH, "%s", "None");
	}
}

void CWeaponDebug::HighlightFocusPed()
{
	const CPed* pFocusPed = GetFocusPed();
	if(pFocusPed)
	{
		// Transform the bounds into world space
		Vector3 vMin(pFocusPed->GetBoundingBoxMin());
		Vector3 vMax(pFocusPed->GetBoundingBoxMax());
		vMin = pFocusPed->TransformIntoWorldSpace(vMin);
		vMax = pFocusPed->TransformIntoWorldSpace(vMax);
	}
}

void CWeaponDebug::UpdateHasherStrings()
{
	u32 uResult = atStringHash(ms_hashString);
	s32  iResult = static_cast<s32>(uResult);
	formatf(ms_hashStringResultUnsigned, MAX_STRING_LENGTH, "%u", uResult);
	formatf(ms_hashStringResultSigned,   MAX_STRING_LENGTH, "%d", iResult);
}

void CWeaponDebug::PrintPoolUsage()
{
	CWeapon::GetPool()->PoolFullCallback();
}

void CWeaponDebug::ClearAllProjectiles()
{
	CProjectileManager::RemoveAllProjectiles();
}

#endif // __BANK
