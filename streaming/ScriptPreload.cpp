#include "ScriptPreload.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/fragmentstore.h"

#include "script/handlers/GameScriptResources.h"
#include "script/script.h"

CScriptPreloadData::CScriptPreloadData(u32 numDwds, u32 numTxds, u32 numClds, u32 numFrags, u32 numDrawables, bool scriptMem) : m_scriptMem(scriptMem)
{
	m_reqDwds.Resize(numDwds);
	m_reqTxds.Resize(numTxds);
    m_reqClds.Resize(numClds);
    m_reqFrags.Resize(numFrags);
    m_reqDrawables.Resize(numDrawables);
}

CScriptPreloadData::~CScriptPreloadData()
{
	CleanUp();
}

s32 CScriptPreloadData::GetSlotForAsset(strLocalIndex assetIdx, atArray<sReqData>& reqs)
{
    s32 firstFree = -1;
	for (s32 i = 0; i < reqs.GetCount(); ++i)
	{
		if (firstFree == -1 && reqs[i].req.GetRequestId().IsInvalid())
            firstFree = i;

        if (reqs[i].idx == assetIdx)
            return i;
	}

	return firstFree;
}

bool CScriptPreloadData::IsDwdPreloaded(strLocalIndex index) const
{
	for (s32 i = 0; i < m_reqDwds.GetCount(); ++i)
	{
		if (m_reqDwds[i].req.GetRequestId().Get() == index.Get())
			return true;
	}
	return false;
}

bool CScriptPreloadData::IsTxdPreloaded(strLocalIndex index) const
{
	for (s32 i = 0; i < m_reqTxds.GetCount(); ++i)
	{
		if (m_reqTxds[i].req.GetRequestId().Get() == index.Get())
			return true;
	}
	return false;
}

bool CScriptPreloadData::IsCldPreloaded(strLocalIndex index) const
{
	for (s32 i = 0; i < m_reqClds.GetCount(); ++i)
	{
		if (m_reqClds[i].req.GetRequestId().Get() == index.Get())
			return true;
	}
	return false;
}

bool CScriptPreloadData::IsFragPreloaded(strLocalIndex index) const
{
	for (s32 i = 0; i < m_reqFrags.GetCount(); ++i)
	{
		if (m_reqFrags[i].req.GetRequestId().Get() == index.Get())
			return true;
	}
	return false;
}

bool CScriptPreloadData::IsDrawablePreloaded(strLocalIndex index) const
{
	for (s32 i = 0; i < m_reqDrawables.GetCount(); ++i)
	{
		if (m_reqDrawables[i].req.GetRequestId().Get() == index.Get())
			return true;
	}
	return false;
}

bool CScriptPreloadData::HasPreloadFinished()
{ 
	bool bResult = true;
	for (int i = 0 ; i < m_reqDwds.GetCount(); ++i)
	{
		if (m_reqDwds[i].req.IsValid())
		{
			bResult &= m_reqDwds[i].req.HasLoaded();
		}
	}
	for (int i = 0 ; i < m_reqTxds.GetCount(); ++i)
	{
		if (m_reqTxds[i].req.IsValid())
		{
			bResult &= m_reqTxds[i].req.HasLoaded();
		}
	}
	for (int i = 0 ; i < m_reqClds.GetCount(); ++i)
	{
		if (m_reqClds[i].req.IsValid())
		{
			bResult &= m_reqClds[i].req.HasLoaded();
		}
	}
	for (int i = 0 ; i < m_reqFrags.GetCount(); ++i)
	{
		if (m_reqFrags[i].req.IsValid())
		{
			bResult &= m_reqFrags[i].req.HasLoaded();
		}
	}
	for (int i = 0 ; i < m_reqDrawables.GetCount(); ++i)
	{
		if (m_reqDrawables[i].req.IsValid())
		{
			bResult &= m_reqDrawables[i].req.HasLoaded();
		}
	}
	return bResult;
}

bool CScriptPreloadData::HasPreloadFinished(u32 handle)
{ 
    if (handle == 0)
        return true;

    s32 dwd = (handle & 0xff) - 1;
    s32 txd = ((handle >> 8) & 0xff) - 1;
    s32 cld = ((handle >> 16) & 0xff) - 1;

	bool bResult = true;
	if (dwd > -1 && m_reqDwds[dwd].req.IsValid())
	{
		bResult &= m_reqDwds[dwd].req.HasLoaded();
	}
	if (txd > -1 && m_reqTxds[txd].req.IsValid())
	{
		bResult &= m_reqTxds[txd].req.HasLoaded();
	}
	if (cld > -1 && m_reqClds[cld].req.IsValid())
	{
		bResult &= m_reqClds[cld].req.HasLoaded();
	}
	return bResult;
}


void CScriptPreloadData::CleanUp()
{ 
	// free script resources
	ReleaseResource(CGameScriptResource::SCRIPT_RESOURCE_DRAWABLE_DICTIONARY, m_reqDwds);
	ReleaseResource(CGameScriptResource::SCRIPT_RESOURCE_TEXTURE_DICTIONARY, m_reqTxds);
	ReleaseResource(CGameScriptResource::SCRIPT_RESOURCE_CLOTH_DICTIONARY, m_reqClds);
	ReleaseResource(CGameScriptResource::SCRIPT_RESOURCE_FRAG_DICTIONARY, m_reqFrags);
	ReleaseResource(CGameScriptResource::SCRIPT_RESOURCE_DRAWABLE, m_reqDrawables);
}

void CScriptPreloadData::ReleaseResource(ScriptResourceType type, atArray<sReqData>& reqs)
{
	if (m_scriptMem && CTheScripts::GetCurrentGtaScriptHandler())
	{
		for (s32 i = 0; i < reqs.GetCount(); ++i)
		{
			if (reqs[i].req.GetRequestId() != -1)
				CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(type, static_cast<ScriptResourceRef>(reqs[i].req.GetRequestId().Get()));
		}
	}

	for (int i = 0 ; i < reqs.GetCount(); ++i)
	{
		reqs[i].req.ClearRequiredFlags(STRFLAG_DONTDELETE);
		reqs[i].req.Release();
	}
	reqs.Reset();
}

bool CScriptPreloadData::AddTxd(strLocalIndex txd, s32& slot, u32 streamingFlags)
{
	if (txd.IsInvalid())
	{
		slot = -1;
		return false;
	}

	slot = GetSlotForAsset(txd, m_reqTxds);
	if (slot == -1)
		return false;

    m_reqTxds[slot].idx = txd;
	const strLocalIndex currentTxdRequestId = m_reqTxds[slot].req.GetRequestId();

	if (m_scriptMem && currentTxdRequestId != -1 && CTheScripts::GetCurrentGtaScriptHandler())
		CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_TEXTURE_DICTIONARY, static_cast<ScriptResourceRef>(currentTxdRequestId.Get()));

	m_reqTxds[slot].req.ClearRequiredFlags(STRFLAG_DONTDELETE);
	m_reqTxds[slot].req.Release();


	if (m_scriptMem && CTheScripts::GetCurrentGtaScriptHandler())
	{
		CScriptResource_TextureDictionary textDict(txd, u32(0));
		CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(textDict);
	}

	streamingFlags |= STRFLAG_DONTDELETE;
	m_reqTxds[slot].req.Request(txd, g_TxdStore.GetStreamingModuleId(), streamingFlags);
    return true;
}

bool CScriptPreloadData::AddDwd(strLocalIndex dwd, s32 parentTxd, s32& slot, u32 streamingFlags)
{
    if (dwd.IsInvalid())
	{
        slot = -1;
        return false;
	}

    slot = GetSlotForAsset(dwd, m_reqDwds);
    if (slot == -1)
        return false;

    m_reqDwds[slot].idx = dwd;
	const strLocalIndex currentDwdRequestId = m_reqDwds[slot].req.GetRequestId();

	if (m_scriptMem && currentDwdRequestId.IsValid() && CTheScripts::GetCurrentGtaScriptHandler())
		CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_DRAWABLE_DICTIONARY, static_cast<ScriptResourceRef>(currentDwdRequestId.Get()));

	m_reqDwds[slot].req.ClearRequiredFlags(STRFLAG_DONTDELETE);
	m_reqDwds[slot].req.Release();


	if (parentTxd != -1)
		g_DwdStore.SetParentTxdForSlot(dwd, strLocalIndex(parentTxd));

	if (m_scriptMem && CTheScripts::GetCurrentGtaScriptHandler())
	{
		CScriptResource_DrawableDictionary drawDict(dwd, u32(0));
		CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(drawDict);
	}

	streamingFlags |= STRFLAG_DONTDELETE;
	m_reqDwds[slot].req.Request(dwd, g_DwdStore.GetStreamingModuleId(), streamingFlags);
    return true;
}

bool CScriptPreloadData::AddCld(strLocalIndex cld, s32& slot, u32 streamingFlags)
{
	if (cld.IsInvalid())
	{
		slot = -1;
		return false;
	}

	slot = GetSlotForAsset(cld, m_reqClds);
	if (slot == -1)
		return false;

    m_reqClds[slot].idx = cld;
	const strLocalIndex currentCldsRequestId = m_reqClds[slot].req.GetRequestId();

	if (m_scriptMem && currentCldsRequestId != -1 && CTheScripts::GetCurrentGtaScriptHandler())
		CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_CLOTH_DICTIONARY, static_cast<ScriptResourceRef>(currentCldsRequestId.Get()));

	m_reqClds[slot].req.ClearRequiredFlags(STRFLAG_DONTDELETE);
	m_reqClds[slot].req.Release();


	if (m_scriptMem && CTheScripts::GetCurrentGtaScriptHandler())
	{
		CScriptResource_ClothDictionary clothDict(cld, u32(0));
		CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(clothDict);
	}

	streamingFlags |= STRFLAG_DONTDELETE;
	m_reqClds[slot].req.Request(cld, g_ClothStore.GetStreamingModuleId(), streamingFlags);
    return true;
}

bool CScriptPreloadData::AddFrag(strLocalIndex frag, s32 parentTxd, s32& slot, u32 streamingFlags)
{
	if (frag.IsInvalid())
	{
		slot = -1;
		return false;
	}

	slot = GetSlotForAsset(frag, m_reqFrags);
	if (slot == -1)
		return false;

    m_reqFrags[slot].idx = frag;
	const strLocalIndex currentFragsRequestId = m_reqFrags[slot].req.GetRequestId();

	if (m_scriptMem && currentFragsRequestId != -1 && CTheScripts::GetCurrentGtaScriptHandler())
		CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_FRAG_DICTIONARY, static_cast<ScriptResourceRef>(currentFragsRequestId.Get()));

	m_reqFrags[slot].req.ClearRequiredFlags(STRFLAG_DONTDELETE);
	m_reqFrags[slot].req.Release();


	if (parentTxd != -1)
	{
		// this check is pretty nasty, but the vehicle artists share mods between vehicles and the parent id WILL change
		// causing streaming refcounting issues. this way we just don't reparent the mod asset and they'll have to make sure
		// the parenting hierarchy matches for the shared mods.
		if (g_FragmentStore.GetParentTxdForSlot(frag) == strLocalIndex(STRLOCALINDEX_INVALID))
		{
			g_FragmentStore.SetParentTxdForSlot(frag, strLocalIndex(parentTxd));
		}
	}

	if (m_scriptMem && CTheScripts::GetCurrentGtaScriptHandler())
	{
		CScriptResource_FragDictionary fragDict(frag, u32(0));
		CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(fragDict);
	}

	streamingFlags |= STRFLAG_DONTDELETE;
	m_reqFrags[slot].req.Request(frag, g_FragmentStore.GetStreamingModuleId(), streamingFlags);
    return true;
}

bool CScriptPreloadData::AddDrawable(strLocalIndex drawable, s32 parentTxd, s32& slot, u32 streamingFlags)
{
	if (drawable.IsInvalid())
	{
		slot = -1;
		return false;
	}

	slot = GetSlotForAsset(drawable, m_reqDrawables);
	if (slot == -1)
		return false;

    m_reqDrawables[slot].idx = drawable;
	const strLocalIndex currentDrawableRequestId = m_reqDrawables[slot].req.GetRequestId();

	if (m_scriptMem && currentDrawableRequestId != -1 && CTheScripts::GetCurrentGtaScriptHandler())
		CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_DRAWABLE, static_cast<ScriptResourceRef>(currentDrawableRequestId.Get()));

	m_reqDrawables[slot].req.ClearRequiredFlags(STRFLAG_DONTDELETE);
	m_reqDrawables[slot].req.Release();


	if (parentTxd != -1)
		g_DrawableStore.SetParentTxdForSlot(drawable, strLocalIndex(parentTxd));

	if (m_scriptMem && CTheScripts::GetCurrentGtaScriptHandler())
	{
		CScriptResource_Drawable scriptDrawable(drawable, u32(0));
		CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(scriptDrawable);
	}

	streamingFlags |= STRFLAG_DONTDELETE;
	m_reqDrawables[slot].req.Request(drawable, g_DrawableStore.GetStreamingModuleId(), streamingFlags);
    return true;
}

u32 CScriptPreloadData::GetHandle(s32 dwd, s32 txd, s32 cld)
{
    u32 ret = 0;
	Assert((dwd + 1 & ~0xff) == 0);
	ret |= (dwd + 1);

	Assert((txd + 1 & ~0xff) == 0);
	ret |= (txd + 1) << 8;

	Assert((cld + 1 & ~0xff) == 0);
	ret |= (cld + 1) << 16;

    return ret;
}

void CScriptPreloadData::ReleaseHandle(u32 handle)
{
	s32 dwd = (handle & 0xff) - 1;
	s32 txd = ((handle >> 8) & 0xff) - 1;
	s32 cld = ((handle >> 16) & 0xff) - 1;

	if (m_scriptMem && CTheScripts::GetCurrentGtaScriptHandler())
	{
        if (dwd > -1 && m_reqDwds[dwd].req.GetRequestId() != -1)
			CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_DRAWABLE_DICTIONARY, static_cast<ScriptResourceRef>(m_reqDwds[dwd].req.GetRequestId().Get()));

        if (txd > -1 && m_reqTxds[txd].req.GetRequestId() != -1)
			CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_TEXTURE_DICTIONARY, static_cast<ScriptResourceRef>(m_reqTxds[txd].req.GetRequestId().Get()));

        if (cld > -1 && m_reqClds[cld].req.GetRequestId() != -1)
			CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_CLOTH_DICTIONARY, static_cast<ScriptResourceRef>(m_reqClds[cld].req.GetRequestId().Get()));
	}

    m_reqDwds[dwd].req.ClearRequiredFlags(STRFLAG_DONTDELETE);
    m_reqTxds[txd].req.ClearRequiredFlags(STRFLAG_DONTDELETE);
    m_reqClds[cld].req.ClearRequiredFlags(STRFLAG_DONTDELETE);

    m_reqDwds[dwd].req.Release();
    m_reqTxds[txd].req.Release();
    m_reqClds[cld].req.Release();

    m_reqDwds[dwd].idx.Invalidate();
    m_reqTxds[dwd].idx.Invalidate();
    m_reqClds[dwd].idx.Invalidate();
}