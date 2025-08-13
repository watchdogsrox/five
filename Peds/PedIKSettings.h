// FILE :    PedIKSettings.h
// PURPOSE : Load in configurable health information for varying types of peds
// CREATED : 10-06-2011

#ifndef PED_IK_SETTINGS_H
#define PED_IK_SETTINGS_H

// Rage headers
#include "atl/hashstring.h"
#include "atl/array.h"
#include "parser/macros.h"
#include "ik/IkManager.h"

#include "system/noncopyable.h"

// Framework headers

// Game headers

class CIKDeltaLimits
{
public:

	atFixedArray<atFixedArray<Vec3V, 2>, 5> m_DeltaLimits;

	PAR_SIMPLE_PARSABLE;
};

class CPedIKSettingsInfo
{
public:
	// Get the hash
	u32 GetHash() { return m_Name.GetHash(); }

#if !__FINAL
	// Get the name from the metadata manager
	const char* GetName() const { return m_Name.GetCStr(); }
#endif // !__FINAL

	const bool IsIKFlagDisabled( CBaseIkManager::eIkSolverType type ) const;

	const CIKDeltaLimits& GetTorsoDeltaLimits() const { return m_TorsoDeltaLimits; }
	const CIKDeltaLimits& GetHeadDeltaLimits() const { return m_HeadDeltaLimits; }
	const CIKDeltaLimits& GetNeckDeltaLimits() const { return m_NeckDeltaLimits; }

private:
	atHashWithStringNotFinal			m_Name;
	IKManagerSolverTypes::FlagsBitSet	m_IKSolversDisabled;

	CIKDeltaLimits						m_TorsoDeltaLimits;
	CIKDeltaLimits						m_HeadDeltaLimits;
	CIKDeltaLimits						m_NeckDeltaLimits;

	PAR_SIMPLE_PARSABLE;
};

class CPedIKSettingsInfoManager
{
	NON_COPYABLE(CPedIKSettingsInfoManager);
public:
	CPedIKSettingsInfoManager();
	~CPedIKSettingsInfoManager()		{}

	// Initialise
	static void Init(const char* pFileName);

	// Shutdown
	static void Shutdown();

	// Access the ped IK information
	static const CPedIKSettingsInfo * const GetInfo(const u32 uNameHash)
	{
		if (uNameHash != 0)
		{
			for (s32 i = 0; i < m_PedIKSettingsManagerInstance.m_aPedIKSettings.GetCount(); i++ )
			{
				if (m_PedIKSettingsManagerInstance.m_aPedIKSettings[i].GetHash() == uNameHash)
				{
					return &m_PedIKSettingsManagerInstance.m_aPedIKSettings[i];
				}
			}
		}
		return m_PedIKSettingsManagerInstance.m_DefaultSet;
	}

	// Conversion functions (currently only used by xml loader)
	static const CPedIKSettingsInfo *const GetInfoFromName(const char * name)	{ return GetInfo( atStringHash(name) ); };
#if __BANK
	static const char * GetInfoName(const CPedIKSettingsInfo *pInfo)				{ return pInfo->GetName(); }
#else
	static const char * GetInfoName(const CPedIKSettingsInfo *)					{ Assert(0); return ""; }
#endif // !__BANK

#if __BANK
	// Add widgets
	static void AddWidgets(bkBank& bank);
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

	atArray<CPedIKSettingsInfo>		m_aPedIKSettings;
	CPedIKSettingsInfo *			m_DefaultSet;

	static CPedIKSettingsInfoManager m_PedIKSettingsManagerInstance;

	PAR_SIMPLE_PARSABLE;
};

#endif//!PED_IK_SETTINGS_H