// FILE :    PedComponentSet.h
// PURPOSE : Load in component information for setting up different types of peds
// CREATED : 21-12-2010

#ifndef PED_COMPONENTSETINFO_H
#define PED_COMPONENTSETINFO_H

// Rage headers
#include "atl/hashstring.h"
#include "atl/array.h"
#include "parser/macros.h"
#include "system/noncopyable.h"

// Framework headers

// Game headers

class CPedComponentSetInfo
{
public:
	// Get the hash
	u32 GetHash() { return m_Name.GetHash(); }

#if !__FINAL
	// Get the name from the metadata manager
	const char* GetName() const { return m_Name.GetCStr(); }
#endif // !__FINAL

	bool	GetHasFacial() const						{ return m_HasFacial;	}
	bool	GetHasRagdollConstraints() const			{ return m_HasRagdollConstraints;	}
	bool	GetHasInventory() const						{ return m_HasInventory;	}
	bool	GetHasVfx() const							{ return m_HasVfx;	}
	bool	GetHasSpeech() const						{ return m_HasSpeech;	}
	bool	GetHasPhone() const							{ return m_HasPhone;	}
	bool	GetHasHelmet() const						{ return m_HasHelmet;	}
	bool	GetHasShockingEventResponse() const			{ return m_HasShockingEventReponse;	}
	bool	GetHasReins() const							{ return m_HasReins;	}
	bool	GetIsRidable() const						{ return m_IsRidable; }

private:

	atHashWithStringNotFinal m_Name;
	bool	m_HasFacial;
	bool	m_HasRagdollConstraints;
	bool	m_HasInventory;
	bool	m_HasVfx;
	bool	m_HasSpeech;
	bool	m_HasPhone;
	bool	m_HasHelmet;
	bool	m_HasShockingEventReponse;
	bool	m_HasReins;
	bool	m_IsRidable;	

	PAR_SIMPLE_PARSABLE;
};

class CPedComponentSetManager
{
	NON_COPYABLE(CPedComponentSetManager);
public:
	CPedComponentSetManager();
	~CPedComponentSetManager()		{}

	// Initialise
	static void Init(const char* pFileName);

	// Shutdown
	static void Shutdown();

	// Access the Component Set information
	static const CPedComponentSetInfo* const GetInfo(const u32 uNameHash);
	static const CPedComponentSetInfo* const FindInfo(const u32 uNameHash);

	// Conversion functions (currently only used by xml loader)
	static const CPedComponentSetInfo* const GetInfoFromName(const char * name)			{ return GetInfo( atStringHash(name) ); };
#if __BANK
	static const char * GetInfoName(const CPedComponentSetInfo * pInfo)			{ return pInfo->GetName(); }
#else
	static const char * GetInfoName(const CPedComponentSetInfo * )				{ Assert(0); return ""; }
#endif // !__BANK

#if __BANK
	// Add widgets
	static void AddWidgets(bkBank& bank);
#endif // __BANK

	static void AppendExtra(const char* pFileName);
	static void RemoveExtra(const char* pFileName);

private:

	// Delete the data
	static void Reset();

	// Load the data
	static void Load(const char* pFileName);

#if __BANK
	// Save the data
	static void Save();
#endif // __BANK

	static void RemoveEntry(CPedComponentSetInfo *pInfoToRemove);

	atArray<CPedComponentSetInfo*> m_Infos;
	CPedComponentSetInfo * m_DefaultSet;

	static CPedComponentSetManager m_ComponentSetManagerInstance;

	PAR_SIMPLE_PARSABLE;
};





// CPedComponentClothInfo

class CPedComponentClothInfo
{
public:
	u32 GetHash() { return m_Name.GetHash(); }

#if !__FINAL
	const char* GetName() const { return m_Name.GetCStr(); }
#endif // !__FINAL

	int	GetComponentID() const			{ return m_ComponentID;	}
	int	GetDrawableID() const			{ return m_DrawableID;	}
	int	GetComponentTargetID() const	{ return m_ComponentTargetID;	}
	int	GetClothSetID() const			{ return m_ClothSetID;	}	

private:
	atHashWithStringNotFinal m_Name;
	int 	m_ComponentID;
	int		m_DrawableID;
	int 	m_ComponentTargetID;
	int		m_ClothSetID;
	
	PAR_SIMPLE_PARSABLE;
};


// CPedComponentClothManager

class CPedComponentClothManager
{
	NON_COPYABLE(CPedComponentClothManager);
public:
	CPedComponentClothManager() {}
	~CPedComponentClothManager() {}

	static void Init(const char* pFileName);
	static void Shutdown();
	static const CPedComponentClothInfo* const GetInfo(const u32 uNameHash);
	static const CPedComponentClothInfo* const GetInfoFromName(const char * name)	{ return GetInfo( atStringHash(name) ); };
#if __BANK
	static const char * GetInfoName(const CPedComponentClothInfo * pInfo)			{ return pInfo->GetName(); }
#else
	static const char * GetInfoName(const CPedComponentClothInfo * )				{ Assert(0); return ""; }
#endif // !__BANK

#if __BANK
	static void AddWidgets(bkBank& bank);
#endif // __BANK

private:
	static void Reset();
	static void Load(const char* pFileName);

#if __BANK
	static void Save();
#endif // __BANK

	atArray<CPedComponentClothInfo> m_Infos;

	static CPedComponentClothManager ms_ComponentClothManagerInstance;

	PAR_SIMPLE_PARSABLE;
};


#endif // PED_COMPONENTSETINFO_H