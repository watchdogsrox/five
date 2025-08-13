//
// name:		ViewportFader.cpp
// description:	Class controlling viewport fades of many sorts.
//

// C++ hdrs
#include "stdio.h"

// Rage headers
#include "grcore/viewport.h"
#include "grcore/font.h"
#include "grcore/texture.h"
#include "grcore/im.h"

#include "script/thread.h"

#include "system/stack.h"

#include "vector/color32.h"
#include "vector/colors.h"

// Framework headers
#include "fwmaths/random.h"
//#include "fwmaths/Maths.h"
#include "fwsys/timer.h"

// Game headers
#include "Debug/Debug.h"

#include "camera/scripted/ScriptDirector.h"
#include "camera/viewports/Viewport.h"
#include "camera/viewports/ViewportFader.h"
#include "camera/viewports/ViewportManager.h"

#include "Frontend/Pausemenu.h"

#include "renderer/PostProcessFX.h"
#include "renderer/renderThread.h"
#include "renderer/DrawLists/DrawListMgr.h"

CAMERA_OPTIMISATIONS()


//---------------------------------------------------
// Reset all fading variables in class to safe values
void CViewportFader::Reset()
{
	m_bReverse				= false;
	m_colour				= Color32(0,0,0,255);
	m_bIsFading				= false;
	m_bHoldFadeAtEnd		= true;
	m_CurrentFade			= 1.0f;
	m_TargetFade			= 1.0f;
	m_FadeSpeed				= 0.0f;
}

//---------------------------------------------------
#if VIEWPORTFADER_PRINTSTACKTRACE
__COMMENT(static) void CViewportFader::PrintStackTrace()
{
	sysStack::PrintStackTrace();
	scrThread *const vm = scrThread::GetActiveThread();
	if (vm)
	{
		Displayf("Currently running script thread \"%s\"\n", vm->GetScriptName());
#if !__FINAL
		vm->PrintStackTrace();
#endif // !__FINAL
	}
	else
	{
		Displayf("No currently running script thread\n");
	}
}
#endif


//------------------------------------------------------------------
// Set a viewport to fade with a tonne of params ( mostly def args )
void CViewportFader::Set(s32 duration, bool bReverseFade, const Color32 *pColour, /*float	fadeMidPointStart, float	fadeMidPointEnd,*/ bool bHoldFadeAtEnd)
{
	PrintStackTrace();

	m_bReverse			= bReverseFade;
	m_bHoldFadeAtEnd    = bHoldFadeAtEnd;

	if (pColour)
		m_colour		= *pColour;

	m_TargetFade = (bReverseFade) ? 0.0f : 1.0f;

	if (duration == 0)
	{
		// Instant fade
		m_FadeSpeed = 60.0f;
	}
	else
	{
		m_FadeSpeed = 1000.0f / (float) duration;
	}

	m_FadeSpeed *= (bReverseFade) ? -1.0f : 1.0f;



	m_bIsFading			= true;

	CalculateFadeValue();
}

void CViewportFader::SetFixedFade(float fadeValue, const Color32 *pColor)
{
	PrintStackTrace();

	if (pColor)
		m_colour		= *pColor;

	m_CurrentFade = m_TargetFade = fadeValue;
	m_bIsFading = false;
	m_bHoldFadeAtEnd = true;
	m_FadeSpeed = 0.0f;
}

//-------------------------------------------------------------------------
// Is a viewport fading?... if pT not null return normalised time into fade
void CViewportFader::CalculateFadeValue()
{
	float elapsedTime = fwTimer::GetTimeStep_NonPausedNonScaledClipped();


	m_CurrentFade += m_FadeSpeed * elapsedTime;
	m_CurrentFade = Clamp(m_CurrentFade, 0.0f, 1.0f);

	// Are we done?
	m_bIsFading &= (m_CurrentFade != m_TargetFade);

	// No point in "holding fade" if we're not fading out. 
	m_bHoldFadeAtEnd &= (m_TargetFade > 0.0f);

	if (!m_bIsFading && !m_bHoldFadeAtEnd)
	{
		m_CurrentFade = m_TargetFade = m_FadeSpeed = 0.0f;
		m_colour = Color32(0,0,0,255);
	}
}

//--------------------------------------------------
//
float CViewportFader::GetAudioFade() const
{
	return GetWeight();
}


void CViewportFader::Process()
{
	CalculateFadeValue();
}



//--------------------------------------
// Render a viepworts fade instructions
void CViewportFader::Render() const
{
	if (IsFading() || m_bHoldFadeAtEnd) // is there something to actually render?... if not fading or not holding there is nothing to do.
	{
		RenderFadeBasic();
		if (GetWeight() > 0.0f)
		{
			PostFX::ResetAdaptedLuminance();
		}
	}
}

//-------------------------------------------------------------------
// not advisable function supplied to script. ( will be a frame out )
// a script shouldn't really be dependent on rendering state
int CViewportFader::CalcFadeAlpha() const
{
	if (IsFading() || m_bHoldFadeAtEnd)
	{
		return GetFadeAlpha();
	}

	return 0;
}

//--------------------------------------------------------
// 
int CViewportFader::GetFadeAlpha() const
{
	float alpha = GetWeight();
	return (int)(alpha*255.0f);
}

//--------------------------------------------------------
// Render a viewport fade to a colour- commonly used code//depot/gta5/build/dev/TROPDIR
void CViewportFader::RenderFadeBasic() const
{

	int fadeAlpha = GetFadeAlpha();
	if (fadeAlpha == 0)
		return;

	Color32 c = m_colour;
	c.SetAlpha(fadeAlpha);

	if(CSystem::IsThisThreadId(SYS_THREAD_RENDER))
	{
		CViewport::DrawBoxOnCurrentViewport(c);
	}
	else
	{
		DLC_Add (CViewport::DrawBoxOnCurrentViewport, c);
	}
}

