//--------------------------------------------------------------------------------------
// companion_http.cpp
//--------------------------------------------------------------------------------------

#if RSG_PC

#include "Companion.h"

#if COMPANION_APP
#include "companion_http.h"
#include "file/asset.h"

// include window libraries
#include <ctime>

#define SHOULD_SHUTDOWN_CIVETWEBSERVER				0			// currently calling `mg_stop` crashes the game, so once up it will always be up
#define ALLOW_CLIENT_TO_CACHE_FILES					0			// if we want to make sure that caching doesn't mess with the code, change this to 0
#define RESTRICT_SERVICE_TO_CONNECTED_CLIENT_ONLY	1			// 1: only the IP from the connected device through pairing will be serviced, 0: allows all

#ifndef UNUSED_PARAMETER
#define UNUSED_PARAMETER(x) (void)(x)
#endif

const int		kInternetRFC1123BufSize		= 30;	// Value is based on `INTERNET_RFC1123_BUFSIZE` defined in <WinInet.h>
const char*		kServerNumThreads			= "4";
const int		kTempBufferSize				= 2048;

const char*		kRFC1123TimeFormat			= "%a, %d %b %Y %H:%M:%S GMT";	// i.e Tue, 15 Nov 1994 12:45:26 GMT -
const char*		kHTMLFilesRootPath			= "sce_companion_httpd/html";
const char*		kHTMLResponseHeader			= "HTTP/1.1 %d %s\r\n";
const char*		kHTMLResponseParam			= "%s: %s\r\n";
const char*		kHTMLEndLine				= "\r\n";
const char*		kHTMLError404Body			= "<html><body>404</body></html>";
const char*		kHTMLError404Headers		= "HTTP/1.1 %d %s\r\nContent-Length: %d\r\nContent-Type: text/html\r\n\r\n";
const char*		kJSONGTAVResponse			= "{ \"GTA5\" : \"%s\" }";

const atString	kRequestParamKey			= atString("request");
const atString	kConnectionParamKey			= atString("Connection");
const atString	kKeepAliveParamValue		= atString("keep-alive");
const atString	kCacheControlParamKey		= atString("Cache-Control");
const atString	kNoCacheParamValue			= atString("no-cache, no-store, must-revalidate");
const atString	kPragmaParamKey				= atString("Pragma");
const atString	kNoCachePragrmaParamValue	= atString("no-cache");
const atString	kPostContentParamKey		= atString("Post-Content");
const atString	kContentTypeParamKey		= atString("Content-Type");
const atString	kContentTypeJSON			= atString("application/json");
const atString	kContentLengthParamKey		= atString("Content-Length");
const atString	kLastModifiedParamKey		= atString("Last-Modified");

//////////////////////////////////////////////////////////////////////////
// Request Handler
//////////////////////////////////////////////////////////////////////////

class RequestHandler : public CivetHandler
{
public:

	bool handleGet(CivetServer *server, struct mg_connection *conn)
	{
		UNUSED_PARAMETER(server);
		char charBuffer[kTempBufferSize];
		int contentSize = 0;

		struct mg_request_info * req_info = mg_get_request_info(conn);
		if(req_info->query_string == NULL)
		{
			// Print out request
			CompanionDisplayf("Request %s %s %s", req_info->request_method, req_info->uri, req_info->query_string);

			sprintf(charBuffer, "sce_companion_httpd/html%s", req_info->uri);
			fiStream* stream = fiStream::Open(charBuffer);
			if(stream)
			{
				contentSize = stream->Size();
				atString fullPath = atString(charBuffer);
				u64 timeLastModified = stream->GetDevice()->GetFileTime(fullPath.c_str());
				atString mimeType = atString(getFileTypeFromExtension(fullPath));

				// Send headers
				mg_printf(conn, "HTTP/1.1 200 OK\r\n");
				mg_printf(conn, "Content-Type: %s\r\n", mimeType.c_str());
				mg_printf(conn, "Content-Length: %d\r\n", contentSize);
				mg_printf(conn, "Last-Modified: %s\r\n", getLastModified(timeLastModified).c_str());
#if ALLOW_CLIENT_TO_CACHE_FILES
				if (mimeType != atString("application/x-javascript"))
				{
					// We don't want javascript files to be cached
					mg_printf(conn, "Cache-Control: max-age=86400\r\n");
				}
				else
#endif
				{
					mg_printf(conn, "Cache-Control: max-age=0\r\n");
				}
				
				mg_printf(conn, "\r\n");

				// Send file data
				int bytesRemaining = contentSize;
				while(bytesRemaining > 0)
				{
					int numBytesToRead = bytesRemaining > kTempBufferSize ? kTempBufferSize : bytesRemaining;
					int bytesRead = rage::fread(charBuffer, 1, numBytesToRead, stream);

					int bytesPushed = mg_write(conn, charBuffer, bytesRead);
					if (bytesPushed > 0 && bytesPushed != numBytesToRead)
					{
						// Failed to send as many bytes as were read, get the difference and rewind
						int bytesToRewind = numBytesToRead - bytesPushed;
						rage::fseek(stream, -bytesToRewind, SEEK_CUR);
					}
					else if(bytesPushed <= 0)
					{
						CompanionWarningf("Data transmission (file: %s) stopped because client disconnected (%d)", fullPath.c_str(), bytesPushed);
						break;
					}

					bytesRemaining -= bytesPushed;
				}

				stream->Close();
			}
			else
			{
				CompanionWarningf("File not found - %s", charBuffer);
				contentSize = (int)strlen(kHTMLError404Body);

				// Send error headers
				mg_printf(conn, kHTMLError404Headers, 404, "Not Found", contentSize);
				
				// Send error body
				mg_printf(conn, "%s", kHTMLError404Body);
			}
		}
		else
		if (strcmp(req_info->query_string, "get_env") == 0)
		{
			const char* envName = g_rlTitleId->m_RosTitleId.GetEnvironmentName();
			contentSize = sprintf_s(charBuffer, kTempBufferSize, "{ \"GTA5Env\" : \"%s\" }", envName);

			mg_printf(conn, "HTTP/1.1 200 OK\r\n");
			mg_printf(conn, "Content-Type: %s\r\n", kContentTypeJSON.c_str());
			mg_printf(conn, "Cache-Control: %s\r\n", kNoCacheParamValue.c_str());
			mg_printf(conn, "Pragma: %s\r\n", kNoCachePragrmaParamValue.c_str());
			mg_printf(conn, "Content-Length: %d\r\n", contentSize);
			mg_printf(conn, "\r\n");

			mg_write(conn, charBuffer, contentSize);
		}
		else
		if (strcmp(req_info->query_string, "blips") == 0)
		{
			CCompanionData* companionData = CCompanionData::GetInstance();
			CCompanionBlipMessage* blipMessage = companionData->GetBlipMessage();

			if (blipMessage)
			{
				char* encodedBlipMessage = blipMessage->GetEncodedMessage();
				contentSize = sprintf_s(charBuffer, kTempBufferSize, kJSONGTAVResponse, encodedBlipMessage);
				
				delete blipMessage;

				mg_printf(conn, "HTTP/1.1 200 OK\r\n");
				mg_printf(conn, "Content-Length: %d\r\n", contentSize);
			}
			else
			{
				companionData->RemoveOldBlipMessages();

				mg_printf(conn, "HTTP/1.1 204 No Content\r\n");
			}

			mg_printf(conn, "Content-Type: %s\r\n", kContentTypeJSON.c_str());
			mg_printf(conn, "Cache-Control: %s\r\n", kNoCacheParamValue.c_str());
			mg_printf(conn, "Pragma: %s\r\n", kNoCachePragrmaParamValue.c_str());
			mg_printf(conn, "\r\n");

			if (contentSize > 0)
				mg_write(conn, charBuffer, contentSize);
		}
		else
		if (strcmp(req_info->query_string, "route") == 0)
		{
			CCompanionData* companionData = CCompanionData::GetInstance();
			CCompanionRouteMessage* routeMessage = companionData->GetRouteMessage();

			if (routeMessage)
			{
				char* encodedRouteMessage = routeMessage->GetEncodedMessage();
				contentSize = sprintf_s(charBuffer, kTempBufferSize, kJSONGTAVResponse, encodedRouteMessage);

				delete routeMessage;

				mg_printf(conn, "HTTP/1.1 200 OK\r\n");
				mg_printf(conn, "Content-Length: %d\r\n", contentSize);
			}
			else
			{
				companionData->RemoveOldRouteMessages();

				mg_printf(conn, "HTTP/1.1 204 No Content\r\n");
			}

			mg_printf(conn, "Content-Type: %s\r\n", kContentTypeJSON.c_str());
			mg_printf(conn, "Cache-Control: %s\r\n", kNoCacheParamValue.c_str());
			mg_printf(conn, "Pragma: %s\r\n", kNoCachePragrmaParamValue.c_str());
			mg_printf(conn, "\r\n");


			if (contentSize > 0)
				mg_write(conn, charBuffer, contentSize);
		}

		return true;
	}

	bool handlePost(CivetServer *server, struct mg_connection *conn)
	{
		UNUSED_PARAMETER(server);

		struct mg_request_info * req_info = mg_get_request_info(conn);
		CompanionDisplayf("Request %s %s %s", req_info->request_method, req_info->uri, req_info->query_string);

		CCompanionData* companionData = CCompanionData::GetInstance();
		if (strcmp(req_info->uri, "/no_waypoint") == 0)
		{
			mg_printf(conn, "HTTP/1.1 204 No Content\r\n");
			mg_printf(conn, "Cache-Control: %s\r\n", kNoCacheParamValue.c_str());
			mg_printf(conn, "Pragma: %s\r\n", kNoCachePragrmaParamValue.c_str());
			mg_printf(conn, "\r\n");

			companionData->RemoveWaypoint();
		}
		else if (strcmp(req_info->uri, "/resend_all") == 0)
		{
			mg_printf(conn, "HTTP/1.1 204 No Content\r\n");
			mg_printf(conn, "Cache-Control: %s\r\n", kNoCacheParamValue.c_str());
			mg_printf(conn, "Pragma: %s\r\n", kNoCachePragrmaParamValue.c_str());
			mg_printf(conn, "\r\n");

			companionData->ResendAll();
		}
		else if (strcmp(req_info->uri, "/toggle_pause") == 0)
		{
			mg_printf(conn, "HTTP/1.1 204 No Content\r\n");
			mg_printf(conn, "Cache-Control: %s\r\n", kNoCacheParamValue.c_str());
			mg_printf(conn, "Pragma: %s\r\n", kNoCachePragrmaParamValue.c_str());
			mg_printf(conn, "\r\n");

			companionData->TogglePause();
		}
		else if (strcmp(req_info->uri, "/waypoint") == 0)
		{
			char postData[512];
			int postDataLen;

			postDataLen = mg_read(conn, postData, sizeof(postData));
			if (postDataLen > 0)
			{
				atString content = atString(postData);

				atArray<atString> vector;
				content.Split(vector, ',', true);
				if (vector.GetCount() == 2)
				{
					double	x =  strtod(vector[0].c_str(), NULL),
						y =  strtod(vector[1].c_str(), NULL);
					companionData->CreateWaypoint(x, y);

					mg_printf(conn, "HTTP/1.1 204 No Content\r\n");
				}
				else
				{
					mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n");
				}

				mg_printf(conn, "Cache-Control: %s\r\n", kNoCacheParamValue.c_str());
				mg_printf(conn, "Pragma: %s\r\n", kNoCachePragrmaParamValue.c_str());
				mg_printf(conn, "\r\n");
			}
			else
			{
				CompanionWarningf("Connection was terminated by peer while reading post data for /waypoint <%d>", postDataLen);
			}
		}

		return true;
	}

private:

	static const char* getFileTypeFromExtension(const atString& fileName)
	{
		const char* name = fileName.c_str();
		if (strstr(name,".html") || strstr(name,".htm"))
			return "text/html";
		else if (strstr(name,".js"))
			return "application/x-javascript";
		else if (strstr(name,".css"))
			return "text/css";
		else if (strstr(name,".png"))
			return "image/png";
		else if (strstr(name,".jpg") || strstr(name,".jpeg"))
			return "image/jpeg";
		else if (strstr(name,".wav"))
			return "audio/x-wav";

		return "application/octet-stream";
	}

	static atString getLastModified(u64 lastModified)
	{
		// We need to convert the FILETIME passed to CRT time_t value
		// msdn: http://blogs.msdn.com/b/joshpoley/archive/2007/12/19/date-time-formats-and-conversions.aspx
		LARGE_INTEGER utcFT;
		utcFT.HighPart = lastModified >> 32;
		utcFT.LowPart = 0x00000000ffffffff & lastModified;
		utcFT.QuadPart = lastModified;

		LARGE_INTEGER jan1970FT = {0};
		jan1970FT.QuadPart = 116444736000000000I64; // January 1st 1970
		time_t crtTime = (utcFT.QuadPart - jan1970FT.QuadPart)/10000000;

		// And finally to a tm structure to print the time in RFC 1123 Date Time Format
		// see http://tools.ietf.org/html/rfc2616#page-20
		char dateBuff[kInternetRFC1123BufSize];	
		struct tm * ptime = gmtime((const time_t*)&crtTime);		
		if (strftime(dateBuff, kInternetRFC1123BufSize, kRFC1123TimeFormat,ptime) != 0)
		{
			return atString(dateBuff);
		}

		return atString("");
	}
};

//////////////////////////////////////////////////////////////////////////
// CHttpServer
//////////////////////////////////////////////////////////////////////////

netIpAddress CHttpServer::allowedClientIp;

CHttpServer::CHttpServer(unsigned short _port) : listeningPort(_port)
{
	server = NULL;
}

CHttpServer::~CHttpServer()
{
	shutdown();
}

int CHttpServer::acceptClient(uint32_t remoteIp)
{
	netIpV4Address ipv4Addr = netIpV4Address(remoteIp);
	netIpAddress incomingClientAddr(ipv4Addr);

	if (allowedClientIp != incomingClientAddr)
	{
		return 0;
	}

	return 1;
}

bool CHttpServer::startListening(const netIpAddress& forClientIp)
{
	if (server != NULL)
	{
#if SHOULD_SHUTDOWN_CIVETWEBSERVER
		shutdown();
#else
		allowedClientIp = forClientIp;
		return true;
#endif
	}

	allowedClientIp = forClientIp;

	char listeningPortString[8];
	sprintf_s(listeningPortString, sizeof(listeningPortString), "%d", listeningPort);

	//char accessList[128];
	//sprintf_s(accessList, sizeof(accessList), "-0.0.0.0/0,+%s,+127.0.0.1", forClientIp.c_str());

	const char* serverOptions[] = 
	{
		"document_root",		kHTMLFilesRootPath,
		"listening_ports",		listeningPortString,
		"num_threads",			kServerNumThreads,
		//"access_control_list",	accessList,
		0
	};

	mg_callbacks callbacks = { 0 };
#if RESTRICT_SERVICE_TO_CONNECTED_CLIENT_ONLY
	callbacks.accept_client_connection = CHttpServer::acceptClient;
#endif

	RequestHandler* rh = rage_new RequestHandler();

	server = rage_new CivetServer(serverOptions, &callbacks);
	server->addHandler("**", rh);
	if (server->getContext() == NULL)
	{
		delete server;
		server = NULL;
		return false;
	}
	

	return true;
}

void CHttpServer::shutdown()
{
#if SHOULD_SHUTDOWN_CIVETWEBSERVER
	if (server != NULL)
	{
		server->close();
		server = NULL;
	}
#endif

	allowedClientIp.Clear();
}

//////////////////////////////////////////////////////////////////////////
// CivetServer.cpp
//////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>
#include <assert.h>
bool CivetHandler::handleGet(CivetServer *server, struct mg_connection *conn)
{
	UNUSED_PARAMETER(server);
	UNUSED_PARAMETER(conn);
	return false;
}

bool CivetHandler::handlePost(CivetServer *server, struct mg_connection *conn)
{
	UNUSED_PARAMETER(server);
	UNUSED_PARAMETER(conn);
	return false;
}

bool CivetHandler::handlePut(CivetServer *server, struct mg_connection *conn)
{
	UNUSED_PARAMETER(server);
	UNUSED_PARAMETER(conn);
	return false;
}

bool CivetHandler::handleDelete(CivetServer *server, struct mg_connection *conn)
{
	UNUSED_PARAMETER(server);
	UNUSED_PARAMETER(conn);
	return false;
}

int CivetServer::requestHandler(struct mg_connection *conn, void *cbdata)
{
	struct mg_request_info *request_info = mg_get_request_info(conn);
	CivetServer *me = (CivetServer*) (request_info->user_data);
	CivetHandler *handler = (CivetHandler *)cbdata;

	if (handler) {
		if (strcmp(request_info->request_method, "GET") == 0) {
			return handler->handleGet(me, conn) ? 1 : 0;
		} else if (strcmp(request_info->request_method, "POST") == 0) {
			return handler->handlePost(me, conn) ? 1 : 0;
		} else if (strcmp(request_info->request_method, "PUT") == 0) {
			return !handler->handlePut(me, conn) ? 1 : 0;
		} else if (strcmp(request_info->request_method, "DELETE") == 0) {
			return !handler->handleDelete(me, conn) ? 1 : 0;
		}
	}

	return 0; // No handler found

}

CivetServer::CivetServer(const char **options,
						 const struct mg_callbacks *_callbacks) :
context(0)
{


	if (_callbacks) {
		context = mg_start(_callbacks, this, options);
	} else {
		struct mg_callbacks callbacks;
		memset(&callbacks, 0, sizeof(callbacks));
		context = mg_start(&callbacks, this, options);
	}
}

CivetServer::~CivetServer()
{
	close();
}

void CivetServer::addHandler(const std::string &uri, CivetHandler *handler)
{
	mg_set_request_handler(context, uri.c_str(), requestHandler, handler);
}

void CivetServer::removeHandler(const std::string &uri)
{
	mg_set_request_handler(context, uri.c_str(), NULL, NULL);
}

void CivetServer::close()
{
	if (context) {
		mg_stop (context);
		context = 0;
	}
}

int CivetServer::getCookie(struct mg_connection *conn, const std::string &cookieName, std::string &cookieValue)
{
	//Maximum cookie length as per microsoft is 4096. http://msdn.microsoft.com/en-us/library/ms178194.aspx
	char _cookieValue[4096];
	const char *cookie = mg_get_header(conn, "Cookie");
	int lRead = mg_get_cookie(cookie, cookieName.c_str(), _cookieValue, sizeof(_cookieValue));
	cookieValue.clear();
	cookieValue.append(_cookieValue);
	return lRead;
}

const char* CivetServer::getHeader(struct mg_connection *conn, const std::string &headerName)
{
	return mg_get_header(conn, headerName.c_str());
}

void
	CivetServer::urlDecode(const char *src, std::string &dst, bool is_form_url_encoded)
{
	urlDecode(src, strlen(src), dst, is_form_url_encoded);
}

void
	CivetServer::urlDecode(const char *src, size_t src_len, std::string &dst, bool is_form_url_encoded)
{
	int i, j, a, b;
#define HEXTOI(x) (isdigit(x) ? x - '0' : x - 'W')

	dst.clear();
	for (i = j = 0; i < (int)src_len; i++, j++) {
		if (src[i] == '%' && i < (int)src_len - 2 &&
			isxdigit(* (const unsigned char *) (src + i + 1)) &&
			isxdigit(* (const unsigned char *) (src + i + 2))) {
				a = tolower(* (const unsigned char *) (src + i + 1));
				b = tolower(* (const unsigned char *) (src + i + 2));
				dst.push_back((char) ((HEXTOI(a) << 4) | HEXTOI(b)));
				i += 2;
		} else if (is_form_url_encoded && src[i] == '+') {
			dst.push_back(' ');
		} else {
			dst.push_back(src[i]);
		}
	}
}

bool
	CivetServer::getParam(struct mg_connection *conn, const char *name,
	std::string &dst, size_t occurrence)
{
	const char *query = mg_get_request_info(conn)->query_string;
	return getParam(query, strlen(query), name, dst, occurrence);
}

bool
	CivetServer::getParam(const char *data, size_t data_len, const char *name,
	std::string &dst, size_t occurrence)
{
	const char *p, *e, *s;
	size_t name_len;

	dst.clear();
	if (data == NULL || name == NULL || data_len == 0) {
		return false;
	}
	name_len = strlen(name);
	e = data + data_len;

	// data is "var1=val1&var2=val2...". Find variable first
	for (p = data; p + name_len < e; p++) {
		if ((p == data || p[-1] == '&') && p[name_len] == '=' &&
			!mg_strncasecmp(name, p, name_len) && 0 == occurrence--) {

				// Point p to variable value
				p += name_len + 1;

				// Point s to the end of the value
				s = (const char *) memchr(p, '&', (size_t)(e - p));
				if (s == NULL) {
					s = e;
				}
				assert(s >= p);

				// Decode variable into destination buffer
				urlDecode(p, (int)(s - p), dst, true);
				return true;
		}
	}
	return false;
}

void
	CivetServer::urlEncode(const char *src, std::string &dst, bool append)
{
	urlEncode(src, strlen(src), dst, append);
}

void
	CivetServer::urlEncode(const char *src, size_t src_len, std::string &dst, bool append)
{
	static const char *dont_escape = "._-$,;~()";
	static const char *hex = "0123456789abcdef";

	if (!append)
		dst.clear();

	for (; src_len > 0; src++, src_len--) {
		if (isalnum(*(const unsigned char *) src) ||
			strchr(dont_escape, * (const unsigned char *) src) != NULL) {
				dst.push_back(*src);
		} else {
			dst.push_back('%');
			dst.push_back(hex[(* (const unsigned char *) src) >> 4]);
			dst.push_back(hex[(* (const unsigned char *) src) & 0xf]);
		}
	}
}


#endif	// COMPANION_APP
#endif	// RSG_PC
