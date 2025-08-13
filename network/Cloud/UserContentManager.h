#ifndef __USER_CONTENT_MANAGER_H__
#define __USER_CONTENT_MANAGER_H__

// rage includes
#include "atl/singleton.h"
#include "data/callback.h"
#include "net/status.h"
#include "network/Cloud/CloudManager.h"
#include "rline/ugc/rlugc.h"
#include "script/thread.h"

// game includes
namespace rage
{
	class bkBank;
	class rlCloudMemberId;
	struct rlYoutubeUpload;
}

// sorting
enum eUgcSortCriteria
{
    eSort_NotSpecified = -1,
    eSort_ByCreatedDate,
    eSort_Max,
};

enum eUgcMissionFiles
{
	eFile_Mission,
	eFile_LoResPhoto,
	eFile_HiResPhoto,
};

typedef int UgcRequestID;
static const int INVALID_UGC_REQUEST_ID = -1;

class CUserContentManager : datBase, public CloudCacheListener BANK_ONLY(, public CloudListener)
{
public:

	static const unsigned MAX_CREATOR_RESULTS = 20;
    static const unsigned MAX_DESCRIPTION = 512;
	static const unsigned MAX_REQUESTED_CONTENT_IDS = 64;
	static const bool DEFAULT_USE_CDN_TEMPLATE = true;

    static const unsigned NUM_SCRIPT_TEXTLABELS_PER_UGC_DESCRIPTION = 8;
	struct sScriptDescriptionStruct
	{
		scrValue nArrayPrefix; 
		scrTextLabel63 textLabel[NUM_SCRIPT_TEXTLABELS_PER_UGC_DESCRIPTION];
	};

public:

	 CUserContentManager();
	~CUserContentManager();

	bool Init();
	void Shutdown();
	void Update();

    void OnGetCredentials(const int localGamerIndex);
    void InitTunables();
    void ResetContentHash();

	// create content
	bool Create(const char* szName, 
                const char* szDesc, 
                const rlUgc::SourceFileInfo* pFileInfo, 
                const unsigned nFiles,
                const char* szTagCSV, 
                const char* szDataJSON,
                rlUgcContentType kType, 
                bool bPublish);

    // create content
    bool Create(const char* szName, 
                const char* szDesc, 
                const rlUgc::SourceFile* pFiles, 
                const unsigned nFiles,
                const char* szTagCSV, 
                const char* szDataJSON,
                rlUgcContentType kType, 
                bool bPublish);

    // copy existing content
    bool Copy(const char* szContentID, rlUgcContentType kType);

    // query types
    enum eQueryType
    {
        eQuery_Invalid = -1,
        eQuery_BookMarked,
        eQuery_MyContent,
        eQuery_CatRockstarNone,
        eQuery_CatRockstarCreated,
        eQuery_CatRockstarCreatedCand,
        eQuery_CatRockstarVerified,
        eQuery_CatRockstarVerifiedCand,
        eQuery_ByContentID,
        eQuery_TopRated,
        eQuery_RecentlyCreated,
        eQuery_RecentlyPlayed,
        eQuery_Num,
    };
	
	// query content - this calls straight through to rlUgc and is managed externally
	bool QueryContent(rlUgcContentType kType, 
                      const char* szQueryName, 
                      const char* pQueryParams, 
                      int nOffset, 
                      int nMax, 
                      int* pTotal, 
                      int* pHash, 
                      rlUgc::QueryContentDelegate fnDelegate, 
                      netStatus* pStatus);
	
	// general-purpose method for querying content
	bool QueryContent(rlUgcContentType kType, 
                      const char* szQueryName, 
                      const char* pQueryParams, 
                      int nOffset, 
                      int nMaxCount,
                      eQueryType kQueryType);

	// allow query cancel
	void CancelQuery();
	
	// content access - specific queries using QueryContent
    bool GetBookmarked(int nOffset, int nMaxCount, rlUgcContentType kType); 
	bool GetMyContent(int nOffset, int nMaxCount, rlUgcContentType kType, bool* pbPublished, int nType, bool* pbOpen); 
    bool GetFriendContent(int nOffset, int nMaxCount, rlUgcContentType kType);
	bool GetClanContent(rlClanId nClanID, int nOffset, int nMaxCount, rlUgcContentType kType);
	bool GetByCategory(rlUgcCategory kCategory, int nOffset, int nMaxCount, rlUgcContentType kType, eUgcSortCriteria nSortCriteria, bool bDescending);
    bool GetByContentID(char szContentIDs[][RLUGC_MAX_CONTENTID_CHARS], unsigned nContentIDs, bool bLatest, rlUgcContentType kType);
    bool GetLatestByContentID(char szContentIDs[][RLUGC_MAX_CONTENTID_CHARS], unsigned nContentIDs, rlUgcContentType kType);
    bool GetTopRated(int nOffset, int nMaxCount, rlUgcContentType kType);
	bool GetMostRecentlyCreated(int nOffset, int nMaxCount, rlUgcContentType kType, bool* pbOpen);
    bool GetMostRecentlyPlayed(int nOffset, int nMaxCount, rlUgcContentType kType);

	// content modifiers
	bool UpdateContent(const char* szContentID, 
                       const char* szName, 
                       const char* szDesc, 
                       const char* szDataJSON, 
                       const rlUgc::SourceFileInfo* pFileInfo, 
                       const unsigned nFiles,
                       const char* szTags, 
                       rlUgcContentType kType);

    // content modifiers
    bool UpdateContent(const char* szContentID, 
                       const char* szName, 
                       const char* szDesc, 
                       const char* szDataJSON, 
                       const rlUgc::SourceFile* pFiles, 
                       const unsigned nFiles,
                       const char* szTags, 
                       rlUgcContentType kType);

    bool Publish(const char* szContentID, const char* szBaseContentID, rlUgcContentType kType, const char* httpRequestHeaders);
    bool SetBookmarked(const char* szContentID, const bool bIsBookmarked, rlUgcContentType kType);
	bool SetDeleted(const char* szContentID, const bool bIsDeleted, rlUgcContentType kType);

    // player modifiers
	bool SetPlayerData(const char* szContentID, const char* szDataJSON, const float* rating, rlUgcContentType kType);

	// data access
	CloudRequestID RequestContentData(const int nContentIndex, const int nFileIndex, bool bFromScript);
    CloudRequestID RequestContentData(const rlUgcContentType kType,
                                      const char* szContentID,
                                      const int nFileID, 
                                      const int nFileVersion,
                                      const rlScLanguage nLanguage, 
                                      bool bFromScript);
    CloudRequestID RequestCdnContentData(const char* contentName, unsigned nCloudRequestFlags = 0);

    // access to descriptions
    CacheRequestID RequestCachedDescription(unsigned nHash);
    bool IsDescriptionRequestInProgress(unsigned nHash);
    bool HasDescriptionRequestFinished(unsigned nHash);
    bool DidDescriptionRequestSucceed(unsigned nHash);
    const char* GetCachedDescription(unsigned nHash);
    bool ReleaseCachedDescription(unsigned nHash);
    void ReleaseAllCachedDescriptions();
    const char* GetStringFromScriptDescription(sScriptDescriptionStruct& scriptDescription, char* szDescription, unsigned nMaxLength);

	// create result access
	void ClearCreateResult();
	bool IsCreatePending();
	bool HasCreateFinished();
	bool DidCreateSucceed();
	int GetCreateResultCode();
	const char* GetCreateContentID();
	s64 GetCreateCreatedDate();
	const rlUgcMetadata* GetCreateResult();
	
	// clears out query results - freeing up memory
	void ClearQueryResults();
	
	// query result access
	bool IsQueryPending();
	bool HasQueryFinished();
	bool DidQuerySucceed();
    bool WasQueryForceCancelled();
    int GetQueryResultCode();
	unsigned GetContentNum();
	int GetContentTotal();
    int GetContentHash(); 

	// query content access
	int GetContentAccountID(int nContentIndex);
    rlUgcContentType GetContentType(int nContentIndex);
    const char* GetContentID(int nContentIndex);
    const char* GetRootContentID(int nContentIndex);
    const rlUgcCategory GetContentCategory(int nContentIndex);
    const char* GetContentName(int nContentIndex);
    u64 GetContentCreatedDate(int nContentIndex);
    const char* GetContentData(int nContentIndex);
    unsigned GetContentDataSize(int nContentIndex);
    const char* GetContentDescription(int nContentIndex);
    unsigned GetContentDescriptionHash(int nContentIndex);
    int GetContentNumFiles(int nContentIndex);
    int GetContentFileID(int nContentIndex, int nFileIndex);
    int GetContentFileVersion(int nContentIndex, int nFileIndex);
	bool GetContentHasLoResPhoto(int nContentIndex);
	bool GetContentHasHiResPhoto(int nContentIndex);
    rlScLanguage GetContentLanguage(int nContentIndex);
    RockstarId GetContentRockstarID(int nContentIndex);
    u64 GetContentUpdatedDate(int nContentIndex);
    const char* GetContentUserID(int nContentIndex);
	bool GetContentCreatorGamerHandle(int nContentIndex, rlGamerHandle* pHandle);
	bool GetContentCreatedByLocalPlayer(int nContentIndex);
	const char* GetContentUserName(int nContentIndex);
    bool GetContentIsUsingScNickname(int nContentIndex);
    int GetContentVersion(int nContentIndex);
    bool IsContentPublished(int nContentIndex);
    bool IsContentVerified(int nContentIndex);
    u64 GetContentPublishedDate(int nContentIndex);
	const char* GetContentPath(int nContentIndex, int nFileIndex, char* pBuffer, const unsigned nBufferMax);

	const char* GetContentStats(int nContentIndex);
	unsigned GetContentStatsSize(int nContentIndex);

	float GetRating(int nContentIndex);
	float GetRatingPositivePct(int nContentIndex);
	float GetRatingNegativePct(int nContentIndex);
	s64 GetRatingCount(int nContentIndex);
	s64 GetRatingPositiveCount(int nContentIndex);
	s64 GetRatingNegativeCount(int nContentIndex);

	// access via ID
	int GetContentIndexFromID(const char* szContentID);

	// get player access
	bool HasPlayerRecord(int nContentIndex);
	bool HasPlayerRated(int nContentIndex);
	float GetPlayerRating(int nContentIndex); 
	bool HasPlayerBookmarked(int nContentIndex);
	const char* GetPlayerData(int nContentIndex);
	unsigned GetPlayerDataSize(int nContentIndex);

	// modify result access
	void ClearModifyResult();
	bool IsModifyPending();
	bool HasModifyFinished();
	bool DidModifySucceed();
	int GetModifyResultCode();
	const char* GetModifyContentID();

	// query content creators - this calls straight through to rlUgc and is managed externally
	bool QueryContentCreators(rlUgcContentType kType, const char* szQueryName, const char* pQueryParams, int nOffset, int nMax, rlUgc::QueryContentCreatorsDelegate fnDelegate, netStatus* pStatus);
	
	// query content creators
	bool QueryContentCreators(rlUgcContentType kType, const char* szQueryName, const char* pQueryParams, int nOffset);
	
	// specific queries
	bool GetCreatorsTopRated(int nOffset, rlUgcContentType kType);
	bool GetCreatorsByUserID(char szData[][RLROS_MAX_USERID_SIZE], unsigned nUserIDs, const rlUgcContentType kType);
	static bool BuildCreatorUserIDParam(const char* pQueryType, char szUserIDs[][RLROS_MAX_USERID_SIZE], unsigned nStrings, atString& out_QueryParams);

	// clears out query results - freeing up memory
	void ClearCreatorResults();

	// query creators result access
	bool IsQueryCreatorsPending();
	bool HasQueryCreatorsFinished();
	bool DidQueryCreatorsSucceed();
	int GetQueryCreatorsResultCode();

	unsigned GetCreatorNum();
	unsigned GetCreatorTotal();

	const char* GetCreatorUserID(int nCreatorIndex);
	const char* GetCreatorUserName(int nCreatorIndex);
	int GetCreatorNumCreated(int nCreatorIndex);
	int GetCreatorNumPublished(int nCreatorIndex);
	float GetCreatorRating(int nCreatorIndex);
	float GetCreatorRatingPositivePct(int nCreatorIndex);
	float GetCreatorRatingNegativePct(int nCreatorIndex);
	s64 GetCreatorRatingCount(int nCreatorIndex);
	s64 GetCreatorRatingPositiveCount(int nCreatorIndex);
	s64 GetCreatorRatingNegativeCount(int nCreatorIndex);
    const char* GetCreatorStats(int nCreatorIndex);
    unsigned GetCreatorStatsSize(int nCreatorIndex);
    bool GetCreatorIsUsingScNickname(int nCreatorIndex);

	static const char* GetAbsPathFromRelPath(const char* szRelPath, char* szAbsPath, unsigned nMaxPath);

	bool LoadOfflineQueryData(rlUgcCategory nCategory); 
	bool VerifyOfflineQueryData(fiStream* pStream);
	void ClearOfflineQueryData();
	void SetContentFromOffline(bool bFromOffline);
	const char* GetOfflineContentPath(const char* szContentID, int nFileID, bool useUpdate, char* szPath, unsigned nMaxPath); 

    virtual void OnCacheFileLoaded(const CacheRequestID nRequestID, bool bLoaded);

    void SetUsingOfflineContent(bool bIsUsingOfflineContent);
    bool IsUsingOfflineContent(); 

	// youtube
	UgcRequestID CreateYoutubeContent(const rlYoutubeUpload* pYoutubeUpload, bool bPublish, netStatus* pStatus);
	UgcRequestID CreateYoutubeContent(const char* szTitle, 
									  const char* szDescription, 
									  const char* szTags, 
									  const rlUgcYoutubeMetadata* pMetadata,
									  const bool bPublish,
									  netStatus* pStatus);

	UgcRequestID QueryYoutubeContent(const int nOffset, 
									 int nMaxCount, 
									 const bool bPublished,
									 int* pTotal, 
									 rlUgc::QueryContentDelegate fnDelegate, 
									 netStatus* pStatus);

	// youtube
	bool PopulateYoutubeMetadata(const char* pData, unsigned nSizeOfData, rlUgcYoutubeMetadata* pMetadata) const;
	bool WriteYoutubeMetadata(RsonWriter& rw, const rlUgcYoutubeMetadata* pMetadata) const;
	bool ReadYoutubeMetadata(const char* pData, unsigned nSizeOfData, rlUgcYoutubeMetadata* pMetadata) const;

	void ClearCompletedRequests();

	bool HasRequestFinished(UgcRequestID nRequestID); 
	bool IsRequestPending(UgcRequestID nRequestID); 
	bool DidRequestSucceed(UgcRequestID nRequestID); 
	void* GetRequestData(UgcRequestID nRequestID);

	static void SetUseCdnTemplate(bool useTemplate);

#if __BANK
	void InitWidgets(rage::bkBank* pBank);
	bool GetSimulateNoUgcWritePrivilege() const { return m_sbSimulateNoUgcWritePrivilege; }
#endif

private:

	enum eOpUGC
	{
		eOp_Idle,
		eOp_Pending,
		eOp_Finished,
	};

	struct sOfflineContentInfo 
	{
        static const unsigned MAX_FILES = 2;

		~sOfflineContentInfo() { Clear(); }

		void Clear()
		{
			m_Description.Clear();
			m_Data.Clear();
		}

		char m_ContentId[RLUGC_MAX_CONTENTID_CHARS];
		PlayerAccountId m_AccountId;
		char m_ContentName[RLUGC_MAX_CONTENT_NAME_CHARS];
		datGrowBuffer m_Description;
		datGrowBuffer m_Data;
		RockstarId m_RockstarId;
		char m_UserId[RLROS_MAX_USERID_SIZE];
		char m_UserName[RLROS_MAX_USER_NAME_SIZE];
        int m_nNumFiles;
        char m_CloudAbsPath[MAX_FILES][RLUGC_MAX_CLOUD_ABS_PATH_CHARS];
    };

	// convenience functions
	bool CheckContentQuery();
	bool CheckContentModify(bool requireWritePermission);

	// can query data convenience
	bool CanQueryData(bool bOfflineAllowed OUTPUT_ONLY(, const char* szFunction));
	bool CanQueryData(int nContentIndex, bool bOfflineAllowed OUTPUT_ONLY(, const char* szFunction));

	// query delegate callback
	void OnContentResult(const int nIndex,
						 const char* szContentID,
						 const rlUgcMetadata* pMetadata,
						 const rlUgcRatings* pRatings,
						 const char* szStatsJSON,
						 const unsigned nPlayers,
						 const unsigned nPlayerIndex,
						 const rlUgcPlayer* pPlayer);

	// query delegate callback
	void OnCreatorResult(const unsigned nTotal, const rlUgcContentCreatorInfo* pCreator);

	// reset queries
	void ResetContentQuery();
	void ResetCreatorQuery();

    // description cache
    void WriteDescriptionCache();

	// URL Templates
	bool RequestUrlTemplates(const int localGamerIndex);
#if __BANK
	void Bank_Create();
    void Bank_ValidateJSON();
    void Bank_GetContentByID();
	void Bank_GetContentByIDs();
	void Bank_GetMyContent();
	void Bank_GetFriendContent();
	void Bank_GetClanContent();
	void Bank_GetByCategory();
	void Bank_GetMostRecentlyCreated();
	void Bank_GetTopRated();
	void Bank_ClearQueryResults();

	void Bank_SaveCategory(rlUgcCategory nCategory);
	void Bank_SaveRockstarCreated();
	void Bank_SaveRockstarVerified(); 

	void Bank_LoadRockstarCreated();
	void Bank_LoadRockstarVerified(); 
	void Bank_ClearOfflineQueryData(); 

	void Bank_TestYoutubeMetadata();
	void Bank_CreateYoutubeContent();
	void Bank_QueryYoutubeContent();
	void Bank_LoadUrlTemplateFile();
	void Bank_RequestUrlTemplates();
    void Bank_DownloadCredits();

	static const unsigned kMAX_NAME = 64;
	char m_szContentFileName[kMAX_NAME];
	char m_szContentDisplayName[kMAX_NAME];
	char m_szDataFileName[kMAX_NAME];
	char m_szContentID[kMAX_NAME];
	char m_szUrlTemplatesFile[RAGE_MAX_PATH];
	bool m_sbSimulateNoUgcWritePrivilege;

	fiStream* m_pSaveStream;
	rlUgcCategory m_nCurrentCategory;
	rlUgc::QueryContentDelegate m_QueryToSaveDelegate;
	netStatus m_QueryToSaveStatus; 
	eOpUGC m_QueryToSaveOp;

	struct sDataRequest
	{
        static const unsigned MAX_LOCAL_PATH = 64; 

		sDataRequest() : nRequestID(-1) {} 
		sDataRequest(CloudRequestID nId) : nRequestID(nId) {} 

		CloudRequestID nRequestID; 
		char m_szLocalPath[MAX_LOCAL_PATH];

        rlUgcContentType m_nContentType;
        char m_szContentID[RLUGC_MAX_CONTENTID_CHARS];
        int m_nFileID;
        int m_nFileVersion;
        rlScLanguage m_nLanguage;
		
		bool operator==(const sDataRequest& that) const
		{
			return nRequestID == that.nRequestID;
		}
	};
	atArray<sDataRequest> m_DataRequests; 

	void OnContentResultToSave(const int nIndex,
							   const char* szContentID,
							   const rlUgcMetadata* pMetadata,
							   const rlUgcRatings* pRatings,
							   const char* szStatsJSON,
							   const unsigned nPlayers,
							   const unsigned nPlayerIndex,
							   const rlUgcPlayer* pPlayer);

	void OnCloudEvent(const CloudEvent* pEvent);
#endif
	
	static const int MAX_QUERY_COUNT = 500;
    static const int MAX_CONTENT_TO_HOLD = 50;

	struct UgcRequestNode
	{
		UgcRequestNode() { m_nRequestID = INVALID_UGC_REQUEST_ID; m_Status = NULL; m_bFlaggedToClear = false; m_nTimeStarted = 0; }
		virtual ~UgcRequestNode() {}
		UgcRequestID m_nRequestID;
		netStatus m_MyStatus;
		netStatus* m_Status;
		bool m_bFlaggedToClear;
		unsigned m_nTimeStarted;
		virtual void* GetRequestData() { return NULL; }
	};

	struct UgcCreateRequestNode : public UgcRequestNode
	{
		UgcCreateRequestNode() { m_Metadata.Clear(); }
		virtual ~UgcCreateRequestNode() { m_Metadata.Clear(); }
		rlUgcMetadata m_Metadata; 
		virtual void* GetRequestData() { return &m_Metadata; }
	};

	UgcRequestID m_UgcRequestCount;
	atArray<UgcRequestNode*> m_RequestList;
 
	rlUgc::QueryContentDelegate m_QueryDelegate;
	int m_nContentTotal;
    int m_nContentNum;
	atArray<rlUgcPlayer*> m_PlayerData;
	atArray<rlUgcContentInfo*> m_ContentData;
    bool m_bFlaggedToCancel;
    bool m_bWasQueryForceCancelled;
    eQueryType m_kQueryType;
    rlUgcContentType m_kQueryContentType;

    int m_QueryHash[eQuery_Num];
    int m_QueryHashForTask;
    
    // descriptions
    static const unsigned DESCRIPTION_BLOCK_SIZE = 64 * 1024; // 64KB
    char* m_pDescriptionBlock;
    unsigned m_nDescriptionBlockOffset;
    unsigned m_nCurrentDescriptionBlock; 
    atArray<unsigned> m_DescriptionHash;

	rlUgc::QueryContentCreatorsDelegate m_CreatorDelegate;
	unsigned m_nCreatorTotal;
	unsigned m_nCreatorNum;
	atArray<rlUgcContentCreatorInfo*> m_CreatorData;

	bool m_bQueryDataOffline;
    int m_nContentOfflineTotal;
	atArray<sOfflineContentInfo*> m_ContentDataOffline;
	
	rlUgcMetadata m_CreateResult;
	rlUgcMetadata m_ModifyResult;

	// status - allow concurrent create, query and modify calls
	netStatus m_CreateStatus;
	netStatus m_QueryStatus;
	netStatus m_ModifyStatus;
	netStatus m_QueryCreatorsStatus; 
	eOpUGC m_CreateOp;
	eOpUGC m_QueryOp;
	eOpUGC m_ModifyOp;
	eOpUGC m_QueryCreatorsOp;

#if !__NO_OUTPUT
	unsigned m_nCreateTimestamp;
	unsigned m_nQueryTimestamp;
	unsigned m_nModifyTimestamp;
	unsigned m_nQueryCreatorsTimestamp;
	unsigned m_nLastQueryResultTimestamp; 
#endif

    bool m_bIsUsingOfflineContent;

	// URL Templates
	atArray<rlUgcUrlTemplate> m_UrlTemplates;
	netStatus m_UrlTemplatesRequestStatus;
	static bool sm_bUseCdnTemplate;

    struct sDescription
    {
        sDescription() : m_szDescription(NULL), m_nCacheRequestID(INVALID_CACHE_REQUEST_ID), m_nHash(0), m_bRequestFinished(false), m_bRequestSucceeded(false), m_bFlaggedForRelease(false) {}
        char* m_szDescription;
        CacheRequestID m_nCacheRequestID;
        unsigned m_nHash;
        bool m_bRequestFinished;
        bool m_bRequestSucceeded;
        bool m_bFlaggedForRelease; 
    };
    atArray<sDescription*> m_ContentDescriptionRequests;
};

typedef atSingleton<CUserContentManager> UserContentManager;
#endif
