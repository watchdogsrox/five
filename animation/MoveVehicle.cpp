#include "MoveVehicle.h"

//rage includes

// GTA includes
#include "Vehicles/VehicleDefines.h"


ANIM_OPTIMISATIONS()

fwMvFloatId CMoveVehicle::ms_MechanismPhase("Mechanism_Phase_In",0x6AD606DB);
fwMvFloatId CMoveVehicle::ms_MechanismRate("Mechanism_Rate",0x70B1CA8F);
fwMvFloatId CMoveVehicle::ms_MechanismPhaseOut("Mechanism_Phase_Out",0xE02034B9);
fwMvClipId CMoveVehicle::ms_MechanismClip("Mechanism_Clip",0xE307D2B6);

CMoveVehicle::CMoveVehicle(CDynamicEntity& dynamicEntity)
: CMoveObject(dynamicEntity)
, m_MechanismFlags(0)
, m_MechanismRate(0.0f)
, m_pMechanismClip(NULL)
{
	
}

//////////////////////////////////////////////////////////////////////////

CMoveVehicle::~CMoveVehicle()
{
	if (m_pMechanismClip)
	{
		m_pMechanismClip.Release();
		m_pMechanismClip = NULL;
	}

	CMoveObject::Shutdown();
}

//////////////////////////////////////////////////////////////////////////

void CMoveVehicle::StartMechanism(const crClip* pClip, float blendDuration, float startPhase, float startRate)
{
	if (pClip==NULL)
	{
		StopMechanism();
		return;
	}

	// trigger the transition
	BroadcastRequest(ms_SecondaryTransitionId);
	SetFloat(ms_SecondaryDurationId, blendDuration);

	SetFloat(ms_MechanismPhase, startPhase);
	SetFloat(ms_MechanismRate, startRate);

	SetFlag(ms_SecondaryOffId, false);
	SetFlag(ms_SecondaryOnId, false);
	SetFlag(ms_SecondaryMechanismId, true);

	if (m_pMechanismClip)
	{
		m_pMechanismClip.Release();
		m_pMechanismClip = NULL;
	}

	m_pMechanismClip = pClip;
	pClip->AddRef();
	SetClip(ms_MechanismClip, pClip);

	m_MechanismFlags.SetFlag(Mechanism_Active);

	//set the state flag based on the phase
	if (startPhase==0.0f)
	{
		m_MechanismFlags.SetFlag(Mechanism_Closed);
	}
	else if (startPhase==1.0f)
	{
		m_MechanismFlags.SetFlag(Mechanism_Open);
	}

	if (startRate==0.0f)
	{
		m_MechanismFlags.SetFlag(Mechanism_Paused);
	}
	else if (startRate>0.0f)
	{
		m_MechanismFlags.SetFlag(Mechanism_Opening);
	}
	else
	{
		m_MechanismFlags.SetFlag(Mechanism_Closing);
	}

	m_MechanismRate = startRate;

	m_SecondaryNetwork.SetNetworkPlayer(NULL);
}

//////////////////////////////////////////////////////////////////////////

void CMoveVehicle::OpenMechanism(float rate)
{
	if (!m_MechanismFlags.IsFlagSet(Mechanism_Open))
	{
		m_MechanismFlags.SetFlag(Mechanism_Opening);
		m_MechanismFlags.ClearFlag(Mechanism_Closed);
		m_MechanismFlags.ClearFlag(Mechanism_Open);
		m_MechanismFlags.ClearFlag(Mechanism_Paused);
		m_MechanismFlags.ClearFlag(Mechanism_Closing);
	}

	m_MechanismRate = abs(rate);
	SetFloat(ms_MechanismRate, m_MechanismRate);
}

//////////////////////////////////////////////////////////////////////////

void CMoveVehicle::CloseMechanism(float rate)
{
	if (!m_MechanismFlags.IsFlagSet(Mechanism_Closed))
	{
		m_MechanismFlags.ClearFlag(Mechanism_Opening);
		m_MechanismFlags.ClearFlag(Mechanism_Closed);
		m_MechanismFlags.ClearFlag(Mechanism_Open);
		m_MechanismFlags.ClearFlag(Mechanism_Paused);
		m_MechanismFlags.ClearFlag(Mechanism_Opening);
		m_MechanismFlags.SetFlag(Mechanism_Closing);
	}

	m_MechanismRate = -abs(rate);
	SetFloat(ms_MechanismRate, m_MechanismRate);
}

//////////////////////////////////////////////////////////////////////////

void CMoveVehicle::PauseMechanism()
{
	if (!m_MechanismFlags.IsFlagSet(Mechanism_Paused))
	{
		m_MechanismFlags.ClearFlag(Mechanism_Opening);
		m_MechanismFlags.ClearFlag(Mechanism_Closed);
		m_MechanismFlags.ClearFlag(Mechanism_Open);
		m_MechanismFlags.SetFlag(Mechanism_Paused);
		m_MechanismFlags.ClearFlag(Mechanism_Opening);
		m_MechanismFlags.ClearFlag(Mechanism_Closing);

		SetFloat(ms_MechanismRate, 0.0f);
	}
}

//////////////////////////////////////////////////////////////////////////

void CMoveVehicle::StopMechanism()
{
	if (m_pMechanismClip)
	{
		m_pMechanismClip.Release();
		m_pMechanismClip = NULL;
	}

	m_MechanismFlags.ClearAllFlags();

	m_MechanismRate = 0.0f;

	SetSecondaryNetwork(NULL, 0.0f);
}

//////////////////////////////////////////////////////////////////////////

bool CMoveVehicle::IsMechanismFullyOpen()
{
	float outPhase = GetFloat(ms_MechanismPhaseOut);
	if (outPhase>-1.0f)
	{
		if (outPhase==1.0f)
		{
			m_MechanismFlags.SetFlag(Mechanism_Open);
		}
		if (outPhase>0.0f)
		{
			m_MechanismFlags.ClearFlag(Mechanism_Closed);
		}
	}

	return m_MechanismFlags.IsFlagSet(Mechanism_Open);
}

//////////////////////////////////////////////////////////////////////////

bool CMoveVehicle::IsMechanismFullyClosed()
{
	float outPhase = GetFloat(ms_MechanismPhaseOut);
	if (outPhase>-1.0f)
	{
		if (outPhase==0.0f)
		{
			m_MechanismFlags.SetFlag(Mechanism_Closed);
		}
		if (outPhase<1.0f)
		{
			m_MechanismFlags.ClearFlag(Mechanism_Open);
		}
	}

	return m_MechanismFlags.IsFlagSet(Mechanism_Closed);
}

//////////////////////////////////////////////////////////////////////////

bool CMoveVehicle::IsMechanismFullyClosedAndNotOpening()
{
    if(IsMechanismFullyClosed())
    {
        if(!m_MechanismFlags.IsFlagSet(Mechanism_Opening))
        {
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////

float CMoveVehicle::GetMechanismRate()
{
	return m_MechanismRate;
}

//////////////////////////////////////////////////////////////////////////

void CMoveVehicle::SetMechanismPhase(float phase)
{
	SetFloat(ms_MechanismPhase, phase);

	if ( phase== 1.0f)
	{
		m_MechanismFlags.SetFlag(Mechanism_Open);
	}
	else if (phase==0.0f)
	{
		m_MechanismFlags.SetFlag(Mechanism_Closed);
	}
}


//////////////////////////////////////////////////////////////////////////

float CMoveVehicle::GetMechanismPhase()
{
	return GetFloat(ms_MechanismPhaseOut);
}


//////////////////////////////////////////////////////////////////////////

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CMoveVehiclePooledObject, VEHICLE_POOL_SIZE, 0.22f, atHashString("CMoveVehicle",0xac9f5436) );