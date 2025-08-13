#if RSG_ORBIS
//--------------------------------------------------------------------------------------
// companion_orbis.cpp
//--------------------------------------------------------------------------------------

#include "companion_orbis.h"

#if COMPANION_APP
//--------------------------------------------------------------------------------------
// CCompanionData class
//--------------------------------------------------------------------------------------
CCompanionData* CCompanionData::GetInstance()
{
	if (!sm_instance)
	{
		sm_instance = rage_new CCompanionDataOrbis();
	}

	return sm_instance;
}
//--------------------------------------------------------------------------------------
// CCompanionDataOrbis class
//--------------------------------------------------------------------------------------
CCompanionDataOrbis::CCompanionDataOrbis() : CCompanionData()
{
}

CCompanionDataOrbis::~CCompanionDataOrbis()
{
	//	Unregister the HTTP request callback
	int ret = sceCompanionHttpdUnregisterRequestCallback();
	if (ret != SCE_OK)
	{
		return;
	}
	//	Terminate CompanionHttpd
	ret = sceCompanionHttpdTerminate();
	//	TODO: Free memory used as option work memory here!

	//	Call down to base class
	CCompanionData::~CCompanionData();
}

void CCompanionDataOrbis::Initialise()
{
	//	Load the companion httpd module
	int ret = sceSysmoduleLoadModule(SCE_SYSMODULE_COMPANION_HTTPD);
	if (ret != SCE_OK ) 
	{
		rlError("sceSysmoduleLoadModule(SCE_SYSMODULE_COMPANION_HTTPD) failed. ret = 0x%x\n", ret);
		return;
	}

	// 	SceAppContentMountPoint mount;
	// 	memset(&mount, 0x00, sizeof(mount));
	// 
	// 	ret = sceAppContentTemporaryDataMount(&mount);
	// 	if (ret != SCE_OK)
	// 	{
	// 		rlError("Unable to mount data for app\n");
	// 		return;
	// 	}

	SceCompanionHttpdOptParam option;
	ret = sceCompanionHttpdOptParamInitialize(&option);
	if (ret != SCE_OK)
	{
		rlError("SceCompanionHttpdOptParam failed to initialise\n");
		return;
	}

	//	Allocate memory used for CompanionHttpd
	void* workMemory = malloc(SCE_COMPANION_HTTPD_DATALENGTH_MIN_WORK_HEAPMEMORY_SIZE);
	if (!workMemory)
	{
		rlError("No memory for CompanionHttpd!\n");
		return;
	}

	//	Set the option parameters
	option.workMemory	= workMemory;
	option.workMemorySize	= SCE_COMPANION_HTTPD_DATALENGTH_MIN_WORK_HEAPMEMORY_SIZE;
	option.workThreadStackSize	= SCE_COMPANION_HTTPD_DATALENGTH_MIN_WORK_STACKMEMORY_SIZE;
	option.port					= 13000;
	option.screenOrientation	= SCE_COMPANION_HTTPD_ORIENTATION_LANDSCAPE;

	option.transceiverThreadCount	= 4;
	option.transceiverStackSize		= SCE_COMPANION_HTTPD_DATALENGTH_MIN_TRANSCEIVER_STACKMEMORY_SIZE;
	option.workDirectory			= "/temp0/"/*mount.data*/;

	//	Initialise CompanionHttpd with our options
	ret = sceCompanionHttpdInitialize(&option);
	if (ret != SCE_OK)
	{
		rlError("Failed to initialise CompanionHttpd\n");
		return;
	}

	//	Register the callback
	ret = sceCompanionHttpdRegisterRequestCallback(CCompanionDataOrbis::CompanionHttpdCallback, this);
	if (ret != SCE_OK)
	{
		rlError("Failed to register Httpd callback\n");
		return;
	}
}

void CCompanionDataOrbis::Update()
{
	//	Update the base class first
	CCompanionData::Update();

	//	Confirm whether an event has occurred
	SceCompanionHttpdEvent e;
	int32_t ret = sceCompanionHttpdGetEvent(&e);
	if (ret != SCE_OK)
	{
		// 		scriptDisplayf("***** Companion App: Get event (%d)", ret);
		// 		switch (ret)
		// 		{
		// 			case SCE_COMPANION_HTTPD_ERROR_INVALID_PARAM:
		// 			{
		// 				scriptDisplayf("***** Companion App: Get event (SCE_COMPANION_HTTPD_ERROR_INVALID_PARAM)\n");
		// 			}
		// 			break;
		// 			case SCE_COMPANION_HTTPD_ERROR_NOT_INITIALIZED:
		// 			{
		// 				scriptDisplayf("***** Companion App: Get event (SCE_COMPANION_HTTPD_ERROR_NOT_INITIALIZED)\n");
		// 			}
		// 			break;
		// 			case SCE_COMPANION_HTTPD_ERROR_NO_EVENT:
		// 			{
		// 				scriptDisplayf("***** Companion App: Get event (SCE_COMPANION_HTTPD_ERROR_NO_EVENT)\n");
		// 			}
		// 			break;
		// 		}
		return;
	}

	switch (e.event)
	{
		case SCE_COMPANION_HTTPD_EVENT_CONNECT:
		{
			m_clientsList.push_back(e.data.userId);
			this->OnClientConnected();
		}
		break;
		case SCE_COMPANION_HTTPD_EVENT_DISCONNECT:
		{
			m_clientsList.DeleteMatches(e.data.userId);
			this->OnClientDisconnected();
		}
		break;
		default:
		{
		}
		break;
	}
}

void CCompanionDataOrbis::OnClientConnected()
{
	if (!IsConnected())
	{
		int ret = sceCompanionHttpdStart();
		if (ret != SCE_OK)
		{
			return;
		}
	}

	//	Call down to base class
	CCompanionData::OnClientConnected();
}

void CCompanionDataOrbis::OnClientDisconnected()
{
	if (m_clientsList.size() > 0)
	{
		return;
	}

	if (IsConnected())
	{
		int ret = sceCompanionHttpdStop();
		if (ret != SCE_OK)
		{
			return;
		}
	}

	//	Call down to base class
	CCompanionData::OnClientDisconnected();
}

int32_t CCompanionDataOrbis::CompanionHttpdCallback(SceUserServiceUserId /*userId*/userId, const SceCompanionHttpdRequest* httpRequest, SceCompanionHttpdResponse* httpResponse, void* params)
{
	//	SYS_CS_SYNC(s_critSecToken);
	CCompanionDataOrbis* companionData = (CCompanionDataOrbis*)params;

	if (httpRequest->method == SCE_COMPANION_HTTPD_METHOD_GET)
	{
		if (strcmp(httpRequest->url, "/index.html?blips") == 0)
		{
			int	ret = sceCompanionHttpdAddHeader("Content-type", "application/json", httpResponse);
			if(ret != SCE_OK)
			{
				return ret;
			}

			if (companionData->GetFirstClient() != userId) 
			{
				// this is not our first connected client - error out
				static const char* jsonErrorMsg = "{\"Error\": \"NotPrimaryClient\"}";
				static const int jsonErrorMsgLen = strlen(jsonErrorMsg);

				ret = sceCompanionHttpdSetBody(jsonErrorMsg, jsonErrorMsgLen, httpResponse);
				if(ret != SCE_OK)
				{
					return ret;
				}
			}
			else
			{
				char data[1024];
				CCompanionBlipMessage* blipMessage = companionData->GetBlipMessage();

				if (blipMessage)
				{
					char* encodedBlipMessage = blipMessage->GetEncodedMessage();
					sprintf(data, "{ \"GTA5\" : \"%s\" }", encodedBlipMessage);
					delete blipMessage;

					ret = sceCompanionHttpdSetBody(data, 1024, httpResponse);
					if(ret != SCE_OK)
					{
						return ret;
					}		
				}
				else
				{
					companionData->RemoveOldBlipMessages();
				}
			}

			return SCE_OK;
		}
		else if (strcmp(httpRequest->url, "/index.html?route") == 0)
		{
			int	ret = sceCompanionHttpdAddHeader("Content-type", "text/html", httpResponse);
			if(ret != SCE_OK)
			{
				return ret;
			}

			if (companionData->GetFirstClient() != userId) 
			{
				// this is not our first connected client - error out
				static const char* jsonErrorMsg = "{\"Error\": \"NotPrimaryClient\"}";
				static const int jsonErrorMsgLen = strlen(jsonErrorMsg);

				ret = sceCompanionHttpdSetBody(jsonErrorMsg, jsonErrorMsgLen, httpResponse);
				if(ret != SCE_OK)
				{
					return ret;
				}
			}
			else
			{
				char data[1024];
				CCompanionRouteMessage* routeMessage = companionData->GetRouteMessage();

				if (routeMessage)
				{
					char* encodedRouteMessage = routeMessage->GetEncodedMessage();
					sprintf(data, "{ \"GTA5\" : \"%s\" }", encodedRouteMessage);
					delete routeMessage;

					ret = sceCompanionHttpdSetBody(data, 1024, httpResponse);
					if(ret != SCE_OK)
					{
						return ret;
					}
				}
				else
				{
					companionData->RemoveOldRouteMessages();
				}
			}			

			return SCE_OK;
		}
		else if (strcmp(httpRequest->url, "/index.html?get_env") == 0)
		{
			int	ret = sceCompanionHttpdAddHeader("Content-type", "application/json", httpResponse);
			if(ret != SCE_OK)
			{
				return ret;
			}

			char data[1024];
			const char* envName = g_rlTitleId->m_RosTitleId.GetEnvironmentName();

			sprintf(data, "{ \"GTA5Env\" : \"%s\" }", envName ? envName : "unknown");

			ret = sceCompanionHttpdSetBody(data, strlen(data), httpResponse);
			if(ret != SCE_OK)
			{
				return ret;
			}

			return SCE_OK;
		}
		// 		else
		// 		{
		// 			Displayf("***** url is '%s'", httpRequest->url);
		// 		}
	}
	else if (httpRequest->method == SCE_COMPANION_HTTPD_METHOD_POST)
	{
		if (companionData->GetFirstClient() != userId) 
		{
			// Do nothing if the post request are not coming from our primary client
			return SCE_OK;
		}

		if (strcmp(httpRequest->url, "/waypoint") == 0)
		{
			std::string params(httpRequest->body, httpRequest->bodySize);
			size_t index = 0;
			size_t param_index = 0;
			double waypointX = 0;
			double waypointY = 0;

			while(index < (int)params.length())
			{
				size_t oldindex = index;
				index = params.find(',', index);
				std::string param_str;

				if(index != std::string::npos)
				{
					//	Index is fine, grab part of the string
					param_str = params.substr(oldindex, index - oldindex);
					index++;
				}
				else
				{
					//	Index is invalid, grab the remainder of the string
					param_str = params.substr(oldindex);
					index = params.length();
				}

				switch(param_index)
				{
					// Waypoint x
					case 0:
					{
						waypointX = strtod(param_str.c_str(), NULL);
					}
					break;
					//	Waypoint y
					case 1:
					{
						waypointY = strtod(param_str.c_str(), NULL);
					}
					break;
				}

				param_index++;
			}

			companionData->CreateWaypoint(waypointX, waypointY);
			return SCE_OK;
		}
		else if (strcmp(httpRequest->url, "/no_waypoint") == 0)
		{
			companionData->RemoveWaypoint();
			return SCE_OK;
		}
		else if (strcmp(httpRequest->url, "/resend_all") == 0)
		{
			companionData->ResendAll();
			return SCE_OK;
		}
		else if (strcmp(httpRequest->url, "/toggle_pause") == 0)
		{
			companionData->TogglePause();
			return SCE_OK;
		}
	}

	return SCE_COMPANION_HTTPD_ERROR_NOT_GENERATE_RESPONSE;
}

#endif	//	COMPANION_APP

#endif	//	RSG_ORBIS