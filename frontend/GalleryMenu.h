#ifndef __GALLERY_MENU_H__
#define __GALLERY_MENU_H__


#include "CMenuBase.h"

#include "rline/rlpresence.h"
#include "Vector/Vector3.h"
#include "grcore/texture.h"

// Game headers
#include "frontend/hud_colour.h"
#include "frontend/PauseMenu.h"
#include "SaveLoad/savegame_defines.h"
#include "text/TextFormat.h"

class CGalleryMenu : public CMenuBase
{
	enum eGalleryActionState
	{
		GA_INVALID = -1,
		GA_MAXIMIZE_PROGRESS_LOADING_SWAP_IMAGE,
		GA_MAXIMIZE_PROGRESS_LOADING_SWAPPING_IMAGE,
		GA_MAXIMIZE_PROGRESS_LOADING_NEW_PAGE_SWAP_IMAGE,
		GA_MAXIMIZE_PROGRESS_LOADING_NEW_PAGE_SWAPPING_IMAGE3,
		GA_MAXIMIZE_PROGRESS_LOADING_NEW_PAGE_SWAPPING_IMAGE2,
		GA_MAXIMIZE_PROGRESS_LOADING_NEW_PAGE_SWAPPING_IMAGE,
		GA_MAXIMIZE_PROGRESS_LOADING_IMAGE,
		GA_MAXIMIZE_PROGRESS_LOADING_FINISHED,
		GA_MAXIMIZE_FAILED,
		GA_NO_ONLINE_PRIVILEGE,
		GA_NO_SOCIAL_SHARING,
		GA_PENDING_SYSTEM_UPDATE,
		GA_UNKNOWN_ERROR,
		GA_NO_ONLINE_PRIVILEGE_PROMPT,
		GA_FACEBOOK_LOADING_IMAGE,
		GA_FACEBOOK_LOADED_IMAGE,
		GA_FACEBOOK_ERROR_UPLOAD_FAILED,
		GA_FACEBOOK_ERROR_PROFILE_SETTING_FAIL,
		GA_FACEBOOK_ERROR,
		GA_SOCIALCLUB_IS_AGE_RESTRICTED,
		GA_SOCIALCLUB_NOT_CONNECTED_TO_SOCIAL_CLUB,
		GA_SOCIALCLUB_ONLINE_POLICY_IS_NOT_UP_TO_DATE,
		GA_SOCIALCLUB_NOT_LINK_CONNECTED,
#if RSG_ORBIS
		GA_NO_PLATFORM_SUBSCRIPTION,
#endif	//	RSG_ORBIS
		GA_NO_USER_CONTENT_PRIVILEGES,
		GA_SOCIALCLUB_NOT_ONLINE_ROS,
		GA_NOT_SIGNED_IN,
		GA_NOT_SIGNED_IN_LOCALLY,
		GA_DELAY_SIGN_IN,
		GA_DELETE_CONFIRM,
		GA_MEME_SAVE_MESSAGE,
		GA_MEME_GALLERY_FULL_MESSAGE,
		GA_GALLERY_EMPTY,
		GA_GALLERY_EMPTY_NOT_CONNECTED_TO_SOCIAL_CLUB,
#if __LOAD_LOCAL_PHOTOS
		GA_UPLOAD_CONFIRM,
		GA_UPLOAD_WARNING
#endif	//	__LOAD_LOCAL_PHOTOS
	};

	enum eGalleryItemState
	{
		eGalleryItemState_Empty = 0, 
		eGalleryItemState_Corrupted,
		eGalleryItemState_Queued,       // Cammera no spinner
		eGalleryItemState_Loading,		// Camera spinner
		eGalleryItemState_Loaded,       // loaded
		eGalleryItemState_Transition,   // transition between maximized
	};

	enum eGalleryScanProgress
	{
		GALLERY_SCAN_PROGRESS_CREATE_SORTED_LIST,
		GALLERY_SCAN_PROGRESS_WAIT_FOR_SORTED_LIST,
		GALLERY_SCAN_PROGRESS_LOAD_TEXT_FOR_SONG_TITLES,
		GALLERY_SCAN_PROGRESS_BEGIN_LOAD_PHOTO,
		GALLERY_SCAN_PROGRESS_CHECK_LOAD_PHOTO,
		GALLERY_SCAN_PROGRESS_FINISHED,
		GALLERY_SCAN_PROGRESS_IDLE
	};

	enum eGalleryState
	{
		eGalleryState_Invalid = -1,
		eGalleryState_InMenu,
		eGalleryState_InKeyboardForName,
		eGalleryState_ProfanityCheckName,
		eGalleryState_ProfanityCheckFailedName,
		eGalleryState_ProfanityCheckUnavilableName,
		eGalleryState_InDeleteProcess,
		eGalleryState_AfterDeleteProcess,
		eGalleryState_Maximize,
		eGalleryState_InKeyboardForMemeText,
		eGalleryState_ProfanityCheckMemeText,
		eGalleryState_ProfanityCheckFailedMemeText,
		eGalleryState_ProfanityCheckUnavilableMemeText,
		eGalleryState_PlaceMemeText,
		eGalleryState_ReviewMemeImage,
		eGalleryState_InMemeSaveProcess,
		eGalleryState_AfterMemeSaveProcess,
#if __LOAD_LOCAL_PHOTOS
		eGalleryState_UploadingLocalPhotoToCloud,

#if __UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME
		eGalleryState_WaitForPhotoSaveToFinish,
#endif	//	__UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME

#endif	//	__LOAD_LOCAL_PHOTOS
		eGalleryState_MaxStates
	};

// 	enum eErrorState
// 	{
// 		eErrorState_Invalid = -1,
// 		eErrorState_None,
// 		eErrorState_GalleryEmpty,
// 		eErrorState_NoOnlinePrivilege,
// 		eErrorState_NoSocialSharing,
// 		eErrorState_UnderAge,
// 		eErrorState_NotSignedInSocialClub,
// 		eErrorState_NotSignedInConsole,
// 		eErrorState_NotSignedInLocally,
// 		eErrorState_SocialClubNotAvailable,
// 		eErrorState_NoPlatformSubscription,
// 		eErrorState_NPAuthError,
// 		eErrorState_NoInternet,
// 		eErrorState_UnknownError
// 	};

	enum eContextState
	{
		eMenuState_ThumbnailMode= 0,
		eMenuState_MaximizeMode,
		eMenuState_ProfanityCheck,
		eMenuState_PlaceTextMode,
		eMenuState_ReviewTextMode,
		eMenuState_FinalizeTextMode,
		eMenuState_CorruptTexture,
		eMenuState_NormalTexture,
		eMenuState_Empty
	};

public:
	CGalleryMenu (CMenuScreen& owner);
	~CGalleryMenu ();

	bool SetErrorPages(bool bCheckEmptyGallery, bool bDoOnlineChecks);
	bool UpdateInput(s32 eInput);

	virtual void Init();
	virtual bool Prepopulate(MenuScreenId newScreenId);
	virtual bool Populate(MenuScreenId newScreenId);
	virtual void LayoutChanged( MenuScreenId iPreviousLayout, MenuScreenId iNewLayout, s32 iUniqueId, eLAYOUT_CHANGED_DIR eDir );
	virtual void LoseFocus();
	virtual void Update();
	virtual void Render(const PauseMenuRenderDataExtra* renderData);

#if RSG_PC
	// interrupts. If true is returned, no further action is performed
	virtual bool CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args);
#endif	//	RSG_PC

	virtual void OnNavigatingContent(bool bAreWe);

	static void OnPresenceEvent(const rlPresenceEvent* evt);

	void SetDelayedSignIn() { m_eGalleryActionState = GA_DELAY_SIGN_IN; }
private:
	s32 CreateGalleryBlip(Vector3& vPosition);

	void UpdateGalleryActions();
	void UpdateGalleryScanProgress();

	void DisplayPhotoInSlot(u32 indexOfPhotoTxd, u32 galleryItemIndex, bool bIsCorrupt = false);
	void DisplayPhotoLoadingInSlot(s32 indexToLoad);

	const char *GetWarningScreenTitle(eGalleryActionState eErrorState);

	//	bWarningScreen - pass TRUE when the text will be displayed using CWarningScreen
	//	The text for CWarningScreen shouldn't contain any buttons like ~INPUT_FRONTEND_ACCEPT~ so we'll need to have different text for those in the American text files. 
	const char *GetWarningScreenString(eGalleryActionState eErrorState, bool bWarningScreen);

	void DisplayWarningScreen(eGalleryActionState eErrorState);
	void ClearWarningScreen();

	void RecalculateMaxPages();
	void Repopulate( bool const forceUnloadAll );
	bool IsSelectedIndexCorrupted();
	bool IsImageCorrupted(u32 i);

	bool IsLastElementInGallery( s32 const entry ) const;
	bool CanMoveToRequestedEntry(s32 sEntry) const;
	bool ShouldNavigateToNewPage(s32 sEntry) const;

	void SetScrollBarArrows(eContextState eMenuState = eMenuState_ThumbnailMode);
	void SetScrollBarCount();
	void SetMenuContext(eContextState eMenuState);

	void SetMaximize(int eState);
	void SetMaximize(int eState, char const * const dictionaryName, char const * const textureName );

	void ReturnToThumbnailView();

	bool CanEnterMemeEditor() const;

	void SetupMemeEditor();
	void EnterMemeTextEntry( char const * const initialText = 0 );

	static void RequestSaveMemePhotoCallback( u8* pJPEGBuffer, u32 size, bool bSuccess );
	void DetachMemeEditorTexture();
	
	void CleanupMemeEditorAudio();
	void CleanupMemeEditor();

    void SetupTextTemplates();
    void CleanupTextTemplates();

	void GetStreetAndZoneStringsForPos(const Vector3& pos, char* streetName, u32 streetNameSize, char* zoneName, u32 zoneNameSize);
	void ClearPhotoSlots();

	//	Called by the constructor and LoseFocus() so that the gallery is always in its initial state when the 
	//	player navigates to it for the first and all subsequent times
	void Initialise();

	void ClearGalleryIfPlayerIsSignedOut();

	bool HasUserContentPrivileges();
	bool HasOnlinePrivileges();
	eGalleryActionState CheckForGalleryProblems(bool bDoOnlineChecks);
	
	void UpdateWaypointContextButton();

#if __LOAD_LOCAL_PHOTOS
	void UpdateUploadContextButton();

	void BeginUploadOfLocalPhoto();
	void CheckUploadOfLocalPhoto();

	void ProcessUploadWarningMessage();
#endif	//	__LOAD_LOCAL_PHOTOS

#if __UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME
	void CheckIfPhotoSaveHasFinished();
#endif	//	__UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME

	void UpdateAfterSelectingAThumbnail(eFRONTEND_INPUT inputSound, bool bSetMenuContext, bool bSetColumnHighlight);
	void SetDescription(bool bDisplayDescription);
	bool StepIntoGalleryPage();
	void StepOutOfGalleryPage();

	bool MaximizeTheSelectedThumbnail();

	void CancelProfanityCheck();

#if RSG_PC
	bool IsValidMenuUniqueIdForAPhoto(s32 menuUniqueId);

	void SetIndexOfPhotoSelectedWithMouse(s32 menuUniqueId);

	void SetDimensionsOfMaximizedPhoto();
#endif	//	RSG_PC

private:
	eGalleryState				m_eMenuState;
	eGalleryScanProgress		m_GalleryScanProgress;
	eGalleryActionState			m_eGalleryActionState;

	CEntryInMergedPhotoListForDeletion	m_EntryForDeletion;
#if __LOAD_LOCAL_PHOTOS
	s32						m_EntryForUpload;
	eGalleryActionState		m_UploadWarningType;
#endif	//	__LOAD_LOCAL_PHOTOS

#if __UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME
	int m_IndexOfPhotoWhoseTitleHasBeenUpdated;
#endif	//	__UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME

	grcTexture*			m_pGalleryTexture;
	strLocalIndex		m_galleryTextureTxdId;
	s32					m_galleryTextureLocalIndex;
	char*				m_szProfanityString;

	u32 m_PhotoToLoad;
	u32 m_IndexToLoad;

#if RSG_PC
	float m_fMemePhotoPosX;
	float m_fMemePhotoPosY;

	float m_fMemePhotoWidth;
	float m_fMemePhotoHeight;
#endif	//	RSG_PC

	int m_iSelectedIndex;
	int m_iSelectedIndexPerPage;
	int m_iMaxNumberOfPages;
	int m_iCurrentPage;
	int m_iMoveInMaximizeOldIndex;
	int m_iMemeTextEntered;
	eOverlayTextColours m_topHudColour;
	eOverlayTextColours m_bottomHudColour;

	bool		m_bRepopulateOnDelete : 1;
	bool		m_bReturnFromMemeSave : 1;
#if __UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME
	bool		m_bReturnFromTitleResave : 1;
#endif	//	__UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME
	bool		m_bBuiltGalleryList : 1;
	bool		m_bPaging : 1;
	bool		m_bQueueSnapToBlipOnEntry : 1;
	bool		m_bErrorOnEmpty : 1 ;
	bool		m_bIsInPrologue : 1;
	bool		m_bIsTextMoveSoundPlaying : 1;
	bool		m_bIsTextZoomSoundPlaying : 1;
	bool		m_bWasMemeSaveTriggeredThisFrame : 1;
	bool		m_bWasFacebookErrorTriggeredThisFrame : 1;
	bool		m_bRadarEnabledOnEntry : 1;

	static u32 m_iDelayedSignInCounter;

	static int const			sm_maxKeyboardTitleLength;
	static int const			sm_maxMemeTextLength;
	static int const			sm_maxMemeTextEntries;
	static float const			sm_minMemeTextSize;
	static float const			sm_maxMemeTextSize;
	static float const			sm_minMemeTextSizeIncrement;

	static float const			sm_triggerThreshold;
	static float const			sm_thumbstickThreshold;
	static float const			sm_thumbstickMultiplier;

	static int					sm_indexOfPhotoForMemeMetadata;
	static bool					sm_memePhotoSaveCallbackReturned;

	static const bool			sm_bDoOnlineErrorChecks;

	atArray<eGalleryItemState> m_ItemState;
};

#endif // __GALLERY_MENU_H__

