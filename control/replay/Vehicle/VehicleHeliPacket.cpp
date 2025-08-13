//
// name:		VehicleHeliPacket.h
//

#include "VehicleHeliPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/Misc/ReplayPacket.h"
//#include "maths/angle.h"
#include "vehicles/Heli.h"
#include "audio/heliaudioentity.h"
#include "fwpheffects/ropemanager.h"

//========================================================================================================================
tPacketVersion CPacketHeliUpdate_V1 = 1;
tPacketVersion CPacketHeliUpdate_V2 = 2;
tPacketVersion CPacketHeliUpdate_V3 = 3;
void CPacketHeliUpdate::Store(CHeli *pHeli, bool storeCreateInfo)
{
	PACKET_EXTENSION_RESET(CPacketHeliUpdate);
	replayAssert( pHeli->GetVehicleType() == VEHICLE_TYPE_HELI);
	//TODO4FIVEreplayAssert( /*pHeli->GetVehicleAudioEntity()->m_HeliState >= 0 &&*/ pHeli->GetVehicleAudioEntity()->m_HeliState < 4 );

	// Conversion Factors for Compression
	//TODO4FIVEconst float FLOAT_TO_U8_RANGE_0_TO_1		= 255.0f;		// Map float (range 0 to 1) to u8

	// Clamp Values
	//TODO4FIVE float minigunSpinSpeed = rage::Clamp( pHeli->m_MinigunSpinSpeed, 0.0f, 1.0f );
	m_VehiclePacketUpdateSize = sizeof(CPacketHeliUpdate);

	CPacketVehicleUpdate::Store(PACKETID_HELIUPDATE, sizeof(CPacketHeliUpdate), pHeli, storeCreateInfo, CPacketHeliUpdate_V3);

	m_WheelAngularVelocity	= pHeli->GetMainRotorSpeed();
	m_ThrottleControl		= pHeli->m_fThrottleControl;
	m_MainRotorSpeed		= pHeli->m_fMainRotorSpeed;
	m_Health				= pHeli->GetHealth();
	//TODO4FIVE m_fMainRotorAngle		= pHeli->m_fMainRotorAngle;
	//TODO4FIVE m_IsAltitudeWarningOn	= pHeli->GetVehicleAudioEntity()->m_IsHeliAltitudeWarningOn;
	m_MainRotorHealth		= GetHealthState( pHeli->m_fMainRotorHealth	);
	m_RearRotorHealth		= GetHealthState( pHeli->m_fRearRotorHealth	);
	m_TailBoomHealth		= GetHealthState( pHeli->m_fTailBoomHealth	);
	//TODO4FIVE m_HeliState				= (u8) (pHeli->GetVehicleAudioEntity()->m_HeliState);
	//TODO4FIVE m_MinigunSpinSpeed		= (u8) (minigunSpinSpeed * FLOAT_TO_U8_RANGE_0_TO_1);*/

	m_LightOn = false;
	m_LightForceOn = false;
	m_lightTarget = NoEntityID;
	if(pHeli->GetSearchLight())
	{
		m_LightOn = pHeli->GetSearchLight()->GetLightOn();
		m_LightForceOn = pHeli->GetSearchLight()->GetForceLightOn();
		pHeli->GetSearchLight()->GetSearchLightTargetReplayId();

		m_LightHasPriority = pHeli->GetSearchLight()->GetHasPriority();
	}

	m_HeliAudioState = static_cast<audHeliAudioEntity*>(pHeli->GetAudioEntity())->GetHeliAudioState();

	m_jetpackThrusterThrottle = pHeli->GetJetPackThrusterThrotle();
	m_jetpackStrafeForceScale = pHeli->GetJetpackStrafeForceScale();

	m_fRopeLengthA = 0.0f;
	m_fRopeLengthB = 0.0f;
	m_fRopeLengthRateA = 0.0f;
	m_fRopeLengthRateB = 0.0f;
	ropeInstance* pRopeA = pHeli->GetRope(false);
	if(pRopeA)
	{
		m_fRopeLengthA = pRopeA->GetLength();
		m_fRopeLengthRateA = pRopeA->GetLengthChangeRate();
	}

	ropeInstance* pRopeB = pHeli->GetRope(true);
	if(pRopeB)
	{
		m_fRopeLengthB = pRopeB->GetLength();
		m_fRopeLengthRateB = pRopeB->GetLengthChangeRate();
	}

	u32* pData = (u32*)BEGIN_PACKET_EXTENSION_WRITE(1, CPacketHeliUpdate);

	*pData = CHeli::eNumRopeIds; ++pData;
	u32* pRopeMask = pData; ++pData;
	float* pRopeValue = (float*)pData;

	*pRopeMask = 0;
	for(int i = 0; i < CHeli::eNumRopeIds; ++i)
	{
		ropeInstance* pRope = pHeli->GetRope((CHeli::eRopeID)i);
		if(pRope)
		{
			*pRopeMask |= 1 << i;

			*pRopeValue = pRope->GetLength(); ++pRopeValue;
			*pRopeValue = pRope->GetLengthChangeRate(); ++pRopeValue;
		}
	}

	pData = (u32*)pRopeValue;

	END_PACKET_EXTENSION_WRITE(pData, CPacketHeliUpdate);
}


void CPacketHeliUpdate::StoreExtensions(CHeli *pHeli, bool storeCreateInfo) 
{ 
	replayAssert( pHeli->GetVehicleType() == VEHICLE_TYPE_HELI);
	CPacketVehicleUpdate::StoreExtensions(PACKETID_HELIUPDATE, sizeof(CPacketHeliUpdate), pHeli, storeCreateInfo);
}

//========================================================================================================================
void CPacketHeliUpdate::Extract(const CInterpEventInfo<CPacketHeliUpdate, CHeli>& info) const
{
	CHeli* pHeli = info.GetEntity();
	replayAssert(pHeli->GetVehicleType() == VEHICLE_TYPE_HELI);
	CPacketVehicleUpdate::Extract(info);

	// Conversion Factors for Compression/Decompression
	//TODO4FIVE const float U8_TO_FLOAT_RANGE_0_TO_1		= 1 / 255.0f;

	// Extract Values That Can Be Interpolated
	//TODO4FIVE if (pHeli->IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD) == TRUE || fInterp == 0.0f || IsNextDeleted())
	//{
	//	pHeli->m_fEngineSpeed = m_WheelAngularVelocity;
	//	pHeli->m_fThrottleControl = m_ThrottleControl;
	//	pHeli->SetHealth(m_Health);
	//	pHeli->m_fMainRotorAngle = m_fMainRotorAngle;
	//	pHeli->m_fRearRotorAngle = pHeli->m_fMainRotorAngle;
	//}
	//else if (HasNextOffset() && pNextPacket)
	//{
	//	ExtractInterpolateableValues(pHeli, pNextPacket, fInterp);
	//}
	//else
	//{
	//	replayAssertf(fa,se, "CPacketHeliUpdate::Extract");
	//}

	//TODO4FIVE pHeli->GetVehicleAudioEntity()->m_HeliState					= m_HeliState;
	//TODO4FIVE pHeli->GetVehicleAudioEntity()->m_IsHeliAltitudeWarningOn	= m_IsAltitudeWarningOn;
	
	pHeli->m_fMainRotorHealth	= GetHealthFromState( m_MainRotorHealth	);
	pHeli->m_fRearRotorHealth	= GetHealthFromState( m_RearRotorHealth	);
	pHeli->m_fTailBoomHealth	= GetHealthFromState( m_TailBoomHealth	);
	//TODO4FIVE pHeli->m_MinigunSpinSpeed	= m_MinigunSpinSpeed * U8_TO_FLOAT_RANGE_0_TO_1;

	if(pHeli->GetSearchLight())
	{
		pHeli->GetSearchLight()->SetLightOn(m_LightOn);
		pHeli->GetSearchLight()->SetForceLightOn(m_LightForceOn);
		pHeli->GetSearchLight()->SetHasPriority(m_LightHasPriority);

		if( m_lightTarget > NoEntityID )
		{
			CEntity * pEntity = CReplayMgr::GetEntity(m_lightTarget);
			if(pEntity)
			{
				pHeli->GetSearchLight()->SetSearchLightTarget((CPhysical*)pEntity);
			}
		}
	}

	pHeli->SetThrottleControl(m_ThrottleControl);
	pHeli->SetMainRotorSpeed(m_MainRotorSpeed);

	if(GetPacketVersion() >= CPacketHeliUpdate_V3)
	{
		u32 const* pData = (u32 const*)GET_PACKET_EXTENSION_READ_PTR_THIS(CPacketHeliUpdate);

		/*u32 ropeCount = *pData;*/ ++pData; // Skip the count
		u32 ropeMask = *pData; ++pData;

		float const* ropeValue = (float const*)pData;
		
		for(int i = 0; i < CHeli::eNumRopeIds; ++i)
		{
			if(ropeMask & (1 << i))
			{
				float ropeLength = *ropeValue; ++ropeValue;
				float ropeLengthRate = *ropeValue; ++ropeValue;

				ropeInstance* pRope = pHeli->GetRope((CHeli::eRopeID)i);
				if(pRope && ropeLength != 0.0f)
				{
					if(pRope->GetLength() != ropeLength)
					{
						pRope->StartUnwindingFront(ropeLengthRate);
					}
					else
					{
						pRope->StopUnwindingFront();
					}
				}
			}
		}
	}
	else
	{
		ropeInstance* pRopeA = pHeli->GetRope(false);
		if(m_fRopeLengthA != 0.0f && pRopeA)
		{
			if(pRopeA->GetLength() != m_fRopeLengthA)
			{
				pRopeA->StartUnwindingFront(m_fRopeLengthRateA);
			}
			else
			{
				pRopeA->StopUnwindingFront();
			}
		}

		ropeInstance* pRopeB = pHeli->GetRope(true);
		if(m_fRopeLengthB != 0.0f && pRopeB)
		{
			if(pRopeB->GetLength() != m_fRopeLengthB)
			{
				pRopeB->StartUnwindingFront(m_fRopeLengthRateB);
			}
			else
			{
				pRopeB->StopUnwindingFront();
			}
		}
	}

	static_cast<audHeliAudioEntity*>(pHeli->GetAudioEntity())->ProcessReplayAudioState(m_HeliAudioState);	

	if(GetPacketVersion() >= CPacketHeliUpdate_V1)
	{
		pHeli->SetJetPackThrusterThrotle(m_jetpackThrusterThrottle);
	}
	if(GetPacketVersion() >= CPacketHeliUpdate_V2)
	{
		pHeli->SetJetpackStrafeForceScale(m_jetpackStrafeForceScale);
	}
}

//=====================================================================================================
void CPacketHeliUpdate::ExtractInterpolateableValues(CHeli* pHeli, CPacketHeliUpdate const* pNextPacket, float fInterp) const
{
	//todo4five temp as these are causing issues in the final builds
	(void) pHeli; (void) pNextPacket; (void) fInterp;

	/*TODO4FIVE pHeli->m_fEngineSpeed = Lerp(fInterp, m_WheelAngularVelocity, pNextPacket->GetAngularVelocity());
	pHeli->m_fThrottleControl = Lerp(fInterp, m_ThrottleControl, pNextPacket->GetThrottleControl());
	pHeli->SetHealth(Lerp(fInterp, m_Health, pNextPacket->GetHealth()));

	if (abs(m_fMainRotorAngle - pNextPacket->GetRotorAngle()) > PI)
	{
		pHeli->m_fMainRotorAngle = Lerp(fInterp, m_fMainRotorAngle, pNextPacket->GetRotorAngle()-TWO_PI);
		if (pHeli->m_fMainRotorAngle < -TWO_PI)
		{
			pHeli->m_fMainRotorAngle += TWO_PI;
		}
		pHeli->m_fRearRotorAngle = pHeli->m_fMainRotorAngle;
	}
	else
	{
		pHeli->m_fMainRotorAngle = Lerp(fInterp, m_fMainRotorAngle, pNextPacket->GetRotorAngle());
		pHeli->m_fRearRotorAngle = pHeli->m_fMainRotorAngle;
	}*/
}

//========================================================================================================================
u8 CPacketHeliUpdate::GetHealthState( f32 health ) const
{
	if( health <= 0 )
	{
		return HELI_HEALTH_BROKEN;
	}
	else if( health > 0 && health <= g_MinHealthForHeliWarning )
	{
		return HELI_HEALTH_LOW;
	}
	else
	{
		return HELI_HEALTH_NORMAL;
	}
}

//========================================================================================================================
f32 CPacketHeliUpdate::GetHealthFromState( u8 healthState ) const
{
	switch( healthState )
	{
	case HELI_HEALTH_BROKEN:
		return 0.0f;

	case HELI_HEALTH_LOW:
		return g_MinHealthForHeliWarning;

	case HELI_HEALTH_NORMAL:
		return VEH_DAMAGE_HEALTH_STD;

	default:
		replayAssertf( false, "CPacketHeliUpdate::GetHealthFromState: Invalid helicopter health state" );
		return -1.0f;
	}
}

void CPacketAutoGyroUpdate::Store(CAutogyro *pAutoGyro, bool storeCreateInfo)
{
	PACKET_EXTENSION_RESET(CPacketAutoGyroUpdate);
	CPacketVehicleUpdate::Store(PACKETID_AUTOGYROUPDATE, sizeof(CPacketAutoGyroUpdate), pAutoGyro, storeCreateInfo);
}


void CPacketAutoGyroUpdate::StoreExtensions(CAutogyro *pAutoGyro, bool storeCreateInfo) 
{ 
	PACKET_EXTENSION_RESET(CPacketAutoGyroUpdate);
	CPacketVehicleUpdate::StoreExtensions(PACKETID_AUTOGYROUPDATE, sizeof(CPacketAutoGyroUpdate), pAutoGyro, storeCreateInfo);
}


void CPacketAutoGyroUpdate::Extract(const CInterpEventInfo<CPacketAutoGyroUpdate, CAutogyro>& info) const
{
	CPacketVehicleUpdate::Extract(info);
}

//========================================================================================================================
void CPacketBlimpUpdate::Store(CBlimp *pBlimp, bool storeCreateInfo)
{
	PACKET_EXTENSION_RESET(CPacketBlimpUpdate);
	CPacketVehicleUpdate::Store(PACKETID_BLIMPUDPATE, sizeof(CPacketBlimpUpdate), pBlimp, storeCreateInfo);
}


void CPacketBlimpUpdate::StoreExtensions(CBlimp *pBlimp, bool storeCreateInfo) 
{ 
	PACKET_EXTENSION_RESET(CPacketBlimpUpdate);
	CPacketVehicleUpdate::StoreExtensions(PACKETID_BLIMPUDPATE, sizeof(CPacketBlimpUpdate), pBlimp, storeCreateInfo);
}


void CPacketBlimpUpdate::Extract(const CInterpEventInfo<CPacketBlimpUpdate, CBlimp>& info) const
{
	CPacketVehicleUpdate::Extract(info);
}

#endif // GTA_REPLAY
