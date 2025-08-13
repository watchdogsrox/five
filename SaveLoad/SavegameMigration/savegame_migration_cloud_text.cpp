
#include "savegame_migration_cloud_text.h"
#include "savegame_migration_cloud_text_parser.h"


#if __ALLOW_EXPORT_OF_SP_SAVEGAMES

// Game headers
#include "Network/General/NetworkUtil.h"
#include "SaveLoad/savegame_channel.h"

PARAM(savegameMigrationLoadLocalTextFile, "Instead of downloading a file containing the localized text for savegame migration, load a local copy of that file");

#define CLOUD_TEXT_FILE_SIZE (40*1024)	//	Taking a guess at how big this buffer needs to be

// **********************************************************************
// * CSaveGameMigrationCloudTextData
// **********************************************************************

CSaveGameMigrationCloudTextData::CSaveGameMigrationCloudTextData()
{
	m_ArrayOfTextLines.Reset();
	m_MapOfTextKeyHashToArrayIndex.Reset();
}

bool CSaveGameMigrationCloudTextData::LoadFromFile(const char* pFilename)
{
	if (PARSER.LoadObject(pFilename, "", *this))
	{
		m_MapOfTextKeyHashToArrayIndex.Reset();
		for (u32 loop = 0; loop < m_ArrayOfTextLines.GetCount(); loop++)
		{
			m_MapOfTextKeyHashToArrayIndex.Insert(atStringHash(m_ArrayOfTextLines[loop].m_TextKey.c_str()), loop);
		}
		m_MapOfTextKeyHashToArrayIndex.FinishInsertion();

		return true;
	}

	return false;
}

const char *CSaveGameMigrationCloudTextData::SearchForText(u32 textKeyHash)
{
	u32 *pArrayIndex = m_MapOfTextKeyHashToArrayIndex.SafeGet(textKeyHash);
	if (pArrayIndex)
	{
		return m_ArrayOfTextLines[*pArrayIndex].m_LocalizedText.c_str();
	}

	return NULL;
}



// **********************************************************************
// * CSaveGameMigrationCloudText
// **********************************************************************

CSaveGameMigrationCloudText::CSaveGameMigrationCloudText()
{
	m_filename.Reset();
	m_fileRequestId = INVALID_CLOUD_REQUEST_ID;
	m_resultCode = 0;
	m_attempts = 0;
	m_retryTimer.Zero();

//	CSaveGameMigrationCloudText::m_TextData;
	m_bInitialised = false;
	m_bTunableHasBeenChecked = false;
	m_bMigrationTunableIsSet = false;
}

void CSaveGameMigrationCloudText::Init(unsigned initMode)
{
	if (initMode == INIT_CORE)
	{

	}
	else if (initMode == INIT_AFTER_MAP_LOADED)
	{
		m_filename = "savegame_migration_cloud_text_";
		m_filename += NetworkUtils::GetLanguageCode();
		m_filename += ".xml";

#if __BANK
		if (PARAM_savegameMigrationLoadLocalTextFile.Get())
		{
			if (!m_bInitialised)
			{
				atString directoryOfLocalFile("common:/non_final/text/");

				ASSET.PushFolder(directoryOfLocalFile);
				if (ASSET.Exists(m_filename, ""))
				{
					m_TextData.LoadFromFile(m_filename);
				}
				else
				{
					savegameErrorf("CSaveGameMigrationCloudText::Init - %s doesn't exist in %s", m_filename.c_str(), directoryOfLocalFile.c_str());
				}
				ASSET.PopFolder();

				m_bInitialised = true;
			}
		}
#endif // __BANK
	}
}


void CSaveGameMigrationCloudText::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
}


void CSaveGameMigrationCloudText::Update()
{
	if (!m_bTunableHasBeenChecked)
	{
		if (Tunables::IsInstantiated() && Tunables::GetInstance().HaveTunablesLoaded())
		{
			m_bMigrationTunableIsSet = NetworkInterface::IsMigrationAvailable();
			m_bTunableHasBeenChecked = true;
		}
	}

	if (m_bMigrationTunableIsSet)
	{
		if(!m_bInitialised && 
			m_fileRequestId == INVALID_CLOUD_REQUEST_ID && 
			m_resultCode == 0 && 
			m_filename.length() > 0 && 
			m_retryTimer.IsComplete(sm_maxTimeoutRetryDelay, false))
		{
			StartDownload();
		}
	}
}


void CSaveGameMigrationCloudText::StartDownload()
{
	const char *pCloudSubDirectory = "text/";
	savegameDisplayf("CSaveGameMigrationCloudText::StartDownload - Starting download of %s from Title cloud directory %s", m_filename.c_str(), pCloudSubDirectory);

	atString titleCloudPath(pCloudSubDirectory);
	titleCloudPath += m_filename;

	m_fileRequestId = CloudManager::GetInstance().RequestGetTitleFile(titleCloudPath.c_str(), CLOUD_TEXT_FILE_SIZE, eRequest_CacheNone);
	m_retryTimer.Zero();
}


void CSaveGameMigrationCloudText::OnCloudEvent(const CloudEvent* pEvent)
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

			bool bSucceeded = pEventData->bDidSucceed;
			m_resultCode = pEventData->nResultCode;
			m_fileRequestId = INVALID_CLOUD_REQUEST_ID;

			// did we get the file...
			if(bSucceeded)
			{
				savegameDisplayf("CSaveGameMigrationCloudText::OnCloudEvent - Download Success - %s", m_filename.c_str());

				char bufferFName[RAGE_MAX_PATH];
				fiDevice::MakeMemoryFileName(bufferFName, sizeof(bufferFName), pEventData->pData, pEventData->nDataSize, false, m_filename.c_str());

				bSucceeded = m_TextData.LoadFromFile(bufferFName);

				if(bSucceeded)
				{
					m_bInitialised = true;
				}
				else
				{
					savegameErrorf("CSaveGameMigrationCloudText::OnCloudEvent - Parsing failed, marking as 404 - %s", m_filename.c_str());
					m_resultCode = NET_HTTPSTATUS_NOT_FOUND;
				}
			}
			else if(IsTimeoutError(static_cast<netHttpStatusCodes>(m_resultCode)) && m_attempts < sm_maxAttempts)
			{
				savegameErrorf("CSaveGameMigrationCloudText::OnCloudEvent - Download Timeout - %s", m_filename.c_str());
				++m_attempts;
				m_resultCode = 0;
				m_retryTimer.Start();
			}
			else
			{
				savegameErrorf("CSaveGameMigrationCloudText::OnCloudEvent - Download Failed - %s, resultCode=%d", m_filename.c_str(), m_resultCode);
			}
		}
		break;

	case CloudEvent::EVENT_AVAILABILITY_CHANGED:
		break;
	};
}


bool CSaveGameMigrationCloudText::IsTimeoutError(netHttpStatusCodes errorCode)
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


const char *CSaveGameMigrationCloudText::SearchForText(u32 textKeyHash)
{
	if (m_bInitialised)
	{
		return m_TextData.SearchForText(textKeyHash);
	}

	return NULL;
}

#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

