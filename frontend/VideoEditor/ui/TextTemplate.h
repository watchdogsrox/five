/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : TextTemplate.h
// PURPOSE : header for TextTemplate.cpp
// AUTHOR  : Derek Payne
// STARTED : 17/06/2015
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef TEXT_TEMPLATE_H
#define TEXT_TEMPLATE_H

#include "control/replay/ReplaySettings.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

#include "atl/hashstring.h"
#include "atl/string.h"
#include "vector/vector2.h"

#include "frontend/hud_colour.h"
#include "renderer/color.h"
#include "replaycoordinator/storage/MontageText.h"

#include "parser/macros.h"

#define USE_TEXT_CANVAS (1)  // use new canvas scaleform movie to render the text and set it up as templates
#define USE_CODE_TEXT	(0)  // use old code-rendered text

#define DEFAULT_TEXT_DURATION_MS (3000)

struct sEditedTextProperties
{
	atHashString		m_template;
	s32					m_positionType;
	Vector2				m_position;
	float				m_scale;
	u32					m_durationMs;
	s32					m_style;
	eHUD_COLOURS		m_hudColor;
	float				m_alpha;
	CRGBA				m_colorRGBA;
	char				m_text[CMontageText::MAX_MONTAGE_TEXT_BYTES];
};


class CTemplateData
{
public:

	atHashString propertyId;
	int index;
	int actionScriptEnum;
	atHashString dataId;
	int defaultValue;
	bool visibleInMenu;

	PAR_SIMPLE_PARSABLE;
};


class CTextTemplateItem
{
public:

	atHashString templateId;
	atHashString textId;
	s32 actionScriptEnum;

	atArray<CTemplateData> data;

	PAR_SIMPLE_PARSABLE;
};


class CPropertyData
{
public:

	s32 valueNumber;
	atHashString valueHash;
	atString valueString;
	atHashString textId;

	PAR_SIMPLE_PARSABLE;
};

class CPropertyItem
{
public:
	atHashString propertyId;

	atArray<CPropertyData> data;

	PAR_SIMPLE_PARSABLE;
};


class CTextTemplateXML
{
public:
	atArray<CTextTemplateItem> CTemplate;
	atArray<CPropertyItem> CProperties;

	PAR_SIMPLE_PARSABLE;
};

class CTextTemplate
{
public:
	static void Init();
	static void Shutdown();
    static void CoreShutdown( unsigned shutdownMode );

	static void SetupTemplate(sEditedTextProperties text1);
	static void SetupTemplate(sEditedTextProperties text1, sEditedTextProperties text2);
	static void UnloadTemplate();

	static bool IsSetup() { return ms_templateIsSetup; }
	static void UpdateTemplate(s32 const c_index, sEditedTextProperties text, atHashString const c_hash = atHashString::Null());

	static void PreReplayEditorRender();
	static void PostReplayEditorRender();
	static void RenderSnapmatic();

	static void UpdateTemplateWindow();
	static void SetTemplateWindow(Vector2 const c_pos, Vector2 const c_size);

	static s32 GetMovieId() { return ms_movieId; }

	static void OpenMovie();
	static void CloseMovie();
	static bool IsMovieActive();

	static void SetCurrentTemplate(atHashString const c_newTemplate);

	static void DefaultText();

	static void EditTextColor(s32 const c_direction);
	static bool FontCanBeEdited();
	static void EditTextFont(s32 const c_direction);
	static void EditTextOpacity(s32 const c_direction);

	static sEditedTextProperties &GetEditedText() { return ms_EditedText; }

private:

	static CTextTemplateXML ms_templateArray;

	static atHashString ms_currentTemplate;

	static sEditedTextProperties ms_EditedText;
	static sEditedTextProperties ms_EditedTextBackup;
	static s32 ms_textProjectEditing;
	static void SetTextIndexEditing(const s32 iProjectIndex) { ms_textProjectEditing = iProjectIndex; }
	static s32 GetTextIndexEditing() { return ms_textProjectEditing; }

	static s32 FindCurrentTemplateActionscriptEnum();

	static s32 GetDefaultValueOfProperty(atHashString const c_propertyId);
	static CPropertyData *GetDefaultPropertyData(atHashString const c_propertyId);

	static bool ms_templateIsSetup;
	static bool ms_shouldRender;

	static s32 ms_movieId;



protected:
	friend class CVideoEditorMenu;
	friend class CVideoEditorTimeline;
	friend class CVideoEditorUi;


	static bool ShouldRender(bool const c_postRender);

	static s32 FindTemplateArrayIndex();

	static sEditedTextProperties &GetEditedTextBackup() { return ms_EditedTextBackup; }
	static atHashString GetCurrentTemplate() { return ms_currentTemplate; }

	static char *FindTemplateName(atHashString const c_templateId);

	static CPropertyData *GetPropertyData(atHashString const c_propertyId, s32 const c_index);
	
	static s32 GetPropertyIndex(atHashString const c_propertyId);
	static s32 GetPropertyCount(atHashString const c_propertyId);

	static s32 GetPropertyValueNumber(s32 const c_propertyIndex, s32 const c_index);
	static s32 GetPropertyValueHash(s32 const c_propertyIndex, atHashString const c_hash);
	static s32 GetPropertyValueString(s32 const c_propertyIndex, atString const c_string);

	static atHashString GetTemplateIdFromArrayIndex(s32 arrayIndex);

	static bool IsPropertyValidInTemplate(atHashString const c_propertyId);

};



#endif // #if defined( GTA_REPLAY ) && GTA_REPLAY

#endif // TEXT_TEMPLATE_H

// eof
