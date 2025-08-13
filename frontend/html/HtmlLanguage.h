//
// filename:	HtmlLanguage.h
// description:	
//

#ifndef INC_HTMLLANGUAGE_H_
#define INC_HTMLLANGUAGE_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "atl/map.h"
#include "atl/string.h"
#include "string/stringhash.h"
#include "vector/color32.h"
// Game headers


// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

enum HtmlElementTag {
	HTML_HTML,
	HTML_TITLE,
	HTML_A,
	HTML_BODY,
	HTML_BOLD,
	HTML_BREAK,
	HTML_CENTER,
	HTML_CODE,
	HTML_DEFLIST,
	HTML_DEFTERM,
	HTML_DEFDESC,
	HTML_DIV,
	HTML_EMBED,
	HTML_EMPHASIZED,
	HTML_HEAD,
	HTML_HEADING1,
	HTML_HEADING2,
	HTML_HEADING3,
	HTML_HEADING4,
	HTML_HEADING5,
	HTML_HEADING6,
	HTML_IMAGE,
	HTML_ITALIC,
	HTML_LINK,
	HTML_LISTITEM,
	HTML_META,
	HTML_OBJECT,
	HTML_ORDERED_LIST,
	HTML_PARAGRAPH,
	HTML_PARAM,
	HTML_SPAN,
	HTML_STRONG,
	HTML_STYLE,
	HTML_TABLE,
	HTML_TABLEROW,
	HTML_TABLEHEADER,
	HTML_TABLEENTRY,
	HTML_UNORDERED_LIST,
	// custom tags 
	HTML_TEXT,
	HTML_SCRIPTOBJ,
	HTML_ELEMENT_INVALID
};

enum HtmlAttributeTag {
	HTML_ATTR_ALIGN,
	HTML_ATTR_LINK,
	HTML_ATTR_ALINK,
	HTML_ATTR_ALT,
	HTML_ATTR_BACKGROUND,
	HTML_ATTR_BGCOLOR,
	HTML_ATTR_BORDER,
	HTML_ATTR_BORDERCOLOR,
	HTML_ATTR_CELLSPACING,
	HTML_ATTR_CELLPADDING,
	HTML_ATTR_CLASS,
	HTML_ATTR_COLOR,
	HTML_ATTR_COLSPAN,
	HTML_ATTR_CONTENT,
	HTML_ATTR_HEIGHT,
	HTML_ATTR_HREF,
	HTML_ATTR_MARGINHEIGHT,
	HTML_ATTR_MARGINWIDTH,
	HTML_ATTR_NAME,
	HTML_ATTR_ROWSPAN,
	HTML_ATTR_SRC,
	HTML_ATTR_STYLE,
	HTML_ATTR_VALIGN,
	HTML_ATTR_WIDTH,
	HTML_ATTR_INVALID
};

enum HtmlValueTag {
	HTML_UNDEFINED=-1,
	// alignment
	HTML_ALIGN_LEFT,
	HTML_ALIGN_RIGHT,
	HTML_ALIGN_CENTER,
	HTML_ALIGN_JUSTIFY,
	HTML_ALIGN_TOP,
	HTML_ALIGN_BOTTOM,
	HTML_ALIGN_MIDDLE,
	HTML_ALIGN_INHERIT,

	// font size
	HTML_XXSMALL,
	HTML_XSMALL,
	HTML_SMALL,
	HTML_MEDIUM,
	HTML_LARGE,
	HTML_XLARGE,
	HTML_XXLARGE,

	// display
	HTML_BLOCK,
	HTML_TABLESECTION,
	HTML_TABLECELL,
	HTML_INLINE,

	// border style
	HTML_NONE,
	HTML_SOLID,
	
	// text decoration
	HTML_UNDERLINE,
	HTML_OVERLINE,
	HTML_LINETHROUGH,
	HTML_BLINK,

	// repeat
	HTML_REPEAT,
	HTML_NO_REPEAT,
	HTML_REPEAT_X,
	HTML_REPEAT_Y,

	// border collapse
	HTML_COLLAPSE,
	HTML_SEPARATE,

	// invalid
	HTML_VALUE_INVALID

};

// --- Structure/Class Definitions ----------------------------------------------


class CHtmlLanguage
{
public:
	CHtmlLanguage();

	static CHtmlLanguage& GetCurrent();

	HtmlElementTag GetElementId(const char* pTag);
	HtmlAttributeTag GetAttributeId(const char* pTag);
	HtmlValueTag GetValueId(const char* pTag);

	Color32 GetColor(const char* pColour);
private:
	void SetElementTag(HtmlElementTag id, const char* pTag);
	void SetAttributeTag(HtmlAttributeTag id, const char* pTag);
	void SetValueTag(HtmlValueTag id, const char* pTag);

	atMap<atString, HtmlElementTag> m_elementTagMap;
	atMap<atString, HtmlAttributeTag> m_attrTagMap;
	atMap<atString, HtmlValueTag> m_valueTagMap;

	static CHtmlLanguage* sm_pCurrent;
};

// --- Globals ------------------------------------------------------------------


#endif // !INC_HTMLLANGUAGE_H_
