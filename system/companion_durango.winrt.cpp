#if RSG_DURANGO
//--------------------------------------------------------------------------------------
// companion_durango.winrt.cpp
//--------------------------------------------------------------------------------------

#include "companion_durango.winrt.h"

#if COMPANION_APP

//--------------------------------------------------------------------------------------
// CCompanionData class
//--------------------------------------------------------------------------------------
CCompanionData* CCompanionData::GetInstance()
{
	if (!sm_instance)
	{
		sm_instance = rage_new CCompanionDataDurango();
	}

	return sm_instance;
}
//--------------------------------------------------------------------------------------
// CompanionSmartGlassConnector class
//--------------------------------------------------------------------------------------
CCompanionSmartGlassConnector::CCompanionSmartGlassConnector(CCompanionDataDurango* parent)
: m_pParent(parent)
, m_readyToSend(true)
{
	m_pDeviceCS = &m_deviceCS;
	InitializeCriticalSection(m_pDeviceCS);
}

CCompanionSmartGlassConnector::~CCompanionSmartGlassConnector()
{
	DeleteCriticalSection(m_pDeviceCS);
}

void CCompanionSmartGlassConnector::Initialise()
{
	//	Create the SmartGlass device watcher
	m_deviceWatcher = ref new SmartGlassDeviceWatcher();
	m_deviceWatcher->DeviceAdded += ref new TypedEventHandler<SmartGlassDeviceWatcher^, SmartGlassDevice^>(this, &CCompanionSmartGlassConnector::OnDeviceAdded);
	m_deviceWatcher->DeviceRemoved += ref new TypedEventHandler<SmartGlassDeviceWatcher^, SmartGlassDevice^>(this, &CCompanionSmartGlassConnector::OnDeviceRemoved);

	SmartGlassDevice::FindAllAsync()->Completed = ref new AsyncOperationCompletedHandler<SmartGlassDeviceCollection^>(this, &CCompanionSmartGlassConnector::OnFindAllCompletion);
}

void CCompanionSmartGlassConnector::OnFindAllCompletion(IAsyncOperation<SmartGlassDeviceCollection^>^ asyncOp, AsyncStatus /*status*/)
{
	SmartGlassDeviceCollection^ devices = asyncOp->GetResults();
	UINT devicesSize = devices->Size;
	for( UINT i = 0; i < devicesSize; i++ )
	{
		OnDeviceAdded(m_deviceWatcher, devices->GetAt(i));
	}
}

void CCompanionSmartGlassConnector::OnMessageSendCompletion(IAsyncAction^ /*asyncOp*/, AsyncStatus status)
{
	if (status == AsyncStatus::Completed)
	{
		this->m_readyToSend = true;
	}
}

void CCompanionSmartGlassConnector::OnDeviceAdded(SmartGlassDeviceWatcher^ /*sender*/, SmartGlassDevice^ device)
{
	bool knownDevice = false;

	//	Need a CS here to prevent changes to the array while iterating
	EnterCriticalSection(m_pDeviceCS);

	for(int i = 0; i < m_deviceCount -1; i++)
	{
		if(m_pDeviceArray[i].id == device->DirectSurface->Id) // Already know about this device, ignore
		{
			CompanionDisplayf("Device to be added already in known list, ignoring");
			knownDevice = true;
			break;
		}
	}

	if(m_deviceCount < MAX_DEVICES && !knownDevice)
	{
		SmartGlassHtmlSurface^ pHtmlSurface = device->HtmlSurface;

		m_pDeviceArray[m_deviceCount].id = pHtmlSurface->Id;
		m_pDeviceArray[m_deviceCount].device = device;
		m_pDeviceArray[m_deviceCount].mode = SGDEVICEMODE_HTML;


		std::wstring pWId(pHtmlSurface->Id->Data());
		std::string pId(pWId.begin(), pWId.end());
		CompanionDisplayf("Adding new device to list = %s", pId.c_str());		

		//	Set up the message received event
		pHtmlSurface->MessageReceived += ref new TypedEventHandler<SmartGlassHtmlSurface^, SmartGlassMessageReceivedEventArgs^>(this, &CCompanionSmartGlassConnector::OnMessageReceived);

		SmartGlassTextEntrySurface^ pTextEntrySurface = device->TextEntrySurface;
		pTextEntrySurface->TextChanged += ref new TypedEventHandler<SmartGlassTextEntrySurface^, SmartGlassTextEntryEventArgs^>(this, &CCompanionSmartGlassConnector::OnTextChanged);
		pTextEntrySurface->TextEntryFinished += ref new TypedEventHandler<SmartGlassTextEntrySurface^, SmartGlassTextEntryEventArgs^>(this, &CCompanionSmartGlassConnector::OnTextEntryFinished);
		
		// set up mode as html
		device->SetActiveSurfaceAsync(pHtmlSurface);

		m_pParent->OnClientConnected();

		m_deviceCount++;
	}

	LeaveCriticalSection(m_pDeviceCS);
}

void CCompanionSmartGlassConnector::OnDeviceRemoved(SmartGlassDeviceWatcher^ /*sender*/, SmartGlassDevice^ device)
{
	//	Need a CS here to prevent changes to the array while iterating
	EnterCriticalSection(m_pDeviceCS);

	//Get the HtmlSurface
	SmartGlassHtmlSurface^ pHtmlSurface = device->HtmlSurface;
	for(int i = 0; i < m_deviceCount; i++)
	{
		if(m_pDeviceArray[i].id == pHtmlSurface->Id)
		{
			CompanionDisplayf("Device disconnected and is device at index %i, device count now: %d",i, m_deviceCount - 1);
			m_pDeviceArray[i].id = nullptr;
			m_pDeviceArray[i].device = nullptr;
			m_pDeviceArray[i].mode = SGDEVICEMODE_NONE;

			//if the pCompanion removed is not the last on the array, swap the last one with the deleted.
			if(i < m_deviceCount - 1)
			{
				CompanionDisplayf("Replacing index %i with device id at index %i",i, m_deviceCount - 1);
				m_pDeviceArray[i] = m_pDeviceArray[m_deviceCount - 1];
				m_pDeviceArray[m_deviceCount - 1].id = nullptr;
				m_pDeviceArray[m_deviceCount - 1].device = nullptr;
				m_pDeviceArray[m_deviceCount - 1].mode = SGDEVICEMODE_NONE;
			}
			m_deviceCount--;
			break;
		}
	}

	if (m_deviceCount > 0)
	{
		pHtmlSurface = m_pDeviceArray[0].device->HtmlSurface;
		m_pDeviceArray[0].device->SetActiveSurfaceAsync(pHtmlSurface);
	}
	else
	{
		m_pParent->OnClientDisconnected();
	}

	

	LeaveCriticalSection(m_pDeviceCS);
}

bool CCompanionSmartGlassConnector::IsCurrentClient(SmartGlassHtmlSurface^ surface)
{
	auto currentDevice = GettCurrentDevice();
	
	return (currentDevice != nullptr && currentDevice->Id == surface->Id);
}

SmartGlassDevice^ CCompanionSmartGlassConnector::GettCurrentDevice()
{
	if (m_deviceCount > 0)
	{
		return m_pDeviceArray[0].device;
	}

	return nullptr;
}

void CCompanionSmartGlassConnector::SwitchCurrentDeviceMode(eSGDeviceMode newMode)
{
	auto currentDevice = GettCurrentDevice();
	if (currentDevice == nullptr)
	{
		return;
	}

	auto companionDevice = &m_pDeviceArray[0];
	if (companionDevice->mode != newMode)
	{
		switch (newMode)
		{
		case SGDEVICEMODE_HTML:
			CompanionDisplayf("Switching current device to HTMLSurface mode");
			currentDevice->SetActiveSurfaceAsync(currentDevice->HtmlSurface);
			break;

		case SGDEVICEMODE_TEXTENTRY:
			{
				CompanionDisplayf("Switching current device to TextEntrySurface mode");

				// Reset text entry
				m_lastTextEntry = L"";

				// Setup text entry
				currentDevice->TextEntrySurface->Language = ref new Platform::String(L"en-US");
				//currentDevice->Prompt = ref new Platform::String(L"Prompt for game entry");
				//currentDevice->MaxLength = 24;

				currentDevice->SetActiveSurfaceAsync(currentDevice->TextEntrySurface);
			}
			break;

		default:
			CompanionDisplayf("Unrecognized mode requested in SwitchCurrentDeviceMode (%i)",newMode);
			newMode = companionDevice->mode;	// don't change the mode
			break;
		}

		companionDevice->mode = newMode;
	}
}

void CCompanionSmartGlassConnector::OnMessageReceived(SmartGlassHtmlSurface^ pHtmlSurface, SmartGlassMessageReceivedEventArgs^ pArgs)
{
	EnterCriticalSection(m_pDeviceCS);

	std::wstring pWId(pHtmlSurface->Id->Data());
	std::string pId(pWId.begin(), pWId.end());

	// The value is expected to be an object with a 'message' field.
	JsonObject^ object = JsonObject::Parse(pArgs->Message);

	if (object->HasKey(L"waypoint"))
	{
		if (IsCurrentClient(pHtmlSurface))
		{
			Platform::String^ message = object->GetNamedString(L"waypoint");
			std::wstring paramsW(message->Begin());


			std::string params(paramsW.begin(), paramsW.end());

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

			this->m_pParent->CreateWaypoint(waypointX, waypointY);
		}
	}
	else if (object->HasKey(L"no_waypoint"))
	{
		if (IsCurrentClient(pHtmlSurface))
		{
			this->m_pParent->RemoveWaypoint();
		}
	}
	else if (object->HasKey(L"resend_all"))
	{
		if (IsCurrentClient(pHtmlSurface))
		{
			CompanionDisplayf("Resend_all received from #%s",pId.c_str());
			this->m_pParent->ResendAll();
		}
	}
	else if (object->HasKey(L"toggle_pause"))
	{
		if (IsCurrentClient(pHtmlSurface))
		{
			this->m_pParent->TogglePause();
		}
	}
	else if (object->HasKey(L"blips") && this->m_readyToSend)
	{
		char data[1024];

		if (IsCurrentClient(pHtmlSurface) ==  false)
		{
			static const char* jsonErrorMsg = "{\"Error\": \"NotPrimaryClient\"}";
			static const int jsonErrorMsgLen = strlen(jsonErrorMsg);

			memcpy(data, jsonErrorMsg, jsonErrorMsgLen);
			data[jsonErrorMsgLen] = 0;
		}
		else
		{
			CCompanionBlipMessage* blipMessage = this->m_pParent->GetBlipMessage();

			if (blipMessage)
			{
				char* encodedBlipMessage = blipMessage->GetEncodedMessage();
				sprintf(data, "{ \"GTA5\" : \"%s\" }", encodedBlipMessage);
				delete blipMessage;
			}

			//this->m_pParent->RemoveOldBlipMessages();
		}

		std::string dataStr = std::string(data);
		std::wstring dataStrW = std::wstring(dataStr.begin(), dataStr.end());
			
		JsonValue^ message = JsonValue::CreateStringValue(Platform::StringReference(dataStrW.c_str()));

		this->m_readyToSend = false;
		IAsyncAction^ action = pHtmlSurface->SubmitMessageAsync(message->Stringify());
		action->Completed = ref new AsyncActionCompletedHandler(this, &CCompanionSmartGlassConnector::OnMessageSendCompletion);		
	}
	else if (object->HasKey(L"routes") && this->m_readyToSend)
	{
		char data[1024];

		if (IsCurrentClient(pHtmlSurface) ==  false)
		{
			static const char* jsonErrorMsg = "{\"Error\": \"NotPrimaryClient\"}";
			static const int jsonErrorMsgLen = strlen(jsonErrorMsg);

			memcpy(data, jsonErrorMsg, jsonErrorMsgLen);
			data[jsonErrorMsgLen] = 0;
		}
		else
		{
			CCompanionRouteMessage* routeMessage = this->m_pParent->GetRouteMessage();

			if (routeMessage)
			{
				char* encodedRouteMessage = routeMessage->GetEncodedMessage();
				sprintf(data, "{ \"GTA5\" : \"%s\" }", encodedRouteMessage);
				delete routeMessage;
			}

			//this->m_pParent->RemoveOldRouteMessages();
		}

		std::string dataStr = std::string(data);
		std::wstring dataStrW = std::wstring(dataStr.begin(), dataStr.end());

		JsonValue^ message = JsonValue::CreateStringValue(Platform::StringReference(dataStrW.c_str()));

		this->m_readyToSend = false;
		IAsyncAction^ action = pHtmlSurface->SubmitMessageAsync(message->Stringify());
		action->Completed = ref new AsyncActionCompletedHandler(this, &CCompanionSmartGlassConnector::OnMessageSendCompletion);
	}
	else if (object->HasKey(L"get_env") && this->m_readyToSend)
	{
		char data[1024];

		if (IsCurrentClient(pHtmlSurface))
		{
			const char* envName = g_rlTitleId->m_RosTitleId.GetEnvironmentName();

			sprintf(data, "{ \"GTA5Env\" : \"%s\" }", envName ? envName : "unknown");

			std::string dataStr = std::string(data);
			std::wstring dataStrW = std::wstring(dataStr.begin(), dataStr.end());

			JsonValue^ message = JsonValue::CreateStringValue(Platform::StringReference(dataStrW.c_str()));

			this->m_readyToSend = false;
			IAsyncAction^ action = pHtmlSurface->SubmitMessageAsync(message->Stringify());
			action->Completed = ref new AsyncActionCompletedHandler(this, &CCompanionSmartGlassConnector::OnMessageSendCompletion);
		}
	}


	LeaveCriticalSection(m_pDeviceCS);
}


// helper method that updates a string and optionally sends the changes to low level keyboard
bool UpdateStringFromString(const wstring& src, wstring& dst, bool ioKeyPresses)
{
	int  numChanges = 0, srcLen = src.length(), dstLen = dst.length();
	for (int i=0; i<srcLen; ++i)
	{
		wchar_t a_char = src.at(i);
		if (a_char >= 65 && a_char < 91)
		{
			a_char += 32;
		}

		if (i < dstLen)
		{
			wchar_t b_char = dst.at(i);
			if (a_char != b_char)
			{
				int numCharsLeft = dstLen - i;
				
				if (ioKeyPresses)
				{
					// at this point it means that a character was deleted but maybe not at the end
					// so we need to pass backspace a couple of times
					for (int j=0; j<numCharsLeft; ++j) 
					{
						//CompanionDisplayf("OnTextChanged + backspace");
						if (!ioKeyboard::SetTextCharPressed(L'\b', KEY_BACK))
						{
							// we can't submit any more key presses - return current change
							if (dstLen - j > 0)
							{
								dst.erase(dstLen - j, string::npos);
							}
							return true;
						}
					}
				}
				dst.erase(i, string::npos);
				dstLen = dst.length();
				numChanges++;
			}
		}

		if (i >= dstLen)
		{
			numChanges++;
			// valid scaleform keyboard chars: 0-9, a-z, ' ', '.', '-'
			if ((a_char >= 48 && a_char < 58) || (a_char >= 97 && a_char < 123) || a_char == 32 || a_char == 45 || a_char == 46)
			{				
				if (ioKeyPresses)
				{
					//CompanionDisplayf("OnTextChanged + %c", a_char);
					if (!ioKeyboard::SetTextCharPressed(a_char, KEY_NULL))
					{
						// we can't submit any more key presses - return current change
						return true;
					}
				}
				dst.append(1, a_char);
			}
		}
	}

	if (srcLen < dst.length())
	{
		numChanges++;
		int numCharsToRemove = dst.length() - srcLen;
		for (int i=0; i<numCharsToRemove; ++i)
		{
			//CompanionDisplayf("OnTextChanged + backspace");
			if (!ioKeyboard::SetTextCharPressed(L'\b', KEY_BACK))
			{
				int dstLen = dst.length();
				if (dstLen - i > 0)
				{
					dst.erase(dstLen - i, string::npos);
				}
				// we can't submit any more key presses - return current change
				return true;
			}
		}
		dst.erase(srcLen,string::npos);
	}

	return (numChanges > 0);
}

void CCompanionSmartGlassConnector::OnTextChanged(SmartGlassTextEntrySurface^ pTextEntrySurface, SmartGlassTextEntryEventArgs^ pArgs)
{
	EnterCriticalSection(m_pDeviceCS);
	std::wstring wText(pTextEntrySurface->Text->Data());
	
	// Compare current text entry with the one from device - hopefully its a single character change
	if (UpdateStringFromString(wText, m_lastTextEntry, true))
	{
		pTextEntrySurface->Text = ref new Platform::String(m_lastTextEntry.c_str());
	}

	
	std::string text(wText.begin(), wText.end());
	std::string changed(m_lastTextEntry.begin(), m_lastTextEntry.end());
	LeaveCriticalSection(m_pDeviceCS);
}


void CCompanionSmartGlassConnector::OnTextEntryFinished(SmartGlassTextEntrySurface^ pTextEntrySurface, SmartGlassTextEntryEventArgs^ pArgs)
{
	bool entryCancelled = pTextEntrySurface->Result == Windows::Xbox::SmartGlass::TextEntryResult::Cancel;

	if (!entryCancelled)
	{
		std::wstring wText(pTextEntrySurface->Text->Data());
		std::string text(wText.begin(), wText.end());

		CompanionDisplayf("OnTextEntryFinished (Accepted): %s",text.c_str());
		ioKeyboard::SetTextCharPressed(L'\r', KEY_RETURN);
	}
	else
	{
		CompanionDisplayf("OnTextEntryFinished (Cancelled)");
	}

	SwitchCurrentDeviceMode(SGDEVICEMODE_HTML);
}

void CCompanionSmartGlassConnector::UpdateCurrentDeviceTextEntry(const atString& text)
{
	EnterCriticalSection(m_pDeviceCS);

	auto currentDevice = GettCurrentDevice();
	if (currentDevice != nullptr)
	{
		auto companionDevice = &m_pDeviceArray[0];
		// Make sure that we have the device is text entry mode
		if (companionDevice->mode == SGDEVICEMODE_TEXTENTRY)
		{
			std::string asciiText = text.c_str();
			std::wstring wideString(asciiText.begin(), asciiText.end());
			// the text is actually different
			if (wideString.compare(m_lastTextEntry) != 0)
			{
				// Update text and pass it to the screen 
				m_lastTextEntry = wideString;
				if (currentDevice->TextEntrySurface->Visible)
				{
					currentDevice->TextEntrySurface->Text = ref new Platform::String(m_lastTextEntry.c_str());
				}
			}
		}	
	}	

	LeaveCriticalSection(m_pDeviceCS);
}

void CCompanionSmartGlassConnector::SetCurrentDeviceToTextEntry()
{
	SwitchCurrentDeviceMode(SGDEVICEMODE_TEXTENTRY);
}

void CCompanionSmartGlassConnector::SetCurrentDeviceToHTML()
{
	SwitchCurrentDeviceMode(SGDEVICEMODE_HTML);
}

//--------------------------------------------------------------------------------------
// CCompanionDataDurango class
//--------------------------------------------------------------------------------------
CCompanionDataDurango::CCompanionDataDurango() : CCompanionData()
{
}

CCompanionDataDurango::~CCompanionDataDurango()
{
	//	Call down to base class
	CCompanionData::~CCompanionData();
}

void CCompanionDataDurango::Initialise()
{
	m_deviceConnector = ref new CCompanionSmartGlassConnector(this);
	m_deviceConnector->Initialise();
}

void CCompanionDataDurango::Update()
{
	if (!IsConnected())
	{
		return;
	}

	//	Update the base class first
	CCompanionData::Update();
}

void CCompanionDataDurango::OnClientConnected()
{
	//	Call down to base class
	CCompanionData::OnClientConnected();
}

void CCompanionDataDurango::OnClientDisconnected()
{
	//	Call down to base class
	CCompanionData::OnClientDisconnected();
}

bool CCompanionDataDurango::SetTextModeForCurrentDevice(bool enable)
{
	if (!IsConnected())
	{
		return false;
	}

	if (enable)
		m_deviceConnector->SetCurrentDeviceToTextEntry();
	else
		m_deviceConnector->SetCurrentDeviceToHTML();

	return true;
}

void CCompanionDataDurango::SetTextEntryForCurrentDevice(const atString& text)
{
	if (IsConnected())
	{
		m_deviceConnector->UpdateCurrentDeviceTextEntry(text);
	}
}



#endif	//	COMPANION_APP

#endif	//	RSG_DURANGO
