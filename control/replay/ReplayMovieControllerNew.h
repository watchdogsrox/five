#ifndef __REPLAY_MOVIE_CONTROLLERNEW_H__
#define __REPLAY_MOVIE_CONTROLLERNEW_H__

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/Misc/MoviePacket.h"
#include "control/replay/Misc/MovieTargetPacket.h"
#include "renderer/RenderTargetMgr.h"

class ReplayMovieControllerNew
{
	class CRenderTargetInfo;

public:

	typedef ReplayMovieInfo<MAX_REPLAY_MOVIE_NAME_SIZE_64>	tMovieInfo;

	static void Init();

	static void	OnEntry();
	static void OnExit();

	static void	Process(bool bPlaying);

	static void	SubmitMoviesThisFrame(const atFixedArray<tMovieInfo, MAX_ACTIVE_MOVIES> &info);
	static void SubmitMovieTargetThisFrame(const MovieDestinationInfo &info);

	static void SubmitSpriteThisFrame(const char* texDictName, const char* textName, float centreX, float centreY, float width, float height, float rotation, int r, int g, int b, int a, bool doStereorize, int renderTargetIndex);
	static void SubmitMovieThisFrame(float centreX, float centreY, float width, float height, float rotation, int r, int g, int b, int a, u32 movieHandle, int renderTargetIndex);

	static void	RegisterMovieAudioLink(u32 modelNameHash, Vector3 &TVPos, u32 movieNameHash, bool isFrontendAudio);

	static void OnRegisterNamedRenderTarget(const char *name, u32 ownerID);
	static void OnReleaseNamedRenderTarget(const char *name);
	static void OnLinkNamedRenderTarget(int ModelHashKey, u32 ownerID, CRenderTargetMgr::namedRendertarget const* pRT);
	static void OnDrawSprite(const char *pTextureDictionaryName, const char *pTextureName, float CentreX, float CentreY, float Width, float Height, float Rotation, int R, int G, int B, int A, bool DoStereorize, int renderTargetID);
	static void OnDrawTVChannel(float CentreX, float CentreY, float Width, float Height, float Rotation, int R, int G, int B, int A, u32 movieHandle, int renderTargetID);
	static void Record();

	static void AddRenderTarget(int i);
	static void SetRenderTargetLink(int i, u32 modelHash);

	static atArray<atFinalHashString> const&	GetKnownRenderTargets() { return m_knownRenderTargets; }
	static u32	GetOwnerHash() { return atStringHash("REPLAY_TV"); }

private:

	//	Removes the records of what movies are doing this frame
	static void	ClearMoviesThisFrame();

	//	Unloads any movies not required 
	static void	PurgeUnusedMovies();

	// Load movies that are required.
	static void	LoadRequiredMovies();

	static void	StartPlayingMovies();
	static void StopPlayingMovies();

	static u32	LinkToRenderTarget(const char* renderTargetName, u32 ownerHash, u32 modelHash);
	static void ReleaseRenderTarget(atFinalHashString renderTargetName, u32 ownerHash, int& renderTargetId);

	static void	DrawToRenderTarget(CRenderTargetInfo const& renderTargetInfo);
	static void BlankRenderTarget(u32 renderTargetId);

	// Render Target handling stuff
	static void HandleRenderTargets();

	//////////////////////////////////
	// Stuff for linking audio
#define	MOVIE_AUDIO_TV_SEARCH_RADIUS (0.5f)
#define MOVIE_AUDIO_MAX_LINKAGE		(16)

	static void LinkMovieAudio(u32 movieNameHash);

	struct  ReplayClosestMovieObjectSearchStruct
	{
		CEntity *pEntity;
		u32		modelNameHashToCheck;
		float	closestDistanceSquared;
		Vector3 coordToCalculateDistanceFrom;	
	};
	static CEntity	*FindEntity(const u32 modelNameHash, float searchRadius, Vector3 &searchPos);
	static bool		GetClosestObjectCB(CEntity* pEntity, void* data);

	struct AudioLinkageInfo
	{
		u32		movieNameHash;
		bool	isFrontendAudio;
		u32		modelNameHash;
		Vector3 pos;
	};

	static	atFixedArray<AudioLinkageInfo,MOVIE_AUDIO_MAX_LINKAGE>	m_AudioLinkInfo;

	// Stuff for linking audio
	//////////////////////////////////

	/////////////////////////////////////////////
	static bool						m_bIsExiting;
	static bool						m_bDoneClear;
	static atArray<tMovieInfo>		m_MoviesThisFrameInfo;

	class CReplayDrawableSprite
	{
	public:
		CReplayDrawableSprite()
			: m_centreX(0.5f)
			, m_centreY(0.5f)
			, m_width(100.0f)
			, m_height(100.0f)
			, m_rotation(0.0f)
			, m_red(255)
			, m_green(255)
			, m_blue(255)
			, m_alpha(255)
			, m_doStereorize(false)
		{}
		atString	m_texDictionaryName;
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


	class CReplayDrawableMovie
	{
	public:
		CReplayDrawableMovie()
			: m_centreX(0.5f)
			, m_centreY(0.5f)
			, m_width(100.0f)
			, m_height(100.0f)
			, m_rotation(0.0f)
			, m_red(255)
			, m_green(255)
			, m_blue(255)
			, m_alpha(255)
			, m_movieHandle(0)
		{}

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
	};

	class CRenderTargetInfo
	{
	public:
		CRenderTargetInfo()
		{
			Reset();
		}

		void Reset()
		{
			m_renderTargetId = 0;
			m_renderTargetIndex = -1;
			m_isDrawingSprite = false;
			m_isDrawingMovie = false;
			m_ownerTag = 0;
			m_linkModelHash = 0;
			m_renderTargetRegistered = false;
			m_renderTargetLinked = false;

			m_movieDestThisFrameInfo.Reset();
		}
		int		m_renderTargetId;
		int		m_renderTargetIndex;
		atFinalHashString m_renderTargetName;
		u32		m_ownerTag;
		u32		m_linkModelHash;
		bool	m_renderTargetRegistered;
		bool	m_renderTargetLinked;

		MovieDestinationInfo	m_movieDestThisFrameInfo;

		bool	m_isDrawingSprite;
		CReplayDrawableSprite m_drawableSpriteInfo;

		bool	m_isDrawingMovie;
		CReplayDrawableMovie m_drawableMovieInfo;
	};

	static atArray<CRenderTargetInfo>	m_playbackRenderTargets;

	static atArray<int>			m_renderTargetRegistersToRecord;

	struct rtLink 
	{
		rtLink()
		{}
		rtLink(int mhk, int rt, u32 o)
			: modelHashKey(mhk)
			, renderTargetIndex(rt)
			, ownerID(o)
		{}
		int modelHashKey;
		int renderTargetIndex;
		u32 ownerID;

		bool operator==(rtLink const& other) const
		{
			return modelHashKey == other.modelHashKey;
		}
	};
	static atArray<rtLink>			m_renderTargetLinksToRecord;
	static atArray<CReplayDrawableSprite> m_spritesDrawnToRecord;

	static atArray<atFinalHashString> m_knownRenderTargets;
	static atArray<atFinalHashString> m_knownTextureDictionariesForSprites;

	static bool		m_usedForMovies;
};


class CPacketRegisterRenderTarget : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketRegisterRenderTarget(int renderTargetIndex);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool	HasExpired(const CEventInfoBase&) const;

	void PrintXML(FileHandle fileID) const	{	CPacketEvent::PrintXML(fileID);	CPacketEntityInfo::PrintXML(fileID);			}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_REGISTER_RENDERTARGET, "Validation of CPacketRegisterRenderTarget Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_REGISTER_RENDERTARGET;
	}

private:

	int			m_renderTargetIndex;
};


class CPacketLinkRenderTarget : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketLinkRenderTarget(u32 modelHashKey, int rtIndex, u32 ownerTag);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	bool	HasExpired(const CEventInfoBase&) const;

	void PrintXML(FileHandle fileID) const	{	CPacketEvent::PrintXML(fileID);	CPacketEntityInfo::PrintXML(fileID);			}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_LINK_RENDERTARGET, "Validation of CPacketLinkRenderTarget Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_LINK_RENDERTARGET;
	}

private:

	atHashString	m_modelHashKey;
	int			m_renderTargetIndex;
	u32			m_ownerTag;
};



#endif	//GTA_REPLAY

#endif	//__REPLAY_MOVIE_CONTROLLER_H__