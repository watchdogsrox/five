#ifndef __REPLAY_MOVIE_CONTROLLER_H__
#define __REPLAY_MOVIE_CONTROLLER_H__

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/Misc/MoviePacket.h"
#include "control/replay/Misc/MovieTargetPacket.h"

class ReplayMovieController
{
public:
	typedef ReplayMovieInfo<MAX_REPLAY_MOVIE_NAME_SIZE>	tMovieInfo;

	static void Init();

	static void	OnEntry();
	static void OnExit();

	static void	Process(bool bPlaying);

	static void	SubmitMoviesThisFrame(const atFixedArray<tMovieInfo, MAX_ACTIVE_MOVIES> &info);
	static void SubmitMovieTargetThisFrame(const MovieDestinationInfo &info);

	static void SubmitSpriteThisFrame(const char* texDictName, const char* textName, float centreX, float centreY, float width, float height, float rotation, int r, int g, int b, int a, bool doStereorize);

	static void	RegisterMovieAudioLink(u32 modelNameHash, Vector3 &TVPos, u32 movieNameHash, bool isFrontendAudio);

	static void OnRegisterNamedRenderTarget(const char *name/*, bool delay*/);
	static void OnReleaseNamedRenderTarget(const char *name);
	static void OnLinkNamedRenderTarget(int ModelHashKey, u32 /*ownerID*/);
	static void OnDrawSprite(const char *pTextureDictionaryName, const char *pTextureName, float CentreX, float CentreY, float Width, float Height, float Rotation, int R, int G, int B, int A, bool DoStereorize);
	static void Record();

	static atArray<atFinalHashString> const&	GetKnownRenderTargets() { return m_knownRenderTargets; }

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
	static void ReleaseRenderTarget(const char* renderTargetName, u32 ownerHash);

	static void	DrawMovieToRenderTarget();
	static void BlankRenderTarget();

	// Render Target handling stuff
	static void HandleRenderTargets();

	static u32	GetOwnerHash() { return atStringHash("REPLAY_TV"); }

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
	static MovieDestinationInfo		m_MovieDestThisFrameInfo;
	static u32						m_RenderTargetId;
	static u32						m_CurrentTVModel;
	static const char				*pRenderTargetName;

	static atString					m_renderTargetName;
	static bool						m_hasRegistered;
	static int						m_linkToModelHashKey;

	static bool						m_isDrawingSprite;
	static atString					m_texDictionaryName;
	static atString					m_textureName;
	static float					m_centreX;
	static float					m_centreY;
	static float					m_width;
	static float					m_height;
	static float					m_rotation;
	static int						m_red;
	static int						m_green;
	static int						m_blue;
	static int						m_alpha;
	static bool						m_doStereorize;

	static atArray<atFinalHashString> m_knownRenderTargets;
	static bool						m_usedForMovies;
};

#endif	//GTA_REPLAY

#endif	//__REPLAY_MOVIE_CONTROLLER_H__