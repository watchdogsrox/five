//
// filename:	css.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "atl/map_struct.h"
#include "file/asset.h"
#include "system/memory.h"
// Game headers
#include "css.h"
#include "HtmlLanguage.h"
#include "HtmlDocument.h"
#include "HtmlRenderState.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

#define PUSHBACK
// --- Structure/Class Definitions ----------------------------------------------

class CCssTokenizer
{
	enum {PUSHBACK_BUFFER_SIZE=256};
public:
	CCssTokenizer() {}

	void Init(fiStream* pStream);

	s32 GetToken(char* pToken, u32 len);
	void SetDelimiters(const char* pDelimiter) {m_pDelimiter = pDelimiter;}

	void PushBack(const char* pStr, u32 len);
	void PushBackChar(char c);
	void SkipWhitespace();
	char PeekChar();
	char GetChar();

private:
	bool IsWhitespace(char c);
	bool IsDelimiter(char c);

	fiStream* m_pStream;
	const char* m_pDelimiter;

	char m_pushbackBuffer[PUSHBACK_BUFFER_SIZE];
	s32 m_pushbackPosn;
};

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

atMap<atString, CCssStyle::StyleId> CCssStyle::m_styleTagMap;

// --- Code ---------------------------------------------------------------------

// --- CCssTokenizer ------------------------------------------------------------

void CCssTokenizer::Init(fiStream* pStream)
{
	m_pStream = pStream;
	m_pushbackPosn = 0;
	m_pDelimiter = NULL;
}

bool CCssTokenizer::IsWhitespace(char c)
{
	if(c == ' ' || c == '\t' || c == '\n')
		return true;
	return false;
}

bool CCssTokenizer::IsDelimiter(char c)
{
	if(c == -1 || (m_pDelimiter && strchr(m_pDelimiter, c) != NULL))
		return true;
	return false;
}

char CCssTokenizer::GetChar()
{
	if(m_pushbackPosn > 0)
	{
		m_pushbackPosn--;
		return m_pushbackBuffer[m_pushbackPosn];
	}
	return (char)m_pStream->FastGetCh();
}

char CCssTokenizer::PeekChar()
{
	char c = GetChar();
	PushBackChar(c);
	return c;
}

void CCssTokenizer::SkipWhitespace()
{
	char c;
	do {
		c = GetChar();
	} while(IsWhitespace(c));
	PushBackChar(c);
}

s32 CCssTokenizer::GetToken(char *pToken, rage::u32 len)
{
	SkipWhitespace();

	char c = GetChar();
	u32 len2 = 0;

	// leave space for '/0' at end 
	len--;
	while(!IsDelimiter(c) && len2 < len)
	{
		*pToken++ = c;
		c = GetChar();
		len2++;
	}
	*pToken++ = '\0';

	return len2;
}

void CCssTokenizer::PushBack(const char* pStr, u32 len)
{
	s32 posn = m_pushbackPosn + len;

	m_pushbackPosn = posn;
	Assert(posn <= PUSHBACK_BUFFER_SIZE);
	while(len--)
	{
		posn--;
		m_pushbackBuffer[posn] = *pStr;
		pStr++;
	}
}

void CCssTokenizer::PushBackChar(char c)
{
	Assert(m_pushbackPosn < PUSHBACK_BUFFER_SIZE);
	m_pushbackBuffer[m_pushbackPosn] = c;
	m_pushbackPosn++;
}


// --- CCssStyle ----------------------------------------------------------------

void CCssStyle::InitTags()
{
	if(m_styleTagMap.GetNumUsed() > 0)
		return;

	SetStyleTag(WIDTH, "width");
	SetStyleTag(HEIGHT, "height");
	SetStyleTag(DISPLAY, "display");
	SetStyleTag(BACKGROUNDCOLOR, "background-color");
	SetStyleTag(BACKGROUNDREPEAT, "background-repeat");
	SetStyleTag(BACKGROUNDPOSITION, "background-position");
	SetStyleTag(BACKGROUNDIMAGE, "background-image");
	SetStyleTag(COLOR, "color");
	SetStyleTag(TEXTALIGN, "text-align");
	SetStyleTag(TEXTDECORATION, "text-decoration");
	SetStyleTag(VERTICALALIGN, "vertical-align");
	SetStyleTag(FONT, "font");
	SetStyleTag(FONTSIZE, "font-size");
	SetStyleTag(FONTSTYLE, "font-style");
	SetStyleTag(FONTWEIGHT, "font-weight");
	SetStyleTag(BORDERCOLLAPSE, "border-collapse");
	SetStyleTag(BORDERSTYLE, "border-style");
	SetStyleTag(BORDERBOTTOMSTYLE, "border-bottom-style");
	SetStyleTag(BORDERLEFTSTYLE, "border-left-style");
	SetStyleTag(BORDERRIGHTSTYLE, "border-right-style");
	SetStyleTag(BORDERTOPSTYLE, "border-top-style");
	SetStyleTag(BORDERCOLOR, "border-color");
	SetStyleTag(BORDERBOTTOMCOLOR, "border-bottom-color");
	SetStyleTag(BORDERLEFTCOLOR, "border-left-color");
	SetStyleTag(BORDERRIGHTCOLOR, "border-right-color");
	SetStyleTag(BORDERTOPCOLOR, "border-top-color");
	SetStyleTag(BORDERWIDTH, "border-width");
	SetStyleTag(BORDERBOTTOMWIDTH, "border-bottom-width");
	SetStyleTag(BORDERLEFTWIDTH, "border-left-width");
	SetStyleTag(BORDERRIGHTWIDTH, "border-right-width");
	SetStyleTag(BORDERTOPWIDTH, "border-top-width");
	SetStyleTag(MARGINBOTTOM, "margin-bottom");
	SetStyleTag(MARGINLEFT, "margin-left");
	SetStyleTag(MARGINRIGHT, "margin-right");
	SetStyleTag(MARGINTOP, "margin-top");
	SetStyleTag(PADDINGBOTTOM, "padding-bottom");
	SetStyleTag(PADDINGLEFT, "padding-left");
	SetStyleTag(PADDINGRIGHT, "padding-right");
	SetStyleTag(PADDINGTOP, "padding-top");
}

void CCssStyle::SetStyleTag(CCssStyle::StyleId id, const char* pTag)
{
	atString tag(pTag);
	tag.Lowercase();

	m_styleTagMap.Insert(tag, id);
}

CCssStyle::StyleId CCssStyle::GetStyleId(const char* pTag)
{
	atString tag(pTag);
	tag.Lowercase();

	StyleId* pId = m_styleTagMap.Access(tag);
	if(pId)
		return *pId;
	return UNKNOWN_STYLE;
}

//
// name:		CCssStyle::CCssStyle
// description:	Constructors
CCssStyle::CCssStyle(StyleId style, s32 value)
{
	m_style = style;
	*(s32*)m_data = value;
	m_type = INT_TYPE;
}
CCssStyle::CCssStyle(StyleId style, float value)
{
	m_style = style;
	*(float*)m_data = value;
	m_type = FLOAT_TYPE;
}
CCssStyle::CCssStyle(StyleId style, bool value)
{
	m_style = style;
	*(bool*)m_data = value;
	m_type = BOOL_TYPE;
}
CCssStyle::CCssStyle(StyleId style, Color32 value)
{
	m_style = style;
	*(Color32*)m_data = value;
	m_type = COLOR_TYPE;
}
CCssStyle::CCssStyle(StyleId style, Vector2 value)
{
	m_style = style;
	*(Vector2*)m_data = value;
	m_type = VECTOR2_TYPE;
}
CCssStyle::CCssStyle(StyleId style, const char* value)
{
//	sysMemEndTemp();
	m_style = style;
	*(const char**)m_data = value;//rage_new char[strlen(value)+1];
//	strcpy(*(char**)m_data, value);
	m_type = STRING_TYPE;
//	sysMemStartTemp();
}

/*CCssStyle::CCssStyle(const CCssStyle& style)
{
	*this = style;
}

CCssStyle::~CCssStyle()
{
	sysMemEndTemp();
	if(m_type == STRING_TYPE)
		delete[] *(char**)m_data;
	sysMemStartTemp();
}

CCssStyle& CCssStyle::operator=(const CCssStyle& style)
{
	m_style = style.m_style;
	m_type = style.m_type;
	if(m_type != STRING_TYPE)
	{
		*(s32*)m_data = *(s32*)style.m_data;
		*(s32*)&m_data[4] = *(s32*)&style.m_data[4];
	}
	else
	{
		sysMemEndTemp();
		*(char**)m_data = rage_new char[strlen(*(char**)style.m_data)+1];
		strcpy(*(char**)m_data, *(char**)style.m_data);
		sysMemStartTemp();
	}
	return *this;
}*/

s32 CCssStyle::GetIntStyle()
{
	// convert if necessary
	switch(m_type)
	{
	case FLOAT_TYPE:
		m_type = INT_TYPE;
		*(s32*)m_data = (s32)(*(float*)m_data);
		break;
	default:
		break;
	}
	Assert(m_type == INT_TYPE);
	return *(s32*)m_data;
}
float CCssStyle::GetFloatStyle()
{
	// convert if necessary
	switch(m_type)
	{
	case INT_TYPE:
		m_type = FLOAT_TYPE;
		*(float*)m_data = (float)(*(s32*)m_data);
		break;
	default:
		break;
	}
	Assert(m_type == FLOAT_TYPE);
	return *(float*)m_data;
}
bool CCssStyle::GetBoolStyle()
{
	Assert(m_type == BOOL_TYPE);
	return *(bool*)m_data;
}
Color32 CCssStyle::GetColorStyle()
{
	Assert(m_type == COLOR_TYPE);
	return *(Color32*)m_data;
}
Vector2 CCssStyle::GetVector2Style()
{
	Assert(m_type == VECTOR2_TYPE);
	return *(Vector2*)m_data;
}
const char* CCssStyle::GetStringStyle()
{
	Assert(m_type == STRING_TYPE);
	return *(const char**)m_data;
}

//
// name:		CCssStyleGroup::AddStyle
// description:	Add style to style group
void CCssStyleGroup::AddStyle(const CCssStyle& style)
{
	for(s32 i=0; i<m_styleArray.GetCount(); i++)
	{
		// if style is already in list then set it and return
		if(style.GetStyleId() == m_styleArray[i].GetStyleId())
		{
			m_styleArray[i] = style;
			return;
		}
	}
	m_styleArray.PushAndGrow(style);
}

//
// name:		CCssStyleGroup::Apply
// description:	Apply style group to render state
void CCssStyleGroup::Apply(struct HtmlRenderState& rs)
{
	for(s32 i=0; i<m_styleArray.GetCount(); i++)
	{
		CCssStyle& style = m_styleArray[i];
		switch(style.m_style)
		{
		case CCssStyle::WIDTH:
			rs.m_forcedWidth = style.GetFloatStyle();
			break;

		case CCssStyle::HEIGHT:
			rs.m_forcedHeight = style.GetFloatStyle();
			break;

		case CCssStyle::DISPLAY:
			rs.m_display = style.GetIntStyle();
			break;

		case CCssStyle::BACKGROUNDCOLOR:
			rs.m_backgroundColor = style.GetColorStyle();
			rs.m_renderBackground = true;
			break;

		case CCssStyle::COLOR:
			rs.m_color = style.GetColorStyle();
			break;

		case CCssStyle::FONTSIZE:
			rs.m_fontSize = style.GetIntStyle();
			break;
		case CCssStyle::FONTSTYLE:
			rs.m_fontStyle = style.GetIntStyle();
			break;
		case CCssStyle::FONTWEIGHT:
			rs.m_fontWeight = style.GetIntStyle();
			break;

		case CCssStyle::BACKGROUNDIMAGE:
			rs.m_pBackgroundImage = CHtmlDocument::GetCurrentDocument()->LoadTexture(style.GetStringStyle());
			break;

		case CCssStyle::BORDERCOLLAPSE:
			if(style.GetIntStyle() == HTML_COLLAPSE)
			{
				rs.m_cellpadding = 0;
				rs.m_cellspacing = 0;
			}
		case CCssStyle::BORDERSTYLE:
			rs.m_borderBottomStyle = style.GetIntStyle();
			rs.m_borderLeftStyle = style.GetIntStyle();
			rs.m_borderRightStyle = style.GetIntStyle();
			rs.m_borderTopStyle = style.GetIntStyle();
			break;
		case CCssStyle::BORDERBOTTOMSTYLE:
			rs.m_borderBottomStyle = style.GetIntStyle();
			break;
		case CCssStyle::BORDERLEFTSTYLE:
			rs.m_borderLeftStyle = style.GetIntStyle();
			break;
		case CCssStyle::BORDERRIGHTSTYLE:
			rs.m_borderRightStyle = style.GetIntStyle();
			break;
		case CCssStyle::BORDERTOPSTYLE:
			rs.m_borderTopStyle = style.GetIntStyle();
			break;
		case CCssStyle::BORDERCOLOR:
			rs.m_borderBottomColor = style.GetColorStyle();
			rs.m_borderLeftColor = style.GetColorStyle();
			rs.m_borderRightColor = style.GetColorStyle();
			rs.m_borderTopColor = style.GetColorStyle();
			break;
		case CCssStyle::BORDERBOTTOMCOLOR:
			rs.m_borderBottomColor = style.GetColorStyle();
			break;
		case CCssStyle::BORDERLEFTCOLOR:
			rs.m_borderLeftColor = style.GetColorStyle();
			break;
		case CCssStyle::BORDERRIGHTCOLOR:
			rs.m_borderRightColor = style.GetColorStyle();
			break;
		case CCssStyle::BORDERTOPCOLOR:
			rs.m_borderTopColor = style.GetColorStyle();
			break;
		case CCssStyle::BORDERWIDTH:
			rs.m_borderBottomWidth = style.GetFloatStyle();
			rs.m_borderLeftWidth = style.GetFloatStyle();
			rs.m_borderRightWidth = style.GetFloatStyle();
			rs.m_borderTopWidth = style.GetFloatStyle();
			break;
		case CCssStyle::BORDERBOTTOMWIDTH:
			rs.m_borderBottomWidth = style.GetFloatStyle();
			break;
		case CCssStyle::BORDERLEFTWIDTH:
			rs.m_borderLeftWidth = style.GetFloatStyle();
			break;
		case CCssStyle::BORDERRIGHTWIDTH:
			rs.m_borderRightWidth = style.GetFloatStyle();
			break;
		case CCssStyle::BORDERTOPWIDTH:
			rs.m_borderTopWidth = style.GetFloatStyle();
			break;

		case CCssStyle::MARGINBOTTOM:
			rs.m_marginBottom = style.GetFloatStyle();
			break;
		case CCssStyle::MARGINLEFT:
			rs.m_marginLeft = style.GetFloatStyle();
			break;
		case CCssStyle::MARGINRIGHT:
			rs.m_marginRight = style.GetFloatStyle();
			break;
		case CCssStyle::MARGINTOP:
			rs.m_marginTop = style.GetFloatStyle();
			break;

		case CCssStyle::PADDINGBOTTOM:
			rs.m_paddingBottom = style.GetFloatStyle();
			break;
		case CCssStyle::PADDINGLEFT:
			rs.m_paddingLeft = style.GetFloatStyle();
			break;
		case CCssStyle::PADDINGRIGHT:
			rs.m_paddingRight = style.GetFloatStyle();
			break;
		case CCssStyle::PADDINGTOP:
			rs.m_paddingTop = style.GetFloatStyle();
			break;

		case CCssStyle::TEXTALIGN:
			rs.m_textAlign = style.GetIntStyle();
			break;
		case CCssStyle::TEXTDECORATION:
			rs.m_textDecoration = style.GetIntStyle();
			break;
		case CCssStyle::VERTICALALIGN:
			rs.m_verticalAlign = style.GetIntStyle();
			break;

		default:
			break;
		}
	}
}

//
// name:		CCssStyleGroup::ApplyHoverPseudoClass
// description:	Apply style group to render state
void CCssStyleGroup::ApplyHoverPseudoClass(struct HtmlRenderState& rs)
{
	for(s32 i=0; i<m_styleArray.GetCount(); i++)
	{
		CCssStyle& style = m_styleArray[i];
		switch(style.m_style)
		{
		case CCssStyle::COLOR:
			rs.m_activeColor = style.GetColorStyle();
			break;

		case CCssStyle::TEXTDECORATION:
			rs.m_activeTextDecoration = style.GetIntStyle();
			break;

		default:
			break;
		}
	}
}

void ProcessStyle(CCssStyle::StyleId id, const char* pBuffer, CCssStyle& style)
{
	s32 iValue;
	//	float fValue;
	//	bool bValue;
	Color32 cValue;
	Vector2 vValue;
	char* sValue;

	switch(id)
	{
	case CCssStyle::COLOR:
	case CCssStyle::BACKGROUNDCOLOR:
	case CCssStyle::BORDERCOLOR:
	case CCssStyle::BORDERBOTTOMCOLOR:
	case CCssStyle::BORDERLEFTCOLOR:
	case CCssStyle::BORDERRIGHTCOLOR:
	case CCssStyle::BORDERTOPCOLOR:
		cValue = CHtmlLanguage::GetCurrent().GetColor(pBuffer);
		style = CCssStyle(id, cValue);
		break;

	case CCssStyle::WIDTH:
	case CCssStyle::HEIGHT:
	case CCssStyle::FONTSIZE:
	case CCssStyle::BORDERWIDTH:
	case CCssStyle::BORDERBOTTOMWIDTH:
	case CCssStyle::BORDERLEFTWIDTH:
	case CCssStyle::BORDERRIGHTWIDTH:
	case CCssStyle::BORDERTOPWIDTH:
	case CCssStyle::MARGINBOTTOM:
	case CCssStyle::MARGINLEFT:
	case CCssStyle::MARGINRIGHT:
	case CCssStyle::MARGINTOP:
	case CCssStyle::PADDINGBOTTOM:
	case CCssStyle::PADDINGLEFT:
	case CCssStyle::PADDINGRIGHT:
	case CCssStyle::PADDINGTOP:
		iValue = atoi(pBuffer);
		style = CCssStyle(id, iValue);
		break;

	case CCssStyle::TEXTALIGN:
	case CCssStyle::TEXTDECORATION:
	case CCssStyle::VERTICALALIGN:
	case CCssStyle::BACKGROUNDREPEAT:
	case CCssStyle::BORDERCOLLAPSE:
		iValue = CHtmlLanguage::GetCurrent().GetValueId(pBuffer);
		style = CCssStyle(id, iValue);
		break;

	case CCssStyle::BACKGROUNDPOSITION:
		{
			sscanf(pBuffer, "%f %f", &vValue.x, &vValue.y);
			style = CCssStyle(id, vValue);
		}
		break;

	case CCssStyle::BACKGROUNDIMAGE:
		sysMemEndTemp();
		sValue = rage_new char[strlen(pBuffer)+1];
		strcpy(sValue, pBuffer);
		style = CCssStyle(id, pBuffer);
		sysMemStartTemp();
		break;

	default:
		Warningf("Unsupported css tag %s\n", pBuffer);
		break;
	}
}

void CCssStyleGroup::ReadFile(const char* pFilename)
{
	fiStream *S = ASSET.Open(pFilename, "");
	CCssTokenizer T;
	char buf[128];

	CCssStyle::InitTags();

	T.Init(S);
	T.SetDelimiters(":");

	while(T.GetToken(buf, sizeof(buf)))
	{
		T.SetDelimiters(";\n");
		CCssStyle::StyleId styleId = CCssStyle::GetStyleId(buf);
		CCssStyle style;
		T.GetToken(buf, sizeof(buf));

		ProcessStyle(styleId, buf, style);
		if(style.GetStyleId() != CCssStyle::UNKNOWN_STYLE)
		{
			AddStyle(style);
		}
		T.SetDelimiters(":\n");
	}

}


#if __DECLARESTRUCT
namespace rage {
	datSwapper_ENUM(CCssStyle::StyleId)
	datSwapper_ENUM(CCssStyle::Type)
}
void CCssStyle::DeclareStruct(datTypeStruct &s)
{
	STRUCT_BEGIN(CCssStyle);
	STRUCT_FIELD(m_style);
	switch(m_type)
	{
	case INT_TYPE:
	case FLOAT_TYPE:
	case COLOR_TYPE:
	case VECTOR2_TYPE:
		STRUCT_FIELD(*(s32*)m_data);
		STRUCT_FIELD(*(((s32*)m_data)+1));
		break;
	default:
		STRUCT_CONTAINED_ARRAY(m_data);
		break;
	}
	STRUCT_FIELD(m_type);
	STRUCT_END();

}
void CCssStyleGroup::DeclareStruct(datTypeStruct &s)
{
	STRUCT_BEGIN(CCssStyleGroup);
	STRUCT_FIELD(m_styleArray);
	STRUCT_END();
}
void CCssStyleClass::DeclareStruct(datTypeStruct &s)
{
	STRUCT_BEGIN(CCssStyleClass);
	STRUCT_FIELD(m_styleMap);
	STRUCT_END();

}
void CCssStyleSheet::DeclareStruct(datTypeStruct &s)
{
	STRUCT_BEGIN(CCssStyleSheet);
	STRUCT_FIELD(m_styleClassMap);
	STRUCT_END();

}

#endif

//
// name:		CCssStyleClass::CCssStyleClass
// description:	Style class constructor
CCssStyleClass::CCssStyleClass(datResource& rsc) : m_styleMap(rsc) 
{
	atMap<s32, CCssStyleGroup>::Iterator entry = m_styleMap.CreateIterator();
	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		CCssStyleGroup::Place(&entry.GetData(),rsc);
	}
}

// use HTML_ELEMENT_INVALID as an id for the default style group
CCssStyleGroup* CCssStyleClass::AddDefaultStyleGroup() 
{
	return AddStyleGroup(HTML_ELEMENT_INVALID);
}
CCssStyleGroup* CCssStyleClass::GetDefaultStyleGroup() 
{
	return GetStyleGroup(HTML_ELEMENT_INVALID);
}

//
// name:		CCssStyleSheet::CCssStyleSheet
// description:	Style sheet constructor
CCssStyleSheet::CCssStyleSheet(datResource& rsc) : m_styleClassMap(rsc) 
{
	atMap<u32, CCssStyleClass>::Iterator entry = m_styleClassMap.CreateIterator();
	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		CCssStyleClass::Place(&entry.GetData(),rsc);
	}
}

CCssStyleClass* CCssStyleSheet::AddStyleClass(const char* pName)
{
	return AddStyleClass(atStringHash(pName));
}

CCssStyleClass* CCssStyleSheet::GetStyleClass(const char* pName)
{
	return GetStyleClass(atStringHash(pName));
}

CCssStyleClass* CCssStyleSheet::AddStyleClass(u32 hash)
{
	Assertf(GetStyleClass(hash) == NULL, "Style class already added");
	m_styleClassMap.Insert(hash, CCssStyleClass());
	return &(m_styleClassMap[hash]);
}

CCssStyleClass* CCssStyleSheet::GetStyleClass(u32 hash)
{
	return m_styleClassMap.Access(hash);
}


//
// name:		CCssStyleSheet::ReadFile
// description:	
void CCssStyleSheet::ReadFile(const char* pFilename)
{
	sysMemStartTemp();
	{
		fiStream *S = ASSET.Open(pFilename, "");
		CCssTokenizer T;
		char buf[128];
		atArray<CCssStyleGroup*> m_styleGroups;
		bool readingStyles = false;

		CCssStyle::InitTags();

		T.Init(S);
		T.SetDelimiters(", \t");

		while(T.GetToken(buf, sizeof(buf)))
		{
			if(!readingStyles)
			{
				CCssStyleClass* pClass = GetDefaultStyleClass();
				CCssStyleGroup* pGroup;
				// parse buffer for element id, class name, pseudo class
				char* pDot = strchr(buf, '.');
				char* pColon = strchr(buf, ':');
				if(pDot)
					*pDot = '\0';

				// if colon onwards is ":link" then ignore
				if(pColon && !strcmp(pColon, ":link"))
				{
					*pColon = '\0';
					pColon = NULL;
				}

				sysMemEndTemp();
				// pDot now points to class name
				if(pDot)
				{
					pDot++;
					pClass = GetStyleClass(pDot);
					if(pClass == NULL)
						pClass = AddStyleClass(pDot);
				}
				else if(pColon)
				{
					pClass = GetStyleClass(pColon);
					if(pClass == NULL)
						pClass = AddStyleClass(pColon);
					*pColon = '\0';
				}
				// if element name is null string then get the default style group for the class
				if(buf[0] == '\0')
				{
					pGroup = pClass->GetDefaultStyleGroup();
					if(pGroup == NULL)
						pGroup = pClass->AddDefaultStyleGroup();
				}
				else
				{
					// add to the list of style groups
					HtmlElementTag element = CHtmlLanguage::GetCurrent().GetElementId(buf);
					Assert(element != HTML_ELEMENT_INVALID);
					pGroup = pClass->GetStyleGroup(element);
					if(pGroup == NULL)
						pGroup = pClass->AddStyleGroup(element);
				}
				sysMemStartTemp();

				m_styleGroups.PushAndGrow(pGroup);

				// Is next char an open curly bracket
				T.SkipWhitespace();
				if(T.PeekChar() == '{')
				{
					T.GetChar();
					readingStyles = true;
					// Set Colon as terminator
					T.SetDelimiters(":");
				}
			}
			else
			{
				if(buf[0] == '}')
				{
					readingStyles = false;
					m_styleGroups.Reset();
					// , and whitespace as terminators for
					T.PushBack(":", 1);
					T.PushBack(&buf[1], strlen(&buf[1]));
					T.SetDelimiters(", \t");
					continue;
				}

				T.SetDelimiters(";\n");

				// Add styles to all the style groups in the array
				CCssStyle::StyleId styleId = CCssStyle::GetStyleId(buf);
				CCssStyle style;
				T.GetToken(buf, sizeof(buf));

				ProcessStyle(styleId, buf, style);
				if(style.GetStyleId() != CCssStyle::UNKNOWN_STYLE)
				{
					for(s32 i=0; i<m_styleGroups.GetCount(); i++)
					{
						sysMemEndTemp();
						m_styleGroups[i]->AddStyle(style);
						sysMemStartTemp();
					}
				}
				// Set Colon as terminator
				T.SetDelimiters(":");
			}
		}
		S->Close();
	}

	sysMemEndTemp();
}


