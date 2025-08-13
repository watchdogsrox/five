//
// filename:	htmllanguage.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "system/memory.h"
// Game headers
#include "htmllanguage.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

CHtmlLanguage* CHtmlLanguage::sm_pCurrent = NULL;

// --- Code ---------------------------------------------------------------------

CHtmlLanguage::CHtmlLanguage()
{
	SetElementTag(HTML_HTML, "html");
	SetElementTag(HTML_TITLE, "title");
	SetElementTag(HTML_HEAD, "head");
	SetElementTag(HTML_A, "a");
	SetElementTag(HTML_BODY, "body");
	SetElementTag(HTML_DIV, "div");
	SetElementTag(HTML_EMBED, "embed");
	SetElementTag(HTML_HEADING1, "h1");
	SetElementTag(HTML_HEADING2, "h2");
	SetElementTag(HTML_HEADING3, "h3");
	SetElementTag(HTML_HEADING4, "h4");
	SetElementTag(HTML_HEADING5, "h5");
	SetElementTag(HTML_HEADING6, "h6");
	SetElementTag(HTML_LINK, "link");
	SetElementTag(HTML_META, "meta");
	SetElementTag(HTML_OBJECT, "object");
	SetElementTag(HTML_PARAGRAPH, "p");
	SetElementTag(HTML_SPAN, "span");
	SetElementTag(HTML_PARAM, "param");
	SetElementTag(HTML_BREAK, "br");
	SetElementTag(HTML_EMPHASIZED, "em");
	SetElementTag(HTML_STRONG, "strong");
	SetElementTag(HTML_CODE, "code");
	SetElementTag(HTML_BOLD, "b");
	SetElementTag(HTML_ITALIC, "i");
	SetElementTag(HTML_CENTER, "center");
	SetElementTag(HTML_UNORDERED_LIST, "ul");
	SetElementTag(HTML_LISTITEM, "li");
	SetElementTag(HTML_ORDERED_LIST, "ol");
	SetElementTag(HTML_SCRIPTOBJ, "scriptobj");
	SetElementTag(HTML_STYLE, "style");
	SetElementTag(HTML_TABLE, "table");
	SetElementTag(HTML_TABLEROW, "tr");
	SetElementTag(HTML_TABLEHEADER, "th");
	SetElementTag(HTML_TABLEENTRY, "td");
	SetElementTag(HTML_TEXT, "text");
	SetElementTag(HTML_IMAGE, "img");
	SetElementTag(HTML_DEFLIST, "dl");
	SetElementTag(HTML_DEFTERM, "dt");
	SetElementTag(HTML_DEFDESC, "dd");

	SetAttributeTag(HTML_ATTR_ALIGN, "align");
	SetAttributeTag(HTML_ATTR_ALINK, "alink");
	SetAttributeTag(HTML_ATTR_LINK, "link");
	SetAttributeTag(HTML_ATTR_ALT, "alt");
	SetAttributeTag(HTML_ATTR_BACKGROUND, "background");
	SetAttributeTag(HTML_ATTR_BGCOLOR, "bgcolor");
	SetAttributeTag(HTML_ATTR_BORDER, "border");
	SetAttributeTag(HTML_ATTR_BORDERCOLOR, "bordercolor");
	SetAttributeTag(HTML_ATTR_CELLSPACING, "cellspacing");
	SetAttributeTag(HTML_ATTR_CELLPADDING, "cellpadding");
	SetAttributeTag(HTML_ATTR_CLASS, "class");
	SetAttributeTag(HTML_ATTR_COLOR, "color");
	SetAttributeTag(HTML_ATTR_COLSPAN, "colspan");
	SetAttributeTag(HTML_ATTR_CONTENT, "content");
	SetAttributeTag(HTML_ATTR_HEIGHT, "height");
	SetAttributeTag(HTML_ATTR_HREF, "href");
	SetAttributeTag(HTML_ATTR_MARGINHEIGHT, "marginheight");
	SetAttributeTag(HTML_ATTR_MARGINWIDTH, "marginwidth");
	SetAttributeTag(HTML_ATTR_NAME, "name");
	SetAttributeTag(HTML_ATTR_ROWSPAN, "rowspan");
	SetAttributeTag(HTML_ATTR_SRC, "src");
	SetAttributeTag(HTML_ATTR_STYLE, "style");
	SetAttributeTag(HTML_ATTR_VALIGN, "valign");
	SetAttributeTag(HTML_ATTR_WIDTH, "width");

	SetValueTag(HTML_ALIGN_LEFT, "left");
	SetValueTag(HTML_ALIGN_RIGHT, "right");
	SetValueTag(HTML_ALIGN_CENTER, "center");
	SetValueTag(HTML_ALIGN_JUSTIFY, "justify");
	SetValueTag(HTML_ALIGN_TOP, "top");
	SetValueTag(HTML_ALIGN_BOTTOM, "bottom");
	SetValueTag(HTML_ALIGN_MIDDLE, "middle");
	SetValueTag(HTML_ALIGN_INHERIT, "inherit");
	SetValueTag(HTML_XXLARGE, "xx-large");
	SetValueTag(HTML_XLARGE, "x-large");
	SetValueTag(HTML_LARGE, "large");
	SetValueTag(HTML_MEDIUM, "medium");
	SetValueTag(HTML_SMALL, "small");
	SetValueTag(HTML_XSMALL, "x-small");
	SetValueTag(HTML_XXSMALL, "xx-small");
	SetValueTag(HTML_BLOCK, "block");
	SetValueTag(HTML_INLINE, "inline");
	SetValueTag(HTML_NONE, "none");
	SetValueTag(HTML_SOLID, "solid");
	SetValueTag(HTML_UNDERLINE, "underline");
	SetValueTag(HTML_OVERLINE, "overline");	
	SetValueTag(HTML_LINETHROUGH, "line-through");
	SetValueTag(HTML_BLINK, "blink");
	SetValueTag(HTML_REPEAT, "repeat");
	SetValueTag(HTML_NO_REPEAT, "no-repeat");
	SetValueTag(HTML_REPEAT_X, "repeat-x");
	SetValueTag(HTML_REPEAT_Y, "repeat-y");
	SetValueTag(HTML_COLLAPSE, "collapse");
	SetValueTag(HTML_SEPARATE, "separate");

}

CHtmlLanguage& CHtmlLanguage::GetCurrent()
{
	if(sm_pCurrent == NULL)
		sm_pCurrent = rage_new CHtmlLanguage;
	return *sm_pCurrent;
}

void CHtmlLanguage::SetElementTag(HtmlElementTag id, const char* pTag)
{
	atString tag(pTag);
	tag.Lowercase();

	m_elementTagMap.Insert(tag, id);
}

HtmlElementTag CHtmlLanguage::GetElementId(const char* pTag)
{
	HtmlElementTag* pId;

	sysMemStartTemp();
	{
		atString tag(pTag);
		tag.Lowercase();
		pId = m_elementTagMap.Access(tag);
	}
	sysMemEndTemp();

	if(pId)
		return *pId;
	return HTML_ELEMENT_INVALID;
}

void CHtmlLanguage::SetAttributeTag(HtmlAttributeTag id, const char* pTag)
{
	atString tag(pTag);
	tag.Lowercase();

	m_attrTagMap.Insert(tag, id);
}

HtmlAttributeTag CHtmlLanguage::GetAttributeId(const char* pTag)
{
	atString tag(pTag);
	tag.Lowercase();

	HtmlAttributeTag* pId = m_attrTagMap.Access(tag);
	if(pId)
		return *pId;
	return HTML_ATTR_INVALID;
}

void CHtmlLanguage::SetValueTag(HtmlValueTag id, const char* pTag)
{
	atString tag(pTag);
	tag.Lowercase();

	m_valueTagMap.Insert(tag, id);
}

HtmlValueTag CHtmlLanguage::GetValueId(const char* pTag)
{
	atString tag(pTag);
	tag.Lowercase();

	HtmlValueTag* pId = m_valueTagMap.Access(tag);
	if(pId)
		return *pId;
	return HTML_VALUE_INVALID;
}

Color32 CHtmlLanguage::GetColor(const char* pColour)
{
	u32 color=0xffffffff;
	// hexadecimal colour
	if(*pColour == '#')
	{
		sscanf(pColour+1, "%x", &color);
	}
	return Color32(color | 0xff000000);
}
