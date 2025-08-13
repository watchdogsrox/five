/////////////////////////////////////////////////////////////////////////////////
// FILE :		PedPhoneComponent.cpp
// PURPOSE :	A class to hold the mobile phone model/position data for a ped/player
//				Pulled out of CPed and CPedPlayer classes.
// AUTHOR :		Mike Currington.
// CREATED :	28/10/10
/////////////////////////////////////////////////////////////////////////////////
#include "Peds/PedPhoneComponent.h"

// rage includes
#include "fwmaths/random.h"
#include "fwsys/timer.h"

// game includes
#include "event/ShockingEvents.h"
#include "game/ModelIndices.h"

using namespace rage;

AI_OPTIMISATIONS()

// default phone offsets
Vector3 CPedPhoneComponent::PED_PHONE_ATTACH_POS_OFFSET = Vector3(0.115f, 0.003f, 0.045f);
Vector3 CPedPhoneComponent::PED_PHONE_ATTACH_ROT_OFFSET = Vector3(-0.125f, -0.513f, 3.083f);



CPedPhoneComponent::CPedPhoneComponent() :
	m_vPhoneAttachPosOffset(PED_PHONE_ATTACH_POS_OFFSET),
	m_vPhoneAttachRotOffset(PED_PHONE_ATTACH_ROT_OFFSET),
	m_nPhoneModelIndex(MI_CELLPHONE),
	m_nPhoneModelHash(MI_CELLPHONE.GetName().GetHash()),
	m_MobileRingTimer(0),
	m_MobileRingState(0xff),
	m_MobileRingType(0)
{
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	StartMobileRinging
// PURPOSE :	Start the peds phone ringing with the given ringtone
/////////////////////////////////////////////////////////////////////////////////
void CPedPhoneComponent::StartMobileRinging(const u32 ringtoneId)
{
	Assign(m_MobileRingState, ringtoneId);
	m_MobileRingTimer = fwTimer::GetTimeInMilliseconds();
}





/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetPhoneModelIndexSafe
// PURPOSE :	Get the ped's phone model index, if pPhone is NULL then returns a
//				valid default model
/////////////////////////////////////////////////////////////////////////////////
u32	CPedPhoneComponent::GetPhoneModelIndexSafe	(const CPedPhoneComponent * pPhone)
{
	if (pPhone)
		return pPhone->GetPhoneModelIndex();
	else
		return MI_CELLPHONE;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetPhoneModelHashSafe
// PURPOSE :	Get the ped's phone model hash, if pPhone is NULL then returns a
//				valid default model
/////////////////////////////////////////////////////////////////////////////////
u32	CPedPhoneComponent::GetPhoneModelHashSafe(const CPedPhoneComponent * pPhone)
{
	if (pPhone)
		return pPhone->GetPhoneModelHash();
	else
		return MI_CELLPHONE.GetName().GetHash();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetPhoneAttachPosSafe
// PURPOSE :	Get the ped's phone attach offset position, if pPhone is NULL then returns a
//				valid default offset
/////////////////////////////////////////////////////////////////////////////////
const rage::Vector3&	CPedPhoneComponent::GetPhoneAttachPosSafe(const CPedPhoneComponent * pPhone)
{
	if (pPhone)
		return pPhone->GetPhoneAttachPosOffset();
	else
		return PED_PHONE_ATTACH_POS_OFFSET;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	GetPhoneAttachRotOffsetSafe
// PURPOSE :	Get the ped's phone attach offset rotation, if pPhone is NULL then returns a
//				valid default offset
/////////////////////////////////////////////////////////////////////////////////
const rage::Vector3&	CPedPhoneComponent::GetPhoneAttachRotOffsetSafe(const CPedPhoneComponent * pPhone)
{
	if (pPhone)
		return pPhone->GetPhoneAttachRotOffset();
	else
		return PED_PHONE_ATTACH_ROT_OFFSET;
}

