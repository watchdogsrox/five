/////////////////////////////////////////////////////////////////////////////////
//
// FILE     :  Text.cpp
// PURPOSE  :  Main wrapper class for all text functions
// AUTHOR   :  Derek Payne
// STARTED  :  17/04/09
//
/////////////////////////////////////////////////////////////////////////////////

// NOTES - this is still work in progress - a lot of stuff still needs shifting around

#ifndef TEXT_H
#define TEXT_H

// Rage headers
#include "rline/socialclub/rlsocialclubcommon.h"
#include "vector/color32.h"

// Framework headers
#include "fwmaths/Rect.h"

// Game headers
#include "Text/TextFile.h"

extern const float HACK_FOR_OLD_SCALE;

enum {
		// main fonts:
		FONT_STYLE_FIRST = 0,

		FONT_STYLE_STANDARD = FONT_STYLE_FIRST,
		FONT_STYLE_CURSIVE,
		FONT_STYLE_ROCKSTAR_TAG,
		FONT_STYLE_LEADERBOARD,
		FONT_STYLE_CONDENSED,
		FONT_STYLE_FIXED_WIDTH_NUMBERS,
		FONT_STYLE_CONDENSED_NOT_GAMERNAME,

		FONT_STYLE_PRICEDOWN,  // gta font

		FONT_STYLE_TAXI,

		FONT_MAX_FONT_STYLES
};

enum {
	FONT_CENTRE = 0,
	FONT_LEFT,
	FONT_RIGHT,
	FONT_JUSTIFY
};

// Holds current setting for font routines
class CTextLayout
{
public:
	//TextParams params;  // stores the scaleform font properties

	Vector2 Scale;
	u8 iLeading;
	u8 Style;
	bool bOutline;
	bool bOutlineCutout;
	bool bDrop;
	bool BackgroundOutline;
	bool Background;
	u8 Orientation;
	bool RenderUpwards;
	float ButtonScale;
	Color32 BGColor;
	Color32 Color;
	bool TextColours;
	bool bAdjustForNonWidescreen;
	float WrapStart;
	float WrapEnd;
	fwRect m_scissor;
#if RSG_PC
	int StereoFlag;
	bool bEnableMultiheadBoxWidthAdjustment;
#endif // RSG_PC

public:
	//
	// Scale
	//

	void  SetScale(const Vector2* vScale);
	void  SetScale(const Vector2& vScale);
	const Vector2& GetScale() const {return Scale;}

#if RSG_PC
	inline void SetStereo(int Stereo)
	{
		StereoFlag = Stereo;
	}

	inline void SetEnableMultiheadBoxWidthAdjustment(bool bEnable)
	{
		bEnableMultiheadBoxWidthAdjustment = bEnable;
	}
#endif // RSG_PC
	//
	// leading
	//
	inline void SetLeading(u8 iNewLeading)
	{
		iLeading = iNewLeading;
	}

	//
	// wrap
	//
	inline void SetWrap(const Vector2 *vNewWrap)
	{
		WrapStart = vNewWrap->x;
		WrapEnd = vNewWrap->y;
	}
	inline void SetWrap(const Vector2& vNewWrap)
	{
		WrapStart = vNewWrap.x;
		WrapEnd = vNewWrap.y;
	}

	//
	// text colours
	//
	inline void SetUseTextColours(bool bOnOff)
	{
		TextColours = bOnOff;
	}

	//
	// colour
	//
	inline void SetColor(Color32 NewColor)
	{
		Color = NewColor;
	}

	//
	// background color
	//
	inline void SetBackgroundColor(Color32 NewColor)
	{
		BGColor = NewColor;
	}

	//
	// 	ButtonScale;
	//
	inline void SetButtonScale(float fScale)
	{
		ButtonScale = fScale;
	}

	//
	// 	RenderUpwards;
	//
	inline void SetRenderUpwards(bool bOnOff)
	{
		RenderUpwards = bOnOff;
	}

	//
	// orientation
	//
	inline void SetOrientation(u8 iNewOrientation)
	{
		Orientation = iNewOrientation;
	}

	//
	// Background;
	//
	inline void SetBackground(bool bBackgroundOnOff, bool bOutlineOnOff = false)
	{
		Background = bBackgroundOnOff;
		BackgroundOutline = bOutlineOnOff;
	}

	//
	// Scissor;
	//
	inline void SetScissor(float const c_x, float const c_y, float const c_width, float const c_height)
	{
		m_scissor.left = c_x;
		m_scissor.top = c_y;
		m_scissor.right = c_width;
		m_scissor.bottom = c_height;
	}

	//
	// non widescreen adjustment:
	//
	void SetAdjustForNonWidescreen(bool bAdjust);

	//
	// BackgroundOutline;
	//
	inline void SetBackgroundOutline(bool bBackgroundOutlineOnOff)
	{
		BackgroundOutline = bBackgroundOutlineOnOff;
	}

	//
	// style
	//
	void SetStyle(s32 iNewStyle);

	//
	// edge
	//
	inline void SetEdge(bool bEdge)
	{
		bOutline = bEdge;
		bDrop = false;
	}

	inline void SetEdgeCutout(bool bEdgeCutOut)
	{
		bOutlineCutout = bEdgeCutOut;
	}

	inline void SetDropShadow(bool bDropShadow)
	{
		bDrop = bDropShadow;
		bOutline = false;
	}


	CTextLayout();
	~CTextLayout();

	void Default();
	float GetCharacterHeight();
	float GetTextPadding();

	void Render(const Vector2 *vPos, const char *pCharacters);
	void Render(const Vector2& vPos, const char *pCharacters);
	s32 GetNumberOfLines(const Vector2 *vPos, const char *pCharacters) const;
	s32 GetNumberOfLines(const Vector2& vPos, const char *pCharacters) const;
	float GetStringWidthOnScreen(const char *pCharacters, bool bIncludeSpaces) const;
};






class CText
{
public:
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static const char *GetLanguageFilename();
	static const char *GetLanguageFilename(int language);
	static char *GetFullFontLibFilename();
	static void InitScaleformFonts();
	static bool ShutdownScaleformFonts();
	static bool IsAsianLanguage();

	static rlScLanguage GetScLanguageFromCurrentLanguageSetting();
	static const char *GetStringOfSupportedLanguagesFromCurrentLanguageSetting();
    static bool IsScLanguageSupported(rlScLanguage language); 

	static void UnloadFont();

	//static float GetDefaultCharacterHeight();
	static void FilterOutTokensFromString(char *String);

	static void Flush();

	static bool AreScaleformFontsInitialised();

	static s32 GetFontMovieId() { return ms_iMovieId; }

#if __BANK
	static void InitWidgets();
	static void ShutdownWidgets();
	static void LoadFontValues();
#endif

private:
	static s32 ms_iMovieId;
	static char ms_cFontFilename[100];

};











#endif

