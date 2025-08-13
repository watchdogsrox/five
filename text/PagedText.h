//
// PagedText.h
//
// Copyright (C) 2012 Rockstar Games.  All Rights Reserved.
//

#ifndef PAGED_TEXT_H
#define PAGED_TEXT_H

// rage
#include "fwnet/netCloudFileDownloadHelper.h"
#include "net/http.h"
#include "string/stringbuilder.h"

// game
#include "Frontend/SimpleTimer.h"
#include "Network/Cloud/CloudManager.h"

#define MAX_PAGES_OF_TEXT (4)


class PagedText
{
public:
	PagedText() {Reset();}

	void Reset();

	int Size() const {return m_numPages;}

	atStringBuilder& operator[] (int index) {return m_textPages[index];}
	const atStringBuilder& operator[] (int index) const {return m_textPages[index];}

	void ParseRawText(const char* pText, int textLen);

protected:

	void FindNextDoubleSpace(const char* pText, int textLen, u32& outIndex, u32& outLen);

private:
	unsigned m_numPages;
	atStringBuilder m_textPages[MAX_PAGES_OF_TEXT];
};

class PagedCloudText : CloudListener
{
public:

	PagedCloudText();

	void Init(const atString& path, bool fromCloud BANK_ONLY(, u8 debugForceAttemptTimeouts));
	void Shutdown();
	void Update();

	int GetNumPages() const {return m_pagedText.Size();}
	const atStringBuilder& GetPageOfText(int index) const {return m_pagedText[index];}

	bool IsPending() const {return m_fileRequestId != INVALID_CLOUD_REQUEST_ID || m_retryTimer.IsStarted();}
	bool DidSucceed() const {return m_didSucceed;}
	int GetResultCode() const { return m_resultCode; }
	const char* GetFilename() const { return m_filename; }
	CloudRequestID GetCloudRequestId() const {return m_fileRequestId;}
	
	// True if the loaded file came from the local disk, not the cloud.
	bool IsLocalFile() const { return m_isLocalFile; }

	void Reset();

	void StartDownload();

	static bank_u32 sm_maxAttempts;
	static bank_s32 sm_maxTimeoutRetryDelay;
	static bank_s32 sm_timeoutRetryDelayVariance;
	static bool IsTimeoutError(netHttpStatusCodes errorCode);

private:

	void OnCloudEvent(const CloudEvent* pEvent);
	void ParseFile(const char* pFilename);

	bool m_didSucceed;
	bool m_isLocalFile;
	u8 m_attempts;
	BANK_ONLY(u8 m_debugForceAttemptTimeouts;)
	int m_resultCode;

	CloudRequestID m_fileRequestId;
	atString m_filename;
	PagedText m_pagedText;
	CSystemTimer m_retryTimer;
};

#endif  // PAGED_TEXT_H

