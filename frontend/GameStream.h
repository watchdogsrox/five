/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : GameStream.h
// PURPOSE : handles the Scaleform hud live info/updates
//
// See: http://rsgediwiki1/wiki/index.php/HUD_Game_Stream for reference.
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef __GAMESTREAM_H__
#define __GAMESTREAM_H__

#include "atl/string.h"
#include "atl/array.h"
#include "basetypes.h"
#include "grcore/setup.h"
#include "grcore/image.h"
#include "grcore/texturecontrol.h"
#include "grcore/texture.h"
#include "frontend/hud_colour.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/mapdatastore.h"
#include "data/growbuffer.h"
#include "Network/Live/NetworkClan.h"

#include "scene/DownloadableTextureManager.h"

#define GAMESTREAM_MAX_SHOWING		5		// Maximum number of posts we can display at once.
#define GAMESTREAM_MAX_PARAMS		16
#define GAMESTREAM_MAX_POSTS		32
#define GAMESTREAM_MAX_TEXTURES		32
#define GAMESTREAM_MAX_COMBINED		1024*40	// Downloadable texture cache limit. ( 40k )
#define GAMESTREAM_MAX_BUSY_TXT		128
#define GAMESTREAM_SPINNER_BACKOFF	6

#define GAMESTREAM_BACKOFF_TIME		200		// ms to wait between scaleform updates.
#define GAMESTREAM_SETTLE_FRAMES	30		// The gs movie has to be enabled for this many frames before calling any of it's methods.
#define GAMESTREAM_LINGER_TIME		1000	// ms to linger around before the post's slot is freed.
#define GAMESTREAM_MS_PERCHAR		25
#define GAMESTREAM_ADD_WAIT_COUNT	10		// Number of updates to back off when an item failed to display
#define GAMESTREAM_MAX_FAILCOUNT	25		// Number of fails before giving up and removing the item.

#define FEED_CACHE_MAX_SIZE			10
#define MAX_ALLOWED_WHILE_PAUSED	2

//#define	GAMESTREAM_MAX_TOOLTIP_ITEM	84		// B* 1216115 TOOLTIPS_0 to TOOLTIPS_84
#define GAMESTREAM_GAMETIP_RANDOM	-1
#define GAMESTREAM_GAMETIP_CYCLE_TIME		11000	// One every 11 seconds.
#define GAMESTREAM_GAMETIP_HISTORY_MAX		16
#define GAMESTREAM_GAMETIP_HISTORY_UNSET	-1

// Time on screen (ms) for each stream type.
#define GAMESTREAM_SHOWTIME_DEFAULT			10000
#define GAMESTREAM_SHOWTIME_MESSAGE_TEXT	13000

#define GAMESTREAM_AWARD_POST_COOLDOWN		2000

#define GAMESTREAM_MAX_LIFETIME				(60*1000)*5	// Used to remove feed items that have been in the queue for longer than 5 minutes. B* 1061358

enum eDisplayState
{
	DISPLAY_STATE_INVALID = -1,
	DISPLAY_STATE_MAP_OFF,
	DISPLAY_STATE_MAP_SMALL,
	DISPLAY_STATE_MAP_LARGE,
	DISPLAY_STATE_LOADING_SCREEN_LOGO,
	DISPLAY_STATE_ONLINE_LOGO
};

#define GAMESTREAM_SHOWCOUNT_FOREVER	0x0fffffff

// Message Text Variables
#define MESSAGE_TEXT_TYPE_MSG		1
#define MESSAGE_TEXT_TYPE_EMAIL		2
#define MESSAGE_TEXT_TYPE_INV		7

enum messageTextArgs
{
	MESSAGE_TEXT_PARAM_TXD = 1,
	MESSAGE_TEXT_PARAM_TXN = 2,
	MESSAGE_TEXT_PARAM_TYPE = 4
};


enum
{
	GAMESTREAM_SHOW_IDX_EXT1=0,
	GAMESTREAM_SHOW_IDX_EXT2,
	GAMESTREAM_SHOW_IDX_EXT3,
	GAMESTREAM_SHOW_IDX_EXT4,
	GAMESTREAM_SHOW_IDX_INT1,
	GAMESTREAM_SHOW_FLAG_COUNT
};


enum gstPostStatus
{
	GAMESTREAM_STATUS_FREE=0,
	GAMESTREAM_STATUS_NEW,
	GAMESTREAM_STATUS_STREAMING_TEXTURE,
	GAMESTREAM_STATUS_DOWNLOADINGTEXTURE,
	GAMESTREAM_STATUS_REQUESTING_CREW_EMBLEM,
	GAMESTREAM_STATUS_READYTOSHOW,
	GAMESTREAM_STATUS_SHOWING,
	GAMESTREAM_STATUS_READYTOREMOVE,
	GAMESTREAM_STATUS_CLEANUP
};

enum gstPostType
{
	GAMESTREAM_TYPE_INVALID = -1,
	// PLEASE MATCH WITH ACTIONSCRIPT "\gta5\art\UI\Actionscript\com\rockstargames\gtav\levelDesign\GAME_STREAM_ENUMS.as"
	GAMESTREAM_TYPE_MESSAGE_TEXT,
	GAMESTREAM_TYPE_STATS,
	GAMESTREAM_TYPE_TICKER,
	GAMESTREAM_TYPE_AWARD,
	GAMESTREAM_TYPE_CREW_TAG,
	GAMESTREAM_TYPE_UNLOCK,
	GAMESTREAM_TYPE_TOOLTIPS,
	GAMESTREAM_TYPE_CREW_RANKUP,
	GAMESTREAM_TYPE_VERSUS,
	GAMESTREAM_TYPE_REPLAY,
	// PLEASE MATCH WITH ACTIONSCRIPT "\gta5\art\UI\Actionscript\com\rockstargames\gtav\levelDesign\GAME_STREAM_ENUMS.as"
	GAMESTREAM_TYPE_COUNT
};

enum gstPostScaleFormMethod
{
	GAMESTREAM_METHOD_SET,
	GAMESTREAM_METHOD_REMOVE,
	GAMESTREAM_METHOD_UPDATE
};

enum
{
	// matches eICON_STYLE
	GAMESTREAM_BUSYSPINNER_ICON_FUTURE1=0,	
	GAMESTREAM_BUSYSPINNER_ICON_SAVING,
	GAMESTREAM_BUSYSPINNER_ICON_FUTURE2,
	GAMESTREAM_BUSYSPINNER_ICON_FUTURE3,
	GAMESTREAM_BUSYSPINNER_ICON_CLOUDACCESS
};

struct gstPostStaticInfo
{
	gstPostType m_postType;
	const char* m_typeName;
	const char* m_UpdateMethod;
	u32			m_ShowTime;
};

struct gstImportantParameters
{
	u8	r;
	u8	g;
	u8	b;
	u8	a;
	u8	flash;
	bool vibrate;
};


struct stFeedItem
{
	stFeedItem()
	{
		Clear();
	}

	void Clear()
	{
		iType = GAMESTREAM_TYPE_INVALID;

		sTitle.Clear();
		sSubtitle.Clear();
		sBody.Clear();
		sTXD.Clear();
		sTXN.Clear();
		sTXDLarge.Clear();
		sTXNLarge.Clear();
		sCloudPath.Clear();
		sCrewTag.Clear();
	
		iScriptID = -1;
		iIcon = 0;
		eColour= HUD_COLOUR_PURE_WHITE;
		bDynamicTexture = false;
		m_clanId = RL_INVALID_CLAN_ID;
		
		m_stat.Clear();
	}

	struct stStatItem
	{
		atString sName;
		int iProgress;
		int iProgressIncrease;

		void Clear()
		{
			sName.Clear();
			iProgress = 0;
			iProgressIncrease = 0;
		}
	};

	gstPostType iType;

	atString sTitle;
	atString sSubtitle;
	atString sBody;
	atString sTXD;
	atString sTXN;
	atString sTXDLarge;
	atString sTXNLarge;
	atString sCloudPath; //cloud path for the file that needs to be requested.
	atString sCrewTag;

	int iScriptID;
	int iIcon;
	eHUD_COLOURS eColour;
	bool bDynamicTexture;
	rlClanId m_clanId; //For crew related messages when you need to grab the emblem
	stStatItem m_stat;	// Optional Stat item for "GAMESTREAM_TYPE_STATS"
};

class CGameStream
{
public:

	CGameStream();
	~CGameStream();

	class gstPost
	{
	public:
		gstPost()
			: ParamsCount(0)
		{
			Reset();
		}
		
		gstPostType			Type;
		gstPostStatus		Status;
		
		const gstPostStaticInfo*	m_pStaticData;
		gstImportantParameters m_ImportantParameters;
		bool IsImportant;

		int		Id;
		int		IdScript;
		int		ShowTime;
		int		CreatedTime;
		int		iReadyToShowTime;		// When this item was marked ReadyToShow
		int		AlarmTime;
		int		FailedCount;
		u32		Sequence;
		
		const char* GetTypeName() const;
		const char* GetUpdateMethod() const;

		bool IsFrozen() const		{ return m_bIsFrozen; }
		void SetFrozen(bool value)	{ m_bIsFrozen = value; }
	
		int		ParamsCount;
		atString Params[GAMESTREAM_MAX_PARAMS];

		bool	m_bDynamicTexture;
		bool	m_bCacheMessage;
		bool	m_bAlwaysRender;
		bool	m_bSnapIntoPosition;
		bool	m_bShouldUpdate;

		bool	m_bChangeBgColor;
		eHUD_COLOURS m_eBgColor;

		CTextureDownloadRequestDesc m_requestDesc;
		TextureDownloadRequestHandle m_requestHandle;
		strLocalIndex m_TextureDictionarySlot;
		atString m_cloudDownloadpath;

		rlClanId m_clanId;
		bool	 m_bClanEmblemRequested;

		bool	RequestTextureForDownload(const char* textureName,const char* filePath);
		bool    UpdateTextureDownloadStatus();
		void	ReleaseAndCancelTexture();

		bool	AddStringParamWithTokens(const char* Data);
		bool	AddStringParamAndParseGamerNameSpaces( const char* Data );
		bool	AddParam( const char* Data	ASSERT_ONLY(, int paramCountToVerify = -1) );
		bool	AddParam( int Data			ASSERT_ONLY(, int paramCountToVerify = -1) );
		bool	AddParam( float Data );
		bool	AddParam( bool Data );
		bool	AddParam( const char* Data, const char* Prefix );

#if __ASSERT
		void	VerifyParamCount(int paramCountToVerify);
#endif // __ASSERT

		void	SetParam( int i, int Data );
		void	SetParam( int i, float Data );
		void	SetParam( int i, bool Data );
		void	SetParam( int i, const char* Data);
		bool	SetFormattedParam( int i, const char* FormattedData );

		void	SetPostImportantParams( int r, int g, int b, int a, int flash, bool vibrate);

		void Reset()
		{
			m_pStaticData = NULL;
			Status = GAMESTREAM_STATUS_FREE;
			IsImportant = false;
			Id = -1;
			IdScript = 0;
			Type = GAMESTREAM_TYPE_INVALID;
			ShowTime = 0;
			AlarmTime = 0;

			m_requestHandle = CTextureDownloadRequest::INVALID_HANDLE;
			m_TextureDictionarySlot = -1;
			m_cloudDownloadpath.Clear();

			m_clanId = RL_INVALID_CLAN_ID;
			m_bClanEmblemRequested = false;
			

			for(int i = 0; i < GAMESTREAM_MAX_PARAMS; ++i)
			{
				Params[i].Clear();
			}
			ParamsCount = 0;
			m_bDynamicTexture = false;
			m_bCacheMessage = true;
			m_bAlwaysRender = false;
			m_bSnapIntoPosition = false;
			m_bChangeBgColor = false;
			m_eBgColor = HUD_COLOUR_INGAME_BG;
			m_bShouldUpdate = false;

			Sequence = 0;

			m_bIsFrozen = false;
		}

	private:
		bool	m_bIsFrozen;

#if !__FINAL
		void DebugPrint();
#endif
	};

	struct gstPs
	{
		float	x;
		float	y;
		float	w;
		float	h;
	};

	enum	eGameStreamTipType
	{
		TIP_TYPE_ANY,
		TIP_TYPE_SP,
		TIP_TYPE_MP,
		TIP_TYPE_REPLAY
	};

	enum eFeedReplayState
	{
		DIRECTOR_RECORDING,
		BUTTON_ICON,
		ACTION_REPLAY,
		BANK_ONLY(MAX_REPLAY_STATES)
	};

	enum eFeedReplayRecordIcon
	{
		BUFFER_ICON,
		START_STOP_ICON
	};

#if __BANK
	void	BankLogRenderState(bool bRenderThisFrame, const char* reason);
	void	SetShowDebugBoundsEnabled(s32 iMovieId, bool bIsEnabled, bool bForceToScaleform = false);
#endif
	void	Update();
	bool	ShouldRender(bool bUpdateShowCounters = true);
	bool	IsScriptHidingThisFrame() const;

	// Msg:				TEXT MESSAGE Message String
	// ContactTxD		TEXT MESSAGE contact, texture dictionary string
	// ContactTxN		TEXT MESSAGE contact, texture name 
	int		PostMessageText( const char* Msg, const char* ContactTxD, const char* ContactTxN, bool IsImportant, int Icon, const char* CharacterName, const char* Subtitle, float timeMultiplier = 1.0f, const char* CrewTagPacked = "", int Icon2 = 0, int replaceId = -1, int iNameColor = HUD_COLOUR_WHITE);

    // Title:			STATS Title String
    // Body:			STATS Body String (use: "NEW ABILITY" if there's no bar or leave blank if there is a bar)
    // Icon:			STATS icon ENUM
    // LevelTotal:		Total level segments (e.g a bar split in 10 = 10)
    // LevelCurrent:	Current level value (e.g number 2 in a bar split in 10) 
	int		PostStats( const char* Title, const char* Body, int LevelTotal, int LevelCurrent, bool IsImportant, const char* ContactTxD, const char* ContactTxN );

	// Body:			TICKER Body String
	int		PostTicker( const char* Body, bool IsImportant, bool bCacheMessage = true, bool bHasTokens = false, int numIconFlashes = 0, bool bAlwaysRender = false );

	// TopLine:			TICKER Top line string
	// Body:			TICKER Body String
	int		PostTickerF10( const char* TopLine, const char* Body, bool IsImportant, bool bCacheMessage = true );
	int		PostAward( const char* AwardName, const char* TxD, const char* TxN, int iXP, eHUD_COLOURS eAwardColour, const char* Title);

	// IsPrivate		is private flag (false = public, true = private)
	// ShowLogoFlag		Rockstar logo flag (false = no R*, true = R* shown)
	// CrewString		Crew String (four alphanumeric characters)
	// CrewRank			Rank (0 to 6)
	// FounderStatus	Founder status (if true, replaces rank symbols with founder symbol)
	// Body				Body String
	// IsImportant		Item Important param. Flashes the component if set to true
	int		PostCrewTag( bool IsPrivate, bool ShowLogoFlag, const char* CrewString, int CrewRank, bool FounderStatus, const char* Body, bool IsImportant, rlClanId crewId, const char* GameName, Color32 CrewColour);
	int		PostUnlock(const char* chTitle, const char* chSubtitle, int iIconType, bool bIsImportant, eHUD_COLOURS eTitleColour);
	int		PostTooltips( const char* Body, bool const IsImportant, eGameStreamTipType const type);
	int		PostCrewRankup(const char* cTitle, const char* cSubtitle, const char* cTXD, const char* cTXN, bool bIsImportant);
	int		PostVersus(const char* ch1TXD, const char* ch1TXN, int val1, const char* ch2TXD, const char* ch2TXN, int val2, eHUD_COLOURS iCustomColor1 = HUD_COLOUR_INVALID, eHUD_COLOURS iCustomColor2 = HUD_COLOUR_INVALID);
	int		PostReplay(eFeedReplayState replayState, const char* cTitle, const char* cSubtitle = "", int iIcon = 0, float fPctComplete = 0.0f, bool bFlashBufferFull = false, const char* cIcon = "");

	bool	GetTextureArgs(gstPostType postType, int& txdArg, int& txnArg);
	void	UpdateFeedItemTexture(const char* cOldTXD, const char* cOldTXN, const char* cNewTXD, const char* cNewTXN);

	void	BusySpinnerOn( const char* Body, int Icon );
	void	BusySpinnerOff();
	bool	IsBusySpinnerOn();

	void	FlushQueue(bool bContinueToShow = false);
	void	ForceFlushQueue();

	void	ForceRenderOn();
	void	ForceRenderOff();
	bool	IsForceRenderOn() const;
	
	void	Pause( void );
	void	Resume( void );
	bool	IsPaused() const { return m_IsPaused; }
	void	RemoveItem( int Id );
	void	ClearCache();

	int		GetLastShownPhoneActivatableScriptID();

	void	Hide( void );
	void	Show( void );

	void	HideForUpdates( int updates );

	void	HideThisUpdate( void );

	bool	PostGametip( s32 gameTip, eGameStreamTipType tipType);

	bool	IsShowEnabled() const;
	bool	GetRenderEnabledGlobal();
	void	SetRenderEnabledGlobal( bool IsEnabled );

	void	ReportLogoIsOn();
	void	ReportLogoIsOff();
		
	// Global enable/disable for the gamestream system. ( default: enabled )
	void	EnableGlobal( bool Enable );

	// Enable/disable a feed type, eg GAMESTREAM_TYPE_DLC, GAMESTREAM_TYPE_FRIENDS
	void	EnableFeedType( int FeedType, bool Enable );

	void	EnableAllFeedTypes( bool Enable );
	
	void	StreamItemFailed( s32 Index );
	bool	InteractWithScaleform( s32 MovieId );
	int		CountShowing() const;
	int		CountQueued() const;
	void	ReportMinimapPosAndSize( float x, float y, float w, float h );
	void	GetMinimapPosAndSize( float* x, float* y, float* w, float* h );

	void	SetDisplayConfigChanged( void );
	void	SetMinimapState( eDisplayState MinimapState );

	void	SetHelptextHeight( float Height );
	void	SetScriptedMenuHeight( float Height );
	bool	UpdateCheckHelpTextHeight( s32 MovieId );
	void	ReportHelptextStatus( Vector2 Pos, Vector2 Size, bool IsActive );

	bool	ChangeMovieParams();
	
	void	SetIsOnLoadingScreen( bool IsOn );
	bool	IsOnLoadingScreen();
	
	void	SetImportantParamsRGBA( int bgR, int bgG, int bgB, int alphaFlash );
	void	SetImportantParamsFlashCount( int flashCount );
	void	SetImportantParamsVibrate( bool vibrate );
	void	SetImportantParams( int bgR, int bgG, int bgB, int alphaFlash, int flashRate, bool vibrate );
	void	FreezeNextPost();
	void	ClearFrozenPost();
	void	SetSnapFeedItemPositions(bool value) { m_bSnapFeedItemPositions = value;}
	void	SetNextPostBackgroundColor( eHUD_COLOURS color );

	void	ResetImportantParams();
	void	DefaultHidden();
	void	AutoPostGameTipUpdate();
	void	AutoPostGameTipOn(eGameStreamTipType tipType = TIP_TYPE_ANY);
	void	AutoPostGameTipOff();
	void	SetTipsLast( int gameTipLast, int onlineTipLast );
	void	ResetPauseMenuCounter() { m_iPauseMenuCount = 0; }
	void	JustReloaded();
	void	HandleShownStreamItem(int Id, int contentStrLen);
   
	const atQueue<stFeedItem, FEED_CACHE_MAX_SIZE>& GetFeedCache() const { return m_FeedCache; }

private:

	u32		GetPostDefaultShowTimeAtIndex(int i);
	void	ProcessPost( int Idx, gstPost* Post );

	
	gstPost* AllocPost( gstPostType Type, int ShowTime, bool bImportant = false);
	void	FreePost( int Idx );
	bool	CanUseScaleform();
	bool	HasAlwaysRenderFeedItem();
	bool	IsTypeShowing( int Type );
	void	SetAlarm( int Idx, int Ms );
	bool	IsAlarm( int Idx );
	int		FindParamType( gstPost* Post, char Type );

	void	CacheMessage(stFeedItem &feedItem);

	int		UpdateShouldRenderCounters();
	int		GetShouldRenderCount();

	bool	PostToScaleform( s32 MovieId, gstPost* Post, gstPostScaleFormMethod Method );
	void	AddInnerScaleformParams(gstPost* post);
//	bool	ReportMinimapSizePosToScaleform( s32 MovieId, int ContainerComponentId, gstPs* Ps );

	void	SetPostChangeBgColor( gstPost* post );

	bool	MakeTextureSpace( int RequiredSize );
	bool	IsShowing( int Idx ) const;
	bool	StrToBool( const char* Str ) const;
	int		MsFromLength( const char* Str ) const;
	int		MsFromLength( int strLen ) const;
	void	PsSet( gstPs* Ps, float x, float y, float w, float h );
	bool	PsCmp( gstPs* Ps1, gstPs* Ps2 );
	void	PsCpy( gstPs* Dst, gstPs* Src );

	void	GetNext( int* ReadyToShow, int* ReadyToRemove, int* ReadyToUpdate );
	bool	CanShowPost(const gstPost& post) const;
	bool	CheckUpdate( int& ReadyToShowIdx );
	bool	IsPostReadyToShow(gstPost* Post);

	bool	HandleMatchingStatsPost( int& ReadyToShowIdx );
	bool	HandleMatchingTickerPost( int& ReadyToShowIdx );
	bool	HandleMatchingMessageTextPost( int& ReadyToShowIdx );
	int		GetReplayDirectorPost();

	void	BusySpinnerUpdate();
	int		UpdateRecordingIcon( int PostIdx, int iIcon, float fPctComplete, bool bBufferFilled );
	bool	UpdateCheckDisplayConfig( s32 MovieId );
	bool	UpdateCheckMinimap( s32 MovieId );
	void	UpdateMinimapState();
	
	bool	InteractWithScaleformFast( s32 MovieId );
	bool	InteractWithScaleformSlow( s32 MovieId );

	bool	IsSafeGameDisplayState( void );
	void	SetImportantParamsScaleform( s32 MovieId, gstImportantParameters* importantParameters);
	bool	UpdateShowHide( s32 MovieId );
	int		CreateNewId();
	int		ConvertToIndex( int Id );
	
	int		GetNextRandomGameTip( eGameStreamTipType tipType  );
    
    bool				m_IsActive;
	bool				m_IsPaused;
	int					m_DisableAdd;
	u32					m_CurrentTime;
	u32					m_CurrentTimeOffset;
	u32					m_TimePausedAt;
	u32					m_CurrentSystemTime;
	u32					m_BackoffTime;
	u32					m_Sequence;

	bool				m_BusySpinnerOn;
	bool				m_BusySpinnerOnCmp;
	bool				m_BusySpinnerUpd;
	char				m_BusySpinnerTxt[GAMESTREAM_MAX_BUSY_TXT+1];
	int					m_BusySpinnerIcon;
	int					m_BusySpinnerBackoffFrames;

	bool				m_DisplayConfigChanged;
	bool				m_ChangeMovieParams;
	eDisplayState		m_MinimapState;
	eDisplayState		m_MinimapStateCmp;

	float				m_HelptextHeight;
	float				m_HelptextHeightCmp;

	float				m_ScriptedMenuHeight;
	float				m_ScriptedMenuHeightCmp;

	bool				m_IsOnLoadingScreen;

	gstPost				m_PostList[GAMESTREAM_MAX_POSTS];
	
	gstPs				m_MinimapNew;
	gstPs				m_MinimapCur;
	bool				m_EnableGlobal;
	bool				m_EnableList[GAMESTREAM_TYPE_COUNT];
	int					m_ShowCounters[GAMESTREAM_SHOW_FLAG_COUNT];	// So independent systems can show/hide the feed.
	sysTimer			m_SysTimer;


//	bool				m_HideUntilFlushComplete;	// Stops rendering until all items have been removed from the queue after FlushQueue() 
	bool				m_IsLogoVisible;

	// These values are copied from here into gstPost when a feed item is posted and the important flag is set.
	int					m_ImportantParamsbgR;
	int					m_ImportantParamsbgG;
	int					m_ImportantParamsbgB;
	int					m_ImportantParamsalphaFlash;
	int					m_ImportantParamsflashRate;

	// This value is sent to Scaleform through SET_NEXT_FEED_POST_BACKGROUND_COLOR after SetBackgroundColorForNextPost() is called
	eHUD_COLOURS		m_bgColor;

	bool				m_ImportantParamsvibrate;
	bool				m_IsForcedRender;
	bool				m_IsShowing;
	bool				m_AutoPostGameTip;
	bool				m_HasShownFirstAutoPostedGameTip;
	bool				m_bFreezeNextPost;
	bool				m_bSnapFeedItemPositions;
	bool				m_bSetFeedBackgroundColor;
	eGameStreamTipType	m_AutoPostGameTipType;
	int					m_IdCounter;
	int					m_GameTipLast;
	int					m_OnlineTipLast;

	int					m_iPauseMenuCount;

	// Last activatable phone ID being shown
	int					m_iLastShownPhoneActivatableID;

	sysTimer			m_GameTipTimer;

	int					m_GameTipHistoryIndex;
	int					m_GameTipHistory[GAMESTREAM_GAMETIP_HISTORY_MAX];
	
	atQueue<stFeedItem, FEED_CACHE_MAX_SIZE> m_FeedCache;


#if __BANK
	bool m_bRenderedLastFrame;
	char m_previousRenderStateReason[256];
	bool m_bShowDebugBounds;
#endif
};

#endif // __GAMESTREAM_H__
