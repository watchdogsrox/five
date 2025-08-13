//
// name:		control.h
// description:	class reading input for player

#ifndef INC_CONTROL_H_
#define INC_CONTROL_H_

// Rage headers
#include "grcore/debugdraw.h"
#include "input/mapper.h"
#include "input/joystick.h"
#include "math/simplemath.h"
#include "output/constantlighteffect.h"
#include "output/logitechlightdevice.h"
#include "output/padlightdevice.h"
#include "output/rumbleeffect.h"
#include "atl/atfixedstring.h"
#include "atl/string.h"
#include "atl/array.h"
#include "file/limits.h"
#include "system/threadtype.h"
#include "system/service.h"

// framework headers
#include "fwactuator/recoileffect.h"

#include "control_mapping.h"
#include "control_channel.h"
#include "pad.h"

#include "script/thread.h"

// gameplay

#include "peds/pedDefines.h"
//replay
#include "control/replay/ReplaySettings.h"

#include "vector/color32.h"

#if __WIN32PC
#include "atl/atfixedstring.h"
#endif // __WIN32PC


#define KEYBOARD_MOUSE_SUPPORT	(RSG_PC)

#if KEYBOARD_MOUSE_SUPPORT
#define KEYBOARD_MOUSE_ONLY(...)	__VA_ARGS__
#else
#define KEYBOARD_MOUSE_ONLY(...)
#endif // KEYBOARD_MOUSE_SUPPORT

#define LIGHT_EFFECTS_SUPPORT (RSG_ORBIS || RSG_PC)
#if LIGHT_EFFECTS_SUPPORT
#define LIGHT_EFFECTS_ONLY(x)	x
#else
#define LIGHT_EFFECTS_ONLY(x)
#endif // LIGHT_EFFECTS_SUPPORT

// Currently force feedback is disabled as we do not run the appropriate physics simulations.
#define FORCE_FEEDBACK_SUPPORT 1
#if FORCE_FEEDBACK_SUPPORT
#define FORCE_FEEDBACK_ONLY(...)	__VA_ARGS__
#else
#define FORCE_FEEDBACK_ONLY(...)
#endif // FORCE_FEEDBACK_SUPPORT

#define USE_STEAM_CONTROLLER (0 && __STEAM_BUILD)
#if USE_STEAM_CONTROLLER
#define STEAM_CONTROLLER_ONLY(...)	__VA_ARGS__
#else
#define STEAM_CONTROLLER_ONLY(...)
#endif // USE_STEAM_CONTROLLER

// Framework headers
////#include "fwmaths\maths.h"

// Game headers

namespace rage
{
	struct fiFindData;
	class ioLightEffect;
	class Quaternion;
}


#define CONTROLS_VERSION_NUMBER (2) // current version of the controls file used

//  CompileTimeAssert(MAX_INPUTS == 197);	// UPDATE once legacy support is removed!
//	CompileTimeAssert(MAX_INPUTS == 75);	// Update this assert and the enum in commands_pad.sch

#define INPUT_FRONTEND_CONTROL_BEGIN	(INPUT_FRONTEND_DOWN)
#define INPUT_FRONTEND_CONTROL_END		(INPUT_FRONTEND_SELECT)

// The different button configurations on the console gamepad.
enum eControlLayout
{
	INVALID_LAYOUT = -1,
	STANDARD_TPS_LAYOUT = 0,
	TRIGGER_SWAP_TPS_LAYOUT,
	SOUTHPAW_TPS_LAYOUT,
	SOUTHPAW_TRIGGER_SWAP_TPS_LAYOUT,
	STANDARD_FPS_LAYOUT,
	TRIGGER_SWAP_FPS_LAYOUT,
	SOUTHPAW_FPS_LAYOUT,
	SOUTHPAW_TRIGGER_SWAP_FPS_LAYOUT,
	STANDARD_FPS_ALTERNATE_LAYOUT,
	TRIGGER_SWAP_FPS_ALTERNATE_LAYOUT,
	SOUTHPAW_FPS_ALTERNATE_LAYOUT,
	SOUTHPAW_TRIGGER_SWAP_FPS_ALTERNATE_LAYOUT,
};


//
//
//
class CControl : public datBase
{
public:
	CControl();
	virtual ~CControl();

	void Init(s32 device);

	void Init(unsigned initMode);

	void Update();

	void SetDefaultLookInversions();

	void SetInitialDefaultMappings(bool forceReload, bool loadAutoSavedSettings = false BANK_ONLY(, const char* customMappingsFile = NULL));

	void ClearInputMappings();

	bool LoadControls(char *pFilename, s32 iVersionNum);
	bool SaveControls(char *pFilename, s32 iVersionNum);
	bool AddActionToString(s32 action, char *pString, ioMapper *map_type);

	// PURPOSE: Allows alternate scripted controls layout (e.g. if the user has southpaw (or other non-default) mode, make the INPUT_SCRIPT_* inputs mimic the user
	//			control layout rather than the default.
	// NOTES:	MUST be called every frame.
	void EnableAlternateScriptedControlsLayout();

	// PURPOSE: Loads the default scripted controls layout.
	void UseDefaultScriptedControlsLayout();

	void SetPad(s32 device);

	// general commands
	const ioValue& GetValue(s32 index) const; // TODO: Re-add once legacy support is removed! {return m_inputs[index];}
	const ioValue& GetNextCameraMode() const {return m_inputs[INPUT_NEXT_CAMERA];}

	// PURPOSE: Get the mapped source for an input.
	// PARAMS:	input - the InputType to get the mapped source for.
	//			device - the device to get the icons for. Default is any device.
	//			mappingIndex - the mapping index (0 = primary, 1 = secondary etc). This is only used on versions that support keyboard and mouse.
	//			allowFallback - if a specified source cannot be retrieved, a different mapping source will be returned
	//				(e.g. if secondary cannot be retrieved and a primary exists, it will be returned instead).
	// RETURNS:	The mapped source or ioSource:UNKNOWN_SOURCE if no source exists or there was an error or KEY_NULL for an unmapped keyboard/mouse.
	// NOTES:	input can be the enum value or the enum name as a string (the string variant is considerably slower). 
	ioSource GetInputSource(InputType input, s32 device = ioSource::IOMD_ANY, u8 mappingIndex = 0, bool allowFallback = true) const;
	ioSource GetInputSource(const char* input, s32 device = ioSource::IOMD_ANY, u8 mappingIndex = 0, bool allowFallback = true) const;

	void GetInputSources(InputType input, ioMapper::ioSourceList& sources) const;
	void GetSpecificInputSources(InputType input, ioMapper::ioSourceList& sources, s32 deviceId) const;

	void EnableAllInputs();
	void EnableFrontendInputs();
	void DisableAllInputs(u32 duration, const ioValue::DisableOptions& options = ioValue::DEFAULT_DISABLE_OPTIONS);
	void DisableAllInputs(const ioValue::DisableOptions& options = ioValue::DEFAULT_DISABLE_OPTIONS);
	void DisableInput(unsigned inputIndex, const ioValue::DisableOptions& options = ioValue::DEFAULT_DISABLE_OPTIONS, bool disableRelatedInputs = true); 
	void DisableInputsOnBackButton(u32 duration = 2000);
	void DisableInput(ioMapperSource source,unsigned param, const ioValue::DisableOptions& options = ioValue::DEFAULT_DISABLE_OPTIONS); 

	// Sets an input to have exclusive input from any sources associated with the input.
	void SetInputExclusive(int action, const ioValue::DisableOptions& options = ioValue::DEFAULT_DISABLE_OPTIONS);
	void EnableInput(unsigned inputIndex, bool enableRelatedInputs = true); 
	bool IsInputDisabled(unsigned inputIndex) const;

	// ped commands
	const ioValue& GetPedWalkLeftRight() const {return m_inputs[INPUT_MOVE_LR];}
	const ioValue& GetPedWalkUpDown() const {return m_inputs[INPUT_MOVE_UD];}
	const ioValue& GetPedLookLeftRight() const {return m_inputs[INPUT_LOOK_LR];}
	const ioValue& GetPedLookUpDown() const {return m_inputs[INPUT_LOOK_UD];}
	const ioValue& GetPedAimWeaponLeftRight() const {return m_inputs[INPUT_LOOK_LR];}
	const ioValue& GetPedAimWeaponUpDown() const {return m_inputs[INPUT_LOOK_UD];}
	const ioValue& GetPedAttack() const {return m_inputs[INPUT_ATTACK];}
	const ioValue& GetPedAttack2() const {return m_inputs[INPUT_ATTACK2];}
	const ioValue& GetPedEnter() const {return m_inputs[INPUT_ENTER];}

	// As the PC version will have the option to always run, all spring accessors need to be handled specially to take this into account!
	bool GetPedSprintIsDown() const;
	bool GetPedSprintIsPressed() const;
	bool GetPedSprintIsReleased() const;
	bool GetPedSprintHistoryIsPressed(u32 durationMS) const;
	bool GetPedSprintHistoryHeldDown(u32 durationMS) const;
	bool GetPedSprintHistoryReleased(u32 durationMS) const;

	//! In some cases we need to query sprint input directly, so need an accessor just in case.
	const ioValue& GetPedSprint() const {return m_inputs[INPUT_SPRINT];}

	// Indicates that the player is currently using a toggle run control scheme.
	bool CanUseToggleRun() const;

	// Resets the toggle run state.
	void ClearToggleRun();
	// Explicitly set the toggle run state.
	void SetToggleRunValue(bool bVal) { m_ToggleRunOn = bVal; }

	// Have we already toggled run to on.
	bool IsToggleRunOn() const { return m_ToggleRunOn; }

	// PURPOSE: Helper function to check if toggle aim is possible.
	bool CanUseToggleAim() const;

	// As the PC version will have the option to toggle aim most press related functions need to be handled specially to take this into account!
	bool GetPedTargetIsDown(float threshold = ioValue::BUTTON_DOWN_THRESHOLD) const;
	bool GetPedTargetIsUp(float threshold = ioValue::BUTTON_DOWN_THRESHOLD) const;
	bool GetPedTargetIsReleased() const;
	float GetPedTargetNorm() const;
	float GetPedTargetLastNorm() const;
	bool GetVehiclePassengerAimDownCheckToggleAim(float threshold = ioValue::BUTTON_DOWN_THRESHOLD) const;

	const ioValue& GetPedJump() const {return m_inputs[INPUT_JUMP];}
	const ioValue& GetPedPhoneTakeOut() const {return m_inputs[INPUT_PHONE];}
	const ioValue& GetPedAccurateAim() const { return m_inputs[INPUT_ACCURATE_AIM]; }
	const ioValue& GetPedContextAction() const { return m_inputs[INPUT_CONTEXT]; }
	const ioValue& GetPedWeaponSpecial() const { return m_inputs[INPUT_WEAPON_SPECIAL]; }
	const ioValue& GetPedWeaponSpecialTwo() const { return m_inputs[INPUT_WEAPON_SPECIAL_TWO]; }
	const ioValue& GetPedDive() const {return m_inputs[INPUT_DIVE];}	

	const ioValue& GetCinematicSlowMotion() const { return m_inputs[INPUT_CINEMATIC_SLOWMO]; }
	const ioValue& GetScriptedFlyUpDown() const { return m_inputs[INPUT_SCRIPTED_FLY_UD]; }
	const ioValue& GetScriptedFlyLeftRight() const { return m_inputs[INPUT_SCRIPTED_FLY_LR]; }
	const ioValue& GetScriptedFlyVertUp() const { return m_inputs[INPUT_SCRIPTED_FLY_ZUP]; }
	const ioValue& GetScriptedFlyVertDown() const { return m_inputs[INPUT_SCRIPTED_FLY_ZDOWN]; }


	// PURPOSE:	Returns the input to select a position in the weapon wheel.
	const ioValue& GetWeaponWheelUpDown() const {return m_inputs[INPUT_WEAPON_WHEEL_UD];}

	// PURPOSE:	Returns the input to select a position in the weapon wheel.
	const ioValue& GetWeaponWheelLeftRight() const {return m_inputs[INPUT_WEAPON_WHEEL_LR];}

	const ioValue& GetRadioWheelUpDown() const		{return m_inputs[INPUT_RADIO_WHEEL_UD];}
	const ioValue& GetRadioWheelLeftRight() const	{return m_inputs[INPUT_RADIO_WHEEL_LR];}

	// PURPOSE:	Returns the input used to switch be between weapons in a specific location of the weapon wheel.
	// NOTES:	Should ONLY be used for the weapon wheel.
	const ioValue& GetWeaponWheelNext() const {return m_inputs[INPUT_WEAPON_WHEEL_NEXT];}

	// PURPOSE:	Returns the input used to switch be between weapons in a specific location of the weapon wheel.
	// NOTES:	Should ONLY be used for the weapon wheel.
	const ioValue& GetWeaponWheelPrev() const {return m_inputs[INPUT_WEAPON_WHEEL_PREV];}

	// PURPOSE:	Switches directly to the next weapon, e.g. used for mouse wheel selection.
	// NOTES:	Should NOT be used for the weapon wheel.
	const ioValue& GetPedSelectNextWeapon() const {return m_inputs[INPUT_SELECT_NEXT_WEAPON];}

	// PURPOSE:	Switches directly to the next weapon, e.g. used for mouse wheel selection.
	// NOTES:	Should NOT be used for the weapon wheel.
	const ioValue& GetPedSelectPrevWeapon() const {return m_inputs[INPUT_SELECT_PREV_WEAPON];}

	const ioValue& GetPedSpecialAbility() const {return m_inputs[INPUT_SPECIAL_ABILITY];}
	const ioValue& GetPedSpecialAbilitySecondary() const {return m_inputs[INPUT_SPECIAL_ABILITY_SECONDARY];}
	const ioValue& GetPedDuck() const {return m_inputs[INPUT_DUCK];}
	const ioValue& GetPedLookBehind() const {return m_inputs[INPUT_LOOK_BEHIND];}
	const ioValue& GetSelectWeapon() const {return m_inputs[INPUT_SELECT_WEAPON];}
	const ioValue& GetPedCollectPickup() const {return m_inputs[INPUT_PICKUP];}
	const ioValue& GetPedSniperZoom() const {return m_inputs[INPUT_SNIPER_ZOOM];}
	const ioValue& GetPedSniperZoomInSecondary() const {return m_inputs[INPUT_SNIPER_ZOOM_IN_SECONDARY];}
	const ioValue& GetPedSniperZoomOutSecondary() const {return m_inputs[INPUT_SNIPER_ZOOM_OUT_SECONDARY];}
	const ioValue& GetPedCover() const {return m_inputs[INPUT_COVER];}
	const ioValue& GetPedDuckAndCover() const {return m_inputs[INPUT_COVER];}
	const ioValue& GetPedReload() const {return m_inputs[INPUT_RELOAD];}
	const ioValue& GetPedTalk() const {return m_inputs[INPUT_TALK];}
	const ioValue& GetPedDetonate() const {return m_inputs[INPUT_DETONATE];}
	const ioValue& GetPedRappelJump() const {return m_inputs[INPUT_RAPPEL_JUMP];}
	const ioValue& GetPedRappelLongJump() const {return m_inputs[INPUT_RAPPEL_LONG_JUMP];}
	const ioValue& GetPedRappelSmashWindow() const {return m_inputs[INPUT_RAPPEL_SMASH_WINDOW];}
	const ioValue& GetHudSpecial() const {return m_inputs[INPUT_HUD_SPECIAL];}
	const ioValue& GetPedArrest() const {return m_inputs[INPUT_ARREST];}
	const ioValue& GetCharacterWheel() const {return m_inputs[INPUT_CHARACTER_WHEEL];}
	const ioValue& GetPedThrowGrenade() const {return m_inputs[INPUT_THROW_GRENADE];}
	const ioValue& GetSwitchVisor() const {return m_inputs[INPUT_SWITCH_VISOR];}
	const ioValue& GetRespawnFaster() const { return m_inputs[INPUT_RESPAWN_FASTER]; }

	// veh melee
	const ioValue& GetVehMeleeHold() const {return m_inputs[INPUT_VEH_MELEE_HOLD];}
	const ioValue& GetVehMeleeLeft() const {return m_inputs[INPUT_VEH_MELEE_LEFT];}
	const ioValue& GetVehMeleeRight() const {return m_inputs[INPUT_VEH_MELEE_RIGHT];}

	// melee commands
	const ioValue& GetMeleeAttackLight() const {return m_inputs[INPUT_MELEE_ATTACK_LIGHT];}
	const ioValue& GetMeleeAttackHeavy() const {return m_inputs[INPUT_MELEE_ATTACK_HEAVY];}
	const ioValue& GetMeleeAttackAlternate() const {return m_inputs[INPUT_MELEE_ATTACK_ALTERNATE];}
	const ioValue& GetMeleeBlock() const {return m_inputs[INPUT_MELEE_BLOCK];}

	enum eMouseSteeringMode
	{
		eMSM_Camera = 0,
		eMSM_Vehicle,
		eMSM_Off,
	};

	static eMouseSteeringMode GetMouseSteeringMode(s32 iPref);
	static bool IsMouseSteeringOn(s32 iPref);

	// vehicle commands
	void SetVehicleSteeringExclusive();
	const ioValue& GetVehicleSteeringLeftRight() const {return m_inputs[INPUT_VEH_MOVE_LR];}
	const ioValue& GetVehicleSteeringUpDown() const {return m_inputs[INPUT_VEH_MOVE_UD];}
	const ioValue& GetVehicleAccelerate() const {return m_inputs[INPUT_VEH_ACCELERATE];}
	const ioValue& GetVehicleBrake() const {return m_inputs[INPUT_VEH_BRAKE];}
	const ioValue& GetVehicleHeadlight() const {return m_inputs[INPUT_VEH_HEADLIGHT];}
	const ioValue& GetVehicleGrapplingHook() const {return m_inputs[INPUT_VEH_GRAPPLING_HOOK];}
	const ioValue& GetVehicleAim() const {return m_inputs[INPUT_VEH_AIM];}
	const ioValue& GetVehicleAttack() const {return m_inputs[INPUT_VEH_ATTACK];}
	const ioValue& GetVehicleAttack2() const {return m_inputs[INPUT_VEH_ATTACK2];}
	const ioValue& GetVehicleExit() const {return m_inputs[INPUT_VEH_EXIT];}
	const ioValue& GetVehicleHandBrake() const {return m_inputs[INPUT_VEH_HANDBRAKE];}
	const ioValue& GetVehicleDuck() const {return m_inputs[INPUT_VEH_DUCK];}
	const ioValue& GetVehicleTarget() const {return m_inputs[INPUT_AIM];}
	bool GetVehicleTargetDown(float threshold = ioValue::BUTTON_DOWN_THRESHOLD) const { return GetPedTargetIsDown(threshold); }
	const ioValue& GetVehicleGunLeftRight() const  {return m_inputs[INPUT_VEH_GUN_LR];}
	const ioValue& GetVehicleGunUpDown() const {return m_inputs[INPUT_VEH_GUN_UD];}
	const ioValue& GetVehicleLookBehind() const {return m_inputs[INPUT_VEH_LOOK_BEHIND];}
	const ioValue& GetVehicleFlyAttackCam() const {return m_inputs[INPUT_VEH_FLY_ATTACK_CAMERA];}
	const ioValue& GetVehicleCinematicCam() const {return m_inputs[INPUT_VEH_CIN_CAM];}
	const ioValue& GetVehicleSelectNextWeapon() const {return m_inputs[INPUT_VEH_SELECT_NEXT_WEAPON];}
	const ioValue& GetVehicleSelectPrevWeapon() const {return m_inputs[INPUT_VEH_SELECT_PREV_WEAPON];}  // This is for PC M&KB, as you have enough keys to cycle forward and back.
	const ioValue& GetVehicleSpecialAbilityFranklin() const {return m_inputs[INPUT_VEH_SPECIAL_ABILITY_FRANKLIN];}
	const ioValue& GetVehicleHydraulicsControlToggle() const {return m_inputs[INPUT_VEH_HYDRAULICS_CONTROL_TOGGLE]; }
	const ioValue& GetVehicleHydraulicsControlLeft() const {return m_inputs[INPUT_VEH_HYDRAULICS_CONTROL_LEFT]; }
	const ioValue& GetVehicleHydraulicsControlRight() const {return m_inputs[INPUT_VEH_HYDRAULICS_CONTROL_RIGHT]; }
	const ioValue& GetVehicleHydraulicsControlUp() const {return m_inputs[INPUT_VEH_HYDRAULICS_CONTROL_UP]; }
	const ioValue& GetVehicleHydraulicsControlDown() const {return m_inputs[INPUT_VEH_HYDRAULICS_CONTROL_DOWN]; }
	const ioValue& GetVehicleCarJump() const {return m_inputs[INPUT_VEH_CAR_JUMP]; }
	const ioValue& GetVehicleRocketBoost() const {return m_inputs[INPUT_VEH_ROCKET_BOOST]; }
	const ioValue& GetVehicleFlyBoost() const {return m_inputs[INPUT_VEH_FLY_BOOST]; }
	const ioValue& GetVehicleParachute() const {return m_inputs[INPUT_VEH_PARACHUTE]; }
	const ioValue& GetVehicleBikeWings() const {return m_inputs[INPUT_VEH_BIKE_WINGS]; }
	
	const ioValue& GetVehicleHorn() const {return m_inputs[INPUT_VEH_HORN];}
	const ioValue& GetVehicleHotwireLeft() const {return m_inputs[INPUT_VEH_HOTWIRE_LEFT];}	
	const ioValue& GetVehicleHotwireRight() const {return m_inputs[INPUT_VEH_HOTWIRE_RIGHT];}	
	const ioValue& GetVehicleNextRadioStation() const {return m_inputs[INPUT_VEH_NEXT_RADIO];}
	const ioValue& GetVehiclePrevRadioStation() const {return m_inputs[INPUT_VEH_PREV_RADIO];}
	const ioValue& GetVehicleRadioWheel() const {return m_inputs[INPUT_VEH_RADIO_WHEEL];}
	const ioValue& GetVehicleRoof() const {return m_inputs[INPUT_VEH_ROOF];}
	const ioValue& GetVehicleTransform() const {return m_inputs[INPUT_VEH_TRANSFORM];}
	const ioValue& GetVehicleJump() const {return m_inputs[INPUT_VEH_JUMP];}
	const ioValue& GetVehicleNitrous() const {return m_inputs[INPUT_VEH_HORN];}
	const ioValue& GetVehicleKERS() const {return m_inputs[INPUT_VEH_HORN];}
	const ioValue& GetVehicleShuffle() const {return m_inputs[INPUT_VEH_SHUFFLE]; }
	const ioValue& GetVehicleDropProjectile() const {return m_inputs[INPUT_VEH_DROP_PROJECTILE]; }
	const ioValue& GetVehicleMouseControlOverride() const { return m_inputs[INPUT_VEH_MOUSE_CONTROL_OVERRIDE]; }

	const ioValue& GetVehicleToggleDriveCameraControl() const {return m_inputs[INPUT_VEH_DRIVE_LOOK]; }
	const ioValue& GetVehicleToggleDriveCameraControl2() const {return m_inputs[INPUT_VEH_DRIVE_LOOK2]; }

	// Bicycle commands - Separated for keyboard support
	const ioValue& GetVehiclePushbikePedal() const {return m_inputs[INPUT_VEH_PUSHBIKE_PEDAL];}
	const ioValue& GetVehiclePushbikeFrontBrake() const {return m_inputs[INPUT_VEH_PUSHBIKE_FRONT_BRAKE];}
	const ioValue& GetVehiclePushbikeRearBrake() const {return m_inputs[INPUT_VEH_PUSHBIKE_REAR_BRAKE];}
	bool GetVehiclePushbikeSprintIsDown() const;

	// Vehicle attack query - it can be mapped to separate inputs to wrap check here.
	bool IsVehicleAttackInputDown() const;
	bool IsVehicleAttackInputReleased() const;
	float GetVehicleAttackInputNorm() const;

	// Aircraft commands - now separated from the driving controls
	void SetVehicleFlySteeringExclusive();
	const ioValue& GetVehicleFlyPitchUpDown() const {return m_inputs[INPUT_VEH_FLY_PITCH_UD];}
	const ioValue& GetVehicleFlyRollLeftRight() const {return m_inputs[INPUT_VEH_FLY_ROLL_LR];}
	const ioValue& GetVehicleFlyThrottleUp() const {return m_inputs[INPUT_VEH_FLY_THROTTLE_UP];}
	const ioValue& GetVehicleFlyThrottleDown() const {return m_inputs[INPUT_VEH_FLY_THROTTLE_DOWN];}
	const ioValue& GetVehicleFlyYawLeft() const {return m_inputs[INPUT_VEH_FLY_YAW_LEFT];}
	const ioValue& GetVehicleFlyYawRight() const {return m_inputs[INPUT_VEH_FLY_YAW_RIGHT];}
	const ioValue& GetVehicleFlyUndercarriage() const {return m_inputs[INPUT_VEH_FLY_UNDERCARRIAGE];}
	const ioValue& GetVehicleFlyAttack() const {return m_inputs[INPUT_VEH_FLY_ATTACK];}
	const ioValue& GetVehicleFlyAttack2() const {return m_inputs[INPUT_VEH_FLY_ATTACK2];}
	const ioValue& GetVehicleFlySelectNextWeapon() const {return m_inputs[INPUT_VEH_FLY_SELECT_NEXT_WEAPON];}
	const ioValue& GetVehicleFlySelectPrevWeapon() const {return m_inputs[INPUT_VEH_FLY_SELECT_PREV_WEAPON];} // This is for PC M&KB, as you have enough keys to cycle forward and back.
	const ioValue& GetVehicleFlySelectLeftTarget() const {return m_inputs[INPUT_VEH_FLY_SELECT_TARGET_LEFT];}
	const ioValue& GetVehicleFlySelectRightTarget() const {return m_inputs[INPUT_VEH_FLY_SELECT_TARGET_RIGHT];}
	const ioValue& GetVehicleFlyVerticalFlight() const {return m_inputs[INPUT_VEH_FLY_VERTICAL_FLIGHT_MODE];}
	const ioValue& GetVehicleFlyDuck() const {return m_inputs[INPUT_VEH_FLY_DUCK];}
	const ioValue& GetVehicleFlyMouseControlOverride() const { return m_inputs[INPUT_VEH_FLY_MOUSE_CONTROL_OVERRIDE]; }
	const ioValue& GetVehicleFlyBombBayDoors() const {return m_inputs[INPUT_VEH_FLY_BOMB_BAY];}
	const ioValue& GetVehicleFlyCountermeasure() const {return m_inputs[INPUT_VEH_FLY_COUNTER];}

	// Submarine commands - now separated from the driving and flying controls
	void SetVehicleSubSteeringExclusive();
	const ioValue& GetVehicleSubPitchUpDown() const {return m_inputs[INPUT_VEH_SUB_PITCH_UD];}
	const ioValue& GetVehicleSubTurnLeftRight() const {return m_inputs[INPUT_VEH_SUB_TURN_LR];}
	const ioValue& GetVehicleSubThrottleUp() const {return m_inputs[INPUT_VEH_SUB_THROTTLE_UP];}
	const ioValue& GetVehicleSubThrottleDown() const {return m_inputs[INPUT_VEH_SUB_THROTTLE_DOWN];}
	const ioValue& GetVehicleSubAscend() const {return m_inputs[INPUT_VEH_SUB_ASCEND];}
	const ioValue& GetVehicleSubDescend() const {return m_inputs[INPUT_VEH_SUB_DESCEND];}
	const ioValue& GetVehicleSubTurnHardLeft() const {return m_inputs[INPUT_VEH_SUB_TURN_HARD_LEFT];}
	const ioValue& GetVehicleSubTurnHardRight() const {return m_inputs[INPUT_VEH_SUB_TURN_HARD_RIGHT];}	
	const ioValue& GetVehicleSubMouseControlOverride() const { return m_inputs[INPUT_VEH_SUB_MOUSE_CONTROL_OVERRIDE]; }

	// Misc Vehicle
	const ioValue& GetVehiclePassengerAttack() const {return m_inputs[INPUT_VEH_PASSENGER_ATTACK];}
	const ioValue& GetVehiclePassengerAim() const {return m_inputs[INPUT_VEH_PASSENGER_AIM];}
	const ioValue& GetVehicleStuntUpDown() const {return m_inputs[INPUT_VEH_STUNT_UD];}
	const ioValue& GetVehicleCinematicUpDown() const 	{return m_inputs[INPUT_VEH_CINEMATIC_UD];}
	const ioValue& GetVehicleCinematicLeftRight() const {return m_inputs[INPUT_VEH_CINEMATIC_LR];}
	const ioValue& GetVehicleSlowMoUpDown() const 	{return m_inputs[INPUT_VEH_SLOWMO_UD];}

	const ioValue& GetVehicleArenaModeModifier() const {return m_inputs[ INPUT_VEH_MELEE_HOLD ]; }
	const ioValue& GetVehicleArenaModeShuntLeft() const {return m_inputs[ INPUT_VEH_MELEE_LEFT ]; }
	const ioValue& GetVehicleArenaModeShuntRight() const {return m_inputs[ INPUT_VEH_MELEE_RIGHT ]; }

	const ioValue& GetVehicleTombstoneDeploy() const { return m_inputs[ INPUT_VEH_ROOF ]; }
	const ioValue& GetQuadLocoReverse() const { return m_inputs[INPUT_QUAD_LOCO_REVERSE]; }

	// parachute commands
	const ioValue& GetParachuteDeploy() const { return m_inputs[INPUT_PARACHUTE_DEPLOY]; }
	const ioValue& GetParachuteDetach() const { return m_inputs[INPUT_PARACHUTE_DETACH]; }
	const ioValue& GetParachuteTurnLeftRight() const { return m_inputs[INPUT_PARACHUTE_TURN_LR]; }
	const ioValue& GetParachutePitchUpDown() const { return m_inputs[INPUT_PARACHUTE_PITCH_UD]; }
	const ioValue& GetParachuteBrakeLeft() const { return m_inputs[INPUT_PARACHUTE_BRAKE_LEFT]; }
	const ioValue& GetParachuteBrakeRight() const { return m_inputs[INPUT_PARACHUTE_BRAKE_RIGHT]; }
	const ioValue& GetParachutePrecisionLanding() const {return m_inputs[INPUT_PARACHUTE_PRECISION_LANDING]; }
	const ioValue& GetParachuteSmoke() const { return m_inputs[INPUT_PARACHUTE_SMOKE]; }

	// Cutscene skipping
	const ioValue& GetCustsceneSkip() const {return m_inputs[INPUT_SKIP_CUTSCENE];}

	// PC specific
	const ioValue& GetDisplayMap() const {return m_inputs[INPUT_MAP];} // Shortcut to display map.
	const ioValue& GetSelectWeaponUnarmed() const {return m_inputs[INPUT_SELECT_WEAPON_UNARMED];}
	const ioValue& GetSelectWeaponMelee() const {return m_inputs[INPUT_SELECT_WEAPON_MELEE];}
	const ioValue& GetSelectWeaponHandgun() const {return m_inputs[INPUT_SELECT_WEAPON_HANDGUN];}
	const ioValue& GetSelectWeaponShotgun() const {return m_inputs[INPUT_SELECT_WEAPON_SHOTGUN];}
	const ioValue& GetSelectWeaponSMG() const {return m_inputs[INPUT_SELECT_WEAPON_SMG];}
	const ioValue& GetSelectWeaponAutoRifle() const {return m_inputs[INPUT_SELECT_WEAPON_AUTO_RIFLE];}
	const ioValue& GetSelectWeaponSniper() const {return m_inputs[INPUT_SELECT_WEAPON_SNIPER];}
	const ioValue& GetSelectWeaponHeavy() const {return m_inputs[INPUT_SELECT_WEAPON_HEAVY];}
	const ioValue& GetSelectWeaponSpecial() const {return m_inputs[INPUT_SELECT_WEAPON_SPECIAL];}
	const ioValue& GetSelectCharacterMichael() const {return m_inputs[INPUT_SELECT_CHARACTER_MICHAEL];}
	const ioValue& GetSelectCharacterFranklin() const {return m_inputs[INPUT_SELECT_CHARACTER_FRANKLIN];}
	const ioValue& GetSelectCharacterTrevor() const {return m_inputs[INPUT_SELECT_CHARACTER_TREVOR];}
	const ioValue& GetSelectCharacterMultiplayer() const {return m_inputs[INPUT_SELECT_CHARACTER_MULTIPLAYER];}
	const ioValue& GetSaveReplayClip() const {return m_inputs[INPUT_SAVE_REPLAY_CLIP];}
	const ioValue& GetSpecialAbilityPC() const {return m_inputs[INPUT_SPECIAL_ABILITY_PC];} // Allows special ability on foot and on car to be the same input and a single button press on the PC.
	const ioValue& GetUserRadioNextTrack() const {return m_inputs[INPUT_VEH_NEXT_RADIO_TRACK];}
	const ioValue& GetUserRadioPrevTrack() const {return m_inputs[INPUT_VEH_PREV_RADIO_TRACK];}
#if GTA_REPLAY
	const ioValue& GetReplayDelete() const				{ return m_inputs[INPUT_REPLAY_MARKER_DELETE]; }
	const ioValue& GetReplayClipDelete() const			{ return m_inputs[INPUT_REPLAY_CLIP_DELETE]; }
	const ioValue& GetReplayPause() const				{ return m_inputs[INPUT_REPLAY_PAUSE]; }
	const ioValue& GetReplayRewind() const				{ return m_inputs[INPUT_REPLAY_REWIND]; }
	const ioValue& GetReplayFastForward() const			{ return m_inputs[INPUT_REPLAY_FFWD]; }
	const ioValue& GetReplayNewMarker() const			{ return m_inputs[INPUT_REPLAY_NEWMARKER]; }
	const ioValue& GetReplayRecord() const				{ return m_inputs[INPUT_REPLAY_RECORD]; }
    const ioValue& GetReplayScreenShot() const			{ return m_inputs[INPUT_REPLAY_SCREENSHOT]; }
    const ioValue& GetReplaySnapmatic() const			{ return m_inputs[INPUT_REPLAY_SNAPMATIC_PHOTO]; }
	const ioValue& GetReplayHideHUD() const				{ return m_inputs[INPUT_REPLAY_HIDEHUD]; }
	const ioValue& GetReplayMoveToStartPoint() const	{ return m_inputs[INPUT_REPLAY_STARTPOINT]; }
	const ioValue& GetReplayMoveToEndPoint() const		{ return m_inputs[INPUT_REPLAY_ENDPOINT]; }
	const ioValue& GetReplayAdvanceFrame() const		{ return m_inputs[INPUT_REPLAY_ADVANCE]; }
	const ioValue& GetReplayBackFrame() const			{ return m_inputs[INPUT_REPLAY_BACK]; }
	const ioValue& GetReplayTools() const				{ return m_inputs[INPUT_REPLAY_TOOLS]; }
	const ioValue& GetReplayRestart() const				{ return m_inputs[INPUT_REPLAY_RESTART]; }
	const ioValue& GetReplayShowHotkey() const			{ return m_inputs[INPUT_REPLAY_SHOWHOTKEY]; }
	const ioValue& GetReplayCycleMarkerLeft() const		{ return m_inputs[INPUT_REPLAY_CYCLEMARKERLEFT]; }
	const ioValue& GetReplayCycleMarkerRight() const	{ return m_inputs[INPUT_REPLAY_CYCLEMARKERRIGHT]; }
	const ioValue& GetReplayFovIncrease() const			{ return m_inputs[INPUT_REPLAY_FOVINCREASE]; }
	const ioValue& GetReplayFovDecrease() const			{ return m_inputs[INPUT_REPLAY_FOVDECREASE]; }
	const ioValue& GetReplayCameraUp() const			{ return m_inputs[INPUT_REPLAY_CAMERAUP]; }
	const ioValue& GetReplayCameraDown() const			{ return m_inputs[INPUT_REPLAY_CAMERADOWN]; }

	const ioValue& GetReplaySave() const				{ return m_inputs[INPUT_REPLAY_SAVE]; }
	const ioValue& GetReplayToggleTime() const			{ return m_inputs[INPUT_REPLAY_TOGGLETIME]; }
	const ioValue& GetReplayToggleTooltips() const		{ return m_inputs[INPUT_REPLAY_TOGGLETIPS]; }
	const ioValue& GetReplayPreview() const				{ return m_inputs[INPUT_REPLAY_PREVIEW]; }

	const ioValue& GetReplayToggleTimeline() const		{ return m_inputs[INPUT_REPLAY_TOGGLE_TIMELINE]; }
	const ioValue& GetReplayTimelinePickupClip() const	{ return m_inputs[INPUT_REPLAY_TIMELINE_PICKUP_CLIP]; }
	const ioValue& GetReplayTimelineDuplicateClip() const	{ return m_inputs[INPUT_REPLAY_TIMELINE_DUPLICATE_CLIP]; }
	const ioValue& GetReplayTimelinePlaceClip() const	{ return m_inputs[INPUT_REPLAY_TIMELINE_PLACE_CLIP]; }
	const ioValue& GetReplayCtrl() const				{ return m_inputs[INPUT_REPLAY_CTRL]; }
	const ioValue& GetReplayTimelineSave() const		{ return m_inputs[INPUT_REPLAY_TIMELINE_SAVE]; }
	const ioValue& GetReplayPreviewAudio() const		{ return m_inputs[INPUT_REPLAY_PREVIEW_AUDIO]; }

	
	const ioValue& GetReplayStartStopRecordingPrimary() const	{ return m_inputs[INPUT_REPLAY_START_STOP_RECORDING]; }
	const ioValue& GetReplayStartStopRecordingSecondary() const	{ return m_inputs[INPUT_REPLAY_START_STOP_RECORDING_SECONDARY]; }

#endif // GTA_REPLAY

	const ioValue& GetCellphoneCameraFocusLock() const {return m_inputs[INPUT_CELLPHONE_CAMERA_FOCUS_LOCK];}
	const ioValue& GetCellphoneCancel() const {return m_inputs[INPUT_CELLPHONE_CANCEL];}

	// direct commands (used by frontend)
	const ioValue& GetFrontendEnterExit() const {return m_inputs[INPUT_FRONTEND_PAUSE];}
	const ioValue& GetFrontendEnterExitAlternate() const {return m_inputs[INPUT_FRONTEND_PAUSE_ALTERNATE];}
	const ioValue& GetFrontendDown() const {return m_inputs[INPUT_FRONTEND_DOWN];}
	const ioValue& GetFrontendUp() const {return m_inputs[INPUT_FRONTEND_UP];}
	const ioValue& GetFrontendLeft() const {return m_inputs[INPUT_FRONTEND_LEFT];}
	const ioValue& GetFrontendRight() const {return m_inputs[INPUT_FRONTEND_RIGHT];}

	const ioValue& GetFrontendLeftRight() const {return m_inputs[INPUT_FRONTEND_AXIS_X];}
	const ioValue& GetFrontendUpDown() const {return m_inputs[INPUT_FRONTEND_AXIS_Y];}
	const ioValue& GetFrontendRStickLeftRight() const {return m_inputs[INPUT_FRONTEND_RIGHT_AXIS_X];}
	const ioValue& GetFrontendRStickUpDown() const {return m_inputs[INPUT_FRONTEND_RIGHT_AXIS_Y];}
	const ioValue& GetFrontendLS() const {return m_inputs[INPUT_FRONTEND_LS];}
	const ioValue& GetFrontendRS() const {return m_inputs[INPUT_FRONTEND_RS];}
	const ioValue& GetFrontendAccept() const {return m_inputs[INPUT_FRONTEND_ACCEPT];}
	const ioValue& GetFrontendCancel() const {return m_inputs[INPUT_FRONTEND_CANCEL];}
	const ioValue& GetFrontendX() const {return m_inputs[INPUT_FRONTEND_X];}
	const ioValue& GetFrontendY() const {return m_inputs[INPUT_FRONTEND_Y];}
	const ioValue& GetFrontendLB() const {return m_inputs[INPUT_FRONTEND_LB];}
	const ioValue& GetFrontendRB() const {return m_inputs[INPUT_FRONTEND_RB];}
	const ioValue& GetFrontendLT() const {return m_inputs[INPUT_FRONTEND_LT];}
	const ioValue& GetFrontendRT() const {return m_inputs[INPUT_FRONTEND_RT];}
	const ioValue& GetFrontendSelect() const {return m_inputs[INPUT_FRONTEND_SELECT];}
	const ioValue& GetFrontendSocialClub() const {return m_inputs[INPUT_FRONTEND_SOCIAL_CLUB];}
	const ioValue& GetFrontendSocialClubSecondary() const {return m_inputs[INPUT_FRONTEND_SOCIAL_CLUB_SECONDARY];}

	// PURPOSE:	Indicates if the cursor is currently being used.
	// NOTES:	On platforms that have a touchpad, when the user let go of the touchpad the cursor x, y pos will be 0. This 
	//			function can be used to see if the cursor's value are valid.
	bool IsUsingCursor() const;
	bool IsUsingRemotePlay() const;

	// PURPOSE: Indicates if the controls have changed. This can happen if the user changes the control layout, switches between FPS and TPS mode or switches between
	//			a controller and the keyboard mouse.
	bool ScriptCheckForControlsChange();
	
	const ioValue& GetCursorAccept() const {return m_inputs[INPUT_CURSOR_ACCEPT];}
	const ioValue& GetCursorCancel() const {return m_inputs[INPUT_CURSOR_CANCEL];}
	const ioValue& GetCursorX() const {return m_inputs[INPUT_CURSOR_X];}
	const ioValue& GetCursorY() const {return m_inputs[INPUT_CURSOR_Y];}
	const ioValue& GetEnterCheatCode() const {return m_inputs[INPUT_ENTER_CHEAT_CODE]; }

	const ioValue& GetMPTextChatAll() const { return m_inputs[INPUT_MP_TEXT_CHAT_ALL]; }
	const ioValue& GetMPTextChatTeam() const { return m_inputs[INPUT_MP_TEXT_CHAT_TEAM];}
	const ioValue& GetMPTextChatFriends() const { return m_inputs[INPUT_MP_TEXT_CHAT_FRIENDS]; }
	const ioValue& GetMPTextChatCrew() const { return m_inputs[INPUT_MP_TEXT_CHAT_CREW];}

	const ioValue& GetPushToTalk() const { return m_inputs[INPUT_PUSH_TO_TALK];}

	// Custom remappable script inputs for minigames, etc. To replace use of direct pad controls and front-end inputs
	const ioValue& GetScriptLeftAxisX() const {return m_inputs[INPUT_SCRIPT_LEFT_AXIS_X];}		// Left stick X analogue input. Values may be > 1 when using a mouse
	const ioValue& GetScriptLeftAxisY() const {return m_inputs[INPUT_SCRIPT_LEFT_AXIS_Y];}		// Left stick Y analogue input. Values may be > 1 when using a mouse
	const ioValue& GetScriptRightAxisX() const {return m_inputs[INPUT_SCRIPT_RIGHT_AXIS_X];}	// Right stick X analogue input. Values may be > 1 when using a mouse
	const ioValue& GetScriptRightAxisY() const {return m_inputs[INPUT_SCRIPT_RIGHT_AXIS_Y];}	// Right stick Y analogue input. Values may be > 1 when using a mouse
	const ioValue& GetScriptRUp() const {return m_inputs[INPUT_SCRIPT_RUP];}					// PS3 = TRIANGLE, 360 = Y 
	const ioValue& GetScriptRDown() const {return m_inputs[INPUT_SCRIPT_RDOWN];}				// PS3 = X, 360 = A           
	const ioValue& GetScriptRLeft() const {return m_inputs[INPUT_SCRIPT_RLEFT];}				// PS3 = SQUARE, 360 = X      
	const ioValue& GetScriptRRight() const {return m_inputs[INPUT_SCRIPT_RRIGHT];}				// PS3 = CIRCLE, 360 = B    
	const ioValue& GetScriptLB() const {return m_inputs[INPUT_SCRIPT_LB];}						// Left shoulder button
	const ioValue& GetScriptRB() const {return m_inputs[INPUT_SCRIPT_RB];}						// Right shoulder button
	const ioValue& GetScriptLT() const {return m_inputs[INPUT_SCRIPT_LT];}						// Left shoulder trigger
	const ioValue& GetScriptRT() const {return m_inputs[INPUT_SCRIPT_RT];}						// Right shoulder trigger
	const ioValue& GetScriptLS() const {return m_inputs[INPUT_SCRIPT_LS];}						// Left stick L3 button,           
	const ioValue& GetScriptRS() const {return m_inputs[INPUT_SCRIPT_RS];}						// Right stick R3 button           
	const ioValue& GetScriptPadLeft() const {return m_inputs[INPUT_SCRIPT_PAD_LEFT];}			// D-Pad / Directional buttons left 
	const ioValue& GetScriptPadUp() const {return m_inputs[INPUT_SCRIPT_PAD_UP];}				// D-Pad / Directional buttons up
	const ioValue& GetScriptPadDown() const {return m_inputs[INPUT_SCRIPT_PAD_DOWN];}			// D-Pad / Directional buttons down
	const ioValue& GetScriptPadRight() const {return m_inputs[INPUT_SCRIPT_PAD_RIGHT];}			// D-Pad / Directional buttons right
	const ioValue& GetScriptSelect() const {return m_inputs[INPUT_SCRIPT_SELECT];}				// Back button on 360, select on PS3
	void SetWeaponSelectExclusive();

	// PURPOSE:	Sets the value of an InputType for the next frame (not this frame).
	// PARAMS:	input - the input to set.
	//			value - the value to set to.
	void SetInputValueNextFrame(InputType input, float value);
	void SetInputValueNextFrame(InputType input, float value, ioSource source);

	// PURPOSE: retrieves the active layout type.
	// NOTES:	This will return GetTpsLayout() or GetFpsLayout() based on IsUsingFpsMode().
	eControlLayout GetActiveLayout() const;

	// commands to record whether the L1 input has been used yet.
	bool GetPedCollectPickupConsumed() {return m_pedCollectedPickupConsumed;}
	void SetPedCollectPickupConsumed(bool bNewVal = true) { m_pedCollectedPickupConsumed = bNewVal; }	


	void InitEmptyControls();


	// PURPOSE:	Gets a string (char *) representation of an ioMapperParamer;
	// PARAMS:	parameter - the parameter to convert to a string.
	// RETURNS:	A string (char *) representation of a parameter (or empty string if the parameter was not found).
	// NOTES:	A linear search is done and this can be relatively expensive so try and cash the value rather than calling
	//				multiple times.
	static const char* GetParameterText( ioMapperParameter parameter );
	
	// PURPOSE:	Gets a string (char *) representation of an ioSource;
	// PARAMS:	source - the source to convert to a string.
	// RETURNS:	A string (char *) representation of a source (or empty string if the source was not found).
	// NOTES:	A linear search is done and this can be relatively expensive so try and cash the value rather than calling
	//				multiple times.
	static const char* GetSourceText( ioMapperSource source );
	static InputType GetInputByName(const char *name);
	static const char* GetInputName(InputType input);
	static const char* GetInputGroupName(InputGroup inputGroup);
	static InputGroup GetInputGroupByName(const char *name);

	typedef atFixedArray<InputType, ControlInput::INPUT_GROUP_LIMIT> InputGroupList;
	static void GetInputsInGroup(InputGroup inputGroup, InputGroupList& inputs);
	static void GetInputsInGroup(const char* groupName, InputGroupList& inputs) { GetInputsInGroup(GetInputGroupByName(groupName), inputs); }

	int InputHowLongAgo() const;
	void SetPedWalkInverted(bool invert);
	void SetPedLookInverted(bool invert);

	bool IsUsingAnalogueAiming() const;

	// TR: TODO: Update this once better force feedback is added.
	void ShakeController(int duration, int frequency, bool isScriptCommand = false, ioRumbleEffect *pRumbleEffect = NULL);

#if LIGHT_EFFECTS_SUPPORT
	enum LightEffectType
	{
		PLAYER_LIGHT_EFFECT = 0,
		CODE_LIGHT_EFFECT,
		SCRIPT_LIGHT_EFFECT,

#if RSG_BANK
		RAG_LIGHT_EFFECT,
#endif // RSG_BANK

		MAX_LIGHT_EFFECT_TYPES,
	};
	void SetLightEffect(const ioLightEffect* effect, LightEffectType type);
	void ClearLightEffect(const ioLightEffect* effect, LightEffectType type);
	bool IsCurrentLightEffect(const ioLightEffect* effect, LightEffectType type ) const;
	void ResetLightDeviceColor();
	void SetLightEffectEnabled(bool enabled);
	bool HasLightEffectDevices() const	{ return m_ActiveLightDevices.size() > NUM_BANK_LIGHT_DEVICES; }
#endif // LIGHT_EFFECTS_SUPPORT

	bool GetOrientation(Quaternion& result);
	void ResetOrientation();

	void StartPlayerPadShake_Distance(u32 Duration, s32 Frequency, float X, float Y, float Z);
	void StartShaking(s32 MotorFrequency0, s32 MotorFrequency1);
	void StartPlayerPadShake(u32 Duration0, s32 MotorFrequency0 = 0, u32 Duration1 = 0, s32 MotorFrequency1 = 0, s32 DelayAfterThisOne = 0);

	const static s32 NO_SUPPRESS = -1;

	void SetShakeSuppressId(s32 supressId) { m_ShakeSupressId = supressId; }

#if HAS_TRIGGER_RUMBLE
	void StartTriggerShake( u32 durationLeft, u32 freqLeft, u32 durationRight, u32 freqRight, s32 DelayAfterThisOne = 0, bool isScriptedCommand = false );
#endif // #if HAS_TRIGGER_RUMBLE

	void ApplyRecoilEffect(u32 duration, float intensity, float triggerIntensity = 0);

	void StopPlayerPadShaking(bool bForce = false);
	void AllowPlayerPadShaking();
	bool IsPlayerPadShaking();
	
	// public so frontend can see it:  need to make this private again asap
	ioMapper m_onFootMap;
	ioMapper m_meleeMap;
	ioMapper m_inVehicleMap;

#if __BANK
	// Not static as there are more than one control.
	void InitWidgets(bkBank& bank, const char *title);
	void SetMappingsTest(s32 iIndex);
#endif // __BANK

	// PURPOSE:	The length of time the sprint button should be held down before we consider it held (sprinting). This is
	//			because tapping the keyboard toggles walk/run and is held to sprint.
	const static u32 KEYBOARD_START_SPRINT_HELD_TIME = 250u;

	// PURPOSE:	The mapping type.
	enum MappingType
	{
		UNKNOWN_MAPPING_TYPE,
		PC_KEYBOARD_MOUSE,
		PAD_AXIS,
		JOYSTICK
	};

	// PURPOSE: Represents a list of inputs.
	typedef atArray<InputType>					InputList;

#if KEYBOARD_MOUSE_SUPPORT
	// PURPOSE:	A list representing the category (e.g. 'On Foot') for the keyboard mapping screen.
	// NOTES:	The hash is the hash to look up the display string in the language files.
	typedef atArray<atHashWithStringNotFinal>	MappingCategoriesList;

	// PURPOSE: Retrieves a list of categories (e.g. 'On Foot') to show in the mapping screen.
	// PARAMS:	category - the list of categories to be populated.
	static void GetMappingCategories(MappingCategoriesList& categories);

	// PURPOSE:	Retrieves a list of input for a given category.
	// PARAMS:	category - the category to retrieve the list of inputs for.
	//			inputs - the list of inputs to be populated.
	static void GetCategoryInputs(const atHashWithStringNotFinal& category, InputList& categoryInputs);

	static bool IsInputMappable(InputType input)	{ return ms_settings.m_InputMappable[input]; }

	// PURPOSE:	Information defining a conflict.
	struct MappingConflict 
	{
		// PURPOSE:	The input that caused the mapping.
		InputType	m_Input;

		// PURPOSE:	The conflict mapping index so we can update the correct mapping.
		u8			m_MappingIndex;


		MappingConflict()
			: m_Input(UNDEFINED_INPUT)
			, m_MappingIndex(0u)
		{}

		MappingConflict(InputType input, u8 index)
			: m_Input(input)
			, m_MappingIndex(index)
		{}
	};

	// PURPOSE:	The amount of inputs that we can return as a conflict.
	typedef atFixedArray<MappingConflict, 10>	MappingConflictList;

	bool WasKeyboardMouseLastKnownSource() const				{ return m_LastKnownSource.m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE; }
	bool WasSteamControllerLastKnownSource() const				{ return m_LastKnownSource.m_DeviceIndex == ioSource::IOMD_GAME; }
	bool UseRelativeLook() const								{ return WasKeyboardMouseLastKnownSource() || WasSteamControllerLastKnownSource(); }
	static bool IsRelativeLookSource(const ioSource& source)	{ return source.m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE || source.m_DeviceIndex == ioSource::IOMD_GAME; }
	s32 GetLastKnownInputDeviceIndex() const					{ return m_LastKnownSource.m_DeviceIndex; }
	void ResetKeyboardMouseSettings();
	InputType GetMappingInProgress() const						{ return m_currentMapping.m_Input; }
	bool IsMappingInProgress() const							{ return GetMappingInProgress() != UNDEFINED_INPUT; }
	const MappingConflictList& GetMappingConflictList() const	{ return m_currentMapping.m_Conflict.m_Conflicts; }
	u32 GetNumberOfConflictingInputs() const					{ return m_currentMapping.m_Conflict.m_NumConflictingInputs; }
	bool HasMappingConflict() const								{ return GetNumberOfConflictingInputs() > 0u; }
	bool AreConflictingInputsMappable() const;
	bool HasNewMappingConflict() const							{ return HasMappingConflict() && m_currentMapping.m_Conflict.m_IsNewConflict; }
	void ReplaceMappingConflict()								{ m_currentMapping.m_ReplaceConflict = true; m_currentMapping.m_Conflict.m_IsNewConflict = false; }
	void UpdateLastFrameDeviceIndex();

	// PURPOSE:	Restores the default mappings.
	// NOTES:	This will delete the users settings file.
	void RestoreDefaultMappings();

	// PURPOSE:	Indicates that the user's keyboard/mouse needs updating.
	bool DoesUsersMappingFileNeedUpdating() const;

	// PURPOSE:	Fills in a list of unmapped inputs.
	// PARAMS:	unmappedInputs - the list of inputs to be filled.
	//			assertOnUnmappedInput - causes an assert whenever an unmapped input is found.
	void GetUnmappedInputs(InputList& unmappedInputs ASSERT_ONLY(, bool assertOnUnmappedInput = false)) const;

	// PURPOSE: Fills in a list of conflicting inputs (filtered for duplicates)
	// PARAMS:  conflictingInputs - the list of inputs to be filled.
	//          assertOnConflict - causes an assert whenever a conflicting input is found
	void GetConflictingInputs(InputList& conflictingInputs, bool ASSERT_ONLY(assertOnConflict) = false) const;


	// PURPOSE:	Starts a mapping scan.
	// PARAMS:	input - the InputType to map.
	//			type - the MappingType (e.g. keyboard/mouse, Joystick etc).
	//			mappingSlot - the mapping slot (0 is primary mapping, 1 is secondary etc).
	void StartInputMappingScan(InputType input, MappingType type, u8 mappingSlot);

	// PURPOSE: Cancels any mappings in progress.
	void CancelCurrentMapping();

	// PURPOSE: Removes the specified mapping if it exists.
	// PARAMS:	input - the InputType to map.
	//			type - the MappingType (e.g. keyboard/mouse, Joystick etc).
	//			mappingSlot - the mapping slot (0 is primary mapping, 1 is secondary etc).
	// NOTES:	This sets a blank mapping in the slot so a mapping still exists. This is need to keep the order!
	void DeleteInputMapping(InputType input, MappingType type, u8 mappingSlot);


	void SetToggleAimEnabled(bool enabled)			{ m_ToggleAim = enabled; m_ToggleRunOn = false; }
	bool GetToggleAimEnabled() const				{ return m_ToggleAim; }
	void SetToggleAimOn(bool isOn)			{ if(m_ToggleAim) m_ToggleAimOn = isOn;}

	void SetDriveCameraToggleOn(bool enabled) { m_DriveCameraToggleOn = enabled; }
	bool GetDriveCameraToggleOn() const { return m_DriveCameraToggleOn; }

	bool GetToggleBikeSprintOn() const { return m_ToggleBikeSprintOn; }

	void SetUseSecondaryDriveCameraToggle(bool enabled) { m_UseSecondaryDriveCameraToggle = enabled; }

	// PURPOSE: Display information about a keyboard key.
	struct KeyInfo
	{
		// PURPOSE:	Icon number representing an invalid icon.
		const static u32 INVALID_ICON = 0xFFFFFFFF;

		// PURPOSE:	Icon number representing use the string in m_Text with a small icon.
		const static u32 TEXT_ICON = 0;

		// PURPOSE:	Icon number representing use the string in m_Text with a large icon.
		const static u32 LARGE_TEXT_ICON = 1;

		// PURPOSE:	The icon number (or INVALID_ICON or TEXT_ICON).
		u32 m_Icon;

		// PURPOSE: If m_Icon is TEXT_ICON, this will be the text to be displayed over a blank icon.
		char m_Text[ControlInput::Keyboard::KEY_INFO_TEXT_LENGTH];
	};

#if DEBUG_DRAW
	// PURPOSE:	Retrieves the on foot mouse sensitivity.
	void DebugDrawMouseInput();
#endif // DEBUG_DRAW
#endif // KEYBOARD_MOUSE_SUPPORT
	static MappingType GetMappingType(ioMapperSource source);

#if USE_STEAM_CONTROLLER
	void CheckForSteamController();
#endif // USE_STEAM_CONTROLLER

#if __WIN32PC
	// PURPOSE: Sets the enable flag for using DirectInput devices.
	void SetEnableDirectInputDevices(bool useDirectInputDevices) { m_UseExtraDevices = useDirectInputDevices; }

	 void ApplyDirectionalForce(float X, float Y, s32 timeMS);

	// PURPOSE:	Saves the user PC mappings.
	// PARAMS:	isAutoSave - if true the settings will be saved to a temporary backup file.
	//			deviceId - the id of the device to save.
	// RETURNS:	true if the controls successfully saved.
	bool SaveUserControls(bool isAutoSave = false, s32 deviceId = ioSource::IOMD_KEYBOARD_MOUSE);

	// PURPOSE: Indicates if the user has an autosaved settings file.
	// PARAMS:	deviceId - the id of the device to save.
	bool HasAutoSavedUserControls(s32 deviceId = ioSource::IOMD_KEYBOARD_MOUSE) const;

	// PURPOSE: Deletes the users auto saved settings file.
	// PARAMS:	deviceId - the id of the device to save.
	bool DeleteAutoSavedUserControls(s32 deviceId = ioSource::IOMD_KEYBOARD_MOUSE) const;

	// PURPOSE: Loads PC Script control mappings.
	// PARAMS:	mappingScheme - the mapping scheme to be loaded.
	// RETURNS:	true if the scheme was loaded (or already loaded).
	bool LoadPCScriptControlMappings(const atHashString& mappingSceme);

	// PURPOSE: Switches PC Script control mappings.
	// PARAMS:	mappingScheme - the mapping scheme to be loaded.
	// RETURNS:	true if the scheme was loaded (or already loaded).
	bool SwitchPCScriptControlMappings(const atHashString& mappingSceme);

	// PURPOSE: Loads the Video Editor scripted controls scheme.
	// RETURNS:	true if the scheme was loaded (or already loaded).
	bool LoadVideoEditorScriptControlMappings();

	// PURPOSE: Unloads the Video Editor scripted controls scheme.
	// RETURNS:	true if the scheme was unloaded (or already unloaded).
	bool UnloadVideoEditorScriptControlMappings();

	// PURPOSE: Shuts down the PC Script control mappings.
	void ShutdownPCScriptControlMappings();

	// PURPOSE: Retrieves the key info for a given keycode.
	static const KeyInfo& GetKeyInfo(u32 keyCode);

#if !RSG_FINAL
	// PURPOSE: Emulates XInput gamepads from DirectInput gamepads.
	// NOTES: Hack to get debug camera working with PS4 pads for working from home. This is in this file as it is CControl that has information about the gamepads.
	static void EmulateXInputPads();
#endif // !RSG_FINAL

	// PURPOSE: Enum listing all the sources to be scanned for when calibrating a Joystick as a gamepad.
	enum GamepadDefinitionSource
	{
		INVALID_PAD_SOURCE = -1,
		PAD_RDOWN,
		PAD_RRIGHT,
		PAD_RLEFT,
		PAD_RUP,
		PAD_L1,
		PAD_L2,
		PAD_R1,
		PAD_R2,
		PAD_START,
		PAD_SELECT,
		PAD_LUP,
		PAD_LDOWN,
		PAD_LLEFT,   
		PAD_LRIGHT, 
		PAD_L3,
		PAD_AXIS_LY_UP,
		PAD_AXIS_LX_RIGHT,
		PAD_R3,
		PAD_AXIS_RY_UP,
		PAD_AXIS_RX_RIGHT,

		NUM_PAD_SOURCES
	};

	// PURPOSE:	Starts a new gamepad definition scan.
	void StartGamepadDefinitionScan();

	// PURPOSE: Ends the gamepad definition scan.
	// PARAMS:	saveGamepadDefinition - true to save the definition (false to cancel).
	void EndGamepadDefinitionScan(bool saveGamepadDefinition);

	// PURPOSE:	Scans a Joystick (DirectInput) device to create a gamepad definition.
	// PARAMS:	source - the source to scan.
	void ScanGamepadDefinitionSource(GamepadDefinitionSource source);

	// PURPOSE: Clears a Joystick (DirectInput) to gamepad mapping.
	// PARMAMS:	source - the source to clear.
	// RETURNS:	true if the source was cleared.
	bool ClearGamepadDefinitionSource(GamepadDefinitionSource source);

	// PURPOSE:	Retrieves the conflicting gamepad definition source (if any) when the user tries to map an already mapped source..
	// RETURNS:	If the user tries to map a gamepad definition source that is already mapped then the original source is returned otherwise INVALID_PAD_SOURCE is.
	GamepadDefinitionSource GetConflictingDefinitionSource() const	{ return m_CurrentJoystickDefinition.m_ConflictingSource; }

	// PURPOSE:	Clears any conflicting gamepad definition sources.
	void ClearConflictingDefinitionSource()							{ m_CurrentJoystickDefinition.m_ConflictingSource = INVALID_PAD_SOURCE; }

	bool IsGamepadDefinitionSourceConflicting() const				{ return GetConflictingDefinitionSource() != INVALID_PAD_SOURCE; }

	// PURPOSE: Cancels the gamepad definition scan.
	// NOTES:	This WILL cancel all previous source scans as well. If you only want to cancel a scan
	//			for a specific source then CancelGampadDefinitionSourceScan(). 
	void CancelGampadDefinitionScan();

	// PURPOSE:	Cancels a running gamepad definition scan for a source.
	void CancelGampadDefinitionSourceScan();

	// PURPOSE: Indicates a gamepad definition scan is in progress.
	bool IsGampadDefinitionScanInProgress() const;

	// PURPOSE: Retrieves the id of the device currently being scanned.
	s32 GetScanningDeviceIndex() const			{ return m_CurrentJoystickDefinition.m_DeviceIndex; }

	// PURPOSE: Sets the id of the device to be scanned.
	void SetScanningDeviceIndex(s32 index)		{ m_CurrentJoystickDefinition.m_DeviceIndex = index; }

	// PURPOSE:	Calibrates a gamepad.
	void CalibrateGamepad(s32 index)			{ ioJoystick::AutoCalibrate(index, true); }

	// PURPOSE: Checks if an index is a valid non XInput gamepad device.
	// PARAMS:	deviceId - the device id to check.
	// NOTES:	This will return false if the device id is an XInput device.
	bool IsValidJoystickDevice(s32 deviceId) const;

#if __BANK
	// Mapping controls are only available on PC version and in RAG.
	void InitMappingWidgets(bkBank& bank);
	void InitJoystickCalibrationWidgets(bkBank& bank);

	// PURPOSE: Saves all the changed user controls.
	void SaveAllUserControls();
#endif // __BANK

#if !__FINAL
	// set what values are returned for simulated debug pad on pc:
	const ioValue& GetDebugLeftX() const {return GetPedWalkLeftRight();}
	const ioValue& GetDebugLeftY() const {return GetPedWalkUpDown();}
	const ioValue& GetDebugRightX() const {return GetPedLookLeftRight();}
	const ioValue& GetDebugRightY() const {return GetPedLookUpDown();}
#endif // !__FINAL

#endif // __WIN32PC

	// PURPOSE:	Sets the thread id of main thread used to update the controls.
	// PARAMS:	threadId - the id of the thread.
	// NOTES:	This is used to thread other than the main control update thread from altering mappings during updating.
	static void SetControlUpdateThread(sysIpcCurrentThreadId threadId)	{ ms_threadId = threadId; }

	// PURPOSE: Returns true if inputs were flagged to be disabled on the previous frame but are no longer actively being disabled (inputs only get disabled one frame after we flag them to be disabled).
	// Used for CTaskCover::CanUseThirdPersonCoverInFirstPerson (see url:bugstar:2079670/url:bugstar:2231880).
	const bool GetAreInputsStillDisabled() const { return m_bInputsStillDisabled; }

private:
	// PURPOSE:	Returns true if the calling thread is the main controls update thread.
	static bool IsControlUpdateThread() { return sysIpcGetCurrentThreadId() == ms_threadId; }
	static sysIpcCurrentThreadId ms_threadId;


	// PURPOSE: reads a settings file.
	// PARAMS:	settingsFile - the file path to load.
	//			eradSafeButSlow - if true, extra error checking will be used whilst loading the settings file.
	//			controlSettings - the settings structure to load the controls into.
	// NOTES: readSafeButSlow is used when loading a settings file that the user
	bool LoadSettingsFile(const char* settingsFile, bool readSafeButSlow, ControlInput::ControlSettings& controlSettings);

	// PURPOSE: reads a settings file and if it succeeds it will unload any inputs already mapped and call LoadSettings.
	// PARAMS:	settingsFile - the file path to load.
	//			deviceId - the deviceId to use. This should be valid pad/device id or an ioSource.
	//			eradSafeButSlow - if true, extra error checking will be used whilst loading the settings file.
	// NOTES: readSafeButSlow is used when loading a settings file that the user
	bool LoadOverrideSettingsFile(const char* settingsFile, s32 deviceId, bool readSafeButSlow);

	// PURPOSE: Loads the scripted settings.
	// PARAMS:	loadStandardControls - true to load the standard controls, false to load the users controls layout.
	// PARAMS:	forceLoad - true to force the controls to be loaded.
	void LoadScriptedSettings(bool loadStandardControls, bool forceLoad);

	// PURPOSE: Loads PC Script control mappings.
	// RETURNS:	true if the scheme was loaded (or already loaded).
	// NOTES:	calling code must make sure no thread m_Cs is locked!
	bool LoadDynamicControlMappings();


	// PURPOSE:	Loads the scripted control settings so we can quickly swap between the two layouts.
	void LoadScriptedMappings();

	// PURPOSE:	Retrieves the current base gamepad layout file.
	// PARAMS:	layout - the layout to load (e.g. southpaw fps).
	// NOTES:	A layout is made up of common mappings, base mappings (e.g. common to a layout such as southpaw) and
	//			specific mappings (e.g. refinements for FPS or TPS southpaw).
	const char* GetGamepadBaseLayoutFile(eControlLayout layout);

	// PURPOSE: Retrieves the specific layout file.
	// PARAMS:	layout - the layout to load (e.g. southpaw fps).
	// NOTES:	A layout is made up of common mappings, base mappings (e.g. common to a layout such as southpaw) and
	//			specific mappings (e.g. refinements for FPS or TPS southpaw).
	const char* GetGamepadSpecificLayoutFile(eControlLayout layout);

	// PURPOSE: Converts a layout to the relevant scripted layout.
	eControlLayout ConvertToScriptedLayout(eControlLayout layout) const;

	// PURPOSE: retrieves the third person shooter scripted layout file.
	// PARAMS:	layout - the layout to load (e.g. southpaw fps).
	const char* GetScriptedInputLayoutFile(eControlLayout layout);


	// PURPOSE: retrieves the current select/cancel layout file.
	static const char* GetAcceptCancelLayoutFile();

	// PURPOSE: retrieves the current duck/handbrake layout file
	static const char* GetDuckHandbrakeLayoutFile();

#if FPS_MODE_SUPPORTED
	// PURPOSE:	indicates if we are using FPS mode.
	bool IsUsingFpsMode() const;

	// PURPOSE: retrieves the first person shooter layout type.
	eControlLayout GetFpsLayout() const;
#endif // FPS_MODE_SUPPORTED

	// PURPOSE: retrieves the third person shooter layout type.
	eControlLayout GetTpsLayout() const;

	void LoadSettings(const ControlInput::ControlSettings &settings, s32 deviceId);
	void ReplaceSettings(const ControlInput::ControlSettings &settings, s32 deviceId);

	CPad& GetPad(); // Dont make this public! its well dodgy given not all controls have a pad - PH

	void DoDisableInput(unsigned input, const ioValue::DisableOptions& options, bool disableRelatedInputs); 
	void DoEnableInput(unsigned input, bool enableRelatedInputs);

	void SetInputOccurred();
	void SupportIoHistory(s32 id) {m_inputs[id].SupportHistory();}

	bool IsInputDisabled(const ioValue &value) const;

	void UpdateInputMappers();

	// PURPOSE:	Updates the inputs that are mapped to system input events.
	void UpdateInputFromSystemEvent(InputType input, u32& timer);

	// PRUPOSE: Used internally for disabling all inputs tied to any source in the source list that the player is using,
	//          e.g. if the player is using keyboard/mouse then the keyboard/mouse sources will be disabled, otherwise the
	//          gamepad sources will be disabled.
	void DisableInputsByActiveSources(const ioMapper::ioSourceList& sources, const ioValue::DisableOptions& options);

	// Used internally for disabling all inputs tied to a particular source.
	void DisableInputsBySource(const ioSource& source, const ioValue::DisableOptions& options);

	// Debug helper function to aid outputting the current scripts call stack.
	static void DebugOutputScriptCallStack();

	// Helper function to disable all the back button inptus.
	void DisableBackButtonInputs();

#if LIGHT_EFFECTS_SUPPORT
	void UpdateLightEffects();

	// PURPOSE:	Sets the light effect color to default color.
	// NOTES:	Does not clear any light effects, these will continue to play.
	void SetLightEffectsToDefault();
#endif // LIGHT_EFFECTS_SUPPORT

#if !__FINAL
	static atString ms_ThreadlStack;
	static sysCriticalSectionToken ms_Lock;

	// borrowed from Debug.cpp::DumpThreadState
	static void ChanneledOutputForStackTrace(const char* fmt, ...);

#if RSG_PC
	// Hack for using PS4 controller whilst working from home.
	static float GetJoystickValue(const ioJoystick& stick, ioMapperParameter param);
	static void SetPadValue(int padIndex, ioMapperParameter param, float value);
#endif // RSG_PC
#endif // !__FINAL

	// settings are static to save memory (and processing expense).
	struct Settings
	{
		typedef atFixedArray<InputType, ControlInput::INPUT_GROUP_LIMIT> InputGroupDefinition;
		typedef atArray<ControlInput::AxisDefinition, 0, u16>            AxisDefinitionList;
		typedef atArray<ControlInput::RelatedInputs>					 RelatedInputs;

		AxisDefinitionList   m_AxisDefinitions;
		InputList            m_HistorySupport;
		atRangeArray<InputGroupDefinition, MAX_INPUTGROUPS> m_InputGroupDefinitions;
		atRangeArray<Mapper, MAX_INPUTS>					m_InputMappers;
		RelatedInputs		 								m_RelatedInputs;
		bool                 m_NeedsInitializing;

#if KEYBOARD_MOUSE_SUPPORT
		ControlInput::MappingSettings	 m_MappingSettings;
		// Dynamic mapping settings.
		ControlInput::DynamicMappingList m_DynamicMappingList;

		// Information about the keyboard keys.
		atFixedArray<KeyInfo, ControlInput::Keyboard::MAX_NUM_KEYS> m_KeyInfo;

		// The categories an input belong to.
		atRangeArray<u32, MAX_INPUTS>	m_InputCategories;

		// PURPOSE:	The mouse settings.
		ControlInput::MouseSettings		m_MouseSettings;

		// The list of inputs that the user cannot map.
		atRangeArray<bool, MAX_INPUTS>	m_InputMappable;

		// Loads the keyboard layout.
		void LoadKeyboardLayout();

		// Loads a keyboard layout file.
		void LoadKeyboardLayoutFile(const char* layoutFile);

		typedef atArray< atFixedString<RAGE_MAX_PATH> > EnumFileList;

		// Enumerate the keyboard layout files.
		static void EnumKeyboardLayoutFilesCallback(const fiFindData& data, void* userArg);
#endif // KEYBOARD_MOUSE_SUPPORT

#if RSG_PC
		// PURPOSE: The gamepad definition list for DirectInput devices.
		ControlInput::Gamepad::DefinitionList m_GamepadDefinitionList;

		// PURPOSE: Reloads/rebuilds the gamepad definition list.
		void RebuildGamepadDefinitionList();

		// PURPOSE: Retrieves a gamepad definition for a joystick.
		// PARMAS:	stick - the stick to retrieve the gamepad definition for.
		// RETURNS:	The gamepad definition or the default will be returned, failing that, NULL is returned.
		const ControlInput::Gamepad::Definition* GetGamepadDefinition(const ioJoystick &stick) const;

		// PURPOSE: Retrieves a gamepad definition for a joystick only if its valid.
		// PARMAS:	stick - the stick to retrieve the gamepad definition for.
		// RETURNS:	The gamepad definition or the default will be returned, failing that, NULL is returned.
		const ControlInput::Gamepad::Definition* GetValidGamepadDefinition(const ioJoystick &stick) const;
#endif // RSG_PC

		void Init();
		Settings();

		// used for sorting inputs by value
		static int InputCompare(const InputType* a, const InputType* b);
	};

	mutable sysCriticalSectionToken m_Cs;

	static Settings ms_settings;

	
	// The default scripted controls layout, this is the same for all control objects.
	static ControlInput::ControlSettings ms_StandardScriptedMappings;

	// The user common settings file.
	ControlInput::ControlSettings m_GamepadCommonSettings;

	// The user base layout settings file for Third Person Shooter mode.
	ControlInput::ControlSettings m_GamepadTpsBaseLayoutSettings;

	// The user third person shooter specific layout settings file.
	ControlInput::ControlSettings m_GamepadTpsSpecificLayoutSettings;

	// The user scripted third person shooter controls layout (based on controller layout).
	ControlInput::ControlSettings m_AlternateTpsScriptedSettings;

	// The user third person layout type.
	eControlLayout m_TpsControlLayout;

	// The usrs handbrake control setting.
	s32 m_HandbrakeControl;

#if FPS_MODE_SUPPORTED
	// The user base layout settings file for First Person Shooter mode.
	ControlInput::ControlSettings m_GamepadFpsBaseLayoutSettings;

	// The user first person shooter specific layout settings file.
	ControlInput::ControlSettings m_GamepadFpsSpecificLayoutSettings;

	// The user scripted first person shooter controls layout (based on controller layout).
	ControlInput::ControlSettings m_AlternateFpsScriptedSettings;

	// The user first person layout type.
	eControlLayout m_FpsControlLayout;
#endif // FPS_MODE_SUPPORTED

	// The user layout settings file.
	ControlInput::ControlSettings m_GamepadAcceptCancelSettings;

	// The user layout settings file.
	ControlInput::ControlSettings m_GamepadDuckBrakeSettings;

#if RSG_ORBIS
	// The user touchpad settings file.
	ControlInput::ControlSettings m_GamepadGestureSettings;
#endif // RSG_ORBIS

#if KEYBOARD_MOUSE_SUPPORT
	// The default keyboard/mouse settings file.
	ControlInput::ControlSettings m_DefaultKeyboardMouseSettings;

	// The user's keyboard/mouse settings file.
	ControlInput::ControlSettings m_UserKeyboardMouseSettings;

	// The keyboard/mouse fixed settings file.
	ControlInput::ControlSettings m_KeyboardMouseFixedSettings;

	// The mouse look settings file.
	ControlInput::ControlSettings m_MouseLookSettings;

	// The mouse car settings file.
	ControlInput::ControlSettings m_MouseCarSettings;

	// The mouse fly settings file.
	ControlInput::ControlSettings m_MouseFlySettings;

	// The mouse sub settings file.
	ControlInput::ControlSettings m_MouseSubSettings;


	// Mouse Settings flag.
	enum MouseSettingsFlags
	{
		// NOTE: MOUSE_INVERTED does *NOT* indicate that the mouse is inverted but
		// indicates that that mouse inverted settings are loaded. This can happen
		// if mouse is not inverted but the gamepad is!
		MOUSE_INVERTED				= BIT(0),
		MOUSE_VEHICLE_CAR			= BIT(1),
		MOUSE_VEHICLE_FLY			= BIT(2),
		MOUSE_VEHICLE_FLY_INVERTED	= BIT(3),
		MOUSE_VEHICLE_SUB			= BIT(4),
		MOUSE_VEHICLE_SUB_INVERTED	= BIT(5),
		MOUSE_CAR_AUTO_CENTER		= BIT(6),
		MOUSE_FLY_AUTO_CENTER		= BIT(7),
		MOUSE_SUB_AUTO_CENTER		= BIT(8),
		MOUSE_FLY_SWAP_XY			= BIT(9)
	};

	// Previous mouse settings flags.
	u32 m_MouseSettings;

	// PURPOSE: Indicates that device used last frame.
	s32 m_LastFrameDeviceIndex;

	// PURPOSE: Indicates that this is the first control load.
	bool m_HasLoadedDefaultControls;

	// Force to use mouse drive for cars, planes, and submarines.
	bool m_DriveCameraToggleOn;

	// If using secondary toggle drive camera control.
	bool m_UseSecondaryDriveCameraToggle;

#endif // KEYBOARD_MOUSE_SUPPORT

	// PURPOSE: The game's inputs.
	ioValue m_inputs[MAX_INPUTS];

	// PURPOSE: Indicates if an input has been mapped.
	bool m_isInputMapped[MAX_INPUTS];
	
	// TODO: Remove this once script pad access has been removed.
	bool	m_pedCollectedPickupConsumed;		// Used to keep track of whether the input has been used yet. Used to make sure L1 doesn't trigger 5 things simultaneously.

	// The scripted inputs mimic the controller layout, however, if the user does not use standard (northpaw) controls then scripts can make the controls mimic the alternate
	// controls rather than standard layouts (for example, if the scripts are reading sticks for look and movement but the user has swapped the sticks [southpaw], then they can
	// make the scripted inputs swap as well). By default this is not enabled. When enabled it is enabled for the remainder of this frame and the whole of the next frame.
	bool    m_useAlternateScriptedControlsNextFrame;

	// The currently loaded layout for scripted inputs;
	s32 m_scriptedInputLayout;

	s32	m_padNum;

	static s32 ms_LastTimeTouched; // there's only One 'user controller', so share these across all control schemes

	ioMapper m_Mappers[MAPPERS_MAX];

	// The time duration to disable inputs for.
	u32 m_InputDisableEndTime;

	// The disable options to use when disabling an input over time.
	ioValue::DisableOptions m_DisableOptions;

	// Returns true if inputs were flagged to be disabled on the previous frame but we are no longer actively disabling the input.
	bool m_bInputsStillDisabled;
	
	// Temporary quck fix for url:bugstar:1223053. Proper fix shelved in CL 3951678 for after lockdown.
	u32 m_BackButtonEnableTime;

	// Pad shake suppress id.
	s32 m_ShakeSupressId;

	// PURPOSE:	Indicates when the system sent a back event to the frontend menus.
	// NOTES:	We use this to fire the relevant input (ioValue) so that the game responds appropriately.
	u32 m_SystemFrontendBackOccurredTime;

	// PURPOSE:	Indicates when the system sent a back event to the phone.
	// NOTES:	We use this to fire the relevant input (ioValue) so that the game responds appropriately.
	u32 m_SystemPhoneBackOccurredTime;

	// PURPOSE:	Indicates when the system sent a play or pause event.
	// NOTES:	We use this to fire the relevant input (ioValue) so that the game responds appropriately.
	u32 m_SystemPlayPauseOccurredTime;

	// PURPOSE:	Indicates when the system sent a view(select) event.
	// NOTES:	We use this to fire the relevant frontend input (ioValue) so that the game responds appropriately.
	u32 m_SystemFrontendViewOccuredTime;

	// PURPOSE:	Indicates when the system sent a view(select) event.
	// NOTES:	We use this to fire the relevant game input (ioValue) so that the game responds appropriately.
	u32 m_SystemGameViewOccuredTime;

	// PURPOSE:	The length of time (ms) that we ignore a system event (e.g. pause) as the player will not have control.
	static dev_u32 SYSTEM_EVENT_TIMOUT_DURATION;

	// PURPOSE:	Callback delegate to handle system events.
	ServiceDelegate m_Delegate;

	// PURPOSE: Delegate callback function to handle system events.
	void HandleSystemEvent(sysServiceEvent* evt);

#if USE_ACTUATOR_EFFECTS
	fwRecoilEffect m_Recoil;
	ioRumbleEffect m_Rumble;

#if HAS_TRIGGER_RUMBLE
	ioTriggerRumbleEffect m_TriggerRumble;
#endif // #if HAS_TRIGGER_RUMBLE

#endif // USE_ACTUATOR_EFFECTS


#if LIGHT_EFFECTS_SUPPORT
	struct LightEffectData
	{
		LightEffectData();

		const ioLightEffect* m_Effect;
		u32					 m_StartTime;
	};

	atRangeArray<LightEffectData, MAX_LIGHT_EFFECT_TYPES> m_LightEffects;

#if RSG_ORBIS
	ioPadLightDevice m_PadLightDevice;
	const static u32 NUM_LIGHT_DEVICES = 1;
#elif RSG_PC
	ioLogitechLedDevice m_LogitechLightDevice;
	const static u32 NUM_LIGHT_DEVICES = 1;
#endif // RSG_PC

	bool m_LightEffectsEnabled;

#if RSG_BANK
	bool m_RagLightEffectEnabled;
	Color32 m_RagLightEffectColor;
	ioConstantLightEffect m_RagLightEffect;

	// PURPOSE: A debug light device so that we can update the light effect colour in rag to detect
	//			discrepancies between the color set and the color shown.
	class DebugLightDevice : public ioLightDevice
	{
	public:
		DebugLightDevice();

		virtual bool IsValid() const;
		virtual bool HasLightSource(Source source) const;
		virtual void SetLightColor(Source source, u8 red, u8 green, u8 blue);
		virtual void SetAllLightsToDefaultColor();
		virtual void Update();

		Color32 m_Color;
	};

	DebugLightDevice m_RagDebugLightDevice;

	const static u32 NUM_BANK_LIGHT_DEVICES = 1;
#else
	const static u32 NUM_BANK_LIGHT_DEVICES = 0;
#endif // RSG_BANK


	atFixedArray<ioLightDevice*, NUM_LIGHT_DEVICES + NUM_BANK_LIGHT_DEVICES> m_ActiveLightDevices;

	void UpdateRagLightEffect();
#endif // LIGHT_EFFECTS_SUPPORT

#if RSG_ORBIS
	// PURPOSE:	Keeps track if the remote player controls are loaded for this player.
	bool m_IsRemotePlayer;

	// PURPOSE: Indicates that the X/O state has changed.
	// NOTES:	This should be constant during load but we are reading it before we the setting has been initialized. This way we update if it ever changes.
	s32 m_IsAcceptCrossSetting;
#endif // RSG_ORBIS

	enum UpdateType {ADD_UPDATE, REMOVE};


#if KEYBOARD_MOUSE_SUPPORT
	// PURPOSE:	Mapping conflict information.
	struct MappingConflictData
	{
		// PURPOSE:	The input that is conflicting.
		MappingConflictList	m_Conflicts;

		// PURPOSE:	The actual number of conflicting inputs.
		u32					m_NumConflictingInputs;

		// PURPOSE:	Indicates that the conflict is a new conflict that has not been dealt with.
		bool				m_IsNewConflict;

		MappingConflictData();

		void Clear();
	};

	// groups all input mapping data into one structure.
	struct CurrentMappingData
	{
		// Keeps track of what is being mapped
		MappingType	m_Type;
		InputType	m_Input;
		u8			m_MappingIndex;
		ioMapper*	m_ScanMapper;


		// Keep track of source of the current mapping.
		ioSource	m_Source;

		// Keep track of any conflicts with this mapping.
		MappingConflictData m_Conflict;

		// What to do with a conflict.
		bool		m_CancelMapping;
		bool		m_ReplaceConflict;

#if RSG_BANK
		// Keep track of the source of the mapping. Rag does not have the UI
		// features of the front end so it will automatically replace conflicts.
		bool		m_MappingCameFromRag;
#endif // RSG_BANK

		CurrentMappingData();

		void Clear();
	};

	// The current mapping information.
	CurrentMappingData m_currentMapping;

#if RSG_PC
	// The current gamepad definition settings.
	struct JoystickDefinitionData
	{
		enum { INVALID_INDEX = -1 };
		ControlInput::Gamepad::Definition m_Data;

		// The GUID of the device.
		atFinalHashString m_Guid;

		// scan options for the pad.
		ioMapper::ScanOptions m_ScanOptions;

		// We use this to know which pad button to search form. We could have
		// used the index which is faster but then a definition could be in an
		// unknown state if something goes wrong with the scan.
		ioMapperParameter m_PadParameter;

		// Used to keep track of any conflicts.
		GamepadDefinitionSource m_ConflictingSource;

		// The index id of the device.
		s32 m_DeviceIndex;

		// Indicates the scan has started.
		bool m_ScanStarted;

		// Indicates that the scan is ready (we ensure no buttons are pressed before we scan).
		bool m_ScanReady;

		// Indicates the device needs calibrating.
		bool m_Calibrate;

		// Checks if a pad parameter will be auto-mapped.
		static bool IsAutoMapped(ioMapperParameter param);
		
		// PURPOSE:	Retrieves auto mapping parameters.
		// PARAMS:	parameter - the parameter base the auto-mappings from (based on the negative direction of an axis. e.g. Left\Up).
		//			opposite - the opposite parameter to automap based on negative.
		//			fullAxis - the fullAxis parameter to automap based on negative.
		// RETURNS:	true if mappings could be retrieved.
		static bool GetAutoMappingParameters(const ioMapperParameter& parameter, ioMapperParameter& opposite, ioMapperParameter& fullAxis);

		// PURPOSE:	Retrieves auto mappings based on a negativeSource;
		// PARAMS:	source - the source base the auto-mappings from (based on the negative direction of an axis. e.g. Left\Up).
		//			opposite - the mapping to the opposite direction to retrieve.
		//			fullAxisMapping - the mapping for the full axis to retrieve.
		//			isPositive - true when mapping in a positive axis direction.
		// RETURNS:	true if mappings could be retrieved.
		// NOTES:	isPositive can be true even when the source is negative, this means the axis will be mapped inverted.
		static bool GetAutoMappings(const ioSource& source, ioSource& opposite, ioSource& fullAxisMapping, bool isPositive);

		// PURPOSE:	Returns an alternative for a gamepad button.
		// PARAMS:	button - the button to get an alternative name for.
		// RETURNS:	the alternative name for a gamepad button or IOM_UNDEFINED if there is no alternative name.
		static ioMapperParameter GetAlternateButtonMappingParameters(ioMapperParameter button);

		// PURPOSE:	Converts a gamepad (360/ps3) mapping to a compatible joystick mapping.
		// PARAMS:	configuration - the joystick configuration.
		//			padMapping - the gamepad mappings.
		//			joystickMappings - the converted joystick mappings (get populated).
		static void ConvertPadMappingsToJoystickMappings( const ControlInput::Gamepad::Definition& configuration,
														  const ControlInput::ControlSettings& padMappings,
														  ControlInput::ControlSettings& joystickMappings );

		JoystickDefinitionData();
	};
	
	JoystickDefinitionData m_CurrentJoystickDefinition;
	
	// PURPOSE: Updates a gamepad definition.
	// PARAMS:	definition - the source to add/update.
	void UpdateCurrentGamepadDefinition( const ControlInput::Gamepad::Source& definition );

	// PURPOSE:	Retrieves the current gamepad definition that already exists in the current mapping.
	// PARAMS:	definition - the source to check.
	// RETURNS:	The GamepadDefinitionSource already mapped to the source or INVALID_PAD_SOURCE if it is not mapped.
	GamepadDefinitionSource GetCurrentGamepadDefinition( const ControlInput::Gamepad::Source& definition ) const;

	// PURPOSE: Checks if a gamepad definition is valid.
	// PARAMS:	definition - the source to check.
	// RETURNS:	true if the definition is valid.
	bool IsGamepadDefinitionValid( const  ControlInput::Gamepad::Source& definition ) const;

	static GamepadDefinitionSource ConvertParameterToGampadInput(ioMapperParameter param);
	static ioMapperParameter ConvertGamepadInputToParameter(GamepadDefinitionSource input);

	// PURPOSE: Lookup table of GamepadDefinitionSource id's to ioMapperParameter ids.
	const static ioMapperParameter ms_GAMEPAD_DEFINITON_TO_MAPPER_PARAM[];

	// Indicates the mouse controls the vehicles.
	// TR TODO: Tie these to the pause menu.
	bool m_MouseVehicleCarAutoCenter;
	bool m_MouseVehicleFlyAutoCenter;
	bool m_MouseVehicleSubAutoCenter;

#endif // RSG_PC

#if USE_STEAM_CONTROLLER
	// PURPOSE:	Update function to update inputs from the steam controller.
	void UpdateSteamController();
	
	// PURPOSE:	Update an input from a steam source.
	// PARAMS:	input - The input to be updated.
	//			value - The value to be set.
	//			source - The source of the value.
	void UpdateInputFromSteamSource(InputType input, float value, EControllerActionOrigin source);

	// PURPOSE:	Retrieves inputs on a steam controller source.
	// PARAMS:	input - The input to get the sources for.
	//			sources - The list of sources to be populated.
	void GetSteamControllerInputSources(InputType input, ioMapper::ioSourceList& sources) const;

	// PURPOSE: Disables inputs on a steam controller source.
	// PARAMS:	source - The source to be disabled.
	//			options - The options of the disabling.
	void DisableInputOnSteamControllerSource(const ioSource& source, const ioValue::DisableOptions& options);

	// PURPOSE:	Helper function to disable all inputs in an array
	// PARAMS:	inputs - The inputs to be disabled.
	//			options - The options of the disabling.
	void DisableInputsInArray(const atArray<InputType>& inputs, const ioValue::DisableOptions& options);
	
	// PURPOSE:	The steam controller to use if we should use one.
	ControllerHandle_t	m_SteamController;

	// PURPOSE:	 The steam controller settings file.
	ControlInput::SteamController::Definition m_SteamControllerSettings;

	// PURPOSE:	The handles to steam actions.
	// NOTES:	This assumes the handles are u64 which they were at the time of this being added.
	atMap<atHashString, u64>	m_SteamHandleMap;

	// PURPOSE: Structure for quickly looking up an action in the steam action arrays.
	struct SteamSource
	{
		// PURPOSE: The type of source.
		enum Type
		{
			// PURPOSE: The source is the button array.
			BUTTON,

			// PURPOSE:	 The source is in the trigger array.
			TRIGGER,

			// PURPOSE:	The source is in the axis array.
			AXIS,

			// PURPOSE: Invalid source location.
			INVALID,
		};

		// PURPOSE: The type of source.
		Type m_Type;

		// PURPOSE:	The index of the source.
		u32  m_Index;

		// PURPOSE: Constructs an invalid index.
		SteamSource()
			: m_Type(INVALID)
			, m_Index(0xffffffff)
		{}
	};

	// PURPOSE:	A list of steam
	atRangeArray<SteamSource, k_EControllerActionOrigin_Count> m_SteamSources;

	// PURPOSE:	The action set to use.
	u64 m_SteamActionSet;

	CompileTimeAssert(sizeof(ControllerActionSetHandle_t) == sizeof(u64));
	CompileTimeAssert(sizeof(ControllerDigitalActionHandle_t) == sizeof(u64));
	CompileTimeAssert(sizeof(ControllerAnalogActionHandle_t) == sizeof(u64));
#endif // USE_STEAM_CONTROLLER

	// Indicates that the player is using toggle aim rather than hold to aim.
	bool m_ToggleAim;

	// Indicates that the player is currently aiming when using toggle aim.
	bool m_ToggleAimOn;

	// PURPOSE: Due to the way the toggle aim works we need to enable it on release but enable it on press.
	//			This ensures we do not re-enable on the release of the press used to disable.
	bool m_ResetToggleAim;

	// Indicates the always sprint for pushbikes is on.
	bool m_ToggleBikeSprintOn;

	// Keeps track of the last source for input.
	ioSource m_LastKnownSource;

	// PURPOSE: Keeps track of the currently loaded dynamic scheme.
	atHashString m_CurrentLoadedDynamicScheme;

	// PURPOSE:	Used to keep track of the loaded script dynamic control scheme.
	atHashString m_CurrentScriptDynamicScheme;

	// PURPOSE: The dynamic control scheme to override the control scheme set by script.
	// NOTES:	This is currently only used for the video editor.
	atHashString m_CurrentCodeOverrideDynamicScheme;

#if DEBUG_DRAW
	// PURPOSE:	Cached on foot mouse sensitivity for debug output.
	float m_OnFootMouseSensitivity;

	// PURPOSE:	Cached in vehicle mouse sensitivity for debug output.
	float m_InVehicleMouseSensitivity;
#endif // DEBUG_DRAW

	// PURPOSE:	Calculates the mouse sensitivity.
	// PARAMS:	percentage - the sensitivity setting between 0...1 inclusive.
	//			min - the minimum sensitivity value
	//			max - the maximum sensitivity value.
	//			powArc -  the power to used to define the arc from 0 to 1.
	float CalculateMouseSensitivity(float percentage, float min, float max, float powArc);

	bool UpdateMappingFromInputScan();

	void UpdateIdenticalMappings( const InputType input, const MappingType type, const ioSource& source, u32 mappingNumber );
	void UpdateIdenticalMappingsInList( const InputType input,
		const ControlInput::InputList& inputList,
		const MappingType type,
		const ioSource& source,
		u32 mappingNumber );

	// Used internally to indicate movement commands came form the keyboard/mouse control scheme.
	// TR TODO: This will be obsolete when a global method is available for detecting the source of input.
	bool IsUsingKeyboardAndMouseForMovement() const;

	void UpdateKeyBindings();

	// PURPOSE: Saves a changed input mapping.
	// PARAMS:	input - the input to save.
	//			deviceId - the id of the device check and save.
	//			defaultSettings - the list of default settings to check for change against.
	//			userSettings - the structure so save the altered user settings out to.
	// NOTES:	If a mapping has not changed from the default it will not be saved.
	void SaveChangedInput(const InputType input, const s32 deviceId, const ControlInput::ControlSettings& defaultSettings, ControlInput::ControlSettings& userSettings) const;

	// PURPOSE:	Detects if two mappings are the same.
	// PARAMS:	lhs - mapping to compare.
	//			rhs - mapping to compare.
	// RETURNS:	true if the two mappings are the same.
	static bool AreTwoMappingsEqual(const ControlInput::Mapping& lhs, const ControlInput::Mapping& rhs);

	// PURPOSE: Populates conflictData.
	// RETURNS: true if there is a conflict.
	bool GetConflictingInputData(const InputType input, const ioSource& source, bool allowConfictWithSelf, MappingConflictData& conflictData, bool assertOnConflict) const;
	bool GetConflictingInputData(const InputType input, const ControlInput::ConflictList& conflictList, const ioSource& source, bool allowConfictWithSelf, MappingConflictData& conflictData, bool assertOnConflict) const;

	// PURPOSE:	Retrieves the index of an input in a conflict list.
	// PARAMS:	conflicts - the list of conflicts to search.
	//			input - the input to search for.
	// RETURNS:	The index or -1 if it does not exist.
	static s32 GetConflictIndex(const MappingConflictList& conflicts, InputType input);

	// PURPOSE: Returns true if the two inputs are exempt from a conflict.
	// NOTES:	If input1 and input2 are identical then false is *ALWAYS* returned. If this is a problem you should check for this case yourself!
	bool HasConflictException(const InputType input1, const InputType input2) const;

	// PURPOSE:	Checks for a user mapping conflict.
	// NOTES:	This will assert on conflicts.
	// RETURNS:	The number of mapping conflicts.
	u32 GetNumberOfMappingConflicts( bool ASSERT_ONLY(assertOnConflict) = false) const;

	// PURPOSE:	Loads keyboard/mouse specific extra settings.
	// PARAMS:	settings - the settings to load.
	// NOTES:	These settings are extra settings that the user cannot change directly other than through specific menu options.
	void AddKeyboardMouseExtraSettings(const ControlInput::ControlSettings &settings);


	// PURPOSE:	The number of mapping screen slots that the user can assign to a control.
	const static s32 NUM_MAPPING_SCREEN_SLOTS = 2u;

	// PURPOSE:	The size of the buffer required for retrieving the user path.
	const static u32 PATH_BUFFER_SIZE = 260;

	// PURPOSE:	Returns the user settings directory.
	// PARAMS:	path - the buffer to fill in with the path.
	//			file - the file path to append to the end.
	static void GetUserSettingsPath(char (&path)[PATH_BUFFER_SIZE], const char* const file = "");

	// PURPOSE: Indicates if the user is signed in to social club.
	// NOTES:	This is used to know when we can load the users settings file.
	static bool IsUserSignedIn();

	// PURPOSE: Keeps track of when the user signs in so that we can update the control files.
	bool m_IsUserSignedIn;

	// PURPOSE:	Indicates we need to re-cache conflict count.
	// NOTES:	mutable as we need to update this flag in a const function. It is only used for caching an expensive function.
	mutable bool m_RecachedConflictCount;

	// PURPOSE:	The number of conflicts with the user controls.
	// NOTES:	mutable as its a chached value from an expensive const function.
	mutable u32 m_CachedConflictCount;
#endif // KEYBOARD_MOUSE_SUPPORT

#if RSG_PC
	void UpdateJoystickBindings();

	// PURPOSE:	Load joystick mappings based on the gamepad definition file.
	// PARAMS:	definition - the gamepad definition.
	//			settings - the settings to be loaded.
	//			padIndex - the gamepad index to load the settings into.
	void LoadGamepadControls(const ControlInput::Gamepad::Definition& definition, const ControlInput::ControlSettings& settings, s32 padIndex);

	// PURPOSE: Saves the current joystick definition.
	// PARAMS:	userFilePath - the path to the file to save to.
	// RETURNS:	true if the file was successfully saved.
	bool SaveCurrentJoystickDefinition(const char* userFilePath) const;

	// PURPOSE: Loads the current joystick definition.
	// PARAMS:	userFilePath - the path of the file to load.
	// RETURNS:	true if the file was successfully saved.
	bool LoadCurrentJoystickDefinition(const char* userFilePath);

	static bool ValidPrimaryDynamicMapping(const ControlInput::DynamicMapping& mapping);
	static bool ValidSecondaryDynamicMapping(const ControlInput::DynamicMapping& mapping);

	void LoadDeviceControls(bool loadAutosave);

	// PURPOSE: Indicates that all direct input devices should also be loaded.
	bool m_UseExtraDevices;


#if RSG_ASSERT
	// PURPOSE:	Keeps track of which script has setup a pc control scheme.
	char m_ControlSchemeScriptOwner[255];
#endif // RSG_ASSERT
#endif // RSG_PC

	// Indicates the always run option is on.
	bool m_ToggleRunOn;

	// PURPOSE: Indicates the controls were refreshed this frame.
	bool m_WasControlsRefreshedThisFrame;

	// PURPOSE: Indicates that script has checked if the controls has changed.
	bool m_HasScriptCheckControlsRefresh;

	// PURPOSE: Indicates the controls were refreshed last frame.
	bool m_WasControlsRefreshedLastFrame;

#if FPS_MODE_SUPPORTED
	bool m_WasUsingFpsMode;
#endif // FPS_MODE_SUPPORTED

#if __BANK
	// Mapping text is only needed for RAG.
	struct RagData
	{
		enum {TEXT_SIZE = 25};

		// indicates the user has forced an input to be disabled from within rag.
		bool	m_ForceDisabled;
		float	m_InputValue;
		bool	m_Enabled;
#if __WIN32PC
		// The text to show for the primary mapping.
		char	m_KeyboardMousePrimary[TEXT_SIZE];
		// The text to show for the secondary mapping.
		char	m_KeyboardMouseSecondary[TEXT_SIZE];
		// The text to show for the joystick mapping.
		char m_Joystick[TEXT_SIZE];
#endif // __WIN32PC

		RagData();
	};
	// Rag needs a copy of the mapping data for each input (to display information for each input)
	RagData m_ragData[MAX_INPUTS];

#if __WIN32PC
	// Group Joystick definition data together.
	struct JoystickDefinitionRagData
	{
		enum {TEXT_SIZE = 50, NAME_LENGTH = 50};

		s32	m_LastDeviceIndex;

		char m_Guid[ControlInput::Gamepad::GUID_SIZE];

		char m_Status[TEXT_SIZE];

		char m_SettingsPath[RAGE_MAX_PATH];

		char m_Name[ioJoystick::MAX_STICKS][NAME_LENGTH];
		const char* m_NameComboHook[ioJoystick::MAX_STICKS];

		char m_Parameters[NUM_PAD_SOURCES][TEXT_SIZE];
		char m_Sources[NUM_PAD_SOURCES][TEXT_SIZE];

		bool m_AutoSave;

		JoystickDefinitionRagData();
	};

	JoystickDefinitionRagData m_JoystickDefinitionRagData;

	char m_RagPCControlScheme[RagData::TEXT_SIZE];
	char m_RagLoadedPCControlScheme[RagData::TEXT_SIZE];

	//void InitMappingWidgets(bkBank& bank, const char *title);
	void AddKeyboardMouseUpdateButtonToRag(bkBank& bank, InputType input, const char* sourceName, bool isPrimary);
	void AddJoystickUpdateButtonToRag(bkBank& bank, InputType input, const char* sourceName);

	void UpdatePrimaryKeyMapping(CallbackData data);
	void UpdateSecondaryKeyMapping(CallbackData data);
	void UpdateJoystickMapping(CallbackData data);
	void SwitchJoystickDefinitionDevice();
	void CalibrateJoystickDevice();
	void SaveJoystickDefinitionDevice();
	void ScanJoystickDefinitionDevice(CallbackData data);
	void UpdateJoystickDefinitionRagData();
	void ReloadMappings();
	void RagLoadPCMappings() { LoadPCScriptControlMappings(atHashString(m_RagPCControlScheme)); }
#endif // __WIN32PC
#endif // __BANK

	static void GetMappingParameterName( const ioSource& source, char *buffer, size_t bufferSize  );

	void UpdateMapping(const InputType input, const MappingType type, const ioSource& source, u32 mappingNumber, bool updateDependentMappings);
	void AddMapping(const InputType input, const ioSource& source);
	bool RemoveMapping(const ioSource& source, InputType inputId, MappingType type, u32 mappingNumber);

	void UpdateDependantMappings(const InputType input, const MappingType type, const ioSource& source, u32 mappingNumber, UpdateType updateType);
	void UpdateAxisDefinitionMappings(const InputType input, const MappingType type, const ioSource& source, u32 mappingNumber, UpdateType updateType);
	ioSource GetAxisCompatableSource(const ioSource& negative, const ioSource& positive);

	void Clear();
};

inline ioSource CControl::GetInputSource(const char* input, s32 device, u8 mappingIndex, bool allowFallback) const
{
	InputType inputValue = GetInputByName(input);
	
	if(controlVerifyf(inputValue != UNDEFINED_INPUT, "'%s' is not a valid input and was used to retrieve an input source!", input))
	{
		return GetInputSource(inputValue, device, mappingIndex, allowFallback);
	}
	else
	{
		return ioSource::UNKNOWN_SOURCE;
	}
}

#if KEYBOARD_MOUSE_SUPPORT
inline bool CControl::IsUsingKeyboardAndMouseForMovement() const
{
	return m_inputs[INPUT_MOVE_UD].GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE
		|| m_inputs[INPUT_MOVE_LR].GetSource().m_DeviceIndex == ioSource::IOMD_KEYBOARD_MOUSE;
}

inline CControl::MappingConflictData::MappingConflictData()
	: m_Conflicts()
	, m_NumConflictingInputs(0u)
	, m_IsNewConflict(false)
{
}

inline void CControl::MappingConflictData::Clear()
{
	m_Conflicts.Reset();
	m_NumConflictingInputs = 0u;
	m_IsNewConflict = false;
}


inline CControl::CurrentMappingData::CurrentMappingData()
	: m_ScanMapper(NULL)
	, m_Type(UNKNOWN_MAPPING_TYPE)
	, m_Input(UNDEFINED_INPUT)
	, m_MappingIndex(0u)
	, m_Source()
	, m_Conflict()
	, m_CancelMapping(false)
	, m_ReplaceConflict(false)
#if RSG_BANK
	, m_MappingCameFromRag(false)
#endif // RSG_BANK
{
}

inline void CControl::CurrentMappingData::Clear()
{
	m_ScanMapper = NULL;
	m_Type = UNKNOWN_MAPPING_TYPE;
	m_Input = UNDEFINED_INPUT;
	m_Source = ioSource::UNKNOWN_SOURCE;
	m_Conflict.Clear();
	m_CancelMapping = false;
	m_ReplaceConflict = false;
	BANK_ONLY(m_MappingCameFromRag = false;)
}

inline bool CControl::ValidPrimaryDynamicMapping(const ControlInput::DynamicMapping& mapping)
{
	bool result = (mapping.m_Mappings[0] >= FIRST_INPUT && mapping.m_Mappings[0] < SCRIPTED_INPUT_FIRST)
		|| (mapping.m_Mappings[0] > SCRIPTED_INPUT_LAST && mapping.m_Mappings[0] < MAX_INPUTS)
		|| mapping.m_Mappings[0] == DYNAMIC_MAPPING_MOUSE_X
		|| mapping.m_Mappings[0] == DYNAMIC_MAPPING_MOUSE_Y;

	controlAssertf(result, "Invalid primary mapping for %s",  GetInputName(mapping.m_Input));
	return result;
}

inline bool CControl::ValidSecondaryDynamicMapping(const ControlInput::DynamicMapping& mapping)
{
	bool result = mapping.m_Mappings.GetCount() == 1
		|| (mapping.m_Mappings[1] >= FIRST_INPUT && mapping.m_Mappings[1] < SCRIPTED_INPUT_FIRST)
		|| (mapping.m_Mappings[1] > SCRIPTED_INPUT_LAST && mapping.m_Mappings[1] < MAX_INPUTS);

	controlAssertf(result, "Invalid secondary mapping for %s",  GetInputName(mapping.m_Input));
	return result;
}
#endif // KEYBOARD_MOUSE_SUPPORT

#if RSG_PC
inline bool CControl::JoystickDefinitionData::IsAutoMapped( ioMapperParameter param )
{
	return param == IOM_AXIS_LY_DOWN || param == IOM_AXIS_LX_RIGHT 
		|| param == IOM_AXIS_RY_DOWN || param == IOM_AXIS_RX_RIGHT
		|| param == IOM_AXIS_LY || param == IOM_AXIS_LX
		|| param == IOM_AXIS_RY || param == IOM_AXIS_RX;

}

inline const CControl::KeyInfo& CControl::GetKeyInfo(u32 keyCode)
{
	return ms_settings.m_KeyInfo[keyCode];
}

#endif // RSG_PC

inline void CControl::DisableInput(unsigned inputIndex, const ioValue::DisableOptions& options, bool disableRelatedInputs)
{
	if (controlVerifyf(inputIndex<MAX_INPUTS, "Invalid input id (%d) whilst trying to disable an input!", inputIndex))
	{
		controlDebugf2("%s has been disabled.", GetInputName(static_cast<InputType>(inputIndex)));
		DebugOutputScriptCallStack();
		DoDisableInput(inputIndex, options, disableRelatedInputs);
	}
}

inline void CControl::EnableInput(unsigned inputIndex, bool enableRelatedInputs)
{
	if (controlVerifyf(inputIndex<MAX_INPUTS, "Invalid input id (%d) whilst trying to enable an input!", inputIndex))
	{
		controlDebugf2("%s has been Enabled.", GetInputName(static_cast<InputType>(inputIndex)));
		DebugOutputScriptCallStack();
		DoEnableInput(inputIndex, enableRelatedInputs);
	}
}

inline bool CControl::GetPedTargetIsDown(float threshold) const
{
	ioValue::ReadOptions options = ioValue::DEFAULT_OPTIONS;
	options.SetFlags(ioValue::ReadOptions::F_READ_SUSTAINED, true);

#if KEYBOARD_MOUSE_SUPPORT
	if(m_ToggleAimOn && m_inputs[INPUT_AIM].IsEnabled())
	{
		return !m_inputs[INPUT_AIM].IsDown(threshold, options);
	}
#endif // KEYBOARD_MOUSE_SUPPORT
	return m_inputs[INPUT_AIM].IsDown(threshold, options);
}

inline bool CControl::GetPedTargetIsUp(float threshold) const
{
	ioValue::ReadOptions options = ioValue::DEFAULT_OPTIONS;
	options.SetFlags(ioValue::ReadOptions::F_READ_SUSTAINED, true);

#if KEYBOARD_MOUSE_SUPPORT
	if(m_ToggleAimOn && m_inputs[INPUT_AIM].IsEnabled())
	{
		return !m_inputs[INPUT_AIM].IsUp(threshold, options);
	}
#endif // KEYBOARD_MOUSE_SUPPORT
	return m_inputs[INPUT_AIM].IsUp(threshold, options);
}

inline bool CControl::GetPedTargetIsReleased() const
{
	ioValue::ReadOptions options = ioValue::DEFAULT_OPTIONS;
	options.SetFlags(ioValue::ReadOptions::F_READ_SUSTAINED, true);

#if KEYBOARD_MOUSE_SUPPORT
	if(m_ToggleAimOn && m_inputs[INPUT_AIM].IsEnabled())
	{
		return m_inputs[INPUT_AIM].IsPressed(ioValue::BUTTON_DOWN_THRESHOLD, options);
	}
#endif // KEYBOARD_MOUSE_SUPPORT
	return m_inputs[INPUT_AIM].IsReleased(ioValue::BUTTON_DOWN_THRESHOLD, options);
}

inline float CControl::GetPedTargetNorm() const
{
	ioValue::ReadOptions options = ioValue::DEFAULT_OPTIONS;
	options.SetFlags(ioValue::ReadOptions::F_READ_SUSTAINED, true);

#if KEYBOARD_MOUSE_SUPPORT
	if(m_ToggleAimOn && m_inputs[INPUT_AIM].IsEnabled())
	{
		// This is actually used for aiming so we invert the norm value returned.
		return 1.0f - m_inputs[INPUT_AIM].GetNorm(options);
	}
#endif // KEYBOARD_MOUSE_SUPPORT
	return m_inputs[INPUT_AIM].GetNorm(options);
}

inline float CControl::GetPedTargetLastNorm() const
{
	ioValue::ReadOptions options = ioValue::DEFAULT_OPTIONS;
	options.SetFlags(ioValue::ReadOptions::F_READ_SUSTAINED, true);

#if KEYBOARD_MOUSE_SUPPORT
	if(m_ToggleAimOn && m_inputs[INPUT_AIM].IsEnabled())
	{
		// This is actually used for aiming so we invert the last norm value returned.
		return 1.0f - m_inputs[INPUT_AIM].GetLastNorm(options);
	}
#endif // KEYBOARD_MOUSE_SUPPORT
	return m_inputs[INPUT_AIM].GetLastNorm(options);
}

inline bool CControl::GetVehiclePassengerAimDownCheckToggleAim(float threshold) const
{
	ioValue::ReadOptions options = ioValue::DEFAULT_OPTIONS;
	options.SetFlags(ioValue::ReadOptions::F_READ_SUSTAINED, true);

#if KEYBOARD_MOUSE_SUPPORT
	if(m_ToggleAimOn && m_inputs[INPUT_VEH_PASSENGER_AIM].IsEnabled())
	{
		return !m_inputs[INPUT_VEH_PASSENGER_AIM].IsDown(threshold, options);
	}
#endif // KEYBOARD_MOUSE_SUPPORT
	return m_inputs[INPUT_VEH_PASSENGER_AIM].IsDown(threshold, options);
}

inline void CControl::GetInputsInGroup(InputGroup inputGroup, InputGroupList& inputs)
{
	if(controlVerifyf(inputGroup >= 0 && inputGroup < MAX_INPUTGROUPS, "Invalid input group '%d' passed in to CControl::GetInputsInGroup()!", inputGroup))
	{
		inputs = ms_settings.m_InputGroupDefinitions[inputGroup];
		controlAssertf( (inputs.size() >= IGT_SINGLE_OR_DOUBLE_AXIS && inputs.size() <= IGT_DOUBLE_DPAD_AXIS) || inputs.size() == IGT_GENERIC_DOUBLE_AXIS,
			"There should be exactly %d (for axis directions), %d (for dpad & axis singular directions), %d (for dpad directions) or %d (for dpad & axis dual directions) inputs in a group!",
			IGT_SINGLE_OR_DOUBLE_AXIS,
			IGT_GENERIC_SINGLE_AXIS,
			IGT_DOUBLE_DPAD_AXIS,
			IGT_GENERIC_DOUBLE_AXIS );
	}
}


inline void CControl::DebugOutputScriptCallStack()
{
#if !__FINAL
	if(Channel_Control.MaxLevel >= DIAG_SEVERITY_DEBUG3 && sysThreadType::IsUpdateThread() && scrThread::GetActiveThread())
	{
		sysCriticalSection critSec(ms_Lock);

		// PrintStackTrace does not exist in final.
		scrThread::GetActiveThread()->PrintStackTrace(ChanneledOutputForStackTrace);
		controlDebugf3("---- With stack: \n%s", ms_ThreadlStack.c_str());
		ms_ThreadlStack.Clear();
	}
#endif // !__FINAL
}

#if LIGHT_EFFECTS_SUPPORT
inline CControl::LightEffectData::LightEffectData()
	: m_Effect(NULL)
	, m_StartTime(0u)
{
}

inline bool CControl::IsCurrentLightEffect(const ioLightEffect* effect, LightEffectType type) const
{
	sysCriticalSection lock(m_Cs);
	return (effect != NULL && m_LightEffects[type].m_Effect == effect);
}
#endif // LIGHT_EFFECTS_SUPPORT

#endif

