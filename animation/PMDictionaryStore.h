//
// filename:	PMDictionaryStore.h
// description:	
//

#ifndef INC_PMDICTIONARYSTORE_H_
#define INC_PMDICTIONARYSTORE_H_


#define USE_PM_STORE (0)

#if USE_PM_STORE
#define FAKE_PMDICTIONARYSTORE (0)

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "crparameterizedmotion/parameterizedmotiondictionary.h"
// Game headers
#include "fwtl\assetstore.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// Rage forward declaration
namespace rage
{
	class crpmParameterizedMotionDictionary;
	class crpmParameterizedMotionData;
}

//
// name:		CPMDictionaryStore
// description:	
class CPmDictionaryStore : public fwAssetRscStore<crpmParameterizedMotionDictionary>
{
public:
	CPmDictionaryStore();

	void Set(strLocalIndex index, crpmParameterizedMotionDictionary* pObject);
	void Remove(strLocalIndex index);

	const crpmParameterizedMotionDictionary* GetPMDict(const strStreamingObjectName pPMDictName);
	const crpmParameterizedMotionData* GetPMData(const strStreamingObjectName pPmDictName, const char* pPMDataName);
	const crpmParameterizedMotionData* GetPMData(s32 dictIndex, u32 hashKey);
};

// --- Globals ------------------------------------------------------------------

extern CPmDictionaryStore g_PmDictionaryStore;
#endif // USE_PM_STORE

#endif // !INC_PMDICTIONARYSTORE_H_
