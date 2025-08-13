/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : MovieClipText.h
// PURPOSE : manages drawing text with textures
// AUTHOR  : James Chagaris
// STARTED : 13/07/2012
//
/////////////////////////////////////////////////////////////////////////////////

// rage:
#include "string/stringutil.h"
#include "system/param.h"

// game:
#include "Frontend/MovieClipText.h"
#include "Frontend/MiniMapCommon.h"
#include "Frontend/ui_channel.h"
#include "Frontend/Scaleform/ScaleFormMgr.h"
#include "System/controlMgr.h"
#include "Text/Text.h"
#include "Text/TextConversion.h"
#include "Text/TextFormat.h"

#define BLANK_SPACE_CHARACTER ("\xE2\x80\xA1") // Double dagger [on certain fonts this has been edited to be a very wide space]
//#define BLANK_SPACE_CHARACTER (" ") // just a regular ol' space

#define NEW_LINE_CHARACTER ("~n~")
#define NEW_LINE_LEN (3)
#define NEW_LINE_NOT_FOUND (-1)

#define INPUT_KEY "INPUT"
#define INPUT_KEY_LEN 5
#define INPUTGROUP_KEY "INPUTGROUP"
#define INPUTGROUP_KEY_LEN 10
#define HUD_COLOR_KEY "HUD_COLOUR_"
#define HUD_COLOR_KEY_LEN 11
#define HUD_COLOR_ABBREVIATED_KEY "HC_"
#define HUD_COLOR_ABBREVIATED_KEY_LEN (3)
#define DEFAULT_COLOR_KEY "s"
#define LSTICK_ROTATE "PAD_LSTICK_ROTATE"
#define RSTICK_ROTATE "PAD_RSTICK_ROTATE"

#define PAD_IMG_PREFIX_FMT "TEXTBTN_%d"
#define KEY_IMG_PREFIX_FMT "TEXTBTN_999" // must have nested movie called "button" with a textfield "Char" in it
#define KEY_WIDE_IMG_PREFIX_FMT "TEXTBTN_996" // must have nested movie called "button" with a textfield "Char" in it

eHUD_COLOURS CMovieClipText::m_CurrentColor = DEFAULT_COLOR;

#if !__FINAL
PARAM(nouseNewButtonTextHud, "[code] use new button text");
#endif

#if __BANK
bool CMovieClipText::ms_bDontUseIcons = false;
#endif // __BANK

bool CMovieClipText::SetFormattedTextWithIcons(const GFxValue* args, bool bExplicitSlot /* = false*/)
{
	// Defining the order of the args here (should match what gets passed in).
	enum eARG
	{
		ARG_CLASS_TYPE, // This value is already checked before this function is called.
		ARG_TEXT,
		ARG_PARENT_MOVIE,
		ARG_TEXT_FIELD,
		ARG_TYPEFACE,
		ARG_FONT_SCALE,
		ARG_LEADING,
		ARG_HALIGNMENT,
		ARG_TRANSLATE_TEXT,
		ARG_EXPLICIT_SLOT
	};

	#if !__FINAL || __FINAL_LOGGING
		const char* thisFuncName = bExplicitSlot ? "SET_FORMATTED_TEXT_WITH_ICONS_EXPLICIT" : "SET_FORMATTED_TEXT_WITH_ICONS";
	#endif

	if (Verifyf(args[ARG_TEXT].IsString(), "%s - the argument %d should be string for the text", thisFuncName, ARG_TEXT))
	{
		const char* locText = (char*)args[ARG_TEXT].GetString();

		if(Verifyf(args[ARG_TEXT_FIELD].IsDisplayObject(), "%s - the argument %d should be the text field for %s",thisFuncName,  ARG_TEXT_FIELD, locText) && 
			Verifyf(args[ARG_PARENT_MOVIE].IsDisplayObject(), "%s - the argument %d should be a container for the icons for %s",thisFuncName,  ARG_PARENT_MOVIE, locText) && 
			Verifyf(args[ARG_FONT_SCALE].IsNumber(), "%s - the argument %d should be a number for the font scale for %s",thisFuncName,  ARG_FONT_SCALE, locText) && 
			Verifyf(args[ARG_LEADING].IsNumber(), "%s - the argument %d should be a number for the line spacing for %s", thisFuncName, ARG_LEADING, locText))
		{
			char* pGxtText = (args[ARG_TRANSLATE_TEXT].IsBool() && args[ARG_TRANSLATE_TEXT].GetBool()) ? TheText.Get(locText) : (char*)locText;

			GFxValue parentMovie = args[ARG_PARENT_MOVIE];
			GFxValue textMovie = args[ARG_TEXT_FIELD];
			const u8 fontStyle = (u8)args[ARG_TYPEFACE].GetNumber();
			const float fontScale = (float)args[ARG_FONT_SCALE].GetNumber();
			const int leading = (int)args[ARG_LEADING].GetNumber();
			GFxDrawText::Alignment hAlignment = GFxDrawText::Align_Left;

			if(args[ARG_HALIGNMENT].IsNumber())
				hAlignment = static_cast<GFxDrawText::Alignment>(static_cast<int>(args[ARG_HALIGNMENT].GetNumber()));
			if(args[ARG_HALIGNMENT].IsBool() && args[ARG_HALIGNMENT].GetBool())
				hAlignment = GFxDrawText::Align_Center;

			const u8 slot = args[ARG_EXPLICIT_SLOT].IsNumber() ? u8(args[ARG_EXPLICIT_SLOT].GetNumber()) : 0;

			GFxValue gfxWidth;
			GFxValue gfxX;
			GFxValue gfxY;

			textMovie.GetMember("_x", &gfxX);
			textMovie.GetMember("_y", &gfxY);
			textMovie.GetMember("_width", &gfxWidth);
			
			float textMovieX = (float)gfxX.GetNumber();
			float textMovieY = (float)gfxY.GetNumber();
			const float textBoxWidth = (float)gfxWidth.GetNumber();
			
			atArray<stBlipInfo> tagCharacters;
			tagCharacters.Reset();

			// Need to add the offset of the text (from the parent movie) to align the blip properly.
			const char* startTag = NULL;
			const char* endTag = NULL;

			char textBuffer[MAX_CHARS_FOR_TEXT_STRING]={0};
			atArray<ImageInfo> tags(0, 5);

			ParserResults parserResults;
			parserResults.Reset();
			m_CurrentColor = DEFAULT_COLOR;

			int blankSpaceCharLen = CTextConversion::GetByteCount(BLANK_SPACE_CHARACTER);
#if GTA_VERSION >= 500
			if((fontStyle == FONT_STYLE_CONDENSED_NOT_GAMERNAME) || (fontStyle == FONT_STYLE_STANDARD))
			{
				blankSpaceCharLen = CTextConversion::GetByteCount(BLANK_NBSP_CHARACTER);
			}
#endif // GTA_VERSION >= 500

			GFxDrawTextManager* drawText = CScaleformMgr::GetMovieMgr()->GetDrawTextManager();

			// avoid CTextFormat and go straight to scaleform
			GFxDrawTextManager::TextParams params;
			CTextFormat::GetFontName(params.FontName, fontStyle);
			params.FontSize = fontScale;
			params.Leading = leading;
			params.HAlignment = hAlignment;

			{
				// textBuffer2 is a temporary array with all markup removed to send to GetCharacterBounds()
				char textBuffer2[MAX_CHARS_FOR_TEXT_STRING]={0};
				char* pTextBuffer = &textBuffer[0];
				char* pTextBuffer2 = &textBuffer2[0];
				int currentChar = 0;
				int iCurrentEmbed = 0;
				GRectF spacerBounds;

				const int MAX_TOKENS_IN_STRING = 32;
				int iTagCharacterWidthIndex = 0;
				float fTagCharacterWidths[MAX_TOKENS_IN_STRING];
				int iCurrentGroupIndex = 0;

				do
				{
					parserResults.Reset();
					startTag = strchr(pGxtText, FONT_CHAR_TOKEN_DELIMITER);
					if(startTag)
					{
						endTag = strchr(startTag+1, FONT_CHAR_TOKEN_DELIMITER);
						if(uiVerifyf(endTag, "SET_FORMATTED_TEXT_WITH_ICONS - The tag found an opening ~ but not a closing ~ in '%s'", locText))
						{
							int textLen = ptrdiff_t_to_int(startTag - pGxtText);
							int tagLen = ptrdiff_t_to_int(endTag-startTag-1);
							int afterTagIndex = 2 + textLen + tagLen;

							if(!uiVerifyf(textLen < MAX_CHARS_FOR_TEXT_STRING - (pTextBuffer-&textBuffer[0]), "Parsing %s to add blips, but it doesn't fit in the %d character limit", locText, MAX_CHARS_FOR_TEXT_STRING))
							{
								break;
							}

							char tagNameBuffer[MAX_TOKEN_LEN]={0};

							// Copy everything up to the first ~
							safecpy(pTextBuffer, pGxtText, textLen + 1);
							safecpy(pTextBuffer2, pGxtText, textLen + 1);
							currentChar += CountUtf8Characters(pTextBuffer2);
							pTextBuffer += textLen;
							pTextBuffer2 += textLen;

							safecpy(tagNameBuffer, startTag+1, tagLen + 1);
							if(ConvertTag(tagNameBuffer, parserResults, slot, !bExplicitSlot))
							{
								// determine the width of our spacing character in this font
								if(spacerBounds.IsEmpty())
								{
									drawText->GetCharacterBounds(
#if GTA_VERSION >= 500
										(fontStyle == FONT_STYLE_CONDENSED_NOT_GAMERNAME) || (fontStyle == FONT_STYLE_STANDARD) ? BLANK_NBSP_CHARACTER : 
#endif
										BLANK_SPACE_CHARACTER,
										0,
										spacerBounds,
										textBoxWidth,
										&params);
								}

								uiDebugf3("%s - tag converted from %s to %s", thisFuncName, tagNameBuffer, parserResults.m_convertedTagNameBuffer);

								bool bExtraSpaceRequiredForNextButton = false;

								int iGroupStartTagCharacterIndex = tagCharacters.GetCount();
								int i;
								for(i = 0; i < MAX_ICON_LENGTH && (parserResults.m_Prefix[i] != 0 || parserResults.m_bBlip); ++i)
								{
									// Add in the special space character where the tag used to be.
									ParserResults tempResults;
									tempResults.Reset();

									stBlipInfo blip;

									const float movieClipTextIconScale = 6.75f;

									bool bTextButton = false;
									bool bWideTextButton = false;
#if RSG_PC
									bTextButton = parserResults.m_Prefix[i] == 't';
									bWideTextButton = parserResults.m_Prefix[i] == 'T';
#endif // RSG_PC
									char imageCharBuffer[MAX_TOKEN_LEN] = {0};
									char instanceName[30] = {0};
									if(parserResults.m_bBlip)
									{
										formatf(imageCharBuffer, parserResults.m_convertedTagNameBuffer);
									}
									else
									{
										ConvertToHTMLImageString(imageCharBuffer, parserResults.m_iInputIDs[i], bTextButton, bWideTextButton);
									}
									formatf(instanceName, "k_%d", iCurrentEmbed);

									blip.tagName = imageCharBuffer;
									
									// create the movieclip to be attached, then measure its width so we know how many spaces to insert
									float itemWidth = 0.0f;
									const float iconScale = (fontScale * movieClipTextIconScale)/100.0f;
									uiDebugf3("%s - Attaching movie, symbol=%s, instanceName=%s, depth=%d", thisFuncName, blip.tagName.c_str(), instanceName, iCurrentEmbed);

									if( BANK_ONLY(!ms_bDontUseIcons && )
										uiVerifyf(parentMovie.AttachMovie(&blip.movieClip, blip.tagName.c_str(), instanceName, iCurrentEmbed), "%s - failed to attach movie %s", thisFuncName, blip.tagName.c_str()) )
									{
										GFxValue width;
										if( uiVerify(blip.movieClip.GetMember("_width", &width)) )
											itemWidth = float(width.GetNumber()) * iconScale;
									}
									parserResults.m_iNumOfRequiredSpaces[i] = Max(1,rage::round(itemWidth / spacerBounds.Width()));

#if GTA_VERSION >= 500
									if((fontStyle == FONT_STYLE_CONDENSED_NOT_GAMERNAME) || (fontStyle == FONT_STYLE_STANDARD))
									{
										if(i > 0)
										{
											const int iExtraSpaces = 1;
											parserResults.m_iNumOfRequiredSpaces[i] += iExtraSpaces;
										}
									}
									else
#endif // GTA_VERSION >= 500
									{
										if(parserResults.m_iNumOfRequiredSpaces[i] == 1)
										{
											if(bExtraSpaceRequiredForNextButton)
											{
												const int iExtraSpaces = 1;
												parserResults.m_iNumOfRequiredSpaces[i] += iExtraSpaces;
											}
											else
											{
												bExtraSpaceRequiredForNextButton = true;
											}
										}
									}

									fTagCharacterWidths[iTagCharacterWidthIndex++] = itemWidth;
									uiAssert(iTagCharacterWidthIndex < MAX_TOKENS_IN_STRING);

									char blankCharBuffer[MAX_TOKEN_LEN] = {'\0'};
									int additionalChars = 1;
									for(int j = 0; j < parserResults.m_iNumOfRequiredSpaces[i]; j++)
									{
										safecat(blankCharBuffer, 
#if GTA_VERSION >= 500
											(fontStyle == FONT_STYLE_CONDENSED_NOT_GAMERNAME) || (fontStyle == FONT_STYLE_STANDARD) ? BLANK_NBSP_CHARACTER : 
#endif
											BLANK_SPACE_CHARACTER, MAX_TOKEN_LEN);
										if(j > 0)
										{
											additionalChars++;
										}
									}

									safecpy(pTextBuffer, blankCharBuffer, MAX_CHARS_FOR_TEXT_STRING - (int)(pTextBuffer-&textBuffer[0]));
									safecpy(pTextBuffer2, blankCharBuffer, MAX_CHARS_FOR_TEXT_STRING - (int)(pTextBuffer2-&textBuffer2[0]));
									pTextBuffer += blankSpaceCharLen * parserResults.m_iNumOfRequiredSpaces[i];
									pTextBuffer2 += blankSpaceCharLen * parserResults.m_iNumOfRequiredSpaces[i];
								
									tempResults.m_bBlip = parserResults.m_bBlip;
									tempResults.m_color = m_CurrentColor;
									strcpy(tempResults.m_convertedTagNameBuffer, parserResults.m_convertedTagNameBuffer);
									tempResults.m_iInputIDs[0] =parserResults.m_iInputIDs[i];
									tempResults.m_iNumOfRequiredSpaces[0] = parserResults.m_iNumOfRequiredSpaces[i];
									tempResults.m_Prefix[0] = parserResults.m_Prefix[i];

									
									blip.parserResult = tempResults;
									blip.iCharacterPosition = currentChar;
									blip.iGroupIndex = -1;
									blip.fGroupInterCharacterWidth = 0.0f;
									blip.fGroupLeftBound = 0.0f;
									tagCharacters.PushAndGrow(blip);

									currentChar += additionalChars;
									++iCurrentEmbed;

									// Blips never take up more than 1 space so we can break early.
									if(parserResults.m_bBlip)
										break;
								}

								if(i > 1)
								{
									// Group of Tag Characters

									int iGroupEndTagCharacterIndex = tagCharacters.GetCount() - 1;

									float fFarLeftBound = 0.0f;
									float fFarRightBound = 0.0f;
									float fTotalBoundWidth = 0.0f;
									float fTotalTagCharacterWidth = 0.0f;
									float fInterTagCharacterWidth = 0.0f;

									atArray<GRectF> boundsRectsLeft;
									boundsRectsLeft.Resize(tagCharacters[iGroupStartTagCharacterIndex].parserResult.m_iNumOfRequiredSpaces[0]);
									drawText->GetCharacterBounds(textBuffer2, tagCharacters[iGroupStartTagCharacterIndex].iCharacterPosition, boundsRectsLeft[0], textBoxWidth, &params);
									fFarLeftBound = boundsRectsLeft[0].Left;

									const int iLastSpaceIndex = tagCharacters[iGroupEndTagCharacterIndex].parserResult.m_iNumOfRequiredSpaces[0] - 1;
									atArray<GRectF> boundsRectsRight;
									boundsRectsRight.Resize(tagCharacters[iGroupEndTagCharacterIndex].parserResult.m_iNumOfRequiredSpaces[0]);
									drawText->GetCharacterBounds(textBuffer2, tagCharacters[iGroupEndTagCharacterIndex].iCharacterPosition + iLastSpaceIndex, boundsRectsRight[iLastSpaceIndex], textBoxWidth, &params);
									fFarRightBound = boundsRectsRight[tagCharacters[iGroupEndTagCharacterIndex].parserResult.m_iNumOfRequiredSpaces[0] - 1].Right;

									fTotalBoundWidth = fFarRightBound - fFarLeftBound;

									for(int i = iGroupStartTagCharacterIndex; i <= iGroupEndTagCharacterIndex; ++i)
									{
										fTotalTagCharacterWidth += fTagCharacterWidths[i];
									}

									fInterTagCharacterWidth = (fTotalBoundWidth - fTotalTagCharacterWidth) / (iGroupEndTagCharacterIndex - iGroupStartTagCharacterIndex);

									for(int i = iGroupStartTagCharacterIndex; i <= iGroupEndTagCharacterIndex; ++i)
									{
										tagCharacters[i].iGroupIndex = iCurrentGroupIndex;
										tagCharacters[i].fGroupLeftBound = fFarLeftBound;
										tagCharacters[i].fGroupInterCharacterWidth = fInterTagCharacterWidth;
									}

									++iCurrentGroupIndex;
								}
							}
							else if(tagNameBuffer[0] == FONT_CHAR_TOKEN_NEWLINE && tagNameBuffer[1] == '\0')
							{
								*pTextBuffer++ = '\n';
								*pTextBuffer2++ = '\n';

								currentChar++;
							}
							else
							{
								afterTagIndex -= textLen;
								pGxtText += textLen;

								// If tag is unknown copy everything including tag
								safecpy(pTextBuffer, pGxtText, afterTagIndex + 1);
								pTextBuffer += afterTagIndex;
							}

							pGxtText += afterTagIndex;
						}
					}
				}while(startTag && endTag);

				// Copy the rest of the string over
				safecpy(pTextBuffer, pGxtText, MAX_CHARS_FOR_TEXT_STRING - (int)(pTextBuffer-&textBuffer[0]) + 1);
				safecpy(pTextBuffer2, pGxtText, MAX_CHARS_FOR_TEXT_STRING - (int)(pTextBuffer-&textBuffer[0]) + 1);

				float fCurrentTagCharacterOffsetX = 0.0f;
				float fCurrentGroupLeftBound = 0.0f;
				iCurrentGroupIndex = -1;

				for(int i = 0; i < tagCharacters.GetCount(); ++i)
				{
#if RSG_PC
					bool bTextButton = tagCharacters[i].parserResult.m_Prefix[0] == 't';
					bool bWideTextButton = tagCharacters[i].parserResult.m_Prefix[0] == 'T';
#endif // RSG_PC

					atArray<GRectF> boundsRects;
					boundsRects.Resize(tagCharacters[i].parserResult.m_iNumOfRequiredSpaces[0]);
					ImageInfo imageInfo;


					for(int j = 0; j < tagCharacters[i].parserResult.m_iNumOfRequiredSpaces[0]; ++j)
					{
						drawText->GetCharacterBounds(
							textBuffer2,
							tagCharacters[i].iCharacterPosition + j,
							boundsRects[j],
							textBoxWidth,
							&params);
					}

#if __BANK
					if(!ms_bDontUseIcons)
#endif // __BANK
					{
						imageInfo.m_name = tagCharacters[i].tagName;
						imageInfo.m_movieClip = tagCharacters[i].movieClip;
						imageInfo.m_color = tagCharacters[i].parserResult.m_color;
						float fPositionX = 0.0f;

						if(tagCharacters[i].fGroupInterCharacterWidth > 0.0f)
						{
							// Handle Groups of Tag Characters
							if(tagCharacters[i].iGroupIndex != iCurrentGroupIndex &&
							   tagCharacters[i].iGroupIndex != -1)
							{
								iCurrentGroupIndex = tagCharacters[i].iGroupIndex;
								fCurrentGroupLeftBound = tagCharacters[i].fGroupLeftBound;
								fCurrentTagCharacterOffsetX = 0.0f;
							}
							fPositionX = tagCharacters[i].fGroupLeftBound + fCurrentTagCharacterOffsetX + (fTagCharacterWidths[i] * 0.5f);
							fCurrentTagCharacterOffsetX += fTagCharacterWidths[i] + tagCharacters[i].fGroupInterCharacterWidth;
						}
						else
						{
							for(int j = 0; j < boundsRects.size(); ++j)
							{
								fPositionX += boundsRects[j].HCenter();
							}
							fPositionX /= tagCharacters[i].parserResult.m_iNumOfRequiredSpaces[0];
						}

						imageInfo.m_x = fPositionX;
						imageInfo.m_y = boundsRects[0].VCenter();

#if RSG_PC
						if(bTextButton || bWideTextButton)
						{
							int iId = tagCharacters[i].parserResult.m_iInputIDs[0];
							if(uiVerifyf(iId!= 0, "Can't use 0, that's weird."))
							{
								const CControl::KeyInfo& info = CControl::GetKeyInfo(iId);

								// In the case of invalid, we will end up showing a blank icon.
								if( uiVerifyf(info.m_Icon == CControl::KeyInfo::TEXT_ICON || info.m_Icon == CControl::KeyInfo::LARGE_TEXT_ICON, "Shouldn't work even hit this case?")
									&& uiVerifyf(info.m_Icon != CControl::KeyInfo::INVALID_ICON, "Invalid key icon for keycode %u", iId))
								{
									GFxValue button, textField;
									if(imageInfo.m_movieClip.IsDefined()) // could fail to attach above. We'll just bail on it here rather than assert (we asserted when it didn't attach anyway)
									{
										if(bTextButton &&
										   uiVerifyf(imageInfo.m_movieClip.GetMember("button", &button),  KEY_IMG_PREFIX_FMT " doesn't have a 'button' to setText on!") &&
										   uiVerifyf(button.GetMember("Char", &textField), KEY_IMG_PREFIX_FMT " doesn't have a 'button.Char' to setText on!"))
										{
											textField.SetText(info.m_Text);
										}
										else if(bWideTextButton &&
												uiVerifyf(imageInfo.m_movieClip.GetMember("button", &button),  KEY_WIDE_IMG_PREFIX_FMT " doesn't have a 'button' to setText on!") &&
												uiVerifyf(button.GetMember("Char", &textField), KEY_WIDE_IMG_PREFIX_FMT " doesn't have a 'button.Char' to setText on!"))
										{
											textField.SetText(info.m_Text);
										}
									}
								}
							}
						}
#endif // RSG_PC

						tags.PushAndGrow(imageInfo);
					}
				}
			}
			char cHtmlFormattedHelpTextString[MAX_CHARS_FOR_TEXT_STRING];
			CTextConversion::TextToHtml(textBuffer, cHtmlFormattedHelpTextString, MAX_CHARS_FOR_TEXT_STRING);
			textMovie.SetTextHTML(cHtmlFormattedHelpTextString);

			// Add all of the images where they belong in the string
			for(int i=0; i<tags.size(); ++i)
			{
				GFxValue& blipMovie = tags[i].m_movieClip;

				if(uiVerifyf(!blipMovie.IsUndefined(), "Didn't attach a movie!?"))
				{
					const float movieClipTextIconYOffset = 0.1f;
					const float movieClipTextIconScale = 6.75f;

					float iconScale = fontScale * movieClipTextIconScale;
					float xPos = tags[i].m_x + textMovieX;
					GFxValue::DisplayInfo displayInfo(xPos, tags[i].m_y + textMovieY + fontScale*movieClipTextIconYOffset);
				
					displayInfo.SetScale(iconScale, iconScale);
					blipMovie.SetDisplayInfo(displayInfo);

					SetMovieColor(blipMovie, tags[i].m_color);
				}
				// Set Texture here
			}

			return true;
		}
	}

	return false;
}

bool CMovieClipText::ConvertTag(const char* pTag, ParserResults& parserResults, u8 slot, bool bAllowFallback)
{
	if(!uiVerifyf(pTag, "SET_FORMATTED_TEXT_WITH_ICONS - pTag=NULL"))
	{
		return false;
	}
	const int maxResultStrLen = sizeof(parserResults.m_convertedTagNameBuffer);

	const char* pBlip = CTextFormat::GetRadarBlipFromToken(pTag);
	// If the string starts with BLIP_, then convert the name to get the blip texture.
	if(pBlip)
	{
		int imageIndex = 0;
		if(sscanf(pBlip, "%d", &imageIndex) == 1)
		{
			const char* pBlipName = CMiniMap_Common::GetBlipName(imageIndex);
			if(pBlipName)
			{
				strncpy(parserResults.m_convertedTagNameBuffer, pBlipName, maxResultStrLen);
				uiDebugf3("SET_FORMATTED_TEXT_WITH_ICONS - found blip(%s)", parserResults.m_convertedTagNameBuffer);
				return true;
			}
		}
		else
		{
			formatf(parserResults.m_convertedTagNameBuffer, maxResultStrLen, "RADAR_%s", pBlip);
			return true;
		}
	}
	else if ( NOTFINAL_ONLY(!PARAM_nouseNewButtonTextHud.Get() && ) !::strnicmp(pTag, INPUT_KEY, INPUT_KEY_LEN) )
	{
		bool bSuccess = false;
		if(!::strnicmp(pTag, INPUTGROUP_KEY, INPUTGROUP_KEY_LEN))
		{
			IconParams params			= CTextFormat::DEFAULT_ICON_PARAMS;
			params.m_AllowXOSwap		= false;
			params.m_CorrectButtonOrder	= false;

			bSuccess =  CTextFormat::GetInputGroupButtons(pTag, parserResults.m_convertedTagNameBuffer, maxResultStrLen, parserResults.m_iInputIDs, MAX_ICON_LENGTH, params);
		}
		else
		{
			IconParams params			= CTextFormat::DEFAULT_ICON_PARAMS;
			params.m_AllowXOSwap		= false;
			params.m_DeviceId			= ioSource::IOMD_ANY;
			params.m_MappingSlot		= slot;
			params.m_AllowIconFallback	= bAllowFallback;
			params.m_CorrectButtonOrder	= false;

#if KEYBOARD_MOUSE_SUPPORT
			char buffer[512] = {0};
			safecpy(buffer, pTag);
			char* pPos = strchr(buffer, ':');

			// If there is a ':' then what follows is telling us the icon index.
			if(pPos != NULL)
			{
				*pPos = 0;
				++pPos;
				params.m_MappingSlot		= u8(atoi(pPos));
				params.m_AllowIconFallback	= false;
			}

			// HACK-OF-SORTS: Currently, when we do not allow fallbacks we are on the key mapping screen. We assume the icons we are looking for are only the keyborad/mouse icons!
			if(!params.m_AllowIconFallback)
			{
				params.m_DeviceId = ioSource::IOMD_KEYBOARD_MOUSE;
			}

			bSuccess =  CTextFormat::GetInputButtons(buffer, parserResults.m_convertedTagNameBuffer, maxResultStrLen, parserResults.m_iInputIDs, MAX_ICON_LENGTH, params );
#else
			bSuccess =  CTextFormat::GetInputButtons(pTag, parserResults.m_convertedTagNameBuffer, maxResultStrLen, parserResults.m_iInputIDs, MAX_ICON_LENGTH, params );
#endif // KEYBOARD_MOUSE_SUPPORT
		}

		FillPCButtonInformation(parserResults);
		parserResults.m_bBlip = false;
		return bSuccess;
	}
	else if(!::strnicmp(pTag, HUD_COLOR_ABBREVIATED_KEY, HUD_COLOR_ABBREVIATED_KEY_LEN))
	{	//	Copied from CTextFormat::GetColourFromToken
		if (strlen(pTag) > HUD_COLOR_ABBREVIATED_KEY_LEN)
		{
			u32 StringAsInteger = 0;
			if (CTextFormat::IsUnsignedInteger(&pTag[HUD_COLOR_ABBREVIATED_KEY_LEN], StringAsInteger))
			{
				m_CurrentColor = static_cast<eHUD_COLOURS>(StringAsInteger);
				return false;// leave the key in the text, so other systems can handle it.
			}
		}
	}
	else if(!::strnicmp(pTag, HUD_COLOR_KEY, HUD_COLOR_KEY_LEN))
	{
		m_CurrentColor = CHudColour::GetColourFromKey(pTag);
		return false;// leave the key in the text, so other systems can handle it.
	}
	else if(!::stricmp(pTag, DEFAULT_COLOR_KEY))
	{
		m_CurrentColor = DEFAULT_COLOR;
		return false;// leave the key in the text, so other systems can handle it.
	}
	else if(!::stricmp(pTag, RSTICK_ROTATE))
	{
		s32 icon = FONT_CHAR_TOKEN_ID_CONTROLLER_RSTICK_ROTATE - FONT_CHAR_TOKEN_ID_CONTROLLER_UP;
		formatf(parserResults.m_convertedTagNameBuffer, maxResultStrLen, PAD_IMG_PREFIX_FMT, icon);
		return true;
	}
	else if(!::stricmp(pTag, LSTICK_ROTATE))
	{
		s32 icon = FONT_CHAR_TOKEN_ID_CONTROLLER_LSTICK_ROTATE - FONT_CHAR_TOKEN_ID_CONTROLLER_UP;
		formatf(parserResults.m_convertedTagNameBuffer, maxResultStrLen, PAD_IMG_PREFIX_FMT, icon);
		return true;
	}

	return false;
}


void CMovieClipText::SetMovieColor(GFxValue& movie, eHUD_COLOURS color)
{
	const Color32& rColor = CHudColour::GetRGBA(color);
	double dAlpha = static_cast<double>(rColor.GetAlphaf()) * 100.0;
	movie.SetColorTransform(rColor);

	GFxValue::DisplayInfo newDisplayInfo;
	movie.GetDisplayInfo(&newDisplayInfo);

	newDisplayInfo.SetAlpha(dAlpha);
	movie.SetDisplayInfo(newDisplayInfo);
}

void CMovieClipText::FillPCButtonInformation(ParserResults& parseResults)
{
	int iNumButtons = 1;

	const char* BUTTON_DELIMITER = KEY_FMT_SEPERATOR;
	for(int i = 0; i < MAX_TOKEN_LEN; ++i)
	{
		if(i == 0)
		{
			parseResults.m_Prefix[0] = parseResults.m_convertedTagNameBuffer[i];
		}

		if(parseResults.m_convertedTagNameBuffer[i] == BUTTON_DELIMITER[0])
		{
			parseResults.m_Prefix[iNumButtons] = parseResults.m_convertedTagNameBuffer[i+1];
			iNumButtons++;
		}

		if(parseResults.m_convertedTagNameBuffer[i] == '\0')
		{
			break;
		}
	}

	/* This is irrelevant as we now read the width of the actual icon and use that for math.
	for(int i = 0; i < iNumButtons; ++i)
	{
		int iInputVal = parseResults.m_iInputIDs[i];
		if(iInputVal > 0)
		{
			if(iInputVal > 999)
			{
				switch(iInputVal)
				{
				case 1002:
				case 1003:
				case 1022:
				case 1023:
				case 1024:
				case 1044:
					parseResults.m_iNumOfRequiredSpaces[i] = 1;
					break;
				default:
					parseResults.m_iNumOfRequiredSpaces[i] = 2;
				}
			}
			else
				parseResults.m_iNumOfRequiredSpaces[i] = 1;
		}
	}
	*/
}

void CMovieClipText::ConvertToHTMLImageString(char* rawImgString, int iID, bool bTextButton, bool bWideTextButton)
{
	if(bTextButton)
	{
		sprintf(rawImgString, KEY_IMG_PREFIX_FMT);
	}
	else if(bWideTextButton)
	{
		sprintf(rawImgString, KEY_WIDE_IMG_PREFIX_FMT);
	}
	else
	{
		sprintf(rawImgString, PAD_IMG_PREFIX_FMT, iID);
	}
}

// eof
