#if RSG_PC
//--------------------------------------------------------------------------------------
// companion_x64.cpp
//--------------------------------------------------------------------------------------

#include "companion_x64.h"

#if COMPANION_APP
#include "file/tcpip.h"
#include "control/replay/file/FileStorePC.h"

#define LIMIT_DEVICE_PAIRING_TO_ROCKSTARID 1
PARAM(CompanionNoSCNeeded, "Companion won't do filtering based on SocialClub ID");

#define LE_CHR(a,b,c,d) ( ((a)<<24) | ((b)<<16) | ((c)<<8) | (d) )

const int kMessagePayloadSize = 60;
const int kConnectionTimeOutDuration = 5;	// this is in seconds

static unsigned char unique_key_ifruit[32] = {
	0x45, 0x3e, 0xea, 0x4a, 0x51, 0x53, 0x20, 0x4b,
	0xa5, 0x18, 0xc0, 0xb4, 0x49, 0xea, 0x06, 0x00,
	0xb7, 0x07, 0xfa, 0x8f, 0xf5, 0xf4, 0x70, 0x72,
	0xf4, 0x06, 0xcf, 0x1d, 0x9b, 0xde, 0xf3, 0xc3
};

const unsigned int kMessageHeader 					= LE_CHR('s', 's', 'p', 'c');
const unsigned int kMessageBroadcastDiscoveryTag 	= LE_CHR('b', 'c', 'd', 'g');
const unsigned int kMessageBroadcastReplyTag 		= LE_CHR('b', 'c', 'r', 't');
const unsigned int kMessageDeviceConnectTag 		= LE_CHR('d', 'c', 't', 'g');
const unsigned int kMessageDeviceDisconnectTag 		= LE_CHR('d', 'd', 't', 'g');
const unsigned int kMessageDevicePingTag 			= LE_CHR('d', 'p', 'g', 't');
const unsigned int kMessageGameConnectionAccepted	= LE_CHR('g', 'c', 'a', 'c');
const unsigned int kMessageGameConnectionBusy		= LE_CHR('g', 'c', 'b', 's');

struct sComMessage
{
	u32	m_Header;
	u32 m_PayloadSize;
	u32 m_MessageTag;
	char m_data[kMessagePayloadSize];
};


//////////////////////////////////////////////////////////////////////////
// CPairedDevice
//////////////////////////////////////////////////////////////////////////
CPairedDevice::CPairedDevice(const netSocketAddress& from) : deviceAddress(from)
{
	connectionTimeOut = time(NULL);
}

CPairedDevice::~CPairedDevice()
{
}

CPairedDevice& CPairedDevice::operator =(const CPairedDevice& other)
{
	deviceAddress = other.deviceAddress;
	connectionTimeOut = other.connectionTimeOut;

	return *this;
}

//////////////////////////////////////////////////////////////////////////
// CPairingManager
//////////////////////////////////////////////////////////////////////////

bool CPairingManager::init(CCompanionDataPC* comp)
{
	// initialize the listening socket - is there to catch any broadcasts for SecondScreen
	if (!netHardware::CreateSocket(&listener, kPairingProtocolPort, NET_PROTO_UDP, NET_SOCKET_NONBLOCKING))
	{
		return false;
	}

	// initialize our crypto lib
	codex = rage_new AES(unique_key_ifruit);

	companion = comp;

	return true;
}

void CPairingManager::update()
{
	if (!listener.CanSendReceive())
	{
		return;
	}

	time_t timeNow = time(NULL);

	if (listener.IsReceivePending())
	{
		netSocketAddress from;
		netSocketError error;
		char buffer[kPairingMaxBufferLen];
		int len = listener.Receive(&from, &buffer, kPairingMaxBufferLen, &error);
		if (len > 0)
		{
			buffer[len] = 0;
			u32* msgPtr = (u32*)buffer;

			// There is nothing to process if this is not for us, the protocol header makes sure
			if (*msgPtr++ == kMessageHeader)
			{				
				processIncomingMessages(len - 4, (char*)msgPtr, from);
			}
		}
	}

	atArray<CPairedDevice>::iterator it = devices.begin();
	while(it != devices.end())
	{
		if (timeNow - (it)->connectionTimeOut > kConnectionTimeOutDuration)
		{
			it = devices.erase(it);
			if (devices.GetCount() == 0)
			{
				companion->OnClientDisconnected();
				companion->GetHttpServer().shutdown();
			}
		}
		else
		{
			++it;
		}
	}
}

void CPairingManager::processIncomingMessages(int len, char* buffer, const netSocketAddress& from)
{
	u32* msgPtr = (u32*)buffer;
	u32 payloadSize = *msgPtr++;

	CPairedDevice* device = getDeviceWithAddress(from);
	if (device)
	{
		u32 msgTag = *msgPtr++;
		switch (msgTag)
		{
		case kMessageDevicePingTag:
			{
				sComMessage message;
				message.m_Header = kMessageHeader;
				message.m_PayloadSize = 4;
				message.m_MessageTag = kMessageDevicePingTag;

				netSocketError error;
				listener.Send(from, &message, 12, &error);

				device->connectionTimeOut = time(NULL);
			}
			break;

		case kMessageDeviceDisconnectTag:
			{
				CompanionDisplayf("Received device disconnect msg, from: %s",from.ToString());

				// iterate through the connected devices and force the device to be removed on next update
				atArray<CPairedDevice>::iterator it = devices.begin();
				while(it != devices.end())
				{
					if ((it)->deviceAddress == from)
					{
						it = devices.erase(it);
					}
					else
					{
						++it;
					}
				}

				// If we have no more devices, disconnect
				if (devices.GetCount() == 0)
				{
					companion->OnClientDisconnected();
					companion->GetHttpServer().shutdown();
				}
			}
			break;

		default:
			break;
		}
	}
	else
	{
		// we don't have a device in our list with that address,
		// all messages not address to a device in a list, are assumed encoded
		codex->Decrypt(msgPtr, payloadSize);

		u32 msgTag = *msgPtr++;
		switch(msgTag)
		{
		case kMessageBroadcastDiscoveryTag:
			{
				CompanionDisplayf("Received broadcast discoverability msg, from: %s",from.ToString());

				if (payloadSize != 8 && (signed int)payloadSize == (len - 4))
				{
					// we expectg a payload of 8, anything else and this is not what we agreed!
					return;
				}
#if LIMIT_DEVICE_PAIRING_TO_ROCKSTARID
				char rockstarIdString[20];
				sprintf_s(rockstarIdString, 20, "%llu", companion->GetRockstarId());
				u32 rIdHash = atStringHash(rockstarIdString,0);

				u32 remoteUserIdHash = *msgPtr;
#if !__FINAL
				const bool requireSClubId = PARAM_CompanionNoSCNeeded.Get() == false;
				if (requireSClubId == true && rIdHash != remoteUserIdHash)
#else
				if (rIdHash != remoteUserIdHash)
#endif // !__FINAL
				{
					return;
				}
#endif

				sComMessage message;
				message.m_Header = kMessageHeader;
				message.m_PayloadSize = kMessagePayloadSize;
				message.m_MessageTag = kMessageBroadcastReplyTag;

				u32 nameLen = kMessagePayloadSize;
				GetComputerName(message.m_data, (LPDWORD)&nameLen);

				codex->Encrypt(&message.m_MessageTag, kMessagePayloadSize + 4);

				netSocketError error;
				listener.Send(from, &message, sizeof(message), &error);
			}
			break;

		case kMessageDeviceConnectTag:
			{
				CompanionDisplayf("Received device connect msg, from: %s",from.ToString());

#if LIMIT_DEVICE_PAIRING_TO_ROCKSTARID
				char rockstarIdString[20];
				sprintf_s(rockstarIdString, 20, "%llu", companion->GetRockstarId());
				u32 rIdHash = atStringHash(rockstarIdString,0);

				u32 remoteUserIdHash = *msgPtr;
#if !__FINAL
				const bool requireSClubId = PARAM_CompanionNoSCNeeded.Get() == false;
				if (requireSClubId == true && rIdHash != remoteUserIdHash)
#else
				if (rIdHash != remoteUserIdHash)
#endif // !__FINAL
				{
					return;
				}
#endif

				sComMessage message;
				message.m_Header = kMessageHeader;
				message.m_PayloadSize = 4;

				if (devices.GetCount() == 0)
				{
					companion->GetHttpServer().startListening(from.GetIp());
					companion->OnClientConnected();

					devices.PushAndGrow(CPairedDevice(from));
					message.m_MessageTag = kMessageGameConnectionAccepted;
				}
				else
				{
					message.m_MessageTag = kMessageGameConnectionBusy;
				}

				netSocketError error;
				listener.Send(from, &message, 12, &error);
			}
			break;

		default:
			CompanionDisplayf("Unrecognized tag was sent over the pairing protocol = %d",msgTag);
			break;
		}
	}
}

CPairedDevice* CPairingManager::getDeviceWithAddress(const netSocketAddress& address)
{
	atArray<CPairedDevice>::iterator it = devices.begin();
	while(it != devices.end())
	{
		CPairedDevice* device = &(*it);
		if (device->deviceAddress == address)
			return device;
		++it;
	}

	return NULL;
}






//--------------------------------------------------------------------------------------
// CCompanionData class
//--------------------------------------------------------------------------------------
CCompanionData* CCompanionData::GetInstance()
{
	if (!sm_instance)
	{
		sm_instance = rage_new CCompanionDataPC();
	}

	return sm_instance;
}
//--------------------------------------------------------------------------------------
// CCompanionDataPC class
//--------------------------------------------------------------------------------------
CCompanionDataPC::CCompanionDataPC() : CCompanionData(), m_httpServer(kCOMPANION_HTTP_PROTOCOL_PORT), m_rockstarId(0)
{
}

CCompanionDataPC::~CCompanionDataPC()
{
	//	Call down to base class
	CCompanionData::~CCompanionData();
}

void CCompanionDataPC::Initialise()
{
	// start our pairing manager
	m_pairingMgr.init(this);

	m_SCMgr = &CLiveManager::GetSocialClubMgr();
}

void CCompanionDataPC::Update()
{
	//	Update the base class first
	CCompanionData::Update();

	if (m_rockstarId == 0 && m_SCMgr->IsConnectedToSocialClub())
	{
		const rlScAccountInfo* accountInfo = m_SCMgr->GetLocalGamerAccountInfo();

		m_rockstarId = accountInfo->m_RockstarId;
	}

	m_pairingMgr.update();
}

void CCompanionDataPC::OnClientConnected()
{
	CompanionDisplayf("CCompanionDataPC::OnClientConnected");
	//	Call down to base class
	CCompanionData::OnClientConnected();
}

void CCompanionDataPC::OnClientDisconnected()
{
	CompanionDisplayf("CCompanionDataPC::OnClientDisconnected");
	//	Call down to base class
	CCompanionData::OnClientDisconnected();
}

RockstarId	CCompanionDataPC::GetRockstarId()
{
	return m_rockstarId;
}

CHttpServer& CCompanionDataPC::GetHttpServer()
{
	return m_httpServer;
}



#endif	//	COMPANION_APP

#endif	//	RSG_PC