#ifndef REPLAYRENDERTARGETHELPER_H_
#define REPLAYRENDERTARGETHELPER_H_


#include "ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/Misc/ReplayPacket.h"

class CPacketRegisterAndLinkRenderTarget : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketRegisterAndLinkRenderTarget(const atString& rtName, bool delay, int hashKey, u32 ownerID);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool	HasExpired(const CEventInfoBase&) const;
	
	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_REGISTERANDLINKRENDERTARGET, "Validation of CPacketRegisterAndLinkRenderTarget Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_REGISTERANDLINKRENDERTARGET;
	}

private:
	char		m_renderTargetName[64];
	bool		m_delay;
	int			m_linkToModelHashKey;
	u32			m_ownerID;
};


class CPacketSetRender: public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketSetRender(int renderID);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SETRENDER, "Validation of CPacketSetRender Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SETRENDER;
	}

private:

	int			m_renderID;
};


class CPacketSetGFXAlign: public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketSetGFXAlign(s32 alignX, s32 alignY);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_SETGFXALIGN, "Validation of CPacketSetGFXAlign Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_SETGFXALIGN;
	}

private:

	s32			m_alignX;
	s32			m_alignY;
};


class CPacketDrawSprite : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketDrawSprite(const char* texDictName, const char* textName, float centreX, float centreY, float width, float height, float rotation, int r, int g, int b, int a, bool doStereorize, int renderTargetIndex);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_DRAWSPRITE, "Validation of CPacketDrawSprite Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_DRAWSPRITE;
	}

private:

	char		m_textureDictionaryName[32];
	char		m_textureName[32];
	float		m_centreX;
	float		m_centreY;
	float		m_width;
	float		m_height;
	float		m_rotation;
	int			m_red;
	int			m_green;
	int			m_blue;
	int			m_alpha;
	bool		m_doStereorize;

	int			m_renderTargetIndex;
};

class CPacketDrawSprite2 : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketDrawSprite2(const char* texDictName, const char* textName, float centreX, float centreY, float width, float height, float rotation, int r, int g, int b, int a, bool doStereorize, int renderTargetIndex);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_DRAWSPRITE2, "Validation of CPacketDrawSprite2 Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_DRAWSPRITE2;
	}

private:

	char		m_textureDictionaryName[128];
	char		m_textureName[128];
	float		m_centreX;
	float		m_centreY;
	float		m_width;
	float		m_height;
	float		m_rotation;
	int			m_red;
	int			m_green;
	int			m_blue;
	int			m_alpha;
	bool		m_doStereorize;

	int			m_renderTargetIndex;
};

class CPacketDrawMovie : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketDrawMovie(float centreX, float centreY, float width, float height, float rotation, int r, int g, int b, int a, u32 movieHandle, int renderTargetIndex);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void	PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);		}
	bool	ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_DRAWMOVIE, "Validation of CPacketDrawMovie Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_DRAWMOVIE;
	}

private:

	float		m_centreX;
	float		m_centreY;
	float		m_width;
	float		m_height;
	float		m_rotation;
	int			m_red;
	int			m_green;
	int			m_blue;
	int			m_alpha;

	u32			m_movieHandle;

	int			m_renderTargetIndex;
};


class CReplayRenderTargetHelper
{
	friend class CPacketRegisterAndLinkRenderTarget;
	friend class CPacketSetGFXAlign;
	friend class CPacketSetRender;
	friend class CPacketDrawSprite;
public:

	static CReplayRenderTargetHelper& GetInstance()
	{
		static CReplayRenderTargetHelper instance;
		return instance;
	}

	CReplayRenderTargetHelper();

	void OnNetworkClanGetEmblemTXDName();
	void OnRegisterNamedRenderTarget(const char *name, bool delay);

	void OnLinkNamedRenderTarget(int ModelHashKey, u32 ownerID);
	void OnSetTextRenderID(int renderID);
	void OnSetScriptGFXAlign(s32 alignX, s32 alignY);
	void OnDrawSprite(const char *pTextureDictionaryName, const char *pTextureName, float CentreX, float CentreY, float Width, float Height, float Rotation, int R, int G, int B, int A, bool DoStereorize);

	void RemoveRenderTargetOnRecording(const char *name);

	void Cleanup();
	void Process();

private:

	bool DoRegisterNamedRenderTarget(const char *name, bool delay, u32 ownerID);
	void DoLinkNamedRenderTarget(int ModelHashKey, u32 ownerID);

	void DoSetTextRenderID(int renderID, bool fromPacket);
	void DoSetScriptGFXAlign(s32 alignX, s32 alignY, bool fromPacket);
	void DoDrawSprite(const char *pTextureDictionaryName, const char *pTextureName, float CentreX, float CentreY, float Width, float Height, float Rotation, int R, int G, int B, int A, bool DoStereorize, bool fromPacket);
	void DoResetScriptGFXAlign();



	atString	m_renderTargetName;
	bool		m_delay;
	int			m_linkToModelHashKey;
	bool		m_hasRegistered;

	atArray<atHashString> m_registeredRenderTargetsOnRecording;

	struct textureToRender
	{
		int			m_renderID;
		s32			m_alignX;
		s32			m_alignY;

		atString	m_textureDictionaryName;
		atString	m_textureName;
		float		m_centreX;
		float		m_centreY;
		float		m_width;
		float		m_height;
		float		m_rotation;
		int			m_red;
		int			m_green;
		int			m_blue;
		int			m_alpha;
		bool		m_doStereorize;
	};

	struct rtInfo 
	{
		atString	rtName;
		u32			rtOwner;
		u32			uniqueID;
		textureToRender texRender;
		bool operator==(const rtInfo& other) const
		{
			return rtName == other.rtName && rtOwner == other.rtOwner && uniqueID == other.uniqueID;
		}
	};
	atArray<rtInfo> m_registeredRenderTargetsOnPlayback;
	rtInfo* pCurrentRTInfo;
	rtInfo* FindRenderTargetInfo(u32 uniqueID)
	{
		for(int i = 0; i < m_registeredRenderTargetsOnPlayback.GetCount(); ++i)
		{
			if(m_registeredRenderTargetsOnPlayback[i].uniqueID == uniqueID)
				return &m_registeredRenderTargetsOnPlayback[i];
		}
		return nullptr;
	}

	int			m_renderID;
	s32			m_alignX;
	s32			m_alignY;

	atString	m_textureDictionaryName;
	atString	m_textureName;
	float		m_centreX;
	float		m_centreY;
	float		m_width;
	float		m_height;
	float		m_rotation;
	int			m_red;
	int			m_green;
	int			m_blue;
	int			m_alpha;
	bool		m_doStereorize;
};


#endif // GTA_REPLAY

#endif // REPLAYRENDERTARGETHELPER_H_