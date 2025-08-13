#include "control/replay/HttpUpload/HttpUpload.h"

//TODO4FIVE
#if GTA_REPLAY && defined(ENABLE_BROWSER) && ENABLE_BROWSER

#include "control/replay/replay.h"
#include "..\..\gta\source\WebBrowser\WebBrowser.h"
#include "parser/treenode.h"
#include "parser/manager.h"
#include "frontend/UIUpload.h"
#include "../../gta/source/Core/app.h"

#include <Wincrypt.h>

#define UPLOAD_LEGAL_URLENCODED_CHARS      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"	
#define UPLOAD_DIGITS                      "0123456789ABCDEF"

#define USE_SSL 1
#define USE_BASE64 0
#define USE_ENCRYPTION 1
#define DOWNLOAD_LIST 1
#define MAX_EPISODES 3

const u32 BUFFER_SIZE = 512 * 1024;	// 512KB

// A random guess: 192Kbps upstream bandwidth
const u32 INITIAL_ESTIMATED_TRANSFER_RATE = 19200;		// bytes per second

// Base64 encoding
const char BASE64_ALPHABET [64] = 
{
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', //   0 -   9
	'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', //  10 -  19
	'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', //  20 -  29
	'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', //  30 -  39
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', //  40 -  49
	'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', //  50 -  59
	'8', '9', '+', '/'								  //  60 -  63
};

bool HttpUpload::m_bInternetConnectionValid = false;

#if !__FINAL
PARAM(scusername, "[VideoUpload] social club username");
PARAM(scpassword, "[VideoUpload] social club password");
#endif

enum MultPartFields
{
	MPF_TITLE,
	MPF_DESCRIPTION,
	MPF_TAGS,
	MPF_MAX
};

char* sMultPartFiledName[MPF_MAX] = 
{
	"title",
	"description",
	"tags",
};

// TODO: need a better way to set field values
char* sMultPartFiledValue[MAX_EPISODES][MPF_MAX] = 
{
	{"GTA4", "GTA4", "GTA4"},
	{"TLAD", "TLAD", "TLAD"},
	{"TBoGT", "TBoGT", "TBoGT"}
};

HttpUpload::HttpUpload(bool bThread)
{
	m_sUserName = rage_new char[INTERNET_MAX_USER_NAME_LENGTH];
	m_sUserName[0] = '\0';
	m_sPassword = rage_new char[INTERNET_MAX_PASSWORD_LENGTH];
	m_sPassword[0] = '\0';
	m_sSessionId = rage_new char[MAX_SESSION_ID_LENGTH];
	m_sSessionId[0] = '\0';
	m_sUploadUrlData = NULL;
	m_sUploadUrl = NULL;
	m_sUploadId = NULL;
	m_sProgressid = NULL;
	m_sUid = NULL;
	m_sBoundary[0] = '\0';
	m_bSendingFile = false;
	m_sFileName[0] = '\0';

	m_sOpusId = NULL;

	m_bDownTextList = false;
	m_szTextUrl[0] = '\0';

	m_iVideoEpisode = 0;

#if !__FINAL
	const char* sUserName = 0;
	const char* sPassword = 0;
	if( PARAM_scusername.Get(sUserName) && PARAM_scpassword.Get(sPassword) )
	{
		SetUserID(sUserName, sPassword);
	}
	else
#endif
	{
		CFileMgr::SetRockStartDir("");
		fiStream* fiUserInfo = NULL;

#if USE_ENCRYPTION
		fiUserInfo = DecryptUserInfo();
		if(fiUserInfo)
		{
#endif	// USE_ENCRYPTION
		if(!ExtractUserInfo("userinfo", "dat", fiUserInfo))
		{
			Displayf("Couldn't extract user info");
			Assert(0);
		}
#if USE_ENCRYPTION
		}
#endif	// USE_ENCRYPTION
	}

	m_bUsingThread = bThread;
	if(bThread)
	{
		m_StopWorkerThread = false;
		m_ThreadId = sysIpcThreadIdInvalid ;
		m_WaitSema = sysIpcCreateSema(0);
		m_WakeUpSema = sysIpcCreateSema(0);

		if(m_WakeUpSema && m_WaitSema)
		{
			m_ThreadId =
				sysIpcCreateThread(&HttpUpload::Worker,
				this,
				sysIpcMinThreadStackSize,
				PRIO_NORMAL,
				"Http Uploading");

			if(AssertVerify(sysIpcThreadIdInvalid != m_ThreadId))
			{
				//Wait for the thread to start.
				sysIpcWaitSema(m_WaitSema);
			}
		}

		if(sysIpcThreadIdInvalid == m_ThreadId)
		{
			if(m_WaitSema)
			{
				sysIpcDeleteSema(m_WaitSema);
				m_WaitSema = NULL;
			}

			if(m_WakeUpSema)
			{
				sysIpcDeleteSema(m_WakeUpSema);
				m_WakeUpSema = NULL;
			}
		}
	}
}

HttpUpload::~HttpUpload()
{
	delete [] m_sUserName;
	m_sUserName = NULL;
	delete [] m_sPassword;
	m_sPassword = NULL;
	delete [] m_sSessionId;
	m_sSessionId = NULL;
	delete [] m_sUploadId;
	m_sUploadId = NULL;
	delete [] m_sUploadUrlData;
	m_sUploadUrlData = NULL;
	delete [] m_sOpusId;
	m_sOpusId = NULL;

#if __DEV
	atString sFullName;
	CFileMgr::SetUserDataDir("Videos\\rendered");
	sFullName = CFileMgr::GetCurrentDirectory();
	sFullName += "temp.xml";
	DeleteFile(sFullName.c_str());
#endif // __DEV

	if(m_bUsingThread)
	{
		if(sysIpcThreadIdInvalid != m_ThreadId)
		{
			m_StopWorkerThread = true;
			sysIpcSignalSema(m_WakeUpSema);
			sysIpcWaitSema(m_WaitSema);
			m_StopWorkerThread = false;
			m_ThreadId = sysIpcThreadIdInvalid;
		}

		if(m_WaitSema)
		{
			sysIpcDeleteSema(m_WaitSema);
			m_WaitSema = NULL;
		}

		if(m_WakeUpSema)
		{
			sysIpcDeleteSema(m_WakeUpSema);
			m_WakeUpSema = NULL;
		}
	}
}

void HttpUpload::SetUserID(const char* sUserName, const char* sPassword)
{
	UrlEncode(sUserName, m_sUserName, INTERNET_MAX_USER_NAME_LENGTH);
	UrlEncode(sPassword, m_sPassword, INTERNET_MAX_PASSWORD_LENGTH);
}

void HttpUpload::SetSessionId( const char* aSessionId)
{
	UrlEncode(aSessionId, m_sSessionId, MAX_SESSION_ID_LENGTH);
}

void HttpUpload::GenerateMultipartBoundary(char* szOutBoundary)
{
	Assert(szOutBoundary!= NULL);
	strcpy(szOutBoundary, "---------------------------");
	Assert(strlen(szOutBoundary) == 27);

	// We need 14 hex digits.
	int r0 = rand() & 0xffff;
	int r1 = rand() & 0xffff;
	int r2 = rand() & 0xffffff;

	sprintf(szOutBoundary+strlen(szOutBoundary), ("%04X%04X%06X"), r0, r1, r2);
}

atString HttpUpload::GenerateFieldHeader(const char* strBoundary, const char* szFieldName)
{
	atString fieldHeader = "--";
	fieldHeader += strBoundary;
	fieldHeader += "\r\n";
	fieldHeader += "Content-Disposition: form-data; name=\"";
	fieldHeader += szFieldName;
	fieldHeader += "\"\r\n\r\n";

	return fieldHeader;
}

atString HttpUpload::GenerateFieldTrailer()
{
	return "\r\n";
}

atString HttpUpload::GenerateFileHeader(char* szBoundary, char* szFieldName, char* szFileName)
{
	atString videoField = "--";
	videoField += szBoundary;
	videoField += "\r\n";
	videoField += "Content-Disposition: form-data; name=\"";
	videoField += szFieldName;
	videoField += "\"; filename=\"";
	videoField += szFileName;
	videoField += "\"\r\n";
	videoField += "Content-Type: video/x-ms-wmv";
	videoField += "\r\n";
	videoField += "\r\n";

	return videoField;
}

atString HttpUpload::GenerateFileTrailer()
{
	return "\r\n";
}

// input is .tag file. need to change file name to .JPG
atString HttpUpload::GenerateImageFileHeader(char* szBoundary, char* szFieldName, const char* szFileName)
{
	atString sJpgName = szFileName;
	sJpgName.Truncate(sJpgName.length()-3);
	sJpgName += "JPG";

	atString videoField = "--";
	videoField += szBoundary;
	videoField += "\r\n";
	videoField += "Content-Disposition: form-data; name=\"";
	videoField += szFieldName;
	videoField += "\"; filename=\"";
	videoField += sJpgName;
	videoField += "\"\r\n";
	videoField += "Content-Type: image/jpeg";
	videoField += "\r\n";
	videoField += "\r\n";

	return videoField;
}

atString HttpUpload::GeneratePreBodyTrailer(char* strBoundary)
{
	atString bodyTrailer = "--";
	bodyTrailer += strBoundary;
	bodyTrailer += "\r\n";
	bodyTrailer += "Content-Disposition: form-data; name=\"uploadbtn\"\r\n\r\nUpload\r\n";
	return bodyTrailer;
}

atString HttpUpload::GenerateBodyTrailer(char* strBoundary)
{
	atString bodyTrailer = "--";
	bodyTrailer += strBoundary;
	bodyTrailer += "--\r\n";
	return bodyTrailer;
}

// special case for accomde
atString HttpUpload::GenerateAccmodField(char* strBoundary)
{
	atString fieldHeader = "--";
	fieldHeader += strBoundary;
	fieldHeader += "\r\n";
	fieldHeader += "Content-Disposition: form-data; accmode=\"3\"";
	fieldHeader += "\r\n\r\n";

	return fieldHeader;
}

u32 HttpUpload::CalculateFieldSize(const char* szBoundary, const char* szFieldName, const char* szFieldValue)
{
	atString fieldHeader = GenerateFieldHeader(szBoundary, szFieldName);
	atString fieldTrailer = GenerateFieldTrailer();
	atString fieldValue = szFieldValue;

	u32 iTotalLength = fieldHeader.GetLength() + fieldTrailer.GetLength()+ fieldValue.GetLength();

	return iTotalLength;
}

u32 HttpUpload::CalculateFileHeaderSize(char* szBoundary, char* szFieldName, char* szFileName)
{
	atString videoField = GenerateFileHeader(szBoundary, szFieldName, szFileName);

	u32 iLength = videoField.GetLength();

	return iLength;
}

u32 HttpUpload::CalculateImageFileHeaderSize(char* szBoundary, char* szFieldName, char* szFileName)
{
	atString videoField = GenerateImageFileHeader(szBoundary, szFieldName, szFileName);

	u32 iLength = videoField.GetLength();

	return iLength;
}

u32 HttpUpload::CalculateAssetLength(atArray<char*> aFileList)
{
	u32 iTotalSize = 0;
	
	for(int i = 0; i < aFileList.GetCount(); i++)
	{
		int iAllocLength = strlen(aFileList[i]) + 1;
		WCHAR* wszFileName = rage_new WCHAR[iAllocLength];
		if( wszFileName == NULL )
		{
			return iTotalSize;
		}
		CTextConversion::ConvertToWideChar(aFileList[i], wszFileName, iAllocLength);

		WCHAR wszWMVFileName[RAGE_MAX_PATH];
		swprintf(wszWMVFileName, RAGE_MAX_PATH, L"%sVideos\\Rendered\\%s", CFileMgr::GetUserDataRootDirectoryW(), wszFileName);
		HANDLE hWMV = INVALID_HANDLE_VALUE;
		hWMV = CreateFileW(wszWMVFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if(hWMV == INVALID_HANDLE_VALUE)
		{
			delete[] wszFileName;
			return iTotalSize;
		}
		iTotalSize += GetFileSize(hWMV, NULL);
		delete[] wszFileName;
		CloseHandle(hWMV);
	
		atString szImageFile = aFileList[i];
		szImageFile.Truncate(szImageFile.length()-3);
		szImageFile += "tag";
		u8* pImageBuffer = NULL;
		u32 uImageSize = 0;
		CFileMgr::SetUserDataDir("Videos\\rendered");
		int iCounter = 8;
		while(!CReplayMgr::LoadTag(szImageFile, &pImageBuffer, uImageSize) && iCounter > 0)
		{
			CFileMgr::SetUserDataDir("Videos\\rendered");
			iCounter--;
			Displayf("Couldn't load image file: %s", szImageFile);
			Assert(0);
		}
		iTotalSize += uImageSize;
		delete pImageBuffer;
		pImageBuffer = NULL;
	}

//
//	CFileMgr::SetUserDataDir("Videos\\rendered");
//	FileHandle fileHandle;
//
//	u32 iTotalSize = 0;
//
//	for(int i = 0; i < aFileList.GetCount(); i++)
//	{
//		fileHandle = CFileMgr::OpenFile(aFileList[i]);
//		if(CFileMgr::IsValidFileHandle(fileHandle))
//		{
//#if USE_BASE64
//			// base64 encode
//			iTotalSize += RecquiredEncodeOutputBufferSize(CFileMgr::GetTotalSize(fileHandle));
//#else
//			iTotalSize += CFileMgr::GetTotalSize(fileHandle);
//#endif	// USE_BASE64
//		}
//		else
//		{
//			Displayf("Couldn't open file: %s", aFileList[i]);
//			Assert(0);
//			return iTotalSize;
//		}
//		CFileMgr::CloseFile(fileHandle);
//
//		atString szImageFile = aFileList[i];
//		szImageFile.Truncate(szImageFile.length()-3);
//		szImageFile += "tag";
//		u8* pImageBuffer = NULL;
//		u32 uImageSize = 0;
//		if(!CReplayMgr::LoadTag(szImageFile, &pImageBuffer, uImageSize))
//		{
//			Displayf("Couldn't load image file: %s", szImageFile);
//			Assert(0);
//		}
//		iTotalSize += uImageSize;
//		delete pImageBuffer;
//		pImageBuffer = NULL;
//	}
//
	return iTotalSize;
}

u32 HttpUpload::CalculateContentLength(atArray<char*> aFileList)
{
	u32 iContentLength = 0;
	// file header and trailer size
	for(int i = 0; i < aFileList.GetCount(); i++)
	{
		char szName[32] = {"File"};
		snprintf(szName+strlen(szName), 31, "%d", i);
		iContentLength += CalculateFileHeaderSize(m_sBoundary, szName, aFileList[i]);
		atString fileTrailer = GenerateFieldTrailer();
		iContentLength += fileTrailer.GetLength();

		char szImageName[32] = {"Image"};
		snprintf(szImageName+strlen(szImageName), 31, "%d", i);
		iContentLength += CalculateImageFileHeaderSize(m_sBoundary, szImageName, aFileList[i]);
		iContentLength += fileTrailer.GetLength();
	}
	// file size
	iContentLength += CalculateAssetLength(aFileList);

	//WCHAR wszFileName[RAGE_MAX_PATH];
	atString szTitle = aFileList[0];
	atString displayName = szTitle;
	if(displayName.EndsWith(".wmv"))
	{
		// remove .wmv extension.		
		szTitle.Truncate(szTitle.length() - 4);
		displayName.Reset();
		displayName = szTitle;
		// remove _WEB extension.
		int lastUnderscoreIndex = -1;
		for(int i = displayName.length()-1; i > 0; i--)
		{
			if(displayName[i] == '_')
			{
				lastUnderscoreIndex = i;
				break;
			}
		}

		if( lastUnderscoreIndex > 0)
		{
			szTitle.Truncate(lastUnderscoreIndex);
		}

		//CTextConversion::ConvertToWideChar(szTitle, wszFileName, RAGE_MAX_PATH);
	}

	// field size
	for(int i =0; i < MPF_MAX; i++)
	{
		if( i == 0)
		{
			iContentLength += CalculateFieldSize(m_sBoundary, sMultPartFiledName[i], szTitle.c_str());
		}
		else
		{
			iContentLength += CalculateFieldSize(m_sBoundary, sMultPartFiledName[i], sMultPartFiledValue[m_iVideoEpisode][i]);
		}
	}

	atString accmodeField = GenerateAccmodField(m_sBoundary);
	iContentLength += accmodeField.GetLength();

	atString preBodyTrailer = GeneratePreBodyTrailer(m_sBoundary);
	iContentLength += preBodyTrailer.GetLength();
	atString bodyTrailer = GenerateBodyTrailer(m_sBoundary); 
	iContentLength += bodyTrailer.GetLength();

	return iContentLength;
}

HRESULT HttpUpload::UploadField(HINTERNET hRequest, const char* szBoundary, const char* szFieldName, const char* szFieldValue)
{
	atString fieldHeader = GenerateFieldHeader(szBoundary, szFieldName);
	Displayf(fieldHeader);

	DWORD dwBytesWritten;
	if (!::InternetWriteFile(hRequest, fieldHeader, fieldHeader.GetLength(), &dwBytesWritten))
	{
		Displayf("InternetWriteFile failed, error = %d (0x%x)\n", GetLastError(), GetLastError());
		return HRESULT_FROM_WIN32(GetLastError());
	}

	atString fieldValue = szFieldValue;
	Displayf(fieldValue);
	if (!InternetWriteFile(hRequest, fieldValue, fieldValue.GetLength(), &dwBytesWritten))
	{
		Displayf("InternetWriteFile failed, error = %d (0x%x)\n", GetLastError(), GetLastError());
		return HRESULT_FROM_WIN32(GetLastError());
	}

	atString fieldTrailer = GenerateFieldTrailer();
	Displayf(fieldTrailer);
	if (!InternetWriteFile(hRequest, fieldTrailer, fieldTrailer.GetLength(), &dwBytesWritten))
	{
		Displayf("InternetWriteFile failed, error = %d (0x%x)\n", GetLastError(), GetLastError());
		return HRESULT_FROM_WIN32(GetLastError());
	}

	return S_OK;
}

// FieldName is "File"
HRESULT HttpUpload::UploadFileHeader(HINTERNET hRequest, char* szBoundary, char* szFieldName, char* szFileName)
{
	atString fileHeader = GenerateFileHeader(szBoundary, szFieldName, szFileName);

	Displayf(fileHeader);

	DWORD dwBytesWritten;
	if (!::InternetWriteFile(hRequest, fileHeader, fileHeader.GetLength(), &dwBytesWritten))
	{
		Displayf("InternetWriteFile failed, error = %d (0x%x)\n", GetLastError(), GetLastError());
		return HRESULT_FROM_WIN32(GetLastError());
	}

	return S_OK;
}

// FieldName is "Image"
HRESULT HttpUpload::UploadImageFileHeader(HINTERNET hRequest, char* szBoundary, char* szFieldName, const char* szFileName)
{
	atString fileHeader = GenerateImageFileHeader(szBoundary, szFieldName, szFileName);

	Displayf(fileHeader);

	DWORD dwBytesWritten;
	if (!::InternetWriteFile(hRequest, fileHeader, fileHeader.GetLength(), &dwBytesWritten))
	{
		Displayf("InternetWriteFile failed, error = %d (0x%x)\n", GetLastError(), GetLastError());
		return HRESULT_FROM_WIN32(GetLastError());
	}

	return S_OK;
}

HRESULT HttpUpload::UploadImageFile(HINTERNET hRequest, char* szBoundary, char* szName, const char* szFileName)
{
	Displayf("-----Start sending image file %s", szFileName);

	HRESULT hr = UploadImageFileHeader(hRequest, m_sBoundary, szName, szFileName);
	if (FAILED(hr))
	{
		Displayf("Error on InternetWriteFile %d\n", GetLastError());
		Assert(0);
		return hr;
	}

	u8* pImageBuffer = NULL;
	u32 uImageSize = 0;
	if(!CReplayMgr::LoadTag(szFileName, &pImageBuffer, uImageSize))
	{
		Displayf("Couldn't load image file: %s", szFileName);
		Assert(0);
		return S_FALSE;
	}

	DWORD dwBytesWritten;
	if(!::InternetWriteFile(hRequest, pImageBuffer, uImageSize, &dwBytesWritten))
	{
		delete pImageBuffer;
		return HRESULT_FROM_WIN32(GetLastError());
	}

	atString fileTrailer = GenerateFileTrailer();
	Displayf(fileTrailer);
	if(!::InternetWriteFile(hRequest, fileTrailer, fileTrailer.GetLength(), &dwBytesWritten))
	{
		Displayf("Error on InternetWriteFile %d\n", GetLastError());
		Assert(0);
		delete pImageBuffer;
		return HRESULT_FROM_WIN32(GetLastError());
	}
	
	delete pImageBuffer;
	return S_OK;
}

HRESULT HttpUpload::UploadFile(HINTERNET hRequest, char* szBoundary, char* szName, char* szFileName)
{
	Displayf("-----Start sending video file %s", szFileName);
	
	// send out file header
	HRESULT hr = UploadFileHeader(hRequest, m_sBoundary, szName, szFileName);
	if (FAILED(hr))
	{
		Displayf("Error on InternetWriteFile %d\n", GetLastError());
		Assert(0);
		return hr;
	}

	int iAllocLength = strlen(szFileName) + 1;
	WCHAR* wszFileName = rage_new WCHAR[iAllocLength];
	if( wszFileName == NULL)
	{
		Assert(0);
		return S_FALSE;
	}
	CTextConversion::ConvertToWideChar(szFileName, wszFileName, iAllocLength);

	WCHAR wszWMVFileName[RAGE_MAX_PATH];
	swprintf(wszWMVFileName, RAGE_MAX_PATH, L"%sVideos\\Rendered\\%s", CFileMgr::GetUserDataRootDirectoryW(), wszFileName);
	HANDLE hWMV = INVALID_HANDLE_VALUE;
	hWMV = CreateFileW(wszWMVFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hWMV == INVALID_HANDLE_VALUE)
	{
		delete[] wszFileName;
		return S_FALSE;
	}

	// read file
	//FileHandle fileHandle = CFileMgr::OpenFile(szFileName);
	//if(!CFileMgr::IsValidFileHandle(fileHandle))
	//{
	//	Displayf("Can't find the upload file: %s", szFileName);
	//	Assert(0);
	//	return S_FALSE;
	//}
	//u32 iFileSize = CFileMgr::GetTotalSize(fileHandle);
	u32 iFileSize = GetFileSize(hWMV, NULL);

	char* pFileBuffer = rage_new char[BUFFER_SIZE];
#if USE_BASE64
	u32 iEncodedBufferSize = RecquiredEncodeOutputBufferSize(BUFFER_SIZE);
	char* pEncodedBuffer = rage_new char[iEncodedBufferSize];
#endif	// USE_BASE64
	DWORD dwBytesWritten = 0;
	DWORD dwBytesRead = 0;

	const int PROGRESS_INTERVAL = 500;	// Report progress twice a second.
	DWORD dwTimeStarted = GetTickCount() / PROGRESS_INTERVAL;
	DWORD dwTimeLast = dwTimeStarted;
	DWORD dwFileBytesSent = 0;
	DWORD dwBytesPerSecond = INITIAL_ESTIMATED_TRANSFER_RATE;
	DWORD dwSecondsToFileCompletion = iFileSize / dwBytesPerSecond;

	for(;;)
	{
		Displayf("read into file buffer");
		//dwBytesRead = CFileMgr::Read(fileHandle, pFileBuffer, BUFFER_SIZE);
		ReadFile(hWMV, pFileBuffer, BUFFER_SIZE, &dwBytesRead, NULL);

#if USE_BASE64
		// base64 encode
		Base64Encode(pFileBuffer, dwBytesRead, pEncodedBuffer);
#endif	// USE_BASE64
		// there is an error during reading.
		if(dwBytesRead == -1)
		{
			delete [] pFileBuffer;
#if USE_BASE64
			delete [] pEncodedBuffer;
#endif	// USE_BASE64
			return S_FALSE;
		}

		// reading is done
		if( dwBytesRead == 0)
		{
			m_pUiUpload->UpdateProgressBar(iFileSize, iFileSize);
			break;
		}
#if USE_BASE64
		if(!::InternetWriteFile(hRequest, pEncodedBuffer, iEncodedBufferSize, &dwBytesWritten))
#else
		if(!::InternetWriteFile(hRequest, pFileBuffer, dwBytesRead, &dwBytesWritten))
#endif	// USE_BASE64
		{
			Displayf("Error on InternetWriteFile %d\n", GetLastError());
			Assert(0);
			delete [] pFileBuffer;
#if USE_BASE64
			delete [] pEncodedBuffer;
#endif	// USE_BASE64
			//CFileMgr::CloseFile(fileHandle);
			CloseHandle(hWMV);
			return HRESULT_FROM_WIN32(GetLastError());
		}

		dwFileBytesSent += dwBytesWritten;

		DWORD dwTimeNow = GetTickCount() / PROGRESS_INTERVAL;
		if (dwTimeNow != dwTimeLast)
		{
			DWORD dwSecondsTaken = dwTimeNow - dwTimeStarted;
			dwBytesPerSecond = dwFileBytesSent / dwSecondsTaken;

			DWORD dwFileBytesRemaining = iFileSize - dwFileBytesSent;
			dwSecondsToFileCompletion = dwFileBytesRemaining / dwBytesPerSecond;

			m_pUiUpload->UpdateProgressBar(dwFileBytesSent, iFileSize);
		}
	}	

	atString fileTrailer = GenerateFileTrailer();
	Displayf(fileTrailer);
	if(!::InternetWriteFile(hRequest, fileTrailer, fileTrailer.GetLength(), &dwBytesWritten))
	{
		Displayf("Error on InternetWriteFile %d\n", GetLastError());
		Assert(0);
		delete [] pFileBuffer;
#if USE_BASE64
		delete [] pEncodedBuffer;
#endif	// USE_BASE64
		//CFileMgr::CloseFile(fileHandle);
		delete[] wszFileName;
		CloseHandle(hWMV);
		return HRESULT_FROM_WIN32(GetLastError());
	}
	delete [] pFileBuffer;
#if USE_BASE64
	delete [] pEncodedBuffer;
#endif	// USE_BASE64
	//CFileMgr::CloseFile(fileHandle);
	delete[] wszFileName;
	CloseHandle(hWMV);

	return S_OK;
}

HRESULT HttpUpload::UploadAssets(HINTERNET hRequest, atArray<char*> aFileList)
{
	GenerateMultipartBoundary(m_sBoundary);
	
	//request header
	atString szPostHeader = "Content-Type: multipart/form-data; boundary=";
	szPostHeader += m_sBoundary;
	if(!::HttpAddRequestHeaders(hRequest, szPostHeader.c_str(), szPostHeader.length(), HTTP_ADDREQ_FLAG_ADD))
	{
#if !__FINAL
		DWORD errCode = GetLastError();
		Displayf("HttpAddRequestHeaders failed. Error = %d\n", errCode);
#endif
	}

	u32 iContentLength = CalculateContentLength(aFileList);

	INTERNET_BUFFERS BufferIn;
	DWORD dwBytesWritten;
	//BOOL bRet;
	BufferIn.dwStructSize = sizeof( INTERNET_BUFFERS ); // Must be set or error will occur
	BufferIn.Next = NULL; 
	BufferIn.lpcszHeader = NULL;
	BufferIn.dwHeadersLength = 0;
	BufferIn.dwHeadersTotal = 0;
	BufferIn.lpvBuffer = NULL;                
	BufferIn.dwBufferLength = 0;
	BufferIn.dwBufferTotal = iContentLength; // This is the only member used other than dwStructSize
	BufferIn.dwOffsetLow = 0;
	BufferIn.dwOffsetHigh = 0;

	if(::HttpSendRequestEx(hRequest, &BufferIn, NULL, 0, 0))
	{
		HRESULT hr;

		atString accmodeField = GenerateAccmodField(m_sBoundary);
		Displayf(accmodeField);
		if(!::InternetWriteFile(hRequest, accmodeField, accmodeField.GetLength(), &dwBytesWritten))
		{

			Displayf("Error on InternetWriteFile %d\n", GetLastError());
			Assert(0);
			return HRESULT_FROM_WIN32(GetLastError());
		}

		//WCHAR wszFileName[RAGE_MAX_PATH];
		atString szTitle = aFileList[0];
		atString displayName = szTitle;
		if(displayName.EndsWith(".wmv"))
		{
			// remove .wmv extension.		
			szTitle.Truncate(szTitle.length() - 4);
			displayName.Reset();
			displayName = szTitle;
			// remove _WEB extension.
			int lastUnderscoreIndex = -1;
			for(int i = displayName.length()-1; i > 0; i--)
			{
				if(displayName[i] == '_')
				{
					lastUnderscoreIndex = i;
					break;
				}
			}

			if( lastUnderscoreIndex > 0)
			{
				szTitle.Truncate(lastUnderscoreIndex);
			}
			//CTextConversion::ConvertToWideChar(szTitle, wszFileName, RAGE_MAX_PATH);
		}

		// send out fields first
		for(int i =0; i < MPF_MAX; i++)
		{
			if( i == 0)
			{
				hr = UploadField(hRequest, m_sBoundary, sMultPartFiledName[i], szTitle.c_str());
			}
			else
			{
				hr = UploadField(hRequest, m_sBoundary, sMultPartFiledName[i], sMultPartFiledValue[m_iVideoEpisode][i]);
			}
			if (FAILED(hr))
			{
				Assert(0);
				return hr;
			}
		}

		CFileMgr::SetUserDataDir("Videos\\rendered");
		for(int i = 0; i < aFileList.GetCount(); i++)
		{
			m_pUiUpload->SetUploadingFileNumber(i);
			char szName[32] = {"File"};
			snprintf(szName+strlen(szName), 31, "%d", i);
			hr = UploadFile(hRequest, m_sBoundary, szName, aFileList[i]);
			if (FAILED(hr))
			{
				Assert(0);
				return hr;
			}

			char szImageName[32] = {"Image"};
			snprintf(szImageName+strlen(szImageName), 31,"%d", i);
			atString szImageFile = aFileList[i];
			szImageFile.Truncate(szImageFile.length()-3);
			szImageFile += "tag";
			hr = UploadImageFile(hRequest, m_sBoundary, szImageName, szImageFile);
			if (FAILED(hr))
			{
				Assert(0);
				return hr;
			}
		}
		
		Displayf("-----Finish sending files");

		// prebody trailer here
		atString preBodyTrailer = GeneratePreBodyTrailer(m_sBoundary);
		Displayf(preBodyTrailer);
		if(!::InternetWriteFile(hRequest, preBodyTrailer, preBodyTrailer.GetLength(), &dwBytesWritten))
		{
			Displayf("Error on InternetWriteFile %d\n", GetLastError());
			Assert(0);
			return HRESULT_FROM_WIN32(GetLastError());
		}

		atString bodyTrailer = GenerateBodyTrailer(m_sBoundary); 
		Displayf(bodyTrailer);
		if(!::InternetWriteFile(hRequest, bodyTrailer, bodyTrailer.GetLength(), &dwBytesWritten))
		{
			Displayf("Error on InternetWriteFile %d\n", GetLastError());
			Assert(0);
			return HRESULT_FROM_WIN32(GetLastError());
		}

		if(!::HttpEndRequest(hRequest, NULL, 0, 0))
		{
			if ( ERROR_INTERNET_FORCE_RETRY == GetLastError() )
			{
				Displayf("Should try to resend data");
				Assert(0);
			}
			else
			{
				Displayf("Error on InternetWriteFile %d\n", GetLastError());
				Assert(0);
			}
			return HRESULT_FROM_WIN32(GetLastError());
		}
#if __DEV
		char aQueryBuf[2048];
		DWORD dwQueryBufLen = sizeof(aQueryBuf);
		BOOL bQuery = ::HttpQueryInfo(hRequest,HTTP_QUERY_RAW_HEADERS_CRLF, aQueryBuf, &dwQueryBufLen, NULL);
		if (bQuery)
		{
			// The query succeeded, specify memory needed for file
			Displayf(aQueryBuf);
		}
		else
		{
			DWORD errCode = GetLastError();
			Displayf("HttpQueryInfo failed. Error = %d\n", errCode);
		}
#endif	// __DEV
		// get response from server
		atString response;
		if(ReadResponseBody(hRequest, &response))
		{
			Displayf("%s\n", response);
			char* pUrlString = rage_new char[response.GetLength()+1];
			strcpy(pUrlString, response.c_str());
			char* pUrl = strtok(pUrlString, "\"");
			if(pUrl)
			{
				pUrl = strtok(NULL, "\"");
			}
			if(pUrl)
			{
				m_sFinishUrlData = pUrl;
			}
			delete [] pUrlString;
		}
		else
		{
			DWORD errCode = GetLastError();
			Displayf("InternetReadFile failed. Error = %d\n", errCode);
			Assert(0);
			return HRESULT_FROM_WIN32(errCode);
		}
	}
	else
	{
		DWORD errCode = GetLastError();
		Displayf("HttpSendRequestEx failed. Error = %d\n", errCode);
		Assert(0);
		return HRESULT_FROM_WIN32(errCode);
	}

	return S_OK;
}

void HttpUpload::DoUpload(atArray<char*> aFileList, int iEpisodeIndex)
{
	m_aFileLis = aFileList;
	//m_StopWorkerThread = false;
	Assert(iEpisodeIndex < MAX_EPISODES);
	m_iVideoEpisode = iEpisodeIndex;
	//TODO4FIVE CFileMgr::SetUserAppEpisodeDirectory(iEpisodeIndex);
}


bool HttpUpload::Perform()
{
	//the sequence of the functions
	//InternetOpen()
	//InternetConnect()
	//InternetOpenRequest()
	//HttpAddRequestHeaders()
	//HttpSendRequestEx()
	//InternetWriteFile()
	//HttpEndrequest()
	//HttpQueryInfo()
	//InternetreadFile()
	//InterNetCloseHandle()

	//DownTextList();	// test

	if(!m_sUserName || !m_sPassword)
	{
		Displayf("Social club username or password is NOT set for video uploading!");
		Assert(0);
		return false;
	}

	char szUrlHostName[INTERNET_MAX_HOST_NAME_LENGTH];
	char szUrlPath[INTERNET_MAX_URL_LENGTH];
	URL_COMPONENTS myUrl;
	memset(&myUrl, 0, sizeof(URL_COMPONENTS));
	myUrl.dwStructSize = sizeof(URL_COMPONENTS);
	myUrl.lpszHostName = szUrlHostName;
	myUrl.dwHostNameLength = sizeof(szUrlHostName);
	myUrl.lpszUrlPath = szUrlPath;
	myUrl.dwUrlPathLength = sizeof(szUrlPath);
#if USE_SSL
	char sUrl [] = "http://mls.rockstargames.com:443/auth"; 
#else
	char sUrl [] = "http://mls.rockstargames.com:7777/auth"; 
	//char sUrl [] = "http://209.20.83.50:7777/auth"; 
#endif	// USE_SSL
	::InternetCrackUrl(sUrl,sizeof(sUrl), ICU_DECODE, &myUrl);

	bool bReturn = true;
	char sHeader[] = "Content-Type: application/x-www-form-urlencoded; charset=utf-8;";
	//char* AcceptTypes[2]={"*/*", 0}; 

	HINTERNET hInternet = ::InternetOpen("GTA4 Video Upload", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if(!hInternet)
	{
		Displayf("Failed to open session");
		Assert(0);
		return false;
	}

	HINTERNET hConnect = ::InternetConnect(hInternet, myUrl.lpszHostName, myUrl.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
	if(!hConnect)
	{
		Displayf("Failed to connect\n");
		Assert(0);
		InternetCloseHandle(hInternet);
		return false;
	}
	
	/////////////////////////////////////////////////////////////////////////////////////////////
	Displayf("Video upload: start login...\n");
	atString sUserId = "username=";
	sUserId += m_sUserName;
	sUserId += "&password=";
	sUserId += m_sPassword;
	Displayf(sUserId);
	if(!SendRequest(hConnect, myUrl.lpszUrlPath, sHeader, sUserId, strlen(sUserId)))
	{
		::InternetCloseHandle(hConnect);
		::InternetCloseHandle(hInternet);
		return false;
	}

	if(m_bDownTextList)
	{
#if DOWNLOAD_LIST
		Displayf("Downloading text list...\n");
		atString listData = "lang=";
#if __RUSSIAN_BUILD
		listData += "ru";
#else
		if (!TheText.IsRussian())
			listData += "en";
		else
			listData += "ru";
#endif	// __RUSSIAN_BUILD
		listData += "&session_id=";
		listData += m_sSessionId;
		if(!SendRequest(hConnect, "api/get_newlist_url_ex", sHeader, listData, listData.GetLength()))
		{
			Displayf("Could get new list URL!");
			Assert(0);
		}

		if(strncmp(m_szTextUrl, "", 256) != 0)
		{
			DownTextList();
		}
#endif	// DOWNLOAD_LIST

		if(!SendRequest(hConnect, "api/get_limits", sHeader, listData, listData.GetLength()))
		{
			Displayf("Could get get_limits URL!");
			Assert(0);
		}


		::InternetCloseHandle(hConnect);
		::InternetCloseHandle(hInternet);
		return true;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////
	Displayf("Video upload: Register upload...\n");
	atString aRegisterUpload;
	aRegisterUpload += "session_id=";
	aRegisterUpload += m_sSessionId;
	aRegisterUpload += "&user_id=";
	aRegisterUpload += m_sOpusId;
	aRegisterUpload += "&return_url=localhost";
	Displayf(aRegisterUpload);
	if(!SendRequest(hConnect, "api/register_upload", sHeader, aRegisterUpload, strlen(aRegisterUpload)))
	{
		::InternetCloseHandle(hConnect);
		::InternetCloseHandle(hInternet);
		return false;
	}
	/////////////////////////////////////////////////////////////////////////////////////////////
	if(!m_sSessionId || !m_sUploadId)
	{
		::InternetCloseHandle(hConnect);
		::InternetCloseHandle(hInternet);
		return false;
	}

	Displayf("Video upload: Upload post URL...\n");
	aRegisterUpload.Reset();
	aRegisterUpload += "session_id=";
	aRegisterUpload += m_sSessionId;
	aRegisterUpload += "&upload_id=";
	aRegisterUpload += m_sUploadId;
	Displayf(aRegisterUpload);
	if(!SendRequest(hConnect, "api/get_upload_post_url", sHeader, aRegisterUpload, strlen(aRegisterUpload), true))
	{
		::InternetCloseHandle(hConnect);
		::InternetCloseHandle(hInternet);
		return false;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////
	Displayf("Video upload: start uploading video...\n");
	URL_COMPONENTS uploadUrl;
	memset(&uploadUrl, 0, sizeof(URL_COMPONENTS));
	uploadUrl.dwStructSize = sizeof(URL_COMPONENTS);
	uploadUrl.lpszHostName = szUrlHostName;
	uploadUrl.dwHostNameLength = sizeof(szUrlHostName);
	uploadUrl.lpszUrlPath = szUrlPath;
	uploadUrl.dwUrlPathLength = sizeof(szUrlPath);
	//::InternetCrackUrl(m_sUploadUrl,strlen(m_sUploadUrl)+1, ICU_DECODE, &uploadUrl);
	::InternetCrackUrl(m_sUploadUrlData,strlen(m_sUploadUrlData)+1, ICU_DECODE, &uploadUrl);
#if USE_SSL
	HINTERNET hUploadConnect = ::InternetConnect(hInternet, uploadUrl.lpszHostName, INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
#else
	HINTERNET hUploadConnect = ::InternetConnect(hInternet, uploadUrl.lpszHostName, uploadUrl.nPort /*INTERNET_DEFAULT_HTTPS_PORT*/, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
#endif	// USE_SSL

	if(!hUploadConnect)
	{
		Displayf("Failed to connect\n");
		Assert(0);
		InternetCloseHandle(hInternet);
		return false;
	}
	DWORD dwOpenRequestFlags = INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP |
		INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS | 
		INTERNET_FLAG_KEEP_CONNECTION |
		INTERNET_FLAG_NO_AUTH |
		INTERNET_FLAG_NO_AUTO_REDIRECT |
		INTERNET_FLAG_NO_COOKIES |
		INTERNET_FLAG_NO_UI |
#if USE_SSL
		INTERNET_FLAG_SECURE|	// uses SSL/TLS transaction semantics
#endif	// USE_SSL
		INTERNET_FLAG_RELOAD;
	//HINTERNET hRequest = ::HttpOpenRequest(hUploadConnect, "POST", uploadUrl.lpszUrlPath, NULL, NULL, NULL, dwOpenRequestFlags, 0);
	HINTERNET hRequest = ::HttpOpenRequest(hUploadConnect, "POST", uploadUrl.lpszUrlPath, NULL, NULL, NULL, dwOpenRequestFlags, 0);
	if(!hRequest)
	{
#if !__FINAL
		DWORD errCode = GetLastError();
		Displayf("HttpOpenRequest failed. Error = %d\n", errCode);
#endif
		Assert(0);
	}

#if USE_SSL
	if(!SetupSecurityOptions(hRequest))
	{
		Displayf("Could set up internet options for SSL.");
		Assert(0);
		::InternetCloseHandle(hUploadConnect);
		::InternetCloseHandle(hConnect);
		::InternetCloseHandle(hInternet);
		return false;
	}
#endif	// USE_SSL

	m_pUiUpload->StartUploadAssets();
	HRESULT hr = UploadAssets(hRequest, m_aFileLis);
	if (FAILED(hr))
	{
		::InternetCloseHandle(hUploadConnect);
		::InternetCloseHandle(hConnect);
		::InternetCloseHandle(hInternet);
		return false;
	}

	/////////////////////////////////////////////////////////////////////////////////////////////
	Displayf("Video upload: LiveFeed...\n");
	::InternetCloseHandle(hConnect);
	memset(&uploadUrl, 0, sizeof(URL_COMPONENTS));
	uploadUrl.dwStructSize = sizeof(URL_COMPONENTS);
	uploadUrl.lpszHostName = szUrlHostName;
	uploadUrl.dwHostNameLength = sizeof(szUrlHostName);
	uploadUrl.lpszUrlPath = szUrlPath;
	uploadUrl.dwUrlPathLength = sizeof(szUrlPath);
	::InternetCrackUrl(m_sFinishUrlData,m_sFinishUrlData.GetLength()+1, ICU_DECODE, &uploadUrl);
#if USE_SSL
	hConnect = ::InternetConnect(hInternet, uploadUrl.lpszHostName, INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
#else
	hConnect = ::InternetConnect(hInternet, uploadUrl.lpszHostName, uploadUrl.nPort /*INTERNET_DEFAULT_HTTPS_PORT*/, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
#endif	// USE_SSL
	if(!hConnect)
	{
		Displayf("Connect to %s failed.", uploadUrl.lpszHostName);
		Assert(0);
		InternetCloseHandle(hInternet);
		return false;
	}
	ParsePostUrl(m_sUploadUrlData);
	aRegisterUpload.Reset();
	aRegisterUpload += "uid=";
	aRegisterUpload += m_sUid;

#if USE_SSL
	if(!SetupSecurityOptions(hRequest))
	{
		Displayf("Could set up internet options for SSL.");
		Assert(0);
		::InternetCloseHandle(hUploadConnect);
		::InternetCloseHandle(hConnect);
		::InternetCloseHandle(hInternet);
		return false;
	}
#endif	// USE_SSL

	Displayf(aRegisterUpload);
	if(!SendRequest(hConnect, uploadUrl.lpszUrlPath, sHeader, aRegisterUpload, strlen(aRegisterUpload), false, false))
	{
		Displayf("Live feed failed");
		Assert(0);

		::InternetCloseHandle(hConnect);
		::InternetCloseHandle(hInternet);
		return false;
	}
	
	m_pUiUpload->SetMlsUpid(m_sSessionId, m_sUploadId);

	::InternetCloseHandle(hRequest);
	::InternetCloseHandle(hUploadConnect);
	::InternetCloseHandle(hConnect);
	::InternetCloseHandle(hInternet);

	return bReturn;
}
INTERNET_BUFFERS HttpUpload::BufferIn;
HINTERNET HttpUpload::hRequest = 0;

DWORD WINAPI HttpUpload::WorkerFunction(IN LPVOID vThreadParm)
{

	if ( !(::HttpSendRequestEx(hRequest, &BufferIn, NULL, 0, 0))) 
	{
		//cerr << "Error on InternetConnnect: " << GetLastError() << endl;
		return 1; // failure
	}
	return 0;  // success

}

bool HttpUpload::SendRequest(HINTERNET hConnect, const char* szUrlPath, const char* szHeader, const char* data, u32 iDataSize, bool bPostUrl, bool bXmlParse)
{
	if( data && iDataSize == 0 )
	{
		Assert(0);
		return false;
	}
	DWORD dwOpenRequestFlags = INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP |
		INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS | 
		INTERNET_FLAG_KEEP_CONNECTION |
		INTERNET_FLAG_NO_AUTH |
		INTERNET_FLAG_NO_AUTO_REDIRECT |
		INTERNET_FLAG_NO_COOKIES |
		INTERNET_FLAG_NO_UI |
		INTERNET_FLAG_NO_CACHE_WRITE |	// (http://support.microsoft.com/kb/177188/EN-US/)
#if USE_SSL
		INTERNET_FLAG_SECURE|	// uses SSL/TLS transaction semantics
		//INTERNET_FLAG_IGNORE_CERT_CN_INVALID |
		//INTERNET_FLAG_IGNORE_CERT_DATE_INVALID |
#endif	// USE_SSL
		INTERNET_FLAG_RELOAD;

	hRequest = ::HttpOpenRequest(hConnect, "POST", szUrlPath, NULL, NULL, NULL, dwOpenRequestFlags/*INTERNET_FLAG_NO_CACHE_WRITE*/, 0);
	if(!hRequest)
	{
		// TODO: display fail message.
		Displayf("HttpOpenRequest failed. Error = %d", GetLastError());
		Assert(0);
		return false;
	}

	unsigned long iTimeOut = 20000;
	::InternetSetOption(hRequest, INTERNET_OPTION_SEND_TIMEOUT, &iTimeOut, sizeof(iTimeOut) );
	::InternetSetOption(hRequest, INTERNET_OPTION_RECEIVE_TIMEOUT, &iTimeOut, sizeof(iTimeOut) );


	bool bReturn = true;
	if(!::HttpAddRequestHeaders(hRequest, szHeader, strlen(szHeader), HTTP_ADDREQ_FLAG_ADD))
	{
		Displayf("HttpAddRequestHeaders failed. Error = %d",  GetLastError());
		Assert(0);
		return false;
	}

#if USE_SSL
	if(!SetupSecurityOptions(hRequest))
	{
		Displayf("Could set up internet options for SSL.");
		Assert(0);
		return false;
	}
#endif	// USE_SSL

	//INTERNET_BUFFERS BufferIn;
	DWORD dwBytesWritten;
	BOOL bRet;
	BufferIn.dwStructSize = sizeof( INTERNET_BUFFERS ); // Must be set or error will occur
	BufferIn.Next = NULL; 
	BufferIn.lpcszHeader = NULL;
	BufferIn.dwHeadersLength = 0;
	BufferIn.dwHeadersTotal = 0;
	BufferIn.lpvBuffer = NULL;                
	BufferIn.dwBufferLength = 0;
	BufferIn.dwBufferTotal = iDataSize; // This is the only member used other than dwStructSize
	BufferIn.dwOffsetLow = 0;
	BufferIn.dwOffsetHigh = 0;

	::InternetSetOption(hRequest, INTERNET_OPTION_SEND_TIMEOUT, &iTimeOut, sizeof(iTimeOut) );
	::InternetSetOption(hRequest, INTERNET_OPTION_RECEIVE_TIMEOUT, &iTimeOut, sizeof(iTimeOut) );

	DWORD dwSize = sizeof(iTimeOut);
	::InternetQueryOption(hRequest, INTERNET_OPTION_RECEIVE_TIMEOUT, &iTimeOut,&dwSize);

	// Create a worker thread 
	HANDLE   hThread; 
	DWORD    dwThreadID;

	hThread = CreateThread(
		NULL,            // Pointer to thread security attributes 
		0,               // Initial thread stack size, in bytes 
		&WorkerFunction,  // Pointer to thread function 
		NULL,     // The argument for the new thread
		0,               // Creation flags 
		&dwThreadID      // Pointer to returned thread identifier 
	);

	// Wait for worker function to complete
	int dwTimeout = 15000; // in milliseconds
#if __DEV
	// shorten the timeout time for DEV
	dwTimeout = 5000;
#endif	// __DEV

	DWORD dwExitCode = 0;
	if ( WaitForSingleObject ( hThread, dwTimeout ) == WAIT_TIMEOUT )
	{
		Displayf("Can not connect to server in %d milliseconds",dwTimeout);
		if ( hRequest )
			InternetCloseHandle ( hRequest );
		// Wait until the worker thread exits
		WaitForSingleObject ( hThread, 30000 );
		Displayf("Thread has exited");
		dwExitCode = 1;
	}

	// The state of the specified object (thread) is signaled
	//dwExitCode = 0;
	if ( !GetExitCodeThread( hThread, &dwExitCode ) )
	{
		Displayf("CError on GetExitCodeThread: %d",GetLastError());
		dwExitCode = 1;
	}

	if (hThread)
	{
		CloseHandle (hThread);
	}

	if ( !dwExitCode )
	{
		// need to send out file header first
		if(m_bSendingFile)
		{
			atString fileHeader = "--";
			fileHeader += m_sBoundary;
			fileHeader += "\r\n";
			fileHeader += "Content-Disposition: form-data; name=\""; // %hs\"; filename=\"%hs\""
			fileHeader += m_sFileName;
			fileHeader += "\"; filename=\"";
			fileHeader += CFileMgr::GetRootDirectory();
			fileHeader += m_sFileName;
			fileHeader += "\"\r\n";
			fileHeader += szHeader;
			fileHeader += "\r\n";
			fileHeader += "\r\n";
			if(!::InternetWriteFile(hRequest, fileHeader, strlen(fileHeader), &dwBytesWritten))
			{
				Assert(0);
				return false;
			}
		}
Again:
		if(data && iDataSize > 0)
		{
			bRet = ::InternetWriteFile(hRequest, data, iDataSize, &dwBytesWritten);
			if(!bRet)
			{
				Displayf("Error on InternetWriteFile %d\n", GetLastError());
				Assert(0);
				bReturn = false;
			}
		}
		if(!::HttpEndRequest(hRequest, NULL, 0, 0))
		{
			if ( ERROR_INTERNET_FORCE_RETRY == GetLastError() )
			{
				goto Again;
			}
			else
			{
				Displayf("Error on HttpEndRequest %d\n", GetLastError());
				Assert(0);
				bReturn = false;
			}
		}
#if __DEV
		char aQueryBuf[2048];
		DWORD dwQueryBufLen = sizeof(aQueryBuf);
		BOOL bQuery = ::HttpQueryInfo(hRequest,HTTP_QUERY_RAW_HEADERS_CRLF, aQueryBuf, &dwQueryBufLen, NULL);
		if (bQuery)
		{
			// The query succeeded, specify memory needed for file
			Displayf(aQueryBuf);
		}
		else
		{
			DWORD errCode = GetLastError();
			Displayf("HttpQueryInfo failed. Error = %d\n", errCode);
		}
#endif	// __DEV

		// get response from server
		atString response;
		if(!ReadResponseBody(hRequest, &response))
		{
			::InternetCloseHandle(hRequest);
			return false;
		}

		if(response.GetLength() > 0)
		{
			Displayf("%s\n", response);
			
			if( bXmlParse)
			{
				if(strstr(response, "xml version=") && strstr(response, "response"))
				{
#if __DEV
					CFileMgr::SetUserDataDir("Videos\\rendered");
					const char szFavoriteName[] = "temp.xml";
					FileHandle hFile = CFileMgr::OpenFileForWriting(szFavoriteName);
					if( !CFileMgr::IsValidFileHandle(hFile) )
					{
						Assert(0);
					}
					CFileMgr::Write(hFile, response.c_str(), response.GetLength()+1);
					CFileMgr::CloseFile(hFile);
#endif	// __DEV

					char buf[RAGE_MAX_PATH];
					fiDevice::MakeMemoryFileName(buf,sizeof(buf),response.c_str(),response.GetLength()+1,false,"temp.xml");

					// pStream is closed in ExtractSessionID.
					fiStream* pStream = fiStream::Open(buf,fiDeviceMemory::GetInstance(),true);

					if(m_bSendingFile)
					{
						Displayf("Received after sending video file");
					}
					if(!m_bSendingFile && !ExtractSessionID(pStream, bPostUrl))
					{
						bReturn = false;
					}
				}
				else
				{
					// internal server error
					Errorf("Error from server: %s\n", response);
					bReturn = false;
				}
			}
		}
	}
	else
	{
#if !__FINAL
		DWORD errCode = GetLastError();
		Displayf("HttpSendRequestEx failed. Error = %d\n", errCode);
#endif
		Assert(0);
		bReturn = false;
	}

	if (hRequest)
	{
		::InternetCloseHandle(hRequest);
	}
	return bReturn;
}

bool HttpUpload::SetupSecurityOptions(HINTERNET hRequest)
{
	//DWORD dwFlag;
	//DWORD dwBuffLen = sizeof(dwFlag);

	//if(!InternetQueryOption (hRequest, INTERNET_OPTION_SECURITY_FLAGS,(LPVOID)&dwFlag, &dwBuffLen))
	//{
	//	return false;
	//}

	//dwFlag |= (SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID );

	//if(!InternetSetOption(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlag, sizeof (dwFlag) ))
	//{
	//	return false;
	//}

#if __DEV
	INTERNET_CERTIFICATE_INFO sInfo;
	DWORD dwSize = sizeof(sInfo);

	if(!InternetQueryOption(hRequest,INTERNET_OPTION_SECURITY_CERTIFICATE_STRUCT, 
		&sInfo, &dwSize))
	{

		DWORD errCode = GetLastError();
		Displayf("InternetQueryOption failed. Error = %d\n", errCode);
	}
	else
	{
		Displayf("Subject info: %s. Issuer info: %s.", sInfo.lpszSubjectInfo, sInfo.lpszIssuerInfo);
	}
#endif	// __DEV

	return true;
}

void HttpUpload::UrlEncode(const char* szIn, char* szOut, u32 iOutLength)
{
	Assert(szIn != NULL && strlen(szIn) < iOutLength);
	
	szOut[0] = '\0';

	char c;
	char hex[4] = "%00";
	for(int i = 0 ; (c = szIn[i]) != 0 ; i++)
	{
		if(strchr(UPLOAD_LEGAL_URLENCODED_CHARS, c))
		{
			// Legal.
			/////////
			int iStrLen = strlen(szOut); 
			szOut[iStrLen] = c;
			szOut[iStrLen+1] = '\0';
		}
		else
		{
			// To hex.
			//////////
			Assert((c / 16) < 16);
			hex[1] = UPLOAD_DIGITS[c / 16];
			hex[2] = UPLOAD_DIGITS[c % 16];
			strcat(szOut, hex);
		}
		Assert(strlen(szOut) < iOutLength);
	}
}

bool HttpUpload::ExtractUserInfo(const char* szFileName, const char* ext, fiStream* fiHandle)
{
	parTree* pTree = NULL;

	if(!fiHandle)
	{
		fiSafeStream S(ASSET.Open(szFileName, ext));
		if(S)
		{
			pTree = PARSER.LoadTree(S);
		}
	}
	else
	{
		pTree = PARSER.LoadTree(fiHandle);
		fiHandle->Close();
	}

	if(pTree)
	{
		parTreeNode* pRootNode = pTree->GetRoot();
		if(pRootNode)
		{
			parTreeNode::ChildNodeIterator it = pRootNode->BeginChildren();
			bool bUserName = false;
			bool bPass = false;
			for(; it != pRootNode->EndChildren(); ++it)
			{
				if(stricmp((*it)->GetElement().GetName(), "Username") == 0)
				{
					if((*it)->HasData())
					{
						char* pData = (*it)->GetData();
						if(pData != NULL && strcmp(pData, "") != 0)
						{
							strcpy(m_sUserName, pData);
							bUserName = true;
						}
					}
				}
				else if(stricmp((*it)->GetElement().GetName(), "Password") == 0)
				{
					if((*it)->HasData())
					{
						char* pData = (*it)->GetData();
						if(pData != NULL && strcmp(pData, "") != 0)
						{
							strcpy(m_sPassword, pData);
							bPass = true;
						}
					}
				}
				else if(stricmp((*it)->GetElement().GetName(), "Nickname") == 0)
				{
					if((*it)->HasData())
					{
						char* pData = (*it)->GetData();
						if(pData != NULL && strcmp(pData, "") != 0)
						{
							m_sNickName = pData;
						}
					}
				}
				else if(stricmp((*it)->GetElement().GetName(), "MemberId") == 0)
				{
					if((*it)->HasData())
					{
						char* pData = (*it)->GetData();
						if(pData != NULL && strcmp(pData, "") != 0)
						{
							m_sMemberId = pData;
						}
					}
				}
				else if(stricmp((*it)->GetElement().GetName(), "AuthToken") == 0)
				{
					if((*it)->HasData())
					{
						char* pData = (*it)->GetData();
						if(pData != NULL && strcmp(pData, "") != 0)
						{
							m_sAuthToken = pData;
						}
					}
				}
			}
			
			delete pTree;
			if( bUserName && bPass)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			delete pTree;
		}
	}
	return false;
}

bool HttpUpload::ExtractSessionID(fiStream* fiHandle, bool bPostUrl)
{
	if(!fiHandle)
	{
		Assert(0);
		return false;
	}

	parTree* pTree = PARSER.LoadTree(fiHandle);
	fiHandle->Close();
	if(pTree)
	{
		parTreeNode* pRootNode = pTree->GetRoot();
		if(pRootNode)
		{
			parTreeNode::ChildNodeIterator it = pRootNode->BeginChildren();
			for(; it != pRootNode->EndChildren(); ++it)
			{
				if(stricmp((*it)->GetElement().GetName(), "error") == 0)
				{
					Displayf("Error in response");
					Errorf("Unable to receive a valid response from the server!");
					return false;
				}
				else if(stricmp((*it)->GetElement().GetName(), "session_id") == 0)
				{	
					Displayf("session_id: ");
					if((*it)->HasData())
					{
						Displayf("%s\n", (*it)->GetData());
						strcpy(m_sSessionId, (*it)->GetData());
					}
					else
					{
						Displayf("couldn't find session ID");
						Assert(0);
					}
				}
				else if(stricmp((*it)->GetElement().GetName(), "opus_id") == 0)
				{
					Displayf("opus_id: ");

					parTreeNode::ChildNodeIterator wit = (*it)->BeginChildren();
					for(; wit != (*it)->EndChildren(); ++wit)
					{
						if(stricmp((*wit)->GetElement().GetName(), "id") == 0)
						{
							if((*wit)->HasData())
							{
								const char* pData = (*wit)->GetData();
								Displayf("%s\n", pData);
								if(!m_sOpusId)
								{
									m_sOpusId = rage_new char[strlen(pData)+1];
									m_sOpusId[0] = '\0';
								}
								strcpy(m_sOpusId, pData);
							}
							else
							{
								Displayf("couldn't find opus ID");
								Assert(0);
							}
						}
					}
				}
				else if(stricmp((*it)->GetElement().GetName(), "result") == 0)
				{	
					Displayf("Upload ID: ");
					if((*it)->HasData())
					{
						Displayf("%s\n", (*it)->GetData());
						if(bPostUrl)
						{
							const char* pData = (*it)->GetData();
							if(!m_sUploadUrlData)
							{
								m_sUploadUrlData = rage_new char[strlen(pData)+1];
								m_sUploadUrlData[0] = '\0';
							}
							strcpy(m_sUploadUrlData, pData);
							//ParsePostUrl(m_sUploadUrlData);
						}
						else
						{
							const char* pData = (*it)->GetData();
							if(!m_sUploadId)
							{
								m_sUploadId = rage_new char[strlen(pData)+1];
								m_sUploadId[0] = '\0';
							}
							strcpy(m_sUploadId, (*it)->GetData());
						}
					}
					else
					{
						Displayf("couldn't find Upload ID");
						Assert(0);
					}
				}
				else if(m_bDownTextList && stricmp((*it)->GetElement().GetName(), "location") == 0)
				{
					Displayf("Fitler text url: ");

					parTreeNode::ChildNodeIterator wit = (*it)->BeginChildren();
					for(; wit != (*it)->EndChildren(); ++wit)
					{
						if(stricmp((*wit)->GetElement().GetName(), "url") == 0)
						{
							if((*wit)->HasData())
							{
								Displayf("%s\n", (*wit)->GetData());
								strncpy(m_szTextUrl, (*wit)->GetData(), sizeof(m_szTextUrl));
							}
							else
							{
								Displayf("couldn't find Filter text url");
								Assert(0);
							}
						}
					}
				}
				else if(m_bDownTextList && stricmp((*it)->GetElement().GetName(), "limits") == 0)
				{
					Displayf("Upload limit: ");

					parTreeNode::ChildNodeIterator wit = (*it)->BeginChildren();
					for(; wit != (*it)->EndChildren(); ++wit)
					{
						if(stricmp((*wit)->GetElement().GetName(), "duration") == 0)
						{
							if((*wit)->HasData())
							{
								Displayf("duration: %s\n", (*wit)->GetData());
								m_szDuration.Reset();
								m_szDuration = (*wit)->GetData();
							}
						}
						else if(stricmp((*wit)->GetElement().GetName(), "file_size") == 0)
						{
							if((*wit)->HasData())
							{
								Displayf("file_size: %s\n", (*wit)->GetData());
								m_szFileSize.Reset();
								m_szFileSize = (*wit)->GetData();
							}
						}

					}
				}
			}
		}
		delete pTree;
		return true;
	}

	Displayf("Couldn't open file temp.xml");
	Assert(0);
	return false;
}

void HttpUpload::ParsePostUrl(char* aPostUrlData)
{
	Assert(aPostUrlData != NULL);
	m_sUploadUrl = strtok(aPostUrlData, "?");
	if(m_sUploadUrl)
	{
		m_sProgressid = strtok(NULL, "=");
		if(m_sProgressid)
		{
			m_sProgressid = strtok(NULL, "&");
			if(m_sProgressid)
			{
				m_sUid = strtok(NULL, "=");
				if(m_sUid)
				{
					m_sUid = strtok(NULL, "=");
				}
			}
		}
	}
}

void HttpUpload::Base64EncodeByteTriple(char* p_pInputBuffer, u32 InputCharacters, char* p_pOutputBuffer)
{
	u32 mask = 0xfc000000;
	u32 buffer = 0;


	char* temp = (char*)&buffer;
	temp[3] = p_pInputBuffer[0];
	if (InputCharacters > 1)
		temp[2] = p_pInputBuffer[1];
	if (InputCharacters > 2)
		temp[1] = p_pInputBuffer[2];

	switch (InputCharacters)
	{
	case 3:
		{
			p_pOutputBuffer[0] = BASE64_ALPHABET[(buffer & mask) >> 26];
			buffer = buffer << 6;
			p_pOutputBuffer[1] = BASE64_ALPHABET[(buffer & mask) >> 26];
			buffer = buffer << 6;
			p_pOutputBuffer[2] = BASE64_ALPHABET[(buffer & mask) >> 26];
			buffer = buffer << 6;
			p_pOutputBuffer[3] = BASE64_ALPHABET[(buffer & mask) >> 26];
			break;
		}
	case 2:
		{
			p_pOutputBuffer[0] = BASE64_ALPHABET[(buffer & mask) >> 26];
			buffer = buffer << 6;
			p_pOutputBuffer[1] = BASE64_ALPHABET[(buffer & mask) >> 26];
			buffer = buffer << 6;
			p_pOutputBuffer[2] = BASE64_ALPHABET[(buffer & mask) >> 26];
			p_pOutputBuffer[3] = '=';
			break;
		}
	case 1:
		{
			p_pOutputBuffer[0] = BASE64_ALPHABET[(buffer & mask) >> 26];
			buffer = buffer << 6;
			p_pOutputBuffer[1] = BASE64_ALPHABET[(buffer & mask) >> 26];
			p_pOutputBuffer[2] = '=';
			p_pOutputBuffer[3] = '=';
			break;
		}
	}
}

void HttpUpload::Base64Encode(char* p_pInputBuffer, u32 p_InputBufferLength, char* p_pOutputBufferString)
{
	u32 FinishedByteQuartetsPerLine = 0;
	u32 InputBufferIndex  = 0;
	u32 OutputBufferIndex = 0;

	memset (p_pOutputBufferString, 0, RecquiredEncodeOutputBufferSize(p_InputBufferLength));

	while (InputBufferIndex < p_InputBufferLength)
	{
		if (p_InputBufferLength - InputBufferIndex <= 2)
		{
			FinishedByteQuartetsPerLine ++;
			Base64EncodeByteTriple(p_pInputBuffer + InputBufferIndex, p_InputBufferLength - InputBufferIndex, p_pOutputBufferString + OutputBufferIndex);
			break;
		}
		else
		{
			FinishedByteQuartetsPerLine++;
			Base64EncodeByteTriple(p_pInputBuffer + InputBufferIndex, 3, p_pOutputBufferString + OutputBufferIndex);
			InputBufferIndex  += 3;
			OutputBufferIndex += 4;
		}

		if (FinishedByteQuartetsPerLine == 19)
		{
			p_pOutputBufferString[OutputBufferIndex  ] = '\r';
			p_pOutputBufferString[OutputBufferIndex+1] = '\n';
			p_pOutputBufferString += 2;
			FinishedByteQuartetsPerLine = 0;
		}
	}
}

u32 HttpUpload::RecquiredEncodeOutputBufferSize (u32 p_InputByteCount)
{
	div_t result = div (p_InputByteCount, 3);

	u32 RecquiredBytes = 0;
	if (result.rem == 0)
	{
		// Number of encoded characters
		RecquiredBytes = result.quot * 4;

		// CRLF -> "\r\n" each 76 characters
		result = div (RecquiredBytes, 76);
		RecquiredBytes += result.quot * 2;

		// Terminating null for the Encoded String
		RecquiredBytes += 1;

		return RecquiredBytes;
	}
	else
	{
		// Number of encoded characters
		RecquiredBytes = result.quot * 4 + 4;

		// CRLF -> "\r\n" each 76 characters
		result = div (RecquiredBytes, 76);
		RecquiredBytes += result.quot * 2;

		// Terminating null for the Encoded String
		RecquiredBytes += 1;

		return RecquiredBytes;
	}
}

void
HttpUpload::Worker(void* p)
{
	HttpUpload* pHttpUpload = (HttpUpload*)p;
	
	sysIpcSignalSema(pHttpUpload->m_WaitSema);

	while(!pHttpUpload->IsUploadingFinished())
	{
		sysIpcWaitSema(pHttpUpload->m_WakeUpSema);

		if(pHttpUpload->IsUploadingFinished())
		{
			break;
		}

		if(!pHttpUpload->Perform())
		{
			UIUpload* pUiUplaod = pHttpUpload->GetUiUpload();
			if(pUiUplaod)
			{
				pUiUplaod->UploadFailed();
			}
		}

		pHttpUpload->SetUploadingFinished(true);
	}

	//Signal the calling thread that we've ended.
	sysIpcSignalSema(pHttpUpload->m_WaitSema);
}

void HttpUpload::RC4(unsigned char* data, int dataLen, const unsigned char* key, int keyLen)
{
	unsigned char s[256];
	memset(s, 0, 256);

	unsigned char k[256];
	memset(k, 0, 256);

	char temp;
	int i, j;

	for(i = 0; i < 256; i++)
	{
		s[i] = (unsigned char)i;
		k[i] = key[ i % keyLen ];
	}

	j=0;
	for(i = 0; i < 256; i++)
	{
		j = (j + s[i] + k[i]) % 256;
		temp = s[i];
		s[i] = s[j];
		s[j] = temp;
	}

	i = j = 0;
	for(int x = 0; x < dataLen; x++)
	{
		i = (i + 1) % 256;
		j = (j + s[i]) % 256;
		temp = s[i];
		s[i] = s[j];
		s[j] = temp;
		int t = (s[i] + s[j]) % 256;
		data[x] ^= s[t];
	}
}

bool HttpUpload::CanOpenConnection(const char* szUrl)
{
	m_bInternetConnectionValid = false;

	DWORD flags;
	if(!::InternetGetConnectedState(&flags, NULL))
	{
		Displayf("No Internet connection. Error: %d\n", GetLastError());
		return m_bInternetConnectionValid;
	}

	if(!::InternetCheckConnection(szUrl, FLAG_ICC_FORCE_CONNECTION, 0))
	{
		Displayf("The attempt to connect to %s failed. Error: %d", szUrl, GetLastError());
		return m_bInternetConnectionValid;
	}

	m_bInternetConnectionValid = true;
	return m_bInternetConnectionValid;
}

bool HttpUpload::IsRockstartSessionIdValid(char* pOutSessionId)
{
#if SECUROM_REQUIRED_CHECK
	CFileMgr::SetRockStartDir("");
	fiStream* fiUserInfo = NULL;
#if USE_ENCRYPTION
	fiUserInfo = DecryptUserInfo();
	if(!fiUserInfo)
	{
		return false;
	}
#endif	// USE_ENCRYPTION

	char sessionId[MAX_SESSION_ID_LENGTH] = {'\0'};
	if(!ExtractRockstartUserInfo(sessionId, NULL, NULL, "userinfo", "dat", fiUserInfo))
	{
		return false;
	}

	if(strcmp(sessionId, "") == 0)
	{
		return false;
	}

	if(pOutSessionId)
	{
		strcpy(pOutSessionId, sessionId);
	}
	return true;
#else
	return true;
#endif	// __FINAL
}

void HttpUpload::GetRockStartUserInfo(char* pOutSessionId, char* pOutUserName, char* pOutPassword)
{
	CFileMgr::SetRockStartDir("");
	fiStream* fiUserInfo = NULL;
#if USE_ENCRYPTION
	fiUserInfo = DecryptUserInfo();
	if(!fiUserInfo)
	{
		return;
	}
#endif	// USE_ENCRYPTION

	ExtractRockstartUserInfo(pOutSessionId, pOutUserName, pOutPassword, "userinfo", "dat", fiUserInfo);
}

bool HttpUpload::ExtractRockstartUserInfo(char* pOutSessionId, char* pOutUserName, char* pOutPassword, const char* szFileName, const char* ext, fiStream* fiHandle)
{
	parTree* pTree = NULL;
	
	if(!fiHandle)
	{
		fiSafeStream S(ASSET.Open(szFileName, ext));
		if(S)
		{
			pTree = PARSER.LoadTree(S);
		}
	}
	else
	{
		pTree = PARSER.LoadTree(fiHandle);
		fiHandle->Close();
	}

	if(pTree)
	{
		parTreeNode* pRootNode = pTree->GetRoot();
		if(pRootNode)
		{
			parTreeNode::ChildNodeIterator it = pRootNode->BeginChildren();
			for(; it != pRootNode->EndChildren(); ++it)
			{
				if(pOutSessionId && stricmp((*it)->GetElement().GetName(), "SessionId") == 0)
				{
					if((*it)->HasData())
					{
						char* pData = (*it)->GetData();
						if(pData != NULL && strcmp(pData, "") != 0)
						{
							strcpy(pOutSessionId, pData);
						}
					}

					break;
				}
				else if(pOutUserName && stricmp((*it)->GetElement().GetName(), "Username") == 0)
				{
					if((*it)->HasData())
					{
						char* pData = (*it)->GetData();
						if(pData != NULL && strcmp(pData, "") != 0)
						{
							strcpy(pOutUserName, pData);
						}
					}
				}
				else if(pOutPassword && stricmp((*it)->GetElement().GetName(), "Password") == 0)
				{
					if((*it)->HasData())
					{
						char* pData = (*it)->GetData();
						if(pData != NULL && strcmp(pData, "") != 0)
						{
							strcpy(pOutPassword, pData);
						}
					}
				}
			}

			delete pTree;

			return true;
		}
		else
		{
			delete pTree;
		}
	}
	return false;
}

// it is the responsibility of the calling application to free it with a call to LocalFree(). 
char* HttpUpload::EncryptData(char* pData, s32 iDatalength, s32 &iEncyptedLength)
{
	//Setup the encrypted data blob
	DATA_BLOB decryptedData;
	decryptedData.pbData = (BYTE*)&pData[0];
	decryptedData.cbData = iDatalength;

	//Setup the necessary CryptUnprotectData parameters
	//LPWSTR pDescrOut = NULL;
	DATA_BLOB encryptedData;

	CRYPTPROTECT_PROMPTSTRUCT PromptStruct;
	PromptStruct.cbSize = sizeof(PromptStruct);
	PromptStruct.dwPromptFlags = CRYPTPROTECT_PROMPT_ON_UNPROTECT;
	PromptStruct.szPrompt = L"This is a user prompt.";

	//Decrypt the data
	BOOL bRes = CryptProtectData( &decryptedData, NULL, NULL, NULL, NULL /*&PromptStruct*/, 0, &encryptedData);
	if(!bRes)
	{
		return NULL;
	}

	//Null-terminate the resulting decrypted string	
	encryptedData.pbData[encryptedData.cbData] = 0;
	iEncyptedLength = encryptedData.cbData;
	return (char*)encryptedData.pbData;
}

// it is the responsibility of the calling application to free it with a call to LocalFree(). 
char* HttpUpload::DecryptData(char* pData, s32 iDatalength, s32 &iDecyptedLength)
{
	//Setup the encrypted data blob
	DATA_BLOB encryptedData;
	encryptedData.pbData = (BYTE*)&pData[0];
	encryptedData.cbData = iDatalength;

	//Setup the necessary CryptUnprotectData parameters
	//LPWSTR pDescrOut = NULL;
	DATA_BLOB decryptedData;

	CRYPTPROTECT_PROMPTSTRUCT PromptStruct;
	PromptStruct.cbSize = sizeof(PromptStruct);
	PromptStruct.dwPromptFlags = CRYPTPROTECT_PROMPT_ON_PROTECT;
	PromptStruct.szPrompt = L"This is a user prompt.";

	//Decrypt the data
	BOOL bRes = CryptUnprotectData( &encryptedData, NULL, NULL, NULL, NULL /*&PromptStruct*/, 0, &decryptedData);
	if(!bRes)
	{
		Displayf("Couldn't decrypt data. Error: %d", GetLastError());
		Assert(0);
		return NULL;
	}
	//Null-terminate the resulting decrypted string	
	decryptedData.pbData[decryptedData.cbData] = 0;
	iDecyptedLength = decryptedData.cbData;

	return (char*)decryptedData.pbData;
}

fiStream* HttpUpload::DecryptUserInfo()
{
	CFileMgr::SetRockStartDir("");
	FileHandle fHandle = CFileMgr::OpenFile("userinfo.dat");
	if(!CFileMgr::IsValidFileHandle(fHandle))
	{
		return NULL;
	}
	
	s32 iSize = CFileMgr::GetTotalSize(fHandle);
	if(iSize <= 0)
	{
		return NULL;
	}
	char* pFileBuffer = rage_new char[iSize];
	s32 iBytesRead = CFileMgr::Read(fHandle, pFileBuffer, iSize);
	if(iBytesRead == -1)
	{
		delete [] pFileBuffer;
		CFileMgr::CloseFile(fHandle);
		return NULL;
	}
	CFileMgr::CloseFile(fHandle);

	s32 iDecryptDataLength = 0;
	char* pDecryptData = DecryptData(pFileBuffer, iBytesRead, iDecryptDataLength);
	delete [] pFileBuffer;
	pFileBuffer = NULL;

	if(pDecryptData == NULL)
	{
		return NULL;
	}
	
	// Don't want to track the LocalAlloc data. So copy LocalAlloc data, and LocalFree it.
	pFileBuffer = rage_new char[iDecryptDataLength];
	memcpy(pFileBuffer, pDecryptData, iDecryptDataLength);
	LocalFree(pDecryptData);

	Displayf(pFileBuffer);

	char buf[RAGE_MAX_PATH];
	fiDevice::MakeMemoryFileName(buf,sizeof(buf),pFileBuffer, iDecryptDataLength,true,"userinfo.dat");

	return fiStream::Open(buf,fiDeviceMemory::GetInstance(),true);
}

bool HttpUpload::ReadResponseBody(HINTERNET hRequest, atString *pOutBuf)
{
	char* pBuf = NULL;
	DWORD dwBytesAvailable;
	DWORD cbRead;
	pOutBuf->Reset();
	while(::InternetQueryDataAvailable(hRequest, &dwBytesAvailable, 0, 0) && dwBytesAvailable)
	{
		pBuf = rage_new char[dwBytesAvailable+1];
		if(!pBuf)
		{
			AssertMsg(0, "couldn't malloc enough memory");
			return false;
		}

		memset(pBuf, 0, dwBytesAvailable);
		if(!::InternetReadFile(hRequest, pBuf, dwBytesAvailable, &cbRead))
		{
			Assert(0);
			Displayf("%d", GetLastError());
			delete [] pBuf;
			return false;
		}

		if(cbRead == 0)
		{
			delete [] pBuf;
			break;
		}
		pBuf[cbRead] = '\0';
		(*pOutBuf) += pBuf;

		delete [] pBuf;
	}

	return true;
}

bool HttpUpload::DownTextList()
{
	char szUrlHostName[INTERNET_MAX_HOST_NAME_LENGTH];
	char szUrlPath[INTERNET_MAX_URL_LENGTH];
	URL_COMPONENTS myUrl;
	memset(&myUrl, 0, sizeof(URL_COMPONENTS));
	myUrl.dwStructSize = sizeof(URL_COMPONENTS);
	myUrl.lpszHostName = szUrlHostName;
	myUrl.dwHostNameLength = sizeof(szUrlHostName);
	myUrl.lpszUrlPath = szUrlPath;
	myUrl.dwUrlPathLength = sizeof(szUrlPath);
	::InternetCrackUrl(m_szTextUrl,strlen(m_szTextUrl)+1, ICU_DECODE, &myUrl);

	bool bReturn = true;

	HINTERNET hInternet = ::InternetOpen("GTA4 down text", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if(!hInternet)
	{
		Displayf("Failed to open session");
		Assert(0);
		return false;
	}

#if USE_SSL
	HINTERNET hConnect = ::InternetConnect(hInternet, myUrl.lpszHostName, INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
#else
	HINTERNET hConnect = ::InternetConnect(hInternet, myUrl.lpszHostName, myUrl.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
#endif	// USE_SSL
	
	if(!hConnect)
	{
		Displayf("Failed to connect\n");
		Assert(0);
		InternetCloseHandle(hInternet);
		return false;
	}

	DWORD dwOpenRequestFlags = INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP |
		INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS | 
		INTERNET_FLAG_KEEP_CONNECTION |
		INTERNET_FLAG_NO_AUTH |
		INTERNET_FLAG_NO_AUTO_REDIRECT |
		INTERNET_FLAG_NO_COOKIES |
		INTERNET_FLAG_NO_UI |
#if USE_SSL
		INTERNET_FLAG_SECURE|	// uses SSL/TLS transaction semantics
#endif	// USE_SSL
		INTERNET_FLAG_RELOAD;

	HINTERNET hRequest = ::HttpOpenRequest(hConnect, "GET", myUrl.lpszUrlPath, NULL, NULL, (const char**)"*/*\0", dwOpenRequestFlags, 0);
	if(!hRequest)
	{
#if !__FINAL
		DWORD errCode = GetLastError();
		Displayf("HttpOpenRequest failed. Error = %d\n", errCode);
#endif
		Assert(0);
		InternetCloseHandle(hConnect);
		InternetCloseHandle(hInternet);
		return false;
	}
	
	if(::HttpSendRequest(hRequest, NULL, 0, NULL, 0))
	{
		// get response from server
		atString response;
		if(!ReadResponseBody(hRequest, &response))
		{
			bReturn = false;
		}

		if(response.GetLength() > 0 && !strstr(response, "404"))
		{
			CFileMgr::SetAppDataDir("Settings");
			const char szFavoriteName[] = "XString.dat";
			FileHandle hFile = CFileMgr::OpenFileForWriting(szFavoriteName);
			if( !CFileMgr::IsValidFileHandle(hFile) )
			{
				Assert(0);
			}
			s32 iEncryptDataLength = 0;
			char* pEncryptData = EncryptData((char*)response.c_str(), response.GetLength()+1, iEncryptDataLength);
			if(pEncryptData)
			{
				CFileMgr::Write(hFile, pEncryptData, iEncryptDataLength);
				LocalFree(pEncryptData);
			}
			CFileMgr::CloseFile(hFile);
		}
	}
	else
	{
		DWORD errCode = GetLastError();
		Displayf("HttpSendRequestEx failed. Error = %d\n", errCode);
		Assert(0);
		return HRESULT_FROM_WIN32(errCode);
	}

	::InternetCloseHandle(hRequest);
	::InternetCloseHandle(hConnect);
	::InternetCloseHandle(hInternet);

	return true;
}

// Send CD key and authToken to gamespy for register. 
bool HttpUpload::CdKeyRegister()
{
	// 1. decrypt cd key
	// 2. Post data
	// 3. delete cd key

	CFileMgr::SetAppDataDir("Settings");
	FileHandle fiHandle = CFileMgr::OpenFile("serial.dat");
	if(CFileMgr::IsValidFileHandle(fiHandle))
	{
		s32 iFileSize = CFileMgr::GetTotalSize(fiHandle);
		char* pFileBuffer = rage_new char[iFileSize];
		if(!pFileBuffer)
		{
			Assert(0);
			CFileMgr::CloseFile(fiHandle);
			return false;
		}
		CFileMgr::Read(fiHandle, pFileBuffer, iFileSize);
		CFileMgr::CloseFile(fiHandle);

		s32 iDecryptedLength = 0;
		char* pDecryptedData = DecryptData(pFileBuffer, iFileSize, iDecryptedLength);
		delete [] pFileBuffer;
		pFileBuffer = NULL;
		
		if(pDecryptedData)
		{
			char szUrlHostName[INTERNET_MAX_HOST_NAME_LENGTH];
			char szUrlPath[INTERNET_MAX_URL_LENGTH];
			URL_COMPONENTS myUrl;
			memset(&myUrl, 0, sizeof(URL_COMPONENTS));
			myUrl.dwStructSize = sizeof(URL_COMPONENTS);
			myUrl.lpszHostName = szUrlHostName;
			myUrl.dwHostNameLength = sizeof(szUrlHostName);
			myUrl.lpszUrlPath = szUrlPath;
			myUrl.dwUrlPathLength = sizeof(szUrlPath);
#if 0 //USE_SSL
			char sUrl [] = "http://stage-rockstarsociety.gamespy.com/rockstarwebservices/GTAIVPC/registercdkey.aspx"; 
#else
			//char sUrl [] = "http://stage-rockstarsociety.gamespy.com/rockstarWebServices/testframework/testregistercdkey.aspx"; 
			char sUrl [] = "http://socialclub.rockstargames.com/rockstarwebservices/GTAIVPC/registercdkey.aspx";
#endif	// USE_SSL
			::InternetCrackUrl(sUrl,sizeof(sUrl), ICU_DECODE, &myUrl);

			char szHeader[] = "Content-Type: application/x-www-form-urlencoded; charset=utf-8;";

			HINTERNET hInternet = ::InternetOpen("GTA4 CD key register", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
			if(!hInternet)
			{
				Displayf("Failed to open session");
				Assert(0);
				LocalFree(pDecryptedData);
				return false;
			}

			HINTERNET hConnect = ::InternetConnect(hInternet, myUrl.lpszHostName, myUrl.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
			if(!hConnect)
			{
				Displayf("Failed to connect\n");
				Assert(0);
				InternetCloseHandle(hInternet);
				LocalFree(pDecryptedData);
				return false;
			}

			atString cdKeys = "cdkey=";
			//cdKeys += pDecryptedData;
			cdKeys += pDecryptedData;
			cdKeys += "&isClient=true&authToken=";
			cdKeys += m_sAuthToken;
			Displayf(cdKeys);

			DWORD dwOpenRequestFlags = INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP |
				INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS | 
				INTERNET_FLAG_KEEP_CONNECTION |
				INTERNET_FLAG_NO_AUTH |
				INTERNET_FLAG_NO_AUTO_REDIRECT |
				INTERNET_FLAG_NO_COOKIES |
				INTERNET_FLAG_NO_UI |
#if 0 //USE_SSL
				INTERNET_FLAG_SECURE|	// uses SSL/TLS transaction semantics
#endif	// USE_SSL
				INTERNET_FLAG_RELOAD;

			HINTERNET hRequest = ::HttpOpenRequest(hConnect, "POST", myUrl.lpszUrlPath, NULL, NULL, NULL, dwOpenRequestFlags, 0);
			if(!hRequest)
			{
				// TODO: display fail message.
				Displayf("HttpOpenRequest failed. Error = %d", GetLastError());
				Assert(0);
				::InternetCloseHandle(hConnect);
				::InternetCloseHandle(hInternet);
				LocalFree(pDecryptedData);
				return false;
			}

			if(!::HttpAddRequestHeaders(hRequest, szHeader, strlen(szHeader), HTTP_ADDREQ_FLAG_ADD))
			{
				Displayf("HttpAddRequestHeaders failed. Error = %d",  GetLastError());
				Assert(0);
				::InternetCloseHandle(hRequest);
				::InternetCloseHandle(hConnect);
				::InternetCloseHandle(hInternet);
				LocalFree(pDecryptedData);
				return false;
			}

			char* data = (char*)cdKeys.c_str();
			s32 iDataSize = cdKeys.GetLength();
			INTERNET_BUFFERS BufferIn;
			DWORD dwBytesWritten;
			BOOL bRet;
			BufferIn.dwStructSize = sizeof( INTERNET_BUFFERS ); // Must be set or error will occur
			BufferIn.Next = NULL; 
			BufferIn.lpcszHeader = NULL;
			BufferIn.dwHeadersLength = 0;
			BufferIn.dwHeadersTotal = 0;
			BufferIn.lpvBuffer = NULL;                
			BufferIn.dwBufferLength = 0;
			BufferIn.dwBufferTotal = iDataSize; // This is the only member used other than dwStructSize
			BufferIn.dwOffsetLow = 0;
			BufferIn.dwOffsetHigh = 0;

			if(::HttpSendRequestEx(hRequest, &BufferIn, NULL, 0, 0))
			{
Again:
				if(data && iDataSize > 0)
				{
					bRet = ::InternetWriteFile(hRequest, data, iDataSize, &dwBytesWritten);
					if(!bRet)
					{
						Displayf("Error on InternetWriteFile %d\n", GetLastError());
						Assert(0);
					}
				}
				if(!::HttpEndRequest(hRequest, NULL, 0, 0))
				{
					if ( ERROR_INTERNET_FORCE_RETRY == GetLastError() )
					{
						goto Again;
					}
					else
					{
						Displayf("Error on HttpEndRequest %d\n", GetLastError());
						Assert(0);
					}
				}
#if __DEV
				char aQueryBuf[2048];
				DWORD dwQueryBufLen = sizeof(aQueryBuf);
				BOOL bQuery = ::HttpQueryInfo(hRequest,HTTP_QUERY_RAW_HEADERS_CRLF, aQueryBuf, &dwQueryBufLen, NULL);
				if (bQuery)
				{
					// The query succeeded, specify memory needed for file
					Displayf(aQueryBuf);
				}
				else
				{
					DWORD errCode = GetLastError();
					Displayf("HttpQueryInfo failed. Error = %d\n", errCode);
				}
#endif	// __DEV
			}
			::InternetCloseHandle(hRequest);
			::InternetCloseHandle(hConnect);
			::InternetCloseHandle(hInternet);

			LocalFree(pDecryptedData);
		}

		//delete cd key file to make sure we only do this once after game installation
		//atString szFullName = CFileMgr::GetCurrentDirectory();
		//szFullName += "serial.dat";
		//DeleteFile(szFullName.c_str());

		return true;
	}
	else
	{
		return false;
	}
}

#endif	// GTA_REPLAY && ENABLE_BROWSER
