//
// filename:	SocialClubNewsStoryMgr.h
// description:	
//

#ifndef INC_SOCIALCLUBNEWSSTORYMGR_H_
#define INC_SOCIALCLUBNEWSSTORYMGR_H_

#include "data/growbuffer.h"
#include "atl/string.h"

#include "frontend/Scaleform/ScaleFormMgr.h"
#include "Network/Cloud/CloudManager.h"
#include "scene/DownloadableTextureManager.h"
#include "text/TextFormat.h"


#define NEWS_TYPE_TRANSITION_HASH				ATSTRINGHASH("transition",			0xb9933991)
#define NEWS_TYPE_PRIORITY_HASH					ATSTRINGHASH("priority",			0x1470EDC0)
#define NEWS_TYPE_NEWSWIRE_HASH					ATSTRINGHASH("newswire",			0xad96ab93)
#define NEWS_TYPE_STORE_HASH					ATSTRINGHASH("store",				0x3ddbbb72)
#define NEWS_TYPE_ROCKSTAR_EDITOR_HASH			ATSTRINGHASH("rockstarEditor",		0x5f640870)
#define NEWS_TYPE_STARTERPACK_OWNED_HASH		ATSTRINGHASH("starterpackowned",	0x15220698)
#define NEWS_TYPE_STARTERPACK_NOTOWNED_HASH		ATSTRINGHASH("starterpacknotowned",	0x18F19804)

namespace rage
{
	class RonsReader;
};
class CNewsItemPresenceEvent;

enum eNewsImgType
{
	NEWS_IMG_TYPE_INVALID = -1,
	NEWS_IMG_TYPE_PORTRAIT,
	NEWS_IMG_TYPE_LANDSCAPE,
	MAX_CLOUD_IMAGES
};

class CNetworkSCNewsStoryRequest : public CloudListener
{

public:

	static const int MAX_NEWS_STORY_KEY_CHAR = 32;
	static const int MAX_NEWS_STORY_TYPES = 10;    //Maps to CNewsItemPresenceEvent::MAX_NUM_TYPES

	CNetworkSCNewsStoryRequest(const CNewsItemPresenceEvent& newNewsStory);
	~CNetworkSCNewsStoryRequest();

	void Update();

	void Cancel();

	bool IsIdle() const		  { return m_state == STATE_NONE; }
	bool IsWaiting() const    { return m_state == STATE_NONE || m_state == STATE_REQUESTED; }
	bool Pending() const	  { return m_state == STATE_WAITING_FOR_CLOUD || PendingImage(); }
	bool PendingImage() const { return m_state == STATE_WAITING_FOR_IMAGE; }
	bool ReceivedData() const { return m_state == STATE_DATA_RCVD || PendingImage() || Success(); }
	bool Success() const	  { return m_state == STATE_RCVD; }
	bool Failed() const		  { return m_state == STATE_ERROR || m_bFailedLastRequest; }

	bool IsReady() const	{ return !Pending() && Success(); }

	//For cloud file handling (when it comes in)
	virtual void OnCloudEvent(const CloudEvent* pEvent);

	const char* GetStoryKey() const { return m_newsStoryKey; }

	const char* GetTitle() const;
	const char* GetHeadline() const;	// Really the "footer" text
	const char* GetSubtitle() const;
	const char* GetContent() const;
	const char* GetTxdName() const;
	const char* GetURL() const;
	atHashString GetStyle() const;
	u32 GetTrackingId() const;
	int GetPriority() const { return m_iPriority; }
	int GetSecondaryPriority() const { return m_iSecondaryPriority; }

	bool HasExtraNewsData() const { return m_extraData.GetLength() > 0; }
	bool GetExtraNewsData(const char* name, int& value) const;
	bool GetExtraNewsData(const char* name, float& value) const;
	bool GetExtraNewsData(const char* name, char* str, int maxStr) const;
	template<int SIZE>
	bool GetExtraNewsData(const char* name, char (&buf)[SIZE]) const
	{
		return GetExtraNewsData(name, buf, SIZE);
	}
	bool HasExtraDataMember(const char* name) const;

	void Request(bool bDoImageRequest, eNewsImgType iImageType = NEWS_IMG_TYPE_PORTRAIT);
	void RequestImage(eNewsImgType m_iImageType = NEWS_IMG_TYPE_PORTRAIT);
	void MarkAsShown()	  { SetState(STATE_SHOWN); }
	void MarkForCleanup() { SetState(STATE_CLEANUP); }

	bool HasType(u32 typeHash) const;

protected:
	bool Start();
	bool HandleReceivedData();
	void SetSubtitle(const char* newSubtitle, int newSubtitleLen);
	bool ReadImageData (const RsonReader& imageRR, eNewsImgType iImageType);
	bool DoImageRequest();
	bool ShowNewsStory();
	bool GetExtraDataMember(const char* name, RsonReader& rr) const;

	void ReleaseTXDRef();

#if !__NO_OUTPUT
	void ReadNewsStoryFromFile(const char* filePath, const char* key);
#endif

	enum State {
		STATE_NONE,
		STATE_REQUESTED,
		STATE_WAITING_FOR_CLOUD,
		STATE_DATA_RCVD,
		STATE_WAITING_FOR_IMAGE,
		STATE_RCVD,
		STATE_SHOWN,
		STATE_CLEANUP,
		STATE_ERROR
	};

	void SetState(State newState);
	State m_state;
	CloudRequestID m_cloudReqID;
	datGrowBuffer m_gb;

	CTextureDownloadRequestDesc m_texRequestDesc;
	TextureDownloadRequestHandle m_texRequestHandle;
	strLocalIndex m_TextureDictionarySlot;

	char m_newsStoryKey[MAX_NEWS_STORY_KEY_CHAR];
	atFixedArray<atHashString, MAX_NEWS_STORY_TYPES> m_newsTypes;

	atString m_title;
	atString m_subtitle;
	atString m_content;
	atString m_url;
	atString m_headline;	// footer
	atString m_extraData;
	atHashString m_style;
	u32 m_trackingId; // This is the hash of the actual story key on ScAdmin used for telemetry

	char m_newStoryTxdName[64];
	struct ImageInfo
	{
		ImageInfo()
		{
			Clear();
		}

		void Clear()
		{
			m_cloudImagePresize = 0;
			m_cloudImagePath[0] = '\0';
		}

		char m_cloudImagePath[128];
		int m_cloudImagePresize;
	};

	ImageInfo m_cloudImage[MAX_CLOUD_IMAGES];

	bool m_bFailedLastRequest;
	bool m_bDoImageRequest;
	eNewsImgType m_iImageType;
	u32 m_ticksInShown;
	int m_iPriority;
	int m_iSecondaryPriority;
};

class CNetworkNewsStoryMgr : public CloudListener
{
public:
	CNetworkNewsStoryMgr() 
		: m_cloudReqID(INVALID_CLOUD_REQUEST_ID)
		, m_bRequestCloudFile(false)
		, m_CloudRequestCompleted(false)
	{}
	~CNetworkNewsStoryMgr() 
	{
		Shutdown();
	}

	static CNetworkNewsStoryMgr& Get();

	void Init();
	void Shutdown();
	void Update();

	bool AddNewsStoryItem(const CNewsItemPresenceEvent& newNewsStory);

	int GetIndexOfNextByType(int startIndex, u32 typeHash, int direction, bool bIdle = false) const;

	int GetNumStories() const { return m_requestArray.GetCount(); }
	int GetNumStories(int newsTypeHash) const;

	int GetIndexByType(int newsTypeHash, int iRequestIndex);

	CNetworkSCNewsStoryRequest* GetStoryAtIndex(int index) const;

	void DoCloudFileRequest();
	void RequestDownloadCloudFile();
	void ParseNews(const char* text, const int len);

	// True if we have finished or failed a cloud request at least once.
	bool HasCloudRequestCompleted();

#if !__NO_OUTPUT
	void ReadNewsFromFile(const char* filePath);
#endif

#if __BANK
	void InitWidgets(bkBank* bank);
	bool AddNewsStoryItem(const char* newsStoryKey);
	void Empty();
#endif

	// If true there are news for the priority feed
	// This is only valid if the profile saves have been loaded and if the news have been downloaded
	bool HasUnseenPriorityNews() const;

	// True if the story is a priority news
	static bool IsPriorityNews(const CNetworkSCNewsStoryRequest& news);

	// Call it to report that the player has seen the news
	void NewsStorySeen(const CNetworkSCNewsStoryRequest& news);

	// True if the player has seen this specific news story
	bool HasSeenNewsStory(const CNetworkSCNewsStoryRequest& news) const;

	// Returns how often the news has been seen
	int GetNewsStoryImpressionCount(unsigned hash, int& slotIdx) const;

	// The delay before the user can skip new news shown at landing page entry
	unsigned GetNewsSkipDelaySec();

	virtual void OnCloudEvent( const CloudEvent* pEvent );

private:

	atArray<CNetworkSCNewsStoryRequest*> m_requestArray;
	CloudRequestID m_cloudReqID;
	bool m_bRequestCloudFile;
	bool m_CloudRequestCompleted;
};

class PrioritizedList
{
public:
    enum eReservedPriority
    {
        UNSKIPPABLE_FEED = -1, 
    };

	PrioritizedList(){}

	struct PriorityItem
	{
		int uID;
		int iPriority;
		int iSecondaryPriority;
		int iCounter;	// Keeps track of when to add the item to the generated list
	};

	void SetPriorityFrequency(int iPriority, u8 iFrequency);
	bool AddEntry(int uID, int iPriority, int iSecondaryPriority);
	bool HasEntry(int uID) const;

	void RefreshListForNewEntries();
	void GenerateNextList();
	void InsertEntryIntoList(int uID, int iIndexToInsertAfter);

	int GetItemUIDAt(int iIndex)		const;
	int GetCount()						const { return generatedList.GetCount(); }
	int GetNumEntries()					const { return entries.GetCount(); }	
	void ResetEntries() { entries.Reset(); }
    void Clear();

private:
	atArray<int> generatedList;
	atArray<PriorityItem> entries;	// Unique item entries
	atMap<int, u8> frequencies;		// Map of priority -> frequency to show item

	u8 GetFrequency(int iPriority);
	PriorityItem* GetEntry(int uID);
};

namespace ReadingUtils
{
	u32 GetWordCount(const char* cText);
	u32 GetAverageAdultTimeToRead(const char* cText);
};

// *** NEWS CONTROLLERS ***
#define TRANSITION_STORY_MINIMUM_ONSCREEN_DURATION (12000)
#define MAX_STORY_PRIORITY (10)

class CNetworkTransitionNewsController
{
public: // declarations and variables
    enum eMode
    {
        MODE_SCRIPT_CYCLING,
        MODE_SCRIPT_INTERACTIVE,
        MODE_CODE_LOADING_SCREEN,
        MODE_CODE_INTERACTIVE,
        MODE_CODE_PRIORITY
    };

public:
	CNetworkTransitionNewsController()
        : m_iPendingRequestID(-1)
		, m_iLastRequestedID(-1)
		, m_iLastShownID(-1)
		, m_iLastPriorityID(-1)
        , m_requestDelta( 0 )
		, m_newsMovieID(SF_INVALID_MOVIE)
		, m_uLastDisplayedTimeMs(0)
		, m_seenNewsMask(0)
		, m_uStoryOnscreenDuration(0)
		, m_bIsActive(false)
		, m_bIsPaused(false)
		, m_bRequestShow(false)
		, m_bIsDisplayingNews(false)
		, m_prioritizedNews()
	{
		// Special case for a 0 priority, which appears at the same frequency as a 1 priority, but always appears first
		m_prioritizedNews.SetPriorityFrequency(0, 1);

		// Define priority frequencies
		for(u8 i = 1; i <= MAX_STORY_PRIORITY; ++i)
		{
			m_prioritizedNews.SetPriorityFrequency(i, i);
		}
	}

	~CNetworkTransitionNewsController() {}

	static CNetworkTransitionNewsController& Get();

	void Update();

	bool InitControl( s32 const movieID, eMode const interactivityMode );
	void Reset();
	bool SkipCurrentNewsItem();
    bool ReadyNextRequest( bool const requestShow );
    bool ReadyPreviousRequest( bool const requestShow );
    bool ReadyRequest( int const delta );
    int GetNewsItemCount() const;
    void ResetPrioritizedNews();
	void UpdatePrioritizedNewsList();

	// Active means the news has been initialized via InitControl
	bool IsActive() const { return m_bIsActive; }

	// Is the news controller paused?
	bool IsPaused() const { return m_bIsPaused; }
	void SetPaused(bool value);

	bool PostNews(const CNetworkSCNewsStoryRequest* pNewsItem, bool bPostImg);
	void ClearNewsDisplay();
	void UpdateDisplayConfig();
	void SetAlignment(bool bAlignRight);

	// This is to get and clear the stories currently seen on screen, not for persistent the player save data.
	u32 GetSeenNewsMask() const;
	void ClearSeenNewsMask();

	bool IsDisplayingNews() const { return m_bIsDisplayingNews; }	// Has data actually been posted to the movie yet
	bool HasValidNewsMovie() const { return m_newsMovieID != SF_INVALID_MOVIE; }
	bool ShouldAutoCycleStories() const { return m_interactivityMode == MODE_CODE_LOADING_SCREEN || m_interactivityMode == MODE_SCRIPT_CYCLING; }
    bool ShouldSkipScriptNews() const { return m_interactivityMode == MODE_CODE_LOADING_SCREEN; }
    bool IsLegacyMode() const { return m_interactivityMode != MODE_CODE_PRIORITY && m_interactivityMode != MODE_CODE_INTERACTIVE; }
    bool IsInteractiveMode() const { return m_interactivityMode == MODE_CODE_INTERACTIVE; }
    bool IsPriorityMode() const { return m_interactivityMode == MODE_CODE_PRIORITY; }
    u32 GetActiveNewsType() const;

	bool CurrentNewsItemHasExtraData();
	bool GetCurrentNewsItemExtraData(const char* name, int& value);
	bool GetCurrentNewsItemExtraData(const char* name, float& value);
	bool GetCurrentNewsItemExtraData(const char* name, char* str, int maxStr);
	template<int SIZE>
	bool GetCurrentNewsItemExtraData(const char* name, char (&buf)[SIZE])
	{
		return GetCurrentNewsItemExtraData(name, buf, SIZE);
	}
	const char* GetCurrentNewsItemUrl();
	u32 GetCurrentNewsItemTrackingId();
	bool HasCurrentNewsItemDataOrIdle();


#if __BANK
	static void InitWidgets();
	static void Bank_CreateTransitionNewsWidgets();
	static void Bank_RenderDebugMovie();
	static void Bank_LoadDebugNewsMovie();
	static void Bank_BeginNewsCycle();
	static void Bank_StopNewsDisplay();
	static void Bank_ShowNewsItem(s32 key);

	void Bank_FillCompleteNewsList();
	void Bank_ClearPrioritizedNewsListDisplay();
	void Bank_FillPrioritizedNewsListDisplay();
#endif

    s32 GetNewsMovieId() const { return m_newsMovieID; }

private:
    int m_iPendingRequestID;
    int m_iLastRequestedID;
    int m_iLastShownID;
    int m_iLastPriorityID;
    int m_requestDelta;
    s32 m_newsMovieID;
    u32 m_uStoryOnscreenDuration;
    u32 m_uLastDisplayedTimeMs;
    u32 m_seenNewsMask;
    eMode m_interactivityMode;

	bool m_bIsActive;
	bool m_bIsPaused;
	bool m_bRequestShow;
	bool m_bIsDisplayingNews;

#if __BANK
	static s32 ms_iDebugNewsMovieID;
	static bkList* ms_pFullNewsList;
	static bkList* ms_pPrioritizedNewsList;
	static int ms_iDebugStoryRequestID;
#endif

	PrioritizedList m_prioritizedNews;

	CNetworkSCNewsStoryRequest* GetPendingStory();
	CNetworkSCNewsStoryRequest* GetLastShownStory();
	bool MakeStoryRequestAt(int iRequestedID);
	bool IsNewsStoryScriptDriven(const CNetworkSCNewsStoryRequest* newsItem) const;

    void ReadyAllRequests();

	bool HasPendingStory();
	bool IsPendingStoryDataRcvd();
	bool IsPendingStoryReady();
	bool HasPendingStoryFailed();
	void MarkPendingStoryAsShown();
	void ClearPendingStory();

	int GetNextIDByPriority();
    int GetPreviousIDByPriority();
    int GetIDByPriority( int const delta );
};

// Pause Menu Newswire and Store Controller
class CNetworkPauseMenuNewsController
{
public:
	CNetworkPauseMenuNewsController()
		: m_newsTypeHash(0)
		, m_iLastRequestedID(-1)
		, m_iDirectionToShow(0)
		, m_imageTypeToRequest(NEWS_IMG_TYPE_LANDSCAPE)
		{}
	  ~CNetworkPauseMenuNewsController() {}

	  static CNetworkPauseMenuNewsController& Get();

	  bool InitControl(u32 newsTypeHash);
	  void Reset();

	  int GetNumStories();

	  bool RequestStory(int iDirection);

	  CNetworkSCNewsStoryRequest* GetPendingStory() const;
	  void MarkPendingStoryAsShown();

	  bool IsPendingStoryDataRcvd();
	  bool IsPendingStoryReady() const;
	  bool HasPendingStoryFailed();
	  int GetPendingStoryIndex();

protected:
	bool MakeStoryRequestAt(int iRequestedID);
	bool HasPendingStory() const;
	void ClearPendingStory();

	u32 m_newsTypeHash;

	int m_iLastRequestedID;
	int m_iDirectionToShow;

	eNewsImgType m_imageTypeToRequest;
};

#endif // !INC_SOCIALCLUBNEWSSTORYMGR_H_
