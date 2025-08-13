/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : TextTemplate.cpp
// PURPOSE : one template
// AUTHOR  : Derek Payne
// STARTED : 17/06/2015
//
/////////////////////////////////////////////////////////////////////////////////
#include "frontend/VideoEditor/ui/TextTemplate.h"

// fw
#include "parser/manager.h"

#include "frontend/VideoEditor/ui/TextTemplate_parser.h"

#include "grcore/allocscope.h"
#include "parser/manager.h"
#include "fwsys/fileExts.h"
#include "fwscene/stores/psostore.h"
#include "text/TextFile.h"
#include "frontend/VideoEditor/ui/Menu.h"
#include "frontend/VideoEditor/ui/Playback.h"
#include "frontend/VideoEditor/ui/Timeline.h"
#include "frontend/WarningScreen.h"
#include "renderer/PostProcessFXHelper.h"
#include "replaycoordinator/ReplayCoordinator.h"

// game
#include "Frontend/ui_channel.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

FRONTEND_OPTIMISATIONS()

//OPTIMISATIONS_OFF()

#define VIDEO_EDITOR_TEXT_TEMPLATES_FILENAME	"TextCanvas"

CTextTemplateXML CTextTemplate::ms_templateArray;
atHashString CTextTemplate::ms_currentTemplate;
s32 CTextTemplate::ms_textProjectEditing;
sEditedTextProperties CTextTemplate::ms_EditedText;
sEditedTextProperties CTextTemplate::ms_EditedTextBackup;
bool CTextTemplate::ms_templateIsSetup;
bool CTextTemplate::ms_shouldRender;

s32 CTextTemplate::ms_movieId = INVALID_MOVIE_ID;

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextTemplate::Init
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////////
void CTextTemplate::Init()
{
	parSettings settings = PARSER.Settings();
	 
	settings.SetFlag(parSettings::CULL_OTHER_PLATFORM_DATA, true);
	PARSER.LoadObject("common:/data/ui/TextTemplates", "XML", ms_templateArray, &settings);
	 
	uiAssertf(ms_templateArray.CTemplate.GetCount() != 0, "No text templates found in XML!");

	// reset variables:
	ms_textProjectEditing = -1;
	ms_templateIsSetup = false;
	ms_shouldRender = false;

	// default to first (standard) template:
	SetCurrentTemplate("TEXT_TEMPLATE_CUSTOM");  // 1st template in array is always the default
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextTemplate::Shutdown
// PURPOSE:
/////////////////////////////////////////////////////////////////////////////////////////
void CTextTemplate::Shutdown()
{
	ms_templateArray.CTemplate.Reset();
}

void CTextTemplate::CoreShutdown( unsigned UNUSED_PARAM( shutdownMode ) )
{
    Shutdown();
    CloseMovie();
}

void CTextTemplate::OpenMovie()
{
#if USE_TEXT_CANVAS
	if (!CScaleformMgr::IsMovieActive(ms_movieId))
	{
		ms_movieId = CScaleformMgr::CreateMovie(VIDEO_EDITOR_TEXT_TEMPLATES_FILENAME, Vector2(0,0), Vector2(1.0f,1.0f), true, -1, -1, true, SF_MOVIE_TAGGED_BY_CODE, false, true);
        uiDisplayf( "CTextTemplate::OpenMovie - Movie opened with id %d", ms_movieId );
	}
#endif
}

void CTextTemplate::CloseMovie()
{
	if (CScaleformMgr::IsMovieActive(ms_movieId))
	{
        uiDisplayf( "CTextTemplate::CloseMovie - Movie closed with id %d", ms_movieId );
		CScaleformMgr::RequestRemoveMovie(ms_movieId);
	}

	ms_movieId = INVALID_MOVIE_ID;
}

bool CTextTemplate::IsMovieActive()
{
	return (CScaleformMgr::IsMovieActive(ms_movieId));
}

void CTextTemplate::PreReplayEditorRender()
{
	if (ShouldRender(false))
	{
		CScaleformMgr::RenderMovie(ms_movieId, fwTimer::GetSystemTimeStep(), true);
	}
}

void CTextTemplate::PostReplayEditorRender()
{
	if (ShouldRender(true))
	{
		CScaleformMgr::RenderMovie(ms_movieId, fwTimer::GetSystemTimeStep(), true);
	}
}

void CTextTemplate::RenderSnapmatic()
{
	if (CScaleformMgr::IsMovieActive(ms_movieId))
	{
		CScaleformMgr::RenderMovie(ms_movieId, fwTimer::GetSystemTimeStep(), true);
	}
}

bool CTextTemplate::ShouldRender(bool const c_postRender)
{
	if (!CScaleformMgr::IsMovieActive(ms_movieId))
	{
		return false;
	}

	if (CWarningScreen::IsActive() || CVideoEditorMenu::IsUserConfirmationScreenActive())  // not on warning screens
	{
		return false;
	}

	if (!c_postRender)
	{
		if (!CVideoEditorPlayback::IsActive())
		{
			return false;
		}

		if (!CReplayCoordinator::ShouldRenderOverlays())
		{
			return false;
		}
	}
	else
	{
		if (!CVideoEditorTimeline::ShouldRenderOverlayOverCentreStageClip() &&
			!CVideoEditorUi::ShouldRenderOverlayDuringTextEdit())
		{
			return false;
		}
	}

	if (!ms_templateIsSetup)  // no template is setup
	{
		return false;
	}

	return USE_TEXT_CANVAS ? true : false;
}


void CTextTemplate::UpdateTemplateWindow()
{
	if (CScaleformMgr::IsMovieActive(ms_movieId))
	{
		if (CVideoEditorPlayback::IsActive())
		{
			CScaleformMgr::UpdateMovieParams(ms_movieId, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f));
		}
		else if (CVideoEditorTimeline::IsTimelineSelected())
		{
			CScaleformMgr::UpdateMovieParams(ms_movieId, CVideoEditorTimeline::GetStagePosition(), CVideoEditorTimeline::GetStageSize());
		}
		else
		{
			CScaleformMgr::UpdateMovieParams(ms_movieId, CVideoEditorMenu::GetStagePosition(), CVideoEditorTimeline::GetStageSize());
		}
	}
}

void CTextTemplate::SetTemplateWindow(Vector2 const c_pos, Vector2 const c_size)
{
	if (CScaleformMgr::IsMovieActive(ms_movieId))
	{
		CScaleformMgr::UpdateMovieParams(ms_movieId, c_pos, c_size);
	}
}

void CTextTemplate::UpdateTemplate(s32 const c_index, sEditedTextProperties text, atHashString const c_hash)
{
	if (!ms_templateIsSetup)
	{
		SetupTemplate(text);
	}
	else
	{
		if (CScaleformMgr::IsMovieActive(ms_movieId))
		{
			sEditedTextProperties &editedText = text;

			atHashString const c_templateId = ms_currentTemplate;

			for (s32 count = 0; count < ms_templateArray.CTemplate.GetCount(); count++)
			{
				if (ms_templateArray.CTemplate[count].templateId == c_templateId)  // a match
				{
					for (s32 templateIdCount = 0; templateIdCount < ms_templateArray.CTemplate[count].data.GetCount(); templateIdCount++)
					{
						atHashString const c_propertyIdHash = ms_templateArray.CTemplate[count].data[templateIdCount].propertyId;

						if (c_hash.IsNull() || (c_hash == c_propertyIdHash && c_index == ms_templateArray.CTemplate[count].data[templateIdCount].index))
						{
							//
							// update the property to actionscript
							//
							if (CScaleformMgr::BeginMethod(ms_movieId, SF_BASE_CLASS_VIDEO_EDITOR, "UPDATE_PROPERTY"))
							{
								CScaleformMgr::AddParamInt(ms_templateArray.CTemplate[count].data[templateIdCount].index);
								CScaleformMgr::AddParamInt(ms_templateArray.CTemplate[count].data[templateIdCount].actionScriptEnum);

								if (c_propertyIdHash == atHashString("TEXT"))
								{
									CScaleformMgr::AddParamString(editedText.m_text);
								}
								else if (c_propertyIdHash == atHashString("FONT"))
								{
									s32 const c_propertyIndex = CTextTemplate::GetPropertyIndex(c_propertyIdHash);
									s32 dataIndex = CTextTemplate::GetPropertyValueNumber(c_propertyIndex, editedText.m_style);

									CPropertyData const *pPropertyData = CTextTemplate::GetPropertyData(c_propertyIdHash, dataIndex);

									if (pPropertyData)
									{
										CScaleformMgr::AddParamString(pPropertyData->valueString);
									}
								}
								else if (c_propertyIdHash == atHashString("COLOR"))
								{
									s32 const c_propertyIndex = CTextTemplate::GetPropertyIndex(c_propertyIdHash);
									s32 dataIndex = CTextTemplate::GetPropertyValueHash(c_propertyIndex, CHudColour::GetKeyFromColour(editedText.m_hudColor));

									CPropertyData const *pPropertyData = CTextTemplate::GetPropertyData(c_propertyIdHash, dataIndex);

									if (pPropertyData)
									{
										CScaleformMgr::AddParamInt(CHudColour::GetColourFromKey(pPropertyData->valueHash));
									}
								}
								else if (c_propertyIdHash == atHashString("POSITION_X"))
								{
									CScaleformMgr::AddParamFloat(editedText.m_position.x);
								}
								else if (c_propertyIdHash == atHashString("POSITION_Y"))
								{
									CScaleformMgr::AddParamFloat(editedText.m_position.y);
								}
								else if (c_propertyIdHash == atHashString("SCALE"))
								{
									float const c_scale = editedText.m_scale * (HACK_FOR_OLD_SCALE * ACTIONSCRIPT_STAGE_SIZE_Y);

									CScaleformMgr::AddParamFloat(c_scale);
								}
								else if (c_propertyIdHash == atHashString("OPACITY"))
								{
									CScaleformMgr::AddParamFloat(editedText.m_alpha * 100.0f);
								}

								CScaleformMgr::EndMethod();
							}
						}
					}
				}
			}
		}
	}
}

void CTextTemplate::SetupTemplate(sEditedTextProperties text1)
{
	if (CScaleformMgr::IsMovieActive(ms_movieId))
	{
		UnloadTemplate();

		if (CScaleformMgr::BeginMethod(ms_movieId, SF_BASE_CLASS_VIDEO_EDITOR, "BEGIN_SETUP_TEMPLATE"))
		{
			CScaleformMgr::AddParamInt(FindCurrentTemplateActionscriptEnum());
			CScaleformMgr::EndMethod();
		}

		ms_templateIsSetup = true;
		ms_shouldRender = true;

		UpdateTemplate(0, text1);

		if (CScaleformMgr::BeginMethod(ms_movieId, SF_BASE_CLASS_VIDEO_EDITOR, "END_SETUP_TEMPLATE"))
		{
			CScaleformMgr::EndMethod();
		}

		if (CScaleformMgr::BeginMethod(ms_movieId, SF_BASE_CLASS_VIDEO_EDITOR, "START_TEMPLATE"))
		{
			CScaleformMgr::EndMethod();
		}
	}
}


void CTextTemplate::SetupTemplate(sEditedTextProperties text1, sEditedTextProperties text2)
{
	if (CScaleformMgr::IsMovieActive(ms_movieId))
	{
		UnloadTemplate();

		if (CScaleformMgr::BeginMethod(ms_movieId, SF_BASE_CLASS_VIDEO_EDITOR, "BEGIN_SETUP_TEMPLATE"))
		{
			CScaleformMgr::AddParamInt(FindCurrentTemplateActionscriptEnum());
			CScaleformMgr::EndMethod();
		}

		ms_templateIsSetup = true;
		ms_shouldRender = true;

		UpdateTemplate(0, text1);
		UpdateTemplate(1, text2);

		if (CScaleformMgr::BeginMethod(ms_movieId, SF_BASE_CLASS_VIDEO_EDITOR, "END_SETUP_TEMPLATE"))
		{
			CScaleformMgr::EndMethod();
		}

		if (CScaleformMgr::BeginMethod(ms_movieId, SF_BASE_CLASS_VIDEO_EDITOR, "START_TEMPLATE"))
		{
			CScaleformMgr::EndMethod();
		}
	}
}

void CTextTemplate::UnloadTemplate()
{
	if (CScaleformMgr::IsMovieActive(ms_movieId))
	{
		if (ms_templateIsSetup)
		{
			if (CScaleformMgr::BeginMethod(ms_movieId, SF_BASE_CLASS_VIDEO_EDITOR, "DISPOSE"))
			{
				CScaleformMgr::EndMethod();
			}
		}
	}

	ms_templateIsSetup = false;
	ms_shouldRender = false;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextTemplate::SetCurrentTemplate
// PURPOSE:	sets the current template in use
/////////////////////////////////////////////////////////////////////////////////////////
void CTextTemplate::SetCurrentTemplate(atHashString const c_newTemplate)
{
	UnloadTemplate();  // dispose any current template
	ms_currentTemplate = c_newTemplate;
}



void CTextTemplate::DefaultText()
{
	sEditedTextProperties &editedText = CTextTemplate::GetEditedText();

	CPropertyData *pPropertyData;

	pPropertyData = CTextTemplate::GetDefaultPropertyData("COLOR");

	if (pPropertyData)
	{
		editedText.m_hudColor = CHudColour::GetColourFromKey(pPropertyData->valueHash);
	}

	pPropertyData = CTextTemplate::GetDefaultPropertyData("FONT");

	if (pPropertyData)
	{
		editedText.m_style = pPropertyData->valueNumber;
	}

	editedText.m_alpha = 1.0f;
	editedText.m_colorRGBA = CHudColour::GetRGB(editedText.m_hudColor, 255);
	editedText.m_text[0] = '\0';
	editedText.m_durationMs = DEFAULT_TEXT_DURATION_MS;
	editedText.m_position = (Vector2(0.5f, 0.418f));
	editedText.m_positionType = 0;
	editedText.m_template.Clear();

	editedText.m_scale = 1.69f;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextTemplate::EditTextColor
// PURPOSE:	adjusts the hudcolor and rgba properties of the text
/////////////////////////////////////////////////////////////////////////////////////////
void CTextTemplate::EditTextColor(s32 const c_direction)
{
	if (c_direction != 0)
	{
		sEditedTextProperties &editedText = CTextTemplate::GetEditedText();

		s32 const c_adjustment = c_direction > 0 ? 1 : -1;
		s32 const c_maxDataCount = CTextTemplate::GetPropertyCount("COLOR");
		s32 const c_propertyIndex = CTextTemplate::GetPropertyIndex("COLOR");
		s32 dataIndex = CTextTemplate::GetPropertyValueHash(c_propertyIndex, CHudColour::GetKeyFromColour(editedText.m_hudColor));

		dataIndex += c_adjustment;

		// wrap:
		if (dataIndex >= c_maxDataCount)
		{
			dataIndex = 0;
		}

		if (dataIndex < 0)
		{
			dataIndex = c_maxDataCount - 1;
		}

		CPropertyData const *pPropertyData = CTextTemplate::GetPropertyData("COLOR", dataIndex);

		if (pPropertyData)
		{
			editedText.m_hudColor = CHudColour::GetColourFromKey(pPropertyData->valueHash);
			editedText.m_colorRGBA = CHudColour::GetRGB(editedText.m_hudColor, (u8)(editedText.m_alpha * 255.0f));

			CTextTemplate::UpdateTemplate(0, editedText, "COLOR");
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextTemplate::FontCanBeEdited
// PURPOSE:	returns true if the current language setting will allow EditTextFont() to have any effect
///////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTextTemplate::FontCanBeEdited()
{
	if ( CText::IsAsianLanguage() || (CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE) == LANGUAGE_RUSSIAN) )
	{
		return false;
	}

	return true;
}


/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextTemplate::EditTextFont
// PURPOSE:	adjusts the font style of the text
/////////////////////////////////////////////////////////////////////////////////////////
void CTextTemplate::EditTextFont(s32 const c_direction)
{
	if (c_direction != 0)
	{
		if (FontCanBeEdited())
		{
			sEditedTextProperties &editedText = CTextTemplate::GetEditedText();

			s32 const c_adjustment = c_direction > 0 ? 1 : -1;

#if USE_TEXT_CANVAS
			s32 const c_maxDataCount = CTextTemplate::GetPropertyCount("FONT");
#else
			s32 const c_maxDataCount = 4;
#endif
			s32 const c_propertyIndex = CTextTemplate::GetPropertyIndex("FONT");
			s32 dataIndex = CTextTemplate::GetPropertyValueNumber(c_propertyIndex, editedText.m_style);

			dataIndex += c_adjustment;

			// wrap:
			if (dataIndex >= c_maxDataCount)
			{
				dataIndex = 0;
			}

			if (dataIndex < 0)
			{
				dataIndex = c_maxDataCount - 1;
			}

			CPropertyData const *pPropertyData = CTextTemplate::GetPropertyData("FONT", dataIndex);

			if (pPropertyData)
			{
				editedText.m_style = pPropertyData->valueNumber;

				CTextTemplate::UpdateTemplate(0, editedText, "FONT");
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextTemplate::EditTextOpacity
// PURPOSE:	adjusts the alpha of the text
/////////////////////////////////////////////////////////////////////////////////////////
void CTextTemplate::EditTextOpacity(s32 const c_direction)
{
	if (c_direction != 0)
	{
		float const c_adjustment = c_direction > 0 ? 0.05f : -0.05f;

		sEditedTextProperties &editedText = CTextTemplate::GetEditedText();

		editedText.m_alpha = Clamp( editedText.m_alpha + c_adjustment, 0.05f, 1.f );  // 5% min alpha
		editedText.m_colorRGBA = CHudColour::GetRGB(editedText.m_hudColor, (u8)(editedText.m_alpha * 255.0f));

		CTextTemplate::UpdateTemplate(0, editedText, "OPACITY");
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextTemplate::FindCurrentTemplateActionscriptEnum
// PURPOSE:	returns the actionscript enum for the current template
/////////////////////////////////////////////////////////////////////////////////////////
s32 CTextTemplate::FindCurrentTemplateActionscriptEnum()
{
	atHashString const c_templateId = ms_currentTemplate;

	for (s32 count = 0; count < ms_templateArray.CTemplate.GetCount(); count++)
	{
		if (ms_templateArray.CTemplate[count].templateId == c_templateId)
		{
			return ms_templateArray.CTemplate[count].actionScriptEnum;
		}
	}

	uiAssertf(0, "CTextTemplate::FindTemplateArrayIndex() couldnt find template '%s'", c_templateId.GetCStr());

	return -1;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextTemplate::FindTemplateArrayIndex
// PURPOSE:	returns the index in the array of the current template
/////////////////////////////////////////////////////////////////////////////////////////
s32 CTextTemplate::FindTemplateArrayIndex()
{
	atHashString const c_templateId = ms_currentTemplate;

	for (s32 count = 0; count < ms_templateArray.CTemplate.GetCount(); count++)
	{
		if (ms_templateArray.CTemplate[count].templateId == c_templateId)
		{
			return count;
		}
	}

	uiAssertf(0, "CTextTemplate::FindTemplateArrayIndex() couldnt find template '%s'", c_templateId.GetCStr());

	return -1;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextTemplate::FindTemplateName
// PURPOSE:	returns the display name of the passed template id
/////////////////////////////////////////////////////////////////////////////////////////
char *CTextTemplate::FindTemplateName(atHashString const c_templateId)
{
	for (s32 count = 0; count < ms_templateArray.CTemplate.GetCount(); count++)
	{
		if (ms_templateArray.CTemplate[count].templateId == c_templateId)
		{
			return TheText.Get(ms_templateArray.CTemplate[count].textId.GetHash(), "");
		}
	}

	uiAssertf(0, "CTextTemplate::FindTemplateName() couldnt find template '%s'", c_templateId.GetCStr());

	return NULL;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextTemplate::GetDefaultPropertyData
// PURPOSE:	returns the property data of the defaulting value for this property id
/////////////////////////////////////////////////////////////////////////////////////////
CPropertyData *CTextTemplate::GetDefaultPropertyData(atHashString const c_propertyId)
{
	s32 const c_propertyIndex = GetPropertyIndex(c_propertyId);

	if (c_propertyIndex != -1)
	{
		s32 const c_defaultIndex = GetDefaultValueOfProperty(c_propertyId);

		return &ms_templateArray.CProperties[c_propertyIndex].data[c_defaultIndex];
	}

	return NULL;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextTemplate::GetPropertyData
// PURPOSE:	returns the property data of the passed template and property id based on the
//			index
/////////////////////////////////////////////////////////////////////////////////////////
CPropertyData *CTextTemplate::GetPropertyData(atHashString const c_propertyId, s32 const c_index)
{
	atHashString const c_templateId = ms_currentTemplate;

	s32 const c_propertyIndex = GetPropertyIndex(c_propertyId);

	if (c_propertyIndex != -1)
	{
		return &ms_templateArray.CProperties[c_propertyIndex].data[c_index];
	}

	return NULL;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextTemplate::GetDefaultValueOfProperty
// PURPOSE:	returns the property index we should use as the default for this template and
//			property id
/////////////////////////////////////////////////////////////////////////////////////////
s32 CTextTemplate::GetDefaultValueOfProperty(atHashString const c_propertyId)
{
	atHashString const c_templateId = ms_currentTemplate;

	for (s32 count = 0; count < ms_templateArray.CTemplate.GetCount(); count++)
	{
		if (ms_templateArray.CTemplate[count].templateId == c_templateId)  // a match
		{
			for (s32 templateIdCount = 0; templateIdCount < ms_templateArray.CTemplate[count].data.GetCount(); templateIdCount++)
			{
				if (ms_templateArray.CTemplate[count].data[templateIdCount].propertyId == c_propertyId)  // a match
				{
					return (ms_templateArray.CTemplate[count].data[templateIdCount].defaultValue);
				}
			}
		}
	}

	return -1;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextTemplate::GetPropertyIndex
// PURPOSE:	returns the property array index of the passed property
/////////////////////////////////////////////////////////////////////////////////////////
s32 CTextTemplate::GetPropertyIndex(atHashString const c_propertyId)
{
	atHashString const c_templateId = ms_currentTemplate;

	for (s32 count = 0; count < ms_templateArray.CTemplate.GetCount(); count++)
	{
		if (ms_templateArray.CTemplate[count].templateId == c_templateId)  // a match
		{
			for (s32 templateIdCount = 0; templateIdCount < ms_templateArray.CTemplate[count].data.GetCount(); templateIdCount++)
			{
				if (ms_templateArray.CTemplate[count].data[templateIdCount].propertyId == c_propertyId)  // a match
				{
					atHashString const c_dataId = ms_templateArray.CTemplate[count].data[templateIdCount].dataId;

					for (s32 propertyCount = 0; propertyCount < ms_templateArray.CProperties.GetCount(); propertyCount++)
					{
						if (ms_templateArray.CProperties[propertyCount].propertyId == c_dataId)  // a match
						{
							return propertyCount;
						}
					}
				}
			}
		}
	}

	return -1;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextTemplate::GetPropertyValueHash
// PURPOSE:	returns the property index of the passed matching ValueNumber
/////////////////////////////////////////////////////////////////////////////////////////
s32 CTextTemplate::GetPropertyValueNumber(s32 const c_propertyIndex, s32 const c_index)
{
	for (s32 dataCount = 0; dataCount < ms_templateArray.CProperties[c_propertyIndex].data.GetCount(); dataCount++)
	{
		if (ms_templateArray.CProperties[c_propertyIndex].data[dataCount].valueNumber == c_index)
		{
			return dataCount;
		}
	}

	uiAssertf(0, "CTextTemplate::GetPropertyValueNumber() couldnt find index %d in property %d", c_index, c_propertyIndex);

	return -1;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextTemplate::GetPropertyValueHash
// PURPOSE:	returns the property index of the passed matching ValueHash
/////////////////////////////////////////////////////////////////////////////////////////
s32 CTextTemplate::GetPropertyValueHash(s32 const c_propertyIndex, atHashString const c_hash)
{
	for (s32 dataCount = 0; dataCount < ms_templateArray.CProperties[c_propertyIndex].data.GetCount(); dataCount++)
	{
		if (ms_templateArray.CProperties[c_propertyIndex].data[dataCount].valueHash == c_hash)
		{
			return dataCount;
		}
	}

	uiAssertf(0, "CTextTemplate::GetPropertyValueHash() couldnt find hash %u (%s) in property %d", c_hash.GetHash(), c_hash.TryGetCStr(), c_propertyIndex);

	return -1;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextTemplate::GetPropertyValueHash
// PURPOSE:	returns the property index of the passed matching ValueHash
/////////////////////////////////////////////////////////////////////////////////////////
s32 CTextTemplate::GetPropertyValueString(s32 const c_propertyIndex, atString const c_string)
{
	for (s32 dataCount = 0; dataCount < ms_templateArray.CProperties[c_propertyIndex].data.GetCount(); dataCount++)
	{
		if (ms_templateArray.CProperties[c_propertyIndex].data[dataCount].valueString == c_string)
		{
			return dataCount;
		}
	}

	uiAssertf(0, "CTextTemplate::GetPropertyValueHash() couldnt find hash %s in property %d", c_string.c_str(), c_propertyIndex);

	return -1;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextTemplate::GetPropertyCount
// PURPOSE:	returns the max amount of items the passed property uses
/////////////////////////////////////////////////////////////////////////////////////////
s32 CTextTemplate::GetPropertyCount(atHashString const c_propertyId)
{
	atHashString const c_templateId = ms_currentTemplate;

	for (s32 count = 0; count < ms_templateArray.CTemplate.GetCount(); count++)
	{
		if (ms_templateArray.CTemplate[count].templateId == c_templateId)  // a match
		{
			for (s32 templateIdCount = 0; templateIdCount < ms_templateArray.CTemplate[count].data.GetCount(); templateIdCount++)
			{
				if (ms_templateArray.CTemplate[count].data[templateIdCount].propertyId == c_propertyId)  // a match
				{
					atHashString const c_dataId = ms_templateArray.CTemplate[count].data[templateIdCount].dataId;

					for (s32 propertyCount = 0; propertyCount < ms_templateArray.CProperties.GetCount(); propertyCount++)
					{
						if (ms_templateArray.CProperties[propertyCount].propertyId == c_dataId)  // a match
						{
							return ms_templateArray.CProperties[propertyCount].data.GetCount();
						}
					}
				}
			}
		}
	}

	uiAssertf(0, "CTextTemplate::GetPropertyCount()");

	return 0;
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextTemplate::GetTemplateIdFromArrayIndex
// PURPOSE:	returns the hash of the passed template index
/////////////////////////////////////////////////////////////////////////////////////////
atHashString CTextTemplate::GetTemplateIdFromArrayIndex(s32 arrayIndex)
{
	if (arrayIndex >= ms_templateArray.CTemplate.GetCount())
	{
		arrayIndex = 0;
	}

	if (arrayIndex < 0)
	{
		arrayIndex = ms_templateArray.CTemplate.GetCount()-1;
	}

	if (uiVerifyf(arrayIndex >= 0 && arrayIndex < ms_templateArray.CTemplate.GetCount(), "CTextTemplate::GetTemplateIdFromArrayIndex - index outside of array %d/%d", arrayIndex, ms_templateArray.CTemplate.GetCount()))
	{
		return ms_templateArray.CTemplate[arrayIndex].templateId;
	}

	return atHashString::Null();
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CTextTemplate::IsPropertyValidInTemplate
// PURPOSE:	is the passed property valid in the passed template?
/////////////////////////////////////////////////////////////////////////////////////////
bool CTextTemplate::IsPropertyValidInTemplate(atHashString const c_propertyId)
{
	atHashString const c_templateId = ms_currentTemplate;

	for (s32 count = 0; count < ms_templateArray.CTemplate.GetCount(); count++)
	{
		if (ms_templateArray.CTemplate[count].templateId == c_templateId)  // a match
		{
			for (s32 templateIdCount = 0; templateIdCount < ms_templateArray.CTemplate[count].data.GetCount(); templateIdCount++)
			{
				if (ms_templateArray.CTemplate[count].data[templateIdCount].propertyId == c_propertyId)  // a match
				{
					return (ms_templateArray.CTemplate[count].data[templateIdCount].visibleInMenu);
				}
			}
		}
	}

	return false;
}



#endif // #if defined( GTA_REPLAY ) && GTA_REPLAY

// eof
