#include "ReplayTrailController.h"


#if GTA_REPLAY

#include "script/commands_graphics.h"
#include "vfx/misc/ScriptIM.h"

#include "streaming/streaming.h"

#include "control/replay/replay.h"

#include "frontend/VideoEditor/ui/Menu.h"

//////////////////////////////////////////////////////////////////////////
// Utility functions to help with streaming in the texture required
grcTexture* GetTextureFromStreamedTxd(const u32 pStreamedTxdName, const u32 pNameOfTexture)
{
	grcTexture *pTexture = NULL;
	strLocalIndex txdId = strLocalIndex(g_TxdStore.FindSlot(pStreamedTxdName));
	if (replayVerifyf(txdId != -1, "Replay:GetTextureFromStreamedTxd - Invalid texture dictionary name %us - the txd must be inside an img file that the game loads", pStreamedTxdName))
	{
		if (replayVerifyf(g_TxdStore.IsValidSlot(txdId), "Replay:GetTextureFromStreamedTxd - failed to get valid txd with name %u", pStreamedTxdName))
		{
			fwTxd* pTxd = g_TxdStore.Get(txdId);
			if (replayVerifyf(pTxd, "Replay:GetTextureFromStreamedTxd - Texture dictionary %u not loaded", pStreamedTxdName))
			{
				pTexture = pTxd->Lookup(pNameOfTexture);
			}
		}
	}

	if (!pTexture)
	{
		replayAssertf(false, "Replay:GetTextureFromStreamedTxd - texture %u is not in texture dictionary %u", pNameOfTexture, pStreamedTxdName);
		pTexture = grcTextureFactory::GetInstance().Create("none");
	}

	return pTexture;
}

bool HasStreamedTxdLoaded(const u32 streamedTxdName)
{
	strLocalIndex index = g_TxdStore.FindSlot(streamedTxdName);
	if(replayVerifyf(index != -1, "Replay:HAS_STREAMED_TEXTURE_DICT_LOADED - Invalid texture dictionary name %u - the txd must be inside an img file that the game loads", streamedTxdName))
	{
		return CStreaming::HasObjectLoaded(index, g_TxdStore.GetStreamingModuleId());
	}
	return false;
}

strLocalIndex RequestStreamedTxd(const u32 streamedTxdName)
{
	strLocalIndex index = g_TxdStore.FindSlot(streamedTxdName);
	if(replayVerifyf(index != -1, "Replay:REQUEST_STREAMED_TEXTURE_DICT - Invalid texture dictionary name %u - the txd must be inside an img file that the game loads", streamedTxdName))
	{
		CStreaming::RequestObject(index, g_TxdStore.GetStreamingModuleId(), STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE);
	}
	return index;
}


//////////////////////////////////////////////////////////////////////////
// Main trail Controller for Replay
CReplayTrailController::CReplayTrailController()
	: m_packetBuffer(nullptr)
	, m_pCurrentPlaybackPacket(nullptr)
	, m_texDictionaryToRelease(0)
{}


//////////////////////////////////////////////////////////////////////////
void CReplayTrailController::Init()
{
	replayAssert(!m_packetBuffer);
	m_packetBuffer = (u8*)sysMemManager::GetInstance().GetReplayAllocator()->Allocate(60000, 16);

	m_pCurrentPlaybackPacket = nullptr;
	m_texDictionaryToRelease = 0;
}


//////////////////////////////////////////////////////////////////////////
void CReplayTrailController::Shutdown()
{
	if(m_packetBuffer)
	{
		sysMemManager::GetInstance().GetReplayAllocator()->Free(m_packetBuffer);
		m_packetBuffer = nullptr;
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayTrailController::Update()
{
	replayAssert(m_packetBuffer);
	if(CReplayMgr::ShouldRecord())
	{
		if(m_polys.GetCount() != 0)
		{
			((CPacketTrailPolyFrame*)m_packetBuffer)->Store(m_texDictionary, m_texture, m_polys, m_colours, m_uvs);
			replayFatalAssertf(((CPacketTrailPolyFrame*)m_packetBuffer)->GetPacketSize() <= 60000, "Not enough space %u", ((CPacketTrailPolyFrame*)m_packetBuffer)->GetPacketSize());
			CReplayMgr::RecordFx<CPacketTrailPolyFrame>(((CPacketTrailPolyFrame*)m_packetBuffer));
		}

		m_polys.clear();
		m_colours.clear();
		m_uvs.clear();
	}
	else if(CReplayMgr::IsReplayInControlOfWorld())
	{
		if(m_pCurrentPlaybackPacket)
		{
			m_pCurrentPlaybackPacket->Render();
			m_pCurrentPlaybackPacket = nullptr;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// Called on exiting a clip to release the texture if we obtained it
void CReplayTrailController::OnExitClip()
{
	replayAssert(m_packetBuffer);
	if(m_texDictionaryToRelease != 0)
	{
		CStreaming::ClearRequiredFlag(m_texDictionaryToRelease, g_TxdStore.GetStreamingModuleId(), STRFLAG_DONTDELETE);
		CStreaming::RemoveObject(m_texDictionaryToRelease, g_TxdStore.GetStreamingModuleId());
		m_texDictionaryToRelease = 0;
		m_pCurrentPlaybackPacket = nullptr;
	}
}


//////////////////////////////////////////////////////////////////////////
// Store a poly that came in on recording
void CReplayTrailController::AddPolyFromGame(u32 texDict, u32 tex, const Vec3V& vec0, const Vec3V& vec1, const Vec3V& vec2, Color32 colour1, Color32 colour2, Color32 colour3, const Vector2& uv1, const Vector2& uv2, const Vector2& uv3)
{
	replayAssert(m_packetBuffer);
	if(CReplayMgr::ShouldRecord())
	{
		CTrailColour colour;
		colour.m_colour1 = colour1.GetColor();
		colour.m_colour2 = colour2.GetColor();
		colour.m_colour3 = colour3.GetColor();

		CTrailUV uv;
		uv.m_uv1.Store(uv1);
		uv.m_uv2.Store(uv2);
		uv.m_uv3.Store(uv3);

		CTrailPoly poly;
		poly.m_vec0.Store(vec0);
		poly.m_vec1.Store(vec1);
		poly.m_vec2.Store(vec2);
		poly.m_colourIndex = (u16)GetColourIndex(colour);
		poly.m_uvIndex = (u16)GetUVIndex(uv);

		m_polys.PushAndGrow(poly);

		m_texDictionary = texDict;
		m_texture = tex;

// 		replayDebugf1("Add Poly %f, %f, %f,\n %f, %f, %f,\n %f, %f, %f,\n %u, %u, %u", 
// 			vec0.GetXf(), vec0.GetYf(), vec0.GetZf(),
// 			vec1.GetXf(), vec1.GetYf(), vec1.GetZf(),
// 			vec2.GetXf(), vec2.GetYf(), vec2.GetZf(),
// 			colour1.GetColor(), colour2.GetColor(), colour3.GetColor());
	}
}



int CReplayTrailController::GetColourIndex(const CTrailColour& colour)
{
	int index = m_colours.Find(colour);
	if(index == -1)
	{
		m_colours.PushAndGrow(colour);
		index = m_colours.GetCount()-1;
	}
	return index;
}

int CReplayTrailController::GetUVIndex(const CTrailUV& uv)
{
	int index = m_uvs.Find(uv);
	if(index == -1)
	{
		m_uvs.PushAndGrow(uv);
		index = m_uvs.GetCount()-1;
	}
	return index;
}


//////////////////////////////////////////////////////////////////////////
// Packet functions for the trails
void CPacketTrailPolyFrame::Store(u32 texDict, u32 tex, const atArray<CReplayTrailController::CTrailPoly>& polys, const atArray<CReplayTrailController::CTrailColour>& colours, const atArray<CReplayTrailController::CTrailUV>& uvs)
{
	CPacketEvent::Store(PACKETID_TRAILPOLYSFRAME, sizeof(CPacketTrailPolyFrame));
	CPacketEntityInfo::Store();

	m_texDictionary = texDict;
	m_texture = tex;

	m_polyCount = polys.GetCount();
	m_colourCount = colours.GetCount();
	m_uvCount = uvs.GetCount();
	m_offsetToPolys = sizeof(CPacketTrailPolyFrame);

	memcpy(GetPolys(), polys.GetElements(), sizeof(CReplayTrailController::CTrailPoly) * m_polyCount);
	AddToPacketSize(sizeof(CReplayTrailController::CTrailPoly) * m_polyCount);
	memcpy(GetColours(), colours.GetElements(), sizeof(CReplayTrailController::CTrailColour) * m_colourCount);
	AddToPacketSize(sizeof(CReplayTrailController::CTrailColour) * m_colourCount);
	memcpy(GetUVs(), uvs.GetElements(), sizeof(CReplayTrailController::CTrailUV) * m_uvCount);
	AddToPacketSize(sizeof(CReplayTrailController::CTrailUV) * m_uvCount);
}


void CPacketTrailPolyFrame::Extract(const CEventInfo<void>& /*eventInfo*/) const
{
	ScriptIM::SetBackFaceCulling(false);
	ScriptIM::SetDepthWriting(true);

	if(!HasStreamedTxdLoaded(m_texDictionary))
	{
		strLocalIndex index = RequestStreamedTxd(m_texDictionary);
		CReplayTrailController::GetInstance().SetTextureDictionaryToRelease(index);
		return;
	}

	CReplayTrailController::GetInstance().SetCurrentPlaybackPacket(this);
}


void CPacketTrailPolyFrame::Render() const
{
	bool UIActive = CWarningScreen::IsActive() || CVideoEditorMenu::IsUserConfirmationScreenActive();
	if(HasStreamedTxdLoaded(m_texDictionary) && !UIActive)
	{
		const CReplayTrailController::CTrailPoly* pPoly = GetPolys();
		const CReplayTrailController::CTrailColour* pColours = GetColours();
		const CReplayTrailController::CTrailUV* pUVs = GetUVs();
		for(int i = 0; i < m_polyCount; ++i)
		{
			grcTexture* pTexture = GetTextureFromStreamedTxd(m_texDictionary, m_texture);

			Vec3V vec0, vec1, vec2;
			pPoly->m_vec0.Load(vec0);
			pPoly->m_vec1.Load(vec1);
			pPoly->m_vec2.Load(vec2);

			const CReplayTrailController::CTrailColour* pColour = &pColours[pPoly->m_colourIndex];
			Color32 colour1(pColour->m_colour1);
			Color32 colour2(pColour->m_colour2);
			Color32 colour3(pColour->m_colour3);

			const CReplayTrailController::CTrailUV* pUV = &pUVs[pPoly->m_uvIndex];
			Vector2 uv1, uv2, uv3;
			pUV->m_uv1.Load(uv1);
			pUV->m_uv2.Load(uv2);
			pUV->m_uv3.Load(uv3);

			ScriptIM::PolyTex(vec0, vec1, vec2, colour1, colour2, colour3, pTexture, uv1, uv2, uv3);

			++pPoly;
		}
	}
}

#endif // GTA_REPLAY