// rage
#include "system/param.h"
#include "net/nethardware.h"

// game
#include "Frontend/BusySpinner.h"
#include "Frontend/GalleryMenu.h"
#include "Frontend/ScaleformMenuHelper.h"

#include "audio/frontendaudioentity.h"
#include "audio/scriptaudioentity.h"
#include "Frontend/FrontendStatsMgr.h"

#include "frontend/hud_colour.h"
#include "frontend/MiniMapRenderThread.h"
#include "frontend/MinimapMenuComponent.h"
#include "frontend/MiniMapCommon.h"
#include "Frontend/PauseMenu.h"
#include "frontend/CMapMenu.h"
#include "frontend/HudTools.h"
#include "Frontend/ScaleformMenuHelper.h"
#include "Frontend/Scaleform/ScaleFormMgr.h"
#include "Frontend/PauseMenu.h"
#include "frontend/WarningScreen.h"
#include "Frontend/SocialClubMenu.h"
#include "frontend/MousePointer.h"
#include "frontend/TextInputBox.h"
#include "frontend/VideoEditor/ui/TextTemplate.h"
#include "Network/NetworkInterface.h"
#include "Stats/StatsMgr.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_photo_manager.h"
#include "Frontend/ButtonEnum.h"
#include "Frontend/ui_channel.h"
#include "input/virtualkeyboard.h"
#include "renderer/PostProcessFXHelper.h"
#include "renderer/sprite2d.h"
#include "rline/rlnp.h"
#include "system/controlMgr.h"
#include "text/TextConversion.h"
#include "text/TextFormat.h"
#include "fwdrawlist/drawlist.h"
#include "fwvehicleai/pathfindtypes.h"
#include "scene/ExtraContent.h"
#include "game/user.h"

#include "fwnet/netprofanityfilter.h"
#include "rline/rlprivileges.h"

FRONTEND_OPTIMISATIONS();

#define MAX_ROWS_ON_PAGE 3
#define MAX_COLUMNS_ON_PAGE 4
#define MAX_ITEMS_ON_PAGE ( MAX_ROWS_ON_PAGE * MAX_COLUMNS_ON_PAGE )
#define BLIP_GALLERY 184

#define DELAYED_SIGNIN_COUNT 5000
	
#define SCROLL_TYPE_ALL 0
#define SCROLL_TYPE_LEFTRIGHT 2
#define SCROLL_TYPE_NONE 3

#define MAP_DISTANCE_REQ 5
#define TITLE_BUFFER_LENGTH (120)

#define MAX_MEME_TEXTS (1)

static rlPresence::Delegate g_GalleryDlgt;
netProfanityFilter::CheckTextToken s_galleryProfanityToken;
u32 CGalleryMenu::m_iDelayedSignInCounter = 0;

int const CGalleryMenu::sm_maxKeyboardTitleLength		= 50;
int const CGalleryMenu::sm_maxMemeTextLength			= PhonePhotoEditor::PHOTO_TEXT_MAX_LENGTH - 1;
int const CGalleryMenu::sm_maxMemeTextEntries			= 2;

float const CGalleryMenu::sm_minMemeTextSize			= 0.5f;
float const CGalleryMenu::sm_maxMemeTextSize			= 10.f;
float const CGalleryMenu::sm_minMemeTextSizeIncrement	= 0.05f;

float const CGalleryMenu::sm_triggerThreshold			= 0.1f;
float const CGalleryMenu::sm_thumbstickThreshold		= 0.1f;
float const CGalleryMenu::sm_thumbstickMultiplier		= 0.01f;

int CGalleryMenu::sm_indexOfPhotoForMemeMetadata		= -1;
bool CGalleryMenu::sm_memePhotoSaveCallbackReturned		= false;

#if __LOAD_LOCAL_PHOTOS
const bool CGalleryMenu::sm_bDoOnlineErrorChecks = false;		//	I really only need to do online checks when uploading photos
#else	//	__LOAD_LOCAL_PHOTOS
const bool CGalleryMenu::sm_bDoOnlineErrorChecks = true;
#endif	//	__LOAD_LOCAL_PHOTOS

//==========================================================
// CGalleryMenu - CGalleryMenu
//==========================================================
CGalleryMenu::CGalleryMenu(CMenuScreen& owner) : CMenuBase(owner), 
m_EntryForDeletion(-1),
#if __LOAD_LOCAL_PHOTOS
m_EntryForUpload(-1),
#endif	//	__LOAD_LOCAL_PHOTOS
m_pGalleryTexture(NULL),
m_galleryTextureTxdId(-1),
m_galleryTextureLocalIndex(-1),
m_iMemeTextEntered(0),
m_bIsInPrologue(false),
m_bRadarEnabledOnEntry(false)
{
	uiDebugf3("%d",(s32)CPhotoManager::GetNumberOfPhotos(false));
	m_szProfanityString = rage_new char[TITLE_BUFFER_LENGTH];
}

CGalleryMenu::~CGalleryMenu()
{
	m_eMenuState = eGalleryState_Invalid;
	delete m_szProfanityString;

	m_ItemState.Reset();
}

void CGalleryMenu::Init()
{
	Initialise();
}


//==========================================================
// CGalleryMenu - Initialise
// Purpose - Resets all member data
//==========================================================
void CGalleryMenu::Initialise()
{
	m_eGalleryActionState = GA_INVALID;
	m_GalleryScanProgress = GALLERY_SCAN_PROGRESS_IDLE;

	m_eMenuState = eGalleryState_Invalid;
	m_iSelectedIndex = 0;
	m_iSelectedIndexPerPage = 0;
	m_iCurrentPage = 0;
	m_PhotoToLoad = 0;
	m_IndexToLoad = 0;
	m_iMaxNumberOfPages = 0;
	m_iMoveInMaximizeOldIndex = -1;
	m_iMemeTextEntered = 0;
	m_iDelayedSignInCounter = 0;

#if RSG_PC
	m_fMemePhotoPosX = 0.0f;
	m_fMemePhotoPosY = 0.0f;

	m_fMemePhotoWidth = 1.0f;
	m_fMemePhotoHeight = 1.0f;
#endif	//	RSG_PC

	m_bRepopulateOnDelete = false;
	m_bReturnFromMemeSave = false;
	m_bBuiltGalleryList = false;
	m_bPaging = false;
	m_bQueueSnapToBlipOnEntry = false;
	
#if __LOAD_LOCAL_PHOTOS
	m_bErrorOnEmpty = false || CPhotoManager::IsListOfPhotosUpToDate(true);
#else	//	__LOAD_LOCAL_PHOTOS
	m_bErrorOnEmpty = false || CPhotoManager::GetNumberOfPhotosIsUpToDate();
#endif	//	__LOAD_LOCAL_PHOTOS

	m_EntryForDeletion.SetInvalid();
#if __LOAD_LOCAL_PHOTOS
	m_EntryForUpload = -1;
#endif	//	__LOAD_LOCAL_PHOTOS

#if __UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME
	m_bReturnFromTitleResave = false;
	m_IndexOfPhotoWhoseTitleHasBeenUpdated = -1;
#endif	//	__UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME

	sMiniMapMenuComponent.SetBlipObject(BLIP_GALLERY);
	m_ItemState.Reset();
	
	for (int i = 0; i < MAX_ITEMS_ON_PAGE; i++)
	{
		m_ItemState.PushAndGrow(eGalleryItemState_Empty);
	}

	CPhotoManager::SetDisplayCloudPhotoEnumerationError(true);
}

void CGalleryMenu::UpdateAfterSelectingAThumbnail(eFRONTEND_INPUT inputSound, bool bSetMenuContext, bool bSetColumnHighlight)
{
	if (inputSound != FRONTEND_INPUT_MAX)
	{
		CPauseMenu::PlayInputSound(inputSound);
	}

	SetScrollBarCount();

	if (m_ItemState[m_iSelectedIndexPerPage] == eGalleryItemState_Loaded )
	{
		if (sMiniMapMenuComponent.DoesBlipExist(m_iSelectedIndexPerPage))
		{
			sMiniMapMenuComponent.SnapToBlipWithDistanceCheck(m_iSelectedIndexPerPage,MAP_DISTANCE_REQ);
		}
	}

	if (bSetMenuContext)
	{
		if (m_ItemState[m_iSelectedIndexPerPage] == eGalleryItemState_Corrupted)
			SetMenuContext(eMenuState_CorruptTexture);
		else 
			SetMenuContext(eMenuState_ThumbnailMode);
	}

	if (bSetColumnHighlight)
	{
		CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
		if( pauseContent.BeginMethod("SET_COLUMN_HIGHLIGHT"))
		{
			pauseContent.AddParam(0);			// The columnIndex - should always be 0
			pauseContent.AddParam(m_iSelectedIndexPerPage);
			pauseContent.AddParam(true);
			pauseContent.EndMethod();
		}
	}
}

void CGalleryMenu::SetDescription(bool bDisplayDescription)
{
	CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
	if (pauseContent.BeginMethod("SET_DESCRIPTION"))
	{
		pauseContent.AddParam(0);
		pauseContent.AddParam((s32)CPhotoManager::GetNumberOfPhotos(false));
		if (bDisplayDescription)
		{
			pauseContent.AddParam(TheText.Get("DESC_CREATED"));
			pauseContent.AddParam(TheText.Get("DESC_LOCATION"));
			pauseContent.AddParam(TheText.Get("DESC_TRACK"));
			pauseContent.AddParam(true);
		}
		else
		{
			pauseContent.AddParam("");
			pauseContent.AddParam("");
			pauseContent.AddParam("");
			pauseContent.AddParam(false);
		}
		pauseContent.EndMethod();
	}
}

bool CGalleryMenu::StepIntoGalleryPage()
{
	if (m_eMenuState == eGalleryState_Invalid )
	{
		photoDisplayf("CGalleryMenu::StepIntoGalleryPage");
#if RSG_PC
		photomouseDisplayf("CGalleryMenu::StepIntoGalleryPage");
#endif	//	RSG_PC

        SetupTextTemplates();

		if (!CPauseMenu::IsNavigatingContent() && m_ItemState[0] == eGalleryItemState_Empty)
		{
			m_GalleryScanProgress = GALLERY_SCAN_PROGRESS_CREATE_SORTED_LIST;

			photoDisplayf("CGalleryMenu::StepIntoGalleryPage - about to call Prepopulate");
			Prepopulate(MENU_UNIQUE_ID_GALLERY);
			m_bQueueSnapToBlipOnEntry = true;

			return true;
		}
		else if (!CPauseMenu::IsNavigatingContent())
		{
			m_GalleryScanProgress = GALLERY_SCAN_PROGRESS_BEGIN_LOAD_PHOTO;

			photoDisplayf("CGalleryMenu::Populate - about to call Prepopulate");
			Prepopulate(MENU_UNIQUE_ID_GALLERY);

			m_PhotoToLoad = m_iCurrentPage * MAX_ITEMS_ON_PAGE;
			m_IndexToLoad = 0;

			SetDescription(true);

			CPauseMenu::PlayInputSound(FRONTEND_INPUT_ACCEPT);
			m_bQueueSnapToBlipOnEntry = true;

			return true;
		}
	}
	else
	{
		photoDisplayf("CGalleryMenu::StepIntoGalleryPage - m_eMenuState = %d. Do nothing since it doesn't equal eGalleryState_Invalid", (s32) m_eMenuState);
#if RSG_PC
		photomouseDisplayf("CGalleryMenu::StepIntoGalleryPage - m_eMenuState = %d. Do nothing since it doesn't equal eGalleryState_Invalid", (s32) m_eMenuState);
#endif	//	RSG_PC
	}

	return false;
}

void CGalleryMenu::StepOutOfGalleryPage()
{
	photoDisplayf("CGalleryMenu::StepOutOfGalleryPage");
#if RSG_PC
	photomouseDisplayf("CGalleryMenu::StepOutOfGalleryPage");
#endif	//	RSG_PC

	CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);

	if( pauseContent.BeginMethod("SET_DATA_SLOT_EMPTY") )
	{
		pauseContent.AddParam(0);			// The Column id
		pauseContent.EndMethod();
	}

	if (CPhotoManager::GetNumberOfPhotos(false) == 0)
	{
		SetDescription(false);
	}
	else
	{
		ClearPhotoSlots();
		SetDescription(true);	//	Should this be false?
	}

	CPhotoManager::RequestUnloadAllGalleryPhotos();
	sMiniMapMenuComponent.ResetBlips();

    CleanupTextTemplates();

	Initialise();
}


bool CGalleryMenu::MaximizeTheSelectedThumbnail()
{
	if (photoVerifyf( (m_iSelectedIndex >= 0) && (m_iSelectedIndex < CPhotoManager::GetNumberOfPhotos(false)), "CGalleryMenu::MaximizeTheSelectedThumbnail - expected m_iSelectedIndex (%d) to be >= 0 and < %d", m_iSelectedIndex, CPhotoManager::GetNumberOfPhotos(false)))
	{
		CUndeletedEntryInMergedPhotoList undeletedEntry(m_iSelectedIndex, true);
		if (CPhotoManager::RequestLoadGalleryPhoto(undeletedEntry))
		{
			CMapMenu::SetMapZoom(ZOOM_LEVEL_GALLERY_MAXIMIZE,true);
			SetMenuContext(eMenuState_MaximizeMode);
			m_eGalleryActionState = GA_MAXIMIZE_PROGRESS_LOADING_IMAGE;
		}

		CPauseMenu::PlayInputSound(FRONTEND_INPUT_ACCEPT);

		return true;
	}

	return false;
}

#if RSG_PC
static const float fMouseWheelScalingFactor = 1.0f;
#endif	//	RSG_PC

//==========================================================
// CGalleryMenu - UpdateInput
// Purpose - Handles all input related to particular screen.
//==========================================================
bool CGalleryMenu::UpdateInput(s32 sInput)
{
	bool bMouseRightClick = false;
	s32 mouseWheelDirection = 0;
#if KEYBOARD_MOUSE_SUPPORT
	bMouseRightClick = (ioMouse::GetReleasedButtons() & ioMouse::MOUSE_RIGHT) != 0;

	if(CMousePointer::IsMouseWheelDown())
	{
		if (m_eMenuState == eGalleryState_PlaceMemeText)
		{
			mouseWheelDirection = -1;
		}
		else
		{
			bool consumeInput = false;

			const bool bViewingPhotos = ( (m_pGalleryTexture != NULL) || (m_eMenuState == eGalleryState_InMenu) );		//	maximized OR thumbnail

			if( !bViewingPhotos || CPhotoManager::GetNumberOfPhotos(false) <= 1 || m_eMenuState == eGalleryState_ReviewMemeImage )
			{
				consumeInput = true;
			}
			else
			{
				if (m_pGalleryTexture) // maximized 
				{
					sInput = PAD_DPADRIGHT;
				}
				else // thumbnail
				{
					if ( IsLastElementInGallery( m_iSelectedIndex ) )
					{
						consumeInput = true;
						m_iSelectedIndex = 0;
						m_iSelectedIndexPerPage = 0;

						if (m_iMaxNumberOfPages == 1 )
						{
							photoDisplayf("CGalleryMenu::UpdateInput - MouseWheelDown pressed - m_iSelectedIndex=%d, m_iSelectedIndexPerPage=%d", m_iSelectedIndex, m_iSelectedIndexPerPage);

							UpdateAfterSelectingAThumbnail(FRONTEND_INPUT_RIGHT, true, true);
						}
						else
						{
							m_iCurrentPage = 0;

							CPauseMenu::PlayInputSound(FRONTEND_INPUT_RIGHT);

							photoDisplayf("CGalleryMenu::UpdateInput - MouseWheelDown pressed - calling Repopulate with m_iCurrentPage=%d", m_iCurrentPage);
							Repopulate( true );

							SetScrollBarCount();
						}
					}
					else
					{
						int iPotentialIndex = m_iSelectedIndex;
						int iPotentialIndexPerPage = m_iSelectedIndexPerPage;

						if( ( m_iSelectedIndexPerPage < MAX_ITEMS_ON_PAGE-1 ) || m_iMaxNumberOfPages <= 1 )
						{
							iPotentialIndex++;
							iPotentialIndexPerPage++;
						}

						if ( ShouldNavigateToNewPage(iPotentialIndexPerPage) )
						{
							consumeInput = true;

							m_iCurrentPage = m_iCurrentPage == ( m_iMaxNumberOfPages - 1 ) ? 0 : m_iCurrentPage + 1;

							m_iSelectedIndex = m_iCurrentPage * MAX_ITEMS_ON_PAGE;
							m_iSelectedIndexPerPage = 0;

							photoDisplayf("CGalleryMenu::UpdateInput - MouseWheelDown pressed - calling Repopulate with m_iCurrentPage=%d", m_iCurrentPage);
							Repopulate( true );
							SetScrollBarCount();

							CPauseMenu::PlayInputSound(FRONTEND_INPUT_RIGHT);
						}
						else if (CanMoveToRequestedEntry(iPotentialIndexPerPage) && iPotentialIndexPerPage != m_iSelectedIndexPerPage)
						{
							m_iSelectedIndex = iPotentialIndex;
							m_iSelectedIndexPerPage = iPotentialIndexPerPage;

							photoDisplayf("CGalleryMenu::UpdateInput - MouseWheelDown pressed - m_iSelectedIndex=i%d, m_iSelectedIndexPerPage=%d", m_iSelectedIndex, m_iSelectedIndexPerPage);

							UpdateAfterSelectingAThumbnail(FRONTEND_INPUT_RIGHT, true, true);
						}
						else
						{
							photoDisplayf("CGalleryMenu::UpdateInput - MouseWheelDown pressed - ");
							consumeInput = true;
						}
					}
				}
			}

			return consumeInput;
		}
	}

	if(CMousePointer::IsMouseWheelUp())
	{
		if (m_eMenuState == eGalleryState_PlaceMemeText)
		{
			mouseWheelDirection = 1;
		}
		else
		{
			bool consumeInput = false;

			const bool bViewingPhotos = ( (m_pGalleryTexture != NULL) || (m_eMenuState == eGalleryState_InMenu) );		//	maximized OR thumbnail

			if( !bViewingPhotos || CPhotoManager::GetNumberOfPhotos(false) <= 1 || m_eMenuState == eGalleryState_ReviewMemeImage )
			{
				consumeInput = true;
			}
			else
			{
				if (m_pGalleryTexture) // maximized 
				{
					sInput = PAD_DPADLEFT;
				}
				else // thumbnail
				{
					if( m_iSelectedIndex == 0 )
					{
						consumeInput = true;

						if ( m_iMaxNumberOfPages == 1 )
						{
							m_iSelectedIndex = CPhotoManager::GetNumberOfPhotos(false)-1;
							m_iSelectedIndexPerPage = m_iSelectedIndex;

							photoDisplayf("CGalleryMenu::UpdateInput - MouseWheelUp pressed - m_iSelectedIndex=%d, m_iSelectedIndexPerPage=%d", m_iSelectedIndex, m_iSelectedIndexPerPage);

							UpdateAfterSelectingAThumbnail(FRONTEND_INPUT_LEFT, true, true);
						}
						else
						{
							m_iSelectedIndex = MAX_ITEMS_ON_PAGE * (m_iMaxNumberOfPages -1);
							m_iSelectedIndexPerPage = 0;
							m_iCurrentPage = m_iMaxNumberOfPages-1;

							CPauseMenu::PlayInputSound(FRONTEND_INPUT_LEFT);

							photoDisplayf("CGalleryMenu::UpdateInput - MouseWheelUp pressed - calling Repopulate with m_iCurrentPage=%d", m_iCurrentPage);
							Repopulate( true );

							SetScrollBarCount();
						}
					}
					else
					{
						int iPotentialIndex = m_iSelectedIndex;
						int iPotentialIndexPerPage = m_iSelectedIndexPerPage;

						if ( m_iSelectedIndexPerPage != 0 || ( m_iMaxNumberOfPages <= 1 ) )
						{
							iPotentialIndex--;
							iPotentialIndexPerPage--;
						}

						if( ShouldNavigateToNewPage(iPotentialIndexPerPage) )
						{
							consumeInput = true;

							m_iCurrentPage = m_iCurrentPage <= 0 ? ( m_iMaxNumberOfPages - 1 ) : m_iCurrentPage - 1;

							m_iSelectedIndex = m_iCurrentPage * MAX_ITEMS_ON_PAGE;
							m_iSelectedIndexPerPage = 0;

							photoDisplayf("CGalleryMenu::UpdateInput - MouseWheelUp pressed - calling Repopulate with m_iCurrentPage=%d", m_iCurrentPage);
							Repopulate( true );

							SetScrollBarCount();
							CPauseMenu::PlayInputSound(FRONTEND_INPUT_LEFT);
						}
						else if (CanMoveToRequestedEntry(iPotentialIndexPerPage)  && iPotentialIndexPerPage != m_iSelectedIndexPerPage)
						{
							m_iSelectedIndex = iPotentialIndex;
							m_iSelectedIndexPerPage = iPotentialIndexPerPage;

							photoDisplayf("CGalleryMenu::UpdateInput - MouseWheelUp pressed - m_iSelectedIndex=%d, m_iSelectedIndexPerPage=%d", m_iSelectedIndex, m_iSelectedIndexPerPage);

							UpdateAfterSelectingAThumbnail(FRONTEND_INPUT_LEFT, true, true);
						}
						else
						{
							photoDisplayf("CGalleryMenu::UpdateInput - MouseWheelUp pressed ");
							consumeInput = true;
						}
					}
				}
			}

			return consumeInput;
		}
	}
#endif // KEYBOARD_MOUSE

	if (IsShowingWarningColumn())
	{
		if (sInput == PAD_CROSS)
		{
			if (	(m_eGalleryActionState == GA_SOCIALCLUB_IS_AGE_RESTRICTED) || 
					(m_eGalleryActionState == GA_SOCIALCLUB_NOT_CONNECTED_TO_SOCIAL_CLUB) || 
					(m_eGalleryActionState == GA_SOCIALCLUB_ONLINE_POLICY_IS_NOT_UP_TO_DATE) || 
					(m_eGalleryActionState == GA_GALLERY_EMPTY_NOT_CONNECTED_TO_SOCIAL_CLUB) )
			{
				SocialClubMenu::SetTourHash(ATSTRINGHASH("Gallery",0x1a7e17bc));

				CPauseMenu::PlayInputSound(FRONTEND_INPUT_ACCEPT);
				CPauseMenu::EnterSocialClub();
				m_eGalleryActionState = GA_INVALID;
				return true;
			}
#if !RSG_ORBIS // B*1817634 - Cannot show sign-in UI on ORBIS
			else if (m_eGalleryActionState == GA_NOT_SIGNED_IN_LOCALLY)		//		(m_eGalleryActionState == GA_NOT_SIGNED_IN) || 
			{
				CLiveManager::ShowSigninUi();
				return true;
			}
#endif
			else if ( ( m_eGalleryActionState == GA_SOCIALCLUB_NOT_LINK_CONNECTED ) || (m_eGalleryActionState == GA_SOCIALCLUB_NOT_ONLINE_ROS) || (m_eGalleryActionState == GA_NO_USER_CONTENT_PRIVILEGES) )
			{
				return true;
			}
#if RSG_ORBIS
			else if (m_eGalleryActionState == GA_NO_PLATFORM_SUBSCRIPTION)
			{
				CLiveManager::ShowAccountUpgradeUI();
				return true;
			}
#endif	//	RSG_ORBIS
		}

		return false;
		
	}

	if (m_eGalleryActionState == GA_FACEBOOK_ERROR || m_eGalleryActionState == GA_FACEBOOK_ERROR_UPLOAD_FAILED || m_eGalleryActionState == GA_FACEBOOK_ERROR_PROFILE_SETTING_FAIL
		|| m_eGalleryActionState == GA_NO_ONLINE_PRIVILEGE_PROMPT )
	{
		if(sInput == PAD_CROSS)
		{
			m_eGalleryActionState = GA_INVALID;
			return true;
		}
		else if (sInput == PAD_CIRCLE)
		{
			// Do Nothing, but do mark input has handled.
			return true;
		}
		return false;
	}
	else if (m_eGalleryActionState == GA_DELETE_CONFIRM)
	{
		if (sInput == PAD_CROSS)
		{
			photoDisplayf("CGalleryMenu::UpdateInput - Confirm Delete - Accept - m_iSelectedIndex = %d, m_iSelectedIndexPerPage = %d", m_iSelectedIndex, m_iSelectedIndex);

			CUndeletedEntryInMergedPhotoList undeletedEntry(m_iSelectedIndex, false);
			m_EntryForDeletion = CPhotoManager::RequestDeleteGalleryPhoto(undeletedEntry);

			m_ItemState[m_iSelectedIndexPerPage] = eGalleryItemState_Empty;

			m_eMenuState = eGalleryState_InDeleteProcess;
			m_eGalleryActionState = GA_INVALID;

		}
		else if (sInput == PAD_CIRCLE)
		{
			photoDisplayf("CGalleryMenu::UpdateInput - Confirm Delete - Cancel - m_iSelectedIndex = %d, m_iSelectedIndexPerPage = %d", m_iSelectedIndex, m_iSelectedIndex);

			m_eGalleryActionState = GA_INVALID;
		}

		return true;
	}
#if __LOAD_LOCAL_PHOTOS
	else if (m_eGalleryActionState == GA_UPLOAD_CONFIRM)
	{
		if (sInput == PAD_CROSS)
		{
			BeginUploadOfLocalPhoto();
		}
		else if (sInput == PAD_CIRCLE)
		{
			photoDisplayf("CGalleryMenu::UpdateInput - Confirm Upload - Cancel - m_iSelectedIndex = %d, m_iSelectedIndexPerPage = %d", m_iSelectedIndex, m_iSelectedIndexPerPage);

			m_eGalleryActionState = GA_INVALID;
		}

		return true;
	}
	else if (m_eGalleryActionState == GA_UPLOAD_WARNING)
	{
		photoDisplayf("CGalleryMenu::UpdateInput - just checking if this can ever be called for m_eGalleryActionState == GA_UPLOAD_WARNING. Do I need to call ProcessUploadWarningMessage() in here?");
		return true;
	}
#endif	//	__LOAD_LOCAL_PHOTOS
	else if( m_eGalleryActionState == GA_MEME_SAVE_MESSAGE )
	{
		if( sInput == PAD_CROSS )
		{
			m_eGalleryActionState = GA_INVALID;
		}

		return true;
	}
	else if( m_eGalleryActionState == GA_MEME_GALLERY_FULL_MESSAGE )
	{
		if( sInput == PAD_CROSS )
		{
			m_eGalleryActionState = GA_INVALID;
		}

		return true;
	}
	else if (	(m_eGalleryActionState == GA_SOCIALCLUB_IS_AGE_RESTRICTED) || 
				(m_eGalleryActionState == GA_SOCIALCLUB_NOT_CONNECTED_TO_SOCIAL_CLUB) || 
				(m_eGalleryActionState == GA_SOCIALCLUB_ONLINE_POLICY_IS_NOT_UP_TO_DATE) || 
				(m_eGalleryActionState == GA_GALLERY_EMPTY_NOT_CONNECTED_TO_SOCIAL_CLUB) )
	{
		if (sInput == PAD_CROSS)
		{
			SocialClubMenu::SetTourHash(ATSTRINGHASH("Gallery",0x1a7e17bc));

			CPauseMenu::EnterSocialClub();
			m_eGalleryActionState = GA_INVALID;
		}

		return true;
	}
	else if ( ( m_eGalleryActionState == GA_SOCIALCLUB_NOT_LINK_CONNECTED ) || ( m_eGalleryActionState == GA_SOCIALCLUB_NOT_ONLINE_ROS ) || (m_eGalleryActionState == GA_NO_USER_CONTENT_PRIVILEGES) )
	{
		return true;
	}
#if RSG_ORBIS
	else if (m_eGalleryActionState == GA_NO_PLATFORM_SUBSCRIPTION)
	{
		return true;
	}
#endif	//	RSG_ORBIS
	else if (SUIContexts::IsActive("GALLERY_MAXIMIZE") && !m_pGalleryTexture)
	{
		if (sInput == PAD_CIRCLE)
		{
			CleanupMemeEditor();
			SetMaximize( eGalleryItemState_Empty );
			SetMenuContext(eMenuState_ThumbnailMode);
			CMapMenu::SetMapZoom(ZOOM_LEVEL_GALLERY,true);

			m_eGalleryActionState = GA_INVALID;
			m_pGalleryTexture = NULL;
			m_galleryTextureTxdId = -1;
			m_galleryTextureLocalIndex = -1;
			m_eMenuState = eGalleryState_InMenu;

			CUndeletedEntryInMergedPhotoList undeletedEntry(m_iSelectedIndex, true);
			CPhotoManager::RequestUnloadGalleryPhoto(undeletedEntry);

			SetMenuContext(eMenuState_ThumbnailMode);

		}
		return true;
	}
	else if (m_eGalleryActionState != GA_INVALID)
	{
		return true;
	}

	if( m_eMenuState == eGalleryState_InKeyboardForMemeText || m_eMenuState == eGalleryState_ProfanityCheckMemeText ||
		m_eMenuState == eGalleryState_ProfanityCheckFailedMemeText || 
		m_eMenuState == eGalleryState_InMemeSaveProcess || m_eMenuState == eGalleryState_AfterMemeSaveProcess )
	{
		// Consume all input until done
		return true;
	}
	else if( m_eMenuState == eGalleryState_PlaceMemeText )
	{
		eFRONTEND_INPUT audioTrigger = FRONTEND_INPUT_MAX;

		bool const c_topText = m_iMemeTextEntered == 0;

		float fShrinkFactor =  CControlMgr::GetMainFrontendControl().GetFrontendLT().GetNorm01();
		float fGrowFactor =  CControlMgr::GetMainFrontendControl().GetFrontendRT().GetNorm01();

		bool bShrink  = fShrinkFactor > sm_triggerThreshold;
		bool bGrow  = fGrowFactor > sm_triggerThreshold;

#if RSG_PC
		const float fMousePosX = ioMouse::GetNormX();
		const float fMousePosY = ioMouse::GetNormY();
		const bool bMouseLeftDown = (ioMouse::GetButtons() & ioMouse::MOUSE_LEFT) != 0;
		const bool bShiftKeyPressed = (CControlMgr::GetKeyboard().GetKeyDown(KEY_LSHIFT, KEYBOARD_MODE_GAME) || CControlMgr::GetKeyboard().GetKeyDown(KEY_RSHIFT, KEYBOARD_MODE_GAME));
		const bool bCtrlKeyPressed = (CControlMgr::GetKeyboard().GetKeyDown(KEY_LCONTROL, KEYBOARD_MODE_GAME) || CControlMgr::GetKeyboard().GetKeyDown(KEY_RCONTROL, KEYBOARD_MODE_GAME));


		if (!bShrink && !bGrow)
		{
			if (!bCtrlKeyPressed && !bShiftKeyPressed)
			{
				if (mouseWheelDirection == 1)
				{
					bGrow = true;
					fGrowFactor = fMouseWheelScalingFactor * (f32)ioMouse::GetDZ();
				}
				else if (mouseWheelDirection == -1)
				{
					bShrink = true;
					fShrinkFactor = -fMouseWheelScalingFactor * (f32)ioMouse::GetDZ();
				}
			}
		}
#endif	//	RSG_PC

		float textSize = c_topText ? PHONEPHOTOEDITOR.GetTopTextSize() : PHONEPHOTOEDITOR.GetBottomTextSize();

		if( bShrink != bGrow )
		{
			textSize += ( bShrink ? -fShrinkFactor : fGrowFactor ) * sm_minMemeTextSizeIncrement;
			textSize = Clamp( textSize, sm_minMemeTextSize, sm_maxMemeTextSize );

			if( !m_bIsTextZoomSoundPlaying )
			{
				g_FrontendAudioEntity.PlaySoundMapZoom();
				m_bIsTextZoomSoundPlaying = true;
			}
		}
		else if( m_bIsTextZoomSoundPlaying )
		{
			g_FrontendAudioEntity.StopSoundMapZoom();
			m_bIsTextZoomSoundPlaying = false;
		}

		float const c_xAxis = CControlMgr::GetMainFrontendControl().GetFrontendLeftRight().GetNorm();
		float const c_yAxis = CControlMgr::GetMainFrontendControl().GetFrontendUpDown().GetNorm();

		bool const c_moveX = abs(c_xAxis) > sm_thumbstickThreshold;
		bool const c_moveY = abs(c_yAxis) > sm_thumbstickThreshold;

		Vector4 position( PHONEPHOTOEDITOR.GetTextPosition() );

		float& xTarget = c_topText ? position.x : position.z;
		float& yTarget = c_topText ? position.y : position.w;

		bool bTextHasBeenDraggedWithMouse = false;

#if RSG_PC
		CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_ARROW);

		if (fMousePosX >= m_fMemePhotoPosX &&
			fMousePosY >= m_fMemePhotoPosY &&
			fMousePosX <= m_fMemePhotoPosX+m_fMemePhotoWidth &&
			fMousePosY <= m_fMemePhotoPosY+m_fMemePhotoHeight)
		{
			if (bMouseLeftDown)
			{
				CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_HAND_GRAB);

				const float fMouseOffsetX = fMousePosX - m_fMemePhotoPosX;
				const float fMouseOffsetY = fMousePosY - m_fMemePhotoPosY;
				const float fFinalOffsetX = fMouseOffsetX / m_fMemePhotoWidth;
				const float fFinalOffsetY = fMouseOffsetY / m_fMemePhotoHeight;

				xTarget = Clamp( fFinalOffsetX, 0.f, 1.f );
				yTarget = Clamp( fFinalOffsetY, 0.f, 1.f );

				bTextHasBeenDraggedWithMouse = true;
			}
			else
			{
				CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_HAND_OPEN);
			}
		}
#endif // #if RSG_PC

		if (!bTextHasBeenDraggedWithMouse)
		{
			if( c_moveX )
			{
				float xPosition = xTarget;
				xPosition += c_xAxis * sm_thumbstickMultiplier;
				xTarget = Clamp( xPosition, 0.f, 1.f );
			}

			if( c_moveY )
			{
				float yPosition = yTarget;
				yPosition += c_yAxis * sm_thumbstickMultiplier;
				yTarget = Clamp( yPosition, 0.f, 1.f );
			}
		}

		// Toggle on/off the audio
		if( ( c_moveX || c_moveY || bTextHasBeenDraggedWithMouse) && !m_bIsTextMoveSoundPlaying )
		{
			g_FrontendAudioEntity.PlaySoundMapMovement();
			m_bIsTextMoveSoundPlaying = true;
		}
		else if( !c_moveX && !c_moveY && !bTextHasBeenDraggedWithMouse && m_bIsTextMoveSoundPlaying )
		{
			g_FrontendAudioEntity.StopSoundMapMovement();
			m_bIsTextMoveSoundPlaying = false;
		}

		s32 fontStyle = c_topText ? PHONEPHOTOEDITOR.GetTopTextStyle() : PHONEPHOTOEDITOR.GetBottomTextStyle();

		//! Can't trust sInput as we need to handle d-pad separate from analogue stick movement here.
		int fontStyleIncrement = CPauseMenu::CheckInput(FRONTEND_INPUT_UP, false, CHECK_INPUT_OVERRIDE_FLAG_IGNORE_ANALOGUE_STICKS ) ? -1 : 
								 CPauseMenu::CheckInput(FRONTEND_INPUT_DOWN, false, CHECK_INPUT_OVERRIDE_FLAG_IGNORE_ANALOGUE_STICKS ) ? 1 : 0;

#if RSG_PC
		if (fontStyleIncrement == 0)
		{
			if (!bCtrlKeyPressed && bShiftKeyPressed)
			{
				fontStyleIncrement = mouseWheelDirection;
			}
		}
#endif	//	RSG_PC

		if( fontStyleIncrement != 0 )
		{
#if USE_CODE_TEXT
			fontStyle += fontStyleIncrement;
			fontStyle = CTextFormat::FilterOverlayFonts( fontStyle, ( fontStyleIncrement > 0 ), c_topText ? PHONEPHOTOEDITOR.GetTopText() : PHONEPHOTOEDITOR.GetBottomText() );

			audioTrigger = fontStyleIncrement < 0 ? FRONTEND_INPUT_UP : FRONTEND_INPUT_DOWN;
#endif

#if USE_TEXT_CANVAS
			CTextTemplate::EditTextFont(fontStyleIncrement);

			if (CTextTemplate::FontCanBeEdited())
			{
				audioTrigger = fontStyleIncrement < 0 ? FRONTEND_INPUT_UP : FRONTEND_INPUT_DOWN;
			}
#endif
		}

		//! Can't trust sInput as we need to handle d-pad separate from analogue stick movement here.
		int colourIncrement = CPauseMenu::CheckInput(FRONTEND_INPUT_LEFT, false, CHECK_INPUT_OVERRIDE_FLAG_IGNORE_ANALOGUE_STICKS ) ? -1 : 
			CPauseMenu::CheckInput(FRONTEND_INPUT_RIGHT, false, CHECK_INPUT_OVERRIDE_FLAG_IGNORE_ANALOGUE_STICKS ) ? 1 : 0;

		eOverlayTextColours& currentHudColour = c_topText ? m_topHudColour : m_bottomHudColour;

#if RSG_PC
		if (colourIncrement == 0)
		{
			if (bCtrlKeyPressed && !bShiftKeyPressed)
			{
				colourIncrement = mouseWheelDirection;
			}
		}
#endif	//	RSG_PC

		if( colourIncrement != 0 )
		{
#if USE_CODE_TEXT
			int tempHudColour = (int)currentHudColour + colourIncrement;
			// Clamp and wrap-around the colour value
			currentHudColour = (eOverlayTextColours)(( tempHudColour >= eOverlayTextColours_MAX ) ? eOverlayTextColours_First : ( tempHudColour < eOverlayTextColours_First ) ? eOverlayTextColours_MAX - 1 : tempHudColour );
#endif

#if USE_TEXT_CANVAS
			CTextTemplate::EditTextColor(colourIncrement);
#endif

			audioTrigger = colourIncrement < 0 ? FRONTEND_INPUT_LEFT :FRONTEND_INPUT_RIGHT;
		}

		if( c_moveX || c_moveY || bTextHasBeenDraggedWithMouse || fontStyleIncrement != 0 || bShrink || bGrow || colourIncrement != 0 
#if __BANK
			|| (CPhotoManager::GetOverrideMemeCharacter() > 0) 
#endif	//	__BANK
			)
		{
#if USE_TEXT_CANVAS
			sEditedTextProperties &editedText = CTextTemplate::GetEditedText();

			editedText.m_position = Vector2(position.x, position.y);
			editedText.m_scale = textSize;

			if (c_moveX || bTextHasBeenDraggedWithMouse)
			{
				CTextTemplate::UpdateTemplate(0, editedText, "POSITION_X");
			}

			if (c_moveY || bTextHasBeenDraggedWithMouse)
			{
				CTextTemplate::UpdateTemplate(0, editedText, "POSITION_Y");
			}

			if (bShrink || bGrow)
			{
				CTextTemplate::UpdateTemplate(0, editedText, "SCALE");
			}
#endif

			CRGBA finalColour = CTextFormat::ConvertToRGBA( currentHudColour );

			const char *pTopText = PHONEPHOTOEDITOR.GetTopText();

#if __BANK
			if (CPhotoManager::GetOverrideMemeCharacter() > 0)
			{
				const u32 MAX_NUMBER_OF_U16_CHARACTERS_TO_DISPLAY = (TITLE_BUFFER_LENGTH/3);
				char16 override_string[MAX_NUMBER_OF_U16_CHARACTERS_TO_DISPLAY+1];	//	Add 1 for the NULL terminator

				const u32 numberOfCharsToDisplay = MIN(CPhotoManager::GetNumberOfOverrideCharactersToDisplay(), MAX_NUMBER_OF_U16_CHARACTERS_TO_DISPLAY);

				u32 arrayIndex = 0;
				while (arrayIndex < numberOfCharsToDisplay)
				{
					override_string[arrayIndex] = CPhotoManager::GetOverrideMemeCharacter() + (char16) arrayIndex;
					arrayIndex++;
				}
				override_string[arrayIndex] = 0;

				static char overridden_utf8_string[TITLE_BUFFER_LENGTH];

				int numConverted = 0;
				rage::WideToUtf8(overridden_utf8_string, override_string, NELEM(override_string), TITLE_BUFFER_LENGTH, &numConverted);

				pTopText = overridden_utf8_string;
			}
#endif	//	__BANK

			PHONEPHOTOEDITOR.SetText( pTopText, PHONEPHOTOEDITOR.GetBottomText(),
										position,
										c_topText ? textSize : PHONEPHOTOEDITOR.GetTopTextSize(), c_topText ? PHONEPHOTOEDITOR.GetBottomTextSize() : textSize,
										c_topText ? fontStyle : PHONEPHOTOEDITOR.GetTopTextStyle(), c_topText ? PHONEPHOTOEDITOR.GetBottomTextStyle() : fontStyle,
										c_topText ? finalColour : PHONEPHOTOEDITOR.GetTopColour(), c_topText ? PHONEPHOTOEDITOR.GetBottomColour() : finalColour );
		}

		if( sInput == PAD_CROSS )
		{
			++m_iMemeTextEntered;

			audioTrigger = FRONTEND_INPUT_ACCEPT;
			m_eMenuState = eGalleryState_ReviewMemeImage;
			SetMenuContext( m_iMemeTextEntered == 2 ? eMenuState_FinalizeTextMode : eMenuState_ReviewTextMode );

			CleanupMemeEditorAudio();

#if RSG_PC
			CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_ARROW);
#endif	//	RSG_PC
		}

		if( (sInput == PAD_CIRCLE) || bMouseRightClick )
		{
			audioTrigger = FRONTEND_INPUT_BACK;
			EnterMemeTextEntry( c_topText ? PHONEPHOTOEDITOR.GetTopText() : PHONEPHOTOEDITOR.GetBottomText() );
			PHONEPHOTOEDITOR.SetText( c_topText ? "" : PHONEPHOTOEDITOR.GetTopText(), "" );

			CleanupMemeEditorAudio();

#if RSG_PC
			CMousePointer::SetMouseCursorStyle(MOUSE_CURSOR_STYLE_ARROW);
#endif	//	RSG_PC
		}

		CPauseMenu::PlayInputSound( audioTrigger );

		return ( sInput == PAD_CROSS || sInput == PAD_CIRCLE || sInput == PAD_DPADUP || sInput == PAD_DPADDOWN || sInput == PAD_DPADLEFT || sInput == PAD_DPADRIGHT 
			|| sInput == PAD_LEFTSHOULDER2 || sInput == PAD_RIGHTSHOULDER2 || (mouseWheelDirection != 0) || bTextHasBeenDraggedWithMouse || bMouseRightClick );
	}
	else if(sInput == PAD_CROSS )
	{
		if( m_eMenuState == eGalleryState_ReviewMemeImage )
		{
			sm_indexOfPhotoForMemeMetadata = m_iSelectedIndex;
			sm_memePhotoSaveCallbackReturned = false;
			m_eGalleryActionState = GA_MEME_SAVE_MESSAGE;
			m_bWasMemeSaveTriggeredThisFrame = true;

			DetachMemeEditorTexture();
			CPauseMenu::SetGalleryLoadingTexture(true);
			SetMaximize(eGalleryItemState_Empty);

			PHONEPHOTOEDITOR.RequestSave( MakeFunctor( CGalleryMenu::RequestSaveMemePhotoCallback ) );
			m_eMenuState = eGalleryState_InMemeSaveProcess;

			return true;
		}
		else if ( m_pGalleryTexture && !SUIContexts::IsActive("GALLERY_DENY_FACEBOOK") )
		{
			CUndeletedEntryInMergedPhotoList undeletedEntry(m_iSelectedIndex, true);

			if (photoVerifyf(CPhotoManager::GetHasPhotoBeenUploadedToSocialClub(undeletedEntry), "CGalleryMenu::UpdateInput - trying to Post to Facebook when the photo hasn't been uploaded to Social Club yet"))
			{
				const char* szContentId = CPhotoManager::GetContentIdOfUploadedPhoto(undeletedEntry);

#if !__NO_OUTPUT
				if (szContentId)
				{
					photoDisplayf("CGalleryMenu::UpdateInput - try to post photo with Content Id %s to Facebook", szContentId);
				}
#endif	//	!__NO_OUTPUT

				bool bIsProfileEnabled = RL_FACEBOOK_SWITCH(CLiveManager::GetFacebookMgr().IsProfileSettingEnabled(), false);
				bool isBusy = RL_FACEBOOK_SWITCH(CLiveManager::GetFacebookMgr().IsBusy(), false);

				bool bIsUserPrivileged = CLiveManager::CheckOnlinePrivileges();

				if (bIsProfileEnabled && bIsUserPrivileged && !isBusy)
				{
					if (szContentId && (strlen(szContentId) > 0) )
					{
						RL_FACEBOOK_ONLY(CLiveManager::GetFacebookMgr().PostTakePhoto(szContentId));
						m_eGalleryActionState = GA_FACEBOOK_LOADING_IMAGE;
					}
					else
					{
						m_bWasFacebookErrorTriggeredThisFrame = true;
						m_eGalleryActionState = GA_FACEBOOK_ERROR;
					}
				}
				else if (!bIsUserPrivileged)
				{
					m_bWasFacebookErrorTriggeredThisFrame = true;
					m_eGalleryActionState = GA_NO_ONLINE_PRIVILEGE_PROMPT;
				}
				else if (!bIsProfileEnabled)
				{
					m_bWasFacebookErrorTriggeredThisFrame = true;
					m_eGalleryActionState = GA_FACEBOOK_ERROR_PROFILE_SETTING_FAIL;
				}
			}
		}
		else if ( m_eMenuState == eGalleryState_InMenu && m_ItemState[m_iSelectedIndexPerPage] != eGalleryItemState_Corrupted)
		{
			photoDisplayf("CGalleryMenu::UpdateInput - MAXIMIZE");

			MaximizeTheSelectedThumbnail();

			return true;
		}
		else if (m_eMenuState == eGalleryState_Invalid )
		{
			if (!StepIntoGalleryPage())
			{
				m_eMenuState = eGalleryState_InMenu;

				photoDisplayf("CGalleryMenu::UpdateInput - PAD_CROSS pressed - changing state to eGalleryState_InMenu");
				SetScrollBarCount();

			}
			return false;
		}
	}
	else if ( (sInput == PAD_CIRCLE) || bMouseRightClick)
	{
		// 2 States:  If maximized or in thumbnail view.
		if (m_pGalleryTexture)
		{
			if( m_eMenuState == eGalleryState_ReviewMemeImage )
			{
				--m_iMemeTextEntered;

				m_eMenuState = eGalleryState_PlaceMemeText;
				CPauseMenu::PlayInputSound(FRONTEND_INPUT_BACK);
				SetMenuContext( eMenuState_PlaceTextMode );

				return true;
			}
			else if (m_eMenuState == eGalleryState_ProfanityCheckName)
			{
				CancelProfanityCheck();

				return true;
			}
			else
			{
				ReturnToThumbnailView();

				CPauseMenu::PlayInputSound(FRONTEND_INPUT_BACK);
				return true;
			}
		}
		else if (m_eMenuState == eGalleryState_InMenu)
		{
			StepOutOfGalleryPage();
		}
	}
	else if ( sInput == PAD_TRIANGLE )
	{
		if( m_pGalleryTexture && m_eMenuState == eGalleryState_Maximize )
		{
			photoDisplayf("CGalleryMenu::UpdateInput - RENAME");
			const int maxTitleLength = 40;

			static char16 s_TitleForKeyboardUI[sm_maxKeyboardTitleLength];
			static char16 s_InitialValueForKeyboard[maxTitleLength];
			CUndeletedEntryInMergedPhotoList undeletedEntry(m_iSelectedIndex, true);

			Utf8ToWide(s_TitleForKeyboardUI, TheText.Get("GAL_RENAME_TITLE"), sm_maxKeyboardTitleLength );
			Utf8ToWide(s_InitialValueForKeyboard,CPhotoManager::GetTitleOfPhoto(undeletedEntry), TITLE_BUFFER_LENGTH);

			ioVirtualKeyboard::Params params;
			params.m_KeyboardType = ioVirtualKeyboard::kTextType_ALPHABET;
			params.m_MaxLength = maxTitleLength;
			params.m_Title = s_TitleForKeyboardUI;
			params.m_InitialValue = s_InitialValueForKeyboard;
			params.m_MultiLineUsage = ioVirtualKeyboard::Params::FORCE_SINGLE_LINE;

			m_eMenuState = eGalleryState_InKeyboardForName;

			CPauseMenu::PlayInputSound(FRONTEND_INPUT_Y);
			CControlMgr::ShowVirtualKeyboard(params);

			return true;
		}
		else if (m_eMenuState == eGalleryState_InMenu && !SUIContexts::IsActive("GALLERY_DISABLE_DELETE") )
		{
			photoDisplayf("CGalleryMenu::UpdateInput - DELETE");
			m_eGalleryActionState = GA_DELETE_CONFIRM;
			CPauseMenu::PlayInputSound(FRONTEND_INPUT_Y);

			return false;
		}
	}
	else if ( sInput == PAD_SQUARE && m_ItemState[m_iSelectedIndexPerPage] != eGalleryItemState_Corrupted )
	{
		if( m_pGalleryTexture && m_ItemState[m_iSelectedIndexPerPage] == eGalleryItemState_Loaded && 
			CanEnterMemeEditor() )
		{
#if __LOAD_LOCAL_PHOTOS
			const u32 maxPhotos = NUMBER_OF_LOCAL_PHOTOS;
#else
			const u32 maxPhotos = MAX_PHOTOS_TO_LOAD_FROM_CLOUD;
#endif
			if( CPhotoManager::GetNumberOfPhotos(false) >= maxPhotos )
			{
				m_eGalleryActionState = GA_MEME_GALLERY_FULL_MESSAGE;
				return true;
			}
			else if( m_eMenuState == eGalleryState_Maximize || ( m_eMenuState == eGalleryState_ReviewMemeImage && m_iMemeTextEntered < MAX_MEME_TEXTS ) ) 
			{
				EnterMemeTextEntry();
				CPauseMenu::PlayInputSound( FRONTEND_INPUT_X );
				return true;
			}
		}
		else if ( (m_eMenuState == eGalleryState_InMenu) && !SUIContexts::IsActive("GALLERY_DISABLE_WAYPOINT") )
		{
			if (sMiniMapMenuComponent.DoesBlipExist(m_iSelectedIndexPerPage))
			{
				sMiniMapMenuComponent.SetWaypoint();
			}
		}
	}
#if __LOAD_LOCAL_PHOTOS
	else if ( sInput == PAD_R3 )
	{
		if ( (m_eMenuState == eGalleryState_InMenu) && (m_ItemState[m_iSelectedIndexPerPage] != eGalleryItemState_Corrupted)  && !SUIContexts::IsActive("GALLERY_DISABLE_UPLOAD") )
		{
			m_UploadWarningType = CheckForGalleryProblems(true);
			if (m_UploadWarningType != GA_INVALID)
			{
				photoDisplayf("CGalleryMenu::UpdateInput - can't UPLOAD due to error type %d", (s32) m_UploadWarningType);
				m_eGalleryActionState = GA_UPLOAD_WARNING;
			}
			else
			{
				photoDisplayf("CGalleryMenu::UpdateInput - UPLOAD");
				m_eGalleryActionState = GA_UPLOAD_CONFIRM;
				CPauseMenu::PlayInputSound(FRONTEND_INPUT_ACCEPT);
			}

			return true;		//	I'll try returning true here. Setting m_eGalleryActionState = GA_DELETE_CONFIRM when the player presses Triangle returns false but very little else does
		}
	}
#endif	//	__LOAD_LOCAL_PHOTOS
	else if ( sInput == PAD_DPADRIGHT )
	{
		bool consumeInput = false;

		if( CPhotoManager::GetNumberOfPhotos(false) <= 1 || m_eMenuState == eGalleryState_ReviewMemeImage )
		{
			consumeInput = true;
		}
		else
		{
			CControlMgr::GetMainPlayerControl(false).DisableAllInputs(CPauseMenu::sm_uDisableInputDuration, ioValue::DisableOptions(ioValue::DisableOptions::F_ALLOW_SUSTAINED));
			if ( m_pGalleryTexture ) // maximized 
			{
				m_iMoveInMaximizeOldIndex = m_iSelectedIndex;

				if( IsLastElementInGallery( m_iSelectedIndex ) )
				{
					consumeInput = true;
					m_iSelectedIndex = 0;
					m_iSelectedIndexPerPage = 0;

					// If we only have a single page, then we just need to jump back to picture 0 and stay on the same page
					if ( m_iMaxNumberOfPages == 1 )
					{
						photoDisplayf("CGalleryMenu::UpdateInput - PAD_DPADRIGHT pressed - m_iSelectedIndex=%d, m_iSelectedIndexPerPage=%d", m_iSelectedIndex, m_iSelectedIndexPerPage);

						UpdateAfterSelectingAThumbnail(FRONTEND_INPUT_RIGHT, false, true);

						m_eGalleryActionState = GA_MAXIMIZE_PROGRESS_LOADING_SWAP_IMAGE;
						CleanupMemeEditor();
						m_pGalleryTexture = NULL;
						m_galleryTextureTxdId = -1;
						m_galleryTextureLocalIndex = -1;
					}
					else // Since we have multiple pages and are at the last item in the entire gallery, then we can just hop to page 0, item 0.
					{
						m_iCurrentPage = 0;

						CPauseMenu::PlayInputSound(FRONTEND_INPUT_RIGHT);

						photoDisplayf("CGalleryMenu::UpdateInput - PAD_DPADRIGHT pressed - calling Repopulate with m_iCurrentPage=%d", m_iCurrentPage);
						Repopulate( true );

						m_eGalleryActionState = GA_MAXIMIZE_PROGRESS_LOADING_NEW_PAGE_SWAP_IMAGE;
					}
				}
				else 
				{
					int iPotentialIndex = m_iSelectedIndex;
					int iPotentialIndexPerPage = m_iSelectedIndexPerPage;

					if (m_iSelectedIndexPerPage < MAX_ITEMS_ON_PAGE -1 )
					{
						iPotentialIndex++;
						iPotentialIndexPerPage++;
					}

					if( ShouldNavigateToNewPage( iPotentialIndexPerPage ) )
					{
						m_iCurrentPage = m_iCurrentPage == ( m_iMaxNumberOfPages - 1 ) ? 0 : m_iCurrentPage + 1;
						m_eGalleryActionState = GA_MAXIMIZE_PROGRESS_LOADING_NEW_PAGE_SWAP_IMAGE;
					}
					else if( CanMoveToRequestedEntry(iPotentialIndexPerPage) )
					{
						m_iSelectedIndex = iPotentialIndex;
						m_iSelectedIndexPerPage = iPotentialIndexPerPage;

						photoDisplayf("CGalleryMenu::UpdateInput - PAD_DPADRIGHT pressed - m_iSelectedIndex=%d, m_iSelectedIndexPerPage=%d", m_iSelectedIndex, m_iSelectedIndexPerPage);

						UpdateAfterSelectingAThumbnail(FRONTEND_INPUT_RIGHT, false, false);

						m_eGalleryActionState = GA_MAXIMIZE_PROGRESS_LOADING_SWAP_IMAGE;
						CleanupMemeEditor();
						m_pGalleryTexture = NULL;
						m_galleryTextureTxdId = -1;
						m_galleryTextureLocalIndex = -1;
					}
					else
					{
						consumeInput = true; // don't update scaleform input if we can't move to requested entry
					}
				}
			}
			else // thumbnail mode
			{		
				if ( IsLastElementInGallery( m_iSelectedIndex ) )
				{
					consumeInput = true;
					m_iSelectedIndex = 0;
					m_iSelectedIndexPerPage = 0;

					if (m_iMaxNumberOfPages == 1 )
					{
						photoDisplayf("CGalleryMenu::UpdateInput - PAD_DPADRIGHT pressed - m_iSelectedIndex=%d, m_iSelectedIndexPerPage=%d", m_iSelectedIndex, m_iSelectedIndexPerPage);

						UpdateAfterSelectingAThumbnail(FRONTEND_INPUT_RIGHT, true, true);
					}
					else
					{
						m_iCurrentPage = 0;

						CPauseMenu::PlayInputSound(FRONTEND_INPUT_RIGHT);

						photoDisplayf("CGalleryMenu::UpdateInput - PAD_DPADRIGHT pressed - calling Repopulate with m_iCurrentPage=%d", m_iCurrentPage);
						Repopulate( true );

						SetScrollBarCount();
					}
				}
				else
				{
					int iPotentialIndex = m_iSelectedIndex;
					int iPotentialIndexPerPage = m_iSelectedIndexPerPage;

					if( ( m_iSelectedIndexPerPage % MAX_COLUMNS_ON_PAGE < MAX_COLUMNS_ON_PAGE -1 ) || m_iMaxNumberOfPages <= 1 )
					{
						iPotentialIndex++;
						iPotentialIndexPerPage++;
					}

					if ( ShouldNavigateToNewPage(iPotentialIndexPerPage) )
					{
						consumeInput = true;

						m_iCurrentPage = m_iCurrentPage == ( m_iMaxNumberOfPages - 1 ) ? 0 : m_iCurrentPage + 1;

						m_iSelectedIndex = m_iCurrentPage * MAX_ITEMS_ON_PAGE;
						m_iSelectedIndexPerPage = 0;

						photoDisplayf("CGalleryMenu::UpdateInput - PAD_DPADRIGHT pressed - calling Repopulate with m_iCurrentPage=%d", m_iCurrentPage);
						Repopulate( true );
						SetScrollBarCount();

						CPauseMenu::PlayInputSound(FRONTEND_INPUT_RIGHT);
					}
					else if (CanMoveToRequestedEntry(iPotentialIndexPerPage) && iPotentialIndexPerPage != m_iSelectedIndexPerPage)
					{
						m_iSelectedIndex = iPotentialIndex;
						m_iSelectedIndexPerPage = iPotentialIndexPerPage;

						photoDisplayf("CGalleryMenu::UpdateInput - PAD_DPADRIGHT pressed - m_iSelectedIndex=i%d, m_iSelectedIndexPerPage=%d", m_iSelectedIndex, m_iSelectedIndexPerPage);

						UpdateAfterSelectingAThumbnail(FRONTEND_INPUT_RIGHT, true, false);
					}
					else
					{
						photoDisplayf("CGalleryMenu::UpdateInput - PAD_DPADRIGHT pressed - ");
						consumeInput = true;
					}
				}
			}
		}

		return consumeInput;
	}
	else if (sInput == PAD_DPADLEFT)
	{
		bool consumeInput = false;

		if( CPhotoManager::GetNumberOfPhotos(false) <= 1 || m_eMenuState == eGalleryState_ReviewMemeImage )
		{
			consumeInput = true;
		}
		else
		{
			CControlMgr::GetMainPlayerControl(false).DisableAllInputs(CPauseMenu::sm_uDisableInputDuration, ioValue::DisableOptions(ioValue::DisableOptions::F_ALLOW_SUSTAINED));
			if (m_pGalleryTexture) // maximized 
			{
				m_iMoveInMaximizeOldIndex = m_iSelectedIndex;

				if ( m_iSelectedIndex == 0 )
				{
					consumeInput = true;
					m_iSelectedIndex = CPhotoManager::GetNumberOfPhotos(false)-1;
					m_iSelectedIndexPerPage = m_iSelectedIndex % MAX_ITEMS_ON_PAGE;

					if ( m_iMaxNumberOfPages == 1 )
					{
						photoDisplayf("CGalleryMenu::UpdateInput - PAD_DPADLEFT pressed - m_iSelectedIndex=%d, m_iSelectedIndexPerPage=%d", m_iSelectedIndex, m_iSelectedIndexPerPage);

						UpdateAfterSelectingAThumbnail(FRONTEND_INPUT_LEFT, false, true);

						m_eGalleryActionState = GA_MAXIMIZE_PROGRESS_LOADING_SWAP_IMAGE;
						CleanupMemeEditor();
						m_pGalleryTexture = NULL;
						m_galleryTextureTxdId = -1;
						m_galleryTextureLocalIndex = -1;
					}
					else
					{
						m_iCurrentPage = m_iMaxNumberOfPages-1;

						CPauseMenu::PlayInputSound(FRONTEND_INPUT_LEFT);

						//	This will lead to the GA_MAXIMIZE_PROGRESS_LOADING_NEW_PAGE_SWAPPING_IMAGE2 case which sets
						//	m_iSelectedIndex = m_iCurrentPage * MAX_ITEMS_ON_PAGE;
						//	m_iSelectedIndexPerPage = 0;

						photoDisplayf("CGalleryMenu::UpdateInput - PAD_DPADLEFT pressed - calling Repopulate with m_iCurrentPage=%d", m_iCurrentPage);
						m_eGalleryActionState = GA_MAXIMIZE_PROGRESS_LOADING_NEW_PAGE_SWAP_IMAGE;
					}
				}
				else
				{
					int iPotentialIndex = m_iSelectedIndex;
					int iPotentialIndexPerPage = m_iSelectedIndexPerPage;

					if (m_iSelectedIndexPerPage > 0 )
					{
						iPotentialIndex--;
						iPotentialIndexPerPage--;
					}

					if (ShouldNavigateToNewPage(iPotentialIndexPerPage))
					{
						m_iCurrentPage = m_iCurrentPage == 0 ? ( m_iMaxNumberOfPages - 1 ) : m_iCurrentPage - 1;
						m_eGalleryActionState = GA_MAXIMIZE_PROGRESS_LOADING_NEW_PAGE_SWAP_IMAGE;
						CPauseMenu::PlayInputSound(FRONTEND_INPUT_LEFT);
						consumeInput = true;
					}
					else if (CanMoveToRequestedEntry(iPotentialIndexPerPage) && iPotentialIndexPerPage != m_iSelectedIndexPerPage)
					{
						m_iSelectedIndex = iPotentialIndex;
						m_iSelectedIndexPerPage = iPotentialIndexPerPage;

						photoDisplayf("CGalleryMenu::UpdateInput - PAD_DPADLEFT pressed - m_iSelectedIndex=%d, m_iSelectedIndexPerPage=%d", m_iSelectedIndex, m_iSelectedIndexPerPage);

						UpdateAfterSelectingAThumbnail(FRONTEND_INPUT_LEFT, false, false);

						m_eGalleryActionState = GA_MAXIMIZE_PROGRESS_LOADING_SWAP_IMAGE;
						CleanupMemeEditor();
						m_pGalleryTexture = NULL;
						m_galleryTextureTxdId = -1;
						m_galleryTextureLocalIndex = -1;

					}
					else
					{
						consumeInput = true;
					}
				}
			}
			else // thumbnail
			{
				if( m_iSelectedIndex == 0 )
				{
					consumeInput = true;

					if ( m_iMaxNumberOfPages == 1 )
					{
						m_iSelectedIndex = CPhotoManager::GetNumberOfPhotos(false)-1;
						m_iSelectedIndexPerPage = m_iSelectedIndex % MAX_ITEMS_ON_PAGE;

						photoDisplayf("CGalleryMenu::UpdateInput - PAD_DPADLEFT pressed - m_iSelectedIndex=%d, m_iSelectedIndexPerPage=%d", m_iSelectedIndex, m_iSelectedIndexPerPage);

						bool bSetColumnHighlight = false;
						if( CanMoveToRequestedEntry( m_iSelectedIndexPerPage ) )
						{
							bSetColumnHighlight = true;
						}

						UpdateAfterSelectingAThumbnail(FRONTEND_INPUT_LEFT, true, bSetColumnHighlight);
					}
					else
					{
						m_iSelectedIndex = MAX_ITEMS_ON_PAGE * (m_iMaxNumberOfPages -1);
						m_iSelectedIndexPerPage = 0;
						m_iCurrentPage = m_iMaxNumberOfPages-1;

						CPauseMenu::PlayInputSound(FRONTEND_INPUT_LEFT);

						photoDisplayf("CGalleryMenu::UpdateInput - PAD_DPADLEFT pressed - calling Repopulate with m_iCurrentPage=%d", m_iCurrentPage);
						Repopulate( true );

						SetScrollBarCount();
					}
				}
				else
				{
					int iPotentialIndex = m_iSelectedIndex;
					int iPotentialIndexPerPage = m_iSelectedIndexPerPage;

					if ( m_iSelectedIndexPerPage % MAX_COLUMNS_ON_PAGE > 0 || ( m_iMaxNumberOfPages <= 1 ) )
					{
						iPotentialIndex--;
						iPotentialIndexPerPage--;
					}

					if( ShouldNavigateToNewPage(iPotentialIndexPerPage) )
					{
						consumeInput = true;

						m_iCurrentPage = m_iCurrentPage <= 0 ? ( m_iMaxNumberOfPages - 1 ) : m_iCurrentPage - 1;

						m_iSelectedIndex = m_iCurrentPage * MAX_ITEMS_ON_PAGE;
						m_iSelectedIndexPerPage = 0;

						photoDisplayf("CGalleryMenu::UpdateInput - PAD_DPADLEFT pressed - calling Repopulate with m_iCurrentPage=%d", m_iCurrentPage);
						Repopulate( true );

						SetScrollBarCount();
						CPauseMenu::PlayInputSound(FRONTEND_INPUT_LEFT);
					}
					else if (CanMoveToRequestedEntry(iPotentialIndexPerPage)  && iPotentialIndexPerPage != m_iSelectedIndexPerPage)
					{
						m_iSelectedIndex = iPotentialIndex;
						m_iSelectedIndexPerPage = iPotentialIndexPerPage;

						photoDisplayf("CGalleryMenu::UpdateInput - PAD_DPADLEFT pressed - m_iSelectedIndex=%d, m_iSelectedIndexPerPage=%d", m_iSelectedIndex, m_iSelectedIndexPerPage);

						UpdateAfterSelectingAThumbnail(FRONTEND_INPUT_LEFT, true, false);
					}
					else
					{
						photoDisplayf("CGalleryMenu::UpdateInput - PAD_DPADLEFT pressed ");
						consumeInput = true;
					}
				}
			}
		}

		return consumeInput;
	}
	else if (sInput == PAD_DPADUP )
	{
		if (  m_eMenuState != eGalleryState_InMenu )
		{
			return true;
		}

		CControlMgr::GetMainPlayerControl(false).DisableAllInputs(CPauseMenu::sm_uDisableInputDuration, ioValue::DisableOptions(ioValue::DisableOptions::F_ALLOW_SUSTAINED));

		int iPotentialIndex = m_iSelectedIndex;
		int iPotentialIndexPerPage = m_iSelectedIndexPerPage;

		if (m_iSelectedIndexPerPage > 3 )
		{
			iPotentialIndex-=MAX_COLUMNS_ON_PAGE;
			iPotentialIndexPerPage-= MAX_COLUMNS_ON_PAGE;
		}
		else
		{
			iPotentialIndex+= (MAX_COLUMNS_ON_PAGE*(MAX_ROWS_ON_PAGE-1));
			iPotentialIndexPerPage+= (MAX_COLUMNS_ON_PAGE*(MAX_ROWS_ON_PAGE-1));
		}
		
		if (CanMoveToRequestedEntry(iPotentialIndexPerPage) )
		{
			m_iSelectedIndex = iPotentialIndex;
			m_iSelectedIndexPerPage = iPotentialIndexPerPage;

			photoDisplayf("CGalleryMenu::UpdateInput - PAD_DPADUP pressed - m_iSelectedIndex=%d, m_iSelectedIndexPerPage=%d", m_iSelectedIndex, m_iSelectedIndexPerPage);

			UpdateAfterSelectingAThumbnail(FRONTEND_INPUT_LEFT, true, false);
		}
		else
		{
			photoDisplayf("CGalleryMenu::UpdateInput - PAD_DPADUP pressed - ");
			return true;
		}
		
	}
	else if (sInput == PAD_DPADDOWN)
	{
		if ( m_eMenuState != eGalleryState_InMenu )
		{
			return true;
		}

		CControlMgr::GetMainPlayerControl(false).DisableAllInputs(CPauseMenu::sm_uDisableInputDuration, ioValue::DisableOptions(ioValue::DisableOptions::F_ALLOW_SUSTAINED));

		int iPotentialIndex = m_iSelectedIndex;
		int iPotentialIndexPerPage = m_iSelectedIndexPerPage;

		if (m_iSelectedIndexPerPage < 8 )
		{
			iPotentialIndex+= MAX_COLUMNS_ON_PAGE;
			iPotentialIndexPerPage+= MAX_COLUMNS_ON_PAGE;
		}
		else
		{
			iPotentialIndex-= (MAX_COLUMNS_ON_PAGE*(MAX_ROWS_ON_PAGE-1));
			iPotentialIndexPerPage-= (MAX_COLUMNS_ON_PAGE*(MAX_ROWS_ON_PAGE-1));
		}

		if (CanMoveToRequestedEntry(iPotentialIndexPerPage) )
		{
			m_iSelectedIndex = iPotentialIndex;
			m_iSelectedIndexPerPage = iPotentialIndexPerPage;

			photoDisplayf("CGalleryMenu::UpdateInput - PAD_DPADDOWN pressed - m_iSelectedIndex=%d, m_iSelectedIndexPerPage=%d", m_iSelectedIndex, m_iSelectedIndexPerPage);
			
			UpdateAfterSelectingAThumbnail(FRONTEND_INPUT_LEFT, true, false);
		}
		else
		{
			photoDisplayf("CGalleryMenu::UpdateInput - PAD_DPADDOWN pressed - ");

			return true;
		}

	}

	return false;	
}


#if RSG_PC
bool CGalleryMenu::IsValidMenuUniqueIdForAPhoto(s32 menuUniqueId)
{
	if ((menuUniqueId >= -PREF_OPTIONS_THRESHOLD) && (menuUniqueId < (MAX_ITEMS_ON_PAGE - PREF_OPTIONS_THRESHOLD)) )
	{
		return true;
	}

	return false;
}

void CGalleryMenu::SetIndexOfPhotoSelectedWithMouse(s32 menuUniqueId)
{
	if (photomouseVerifyf(IsValidMenuUniqueIdForAPhoto(menuUniqueId), "CGalleryMenu::SetIndexOfPhotoSelectedWithMouse - menuUniqueId = %d. It's not a valid index for a photo thumbnail", menuUniqueId))
	{
		s32 indexWithinPage = menuUniqueId + PREF_OPTIONS_THRESHOLD;
		if (photomouseVerifyf((indexWithinPage >= 0) && (indexWithinPage < m_ItemState.GetCount()), "CGalleryMenu::SetIndexOfPhotoSelectedWithMouse - indexWithinPage = %d. Expected it to be between 0 and %d", indexWithinPage, m_ItemState.GetCount()))
		{
			if (photomouseVerifyf(CanMoveToRequestedEntry(indexWithinPage), "CGalleryMenu::SetIndexOfPhotoSelectedWithMouse - CanMoveToRequestedEntry() returned false for indexWithinPage = %d. m_ItemState[indexWithinPage] = %d", indexWithinPage, (s32) m_ItemState[indexWithinPage]))
			{
				int newSelectedIndex = (MAX_ITEMS_ON_PAGE * m_iCurrentPage) + indexWithinPage;
				
				eFRONTEND_INPUT inputSound = FRONTEND_INPUT_MAX;
				if ( (m_iSelectedIndexPerPage != indexWithinPage) || (m_iSelectedIndex != newSelectedIndex) )
				{	//	Don't play a sound if this photo is already selected
					inputSound = FRONTEND_INPUT_LEFT;
				}

				m_iSelectedIndex = newSelectedIndex;
				m_iSelectedIndexPerPage = indexWithinPage;

				photoDisplayf("CGalleryMenu::SetIndexOfPhotoSelectedWithMouse m_iSelectedIndex is now %d. m_iSelectedIndexPerPage is now %d", m_iSelectedIndex, m_iSelectedIndexPerPage);

				UpdateAfterSelectingAThumbnail(inputSound, true, true);
			}
		}
	}
}

// interrupts. If true is returned, no further action is performed
bool CGalleryMenu::CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args)
{
	if (methodName == ATSTRINGHASH("MENU_SHIFT_DEPTH_PROCESSED",0xe07398c6))
	{
		photomouseDisplayf("CGalleryMenu::CheckIncomingFunctions - MENU_SHIFT_DEPTH_PROCESSED");
	}

	if (methodName == ATSTRINGHASH("TRIGGER_PAUSE_MENU_EVENT",0x51556734))
	{
		if (photomouseVerifyf(args[1].IsNumber() && args[2].IsNumber() && args[3].IsNumber(), "CGalleryMenu::CheckIncomingFunctions - TRIGGER_PAUSE_MENU_EVENT params not compatible: %s %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2]), sfScaleformManager::GetTypeName(args[3]) ))
		{
			MenuScreenId iTriggerId((s32)args[1].GetNumber() - PREF_OPTIONS_THRESHOLD);
			s32 iMenuIndex = (s32)args[2].GetNumber();

			s32 iNewMenuIndex = args[3].IsDefined() ? (s32)args[3].GetNumber() : MENU_UNIQUE_ID_INVALID;

			if (iNewMenuIndex != MENU_UNIQUE_ID_INVALID)
			{
				iNewMenuIndex -= PREF_OPTIONS_THRESHOLD;
			}

			photomouseDisplayf("CGalleryMenu::CheckIncomingFunctions - TRIGGER_PAUSE_MENU_EVENT - Actionscript requested %d is triggered with menu index of %d. New menu index is %d", iTriggerId.GetValue(), iMenuIndex, iNewMenuIndex);

			if (iTriggerId.GetValue() == MENU_UNIQUE_ID_GALLERY)
			{
				if (IsValidMenuUniqueIdForAPhoto(iNewMenuIndex))
				{
					StepIntoGalleryPage();
					return true;
				}
			}
			else if (iMenuIndex == MENU_UNIQUE_ID_GALLERY_ITEM)
			{
				if ( (m_eMenuState == eGalleryState_InMenu) && (m_eGalleryActionState == GA_INVALID) )
				{
					if (IsValidMenuUniqueIdForAPhoto(iTriggerId.GetValue()))
					{
						s32 indexWithinPage = iTriggerId.GetValue() + PREF_OPTIONS_THRESHOLD;
						if (photomouseVerifyf((indexWithinPage >= 0) && (indexWithinPage < m_ItemState.GetCount()), "CGalleryMenu::CheckIncomingFunctions - TRIGGER_PAUSE_MENU_EVENT - indexWithinPage = %d. Expected it to be between 0 and %d", indexWithinPage, m_ItemState.GetCount()))
						{
							photomouseDisplayf("CGalleryMenu::CheckIncomingFunctions - TRIGGER_PAUSE_MENU_EVENT - indexWithinPage = %d m_iSelectedIndexPerPage = %d", indexWithinPage, m_iSelectedIndexPerPage);
							if (indexWithinPage == m_iSelectedIndexPerPage)
							{
								if (m_ItemState[m_iSelectedIndexPerPage] == eGalleryItemState_Loaded )
								{
									photomouseDisplayf("CGalleryMenu::CheckIncomingFunctions - TRIGGER_PAUSE_MENU_EVENT - m_ItemState[%d] is eGalleryItemState_Loaded so call MaximizeTheSelectedThumbnail()", m_iSelectedIndexPerPage);
									MaximizeTheSelectedThumbnail();
									return true;
								}
								else
								{
									photomouseDisplayf("CGalleryMenu::CheckIncomingFunctions - TRIGGER_PAUSE_MENU_EVENT - m_ItemState[%d] is not eGalleryItemState_Loaded, it's %d so don't maximize the photo", m_iSelectedIndexPerPage, m_ItemState[m_iSelectedIndexPerPage]);
								}
							}
						}
					}
				}
				else
				{
					photomouseDisplayf("CGalleryMenu::CheckIncomingFunctions - TRIGGER_PAUSE_MENU_EVENT - don't attempt to maximize a photo because we're not in the thumbnail view. m_eMenuState = %d. m_eGalleryActionState = %d", m_eMenuState, m_eGalleryActionState);
				}
			}
		}
	}

#if RSG_PC
	if (methodName == ATSTRINGHASH("HANDLE_SCROLL_CLICK", 0x9CE8ECE9))
	{
		if (uiVerifyf(args[1].IsNumber(), "HANDLE_SCROLL_CLICK params not compatible: %s %s", sfScaleformManager::GetTypeName(args[1])))
		{
			if (m_pGalleryTexture) // maximized
			{
				s32 iInput = PAD_NO_BUTTON_PRESSED;
				if((s32)args[1].GetNumber() == SCROLL_CLICK_DOWN)
				{
					iInput = PAD_DPADLEFT;
				}
				if((s32)args[1].GetNumber() == SCROLL_CLICK_UP)
				{
					iInput = PAD_DPADRIGHT;
				}

				UpdateInput(iInput);
			}
			else // thumbnail
			{
				if( (CPhotoManager::GetNumberOfPhotos(false) >= 1 && m_eMenuState != eGalleryState_ReviewMemeImage) && m_iMaxNumberOfPages > 1)
				{
					if((s32)args[1].GetNumber() == SCROLL_CLICK_DOWN)
					{
						m_iCurrentPage = m_iCurrentPage <= 0 ? ( m_iMaxNumberOfPages - 1 ) : m_iCurrentPage - 1;
						m_iSelectedIndex = m_iCurrentPage * MAX_ITEMS_ON_PAGE;
						m_iSelectedIndexPerPage = 0;

						photoDisplayf("CGalleryMenu::HANDLE_SCROLL_CLICK - Left Arrow clicked - calling Repopulate with m_iCurrentPage=%d", m_iCurrentPage);
						Repopulate( true );
						SetScrollBarCount();
						CPauseMenu::PlayInputSound(FRONTEND_INPUT_LEFT);
					}
					else if((s32)args[1].GetNumber() == SCROLL_CLICK_UP)
					{
						m_iCurrentPage = m_iCurrentPage == ( m_iMaxNumberOfPages - 1 ) ? 0 : m_iCurrentPage + 1;
						m_iSelectedIndex = m_iCurrentPage * MAX_ITEMS_ON_PAGE;
						m_iSelectedIndexPerPage = 0;

						photoDisplayf("CGalleryMenu::HANDLE_SCROLL_CLICK - Right Arrow clicked - calling Repopulate with m_iCurrentPage=%d", m_iCurrentPage);
						Repopulate( true );
						SetScrollBarCount();
						CPauseMenu::PlayInputSound(FRONTEND_INPUT_RIGHT);
					}
				}
			}
		}
		return true;
	}
#endif // RSG_PC


	if (methodName == ATSTRINGHASH("LAYOUT_CHANGED",0xd9550e07) 
//		|| methodName == ATSTRINGHASH("LAYOUT_CHANGED_FOR_SCRIPT_ONLY",0x02cbf996) 
//		|| methodName == ATSTRINGHASH("LAYOUT_CHANGED_NO_LOAD",0x43ef4af5)
//		|| methodName == ATSTRINGHASH("LAYOUT_CHANGE_INITIAL_FILL",0x8376a368) 
		)
	{
		if (photomouseVerifyf(args[1].IsNumber() && args[2].IsNumber() && args[3].IsNumber() && args[4].IsNumber(), "CGalleryMenu::CheckIncomingFunctions - LAYOUT_CHANGED params not compatible: %s %s %s %s", sfScaleformManager::GetTypeName(args[1]), sfScaleformManager::GetTypeName(args[2]), sfScaleformManager::GetTypeName(args[3]), sfScaleformManager::GetTypeName(args[4])))
		{
			MenuScreenId iPreviousLayout((s32)args[1].GetNumber() - PREF_OPTIONS_THRESHOLD);
			MenuScreenId iNewLayout((s32)args[2].GetNumber() - PREF_OPTIONS_THRESHOLD); // dont use iNewLayout yet so dont initialise it yet in code
			s32 iUniqueId = (s32)args[3].GetNumber();
#if !__NO_OUTPUT
			s32 iDirection = (s32)args[4].GetNumber();
#endif	//	!__NO_OUTPUT

			photomouseDisplayf("CGalleryMenu::CheckIncomingFunctions - LAYOUT_CHANGED - iPreviousLayout=%d, iNewLayout=%d, iUniqueId=%d, iDirection=%d", iPreviousLayout.GetValue(), iNewLayout.GetValue(), iUniqueId, iDirection);

			if (iNewLayout.GetValue() == MENU_UNIQUE_ID_GALLERY)
			{	//	example - iPreviousLayout=-994, iNewLayout=3, iUniqueId=-1, iDirection=-1
				if (IsValidMenuUniqueIdForAPhoto(iPreviousLayout.GetValue()))
				{
					photomouseAssertf(iDirection == -1, "CGalleryMenu::CheckIncomingFunctions - LAYOUT_CHANGED - iDirection=%d. Expected it to be -1 when moving back out of the gallery page", iDirection);

					StepOutOfGalleryPage();
					return true;
				}
			}

			if (iUniqueId == MENU_UNIQUE_ID_GALLERY_ITEM)
			{	// example - iPreviousLayout=-996, iNewLayout=-994, iUniqueId=33, iDirection=0
				if (IsValidMenuUniqueIdForAPhoto(iPreviousLayout.GetValue()) && IsValidMenuUniqueIdForAPhoto(iNewLayout.GetValue()))
				{
					photomouseAssertf(iDirection == 0, "CGalleryMenu::CheckIncomingFunctions - LAYOUT_CHANGED - iDirection=%d. Expected it to be 0 when clicking on a photo thumbnail", iDirection);

					SetIndexOfPhotoSelectedWithMouse(iNewLayout.GetValue());

					return true;
				}
			}
		}
	}


	if (methodName == ATSTRINGHASH("IS_NAVIGATING_CONTENT",0x68249689))
	{
		if (photomouseVerifyf(args[1].IsBool(), "CGalleryMenu::CheckIncomingFunctions - IS_NAVIGATING_CONTENT params not compatible: %s", sfScaleformManager::GetTypeName(args[1])))
		{
			bool bNavigatingContent = args[1].GetBool();

			photomouseDisplayf("CGalleryMenu::CheckIncomingFunctions - IS_NAVIGATING_CONTENT - bNavigatingContent=%s", bNavigatingContent?"true":"false");

			if (bNavigatingContent)
			{
			}
		}
	}

	return false;
}
#endif	//	RSG_PC


//=================================================================
// CGalleryMenu - IsSelectedIndexCorrupted
// Purpose: Tests to see if an image is registered as being corrupt
//=================================================================
bool CGalleryMenu::IsSelectedIndexCorrupted()
{
	for (int i = 0; i < m_ItemState.GetCount(); i++)
	{
		if (m_ItemState[i] == eGalleryItemState_Corrupted)
			return true;
	}

	return false;
}

bool CGalleryMenu::Prepopulate(MenuScreenId newScreenId) 
{
	if(newScreenId.GetValue() != MENU_UNIQUE_ID_GALLERY)
	{
		return false;
	}

	photoDisplayf("CGalleryMenu::Prepopulate");
	CMapMenu::SetMapZoom(ZOOM_LEVEL_GALLERY,true);

//	Graeme - 4th August 2014 - I'm going to see if we can do without updating GALLERY_DENY_FACEBOOK here. I'll do it in CGalleryMenu::SetMaximize() instead. The "Post to Facebook" button is only ever displayed in maximized view.
// 	if ( (!CLiveManager::CheckOnlinePrivileges()) || (!CLiveManager::GetFacebookMgr().CanReportToFacebook()) )  // fix for 1655147 - we need to make sure that even if we are linked to facebook, we can actually post (need a Gold account for this), otherwise dont show button press
// 	{
// 		SUIContexts::Activate("GALLERY_DENY_FACEBOOK");
// 	}
// 	else
// 	{
// 		if (SUIContexts::IsActive("GALLERY_DENY_FACEBOOK"))
// 		{
// 			SUIContexts::Deactivate("GALLERY_DENY_FACEBOOK");
// 		}
// 	}

	return true; 
}


bool CGalleryMenu::Populate(MenuScreenId newScreenId)
{
	if(newScreenId.GetValue() != MENU_UNIQUE_ID_GALLERY)
	{
		return false;
	}


	if (!g_GalleryDlgt.IsBound())
	{
		g_GalleryDlgt.Bind(CGalleryMenu::OnPresenceEvent);
		rlPresence::AddDelegate(&g_GalleryDlgt);
		m_bIsInPrologue = CMiniMap::GetInPrologue();   // Intention is to be called only once on entry.

		CMiniMap::SetInPrologue(false);
	}

	if ( SetErrorPages(m_bErrorOnEmpty, sm_bDoOnlineErrorChecks) )
	{
		photoDisplayf("CGalleryMenu::GALLERY_SCAN_PROGRESS_WAIT_FOR_SORTED_LIST - Not signed in to console network");
		return false;
	}

	if (!CPauseMenu::IsNavigatingContent() && !sMiniMapMenuComponent.IsActive())
	{
		

		CScaleformMenuHelper::HIDE_COLUMN_SCROLL(PM_COLUMN_LEFT);
		sMiniMapMenuComponent.InitClass();

		SetDescription(true);
	}

	if (!m_bBuiltGalleryList && CPauseMenu::IsNavigatingContent())
	{
		m_GalleryScanProgress = GALLERY_SCAN_PROGRESS_CREATE_SORTED_LIST;

		photoDisplayf("CGalleryMenu::Populate - about to call Prepopulate");
		return Prepopulate(newScreenId);
	}


	if (CPauseMenu::IsNavigatingContent())
		SetScrollBarCount();

	if (m_bPaging)
	{
		SetMenuContext(eMenuState_MaximizeMode);
	}
	else
	{
		SetMenuContext(eMenuState_ThumbnailMode);
	}


	ClearPhotoSlots();

	if (CPhotoManager::GetNumberOfPhotos(false) == 0 && m_bErrorOnEmpty)
	{
		photoDisplayf("CGalleryMenu::Update - eGalleryState_InDeleteProcess. No Photos left.  Displaying Empty Screen.");
		return SetErrorPages(true, sm_bDoOnlineErrorChecks);
	}
	else
	{

		CScaleformMenuHelper::DISPLAY_DATA_SLOT( PM_COLUMN_LEFT );	
	
		CMapMenu::SetMapZoom(ZOOM_LEVEL_GALLERY, true);
		// we need the centre and north blips to be set up for pausemap as soon as the pausemap starts updating, so force through and update of
		// these particular blips here
		CMiniMap::UpdateCentreAndNorthBlips();
	}

	m_bErrorOnEmpty = true;

	return true;
}

const char *CGalleryMenu::GetWarningScreenTitle(eGalleryActionState eErrorState)
{
	switch (eErrorState)
	{
		case GA_GALLERY_EMPTY_NOT_CONNECTED_TO_SOCIAL_CLUB:
		case GA_GALLERY_EMPTY :
			return "ERROR_EMPTY_TITLE";
//			break;

		case GA_SOCIALCLUB_NOT_LINK_CONNECTED:
		case GA_SOCIALCLUB_IS_AGE_RESTRICTED :
		case GA_NO_SOCIAL_SHARING:
		case GA_PENDING_SYSTEM_UPDATE:
#if RSG_ORBIS
		case GA_NO_PLATFORM_SUBSCRIPTION :
#endif	//	RSG_ORBIS
		case GA_NO_USER_CONTENT_PRIVILEGES :
			return "CWS_WARNING";
//			break;

		case GA_SOCIALCLUB_NOT_CONNECTED_TO_SOCIAL_CLUB :
		case GA_SOCIALCLUB_ONLINE_POLICY_IS_NOT_UP_TO_DATE :
		case GA_SOCIALCLUB_NOT_ONLINE_ROS:
			return "ERROR_GAL_HDR";
//			break;

		case GA_NOT_SIGNED_IN:
		case GA_NOT_SIGNED_IN_LOCALLY :
			return "WARNING_NOT_CONNECTED_TITLE";
//			break;

//		case eErrorState_NoOnlinePrivilege:	return "CWS_WARNING";
//		case eErrorState_NoPlatformSubscription: return "CWS_WARNING";

		default :
			break;
	}

	return "CWS_WARNING";		//	eErrorState_UnknownError
}

const char *CGalleryMenu::GetWarningScreenString(eGalleryActionState eErrorState, bool bWarningScreen)
{
	switch (eErrorState)
	{
	case GA_GALLERY_EMPTY_NOT_CONNECTED_TO_SOCIAL_CLUB:
		if (bWarningScreen)
		{
			return "GAL_WAR_EMP_LOCAL_NO_SC";
		}
		return "ERROR_EMPTY_LOCAL_NO_SC";
	case GA_GALLERY_EMPTY :
#if __LOAD_LOCAL_PHOTOS
		return "ERROR_EMPTY_LOCAL";
#else	//	__LOAD_LOCAL_PHOTOS
		return "ERROR_EMPTY";
#endif	//	__LOAD_LOCAL_PHOTOS
//		break;

	case GA_SOCIALCLUB_NOT_LINK_CONNECTED:
		return "GAL_HUD_NOCONNECT";
//		break;

	case GA_SOCIALCLUB_IS_AGE_RESTRICTED :
		return "HUD_AGERES";
//		break;

	case GA_SOCIALCLUB_NOT_CONNECTED_TO_SOCIAL_CLUB :
#if __LOAD_LOCAL_PHOTOS
		if (bWarningScreen)
		{
			return "GAL_WAR_NO_SC_LOCAL";
		}
		return "ERROR_NO_SC_LOCAL";
#else	//	__LOAD_LOCAL_PHOTOS
		return "ERROR_NO_SC";
#endif	//	__LOAD_LOCAL_PHOTOS
//		break;

	case GA_SOCIALCLUB_ONLINE_POLICY_IS_NOT_UP_TO_DATE :
#if __LOAD_LOCAL_PHOTOS
		if (bWarningScreen)
		{
			return "GAL_WAR_UPDT_SC_LOCAL";
		}
		return "ERROR_UPDATE_SC_LOCAL";
#else	//	__LOAD_LOCAL_PHOTOS
		return "ERROR_UPDATE_SC";
#endif	//	__LOAD_LOCAL_PHOTOS
//		break;

	case GA_SOCIALCLUB_NOT_ONLINE_ROS:
		return "SCLB_NO_ROS";
//		break;

	case GA_NOT_SIGNED_IN:
		if (bWarningScreen)
		{
			return "GAL_WAR_NO_CONN";
		}
		return "NOT_CONNECTED";
//		break;

	case GA_NOT_SIGNED_IN_LOCALLY :
		return "NOT_CONNECTED_LOCAL";
//		break;

	case GA_NO_SOCIAL_SHARING:
#if __LOAD_LOCAL_PHOTOS
		return "ERROR_GAL_SHARING_LOCAL";
#else	//	__LOAD_LOCAL_PHOTOS
		return "ERROR_GAL_SHARING";
#endif	//	__LOAD_LOCAL_PHOTOS
//		break;

	case GA_PENDING_SYSTEM_UPDATE:
#if __BANK
		if (CPhotoManager::sm_bPhotoNetworkErrorPendingSystemUpdate)
		{
			return "HUD_SYS_UPD_RQ";
		}
		else
#endif	//	__BANK
		{
			const char *pOfflineReason = CPauseMenu::GetOfflineReasonAsLocKey();

			if (bWarningScreen)
			{
				if (strcmp(pOfflineReason, "NOT_CONNECTED") == 0)
				{
					return "GAL_WAR_NO_CONN";
				}
			}

			return pOfflineReason;
		}
//		break;

//	case eErrorState_NoOnlinePrivilege: return "HUD_PERM";

#if RSG_ORBIS
	case GA_NO_PLATFORM_SUBSCRIPTION :
		return "GAL_PSPLUS";
//		break;
#endif	//	RSG_ORBIS

	case GA_NO_USER_CONTENT_PRIVILEGES :
		return "GAL_WAR_PERM";
//		break;

	default :
		break;
	}

	return "SCLB_NO_ROS";		//	eErrorState_UnknownError
}

//==================================================================
// CGalleryMenu - DisplayWarningScreen
// Purpose: State for setting parameters
//==================================================================
void CGalleryMenu::DisplayWarningScreen(eGalleryActionState eErrorState)
{
	const char* pszTitle = GetWarningScreenTitle(eErrorState);
	const char* pszString = GetWarningScreenString(eErrorState, false);
	switch(eErrorState)
	{
		case GA_GALLERY_EMPTY:
		case GA_SOCIALCLUB_NOT_LINK_CONNECTED:
		case GA_SOCIALCLUB_IS_AGE_RESTRICTED :
		case GA_NO_SOCIAL_SHARING:
		case GA_PENDING_SYSTEM_UPDATE:
		case GA_NO_USER_CONTENT_PRIVILEGES :
			SUIContexts::Activate("HIDE_ACCEPTBUTTON");
			break;

		case GA_GALLERY_EMPTY_NOT_CONNECTED_TO_SOCIAL_CLUB:
		case GA_SOCIALCLUB_NOT_CONNECTED_TO_SOCIAL_CLUB :
		case GA_SOCIALCLUB_ONLINE_POLICY_IS_NOT_UP_TO_DATE :
		case GA_SOCIALCLUB_NOT_ONLINE_ROS:
#if RSG_ORBIS
		case GA_NO_PLATFORM_SUBSCRIPTION :
#endif	//	RSG_ORBIS
			break;

		case GA_NOT_SIGNED_IN:
			SUIContexts::Activate("HIDE_ACCEPTBUTTON");
			break;

		case GA_NOT_SIGNED_IN_LOCALLY :
#if RSG_ORBIS // B*1817634 - Cannot show Sign-in UI on PS4
			SUIContexts::Activate("HIDE_ACCEPTBUTTON");
#endif
			break;

// 		case eErrorState_NoOnlinePrivilege:				//	Nothing was setting this
// 			SUIContexts::Activate("HIDE_ACCEPTBUTTON");
// 			break;

// 		case eErrorState_NoPlatformSubscription:		//	Nothing was setting this
// 			SUIContexts::Activate("HIDE_ACCEPTBUTTON");
// 			break;
		
// 		case eErrorState_UnknownError:					//	Nothing was setting this
		default:
 			SUIContexts::Activate("HIDE_ACCEPTBUTTON");
			break;
	}

	ShowColumnWarning_EX(PM_COLUMN_MAX, pszString, pszTitle);
}

void CGalleryMenu::ClearWarningScreen()
{
	ClearWarningColumn();

	if( SUIContexts::IsActive( "HIDE_ACCEPTBUTTON" ) )
	{
		SUIContexts::Deactivate("HIDE_ACCEPTBUTTON");
	}
}

//==================================================================
// CGalleryMenu - SetMenuContext
// Purpose: A simple method for controlling contexts within the page
//==================================================================
void CGalleryMenu::SetMenuContext(eContextState eMenuState)
{
	photoDisplayf("CGalleryMenu::SetMenuContext");

	bool bActivateProfanityCheckContext = false;

	switch(eMenuState)
	{
		case eMenuState_ThumbnailMode:
		{
			if( SUIContexts::IsActive( "GALLERY_EMPTY" ) )
			{
				SUIContexts::Deactivate( "GALLERY_EMPTY" );
			}

			if( SUIContexts::IsActive( "GALLERY_REVIEW_TEXT" ) )
			{
				SUIContexts::Deactivate( "GALLERY_REVIEW_TEXT" );
			}

			if( SUIContexts::IsActive( "GALLERY_FINALIZE_TEXT" ) )
			{
				SUIContexts::Deactivate( "GALLERY_FINALIZE_TEXT" );
			}

			if (!SUIContexts::IsActive("GALLERY_THUMBNAIL"))
			{
				SUIContexts::Deactivate("GALLERY_MAXIMIZE"); // Mutually exclusive with GALLERY_SINGLEPAGE
				SUIContexts::Activate("GALLERY_THUMBNAIL");
			}

			if (SUIContexts::IsActive("GALLERY_IMAGE_CORRUPTION"))
			{
				SUIContexts::Deactivate("GALLERY_IMAGE_CORRUPTION");
			}

			UpdateWaypointContextButton();

#if __LOAD_LOCAL_PHOTOS
			UpdateUploadContextButton();
#endif	//	__LOAD_LOCAL_PHOTOS

			if (CPauseMenu::IsNavigatingContent())
			{
				SetScrollBarArrows(eMenuState_ThumbnailMode);
			}
			

			break;
		}
		case eMenuState_MaximizeMode:
		{
			if (!SUIContexts::IsActive("GALLERY_MAXIMIZE"))
			{
				if(SUIContexts::IsActive( "GALLERY_PLACE_TEXT" ) )
				{
					SUIContexts::Deactivate("GALLERY_PLACE_TEXT");
				}

				SUIContexts::Deactivate("GALLERY_THUMBNAIL"); // Mutually exclusive with GALLERY_MULTIPLEPAGES 

				SUIContexts::Activate("GALLERY_MAXIMIZE");
			}

			if( !CanEnterMemeEditor() )
			{
				SUIContexts::Activate( "GALLERY_NO_MEME" );
			}
			else
			{
				SUIContexts::Deactivate( "GALLERY_NO_MEME" );
			}

			if (CPauseMenu::IsNavigatingContent())
			{
				SetScrollBarArrows(eMenuState_MaximizeMode);
				SetScrollBarCount();
			}

			break;
		}

		case eMenuState_ProfanityCheck :

			SUIContexts::Deactivate("GALLERY_MAXIMIZE");
			SUIContexts::Deactivate("GALLERY_THUMBNAIL");

			SUIContexts::Deactivate("GALLERY_REVIEW_TEXT");
			SUIContexts::Deactivate("GALLERY_FINALIZE_TEXT");
			SUIContexts::Deactivate("GALLERY_PLACE_TEXT");

			bActivateProfanityCheckContext = true;

			break;

		case eMenuState_PlaceTextMode:
			
			CScaleformMenuHelper::HIDE_COLUMN_SCROLL( PM_COLUMN_LEFT );

			SUIContexts::Deactivate("GALLERY_MAXIMIZE");
			SUIContexts::Deactivate("GALLERY_REVIEW_TEXT");
			SUIContexts::Deactivate("GALLERY_FINALIZE_TEXT");

			SUIContexts::Activate("GALLERY_PLACE_TEXT");

#if USE_TEXT_CANVAS
			if (CTextTemplate::FontCanBeEdited())
			{
				SUIContexts::Activate("GALLERY_ALLOW_FONT_EDITS");
			}
			else
			{
				SUIContexts::Deactivate("GALLERY_ALLOW_FONT_EDITS");
			}
#endif	//	USE_TEXT_CANVAS

#if USE_CODE_TEXT
			SUIContexts::Activate("GALLERY_ALLOW_FONT_EDITS");
#endif	//	USE_CODE_TEXT

			break;
			
		case eMenuState_FinalizeTextMode:
			SUIContexts::Activate("GALLERY_FINALIZE_TEXT");
			// Deliberate fall-through, finalize and review share contexts

		case eMenuState_ReviewTextMode:
			SUIContexts::Deactivate("GALLERY_PLACE_TEXT");
			SUIContexts::Activate("GALLERY_REVIEW_TEXT");

			break;

		case eMenuState_CorruptTexture:
		{
			SUIContexts::Deactivate("GALLERY_IMAGE_NORMAL");
			SUIContexts::Deactivate("GALLERY_REVIEW_TEXT");
			SUIContexts::Deactivate("GALLERY_FINALIZE_TEXT");
			SUIContexts::Deactivate("GALLERY_PLACE_TEXT");

			SUIContexts::Activate("GALLERY_IMAGE_CORRUPTION");
			break;
		}
		case eMenuState_NormalTexture:
		{
			SUIContexts::Deactivate("GALLERY_IMAGE_CORRUPTION");
			SUIContexts::Activate("GALLERY_IMAGE_NORMAL");
			break;
		}
		case eMenuState_Empty:
		{
			
			if (SUIContexts::IsActive("GALLERY_IMAGE_CORRUPTION"))
				SUIContexts::Deactivate("GALLERY_IMAGE_CORRUPTION");

			SUIContexts::Activate("GALLERY_EMPTY");
		}
	};

	if (bActivateProfanityCheckContext)
	{
		SUIContexts::Activate("GALLERY_PROFANITY_CHECK");
		CBusySpinner::On( TheText.Get("PROFAN_CHECKING"), BUSYSPINNER_CLOUD, SPINNER_SOURCE_PROFANITY_CHECK );		//	Add new text label to american text file
	}
	else
	{
		SUIContexts::Deactivate("GALLERY_PROFANITY_CHECK");
		CBusySpinner::Off(SPINNER_SOURCE_PROFANITY_CHECK);
	}

	CPauseMenu::RedrawInstructionalButtons();
}

void CGalleryMenu::RecalculateMaxPages()
{
	float fMaxPages = ceil(((float)CPhotoManager::GetNumberOfPhotos(false) / (float)MAX_ITEMS_ON_PAGE));
	m_iMaxNumberOfPages = (int)(fMaxPages);
}

void CGalleryMenu::Repopulate( bool const forceUnloadAll )
{
	photoDisplayf("CGalleryMenu::Repopulate");

	CScaleformMenuHelper::SET_DATA_SLOT_EMPTY( PM_COLUMN_LEFT );	

	SetDescription(true);

	if (!m_bRepopulateOnDelete )
	{
		m_bBuiltGalleryList = false;
		m_GalleryScanProgress = GALLERY_SCAN_PROGRESS_CREATE_SORTED_LIST;	
	}
	else
	{
		SUIContexts::Activate("GALLERY_DISABLE_DELETE");	//	Graeme - after deleting a photo, hide the Delete button until all the photos have loaded on the current gallery page (see GALLERY_SCAN_PROGRESS_FINISHED)
		Populate(MENU_UNIQUE_ID_GALLERY);
		m_GalleryScanProgress = GALLERY_SCAN_PROGRESS_BEGIN_LOAD_PHOTO;

		RecalculateMaxPages();
	}
	
	m_PhotoToLoad = m_iCurrentPage * MAX_ITEMS_ON_PAGE;
	m_IndexToLoad = 0;

	sMiniMapMenuComponent.ResetBlips();
	m_ItemState.Reset();

	for (int i = 0; i < MAX_ITEMS_ON_PAGE; i++)
	{
		m_ItemState.PushAndGrow(eGalleryItemState_Empty);
	}

	if( forceUnloadAll )
	{
		CPhotoManager::RequestUnloadAllGalleryPhotos();
	}

	Prepopulate(MENU_UNIQUE_ID_GALLERY);

}

//==========================================================
// CGalleryMenu - Render
// Purpose - Renders maximize texture if needed
//==========================================================
void CGalleryMenu::Render(const PauseMenuRenderDataExtra* renderData)
{
	if (!CMiniMap::GetVisible())
	{
		return;
	}

	if(const SGeneralPauseDataConfig* pData = CPauseMenu::GetMenuArray().GeneralData.MovieSettings.Access("GalleryBG"))
	{
		Vector2 vPos ( pData->vPos  );
		Vector2 vSize( pData->vSize );

#if SUPPORT_MULTI_MONITOR
		CSprite2d::MovePosVectorToScreenGUI(vPos, vSize);
#endif //SUPPORT_MULTI_MONITOR

		CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(WIDESCREEN_FORMAT_CENTRE, &vPos, &vSize);

		if (CPauseMenu::IsNavigatingContent() && CPhotoManager::GetNumberOfPhotos(false) > 0)
		{
			// This check prevents the in-game minimap from briefly rendering when clicking into the gallery menu. url:bugstar:2210682
			if (!CMiniMap_RenderThread::IsInGameMiniMapDisplaying())
			{
				CMiniMap_RenderThread::RenderPauseMap(vPos, vSize, renderData->fAlphaFader, renderData->fBlipAlphaFader, renderData->fSizeScalar);
			}
		}
		else if(!IsShowingWarningColumn())
		{
			CScaleformMgr::ScalePosAndSize(vPos, vSize, renderData->fSizeScalar);

			float screenWidth	= (float)VideoResManager::GetUIWidth();
			float screenHeight	= (float)VideoResManager::GetUIHeight();
			int scissorX = rage::round(vPos.x*screenWidth);
			int scissorY = rage::round(vPos.y*screenHeight);
			int scissorW = rage::round(vSize.x*screenWidth);
			int scissorH = rage::round(vSize.y*screenHeight);

			GRCDEVICE.SetScissor( scissorX, scissorY, scissorW, scissorH );

			CRGBA color = CHudColour::GetRGBA(HUD_COLOUR_PAUSE_BG);
			color = color.MultiplyAlpha( u8(255.0f * renderData->fAlphaFader * CHudColour::GetRGBA(HUD_COLOUR_PAUSE_DESELECT).GetAlphaf() ) );
			CSprite2d::DrawRect(fwRect(vPos.x, vPos.y, vPos.x+vSize.x, vPos.y+vSize.y), color);
			GRCDEVICE.DisableScissor();
		}
	}
}

void CGalleryMenu::ClearPhotoSlots()
{
	int iCount = 0;
	CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);

	if (m_iMaxNumberOfPages > 1)
	{
		iCount = (s32)CPhotoManager::GetNumberOfPhotos(false) - ((m_iCurrentPage * MAX_ITEMS_ON_PAGE));
		iCount = Min(iCount, MAX_ITEMS_ON_PAGE);
	}
	else
	{
		iCount = Min((s32)CPhotoManager::GetNumberOfPhotos(false), MAX_ITEMS_ON_PAGE);
	}

	int i = 0;

	//for (; i < Min(iCount, MAX_ITEMS_ON_PAGE); i++)
	//{
	//	if( pauseContent.BeginMethod("SET_DATA_SLOT") )
	//	{
	//		pauseContent.AddParam(0);			// The Column id
	//		pauseContent.AddParam(i);			// The incrementing index 
	//		pauseContent.AddParam(999);  // The numbered slot the information is to be added to
	//		pauseContent.AddParam(999);  // The numbered slot the information is to be added to
	//		pauseContent.AddParam(eGalleryItemState_Queued);  // The menu ID
	//		pauseContent.AddParam(0);  // The unique ID
	//		pauseContent.AddParam(1);  // The unique ID
	//		pauseContent.AddParam("");  // The Menu item type (see below)
	//		pauseContent.AddParam("");  // The initial index of the slot (0 default, can be 0,1,2...x in a multiple selection)
	//		pauseContent.AddParam("");  // active or inactive
	//		pauseContent.AddParam("");  // The text label
	//		pauseContent.AddParam(1);  // The text label
	//		pauseContent.AddParam(false);  // The text label
	//		pauseContent.EndMethod();
	//	}
	//}

	for (; i < MAX_ITEMS_ON_PAGE; i++)
	{
		if( pauseContent.BeginMethod("SET_DATA_SLOT") )
		{
			pauseContent.AddParam(0);			// The Column id
			pauseContent.AddParam(i);			// The incrementing index 
#if RSG_PC
			pauseContent.AddParam(i);  // The numbered slot the information is to be added to
			pauseContent.AddParam(MENU_UNIQUE_ID_GALLERY_ITEM);  // The numbered slot the information is to be added to
#else
			pauseContent.AddParam(999);  // The numbered slot the information is to be added to
			pauseContent.AddParam(999);  // The numbered slot the information is to be added to
#endif
			pauseContent.AddParam(eGalleryItemState_Empty);  // The menu ID
			pauseContent.AddParam(0);  // The unique ID
			pauseContent.AddParam(1);  // The unique ID
			pauseContent.AddParam("");  // The Menu item type (see below)
			pauseContent.AddParam("");  // The initial index of the slot (0 default, can be 0,1,2...x in a multiple selection)
			pauseContent.AddParam("");  // active or inactive
			pauseContent.AddParam("");  // The text label
			pauseContent.AddParam(1);  // The text label
			pauseContent.AddParam(false);  // The text label
			pauseContent.EndMethod();
		}
	}

	CScaleformMenuHelper::DISPLAY_DATA_SLOT( PM_COLUMN_LEFT );	

}
//===============================================================
// CGalleryMenu - SetMaximize
// Purpose: Maximize image
//===============================================================
void CGalleryMenu::SetMaximize(int eState)
{
	CUndeletedEntryInMergedPhotoList undeletedEntry(m_iSelectedIndex, true);

	char textureName[CTextureDictionariesForGalleryPhotos::ms_MaxLengthOfTextureName];
	CPhotoManager::GetNameOfPhotoTextureDictionary(undeletedEntry, textureName, CTextureDictionariesForGalleryPhotos::ms_MaxLengthOfTextureName);

	SetMaximize( eState, textureName, textureName );
}

void CGalleryMenu::SetMaximize( int eState, char const * const dictionaryName, char const * const textureName )
{
	CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);

	if( pauseContent.BeginMethod("SET_COLUMN_TITLE"))
	{
		pauseContent.AddParam(0);			// The columnIndex - should always be 0
		pauseContent.AddParam(dictionaryName);
		pauseContent.AddParam(textureName);
		pauseContent.AddParam(eState);
		pauseContent.EndMethod();
	}

	bool bShowFacebookButton = false;

#if RL_FACEBOOK_ENABLED
	if (eState == eGalleryItemState_Loaded)
	{
		if ( (CLiveManager::CheckOnlinePrivileges()) && (CLiveManager::GetFacebookMgr().CanReportToFacebook()) )  // fix for 1655147 - we need to make sure that even if we are linked to facebook, we can actually post (need a Gold account for this), otherwise dont show button press
		{
			CUndeletedEntryInMergedPhotoList undeletedEntry(m_iSelectedIndex, true);

			if (CPhotoManager::IsListOfPhotosUpToDate(false))
			{
				if (CPhotoManager::GetHasPhotoBeenUploadedToSocialClub(undeletedEntry))
				{
					bShowFacebookButton = true;
				}
			}
			else
			{
				photoDisplayf("CGalleryMenu::SetMaximize - IsListOfPhotosUpToDate returned false so we can't check if the photo has already been uploaded in order to show the Facebook button");
			}
		}
	}
#endif // RL_FACEBOOK_ENABLED

	if (bShowFacebookButton)
	{
		if (SUIContexts::IsActive("GALLERY_DENY_FACEBOOK"))
		{
			SUIContexts::Deactivate("GALLERY_DENY_FACEBOOK");
			CPauseMenu::RedrawInstructionalButtons();
		}
	}
	else
	{
		if (!SUIContexts::IsActive("GALLERY_DENY_FACEBOOK"))
		{
			SUIContexts::Activate("GALLERY_DENY_FACEBOOK");
			CPauseMenu::RedrawInstructionalButtons();
		}
	}
}

void CGalleryMenu::ReturnToThumbnailView()
{
	CleanupMemeEditor();

	SetMaximize( eGalleryItemState_Empty );
	SetMenuContext(eMenuState_ThumbnailMode);
	//CPauseMenu::SetGalleryMaximizeActive(false);
	CMapMenu::SetMapZoom(ZOOM_LEVEL_GALLERY,true);

	m_pGalleryTexture = NULL;
	m_galleryTextureTxdId = -1;
	m_galleryTextureLocalIndex = -1;
	m_eMenuState = eGalleryState_InMenu;

	CUndeletedEntryInMergedPhotoList undeletedEntry(m_iSelectedIndex, true);
	CPhotoManager::RequestUnloadGalleryPhoto(undeletedEntry);

}

//==================================================================================
// CGalleryMenu - LayoutChanged
// Purpose: Standard dynamic menu override to see when a page should be repopulated
//==================================================================================
void CGalleryMenu::LayoutChanged( MenuScreenId UNUSED_PARAM(iPreviousLayout), MenuScreenId iNewLayout, s32 UNUSED_PARAM(iUniqueId), eLAYOUT_CHANGED_DIR UNUSED_PARAM(eDir) )
{
	photoDisplayf("CGalleryMenu::LayoutChanged");

	// only init if it matches what we expect
	if( iNewLayout == m_Owner.MenuScreen)
		sMiniMapMenuComponent.InitClass();

	Prepopulate(iNewLayout);
}

void CGalleryMenu::OnNavigatingContent(bool bAreWe)
{
	if (bAreWe)
	{
#if __LOAD_LOCAL_PHOTOS

		if (m_bQueueSnapToBlipOnEntry == false)
		{
			photoDisplayf("CGalleryMenu::OnNavigatingContent - about to call UpdateUploadContextButton");
#if RSG_PC
			photomouseDisplayf("CGalleryMenu::OnNavigatingContent - about to call UpdateUploadContextButton");
#endif	//	RSG_PC

			UpdateUploadContextButton();

			UpdateWaypointContextButton();
		}
		else
		{
			photoDisplayf("CGalleryMenu::OnNavigatingContent - m_bQueueSnapToBlipOnEntry is true so do nothing");
#if RSG_PC
			photomouseDisplayf("CGalleryMenu::OnNavigatingContent - m_bQueueSnapToBlipOnEntry is true so do nothing");
#endif	//	RSG_PC
		}
#endif	//	__LOAD_LOCAL_PHOTOS

		SUIContexts::Activate("GALLERY_DISABLE_DELETE");	// To prevent the delete button from flickering, make sure it's hidden when we first enter the menu to prevent a flicker, as no images are loaded yet

		if (!CPauseMenu::GetMenuPreference(PREF_RADAR_MODE))
		{
			m_bRadarEnabledOnEntry = true;
			CPauseMenu::SetMenuPreference(PREF_RADAR_MODE, 1);
		}

	}
}


//===============================================================
// CGalleryMenu - LoseFocus
// Purpose: Called when we are leaving the page
//===============================================================
void CGalleryMenu::LoseFocus()
{	
	photoDisplayf("CGalleryMenu::LoseFocus");
	uiDebugf3("CGalleryMenu::LoseFocus");

    CleanupTextTemplates();

	//! Cleanup meme editor if active.
	CleanupMemeEditor();

	if (m_pGalleryTexture)
	{
		SetMaximize(eGalleryItemState_Empty);
		m_pGalleryTexture = NULL;
		m_galleryTextureTxdId = -1;
		m_galleryTextureLocalIndex = -1;

		CUndeletedEntryInMergedPhotoList undeletedEntry(m_iSelectedIndex, true);
		CPhotoManager::RequestUnloadGalleryPhoto(undeletedEntry);

	}
	else
	{
		CMapMenu::SetMapZoom(ZOOM_LEVEL_1,true);
	}

	Initialise();

	if (m_bIsInPrologue)
	{
		m_bIsInPrologue = false;
		CMiniMap::SetInPrologue(true);
	}

	if (m_bRadarEnabledOnEntry)
	{
		m_bRadarEnabledOnEntry = false;
		CPauseMenu::SetMenuPreference(PREF_RADAR_MODE, 0);

	}


	sMiniMapMenuComponent.ShutdownClass();
	CMiniMap::UpdateCentreAndNorthBlips();

	ClearWarningScreen();
	CPhotoManager::RequestUnloadAllGalleryPhotos();

	if (g_GalleryDlgt.IsBound())
		g_GalleryDlgt.Unbind();

	CMiniMap::SetVisible(true);
	rlPresence::RemoveDelegate(&g_GalleryDlgt);
}

//==========================================================
// CGalleryMenu - CreateGalleryBlip
// Purpose: Generate a new blip for the gallery
//==========================================================
s32 CGalleryMenu::CreateGalleryBlip(Vector3& vPosition)
{
	// Zero out the z portion so that blips are always on level.
	vPosition.z = 0.0f;
	s32 sBlipId = CMiniMap::CreateBlip(true, BLIP_TYPE_CUSTOM, vPosition, BLIP_DISPLAY_CUSTOM_MAP_ONLY, NULL);
	
	return sBlipId;
}

bool CGalleryMenu::IsLastElementInGallery( s32 const entry ) const
{
	return (entry == (s32)CPhotoManager::GetNumberOfPhotos(false) - 1 );
}

//==========================================================
// CGalleryMenu - CanMoveToRequestedEntry
// Purpose: Tests to see if we can move to sEntry
//==========================================================
bool CGalleryMenu::CanMoveToRequestedEntry(s32 sIndexPerPage) const
{
	bool bResult = false;

	//Test to see if this index is a valid photo.  We can do this by querying if a valid radar blip exists.
	//Edit: Lies... corrupted images dont show blips.
	eGalleryItemState eState = m_ItemState[sIndexPerPage];

	if (eState != eGalleryItemState_Empty && eState != eGalleryItemState_Loading )
	{
		bResult = true;
	}

	return bResult;
}

//===============================================================
// CGalleryMenu - IsImageCorrupted
// Purpose: Tests to see if an image is corrupted
//===============================================================

bool CGalleryMenu::IsImageCorrupted(u32 uIndex)
{
	bool bResult = false;

	if (m_ItemState[uIndex] == eGalleryItemState_Corrupted)
	{
		bResult = true;
	}

	return bResult;
}

//===============================================================
// CGalleryMenu - ShouldNavigateToNewPage
// Purpose: Tests to see if we should be navigating to a new page
//===============================================================
bool CGalleryMenu::ShouldNavigateToNewPage(s32 sIndexPerPage) const
{
	bool bResult = false;

	if ( CPhotoManager::GetNumberOfPhotos(false) <= MAX_ITEMS_ON_PAGE || m_iMaxNumberOfPages  <= 1 )
	{
		bResult = false;
	}
	else if (( m_iSelectedIndexPerPage == 0 && sIndexPerPage == 0) || ( m_iSelectedIndexPerPage == 4 && sIndexPerPage == 4) || ( m_iSelectedIndexPerPage == 8 && sIndexPerPage == 8))
	{
		// We want to move a page to the LEFT
		bResult = true;
	}
	else if (( m_iSelectedIndexPerPage == 3 && sIndexPerPage == 3) || ( m_iSelectedIndexPerPage == 7 && sIndexPerPage == 7) || ( m_iSelectedIndexPerPage == 11 && sIndexPerPage == 11))
	{
		// We want to move a page to the RIGHT
		bResult = true;
	}

	return bResult;
}

//===============================================================
// CGalleryMenu - Update
// Purpose: 
//===============================================================
void CGalleryMenu::Update()
{
	UpdateGalleryScanProgress();
	UpdateGalleryActions();

}


void CGalleryMenu::CancelProfanityCheck()
{
	photoAssertf( (m_eMenuState == eGalleryState_ProfanityCheckName) || (m_eMenuState == eGalleryState_ProfanityCheckMemeText), "CGalleryMenu::CancelProfanityCheck - expected m_eMenuState to be eGalleryState_ProfanityCheckName or eGalleryState_ProfanityCheckMemeText");

	netProfanityFilter::ClearRequest(s_galleryProfanityToken);

	CScaleformMenuHelper::SET_COLUMN_HIGHLIGHT(PM_COLUMN_LEFT, m_iSelectedIndexPerPage);

	if( m_eMenuState == eGalleryState_ProfanityCheckName )
	{
		m_eMenuState = eGalleryState_Maximize;
		SetMenuContext(eMenuState_MaximizeMode);
	}
	else
	{
		if( m_iMemeTextEntered <= 0 )
		{
			m_eMenuState = eGalleryState_Maximize;
			SetMenuContext(eMenuState_MaximizeMode);
		}
		else
		{
			m_eMenuState = eGalleryState_ReviewMemeImage;
			SetMenuContext(eMenuState_ReviewTextMode);
		}
	}
}


//===============================================================
// CGalleryMenu - UpdateGalleryActions
// Purpose: Updates counts in AS for the scroll bar
//===============================================================
void CGalleryMenu::UpdateGalleryActions()
{

	switch (m_eGalleryActionState)
	{
		case GA_MAXIMIZE_PROGRESS_LOADING_NEW_PAGE_SWAP_IMAGE:
			{
				// Request to remove the old one, add the new one.
				CUndeletedEntryInMergedPhotoList deleteEntry(m_iMoveInMaximizeOldIndex, true);
				CPhotoManager::RequestUnloadGalleryPhoto(deleteEntry);

				m_eGalleryActionState = GA_MAXIMIZE_PROGRESS_LOADING_NEW_PAGE_SWAPPING_IMAGE;

				break;
			}
		case GA_MAXIMIZE_PROGRESS_LOADING_NEW_PAGE_SWAPPING_IMAGE:
			{
				CUndeletedEntryInMergedPhotoList deleteEntry(m_iMoveInMaximizeOldIndex, true);

				if (CPhotoManager::GetUnloadGalleryPhotoStatus(deleteEntry) == MEM_CARD_COMPLETE)
				{
					m_eGalleryActionState = GA_MAXIMIZE_PROGRESS_LOADING_NEW_PAGE_SWAPPING_IMAGE2;
					break;
				}
			}
		case GA_MAXIMIZE_PROGRESS_LOADING_NEW_PAGE_SWAPPING_IMAGE2:
			{
				m_iSelectedIndex = m_iCurrentPage * MAX_ITEMS_ON_PAGE;
				m_iSelectedIndexPerPage = 0;
				m_bPaging = true;


				CUndeletedEntryInMergedPhotoList loadEntry(m_iSelectedIndex, true);
				char textureName[CTextureDictionariesForGalleryPhotos::ms_MaxLengthOfTextureName];
				CPhotoManager::GetNameOfPhotoTextureDictionary(loadEntry, textureName, CTextureDictionariesForGalleryPhotos::ms_MaxLengthOfTextureName);

				photoDisplayf("CGalleryMenu::GA_MAXIMIZE_PROGRESS_LOADING_NEW_PAGE_SWAPPING_IMAGE2");
				CleanupMemeEditor();
				Repopulate( true );
				SetScrollBarCount();
				SetMaximize(eGalleryItemState_Transition);

				m_eGalleryActionState = GA_MAXIMIZE_PROGRESS_LOADING_NEW_PAGE_SWAPPING_IMAGE3;

				break;
			}
		case GA_MAXIMIZE_PROGRESS_LOADING_NEW_PAGE_SWAPPING_IMAGE3:
			{
				CUndeletedEntryInMergedPhotoList loadEntry(m_iSelectedIndex, true);
				char textureName[CTextureDictionariesForGalleryPhotos::ms_MaxLengthOfTextureName];
				CPhotoManager::GetNameOfPhotoTextureDictionary(loadEntry, textureName, CTextureDictionariesForGalleryPhotos::ms_MaxLengthOfTextureName);

				m_iMoveInMaximizeOldIndex = -1;

				photoDisplayf("CGalleryMenu::GA_MAXIMIZE_PROGRESS_LOADING_NEW_PAGE_SWAPPING_IMAGE3");
				if (CPhotoManager::RequestLoadGalleryPhoto(loadEntry))
				{
					CUndeletedEntryInMergedPhotoList loadEntry2(m_iSelectedIndex, false);
					CPhotoManager::RequestLoadGalleryPhoto(loadEntry2);
					photoDisplayf("CGalleryMenu::GA_MAXIMIZE_PROGRESS_LOADING_NEW_PAGE_SWAPPING_IMAGE3 -RequstLoadGalleryPhoto");
					m_eGalleryActionState = GA_MAXIMIZE_PROGRESS_LOADING_IMAGE;
					SetMaximize(eGalleryItemState_Loading);
				}
				break;
			}
		case GA_MAXIMIZE_PROGRESS_LOADING_SWAP_IMAGE:
			{
				// Request to remove the old one, add the new one.
				CUndeletedEntryInMergedPhotoList deleteEntry(m_iMoveInMaximizeOldIndex, true);
				CPhotoManager::RequestUnloadGalleryPhoto(deleteEntry);
				SetMaximize(eGalleryItemState_Transition);

				m_eGalleryActionState = GA_MAXIMIZE_PROGRESS_LOADING_SWAPPING_IMAGE;
				break;
			}
		case GA_MAXIMIZE_PROGRESS_LOADING_SWAPPING_IMAGE:
			{
				CUndeletedEntryInMergedPhotoList deleteEntry(m_iMoveInMaximizeOldIndex, true);

				if (CPhotoManager::GetUnloadGalleryPhotoStatus(deleteEntry) == MEM_CARD_COMPLETE)
				{

					CUndeletedEntryInMergedPhotoList loadEntry(m_iSelectedIndex, true);
					char textureName[CTextureDictionariesForGalleryPhotos::ms_MaxLengthOfTextureName];
					CPhotoManager::GetNameOfPhotoTextureDictionary(loadEntry, textureName, CTextureDictionariesForGalleryPhotos::ms_MaxLengthOfTextureName);

					m_iMoveInMaximizeOldIndex = -1;

					if (CPhotoManager::RequestLoadGalleryPhoto(loadEntry))
					{
						m_eGalleryActionState = GA_MAXIMIZE_PROGRESS_LOADING_IMAGE;
						SetMaximize(eGalleryItemState_Loading);
					}
				}
				else if (CPhotoManager::GetUnloadGalleryPhotoStatus(deleteEntry) == MEM_CARD_ERROR)
				{
					m_eGalleryActionState = GA_FACEBOOK_ERROR;
				}

				break;
			}
		case GA_MAXIMIZE_PROGRESS_LOADING_IMAGE:
			{
				static bool bFirst = true;

				MemoryCardError loadMaximizedProgress = CPhotoManager::GetLoadGalleryPhotoStatus(CUndeletedEntryInMergedPhotoList(m_iSelectedIndex, true));
				MemoryCardError loadMinimizedProgress = CPhotoManager::GetLoadGalleryPhotoStatus(CUndeletedEntryInMergedPhotoList(m_iSelectedIndex, false));
				if ((loadMaximizedProgress == MEM_CARD_COMPLETE) && loadMinimizedProgress == MEM_CARD_COMPLETE
#if USE_TEXT_CANVAS
					&& CTextTemplate::IsMovieActive()
#endif
					)
				{
					photoDisplayf("CGalleryMenu::GA_MAXIMIZE_PROGRESS_LOADING_IMAGE - MEM_CARD_COMPLETE ");
					m_eGalleryActionState = GA_MAXIMIZE_PROGRESS_LOADING_FINISHED;
					bFirst =  true;
				}
				else if ((loadMaximizedProgress == MEM_CARD_ERROR)  || loadMinimizedProgress == MEM_CARD_ERROR)
				{
					photoDisplayf("CGalleryMenu::GA_MAXIMIZE_PROGRESS_LOADING_IMAGE - MEM_CARD_ERROR ");
					photoDisplayf("CGalleryMenu::GA_MAXIMIZE_PROGRESS_LOADING_IMAGE - loadMaximizedProgress: %d loadMinimizedProgress: %d ",loadMaximizedProgress, loadMinimizedProgress );
					m_eGalleryActionState = GA_MAXIMIZE_FAILED;
				}
				else
				{
					if (bFirst)
					{
						SetMaximize(eGalleryItemState_Loading);
						bFirst = false;
					}
				}
				break;
			}
		case GA_MAXIMIZE_PROGRESS_LOADING_FINISHED:
		{
			CUndeletedEntryInMergedPhotoList undeletedEntry(m_iSelectedIndex, true);
			char textureName[CTextureDictionariesForGalleryPhotos::ms_MaxLengthOfTextureName];
			CPhotoManager::GetNameOfPhotoTextureDictionary(undeletedEntry, textureName, CTextureDictionariesForGalleryPhotos::ms_MaxLengthOfTextureName);

			m_eGalleryActionState = GA_INVALID;
			m_galleryTextureTxdId = g_TxdStore.FindSlot(textureName);

			if ( m_galleryTextureTxdId != -1 )
			{
				fwTxd* pTxd = g_TxdStore.Get( m_galleryTextureTxdId );

				if (pTxd)
				{
					m_pGalleryTexture = pTxd->Lookup(textureName);
					m_galleryTextureLocalIndex = pTxd->LookupLocalIndex( pTxd->ComputeHash( textureName ) );
				}
			}

			m_eMenuState = eGalleryState_Maximize;

			SetupMemeEditor();
			SetMaximize( eGalleryItemState_Loaded );
			SetMenuContext(eMenuState_MaximizeMode);
			break;
		}
		case GA_FACEBOOK_LOADING_IMAGE:
		{
			bool isBusy = RL_FACEBOOK_SWITCH(CLiveManager::GetFacebookMgr().IsBusy(), false);
			bool didSucceed = RL_FACEBOOK_SWITCH(CLiveManager::GetFacebookMgr().DidSucceed(), false);

			if (!isBusy)
			{
				if (didSucceed)
				{
					if (SUIContexts::IsActive("GALLERY_DENY_FACEBOOK"))
					{
						SUIContexts::Deactivate("GALLERY_DENY_FACEBOOK");
						CPauseMenu::RedrawInstructionalButtons();
					}

					photoDisplayf("CGalleryMenu::UpdateGalleryActions - LiveManager::FaceBookMgr returning succeeded...");
					m_eGalleryActionState = GA_FACEBOOK_LOADED_IMAGE;
				}
				else
				{
					if (SUIContexts::IsActive("GALLERY_DENY_FACEBOOK"))
					{
						SUIContexts::Deactivate("GALLERY_DENY_FACEBOOK");
						CPauseMenu::RedrawInstructionalButtons();
					}

					photoDisplayf("CGalleryMenu::UpdateGalleryActions - WarningMessage, upload failed.");
					m_eGalleryActionState = GA_FACEBOOK_ERROR_UPLOAD_FAILED;
				}
			}
			else
			{
				if (!SUIContexts::IsActive("GALLERY_DENY_FACEBOOK"))
				{
					SUIContexts::Activate("GALLERY_DENY_FACEBOOK");
					CPauseMenu::RedrawInstructionalButtons();
				}

				
				photoDisplayf("CGalleryMenu::UpdateGalleryActions - LiveManager::FaceBookMgr returning busy...");
			}
			break;
		}
		case GA_FACEBOOK_ERROR:
		{
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "ERROR_FB_HEADER", "ERROR_FACEBOOK", FE_WARNING_OK);

			if( m_bWasFacebookErrorTriggeredThisFrame )
			{
				m_bWasFacebookErrorTriggeredThisFrame = false;
			}
			else if (CWarningScreen::CheckAllInput() == FE_WARNING_OK)
				m_eGalleryActionState = GA_INVALID;

			break;
		}
		case GA_FACEBOOK_ERROR_UPLOAD_FAILED:
		{
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "ERROR_FB_HEADER", "ERROR_FACEBOOK", FE_WARNING_OK);

			if (CWarningScreen::CheckAllInput() == FE_WARNING_OK)
				m_eGalleryActionState = GA_INVALID;

			break;
		}
		case GA_FACEBOOK_ERROR_PROFILE_SETTING_FAIL:
		{
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "ERROR_FB_HEADER", "ERROR_PROFILESETTING_FACEBOOK", FE_WARNING_OK);

			if( m_bWasFacebookErrorTriggeredThisFrame )
			{
				m_bWasFacebookErrorTriggeredThisFrame = false;
			}
			else if (CWarningScreen::CheckAllInput() == FE_WARNING_OK)
				m_eGalleryActionState = GA_INVALID;

			break;
		}
		case GA_NO_ONLINE_PRIVILEGE_PROMPT:
		{
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "ERROR_GAL_HDR", "ERROR_NOT_PRIVILEGE", FE_WARNING_OK);
			
			if( m_bWasFacebookErrorTriggeredThisFrame )
			{
				m_bWasFacebookErrorTriggeredThisFrame = false;
			}
			else if (CWarningScreen::CheckAllInput() == FE_WARNING_OK )
				m_eGalleryActionState = GA_INVALID;

			break;
		}
		case GA_MEME_SAVE_MESSAGE:
		{
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "SG_HDNG", "GAL_MEME_SAVE", FE_WARNING_OK);

			if( m_bWasMemeSaveTriggeredThisFrame )
			{
				m_bWasMemeSaveTriggeredThisFrame = false;
			}
			else if ( CWarningScreen::CheckAllInput() == FE_WARNING_OK )
			{
				m_eGalleryActionState = GA_INVALID;
			}

			break;
		}
		case GA_MEME_GALLERY_FULL_MESSAGE:
		{
			CWarningScreen::SetMessageWithHeaderAndSubtext( WARNING_MESSAGE_STANDARD, "SG_HDNG", "GAL_MEME_FULL1", "GAL_MEME_FULL2", FE_WARNING_OK );

			if ( CWarningScreen::CheckAllInput() == FE_WARNING_OK )
			{
				m_eGalleryActionState = GA_INVALID;
			}

			break;
		}
		case GA_DELETE_CONFIRM:
		{
			CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "ERROR_DEL_HDR", "ERROR_DELETE", FE_WARNING_YES_NO);

			eWarningButtonFlags result = CWarningScreen::CheckAllInput();
			if( result == FE_WARNING_YES )
			{
				photoDisplayf("CGalleryMenu::UpdateInput - Confirm Delete - Accept - m_iSelectedIndex = %d, m_iSelectedIndexPerPage = %d", m_iSelectedIndex, m_iSelectedIndex);

				CUndeletedEntryInMergedPhotoList undeletedEntry(m_iSelectedIndex, false);
				m_EntryForDeletion = CPhotoManager::RequestDeleteGalleryPhoto(undeletedEntry);

				m_ItemState[m_iSelectedIndexPerPage] = eGalleryItemState_Empty;

				m_eMenuState = eGalleryState_InDeleteProcess;
				m_eGalleryActionState = GA_INVALID;
			} else if ( result == FE_WARNING_NO )
			{
				m_eGalleryActionState = GA_INVALID;
			}
			break;
		}

#if __LOAD_LOCAL_PHOTOS
		case GA_UPLOAD_CONFIRM:
			{
				CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "ERROR_UPLD_HDR", "ERROR_UPLOAD", FE_WARNING_YES_NO);

				eWarningButtonFlags result = CWarningScreen::CheckAllInput();
				if( result == FE_WARNING_YES )
				{
					BeginUploadOfLocalPhoto();
				}
				else if ( result == FE_WARNING_NO )
				{
					m_eGalleryActionState = GA_INVALID;
				}
				break;
			}

		case GA_UPLOAD_WARNING :
			{
				ProcessUploadWarningMessage();
			}
			break;
#endif	//	__LOAD_LOCAL_PHOTOS

		case GA_FACEBOOK_LOADED_IMAGE:
		{
			m_eGalleryActionState = GA_INVALID;
			break;
		}
		case GA_MAXIMIZE_FAILED:
		{
			CleanupMemeEditor();
			SetMaximize( eGalleryItemState_Empty );
			SetMenuContext( eMenuState_ThumbnailMode );
			CMapMenu::SetMapZoom(ZOOM_LEVEL_GALLERY,true);

			m_eGalleryActionState = GA_INVALID;
			m_pGalleryTexture = NULL;
			m_galleryTextureTxdId = -1;
			m_galleryTextureLocalIndex = -1;
			m_eMenuState = eGalleryState_InMenu;
			break;
		}
		case GA_DELAY_SIGN_IN:
		{
			u32 uTimeDelta = fwTimer::GetSystemTimeInMilliseconds() - m_iDelayedSignInCounter;
			if (uTimeDelta > DELAYED_SIGNIN_COUNT || CLiveManager::GetSocialClubMgr().IsConnectedToSocialClub())
			{
				m_eGalleryActionState = GA_INVALID;
				ClearWarningScreen();

				Initialise();
				m_bQueueSnapToBlipOnEntry = true;
				if (Populate(MENU_UNIQUE_ID_GALLERY))
					CMiniMap::SetVisible(true);

				m_iDelayedSignInCounter = 0;
			}
			
			break;
		}

		case GA_SOCIALCLUB_IS_AGE_RESTRICTED :
		case GA_SOCIALCLUB_NOT_CONNECTED_TO_SOCIAL_CLUB :
		case GA_SOCIALCLUB_ONLINE_POLICY_IS_NOT_UP_TO_DATE :
		case GA_NO_ONLINE_PRIVILEGE:
		case GA_NO_SOCIAL_SHARING:
		case GA_INVALID:
		default:
			break;
	}

	// Update the virtual keyboard
	if ( ( m_eMenuState == eGalleryState_InKeyboardForName || m_eMenuState == eGalleryState_InKeyboardForMemeText ) 
		&& CControlMgr::UpdateVirtualKeyboard() != ioVirtualKeyboard::kKBState_PENDING )
	{
		if ( CControlMgr::GetVirtualKeyboardState() == ioVirtualKeyboard::kKBState_SUCCESS )
		{
			photoDisplayf("CGalleryMenu::Update - Checking text for profanity...");
			formatf(m_szProfanityString,TITLE_BUFFER_LENGTH,"%s",CControlMgr::GetVirtualKeyboardResult());
			netProfanityFilter::VerifyStringForProfanity(m_szProfanityString, CText::GetScLanguageFromCurrentLanguageSetting(), true, s_galleryProfanityToken);

			m_eMenuState = ( m_eMenuState == eGalleryState_InKeyboardForName ) ? eGalleryState_ProfanityCheckName : eGalleryState_ProfanityCheckMemeText;
			SetMenuContext(eMenuState_ProfanityCheck);
		}
		else
		{
			if( m_eMenuState == eGalleryState_InKeyboardForName )
			{
				m_eMenuState = eGalleryState_Maximize;
			}
			else
			{
				if( m_iMemeTextEntered <= 0 )
				{
					m_eMenuState = eGalleryState_Maximize;
				}
				else
				{
					m_eMenuState = eGalleryState_ReviewMemeImage;
					
				}

				SetMenuContext( m_iMemeTextEntered <= 0 ? eMenuState_MaximizeMode : eMenuState_ReviewTextMode );
			}
		}
	}
	else if ( m_eMenuState == eGalleryState_ProfanityCheckName || m_eMenuState == eGalleryState_ProfanityCheckMemeText )
	{
		netProfanityFilter::ReturnCode rc = netProfanityFilter::GetStatusForRequest(s_galleryProfanityToken);

		if (rc == netProfanityFilter::RESULT_STRING_OK)
		{
			if( m_eMenuState == eGalleryState_ProfanityCheckName )
			{
				photoDisplayf("CGalleryMenu::Update - Updating title!");

				CUndeletedEntryInMergedPhotoList undeletedEntry(m_iSelectedIndex, false);

#if __UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME
				bool bWaitForSaveToFinish = false;
#endif	//	__UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME
				if (CPhotoManager::SetTitleOfPhoto(undeletedEntry,m_szProfanityString))
				{
#if __UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME
					bWaitForSaveToFinish = true;
#endif	//	__UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME
				}
				DisplayPhotoInSlot(m_iSelectedIndex, m_iSelectedIndexPerPage);
				CScaleformMenuHelper::SET_COLUMN_HIGHLIGHT(PM_COLUMN_LEFT, m_iSelectedIndexPerPage);

#if __UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME
				if (bWaitForSaveToFinish)
				{
					CPauseMenu::SetGalleryLoadingTexture(true);
					SetMaximize(eGalleryItemState_Empty);

					m_IndexOfPhotoWhoseTitleHasBeenUpdated = m_iSelectedIndex;
					m_eMenuState = eGalleryState_WaitForPhotoSaveToFinish;
				}
				else
#endif	//	__UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME
				{
					m_eMenuState = eGalleryState_Maximize;
					SetMenuContext(eMenuState_MaximizeMode);
				}
			}
			else if ( m_eMenuState == eGalleryState_ProfanityCheckMemeText )
			{
				// Refresh appropriate text based on which string we are on
				s32 fontStyle = ( (m_iMemeTextEntered == 0) ? PHONEPHOTOEDITOR.GetTopTextStyle() : PHONEPHOTOEDITOR.GetBottomTextStyle() );
				photoDisplayf("fontStyle before calling FilterOverlayFonts is %d", fontStyle);

				fontStyle = CTextFormat::FilterOverlayFonts( fontStyle, true, m_szProfanityString);
				photoDisplayf("fontStyle after calling FilterOverlayFonts is %d", fontStyle);

				PHONEPHOTOEDITOR.SetText( (m_iMemeTextEntered == 0) ? m_szProfanityString : PHONEPHOTOEDITOR.GetTopText(), 
					(m_iMemeTextEntered == 0) ? 0 : m_szProfanityString,
					PHONEPHOTOEDITOR.GetTextPosition(),
					PHONEPHOTOEDITOR.GetTopTextSize(), PHONEPHOTOEDITOR.GetBottomTextSize(),
					(m_iMemeTextEntered == 0) ? fontStyle : PHONEPHOTOEDITOR.GetTopTextStyle(), (m_iMemeTextEntered == 0) ? PHONEPHOTOEDITOR.GetBottomTextStyle() : fontStyle,
					PHONEPHOTOEDITOR.GetTopColour(), PHONEPHOTOEDITOR.GetBottomColour() );

				m_eMenuState = eGalleryState_PlaceMemeText;
				SetMenuContext( eMenuState_PlaceTextMode );
			}
		}
		else if( rc == netProfanityFilter::RESULT_ERROR || rc == netProfanityFilter::RESULT_INVALID_TOKEN )
		{
			photoDisplayf("CGalleryMenu::netProfanityFilter. %d ", rc);
			m_eMenuState = (m_eMenuState == eGalleryState_ProfanityCheckName) ? eGalleryState_ProfanityCheckUnavilableName : eGalleryState_ProfanityCheckUnavilableMemeText;
		}
		else if (rc == netProfanityFilter::RESULT_STRING_FAILED )
		{
			photoDisplayf("CGalleryMenu::netProfanityFilter. %d ", rc);
			m_eMenuState = (m_eMenuState == eGalleryState_ProfanityCheckName) ? eGalleryState_ProfanityCheckFailedName : eGalleryState_ProfanityCheckFailedMemeText;
		}
		else if (netProfanityFilter::GetStatusForRequest(s_galleryProfanityToken) == netProfanityFilter::RESULT_PENDING)
		{
			//CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "GLOBAL_ALERT_DEFAULT", "ERROR_CHECKPROFANITY", FE_WARNING_CANCEL);

			//if (CPauseMenu::CheckInput(FRONTEND_INPUT_BACK, false, CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE | CHECK_INPUT_OVERRIDE_FLAG_WARNING_MESSAGE))
			//{
			//	CScaleformMenuHelper::SET_COLUMN_HIGHLIGHT(PM_COLUMN_LEFT, m_iSelectedIndexPerPage);
			//	m_eMenuState = eGalleryState_Maximize;
			//}

			if (CPauseMenu::CheckInput(FRONTEND_INPUT_BACK, false, CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE | CHECK_INPUT_OVERRIDE_FLAG_WARNING_MESSAGE))
			{
				CancelProfanityCheck();
			}
		}
	}
	else if( m_eMenuState == eGalleryState_ProfanityCheckUnavilableName || m_eMenuState == eGalleryState_ProfanityCheckUnavilableMemeText )
	{
		CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "GLOBAL_ALERT_DEFAULT", "UNAVAL_PROFANITY_FILT", FE_WARNING_OK);

		if (CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT, false, CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE | CHECK_INPUT_OVERRIDE_FLAG_WARNING_MESSAGE))
		{
			CScaleformMenuHelper::SET_COLUMN_HIGHLIGHT(PM_COLUMN_LEFT, m_iSelectedIndexPerPage);

			if( m_eMenuState == eGalleryState_ProfanityCheckUnavilableMemeText )
			{
				CleanupMemeEditor();
			}

			m_eMenuState = eGalleryState_Maximize;
			SetMenuContext( m_iMemeTextEntered <= 0 ? eMenuState_MaximizeMode : eMenuState_ReviewTextMode );
		}
	}
	else if (m_eMenuState == eGalleryState_ProfanityCheckFailedName || m_eMenuState == eGalleryState_ProfanityCheckFailedMemeText )
	{
		CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, "GLOBAL_ALERT_DEFAULT", "ERROR_FAILEDPROFANITY", FE_WARNING_OK);

		if (CPauseMenu::CheckInput(FRONTEND_INPUT_ACCEPT, false, CHECK_INPUT_OVERRIDE_FLAG_STORAGE_DEVICE | CHECK_INPUT_OVERRIDE_FLAG_WARNING_MESSAGE))
		{
			CScaleformMenuHelper::SET_COLUMN_HIGHLIGHT(PM_COLUMN_LEFT, m_iSelectedIndexPerPage);

			if( m_eMenuState == eGalleryState_ProfanityCheckFailedName )
			{
				m_eMenuState = eGalleryState_Maximize;
				SetMenuContext(eMenuState_MaximizeMode);
			}
			else
			{
				if( m_iMemeTextEntered <= 0 )
				{
					m_eMenuState = eGalleryState_Maximize;
				}
				else
				{
					m_eMenuState = eGalleryState_ReviewMemeImage;
				}

				SetMenuContext( m_iMemeTextEntered <= 0 ? eMenuState_MaximizeMode : eMenuState_ReviewTextMode );
			}
		}
	}
	else if( m_eMenuState == eGalleryState_InMemeSaveProcess )
	{
		if( sm_memePhotoSaveCallbackReturned )
		{
			MemoryCardError saveStatus = CPhotoManager::GetSaveGivenPhotoStatus();
			if ( saveStatus == MEM_CARD_COMPLETE )
			{
				// Reset the save flag and cleanup the meme editor so next time we come around we'll
				// Drop into the else block below hopefully... 
				sm_memePhotoSaveCallbackReturned = false;
				CleanupMemeEditor();
			}
			else if( saveStatus == MEM_CARD_BUSY )
			{
				photoDisplayf("CGalleryMenu::Update - eGalleryState_InMemeSaveProcess. MEM_CARD_BUSY");
			}
			else if ( saveStatus == MEM_CARD_ERROR )
			{
				photoDisplayf("CGalleryMenu::Update - eGalleryState_InMemeSaveProcess. MEM_CARD_ERROR."
					"Save failed by CPhotoManager, returning to thumbnail view.");

				ReturnToThumbnailView();
				CPauseMenu::SetGalleryLoadingTexture(false);
			}
		}
		else if( !PHONEPHOTOEDITOR.IsRequestingSave() && !PHONEPHOTOEDITOR.IsWaitingUserInput() )
		{
			// ... now that the photo-editor is cleaned up, progress back to the normal gallery
			m_eMenuState = eGalleryState_AfterMemeSaveProcess;
		}
	}
	else if( m_eMenuState == eGalleryState_AfterMemeSaveProcess )
	{
		m_bReturnFromMemeSave = true;
		m_eMenuState = eGalleryState_InMenu;

		if( m_iCurrentPage == 0 )
		{
			CUndeletedEntryInMergedPhotoList undeletedEntry(m_iSelectedIndex + 1, true);
			CPhotoManager::RequestUnloadGalleryPhoto(undeletedEntry);

			if ( (m_ItemState[MAX_ITEMS_ON_PAGE-1] == eGalleryItemState_Loaded)
				|| (m_ItemState[MAX_ITEMS_ON_PAGE-1] == eGalleryItemState_Loading) )
			{
				photoDisplayf("CGalleryMenu::Update - eGalleryState_AfterMemeSaveProcess - about to request unload of the photo that was in slot 12 (index 11) and is now in slot 13 (index 12)");
				CUndeletedEntryInMergedPhotoList undeletedEntryOfPhotoThatWasInLastSlot(MAX_ITEMS_ON_PAGE, false);
				CPhotoManager::RequestUnloadGalleryPhoto(undeletedEntryOfPhotoThatWasInLastSlot);
			}
		}
		else
		{
			CPhotoManager::RequestUnloadAllGalleryPhotos();
		}

		m_iCurrentPage = 0;
		m_iSelectedIndex = 0;
		m_iSelectedIndexPerPage = 0;

		Repopulate( false );

		return;
	}
#if __UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME
	else if (m_eMenuState == eGalleryState_WaitForPhotoSaveToFinish)
	{
		CheckIfPhotoSaveHasFinished();
	}
#endif	//	__UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME
	else if (m_eMenuState == eGalleryState_InDeleteProcess)
	{
		MemoryCardError deleteStatus = CPhotoManager::GetDeleteGalleryPhotoStatus(m_EntryForDeletion);
		if (deleteStatus == MEM_CARD_COMPLETE && m_GalleryScanProgress == GALLERY_SCAN_PROGRESS_IDLE)
		{
			photoDisplayf("CGalleryMenu::Update - eGalleryState_InDeleteProcess - MEM_CARD_COMPLETE. ");

			m_EntryForDeletion.SetInvalid();
			m_eMenuState = eGalleryState_InMenu;
			m_bRepopulateOnDelete = true;

			if (SetErrorPages(true, sm_bDoOnlineErrorChecks))
			{
				photoDisplayf("CGalleryMenu::Update - eGalleryState_InDeleteProcess. No Photos left.  Displaying Empty Screen.");
				CPauseMenu::SetGalleryLoadingTexture(false);
			}
			else
			{
				photoDisplayf("CGalleryMenu::Update - eGalleryState_InDeleteProcess. Photos left. Number of Photos =  %d m_iSelectedIndex = %d", CPhotoManager::GetNumberOfPhotos(false), m_iSelectedIndex);
				CPauseMenu::SetGalleryLoadingTexture(false);

				if ((u32)m_iSelectedIndex == CPhotoManager::GetNumberOfPhotos(false))
				{
					photoDisplayf("CGalleryMenu::Update - eGalleryState_InDeleteProcess. m_iSelectedIndexPerPage =  %d m_iSelectedIndex = %d", m_iSelectedIndexPerPage, m_iSelectedIndex);


					if (m_iSelectedIndexPerPage > 0)
					{
						m_iSelectedIndexPerPage--;
						m_iSelectedIndex--;

						m_PhotoToLoad = m_iSelectedIndex;
					}
					else
					{
						if (m_iCurrentPage > 0)
							m_iCurrentPage--;

						m_iSelectedIndexPerPage = MAX_ITEMS_ON_PAGE - 1;
						m_iSelectedIndex--;
					}

				}

				Repopulate( false );

				SetDescription(true);
			}

		}
		else if (deleteStatus == MEM_CARD_BUSY)
		{
			photoDisplayf("CGalleryMenu::Update - eGalleryState_InDeleteProcess. MEM_CARD_BUSY");
			CPauseMenu::SetGalleryLoadingTexture(true);
		}
		else if (deleteStatus == MEM_CARD_ERROR && m_GalleryScanProgress == GALLERY_SCAN_PROGRESS_IDLE)
		{
			photoDisplayf("CGalleryMenu::Update - eGalleryState_InDeleteProcess. MEM_CARD_ERROR - Delete failed by CPhotoManager. Doing nothing.");

			m_EntryForDeletion.SetInvalid();
			m_eMenuState = eGalleryState_InMenu;
			m_bRepopulateOnDelete = false;

			//m_bRepopulateOnDelete = true;

			CPauseMenu::SetGalleryLoadingTexture(false);
			//Repopulate();
		}

	}
#if __LOAD_LOCAL_PHOTOS
	else if (m_eMenuState == eGalleryState_UploadingLocalPhotoToCloud)
	{
		CheckUploadOfLocalPhoto();
	}
#endif	//	__LOAD_LOCAL_PHOTOS

}


#if __UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME
void CGalleryMenu::CheckIfPhotoSaveHasFinished()
{
	CUndeletedEntryInMergedPhotoList undeletedEntry(m_IndexOfPhotoWhoseTitleHasBeenUpdated, false);
	
	switch (CPhotoManager::GetResaveMetadataForGalleryPhotoStatus(undeletedEntry))
	{
		case MEM_CARD_COMPLETE :
			{
				m_bReturnFromTitleResave = true;
				m_eMenuState = eGalleryState_InMenu;

				if( m_iCurrentPage == 0 )
				{
					photoDisplayf("CGalleryMenu::CheckIfPhotoSaveHasFinished - request unload of the maximised photo. It should be in slot 0 now");

					CUndeletedEntryInMergedPhotoList undeletedEntry(0, true);			//	If the save succeeded, the maximised photo should now be in slot 0
					CPhotoManager::RequestUnloadGalleryPhoto(undeletedEntry);
				}
				else
				{
					photoDisplayf("CGalleryMenu::CheckIfPhotoSaveHasFinished - request unload of all gallery photos");
					CPhotoManager::RequestUnloadAllGalleryPhotos();
				}

				m_iCurrentPage = 0;
				m_iSelectedIndex = 0;
				m_iSelectedIndexPerPage = 0;

				Repopulate( false );
			}
			break;

		case MEM_CARD_BUSY :
			photoDisplayf("CGalleryMenu::CheckIfPhotoSaveHasFinished - MEM_CARD_BUSY");
			break;

		case MEM_CARD_ERROR :
			photoDisplayf("CGalleryMenu::CheckIfPhotoSaveHasFinished - MEM_CARD_ERROR - Save failed by CPhotoManager, returning to thumbnail view.");

			ReturnToThumbnailView();
			CPauseMenu::SetGalleryLoadingTexture(false);
			break;
	}
}
#endif	//	__UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME


//===============================================================
// CGalleryMenu - UpdateGalleryScanProgress
// Purpose: Updates counts in AS for the scroll bar
//===============================================================
void CGalleryMenu::UpdateGalleryScanProgress()
{
	switch (m_GalleryScanProgress)
	{
		case GALLERY_SCAN_PROGRESS_CREATE_SORTED_LIST :
		{
			//	Wait until any previous photo manager operations are finished before starting

			if (CPhotoManager::IsQueueEmpty())
			{
				//	If this doesn't start successfully, should we just try again next frame? Or just give up and go to GALLERY_SCAN_PROGRESS_FINISHED?
				bool bSuccess = CPhotoManager::RequestCreateSortedListOfPhotos(false, true);
				if (bSuccess)
				{
					m_GalleryScanProgress = GALLERY_SCAN_PROGRESS_WAIT_FOR_SORTED_LIST;
					photoDisplayf("CGalleryMenu::Update - CREATE_SORTED_LIST - successfully queued");
				}
				else
				{
					photoDisplayf("CGalleryMenu::Update - CREATE_SORTED_LIST - failed to queue");
				}
			}
			else
			{
				photoDisplayf("CGalleryMenu::Update - CREATE_SORTED_LIST - waiting for PhotoManager queue to be empty");
			}
		}
		break;

		case GALLERY_SCAN_PROGRESS_WAIT_FOR_SORTED_LIST :
		{
			switch (CPhotoManager::GetCreateSortedListOfPhotosStatus(false, true))
			{
				case MEM_CARD_COMPLETE :
				{
					if (SetErrorPages(true, sm_bDoOnlineErrorChecks))
					{
						m_GalleryScanProgress = GALLERY_SCAN_PROGRESS_FINISHED;
					}
					else
					{
						RecalculateMaxPages();
						m_GalleryScanProgress = GALLERY_SCAN_PROGRESS_LOAD_TEXT_FOR_SONG_TITLES;

#if __UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME
						if( m_bReturnFromMemeSave || m_bReturnFromTitleResave)
#else
						if( m_bReturnFromMemeSave )
#endif	//	__UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME
						{
							// We can do this here on Meme save, as we know we'll always be on item zero and 
							// won't end up with navigation wonkiness
							ReturnToThumbnailView();
							CPauseMenu::SetGalleryLoadingTexture(false);
						}

						SetDescription(true);

						sMiniMapMenuComponent.SetActive(true);
						SetScrollBarCount();

					}
					
					break;
				}
				case MEM_CARD_BUSY :
				{
					break;
				}
				case MEM_CARD_ERROR :
				{
					photoDisplayf("CGalleryMenu::Update - WAIT_FOR_SORTED_LIST - GetCreateSortedListOfFilesStatus() has failed");
					break;
				}
			}
		}
		break;

		case GALLERY_SCAN_PROGRESS_LOAD_TEXT_FOR_SONG_TITLES :
		{
			TheText.RequestAdditionalText("TRACKID", RADIO_WHEEL_TEXT_SLOT);

			if (TheText.HasAdditionalTextLoaded(RADIO_WHEEL_TEXT_SLOT))
			{
				photoDisplayf("CGalleryMenu::Update - the text block containing the song titles has loaded. Moving on to load the first photo");
				m_GalleryScanProgress = GALLERY_SCAN_PROGRESS_BEGIN_LOAD_PHOTO;
				SUIContexts::Activate("GALLERY_DISABLE_DELETE");
				SetMenuContext(eMenuState_ThumbnailMode);
				SetScrollBarCount();
			}
		}
		break;

	case GALLERY_SCAN_PROGRESS_BEGIN_LOAD_PHOTO :
	{

		if (!m_bBuiltGalleryList)
		{
			photoDisplayf("CGalleryMenu::Update - BEGIN_LOAD_PHOTO - m_bBuiltGalleryList is false so call Populate now");

			m_bBuiltGalleryList = true;

			//CPauseMenu::SetBusySpinner(false);

			Populate(MENU_UNIQUE_ID_GALLERY);

		}
		if (CPhotoManager::RequestLoadGalleryPhoto(CUndeletedEntryInMergedPhotoList(m_PhotoToLoad, false)))
		{
			photoDisplayf("CGalleryMenu::Update - BEGIN_LOAD_PHOTO - RequestLoadGalleryPhoto called for photo %u", m_PhotoToLoad);
			m_GalleryScanProgress = GALLERY_SCAN_PROGRESS_CHECK_LOAD_PHOTO;
			DisplayPhotoLoadingInSlot(m_IndexToLoad);

			m_ItemState[m_IndexToLoad] = eGalleryItemState_Loading;

		}
		else
		{
			//	Either give up and go to GALLERY_SCAN_PROGRESS_FINISHED
			//	Or try calling RequestLoadGalleryPhoto again next frame
			//				m_GalleryScanProgress = GALLERY_SCAN_PROGRESS_FINISHED;
			m_GalleryScanProgress = GALLERY_SCAN_PROGRESS_CHECK_LOAD_PHOTO;
			photoDisplayf("CGalleryMenu::Update - BEGIN_LOAD_PHOTO - RequestLoadGalleryPhoto failed for photo %u", m_PhotoToLoad);
		}
	}
	break;

	case GALLERY_SCAN_PROGRESS_CHECK_LOAD_PHOTO :
	{
		MemoryCardError loadProgress = CPhotoManager::GetLoadGalleryPhotoStatus(CUndeletedEntryInMergedPhotoList(m_PhotoToLoad, false));

		switch (loadProgress)
		{
			case MEM_CARD_COMPLETE :
			case MEM_CARD_ERROR :

			if (MEM_CARD_COMPLETE == loadProgress)
			{
				photoDisplayf("CGalleryMenu::Update - CHECK_LOAD_PHOTO - GetLoadGalleryPhotoStatus completed. About to display photo %u in slot %u", m_PhotoToLoad, m_IndexToLoad);
				DisplayPhotoInSlot(m_PhotoToLoad, m_IndexToLoad);

				CUndeletedEntryInMergedPhotoList undeletedEntryRequiringABlip(m_PhotoToLoad, false);
				if (!CPhotoManager::GetIsMugshot(undeletedEntryRequiringABlip))
				{
					Vector3 vPosition;
					CPhotoManager::GetPhotoLocation(undeletedEntryRequiringABlip, vPosition);
					sMiniMapMenuComponent.CreateBlip(vPosition,m_IndexToLoad);
				}

				m_ItemState[m_IndexToLoad] = eGalleryItemState_Loaded;

				if (m_iSelectedIndexPerPage == (int)m_IndexToLoad && m_eMenuState == eGalleryState_InMenu)
				{
					CScaleformMenuHelper::SET_COLUMN_HIGHLIGHT(PM_COLUMN_LEFT, m_IndexToLoad);
					if (sMiniMapMenuComponent.DoesBlipExist(m_iSelectedIndexPerPage))
					{
						sMiniMapMenuComponent.SnapToBlipWithDistanceCheck(m_iSelectedIndexPerPage, MAP_DISTANCE_REQ);
					}

				}
			}
			else
			{
				DisplayPhotoInSlot(m_PhotoToLoad, m_IndexToLoad, true);
				m_ItemState[m_IndexToLoad] = eGalleryItemState_Corrupted;

				photoDisplayf("CGalleryMenu::Update - CHECK_LOAD_PHOTO - GetLoadGalleryPhotoStatus failed");
			}

			if (m_bPaging)
			{
				CScaleformMenuHelper::SET_COLUMN_FOCUS(PM_COLUMN_LEFT, true);
				m_bPaging = false;
			}

			if ( m_bRepopulateOnDelete && ((u32)m_iSelectedIndexPerPage == m_IndexToLoad) )
			{
				s32 sIndexToMoveTo = 0;

				if (CPhotoManager::GetNumberOfPhotos(false) > MAX_ITEMS_ON_PAGE)
				{
					sIndexToMoveTo = (s32)m_iSelectedIndexPerPage;
				}
				else
				{
					if ((u32)m_iSelectedIndexPerPage == 0 && CPhotoManager::GetNumberOfPhotos(false) == 0)
					{
						sIndexToMoveTo = -1;		// ABORT
					}
					else if ((u32)m_iSelectedIndexPerPage == CPhotoManager::GetNumberOfPhotos(false)-1)
					{
						// Already handled above
						sIndexToMoveTo = (s32)m_iSelectedIndexPerPage;
					}
					else

					{
						sIndexToMoveTo =(s32) m_iSelectedIndexPerPage;
					}
				}

				if (sIndexToMoveTo > -1)
				{
					CScaleformMenuHelper::SET_COLUMN_HIGHLIGHT(PM_COLUMN_LEFT, sIndexToMoveTo);
				}

				SetScrollBarCount();

			}

			m_PhotoToLoad++;
			m_IndexToLoad++;

			if ( (m_PhotoToLoad < CPhotoManager::GetNumberOfPhotos(false)) && (m_IndexToLoad < MAX_ITEMS_ON_PAGE) )
			{
					photoDisplayf("CGalleryMenu::Update - CHECK_LOAD_PHOTO - moving on to load photo %u into slot %u", m_PhotoToLoad, m_IndexToLoad);
					m_GalleryScanProgress = GALLERY_SCAN_PROGRESS_BEGIN_LOAD_PHOTO;
			}
			else
			{
				photoDisplayf("CGalleryMenu::Update - CHECK_LOAD_PHOTO - finished");
				m_GalleryScanProgress = GALLERY_SCAN_PROGRESS_FINISHED;
			}
			break;

			case MEM_CARD_BUSY :
				break;
			}
		}
		break;

	case GALLERY_SCAN_PROGRESS_FINISHED :

		CPauseMenu::SetGalleryLoadingTexture(false);
		m_bRepopulateOnDelete = false;
		m_bReturnFromMemeSave = false;
#if __UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME
		m_bReturnFromTitleResave = false;
#endif	//	__UPDATING_LOCAL_PHOTO_TITLE_AFFECTS_MOD_TIME

		SetScrollBarCount();
		SUIContexts::Deactivate("GALLERY_DISABLE_DELETE");
		m_GalleryScanProgress = GALLERY_SCAN_PROGRESS_IDLE;

#if __LOAD_LOCAL_PHOTOS
		//	Instead of UpdateUploadContextButton(), could I call SetMenuContext(eMenuState_ThumbnailMode); here?
		//	Or maybe SetMenuContext( m_ItemState[m_iSelectedIndexPerPage] == eGalleryItemState_Corrupted ? eMenuState_CorruptTexture : eMenuState_ThumbnailMode );

		UpdateUploadContextButton();
#endif	//	__LOAD_LOCAL_PHOTOS

		UpdateWaypointContextButton();

		CPauseMenu::RedrawInstructionalButtons();

		break;

	case GALLERY_SCAN_PROGRESS_IDLE:
		break;
	}


	if (m_bQueueSnapToBlipOnEntry && ( (m_ItemState[0] == eGalleryItemState_Loaded) || (m_ItemState[0] == eGalleryItemState_Corrupted) ) )
	{
		if (CPauseMenu::IsNavigatingContent())
		{
#if RSG_PC
			photomouseDisplayf("CGalleryMenu::UpdateGalleryScanProgress - m_bQueueSnapToBlipOnEntry is set and we're already navigating content so we don't need to call MENU_SHIFT_DEPTH. We'll call UpdateAfterSelectingAThumbnail() instead");
#endif	//	RSG_PC

			if (photoVerifyf(m_iSelectedIndexPerPage == 0, "CGalleryMenu::UpdateGalleryScanProgress - expected m_iSelectedIndexPerPage to be 0, but it's %d", m_iSelectedIndexPerPage))
			{
				UpdateAfterSelectingAThumbnail(FRONTEND_INPUT_LEFT, true, true);
			}
		}
		else
		{
#if RSG_PC
			photomouseDisplayf("CGalleryMenu::UpdateGalleryScanProgress - m_bQueueSnapToBlipOnEntry is set and we're not already navigating content so call MENU_SHIFT_DEPTH");
#endif	//	RSG_PC

			CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT).CallMethod( "MENU_SHIFT_DEPTH", 1 );
		}

		m_eMenuState = eGalleryState_InMenu;
		m_bQueueSnapToBlipOnEntry = false;
		if (m_ItemState[m_iSelectedIndexPerPage] == eGalleryItemState_Loaded )
		{
			if (sMiniMapMenuComponent.DoesBlipExist(m_iSelectedIndexPerPage))
			{
				sMiniMapMenuComponent.SnapToBlipWithDistanceCheck(m_iSelectedIndexPerPage, MAP_DISTANCE_REQ);
			}
		}
	}
}

//===============================================================
// CGalleryMenu - SetScrollBarCount
// Purpose: Updates counts in AS for the scroll bar
//===============================================================
void CGalleryMenu::SetScrollBarCount()
{
	photoDisplayf("CGalleryMenu::SetScrollBarCount called.");

	if( !SUIContexts::IsActive( "GALLERY_PLACE_TEXT" ) && !SUIContexts::IsActive( "GALLERY_REVIEW_TEXT" ) 
		&& m_eGalleryActionState == GA_INVALID )
	{
		if (!SUIContexts::IsActive("GALLERY_MAXIMIZE"))
		{
			char pszCaption[64];
			CNumberWithinMessage pArrayOfNumbers[2];
			pArrayOfNumbers[0].Set((float)m_iCurrentPage+1,0);
			pArrayOfNumbers[1].Set((float)m_iMaxNumberOfPages,0);

			CMessages::InsertNumbersAndSubStringsIntoString(TheText.Get("GAL_NUM_PAGES"),pArrayOfNumbers,2,NULL,0,pszCaption,64);
			CScaleformMenuHelper::SET_COLUMN_SCROLL(PM_COLUMN_LEFT, pszCaption);
		}
		else
		{
			CScaleformMenuHelper::SET_COLUMN_SCROLL(PM_COLUMN_LEFT, m_iSelectedIndex, (s32)CPhotoManager::GetNumberOfPhotos(false), MAX_ITEMS_ON_PAGE);
		}
	}
}

void CGalleryMenu::SetScrollBarArrows(eContextState eMenuState)
{
	CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);
	if( m_eGalleryActionState == GA_INVALID && pauseContent.BeginMethod("INIT_COLUMN_SCROLL") )
	{
		pauseContent.AddParam(0);
		pauseContent.AddParam(true);
		pauseContent.AddParam(2);

		if (eMenuState == eMenuState_ThumbnailMode && !RSG_PC)
		{
			pauseContent.AddParam(SCROLL_TYPE_ALL);
			pauseContent.AddParam(2);		
		}
		else
		{
			pauseContent.AddParam(SCROLL_TYPE_LEFTRIGHT);
			pauseContent.AddParam(2);
		}

		pauseContent.AddParam(true);
		pauseContent.EndMethod();
	}
}
//===============================================================
// CGalleryMenu - SetScrollBarCount
// Purpose: Updates counts in AS for the scroll bar
//===============================================================
void CGalleryMenu::DisplayPhotoInSlot(u32 indexOfPhotoTxd, u32 galleryItemIndex, bool bIsCorrupt)
{
	photoDisplayf("CGalleryMenu::DisplayPhotoInSlot called for photo index %u and item index %u", indexOfPhotoTxd, galleryItemIndex);

	if (photoVerifyf(indexOfPhotoTxd < CPhotoManager::GetNumberOfPhotos(false), "CGalleryMenu::DisplayPhotoInSlot - photo index %u is larger than the number of photos %u", indexOfPhotoTxd, CPhotoManager::GetNumberOfPhotos(false)))
	{
		if (photoVerifyf(galleryItemIndex < MAX_ITEMS_ON_PAGE, "CGalleryMenu::DisplayPhotoInSlot - item index %u is larger than the max items on the gallery page %u", galleryItemIndex, MAX_ITEMS_ON_PAGE))
		{
			CUndeletedEntryInMergedPhotoList undeletedEntry(indexOfPhotoTxd, false);

			char textureName[CTextureDictionariesForGalleryPhotos::ms_MaxLengthOfTextureName];

			if (bIsCorrupt)
			{
				safecpy(textureName, "", CTextureDictionariesForGalleryPhotos::ms_MaxLengthOfTextureName);
			}
			else
			{
				CPhotoManager::GetNameOfPhotoTextureDictionary(undeletedEntry, textureName, CTextureDictionariesForGalleryPhotos::ms_MaxLengthOfTextureName);
			}

			char streetZone[256]= {'\0'};
			char songInfo[96] = {'\0'};
			char creation_date_string[20] = {'\0'};

			if (!bIsCorrupt)
			{
				char streetName[96] = {'\0'};

				bool bPhotoIsAMugshot = CPhotoManager::GetIsMugshot(undeletedEntry);

				if (bPhotoIsAMugshot)
				{
					if (TheText.DoesTextLabelExist("MUG_LOC"))
					{
						formatf(streetZone,NELEM(streetZone),"%s", TheText.Get("MUG_LOC"));
					}
					else
					{
						safecpy(streetZone, "");
					}
				}
				else
				{
					Vector3 vPosition;
					CPhotoManager::GetPhotoLocation(undeletedEntry,vPosition);

					char zoneName[96] = {'\0'};
					GetStreetAndZoneStringsForPos(vPosition,streetName, NELEM(streetName),zoneName,NELEM(zoneName));

					if (strlen(streetName) > 0 && strlen(zoneName) > 0)
						formatf(streetZone,NELEM(streetZone),"%s - %s", streetName, zoneName);
					else if (strlen(streetName) > 0)
						formatf(streetZone,NELEM(streetZone),"%s", streetName);
					else if (strlen(zoneName) > 0)
						formatf(streetZone,NELEM(streetZone),"%s", zoneName);
					else
						formatf(streetZone,NELEM(streetZone),"");
				}
			


				if (bPhotoIsAMugshot)
				{
					safecpy(songInfo, "");
				}
				else
				{
					if (strlen( CPhotoManager::GetSongArtist(undeletedEntry)) > 0)
						formatf(songInfo,NELEM(songInfo),"%s - %s", CPhotoManager::GetSongArtist(undeletedEntry), CPhotoManager::GetSongTitle(undeletedEntry));
					else
						formatf(songInfo,NELEM(songInfo),"%s",TheText.Get("DESC_NOTRACK"));
				}


				CDate creationDate;
				bool bCreationDateIsValid = CPhotoManager::GetCreationDate(undeletedEntry, creationDate);

				if (bCreationDateIsValid)
				{
					bool stringIsValid = creationDate.ConstructStringFromDate(creation_date_string, NELEM(creation_date_string));

					if (stringIsValid)
					{
						photoDisplayf("CGalleryMenu::DisplayPhotoInSlot - creation date is %s", creation_date_string);
					}
					else
					{
						creation_date_string[0]='\0';
						photoDisplayf("CGalleryMenu::DisplayPhotoInSlot - failed to construct string from creation date");
					}
				}
				else
				{
					photoDisplayf("CGalleryMenu::DisplayPhotoInSlot - creation date of photo isn't valid");
				}
			}

			CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);

			if( pauseContent.BeginMethod("UPDATE_SLOT"))
			{
				pauseContent.AddParam(0);			// The columnIndex - should always be 0
				pauseContent.AddParam((s32)galleryItemIndex);
#if RSG_PC
				pauseContent.AddParam((s32)galleryItemIndex);
				pauseContent.AddParam(MENU_UNIQUE_ID_GALLERY_ITEM);
#else
				pauseContent.AddParam(999);
				pauseContent.AddParam(999);
#endif
				pauseContent.AddParam(bIsCorrupt? eGalleryItemState_Corrupted: eGalleryItemState_Loaded);
				pauseContent.AddParam(0);
				pauseContent.AddParam(1);
				
				if (bIsCorrupt)
				{
					pauseContent.AddParam("");
					pauseContent.AddParam("");
					pauseContent.AddParam("0");
					pauseContent.AddParam("0");
				}
				else
				{
					pauseContent.AddParam(CPhotoManager::GetTitleOfPhoto(undeletedEntry));
					pauseContent.AddParam(creation_date_string);
					pauseContent.AddParam(textureName);
					pauseContent.AddParam(textureName);
				}

				pauseContent.AddParam(1);
				pauseContent.AddParam(false);	//	 bIsCorrupt? false : CPhotoManager::GetIsPhotoBookmarked(undeletedEntry));
				pauseContent.AddParam(streetZone);
				pauseContent.AddParam(songInfo);

				bool bDisplaySocialClubIcon = false;
#if __LOAD_LOCAL_PHOTOS
				if (!bIsCorrupt)
				{
					if (CPhotoManager::IsListOfPhotosUpToDate(false))
					{
						bDisplaySocialClubIcon = CPhotoManager::GetHasPhotoBeenUploadedToSocialClub(undeletedEntry);
					}
					else
					{
						photoDisplayf("CGalleryMenu::DisplayPhotoInSlot - IsListOfPhotosUpToDate returned false so we can't check GetHasPhotoBeenUploadedToSocialClub to display the Social Club icon on the photo");
					}
				}
#endif
				pauseContent.AddParam(bDisplaySocialClubIcon);

				pauseContent.EndMethod();
			}
		}
	}
}

void CGalleryMenu::DisplayPhotoLoadingInSlot(s32 indexToLoad)
{
	photoDisplayf("CGalleryMenu::DisplayPhotoLoadingInSlot called for photo index %d and item index %d", indexToLoad, indexToLoad);

	CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);

	if( pauseContent.BeginMethod("UPDATE_SLOT"))
	{
		pauseContent.AddParam(0);			// The Column id
		pauseContent.AddParam(indexToLoad);			// The incrementing index 
#if RSG_PC
		pauseContent.AddParam(indexToLoad);  // The numbered slot the information is to be added to
		pauseContent.AddParam(MENU_UNIQUE_ID_GALLERY_ITEM);  // The numbered slot the information is to be added to
#else
		pauseContent.AddParam(999);  // The numbered slot the information is to be added to
		pauseContent.AddParam(999);  // The numbered slot the information is to be added to
#endif
		pauseContent.AddParam(eGalleryItemState_Loading);  // The menu ID
		pauseContent.AddParam(0);  // The unique ID
		pauseContent.AddParam(1);  // The unique ID
		pauseContent.AddParam("");  // The Menu item type (see below)
		pauseContent.AddParam("");  // The initial index of the slot (0 default, can be 0,1,2...x in a multiple selection)
		pauseContent.AddParam("");  // active or inactive
		pauseContent.AddParam("");  // The text label
		pauseContent.AddParam(1);  // The text label
		pauseContent.AddParam(false);  // The text label
		pauseContent.EndMethod();
	}

}

void CGalleryMenu::ClearGalleryIfPlayerIsSignedOut()
{
	CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);

	if( pauseContent.BeginMethod("SET_DATA_SLOT_EMPTY") )
	{
		pauseContent.AddParam(0);			// The Column id
		pauseContent.EndMethod();
	}

	SetDescription(false);

	CScaleformMenuHelper::HIDE_COLUMN_SCROLL(PM_COLUMN_LEFT);
	sMiniMapMenuComponent.ResetBlips();
	CMiniMap::SetVisible(false);

	//! Clean up any pending maximized view
	if( m_pGalleryTexture )
	{
		ReturnToThumbnailView();
	}

	SetMenuContext( eMenuState_Empty );
	CScaleformMenuHelper::HIDE_COLUMN_SCROLL( PM_COLUMN_LEFT );
}


bool CGalleryMenu::HasUserContentPrivileges()
{
#if RSG_ORBIS
	bool bIsUgcRestricted = false;

	//	Call GetUgcRestriction() directly to bypass the Playstation Plus check in rlGamerInfo::HasUserContentPrivileges() - Bug 2076043
	if (g_rlNp.GetUgcRestriction(NetworkInterface::GetLocalGamerIndex(), &bIsUgcRestricted) == SCE_OK)
	{
		if (!bIsUgcRestricted)
		{
			return true;
		}
	}
#else	//	RSG_ORBIS

//	This UserContentPrivileges check was copied from CommandHaveUserContentPrivileges()

#if RSG_XENON || RSG_PC || RSG_DURANGO
	// Xbox TRC TCR 094 - use every gamer index
	s32 userContentGamerIndex = GAMER_INDEX_EVERYONE;
#elif RSG_PS3
	// Playstation R4063 - use the local gamer index
	s32 userContentGamerIndex = GAMER_INDEX_LOCAL;
#endif

	if (CLiveManager::CheckUserContentPrivileges(userContentGamerIndex))
	{
		return true;
	}
#endif	//	RSG_ORBIS

	return false;
}

bool CGalleryMenu::HasOnlinePrivileges()
{
#if RSG_ORBIS
	//	Call IsNpAvailable() directly to bypass the Playstation Plus check in rlGamerInfo::HasMultiplayerPrivileges() - Bug 2099793
	return g_rlNp.IsNpAvailable(NetworkInterface::GetLocalGamerIndex());
#else
	return CLiveManager::CheckOnlinePrivileges();
#endif
}


CGalleryMenu::eGalleryActionState CGalleryMenu::CheckForGalleryProblems(bool bDoOnlineChecks)
{
	CGalleryMenu::eGalleryActionState returnValue = GA_INVALID;

#if __BANK
	if (CPhotoManager::sm_bPhotoNetworkErrorNotSignedInLocally)
	{
		photoDisplayf("CGalleryMenu::CheckForGalleryProblems - sm_bPhotoNetworkErrorNotSignedInLocally");
		return GA_NOT_SIGNED_IN_LOCALLY;
	}
	else if (bDoOnlineChecks && CPhotoManager::sm_bPhotoNetworkErrorCableDisconnected)
	{
		photoDisplayf("CGalleryMenu::CheckForGalleryProblems - sm_bPhotoNetworkErrorCableDisconnected");
		return GA_SOCIALCLUB_NOT_LINK_CONNECTED;
	}
	else if (bDoOnlineChecks && CPhotoManager::sm_bPhotoNetworkErrorPendingSystemUpdate)
	{
		photoDisplayf("CGalleryMenu::CheckForGalleryProblems - sm_bPhotoNetworkErrorPendingSystemUpdate");
		return GA_PENDING_SYSTEM_UPDATE;
	}
	else if (bDoOnlineChecks && CPhotoManager::sm_bPhotoNetworkErrorNotSignedInOnline)
	{
		photoDisplayf("CGalleryMenu::CheckForGalleryProblems - sm_bPhotoNetworkErrorNotSignedInOnline");
		return GA_NOT_SIGNED_IN;
	}
	else if (bDoOnlineChecks && CPhotoManager::sm_bPhotoNetworkErrorUserIsUnderage)
	{
		photoDisplayf("CGalleryMenu::CheckForGalleryProblems - sm_bPhotoNetworkErrorUserIsUnderage");
		return GA_SOCIALCLUB_IS_AGE_RESTRICTED;
	}
	else if (bDoOnlineChecks && CPhotoManager::sm_bPhotoNetworkErrorNoUserContentPrivileges)
	{
		photoDisplayf("CGalleryMenu::CheckForGalleryProblems - sm_bPhotoNetworkErrorNoUserContentPrivileges");
		return GA_NO_USER_CONTENT_PRIVILEGES;
	}
	else if (bDoOnlineChecks && CPhotoManager::sm_bPhotoNetworkErrorNoSocialSharing)
	{
		photoDisplayf("CGalleryMenu::CheckForGalleryProblems - sm_bPhotoNetworkErrorNoSocialSharing");
		return GA_NO_SOCIAL_SHARING;
	}
	else if (bDoOnlineChecks && CPhotoManager::sm_bPhotoNetworkErrorSocialClubNotAvailable)
	{
		photoDisplayf("CGalleryMenu::CheckForGalleryProblems - sm_bPhotoNetworkErrorSocialClubNotAvailable");
		return GA_SOCIALCLUB_NOT_ONLINE_ROS;
	}
	else if (bDoOnlineChecks && CPhotoManager::sm_bPhotoNetworkErrorNotConnectedToSocialClub)
	{
		photoDisplayf("CGalleryMenu::CheckForGalleryProblems - sm_bPhotoNetworkErrorNotConnectedToSocialClub");
		return GA_SOCIALCLUB_NOT_CONNECTED_TO_SOCIAL_CLUB;
	}
	else if (bDoOnlineChecks && CPhotoManager::sm_bPhotoNetworkErrorOnlinePolicyIsOutOfDate)
	{
		photoDisplayf("CGalleryMenu::CheckForGalleryProblems - sm_bPhotoNetworkErrorOnlinePolicyIsOutOfDate");
		return GA_SOCIALCLUB_ONLINE_POLICY_IS_NOT_UP_TO_DATE;
	}
#endif	//	__BANK


#if __LOAD_LOCAL_PHOTOS
	if (!CLiveManager::IsSignedIn())
	{
		photoDisplayf("CGalleryMenu::CheckForGalleryProblems - player is not signed in locally");
		returnValue = GA_NOT_SIGNED_IN_LOCALLY;
	}
	else
#endif	//	__LOAD_LOCAL_PHOTOS
	if(bDoOnlineChecks && !netHardware::IsLinkConnected())
	{	
		photoDisplayf("CGalleryMenu::CheckForGalleryProblems - player has no internet");

		returnValue = GA_SOCIALCLUB_NOT_LINK_CONNECTED;
	}
	else if (bDoOnlineChecks && !CLiveManager::IsOnline())
	{
		returnValue = GA_NOT_SIGNED_IN;

#if RSG_NP
		if( !g_rlNp.IsNpAvailable( NetworkInterface::GetLocalGamerIndex() ) )
		{
			const rlNpAuth::NpUnavailabilityReason c_npReason = g_rlNp.GetNpAuth().GetNpUnavailabilityReason( NetworkInterface::GetLocalGamerIndex() );
			photoDisplayf("CGalleryMenu::CheckForGalleryProblems - "
				"Network is not available. Reason code %d", c_npReason );

			if( c_npReason != rlNpAuth::NP_REASON_INVALID )
			{
				returnValue = GA_PENDING_SYSTEM_UPDATE;
			}
		}
#endif

		if (returnValue == GA_NOT_SIGNED_IN)
		{
			photoDisplayf("CGalleryMenu::CheckForGalleryProblems - Not signed in to console network");
		}
		else
		{
			photoDisplayf("CGalleryMenu::CheckForGalleryProblems - pending system update");
		}
	}
	else if(bDoOnlineChecks && CLiveManager::CheckIsAgeRestricted())
	{
		photoDisplayf("CGalleryMenu::CheckForGalleryProblems - player is under 18");

		returnValue = GA_SOCIALCLUB_IS_AGE_RESTRICTED;
	}
// #if RSG_ORBIS
// 	else if(bDoOnlineChecks && !CLiveManager::HasPlatformSubscription())
// 	{
// 		photoDisplayf("CGalleryMenu::CheckForGalleryProblems - player does not have platform subscription (Playstation Plus)");
// 
// 		returnValue = GA_NO_PLATFORM_SUBSCRIPTION;
// 	}
// #endif	//	RSG_ORBIS
#if !RSG_DURANGO
	else if (bDoOnlineChecks && !HasUserContentPrivileges())
	{
		photoDisplayf("CGalleryMenu::CheckForGalleryProblems - player does not have User Content privileges");

		returnValue = GA_NO_USER_CONTENT_PRIVILEGES;
	}
#endif	//	!RSG_DURANGO
	else if(bDoOnlineChecks && !CLiveManager::GetSocialNetworkingSharingPrivileges())
	{
		// Display system UI.
		XBOX_ONLY(CLiveManager::ResolvePlatformPrivilege(NetworkInterface::GetLocalGamerIndex(), rlPrivileges::PrivilegeTypes::PRIVILEGE_SOCIAL_NETWORK_SHARING, true));

		photoDisplayf("CGalleryMenu::CheckForGalleryProblems - doesn't have social network sharing privileges");

		returnValue = GA_NO_SOCIAL_SHARING;
	}
	else if(bDoOnlineChecks && !CLiveManager::IsOnlineRos() )
	{
		photoDisplayf("CGalleryMenu::CheckForGalleryProblems - Social Club not available");

		returnValue = GA_SOCIALCLUB_NOT_ONLINE_ROS;
	}
	else if (bDoOnlineChecks && !CLiveManager::GetSocialClubMgr().IsConnectedToSocialClub())
	{
		photoDisplayf("CGalleryMenu::CheckForGalleryProblems - Not signed in to social Club");

		returnValue = GA_SOCIALCLUB_NOT_CONNECTED_TO_SOCIAL_CLUB;
	}
	else if (bDoOnlineChecks && !CLiveManager::GetSocialClubMgr().IsOnlinePolicyUpToDate())
	{
		photoDisplayf("CGalleryMenu::CheckForGalleryProblems - Social Club online policies are out of date");

		returnValue = GA_SOCIALCLUB_ONLINE_POLICY_IS_NOT_UP_TO_DATE;
	}

	return returnValue;
}


bool CGalleryMenu::SetErrorPages(bool bCheckEmptyGallery, bool bDoOnlineChecks)
{
	bool bResult = false;

	eGalleryActionState galleryProblemCode = CheckForGalleryProblems(bDoOnlineChecks);

	if (galleryProblemCode != GA_INVALID)
	{
		m_eGalleryActionState = galleryProblemCode;
		DisplayWarningScreen(m_eGalleryActionState);
		bResult = true;

		switch (m_eGalleryActionState)
		{
			case GA_NOT_SIGNED_IN_LOCALLY :
			case GA_NOT_SIGNED_IN :
			case GA_PENDING_SYSTEM_UPDATE :
				ClearGalleryIfPlayerIsSignedOut();
				break;

			case GA_SOCIALCLUB_NOT_LINK_CONNECTED :
			case GA_SOCIALCLUB_IS_AGE_RESTRICTED :
			case GA_NO_SOCIAL_SHARING :
			case GA_SOCIALCLUB_NOT_ONLINE_ROS :
			case GA_SOCIALCLUB_NOT_CONNECTED_TO_SOCIAL_CLUB :
			case GA_SOCIALCLUB_ONLINE_POLICY_IS_NOT_UP_TO_DATE :
#if RSG_ORBIS
			case GA_NO_PLATFORM_SUBSCRIPTION :
#endif	//	RSG_ORBIS
			case GA_NO_USER_CONTENT_PRIVILEGES :
				sMiniMapMenuComponent.SetActive(false);
				break;

			default :
				photoAssertf(0, "CGalleryMenu::SetErrorPages - eGalleryActionState %d is not handled in this switch", (s32) m_eGalleryActionState);
				break;
		}
	}
	else if (CPhotoManager::GetNumberOfPhotos(false) == 0 && bCheckEmptyGallery)
	{
		photoDisplayf("CGalleryMenu::Update - eGalleryState_InDeleteProcess. No Photos left.  Displaying Empty Screen.");
		CScaleformMovieWrapper& pauseContent = CPauseMenu::GetMovieWrapper(PAUSE_MENU_MOVIE_CONTENT);

		if( pauseContent.BeginMethod("SET_DATA_SLOT_EMPTY") )
		{
			pauseContent.AddParam(0);			// The Column id
			pauseContent.EndMethod();
		}

		SetDescription(false);

		CScaleformMenuHelper::HIDE_COLUMN_SCROLL(PM_COLUMN_LEFT);

		sMiniMapMenuComponent.SetActive(false);

		// If we're online, have online privileges, and aren't yet connected to social club, show them the warning screen that entices the player to sign up/in to social club, else let's show the standard empty gallery message
		m_eGalleryActionState = (CLiveManager::IsOnline() && 
								HasOnlinePrivileges() && 
								!CLiveManager::GetSocialClubMgr().IsConnectedToSocialClub()) ? GA_GALLERY_EMPTY_NOT_CONNECTED_TO_SOCIAL_CLUB : GA_GALLERY_EMPTY;

		DisplayWarningScreen(m_eGalleryActionState);

		SetMenuContext(eMenuState_Empty);

		bResult = true;
	}

	return bResult;
}

bool CGalleryMenu::CanEnterMemeEditor() const
{
	if (CPauseMenu::IsSP())	// Not available in MP due to rendering constraints: B*1698009
	{
		CUndeletedEntryInMergedPhotoList maximisedEntry(m_iSelectedIndex, true);
		if (CPhotoManager::GetCanPhotoBeUploadedToSocialClub(maximisedEntry))
		{	//	A photo can only be uploaded to Social Club if it has a Valid CRC Signature.
			//	This is also the condition we should use to decide whether to allow the Meme Editor to be used for the photo.
			//	The Meme Editor will save a new photo with a recalculated signature (which will be valid) so that could be exploited to generate a valid signature for a dodgy photo.
			return true;
		}
	}

	return false;
}

void CGalleryMenu::SetupMemeEditor()
{
	if( !CanEnterMemeEditor() )
		return;

	photoAssertf( !PHONEPHOTOEDITOR.IsEditing(), "Logic Error - Attempting to double-enter meme editor!" );
	if( !PHONEPHOTOEDITOR.IsEditing() && m_pGalleryTexture && m_galleryTextureTxdId != -1 && m_galleryTextureLocalIndex != -1 )
	{
		m_iMemeTextEntered = 0;
		m_topHudColour = m_bottomHudColour = eOverlayTextColours_First;

		if( photoVerifyf( PHONEPHOTOEDITOR.StartEditing( m_galleryTextureTxdId.Get() ), "Unable to start editing photo! See above output for more detail" ) )
		{
			fwTxd* txd = g_TxdStore.GetSafeFromIndex( m_galleryTextureTxdId );
			if( txd )
			{
				txd->SetEntryUnsafe( m_galleryTextureLocalIndex, PHONEPHOTOEDITOR.GetTargetTexture() );
			}
		}
	}

	sm_indexOfPhotoForMemeMetadata = -1;
	sm_memePhotoSaveCallbackReturned = false;
	m_bIsTextMoveSoundPlaying = false;
	m_bIsTextZoomSoundPlaying = false;

#if RSG_PC
	SetDimensionsOfMaximizedPhoto();
#endif	//	RSG_PC

#if USE_TEXT_CANVAS
	CTextTemplate::SetCurrentTemplate("TEXT_TEMPLATE_SNAPMATIC");
	CTextTemplate::SetTemplateWindow(Vector2(0.0f, 0.0f), Vector2(0.5f, 0.5f));

	CTextTemplate::DefaultText();
	CTextTemplate::SetupTemplate(CTextTemplate::GetEditedText());
#endif // USE_TEXT_CANVAS
}

#if RSG_PC
void CGalleryMenu::SetDimensionsOfMaximizedPhoto()
{
	//	Jeff said that the rectangle has a width of 578 pixels and a height of 322 pixels
	//	On the 0...1 scale, he said that's 0.4515625 width x 0.447222 height (It must be using 1280 * 720)
	//	For 4:3, scale the width by vSize43 in pausemenu.xml
	//		<vPos x="0.162" y="0.222"/>
	//		<vSize x="1.0" y="1.0"/>
	//		<vPos43 x="0.08" y="0.222"/>
	//		<vSize43 x="1.238" y="1.0"/>

	if(const SGeneralPauseDataConfig* pData = CPauseMenu::GetMenuArray().GeneralData.MovieSettings.Access("PAUSE_MENU_SP_CONTENT"))
	{
		Vector2 vPos ( pData->vPos  );
		Vector2 vSize( pData->vSize );

		CHudTools::AdjustNormalized16_9ValuesForCurrentAspectRatio(WIDESCREEN_FORMAT_CENTRE, &vPos, &vSize);

		m_fMemePhotoPosX = vPos.x;
		m_fMemePhotoPosY = vPos.y;

		m_fMemePhotoWidth = 0.4515625f * vSize.x;
		m_fMemePhotoHeight = 0.447222f * vSize.y;
	}
}
#endif	//	RSG_PC

void CGalleryMenu::EnterMemeTextEntry( char const * const initialText )
{
	photoDisplayf("CGalleryMenu::UpdateInput - Adding meme text");

	static char16 s_TitleForKeyboardUI[ sm_maxKeyboardTitleLength ];
	static char16 s_intialTextForUI[ sm_maxMemeTextLength ];

	Utf8ToWide(s_TitleForKeyboardUI, TheText.Get("GAL_MEME_TEXT_TITLE"), sm_maxKeyboardTitleLength );
	if( initialText )
	{
		Utf8ToWide(s_intialTextForUI, initialText, sm_maxMemeTextLength );
	}

	ioVirtualKeyboard::Params params;
	params.m_AlphabetType = ioVirtualKeyboard::kAlphabetType_BASIC_ENGLISH;
	params.m_KeyboardType = ioVirtualKeyboard::kTextType_ALPHABET;
	params.m_MaxLength = sm_maxMemeTextLength;
	params.m_Title = s_TitleForKeyboardUI;
	params.m_InitialValue = initialText ? s_intialTextForUI : 0;

	m_eMenuState = eGalleryState_InKeyboardForMemeText;

	CControlMgr::ShowVirtualKeyboard(params);
}

void CGalleryMenu::RequestSaveMemePhotoCallback( u8* pJPEGBuffer, u32 size, bool bSuccess )
{
	sm_memePhotoSaveCallbackReturned = true;

	if( pJPEGBuffer && size && bSuccess && sm_indexOfPhotoForMemeMetadata >= 0 )
	{
		CPhotoManager::RequestSaveGivenPhoto( pJPEGBuffer, size, sm_indexOfPhotoForMemeMetadata );
	}
	else
	{
		photoErrorf("CGalleryMenu::RequestSaveMemePhotoCallback - failed");
	}
}

void CGalleryMenu::DetachMemeEditorTexture()
{
	fwTxd* txd = g_TxdStore.GetSafeFromIndex( m_galleryTextureTxdId );
	if( txd && txd->GetEntry( m_galleryTextureLocalIndex ) != m_pGalleryTexture )
	{
		txd->SetEntryUnsafe( m_galleryTextureLocalIndex, m_pGalleryTexture );
	}
}

void CGalleryMenu::CleanupMemeEditorAudio()
{
	if( m_bIsTextMoveSoundPlaying )
	{
		g_FrontendAudioEntity.StopSoundMapMovement();
		m_bIsTextMoveSoundPlaying = false;
	}

	if( m_bIsTextZoomSoundPlaying )
	{
		g_FrontendAudioEntity.StopSoundMapZoom();
		m_bIsTextZoomSoundPlaying = false;
	}
}

void CGalleryMenu::CleanupMemeEditor()
{
#if RSG_PC
	if(STextInputBox::GetInstance().IsActive())
	{
		STextInputBox::GetInstance().Close();
		STextInputBox::GetInstance().DestroyState();
	}
#endif // RSG_PC

	CleanupMemeEditorAudio();

	if( PHONEPHOTOEDITOR.IsEditing() || PHONEPHOTOEDITOR.IsWaitingUserInput() )
	{
		DetachMemeEditorTexture();

		PHONEPHOTOEDITOR.ResetTextParameters();
		PHONEPHOTOEDITOR.FinishEditing();

		m_iMemeTextEntered = 0;		

		SUIContexts::Deactivate("GALLERY_REVIEW_TEXT");
		SUIContexts::Deactivate("GALLERY_FINALIZE_TEXT");
		SUIContexts::Deactivate("GALLERY_PLACE_TEXT");
	}
}

void CGalleryMenu::SetupTextTemplates()
{
#if USE_TEXT_CANVAS
    CTextTemplate::Init();
    CTextTemplate::OpenMovie();
#endif
}

void CGalleryMenu::CleanupTextTemplates()
{
#if USE_TEXT_CANVAS
    CTextTemplate::CloseMovie();
    CTextTemplate::Shutdown();
#endif

}

void CGalleryMenu::GetStreetAndZoneStringsForPos(const Vector3& pos, char* streetName, u32 streetNameSize, char* zoneName, u32 zoneNameSize)
{
	CNodeAddress aNode;
	s32 NodesFound = ThePaths.RecordNodesInCircle(pos, 5.0f, 1, &aNode, false, false, false);
	if(NodesFound != 0)
	{
		u32 streetID = ThePaths.FindNodePointer(aNode)->m_streetNameHash;

		if(streetID != 0)
		{
			safecpy( streetName, TheText.Get(streetID, "Streetname"), streetNameSize );
		}
	}

	CPopZone *zone = CPopZones::FindSmallestForPosition(&pos, ZONECAT_ANY, ZONETYPE_SP);
	naAssertf(zone, "Attempted to find smallest zone for position but a null ptr was returned");
	if(zone)
	{
		safecpy( zoneName, zone->GetTranslatedName(), zoneNameSize );
	}
}

void CGalleryMenu::OnPresenceEvent(const rlPresenceEvent* evt)
{
	if (evt)
	{
		if (evt->GetId() == PRESENCE_EVENT_SIGNIN_STATUS_CHANGED)
		{
			const rlPresenceEventSigninStatusChanged* s = evt->m_SigninStatusChanged;
			CGalleryMenu* pGallery = verify_cast<CGalleryMenu*>(CPauseMenu::GetCurrentScreenData().GetDynamicMenu());

			if(s && ( s->SignedOffline() || s->SignedOut()))
			{
				pGallery->SetErrorPages(true, sm_bDoOnlineErrorChecks);
			}
			else if ( s && (s->SignedOnline() || s->SignedIn()))
			{
				pGallery->SetDelayedSignIn();
				m_iDelayedSignInCounter = fwTimer::GetSystemTimeInMilliseconds();
			}

		}
	}
}


void CGalleryMenu::UpdateWaypointContextButton()
{
	bool bShowWaypointButton = false;
	if (CPauseMenu::IsNavigatingContent())
	{
		if ( (m_GalleryScanProgress == GALLERY_SCAN_PROGRESS_FINISHED) || (m_GalleryScanProgress == GALLERY_SCAN_PROGRESS_IDLE) )
		{
			if ( (m_iSelectedIndex >= 0) && (m_iSelectedIndex < CPhotoManager::GetNumberOfPhotos(false)) )
			{
				if (CPhotoManager::IsListOfPhotosUpToDate(false))
				{
					if (photoVerifyf((m_iSelectedIndexPerPage >= 0) && (m_iSelectedIndexPerPage < m_ItemState.GetCount()), "CGalleryMenu::UpdateWaypointContextButton - m_iSelectedIndexPerPage = %d. Expected it to be between 0 and %d", m_iSelectedIndexPerPage, m_ItemState.GetCount()))
					{
						if (m_ItemState[m_iSelectedIndexPerPage] == eGalleryItemState_Loaded)
						{
							CUndeletedEntryInMergedPhotoList undeletedEntry(m_iSelectedIndex, false);
							if (!CPhotoManager::GetIsMugshot(undeletedEntry))
							{
								bShowWaypointButton = true;
							}
						}
						else
						{
							photoDisplayf("CGalleryMenu::UpdateWaypointContextButton - m_iSelectedIndexPerPage = %d m_ItemState for this index is %d. Since the photo isn't loaded, default to not showing the Waypoint button", m_iSelectedIndexPerPage, (s32) m_ItemState[m_iSelectedIndexPerPage]);
						}
					}
				}
				else
				{
					photoDisplayf("CGalleryMenu::UpdateWaypointContextButton - IsListOfPhotosUpToDate returned false so we can't check if the photo is a mugshot. Default to not showing the Waypoint button");
				}
			}
		}
	}

	if (bShowWaypointButton)
	{
		SUIContexts::Deactivate("GALLERY_DISABLE_WAYPOINT");
	}
	else
	{
		SUIContexts::Activate("GALLERY_DISABLE_WAYPOINT");
	}
}

#if __LOAD_LOCAL_PHOTOS
void CGalleryMenu::UpdateUploadContextButton()
{
	bool bShowUploadButton = false;
	if (CPauseMenu::IsNavigatingContent())
	{
		if ( (m_GalleryScanProgress == GALLERY_SCAN_PROGRESS_FINISHED) || (m_GalleryScanProgress == GALLERY_SCAN_PROGRESS_IDLE) )
		{
			if ( (m_iSelectedIndex >= 0) && (m_iSelectedIndex < CPhotoManager::GetNumberOfPhotos(false)) )
			{
				if (CPhotoManager::IsListOfPhotosUpToDate(false))
				{
					if (photoVerifyf((m_iSelectedIndexPerPage >= 0) && (m_iSelectedIndexPerPage < m_ItemState.GetCount()), "CGalleryMenu::UpdateUploadContextButton - m_iSelectedIndexPerPage = %d. Expected it to be between 0 and %d", m_iSelectedIndexPerPage, m_ItemState.GetCount()))
					{
						if (m_ItemState[m_iSelectedIndexPerPage] == eGalleryItemState_Loaded)
						{
							CUndeletedEntryInMergedPhotoList undeletedEntry(m_iSelectedIndex, false);
							if (!CPhotoManager::GetHasPhotoBeenUploadedToSocialClub(undeletedEntry) && CPhotoManager::GetCanPhotoBeUploadedToSocialClub(undeletedEntry))
							{
								bShowUploadButton = true;
							}
						}
						else
						{
							photoDisplayf("CGalleryMenu::UpdateUploadContextButton - m_iSelectedIndexPerPage = %d m_ItemState for this index is %d. Since the photo isn't loaded, default to not showing the Upload button", m_iSelectedIndexPerPage, (s32) m_ItemState[m_iSelectedIndexPerPage]);
						}
					}
				}
				else
				{
					photoDisplayf("CGalleryMenu::UpdateUploadContextButton - IsListOfPhotosUpToDate returned false so we can't check if the photo has already been uploaded. Default to not showing the Upload button");
				}
			}
		}
	}

	if (bShowUploadButton)
	{
		SUIContexts::Deactivate("GALLERY_DISABLE_UPLOAD");
	}
	else
	{
		SUIContexts::Activate("GALLERY_DISABLE_UPLOAD");
	}
}

void CGalleryMenu::BeginUploadOfLocalPhoto()
{
	photoDisplayf("CGalleryMenu::BeginUploadOfLocalPhoto - Confirm Upload - Accept - m_iSelectedIndex = %d, m_iSelectedIndexPerPage = %d", m_iSelectedIndex, m_iSelectedIndexPerPage);

	CUndeletedEntryInMergedPhotoList undeletedEntry(m_iSelectedIndex, false);
	if (CPhotoManager::RequestUploadLocalPhotoToCloud(undeletedEntry))
	{
		photoDisplayf("CGalleryMenu::BeginUploadOfLocalPhoto - RequestUploadLocalPhotoToCloud succeeded");

		m_EntryForUpload = m_iSelectedIndex;

		m_eMenuState = eGalleryState_UploadingLocalPhotoToCloud;

		// Set up instructional buttons to only display B for Back during the uploading of a photo
		SetMenuContext( eMenuState_Empty );
	}
	else
	{
		photoDisplayf("CGalleryMenu::BeginUploadOfLocalPhoto - RequestUploadLocalPhotoToCloud failed");
	}

	m_eGalleryActionState = GA_INVALID;
}

void CGalleryMenu::CheckUploadOfLocalPhoto()
{
	CUndeletedEntryInMergedPhotoList undeletedEntry(m_EntryForUpload, false);
	switch (CPhotoManager::GetUploadLocalPhotoToCloudStatus(undeletedEntry))
	{
		case MEM_CARD_COMPLETE :
			photoDisplayf("CGalleryMenu::CheckUploadOfLocalPhoto - upload has completed");
			m_eMenuState = eGalleryState_InMenu;
			m_EntryForUpload = -1;

			CPauseMenu::SetGalleryLoadingTexture(false);

			Repopulate( false );
			break;

		case MEM_CARD_BUSY :
			photoDisplayf("CGalleryMenu::CheckUploadOfLocalPhoto - upload is still in progress");

			// Pass in an extra parameter to say that the instructional buttons should be displayed even though the rest of the gallery page isn't rendered
			CPauseMenu::SetGalleryLoadingTexture(true, true);
			break;

		case MEM_CARD_ERROR :
			photoDisplayf("CGalleryMenu::CheckUploadOfLocalPhoto - upload has failed");
			m_eMenuState = eGalleryState_InMenu;
			m_EntryForUpload = -1;
			CPauseMenu::SetGalleryLoadingTexture(false);
			break;
	}
}

void CGalleryMenu::ProcessUploadWarningMessage()
{
	eWarningButtonFlags buttonFlags = FE_WARNING_NONE;

	//	Set up correct buttonFlags
	switch (m_UploadWarningType)
	{
		case GA_NOT_SIGNED_IN :							//	NOT_CONNECTED
			buttonFlags = FE_WARNING_OK;
			break;

		case GA_NOT_SIGNED_IN_LOCALLY :					//	NOT_CONNECTED_LOCAL
#if RSG_ORBIS // B*1817634 - Cannot show Sign-in UI on PS4
			buttonFlags = FE_WARNING_OK;
#else
			buttonFlags = FE_WARNING_OK_CANCEL;
#endif
			break;

		case GA_SOCIALCLUB_NOT_LINK_CONNECTED :			//	GAL_HUD_NOCONNECT
		case GA_PENDING_SYSTEM_UPDATE :					//	HUD_SYS_UPD_RQ, HUD_GAME_UPD_RQ, HUD_PROFILECHNG, HUD_DISCON, NOT_CONNECTED
		case GA_SOCIALCLUB_IS_AGE_RESTRICTED :			//	HUD_AGERES
		case GA_NO_SOCIAL_SHARING :						//	ERROR_GAL_SHARING_LOCAL
		case GA_SOCIALCLUB_NOT_ONLINE_ROS :				//	SCLB_NO_ROS
		case GA_NO_USER_CONTENT_PRIVILEGES :
			buttonFlags = FE_WARNING_OK;
			break;

#if RSG_ORBIS
		case GA_NO_PLATFORM_SUBSCRIPTION :
			buttonFlags = FE_WARNING_YES_NO;
			break;
#endif	//	RSG_ORBIS

		case GA_SOCIALCLUB_NOT_CONNECTED_TO_SOCIAL_CLUB :	//	ERROR_NO_SC_LOCAL
		case GA_SOCIALCLUB_ONLINE_POLICY_IS_NOT_UP_TO_DATE :	//	ERROR_UPDATE_SC_LOCAL
			buttonFlags = FE_WARNING_OK_CANCEL;
			break;

		default :
			photoAssertf(0, "CGalleryMenu::ProcessUploadWarningMessage - unexpected m_UploadWarningType value %d", (s32) m_UploadWarningType);
			break;
	}
	
	CWarningScreen::SetMessageWithHeader(WARNING_MESSAGE_STANDARD, GetWarningScreenTitle(m_UploadWarningType), GetWarningScreenString(m_UploadWarningType, true), buttonFlags);

	eWarningButtonFlags result = CWarningScreen::CheckAllInput();

	//	Check for button press and perform appropriate action
	switch (m_UploadWarningType)
	{
		case GA_NOT_SIGNED_IN :
			if( result == FE_WARNING_OK )
			{
				m_eGalleryActionState = GA_INVALID;
			}
			break;

		case GA_NOT_SIGNED_IN_LOCALLY :
#if RSG_ORBIS // B*1817634 - Cannot show Sign-in UI on PS4
			if( result == FE_WARNING_OK )
			{
				m_eGalleryActionState = GA_INVALID;
			}
#else
			if( result == FE_WARNING_OK )
			{
				CLiveManager::ShowSigninUi();
				m_eGalleryActionState = GA_INVALID;
			}
			else if( result == FE_WARNING_CANCEL )
			{
				m_eGalleryActionState = GA_INVALID;
			}
#endif
			break;

		case GA_SOCIALCLUB_NOT_LINK_CONNECTED :			//	GAL_HUD_NOCONNECT
		case GA_PENDING_SYSTEM_UPDATE :					//	HUD_SYS_UPD_RQ, HUD_GAME_UPD_RQ, HUD_PROFILECHNG, HUD_DISCON, NOT_CONNECTED
		case GA_SOCIALCLUB_IS_AGE_RESTRICTED :			//	HUD_AGERES
		case GA_NO_SOCIAL_SHARING :						//	ERROR_GAL_SHARING_LOCAL
		case GA_SOCIALCLUB_NOT_ONLINE_ROS :				//	SCLB_NO_ROS
		case GA_NO_USER_CONTENT_PRIVILEGES :
			if( result == FE_WARNING_OK )
			{
				m_eGalleryActionState = GA_INVALID;
			}
			break;

#if RSG_ORBIS
		case GA_NO_PLATFORM_SUBSCRIPTION :
			if(result == FE_WARNING_YES)
			{
				CLiveManager::ShowAccountUpgradeUI();
				m_eGalleryActionState = GA_INVALID;
			}
			else if (result == FE_WARNING_NO)
			{
				m_eGalleryActionState = GA_INVALID;
			}
			break;
#endif	//	RSG_ORBIS

		case GA_SOCIALCLUB_NOT_CONNECTED_TO_SOCIAL_CLUB :
		case GA_SOCIALCLUB_ONLINE_POLICY_IS_NOT_UP_TO_DATE :
			if( result == FE_WARNING_OK )
			{
				SocialClubMenu::SetTourHash(ATSTRINGHASH("Gallery",0x1a7e17bc));

	//			CPauseMenu::PlayInputSound(FRONTEND_INPUT_ACCEPT);
				CPauseMenu::EnterSocialClub();
				m_eGalleryActionState = GA_INVALID;
			}
			else if( result == FE_WARNING_CANCEL )
			{
				m_eGalleryActionState = GA_INVALID;
			}
			break;

		default :
			photoAssertf(0, "CGalleryMenu::ProcessUploadWarningMessage - unexpected m_UploadWarningType value %d", (s32) m_UploadWarningType);
			break;
	}
}

#endif	//	__LOAD_LOCAL_PHOTOS

// eof
