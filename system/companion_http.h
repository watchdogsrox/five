//--------------------------------------------------------------------------------------
// companion_http.h
//--------------------------------------------------------------------------------------
#if RSG_PC

#pragma once
#include "companion.h"

//#if COMPANION_APP

#include "atl/string.h"
#include "atl/map.h"
#include "file/device.h"
#include "diag/civetweb.h"
#include <map>
#include <string>


//	HTML file structure
typedef struct  
{
	atString	m_Path;
	const char* m_Type;
	u64			m_Size;
	u64			m_ModifiedTime;	// helps with caching info
}sHTMLFileStructure;

typedef atMap<atString, sHTMLFileStructure> tHtmlLibrary;

//--------------------------------------------------------------------------------------
// forward declaration
class CivetServer;

class CivetHandler
{
public:
    virtual ~CivetHandler() {}
    virtual bool handleGet(CivetServer *server, struct mg_connection *conn);
    virtual bool handlePost(CivetServer *server, struct mg_connection *conn);
    virtual bool handlePut(CivetServer *server, struct mg_connection *conn);
    virtual bool handleDelete(CivetServer *server, struct mg_connection *conn);
};

class CivetServer
{
public:
    CivetServer(const char **options, const struct mg_callbacks *callbacks = 0);
    virtual ~CivetServer();
    void close();
    const struct mg_context *getContext() const {
        return context;
    }
    void addHandler(const std::string &uri, CivetHandler *handler);
    void removeHandler(const std::string &uri);
    static int getCookie(struct mg_connection *conn, const std::string &cookieName, std::string &cookieValue);
    static const char* getHeader(struct mg_connection *conn, const std::string &headerName);
    static bool getParam(struct mg_connection *conn, const char *name, std::string &dst, size_t occurrence=0);
    static bool getParam(const std::string &data, const char *name, std::string &dst, size_t occurrence=0) {
        return getParam(data.c_str(), data.length(), name, dst, occurrence);
    }
    static bool getParam(const char *data, size_t data_len, const char *name, std::string &dst, size_t occurrence=0);
    static void urlDecode(const std::string &src, std::string &dst, bool is_form_url_encoded=true) {
        urlDecode(src.c_str(), src.length(), dst, is_form_url_encoded);
    }
    static void urlDecode(const char *src, size_t src_len, std::string &dst, bool is_form_url_encoded=true);
    static void urlDecode(const char *src, std::string &dst, bool is_form_url_encoded=true);
    static void urlEncode(const std::string &src, std::string &dst, bool append=false) {
        urlEncode(src.c_str(), src.length(), dst, append);
    }
    static void urlEncode(const char *src, std::string &dst, bool append=false);
    static void urlEncode(const char *src, size_t src_len, std::string &dst, bool append=false);
protected:
    struct mg_context *context;
private:
    static int requestHandler(struct mg_connection *conn, void *cbdata);
};

//--------------------------------------------------------------------------------------

class CHttpServer
{
public:

	CHttpServer(unsigned short _port);
	~CHttpServer();

	bool				startListening(const netIpAddress& forClientIp);
	void				shutdown();

private:

	static int			acceptClient(uint32_t remoteIp);

	CivetServer*		server;
	unsigned short		listeningPort;

	static netIpAddress	allowedClientIp;
};

//#endif // COMPANION_APP
#endif // RSG_PC