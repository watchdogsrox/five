// rage
#include "fwrenderer/renderthread.h"

// game
#include "frontend/PolicyMenu.h"

#include "frontend/scaleform/ScaleFormMgr.h"

#define SC_BUTTON_MOVIE_FILENAME	"PAUSE_MENU_INSTRUCTIONAL_BUTTONS"

sysCriticalSectionToken PolicyMenu::sm_renderingSection;


///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
VideoUploadPolicyMenu::VideoUploadPolicyMenu() 
{
	Shutdown();

	const char* pMovie = "ONLINE_POLICIES";

	m_scMovie.CreateMovieAndWaitForLoad(SF_BASE_CLASS_GENERIC, pMovie, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f));
	CScaleformMgr::ForceMovieUpdateInstantly(m_scMovie.GetMovieID(), true);

	m_buttonMovie.CreateMovieAndWaitForLoad(SF_BASE_CLASS_GENERIC, SC_BUTTON_MOVIE_FILENAME, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f));
	CScaleformMgr::ForceMovieUpdateInstantly(m_buttonMovie.GetMovieID(), true);
}

VideoUploadPolicyMenu::~VideoUploadPolicyMenu()
{
	Shutdown();

	m_buttonMovie.Clear();
}

void VideoUploadPolicyMenu::Init()
{
	sm_isActive = true;

	m_buttonMovie.CallMethod("CONSTRUCTION_INNARDS");

	GoToState(VU_SHOWING);
}

void VideoUploadPolicyMenu::Shutdown()
{
	BaseSocialClubMenu::Shutdown();

	m_state = VU_IDLE;
}

void VideoUploadPolicyMenu::Render()
{
	if(m_state != VU_IDLE)
	{
		m_scMovie.Render();
		m_buttonMovie.Render();
	}
}

void VideoUploadPolicyMenu::Update()
{
	if(m_state == VU_IDLE)
	{
		return;
	}


	DelayButtonCheck();

	if(!IsFlaggedForDeletion())
	{
		switch(m_state)
		{
		case VU_SHOWING:
			UpdateShowing();
			break;
		default : break;
		};
	}
}

void VideoUploadPolicyMenu::GoToState(eVUPolicyState newState)
{
	uiDebugf1("VideoUploadPolicyMenu::GoToState - Old State=%d, New State=%d.", m_state, newState);
	//	SetBusySpinner(false);

	switch(newState)
	{
	case VU_IDLE:
	case VU_ACCEPT:
	case VU_CANCEL:
		m_state = newState;
		break;
	case VU_SHOWING:
		GoToShowing();
		break;
	default :
		uiAssertf(0, "VideoUploadPolicyMenu::GoToState - Invalid state");
		break;
	};
}

void VideoUploadPolicyMenu::GoToShowing()
{
	m_state = VU_SHOWING;

	m_scMovie.CallMethod("DISPLAY_DOWNLOADED_POLICY");
	m_scMovie.CallMethod("SET_POLICY_TITLE", "VEUI_UPLOAD_POLICIES_TITLE");
	m_scMovie.CallMethod("SET_POLICY_TEXT", TheText.Get("VEUI_PUBLISH_ACCEPT_B"));
	m_scMovie.CallMethod("SCROLL_POLICY_TEXT", SO_STATIC);

	SetButtons(BUTTON_UPLOAD | BUTTON_CANCEL);
}

void VideoUploadPolicyMenu::UpdateShowing()
{
	if(CheckInput(FRONTEND_INPUT_BACK))
	{
		GoToState(VU_CANCEL);
	}
	else if(CheckInput(FRONTEND_INPUT_ACCEPT))
	{
		GoToState(VU_ACCEPT);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
void PolicyMenu::Open()
{
	uiDebugf1("PolicyMenu::Open");

	if(!SVideoUploadPolicyMenu::IsInstantiated())
	{
		SVideoUploadPolicyMenu::Instantiate();
		SVideoUploadPolicyMenu::GetInstance().Init();
	}
}

void PolicyMenu::Close()
{
	uiDebugf1("PolicyMenu::Close - Closing the PolicyMenu menu now.");

	gRenderThreadInterface.Flush();

	LockRender();

	if(SVideoUploadPolicyMenu::IsInstantiated())
	{
		SVideoUploadPolicyMenu::Destroy();
	}

	UnlockRender();
}

void PolicyMenu::UpdateWrapper()
{
	PF_AUTO_PUSH_TIMEBAR("PolicyMenu UpdateWrapper");

	// Note:  What happens here is static and can affect any open SocialClubMenu

	if(SVideoUploadPolicyMenu::IsInstantiated())
	{
		if(IsFlaggedForDeletion())
		{
			// we don't need this horrible delay like social club does
			sm_framesUntilDeletion = 0;
			Close();
			return;
		}		
		
		SVideoUploadPolicyMenu::GetInstance().Update();
	}
}

void PolicyMenu::RenderWrapper()
{
	// if flagged for deletion, don't render it
	if(IsFlaggedForDeletion())
	{
		return;
	}

	LockRender();
	if(SVideoUploadPolicyMenu::IsInstantiated())
	{
		SVideoUploadPolicyMenu::GetInstance().Render();
	}
	UnlockRender();
}


PolicyMenu::PolicyMenu()
{
	Shutdown();
}

PolicyMenu::~PolicyMenu()
{
	Shutdown();
}

void PolicyMenu::Init()
{
	sm_isActive = true;
}

bool PolicyMenu::HasAccepted()
{
	if(SVideoUploadPolicyMenu::IsInstantiated())
	{
		return SVideoUploadPolicyMenu::GetInstance().HasAccepted();
	}
	return false;
}

bool PolicyMenu::HasCancelled()
{
	if(SVideoUploadPolicyMenu::IsInstantiated())
	{
		return SVideoUploadPolicyMenu::GetInstance().HasCancelled();
	}
	return false;
}


// eof
