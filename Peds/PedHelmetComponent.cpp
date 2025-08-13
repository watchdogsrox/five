/////////////////////////////////////////////////////////////////////////////////
// FILE :		PedHelmetComponent.cpp
// PURPOSE :	A class to hold the mobile phone model/position data for a ped/player
//				Pulled out of CPed and CPedPlayer classes.
// AUTHOR :		Mike Currington.
// CREATED :	28/10/10
/////////////////////////////////////////////////////////////////////////////////
#include "Peds/PedHelmetComponent.h"

// rage includes


// game includes
#include "Camera/CamInterface.h"
#include "Camera/Cinematic/CinematicDirector.h"
#include "Peds/Ped.h"
#include "peds/ped_channel.h"
#include "scene/ExtraMetadataMgr.h"
#include "vehicles/Metadata/VehicleSeatInfo.h"
#include "bank/bank.h"

using namespace rage;

AI_OPTIMISATIONS()

//
// Constants

const Vector3 HELMET_POS_OFFSET(0.0f,         0.125f, 0.0f);
const Vector3 HELMET_ROT_OFFSET(-1.57079637f, 0.0f,   0.0f);

// 
// Statics

atMap<u32, atArray<int> > CPedHelmetComponent::sm_VehicleHashToHelmetPropIndex;

#if __BANK
atHashString CPedHelmetComponent::sm_uVehicleHashString;
int CPedHelmetComponent::sm_iHelmetPropIndex = -1;
#endif	// __BANK


CPedHelmetComponent::CPedHelmetComponent(CPed * pParentPed) :
m_pParentPed(pParentPed),
m_fHelmetInactiveDeletionTimer(0.0f),
m_nHelmetVisorUpPropIndex(-1),
m_nHelmetVisorDownPropIndex(-1),
m_bIsVisorUp(false),
m_bSupportsVisors(false),
m_nHelmetPropIndex(-1),
m_nHelmetTexIndex(-1),
m_bUseOverridenHelmetPropIndexOnBiycles(true),
m_iMostRecentVehicleType(-1),
m_nStoredHatPropIdx(-1),
m_nStoredHatTexIdx(-1),
m_nPreLoadedTexture(-1),
m_DesiredPropFlag(PV_FLAG_NONE),
m_CurrentPropFlag(PV_FLAG_NONE),
m_NetworkHelmetIndex(-1),
m_NetworkTextureIndex(-1),
m_bPreLoadingProp(false),
m_bPendingHatRestore(false),
m_nHelmetPropIndexFromHashMap(-1)
{
}

void CPedHelmetComponent::Update(void)
{
	// we've been asked to restore a stored hat at a later point by TaskTakeOffHelmet that was aborted.
	// The helmet was a "delayed delete" resource so couldn't be deleted while TaskTakeOffHelmet was 
	// still running and there were no other slots to use (ped has watch, helmet and glasses on = 3 slots)
	if(m_pParentPed && m_bPendingHatRestore)
	{
		RestoreOldHat(true);

		if (GetStoredHatPropIndex() == -1 && GetStoredHatTexIndex() == -1)
		{
			m_bPendingHatRestore = false;
		}
	}

	// B*2214377: Cache last vehicle type. Used in CPedHelmetComponent::GetHelmetPropIndex so we know if we were last on a bicycle (needed for taking off the right helmet). 
	if (m_pParentPed && m_pParentPed->GetIsInVehicle())
	{
		CVehicle *pVehicle = m_pParentPed->GetVehiclePedInside();
		if (pVehicle)
		{
			m_iMostRecentVehicleType = (int)pVehicle->GetVehicleType();
		}
	}
}

void CPedHelmetComponent::UpdateInactive(const bool okToRemoveImmediately)
{
	m_fHelmetInactiveDeletionTimer += fwTimer::GetTimeStep();
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(CLIP_SET_TAKE_OFF_HELMET);
	if (pClipSet)
		pClipSet->Request_DEPRECATED();

	if(okToRemoveImmediately || m_fHelmetInactiveDeletionTimer > 5.0f)
	{
		pedDebugf1("CPedHelmetComponent[%s][%p] - UpdateInactive -  removing helmet", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed);
		// Delete the helmet if we are not on screen or have had it for more than a certain time
		DisableHelmet();
	}
}


bool CPedHelmetComponent::IsHelmetEnabled() const
{
	if (m_pParentPed->IsNetworkClone() && m_NetworkHelmetIndex == -1)
	{
		return false;
	}

	if(m_pParentPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HasHelmet ))
	{
		return true;
	}

	return false;
}

bool CPedHelmetComponent::HasPilotHelmetEquippedInAircraftInFPS(bool bTreatBonnetCameraAsFirstPerson) const
{
#if FPS_MODE_SUPPORTED
	const bool c_IsValidCamera = ( bTreatBonnetCameraAsFirstPerson && camInterface::GetCinematicDirector().IsRenderingCinematicMountedCamera()) ||
								 (!bTreatBonnetCameraAsFirstPerson && m_pParentPed->IsInFirstPersonVehicleCamera());

	const bool c_HasPilotHelmetOrIsMP = ((CPedPropsMgr::GetPedPropIdx(m_pParentPed, ANCHOR_HEAD) != -1 && CPedPropsMgr::DoesPedHavePropOrComponentWithFlagSet(*m_pParentPed, PV_FLAG_PILOT_HELMET)) || NetworkInterface::IsGameInProgress());
	
	if (c_IsValidCamera 
		&& !m_pParentPed->GetPedResetFlag(CPED_RESET_FLAG_PuttingOnHelmet) && !m_pParentPed->GetPedResetFlag(CPED_RESET_FLAG_DisableHelmetCullFPS)
		&& c_HasPilotHelmetOrIsMP
		&& m_pParentPed->GetIsInVehicle() && m_pParentPed->GetIsDrivingVehicle())
	{
		CVehicle *pVehicle = static_cast<CVehicle*>(m_pParentPed->GetVehiclePedInside());
		if (pVehicle && pVehicle->GetIsAircraft())
		{
			return true;
		}
	}
#endif
	return false;
}

bool CPedHelmetComponent::EnableHelmet(eAnchorPoints anchorOverrideID, bool bTakingOff)
{
	pedDebugf1("CPedHelmetComponent[%s][%p]  - EnableHelmet -  Enable helmet requested", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed);

	if(m_pParentPed->CanPutOnHelmet() || bTakingOff )
	{
		//Nothing has set the desired helmet type so fall back to default, i.e. motorbike helmet.
		if(m_DesiredPropFlag == PV_FLAG_NONE)
		{
			CVehicle* pVehicle = m_pParentPed->GetVehiclePedInside();
			if(pVehicle && pVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
			{
				m_DesiredPropFlag = PV_FLAG_RANDOM_HELMET;
			}
			else if (pVehicle && (pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI || pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE))
			{
				if (ShouldUsePilotFlightHelmet(m_pParentPed))
				{
					m_DesiredPropFlag = PV_FLAG_PILOT_HELMET;
				}
				else
				{
					m_DesiredPropFlag = PV_FLAG_FLIGHT_HELMET;
				}
			}
			else
			{
				m_DesiredPropFlag = PV_FLAG_DEFAULT_HELMET;
			}
		}

		s32 nHelmetIndices = GetHelmetPropIndex(m_DesiredPropFlag);
		if(nHelmetIndices >= 0)
		{
			// backup any current head prop (hat), if we don't already have a helmet (we'll be called multiple times) because we are about to enable it
			if(!m_pParentPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HasHelmet ) && (GetStoredHatPropIndex() == -1 || GetStoredHatTexIndex() == -1))
			{
				int nHatPropIdx = CPedPropsMgr::GetPedPropIdx(m_pParentPed, ANCHOR_HEAD);
				if(nHatPropIdx != -1)
				{
					SetStoredHatIndices(nHatPropIdx, CPedPropsMgr::GetPedPropTexIdx(m_pParentPed, ANCHOR_HEAD));

					pedDebugf1("CPedHelmetComponent[%s][%p] - EnableHelmet -  Storing restore hat prop:%i tex:%i", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed, nHatPropIdx,  CPedPropsMgr::GetPedPropTexIdx(m_pParentPed, ANCHOR_HEAD));
				}
			}

			//Vector3 vPosOffset;
			//Quaternion qRotOffset;

			//if(anchorOverrideID != ANCHOR_NONE)
			//{
			//	m_pParentPed->GetBikeHelmetOffsets(vPosOffset, qRotOffset);
			//}

			int nTextureIndex = GetHelmetTexIndex(nHelmetIndices);

			if(CPedPropsMgr::GetNumActiveProps(m_pParentPed) < MAX_PROPS_PER_PED || CPedPropsMgr::GetPedPropIdx(m_pParentPed, ANCHOR_HEAD) != -1)
			{
				CPedPropsMgr::SetPedProp(m_pParentPed, ANCHOR_HEAD, nHelmetIndices, nTextureIndex, anchorOverrideID, NULL, NULL);

				pedDebugf1("CPedHelmetComponent[%s][%p] - EnableHelmet - Creating helmet, prop:%i tex:%i anchorOverrideID:%i", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed, nHelmetIndices, nTextureIndex, anchorOverrideID);
				m_pParentPed->SetPedConfigFlag( CPED_CONFIG_FLAG_HasHelmet, true );
				m_CurrentPropFlag = m_DesiredPropFlag;
				m_fHelmetInactiveDeletionTimer = 0.0f;

				// Set the synced values so the owner will inform the clone what we need....
				if(NetworkInterface::IsGameInProgress())
				{
					if(!m_pParentPed->IsNetworkClone())
					{
						m_NetworkHelmetIndex	= nHelmetIndices;
						m_NetworkTextureIndex	= nTextureIndex;
					}
				}
			}
		}
	}

	return IsHelmetEnabled();
}

bool CPedHelmetComponent::IsHelmetInHand()
{
	if(m_pParentPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HasHelmet ))
	{
		s32 nHelmetIndex = GetHelmetPropIndex(m_CurrentPropFlag);
		if(nHelmetIndex >= 0)
		{
			if(CPedPropsMgr::GetPedPropIdx(m_pParentPed, ANCHOR_HEAD) == nHelmetIndex)
			{
				if ( CPedPropsMgr::GetPedPropActualAnchorPoint(m_pParentPed,ANCHOR_HEAD) == ANCHOR_PH_L_HAND)
				{
					return true;
				}
			}
		}
	}

	return false;
}

void CPedHelmetComponent::DisableHelmet(bool bDropHelmet, const Vector3* impulse, bool bSafeRestore)
{
	pedDebugf1("CPedHelmetComponent[%p][%s] - DisableHelmet -  Disable helmet requested, drop:%i", m_pParentPed, m_pParentPed ? m_pParentPed->GetModelName() : "NULL", bDropHelmet);

	if(IsHelmetEnabled())
	{
		if(bDropHelmet)
		{
			CPedPropsMgr::KnockOffProps(m_pParentPed, false, false, false, true, impulse);
			pedDebugf1("CPedHelmetComponent[%s][%p] - DisableHelmet -  KnockOffProps", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed);
		}
		else
		{
			CPedPropsMgr::ClearPedProp(m_pParentPed, ANCHOR_HEAD);
			pedDebugf1("CPedHelmetComponent[%s][%p] - DisableHelmet -  ClearPedProp", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed);
		}

		// restore any previously worn hat
		RestoreOldHat(bSafeRestore);
	}

	m_fHelmetInactiveDeletionTimer = 0.0f;

}

void CPedHelmetComponent::RestoreOldHat(bool bSafeRestore)
{
	if (CPedPropsMgr::GetPedPropIdx(m_pParentPed, ANCHOR_HEAD) != GetStoredHatPropIndex() ||
		CPedPropsMgr::GetPedPropTexIdx(m_pParentPed, ANCHOR_HEAD) != GetStoredHatTexIndex())
	{
		if ((GetStoredHatPropIndex() != -1 && GetStoredHatTexIndex() != -1) &&
			(!bSafeRestore || 
			CPedPropsMgr::GetNumActiveProps(m_pParentPed) < MAX_PROPS_PER_PED ||	// We can for sure put on old hat in a free slot
			!CPedPropsMgr::IsPedPropDelayedDelete(m_pParentPed, ANCHOR_HEAD)))		// We cannot override the hat slot due to delayed delete
		{
			CPedPropsMgr::SetPedProp(m_pParentPed, ANCHOR_HEAD, GetStoredHatPropIndex(), GetStoredHatTexIndex(), ANCHOR_NONE, NULL, NULL);
			pedDebugf1("CPedHelmetComponent[%s][%p] - DisableHelmet -  Restoring hat prop:%i tex:%i", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed, GetStoredHatPropIndex(), GetStoredHatTexIndex());
		}
	}

	if (!bSafeRestore ||
		(CPedPropsMgr::GetPedPropIdx(m_pParentPed, ANCHOR_HEAD) == GetStoredHatPropIndex() &&
		CPedPropsMgr::GetPedPropTexIdx(m_pParentPed, ANCHOR_HEAD) == GetStoredHatTexIndex()))
	{
		pedDebugf1("CPedHelmetComponent[%s][%p] - DisableHelmet - RestoreOldHat : Clearing ped helmet", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed);

		m_pParentPed->SetPedConfigFlag( CPED_CONFIG_FLAG_HelmetHasBeenShot, false );
		m_pParentPed->ResetNumArmouredHelmetHits();
		m_pParentPed->SetPedConfigFlag( CPED_CONFIG_FLAG_HasHelmet, false );
		m_CurrentPropFlag = PV_FLAG_NONE;
		m_DesiredPropFlag = PV_FLAG_NONE;
		SetStoredHatIndices(-1, -1);

		// wipe the flags - do this on the clone too...
		if(NetworkInterface::IsGameInProgress())
		{
			m_NetworkHelmetIndex = -1;
			m_NetworkTextureIndex = -1;
		}	
	}
}

void CPedHelmetComponent::SetHelmetTexIndex(int nHelmetTexIndex, bool bScriptGiveHelmet)
{
	m_nHelmetTexIndex = -1;

	if(nHelmetTexIndex >= 0)
	{
		//B*2872969 - we shouldnt be setting texture if prop ID was not setup before hand, because it doesnt make sense otherwise.
		s32 nPropIndices = -1;
		if (NetworkInterface::IsGameInProgress() && !bScriptGiveHelmet)
		{
			nPropIndices = m_nHelmetPropIndex;
		}
		else
		{
			// Gonna keep the old behaviour for SP and when this is coming from GIVE_PED_HELMET. This will return random prop id from desired archetype.
			// I don't think it's actually working like script expect it to, but don't want to be changing the behaviour at this point in the project.
			nPropIndices = GetHelmetPropIndex(m_DesiredPropFlag);
		}

		if(aiVerifyf(nPropIndices >= 0, "Trying to set helmet texture when prop index is not setup, nPropIndices - %i; bScriptGiveHelmet - %s", nPropIndices, bScriptGiveHelmet ? "true":"false"))
		{
			Assertf(nHelmetTexIndex >= 0 && nHelmetTexIndex < CPedPropsMgr::GetMaxNumPropsTex(m_pParentPed, ANCHOR_HEAD, nPropIndices),
				"Invalid texture index[%i] attempting to be used when putting on a helmet Ped[%s], p_head Index:[%i]",nHelmetTexIndex, m_pParentPed->GetModelName(), nPropIndices );

			if(nHelmetTexIndex >= 0 && nHelmetTexIndex < CPedPropsMgr::GetMaxNumPropsTex(m_pParentPed, ANCHOR_HEAD, nPropIndices))
			{
				m_nHelmetTexIndex = nHelmetTexIndex;

				pedDebugf1("CPedHelmetComponent[%s][%p] - SetHelmetTexIndex -  Set helmet override texture id tex:%i", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed, m_nHelmetTexIndex);
			}
		}
	}
}

void CPedHelmetComponent::SetHelmetPropIndex(int nHelmetPropIndex, bool bIncludeBicycles)
{
	m_nHelmetPropIndex = -1;

	if(nHelmetPropIndex >= 0)
	{
		Assertf(nHelmetPropIndex < (int)CPedPropsMgr::GetMaxNumProps(m_pParentPed, ANCHOR_HEAD), "Invalid prop p_head index:[%i] being set on ped[%s]", nHelmetPropIndex, m_pParentPed->GetModelName() );

		if(nHelmetPropIndex < (int)CPedPropsMgr::GetMaxNumProps(m_pParentPed, ANCHOR_HEAD))
		{
			m_nHelmetPropIndex = nHelmetPropIndex;
			m_bUseOverridenHelmetPropIndexOnBiycles = bIncludeBicycles;

			pedDebugf1("CPedHelmetComponent[%s][%p] - SetHelmetPropIndex -  Set helmet override prop id prop:%i", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed, m_nHelmetPropIndex);
		}
	}
}

void CPedHelmetComponent::SetVisorPropIndices(bool bSupportsVisors, bool bVisorUp, s32 HelmetVisorUpPropId, s32 HelmetVisorDownPropId)
{
	pedDebugf1("CPedHelmetComponent[%s][%p] - SetVisorPropIndices: SupportsVisors - %s; IsVisorUp new - %s; VisorUp id old - %i, new - %i; VisorDown old - %i, new - %i", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed, bSupportsVisors ? "true":"false", bVisorUp ? "true":"false", m_nHelmetVisorUpPropIndex, HelmetVisorUpPropId, m_nHelmetVisorDownPropIndex, HelmetVisorDownPropId);

	m_bSupportsVisors = bSupportsVisors;
	m_bIsVisorUp = bVisorUp;
	m_nHelmetVisorUpPropIndex = HelmetVisorUpPropId;
	m_nHelmetVisorDownPropIndex = HelmetVisorDownPropId;
}

void CPedHelmetComponent::SetPropFlag(ePedCompFlags propflag)
{
	Assertf(propflag == PV_FLAG_NONE || GetHelmetPropIndex(propflag) >= 0, "Attempting to set the desired helmet type but ped (%s) doesn't any prop set up with this flag\n", m_pParentPed->GetModelName());
	
	m_DesiredPropFlag = propflag; 
}

bool CPedHelmetComponent::HasHelmetForVehicle(const CVehicle* pVehicle) const
{
	if(pVehicle)
	{	
		if(pVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
		{
			return HasHelmetOfType(PV_FLAG_RANDOM_HELMET);
		}
		else if(pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE || pVehicle->GetVehicleType() == VEHICLE_TYPE_QUADBIKE || pVehicle->InheritsFromAmphibiousQuadBike())
		{
			return HasHelmetOfType(PV_FLAG_DEFAULT_HELMET);
		}
		else if (pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI || pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE)
		{
			return HasHelmetOfType(PV_FLAG_FLIGHT_HELMET);
		}
		
		Assertf(0, "CPedHelmetComponent::HasHelmetForVehicle - Vehicle:%s currently isn't support", pVehicle->GetModelName() );
	}
	return false;
}

bool CPedHelmetComponent::IsAlreadyWearingAHelmetProp(const CVehicle* pVehicle) const
{
	if(pVehicle)
	{	
		ePedCompFlags propFlag = PV_FLAG_DEFAULT_HELMET;

		if(pVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
		{
			propFlag = PV_FLAG_RANDOM_HELMET;
		}
		else if (pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI || pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE)
		{
			propFlag = PV_FLAG_FLIGHT_HELMET;
		}
		
		//Already wearing a helmet which is flagged as a helmet prop, this won't set CPED_CONFIG_FLAG_HasHelmet so it's not taken off. 
		if(CPedPropsMgr::GetPedPropIdx(m_pParentPed, ANCHOR_HEAD) != -1)
		{
			if(CPedPropsMgr::DoesPedHavePropOrComponentWithFlagSet(*m_pParentPed,propFlag))
			{
				return true;
			}

			//Don't allow script helmets to be replaced with another motor bike helmet
			if(propFlag == PV_FLAG_DEFAULT_HELMET && CPedPropsMgr::DoesPedHavePropOrComponentWithFlagSet(*m_pParentPed, PV_FLAG_SCRIPT_HELMET))
			{
				return true;
			}

			//Don't replace the pilot mask (Flight School DLC) with other helmets (motorbike, plane, etc)
			if(CPedPropsMgr::DoesPedHavePropOrComponentWithFlagSet(*m_pParentPed, PV_FLAG_PILOT_HELMET))
			{
				return true;
			}

			//check for armored helmet (LTS DLC)
			if(CPedPropsMgr::CheckPropFlags(m_pParentPed, ANCHOR_HEAD, PV_FLAG_ARMOURED))
			{
				return true;
			}
		}
	}
	return false;
}

bool CPedHelmetComponent::IsWearingHelmetWithGoggles()
{
	s32 nVisorPropIndex = GetHelmetPropIndex(m_CurrentPropFlag);
	int nVisorTextureIndex = GetHelmetTexIndex(nVisorPropIndex);
	if (nVisorPropIndex >= 0 && nVisorTextureIndex >= 0)
	{
		u32 uPropNameHash = EXTRAMETADATAMGR.GetHashNameForProp(m_pParentPed, ANCHOR_HEAD, (u32)nVisorPropIndex, (u32)nVisorTextureIndex);
		return EXTRAMETADATAMGR.DoesShopPedApparelHaveRestrictionTag((u32)uPropNameHash, atHashString("GOGGLES_HELMET",0xF7104C9B));
	}

	return false;
}

bool CPedHelmetComponent::HasHelmetOfType(ePedCompFlags PropFlag) const
{
	return CPedPropsMgr::GetPropIdxWithFlags(m_pParentPed, ANCHOR_HEAD, PropFlag) >= 0;
}

s32 CPedHelmetComponent::GetHelmetPropIndex(ePedCompFlags PropFlag) const
{
	if(NetworkInterface::IsGameInProgress())
	{
		if(m_pParentPed && m_pParentPed->IsNetworkClone() && !m_pParentPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor))
		{
			CNetObjPed* netObjPed = static_cast<CNetObjPed*>(m_pParentPed->GetNetworkObject());
			if(netObjPed)
			{
				Assert(m_NetworkHelmetIndex != -1);
				return m_NetworkHelmetIndex;
			}
		}
	}

	// B*2214377: Allow script to ignore overriden helmet prop index if on/recently on a bicycle.
	bool bOnOrRecentlyOnBicycle = ((VehicleType)m_iMostRecentVehicleType == VEHICLE_TYPE_BICYCLE);

	//If the prop index has been overriden then use that
	//Only use the m_nHelmetPropIndex in mp for bikes and quads and visor switching on foot
	if(m_nHelmetPropIndex != -1 && UseOverrideInMP() && (!bOnOrRecentlyOnBicycle || (bOnOrRecentlyOnBicycle && m_bUseOverridenHelmetPropIndexOnBiycles)))
	{
		pedDebugf1("CPedHelmetComponent[%s][%p] - GetHelmetPropIndex - Found set m_nHelmetPropIndex on ped, returning prop:%i", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed, m_nHelmetPropIndex);
		return m_nHelmetPropIndex;
	}

	if (m_nHelmetPropIndexFromHashMap != -1)
	{
		pedDebugf1("CPedHelmetComponent[%s][%p] - GetHelmetPropIndex - Found set m_nHelmetPropIndexFromHashMap on ped, returning prop:%i", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed, m_nHelmetPropIndex);
		return m_nHelmetPropIndexFromHashMap;
	}

	// New metadata prop system
	pedDebugf1("CPedHelmetComponent[%s][%p] - GetHelmetPropIndex - No previously set PropIndex on ped, returning random helmet from CPedPropsMgr with given flags", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed);
	return CPedPropsMgr::GetPropIdxWithFlags(m_pParentPed, ANCHOR_HEAD, PropFlag);
}

bool CPedHelmetComponent::IsHelmetTexValid(CPed* pPed, s32 HelmetTexId, ePedCompFlags PropFlag)
{
	s32 nPropIndices = pPed->GetHelmetComponent()->GetHelmetPropIndex(PropFlag);

	return HelmetTexId < (int)CPedPropsMgr::GetMaxNumPropsTex(pPed, ANCHOR_HEAD, nPropIndices);
}

int CPedHelmetComponent::GetHelmetTexIndex(int nPropIndex)
{
	if(NetworkInterface::IsGameInProgress())
	{
		if(m_pParentPed && m_pParentPed->IsNetworkClone() && !m_pParentPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor))
		{
			CNetObjPed* netObjPed = static_cast<CNetObjPed*>(m_pParentPed->GetNetworkObject());
			if(netObjPed)
			{
				pedDebugf1("CPedHelmetComponent[%s][%p] - GetHelmetTexIndex - Network clone texture index, tex:%i", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed, m_NetworkTextureIndex);
				Assert(m_NetworkTextureIndex != -1);
				return m_NetworkTextureIndex;
			}
		}
	}

	if(m_nHelmetTexIndex != -1 && UseOverrideInMP())
	{
		pedDebugf1("CPedHelmetComponent[%s][%p] - GetHelmetTexIndex - Using override texture, tex:%i", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed, m_nHelmetTexIndex);
		return m_nHelmetTexIndex;
	}

	if(m_nPreLoadedTexture != -1 )
	{
		pedDebugf1("CPedHelmetComponent[%s][%p] - GetHelmetTexIndex - Using preloaded texture, tex:%i", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed, m_nPreLoadedTexture);
		return m_nPreLoadedTexture;
	}
	
	// Random bike helmet texture variant
	int nTextureIndex = 0;
	if(IsHelmetEnabled())
	{
		// Currently have a helmet - return its texture index
		nTextureIndex = CPedPropsMgr::GetPedPropTexIdx(m_pParentPed, ANCHOR_HEAD);

		//If GetPedPropTexIdx returns -1 then use the passed in prop index
		if(nTextureIndex == -1)
		{
			nTextureIndex = fwRandom::GetRandomNumberInRange(0, (int)CPedPropsMgr::GetMaxNumPropsTex(m_pParentPed, ANCHOR_HEAD, nPropIndex));
		}
	}
	else
	{
		nTextureIndex = fwRandom::GetRandomNumberInRange(0, (int)CPedPropsMgr::GetMaxNumPropsTex(m_pParentPed, ANCHOR_HEAD, nPropIndex));
	}

	pedDebugf1("CPedHelmetComponent[%s][%p] - GetHelmetTexIndex - Using random texture, tex:%i", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed, nTextureIndex);
	return nTextureIndex;
}

void CPedHelmetComponent::SetStoredHatIndices(int nPropIndex, int nTexIndex)
{
	Assert(nPropIndex >= -1 && nPropIndex < CPedPropsMgr::GetMaxNumProps(m_pParentPed, ANCHOR_HEAD));
	m_nStoredHatPropIdx = Clamp<int>(nPropIndex, -1, CPedPropsMgr::GetMaxNumProps(m_pParentPed, ANCHOR_HEAD));

	if(m_nStoredHatPropIdx != -1)
		m_nStoredHatTexIdx = nTexIndex;
	else
		m_nStoredHatTexIdx = -1;

	pedDebugf1("CPedHelmetComponent[%s][%p] - SetStoredHatIndices -  Storing hat data prop:%i tex:%i", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed, m_nStoredHatPropIdx, m_nStoredHatTexIdx);
}

void CPedHelmetComponent::RestoreStoredHatIfSafe(const float fMinDistance)
{
	Assert(m_pParentPed->GetHelmetComponent() == this);

	if(GetStoredHatPropIndex() >= CPedPropsMgr::GetMaxNumProps(m_pParentPed, ANCHOR_HEAD))
	{
		AI_OPTIMISATIONS_OFF_ONLY(Assert(false)); // PRAGMA-OPTIMIZE-ALLOW

		// The stored hat prop has become invalid for some reason - so clear it
		SetStoredHatIndices(-1, -1);
	}

	if(!IsHelmetEnabled() && GetStoredHatPropIndex() != -1)
	{
		// Do a distance check to the camera, so we don't pop hats on in front of player
		const Vector3& vCamPosition = camInterface::GetPos();

		if(m_pParentPed->IsLocalPlayer() || m_pParentPed->PopTypeIsMission() || NetworkInterface::IsGameInProgress() || ((vCamPosition - VEC3V_TO_VECTOR3(m_pParentPed->GetTransform().GetPosition())).Mag2() > square(fMinDistance)))
		{
			RestoreStoredHat();
		}
	}
}

bool CPedHelmetComponent::RestoreStoredHat()
{
	bool bRestored = false;
	if (GetStoredHatPropIndex() != -1)
	{
		CPedPropsMgr::SetPedProp(m_pParentPed, ANCHOR_HEAD, GetStoredHatPropIndex(), GetStoredHatTexIndex(), ANCHOR_NONE, NULL, NULL);
		pedDebugf1("CPedHelmetComponent[%s][%p] - RestoreStoredHat -  Restoring hat prop:%i tex:%i", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed, GetStoredHatPropIndex(), GetStoredHatTexIndex());
		bRestored = true;
	}
	SetStoredHatIndices(-1, -1);
	return bRestored;
}


bool CPedHelmetComponent::RequestPreLoadProp(bool bRestoreHat)
{
	s32 nHelmetIndices = -1;
	int nTextureIndex = -1;

	if(!bRestoreHat)
	{
		ePedCompFlags propFlag = m_DesiredPropFlag;

		if(propFlag == PV_FLAG_NONE)
		{
			CVehicle* pVehicle = m_pParentPed->GetVehiclePedInside();
			if(pVehicle)
			{	
				if(pVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
				{
					propFlag = PV_FLAG_RANDOM_HELMET;
				}
				else if(pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE || pVehicle->GetVehicleType() == VEHICLE_TYPE_QUADBIKE || pVehicle->InheritsFromAmphibiousQuadBike())
				{
					propFlag = PV_FLAG_DEFAULT_HELMET;
				}
				else if (pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI)
				{
					propFlag = PV_FLAG_FLIGHT_HELMET;
				}
			}
		}
		Assertf(propFlag != PV_FLAG_NONE, "CPedHelmetComponent::RequestPreLoadProp - Invalid PropFlag");

		nHelmetIndices = m_pParentPed->GetHelmetComponent()->GetHelmetPropIndex(m_DesiredPropFlag);
		if(nHelmetIndices >= 0)
		{
			nTextureIndex = GetHelmetTexIndex(nHelmetIndices);
		}
	}
	else
	{
		nHelmetIndices = GetStoredHatPropIndex();
		nTextureIndex = GetStoredHatTexIndex();
	}

	if(nHelmetIndices >= 0 && nTextureIndex >= 0)
	{
		CPedPropsMgr::SetPreloadProp(m_pParentPed, ANCHOR_HEAD, nHelmetIndices, nTextureIndex);
		pedDebugf1("CPedHelmetComponent[%s][%p] - RequestPreLoadProp -  Requesting preload prop:%i tex:%i", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed, nHelmetIndices, nTextureIndex);
		
		//Sort the texture for preloaded prop
		m_nPreLoadedTexture = nTextureIndex;
		m_bPreLoadingProp = true;

		// record which texture we've preloaded for the network...
		if(NetworkInterface::IsGameInProgress())
		{
			// if we're putting a helmet on, sync the prop / tex indices as soon as we start preloading so the clone can start.
			// if we're putting a hat back on after taking the helmet off, don't sync otherwise the clone helmet and hat models 
			// can swap at inappropriate times. The clone stores the correct hat and texture index in GetStoredHat<Prop/Tex>Index() 
			// locally anyway.
			if(!bRestoreHat)
			{
				if(!m_pParentPed->IsNetworkClone())
				{
					m_NetworkHelmetIndex  = nHelmetIndices;
					m_NetworkTextureIndex = nTextureIndex;	
				}
			}
		}

		return true;
	}

	return false;
}

bool CPedHelmetComponent::IsPreLoadingProp()
{
	return m_bPreLoadingProp;
}

bool CPedHelmetComponent::HasPreLoadedProp()
{
	if(IsPreLoadingProp() && CPedPropsMgr::HasPreloadPropsFinished(m_pParentPed))
	{
		m_bPreLoadingProp = false;
		return true;
	}
	return false;
}

void CPedHelmetComponent::ClearPreLoadedProp()
{
	m_nPreLoadedTexture = -1;
	m_bPreLoadingProp = false;
	CPedPropsMgr::CleanUpPreloadProps(m_pParentPed);
	pedDebugf1("CPedHelmetComponent[%s][%p] - ClearPreLoadedProp -  Clearing preload prop", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed);
}

bool CPedHelmetComponent::UseOverrideInMP() const
{
	if(NetworkInterface::IsGameInProgress())
	{
		//Override for motorcycles, quadbikes.  Originally implemented to fix url:bugstar:1523777 and submitted to dev in CL 4569275 (colivant), modification for bifta/golf-carts/etc follow on (lavalley).
		//Also override when ped has a helmet that supports visor switching (pbernotas) - B*2907088
		//Portions extraced from CPed::SetPedInVehicle where it does similiar processing (but not exactly the same)
		CVehicle* pVehicle = m_pParentPed->GetVehiclePedInside();
		bool bHelmetSupportsVisor = m_pParentPed->GetHelmetComponent() && m_pParentPed->GetHelmetComponent()->GetCurrentHelmetSupportsVisors();
		bool bTakingOffHelmet = m_pParentPed->GetPedResetFlag(CPED_RESET_FLAG_IsRemovingHelmet);
		if (!pVehicle && !bHelmetSupportsVisor && !bTakingOffHelmet)
		{
			pedDebugf1("CPedHelmetComponent[%s][%p] - UseOverrideInMP failed - not in vehicle and not visor switching on foot.", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed);
			return false;
		}

		if (pVehicle)
		{
			//never override for these vehicles - use the vehicle specific headgear
			if (pVehicle->InheritsFromPlane() || pVehicle->InheritsFromHeli() || pVehicle->InheritsFromAutogyro() || pVehicle->InheritsFromBlimp())
			{
				pedDebugf1("CPedHelmetComponent[%s][%p] - UseOverrideInMP failed - in a plane/heli/autogyro/blimp", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed);
				return false;
			}

			//always override for these vehicles - getting the player custom headgear
			if (pVehicle->InheritsFromBike() || pVehicle->InheritsFromQuadBike() || pVehicle->InheritsFromAmphibiousQuadBike())
				return true;
		

			//further determine below if the headgear should be retained or not...

			// HACK: FLAG_PEDS_INSIDE_CAN_BE_SET_ON_FIRE_MP is tagged on vehicles that have open tops (caddy, dune, mower, etc.), only flag we have to identify these.
			bool KeepHatOnInOpenTopVehicles = pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_PEDS_INSIDE_CAN_BE_SET_ON_FIRE_MP);
			bool KeepHeadProp = KeepHatOnInOpenTopVehicles;
			if (!KeepHeadProp)
			{
				//do some extra evaluation
				s32 iPedsSeatIndex = pVehicle ? pVehicle->GetSeatManager()->GetPedsSeatIndex(m_pParentPed) : -1;
				const CVehicleSeatInfo* pSeatInfo = (iPedsSeatIndex != -1) ? pVehicle->GetSeatInfo(iPedsSeatIndex) : NULL;
				KeepHeadProp = (pSeatInfo && pSeatInfo->GetKeepOnHeadProp());
			}

			if (KeepHatOnInOpenTopVehicles)
			{
				// HACK: Only remove hats on the BIFTA if we have a roof mod.
				static const u32 biftaHash = ATSTRINGHASH("bifta", 0xeb298297);
				const u32 vehModelHash = pVehicle->GetVehicleModelInfo()->GetModelNameHash();
				if (vehModelHash == biftaHash)
				{
					const CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();
					if (variation.GetModIndex(VMT_ROOF) != INVALID_MOD)
						KeepHeadProp = false;
				}
			}

			// Remove helmet if not getting on a bike
			if(!KeepHeadProp && pVehicle->GetVehicleType() != VEHICLE_TYPE_BIKE && pVehicle->GetVehicleType() != VEHICLE_TYPE_QUADBIKE && pVehicle->GetVehicleType() != VEHICLE_TYPE_BICYCLE && pVehicle->GetVehicleType() != VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE && IsHelmetEnabled())
			{
				pedDebugf1("CPedHelmetComponent[%s][%p] - UseOverrideInMP failed - not on a bike", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed);
				return false;
			}
		}
	}

	return true;
}

// B*2216490: Sets an override helmet prop index from the hash map if available for the current vehicle.
void CPedHelmetComponent::SetHelmetIndexFromHashMap()
{
	// B*2216490: If ped is in a vehicle which has had a prop index specified vehicle-helmet hashmap, use that.
	if (m_nHelmetPropIndex == -1 && m_nHelmetPropIndexFromHashMap == -1 && m_pParentPed && m_pParentPed->GetIsInVehicle())
	{
		const CVehicle *pVehicle = m_pParentPed->GetVehiclePedInside();
		if (pVehicle && pVehicle->GetBaseModelInfo())
		{
			const u32 uVehicleHash = pVehicle->GetBaseModelInfo()->GetModelNameHash();

			// Ensure an entry exists for this vehicle.
			if (sm_VehicleHashToHelmetPropIndex.Access(uVehicleHash))
			{
				atArray<int> *propIndexArray = sm_VehicleHashToHelmetPropIndex.Access(uVehicleHash);

				// Pick a random prop index from the array.
				const int iNumOfProps = propIndexArray->GetCount();
				const int iPropToSelect = fwRandom::GetRandomNumberInRange(0, iNumOfProps);
				const int iHelmetPropIndex = (*propIndexArray)[iPropToSelect];

				// Cache prop flag (used in CPedHelmetComponent::GetHelmetPropIndex).
				m_nHelmetPropIndexFromHashMap = iHelmetPropIndex;
				pedDebugf1("CPedHelmetComponent[%s][%p] - SetHelmetIndexFromHashMap - Set helmet override prop id from vehicle-helmet hashmap prop:%i", m_pParentPed ? m_pParentPed->GetModelName() : "NULL", m_pParentPed, m_nHelmetPropIndexFromHashMap);
			}
		}
	}
}

// B*2216490: Functions accessed from script to define vehicle specific helmet prop indexes.
void CPedHelmetComponent::AddVehicleHelmetAssociation(const u32 uVehicleHash, const int iHelmetPropIndex)
{
	if (uVehicleHash != 0 && iHelmetPropIndex != -1.0f)
	{
		// Already have an entry for this vehicle hash, so just add the helmet index to the array.
		if (sm_VehicleHashToHelmetPropIndex.Access(uVehicleHash))
		{
			// Don't add the helmet index if it's already been added.
			if (sm_VehicleHashToHelmetPropIndex.Access(uVehicleHash)->Find(iHelmetPropIndex) == -1)
			{
				sm_VehicleHashToHelmetPropIndex.Access(uVehicleHash)->PushAndGrow(iHelmetPropIndex);
			}
		}
		else
		{
			// We haven't created an entry for this vehicle yet, so initialize it and the helmet array.
			atArray<int> helmetPropIndexArray;
			helmetPropIndexArray.Reset();
			helmetPropIndexArray.PushAndGrow(iHelmetPropIndex);
			sm_VehicleHashToHelmetPropIndex.Insert(uVehicleHash, helmetPropIndexArray);
		}
	}
}

void CPedHelmetComponent::ClearVehicleHelmetAssociation(const u32 uVehicleHash)
{
	if (uVehicleHash != 0)
	{
		if (sm_VehicleHashToHelmetPropIndex.Access(uVehicleHash))
		{
			sm_VehicleHashToHelmetPropIndex.Delete(uVehicleHash);
		}
	}
}

void CPedHelmetComponent::ClearAllVehicleHelmetAssociations()
{
	sm_VehicleHashToHelmetPropIndex.Reset();
}

bool CPedHelmetComponent::ShouldUsePilotFlightHelmet(const CPed *pPed)
{
	if (!pPed)
		return false;

	CVehicle *pVehicle = static_cast<CVehicle*>(pPed->GetVehiclePedInside());

	if (!pVehicle)
		return false;

	// In some cases, we want to use the regular flight helmet even if the seat has weapons.
	if (pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_STANDARD_FLIGHT_HELMET))
		return false;

	// Only allow driver or passengers with an available vehicle weapon to use the pilot helmet.
	s32 iSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
	const bool bSeatHasWeapons = pVehicle->GetSeatManager() && pVehicle->GetSeatHasWeaponsOrTurrets(iSeatIndex);
	bool bSeatHasValidWeapon = bSeatHasWeapons;
	if (bSeatHasWeapons)
	{
		// B*2295117: Loop over vehicle weapons. Don't equip pilot helmet if VEHICLE_WEAPON_SEARCHLIGHT is the only weapon available.
		CVehicleWeaponMgr* pWeaponMgr = pVehicle->GetVehicleWeaponMgr();
		if (pWeaponMgr)
		{
			bool bFoundSearchLight = false;
			bool bFoundOtherWeapon = false;
			static const atHashWithStringNotFinal searchLightWeaponName("VEHICLE_WEAPON_SEARCHLIGHT",0xCDAC517D);
			for (int i = 0; i < pWeaponMgr->GetNumWeaponsForSeat(iSeatIndex); i++)
			{
				CVehicleWeapon *pVehWeapon = pWeaponMgr->GetVehicleWeapon(i);
				if (pVehWeapon)
				{
					if (pVehWeapon->GetHash() == searchLightWeaponName)
					{
						bFoundSearchLight = true;
					}
					else
					{
						bFoundOtherWeapon = true;
					}
				}
			}
			
			if (bFoundSearchLight && !bFoundOtherWeapon)
			{
				bSeatHasValidWeapon = false;
			}
		}
	}

	const CVehicleSeatInfo* pSeatInfo = pVehicle->IsSeatIndexValid(iSeatIndex) ? pVehicle->GetSeatInfo(iSeatIndex) : NULL;
	bool bGivePilotHelmetForSeat = pSeatInfo && pSeatInfo->GetGivePilotHelmetForSeat();
	if ((bGivePilotHelmetForSeat || pPed->GetIsDrivingVehicle() || bSeatHasWeapons) && (pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI || pVehicle->GetVehicleType() == VEHICLE_TYPE_PLANE))
	{
		CVehicleModelInfo* pVehModelInfo = pVehicle->GetVehicleModelInfo();
		if (bSeatHasValidWeapon || (pVehModelInfo && pVehModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_PILOT_HELMET)))
		{
			return true;
		}
	}
	return false;
}

fwMvClipSetId CPedHelmetComponent::GetAircraftPutOnTakeOffHelmetFPSClipSet(const CVehicle* pVehicle) 
{ 
	if (aiVerify(pVehicle && pVehicle->GetIsAircraft())) 
	{
		if (pVehicle->GetIsHeli())
		{
			const fwClipSet* pHeliClipSet = fwClipSetManager::GetClipSet(CLIP_SET_PUT_ON_TAKE_OFF_HELMET_FPS_HELI);
			if (pHeliClipSet && (pHeliClipSet->GetClipDictionaryIndex() != -1))
			{
				return CLIP_SET_PUT_ON_TAKE_OFF_HELMET_FPS_HELI;
			}
		}

		return CLIP_SET_PUT_ON_TAKE_OFF_HELMET_FPS;
	}

	return CLIP_SET_ID_INVALID;
}

#if __BANK
void CPedHelmetComponent::InitWidgets()
{
	bkBank& bank = BANKMGR.CreateBank("Ped Helmets");
	bank.PushGroup("Vehicle-Helmet Hashmap", false);
	bank.AddText("Vehicle Hash:", &sm_uVehicleHashString, false);
	bank.AddSlider("Helmet Prop Index:", &sm_iHelmetPropIndex, -1, 40, 1);
	bank.AddButton("Add Vehicle-Helmet Association", WidgetAddVehicleHelmetAssociation);
	bank.AddButton("Clear Vehicle-Helmet Association", WidgetClearVehicleHelmetAssociation);
	bank.AddButton("Clear All Vehicle-Helmet Associations", WidgetClearAllVehicleHelmetAssociations);
	bank.PopGroup();
}

void CPedHelmetComponent::WidgetAddVehicleHelmetAssociation()
{
	CPedHelmetComponent::AddVehicleHelmetAssociation(sm_uVehicleHashString.GetHash(), sm_iHelmetPropIndex);
}

void CPedHelmetComponent::WidgetClearVehicleHelmetAssociation()
{
	CPedHelmetComponent::ClearVehicleHelmetAssociation(sm_uVehicleHashString.GetHash());
}

void CPedHelmetComponent::WidgetClearAllVehicleHelmetAssociations()
{
	CPedHelmetComponent::ClearAllVehicleHelmetAssociations();
}
#endif	// __BANK