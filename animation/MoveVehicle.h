// 
// animation/MoveVehicle.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#ifndef MOVE_VEHICLE_H
#define MOVE_VEHICLE_H

#include "MoveObject.h"

//////////////////////////////////////////////////////////////////////////
//	Vehicle main network class. Currently inherits all of its
//	functionality from CMoveObject, but we'll likely be adding
//	vehicle specific content here in future.
//////////////////////////////////////////////////////////////////////////

class CMoveVehicle : public CMoveObject
{
public:

	// PURPOSE: 
	CMoveVehicle(CDynamicEntity& dynamicEntity);

	// PURPOSE:
	virtual ~CMoveVehicle();

	//////////////////////////////////////////////////////////////////////////
	//	Mechanism interface - useful for vehicle convertible roof etc
	//////////////////////////////////////////////////////////////////////////

	void StartMechanism(const crClip* pClip, float blendDuration, float startPhase = 0.0f, float startRate = 1.0f);

	void OpenMechanism(float rate);
	void CloseMechanism(float rate);
	void PauseMechanism();

	void StopMechanism();

	bool IsMechanismActive() const { return m_MechanismFlags.IsFlagSet(Mechanism_Active); }

	bool IsMechanismFullyOpen();
	bool IsMechanismFullyClosed();
    bool IsMechanismFullyClosedAndNotOpening();
	
	float GetMechanismRate();
	void  SetMechanismPhase(float phase);
	float GetMechanismPhase();

protected:

	enum eMechanismFlags
	{
		Mechanism_Active = BIT0,
		Mechanism_Opening = BIT1,
		Mechanism_Open = BIT2,
		Mechanism_Closing = BIT3,
		Mechanism_Closed = BIT4,
		Mechanism_Paused = BIT5
	};

	// Stores mechanism state
	fwFlags32 m_MechanismFlags;

	// the rate the mechanism has been set to blend in and out at
	float m_MechanismRate;

	// the clip being played back by the mechanism
	pgRef<const crClip> m_pMechanismClip;

	static fwMvFloatId ms_MechanismPhase;
	static fwMvFloatId ms_MechanismRate;
	static fwMvFloatId ms_MechanismPhaseOut;
	static fwMvClipId ms_MechanismClip;

};

////////////////////////////////////////////////////////////////////////////////

class CMoveVehiclePooledObject : public CMoveVehicle
{
public:

	CMoveVehiclePooledObject(CDynamicEntity& dynamicEntity)
		: CMoveVehicle(dynamicEntity) 
	{
	}

	FW_REGISTER_CLASS_POOL(CMoveVehiclePooledObject);
};

#endif // MOVE_VEHICLE_H