//
// system/BacktraceDumpUpload.winrt.cpp
//
// Copyright (C) 2022 Rockstar Games.  All Rights Reserved.
//

#include "BacktraceDumpUpload.winrt.h"

#if RSG_DURANGO

#pragma warning(push)
#pragma warning(disable: 4668)
#pragma warning(disable: 4265)
#include <xdk.h>
#include <collection.h>
#include <ixmlhttprequest2.h>
#include <wrl.h>
#pragma warning(pop)

using namespace Windows::Foundation;
using namespace Microsoft::WRL;
using namespace Windows::Foundation::Collections;

extern __THREAD int RAGE_LOG_DISABLE;

#include "net/http.h"

PARAM(uploadDumpsWithRageNet, "Use RageNet to upload dumps on Durango");

// Borrowed from exception.cpp
class ScopedRageLogDisabler
{
public:
	ScopedRageLogDisabler() { ++RAGE_LOG_DISABLE; }
	~ScopedRageLogDisabler() { --RAGE_LOG_DISABLE; }
};

typedef atMap<atString, atString> AnnotationMap;
typedef AnnotationMap::ConstIterator AnnotationIterator;

// Helper class that provides the header, file data and footer for a file being uploaded as a multipart/form-data request
class MultipartFileHelper
{
public:
	MultipartFileHelper() : m_boundary(nullptr), m_file(INVALID_HANDLE_VALUE), m_fileSize(0), m_inited(false), m_hasWrittenHeader(false) {}
	~MultipartFileHelper()
	{
		if (m_file != INVALID_HANDLE_VALUE)
		{
			CloseHandle(m_file);
			m_file = INVALID_HANDLE_VALUE;
		}
	}

	void Init(const char* boundary, const wchar_t* fileFullPath, const char* fileName)
	{
		m_boundary = boundary;
		m_filename = fileName;

		m_file = CreateFileW(fileFullPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
		if (m_file == INVALID_HANDLE_VALUE)
		{
			Errorf("Unable to open file");
			return;
		}

		LARGE_INTEGER fileSize;
		if (!GetFileSizeEx(m_file, &fileSize))
		{
			Errorf("Unable to get size of dump file");
			return;
		}

		m_fileSize = fileSize.QuadPart;
		m_inited = true;
	}

	bool IsInited() { return m_inited; }

	// Header looks like this:
	// --<boundary><CRLF>
	// Content-Disposition: form-data; name="<filename>"; filename="<filename>"<CRLF>
	// Content-Type: application/octet-stream<CRLF>
	// <CRLF>
	const char* GetHeaderFormat()
	{
		return "--%s\r\n"
			"Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n"
			"Content-Type: application/octet-stream\r\n"
			"\r\n";
	}

	u32 GetHeaderLength()
	{
		// Get format length without the format specifiers (3 x 2 characters, '%' and 's')
		u32 fmtLen = strlen(GetHeaderFormat()) - 3 * 2;

		// Add in the lengths of the substituted strings
		return fmtLen + strlen(m_boundary) + strlen(m_filename) * 2;
	}

	u64 GetTotalLengthIncludingHeader()
	{
		return GetHeaderLength() + m_fileSize + 2; // +2 for the following CRLF
	}

	HRESULT Read(void* pv, ULONG cb, ULONG* pcbRead)
	{
		if (!m_inited)
		{
			*pcbRead = 0;
			return S_FALSE;
		}

		if (!m_hasWrittenHeader)
		{
			u32 len = GetHeaderLength();
			if (cb < len)
				return S_FALSE;

			sprintf_s((char*)pv, cb, GetHeaderFormat(), m_boundary, m_filename, m_filename);
			*pcbRead = len;
			m_hasWrittenHeader = true;
			return S_OK;
		}
		else
		{
			DWORD bytesReadFromFile = 0;
			ReadFile(m_file, pv, cb, &bytesReadFromFile, NULL);
			*pcbRead = bytesReadFromFile;

			if (*pcbRead == cb)
				return S_OK;
			else
			{
				// If there is space for the terminating newline, add it
				if (*pcbRead <= cb - 2)
				{
					char* dst = (char*)pv;
					dst[(*pcbRead)++] = '\r';
					dst[(*pcbRead)++] = '\n';
					return S_FALSE;
				}
				else
				{
					// Otherwise add it next time (after a zero-byte read from the file)
					return S_OK;
				}
			}
		}
	}

private:
	const char* m_filename;
	HANDLE m_file;
	u64 m_fileSize;
	const char* m_boundary;
	bool m_inited, m_hasWrittenHeader;
};

// Helper class that provides the header, value and footer for each annotation being set with a multipart/form-data request
class MultipartAnnotationsHelper
{

public:
	MultipartAnnotationsHelper() : m_boundary(nullptr), m_boundaryLength(0), m_headerUnformattedLength(0), m_annotations(nullptr), m_iterator(sm_dummyMap.CreateIterator()) {}

	void Init(const char* boundary, const AnnotationMap* annotations)
	{
		m_boundary = boundary;
		m_boundaryLength = strlen(boundary);
		m_headerUnformattedLength = strlen(GetHeaderFormat()) - 3 * 2; // Subtract 3x format specifiers, "%s"

		if (annotations)
		{
			m_annotations = annotations;
		}
		else
		{
			m_annotations = &sm_dummyMap;
		}
		m_iterator = annotations->CreateIterator();
	}

	const char* GetHeaderFormat()
	{
		return "--%s\r\n"
			"Content-Disposition: form-data; name=\"%s\"\r\n"
			"\r\n"
			"%s\r\n";
	}

	u64 GetHeaderAndDataLength(AnnotationIterator it)
	{
		return m_headerUnformattedLength + m_boundaryLength + it.GetKey().GetLength() + it.GetData().GetLength();
	}

	u64 GetTotalLengthIncludingHeaders()
	{
		if (!m_annotations)
			return 0;

		u64 total = 0;
		for (AnnotationIterator it = m_annotations->CreateIterator(); !it.AtEnd(); it.Next())
		{
			total += GetHeaderAndDataLength(it);
		}

		return total;
	}

	HRESULT Read(void* pv, ULONG cb, ULONG* pcbRead)
	{
		if (m_iterator.AtEnd())
			return S_FALSE;

		u32 len = GetHeaderAndDataLength(m_iterator);
		if (cb < len)
			return S_FALSE;

		sprintf_s((char*)pv, cb, GetHeaderFormat(), m_boundary, m_iterator.GetKey().c_str(), m_iterator.GetData().c_str());
		*pcbRead = len;
		m_iterator.Next();

		return m_iterator.AtEnd() ? S_FALSE : S_OK;
	}

private:
	const char* m_boundary;
	u32 m_boundaryLength, m_headerUnformattedLength;
	const AnnotationMap* m_annotations;
	AnnotationIterator m_iterator;

	static const atMap<atString, atString> sm_dummyMap;
};

// Dummy (empty) map used when no annotations map is provided, because atMap::ConstIterator can't be used without a map
const atMap<atString, atString> MultipartAnnotationsHelper::sm_dummyMap;

// Class that represents the body of the multipart upload request for the dump, the log and the annotations
// The underlying HTTP library will call Read() multiple times (until it returns S_FALSE).
// Each iteration, we delegate the read to the current helper (dump, log or annotations), and when each is finished, we change to the next one until all are done.
// Finally, we read a terminating boundary ("--<boundary>--") which ends the content.
// Note that GetTotalLength() needs to return the exact length of the content (and is called *before* we start reading it) so any changes need to be reflected there.
// The methods for IDispatch and IUnknown are just required boilerplate, they do nothing of interest.
// (Adapted from netXblHttpRequestStream in xblhttp.cpp)
class DumpRequestStream : public RuntimeClass<RuntimeClassFlags<ClassicCom>, ISequentialStream, IDispatch>
{
public:
	DumpRequestStream() : m_currentPart(PART_BEGIN)
	{
	}

	~DumpRequestStream()
	{
	}

	const char* GetBoundary() { return "multipart-boundary-multipart-boundary-multipart-boundary"; }

	void Init(const wchar_t* dumpPath, const wchar_t* logPath, const AnnotationMap* annotations)
	{
		m_dumpFile.Init(GetBoundary(), dumpPath, "upload_file_minidump");
		m_logFile.Init(GetBoundary(), logPath, "crashcontext.log");
		m_annotations.Init(GetBoundary(), annotations);
	}

	HRESULT ReadEnding(void* pv, ULONG cb, ULONG* pcbRead)
	{
		ULONG len = strlen(GetBoundary()) + 4;
		if (cb < len)
			return S_FALSE;

		sprintf_s((char*)pv, cb, "--%s--", GetBoundary());
		*pcbRead = len;
		return S_FALSE;
	}

	// ISequentialStream
	STDMETHODIMP Read(void* pv, ULONG cb, ULONG* pcbRead)
	{
		HRESULT hr = S_FALSE;
		*pcbRead = 0;

		switch (m_currentPart)
		{
		case PART_DUMP: hr = m_dumpFile.Read(pv, cb, pcbRead); break;
		case PART_LOG: hr = m_logFile.Read(pv, cb, pcbRead); break;
		case PART_ANNOTATIONS: hr = m_annotations.Read(pv, cb, pcbRead); break;
		case PART_LAST_BOUNDARY: hr = ReadEnding(pv, cb, pcbRead); break;
		default: break;
		}

		// When one part has finished, start the next part
		if (hr == S_FALSE && m_currentPart < PART_END)
		{
			hr = S_OK;
			m_currentPart = (Part)((int)m_currentPart + 1);
		}

		return hr;
	}

	u64 GetTotalLength()
	{
		return
			m_dumpFile.GetTotalLengthIncludingHeader() +
			m_logFile.GetTotalLengthIncludingHeader() +
			m_annotations.GetTotalLengthIncludingHeaders() +
			strlen(GetBoundary()) + 4; // Final boundary with "--" before and after it
	}

	STDMETHODIMP Write(const void* UNUSED_PARAM(pv), ULONG UNUSED_PARAM(cb), ULONG* UNUSED_PARAM(pcbWritten)) { return S_FALSE; }

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject)
	{
		if (!ppvObject)
		{ 
			return E_INVALIDARG;
		}

		if (riid == IID_IUnknown)
		{
			*ppvObject = static_cast<IUnknown*>((IDispatch*)this);
		}
		else if (riid == IID_IDispatch)
		{
			*ppvObject = static_cast<IDispatch*>(this);
		}
		else if (riid == IID_ISequentialStream)
		{
			*ppvObject = static_cast<ISequentialStream*>(this);
		}
		else 
		{
			*ppvObject = nullptr;
			return E_NOINTERFACE;
		}

		AddRef();

		return S_OK;
	}

	// IDispatch
	STDMETHODIMP GetTypeInfoCount(unsigned int FAR* pctinfo)
	{
		if (pctinfo)
		{
			*pctinfo = 0;
		}
		return E_NOTIMPL;
	}

	STDMETHODIMP GetTypeInfo(unsigned int UNUSED_PARAM(iTInfo), LCID UNUSED_PARAM(lcid), ITypeInfo FAR* FAR* ppTInfo)
	{
		if (ppTInfo)
		{
			*ppTInfo = nullptr;
		}
		return E_NOTIMPL;
	}

	STDMETHODIMP GetIDsOfNames(REFIID UNUSED_PARAM(riid), OLECHAR** UNUSED_PARAM(rgszNames), unsigned int UNUSED_PARAM(cNames), LCID UNUSED_PARAM(lcid), DISPID* UNUSED_PARAM(rgDispId))
	{
		return DISP_E_UNKNOWNNAME;
	}

	STDMETHODIMP Invoke(DISPID UNUSED_PARAM(dispIdMember), REFIID UNUSED_PARAM(riid), LCID UNUSED_PARAM(lcid), WORD UNUSED_PARAM(wFlags), DISPPARAMS* UNUSED_PARAM(pDispParams), VARIANT* UNUSED_PARAM(pVarResult), EXCEPINFO* UNUSED_PARAM(pExcepInfo), unsigned int* UNUSED_PARAM(puArgErr))
	{
		return S_OK;
	}

private:
	// Helpers that handle the dump, the log and the annotations
	MultipartFileHelper m_dumpFile, m_logFile;
	MultipartAnnotationsHelper m_annotations;

	// Which part we are sending next
	enum Part
	{
		PART_BEGIN,

		PART_ANNOTATIONS = PART_BEGIN, // The annotations (note one Read iteration is needed per annotation)
		PART_DUMP,                     // The minidump file itself
		PART_LOG,                      // The crashcontext.log file
		PART_LAST_BOUNDARY,            // The terminating boundary ("--<boundary>--")

		PART_END
	};
	Part m_currentPart;
};

// Provides callback functions that are called by IXHR2 in response to various events.
// Since we've crashed and are not making any decisions based on whether the upload succeeds or not, we basically ignore all of these except OnError,
// and even that one is only used for debugging purposes.
class DumpUploadCallbacks : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IXMLHTTPRequest2Callback>
{
public:
	DumpUploadCallbacks() : m_finished(false), m_error(false) {}

	STDMETHODIMP OnRedirect(IXMLHTTPRequest2 *UNUSED_PARAM(pXHR), const WCHAR *UNUSED_PARAM(pwszRedirectUrl)) { return S_OK; }
	STDMETHODIMP OnHeadersAvailable(IXMLHTTPRequest2 *UNUSED_PARAM(pXHR), DWORD UNUSED_PARAM(dwStatus), const WCHAR *UNUSED_PARAM(pwszStatus)) { return S_OK; }
	STDMETHODIMP OnDataAvailable(IXMLHTTPRequest2 *UNUSED_PARAM(pXHR), ISequentialStream *UNUSED_PARAM(pResponseStream)) { return S_OK; }
	STDMETHODIMP OnResponseReceived(IXMLHTTPRequest2 *UNUSED_PARAM(pXHR), ISequentialStream *UNUSED_PARAM(pResponseStream)) { m_finished = true; return S_OK; }
	STDMETHODIMP OnError(IXMLHTTPRequest2 *UNUSED_PARAM(pXHR), HRESULT OUTPUT_ONLY(hrError))
	{
		Errorf("OnError(XHR2) = 0x%08X", hrError);
		m_finished = true;
		m_error = true;
		return S_OK;
	}

	bool HasFinished() { return m_finished; }
	bool HadError() { return m_error; }

private:
	bool m_finished, m_error;
};

#if !RSG_FINAL
bool DoRageNetDumpUpload(const wchar_t* dumpFullPath, const char* url)
{
	HANDLE file = CreateFileW(dumpFullPath, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (file == INVALID_HANDLE_VALUE)
	{
		Errorf("Unable to open dump file");
		return false;
	}

	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(file, &fileSize))
	{
		Errorf("Unable to get size of dump file");
		CloseHandle(file);
		return false;
	}

	rage::netHttpRequest request;
	netStatus status;
	request.Init(nullptr, netTcp::GetInsecureSslCtx());
	request.BeginPost(url, nullptr, 60, "Backtrace Dump Upload", nullptr, &status);

	u8* fileBuffer = rage_new u8[fileSize.QuadPart];


	const DWORD MAX_READ_BYTES = 4 * 1024 * 1024;

	for (u64 fileOffset = 0; fileOffset < fileSize.QuadPart;)
	{
		u64 remaining = fileSize.QuadPart - fileOffset;
		DWORD toRead = (remaining < MAX_READ_BYTES) ? remaining : MAX_READ_BYTES;
		ReadFile(file, &fileBuffer[fileOffset], toRead, NULL, NULL);
		fileOffset += toRead;
	}

	CloseHandle(file);

	request.AppendContentZeroCopy(fileBuffer, fileSize.QuadPart);

	request.Commit();

	while (status.Pending())
	{
		request.Update();
		Sleep(0);
	}

	return true;
}
#endif // !RSG_FINAL

bool BacktraceDumpUpload::UploadDump(const wchar_t* dumpFullPath, const wchar_t* logFullPath, const atMap<atString, atString>* annotations, const char* url)
{
#if !RSG_FINAL
	// Test dump upload with RageNet.  Note that the log and annotations are not included.
	if (PARAM_uploadDumpsWithRageNet.Get())
	{
		return DoRageNetDumpUpload(dumpFullPath, url);
	}
#endif

	// COM objects that we'll use to upload the dump with:
	ComPtr<IXMLHTTPRequest2> pXHR;             // The IXHR2 object, wraps a HTTP request
	ComPtr<DumpUploadCallbacks> pXHRCallbacks; // Callbacks object, defined above, which handles responses from IXHR2
	ComPtr<DumpRequestStream> pRequestStream;  // Request stream object, which provides the body of the request via calls made from IXHR2 to its Read() method

	// WinRT calls new, so temporarily disable the error
	ScopedRageLogDisabler disablerObject;

	// Make and init the request stream
	HRESULT hr = MakeAndInitialize<DumpRequestStream>(&pRequestStream);
	if (FAILED(hr))
	{
		Errorf("MakeAndInitialize<DumpRequestStream> = 0x%08X", hr);
		return false;
	}
	else
	{
		// There doesn't seem to be a way of initing a RuntimeClass subclass through its constructor, so do it here
		pRequestStream->Init(dumpFullPath, logFullPath, annotations);
	}

	// Make the callbacks provider
	hr = Microsoft::WRL::Details::MakeAndInitialize<DumpUploadCallbacks>(&pXHRCallbacks);
	if (FAILED(hr))
	{
		Errorf("MakeAndInitialize<DumpUploadCallbacks> = 0x%08X", hr);
		return false;
	}

	// Make the IXHR2 object
	hr = CoCreateInstance(__uuidof(FreeThreadedXMLHTTP60), NULL, CLSCTX_SERVER, IID_PPV_ARGS(&pXHR));
	if (FAILED(hr))
	{
		Errorf("CoCreateInstance(IXHR2) = 0x%08X", hr);
		return false;
	}

	// Convert the URL to wide chars.  It's used as UTF8 in other places (Crashpad, Breakpad).
	USES_CONVERSION;
	const char16* urlW = UTF8_TO_WIDE(url);

	// Open the request
	hr = pXHR->Open(L"POST", (const WCHAR*)urlW, pXHRCallbacks.Get(), nullptr, nullptr, nullptr, nullptr);
	if (FAILED(hr))
	{
		Errorf("Error opening IXHR2");
		return false;
	}

	// Set the content type
	// Content-Type: multipart/form-data; boundary=<boundary>
	char contentTypeBuffer[256];
	sprintf_s(contentTypeBuffer, "multipart/form-data; boundary=%s", pRequestStream->GetBoundary());
	const char16* contentTypeW = UTF8_TO_WIDE(contentTypeBuffer);
	pXHR->SetRequestHeader(L"Content-Type", (const WCHAR*)contentTypeW);

	// Set the connection timeout (this only affects the initial connection to the server, not the full upload)
	hr = pXHR->SetProperty(XHR_PROP_TIMEOUT, 30 * 1000);

	// Send the request
	hr = pXHR->Send(pRequestStream.Get(), (ULONGLONG)pRequestStream->GetTotalLength());
	if (FAILED(hr))
	{
		Errorf("Error sending XHR");
		return false;
	}

	// Wait for the request to finish (successfully or otherwise)
	while (!pXHRCallbacks->HasFinished())
	{
		Sleep(0);
	}

	return !pXHRCallbacks->HadError();
}

#endif // RSG_DURANGO