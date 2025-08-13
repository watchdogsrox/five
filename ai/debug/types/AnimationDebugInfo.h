#pragma once

// game headers
#include "ai/debug/system/AIDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

#include "crclip/clip.h"
#include "crmotiontree/iterator.h"
#include "crmotiontree/node.h"
#include "atl/string.h"
#include "scene/RegdRefTypes.h"

class CAnimationDebugInfo;

class CAnimClipDebugIterator : public crmtIterator
{	
public:

	static atString GetClipDebugInfo(const crClip* pClip, float Time, float weight, bool& addedContent, bool& preview, float rate = 1.0f, bool hideRate = false, bool hideName = false);

	CAnimClipDebugIterator(CAnimationDebugInfo* pAnimDebugInfo, crmtNode* referenceNode = NULL);
	virtual ~CAnimClipDebugIterator() {};

	virtual void Visit(crmtNode&);	

protected:

	float CalcVisibleWeight(crmtNode& node);

	CAnimationDebugInfo* m_AnimDebugInfo;
};

class CAnimationDebugInfo : public CAIDebugInfo
{
public:

	friend class CAnimClipDebugIterator;

	CAnimationDebugInfo(const CDynamicEntity* pDyn, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CAnimationDebugInfo() {}

	virtual void Print();

private:

	bool ValidateInput();
	void PrintAnimations();

	RegdConstDyn m_Dyn;

};

class CIkDebugInfo : public CAIDebugInfo
{
public:

	CIkDebugInfo(const CPed* pPed, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CIkDebugInfo() {}

	virtual void Print();

private:

	bool ValidateInput();
	void PrintIkDebug();

	RegdConstPed m_Ped;

};

#endif // AI_DEBUG_OUTPUT_ENABLED