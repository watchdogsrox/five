/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : BusySpinner.cpp
// PURPOSE : allows control of the busy spinner via multiple systems.
//
// See: url:bugstar:966421 for reference.
//
/////////////////////////////////////////////////////////////////////////////////

#include "fwsys/gameskeleton.h"
#include "frontend/NewHud.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/GameStreamMgr.h"
#include "frontend/BusySpinner.h"
#include "frontend/MobilePhone.h"
#include "Frontend/ui_channel.h"
#include "SaveLoad/savegame_channel.h"
#include "SaveLoad/savegame_messages.h"
#include "Text/Messages.h"
#include "Text/TextConversion.h"

#if RSG_ORBIS
#include "control/videorecording/videorecording.h"
#endif

FRONTEND_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(ui,spinner)

int CBusySpinner::ms_ShowingIndex = SPINNER_SHOWING_NONE;
CBusySpinner::sSpinner CBusySpinner::ms_SpinnerList[SPINNER_SOURCE_MAX];
u32 CBusySpinner::ms_BackoffTime = 0;
sysTimer CBusySpinner::ms_SysTimer;
char CBusySpinner::ms_CurrentBodyText[SPINNER_MAX_MESSAGE_LEN];
int CBusySpinner::ms_CurrentIcon;
char CBusySpinner::ms_CurrentBodyTextActive[SPINNER_MAX_MESSAGE_LEN];
int CBusySpinner::ms_CurrentIconActive;
bool CBusySpinner::ms_IsBusySpinnerOn;
bool CBusySpinner::ms_IsBusySpinnerDisplaying;
atArray<s32> CBusySpinner::sm_iInstructionalButtonMovies;
bool CBusySpinner::sm_bReinitSpinnerOnAllMovies = false;
bool CBusySpinner::ms_bInstantUpdate = false;
sysTimer CBusySpinner::sm_time;


s32 CBusySpinner::ms_iSpinnerMovie = -1;
eBS_STATE CBusySpinner::ms_iSpinnerMovieState = BS_STATE_INACTIVE;


// ---------------------------------------------------------
void CBusySpinner::Init(unsigned initMode)
{
	spinnerDisplayf("CBusySpinner::Init - Mode %d", initMode);

    if( initMode == rage::INIT_CORE )
    {
		ms_BackoffTime = 0;
		ms_SysTimer.Reset();
		ms_ShowingIndex = SPINNER_SHOWING_NONE;
		for( int i=0; i!=SPINNER_SOURCE_MAX; i++ )
		{
			ms_SpinnerList[i].Icon = SPINNER_ICON_NONE;
			ms_SpinnerList[i].BodyText[0] = '\0';
		}
		ms_CurrentBodyText[0]='\0';
		ms_CurrentIcon = SPINNER_ICON_NONE;
		ms_CurrentBodyTextActive[0]='\0';
		ms_CurrentIconActive = SPINNER_ICON_NONE;
		ms_IsBusySpinnerOn = false;
		ms_IsBusySpinnerDisplaying = false;
	}

	sm_time.Reset();
}

// ---------------------------------------------------------
void CBusySpinner::Shutdown(unsigned shutdownMode)
{
	if ( shutdownMode == rage::SHUTDOWN_CORE )
	{
		spinnerDisplayf("Shutdown::Init - Mode SHUTDOWN_CORE");
#if __BANK
		ShutdownWidgets();
#endif
	}
	else if (shutdownMode == rage::SHUTDOWN_SESSION)
	{
		spinnerDisplayf("Shutdown::Init - Mode SHUTDOWN_SESSION");

		CBusySpinner::Off( SPINNER_SOURCE_SCRIPT );
	}
}

// ---------------------------------------------------------
void CBusySpinner::Update()
{
	CheckSpinnerMoviesAreStillActive();

	if( !CanChangeSpinner() )
	{
		return;
	}

	CheckSaveGameSystemMessages();

	ms_CurrentIcon = ms_SpinnerList[0].Icon;
	ms_CurrentBodyText[0] = '\0';

	int i = 1;
#if GTA_REPLAY
	// For replay editor save messages, always display the relevant text
	if (CSavingMessage::GetMessageType() == CSavingMessage::STORAGE_DEVICE_MESSAGE_SAVING_REPLAY || 
		CSavingMessage::GetMessageType() == CSavingMessage::STORAGE_DEVICE_MESSAGE_SAVING_REPLAY_PROJECT ||
		CSavingMessage::GetMessageType() == CSavingMessage::STORAGE_DEVICE_MESSAGE_SAVING_REPLAY_PROJECT_NO_TEXT ||
		CSavingMessage::GetMessageType() == CSavingMessage::STORAGE_DEVICE_MESSAGE_SAVING_REPLAY_THUMBNAIL)
	{
		char* pMessageText = CSavingGameMessage::GetMessageText();
		if( pMessageText[0] != '\0' )
		{
			i = 0;
		}
	}
#endif

	for( ; i!=SPINNER_SOURCE_MAX; i++ )
	{
		if( ms_SpinnerList[i].Icon != SPINNER_ICON_NONE )
		{
			if( ms_CurrentIcon == SPINNER_ICON_NONE )
			{
				spinnerDisplayf("CBusySpinner::Update - ms_CurrentIcon = %d. ",ms_SpinnerList[i].Icon );
				ms_CurrentIcon = ms_SpinnerList[i].Icon;
			}
			if( ms_CurrentBodyText[0]=='\0' )
			{
				spinnerDisplayf("CBusySpinner::Update - ms_CurrentBodyText = %s. ",ms_SpinnerList[i].BodyText );
				safecpy( ms_CurrentBodyText, ms_SpinnerList[i].BodyText, SPINNER_MAX_MESSAGE_LEN );
			}
		}
	}

	if( strcmp(ms_CurrentBodyText,ms_CurrentBodyTextActive)!=0 || ms_CurrentIcon!=ms_CurrentIconActive )
	{
		spinnerDisplayf("CBusySpinner::Update - strcmp(ms_CurrentBodyText,ms_CurrentBodyTextActive)!=0 || ms_CurrentIcon!=ms_CurrentIconActive ");
		spinnerDisplayf("CBusySpinner::Update - ms_CurrentBodyText = %s. ms_CurrentIcon = %d. ms_CurrentIconActive = %d.",ms_CurrentBodyText, ms_CurrentIcon, ms_CurrentIconActive);

		if( ms_CurrentIcon==SPINNER_ICON_NONE )
		{
			spinnerDisplayf("CBusySpinner::CheckSaveGameSystemMessages - Calling On on SPINNER_SOURCE_SAVEGAME. ");

			HideSpinnerOnAllMovies();  // fix for 1647673 - ensure any movies that are "spinner compatible" are cleared of any spinners too
			HideSpinner();  // this will actually remove the spinner if required.

			ms_IsBusySpinnerOn = false;
			ms_IsBusySpinnerDisplaying = false;
		}
		else
		{
			spinnerDisplayf("CBusySpinner::Update - ms_IsBusySpinnerOn = true");
			ms_IsBusySpinnerOn = true;
		}

		safecpy( ms_CurrentBodyTextActive, ms_CurrentBodyText, SPINNER_MAX_MESSAGE_LEN );
		ms_CurrentIconActive = ms_CurrentIcon;
	}

	UpdateSpinnerDisplay();
}


void CBusySpinner::Render()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CBusySpinner::Render() can only be called on the RenderThread!");

		return;
	}

	if ( (ms_iSpinnerMovieState == BS_STATE_ACTIVE || ms_iSpinnerMovieState == BS_STATE_RESETUP) && (CScaleformMgr::IsMovieActive(ms_iSpinnerMovie)) )
	{
		bool bRenderingAnInstructionalButtonMovie = false;

		for (s32 i = 0; i < sm_iInstructionalButtonMovies.GetCount(); i++)
		{
			if (CScaleformMgr::IsMovieRendering(sm_iInstructionalButtonMovies[i]))
			{
				bRenderingAnInstructionalButtonMovie = true;
				break;
			}
		}

		if (!bRenderingAnInstructionalButtonMovie)
		{
			GFxMovieView::ScaleModeType scaleMode = GFxMovieView::SM_ShowAll;

			if (!CHudTools::GetWideScreen())
				scaleMode = GFxMovieView::SM_NoBorder;		

			CScaleformMgr::ChangeMovieParams(ms_iSpinnerMovie, Vector2(0,0), Vector2(1.0f,1.0f), scaleMode);

			if (CBusySpinner::CanRender())  // last minute check to see if we can render it here.  Fixes ignorable assert 1752697
			{
				static u32 s_frameCount = 0;
				static s32 s_frameCounter =  0;

#define MAX_IDLE_FRAMES (10)
#define MAX_TIMESTEP_MS (1.2f)

				//if (s_frameCount != fwTimer::GetSystemFrameCount() || s_frameCounter > MAX_IDLE_FRAMES)  // only want to update this movie once per frame, or if we havnt had a different frame between 10 renders
				if(sm_time.GetMsTime() > 16.0f)
				{
					float const c_timeStepMs = sm_time.GetMsTimeAndReset() / 1000.0f;
					float const c_finalTimeStepMs = c_timeStepMs > MAX_TIMESTEP_MS ? MAX_TIMESTEP_MS : c_timeStepMs;  // ensures it never spins too fast

					CScaleformMgr::RenderMovie(ms_iSpinnerMovie, c_finalTimeStepMs, false, true);

					s_frameCount = fwTimer::GetSystemFrameCount();
					s_frameCounter = 0;
				}
				else
				{
					CScaleformMgr::DoNotUpdateThisFrame(ms_iSpinnerMovie);
					CScaleformMgr::RenderMovie(ms_iSpinnerMovie, 0.0f, false, false);

					s_frameCounter++;
				}
			}
		}
	}
}



void CBusySpinner::Preload(bool waitForLoad/*=false*/)
{
	if (!CScaleformMgr::IsMovieActive(ms_iSpinnerMovie))
	{
		if (waitForLoad)
			ms_iSpinnerMovie = CScaleformMgr::CreateMovieAndWaitForLoad("BUSY_SPINNER", Vector2(0,0), Vector2(1.0f,1.0f), false);
		else
			ms_iSpinnerMovie = CScaleformMgr::CreateMovie("BUSY_SPINNER", Vector2(0,0), Vector2(1.0f,1.0f), false);

		uiAssertf(ms_iSpinnerMovie != -1, "CBusySpinner - couldnt pre-load in BUSY_SPINNER movie!");
	}

	spinnerDisplayf("CBusySpinner:: Busy Spinner movie has been pre-loaded [%i], [%i]", ms_iSpinnerMovie, CScaleformMgr::IsMovieActive(ms_iSpinnerMovie));
}



// ---------------------------------------------------------
void CBusySpinner::On( const char* bodyText, int Icon, int sourceIndex )
{
	Assertf( sourceIndex>=0 && sourceIndex<SPINNER_SOURCE_MAX, "CBusySpinner: sourceIndex out of range");
	Assertf( bodyText!=NULL, "CBusySpinner: bodyText is NULL");

	if( bodyText != NULL && sourceIndex>=0 && sourceIndex<SPINNER_SOURCE_MAX )
	{
		sSpinner* spinner = &ms_SpinnerList[sourceIndex];

		if ( (spinner->Icon != Icon) || (strcmp(spinner->BodyText, bodyText) != 0) )
		{
#if !__NO_OUTPUT
			spinnerDisplayf("CBusySpinner::On called for sourceIndex %d", sourceIndex);
#endif	//	!__NO_OUTPUT

			spinner->Icon = Icon;
			safecpy( spinner->BodyText, bodyText, SPINNER_MAX_MESSAGE_LEN );

			if (ms_iSpinnerMovieState == BS_STATE_ACTIVE)  // fix for 1483847
			{
				if ( ms_iSpinnerMovie )
					ms_iSpinnerMovieState = BS_STATE_RESETUP;
				else
					ms_iSpinnerMovieState = BS_STATE_INACTIVE;
			}
		}
	}
}

// ---------------------------------------------------------
void CBusySpinner::Off( int sourceIndex )
{
	if(uiVerifyf(sourceIndex >= 0 && sourceIndex < SPINNER_SOURCE_MAX, "CBusySpinner::Off -- sourceIndex %d out of range", sourceIndex))
	{
#if !__NO_OUTPUT
		if (ms_SpinnerList[sourceIndex].Icon != SPINNER_ICON_NONE)
		{
			spinnerDisplayf("CBusySpinner::Off called for sourceIndex %d", sourceIndex);
		}
#endif	//	!__NO_OUTPUT

		ms_SpinnerList[sourceIndex].Icon		= SPINNER_ICON_NONE;
		ms_SpinnerList[sourceIndex].BodyText[0] = '\0';
		ms_bInstantUpdate = true;
	}
}

// ---------------------------------------------------------
bool CBusySpinner::IsOn()
{
	return( ms_IsBusySpinnerOn );
}

// ---------------------------------------------------------
bool CBusySpinner::IsDisplaying()
{
	return( ms_IsBusySpinnerDisplaying );
}

bool CBusySpinner::CanRender()
{
#if RSG_ORBIS
#if VIDEO_RECORDING_ENABLED
	if (VideoRecording::IsRecording() && !VideoRecording::IsPaused())
		return false;
#endif
#endif

	return (CScaleformMgr::IsMovieActive(ms_iSpinnerMovie));
}

// ---------------------------------------------------------
bool CBusySpinner::CanChangeSpinner()
{
	const bool bUpdateInstantlyThisFrame = ms_bInstantUpdate;

	ms_bInstantUpdate = false;  // reset for next frame now we have checked

	u32 currentTime =  ms_SysTimer.GetSystemMsTime();
	if( currentTime >= ms_BackoffTime || bUpdateInstantlyThisFrame)
	{
		ms_BackoffTime = (currentTime+SPINNER_BACKOFF_TIME);
		return( true );
	}
	return( false );
}

// ---------------------------------------------------------
void CBusySpinner::CheckSaveGameSystemMessages()
{
	char* pMessageText = CSavingGameMessage::GetMessageText();
	if( pMessageText[0] != '\0' || CSavingGameMessage::IsSavingMessageActive() )
	{
		char cHtmlFormattedHelpTextString[MAX_CHARS_FOR_TEXT_STRING];
		char* html = CTextConversion::TextToHtml(pMessageText,cHtmlFormattedHelpTextString,MAX_CHARS_FOR_TEXT_STRING);
		On( html, CSavingGameMessage::GetIconStyle(), SPINNER_SOURCE_SAVEGAME );

		spinnerDisplayf("CBusySpinner::CheckSaveGameSystemMessages - Calling On on SPINNER_SOURCE_SAVEGAME. ");
	}
	else
	{
		Off( SPINNER_SOURCE_SAVEGAME );
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CBusySpinner::UpdateSpinnerDisplay
// PURPOSE: main logic for spinner movies
/////////////////////////////////////////////////////////////////////////////////////
void CBusySpinner::UpdateSpinnerDisplay()
{
	if (!ms_IsBusySpinnerOn)
	{
		return;
	}

	if (ms_iSpinnerMovieState != BS_STATE_ACTIVE)
	{
		UpdateSpinnerStates();  // try to load in the spinner movie if its not loaded
	}
	else
	{
		// if a new movie has loaded in then reinit the call to display spinners
		if (sm_bReinitSpinnerOnAllMovies)
		{
			spinnerDisplayf("CBusySpinner::UpdateSpinnerDisplay - sm_bReinitSpinnerOnAllMovies = true.  DisplayingSpinnersOnAllMovies. ");
			DisplaySpinnerOnAllMovies();
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CBusySpinner::CheckSpinnerMoviesAreStillActive
// PURPOSE: goes through list of "spinner compatibile" movies and sees if they are
//			still active
/////////////////////////////////////////////////////////////////////////////////////
void CBusySpinner::CheckSpinnerMoviesAreStillActive()
{
	// we need to check that these movies we store are still active
	for (s32 i = 0; i < sm_iInstructionalButtonMovies.GetCount(); i++)
	{
		s32 iMovieId = sm_iInstructionalButtonMovies[i];
		if ( (!CScaleformMgr::HasMovieGotAnyState(iMovieId)) || (CScaleformMgr::IsMovieShuttingDown(iMovieId)) )
		{
			// remove this from the list if its not active at this stage any longer
			sm_iInstructionalButtonMovies.DeleteFast(i);
			sm_bReinitSpinnerOnAllMovies = true;
			spinnerDisplayf("CBusySpinner - detected Movie %d no longer active so decreasing number of spinner compatible movies to %d", iMovieId, sm_iInstructionalButtonMovies.GetCount());
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CBusySpinner::RegisterInstructionalButtonMovie
// PURPOSE: add a "spinner compatible" movie to the list
/////////////////////////////////////////////////////////////////////////////////////
void CBusySpinner::RegisterInstructionalButtonMovie(s32 iNewButtonMovieId)
{
	bool bFound = false;

	for (s32 i = 0; i < sm_iInstructionalButtonMovies.GetCount(); i++)
	{
		if (iNewButtonMovieId == sm_iInstructionalButtonMovies[i])
		{
			bFound = true;
			break;
		}
	}

	if (!bFound)  // if not found in existing list, then add it
	{
		sm_iInstructionalButtonMovies.PushAndGrow(iNewButtonMovieId);
		sm_bReinitSpinnerOnAllMovies = true;

		spinnerDisplayf("Movie %d registered as a 'spinner compatible movie increasing number to %d", iNewButtonMovieId, sm_iInstructionalButtonMovies.GetCount());
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CBusySpinner::UnregisterInstructionalButtonMovie
// PURPOSE: removes the "spinner compatible" movie to the list
/////////////////////////////////////////////////////////////////////////////////////
void CBusySpinner::UnregisterInstructionalButtonMovie(s32 iButtonMovieId)
{
	for (s32 i = 0; i < sm_iInstructionalButtonMovies.GetCount(); i++)
	{
		if (iButtonMovieId == sm_iInstructionalButtonMovies[i])
		{
			sm_iInstructionalButtonMovies.DeleteFast(i);

			spinnerDisplayf("Movie %d un-registered as a 'spinner compatible movie decreasing number to %d", iButtonMovieId, sm_iInstructionalButtonMovies.GetCount());
			return;
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CBusySpinner::SetSavingText
// PURPOSE: invokes the scaleform method on the movie passed
/////////////////////////////////////////////////////////////////////////////////////
void CBusySpinner::SetSavingText(s32 iMovieId)
{
	if (CScaleformMgr::IsMovieActive(iMovieId))
	{
		spinnerDisplayf("CBusySpinner::SetSavingText has been called on movie %d.", iMovieId );
		if (CScaleformMgr::BeginMethod(iMovieId, SF_BASE_CLASS_GENERIC, "SET_SAVING_TEXT"))
		{
			CScaleformMgr::AddParamInt(ms_CurrentIcon);
			CScaleformMgr::AddParamString(ms_CurrentBodyText);
			CScaleformMgr::EndMethod();
			ms_IsBusySpinnerDisplaying = true;
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CBusySpinner::HideSavingText
// PURPOSE: invokes the scaleform method on the movie passed
/////////////////////////////////////////////////////////////////////////////////////
void CBusySpinner::HideSavingText(s32 iMovieId)
{
	if (CScaleformMgr::IsMovieActive(iMovieId))
	{
		spinnerDisplayf("CBusySpinner::HideSavingText has been called on movie: %d.", iMovieId);

		if (CScaleformMgr::BeginMethod(iMovieId, SF_BASE_CLASS_GENERIC, "REMOVE_SAVING"))
		{
			CScaleformMgr::EndMethod();
			ms_IsBusySpinnerDisplaying = false;
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CBusySpinner::DisplaySpinnerOnAllMovies
// PURPOSE: goes through all movies and invokes methods on them all
/////////////////////////////////////////////////////////////////////////////////////
void CBusySpinner::DisplaySpinnerOnAllMovies()
{
	bool bFoundMovieThatIsntReadyForMethod = false;

	spinnerDisplayf("CBusySpinner::DisplaySpinnerOnAllMovies has been called");
	
	SetSavingText(ms_iSpinnerMovie);

	for (s32 i = 0; i < sm_iInstructionalButtonMovies.GetCount(); i++)
	{
		if (CScaleformMgr::IsMovieActive(sm_iInstructionalButtonMovies[i]))
		{
			SetSavingText(sm_iInstructionalButtonMovies[i]);
		}
		else
		{
			if (CScaleformMgr::HasMovieGotAnyState(sm_iInstructionalButtonMovies[i]))  // has a state but its not active
			{
				bFoundMovieThatIsntReadyForMethod = true;
			}
		}
	}

	if (!bFoundMovieThatIsntReadyForMethod)
	{
		sm_bReinitSpinnerOnAllMovies = false;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CBusySpinner::HideSpinnerOnAllMovies
// PURPOSE: goes through all movies and invokes methods on them all
/////////////////////////////////////////////////////////////////////////////////////
void CBusySpinner::HideSpinnerOnAllMovies()
{
	spinnerDisplayf("CBusySpinner::HideSpinnerOnAllMovies has been called");

	for (s32 i = 0; i < sm_iInstructionalButtonMovies.GetCount(); i++)
	{
		HideSavingText(sm_iInstructionalButtonMovies[i]);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CBusySpinner::UpdateSpinnerStates
// PURPOSE: logic of loading in the spinner movie
/////////////////////////////////////////////////////////////////////////////////////
void CBusySpinner::UpdateSpinnerStates()
{
#ifdef USE_THEFEED_SAVING_MESSAGE
	CGameStreamMgr::GetGameStream()->BusySpinnerOn( pMessageText, iIconStyle );
#else
	switch (ms_iSpinnerMovieState)
	{
		case BS_STATE_INACTIVE:
		{
			if (!CScaleformMgr::IsMovieActive(ms_iSpinnerMovie))
			{
				ms_iSpinnerMovie = CScaleformMgr::CreateMovie("BUSY_SPINNER", Vector2(0,0), Vector2(1.0f,1.0f), false);
				
				if (uiVerifyf(ms_iSpinnerMovie != -1, "CBusySpinner - couldnt load in BUSY_SPINNER movie!"))
				{
					ms_iSpinnerMovieState = BS_STATE_LOADING;
				}
			}

			// fall through....
		}

		case BS_STATE_LOADING:
		{
			if (CScaleformMgr::IsMovieActive(ms_iSpinnerMovie))
			{
				ms_iSpinnerMovieState = BS_STATE_SETUP;
			}

			// fall through...
		}

		case BS_STATE_SETUP:
		case BS_STATE_RESETUP:
		{
			if (CScaleformMgr::IsMovieActive(ms_iSpinnerMovie))
			{
				DisplaySpinnerOnAllMovies();

				ms_iSpinnerMovieState = BS_STATE_ACTIVE;
			}

			break;
		}

		default:
		{
			// nothing
			break;
		}
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CBusySpinner::HideSpinner
// PURPOSE: logic of unloading in the spinner movie
/////////////////////////////////////////////////////////////////////////////////////
void CBusySpinner::HideSpinner()
{
#ifdef USE_THEFEED_SAVING_MESSAGE
	CGameStreamMgr::GetGameStream()->BusySpinnerOff();
#else
	spinnerDisplayf("CBusySpinner::HideSpinner has been called");

	switch (ms_iSpinnerMovieState)
	{
		case BS_STATE_INACTIVE:
		{
			spinnerDisplayf("CBusySpinner::HideSpinner - BS_STATE_INACTIVE");
			// nothing, its not there
			break;
		}

		case BS_STATE_LOADING:
		case BS_STATE_SETUP:
		case BS_STATE_RESETUP:
		{
			spinnerDisplayf("CBusySpinner::HideSpinner - BS_STATE_LOADING / BS_STATE_SETUP / BS_STATE_RESETUP. ");
			if (CScaleformMgr::IsMovieActive(ms_iSpinnerMovie))
			{
				spinnerDisplayf("CBusySpinner::HideSpinner - RequestRemoveMovie. ");
				CScaleformMgr::RequestRemoveMovie(ms_iSpinnerMovie);
			}

			ms_iSpinnerMovie = -1;

			break;
		}

		case BS_STATE_ACTIVE:
		{
			spinnerDisplayf("CBusySpinner::HideSpinner - BS_STATE_ACTIVE ");

			HideSpinnerOnAllMovies();

			if (CScaleformMgr::IsMovieActive(ms_iSpinnerMovie))
			{
				spinnerDisplayf("CBusySpinner::HideSpinner - RequestRemoveMovie. ");
				CScaleformMgr::RequestRemoveMovie(ms_iSpinnerMovie);
			}

			ms_iSpinnerMovie = -1;

			break;
		}

		default:
		{
			spinnerDisplayf("CBusySpinner::HideSpinner - default ");
			// nothing
			break;
		}
	}

	ms_iSpinnerMovieState = BS_STATE_INACTIVE;
	
#endif
}


// ----------------------------------------------------------
// Debug
// ----------------------------------------------------------

#if __BANK

static bool ms_bBankSpinnerWidgetsCreated;
static char ms_cBankSpinnerSourceIndex[8];
static char ms_cBankSpinnerIcon[8];
static char ms_cBankSpinnerBody[128];

// ---------------------------------------------------------
void CBusySpinner::DebugSpinnerPreload()
{
	CBusySpinner::Preload();
}

// ---------------------------------------------------------
void CBusySpinner::DebugSpinnerOn()
{
	CBusySpinner::On( ms_cBankSpinnerBody, atoi(ms_cBankSpinnerIcon), atoi(ms_cBankSpinnerSourceIndex) );
}

// ---------------------------------------------------------
void CBusySpinner::DebugSpinnerOff()
{
	CBusySpinner::Off( atoi(ms_cBankSpinnerSourceIndex) );
}

// ---------------------------------------------------------
void CBusySpinner::DebugCreateTheSpinnerBankWidgets()
{
	if( ms_bBankSpinnerWidgetsCreated )
	{
		return; // Please do not press this button again.
	}

	bkBank* bank = BANKMGR.FindBank("ui");
	if( bank == NULL )
	{
		return;
	}
	
	bank->PushGroup("BusySpinner");
		
		strcpy( ms_cBankSpinnerSourceIndex, "0" );
		strcpy( ms_cBankSpinnerIcon, "1" );
		memset( ms_cBankSpinnerBody, 0, sizeof(ms_cBankSpinnerBody) );
		bank->AddButton("Preload", &DebugSpinnerPreload);
		bank->AddButton("On", &DebugSpinnerOn);
		bank->AddButton("Off", &DebugSpinnerOff);
		bank->AddSeparator();
		bank->AddText("Body", ms_cBankSpinnerBody, sizeof(ms_cBankSpinnerBody), false);
		bank->AddText("Icon", ms_cBankSpinnerIcon, sizeof(ms_cBankSpinnerIcon), false);
		bank->AddText("Source Index", ms_cBankSpinnerSourceIndex, sizeof(ms_cBankSpinnerSourceIndex), false);
			

	bank->PopGroup(); // "BusySpinner"
	ms_bBankSpinnerWidgetsCreated = true;
}

// ---------------------------------------------------------
void CBusySpinner::InitWidgets()
{
	ms_bBankSpinnerWidgetsCreated = false;

	bkBank* pBank = BANKMGR.FindBank( "ui" );
	if( pBank == NULL )
	{
		pBank = &BANKMGR.CreateBank( "ui" );
	}

	if( pBank != NULL )
	{
		pBank->AddButton("Create the BusySpinner widgets", &DebugCreateTheSpinnerBankWidgets);
	}
}

// ---------------------------------------------------------
void CBusySpinner::ShutdownWidgets()
{
	bkBank *bank = BANKMGR.FindBank( "ui" );
	if( bank != NULL )
	{
		bank->Destroy();
	}
}

#endif // __BANK

