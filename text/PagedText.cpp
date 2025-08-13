//
// PagedText.cpp
//
// Copyright (C) 2012 Rockstar Games.  All Rights Reserved.
//

// rage
#include "parser/manager.h"
#include "parser/macros.h"
#include "parser/treenode.h"
#include "fwnet/netchannel.h"
#include "fwmaths/random.h"

//framework
#include "streaming/streamingengine.h"

// game
#include "Text/PagedText.h"
#include "frontend/Scaleform/ScaleFormMgr.h"

NETWORK_OPTIMISATIONS()

RAGE_DECLARE_SUBCHANNEL(net, sc)
#undef __net_channel
#define __net_channel net_sc

// we need to use a smaller size to make room for any HTML conversions.
#define MAX_PAGE_TEXT_SIZE (MAX_CHARS_FOR_TEXT_STRING - 70)
#define PREALLOCATE_POLICY_BUFFER (85*1024)
#define MAX_PAGE_SIZE (45*1024)

static const char* sCloudTextTag = "Text";

bank_u32 PagedCloudText::sm_maxAttempts = 3;
bank_s32 PagedCloudText::sm_maxTimeoutRetryDelay = 12000;
bank_s32 PagedCloudText::sm_timeoutRetryDelayVariance = 3000;

void PagedText::Reset()
{
	m_numPages=0;

	for(int i=0; i<MAX_PAGES_OF_TEXT; ++i)
	{
		m_textPages[i].Clear();
	}
}

void PagedText::ParseRawText(const char* pText, int textLen)
{
	Reset();

	if(gnetVerify(pText) && gnetVerify(textLen))
	{
		gnetDisplay("PagedText::ParseRawText - Parsing Text from cloud, length is %d characters", textLen);

		gnetAssert(m_numPages == 0);
		m_textPages[m_numPages].Preallocate((unsigned)MIN(textLen, MAX_PAGE_SIZE));

		u32 doubleNewIndex = textLen;
		u32 doubleNewLen = 0;
		FindNextDoubleSpace(pText, textLen, doubleNewIndex, doubleNewLen);

		for (u32 i = 0; i<textLen; ++i)
		{
			if (pText[i+0] == '[' && 
				i+5<textLen		&&
				pText[i+1] == 'P' && 
				pText[i+2] == 'A' && 
				pText[i+3] == 'G' && 
				pText[i+4] == 'E' &&
				pText[i+5] == ']' )
			{
				i += 5;

				++m_numPages;

				unsigned preAllocSize = MIN((textLen-i), MAX_PAGE_SIZE);
				gnetDisplay("PagedText::ParseRawText - New Page found at index %d, textLen=%d, prealloc=%d", i, textLen, preAllocSize);
				m_textPages[m_numPages].Preallocate(preAllocSize);

				continue;
			}

			if(i == doubleNewIndex)
			{
				i += doubleNewLen - 1;
				FindNextDoubleSpace(pText+i, textLen, doubleNewIndex, doubleNewLen);
				doubleNewIndex += i;

				m_textPages[m_numPages].Append("\r\n");

				continue;
			}

			if(pText[i] != '\0') // Let's not add an extra \0 at the end of the string, it just wastes memory (and asserts)
			{
				m_textPages[m_numPages].Append(pText[i]);
			}
		}

		++m_numPages;


#if !__NO_OUTPUT
		gnetDisplay("PagedText::ParseRawText - %d pages parsed.", Size());
		for(int i=0; i<Size(); ++i)
		{
			gnetDisplay("PagedText::ParseRawText - Page %d length is %d characters", i, m_textPages[i].Length());
		}
#endif
	}
}

void PagedText::FindNextDoubleSpace(const char* pText, int textLen, u32& outIndex, u32& outLen)
{
	const char* doubleNews[] = {
		"\r\n\r\n",
		"\r\n \r\n",
	};

	outIndex = textLen+1;
	outLen = 0;

	for(int i=0; i<NELEM(doubleNews); ++i)
	{
		const char* pDoubleNew = strstr(pText, doubleNews[i]);
		u32 doubleNewIndex = (u32)(pDoubleNew ? (pDoubleNew - pText) : (textLen+1));
		if(doubleNewIndex < outIndex)
		{
			outIndex = doubleNewIndex;
			outLen = (u32)strlen(doubleNews[i]);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
PagedCloudText::PagedCloudText()
{
	m_fileRequestId = INVALID_CLOUD_REQUEST_ID;
	Shutdown();
}

void PagedCloudText::Init(const atString& filename, bool fromCloud BANK_ONLY(, u8 debugForceAttemptTimeouts))
{
	m_filename = filename;
	m_retryTimer.Zero();

	if(!fromCloud)
	{
		m_filename = "platform:/data/";
		m_filename += filename;

		gnetDisplay("PagedCloudText::Init - Going to Parse the local version of %s", m_filename.c_str());
		ParseFile(m_filename.c_str());

		if(!m_didSucceed)
		{
			gnetDisplay("PagedCloudText::Init - ParseFile of local version failed for %s, will try to download from the cloud", m_filename.c_str());
		}

		m_isLocalFile = m_didSucceed;
	}

	if(!m_didSucceed)
	{
		m_filename = filename;
	}

	BANK_ONLY(m_debugForceAttemptTimeouts = debugForceAttemptTimeouts;)
}

void PagedCloudText::Shutdown()
{
	m_filename.Reset();
	m_pagedText.Reset();

	Reset();
}

void PagedCloudText::Reset()
{
	m_attempts = 0;
	m_resultCode = 0;
	m_didSucceed = false;
	m_isLocalFile = false;
	m_fileRequestId = INVALID_CLOUD_REQUEST_ID;
	m_retryTimer.Zero();

	BANK_ONLY(m_debugForceAttemptTimeouts = 0;)
}

void PagedCloudText::Update()
{
	if(m_fileRequestId == INVALID_CLOUD_REQUEST_ID &&
		m_resultCode == 0 &&
		!m_didSucceed &&
		m_filename.length() > 0 &&
		GetNumPages() == 0 &&
		m_retryTimer.IsComplete(sm_maxTimeoutRetryDelay, false))
	{
		StartDownload();
	}
}

void PagedCloudText::StartDownload()
{
	gnetDisplay("PagedCloudText::StartDownload - Starting download of %s", m_filename.c_str());

	// Not caching since it causes frame hitches, and the player should only enter this menu once.
	m_fileRequestId = CloudManager::GetInstance().RequestGetTitleFile(m_filename.c_str(), PREALLOCATE_POLICY_BUFFER, eRequest_CacheNone);
	m_retryTimer.Zero();
}

void PagedCloudText::OnCloudEvent(const CloudEvent* pEvent)
{
	switch(pEvent->GetType())
	{
	case CloudEvent::EVENT_REQUEST_FINISHED:
		{
			// grab event data
			const CloudEvent::sRequestFinishedEvent* pEventData = pEvent->GetRequestFinishedData();

			// check if we're interested
			if(pEventData->nRequestID != m_fileRequestId)
				return;

			m_didSucceed = pEventData->bDidSucceed;
			m_resultCode = pEventData->nResultCode;
			m_fileRequestId = INVALID_CLOUD_REQUEST_ID;

#if __BANK
			if(m_attempts < m_debugForceAttemptTimeouts)
			{
				m_didSucceed = false;
				m_resultCode = NET_HTTPSTATUS_REQUEST_TIMEOUT;
			}
#endif

			// did we get the file...
			if(m_didSucceed)
			{
				gnetDisplay("PagedCloudText::OnCloudEvent - Download Success - %s", m_filename.c_str());

				char bufferFName[RAGE_MAX_PATH];
				fiDevice::MakeMemoryFileName(bufferFName, sizeof(bufferFName), pEventData->pData, pEventData->nDataSize, false, m_filename.c_str());

				ParseFile(bufferFName);
				if(!m_didSucceed)
				{
					gnetDisplay("PagedCloudText::OnCloudEvent - Parsing failed, marking as 404 - %s", m_filename.c_str());
					m_resultCode = NET_HTTPSTATUS_NOT_FOUND;
				}
			}
			else if(IsTimeoutError(static_cast<netHttpStatusCodes>(m_resultCode)) && m_attempts < sm_maxAttempts)
			{
				gnetDisplay("PagedCloudText::OnCloudEvent - Download Timeout - %s", m_filename.c_str());
				++m_attempts;
				m_resultCode = 0;
				m_retryTimer.Start(fwRandom::GetRandomNumberInRange(0, sm_timeoutRetryDelayVariance));
			}
			else
			{
				gnetDisplay("PagedCloudText::OnCloudEvent - Download Failed - %s, resultCode=%d", m_filename.c_str(), m_resultCode);

				// We give precedence to the cloud version but if the download fails we'll then
				// fall back to the local version.
				// The use will see the accept-screen with the on-disk text and he has to accept it.
				// The next time he goes online again he'll have to re-download and re-accept the legals
				// if the local text had an older version.
				atString localName = m_filename;
				Init(localName, false BANK_ONLY(, m_debugForceAttemptTimeouts));

				if (m_didSucceed)
				{
					gnetDisplay("PagedCloudText::OnCloudEvent - Reverting to local file - %s. This file might have a different version number than the server.", m_filename.c_str());
				}
			}
		}
		break;

	case CloudEvent::EVENT_AVAILABILITY_CHANGED:
		break;
	};
}

void PagedCloudText::ParseFile(const char* pFilename)
{
	m_didSucceed = false;

	//Now load the text file.
	INIT_PARSER;
	{
		parTree* pDataXML = PARSER.LoadTree(pFilename, "");

		if(pDataXML)
		{
			const parTreeNode* pRootNode = pDataXML->GetRoot();
			if(gnetVerify(pRootNode))
			{
				const parTreeNode* pTextNode = pRootNode->FindChildWithName(sCloudTextTag);

				if(gnetVerify(pTextNode))
				{
					m_pagedText.ParseRawText(pTextNode->GetData(), pTextNode->GetDataSize());
					m_didSucceed = true;
				}
			}
		}

		delete pDataXML;
	}
	SHUTDOWN_PARSER;
}

bool PagedCloudText::IsTimeoutError(netHttpStatusCodes errorCode)
{
	bool isTimeout = false;

	switch(errorCode)
	{
	case NET_HTTPSTATUS_REQUEST_TIMEOUT:
	case NET_HTTPSTATUS_INTERNAL_SERVER_ERROR:
	case NET_HTTPSTATUS_SERVICE_UNAVAILABLE:
		isTimeout = true;
		break;
	default:
		break;
	}

	return isTimeout;
}

// eof