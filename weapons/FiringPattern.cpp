//
// weapons/firingpattern.cpp
//
// Copyright (C) 1999-2010 Rockstar North.  All Rights Reserved. 
//

// File header
#include "Weapons/FiringPattern.h"

// Rage headers
#include "bank/bank.h"
#include "fwsys/fileExts.h"
#include "parser/manager.h"

// Parser headers
#include "FiringPattern_parser.h"

// Game headers
#include "Peds/Ped.h"
#include "Scene/DataFileMgr.h"
#include "Weapons/WeaponDebug.h"

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()

#if __BANK
PARAM(rawFiringPatternMetadata, "Load the raw XML firing pattern metadata from the assets folder");
const char* g_FiringPatternMetadataAssetsSubPath = "export/data/metadata/ai/firingpatterns.pso.meta";
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////
// CFiringPatternInfo
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////

CFiringPatternInfo::CFiringPatternInfo()
{
}

////////////////////////////////////////////////////////////////////////////////
// CFiringPatternInfoManager
////////////////////////////////////////////////////////////////////////////////

// Static manager object
CFiringPatternInfoManager CFiringPatternInfoManager::ms_Instance;

////////////////////////////////////////////////////////////////////////////////

void CFiringPatternInfoManager::Init()
{
	// Load
	Load();
}

////////////////////////////////////////////////////////////////////////////////

void CFiringPatternInfoManager::Shutdown()
{
	// Delete
	Reset();
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CFiringPatternInfoManager::AddWidgets(bkBank& bank)
{
	bank.PushGroup("Firing Patterns");

	bank.AddButton("Load", Load);
	bank.AddButton("Save", Save);

	bank.PushGroup("Infos");
	for(s32 i = 0; i < ms_Instance.m_Infos.GetCount(); i++)
	{
		bank.PushGroup(ms_Instance.m_Infos[i]->GetName());
		PARSER.AddWidgets(bank, ms_Instance.m_Infos[i]);
		bank.PopGroup(); // ms_Instance.m_Infos[i]->GetName()
	}
	bank.PopGroup(); // "Infos"
	bank.PopGroup(); // "Firing Patterns"
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

void CFiringPatternInfoManager::Reset()
{
	// Delete the infos
	for(s32 i = 0; i < ms_Instance.m_Infos.GetCount(); i++)
	{
		delete ms_Instance.m_Infos[i];
	}
	ms_Instance.m_Infos.Reset();
	ms_Instance.m_InfoPtrs.Reset();
}

////////////////////////////////////////////////////////////////////////////////

void CFiringPatternInfoManager::Load()
{
	// Delete any existing data
	Reset();

	char metadataFilename[RAGE_MAX_PATH];

#if __BANK
	if(PARAM_rawFiringPatternMetadata.Get())
	{
		formatf(metadataFilename, RAGE_MAX_PATH, "%s%s", CFileMgr::GetAssetsFolder(), g_FiringPatternMetadataAssetsSubPath);

		ASSERT_ONLY(bool hasSucceeded = )PARSER.InitAndLoadObject(metadataFilename, "", ms_Instance);
		weaponFatalAssertf(hasSucceeded, "Failed to parse the metadata XML file (%s)", metadataFilename);
	}
	else
#endif // __BANK
	{
		const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::FIRINGPATTERNS_FILE);
		weaponFatalAssertf(pData && DATAFILEMGR.IsValid(pData), "Failed to find the firing pattern metadata PSO file");

		formatf(metadataFilename, RAGE_MAX_PATH, "%s.%s", pData->m_filename, META_FILE_EXT);

		psoFile* pPsoFile = psoLoadFile(metadataFilename, PSOLOAD_PREP_FOR_PARSER_LOADING, atFixedBitSet32().Set(psoFile::REQUIRE_CHECKSUM));
		weaponFatalAssertf(pPsoFile, "Failed to load the metadata PSO file (%s)", metadataFilename);

		ASSERT_ONLY(const bool bHasSucceeded = ) psoInitAndLoadObject(*pPsoFile, ms_Instance);
		weaponFatalAssertf(bHasSucceeded, "Failed to parse the metadata PSO file (%s)", metadataFilename);

		delete pPsoFile;
	}

	ms_Instance.m_InfoPtrs.Reserve(ms_Instance.m_Infos.GetCount());

	// Copy the array and sort it for searching
	for(s32 i = 0; i < ms_Instance.m_Infos.GetCount(); i++)
	{
		ms_Instance.m_InfoPtrs.PushAndGrow(ms_Instance.m_Infos[i]);
	}
	ms_Instance.m_InfoPtrs.QSort(0, ms_Instance.m_InfoPtrs.GetCount(), CFiringPatternInfo::PairSort);

	// Init the widgets
	BANK_ONLY(CWeaponDebug::InitBank());
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CFiringPatternInfoManager::Save()
{
	char filename[RAGE_MAX_PATH];
	formatf(filename, RAGE_MAX_PATH, "%s%s", CFileMgr::GetAssetsFolder(), g_FiringPatternMetadataAssetsSubPath);
	weaponVerifyf(PARSER.SaveObject(filename, "meta", &ms_Instance, parManager::XML), "Failed to save firing patterns");
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////
// CFiringPattern
////////////////////////////////////////////////////////////////////////////////

const s32 CFiringPattern::INFINITE_NUMBER_OF_BURSTS          = -1;
const s32 CFiringPattern::INFINITE_NUMBER_OF_SHOTS_PER_BURST = -1;
const s32 CFiringPattern::DEFAULT_NUMBER_OF_BURSTS           = -1;
const s32 CFiringPattern::DEFAULT_NUMBER_OF_SHOTS_PER_BURST  = -1;
const f32 CFiringPattern::DEFAULT_TIME_TO_DELAY_FIRING       = -1.0f;
const f32 CFiringPattern::DEFAULT_TIME_TILL_NEXT_SHOT        = -1.0f;
const f32 CFiringPattern::DEFAULT_TIME_TILL_NEXT_SHOT_MOD    =  1.0f;
const f32 CFiringPattern::DEFAULT_TIME_TILL_NEXT_BURST       = -1.0f;
const f32 CFiringPattern::DEFAULT_TIME_TILL_NEXT_BURST_MOD   =  1.0f;

////////////////////////////////////////////////////////////////////////////////

CFiringPattern::CFiringPattern()
: m_uFiringPatternHash(0)
, m_EquippedWeaponHash(u32(0))
, m_pFiringPatternInfo(NULL)
, m_iNumberOfBursts(DEFAULT_NUMBER_OF_BURSTS)
, m_iNumberOfShotsPerBurst(DEFAULT_NUMBER_OF_SHOTS_PER_BURST)
, m_fTimeToDelayFiring(DEFAULT_TIME_TO_DELAY_FIRING)
, m_fTimeTillNextShot(DEFAULT_TIME_TILL_NEXT_SHOT)
, m_fTimeTillNextShotModifier(DEFAULT_TIME_TILL_NEXT_SHOT_MOD)
, m_fTimeTillNextBurst(DEFAULT_TIME_TILL_NEXT_BURST)
, m_fTimeTillNextBurstModifier(DEFAULT_TIME_TILL_NEXT_BURST_MOD)
, m_fBurstCooldownOnUnsetTimer(0.0f)
, m_uBurstCooldownOnUnsetPatternHash(0)
, m_bForceReset(false)
, m_bBurstFinishedThisFrame(false)
{
}

////////////////////////////////////////////////////////////////////////////////

CFiringPattern::CFiringPattern(const CFiringPattern& other)
: m_uFiringPatternHash(other.m_uFiringPatternHash)
, m_EquippedWeaponHash(other.m_EquippedWeaponHash)
, m_pFiringPatternInfo(other.m_pFiringPatternInfo)
, m_iNumberOfBursts(other.m_iNumberOfBursts)
, m_iNumberOfShotsPerBurst(other.m_iNumberOfShotsPerBurst)
, m_fTimeToDelayFiring(other.m_fTimeToDelayFiring)
, m_fTimeTillNextShot(other.m_fTimeTillNextShot)
, m_fTimeTillNextShotModifier(other.m_fTimeTillNextShotModifier)
, m_fTimeTillNextBurst(other.m_fTimeTillNextBurst)
, m_fTimeTillNextBurstModifier(other.m_fTimeTillNextBurstModifier)
, m_fBurstCooldownOnUnsetTimer(other.m_fBurstCooldownOnUnsetTimer)
, m_uBurstCooldownOnUnsetPatternHash(other.m_uBurstCooldownOnUnsetPatternHash)
, m_bForceReset(other.m_bForceReset)
, m_bBurstFinishedThisFrame(other.m_bBurstFinishedThisFrame)
{
}

////////////////////////////////////////////////////////////////////////////////

void CFiringPattern::SetFiringPattern(u32 uFiringPatternHash, bool bForceReset)
{
	m_bForceReset = bForceReset || (m_uFiringPatternHash != uFiringPatternHash);
	m_uFiringPatternHash = uFiringPatternHash;
	m_EquippedWeaponHash.Clear();
}

////////////////////////////////////////////////////////////////////////////////

void CFiringPattern::PreProcess(const CPed& ped)
{
	weaponFatalAssertf(ShouldProcess(ped), "Shouldn't be processing");

	// Query if the weapon has an alias
	const CWeaponInfo* pEquippedWeaponInfo = ped.GetWeaponManager()->GetEquippedWeaponInfo();
	if(m_bForceReset || m_EquippedWeaponHash.GetHash() != pEquippedWeaponInfo->GetHash())
	{
		// Store the weapon
		m_EquippedWeaponHash = pEquippedWeaponInfo->GetHash();

		// Get the firing pattern we are supposed to be using
		const CFiringPatternInfo* pFiringPatternInfo = CFiringPatternInfoManager::GetInfo(pEquippedWeaponInfo->GetFiringPatternAlias(m_uFiringPatternHash));

		// Do we need to reset the data?
		if(m_bForceReset || m_pFiringPatternInfo != pFiringPatternInfo)
		{
			// When unsetting a flagged pattern, kick off a separate timer that prevents this specific hash from firing until over, no matter how many times we switch pattern
			if(m_pFiringPatternInfo && m_pFiringPatternInfo->GetBurstCooldownOnUnset() && m_uBurstCooldownOnUnsetPatternHash == 0 && m_pFiringPatternInfo != pFiringPatternInfo)
			{
				if(m_fTimeTillNextBurst > 0.0f)
				{
					// If we're already on a burst cooldown, copy the current timer
					m_fBurstCooldownOnUnsetTimer = m_fTimeTillNextBurst;
				}
				else if (m_pFiringPatternInfo->GetNumberOfShotsPerBurst() != INFINITE_NUMBER_OF_SHOTS_PER_BURST)
				{
					// Set the cooldown based on the number of rockets that have been fired
					s32 iShotsFired = m_pFiringPatternInfo->GetNumberOfShotsPerBurst() - m_iNumberOfShotsPerBurst;
					m_fBurstCooldownOnUnsetTimer = m_pFiringPatternInfo->GetTimeBetweenBursts(m_fTimeTillNextBurstModifier) * ((float)iShotsFired/(float)m_pFiringPatternInfo->GetNumberOfShotsPerBurst());
				}

				m_uBurstCooldownOnUnsetPatternHash = m_pFiringPatternInfo->GetHash();
			}

			// Assign the new firing pattern
			m_pFiringPatternInfo = pFiringPatternInfo;
			if(m_pFiringPatternInfo)
			{
				m_iNumberOfBursts = m_pFiringPatternInfo->GetNumberOfBursts();
				m_iNumberOfShotsPerBurst = m_pFiringPatternInfo->GetNumberOfShotsPerBurst();
				m_fTimeToDelayFiring = m_pFiringPatternInfo->GetTimeBeforeFiring();
				m_fTimeTillNextShot = 0.0f;
				m_fTimeTillNextBurst = 0.0f;
			}
			else
			{
				Reset();
			}

			// Clear flag
			m_bForceReset = false;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CFiringPattern::ProcessFired()
{
	// Only decrement if not set to infinite shots per burst
	if(m_iNumberOfShotsPerBurst != INFINITE_NUMBER_OF_SHOTS_PER_BURST)
	{
		m_iNumberOfShotsPerBurst--;
		m_iNumberOfShotsPerBurst = MAX(m_iNumberOfShotsPerBurst, 0);
	}

	if(m_iNumberOfShotsPerBurst != 0)
	{
		// Re-init the time between shots
		if(m_pFiringPatternInfo)
		{
			m_fTimeTillNextShot = m_pFiringPatternInfo->GetTimeBetweenShots(m_fTimeTillNextShotModifier);
		}
	}

	if(IsBurstFinished())
	{
		ProcessBurstFinished();
	}
}

////////////////////////////////////////////////////////////////////////////////

void CFiringPattern::ProcessBurstFinished()
{
	// Mark the firing pattern as having finished the burst
	m_bBurstFinishedThisFrame = true;

	// Only decrement if not set to infinite bursts
	if(m_iNumberOfBursts != INFINITE_NUMBER_OF_BURSTS)
	{
		m_iNumberOfBursts--;
		m_iNumberOfBursts = MAX(m_iNumberOfBursts, 0);
	}

	if(m_iNumberOfBursts != 0)
	{
		// Re-init the number of shots per burst
		if(m_iNumberOfShotsPerBurst != INFINITE_NUMBER_OF_SHOTS_PER_BURST)
		{
			if(m_pFiringPatternInfo)
			{
 				m_iNumberOfShotsPerBurst = m_pFiringPatternInfo->GetNumberOfShotsPerBurst();
			}
		}

		// Re-init the time between bursts
		if(m_pFiringPatternInfo)
		{
			m_fTimeTillNextBurst = m_pFiringPatternInfo->GetTimeBetweenBursts(m_fTimeTillNextBurstModifier);
		}

		// Clear any timer till next shot, as when the burst timer finishes we want to fire straight away
		m_fTimeTillNextShot = 0.0f;
	}
}

////////////////////////////////////////////////////////////////////////////////
