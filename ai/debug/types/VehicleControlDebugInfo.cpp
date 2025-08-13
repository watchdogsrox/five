#include "ai\debug\types\VehicleControlDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

#include "vehicleAi\VehicleIntelligence.h"
#include "Vehicles/Heli.h"
#include "vehicles/Submarine.h"
#include "vehicleAi/Task/TaskVehicleGotoAutomobile.h"
#include "vehicles\transmission.h"

CVehicleControlDebugInfo::CVehicleControlDebugInfo(const CVehicle* pVeh, DebugInfoPrintParams rPrintParams, s32 iNumberOfLines)
: CVehicleDebugInfo(pVeh, rPrintParams, iNumberOfLines)
{

}

void CVehicleControlDebugInfo::PrintRenderedSelection(bool bPrintAll)
{
	if (!m_Vehicle || !m_Vehicle->GetIntelligence())
		return;

	bool bPrintHeader = bPrintAll || CVehicleIntelligence::m_bDisplayControlDebugInfo || 
						CVehicleIntelligence::m_bDisplayControlTransmissionInfo || CVehicleIntelligence::m_bDisplayAILowLevelControlInfo;

	if(bPrintHeader)
	{
		WriteLogEvent("VEHICLE CONTROL INFO");
		PushIndent();
		PushIndent();
	}

	if( CVehicleIntelligence::m_bDisplayControlDebugInfo || bPrintAll )
	{
		WriteHeader("CONTROL INFO");
		PushIndent();
		if(m_Vehicle->GetIsRotaryAircraft())
		{
			const CRotaryWingAircraft* pAirCraft = static_cast<const CRotaryWingAircraft*>(m_Vehicle.Get());
			ColorPrintLn(Color_white, "Throttle: %.2f Pitch: %.2f Yaw: %.2f", pAirCraft->GetThrottleControl(), pAirCraft->GetPitchControl(), pAirCraft->GetYawControl());
		}
		else if(m_Vehicle->InheritsFromSubmarine())
		{
			CVehicle* pVehicle = const_cast<CVehicle*>(m_Vehicle.Get()); //lovely
			CSubmarineHandling* pSubHandling = pVehicle->GetSubHandling();
			ColorPrintLn(Color_white, "Throttle: %.2f Pitch: %.2f Yaw: %.2f Dive: %.2f Rudder: %.2f", m_Vehicle->GetThrottle(), pSubHandling->GetPitchControl(), 
											pSubHandling->GetYawControl(), pSubHandling->GetDiveControl(), pSubHandling->GetRudderAngle());
		}
		else
		{
			CVehControls vehControls;
			vehControls.SetSteerAngle( m_Vehicle->GetSteerAngle() );
			vehControls.m_throttle = m_Vehicle->GetThrottle();
			vehControls.m_brake	= m_Vehicle->GetBrake();
			vehControls.m_handBrake	= m_Vehicle->GetHandBrake();
			ColorPrintLn(Color_white, "Steer: %.2f Gas: %.2f Brake: %.2f HandBrake: %s SteerBias: %.2f", vehControls.m_steerAngle, vehControls.m_throttle, 
																vehControls.m_brake, vehControls.m_handBrake?"On":"Off", m_Vehicle->m_fSteerInputBias);

			float forwardSpeed = DotProduct(m_Vehicle->GetVelocity(), VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetB()));
			float desiredSpeed = 0.0f;
			if( m_Vehicle->GetIntelligence()->GetActiveTaskSimplest() )
			{
				desiredSpeed = m_Vehicle->GetIntelligence()->GetActiveTaskSimplest()->GetCruiseSpeed();
			}

			ColorPrintLn(Color_white, "ForwardSpeed: %.2f DesiredSpeed: %.2f SmoothedSpeed: %.2f", forwardSpeed, desiredSpeed, sqrt(m_Vehicle->GetIntelligence()->GetSmoothedSpeedSq()));
#ifdef __BANK
			ColorPrintLn(Color_blue, "Cheat power increase (%.3f)", m_Vehicle->m_fDebugCheatPowerIncrease);
#endif
		}
		PopIndent();
	}

	if( CVehicleIntelligence::m_bDisplayControlTransmissionInfo || bPrintAll )
	{
		WriteHeader("TRANSMISSION INFO");
		PushIndent();
		const CTransmission& trans = m_Vehicle->m_Transmission;
		ColorPrintLn(Color_purple, "Gear: %d Clutch: %.2f Throttle: %.2f Rev: %.2f", trans.GetGear(), trans.GetClutchRatio(), trans.GetThrottle(), trans.GetRevRatio() );
		PopIndent();
	}

	if( CVehicleIntelligence::m_bDisplayAILowLevelControlInfo || bPrintAll )
	{
		CTaskVehicleGoToPointAutomobile* pTask = static_cast<CTaskVehicleGoToPointAutomobile*>(m_Vehicle->GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_PRIMARY, CTaskTypes::TASK_VEHICLE_GOTO_POINT_AUTOMOBILE));
		if( pTask )
		{
			WriteHeader("LOW LEVEL CONTROL INFO");
			PushIndent();
			ColorPrintLn(Color_white, "Max throttle from traction (%f)", pTask->GetMaxThrottleFromTraction());
			PopIndent();
		}
	}

	if(bPrintHeader)
	{
		PopIndent();
		PopIndent();
	}
}

#endif