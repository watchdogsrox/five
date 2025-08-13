/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : Credits.cpp
// PURPOSE : manages the processing of end-game-credits
//			 gets the data from a file in the data directory
// AUTHOR  : Derek Payne
// STARTED : 14/06/2006
//
/////////////////////////////////////////////////////////////////////////////////

// Rage headers
#include "grcore/allocscope.h"
#include "parser/manager.h"
#include "fwsys/fileExts.h"
#include "fwscene/stores/psostore.h"

#if RSG_PC
#include "rline/rlpc.h"
#endif // RSG_PC

// game:
#include "camera/viewports/Viewport.h"
#include "camera/viewports/ViewportManager.h"
#include "core/game.h"
#include "frontend\Credits.h"
#include "frontend\Credits_parser.h"
#include "frontend/VideoEditor/ui/InstructionalButtons.h"
#include "frontend\NewHud.h"
#include "frontend\Hud_Colour.h"
#include "renderer/sprite2d.h"
#include "streaming/streaming.h"
#include "system\ControlMgr.h"
#include "Text\Text.h"

#include "fwsys/timer.h"

// bank:
#include "bank\bank.h"
#include "bank\bkmgr.h"

#define CREDITS_FASTER 1
#define CREDITS_SLOWER -1

#define SPACE_SCALER_SLOWEST (0.0f)
#define SPACE_SCALER (0.0000856f)
#define SPACE_SCALER_FASTEST (0.000642f)
#define SPACE_SCALER_INTERVAL (0.0000214f)

#define MARGIN				(0.15f)
#define NUMBER_OF_LINES_DOWN_TO_PROCESS	(60)  // we need to cater for all the text on-screen and enough "down" when final text has scrolled off the toip of the screen

static sysCriticalSectionToken s_creditsCritSecToken;

static const strStreamingObjectName	CREDITS_TXD_PATH("platform:/textures/credits", 0xdc6683fa);

bool	CCredits::bCreditsRunning;
bool	CCredits::bShutdownPending = false;
u32	CCredits::CreditsStartTime;
float	CCredits::PixelsScrolled;
bool CCredits::sm_bReachedEnd = false;
bool CCredits::sm_bUsingLimitedFont = false;
CCreditArray CCredits::m_CreditArray;

bool CCredits::sm_launchedFromPauseMenu = false;
float CCredits::sm_currentSpeed = SPACE_SCALER;

CSprite2d CCredits::sm_Sprite;
strLocalIndex CCredits::sm_iTxdId = strLocalIndex(-1);
grcBlendStateHandle CCredits::ms_StandardSpriteBlendStateHandle = grcStateBlock::BS_Invalid;

bool	CCredits::bScriptWantsCreditsBeforeFade;

u32 CCredits::iStartLineRenderedThisFrame = 0;
float CCredits::fStartPixelsDown;

bool CCredits::sm_usingCloudCredits = false;
CCreditsDownloadHelper* CCredits::sm_creditsDownloadHelper;

#if __BANK
s32	CCredits::sm_iDebugPercentage = 0;
#endif

CTextLayout CCredits::CreditTextLayout;

CCreditsDownloadHelper::CCreditsDownloadHelper(CCreditArray& creditsArray)
	: m_creditsArray(creditsArray)
{
	Clear();
}

CCreditsDownloadHelper::~CCreditsDownloadHelper()
{
	Clear();
}

CloudRequestID CCreditsDownloadHelper::StartDownload(const char* contentName, unsigned nCloudRequestFlags)
{
	Clear();
	SetupParserConversionDictionary();
	m_fileRequestId = UserContentManager::GetInstance().RequestCdnContentData(contentName, nCloudRequestFlags);

	if (m_fileRequestId == INVALID_CLOUD_REQUEST_ID)
	{
		Clear();
	}

	return m_fileRequestId;
}

void CCreditsDownloadHelper::Clear()
{
	m_fileRequestId = INVALID_CLOUD_REQUEST_ID;
	m_resultCode = RLSC_ERROR_NONE;
	m_hasError = false;
	m_downloadSuccess = false;
	m_fileParseSuccess = false;
	m_useLocal = false;
	m_creditsArray.CreditItems.Reset();

	m_parserConversionMap.Reset();
}

void CCreditsDownloadHelper::OnCloudEvent(const CloudEvent* pEvent)
{
	switch (pEvent->GetType())
	{
	case CloudEvent::EVENT_REQUEST_FINISHED:
	{
		// grab event data
		const CloudEvent::sRequestFinishedEvent* pEventData = pEvent->GetRequestFinishedData();

		// check if we're interested
		if (pEventData->nRequestID != m_fileRequestId)
		{
			return;
		}

		m_downloadSuccess = pEventData->bDidSucceed;
		m_resultCode = pEventData->nResultCode;
		m_fileRequestId = INVALID_CLOUD_REQUEST_ID;

		if (m_downloadSuccess)
		{
			gnetDisplay("CCreditsDownloadHelper::OnCloudEvent - Download Success - %s", m_filename.c_str());

			char bufferFName[RAGE_MAX_PATH];
			fiDevice::MakeMemoryFileName(bufferFName, sizeof(bufferFName), pEventData->pData, pEventData->nDataSize, false, m_filename.c_str());

			m_fileParseSuccess = ParseFile(bufferFName);
			if (!m_fileParseSuccess)
			{
				gnetDisplay("CCreditsDownloadHelper::OnCloudEvent - Parsing failed, marking as 404 - %s", m_filename.c_str());
				m_resultCode = NET_HTTPSTATUS_NOT_FOUND;
				m_hasError = true;
				m_useLocal = true;
			}
			else
			{
				CVideoEditorInstructionalButtons::Refresh();
			}
		}
		else
		{
			// We give precedence to the cloud version but if the download fails we'll then fall back to the local version.
			gnetDisplay("CCreditsDownloadHelper::OnCloudEvent - Download Failed - %s, resultCode=%d", m_filename.c_str(), m_resultCode);

			m_hasError = true;
			m_useLocal = true;
		}
	}
	default:
		break;
	}
}

bool CCreditsDownloadHelper::ParseFile(const char* pFilename)
{
	bool fileParseSuccess = false;
	INIT_PARSER;
	{
		parTree* pDataXML = PARSER.LoadTree(pFilename, "");

		if (pDataXML)
		{
			const parTreeNode* pRootNode = pDataXML->GetRoot();
			if (pRootNode)
			{
				const parTreeNode* pArrayNode = pRootNode->GetChild();
				parTreeNode::ChildNodeIterator pChildrenStart = pArrayNode->BeginChildren();
				parTreeNode::ChildNodeIterator pChildrenEnd = pArrayNode->EndChildren();

				int creditCount = 0;
				for (parTreeNode::ChildNodeIterator ci = pChildrenStart; ci != pChildrenEnd; ++ci)
				{
					creditCount++;
				}

				m_creditsArray.CreditItems.Reset();
				m_creditsArray.CreditItems.Reserve(creditCount);

				for (parTreeNode::ChildNodeIterator ci = pChildrenStart; ci != pChildrenEnd; ++ci)
				{
					parTreeNode* curNode = *ci;
					CCreditItem curCredit;

					atHashString lineType = curNode->FindChildWithIndex(0)->GetData();
					curCredit.LineType = GetLineType(lineType.GetHash());

					const parTreeNode* c_node1 = curNode->FindChildWithIndex(1);
					const parTreeNode* c_node2 = curNode->FindChildWithIndex(2);
					const parTreeNode* c_node3 = curNode->FindChildWithIndex(3);

					const char* line1 = c_node1 ? c_node1->GetData() : nullptr;
					const char* line2 = c_node2 ? c_node2->GetData() : nullptr;
					const char* line3 = c_node3 ? c_node3->GetData() : nullptr;

					if (line1)
					{
						curCredit.cTextId1.Set(line1, (int)strlen(line1), 0);
					}

					if (line2)
					{
						curCredit.cTextId2.Set(line2, (int)strlen(line2), 0);
					}
					
					if (line3)
					{
						curCredit.cTextId3.Set(line3, (int)strlen(line3), 0);
					}

					const parTreeNode* c_literalString = curNode->FindChildWithIndex(4);
					curCredit.bLiteral = c_literalString ? c_literalString->GetElement().FindAttributeBoolValue("value", false) : false;

					m_creditsArray.CreditItems.PushAndGrow(curCredit);
				}
			}

			fileParseSuccess = m_creditsArray.CreditItems.GetCount() > 0;
		}

		delete pDataXML;
	}
	SHUTDOWN_PARSER;

	return fileParseSuccess;
}

eCreditType CCreditsDownloadHelper::GetLineType(u32 creditHash)
{
	eCreditType returnType = JOB_BIG;
	eCreditType* lookupVal = m_parserConversionMap.Access(creditHash);
	if (lookupVal)
	{
		returnType = *lookupVal;
	}

	return returnType;
}

void CCreditsDownloadHelper::SetupParserConversionDictionary()
{
	m_parserConversionMap.Insert(0x969D857B, JOB_BIG);
	m_parserConversionMap.Insert(0x2C3E17BC, JOB_MED);
	m_parserConversionMap.Insert(0x9699C7AD, JOB_SMALL);
	m_parserConversionMap.Insert(0x7EB04BDA, NAME_BIG);
	m_parserConversionMap.Insert(0xE6A96DB1, NAME_MED);
	m_parserConversionMap.Insert(0x643F3E6D, NAME_SMALL);
	m_parserConversionMap.Insert(0xD55FBCCA, SPACE_BIG);
	m_parserConversionMap.Insert(0x52D63917, SPACE_MED);
	m_parserConversionMap.Insert(0xAC816538, SPACE_SMALL);
	m_parserConversionMap.Insert(0x3B486986, SPACE_END);
	m_parserConversionMap.Insert(0xF35FD8B0, SPRITE_1);
	m_parserConversionMap.Insert(0x7A76BDC, LEGALS);
	m_parserConversionMap.Insert(0x9292BB6D, AUDIO_NAME);
	m_parserConversionMap.Insert(0x669E929, AUDIO_LEGALS);
	m_parserConversionMap.Insert(0xE9223F47, JOB_AND_NAME_MED);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CCredits::Init()
// PURPOSE:	main init function
/////////////////////////////////////////////////////////////////////////////////////
void CCredits::Init(unsigned initMode)
{
	if (initMode == INIT_AFTER_MAP_LOADED)
	{
		Assertf(!bCreditsRunning, "Credits are already running!");

		sm_iTxdId = -1;

		sm_creditsDownloadHelper = rage_new CCreditsDownloadHelper(m_CreditArray);
		sm_usingCloudCredits = false;
		sm_currentSpeed = SPACE_SCALER;

		Load();

		// some languages require a little extra spacing 1535969
		switch (CPauseMenu::GetMenuPreference(PREF_CURRENT_LANGUAGE))
		{
		case LANGUAGE_RUSSIAN:
		case LANGUAGE_CHINESE_TRADITIONAL:
		case LANGUAGE_CHINESE_SIMPLIFIED:
		case LANGUAGE_KOREAN:
		case LANGUAGE_JAPANESE:
			sm_bUsingLimitedFont = true;
			break;
		default:
			sm_bUsingLimitedFont = false;
		}

		sm_bReachedEnd = false;
		bCreditsRunning = true;
		bShutdownPending = false;
		CreditsStartTime = fwTimer::GetSystemTimeInMilliseconds();
		PixelsScrolled = 0.0f;

		iStartLineRenderedThisFrame = 0;
		fStartPixelsDown = 0.0f;

		bScriptWantsCreditsBeforeFade = false;  // reset the flag so that credits appear over the fades until Script says otherwise (when skipped)

		CreditTextLayout.Default();

		CreditTextLayout.SetDropShadow(true);
		CreditTextLayout.SetAdjustForNonWidescreen(true);

		if (sm_launchedFromPauseMenu)
		{
			CVideoEditorInstructionalButtons::Init();
			CVideoEditorInstructionalButtons::Refresh();
		}

		if (sm_bUsingLimitedFont)
		{
			CreditTextLayout.SetLeading(2);
		}

		// stateblocks
		grcBlendStateDesc bsd;

		//	The remaining BlendStates should all have these two flags set
		bsd.BlendRTDesc[0].BlendEnable = true;
		bsd.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
		bsd.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
		bsd.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
		bsd.BlendRTDesc[0].SrcBlendAlpha = grcRSV::BLEND_SRCALPHA;
		bsd.BlendRTDesc[0].DestBlendAlpha = grcRSV::BLEND_INVSRCALPHA;
		bsd.BlendRTDesc[0].BlendOpAlpha = grcRSV::BLENDOP_ADD;
		bsd.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
		ms_StandardSpriteBlendStateHandle = grcStateBlock::CreateBlendState(bsd);

#if __BANK
		sm_iDebugPercentage = 0;
#endif // __BANK
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CCredits::Shutdown()
// PURPOSE:	shuts down the credit items
/////////////////////////////////////////////////////////////////////////////////////
void CCredits::Shutdown(unsigned shutdownMode)
{
	SYS_CS_SYNC(s_creditsCritSecToken);

	if (shutdownMode == SHUTDOWN_WITH_MAP_LOADED || shutdownMode == SHUTDOWN_CORE || shutdownMode == SHUTDOWN_SESSION)
	{
		if (sm_creditsDownloadHelper != nullptr)
		{
			sm_usingCloudCredits = false;
			sm_creditsDownloadHelper->Clear();

			delete sm_creditsDownloadHelper;
			sm_creditsDownloadHelper = nullptr;
		}

		bCreditsRunning = false;

		m_CreditArray.CreditItems.Reset();

		if (sm_Sprite.GetTexture())
		{
			sm_Sprite.Delete();

			if (sm_iTxdId != -1)
			{
				g_TxdStore.RemoveRef(sm_iTxdId, REF_OTHER);
			}
		}

		CVideoEditorInstructionalButtons::Shutdown();
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CCredits::Load()
// PURPOSE: loads from the credits.dat data file
/////////////////////////////////////////////////////////////////////////////////////
void CCredits::Load()
{
	const char* c_creditsFileName = "Credits";
	if (sm_launchedFromPauseMenu)
	{
		if (sm_creditsDownloadHelper->StartDownload(c_creditsFileName) != INVALID_CLOUD_REQUEST_ID)
		{
			sm_usingCloudCredits = true;
		}
		else
		{
			// LoadLocalCreditsFile();

			// To avoid potential issues we're just going to bail for now.
			// Please contact Alex Hadjadj for further questions.
			bShutdownPending = true;
		}
	}
	else
	{
		LoadLocalCreditsFile();
	}

	Assertf(sm_iTxdId == -1, "Credit sprite already active");

	sm_iTxdId = g_TxdStore.FindSlot(CREDITS_TXD_PATH);
	if (sm_iTxdId != -1)
	{
		g_TxdStore.StreamingRequest(sm_iTxdId, (STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE));
		CStreaming::SetDoNotDefrag(sm_iTxdId, g_TxdStore.GetStreamingModuleId());
	}
}

void CCredits::LoadLocalCreditsFile()
{
	sm_usingCloudCredits = false;
	fwPsoStoreLoader::LoadDataIntoObject("platform:/data/credits", META_FILE_EXT, CCredits::m_CreditArray);
	Assertf(m_CreditArray.CreditItems.GetCount() != 0, "No credit items to display!");
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CCredits::Update()
// PURPOSE: checks for quitting the credits
/////////////////////////////////////////////////////////////////////////////////////
void CCredits::Update()
{
	if (!bCreditsRunning)
		return;

	SYS_CS_SYNC(s_creditsCritSecToken);
	if (bShutdownPending)
	{
		Shutdown(SHUTDOWN_WITH_MAP_LOADED);
	}

	if (sm_launchedFromPauseMenu)
	{
		CVideoEditorInstructionalButtons::Update();
	}

	if (sm_usingCloudCredits && sm_creditsDownloadHelper->UseLocalFile())
	{
		// Use local files if we timeout, error, or fail to prase the cloud XML
		// LoadLocalCreditsFile();

		// To avoid potential issues we're just going to bail for now.
		// Please contact Alex Hadjadj for further questions.
		bShutdownPending = true;
	}

	//
	// check and setup the credits texture
	//
	if ((sm_iTxdId != -1) && (g_TxdStore.HasObjectLoaded(sm_iTxdId)))
	{
		if (!sm_Sprite.GetTexture())
		{
			g_TxdStore.ClearRequiredFlag(sm_iTxdId.Get(), STRFLAG_DONTDELETE);

			g_TxdStore.AddRef(sm_iTxdId, REF_OTHER);

			g_TxdStore.PushCurrentTxd();
			g_TxdStore.SetCurrentTxd(sm_iTxdId);
			if (Verifyf(fwTxd::GetCurrent(), "Current Texture Dictionary (id %u) is NULL ", sm_iTxdId.Get()))
			{
				sm_Sprite.SetTexture("Credit_Logos");
			}

			g_TxdStore.PopCurrentTxd();
		}
	}
}

void CCredits::UpdateInput()
{
	if (sm_launchedFromPauseMenu)
	{
#if !RSG_PC
		if (CControlMgr::GetPlayerPad() && !CControlMgr::GetPlayerPad()->IsConnected(PPU_ONLY(true)))
		{
			CWarningScreen::SetControllerReconnectMessageWithHeader(WARNING_MESSAGE_STANDARD, "SG_HDNG", "NO_PAD", FE_WARNING_NONE);
			return;
		}
#endif

		if (CPauseMenu::CheckInput(FRONTEND_INPUT_LEFT))
		{
			AdjustCreditScrollSpeed(CREDITS_SLOWER);
		}
		else if (CPauseMenu::CheckInput(FRONTEND_INPUT_RIGHT))
		{
			AdjustCreditScrollSpeed(CREDITS_FASTER);
		}
		else if (CPauseMenu::CheckInput(FRONTEND_INPUT_BACK))
		{
			bShutdownPending = true;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CCredits::Render()
// PURPOSE: renders the credits
/////////////////////////////////////////////////////////////////////////////////////
void CCredits::Render(bool bBeforeFade)
{
	SYS_CS_SYNC(s_creditsCritSecToken);

	if (!bCreditsRunning || bShutdownPending)
		return;

#if RSG_PC
	if (g_rlPc.IsUiShowing())
		return;
#endif // RSG_PC

	if (bBeforeFade != bScriptWantsCreditsBeforeFade)
		return;

	if (sm_usingCloudCredits)
	{
		const bool c_downloadingCredits = sm_creditsDownloadHelper->IsDownloading();
		const bool c_fileParsed = sm_creditsDownloadHelper->IsFileParseSuccessful();
		const bool c_hasError = sm_creditsDownloadHelper->HasError();

		if ((c_downloadingCredits || !c_fileParsed) && !c_hasError)
		{
			// Still downloading/processing, just wait.
			CVideoEditorInstructionalButtons::Render();
			return;
		}
	}

	float PixelsDown = 0;

	PixelsDown += fStartPixelsDown;  // apply the pixels down from what we rendered last frame

	if (!fwTimer::IsGamePaused() ||
		sm_launchedFromPauseMenu)
	{
		float offset = SPACE_SCALER;

		if (sm_launchedFromPauseMenu)
		{
			offset = sm_currentSpeed;
		}

		PixelsScrolled += fwTimer::GetTimeStepInMilliseconds() * offset;
	}

	CreditTextLayout.SetWrap(GetWrapLeftRight());
	CreditTextLayout.SetOrientation(FONT_CENTRE);

	bool bCheckForStartLine = true;

	int iEndLineRenderedThisFrame = (iStartLineRenderedThisFrame + NUMBER_OF_LINES_DOWN_TO_PROCESS);

	// go through each credit item and display it
	for (u32 i = iStartLineRenderedThisFrame; i < iEndLineRenderedThisFrame; i++)
	{
		Vector2 vSize(0, 0);
		bool bSprite = false;
		bool bSpace = false;
		bool bTranslatedText = false;

		if (i < m_CreditArray.CreditItems.GetCount())
		{
			const bool c_literal = m_CreditArray.CreditItems[i].bLiteral;
			switch (m_CreditArray.CreditItems[i].LineType)
			{
			case JOB_BIG:
			{
				vSize = Vector2(0.600f, 0.795f);
				CreditTextLayout.SetColor(CHudColour::GetRGBA(HUD_COLOUR_WHITE));
				CreditTextLayout.SetStyle(FONT_STYLE_STANDARD);
				bTranslatedText = !c_literal;
				break;
			}

			case JOB_MED:
			{
				vSize = Vector2(0.568f, 0.69f);  // made this one a bit smaller, and we will add the difference as extra padding after the render below
				CreditTextLayout.SetColor(CHudColour::GetRGBA(HUD_COLOUR_WHITE));
				CreditTextLayout.SetStyle(FONT_STYLE_CURSIVE);
				bTranslatedText = !c_literal;
				break;
			}

			case JOB_SMALL:
			{
				vSize = Vector2(0.418f, 0.595f);
				CreditTextLayout.SetColor(CHudColour::GetRGBA(HUD_COLOUR_WHITE));
				CreditTextLayout.SetStyle(FONT_STYLE_STANDARD);
				bTranslatedText = !c_literal;
				break;
			}

			case NAME_BIG:
			{
				vSize = Vector2(0.478f, 0.575f);
				CreditTextLayout.SetColor(CHudColour::GetRGBA(HUD_COLOUR_WHITE));
				CreditTextLayout.SetStyle(FONT_STYLE_CONDENSED);
				break;
			}

			case NAME_MED:
			{
				vSize = Vector2(0.458f, 0.555f);
				CreditTextLayout.SetColor(CHudColour::GetRGBA(HUD_COLOUR_WHITE));
				CreditTextLayout.SetStyle(FONT_STYLE_CONDENSED);
				break;
			}

			case NAME_SMALL:
			{
				vSize = Vector2(0.418f, 0.595f);
				CreditTextLayout.SetColor(CHudColour::GetRGBA(HUD_COLOUR_WHITE));
				CreditTextLayout.SetStyle(FONT_STYLE_CONDENSED);
				break;
			}

			case LEGALS:
			{
				vSize = Vector2(0.458f, 0.555f);
				CreditTextLayout.SetColor(CHudColour::GetRGBA(HUD_COLOUR_WHITE));

				if (!sm_bUsingLimitedFont)
				{
					CreditTextLayout.SetStyle(FONT_STYLE_CONDENSED);
				}
				else
				{
					CreditTextLayout.SetStyle(FONT_STYLE_STANDARD);
				}
				bTranslatedText = !c_literal;
				break;
			}

			case AUDIO_NAME:
			{
				vSize = Vector2(0.2f, 0.54f);
				CreditTextLayout.SetColor(CHudColour::GetRGBA(HUD_COLOUR_WHITE));
				CreditTextLayout.SetStyle(FONT_STYLE_CONDENSED);
				break;
			}

			case AUDIO_LEGALS:
			{
				vSize = Vector2(0.2f, 0.42f);
				CreditTextLayout.SetColor(CHudColour::GetRGBA(HUD_COLOUR_WHITE));
				CreditTextLayout.SetStyle(FONT_STYLE_STANDARD);
				bTranslatedText = !c_literal;
				break;
			}

			case JOB_AND_NAME_MED:
			{
				// the size, font & whether translated will get set for this one later on... as we are in submission & this new feature wants added so this is the quickest way to add it without too much rework
				CreditTextLayout.SetColor(CHudColour::GetRGBA(HUD_COLOUR_WHITE));
				break;
			}

			case SPACE_BIG:
			{
				bSpace = true;
				vSize = Vector2(0.0f, 0.15f);
				break;
			}

			case SPACE_MED:
			{
				bSpace = true;
				vSize = Vector2(0.0f, 0.10f);
				break;
			}

			case SPACE_SMALL:
			{
				bSpace = true;
				vSize = Vector2(0.0f, 0.05f);
				break;
			}

			case SPACE_END:
			{
				bSpace = true;
				vSize = Vector2(0.0f, 1.0f);  // very large amount so everything is scrolled off the screen
				break;
			}

			case SPRITE_1:
			{
				bSpace = true;
				bSprite = true;
				vSize = Vector2(0.8f, 0.266f);
				break;
			}

			default:
			{
				Assertf(0, "Invalid Credit line type");
			}
			}
		}
		else
		{
			sm_bReachedEnd = true;
			bSpace = true;
			vSize = Vector2(0.0f, 2.0f);  // very large amount so everything is scrolled off the screen
		}

		CHudTools::AdjustForWidescreen(WIDESCREEN_FORMAT_SIZE_ONLY, NULL, &vSize, NULL);

		if (!bSprite)
		{
			if (!bSpace)
			{
				bool bPrintedThisLineThisFrame = false;

				const char* pTextToDisplay = NULL;
				
				if (m_CreditArray.CreditItems[i].cTextId2.GetLength() > 0)
				{
					// 2 items so have one left justified, one right:
					Vector2 vExtreems;
					float fDivisor = 1.0f;
#if SUPPORT_MULTI_MONITOR
					if (GRCDEVICE.GetMonitorConfig().isMultihead())
						fDivisor = 3.0f;
#endif // SUPPORT_MULTI_MONITOR
					if (gVpMan.IsUsersMonitorWideScreen())  // bring these in a little to fix 1589960
					{
						Vector2 vWrap = GetWrapLeftRight();
						float fOffset = 0.17f;
						fOffset /= fDivisor;
						vExtreems = Vector2(vWrap.x + fOffset, vWrap.y - fOffset);
					}
					else
					{
						Vector2 vWrap = GetWrapLeftRight();
						float fOffset = 0.12f;
						fOffset /= fDivisor;
						vExtreems = Vector2(vWrap.x + fOffset, vWrap.y - fOffset);
					}

					CreditTextLayout.SetWrap(&vExtreems);

					bool hasCenterColumn = ((m_CreditArray.CreditItems[i].cTextId3.GetLength() > 0) && (stricmp(m_CreditArray.CreditItems[i].cTextId3.c_str(), "BLANK")));
					if (hasCenterColumn)
					{
						CreditTextLayout.SetOrientation(FONT_RIGHT);
						if (bTranslatedText)
						{
							pTextToDisplay = TheText.Get(m_CreditArray.CreditItems[i].cTextId3.c_str());
						}
						else
						{
							pTextToDisplay = m_CreditArray.CreditItems[i].cTextId3.c_str();
						}

						bPrintedThisLineThisFrame = PrintCreditText(0.0f, vSize.x, vSize.y, pTextToDisplay, PixelsDown, PixelsScrolled, false);
					}

					if ((m_CreditArray.CreditItems[i].cTextId2.GetLength() > 0) && (stricmp(m_CreditArray.CreditItems[i].cTextId2.c_str(), "BLANK")))
					{
						CreditTextLayout.SetOrientation(hasCenterColumn ? (u8)FONT_CENTRE : (u8)FONT_RIGHT);
						float posX = hasCenterColumn ? 0.5f : 0.0f;

						if (m_CreditArray.CreditItems[i].LineType == JOB_AND_NAME_MED)
						{
							vSize = Vector2(0.458f, 0.555f);
							CreditTextLayout.SetStyle(FONT_STYLE_CONDENSED);
							bTranslatedText = false;
						}

						if (bTranslatedText)
						{
							pTextToDisplay = TheText.Get(m_CreditArray.CreditItems[i].cTextId2.c_str());
						}
						else
						{
							pTextToDisplay = m_CreditArray.CreditItems[i].cTextId2.c_str();
						}
						
						bPrintedThisLineThisFrame = PrintCreditText(posX, vSize.x, vSize.y, pTextToDisplay, PixelsDown, PixelsScrolled, false);
					}

					if ((m_CreditArray.CreditItems[i].cTextId1.GetLength() > 0) && (stricmp(m_CreditArray.CreditItems[i].cTextId1.c_str(), "BLANK")))
					{
						CreditTextLayout.SetOrientation(FONT_LEFT);

						if (m_CreditArray.CreditItems[i].LineType == JOB_AND_NAME_MED)
						{
							vSize = Vector2(0.568f, 0.69f);  // made this one a bit smaller, and we will add the difference as extra padding after the render below
							CreditTextLayout.SetStyle(FONT_STYLE_CURSIVE);

							const bool c_literal = m_CreditArray.CreditItems[i].bLiteral;
							bTranslatedText = !c_literal;
						}

						if (bTranslatedText)
						{
							pTextToDisplay = TheText.Get(m_CreditArray.CreditItems[i].cTextId1.c_str());
						}
						else
						{
							pTextToDisplay = m_CreditArray.CreditItems[i].cTextId1.c_str();
						}
						bPrintedThisLineThisFrame = PrintCreditText(vExtreems.x, vSize.x, vSize.y, pTextToDisplay, PixelsDown, PixelsScrolled, true);
					}

					if (m_CreditArray.CreditItems[i].LineType == JOB_AND_NAME_MED)  // add some padding as we changed the font size
					{
						PixelsDown += 0.005f;
					}
				}
				else
				{
					if (m_CreditArray.CreditItems[i].cTextId1.GetLength() > 0)
					{
						// one item, so CENTRE it:
						CreditTextLayout.SetWrap(GetWrapLeftRight());
						CreditTextLayout.SetOrientation(FONT_CENTRE);

						if (bTranslatedText)
						{
							pTextToDisplay = TheText.Get(m_CreditArray.CreditItems[i].cTextId1.c_str());
						}
						else
						{
							pTextToDisplay = m_CreditArray.CreditItems[i].cTextId1.c_str();
						}

						bPrintedThisLineThisFrame = PrintCreditText(0.5f, vSize.x, vSize.y, pTextToDisplay, PixelsDown, PixelsScrolled, true);

						if (m_CreditArray.CreditItems[i].LineType == JOB_MED)  // add some padding as we changed the font size
						{
							PixelsDown += 0.005f;
						}
					}
				}

				// check and store the 1st line we printed this frame so we know we dont need to bother testing this one again next frame
				if (bCheckForStartLine)
				{
					if (bPrintedThisLineThisFrame)
					{
						if (iStartLineRenderedThisFrame != i)
						{
							//						Displayf("\nNEW START LINE: %d\n", i);

							iStartLineRenderedThisFrame = i + 1;  // store this credit as the 1st one printed this frame

							fStartPixelsDown = PixelsDown;
						}

						bCheckForStartLine = false;
					}
				}
			}
			else
			{
				if (PrintCreditSpace(vSize.y, PixelsDown, PixelsScrolled))
				{
					if (bCheckForStartLine)
					{
						if (iStartLineRenderedThisFrame != i)
						{
							//						Displayf("\nNEW START SPACE: %d\n", i);

							iStartLineRenderedThisFrame = i + 1;  // store this credit as the 1st one printed this frame

							fStartPixelsDown = PixelsDown;
						}

						bCheckForStartLine = false;
					}
				}
			}
		}
		else
			//
			// sprite:
			//
		{
			if (DrawSprite(vSize.x, vSize.y, PixelsDown, PixelsScrolled))
			{
				if (bCheckForStartLine)
				{
					if (iStartLineRenderedThisFrame != i)
					{
						iStartLineRenderedThisFrame = i + 1;  // store this credit as the 1st one printed this frame

						fStartPixelsDown = PixelsDown;
					}

					bCheckForStartLine = false;
				}
			}
		}
	}

	CText::Flush();

	// Do wide screen (again) to go on top of the text
	/*if (camInterface::IsWidescreen())
	{
		camInterface::DrawBordersForWideScreen();
	}*/

	// Make sure credits get switched off at the end.
	if (!bShutdownPending && ((-PixelsScrolled) + PixelsDown + 1.0f) < MARGIN)
	{
		bShutdownPending = true;
	}

	if (sm_launchedFromPauseMenu)
	{
		CVideoEditorInstructionalButtons::Render();
	}
}

void CCredits::AdjustCreditScrollSpeed(int fasterSlower)
{
	sm_currentSpeed += fasterSlower > 0 ? SPACE_SCALER_INTERVAL : -SPACE_SCALER_INTERVAL;
	sm_currentSpeed = Clamp<float>(sm_currentSpeed, SPACE_SCALER_SLOWEST, SPACE_SCALER_FASTEST);
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CCredits::PrintCreditText()
// PURPOSE: renders one line of credits
/////////////////////////////////////////////////////////////////////////////////////
bool CCredits::PrintCreditText(float PosX, float ScaleX, float ScaleY, const char* pString, float& PixelsDown, float PixelsScrolled, bool bMoveOn)
{
	bool	bRenderedThisFrame = false;
	Vector2 vOnScreenPos(PosX, MARGIN + 1.0f + PixelsDown - PixelsScrolled);
	s32	iNumLines = 1;

	CreditTextLayout.SetScale(Vector2(ScaleX, ScaleY));
#if RSG_PC
	CreditTextLayout.SetEnableMultiheadBoxWidthAdjustment(false);
#endif // RSG_PC

	if (vOnScreenPos.y > -MARGIN && vOnScreenPos.y < 1.0f + MARGIN)
	{
		CreditTextLayout.Render(&vOnScreenPos, pString);
		bRenderedThisFrame = true;
	}

	if (bMoveOn)
	{
		iNumLines = rage::Max(CreditTextLayout.GetNumberOfLines(&vOnScreenPos, pString), 1);

		float fCharHeight = CreditTextLayout.GetCharacterHeight();

		if (sm_bUsingLimitedFont)
		{
			fCharHeight += 0.004f;  // a little extra spacing
		}

		PixelsDown += (fCharHeight * iNumLines);
	}

	return bRenderedThisFrame;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CCredits::DrawSprite()
// PURPOSE: renders the sprite
/////////////////////////////////////////////////////////////////////////////////////
bool CCredits::DrawSprite(float ScaleX, float ScaleY, float& PixelsDown, float PixelsScrolled)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

		bool	bRenderedThisFrame = false;

	Vector2 vOnScreenPos(0.5f - (ScaleX * 0.5f), (MARGIN + 1.0f + PixelsDown - PixelsScrolled) - ScaleY);

	if ((vOnScreenPos.y > -ScaleY) && (vOnScreenPos.y < 1.0f + ScaleY))
	{
		if (sm_Sprite.GetTexture() && sm_Sprite.GetShader())
		{
			grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
			grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
			grcStateBlock::SetBlendState(ms_StandardSpriteBlendStateHandle);

			sm_Sprite.SetRenderState();

			Vector2& vPos = vOnScreenPos;
			Vector2 vSize = Vector2(ScaleX, ScaleY);

#if SUPPORT_MULTI_MONITOR
			if (GRCDEVICE.GetMonitorConfig().isMultihead())
			{
				const fwRect area = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor().getArea();
				vPos.x = (vPos.x / 3.0f) + area.left;
				vSize.x *= 0.33333f;
			}
#endif // SUPPORT_MULTI_MONITOR

			Vector2 v[4], uv[4];
			v[0].Set(vPos.x, vPos.y + vSize.y);
			v[1].Set(vPos.x, vPos.y);
			v[2].Set(vPos.x + vSize.x, vPos.y + vSize.y);
			v[3].Set(vPos.x + vSize.x, vPos.y);

#define __UV_OFFSET (0.002f)
			uv[0].Set(__UV_OFFSET, 1.0f - __UV_OFFSET);
			uv[1].Set(__UV_OFFSET, __UV_OFFSET);
			uv[2].Set(1.0f - __UV_OFFSET, 1.0f - __UV_OFFSET);
			uv[3].Set(1.0f - __UV_OFFSET, __UV_OFFSET);

			sm_Sprite.Draw(v[0], v[1], v[2], v[3], uv[0], uv[1], uv[2], uv[3], CRGBA(255, 255, 255, 255));
		}
	}
	else if (vOnScreenPos.y < (-ScaleY))
	{
		bRenderedThisFrame = true;
	}

	//PixelsDown += ScaleY;

	return bRenderedThisFrame;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CCredits::PrintCreditSpace()
// PURPOSE: moves down one space
/////////////////////////////////////////////////////////////////////////////////////
bool CCredits::PrintCreditSpace(float ScaleY, float& PixelsDown, float PixelsScrolled)
{
	bool bRenderedThisFrame = false;
	float	fOnScreenY = MARGIN + 1.0f + PixelsDown - PixelsScrolled;

	if (fOnScreenY > -MARGIN && fOnScreenY < 1.0f + MARGIN)
	{
		bRenderedThisFrame = true;
	}

	PixelsDown += (ScaleY);

	return bRenderedThisFrame;
}

Vector2 CCredits::GetWrapLeftRight()
{
	Vector2 vWrap(0.08f, 0.92f);
#if SUPPORT_MULTI_MONITOR
	if (GRCDEVICE.GetMonitorConfig().isMultihead())
	{
		const fwRect area = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor().getArea();
		float fLeft = area.left + 0.027f;
		float fRight = area.right - 0.027f;
		vWrap.x = fLeft;
		vWrap.y = fRight;
	}
#endif // SUPPORT_MULTI_MONITOR

	return vWrap;
}

void CCredits::UpdateInstructionalButtons()
{
	if (!sm_launchedFromPauseMenu)
	{
		return;
	}


	if (sm_usingCloudCredits)
	{
		const bool c_downloadingCredits = sm_creditsDownloadHelper->IsDownloading();
		const bool c_fileParsed = sm_creditsDownloadHelper->IsFileParseSuccessful();
		const bool c_hasError = sm_creditsDownloadHelper->HasError();

		if ((c_downloadingCredits || !c_fileParsed) && !c_hasError)
		{
			char const* const c_downloadingLabel = TheText.Get(atHashString("INSTALL_SCREEN_DOWNLOAD", 0x2A81BA7E));
			CVideoEditorInstructionalButtons::AddButton(ICON_SPINNER, c_downloadingLabel);
		}
		else 
		{
			char const* const c_backLabel = TheText.Get(atHashString("IB_BACK", 0xE5FC11CD));
			CVideoEditorInstructionalButtons::AddButton(rage::INPUT_FRONTEND_CANCEL, c_backLabel, true);

			char const* const c_fasterLabel = TheText.Get(atHashString("IB_FASTER", 0x7015C9E3));
			CVideoEditorInstructionalButtons::AddButton(rage::INPUT_FRONTEND_RIGHT, c_fasterLabel, true);

			char const* const c_slowerLabel = TheText.Get(atHashString("IB_SLOWER", 0x684A58B8));
			CVideoEditorInstructionalButtons::AddButton(rage::INPUT_FRONTEND_LEFT, c_slowerLabel, true);
		}
	}
}

//
// DEBUG CODE CAN GO HERE AT THE END:
//

#if __BANK

//
// name:		CCredits::InitWidgets
// description:	Add debug widgets
void CCredits::InitWidgets()
{
	bkBank& bank = BANKMGR.CreateBank("Credits");

	bank.AddButton("Start/End Credits", &CCredits::StartEndCreditsButton);
	bank.AddSlider("Jump to %", &sm_iDebugPercentage, 0, 100, 1, &CCredits::JumpToPosition);

}

//
// name:		CCredits::StartEndCreditsButton()
// description:	starts or finishes the credits
void CCredits::StartEndCreditsButton()
{
	// start a cutscene:

	if (!IsRunning())
	{
		Init(INIT_AFTER_MAP_LOADED);
	}
	else
	{
		Shutdown(SHUTDOWN_WITH_MAP_LOADED);
	}
}

//
// name:		CCredits::JumpToPosition()
// description:	jump to % position in credits
void CCredits::JumpToPosition()
{
	iStartLineRenderedThisFrame = (u32)(((float)m_CreditArray.CreditItems.GetCount() / 100.0f) * (float)sm_iDebugPercentage);

	Displayf("CREDITS: JUMPED TO %d%% (%d)", sm_iDebugPercentage, iStartLineRenderedThisFrame);
}

#endif //#if __BANK



// eof
