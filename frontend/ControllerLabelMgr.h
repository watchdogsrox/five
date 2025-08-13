#ifndef CONTROMERLABELMGR_H
#define CONTROMERLABELMGR_H

#include "Text/TextFile.h"
#include "atl/array.h"

namespace rage
{
	class parTreeNode;
}

struct sControlGroup
{
public:

	struct Label
	{
		Label() : m_LabelHash(0), m_ContextHash(0) {}
		Label(u32 labelHash, u32 contextHash) : m_LabelHash(labelHash), m_ContextHash(contextHash) {}
		bool operator==(const Label& other) const	{ return m_LabelHash == other.m_LabelHash && m_ContextHash == other.m_ContextHash; }
		u32 m_LabelHash;
		u32 m_ContextHash;
	};

	// Typedefs
	typedef atMap<int, atArray<Label> > LabelGroupMap;
	typedef atMap<u32, atArray<int> > ConfigsMap;

	LabelGroupMap m_LabelGroups;
	ConfigsMap m_Configs;
};

class CControllerLabelMgr
{
public:
	CControllerLabelMgr();
	~CControllerLabelMgr();

	// Typedefs
	typedef atMap<u32, sControlGroup> ControllerMap;

	static void Update();
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static void LoadDataXMLFile(const unsigned initMode);
	static void LoadDataXMLFile(const char* pFileName);

	static void SetLabelScheme(u32 uContext, u32 uControlGroup);

	static void AdjustLabels();

	static bool GetLabelsChanged() {return m_bLabelsDirty;}
	static void SetLabelsChanged(bool bDirty) {m_bLabelsDirty = bDirty;}
	static atArray<sControlGroup::Label>& GetCurrentLabels() { return m_CurrentControlLabels; }

private:
	static u32 m_uLabelSwapDelay;
	static u32 m_uTimeOfLastSwap;
	static bool m_bLabelsDirty;
	static ControllerMap m_AllControls;

	static u32 m_uCurrentContext;
	static u32 m_uCurrentConfig;
	static atArray<sControlGroup::Label> m_CurrentControlLabels;

	static bool ShouldSkipLabelDueToContext(u32 uContext);
	static int GetPlatformControlsFile();
};

#endif // CONTROMERLABELMGR_H
