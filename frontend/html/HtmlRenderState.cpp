//
// filename:	HtmlRenderState.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
// Game headers
#include "HtmlLanguage.h"
#include "HtmlRenderState.h"
#include "HtmlDocument.h"
#ifndef HTML_RESOURCE 
#include "Htmlviewport.h"
#include "Text/Text.h"
#include "camera/viewports/Viewport.h"
#endif
// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

// --- HtmlState ---------------------------------------------------------------

//
// name:		HtmlRenderState::HtmlRenderState
// description:	Setup default render state
HtmlRenderState::HtmlRenderState()
{
	m_forcedWidth = -1;
	m_forcedHeight = -1;
	m_width = -1;
	m_height = -1;
	m_currentX = 0.0f;
	m_currentY = 0.0f;
	
	m_display = HTML_INLINE;

	// background settings
	m_renderBackground = false;
	m_backgroundColor = Color32(255,255,255);
	m_pBackgroundImage = NULL;
	m_backgroundPosition = Vector2(0.0f, 0.0f);
	m_backgroundRepeat = HTML_REPEAT;
	// text settings
	m_color = Color32(0,0,0);
	//m_direction = 0;
	//m_letterSpacing;
	m_textAlign = HTML_UNDEFINED;
	m_verticalAlign = HTML_UNDEFINED;
	m_textDecoration = HTML_NONE;
	//m_textIndent;
	//m_textTransform;
	//m_wordSpacing;
	// font settings
	m_font = 0;
	m_fontSize = 12;
	m_fontStyle = 0;
	m_fontWeight = 0;
	m_lineHeight = 1.1f;
	// border settings
	m_borderBottomColor = Color32(0x0,0x0,0x0);
	m_borderBottomStyle = HTML_NONE;
	m_borderBottomWidth = 0;
	m_borderLeftColor = Color32(0x0,0x0,0x0);
	m_borderLeftStyle = HTML_NONE;
	m_borderLeftWidth = 0;
	m_borderRightColor = Color32(0x0,0x0,0x0);
	m_borderRightStyle = HTML_NONE;
	m_borderRightWidth = 0;
	m_borderTopColor = Color32(0x0,0x0,0x0);
	m_borderTopStyle = HTML_NONE;
	m_borderTopWidth = 0;
	// margins
	m_marginBottom = 0;
	m_marginLeft = 0;
	m_marginRight = 0;
	m_marginTop = 0;
	// padding
	m_paddingBottom = 0;
	m_paddingLeft = 0;
	m_paddingRight = 0;
	m_paddingTop = 0;

	m_active = false;
	// list
	//u32 m_listStyleImage;
	//u32 m_listStyleType;
	m_cellpadding = 1;
	m_cellspacing = 1;
	m_colSpan = 1;
	m_rowSpan = 1;

	m_activeColor = Color32(0,0,255);
	m_activeTextDecoration = HTML_NONE;
}

HtmlRenderState::HtmlRenderState(datResource& rsc) : 
m_backgroundColor(rsc),
m_pBackgroundImage(rsc),
m_color(rsc),
m_borderBottomColor(rsc),
m_borderLeftColor(rsc),
m_borderRightColor(rsc),
m_borderTopColor(rsc),
m_activeColor(rsc)
{
}

#if __DECLARESTRUCT
void HtmlRenderState::DeclareStruct(datTypeStruct &s)
{
	STRUCT_BEGIN(HtmlRenderState);
	STRUCT_FIELD(m_display);
	// size of current frame
	STRUCT_FIELD(m_forcedWidth);
	STRUCT_FIELD(m_forcedHeight);
	STRUCT_FIELD(m_width);
	STRUCT_FIELD(m_height);
	STRUCT_FIELD(m_cellHeight);
	// position within current frame
	STRUCT_FIELD(m_currentX);
	STRUCT_FIELD(m_currentY);

	// background settings
	STRUCT_FIELD(m_backgroundColor);
	STRUCT_FIELD(m_pBackgroundImage);
	STRUCT_FIELD(m_backgroundPosition);
	STRUCT_FIELD(m_backgroundRepeat);
	// text settings
	STRUCT_FIELD(m_color);
	//u32 m_direction;
	//u32 m_letterSpacing;
	STRUCT_FIELD(m_textAlign);
	STRUCT_FIELD(m_verticalAlign);
	STRUCT_FIELD(m_textDecoration);
	//u32 m_textIndent;
	//u32 m_textTransform;
	//u32 m_wordSpacing;
	// font settings
	STRUCT_FIELD(m_font);
	STRUCT_FIELD(m_fontSize);
	STRUCT_FIELD(m_fontStyle);
	STRUCT_FIELD(m_fontWeight);
	STRUCT_FIELD(m_lineHeight);
	// border settings
	STRUCT_FIELD(m_borderBottomColor);
	STRUCT_FIELD(m_borderBottomStyle);
	STRUCT_FIELD(m_borderBottomWidth);
	STRUCT_FIELD(m_borderLeftColor);
	STRUCT_FIELD(m_borderLeftStyle);
	STRUCT_FIELD(m_borderLeftWidth);
	STRUCT_FIELD(m_borderRightColor);
	STRUCT_FIELD(m_borderRightStyle);
	STRUCT_FIELD(m_borderRightWidth);
	STRUCT_FIELD(m_borderTopColor);
	STRUCT_FIELD(m_borderTopStyle);
	STRUCT_FIELD(m_borderTopWidth);
	// margins
	STRUCT_FIELD(m_marginBottom);
	STRUCT_FIELD(m_marginLeft);
	STRUCT_FIELD(m_marginRight);
	STRUCT_FIELD(m_marginTop);
	// padding
	STRUCT_FIELD(m_paddingBottom);
	STRUCT_FIELD(m_paddingLeft);
	STRUCT_FIELD(m_paddingRight);
	STRUCT_FIELD(m_paddingTop);
	// list
	//u32 m_listStyleImage;
	//u32 m_listStyleType;
	//size

	STRUCT_FIELD(m_cellpadding);
	STRUCT_FIELD(m_cellspacing);
	STRUCT_FIELD(m_colSpan);
	STRUCT_FIELD(m_rowSpan);

	STRUCT_FIELD(m_renderBackground);
	STRUCT_FIELD(m_active);
	STRUCT_CONTAINED_ARRAY(m_pad);

	STRUCT_FIELD(m_activeColor);
	STRUCT_FIELD(m_activeTextDecoration);

	STRUCT_END();
}
#endif


//
// name:		HtmlRenderState::Inherit
// description:	Inherit render state values
void HtmlRenderState::Inherit(const HtmlRenderState& state)
{
	m_currentX = state.m_currentX;
	m_currentY = state.m_currentY;

	// text settings
	m_color = state.m_color;
	m_activeColor = state.m_activeColor;
   	if(state.m_display == HTML_INLINE )
	{
		if(state.m_textAlign != HTML_UNDEFINED)
			m_textAlign = state.m_textAlign;
		if(state.m_verticalAlign != HTML_UNDEFINED)
			m_verticalAlign = state.m_verticalAlign;

		m_textDecoration = state.m_textDecoration;
		m_activeTextDecoration = state.m_activeTextDecoration;
	}
	//m_textIndent;
	//m_textTransform;
	//m_wordSpacing;
	// font settings
	m_font = state.m_font;
	m_fontSize = state.m_fontSize;
	m_fontStyle = state.m_fontStyle;
	m_fontWeight = state.m_fontWeight;
	m_lineHeight = state.m_lineHeight;
	// margins
	//m_marginBottom = state.m_marginBottom;
	//m_marginLeft = state.m_marginLeft;
	//m_marginRight = state.m_marginRight;
	//m_marginTop = state.m_marginTop;
	// padding
	//m_paddingBottom = state.m_paddingBottom;
	//m_paddingLeft = state.m_paddingLeft;
	//m_paddingRight = state.m_paddingRight;
	//m_paddingTop = state.m_paddingTop;
}

//
// name:		HtmlRenderState::Inherit
// description:	Inherit render state values
void HtmlRenderState::InheritTableState(const HtmlRenderState& state)
{
	m_cellpadding = state.m_cellpadding;
	m_cellspacing = state.m_cellspacing;

	m_borderBottomWidth = state.m_borderBottomWidth;
	m_borderLeftWidth = state.m_borderLeftWidth;
	m_borderRightWidth = state.m_borderRightWidth;
	m_borderTopWidth = state.m_borderTopWidth;
}
#ifndef HTML_RESOURCE
//
// name:		CHtmlRenderer::SetFontProperties
// description:	Set Font properties from this render state
void HtmlRenderState::SetFontProperties()
{
#ifndef HTML_RESOURCE
#define FONT_SCALER_VALUE (0.8f) // StuartP has changed the font so he doesnt want to redo all the HTML pages again so this scales it - basically 12pt is rendered as 10pt
	CHtmlDocument::sm_HtmlTextLayout.SetProportional(true);
	CHtmlDocument::sm_HtmlTextLayout.SetScale(Vector2((m_fontSize*FONT_SCALER_VALUE) / 32.0f, (m_fontSize*FONT_SCALER_VALUE) / 32.0f));
	CHtmlDocument::sm_HtmlTextLayout.SetLineHeightMult(m_lineHeight);
	CHtmlDocument::sm_HtmlTextLayout.SetColor(m_color);
	if(m_active)
		CHtmlDocument::sm_HtmlTextLayout.SetColor(m_activeColor);

	CHtmlDocument::sm_HtmlTextLayout.SetStyle(FONT_STYLE_STANDARD);

	CHtmlDocument::sm_HtmlTextLayout.SetOrientation(FONT_LEFT);
#endif
}
#endif
