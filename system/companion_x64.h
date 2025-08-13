//--------------------------------------------------------------------------------------
// companion_x64.h
//--------------------------------------------------------------------------------------
#if RSG_PC

#pragma once

#include "companion.h"

#if COMPANION_APP

#include <windows.h>
#include "net/nethardware.h"
#include "net/netsocket.h"
#include "atl/array.h"

#include "companion_http.h"
#include "network/Live/SocialClubMgr.h"
#include "data/aes.h"

static const unsigned short kCOMPANION_PAIRING_PROTOCOL_PORT = 5261;
static const unsigned short kCOMPANION_HTTP_PROTOCOL_PORT = 8080;

// forward declaration so that it can call CompanionDataPC methods
class CCompanionDataPC;


//--------------------------------------------------------------------------------------
// Companion Paired Device (handles state) for PC
//--------------------------------------------------------------------------------------

class CPairedDevice
{
public:
	CPairedDevice() {}
	CPairedDevice(const netSocketAddress& address);
	~CPairedDevice();

	CPairedDevice&			operator=(const CPairedDevice& other);
	
	netSocketAddress		deviceAddress;
	time_t					connectionTimeOut;
};

//--------------------------------------------------------------------------------------
// Companion Pairing Manager for PC
//--------------------------------------------------------------------------------------

class CPairingManager
{
public:

	CPairingManager() : kPairingProtocolPort(kCOMPANION_PAIRING_PROTOCOL_PORT) {}
	~CPairingManager() {}

	bool				init(CCompanionDataPC* companion);
	void				update();

private:

	CPairedDevice*		getDeviceWithAddress(const netSocketAddress& address);
	void				processIncomingMessages(int len, char* buffer, const netSocketAddress& from);

	const short			kPairingProtocolPort;
	static const short	kPairingMaxBufferLen = 512;

	netSocket			listener;
	atArray<CPairedDevice> devices;
	AES*				codex;
	CCompanionDataPC*	companion;
};


//--------------------------------------------------------------------------------------
// Companion Data class for PC
//--------------------------------------------------------------------------------------

class CCompanionDataPC : public CCompanionData
{
public:
	CCompanionDataPC();
	~CCompanionDataPC();

	void			Initialise();
	void			Update();

	void			OnClientConnected();
	void			OnClientDisconnected();

	RockstarId		GetRockstarId();
	CHttpServer&	GetHttpServer();

private:

	CPairingManager m_pairingMgr;
	CHttpServer		m_httpServer;
	SocialClubMgr*	m_SCMgr;

	RockstarId		m_rockstarId;

protected:
};

#endif	//	COMPANION_APP
#endif	//	RSG_PC