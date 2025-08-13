//
// filename:	netLocalizedCloudStrings.h
// description:	
//

#ifndef INC_NETLOCALIZEDCLOUDSTRINGS_H_
#define INC_NETLOCALIZEDCLOUDSTRINGS_H_

#include "atl/map.h"
#include "data/growbuffer.h"
#include "net/status.h"

#include "Network/Cloud/CloudManager.h"

//FWD
class netLocalizedStringInCloudRequest;

class netLocalizedStringInCloudMgr
{
public:
	static const int CLOUD_STRING_KEY_SIZE = 32;

	static netLocalizedStringInCloudMgr& Get();

	netLocalizedStringInCloudMgr();
	~netLocalizedStringInCloudMgr();

	bool ReqestString(const char* key);
	void ReleaseRequest(const char* key);

	const char* GetStringForRequest(const char* key) const;
	bool IsRequestPending(const char* key) const;
	bool IsRequestFailed(const char* key) const;
private:

	const netLocalizedStringInCloudRequest* GetEntry(const char* key) const;
	netLocalizedStringInCloudRequest* GetEntryNonConst(const char* key);

	typedef atMap<u32, netLocalizedStringInCloudRequest*> StringRequestMap;
	StringRequestMap m_reqMap;
};


#define STRINGS_IN_CLOUD netLocalizedStringInCloudMgr::Get()

class netLocalizedStringInCloudRequest : public CloudListener
{
public:
	netLocalizedStringInCloudRequest();
	virtual ~netLocalizedStringInCloudRequest();

	bool Start(const char* messageKey, const char* lang);
	void Cancel();

	bool IsPending() const	{ return m_status.Pending(); }
	bool Succeeded() const	{ return m_status.Succeeded(); }
	bool Failed() const		{ return m_status.Failed() || m_status.Canceled(); }

	const char* GetString() const;

	//For cloud file handling (when it comes in)
	virtual void OnCloudEvent(const CloudEvent* pEvent);
private:

	bool DoStringFixup();

	CloudRequestID m_cloudReqID;
	datGrowBuffer m_gb;
	netStatus m_status;
	const char* m_stringPtr;

	OUTPUT_ONLY(char m_messageKey[netLocalizedStringInCloudMgr::CLOUD_STRING_KEY_SIZE]);
};



#endif // !INC_NETLOCALIZEDCLOUDSTRINGS_H_
