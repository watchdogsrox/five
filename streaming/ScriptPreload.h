// Title	:	ScriptPreload.h
// Author	:	Flavius Alecu
// Started	:	07/01/2013

// Exposes preloading of assets for peds and vehicles to script

#ifndef _SCRIPTPRELOAD_H_
#define _SCRIPTPRELOAD_H_

#include "fwscript/scripthandler.h"

#include "streaming/streamingrequest.h"

struct sReqData
{
    strRequest req;
    strLocalIndex idx;
};

class CScriptPreloadData
{
public:
	CScriptPreloadData(u32 numDwds, u32 numTxds, u32 numClds, u32 numFrags, u32 numDrawables, bool scriptMem);
	~CScriptPreloadData();

	bool IsDwdPreloaded(strLocalIndex index) const;
	bool IsTxdPreloaded(strLocalIndex index) const;
	bool IsCldPreloaded(strLocalIndex index) const;
	bool IsFragPreloaded(strLocalIndex index) const;
	bool IsDrawablePreloaded(strLocalIndex index) const;

	bool HasPreloadFinished();
	bool HasPreloadFinished(u32 handle);
	void CleanUp();

	bool AddDwd(strLocalIndex dwd, s32 parentTxd, s32& slot, u32 streamingFlags = 0);
	bool AddTxd(strLocalIndex txd, s32& slot, u32 streamingFlags = 0);
	bool AddCld(strLocalIndex cld, s32& slot, u32 streamingFlags = 0);
	bool AddFrag(strLocalIndex frag, s32 parentTxd, s32& slot, u32 streamingFlags = 0);
	bool AddDrawable(strLocalIndex drawable, s32 parentTxd, s32& slot, u32 streamingFlags = 0);

    u32 GetHandle(s32 dwd, s32 txd, s32 cld);
    void ReleaseHandle(u32 handle);

private:
	s32 GetSlotForAsset(strLocalIndex assetIdx, atArray<sReqData>& reqs);

	void ReleaseResource(ScriptResourceType type, atArray<sReqData>& reqs);

	atArray<sReqData>	m_reqDwds;
	atArray<sReqData>	m_reqTxds;
	atArray<sReqData>	m_reqClds;
	atArray<sReqData>	m_reqFrags;
	atArray<sReqData>	m_reqDrawables;

	bool m_scriptMem;
};

#endif // _SCRIPTPRELOAD_H_
