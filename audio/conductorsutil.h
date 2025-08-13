#ifndef AUD_CONDUCTORSUTIL_H
#define AUD_CONDUCTORSUTIL_H

#include "vector\vector3.h"

class CEntity;
namespace conductors
{
	// Different conductors
	enum Conductor
	{
		GunfightConductor = 0,
		VehicleConductor,
		SuperConductor,
		MaxNumConductors
	};
	// Possible messages between conductors.
	enum ConductorsMessage
	{
		//superconductor
		SwitchConductorsOff = 0,
		MuteGameWorld,
		UnmuteGameWorld,
		StopQuietScene,
		ResetConductors,
		InvalidZone,// TODO : do the logic for suggesting a different zone.
		//gunfight conductor
		PedDying,
		FiringWhileHeadingToCover,
		GotIntoCover,
		PlayerGotIntoCover,
		PlayerExitCover,
		StartRunAwayScene,
		StopRunAwayScene,
		FakeRicochet,
		VehicleDebris,
		BuildingDebris,
		//VehicleConductor
		SirenTurnedOn,
		ScriptFakeSirens,
		StuntJump,
		MaxNumConductorsMessage,
	};
	struct ConductorMessageData 
	{
		Vector3						vExtraInfo;
		Conductor					conductorName;
		ConductorsMessage			message;
		CEntity						*entity;
		bool						bExtraInfo;
	};
	// Info that specifies the current state that the conductor can be. (To superconductor)
	enum ConductorsInfoType
	{
		Info_Invalid = -1,
		Info_NothingToDo = 0,
		Info_DoBasicJob, 
		Info_DoSpecialStuffs, 
		MaxNumConductorsInfo,
	};
	// Orders that the superconductor can send to the different conductors. 
	enum ConductorsOrders
	{
		Order_Invalid = -1,
		Order_DoNothing,
		//GunfightConductor 
		Order_LowIntensity,
		Order_MediumIntensity,
		Order_HighIntensity,
		Order_LowSpecialIntensity,
		Order_MediumSpecialIntensity,
		Order_HighSpecialIntensity,
		Order_StuntJump,
		MaxNumConductorsOrders,
	};
	// Conductors states
	enum ConductorStates
	{
		ConductorState_Idle = 0,
		ConductorState_EmphasizeLowIntensity,
		ConductorState_EmphasizeMediumIntensity,
		ConductorState_EmphasizeHighIntensity,
	};
	// Possible conductors targets. 
	enum ConductorsTargets
	{
		Near_Primary = 0,
		Near_Secondary,
		Far_Primary,
		Far_Secondary,
		MaxNumConductorsTargets,
		MaxNumVehConductorsTargets = 2,
	};
	// Level of emphasize intensity
	enum EmphasizeIntensity
	{
		Intensity_Invalid = -1,
		Intensity_Low = 0,
		Intensity_Medium,
		Intensity_High,
	};
	// Info struct the conductros will send to the superconductor. 
	struct ConductorsInfo
	{
		ConductorsInfoType info;
		EmphasizeIntensity emphasizeIntensity;
		f32				   confidence;
	};
};
using namespace conductors;

#endif
