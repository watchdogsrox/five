// FILE :    HealthConfig.h
// PURPOSE : Load in configurable health information for varying types of peds
// CREATED : 10-06-2011

#ifndef HEALTH_CONFIG_H
#define HEALTH_CONFIG_H

// Rage headers
#include "atl/hashstring.h"
#include "atl/array.h"
#include "parser/macros.h"
#include "system/noncopyable.h"

// Framework headers

// Game headers

class CHealthConfigInfo
{
public:
	// Get the hash
	u32 GetHash() { return m_Name.GetHash(); }

#if !__FINAL
	// Get the name from the metadata manager
	const char* GetName() const { return m_Name.GetCStr(); }
#endif // !__FINAL

	const float GetDefaultHealth() const { return m_DefaultHealth; }
	const float GetDefaultArmour() const { return m_DefaultArmour; }
	const float GetDefaultEndurance() const { return m_DefaultEndurance; }

	const float GetFatiguedHealthThreshold() const { return m_FatiguedHealthThreshold; }
	const float GetInjuredHealthThreshold() const { return m_InjuredHealthThreshold; }
	const float GetDyingHealthThreshold() const { return m_DyingHealthThreshold; }
	const float GetHurtHealthThreshold() const { return m_HurtHealthThreshold; }
	const float GetDogTakedownThreshold() const { return m_DogTakedownThreshold; }
	const float GetWritheFromBulletDamageTheshold() const { return m_WritheFromBulletDamageTheshold; }
	const bool  ShouldCheckForFatalMeleeCardinalAttack() const { return m_MeleeCardinalFatalAttackCheck; }
	const bool  GetInvincible() const { return m_Invincible; }

private:
	atHashWithStringNotFinal	m_Name;

	float						m_DefaultHealth;
	float						m_DefaultArmour;
	float						m_DefaultEndurance;

	float						m_FatiguedHealthThreshold;
	float						m_InjuredHealthThreshold;
	float						m_DyingHealthThreshold;
	float						m_HurtHealthThreshold;
	float						m_DogTakedownThreshold;
	float						m_WritheFromBulletDamageTheshold;
	bool						m_MeleeCardinalFatalAttackCheck;
	bool						m_Invincible;

	

	PAR_SIMPLE_PARSABLE;
};

class CHealthConfigInfoManager
{
	NON_COPYABLE(CHealthConfigInfoManager);
public:
	CHealthConfigInfoManager();
	~CHealthConfigInfoManager()		{}

	// Initialise
	static void Init(const char* pFileName);

	// Shutdown
	static void Shutdown();

	// Access the Health Config information
	static const CHealthConfigInfo * const GetInfo(const u32 uNameHash)
	{
		if (uNameHash != 0)
		{
			for (s32 i = 0; i < m_HealthConfigManagerInstance.m_aHealthConfig.GetCount(); i++ )
			{
				if (m_HealthConfigManagerInstance.m_aHealthConfig[i].GetHash() == uNameHash)
				{
					return &m_HealthConfigManagerInstance.m_aHealthConfig[i];
				}
			}
		}
		return m_HealthConfigManagerInstance.m_DefaultSet;
	}

	// Conversion functions (currently only used by xml loader)
	static const CHealthConfigInfo *const GetInfoFromName(const char * name)	{ return GetInfo( atStringHash(name) ); };
#if __BANK
	static const char * GetInfoName(const CHealthConfigInfo *pInfo)				{ return pInfo->GetName(); }
#else
	static const char * GetInfoName(const CHealthConfigInfo *)					{ Assert(0); return ""; }
#endif // !__BANK

#if __BANK
	// Add widgets
	static void InitWidgets();
#endif // __BANK

private:

	// Delete the data
	static void Reset();

	// Load the data
	static void Load(const char* pFileName);

#if __BANK
	// Save the data
	static void Save();
#endif // __BANK

	atArray<CHealthConfigInfo>	m_aHealthConfig;
	CHealthConfigInfo *			m_DefaultSet;

	static CHealthConfigInfoManager m_HealthConfigManagerInstance;

	PAR_SIMPLE_PARSABLE;
};

#endif//!HEALTH_CONFIG_H