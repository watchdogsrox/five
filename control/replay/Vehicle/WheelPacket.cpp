
#include "WheelPacket.h"

#if GTA_REPLAY

#include "Control/replay/ReplayExtensions.h"

u32 CPacketWheel::ms_RecordSession = 1;

//========================================================================================================================
// CPacketWheel.
//========================================================================================================================
tPacketVersion CPacketWheel_V1 = 1;
void CPacketWheel::Store(const CVehicle *pVehicle, u16 sessionBlockIndex)
{
	CPacketBase::Store(PACKETID_WHEEL, sizeof(CPacketWheel), CPacketWheel_V1);
	WheelValueExtensionData *pWheelExtensionData = GetExtensionDataForRecording((CVehicle *)pVehicle, false, sessionBlockIndex);

	m_NoOfWheels = 0;
	m_NoOf4ByteElements = 0;
	m_sessionBlockIndex = sessionBlockIndex;

	m_deadTyres = 0;

	if(pWheelExtensionData)
	{
		// The WheelValueExtensionData is updated when we record the create packet for vehicles. See CPacketVehicleCreate::Store() or CPacketVehicleCreate::StoreExtensions()).
		m_NoOfWheels = (u16)pWheelExtensionData->m_NoOfWheels;

		u32 *pDest = GetWheelData();
		m_NoOf4ByteElements = (u16)pWheelExtensionData->StoreDifferencesFromReferenceSetInPackedStream(sessionBlockIndex, pDest);
		AddToPacketSize(m_NoOf4ByteElements*sizeof(int));
	}
	
	// url:bugstar:4259421 - sometimes we're getting the wrong tyre health value on playback.  Rather than unpick the 
	// wheel value packing (was written years ago by someone else) just record a bit for each tyre that should be
	// blown.
	for(int i = 0; i < m_NoOfWheels; ++i)
	{
		CWheel const* pWheel = pVehicle->GetWheel(i);
		if(pWheel && pWheel->GetTyreHealth() == 0.0f)
			m_deadTyres |= 1 << i;
	}
}


void CPacketWheel::Extract(ReplayController& controller, CVehicle *pVehicle, CPacketWheel *pNext, float interp)
{
	if(m_NoOf4ByteElements == 0)
		return;

	WheelValueExtensionData *pWheelExtensionData = CPacketWheel::GetExtensionDataForPlayback(pVehicle);

	if(pWheelExtensionData)
	{
		ReplayWheelValues prevWheel[NUM_VEH_CWHEELS_MAX];
		ReplayWheelValues nextWheel[NUM_VEH_CWHEELS_MAX];
		pWheelExtensionData->FormWheelValuesFromPackedStreamAndReferenceSet(m_sessionBlockIndex, m_NoOfWheels, GetWheelData(), &prevWheel[0]);
		
		if(pNext)
			pWheelExtensionData->FormWheelValuesFromPackedStreamAndReferenceSet(pNext->m_sessionBlockIndex, pNext->m_NoOfWheels, pNext->GetWheelData(), &nextWheel[0]);

		for(u32 i=0; i<m_NoOfWheels; i++)
		{
			CWheel *pWheel = pVehicle->GetWheel(i);

			if(pWheel)
			{
				prevWheel[i].Extract(controller, pVehicle, pWheel, pNext ? &nextWheel[i] : NULL, interp);

				if(GetPacketVersion() >= CPacketWheel_V1 && (m_deadTyres & (1 << i)) != 0)
				{
					pWheel->SetTyreHealth(0.0f);
				}
			}
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketWheel::OnRecordStart()
{
	ms_RecordSession++;
}


//////////////////////////////////////////////////////////////////////////
WheelValueExtensionData *CPacketWheel::GetExtensionDataForRecording(CVehicle *pVehicle, bool isForDeletePacket, u16 sessionBlockIndex)
{
	bool wasAdded;
	WheelValueExtensionData *pWheelExtensionData = SetUpExtensions(pVehicle, isForDeletePacket, wasAdded);

	if(pWheelExtensionData)
	{
		// Are we to initialize the wheel values ?
		if((pWheelExtensionData->m_RecordSession != ms_RecordSession) ||(wasAdded == true))
		{
			pWheelExtensionData->InitializeForRecording(pVehicle, sessionBlockIndex);
			pWheelExtensionData->m_RecordSession = ms_RecordSession;
		}
	}
	return pWheelExtensionData;
}


WheelValueExtensionData *CPacketWheel::GetExtensionDataForPlayback(CVehicle *pVehicle)
{
	bool unused;
	WheelValueExtensionData *pWheelExtensionData = SetUpExtensions(pVehicle, false, unused);
	return pWheelExtensionData;
}


WheelValueExtensionData *CPacketWheel::SetUpExtensions(CVehicle *pVehicle, bool isForDeletePacket, bool &wasAdded)
{
	wasAdded = false;
	WheelValueExtensionData *pWheelExtensionData = NULL;

	// Add the extension if needs be.
	if(pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE)
	{
		ReplayBicycleExtension *extension = ReplayBicycleExtension::GetExtension(pVehicle);

		// Don't bother adding extensions if request is for a delete packet (we expect to have them anyway).
		if(!extension && !isForDeletePacket)
		{
			extension = ReplayBicycleExtension::Add(pVehicle);
			wasAdded = true;
		}

		if(extension)
			pWheelExtensionData = &extension->m_WheelData;
	}
	else
	{
		ReplayVehicleExtension *extension = ReplayVehicleExtension::GetExtension(pVehicle);

		// Don't bother adding extensions if request if for a delete packet (we expect t have them anyway).
		if(!extension && !isForDeletePacket)
		{
			extension = ReplayVehicleExtension::Add(pVehicle);
			extension->Reset();
			wasAdded = true;
		}

		if(extension)
			pWheelExtensionData = &extension->m_WheelData;
	}
	return pWheelExtensionData;
}

#endif // GTA_REPLAY
