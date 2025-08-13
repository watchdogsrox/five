//
// filename:	streamedscripts.h
// description:	
//

#ifndef INC_STREAMEDSCRIPTS_H_
#define INC_STREAMEDSCRIPTS_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "script\program.h"
// Game headers
#include "script\script_channel.h"
#include "fwtl\assetstore.h"
#include "script\script_memory_tracking.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

class CStreamedScripts : public fwAssetRscStore<scrProgram, fwAssetNameDef<scrProgram> >
{
public:
	CStreamedScripts();

	virtual strLocalIndex Register(const char* name);
	virtual u32 GetStreamerReadFlags() const;
	virtual void RemoveRef(strLocalIndex index, strRefKind kind);
	virtual int GetNumRefs(strLocalIndex index) const;
	void ShutdownLevel();

	u32 CalculateScriptStoreFingerprint(u32 startHash, bool bDump);

	void	StoreBGScriptPackStreamIndex(strIndex val);

#if __DEV
	void ValidateAllScripts();
#endif
	scrProgramId GetProgramId(s32 index);


	// this is required in final builds too, so GetName() can't be used instead
	const char*	 GetScriptName(s32 index);

#if __SCRIPT_MEM_CALC
	void CalculateMemoryUsage(u32& nVirtualSize, u32& nPhysicalSize, const bool debug);
#endif

private:
#if __DEV
	u8 GetIndexOfScriptImgContainingScoFile(int nScriptId);
	void ValidateAllScriptsInASingleImg(const strStreamingObjectNameString pNameOfStartupScript);
#endif

	strIndex	m_BGScriptPackStreamIndex;
};

// have to specialise the template version of Remove as remove calls the destructor and the destructor
// for scrProgram is private
template<> void fwAssetRscStore<scrProgram, fwAssetNameDef<scrProgram> >::Remove(strLocalIndex index);

// --- Globals ------------------------------------------------------------------

extern CStreamedScripts g_StreamedScripts;

#endif // !INC_STREAMEDSCRIPTS_H_
