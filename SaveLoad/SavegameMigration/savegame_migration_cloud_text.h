#ifndef SAVEGAME_MIGRATION_CLOUD_TEXT_H
#define SAVEGAME_MIGRATION_CLOUD_TEXT_H

#include "SaveLoad/savegame_defines.h"

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES

// Rage headers
#include "atl/binmap.h"
#include "parser/macros.h"

// Game headers
#include "Frontend/SimpleTimer.h"
#include "Network/Cloud/CloudManager.h"


class CSaveGameMigrationCloudTextLine
{
public:
	CSaveGameMigrationCloudTextLine() {}

	atString m_TextKey;
	atString m_LocalizedText;

	PAR_SIMPLE_PARSABLE;
};


class CSaveGameMigrationCloudTextData
{
public :
	CSaveGameMigrationCloudTextData();

	bool LoadFromFile(const char* pFilename);

	const char *SearchForText(u32 textKeyHash);

private:
	atArray<CSaveGameMigrationCloudTextLine> m_ArrayOfTextLines;
	atBinaryMap<u32, u32> m_MapOfTextKeyHashToArrayIndex;

	PAR_SIMPLE_PARSABLE;
};


class CSaveGameMigrationCloudText : CloudListener
{
public:
	CSaveGameMigrationCloudText();

	void Init(unsigned initMode);
	void Shutdown(unsigned shutdownMode);

	void Update();

	const char *SearchForText(u32 textKeyHash);

	bool IsInitialised() { return m_bInitialised; }

private:
	void StartDownload();

	void OnCloudEvent(const CloudEvent* pEvent);

	bool IsTimeoutError(netHttpStatusCodes errorCode);

// Private data
	static const u32 sm_maxAttempts = 3;
	static const s32 sm_maxTimeoutRetryDelay = 10000;

	atString m_filename;
	CloudRequestID m_fileRequestId;
	int m_resultCode;
	u8 m_attempts;
	CSystemTimer m_retryTimer;

	CSaveGameMigrationCloudTextData m_TextData;
	bool m_bInitialised;
	bool m_bTunableHasBeenChecked;
	bool m_bMigrationTunableIsSet;
};

#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#endif // SAVEGAME_MIGRATION_CLOUD_TEXT_H

