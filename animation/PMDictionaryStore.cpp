//
// filename:	PMDictionaryStore.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "crparameterizedmotion/parameterizedmotiondictionary.h"
#include "file\asset.h"
#include "file\packfile.h"
#include "fwsys\fileExts.h"
#include "paging\rscbuilder.h"

// Game headers
#include "anim_channel.h"
#include "network\NetworkInterface.h"

#include "PMDictionaryStore.h"

#if USE_PM_STORE

ANIM_OPTIMISATIONS()

// --- Globals ------------------------------------------------------------------

CPmDictionaryStore g_PmDictionaryStore;

// --- Code ---------------------------------------------------------------------

// --- CPMDictionaryStore --------------------------------------------------------------------------------------

//
// name:		CPmDictionaryStore::CPmDictionaryStore
// description:	Constructor for Parameterized motion dictionary store
CPmDictionaryStore::CPmDictionaryStore() : fwAssetRscStore<crpmParameterizedMotionDictionary>("PmStore", 
																							 PMDICTIONARY_FILE_ID, 
																							 CONFIGURED_FROM_FILE, 
																							 128,
																							 false, 
																							 PMDICTIONARY_FILE_VERSION)
{
}

// Load parameterized motion dictionary from a file into this slot
/*bool CPmDictionaryStore::LoadFile(s32 index, const char* pFilename)
{
	if (NetworkInterface::IsGameInProgress())
	{
		animAssertf(0, "Error - Trying to load parameterized motion dictionaries during network game. Index = %d. Filename = %s", index, pFilename);
	}
	
	ObjectDef<crpmParameterizedMotionDictionary>* pDef = m_pool.GetSlot(index);

	animAssertf(pDef, "No parameterized motion dictionary at this slot");
	animAssertf(pDef->m_pObject==NULL, "parameterized motion dictionary is already in memory");

	if (CStreaming::GetCleanup().MakeSpaceForResourceFile(pFilename, PMDICTIONARY_FILE_EXT_PATTERN, PMDICTIONARY_FILE_VERSION))
	{
		pgRscBuilder::Load(pDef->m_pObject, pFilename, PMDICTIONARY_FILE_EXT_PATTERN, PMDICTIONARY_FILE_VERSION);
	}
	else
	{
		animAssert(0);
	}
	return (pDef->m_pObject != NULL);
}*/



// Accept a resourced parameterized motion dictionary from a memory into this slot
void CPmDictionaryStore::Set(strLocalIndex index, crpmParameterizedMotionDictionary* m_pObject)
{
	fwAssetDef<crpmParameterizedMotionDictionary>* pDef = m_pool.GetSlot(index);

	animAssertf(pDef, "No parameterized motion dictionary at this slot");
	animAssertf(pDef->m_pObject==NULL, "parameterized motion dictionary is already in memory");

	UPDATE_KNOWN_REF(pDef->m_pObject, m_pObject);

	if (pDef->m_pObject)
	{
		pDef->m_pObject->AddRef();
	}
}


// Remove a parameterized motion dictionary from this slot
void CPmDictionaryStore::Remove(strLocalIndex index)
{
	fwAssetDef<crpmParameterizedMotionDictionary>* pDef = m_pool.GetSlot(index);

	animAssertf(pDef, "No parameterized motion dictionary at this slot");
	animAssertf(pDef->m_pObject!=NULL, "parameterized motion dictionary is not in memory");

	SAFE_REMOVE_KNOWN_REF(pDef->m_pObject);
	pDef->m_pObject->Release();
	pDef->m_pObject = NULL;
}

// Return the parameterized motion dictionary at this named slot
const crpmParameterizedMotionDictionary* CPmDictionaryStore::GetPMDict(const strStreamingObjectName pPMDictName)
{
	const crpmParameterizedMotionDictionary* pPMDict = NULL;
	s32 dictIndex = FindSlot(pPMDictName);
	animAssert(dictIndex != -1);
	if(dictIndex!=-1)
	{
		animAssertf(Get(dictIndex), "%s : PMDictionary is not loaded", GetName(dictIndex));
		pPMDict = Get(dictIndex);
	}
	return pPMDict;
}

// Return the parameterized motion data
const crpmParameterizedMotionData* CPmDictionaryStore::GetPMData(const strStreamingObjectName pPmDictName, const char* pPMDataName)
{
	const crpmParameterizedMotionData* pPMData = NULL;
	s32 dictIndex = FindSlot(pPmDictName);
	animAssert(dictIndex != -1);
	if(dictIndex!=-1)
	{
		const crpmParameterizedMotionDictionary* pPMDict = Get(dictIndex);
		animAssertf(Get(dictIndex), "%s : PMDictionary is not loaded", GetName(dictIndex));
		if(pPMDict)
		{
			pPMData = pPMDict->GetParameterizedMotion(pPMDataName);
		}
	}
	return pPMData;
}

// Return the parameterized motion data
const crpmParameterizedMotionData* CPmDictionaryStore::GetPMData(s32 dictIndex, u32 hashKey)
{
	const crpmParameterizedMotionData* pPMData = NULL;

	animAssert(dictIndex != -1);
	if(dictIndex!=-1)
	{
		animAssertf(Get(dictIndex), "%s : PMDictionary is not loaded", GetName(dictIndex));
		const crpmParameterizedMotionDictionary* pPMDict = Get(dictIndex);
		if(pPMDict)
		{
			pPMData = pPMDict->GetParameterizedMotion(hashKey);
		}
	}
	return pPMData;
}

#endif // USE_PM_STORE

