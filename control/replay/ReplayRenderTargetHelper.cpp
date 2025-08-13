#include "ReplayRenderTargetHelper.h"


#if GTA_REPLAY

#include "ReplayMovieController.h"
#include "SaveLoad/savegame_photo_manager.h"
#include "script/commands_graphics.h"
#include "script/script.h"
#include "vfx/misc/MovieManager.h"

const char* pAcceptableRenderTargets[] = { "clubhouse_table",
											nullptr };

const char* pAcceptableTextureNames[] = { "ClanTexture",
											"SYSTEM_TOP_OFFLINE_1",
											"SYSTEM_TOP_OFFLINE_2",
											"SYSTEM_BOTTOM_OFFLINE_1",
											"SYSTEM_BOTTOM_OFFLINE_2",
											nullptr };


//////////////////////////////////////////////////////////////////////////
CPacketRegisterAndLinkRenderTarget::CPacketRegisterAndLinkRenderTarget(const atString& rtName, bool delay, int hashKey, u32 ownerID)
	: CPacketEvent(PACKETID_REGISTERANDLINKRENDERTARGET, sizeof(CPacketRegisterAndLinkRenderTarget))
{
	strcpy_s(m_renderTargetName, 64, rtName.c_str());
	m_delay = delay;
	m_linkToModelHashKey = hashKey;
	m_ownerID = ownerID;
}


//////////////////////////////////////////////////////////////////////////
void CPacketRegisterAndLinkRenderTarget::Extract(const CEventInfo<void>&) const
{
	CReplayRenderTargetHelper::GetInstance().DoRegisterNamedRenderTarget(m_renderTargetName, m_delay, m_ownerID);
	CReplayRenderTargetHelper::GetInstance().DoLinkNamedRenderTarget(m_linkToModelHashKey, m_ownerID);
}


bool CPacketRegisterAndLinkRenderTarget::HasExpired(const CEventInfoBase&) const
{ 
	if(gRenderTargetMgr.GetNamedRendertargetId(m_renderTargetName,m_ownerID) == 0)
	{
		CReplayRenderTargetHelper::GetInstance().RemoveRenderTargetOnRecording(m_renderTargetName);
		return true;
	}
	else
	{
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////
CPacketSetRender::CPacketSetRender(int renderID)
	: CPacketEvent(PACKETID_SETRENDER, sizeof(CPacketSetRender))
{
	m_renderID = renderID;
}


//////////////////////////////////////////////////////////////////////////
void CPacketSetRender::Extract(const CEventInfo<void>&) const
{
	CReplayRenderTargetHelper::GetInstance().DoSetTextRenderID(m_renderID, true);
}


//////////////////////////////////////////////////////////////////////////
CPacketSetGFXAlign::CPacketSetGFXAlign(s32 alignX, s32 alignY)
	: CPacketEvent(PACKETID_SETGFXALIGN, sizeof(CPacketSetGFXAlign))
{
	m_alignX = alignX;
	m_alignY = alignY;
}


//////////////////////////////////////////////////////////////////////////
void CPacketSetGFXAlign::Extract(const CEventInfo<void>&) const
{
	CReplayRenderTargetHelper::GetInstance().DoSetScriptGFXAlign(m_alignX, m_alignY, true);
}


//////////////////////////////////////////////////////////////////////////
CPacketDrawSprite::CPacketDrawSprite(const char* texDictName, const char* textName, float centreX, float centreY, float width, float height, float rotation, int r, int g, int b, int a, bool doStereorize, int renderTargetIndex)
	: CPacketEvent(PACKETID_DRAWSPRITE, sizeof(CPacketDrawSprite), 1)
{
	strcpy_s(m_textureDictionaryName, 32, texDictName);
	strcpy_s(m_textureName, 32, textName);
	m_centreX = centreX;
	m_centreY = centreY;
	m_width = width;
	m_height = height;
	m_rotation = rotation;
	m_red = r;
	m_green = g;
	m_blue = b;
	m_alpha = a;
	m_doStereorize = doStereorize;

	m_renderTargetIndex = renderTargetIndex;
}


//////////////////////////////////////////////////////////////////////////
void CPacketDrawSprite::Extract(const CEventInfo<void>&) const
{
	if(GetPacketVersion() >= 1)
		ReplayMovieControllerNew::SubmitSpriteThisFrame(m_textureDictionaryName, m_textureName, m_centreX, m_centreY, m_width, m_height, m_rotation, m_red, m_green, m_blue, m_alpha, m_doStereorize, m_renderTargetIndex);
	else
		ReplayMovieController::SubmitSpriteThisFrame(m_textureDictionaryName, m_textureName, m_centreX, m_centreY, m_width, m_height, m_rotation, m_red, m_green, m_blue, m_alpha, m_doStereorize);
}


//////////////////////////////////////////////////////////////////////////
CPacketDrawSprite2::CPacketDrawSprite2(const char* texDictName, const char* textName, float centreX, float centreY, float width, float height, float rotation, int r, int g, int b, int a, bool doStereorize, int renderTargetIndex)
	: CPacketEvent(PACKETID_DRAWSPRITE2, sizeof(CPacketDrawSprite2), 1)
{
	strcpy_s(m_textureDictionaryName, 128, texDictName);
	strcpy_s(m_textureName, 128, textName);
	m_centreX = centreX;
	m_centreY = centreY;
	m_width = width;
	m_height = height;
	m_rotation = rotation;
	m_red = r;
	m_green = g;
	m_blue = b;
	m_alpha = a;
	m_doStereorize = doStereorize;

	m_renderTargetIndex = renderTargetIndex;
}


//////////////////////////////////////////////////////////////////////////
void CPacketDrawSprite2::Extract(const CEventInfo<void>&) const
{
	if(GetPacketVersion() >= 1)
		ReplayMovieControllerNew::SubmitSpriteThisFrame(m_textureDictionaryName, m_textureName, m_centreX, m_centreY, m_width, m_height, m_rotation, m_red, m_green, m_blue, m_alpha, m_doStereorize, m_renderTargetIndex);
	else
		ReplayMovieController::SubmitSpriteThisFrame(m_textureDictionaryName, m_textureName, m_centreX, m_centreY, m_width, m_height, m_rotation, m_red, m_green, m_blue, m_alpha, m_doStereorize);
}

//////////////////////////////////////////////////////////////////////////
CPacketDrawMovie::CPacketDrawMovie(float centreX, float centreY, float width, float height, float rotation, int r, int g, int b, int a, u32 movieHandle, int renderTargetIndex)
	: CPacketEvent(PACKETID_DRAWMOVIE, sizeof(CPacketDrawMovie))
{
	m_centreX = centreX;
	m_centreY = centreY;
	m_width = width;
	m_height = height;
	m_rotation = rotation;
	m_red = r;
	m_green = g;
	m_blue = b;
	m_alpha = a;
	m_movieHandle = movieHandle;

	m_renderTargetIndex = renderTargetIndex;

}


//////////////////////////////////////////////////////////////////////////
void CPacketDrawMovie::Extract(const CEventInfo<void>&) const
{
	ReplayMovieControllerNew::SubmitMovieThisFrame(m_centreX, m_centreY, m_width, m_height, m_rotation, m_red, m_green, m_blue, m_alpha, m_movieHandle, m_renderTargetIndex);
}


//////////////////////////////////////////////////////////////////////////
CReplayRenderTargetHelper::CReplayRenderTargetHelper()
{
	m_hasRegistered = false;
	pCurrentRTInfo = nullptr;
}

void CReplayRenderTargetHelper::OnNetworkClanGetEmblemTXDName()
{

}


//////////////////////////////////////////////////////////////////////////
void CReplayRenderTargetHelper::OnRegisterNamedRenderTarget(const char *name, bool delay)
{
	replayAssert(!m_hasRegistered);

	m_renderTargetName = name;
	m_delay = delay;

	replayDebugf1("CReplayRenderTargetHelper::OnRegisterNamedRenderTarget %s", name);

	m_hasRegistered = true;
}


//////////////////////////////////////////////////////////////////////////
void CReplayRenderTargetHelper::OnLinkNamedRenderTarget(int ModelHashKey, u32 ownerID)
{
	if(m_hasRegistered)
	{
		m_linkToModelHashKey = ModelHashKey;

		bool recordPacket = false;

		int rtIndex = 0;
		const char* prtName = pAcceptableRenderTargets[0];
		while(prtName)
		{
			if(m_renderTargetName == prtName)
			{
				recordPacket = true;
				break;
			}

			prtName = pAcceptableRenderTargets[++rtIndex];
		}

		if(recordPacket)
		{
			replayDebugf1("CReplayRenderTargetHelper::OnLinkNamedRenderTarget RECORDED");
			replayDebugf1("  m_renderTargetName %s", m_renderTargetName.c_str());
			replayDebugf1("  m_delay %s", m_delay ? "true" : "false");
			replayDebugf1("  m_linkToModelHashKey %d", m_linkToModelHashKey);

#if DO_REPLAY_OUTPUT
			scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
			u32 rtID = gRenderTargetMgr.GetNamedRendertargetId(m_renderTargetName.c_str(), scriptThreadId);
#endif // DO_REPLAY_OUTPUT
			replayDebugf1("  ** Render target ID %u", rtID);

			CReplayMgr::RecordFx(CPacketRegisterAndLinkRenderTarget(m_renderTargetName, m_delay, m_linkToModelHashKey, ownerID));

			atHashString hash(m_renderTargetName.c_str());
			if(m_registeredRenderTargetsOnRecording.Find(hash) == -1)
				m_registeredRenderTargetsOnRecording.PushAndGrow(hash);
		}

		// Reset vars
		m_hasRegistered = false;
		m_renderTargetName = "";
		m_delay = false;
		m_linkToModelHashKey = 0;
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayRenderTargetHelper::OnSetTextRenderID(int renderID)
{
	if(m_registeredRenderTargetsOnRecording.GetCount() == 0)
		return;

	m_renderID = renderID;

	CReplayMgr::RecordFx(CPacketSetRender(m_renderID));
}


//////////////////////////////////////////////////////////////////////////
void CReplayRenderTargetHelper::OnSetScriptGFXAlign(s32 alignX, s32 alignY)
{
	if(m_registeredRenderTargetsOnRecording.GetCount() == 0)
		return;

	m_alignX = alignX;
	m_alignY = alignY;

	CReplayMgr::RecordFx(CPacketSetGFXAlign(m_alignX, m_alignY));
}


//////////////////////////////////////////////////////////////////////////
void CReplayRenderTargetHelper::OnDrawSprite(const char *pTextureDictionaryName, const char *pTextureName, float CentreX, float CentreY, float Width, float Height, float Rotation, int R, int G, int B, int A, bool DoStereorize)
{
	if(m_registeredRenderTargetsOnRecording.GetCount() == 0)
		return;

	m_textureDictionaryName = pTextureDictionaryName;
	m_textureName = pTextureName;
	m_centreX = CentreX;
	m_centreY = CentreY;
	m_width = Width;
	m_height = Height;
	m_rotation = Rotation;
	m_red = R;
	m_green = G;
	m_blue = B;
	m_alpha = A;
	m_doStereorize = DoStereorize;



	bool recordPacket = false;

	int texIndex = 0;
	const char* ptexName = pAcceptableTextureNames[0];
	while(ptexName)
	{
		if(m_textureName.StartsWith(ptexName))
		{
			recordPacket = true;
			break;
		}

		ptexName = pAcceptableTextureNames[++texIndex];
	}

	if(recordPacket)
	{
		CReplayMgr::RecordFx(CPacketDrawSprite(m_textureDictionaryName, m_textureName, m_centreX, m_centreY, m_width, m_height, m_rotation, m_red, m_green, m_blue, m_alpha, m_doStereorize, 0));

#if 0
		replayDebugf1("CReplayRenderTargetHelper::OnDrawSprite");
		replayDebugf1("  m_renderID %d", m_renderID);
		replayDebugf1("  m_alignX %d", m_alignX);
		replayDebugf1("  m_alignY %d", m_alignY);
		replayDebugf1("  m_textureDictionaryName %s", m_textureDictionaryName.c_str());
		replayDebugf1("  m_textureName %s", m_textureName.c_str());
		replayDebugf1("  m_centreX %f", m_centreX);
		replayDebugf1("  m_centreY %f", m_centreY);
		replayDebugf1("  m_width %f", m_width);
		replayDebugf1("  m_height %f", m_height);
		replayDebugf1("  m_rotation %f", m_rotation);
		replayDebugf1("  m_red %d", m_red);
		replayDebugf1("  m_green %d", m_green);
		replayDebugf1("  m_blue %d", m_blue);
		replayDebugf1("  m_alpha %d", m_alpha);
		replayDebugf1("  m_doStereorize %s", m_doStereorize ? "true" : "false");
#endif

		m_renderID = 0;
		m_alignX = 0;
		m_alignY = 0;
		m_textureDictionaryName = "";
		m_textureName = "";
		m_centreX = 0.0f;
		m_centreY = 0.0f;
		m_width = 0.0f;
		m_height = 0.0f;
		m_rotation = 0.0f;
		m_red = 0;
		m_green = 0;
		m_blue = 0;
		m_alpha = 0;
		m_doStereorize = false;
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayRenderTargetHelper::RemoveRenderTargetOnRecording(const char *name)
{
	atHashString hash(name);
	int index = m_registeredRenderTargetsOnRecording.Find(hash);
	if(index != -1)
	{
		m_registeredRenderTargetsOnRecording.Delete(index);
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayRenderTargetHelper::Cleanup()
{
	for(int i = 0; i < m_registeredRenderTargetsOnPlayback.GetCount(); ++i)
	{
		const rtInfo& inf = m_registeredRenderTargetsOnPlayback[i];
		gRenderTargetMgr.ReleaseNamedRendertarget(inf.rtName.c_str(), inf.rtOwner);
	}
	m_registeredRenderTargetsOnPlayback.clear();

	CScriptHud::ms_CurrentScriptGfxAlignment.Reset();
	CScriptHud::ms_iCurrentScriptGfxDrawProperties &= ~SCRIPT_GFX_VISIBLE_WHEN_PAUSED;
}


//////////////////////////////////////////////////////////////////////////
void CReplayRenderTargetHelper::Process()
{
	for(int i = 0; i < m_registeredRenderTargetsOnPlayback.GetCount(); ++i)
	{
		const rtInfo& inf = m_registeredRenderTargetsOnPlayback[i];
		if(inf.texRender.m_textureName.GetLength() != 0)
		{
			DoSetTextRenderID(inf.texRender.m_renderID, false);
			DoSetScriptGFXAlign(inf.texRender.m_alignX, inf.texRender.m_alignY, false);
			DoDrawSprite(inf.texRender.m_textureDictionaryName, inf.texRender.m_textureName, inf.texRender.m_centreX, inf.texRender.m_centreY, inf.texRender.m_width, inf.texRender.m_height, inf.texRender.m_rotation, inf.texRender.m_red, inf.texRender.m_green, inf.texRender.m_blue, inf.texRender.m_alpha, inf.texRender.m_doStereorize, false);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
bool CReplayRenderTargetHelper::DoRegisterNamedRenderTarget(const char *name, bool delay, u32 ownerID)
{
	if (gRenderTargetMgr.RegisterNamedRendertarget(name,ownerID))
	{
		if(gRenderTargetMgr.SetNamedRendertargetDelayLoad(atFinalHashString(name), ownerID, delay))
		{
			rtInfo inf;
			inf.rtName = name;
			inf.rtOwner = ownerID;
			inf.uniqueID = gRenderTargetMgr.GetNamedRendertargetId(name, ownerID);

			if(m_registeredRenderTargetsOnPlayback.Find(inf) == -1)
				m_registeredRenderTargetsOnPlayback.PushAndGrow(inf);

			return true;
		}
	}
	return false;
}


void CReplayRenderTargetHelper::DoLinkNamedRenderTarget(int ModelHashKey, u32 ownerID)
{
	fwModelId modelId;
	CBaseModelInfo* modelinfo = CModelInfo::GetBaseModelInfoFromHashKey(ModelHashKey,&modelId);
	replayAssertf(modelinfo, "Failed to find model to link render target - %d", ModelHashKey);
	if( modelinfo )
	{
		modelinfo->SetCarryScriptedRT(true);
		modelinfo->SetScriptId(ownerID);

		if( modelinfo->GetDrawable() )
		{
			gRenderTargetMgr.LinkNamedRendertargets(modelId.GetModelIndex(), ownerID);
		}
	}
}


void CReplayRenderTargetHelper::DoSetTextRenderID(int renderID, bool fromPacket)
{
	if(fromPacket)
	{
		rtInfo* inf = FindRenderTargetInfo(renderID);
		if(inf)
		{
			inf->texRender.m_renderID = renderID;
			pCurrentRTInfo = inf;
		}
	}
	else
	{
		CScriptHud::scriptTextRenderID = renderID;
		gRenderTargetMgr.UseRenderTarget((CRenderTargetMgr::RenderTargetId)renderID);
		CScriptHud::CurrentScriptWidescreenFormat = WIDESCREEN_FORMAT_STRETCH;
	}
}


void CReplayRenderTargetHelper::DoSetScriptGFXAlign(s32 alignX, s32 alignY, bool fromPacket)
{
	if(fromPacket)
	{
		if(pCurrentRTInfo)
		{
			pCurrentRTInfo->texRender.m_alignX = alignX;
			pCurrentRTInfo->texRender.m_alignY = alignY;
		}
	}
	else
	{
		CScriptHud::ms_CurrentScriptGfxAlignment.m_alignX = (u8)alignX;
		CScriptHud::ms_CurrentScriptGfxAlignment.m_alignY = (u8)alignY;

		CScriptHud::ms_iCurrentScriptGfxDrawProperties |= SCRIPT_GFX_VISIBLE_WHEN_PAUSED;
	}
}


void CReplayRenderTargetHelper::DoDrawSprite(const char *pTextureDictionaryName, const char *pTextureName, float CentreX, float CentreY, float Width, float Height, float Rotation, int R, int G, int B, int A, bool DoStereorize, bool fromPacket)
{
	if(fromPacket)
	{
		if(pCurrentRTInfo)
		{
			pCurrentRTInfo->texRender.m_textureDictionaryName = pTextureDictionaryName;
			pCurrentRTInfo->texRender.m_textureName = pTextureName;
			pCurrentRTInfo->texRender.m_centreX = CentreX;
			pCurrentRTInfo->texRender.m_centreY = CentreY;
			pCurrentRTInfo->texRender.m_width = Width;
			pCurrentRTInfo->texRender.m_height = Height;
			pCurrentRTInfo->texRender.m_rotation = Rotation;
			pCurrentRTInfo->texRender.m_red = R;
			pCurrentRTInfo->texRender.m_green = G;
			pCurrentRTInfo->texRender.m_blue = B;
			pCurrentRTInfo->texRender.m_alpha = A;
			pCurrentRTInfo->texRender.m_doStereorize = DoStereorize;

			pCurrentRTInfo = nullptr;
		}
	}
	else
	{
		grcTexture* pTextureToDraw = graphics_commands::GetTextureFromStreamedTxd(pTextureDictionaryName, pTextureName);

		EScriptGfxType gfxType = SCRIPT_GFX_SPRITE;

		if (DoStereorize)
			gfxType = SCRIPT_GFX_SPRITE_STEREO;

		if (CPhotoManager::IsNameOfMissionCreatorTexture(pTextureName, pTextureDictionaryName) || (SCRIPTDOWNLOADABLETEXTUREMGR.IsNameOfATexture(pTextureName)) )
		{
			gfxType = SCRIPT_GFX_MISSION_CREATOR_PHOTO_PREVIEW;
		}

		static bool b = true;
		if(b)
			graphics_commands::DrawSpriteTex(pTextureToDraw, CentreX, CentreY, Width, Height, Rotation, R, G, B, A, INVALID_MOVIE_HANDLE, gfxType, Vector2(0.0f,0.0f), Vector2(1.0f, 1.0f), false);
	}
}


void CReplayRenderTargetHelper::DoResetScriptGFXAlign()
{
	CScriptHud::ms_CurrentScriptGfxAlignment.Reset();

	CScriptHud::ms_iCurrentScriptGfxDrawProperties &= ~SCRIPT_GFX_VISIBLE_WHEN_PAUSED;
}




#endif // GTA_REPLAY