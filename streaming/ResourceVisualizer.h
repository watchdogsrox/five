
#pragma once

#if !__FINAL

#include "atl/singleton.h"
#include "data/base.h"
#include "diag/tracker.h"

namespace rage
{
	class fiStream;
	class strIndex;
}

class CResourceVisualizer : public datBase
{
private:
	bool m_enabled;

private:
	void DumpStreamingReport(bool append, const char* filename);
	fiStream* OpenFile(bool append, const char* filename);
	
	void GetResourceInfo(const datResourceInfo& info, size_t (&UsedBySizeVirtual)[32], size_t (&UsedBySizePhysical)[32]);
	s32 OrderByMemorySize(strIndex i, strIndex j);

public:
	CResourceVisualizer() : m_enabled(true) { }

	void InitClass() { }
	void ShutdownClass() { }
	void Update() { }

	BANK_ONLY(void InitWidgets();)
	void Dump();
	void Toggle();
};

typedef atSingleton<CResourceVisualizer> CResourceVisualizerManager;

#endif
