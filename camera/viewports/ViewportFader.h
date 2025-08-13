//
// name:		viewportFader.h
// description:	Class controlling viewport fades.
//
#ifndef INC_VIEWPORTFADER_H_
#define INC_VIEWPORTFADER_H_

#include "Debug\debug.h"

#define VIEWPORTFADER_PRINTSTACKTRACE  (!__NO_OUTPUT)

//-----------------------------------------------------------------------------
// A Viewport fader facilitator - has grown more complex than desired... sorry! 
class CViewportFader
{
public :
	CViewportFader()	{ Init();		}
	~CViewportFader()	{ Shutdown();	}

	void Init()			{ Reset();		}
	void Shutdown()		{ }
	void Reset();
	
	void				Set(	s32 duration, 
								bool	bReverseFade		= false, 
								const Color32 *pColour		= NULL, 
								bool	bHoldFadeAtEnd		= false);

	void				SetFixedFade(float fadeValue, const Color32 *pColor = NULL);
	
	void				Process();
	void				Render() const;

	bool				IsFading() const		{ return m_bIsFading; }
	bool				IsHolding() const		{ return m_bHoldFadeAtEnd; }
	bool				IsReversed() const		{ return m_bReverse; }
	bool				IsFadedIn() const		{ return !IsFading() && !IsHolding(); }
	bool				IsFadedOut() const		{ return !IsFading() && IsHolding(); }
	bool				IsFadingIn() const		{ return IsFading() && IsReversed(); }
	bool				IsFadingOut() const		{ return IsFading() && !IsReversed(); }
	float				GetAudioFade() const;
	int					GetFadeAlpha() const;
	int					CalcFadeAlpha() const;

private:
	void				CalculateFadeValue();

	// Current fade value, 0.0 = faded in, 1.0 = faded out
	float				GetWeight() const		{ return m_CurrentFade; }

	void				RenderFadeBasic() const;

	float				m_TargetFade;				// Target value for the fade (1.0 to fade into the color, 0.0 to fade out of it)
	float				m_CurrentFade;				// Current fade value (0.0=not faded, 1.0=completely faded)
	float				m_FadeSpeed;				// Number to add to m_CurrentFade every frame until it hits the target frame
	Color32				m_colour;					// The colour of the fade
	bool				m_bReverse			: 1;	// Visually a fade can reverse what it looks like, but doesnt reverse its midpoints ( get yer head around that )
	bool				m_bIsFading			: 1;	// an indication that a fade took place last frame ( for reseting safely )
	bool				m_bHoldFadeAtEnd	: 1;	// holds the fade at its end value, and reports the fade as complete

#	if !VIEWPORTFADER_PRINTSTACKTRACE
	static __forceinline void PrintStackTrace() {}
#	else
	static void PrintStackTrace();
#	endif
};




#endif // !INC_VIEWPORTFADER_H_
