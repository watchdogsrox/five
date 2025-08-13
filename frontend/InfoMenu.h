#ifndef __INFO_MENU_H__
#define __INFO_MENU_H__

#include "CMenuBase.h"
#include "text/messages.h"
#include "GameStream.h"
#include "Network/SocialClubServices/SocialClubNewsStoryMgr.h"

class CInfoMenu : public CMenuBase
{
public:
	CInfoMenu(CMenuScreen& owner);
	~CInfoMenu();

	bool UpdateInput(s32 eInput);
	virtual void Update();
	virtual bool Populate(MenuScreenId newScreenId);
	virtual void LayoutChanged( MenuScreenId iPreviousLayout, MenuScreenId iNewLayout, s32 iUniqueId, eLAYOUT_CHANGED_DIR UNUSED_PARAM(eDir) );
	virtual void LoseFocus();
	virtual bool CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args);

private:
	struct stStreamedFeedItems
	{
		stFeedItem stItem;
		CTextureDownloadRequestDesc requestDesc;
		TextureDownloadRequestHandle requestHandle;
		bool bClanEmblemRequested;
		strLocalIndex iTextureDictionarySlot;
		gstPostStatus Status;

		stStreamedFeedItems()
		{
			Reset();
		}

		bool RequestTextureForDownload(const char* textureName,const char* filePath);
		bool UpdateTextureDownloadStatus();
		void ReleaseAndCancelTexture();

		void Reset()
		{
			Status = GAMESTREAM_STATUS_FREE;

			requestHandle = CTextureDownloadRequest::INVALID_HANDLE;
			iTextureDictionarySlot = -1;
			bClanEmblemRequested = false;
		}
	};

	void FillContent(MenuScreenId iPaneId);
	void PopulateWarningMessage(s32 iMenuID);
	bool PopulateInfoMenu(CMessages::ePreviousMessageTypes iInfoScreen);
	bool PopulateFeedMenu();
	bool PopulateNewswireMenu();
	void UpdateFeedItem(stStreamedFeedItems& item);
	void SetupStreamingParams(stStreamedFeedItems& item);
	void FreePost(stStreamedFeedItems& item);
	bool PostFeedItem(stStreamedFeedItems& item, int iIndex, bool bIsUpdate);
	void UpdateNewswireColumnScroll(int iNumStories, int iSelectedStory);
	void PostNewswireStory(CNetworkSCNewsStoryRequest* pNewsItem, bool bUpdateTexture);
	void ClearNewswireStory();

	MenuScreenId m_CurrentScreen;
	stStreamedFeedItems m_StreamedFeedItems[FEED_CACHE_MAX_SIZE];
	int m_iStreamedFeedCount;
	bool m_bStoryTextPosted;
	int m_iMouseScrollDirection;
};

#endif // __INFO_MENU_H__

