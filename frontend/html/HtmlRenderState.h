//
// filename:	HtmlRenderState.h
// description:	
//

#ifndef INC_HTMLRENDERSTATE_H_
#define INC_HTMLRENDERSTATE_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "vector/color32.h"
#include "vector/vector2.h"
// Game headers

// --- Defines ------------------------------------------------------------------
// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

namespace rage {
	class grcTexture;
}

struct HtmlRenderState {

	HtmlRenderState();

	HtmlRenderState(datResource& rsc);

	IMPLEMENT_PLACE_INLINE(HtmlRenderState);

#if __DECLARESTRUCT
	void DeclareStruct(datTypeStruct &s);
#endif

	void Inherit(const HtmlRenderState& state);
	void InheritTableState(const HtmlRenderState& state);
	void SetFontProperties();

	s32 m_display;
	// size of current frame
	float m_forcedWidth;
	float m_forcedHeight;
	float m_width;
	float m_height;
	float m_cellHeight;
	// position within current frame
	float m_currentX;
	float m_currentY;
	
	// background settings
	Color32 m_backgroundColor;
	datRef<grcTexture> m_pBackgroundImage;
	Vector2 m_backgroundPosition;
	s32 m_backgroundRepeat;

	// text settings
	Color32 m_color;
	//u32 m_direction;
	//u32 m_letterSpacing;
	s32 m_textAlign;
	s32 m_verticalAlign;
	s32 m_textDecoration;
	//u32 m_textIndent;
	//u32 m_textTransform;
	//u32 m_wordSpacing;
	// font settings
	u32 m_font;
	s32 m_fontSize;
	u32 m_fontStyle;
	u32 m_fontWeight;
	float m_lineHeight;
	// border settings
	Color32 m_borderBottomColor;
	u32 m_borderBottomStyle;
	float m_borderBottomWidth;
	Color32 m_borderLeftColor;
	u32 m_borderLeftStyle;
	float m_borderLeftWidth;
	Color32 m_borderRightColor;
	u32 m_borderRightStyle;
	float m_borderRightWidth;
	Color32 m_borderTopColor;
	u32 m_borderTopStyle;
	float m_borderTopWidth;
	// margins
	float m_marginBottom;
	float m_marginLeft;
	float m_marginRight;
	float m_marginTop;
	// padding
	float m_paddingBottom;
	float m_paddingLeft;
	float m_paddingRight;
	float m_paddingTop;
	// list
	//u32 m_listStyleImage;
	//u32 m_listStyleType;
	//size

	float m_cellpadding;
	float m_cellspacing;
	s32 m_colSpan;
	s32 m_rowSpan;

	// flags
	bool m_renderBackground;
	bool m_active;
	char m_pad[2];
	// active render state
	Color32 m_activeColor;
	s32 m_activeTextDecoration;
};


// --- Globals ------------------------------------------------------------------

#endif // !INC_HTMLRENDERSTATE_H_
