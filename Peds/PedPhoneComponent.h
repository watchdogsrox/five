/////////////////////////////////////////////////////////////////////////////////
// FILE :		PedPhoneComponent.h
// PURPOSE :	A class to hold the mobile phone model/position data for a ped/player
//				Pulled out of CPed and CPedPlayer classes.
// AUTHOR :		Mike Currington.
// CREATED :	02/11/10
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_PEDPHONECOMPONENT_H
#define INC_PEDPHONECOMPONENT_H

// rage includes
#include "vector/vector3.h"

// game includes

class CPedPhoneComponent
{
public:
	CPedPhoneComponent();

	static const u32 DEFAULT_RING_STATE = 0xff;
	// get the ped's phone model index
	s32			GetPhoneModelIndex		() const							{ return m_nPhoneModelIndex; }
	u32			GetPhoneModelHash		() const							{ return m_nPhoneModelHash; }

	// offsets for the phone attachment
	const rage::Vector3&	GetPhoneAttachPosOffset	() const				{ return m_vPhoneAttachPosOffset; }
	const rage::Vector3&	GetPhoneAttachRotOffset	() const				{ return m_vPhoneAttachRotOffset; }

	// set the ped's phone model index
	void		SetPhoneModelIndex(s32 nPhoneModelIndex, u32 nPhoneModelHash) { m_nPhoneModelIndex = nPhoneModelIndex; m_nPhoneModelHash = nPhoneModelHash; }

	// set the offsets for the phone attachment
	void		SetPhoneAttachPosOffset(const rage::Vector3& vPosOffset)	{ m_vPhoneAttachPosOffset = vPosOffset; }
	void		SetPhoneAttachRotOffset(const rage::Vector3& vRotOffset)	{ m_vPhoneAttachRotOffset = vRotOffset; }

	// Phone Ringing Functions
	u32			GetMobileRingState() const									{ return m_MobileRingState;	}
	u32			GetMobileRingTimer() const									{ return m_MobileRingTimer;	}
	void		SetMobileRingType(const u32 ringType)						{ Assign(m_MobileRingType, ringType); }
	u32			GetMobileRingType() const									{ return m_MobileRingType; }
	void		StartMobileRinging(const u32 ringtoneId);
	void		StopMobileRinging()											{ m_MobileRingState = DEFAULT_RING_STATE;	}


	//
	// Static ('safe') versions of the accessors will take a NULL component pointer and give the default phone model and position
	//

	// get the ped's phone model index
	static u32	GetPhoneModelIndexSafe	(const CPedPhoneComponent * pPhone);
	static u32	GetPhoneModelHashSafe	(const CPedPhoneComponent * pPhone);

	// offsets for the phone attachment
	static const rage::Vector3&	GetPhoneAttachPosSafe(const CPedPhoneComponent * pPhone);
	static const rage::Vector3&	GetPhoneAttachRotOffsetSafe(const CPedPhoneComponent * pPhone);

#if 0 // CS
	bool HasPhoneEquipped() const { return (GetWeaponMgr()->GetWeaponObject() && GetWeaponMgr()->GetWeaponObject()->GetModelIndex() == GetPhoneModelIndex()); }
#endif // 0

protected:
	rage::Vector3	m_vPhoneAttachPosOffset;
	rage::Vector3	m_vPhoneAttachRotOffset;
	s32				m_nPhoneModelIndex;
	s32				m_nPhoneModelHash;			// could probably figure this from the index, but is sitting in pad bytes in the class so free!

	// Phone variables
	u32				m_MobileRingTimer;
	u8				m_MobileRingState;
	u8				m_MobileRingType;

	// defaults
	static rage::Vector3 PED_PHONE_ATTACH_POS_OFFSET;
	static rage::Vector3 PED_PHONE_ATTACH_ROT_OFFSET;
};


#endif // INC_PEDPHONECOMPONENT_H
