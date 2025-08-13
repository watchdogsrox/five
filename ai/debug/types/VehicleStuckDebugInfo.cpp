#include "ai\debug\types\VehicleStuckDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

#include "vehicles\vehicle.h"
#include "vehicleAi\VehicleIntelligence.h"

CVehicleStuckDebugInfo::CVehicleStuckDebugInfo(const CVehicle* pVeh, DebugInfoPrintParams printParams, s32 iNumberOfLines)
: CVehicleDebugInfo(pVeh, printParams, iNumberOfLines)
{

}

void CVehicleStuckDebugInfo::PrintRenderedSelection(bool bPrintAll)
{
	if(!CVehicleIntelligence::m_bDisplayStuckDebugInfo && !bPrintAll)
		return;

	if (!m_Vehicle || !m_Vehicle->GetIntelligence())
		return;

	WriteLogEvent("VEHICLE STUCK INFO");
	PushIndent();
	PushIndent();
	PushIndent();
	
	ColorPrintLn(Color_red, "MillisecondsNotMoving:%d ent:%p veh:%p", m_Vehicle->GetIntelligence()->MillisecondsNotMoving, 
											m_Vehicle->GetIntelligence()->m_pObstructingEntity.Get(), m_Vehicle->GetIntelligence()->m_pCarThatIsBlockingUs.Get());
	ColorPrintLn(Color_red, "Parked: %s", m_Vehicle->m_nVehicleFlags.bIsThisAParkedCar ? "true" : "false");
	ColorPrintLn(Color_red, "Stationary: %s",  m_Vehicle->m_nVehicleFlags.bIsThisAStationaryCar ? "true" : "false");

	const bool bDrivable = m_Vehicle->GetStatus() != STATUS_WRECKED && !m_Vehicle->m_nVehicleFlags.bIsDrowning && m_Vehicle->m_nVehicleFlags.bIsThisADriveableCar;
	ColorPrintLn(Color_red, "Drivable: %s",  bDrivable ? "true" : "false");
	ColorPrintLn(Color_red, "Engine On: %s",  m_Vehicle->m_nVehicleFlags.bEngineOn ? "true" : "false");
	ColorPrintLn(Color_red, "Collision: %s", m_Vehicle->IsCollisionEnabled() ? "on" : "off");
	ColorPrintLn(Color_red, "Contact wheels: %d", m_Vehicle->GetNumContactWheels());

	Color32 fixedColour = Color_green;
	if ( m_Vehicle->IsProtectedBaseFlagSet(fwEntity::IS_FIXED_UNTIL_COLLISION) )
	{
		fixedColour = Color_red;
		ColorPrintLn(fixedColour, "Fixed waiting for collision!");
	}
	else if( m_Vehicle->IsBaseFlagSet(fwEntity::IS_FIXED_BY_NETWORK) )
	{
		fixedColour = Color_red;
		ColorPrintLn(fixedColour, "Fixed by network!");
	}
	else if( m_Vehicle->IsBaseFlagSet(fwEntity::IS_FIXED) )
	{
		fixedColour = Color_red;
		ColorPrintLn(fixedColour, "Fixed by code/script!");
	}
	else
	{
		ColorPrintLn(fixedColour, "Not fixed");
	}

	bool bNeverActivate =  (m_Vehicle->GetCurrentPhysicsInst() && m_Vehicle->GetCurrentPhysicsInst()->GetInstFlag(phInst::FLAG_NEVER_ACTIVATE));
	ColorPrintLn(bNeverActivate?Color_red:Color_green, "Never Activate: %s", bNeverActivate ? "true" : "false");
	ColorPrintLn(Color_red, "Load collision: %s", m_Vehicle->ShouldLoadCollision() ? "yes" : "no");
	ColorPrintLn(Color_yellow, "Stuck on roof timer: %5.3f", m_Vehicle->GetVehicleDamage()->GetStuckTimer(VEH_STUCK_ON_ROOF));
	ColorPrintLn(Color_yellow, "Stuck on side timer: %5.3f", m_Vehicle->GetVehicleDamage()->GetStuckTimer(VEH_STUCK_ON_SIDE));
	ColorPrintLn(Color_yellow, "Stuck hung up timer: %5.3f", m_Vehicle->GetVehicleDamage()->GetStuckTimer(VEH_STUCK_HUNG_UP));
	ColorPrintLn(Color_yellow, "Stuck jammed timer: %5.3f", m_Vehicle->GetVehicleDamage()->GetStuckTimer(VEH_STUCK_JAMMED));

	if(m_Vehicle->GetCurrentPhysicsInst() && m_Vehicle->GetVehicleModelInfo())
	{
		float fMaxSpeed = static_cast<phArchetypePhys*>(m_Vehicle->GetCurrentPhysicsInst()->GetArchetype())->GetMaxSpeed();
		gtaFragType* pFragType = static_cast<gtaFragType *>(m_Vehicle->GetVehicleModelInfo()->GetFragType());
		phArchetypeDamp *pArchetype = pFragType->GetPhysics(0)->GetArchetype();
		ColorPrintLn(Color_yellow, "Entity Max Speed: %5.3f Original Max Speed: %5.3f", fMaxSpeed, pArchetype->GetMaxSpeed());
	}

	PopIndent();
	PopIndent();
	PopIndent();
}

#endif