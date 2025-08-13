//
// filename:	css.h
// description:	Classes for storing style sheets. Initial version includes style group
//				definitions. 
//

#ifndef INC_CSS_H_
#define INC_CSS_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "atl/array.h"
#include "atl/array_struct.h"
#include "atl/map.h"
#include "data/struct.h"
#include "vector/color32.h"
#include "vector/vector2.h"


// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

class CCssStyle
{
public:
	friend class CCssStyleGroup;

	enum Type {
		INT_TYPE,
		FLOAT_TYPE,
		BOOL_TYPE,
		COLOR_TYPE,
		VECTOR2_TYPE,
		STRING_TYPE,
		UNKNOWN_TYPE
	};
	enum StyleId {
		WIDTH,
		HEIGHT,
		DISPLAY, 
		BACKGROUNDCOLOR,
		BACKGROUNDREPEAT,
		BACKGROUNDPOSITION,
		BACKGROUNDIMAGE,
		COLOR,
		TEXTALIGN,
		TEXTDECORATION,
		VERTICALALIGN,
		FONT,
		FONTSIZE,
		FONTSTYLE,
		FONTWEIGHT,
		BORDERCOLLAPSE,
		BORDERSTYLE,
		BORDERBOTTOMSTYLE,
		BORDERLEFTSTYLE,
		BORDERRIGHTSTYLE,
		BORDERTOPSTYLE,
		BORDERCOLOR,
		BORDERBOTTOMCOLOR,
		BORDERLEFTCOLOR,
		BORDERRIGHTCOLOR,
		BORDERTOPCOLOR,
		BORDERWIDTH,
		BORDERBOTTOMWIDTH,
		BORDERLEFTWIDTH,
		BORDERRIGHTWIDTH,
		BORDERTOPWIDTH,
		MARGINBOTTOM,
		MARGINLEFT,
		MARGINRIGHT,
		MARGINTOP,
		PADDINGBOTTOM,
		PADDINGLEFT,
		PADDINGRIGHT,
		PADDINGTOP,
		UNKNOWN_STYLE
	};

	CCssStyle() : m_style(UNKNOWN_STYLE), m_type(UNKNOWN_TYPE) {}
//	~CCssStyle();

	CCssStyle(StyleId, s32);
	CCssStyle(StyleId, float);
	CCssStyle(StyleId, bool);
	CCssStyle(StyleId, Color32);
	CCssStyle(StyleId, Vector2);
	CCssStyle(StyleId, const char*);
//	CCssStyle(const CCssStyle& style);

//	CCssStyle& operator=(const CCssStyle& style);

	s32 GetIntStyle();
	float GetFloatStyle();
	bool GetBoolStyle();
	Color32 GetColorStyle();
	Vector2 GetVector2Style();
	const char* GetStringStyle();

	StyleId GetStyleId() const { return m_style;}

#if __DECLARESTRUCT
	void DeclareStruct(datTypeStruct &s);
#endif // !__FINAL

	static void InitTags();
	static StyleId GetStyleId(const char* pTag);
private:
	StyleId m_style;
	char m_data[8];
	Type m_type;

	static void SetStyleTag(StyleId id, const char* pTag);
	static atMap<atString, StyleId> m_styleTagMap;
};

//
// name:		CCssStyleGroup
// description:	Collection of styles
class CCssStyleGroup
{
public:
	CCssStyleGroup() {}
	CCssStyleGroup(datResource& rsc) : m_styleArray(rsc) {}

	IMPLEMENT_PLACE_INLINE(CCssStyleGroup);

#if __DECLARESTRUCT
	void DeclareStruct(datTypeStruct &s);
#endif // !__FINAL

	void AddStyle(const CCssStyle& style);
	void Apply(struct HtmlRenderState& rs);
	void ApplyHoverPseudoClass(struct HtmlRenderState& rs);

	void ReadFile(const char* pFilename);
private:
	atArray<CCssStyle> m_styleArray;
};

class CCssStyleClass
{
public:
	CCssStyleClass() {}
	CCssStyleClass(datResource& rsc);

	IMPLEMENT_PLACE_INLINE(CCssStyleClass);

#if __DECLARESTRUCT
	void DeclareStruct(datTypeStruct &s);
#endif // !__FINAL

	void Shutdown();

	CCssStyleGroup* AddStyleGroup(s32 tag);
	CCssStyleGroup* GetStyleGroup(s32 tag);
	CCssStyleGroup* AddDefaultStyleGroup();
	CCssStyleGroup* GetDefaultStyleGroup();

	//void ReadFile(const char* pFilename);

private:
	atMap<s32, CCssStyleGroup> m_styleMap;

};

//
// name:		CCssStyleSheet
// description:	Collection of style groups accessible through ids
class CCssStyleSheet
{
public:
	CCssStyleSheet() {}
	CCssStyleSheet(datResource& rsc);

	IMPLEMENT_PLACE_INLINE(CCssStyleSheet);

#if __DECLARESTRUCT
	void DeclareStruct(datTypeStruct &s);
#endif // !__FINAL

	void Init();
	void Shutdown();

	CCssStyleClass* AddStyleClass(const char* pName);
	CCssStyleClass* GetStyleClass(const char* pName);
	CCssStyleClass* GetDefaultStyleClass() {return GetStyleClass((u32)0);}

	void ReadFile(const char* pFilename);

private:
	CCssStyleClass* AddStyleClass(u32 hash);
	CCssStyleClass* GetStyleClass(u32 hash);

	atMap<u32, CCssStyleClass> m_styleClassMap;

};

// --- Inline functions ------------------------------------------------------------------

// --- CCssStyleClass -------------------------------------------------------------------

inline void CCssStyleClass::Shutdown()
{
	m_styleMap.Kill();
}

inline CCssStyleGroup* CCssStyleClass::AddStyleGroup(s32 tag)
{
	Assertf(GetStyleGroup(tag) == NULL, "Style group already added");
	m_styleMap.Insert(tag, CCssStyleGroup());
	return &(m_styleMap[tag]);
}

inline CCssStyleGroup* CCssStyleClass::GetStyleGroup(s32 tag)
{
	return m_styleMap.Access(tag);
}


// --- CCssStyleSheet -------------------------------------------------------------------

inline void CCssStyleSheet::Init()
{
	// Add default style class
	AddStyleClass((u32)0);
}

inline void CCssStyleSheet::Shutdown()
{
	m_styleClassMap.Kill();
}

#endif // !INC_CSS_H_
