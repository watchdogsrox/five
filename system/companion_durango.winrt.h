//--------------------------------------------------------------------------------------
// companion_durango.winrt.h
//--------------------------------------------------------------------------------------
#if RSG_DURANGO

#pragma once

#include "companion.h"

#if COMPANION_APP

using namespace Windows::Data::Json;
using namespace Windows::Foundation;
using namespace Windows::Xbox::SmartGlass;

#define MAX_DEVICES		4

class CCompanionDataDurango;

typedef enum eSGDeviceMode
{
	SGDEVICEMODE_NONE,

	SGDEVICEMODE_HTML,
	SGDEVICEMODE_TEXTENTRY,
} eSGDeviceMode;

//--------------------------------------------------------------------------------------
// CompanionSmartGlassDevice structure
//--------------------------------------------------------------------------------------
typedef struct
{
	Platform::String^	id;
	SmartGlassDevice^   device;
	eSGDeviceMode		mode;

}sCompanionSmartGlassDevice;
//--------------------------------------------------------------------------------------
// CompanionSmartGlassConnector class
//--------------------------------------------------------------------------------------
ref class CCompanionSmartGlassConnector
{
friend class CCompanionDataDurango;
public:
	void Initialise();

	void OnDeviceAdded(SmartGlassDeviceWatcher^ sender, SmartGlassDevice^ device);
	void OnDeviceRemoved(SmartGlassDeviceWatcher^ sender, SmartGlassDevice^ device);
	void OnFindAllCompletion(IAsyncOperation<SmartGlassDeviceCollection^>^ asyncOp, AsyncStatus status);
	void OnMessageSendCompletion(IAsyncAction^ asyncOp, AsyncStatus status);

	void OnMessageReceived(SmartGlassHtmlSurface^ pHtmlSurface, SmartGlassMessageReceivedEventArgs^ pArgs);
	void OnTextChanged(SmartGlassTextEntrySurface^ pTextEntrySurface, SmartGlassTextEntryEventArgs^ pArgs);
	void OnTextEntryFinished(SmartGlassTextEntrySurface^ pTextEntrySurface, SmartGlassTextEntryEventArgs^ pArgs);

	void SendMessage(Platform::String^ pMessage);

	void SetCurrentDeviceToTextEntry();
	void SetCurrentDeviceToHTML();

internal:
	CCompanionSmartGlassConnector(CCompanionDataDurango* parent);

	void UpdateCurrentDeviceTextEntry(const atString& text);

private:
	~CCompanionSmartGlassConnector();

	bool IsCurrentClient(SmartGlassHtmlSurface^ surface);
	SmartGlassDevice^ GettCurrentDevice();
	void SwitchCurrentDeviceMode(eSGDeviceMode newMode);

	CCompanionDataDurango*		m_pParent;
	SmartGlassDeviceWatcher^	m_deviceWatcher;
	sCompanionSmartGlassDevice	m_pDeviceArray[MAX_DEVICES];
	int							m_deviceCount;
	CRITICAL_SECTION			m_deviceCS;
	LPCRITICAL_SECTION			m_pDeviceCS;
	bool						m_readyToSend;
	wstring						m_lastTextEntry;	// only one device can send text at a time
};
//--------------------------------------------------------------------------------------
// Companion Data class for Durango
//--------------------------------------------------------------------------------------
class CCompanionDataDurango : public CCompanionData
{
public:
	CCompanionDataDurango();
	~CCompanionDataDurango();

	void Initialise();
	void Update();

	void OnClientConnected();
	void OnClientDisconnected();

	bool SetTextModeForCurrentDevice(bool enable);
	void SetTextEntryForCurrentDevice(const atString&);

private:
	CCompanionSmartGlassConnector^	m_deviceConnector;
protected:
};

#endif	//	COMPANION_APP
#endif	//	RSG_DURANGO
