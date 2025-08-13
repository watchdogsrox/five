#include "control/replay/ReplaySettings.h"

//TODO4FIVE
#if GTA_REPLAY && defined(ENABLE_BROWSER) && ENABLE_BROWSER

#pragma once

#include <WinInet.h>

#include "atl/string.h"
#include "system/FileMgr.h"

const int MAX_SESSION_ID_LENGTH = 64;

class UIUpload;

class HttpUpload
{
public:
	HttpUpload(bool bThread = false);
	~HttpUpload();

	void DoUpload(atArray<char*> aFileList, int iEpisodeIndex);

	void SetUserID(const char* sUserName, const char* sPassword);

	void SetSessionId( const char* aSessionId);
	char*  GetSessionId() { return m_sSessionId; }

	void GenerateMultipartBoundary(char* szOutBoundary);

	void SetUiUpload( UIUpload* pUiClip) { m_pUiUpload = pUiClip; }
	UIUpload* GetUiUpload() {return m_pUiUpload;}

	bool IsUploadingFinished() { return m_StopWorkerThread; }
	void SetUploadingFinished( bool bIn ) { m_StopWorkerThread = bIn; }
	void SingalUploading() {sysIpcSignalSema(m_WakeUpSema); }

	// check if there is a session id in the userinfo.dat
	static bool IsRockstartSessionIdValid(char* pOutSessionId = NULL);
	static void GetRockStartUserInfo(char* pOutSessionId, char* pOutUserName, char* pOutPassword);
	static bool CanOpenConnection(const char* szUrl);
	static bool IsInternetConnectionValid() { return m_bInternetConnectionValid; }
	static void UrlEncode(const char* szIn, char* szOut, u32 iOutLength);
	static bool ReadResponseBody(HINTERNET hRequest, atString *pOutBuf);

	void SetDownloadTextList(bool bIn) { m_bDownTextList = bIn; }

	static char* EncryptData(char* pData, s32 iDatalength, s32 &iEncyptedLength);
	static char* DecryptData(char* pData, s32 iDatalength, s32 &iDecyptedLength);

	const char* GetNickName() { return m_sNickName.c_str(); }
	const char* GetMemberId() { return m_sMemberId.c_str(); }
	
	int	GetLimitDuration() { return m_szDuration.c_str() ? atoi(m_szDuration.c_str()) : -1;};
	int GetLimitFileSize() { return m_szFileSize.c_str() ? atoi(m_szFileSize.c_str()) : -1; }

	const char* GetLimitDurationChar() { return m_szDuration.c_str(); };
	const char* GetLimitFileSizeChar() { return m_szFileSize.c_str(); }

	bool CdKeyRegister();

	bool Perform();
	static DWORD WINAPI WorkerFunction(IN LPVOID vThreadParm);
	static 	INTERNET_BUFFERS BufferIn;
	static HINTERNET hRequest;

private:
	char* m_sUserName;
	char* m_sPassword;
	char* m_sSessionId;
	char* m_sOpusId;
	char* m_sUploadId;
	atString m_sNickName;
	atString m_sMemberId;
	atString m_sAuthToken;

	char* m_sUploadUrlData;	// contains url, progressid, and uid
	char* m_sUploadUrl;
	char* m_sProgressid;
	char* m_sUid;
	atString m_sFinishUrlData;	// contains url and uid

	char m_sBoundary[40];
	char m_sFileName[40];
	bool m_bSendingFile;

	int m_iVideoEpisode;	// the episode of the video, NOT the current game episode.

	UIUpload* m_pUiUpload;

	atArray<char*> m_aFileLis;

	bool m_bDownTextList;

	void Base64EncodeByteTriple  (char* p_pInputBuffer, u32 InputCharacters, char* p_pOutputBuffer);
	void Base64Encode (char* p_pInputBuffer, u32 p_InputBufferLength, char* p_pOutputBufferString);
	u32 RecquiredEncodeOutputBufferSize(u32 p_InputByteCount);

	// size of all the video files
	u32 CalculateAssetLength(atArray<char*> aFileList);
	// total content size.
	u32 CalculateContentLength(atArray<char*> aFileList);

	bool ExtractSessionID(fiStream* fiHandle, bool bPostUrl = false);
	bool ExtractUserInfo(const char* szFileName, const char* ext, fiStream* fiHandle = NULL);	// extract username and password from userinfo.dat file.
	static void RC4(unsigned char* data, int dataLen, const unsigned char* key, int keyLen);
	static fiStream* DecryptUserInfo();
	static bool ExtractRockstartUserInfo(char* pOutSessionIdconst, char* pOutUserName, char* pPassword, const char* szFileName, const char* ext, fiStream* fiHandle = NULL);

	bool SetupSecurityOptions(HINTERNET hRequest);
	bool SendRequest(HINTERNET hConnect, const char* szUrlPath, const char* szHeader, const char* data, u32 iDataSize, bool bPostUrl = false, bool bXmlParse = true);

	void ParsePostUrl(char* aPostUrlData);

	u32 CalculateFieldSize(const char* szBoundary, const char* szFieldName, const char* szFieldValue);
	u32 CalculateFileHeaderSize(char* szBoundary, char* szFieldName, char* szFileName);
	u32 CalculateImageFileHeaderSize(char* szBoundary, char* szFieldName, char* szFileName);
	atString GenerateFieldTrailer();
	atString GenerateFileTrailer();
	atString GenerateAccmodField(char* strBoundary);
	atString GeneratePreBodyTrailer(char* strBoundary);
	atString GenerateBodyTrailer(char* strBoundary);
	atString GenerateFieldHeader(const char* szBoundary, const char* szFieldName);
	atString GenerateFileHeader(char* szBoundary, char* szFieldName, char* szFileName);
	atString GenerateImageFileHeader(char* szBoundary, char* szFieldName, const char* szFileName);
	HRESULT UploadField(HINTERNET hRequest, const char* szBoundary, const char* szFieldName, const char* szFieldValue);
	HRESULT UploadFile(HINTERNET hRequest, char* szBoundary, char* szName, char* szFileName);
	HRESULT UploadFileHeader(HINTERNET hRequest, char* szBoundary, char* szFieldName, char* szFileName);
	HRESULT UploadAssets(HINTERNET hRequest, atArray<char*> aFileList);
	HRESULT UploadImageFile(HINTERNET hRequest, char* szBoundary, char* szName, const char* szFileName);
	HRESULT UploadImageFileHeader(HINTERNET hRequest, char* szBoundary, char* szFieldName, const char* szFileName);

	bool DownTextList();
	char m_szTextUrl[INTERNET_MAX_HOST_NAME_LENGTH];
	atString m_szDuration;
	atString m_szFileSize;

	static bool m_bInternetConnectionValid;

	bool m_bUsingThread;
	//--------------------------------------------------------------------------------------------------------------------
	sysIpcThreadId					m_ThreadId;
	sysIpcSema						m_WaitSema;
	sysIpcSema						m_WakeUpSema;
	bool							m_StopWorkerThread;
	static void Worker(void*);

	//--------------------------------------------------------------------------------------------------------------------
};

#endif	//__WIN32PC