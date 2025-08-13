//
// filename:	VehicleFactory.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------

// C headers

// Rage headers
#if __BANK
	#include "bank/bkmgr.h"
	#include "bank/bank.h"
	#include "bank/combo.h"
	#include "bank/slider.h"
	#include "physics/WorldProbe/worldprobe.h"
	#include "fwsys/gameskeleton.h"
	#include "fwdecorator/decoratorExtension.h"
	#include "fwdecorator/decoratorInterface.h"
#endif
#include "ai/task/task.h"
#include "file/default_paths.h"
#include "fragment/tune.h"
#include "fwdebug/picker.h"
#include "fwdrawlist/drawlistmgr.h"
#include "fwscript/scriptguid.h"
#include "string/stringutil.h"
#include "system/nelem.h"
#include "system/new.h"

#include "file/stream.h"
#include "file/token.h"

// Game headers
#include "animation/MoveVehicle.h"
#include "audio/heliaudioentity.h"
#include "audio/trainaudioentity.h"
#include "audio/caraudioentity.h"
#include "audio/planeaudioentity.h"
#include "audio/boataudioentity.h"
#include "audio/bicycleaudioentity.h"
#include "audio/traileraudioentity.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/system/CameraManager.h"
#include "camera/helpers/Frame.h"
#include "camera/viewports/ViewportManager.h"
#include "Control/replay/Replay.h"
#include "debug/DebugScene.h"
#include "debug/debugGlobals.h"
#include "frontend/MiniMap.h"
#include "game/weather.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "modelInfo/vehicleModelInfo.h"
#include "modelinfo/VehicleModelInfoColors.h"
#include "network/Objects/NetworkObjectPopulationMgr.h"
#include "pathserver/ExportCollision.h"
#include "peds/ped.h"
#include "peds/PedFactory.h"
#include "peds/PedIntelligence.h"
#include "peds/PopCycle.h"
#include "peds/popzones.h"
#include "physics/gtaInst.h"
#include "physics/physics.h"
#include "scene/DataFileMgr.h"
#include "scene/world/gameWorld.h"
#include "scene/RegdRefTypes.h"
#include "script/Handlers/GameScriptEntity.h"
#include "script/script.h"
#include "script/script_cars_and_peds.h"
#include "streaming/populationstreaming.h"
#include "streaming/streaming.h"		// For CStreaming::HasObjectLoaded(), etc.
#include "system/memmanager.h"
#include "Task/Physics/TaskNMBalance.h"
#include "Task/Vehicle/TaskCarAccessories.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "timecycle/TimeCycle.h"
#include "vehicleAI/task/TaskVehicleAnimation.h"
#include "vehicleAi/task/TaskVehiclePlayer.h"
#include "vehicleAI/VehicleIntelligence.h"
#include "vehicles/AmphibiousAutomobile.h"
#include "vehicles/bike.h"
#include "vehicles/DraftVehicle.h"
#include "vehicles/bmx.h"
#include "vehicles/boat.h"
#include "vehicles/heli.h"
#include "vehicles/planes.h"
#include "vehicles/vehiclepopulation.h"
#include "vehicles/Submarine.h"
#include "vehicles/trailer.h"
#include "vehicles/train.h"
#include "vehicles/VehicleDefines.h"
#include "vehicles/vehicleFactory.h"
#include "vehicles/VehicleGadgets.h"
#include "renderer/ApplyDamage.h"
#include "vehicles/Metadata/AIHandlingInfo.h"
#include "vehicles/Metadata/VehicleMetadataManager.h"
#include "vehicles/Metadata/VehicleDebug.h"
#include "vehicles/Metadata/VehicleSeatInfo.h"
#include "shaders/CustomShaderEffectVehicle.h"
#include "script/commands_vehicle.h"
#include "Tools/smokeTest.h"
#include "vfx/misc/Fire.h"
#include "vfx/particles/PtFxManager.h"
#include "vfx/vehicleglass/VehicleGlassSmashTest.h"
#include "network/NetworkInterface.h"
#include "network/Debug/NetworkDebug.h"
#include "system/poolallocator.h"
#include "renderer/ApplyDamage.h"

#if __BANK
#include "Task/Default/TaskPlayer.h"
#endif //__BANK

AI_VEHICLE_OPTIMISATIONS()
VEHICLE_OPTIMISATIONS()

RAGE_DEFINE_CHANNEL(boatTrailer)

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------
PARAM(defaultVehicle,"define default vehicle (used for U/Shift+U)");
PARAM(defaultVehicleTrailer,"define default vehicle trailer (used for U/Shift+U)");

PARAM(debugvehicle,"create vehicle bank by default");
XPARAM(debugtrain);

XPARAM(disablewheelintegrationtask);

// --- Static Globals -----------------------------------------------------------

static ObjectNameIdAssociation carIds[VEH_NUM_NODES + 1] = 
{
	{"chassis",			VEH_CHASSIS},
	{"chassis_lowlod",	VEH_CHASSIS_LOWLOD},
	{"chassis_dummy",	VEH_CHASSIS_DUMMY},

	{"door_dside_f",	VEH_DOOR_DSIDE_F},
	{"door_dside_r",	VEH_DOOR_DSIDE_R},
	{"door_pside_f",	VEH_DOOR_PSIDE_F},
	{"door_pside_r",	VEH_DOOR_PSIDE_R},

	{"handle_dside_f",	VEH_HANDLE_DSIDE_F},
	{"handle_dside_r",	VEH_HANDLE_DSIDE_R},
	{"handle_pside_f",	VEH_HANDLE_PSIDE_F},
	{"handle_pside_r",	VEH_HANDLE_PSIDE_R},

	{"wheel_lf",		VEH_WHEEL_LF},
	{"wheel_rf",		VEH_WHEEL_RF},
	{"wheel_lm1",		VEH_WHEEL_LM1},
	{"wheel_rm1",		VEH_WHEEL_RM1},
	{"wheel_lm2",		VEH_WHEEL_LM2},
	{"wheel_rm2",		VEH_WHEEL_RM2},
	{"wheel_lm3",		VEH_WHEEL_LM3},
	{"wheel_rm3",		VEH_WHEEL_RM3},
	{"wheel_lr",		VEH_WHEEL_LR},
	{"wheel_rr",		VEH_WHEEL_RR},

	{"suspension_lf",	VEH_SUSPENSION_LF},
	{"suspension_rf",	VEH_SUSPENSION_RF},
	{"suspension_lm",	VEH_SUSPENSION_LM},
	{"suspension_rm",	VEH_SUSPENSION_RM},
	{"suspension_lr",	VEH_SUSPENSION_LR},
	{"suspension_rr",	VEH_SUSPENSION_RR},
	{"spring_rf",		VEH_SPRING_RF},
	{"spring_lf",		VEH_SPRING_LF},
	{"spring_rr",		VEH_SPRING_RR},
	{"spring_lr",		VEH_SPRING_LR},
	{"transmission_f",	VEH_TRANSMISSION_F},
	{"transmission_m",	VEH_TRANSMISSION_M},
	{"transmission_r",	VEH_TRANSMISSION_R},
	{"hub_lf",			VEH_WHEELHUB_LF},
	{"hub_rf",			VEH_WHEELHUB_RF},
	{"hub_lm1",			VEH_WHEELHUB_LM1},
	{"hub_rm1",			VEH_WHEELHUB_RM1},
	{"hub_lm2",			VEH_WHEELHUB_LM2},
	{"hub_rm2",			VEH_WHEELHUB_RM2},
	{"hub_lm3",			VEH_WHEELHUB_LM3},
	{"hub_rm3",			VEH_WHEELHUB_RM3},
	{"hub_lr",			VEH_WHEELHUB_LR},
	{"hub_rr",			VEH_WHEELHUB_RR},

	{"windscreen",		VEH_WINDSCREEN},
	{"windscreen_r",	VEH_WINDSCREEN_R},
	{"window_lf",		VEH_WINDOW_LF},
	{"window_rf",		VEH_WINDOW_RF},
	{"window_lr",		VEH_WINDOW_LR},
	{"window_rr",		VEH_WINDOW_RR},
	{"window_lm",		VEH_WINDOW_LM},
	{"window_rm",		VEH_WINDOW_RM},

	{"bodyshell",		VEH_BODYSHELL},
	{"bumper_f",		VEH_BUMPER_F},
	{"bumper_r",		VEH_BUMPER_R},
	{"wing_rf",			VEH_WING_RF},
	{"wing_lf",			VEH_WING_LF},
	{"bonnet",			VEH_BONNET},
	{"boot",			VEH_BOOT},
	{"cargodoor",		VEH_BOOT_2},
	{"exhaust",			VEH_EXHAUST},
	{"exhaust_2",		VEH_EXHAUST_2},
	{"exhaust_3",		VEH_EXHAUST_3},
	{"exhaust_4",		VEH_EXHAUST_4},
	{"exhaust_5",		VEH_EXHAUST_5},
	{"exhaust_6",		VEH_EXHAUST_6},
	{"exhaust_7",		VEH_EXHAUST_7},
	{"exhaust_8",		VEH_EXHAUST_8},
	{"exhaust_9",		VEH_EXHAUST_9},
	{"exhaust_10",		VEH_EXHAUST_10},
	{"exhaust_11",		VEH_EXHAUST_11},
	{"exhaust_12",		VEH_EXHAUST_12},
	{"exhaust_13",		VEH_EXHAUST_13},
	{"exhaust_14",		VEH_EXHAUST_14},
	{"exhaust_15",		VEH_EXHAUST_15},
	{"exhaust_16",		VEH_EXHAUST_16},
	{"engine",			VEH_ENGINE},
	{"overheat",		VEH_OVERHEAT},
	{"overheat_2",		VEH_OVERHEAT_2},
	{"petrolcap",		VEH_PETROLCAP},
	{"petroltank",		VEH_PETROLTANK},
	{"petroltank_l",	VEH_PETROLTANK_L},
	{"petroltank_r",	VEH_PETROLTANK_R},
	{"steering",		VEH_STEERING_WHEEL},
	{"steeringwheel",	VEH_CAR_STEERING_WHEEL},
	{"hbgrip_l",		VEH_HBGRIP_L},
	{"hbgrip_r",		VEH_HBGRIP_R},
	{"thrust",			VEH_ROCKET_BOOST},
	{"thrust_2",		VEH_ROCKET_BOOST_2},
	{"thrust_3",		VEH_ROCKET_BOOST_3},
	{"thrust_4",		VEH_ROCKET_BOOST_4},
	{"thrust_5",		VEH_ROCKET_BOOST_5},
	{"thrust_6",		VEH_ROCKET_BOOST_6},
	{"thrust_7",		VEH_ROCKET_BOOST_7},
	{"thrust_8",		VEH_ROCKET_BOOST_8},

	{"headlight_l",		VEH_HEADLIGHT_L},
	{"headlight_r",		VEH_HEADLIGHT_R},
	{"taillight_l",		VEH_TAILLIGHT_L},
	{"taillight_r",		VEH_TAILLIGHT_R},
	{"indicator_lf",	VEH_INDICATOR_LF},
	{"indicator_rf",	VEH_INDICATOR_RF},
	{"indicator_lr",	VEH_INDICATOR_LR},
	{"indicator_rr",	VEH_INDICATOR_RR},
	{"brakelight_l",	VEH_BRAKELIGHT_L},
	{"brakelight_r",	VEH_BRAKELIGHT_R},
	{"brakelight_m",	VEH_BRAKELIGHT_M},
	{"reversinglight_l",VEH_REVERSINGLIGHT_L},
	{"reversinglight_r",VEH_REVERSINGLIGHT_R},
	{"neon_l",			VEH_NEON_L},
	{"neon_r",			VEH_NEON_R},
	{"neon_f",			VEH_NEON_F},
	{"neon_b",			VEH_NEON_B},
	{"extralight_1",	VEH_EXTRALIGHT_1},
	{"extralight_2",	VEH_EXTRALIGHT_2},
	{"extralight_3",	VEH_EXTRALIGHT_3},
	{"extralight_4",	VEH_EXTRALIGHT_4},
	
	{"emissives",		VEH_EMISSIVES},
	{"numberplate",		VEH_NUMBERPLATE},
	{"interiorlight",	VEH_INTERIORLIGHT},

	{"platelight",      VEH_PLATELIGHT},
	{"dashglow",        VEH_DASHLIGHT},
	{"doorlight_lf",    VEH_DOORLIGHT_LF},
	{"doorlight_rf",    VEH_DOORLIGHT_RF},
	{"doorlight_lr",    VEH_DOORLIGHT_LR},
	{"doorlight_rr",    VEH_DOORLIGHT_RR},

	{"siren1",			VEH_SIREN_1},
	{"siren2",			VEH_SIREN_2},
	{"siren3",			VEH_SIREN_3},
	{"siren4",			VEH_SIREN_4},
	{"siren5",			VEH_SIREN_5},
	{"siren6",			VEH_SIREN_6},
	{"siren7",			VEH_SIREN_7},
	{"siren8",			VEH_SIREN_8},
	{"siren9",			VEH_SIREN_9},
	{"siren10",			VEH_SIREN_10},
	{"siren11",			VEH_SIREN_11},
	{"siren12",			VEH_SIREN_12},
	{"siren13",			VEH_SIREN_13},
	{"siren14",			VEH_SIREN_14},
	{"siren15",			VEH_SIREN_15},
	{"siren16",			VEH_SIREN_16},
	{"siren17",			VEH_SIREN_17},
	{"siren18",			VEH_SIREN_18},
	{"siren19",			VEH_SIREN_19},
	{"siren20",			VEH_SIREN_20},
	
	{"siren_glass1",	VEH_SIREN_GLASS_1},
	{"siren_glass2",	VEH_SIREN_GLASS_2},
	{"siren_glass3",	VEH_SIREN_GLASS_3},
	{"siren_glass4",	VEH_SIREN_GLASS_4},
	{"siren_glass5",	VEH_SIREN_GLASS_5},
	{"siren_glass6",	VEH_SIREN_GLASS_6},
	{"siren_glass7",	VEH_SIREN_GLASS_7},
	{"siren_glass8",	VEH_SIREN_GLASS_8},
	{"siren_glass9",	VEH_SIREN_GLASS_9},
	{"siren_glass10",	VEH_SIREN_GLASS_10},
	{"siren_glass11",	VEH_SIREN_GLASS_11},
	{"siren_glass12",	VEH_SIREN_GLASS_12},
	{"siren_glass13",	VEH_SIREN_GLASS_13},
	{"siren_glass14",	VEH_SIREN_GLASS_14},
	{"siren_glass15",	VEH_SIREN_GLASS_15},
	{"siren_glass16",	VEH_SIREN_GLASS_16},
	{"siren_glass17",	VEH_SIREN_GLASS_17},
	{"siren_glass18",	VEH_SIREN_GLASS_18},
	{"siren_glass19",	VEH_SIREN_GLASS_19},
	{"siren_glass20",	VEH_SIREN_GLASS_20},

	{"dials",           VEH_DIALS},

	{"light_cover",     VEH_LIGHTCOVER},

	{"bobble_head",     VEH_BOBBLE_HEAD},
	{"bobble_base",     VEH_BOBBLE_BASE},
	{"bobble_hand",     VEH_BOBBLE_HAND},
	{"engineblock",     VEH_BOBBLE_ENGINE},

	{"spoiler",			VEH_SPOILER},
	{"struts",			VEH_STRUTS},

	{"spflap_l",		VEH_FLAP_L},
	{"spflap_r",		VEH_FLAP_R},

	{"misc_a",			VEH_MISC_A},
	{"misc_b",			VEH_MISC_B},
	{"misc_c",			VEH_MISC_C},
	{"misc_d",			VEH_MISC_D},
	{"misc_e",			VEH_MISC_E},
	{"misc_f",			VEH_MISC_F},
	{"misc_g",			VEH_MISC_G},
	{"misc_h",			VEH_MISC_H},
	{"misc_i",			VEH_MISC_I},
	{"misc_j",			VEH_MISC_J},
	{"misc_k",			VEH_MISC_K},
	{"misc_l",			VEH_MISC_L},
	{"misc_m",			VEH_MISC_M},
	{"misc_n",			VEH_MISC_N},
	{"misc_o",			VEH_MISC_O},
	{"misc_p",			VEH_MISC_P},
	{"misc_q",			VEH_MISC_Q},
	{"misc_r",			VEH_MISC_R},
	{"misc_s",			VEH_MISC_S},
	{"misc_t",			VEH_MISC_T},
	{"misc_u",			VEH_MISC_U},
	{"misc_v",			VEH_MISC_V},
	{"misc_w",			VEH_MISC_W},
	{"misc_x",			VEH_MISC_X},
	{"misc_y",			VEH_MISC_Y},
	{"misc_z",			VEH_MISC_Z},
	{"misc_1",			VEH_MISC_1},
	{"misc_2",			VEH_MISC_2},

	{"weapon_1a",		VEH_WEAPON_1A},
	{"weapon_1b",		VEH_WEAPON_1B},
	{"weapon_1c",		VEH_WEAPON_1C},
	{"weapon_1d",		VEH_WEAPON_1D},
	{"weapon_1a_rot",	VEH_WEAPON_1A_ROT},
	{"weapon_1b_rot",	VEH_WEAPON_1B_ROT},
	{"weapon_1c_rot",	VEH_WEAPON_1C_ROT},
	{"weapon_1d_rot",	VEH_WEAPON_1D_ROT},
	{"weapon_2a",		VEH_WEAPON_2A},
	{"weapon_2b",		VEH_WEAPON_2B},
	{"weapon_2c",		VEH_WEAPON_2C},
	{"weapon_2d",		VEH_WEAPON_2D},
	{"weapon_2e",		VEH_WEAPON_2E},
	{"weapon_2f",		VEH_WEAPON_2F},
	{"weapon_2g",		VEH_WEAPON_2G},
	{"weapon_2h",		VEH_WEAPON_2H},
	{"weapon_2a_rot",	VEH_WEAPON_2A_ROT},
	{"weapon_2b_rot",	VEH_WEAPON_2B_ROT},
	{"weapon_2c_rot",	VEH_WEAPON_2C_ROT},
	{"weapon_2d_rot",	VEH_WEAPON_2D_ROT},
	{"weapon_3a",		VEH_WEAPON_3A},
	{"weapon_3b",		VEH_WEAPON_3B},
	{"weapon_3c",		VEH_WEAPON_3C},
	{"weapon_3d",		VEH_WEAPON_3D},
	{"weapon_3a_rot",	VEH_WEAPON_3A_ROT},
	{"weapon_3b_rot",	VEH_WEAPON_3B_ROT},
	{"weapon_3c_rot",	VEH_WEAPON_3C_ROT},
	{"weapon_3d_rot",	VEH_WEAPON_3D_ROT},
	{"weapon_4a",		VEH_WEAPON_4A},
	{"weapon_4b",		VEH_WEAPON_4B},
	{"weapon_4c",		VEH_WEAPON_4C},
	{"weapon_4d",		VEH_WEAPON_4D},
	{"weapon_4a_rot",	VEH_WEAPON_4A_ROT},
	{"weapon_4b_rot",	VEH_WEAPON_4B_ROT},
	{"weapon_4c_rot",	VEH_WEAPON_4C_ROT},
	{"weapon_4d_rot",	VEH_WEAPON_4D_ROT},

	{"weapon_5a",		VEH_WEAPON_5A},
	{"weapon_5b",		VEH_WEAPON_5B},
	{"weapon_5c",		VEH_WEAPON_5C},
	{"weapon_5d",		VEH_WEAPON_5D},
	{"weapon_5e",		VEH_WEAPON_5E},
	{"weapon_5f",		VEH_WEAPON_5F},
	{"weapon_5g",		VEH_WEAPON_5G},
	{"weapon_5h",		VEH_WEAPON_5H},
	{"weapon_5i",		VEH_WEAPON_5I},
	{"weapon_5j",		VEH_WEAPON_5J},
	{"weapon_5k",		VEH_WEAPON_5K},
	{"weapon_5l",		VEH_WEAPON_5L},
	{"weapon_5m",		VEH_WEAPON_5M},
	{"weapon_5n",		VEH_WEAPON_5N},
	{"weapon_5o",		VEH_WEAPON_5O},
	{"weapon_5p",		VEH_WEAPON_5P},
	{"weapon__5q",		VEH_WEAPON_5Q},
	{"weapon_5r",		VEH_WEAPON_5R},
	{"weapon_5s",		VEH_WEAPON_5S},
	{"weapon_5t",		VEH_WEAPON_5T},
	{"weapon_5u",		VEH_WEAPON_5U},
	{"weapon_5v",		VEH_WEAPON_5V},
	{"weapon_5w",		VEH_WEAPON_5W},
	{"weapon_5x",		VEH_WEAPON_5X},
	{"weapon_5y",		VEH_WEAPON_5Y},
	{"weapon_5z",		VEH_WEAPON_5Z},
	{"weapon_5aa",		VEH_WEAPON_5AA},
	{"weapon_5an",		VEH_WEAPON_5AB},
	{"weapon_5ac",		VEH_WEAPON_5AC},
	{"weapon_5ad",		VEH_WEAPON_5AD},
	{"weapon_5ae",		VEH_WEAPON_5AE},
	{"weapon_5af",		VEH_WEAPON_5AF},
	{"weapon_5ag",		VEH_WEAPON_5AG},
	{"weapon_5ah",		VEH_WEAPON_5AH},
	{"weapon_5ai",		VEH_WEAPON_5AI},
	{"weapon_5aj",		VEH_WEAPON_5AJ},
	{"weapon_5ak",		VEH_WEAPON_5AK},

	{"weapon_5a_rot",		VEH_WEAPON_5A_ROT},
	{"weapon_5b_rot",		VEH_WEAPON_5B_ROT},
	{"weapon_5c_rot",		VEH_WEAPON_5C_ROT},
	{"weapon_5d_rot",		VEH_WEAPON_5D_ROT},
	{"weapon_5e_rot",		VEH_WEAPON_5E_ROT},
	{"weapon_5f_rot",		VEH_WEAPON_5F_ROT},
	{"weapon_5g_rot",		VEH_WEAPON_5G_ROT},
	{"weapon_5h_rot",		VEH_WEAPON_5H_ROT},
	{"weapon_5i_rot",		VEH_WEAPON_5I_ROT},
	{"weapon_5j_rot",		VEH_WEAPON_5J_ROT},
	{"weapon_5k_rot",		VEH_WEAPON_5K_ROT},
	{"weapon_5l_rot",		VEH_WEAPON_5L_ROT},
	{"weapon_5m_rot",		VEH_WEAPON_5M_ROT},
	{"weapon_5n_rot",		VEH_WEAPON_5N_ROT},
	{"weapon_5o_rot",		VEH_WEAPON_5O_ROT},
	{"weapon_5p_rot",		VEH_WEAPON_5P_ROT},
	{"weapon_5q_rot",		VEH_WEAPON_5Q_ROT},
	{"weapon_5r_rot",		VEH_WEAPON_5R_ROT},
	{"weapon_5s_rot",		VEH_WEAPON_5S_ROT},
	{"weapon_5t_rot",		VEH_WEAPON_5T_ROT},
	{"weapon_5u_rot",		VEH_WEAPON_5U_ROT},
	{"weapon_5v_rot",		VEH_WEAPON_5V_ROT},
	{"weapon_5w_rot",		VEH_WEAPON_5W_ROT},
	{"weapon_5x_rot",		VEH_WEAPON_5X_ROT},
	{"weapon_5y_rot",		VEH_WEAPON_5Y_ROT},
	{"weapon_5z_rot",		VEH_WEAPON_5Z_ROT},
	{"weapon_5aa_rot",		VEH_WEAPON_5AA_ROT},
	{"weapon_5an_rot",		VEH_WEAPON_5AB_ROT},
	{"weapon_5ac_rot",		VEH_WEAPON_5AC_ROT},
	{"weapon_5ad_rot",		VEH_WEAPON_5AD_ROT},
	{"weapon_5ae_rot",		VEH_WEAPON_5AE_ROT},
	{"weapon_5af_rot",		VEH_WEAPON_5AF_ROT},
	{"weapon_5ag_rot",		VEH_WEAPON_5AG_ROT},
	{"weapon_5ah_rot",		VEH_WEAPON_5AH_ROT},
	{"weapon_5ai_rot",		VEH_WEAPON_5AI_ROT},
	{"weapon_5aj_rot",		VEH_WEAPON_5AJ_ROT},
	{"weapon_5ak_rot",		VEH_WEAPON_5AK_ROT},


	{"weapon_6a",		VEH_WEAPON_6A},
	{"weapon_6b",		VEH_WEAPON_6B},
	{"weapon_6c",		VEH_WEAPON_6C},
	{"weapon_6d",		VEH_WEAPON_6D},
	{"weapon_6a_rot",	VEH_WEAPON_6A_ROT},
	{"weapon_6b_rot",	VEH_WEAPON_6B_ROT},
	{"weapon_6c_rot",	VEH_WEAPON_6C_ROT},
	{"weapon_6d_rot",	VEH_WEAPON_6D_ROT},
	{"turret_1base",	VEH_TURRET_1_BASE},
	{"turret_1barrel",	VEH_TURRET_1_BARREL},
	{"turret_2base",	VEH_TURRET_2_BASE},
	{"turret_2barrel",	VEH_TURRET_2_BARREL},
	{"turret_3base",	VEH_TURRET_3_BASE},
	{"turret_3barrel",	VEH_TURRET_3_BARREL},
	{"turret_4base",	VEH_TURRET_4_BASE},
	{"turret_4barrel",	VEH_TURRET_4_BARREL},
	{"ammobelt",		VEH_TURRET_AMMO_BELT},

	{"searchlight_base",	VEH_SEARCHLIGHT_BASE},
	{"searchlight_light",	VEH_SEARCHLIGHT_BARREL},

	{"attach_female",	VEH_ATTACH},
	{"roof",			VEH_ROOF},
	{"roof2",			VEH_ROOF2},

	{"soft_1",			VEH_SOFTTOP_1},
	{"soft_2",			VEH_SOFTTOP_2},
	{"soft_3",			VEH_SOFTTOP_3},
	{"soft_4",			VEH_SOFTTOP_4},
	{"soft_5",			VEH_SOFTTOP_5},
	{"soft_6",			VEH_SOFTTOP_6},
	{"soft_7",			VEH_SOFTTOP_7},
	{"soft_8",			VEH_SOFTTOP_8},
	{"soft_9",			VEH_SOFTTOP_9},
	{"soft_10",			VEH_SOFTTOP_10},
	{"soft_11",			VEH_SOFTTOP_11},
	{"soft_12",			VEH_SOFTTOP_12},
	{"soft_13",			VEH_SOFTTOP_13},

	{"pontoon_l",		VEH_PONTOON_L},
	{"pontoon_r",		VEH_PONTOON_R},

	{"forks",			VEH_FORKS},
	{"mast",			VEH_MAST},
	{"carriage",		VEH_CARRIAGE},
	{"fork_l",			VEH_FORK_LEFT},
	{"fork_r",			VEH_FORK_RIGHT},
	{"forks_attach",	VEH_FORKS_ATTACH},

	{"frame_1",			VEH_HANDLER_FRAME_1},
	{"frame_2",			VEH_HANDLER_FRAME_2},
	{"frame_3",			VEH_HANDLER_FRAME_3},
	{"frame_pickup_1",	VEH_HANDLER_FRAME_PICKUP_1},
	{"frame_pickup_2",	VEH_HANDLER_FRAME_PICKUP_2},
	{"frame_pickup_3",	VEH_HANDLER_FRAME_PICKUP_3},
	{"frame_pickup_4",	VEH_HANDLER_FRAME_PICKUP_4},

	{"no_ped_col_step_l", VEH_DODO_NO_PED_COL_STEP_L},
	{"no_ped_col_strut_1_l", VEH_DODO_NO_PED_COL_STRUT_1_L},
	{"no_ped_col_strut_2_l", VEH_DODO_NO_PED_COL_STRUT_2_L},
	{"no_ped_col_step_r", VEH_DODO_NO_PED_COL_STEP_R},
	{"no_ped_col_strut_1_r", VEH_DODO_NO_PED_COL_STRUT_1_R},
	{"no_ped_col_strut_2_r", VEH_DODO_NO_PED_COL_STRUT_2_R},

	{"freight_cont",	VEH_FREIGHTCONT2_CONTAINER},
	{"freight_bogey",	VEH_FREIGHTCONT2_BOGEY},

	{"freightgrain_slidedoor",	VEH_FREIGHTGRAIN_SLIDING_DOOR},

	{"door_hatch_r",	VEH_DOOR_HATCH_R},
	{"door_hatch_l",	VEH_DOOR_HATCH_L},

	{"tow_arm",			VEH_TOW_ARM},
	{"tow_mount_a",		VEH_TOW_MOUNT_A},
	{"tow_mount_b",		VEH_TOW_MOUNT_B},

	{"tipper",			VEH_TIPPER},

	{"combine_reel",	VEH_PICKUP_REEL},
	{"combine_auger",	VEH_AUGER},

	{"slipstream_l",	VEH_SLIPSTREAM_L},
	{"slipstream_r",	VEH_SLIPSTREAM_R},

	{"arm_1",			VEH_ARM1},
	{"arm_2",			VEH_ARM2},
	{"arm_3",			VEH_ARM3},
	{"arm_4",			VEH_ARM4},

	{"scoop",			VEH_DIGGER_ARM},
	{"bodyshell",		VEH_ARTICULATED_DIGGER_ARM_BASE},
	{"boom",			VEH_ARTICULATED_DIGGER_ARM_BOOM},
	{"stick",			VEH_ARTICULATED_DIGGER_ARM_STICK},
	{"bucket",			VEH_ARTICULATED_DIGGER_ARM_BUCKET},

	{"shovel_2",		VEH_DIGGER_SHOVEL_2},
	{"shovel_3",		VEH_DIGGER_SHOVEL_3},

	{"Lookat_UpprPiston_head",	VEH_CUTTER_ARM_1},
	{"Lookat_LowrPiston_boom",	VEH_CUTTER_ARM_2},
	{"Boom_Driver",				VEH_CUTTER_BOOM_DRIVER},
	{"cutter_driver",			VEH_CUTTER_CUTTER_DRIVER},

	{"vehicle_blocker",			VEH_VEHICLE_BLOCKER},

	{"extra_1",			VEH_EXTRA_1},
	{"extra_2",			VEH_EXTRA_2},
	{"extra_3",			VEH_EXTRA_3},
	{"extra_4",			VEH_EXTRA_4},
	{"extra_5",			VEH_EXTRA_5},
	{"extra_6",			VEH_EXTRA_6},
	{"extra_7",			VEH_EXTRA_7},
	{"extra_8",			VEH_EXTRA_8},
	{"extra_9",			VEH_EXTRA_9},
	{"extra_ten",		VEH_EXTRA_10}, // Name it to "extra_ten", as the hash of "extra_10" clashes with "headlight_r" 

	{"extra_11",		VEH_EXTRA_11},
	{"extra_12",		VEH_EXTRA_12},

	{"break_extra_1",	VEH_BREAKABLE_EXTRA_1},
	{"break_extra_2",	VEH_BREAKABLE_EXTRA_2},
	{"break_extra_3",	VEH_BREAKABLE_EXTRA_3},
	{"break_extra_4",	VEH_BREAKABLE_EXTRA_4},
	{"break_extra_5",	VEH_BREAKABLE_EXTRA_5},
	{"break_extra_6",	VEH_BREAKABLE_EXTRA_6},
	{"break_extra_7",	VEH_BREAKABLE_EXTRA_7},
	{"break_extra_8",	VEH_BREAKABLE_EXTRA_8},
	{"break_extra_9",	VEH_BREAKABLE_EXTRA_9},
	{"break_extra_10",	VEH_BREAKABLE_EXTRA_10},

	{"mod_col_1",		VEH_MOD_COLLISION_1},
	{"mod_col_2",		VEH_MOD_COLLISION_2},
	{"mod_col_3",		VEH_MOD_COLLISION_3},
	{"mod_col_4",		VEH_MOD_COLLISION_4},
	{"mod_col_5",		VEH_MOD_COLLISION_5},
	{ "mod_col_6",		VEH_MOD_COLLISION_6 },
	{ "mod_col_7",		VEH_MOD_COLLISION_7 },
	{ "mod_col_8",		VEH_MOD_COLLISION_8 },
	{ "mod_col_9",		VEH_MOD_COLLISION_9 },
	{ "mod_col_10",		VEH_MOD_COLLISION_10 },
	{ "mod_col_11",		VEH_MOD_COLLISION_11 },
	{ "mod_col_12",		VEH_MOD_COLLISION_12 },
	{ "mod_col_13",		VEH_MOD_COLLISION_13 },
	{ "mod_col_14",		VEH_MOD_COLLISION_14 },
	{ "mod_col_15",		VEH_MOD_COLLISION_15 },
	{ "mod_col_16",		VEH_MOD_COLLISION_16 },

	{"parachute_open",	VEH_PARACHUTE_OPEN},
	{"parachute_deploy",VEH_PARACHUTE_DEPLOY},

	{"wedge",			VEH_RAMMING_SCOOP},

	{"turret_1mod",	VEH_TURRET_1_MOD},
	{"turret_2mod",	VEH_TURRET_2_MOD},
	{"turret_3mod",	VEH_TURRET_3_MOD},
	{"turret_4mod",	VEH_TURRET_4_MOD},

	{"missileflap_2a",	VEH_MISSILEFLAP_2A},
	{"missileflap_2b",	VEH_MISSILEFLAP_2B},
	{"missileflap_2c",	VEH_MISSILEFLAP_2C},
	{"missileflap_2d",	VEH_MISSILEFLAP_2D},
	{"missileflap_2e",	VEH_MISSILEFLAP_2E},
	{"missileflap_2f",	VEH_MISSILEFLAP_2F},

	{"extendable_l",	VEH_EXTENDABLE_SIDE_L},
	{"extendable_r",	VEH_EXTENDABLE_SIDE_R},

	{"extendable_lf",	VEH_EXTENDABLE_A},
	{"extendable_rf",	VEH_EXTENDABLE_B},
	{"extendable_lr",	VEH_EXTENDABLE_C},
	{"extendable_rr",	VEH_EXTENDABLE_D},

	{ "rotator_l",	VEH_ROTATOR_L },
	{ "rotator_r",	VEH_ROTATOR_R },
	
	{"mod_a",	VEH_MOD_A},
	{"mod_b",	VEH_MOD_B},
	{"mod_c",	VEH_MOD_C},
	{"mod_d",	VEH_MOD_D},
	{"mod_e",	VEH_MOD_E},
	{"mod_f",	VEH_MOD_F},
	{"mod_g",	VEH_MOD_G},
	{"mod_h",	VEH_MOD_H},
	{"mod_i",	VEH_MOD_I},
	{"mod_j",	VEH_MOD_J},
	{"mod_k",	VEH_MOD_K},
	{"mod_l",	VEH_MOD_L},
	{"mod_m",	VEH_MOD_M},
	{"mod_n",	VEH_MOD_N},
	{"mod_o",	VEH_MOD_O},
	{"mod_p",	VEH_MOD_P},
	{"mod_q",	VEH_MOD_Q},
	{"mod_r",	VEH_MOD_R},
	{"mod_s",	VEH_MOD_S},
	{"mod_t",	VEH_MOD_T},
	{"mod_u",	VEH_MOD_U},
	{"mod_v",	VEH_MOD_V},
	{"mod_w",	VEH_MOD_W},
	{"mod_x",	VEH_MOD_X},
	{"mod_y",	VEH_MOD_Y},
	{"mod_z",	VEH_MOD_Z},
	{"mod_aa",	VEH_MOD_AA},
	{"mod_ab",	VEH_MOD_AB},
	{"mod_ac",	VEH_MOD_AC},
	{"mod_ad",	VEH_MOD_AD},
	{"mod_ae",	VEH_MOD_AE},
	{"mod_af",	VEH_MOD_AF},
	{"mod_ag",	VEH_MOD_AG},
	{"mod_ah",	VEH_MOD_AH},
	{"mod_ai",	VEH_MOD_AI},
	{"mod_aj",	VEH_MOD_AJ},
	{"mod_ak",	VEH_MOD_AK},

	{"turret_a1mod",	VEH_TURRET_A1_MOD},
	{"turret_a2mod",	VEH_TURRET_A2_MOD},
	{"turret_a3mod",	VEH_TURRET_A3_MOD},
	{"turret_a4mod",	VEH_TURRET_A4_MOD},

	{"turret_b1mod",	VEH_TURRET_B1_MOD},
	{"turret_b2mod",	VEH_TURRET_B2_MOD},
	{"turret_b3mod",	VEH_TURRET_B3_MOD},
	{"turret_b4mod",	VEH_TURRET_B4_MOD},

	{"turret_a1base",	VEH_TURRET_A1_BASE},
	{"turret_a1barrel",	VEH_TURRET_A1_BARREL},
	{"turret_a2base",	VEH_TURRET_A2_BASE},
	{"turret_a2barrel",	VEH_TURRET_A2_BARREL},
	{"turret_a3base",	VEH_TURRET_A3_BASE},
	{"turret_a3barrel",	VEH_TURRET_A3_BARREL},
	{"turret_a4base",	VEH_TURRET_A4_BASE},
	{"turret_a4barrel",	VEH_TURRET_A4_BARREL},
	{"a_ammobelt",		VEH_TURRET_A_AMMO_BELT},

	{"turret_b1base",	VEH_TURRET_B1_BASE},
	{"turret_b1barrel",	VEH_TURRET_B1_BARREL},
	{"turret_b2base",	VEH_TURRET_B2_BASE},
	{"turret_b2barrel",	VEH_TURRET_B2_BARREL},
	{"turret_b3base",	VEH_TURRET_B3_BASE},
	{"turret_b3barrel",	VEH_TURRET_B3_BARREL},
	{"turret_b4base",	VEH_TURRET_B4_BASE},
	{"turret_b4barrel",	VEH_TURRET_B4_BARREL},
	{"b_ammobelt",		VEH_TURRET_B_AMMO_BELT},

	{"suspension_lm1",	VEH_SUSPENSION_LM1},
	{"suspension_rm1",	VEH_SUSPENSION_RM1},
	{"transmission_m1",	VEH_TRANSMISSION_M1},

	{"leg_fl",	VEH_LEG_FL},
	{"leg_fr",	VEH_LEG_FR},
	{"leg_rl",	VEH_LEG_RL},
	{"leg_rr",	VEH_LEG_RR},

	{"panel_l1",	VEH_PANEL_L1},
	{"panel_l2",	VEH_PANEL_L2},
	{"panel_l3",	VEH_PANEL_L3},
	{"panel_l4",	VEH_PANEL_L4},
	{"panel_l5",	VEH_PANEL_L5},

	{"panel_r1",	VEH_PANEL_R1},
	{"panel_r2",	VEH_PANEL_R2},
	{"panel_r3",	VEH_PANEL_R3},
	{"panel_r4",	VEH_PANEL_R4},
	{"panel_r5",	VEH_PANEL_R5},

	{"f_wing_l",	VEH_FOLDING_WING_L},
	{"f_wing_r",	VEH_FOLDING_WING_R},

	{"stand",		VEH_STAND},

	{"pbusspeaker", VEH_SPEAKER},

	{ "nozzles_f",	VEH_NOZZLE_F },
	{ "nozzles_r",	VEH_NOZZLE_R },

	// Extra arena mode bones
	{ "rblade_1mod",	VEH_BLADE_R_1_MOD },
	{ "rblade_1fast",	VEH_BLADE_R_1_FAST },

	{ "rblade_2mod",	VEH_BLADE_R_2_MOD },
	{ "rblade_2fast",	VEH_BLADE_R_2_FAST },

	{ "rblade_3mod",	VEH_BLADE_R_3_MOD },
	{ "rblade_3fast",	VEH_BLADE_R_3_FAST },

	{ "fblade_1mod",	VEH_BLADE_F_1_MOD },
	{ "fblade_1fast",	VEH_BLADE_F_1_FAST },

	{ "fblade_2mod",	VEH_BLADE_F_2_MOD },
	{ "fblade_2fast",	VEH_BLADE_F_2_FAST },

	{ "fblade_3mod",	VEH_BLADE_F_3_MOD },
	{ "fblade_3fast",	VEH_BLADE_F_3_FAST },

	{ "sblade_1mod",	VEH_BLADE_S_1_MOD },
	{ "sblade_l_1fast",	VEH_BLADE_S_1_L_FAST },
	{ "sblade_r_1fast",	VEH_BLADE_S_1_R_FAST },

	{ "sblade_2mod",	VEH_BLADE_S_2_MOD },
	{ "sblade_l_2fast",	VEH_BLADE_S_2_L_FAST },
	{ "sblade_r_2fast",	VEH_BLADE_S_2_R_FAST },

	{ "sblade_3mod",	VEH_BLADE_S_3_MOD },
	{ "sblade_l_3fast",	VEH_BLADE_S_3_L_FAST },
	{ "sblade_r_3fast",	VEH_BLADE_S_3_R_FAST },

	{ "spike_1mod",		VEH_SPIKE_1_MOD },
	{ "spike_1ped_col",	VEH_SPIKE_1_PED },
	{ "spike1car_col",	VEH_SPIKE_1_CAR },

	{ "spike_2mod",		VEH_SPIKE_2_MOD },
	{ "spike_2ped_col",	VEH_SPIKE_2_PED },
	{ "spike_2car_col",	VEH_SPIKE_2_CAR },

	{ "spike_3mod",		VEH_SPIKE_3_MOD },
	{ "spike_3ped_col",	VEH_SPIKE_3_PED },
	{ "spike_3car_col",	VEH_SPIKE_3_CAR },

	{ "scoop_1mod",		VEH_SCOOP_1_MOD },
	{ "scoop_2mod",		VEH_SCOOP_2_MOD },
	{ "scoop_3mod",		VEH_SCOOP_3_MOD },

	{ "miscwobble_1",		VEH_BOBBLE_MISC_1 },
	{ "miscwobble_2",		VEH_BOBBLE_MISC_2 },
	{ "miscwobble_3",		VEH_BOBBLE_MISC_3 },
	{ "miscwobble_4",		VEH_BOBBLE_MISC_4 },
	{ "miscwobble_5",		VEH_BOBBLE_MISC_5 },
	{ "miscwobble_6",		VEH_BOBBLE_MISC_6 },
	{ "miscwobble_7",		VEH_BOBBLE_MISC_7 },
	{ "miscwobble_8",		VEH_BOBBLE_MISC_8 },
		
	{ "supercharger_1",		VEH_SUPERCHARGER_1 },
	{ "supercharger_2",		VEH_SUPERCHARGER_2 },
	{ "supercharger_3",		VEH_SUPERCHARGER_3 },

	{ "ramp_1mod",		VEH_RAMP_1_MOD },
	{ "ramp_2mod",		VEH_RAMP_2_MOD },
	{ "ramp_3mod",		VEH_RAMP_3_MOD },

	{ "tombstone",		VEH_TOMBSTONE },

	{ "spike_1modf",		VEH_FRONT_SPIKE_1 },
	{ "spike_2modf",		VEH_FRONT_SPIKE_2 },
	{ "spike_3modf",		VEH_FRONT_SPIKE_3 },

	{ "ram_1mod",		VEH_RAMMING_BAR_1 },
	{ "ram_2mod",		VEH_RAMMING_BAR_2 },
	{ "ram_3mod",		VEH_RAMMING_BAR_3 },
	{ "ram_4mod",		VEH_RAMMING_BAR_4 },

	{0,0}
};

static ObjectNameIdAssociation quadBikeIds[QUADBIKE_NUM_OVERRIDDEN_NODES + 1] =
{
	{"handlebars",		QUADBIKE_HANDLEBARS},
	{"forks_u",			QUADBIKE_FORKS_U},
	{"forks_l",			QUADBIKE_FORKS_L},
	{0,0}
};

static ObjectNameIdAssociation bikeIds[BIKE_NUM_OVERRIDDEN_NODES + 1] =
{
	{"chassis",			BIKE_CHASSIS},
	{"forks_u",			BIKE_FORKS_U},
	{"forks_l",			BIKE_FORKS_L},
	{"handlebars",		BIKE_HANDLEBARS},
	{"wheel_f",			BIKE_WHEEL_F},
	{"swingarm",		BIKE_SWINGARM},
	{"wheel_r",			BIKE_WHEEL_R},
	{0,0}
};

static ObjectNameIdAssociation bmxIds[BMX_NUM_OVERRIDDEN_NODES + 1] =
{
	{"chassis",			BMX_CHASSIS},
	{"forks_u",			BMX_FORKS_U},
	{"forks_l",			BMX_FORKS_L},
	{"wheel_lf",		BMX_WHEEL_F},
	{"wheel_lr",		BMX_WHEEL_R},
	{"transmission_r",	BMX_SWINGARM},
	{"handlebars",		BMX_HANDLEBARS},
	{"crank",			BMX_CHAINSET},
	{"pedal_r",			BMX_PEDAL_L},
	{"pedal_l",			BMX_PEDAL_R},
	{0,0}
};


static ObjectNameIdAssociation boatIds[BOAT_NUM_OVERRIDDEN_NODES + 1] =
{
	{"static_prop",		BOAT_STATIC_PROP},
	{"moving_prop",		BOAT_MOVING_PROP},
	{"static_prop2",	BOAT_STATIC_PROP2},
	{"moving_prop2",	BOAT_MOVING_PROP2},
	{"rudder",			BOAT_RUDDER},
	{"rudder2",			BOAT_RUDDER2},
	{"handlebars",		BOAT_HANDLEBARS},
	{0,0}
};

static ObjectNameIdAssociation trainIds[TRAIN_NUM_OVERRIDDEN_NODES + 1] =
{
	{"wheel_rf1_dummy",			TRAIN_WHEEL_RF1},
	{"wheel_rf2_dummy",			TRAIN_WHEEL_RF2},
	{"wheel_rf3_dummy",			TRAIN_WHEEL_RF3},
	{"wheel_rb1_dummy",			TRAIN_WHEEL_RR1},
	{"wheel_rb2_dummy",			TRAIN_WHEEL_RR2},
	{"wheel_rb3_dummy",			TRAIN_WHEEL_RR3},
	{"wheel_lf1_dummy",			TRAIN_WHEEL_LF1},
	{"wheel_lf2_dummy",			TRAIN_WHEEL_LF2},
	{"wheel_lf3_dummy",			TRAIN_WHEEL_LF3},
	{"wheel_lb1_dummy",			TRAIN_WHEEL_LR1},
	{"wheel_lb2_dummy",			TRAIN_WHEEL_LR2},
	{"wheel_lb3_dummy",			TRAIN_WHEEL_LR3},
	{"bogie_front",				TRAIN_BOGIE_FRONT},
	{"bogie_rear",				TRAIN_BOGIE_REAR},
	{"interior",				TRAIN_INTERIOR},
#if !__FINAL
	{"armyboxcar_slidedoor",	TRAIN_BOXCAR_SLIDE_DOOR},
	{"armyboxcar_slidedoor2",	TRAIN_BOXCAR_SLIDE_DOOR2},
#endif // #if !__FINAL
	{0,0}
};

static ObjectNameIdAssociation heliIds[HELI_NUM_OVERRIDDEN_NODES + 1] =
{
	{"rotor_main",		HELI_ROTOR_MAIN},
	{"rotor_rear", 		HELI_ROTOR_REAR},
	{"rotor_main_2",	HELI_ROTOR_MAIN_2},
	{"rotor_rear_2",	HELI_ROTOR_REAR_2},
	{"rudder",		 	HELI_RUDDER},
	{"elevators", 		HELI_ELEVATORS},
	{"tail",			HELI_TAIL},
	{"outriggers_l",	HELI_OUTRIGGERS_L},
	{"outriggers_r",	HELI_OUTRIGGERS_R},
	{"rope_attach_a",	HELI_ROPE_ATTACH_A},
	{"rope_attach_b",	HELI_ROPE_ATTACH_B},
	{"extra_7",			HELI_NOSE_VARIANT_A},
	{"extra_8",			HELI_NOSE_VARIANT_B},
	{"extra_9",			HELI_NOSE_VARIANT_C},
	{"wing_l",			HELI_WING_L},
	{"wing_r",			HELI_WING_R},
	{"airbrake_l",		HELI_AIRBRAKE_L},
	{"airbrake_r",		HELI_AIRBRAKE_R},
	{"afterburner",		HELI_AFTERBURNER},
	{"afterburner_2",	HELI_AFTERBURNER_2},
    {"hbgrip_low_l",	HELI_HBGRIP_LOW_L},
    {"hbgrip_low_r",	HELI_HBGRIP_LOW_R},
	
	{0,0}
};

static ObjectNameIdAssociation blimpIds[BLIMP_NUM_OVERRIDDEN_NODES + 1] =
{
	{"prop_1",			BLIMP_PROP_1},
	{"prop_2",			BLIMP_PROP_2},
	{"elevator_l", 		BLIMP_ELEVATOR_L},
	{"elevator_r", 		BLIMP_ELEVATOR_R},
	{"rudder_l", 		BLIMP_RUDDER_UPPER},
	{"rudder_r", 		BLIMP_RUDDER_LOWER},
	{"misc_c", 			BLIMP_SHELL},
	{"misc_d", 			BLIMP_SHELL_FRAME_1},
	{"misc_e", 			BLIMP_SHELL_FRAME_2},
	{"misc_f", 			BLIMP_SHELL_FRAME_3},
	{"misc_g", 			BLIMP_SHELL_FRAME_4},
	{"engine_l",		BLIMP_ENGINE_L},
	{"engine_r",		BLIMP_ENGINE_R},
	{0,0}
};

static ObjectNameIdAssociation planeIds[PLANE_NUM_OVERRIDDEN_NODES + 1] =
{
	{"prop_1",			PLANE_PROP_1},
	{"prop_2",			PLANE_PROP_2},
	{"prop_3", 			PLANE_PROP_3},
	{"prop_4", 			PLANE_PROP_4},
	{"prop_5", 			PLANE_PROP_5},
	{"prop_6", 			PLANE_PROP_6},
	{"prop_7", 			PLANE_PROP_7},
	{"prop_8", 			PLANE_PROP_8},
	{"rudder",			PLANE_RUDDER},
	{"rudder_2",		PLANE_RUDDER_2},
	{"elevator_l", 		PLANE_ELEVATOR_L},
	{"elevator_r", 		PLANE_ELEVATOR_R},
	{"aileron_l", 		PLANE_AILERON_L},
	{"aileron_r",		PLANE_AILERON_R},
	{"airbrake_l", 		PLANE_AIRBRAKE_L},
	{"airbrake_r",		PLANE_AIRBRAKE_R},
	{"wing_l",			PLANE_WING_L},
	{"wing_r",			PLANE_WING_R},
	{"wing_lr",			PLANE_WING_RL},
	{"wing_rr",			PLANE_WING_RR},
	{"tail",			PLANE_TAIL},
	{"engine_l",		PLANE_ENGINE_L},
	{"engine_r",		PLANE_ENGINE_R}, 
	{"nozzles_f",		PLANE_NOZZLE_F},
	{"nozzles_r",		PLANE_NOZZLE_R},
	{"afterburner",		PLANE_AFTERBURNER},
	{"afterburner_2",	PLANE_AFTERBURNER_2},
	{"afterburner_3",	PLANE_AFTERBURNER_3},
	{"afterburner_4",	PLANE_AFTERBURNER_4},
	{"wingtip_1",		PLANE_WINGTIP_1},
	{"wingtip_2",		PLANE_WINGTIP_2},
	{"exhaustvent",		PLANE_EXHAUST_VENT},
	{"handlebars",		PLANE_HANDLEBARS},
	{"wingflap_l",		PLANE_WINGFLAP_L},
	{"wingflap_r",		PLANE_WINGFLAP_R},
	{0,0}
};

static ObjectNameIdAssociation landingGearIds[ LANDING_GEAR_NUM_OVERRIDDEN_NODES + 1 ] = 
{
	{"gear_door_fl",	LANDING_GEAR_DOOR_FL},
	{"gear_door_fr",	LANDING_GEAR_DOOR_FR},
	{"gear_door_rl1",	LANDING_GEAR_DOOR_RL1},
	{"gear_door_rr1",	LANDING_GEAR_DOOR_RR1},
	{"gear_door_rl2",	LANDING_GEAR_DOOR_RL2},
	{"gear_door_rr2",	LANDING_GEAR_DOOR_RR2},
	{"gear_door_rml",	LANDING_GEAR_DOOR_RML},
	{"gear_door_rmr",	LANDING_GEAR_DOOR_RMR},
	{"gear_f",			LANDING_GEAR_F},
	{"gear_rl",			LANDING_GEAR_RL},
	{"gear_lm1",		LANDING_GEAR_LM1},
	{"gear_rr",			LANDING_GEAR_RR},
	{"gear_rm1",		LANDING_GEAR_RM1},
	{"gear_rm",			LANDING_GEAR_RM},
	{0,0}
};

static ObjectNameIdAssociation subIds[SUB_NUM_OVERRIDDEN_NODES + 1] =
{
	{"prop_1",			SUB_PROPELLER_1},
	{"prop_2",			SUB_PROPELLER_2},
	{"prop_3",			SUB_PROPELLER_3},
	{"prop_4",			SUB_PROPELLER_4},
	{"prop_left",		SUB_PROPELLER_LEFT},
	{"prop_right",		SUB_PROPELLER_RIGHT},
	{"rudder",			SUB_RUDDER},
	{"rudder2",			SUB_RUDDER2},
	{"elevator_l",		SUB_ELEVATOR_L},
	{"elevator_r",		SUB_ELEVATOR_R},
	{0,0}	
};

static ObjectNameIdAssociation trailerIds[TRAILER_NUM_OVERRIDDEN_NODES +1] = 
{
	{"legs",			TRAILER_LEGS},
	{"attach_male",		TRAILER_ATTACH},
	{0, 0}
};

static ObjectNameIdAssociation draftVehIds[DRAFTVEH_NUM_OVERRIDDEN_NODES +1] = 
{
	{"draft_animal_attach_lr",		DRAFTVEH_ANIMAL_ATTACH_LR},
	{"draft_animal_attach_rr",		DRAFTVEH_ANIMAL_ATTACH_RR},
	{"draft_animal_attach_lm",		DRAFTVEH_ANIMAL_ATTACH_LM},
	{"draft_animal_attach_rm",		DRAFTVEH_ANIMAL_ATTACH_RM},
	{"draft_animal_attach_lf",		DRAFTVEH_ANIMAL_ATTACH_LF},
	{"draft_animal_attach_rf",		DRAFTVEH_ANIMAL_ATTACH_RF},
	{0, 0}
};

static ObjectNameIdAssociation submarineCarIds[SUBMARINECAR_NUM_OVERRIDDEN_NODES +1] = 
{
	{"static_prop",			SUBMARINECAR_PROPELLER_1},
	{"static_prop2",		SUBMARINECAR_PROPELLER_2},
	{"rudder",				SUBMARINECAR_RUDDER_1},
	{"rudder2",				SUBMARINECAR_RUDDER_2},
	{"elevator_l",			SUBMARINECAR_ELEVATOR_L},
	{"elevator_r",			SUBMARINECAR_ELEVATOR_R},
	{"wheelcover_l",		SUBMARINECAR_WHEELCOVER_L},
	{"wheelcover_r",		SUBMARINECAR_WHEELCOVER_R},
    {"fin_mount",		    SUBMARINECAR_FIN_MOUNT},
    {"dplane_l",		    SUBMARINECAR_DRIVE_PLANE_L},
    {"dplane_r",		    SUBMARINECAR_DRIVE_PLANE_R},
	{0,0}	
};

static ObjectNameIdAssociation amphibiousAutomobileIds[AMPHIBIOUS_AUTOMOBILE_NUM_OVERRIDDEN_NODES +1] = 
{
	{"static_prop",		AMPHIBIOUS_AUTOMOBILE_STATIC_PROP},
	{"moving_prop",		AMPHIBIOUS_AUTOMOBILE_MOVING_PROP},
	{"static_prop2",	AMPHIBIOUS_AUTOMOBILE_STATIC_PROP2},
	{"moving_prop2",	AMPHIBIOUS_AUTOMOBILE_MOVING_PROP2},
	{"rudder",			AMPHIBIOUS_AUTOMOBILE_RUDDER},
	{"rudder2",			AMPHIBIOUS_AUTOMOBILE_RUDDER2},
	{0,0}	
};

static ObjectNameIdAssociation amphibiousQuadIds[AMPHIBIOUS_QUAD_NUM_OVERRIDDEN_NODES +1] = 
{
	{"handlebars",		QUADBIKE_HANDLEBARS},
	{"static_prop",		AMPHIBIOUS_AUTOMOBILE_STATIC_PROP},
	{0,0}	
};


// --- Static class members -----------------------------------------------------
ObjectNameIdAssociation* CVehicleFactory::pBmxModelOverrideDesc = bmxIds;

ObjectNameIdAssociation* CVehicleFactory::ms_modelDesc[VEHICLE_TYPE_NUM] = 
{
	CVehicleFactory::pVehModelDesc,
	CVehicleFactory::pPlaneModelOverrideDesc,
	CVehicleFactory::pTrailerModelOverrideDesc,
	CVehicleFactory::pQuadBikeModelOverrideDesc,
	CVehicleFactory::pDraftVehModelOverrideDesc,
	CVehicleFactory::pSubmarineCarModelOverrideDesc,
	CVehicleFactory::pAmphibiousAutomobileModelOverrideDesc,
	CVehicleFactory::pAmphibiousQuadModelOverrideDesc,
	CVehicleFactory::pHeliModelOverrideDesc,
	CVehicleFactory::pBlimpModelOverrideDesc,
	CVehicleFactory::pHeliModelOverrideDesc,
	CVehicleFactory::pBikeModelOverrideDesc,
	CVehicleFactory::pBmxModelOverrideDesc,
	CVehicleFactory::pBoatModelOverrideDesc,
	CVehicleFactory::pTrainModelOverrideDesc,
	CVehicleFactory::pSubModelOverrideDesc,
};

u16* CVehicleFactory::ms_modelDescHashes[VEHICLE_TYPE_NUM] = 
{
	CVehicleFactory::pVehModelDescHashes,
	CVehicleFactory::pPlaneModelOverrideDescHashes,
	CVehicleFactory::pTrailerModelOverrideDescHashes,
	CVehicleFactory::pQuadBikeModelOverrideDescHashes,
	CVehicleFactory::pDraftVehModelOverrideDescHashes,
	CVehicleFactory::pSubmarineCarModelOverrideDescHashes,
	CVehicleFactory::pAmphibiousAutomobileModelOverrideDescHashes,
	CVehicleFactory::pAmphibiousQuadModelOverrideDescHashes,
	CVehicleFactory::pHeliModelOverrideDescHashes,
	CVehicleFactory::pBlimpModelOverrideDescHashes,
	CVehicleFactory::pHeliModelOverrideDescHashes,
	CVehicleFactory::pBikeModelOverrideDescHashes,
	CVehicleFactory::pBmxModelOverrideDescHashes,
	CVehicleFactory::pBoatModelOverrideDescHashes,
	CVehicleFactory::pTrainModelOverrideDescHashes,
	CVehicleFactory::pSubModelOverrideDescHashes	
};

// +1 for the all option
#define BANK_VEHICLE_TYPES (VEHICLE_TYPE_NUM+1)
#define BANK_VEHICLE_TYPE_ALL (VEHICLE_TYPE_NUM)

const char* CVehicleFactory::ms_modelTypes[] = 
{
	"Vehicle",
	"Plane",
	"Trailer",
	"QuadBike",
	"Draft",
	"Submarine Car",
	"Amphibious Automobile",
	"Amphibious QuadBike",
	"Heli",
	"Blimp",
	"AutoGyro",
	"Bike",
	"Bmx",
	"Boat",
	"Train",
	"Sub",
	"All"
};

#define BANK_VEHICLE_SWANK (SWANKNESS_MAX+1)
#define BANK_VEHICLE_SWANK_ALL (SWANKNESS_MAX)

const char* CVehicleFactory::ms_swankTypes[BANK_VEHICLE_SWANK] = 
{
	"Crap",
	"Nice crap",
	"Meh",
	"Decent",
	"Nice",
	"Super nice",
	"All"
};

#define BANK_VEHICLE_CLASS (VC_MAX + 1)
#define BANK_VEHICLE_CLASS_ALL (VC_MAX)
const char* CVehicleFactory::ms_classTypes[ BANK_VEHICLE_CLASS ] =
{
	"COMPACT",
	"SEDAN",
	"SUV",
	"COUPE",
	"MUSCLE",
	"SPORT_CLASSIC",
	"SPORT",
	"SUPER",
	"MOTORCYCLE",
	"OFF_ROAD",
	"INDUSTRIAL",
	"UTILITY",
	"VAN",
	"CYCLE",
	"BOAT",
	"HELICOPTER",
	"PLANE",
	"SERVICE",
	"EMERGENCY",
	"MILITARY",
	"COMMERCIAL",
	"RAIL",
    "OPEN WHEEL",
	"ALL"
};

CompileTimeAssert(VEHICLE_TYPE_NUM == (sizeof(CVehicleFactory::ms_modelDesc)/sizeof(ObjectNameIdAssociation*)));	// If this fails someone has added a new vehicle type. Check array above!
CompileTimeAssert(VEHICLE_TYPE_NUM == (sizeof(CVehicleFactory::ms_modelDescHashes)/sizeof(u16*)));	// If this fails someone has added a new vehicle type. Check array above!
CompileTimeAssert(BANK_VEHICLE_TYPES == (sizeof(CVehicleFactory::ms_modelTypes)/sizeof(const char*)));	// If this fails someone has added a new vehicle type. Check array above!

CVehicleFactory* CVehicleFactory::sm_pInstance = NULL;

ObjectNameIdAssociation* CVehicleFactory::pVehModelDesc = carIds;
ObjectNameIdAssociation* CVehicleFactory::pQuadBikeModelOverrideDesc = quadBikeIds;
ObjectNameIdAssociation* CVehicleFactory::pBikeModelOverrideDesc = bikeIds;
ObjectNameIdAssociation* CVehicleFactory::pBoatModelOverrideDesc = boatIds;
ObjectNameIdAssociation* CVehicleFactory::pTrainModelOverrideDesc = trainIds;
ObjectNameIdAssociation* CVehicleFactory::pHeliModelOverrideDesc = heliIds;
ObjectNameIdAssociation* CVehicleFactory::pBlimpModelOverrideDesc = blimpIds;
ObjectNameIdAssociation* CVehicleFactory::pPlaneModelOverrideDesc = planeIds;
ObjectNameIdAssociation* CVehicleFactory::pSubModelOverrideDesc = subIds;
ObjectNameIdAssociation* CVehicleFactory::pTrailerModelOverrideDesc = trailerIds;
ObjectNameIdAssociation* CVehicleFactory::pDraftVehModelOverrideDesc = draftVehIds;
ObjectNameIdAssociation* CVehicleFactory::pSubmarineCarModelOverrideDesc = submarineCarIds;
ObjectNameIdAssociation* CVehicleFactory::pAmphibiousAutomobileModelOverrideDesc = amphibiousAutomobileIds;
ObjectNameIdAssociation* CVehicleFactory::pAmphibiousQuadModelOverrideDesc = amphibiousQuadIds;

ObjectNameIdAssociation* CVehicleFactory::pLandingGearModelOverrideDesc = landingGearIds;

u16 CVehicleFactory::pVehModelDescHashes[VEH_NUM_NODES];
u16 CVehicleFactory::pQuadBikeModelOverrideDescHashes[QUADBIKE_NUM_OVERRIDDEN_NODES];
u16 CVehicleFactory::pBikeModelOverrideDescHashes[BIKE_NUM_OVERRIDDEN_NODES];
u16 CVehicleFactory::pBmxModelOverrideDescHashes[BMX_NUM_OVERRIDDEN_NODES];
u16 CVehicleFactory::pBoatModelOverrideDescHashes[BOAT_NUM_OVERRIDDEN_NODES];
u16 CVehicleFactory::pTrainModelOverrideDescHashes[TRAIN_NUM_OVERRIDDEN_NODES];
u16 CVehicleFactory::pHeliModelOverrideDescHashes[HELI_NUM_OVERRIDDEN_NODES];
u16 CVehicleFactory::pBlimpModelOverrideDescHashes[BLIMP_NUM_OVERRIDDEN_NODES];
u16 CVehicleFactory::pPlaneModelOverrideDescHashes[PLANE_NUM_OVERRIDDEN_NODES];
u16 CVehicleFactory::pSubModelOverrideDescHashes[SUB_NUM_OVERRIDDEN_NODES];
u16 CVehicleFactory::pTrailerModelOverrideDescHashes[TRAILER_NUM_OVERRIDDEN_NODES];
u16 CVehicleFactory::pDraftVehModelOverrideDescHashes[DRAFTVEH_NUM_OVERRIDDEN_NODES];
u16 CVehicleFactory::pSubmarineCarModelOverrideDescHashes[SUBMARINECAR_NUM_OVERRIDDEN_NODES];
u16 CVehicleFactory::pAmphibiousAutomobileModelOverrideDescHashes[AMPHIBIOUS_AUTOMOBILE_NUM_OVERRIDDEN_NODES];
u16 CVehicleFactory::pAmphibiousQuadModelOverrideDescHashes[AMPHIBIOUS_QUAD_NUM_OVERRIDDEN_NODES];
u16 CVehicleFactory::pLandingGearModelOverrideDescHashes[LANDING_GEAR_NUM_OVERRIDDEN_NODES];


CVehicleFactory::sDestroyedVeh CVehicleFactory::ms_reuseDestroyedVehArray[MAX_DESTROYED_VEHS_CACHED];
bool					CVehicleFactory::ms_reuseDestroyedVehs			= true;		// when destroying vehicles we cache them for a while and reuse them when the same type is spawned again
u32						CVehicleFactory::ms_reuseDestroyedVehCacheTime	= 10000;	// time in ms how long we should keep cached vehicles around
u32						CVehicleFactory::ms_reuseDestroyedVehCount		= 0;		// keep track of how many vehicles we have cached

bool					CVehicleFactory::ms_bFarDrawVehicles = false;
u8						CVehicleFactory::ms_uCurrentBoatTrailersWithoutBoats = 0;
u8						CVehicleFactory::ms_uCurrentBoatTrailersWithBoats = 0;


// --- Debug stuff? ----
#if __BANK
	bool CVehicleFactory::ms_reuseDestroyedVehsDebugOutput = false;
	bool CVehicleFactory::ms_rearDoorsCanOpen = false;

	// vehicle combo box variables

	s32 CVehicleFactory::numDLCNames = 0;
	bkCombo* CVehicleFactory::pLayoutCombo = NULL;
	bkCombo* CVehicleFactory::pDLCCombo = NULL;
	atArray<const char*> CVehicleFactory::emptyNames;
	s32 CVehicleFactory::currVehicleNameSelection = 0;
	s32 CVehicleFactory::currVehicleTypeSelection = BANK_VEHICLE_TYPE_ALL;
	s32 CVehicleFactory::currVehicleLayoutSelection = 0;
	s32 CVehicleFactory::currVehicleDLCSelection = 0;
	s32 CVehicleFactory::currVehicleSwankSelection = BANK_VEHICLE_SWANK_ALL;
	s32 CVehicleFactory::currVehicleClassSelection = BANK_VEHICLE_CLASS_ALL;
	s32 CVehicleFactory::currVehicleTrailerNameSelection = 0;
	atArray<const char*> CVehicleFactory::vehicleNames;

	static s32*			vehicleSorts;
	static u32			numVehicleNames;
	static s32*			vehicleTrailerSorts;
	static u32			numVehicleTrailerNames;
	static bkBank*		pBank = NULL;
	static bkButton*	pCreateBankButton = NULL;
	static bkCombo*		pVehicleCombo = NULL;
	static bkCombo*		pVehicleTrailerCombo = NULL;
	static bool			bNeedToUpdateVehicleList = false; //Do we need to update the vehicle list?
	static bool			bVehicleNeedsToBeHotwired = false;
	static bool			bVehicleInstantStartEngineOnWarp = true;
	static bool			bCreateAsMissionCar = false;
	static bool			bCreateAsPersonalCar = false;
	static bool			bCreateAsGangCar = false;
	static bool			bPlaceOnRoadProperly = false;
	static bool			bLockDoors = false;
	static int			nForceExtraOn = 0;
	static bool			bBulletProof = false;
	static bool			bFireProof = false;
	static bool			bCollisionProof = false;
	static bool			bMeleeProof = false;
	static bool			bExplosionProof = false;
 
	#define CAR_NAME_LENGTH (16)
	static s32 numCarsAvailable = 0;
	u32 CVehicleFactory::carModelId = fwModelId::MI_INVALID;
	u32 CVehicleFactory::trailerModelId = fwModelId::MI_INVALID;
	//static s32 carModelNum = 0;
	static float carDropHeight = 5.0f;
	static float carTestHeight = -1.5f;
	static float carDropRotation = 0.0f;
	static bool bUseDebugCamRotation = true;
	static s32 carSoundHornTime = 0;

	static bool bLowerOrRaiseRoofInstantly = false;
	static bool bDoorCanBeReset = false;

	static bool sbCheatAbs = false;
	static bool sbCheatTractionControl = false;
	static bool sbCheatStabilityControl = false;
	static bool sbCheatGrip1 = false;
	static bool sbCheatGrip2 = false;
	static bool sbCheatPower1 = false;
	static bool sbCheatPower2 = false;
	static float sfTopSpeedAdjustment = 0.0f;
#if __DEV
	static float sfPetrolTankLevel = 65.0f;
#endif
	float gfPlayerCarInvMassScale = 1.8f;

	RegdVeh CVehicleFactory::ms_pLastCreatedVehicle(NULL);
	RegdVeh CVehicleFactory::ms_pLastLastCreatedVehicle(NULL);
	bool CVehicleFactory::ms_bFireCarForcePos = false;
	bool CVehicleFactory::ms_bDisplayVehicleNames = false;
	bool CVehicleFactory::ms_bDisplayVehicleLayout = false;
	bool CVehicleFactory::ms_bDisplayCreateLocation = false;
	bool CVehicleFactory::ms_bRegenerateFireVehicle = true;
    bool CVehicleFactory::ms_bDisplayTyreWearRate = false;
	bool CVehicleFactory::ms_bDisplaySteerAngle = false;
	bool CVehicleFactory::ms_bPlayAnimPhysical = false;
	bool CVehicleFactory::ms_bSpawnTrailers = true;
	bool CVehicleFactory::ms_bForceHd = false;
	float CVehicleFactory::ms_fLodMult = 1.f;
	s8 CVehicleFactory::ms_clampedRenderLod = -1;
	bool CVehicleFactory::ms_bflyingAce = false;
	s32 CVehicleFactory::ms_iSeatToEnter = 0;
	s32 CVehicleFactory::ms_iDoorToOpen = 0;
	s32 CVehicleFactory::ms_iTyreNumber = 0;
	s32 CVehicleFactory::ms_variationDebugColor = 0;
	s32 CVehicleFactory::ms_variationDebugWindowTint = -1;
	bool CVehicleFactory::ms_variationAllExtras = false;
	bool CVehicleFactory::ms_variationDebugDraw = false;
	bool CVehicleFactory::ms_variationExtras[VEH_LAST_EXTRA - VEH_EXTRA_1 + 1] = {false};
	bkSlider* CVehicleFactory::ms_variationColorSlider = NULL;
	bkSlider* CVehicleFactory::ms_variationWindowTintSlider = NULL;
	float CVehicleFactory::ms_fSetEngineNewHealth = ENGINE_HEALTH_MAX;

	// mods
	s32 CVehicleFactory::ms_variationMod[] = {  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
												-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
												-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
												-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
												-1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

	bkSlider* CVehicleFactory::ms_variationSliders[] = {	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
															NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
															NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
															NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
															NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };



	//s32 CVehicleFactory::ms_variationSpoilerMod = -1;
	//s32 CVehicleFactory::ms_variationBumperFMod = -1;
	//s32 CVehicleFactory::ms_variationBumperRMod = -1;
	//s32 CVehicleFactory::ms_variationSkirtMod = -1;
	//s32 CVehicleFactory::ms_variationExhaustMod = -1;
	//s32 CVehicleFactory::ms_variationChassisMod = -1;
	//s32 CVehicleFactory::ms_variationGrillMod = -1;
	//s32 CVehicleFactory::ms_variationBonnetMod = -1;
	//s32 CVehicleFactory::ms_variationWingLMod = -1;
	//s32 CVehicleFactory::ms_variationWingRMod = -1;
	//s32 CVehicleFactory::ms_variationRoofMod = -1;
	//s32 CVehicleFactory::ms_variationEngineMod = -1;
	//s32 CVehicleFactory::ms_variationBrakesMod = -1;
	//s32 CVehicleFactory::ms_variationGearboxMod = -1;
	//s32 CVehicleFactory::ms_variationHornMod = -1;
	//s32 CVehicleFactory::ms_variationSuspensionMod = -1;
	//s32 CVehicleFactory::ms_variationArmourMod = -1;
	//s32 CVehicleFactory::ms_variationWheelMod = -1;
	//s32 CVehicleFactory::ms_variationRearWheelMod = -1;
	//s32 CVehicleFactory::ms_variationLiveryMod = -1;
	//bkSlider* CVehicleFactory::ms_variationSpoilerSlider = NULL;
	//bkSlider* CVehicleFactory::ms_variationBumperFSlider = NULL;
	//bkSlider* CVehicleFactory::ms_variationBumperRSlider = NULL;
	//bkSlider* CVehicleFactory::ms_variationSkirtSlider = NULL;
	//bkSlider* CVehicleFactory::ms_variationExhaustSlider = NULL;
	//bkSlider* CVehicleFactory::ms_variationChassisSlider = NULL;
	//bkSlider* CVehicleFactory::ms_variationGrillSlider = NULL;
	//bkSlider* CVehicleFactory::ms_variationBonnetSlider = NULL;
	//bkSlider* CVehicleFactory::ms_variationWingLSlider = NULL;
	//bkSlider* CVehicleFactory::ms_variationWingRSlider = NULL;
	//bkSlider* CVehicleFactory::ms_variationRoofSlider = NULL;
	//bkSlider* CVehicleFactory::ms_variationEngineSlider = NULL;
	//bkSlider* CVehicleFactory::ms_variationBrakesSlider = NULL;
	//bkSlider* CVehicleFactory::ms_variationGearboxSlider = NULL;
	//bkSlider* CVehicleFactory::ms_variationHornSlider = NULL;
	//bkSlider* CVehicleFactory::ms_variationSuspensionSlider = NULL;
	//bkSlider* CVehicleFactory::ms_variationArmourSlider = NULL;
	//bkSlider* CVehicleFactory::ms_variationWheelSlider = NULL;
	//bkSlider* CVehicleFactory::ms_variationRearWheelSlider = NULL;
	//bkSlider* CVehicleFactory::ms_variationLiveryModSlider = NULL;

	bkSlider* CVehicleFactory::ms_liverySlider = NULL;
	bkSlider* CVehicleFactory::ms_livery2Slider = NULL;
	s32 CVehicleFactory::ms_currentLivery = -1;
	s32 CVehicleFactory::ms_currentLivery2 = -1;
	float CVehicleFactory::ms_vehicleLODScaling = 1.0f;
	float CVehicleFactory::ms_vehicleHiStreamScaling = 1.0f;
	bool CVehicleFactory::ms_bShowVehicleLODLevelData = false;
	bool CVehicleFactory::ms_bShowVehicleLODLevel = false;
	bool CVehicleFactory::ms_variationRenderMods = true;
	bool CVehicleFactory::ms_variationRandomMods = true;

	u32 CVehicleFactory::ms_numCachedCarsCreated = 0;
	u32 CVehicleFactory::ms_numNonCachedCarsCreated = 0;
	bool CVehicleFactory::ms_resetCacheStats = false;

	bool CVehicleFactory::ms_bLogCreatedVehicles = false;
	bool CVehicleFactory::ms_bLogDestroyedVehicles = false;
	u32 CVehicleFactory::ms_iDestroyedVehicleCount = 0;
	bool CVehicleFactory::ms_bForceDials = false;


	static Vector3 sFireCarPos(VEC3_ZERO);
	static Vector3 sFireCarDirn(1.0f, 0.0f, 0.0f);
	static Vector3 sFireCarOrient(VEC3_ZERO);
	static Vector3 sFireObjectOffset(0.0f, 10.0f, 0.0f);
	static float sFireCarVel = 5.0f;
	static Vector3 svFireCarSpin(VEC3_ZERO);

	static Vector3 sRoofImpulse(VEC3_ZERO);
	static float sDoorOpenAmount = 1.0f;

	static bool sSpawnCarsOnTrailer = true;
    static bool sSpawnCarsWithNoBreaking = false;

CVehicle *CVehicleFactory::GetCreatedVehicle()
{
	return (CVehicleFactory::ms_pLastCreatedVehicle);
}
CVehicle *CVehicleFactory::GetPreviouslyCreatedVehicle()
{
	return (CVehicleFactory::ms_pLastLastCreatedVehicle);
}
#endif // __BANK


	// --- Code ---------------------------------------------------------------------

//
// name:		CVehicleFactory::CVehicleFactory
// description:	Constructor
CVehicleFactory::CVehicleFactory()
{
#if (__XENON || __PS3)
	#define VEHICLE_HEAP_SIZE (464 << 10)
#else
	#define VEHICLE_HEAP_SIZE ((464 << 10) * 10)
#endif

	sysMemStartFlex();
	CVehicle::InitPool(VEHICLE_HEAP_SIZE, VEHICLE_POOL_SIZE, MEMBUCKET_GAMEPLAY);
	sysMemEndFlex();

#if __DEV
	CVehicle::GetPool()->RegisterPoolCallback(CVehicle::PoolFullCallback);
#endif // __DEV

	CWheel::InitPool( MEMBUCKET_GAMEPLAY );

#if (__XENON || __PS3)
	#define VEHICLE_AUDIO_HEAP_SIZE (488 << 10)
#else
	#define VEHICLE_AUDIO_HEAP_SIZE ((488 << 10) * 10)
#endif

	audVehicleAudioEntity::InitPool(VEHICLE_AUDIO_HEAP_SIZE, VEHICLE_POOL_SIZE, MEMBUCKET_AUDIO);

	BANK_ONLY(CWheel::ms_bUseWheelIntegrationTask = !PARAM_disablewheelintegrationtask.Get();)

	CAircraftDamage::DamageFlame::InitPool( MEMBUCKET_GAMEPLAY );

	for (s32 i = 0; i < VEH_NUM_NODES; ++i)
        if (pVehModelDesc[i].pName)
            pVehModelDescHashes[i] = atHash16U(pVehModelDesc[i].pName);
	for (s32 i = 0; i < QUADBIKE_NUM_OVERRIDDEN_NODES; ++i)
        if (pQuadBikeModelOverrideDesc[i].pName)
            pQuadBikeModelOverrideDescHashes[i] = atHash16U(pQuadBikeModelOverrideDesc[i].pName);
	for (s32 i = 0; i < BIKE_NUM_OVERRIDDEN_NODES; ++i)
        if (pBikeModelOverrideDesc[i].pName)
            pBikeModelOverrideDescHashes[i] = atHash16U(pBikeModelOverrideDesc[i].pName);
	for (s32 i = 0; i < BMX_NUM_OVERRIDDEN_NODES; ++i)
        if (pBmxModelOverrideDesc[i].pName)
            pBmxModelOverrideDescHashes[i] = atHash16U(pBmxModelOverrideDesc[i].pName);
	for (s32 i = 0; i < BOAT_NUM_OVERRIDDEN_NODES; ++i)
        if (pBoatModelOverrideDesc[i].pName)
            pBoatModelOverrideDescHashes[i] = atHash16U(pBoatModelOverrideDesc[i].pName);
	for (s32 i = 0; i < TRAIN_NUM_OVERRIDDEN_NODES; ++i)
        if (pTrainModelOverrideDesc[i].pName)
            pTrainModelOverrideDescHashes[i] = atHash16U(pTrainModelOverrideDesc[i].pName);
	for (s32 i = 0; i < HELI_NUM_OVERRIDDEN_NODES; ++i)
        if (pHeliModelOverrideDesc[i].pName)
            pHeliModelOverrideDescHashes[i] = atHash16U(pHeliModelOverrideDesc[i].pName);
	for (s32 i = 0; i < BLIMP_NUM_OVERRIDDEN_NODES; ++i)
        if (pBlimpModelOverrideDesc[i].pName)
            pBlimpModelOverrideDescHashes[i] = atHash16U(pBlimpModelOverrideDesc[i].pName);
	for (s32 i = 0; i < PLANE_NUM_OVERRIDDEN_NODES; ++i)
        if (pPlaneModelOverrideDesc[i].pName)
            pPlaneModelOverrideDescHashes[i] = atHash16U(pPlaneModelOverrideDesc[i].pName);
	for (s32 i = 0; i < SUB_NUM_OVERRIDDEN_NODES; ++i)
        if (pSubModelOverrideDesc[i].pName)
            pSubModelOverrideDescHashes[i] = atHash16U(pSubModelOverrideDesc[i].pName);
	for (s32 i = 0; i < TRAILER_NUM_OVERRIDDEN_NODES; ++i)
        if (pTrailerModelOverrideDesc[i].pName)
            pTrailerModelOverrideDescHashes[i] = atHash16U(pTrailerModelOverrideDesc[i].pName);
	for (s32 i = 0; i < DRAFTVEH_NUM_OVERRIDDEN_NODES; ++i)
        if (pDraftVehModelOverrideDesc[i].pName)
            pDraftVehModelOverrideDescHashes[i] = atHash16U(pDraftVehModelOverrideDesc[i].pName);
	for (s32 i = 0; i < SUBMARINECAR_NUM_OVERRIDDEN_NODES; ++i)
		if (pSubmarineCarModelOverrideDesc[i].pName)
			pSubmarineCarModelOverrideDescHashes[i] = atHash16U(pSubmarineCarModelOverrideDesc[i].pName);
	for (s32 i = 0; i < AMPHIBIOUS_AUTOMOBILE_NUM_OVERRIDDEN_NODES; ++i)
		if (pAmphibiousAutomobileModelOverrideDesc[i].pName)
			pAmphibiousAutomobileModelOverrideDescHashes[i] = atHash16U(pAmphibiousAutomobileModelOverrideDesc[i].pName);
	for (s32 i = 0; i < AMPHIBIOUS_QUAD_NUM_OVERRIDDEN_NODES; ++i)
		if (pAmphibiousQuadModelOverrideDesc[i].pName)
			pAmphibiousQuadModelOverrideDescHashes[i] = atHash16U(pAmphibiousQuadModelOverrideDesc[i].pName);
	for (s32 i = 0; i < LANDING_GEAR_NUM_OVERRIDDEN_NODES; ++i)
		if (pLandingGearModelOverrideDesc[i].pName)
			pLandingGearModelOverrideDescHashes[i] = atHash16U(pLandingGearModelOverrideDesc[i].pName);
}

//
// name:		CVehicleFactory::~CVehicleFactory
// description:	Destructor
CVehicleFactory::~CVehicleFactory()
{
	CVehicle::ShutdownPool();
	CWheel::ShutdownPool();

	ClearDestroyedVehCache();
	CAircraftDamage::DamageFlame::ShutdownPool();
#if __BANK
	vehicleNames.Reset();

	delete []vehicleSorts;
	delete []vehicleTrailerSorts;
	vehicleSorts = vehicleTrailerSorts = NULL;
#endif //__BANK
}


void CVehicleFactory::CreateFactory()
{
	Assert(sm_pInstance == NULL);
	sm_pInstance = rage_new CVehicleFactory;
}

void CVehicleFactory::DestroyFactory()
{
	Assert(sm_pInstance);

	delete sm_pInstance;
	sm_pInstance = NULL;
}


//
// name:		CVehicleFactory::Create
// description:	Default factory create function. Individual projects can override this
// parameters:	vehicleType
//				modelIndex
//				created
CVehicle* CVehicleFactory::Create(fwModelId modelId, const eEntityOwnedBy ownedBy, const u32 popType, const Matrix34 *pMat, bool bClone, bool bCreateAsInactive)
{
	PF_AUTO_PUSH_TIMEBAR("Vehicle Create");
	CVehicle* pReturnVehicle = NULL;
	CVehicleModelInfo* pModelInfo = ((CVehicleModelInfo* )CModelInfo::GetBaseModelInfo(modelId));
	Assert(pModelInfo);
	Assert(pModelInfo->GetModelType()==MI_TYPE_VEHICLE);

	if(pModelInfo==NULL)
	{
		Warningf("CVehicleFactory::Create pModelInfo==NULL --> return NULL");
		return NULL;
	}

#if __BANK
	if (pModelInfo->GetVehicleType() == VEHICLE_TYPE_TRAILER && !ms_bSpawnTrailers)
	{
		Warningf("CVehicleFactory::Create pModelInfo->GetVehicleType() == VEHICLE_TYPE_TRAILER && !ms_bSpawnTrailers --> return NULL");
		return NULL;
	}
#endif

	if(!FRAGCACHEMGR->CanGetNewCacheEntries(1))
	{
		Warningf("CVehicleFactory::Create FRAGCACHEMGR->CanGetNewCacheEntries(1) == false --> return NULL");
		return NULL;
	}

	bool wasCached = false;
#if 0
	if (ownedBy != ENTITY_OWNEDBY_SCRIPT)
		pReturnVehicle = CreateVehFromDestroyedCache(modelId, ownedBy, popType, pMat, bCreateAsInactive);
#endif

	if (!pReturnVehicle)
	{
		pReturnVehicle = CreateVehicle(pModelInfo->GetVehicleType(), ownedBy, popType);

		Assert(pReturnVehicle && !pReturnVehicle->GetIsRetainedByInteriorProxy());
		Assert(pReturnVehicle && pReturnVehicle->GetOwnerEntityContainer() == NULL);

		if(pReturnVehicle==NULL)
		{
			Warningf("CVehicleFactory::Create pReturnVehicle==NULL --> return NULL");
			return NULL;
		}

		CVehiclePopulation::IncrementNumVehiclesCreatedThisCycle();

		pReturnVehicle->m_nVehicleFlags.bCreatedByFactory = true;

		if(pMat)
		{
			pReturnVehicle->SetMatrix(*pMat);
#if DEBUG_DRAW
			pReturnVehicle->m_vCreatedPos = RCC_VEC3V(pMat->d);
			pReturnVehicle->m_vCreatedDir = RCC_VEC3V(pMat->b);
#endif
		}
		else
		{
			Matrix34 identity(Matrix34::IdentityType);
			pReturnVehicle->SetMatrix(identity);
		}

		// SetModelId will try to activate unless this is set.
		pReturnVehicle->m_nVehicleFlags.bCreatingAsInactive = bCreateAsInactive;

		// call SetModelId() AFTER we init the matrix so that physics can be initialized properly
		pReturnVehicle->SetModelId(modelId);

		SetFarDraw(pReturnVehicle);

		BANK_ONLY(ms_numNonCachedCarsCreated++;)
	}
	else
	{
		wasCached = true;
		BANK_ONLY(ms_numCachedCarsCreated++;)
	}

	if (pModelInfo && pModelInfo->GetVehicleType() == VEHICLE_TYPE_TRAILER && pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPAWN_BOAT_ON_TRAILER))
	{
		++ms_uCurrentBoatTrailersWithoutBoats;
		boatTrailerDebugf3("Create: added trailer, ms_uCurrentBoatTrailersWithoutBoats = %u", ms_uCurrentBoatTrailersWithoutBoats);
#if !__NO_OUTPUT		
		if(ms_uCurrentBoatTrailersWithBoats > ms_uMaxBoatTrailersWithBoats)
		{
			boatTrailerWarningf("Too many trailers with boats (%u, max %u)", ms_uCurrentBoatTrailersWithBoats, ms_uMaxBoatTrailersWithBoats);
		}
		if(ms_uCurrentBoatTrailersWithoutBoats > (ms_uMaxBoatTrailersWithoutBoats + 1)) // Allow one extra, as it could turn into a trailer with a boat.
		{				
			boatTrailerWarningf("Too many trailers without boats (%u, max %u + 1)", ms_uCurrentBoatTrailersWithoutBoats, ms_uMaxBoatTrailersWithoutBoats);
		}
#endif // !__NO_OUTPUT
	}

	// Shouldn't already be in the spatial array at this point, regardless of whether they were created
	// fresh or came from the cache.
	Assert(!pReturnVehicle->GetSpatialArrayNode().IsInserted());

	Assert(!pReturnVehicle->GetIsRetainedByInteriorProxy());
	Assert(pReturnVehicle->GetOwnerEntityContainer() == NULL);

	// ******* NETWORK REGISTRATION HAS TO BE DONE BEFORE ANY FURTHER STATE CHANGES ON THE VEHICLE! *******************

	////////////////////
	// THIS SHOULD REALLY BE SPLIT BETWEEN PROJECT SPECIFIC AND CORE VEHICLE TYPES IN THEIR RESPECTIVE VEHICLE FACTORIES
	////////////////////

	pReturnVehicle->m_nPhysicalFlags.bNotToBeNetworked = !bClone;

	if (bClone && NetworkInterface::IsGameInProgress())
	{		
		if(!RegisterVehicleToNetwork(pReturnVehicle, popType))
		{
			// vehicle could not be registered as there is no available network objects for it, so it has to be removed
			// unless it is a mission object, which is queued to be registered later
			Destroy(pReturnVehicle);
			pReturnVehicle = NULL;
		}
	}

	if (pReturnVehicle)
	{
		if (!wasCached)
			AssignVehicleGadgets(pReturnVehicle);

		// modify the vehicle dirt level based on the popzone values
		float   dirtMin = CPopCycle::GetCurrentZoneVehDirtMin();
		float   dirtMax = CPopCycle::GetCurrentZoneVehDirtMax();
		Color32 dirtCol = CPopCycle::GetCurrentZoneDirtCol();

		if (dirtMin >= 0.f && dirtMax >= 0.f)
		{
			u32 vehDirtMin = pModelInfo->GetDefaultDirtLevelMin();
			u32 vehDirtMax = pModelInfo->GetDefaultDirtLevelMax();
			u32 popDirtMin = (u32)(dirtMin * 15.f);
			u32 popDirtMax = (u32)(dirtMax * 15.f);
			float resDirt = (float)fwRandom::GetRandomNumberInRange((int)(vehDirtMin + popDirtMin) / 2, (int)(vehDirtMax + popDirtMax) / 2);
			resDirt = Clamp(resDirt, 0.0f, 15.0f);
			pReturnVehicle->SetBodyDirtLevel(resDirt);
		}

		if (dirtCol.GetColor() != 0)
		{
			// modify dirt tint based on weather modifier
			float dirtWeatherMod = g_timeCycle.GetStaleDirtModifier();
			Color32 col(dirtCol.GetRedf() * dirtWeatherMod, dirtCol.GetGreenf() * dirtWeatherMod, dirtCol.GetBluef() * dirtWeatherMod, dirtCol.GetAlphaf());

			pReturnVehicle->SetBodyDirtColor(col);
		}

		// Make it snow
		const u32	xmasWeather = g_weather.GetTypeIndex("XMAS");
		const bool	isXmasWeather = (g_weather.GetPrevTypeIndex() == xmasWeather) || (g_weather.GetNextTypeIndex() == xmasWeather);
		if((CNetwork::IsGameInProgress() REPLAY_ONLY(|| CReplayMgr::AllowXmasSnow())) && isXmasWeather)
		{
			Color32 snowCol(240,240,240,255);
			pReturnVehicle->SetBodyDirtColor(snowCol);
		}

		CSpatialArrayNode& spatialArrayNode = pReturnVehicle->GetSpatialArrayNode();
		Assert(!spatialArrayNode.IsInserted());
		CVehicle::GetSpatialArray().Insert(spatialArrayNode);
		if(spatialArrayNode.IsInserted())
		{
			pReturnVehicle->UpdateInSpatialArray();
		}

		pReturnVehicle->m_vecInitialSpawnPosition = VECTOR3_TO_VEC3V(pMat->GetVector(3));
	
		pReturnVehicle->UpdateBodyColourRemapping(false);
	}


#if __BANK
	if(ms_bLogCreatedVehicles && pReturnVehicle)
	{
		CViewport* gameViewport = gVpMan.GetGameViewport();
		if(gameViewport)
		{
			static u32 iCount = 0;
			char text[128];

			const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(pReturnVehicle->GetTransform().GetPosition());
			const Vector3 vCameraPos = VEC3V_TO_VECTOR3(gameViewport->GetGrcViewport().GetCameraPosition());
			const float fDist = (vCameraPos - vVehiclePos).Mag();

			sprintf(text, "%i : %s", iCount, pReturnVehicle->GetModelName());
			grcDebugDraw::Text(vVehiclePos, Color_green, 0, -grcDebugDraw::GetScreenSpaceTextHeight(), text, false, -(s32)(10.0f / fwTimer::GetTimeStep()));
			grcDebugDraw::Sphere(vVehiclePos, 0.25f, Color_green4, false, -(s32)(10.0f / fwTimer::GetTimeStep()));

			Displayf("**************************************************************************************************");
			Displayf("CVehicleFactory::Create() : %i", iCount);
			Displayf("NAME : %s, DISTANCE : %.1f, POSITION : %.1f, %.1f, %.1f, VEHICLE: 0x%p, RANGESCALE : %.1f", pReturnVehicle->GetModelName(), fDist, vVehiclePos.x, vVehiclePos.y, vVehiclePos.z, pReturnVehicle, CVehiclePopulation::GetPopulationRangeScale());
			sysStack::PrintStackTrace();
			Displayf("**************************************************************************************************");

			iCount++;
		}
	}
#endif // __BANK

	return pReturnVehicle;
}


bool CVehicleFactory::RegisterVehicleToNetwork(CVehicle* pReturnVehicle, const u32 popType)
{	
	bool bRegistered = false;
	NetObjFlags globalFlags = (popType == POPTYPE_MISSION) ? CNetObjGame::GLOBALFLAG_SCRIPTOBJECT : 0;

	switch (pReturnVehicle->GetVehicleType())
	{
	case VEHICLE_TYPE_CAR:
		bRegistered = NetworkInterface::RegisterObject((CAutomobile*)pReturnVehicle, 0, globalFlags) ;
		break;
	case VEHICLE_TYPE_BIKE:
		bRegistered = NetworkInterface::RegisterObject((CBike*)pReturnVehicle, 0, globalFlags);
		break;
	case VEHICLE_TYPE_QUADBIKE:
		bRegistered = NetworkInterface::RegisterObject((CQuadBike*)pReturnVehicle, 0, globalFlags);
		break;
	case VEHICLE_TYPE_BICYCLE:
		bRegistered = NetworkInterface::RegisterObject((CBmx*)pReturnVehicle, 0, globalFlags);
		break;
	case VEHICLE_TYPE_TRAIN:
		bRegistered = NetworkInterface::RegisterObject((CTrain*)pReturnVehicle, 0, globalFlags, true);
		break;
	case VEHICLE_TYPE_BOAT:
		bRegistered = NetworkInterface::RegisterObject((CBoat*)pReturnVehicle, 0, globalFlags);
		break;
	case VEHICLE_TYPE_HELI:
		bRegistered = NetworkInterface::RegisterObject((CHeli*)pReturnVehicle, 0, globalFlags);
		break;
	case VEHICLE_TYPE_PLANE:
		bRegistered = NetworkInterface::RegisterObject((CPlane*)pReturnVehicle, 0, globalFlags);
		break;
	case VEHICLE_TYPE_AUTOGYRO:
		bRegistered = NetworkInterface::RegisterObject((CAutogyro*)pReturnVehicle, 0, globalFlags);
		break;
	case VEHICLE_TYPE_SUBMARINE:
		bRegistered = NetworkInterface::RegisterObject((CSubmarine*)pReturnVehicle, 0, globalFlags);
		break;
	case VEHICLE_TYPE_TRAILER:
		bRegistered = NetworkInterface::RegisterObject((CTrailer*)pReturnVehicle, 0, globalFlags);
		break;
#if ENABLE_DRAFT_VEHICLE
	case VEHICLE_TYPE_DRAFT:
		bRegistered = NetworkInterface::RegisterObject((CDraftVeh*)pReturnVehicle, 0, globalFlags);
		break;
#endif
	case VEHICLE_TYPE_SUBMARINECAR:
		bRegistered = NetworkInterface::RegisterObject((CSubmarineCar*)pReturnVehicle, 0, globalFlags);
		break;
	case VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE:
		bRegistered = NetworkInterface::RegisterObject((CAmphibiousAutomobile*)pReturnVehicle, 0, globalFlags);
		break;
	case VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE:
		bRegistered = NetworkInterface::RegisterObject((CAmphibiousQuadBike*)pReturnVehicle, 0, globalFlags);
		break;
	case VEHICLE_TYPE_BLIMP:
		bRegistered = NetworkInterface::RegisterObject((CBlimp*)pReturnVehicle, 0, globalFlags);
		break;
	default:
		Assert(0);
	}

	if (!bRegistered && !(popType == POPTYPE_MISSION || popType == POPTYPE_PERMANENT))
	{
#if __ASSERT
		Warningf("CVehicleFactory::Create !bRegistered && !(popType == POPTYPE_MISSION || popType == POPTYPE_PERMANENT) -- train[%d] --> Destroy and pReturnVehicle = NULL bRegistered[%d] popType[%d]",pReturnVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN,bRegistered,popType);
#if __BANK
		Warningf("CVehicleFactory::Create Note: GetLastRegistrationFailureReason[%s]",NetworkInterface::GetLastRegistrationFailureReason());

		int typecount = 0;
		CVehicle::Pool* pool = CVehicle::GetPool();
		s32 i = (s32) pool->GetSize();
		while(i--)
		{
			CVehicle* pVehicle = pool->GetSlot(i);
			if( pVehicle && (pReturnVehicle->GetVehicleType() == pVehicle->GetVehicleType()) )
				typecount++;
		}

		netGameObjectWrapper<CDynamicEntity> wrapper(pReturnVehicle);
		NetworkObjectType objectType = static_cast<NetworkObjectType>(wrapper.GetNetworkObjectType());

		u32 numLocalObjectsOwnedOfType   = CNetworkObjectPopulationMgr::GetNumLocalObjects(objectType);
		u32 numRemoteObjectsOfType		 = CNetworkObjectPopulationMgr::GetNumRemoteObjects(objectType);
		u32 maxObjectsAllowedToOwnOfType = CNetworkObjectPopulationMgr::GetMaxObjectsAllowedToOwnOfType(objectType);

		bool bIsOwningTooManyObjects = NetworkInterface::GetObjectManager().IsOwningTooManyObjects();

		int totalNumVehicles = CNetworkObjectPopulationMgr::GetTotalNumVehicles();

		Warningf("CVehicleFactory::Create Note: typecount[%d] numLocalObjectsOwnedOfType[%d] numRemoteObjectsOfType[%d] maxObjectsAllowedToOwnOfType[%d] bIsOwningTooManyObjects[%d] TotalNumVehicles[%d] MAX_NUM_NETOBJVEHICLES[%d]",typecount,numLocalObjectsOwnedOfType,numRemoteObjectsOfType,maxObjectsAllowedToOwnOfType,bIsOwningTooManyObjects,totalNumVehicles,MAX_NUM_NETOBJVEHICLES);
#endif
#endif

		// vehicle could not be registered as there is no available network objects for it, so it has to be removed
		// unless it is a mission object, which is queued to be registered later
		// returning false here means the vehicle will be destroyed
		return false;
	}
	

	return true;
}

//
// name:		CVehicleFactory::CreateAndAttachTrailer
// description:	Creates a trailer and attaches it to the vehicle specified
// parameters:	trailer model id
//				target vehicle pointer to attach trailer to
//				vehicle creation matrix
CVehicle* CVehicleFactory::CreateAndAttachTrailer(fwModelId trailerId, CVehicle* targetVehicle, const Matrix34& creationMatrix, const eEntityOwnedBy ownedBy, const u32 popType, const bool bExtendBoxesForCollisionTests)
{
	int parentBone = targetVehicle->GetBoneIndex(VEH_ATTACH);
	CVehicle* trailer = NULL;
	if (trailerId.IsValid() && parentBone > -1)
	{
		if (!CModelInfo::HaveAssetsLoaded(trailerId))
		{
			// if this is a boat trailer, don't request it unless we have boats loaded
			CVehicleModelInfo* tmi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(trailerId);
			if (tmi && tmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPAWN_BOAT_ON_TRAILER))
			{
				if (gPopStreaming.GetLoadedBoats().CountMembers() <= 0)
				{
					return NULL;
				}
			}

			CModelInfo::RequestAssets(trailerId, STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
			Assert(targetVehicle);
			trailerId = targetVehicle->GetVehicleModelInfo()->GetRandomLoadedTrailer();
			if (!trailerId.IsValid())
				return NULL;

			if (CNetwork::IsGameInProgress() && !CVehicle::IsTypeAllowedInMP(trailerId.GetModelIndex()))
				return NULL;
		}

		CVehicleModelInfo* trailerModelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(trailerId);
		Assertf(trailerModelInfo, "Invalid model info for trailer id %d (%s)", trailerId.GetModelIndex(), trailerModelInfo->GetModelName());
		if (trailerModelInfo && trailerModelInfo->GetStructure())
		{
			if (trailerModelInfo->GetDistanceSqrToClosestInstance(creationMatrix.d, false) < (trailerModelInfo->GetIdenticalModelSpawnDistance() * trailerModelInfo->GetIdenticalModelSpawnDistance()))
				return NULL;

			if ( trailerModelInfo && trailerModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPAWN_BOAT_ON_TRAILER) )
			{
				const bool bTooManyTrailersWithoutBoats = ms_uCurrentBoatTrailersWithoutBoats > ms_uMaxBoatTrailersWithoutBoats; // Strictly greater-than; if it's equal, allow it, as it could turn into a trailer with a boat
				const bool bAtMaximumTrailersWithBoats  = ms_uCurrentBoatTrailersWithBoats >= ms_uMaxBoatTrailersWithBoats;
				const bool bTrailerWithoutBoatCanAddBoat = (ms_uCurrentBoatTrailersWithoutBoats <= ms_uMaxBoatTrailersWithoutBoats && !bAtMaximumTrailersWithBoats);
				
				// If this is a boat trailer, ensure we are not over the maximum number.
				if ( bTooManyTrailersWithoutBoats || !bTrailerWithoutBoatCanAddBoat )  
				{
					boatTrailerDebugf3("CreateAndAttachTrailer: Disallowed trailer creation, ms_uCurrentBoatTrailersWithoutBoats = %u, ms_uCurrentTrailersWithBoats = %u", ms_uCurrentBoatTrailersWithoutBoats, ms_uCurrentBoatTrailersWithBoats);
					return NULL;
				}
			}

			int trailerBone = trailerModelInfo->GetBoneIndex(TRAILER_ATTACH);

			// Assert that it is possible to attach this vehicle to other vehicle
			Assertf(trailerBone > -1, "This trailer has no attach point: %s", trailerModelInfo->GetModelName());
			if(trailerBone > -1)
			{
				// simulate attach to parent to get the matrix
				Matrix34 matNew(Matrix34::IdentityType);

				Mat34V trailerMtx;
				trailerModelInfo->GetFragType()->GetSkeletonData().ComputeObjectTransform(trailerBone, trailerMtx);
				matNew.Transpose3x4(RCC_MATRIX34(trailerMtx));

				Matrix34 matParent;
				targetVehicle->GetGlobalMtx(parentBone, matParent);
				matNew.Dot(matParent);

				// check trailer for collision and spawn it only if there's room
				Assert(trailerModelInfo->GetFragType());
				phBound *bound = (phBound *)trailerModelInfo->GetFragType()->GetPhysics(0)->GetCompositeBounds();
				int nTestTypeFlags = ArchetypeFlags::GTA_BOX_VEHICLE_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_PED_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE;
				// extend bbox check 2m down (and 2m up)
				const Vector3 vecExtendBoxMinLarge(-0.25f,-1.5f,-2.0f);
				const Vector3 vecExtendBoxMaxLarge(0.25f, 0.25f, 0.0f);
				const Vector3 vecExtendBoxMinReg(0.0f,0.0f,-2.0f);
				const Vector3 vecExtendBoxMaxReg(0.0f, 0.0f, 0.0f);

				const Vector3 vecExtendBoxMin = bExtendBoxesForCollisionTests ? vecExtendBoxMinLarge : vecExtendBoxMinReg;
				const Vector3 vecExtendBoxMax = bExtendBoxesForCollisionTests ? vecExtendBoxMaxLarge : vecExtendBoxMaxReg;

				WorldProbe::CShapeTestBoundingBoxDesc bboxDesc;
				bboxDesc.SetBound(bound);
				bboxDesc.SetTransform(&matNew);
				bboxDesc.SetIncludeFlags(nTestTypeFlags);
				bboxDesc.SetExcludeEntity(targetVehicle);
				bboxDesc.SetExtensionToBoundingBoxMin(vecExtendBoxMin);
				bboxDesc.SetExtensionToBoundingBoxMax(vecExtendBoxMax);

				if(!WorldProbe::GetShapeTestManager()->SubmitTest(bboxDesc) BANK_ONLY(|| ms_pLastCreatedVehicle == targetVehicle))
				{
					if (CModelInfo::HaveAssetsLoaded(trailerId))
					{
						trailer = CVehicleFactory::GetFactory()->Create(trailerId, ownedBy, popType, &creationMatrix);

						// warp the trailer onto the vehicle
						if (trailer && trailer->GetVehicleType() == VEHICLE_TYPE_TRAILER)
						{
							((CTrailer*)trailer)->AttachToParentVehicle(targetVehicle, true);
							targetVehicle->m_nVehicleFlags.bCanMakeIntoDummyVehicle = CVehicleAILodManager::ms_bAllowTrailersToConvertFromReal;
							trailer->m_nVehicleFlags.bCanMakeIntoDummyVehicle = CVehicleAILodManager::ms_bAllowTrailersToConvertFromReal;

							// Make sure to set bHasParentVehicle. This normally happens at the beginning of the trailer's intelligence update,
							// but we need it now so the trailer and its parent can both go superdummy from the start.
							Assertf(trailer->GetAttachParentVehicleDummyOrPhysical(), "Expected valid trailer attachment.");
							trailer->m_nVehicleFlags.bHasParentVehicle = true;
						}
						else if (trailer)
						{
							Destroy(trailer);
							trailer = NULL;
						}
					}
				}
			}
		}
	}

#if !__NO_OUTPUT
	if (trailer)
	{
		CVehicleModelInfo* tmi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(trailerId);
		if (tmi && tmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPAWN_BOAT_ON_TRAILER))
		{
			// ms_uCurrentBoatTrailersWithoutBoats incremented in Create() above.
			boatTrailerDebugf3("CreateAndAttachTrailer: added trailer, ms_uCurrentBoatTrailersWithoutBoats = %u", ms_uCurrentBoatTrailersWithoutBoats);
			if(ms_uCurrentBoatTrailersWithBoats > ms_uMaxBoatTrailersWithBoats)
			{
				boatTrailerWarningf("Too many trailers with boats (%u, max %u)", ms_uCurrentBoatTrailersWithBoats, ms_uMaxBoatTrailersWithBoats);
			}
			if(ms_uCurrentBoatTrailersWithoutBoats > (ms_uMaxBoatTrailersWithoutBoats + 1)) // Allow one extra, as it could turn into a trailer with a boat.
			{				
				boatTrailerWarningf("Too many trailers without boats (%u, max %u + 1)", ms_uCurrentBoatTrailersWithoutBoats, ms_uMaxBoatTrailersWithoutBoats);
			}
			
		}
	}
#endif // !__NO_OUTPUT

	return trailer;
}

float ProbeForWheelPosition(const CVehicle* trailer, const Vector3& carPos, const Vector3& wheelOffset, float wheelHeightAboveGround)
{
	const float probeOffsetZ = 2.f;
	float bestZ = carPos.z;

	const int nInitialNumResults = 8;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<nInitialNumResults> probeResults;
	probeDesc.SetResultsStructure(&probeResults);
	Vector3 probeStart = carPos;
	probeStart.x += wheelOffset.x;
	probeStart.y += wheelOffset.y;
	Vector3 probeEnd = probeStart;
	probeEnd.z -= probeOffsetZ;
	probeDesc.SetStartAndEnd(probeStart, probeEnd);
	probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
	probeDesc.SetIncludeEntity(trailer);
	WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
	int nBestHit = -1;
	float fBestDeltaZ = LARGE_FLOAT;

	WorldProbe::ResultIterator it;
	int hit = 0;
	for(it = probeResults.begin(); it < probeResults.end(); ++it, ++hit)
	{
		if(it->GetHitDetected() && rage::Abs((it->GetHitPosition() - carPos).z) < fBestDeltaZ)
		{
			nBestHit = hit;
			fBestDeltaZ = rage::Abs((it->GetHitPosition() - carPos).z);
		}
	}

	if(nBestHit > -1)
		bestZ = probeResults[nBestHit].GetHitPosition().z + wheelHeightAboveGround;

	return bestZ;
}

void ProbeForBoatPosition(CVehicle* spawnBoat, Matrix34* boatMat, const Vector3& trailerPos, const Vector3& lowSupport, const Vector3& highSupport, const Vector3& frontSupport)
{
	const int nInitialNumResults = 8;

	int nBestHit = -1;
	float fBestDeltaZ = LARGE_FLOAT;

	// test low support
	{
		WorldProbe::CShapeTestProbeDesc probeDesc;
		WorldProbe::CShapeTestFixedResults<nInitialNumResults> probeResults;
		probeDesc.SetResultsStructure(&probeResults);

		Vector3 probeStart = trailerPos + lowSupport;
		Vector3 probeEnd = probeStart;
		probeEnd.z = boatMat->d.z;
		probeDesc.SetStartAndEnd(probeStart, probeEnd);
		probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
		probeDesc.SetIncludeEntity(spawnBoat);
		WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);

		WorldProbe::ResultIterator it;
		int hit = 0;
		for(it = probeResults.begin(); it < probeResults.end(); ++it, ++hit)
		{
			if(it->GetHitDetected() && rage::Abs((it->GetHitPosition() - probeStart).z) < fBestDeltaZ)
			{
				nBestHit = hit;
				fBestDeltaZ = rage::Abs((it->GetHitPosition() - probeStart).z);
			}
		}
	}

	// test high support (optional if we don't care about minor glitches)
	{
		WorldProbe::CShapeTestProbeDesc probeDesc;
		WorldProbe::CShapeTestFixedResults<nInitialNumResults> probeResults;
		probeDesc.SetResultsStructure(&probeResults);

		Vector3 probeStart = trailerPos + highSupport;
		Vector3 probeEnd = probeStart;
		probeEnd.z = boatMat->d.z;
		probeDesc.SetStartAndEnd(probeStart, probeEnd);
		probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
		probeDesc.SetIncludeEntity(spawnBoat);
		WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);

		WorldProbe::ResultIterator it;
		int hit = 0;
		for(it = probeResults.begin(); it < probeResults.end(); ++it, ++hit)
		{
			if(it->GetHitDetected() && rage::Abs((it->GetHitPosition() - probeStart).z) < fBestDeltaZ)
			{
				nBestHit = hit;
				fBestDeltaZ = rage::Abs((it->GetHitPosition() - probeStart).z);
			}
		}
	}

	// fix position if we hit anything
	if (nBestHit >= 0)
	{
		boatMat->d.z -= fBestDeltaZ;
		spawnBoat->SetMatrix(*boatMat);
	}

	// test front support and move boat forward if needed
	{
		WorldProbe::CShapeTestProbeDesc probeDesc;
		WorldProbe::CShapeTestFixedResults<nInitialNumResults> probeResults;
		probeDesc.SetResultsStructure(&probeResults);

		Vec3V vForward = spawnBoat->GetVehicleForwardDirection();
		Vector3 forward = RCC_VECTOR3(vForward);
		forward.Normalize();

		Vector3 probeStart = trailerPos + frontSupport;
		Vector3 probeEnd = probeStart - forward * 2.f;
		probeDesc.SetStartAndEnd(probeStart, probeEnd);
		probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
		probeDesc.SetIncludeEntity(spawnBoat);
		WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);

		nBestHit = -1;
		float fBestDelta = LARGE_FLOAT;
		WorldProbe::ResultIterator it;
		int hit = 0;
		for(it = probeResults.begin(); it < probeResults.end(); ++it, ++hit)
		{
			if(it->GetHitDetected())
			{
				float dist = (it->GetHitPosition() - probeStart).Mag2();
				if (dist < fBestDelta)
				{
					nBestHit = hit;
					fBestDelta = dist;
				}
			}
		}

		if (nBestHit >= 0)
		{
			boatMat->d += forward * rage::Sqrtf(fBestDelta);
			spawnBoat->SetMatrix(*boatMat);
		}
	}
}

bool CVehicleFactory::GetPositionAndRotationForCargoTrailer(CTrailer* trailer, const int index, CVehicle* pVehicle, Vector3& vPosOut, Quaternion& qRotationOut)
{
	if (!trailer || !pVehicle)
	{
		return false;
	}

	Matrix34 creationMat;
	Matrix34 trailerMat = MAT34V_TO_MATRIX34(trailer->GetMatrix());

	CVehicleModelInfo* vehMi = pVehicle->GetVehicleModelInfo();
	// make sure vehicle has loaded
	if(!CModelInfo::HaveAssetsLoaded(pVehicle->GetModelId()))
	{
		return false;
	}

	// rotate offset so it lines up with the trailer orientation
	Vector3 offset = CTrailer::sm_offsets[index];
	trailerMat.Transform3x3(offset);

	creationMat = trailerMat;
	creationMat.d += offset;

	// try to find correct height for vehicle placement
	float frontHeightAboveGround, rearHeightAboveGround;
	CVehicle::CalculateHeightsAboveRoad(pVehicle->GetModelId(), &frontHeightAboveGround, &rearHeightAboveGround);

	Vector3 posFront = CWheel::GetWheelOffset(vehMi, VEH_WHEEL_LF);
	Vector3 posRear  = CWheel::GetWheelOffset(vehMi, VEH_WHEEL_LR);

	// rotate wheel position vectors to world space
	Vector3 transPosFront;
	Vector3 transPosRear;
	trailerMat.Transform3x3(posFront, transPosFront);
	trailerMat.Transform3x3(posRear, transPosRear);

	// find collision for wheels
	float rearWheelZ = ProbeForWheelPosition(trailer, creationMat.d, transPosRear, rearHeightAboveGround);
	float frontWheelZ = ProbeForWheelPosition(trailer, creationMat.d, transPosFront, frontHeightAboveGround);

	// set z position of vehicle to middle and rotate vehicle
	creationMat.d.z = 0.5f * (rearWheelZ + frontWheelZ);
	float wheelToWheelDist = rage::Abs(posFront.y - posRear.y);	// along y, forward axis of vehicle
	float heightDiff = frontWheelZ - rearWheelZ;
	float angle = atanf(heightDiff / wheelToWheelDist);
	float origAngle = rage::Atan2f(creationMat.b.z, creationMat.c.z); // get current angle around x (since the one we just calculated should be the final angle)
	angle -= origAngle;
	creationMat.RotateLocalX(angle); // rotate around x so the forward axis is rotated

	Matrix34 trailerAttachMat;
	trailer->GetGlobalMtx(trailer->GetBoneIndex(TRAILER_ATTACH), trailerAttachMat);
	Matrix34 offsetMat;
	offsetMat.DotTranspose(creationMat, trailerAttachMat);
	Quaternion offsetRot;
	offsetRot.FromMatrix34(offsetMat);

	vPosOut = offsetMat.d;
	qRotationOut = offsetRot;

	return true;
}

//
// name:		CVehicleFactory::AddCarsOnTrailer
// description:	Adds cars on the trailer specified. the cars to be spawned need to be loaded into memory.
// parameters:	trailer pointer
//				location for creation
//				owned by identifier
//				population type identifier
//				optional list of cars to spawn. if NULL is passed it it will use random cars available in memory (that have the spawn_on_trailer flag set)
void CVehicleFactory::AddCarsOnTrailer(CTrailer* trailer, fwInteriorLocation location, eEntityOwnedBy ownedBy, ePopType popType, fwModelId* carsToSpawn)
{
	CVehicleModelInfo* mi = trailer->GetVehicleModelInfo();
	Assert(mi);

	if (!mi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPAWN_ON_TRAILER))
		return;

	// if no car list is passed in, try to create a list from vehicles loaded in memory that have the spawn on trailer flag set
	fwModelId loadedVehIds[MAX_CARGO_VEHICLES];
	if (!carsToSpawn)
	{
		CLoadedModelGroup loadedCars = gPopStreaming.GetAppropriateLoadedCars();
		loadedCars.RemoveVehModelsWithFlagSet(CVehicleModelInfoFlags::FLAG_SPAWN_ON_TRAILER, false);
		s32 numLoadedCars = loadedCars.CountMembers();

		if (numLoadedCars == 0)
			return;

		for (s32 i = 0; i < MAX_CARGO_VEHICLES; ++i)
		{
			loadedVehIds[i] = fwModelId(strLocalIndex(loadedCars.GetMember(fwRandom::GetRandomNumberInRange(0, numLoadedCars))));

			if (CScriptCars::GetSuppressedCarModels().HasModelBeenSuppressed(loadedVehIds[i].GetModelIndex()))
				loadedVehIds[i].Invalidate();
		}
		carsToSpawn = loadedVehIds;
	}

	// create our vehicles
	Matrix34 creationMat;
	Matrix34 trailerMat = MAT34V_TO_MATRIX34(trailer->GetMatrix());

	// Pick a random number of cargo vehicles for this trailer
	static int kMinCargoVehs = 3;
	static int kMaxCargoVehs = 5;
	int iNumCargoVehsToPlace = fwRandom::GetRandomNumberInRange(Min(kMinCargoVehs,kMaxCargoVehs),kMaxCargoVehs+1);
	for (s32 i = 0; i < MAX_CARGO_VEHICLES; ++i)
	{
		float fRand = fwRandom::GetRandomNumberInRange(0.0f,1.0f);
		if(fRand > (float)(iNumCargoVehsToPlace) / (MAX_CARGO_VEHICLES-i))
		{
			continue;
		}
		iNumCargoVehsToPlace--;

		fwModelId vehId = carsToSpawn[i];
		if (!vehId.IsValid())
			continue;

		CVehicleModelInfo* vehMi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(vehId);

		// make sure vehicle has loaded
		if(!CModelInfo::HaveAssetsLoaded(vehId))
		{
			continue;
		}

		// rotate offset so it lines up with the trailer orientation
		Vector3 offset = CTrailer::sm_offsets[i];
		trailerMat.Transform3x3(offset);

		creationMat = trailerMat;
		creationMat.d += offset;

		// try to find correct height for vehicle placement
		float frontHeightAboveGround, rearHeightAboveGround;
		CVehicle::CalculateHeightsAboveRoad(vehId, &frontHeightAboveGround, &rearHeightAboveGround);

		Vector3 posFront = CWheel::GetWheelOffset(vehMi, VEH_WHEEL_LF);
		Vector3 posRear  = CWheel::GetWheelOffset(vehMi, VEH_WHEEL_LR);

		// rotate wheel position vectors to world space
		Vector3 transPosFront;
		Vector3 transPosRear;
		trailerMat.Transform3x3(posFront, transPosFront);
		trailerMat.Transform3x3(posRear, transPosRear);

		// find collision for wheels
		float rearWheelZ = ProbeForWheelPosition(trailer, creationMat.d, transPosRear, rearHeightAboveGround);
		float frontWheelZ = ProbeForWheelPosition(trailer, creationMat.d, transPosFront, frontHeightAboveGround);

		// set z position of vehicle to middle and rotate vehicle
		creationMat.d.z = 0.5f * (rearWheelZ + frontWheelZ);
		float wheelToWheelDist = rage::Abs(posFront.y - posRear.y);	// along y, forward axis of vehicle
		float heightDiff = frontWheelZ - rearWheelZ;
		float angle = atanf(heightDiff / wheelToWheelDist);
		float origAngle = rage::Atan2f(creationMat.b.z, creationMat.c.z); // get current angle around x (since the one we just calculated should be the final angle)
		angle -= origAngle;
		creationMat.RotateLocalX(angle); // rotate around x so the forward axis is rotated

		// spawn vehicle
		CVehicle* spawnVeh = CVehicleFactory::GetFactory()->Create(vehId, ownedBy, popType, &creationMat);
		CGameWorld::Add(spawnVeh, location);

		Matrix34 trailerAttachMat;
		trailer->GetGlobalMtx(trailer->GetBoneIndex(TRAILER_ATTACH), trailerAttachMat);
		Matrix34 offsetMat;
		offsetMat.DotTranspose(creationMat, trailerAttachMat);
		Quaternion offsetRot;
		offsetRot.FromMatrix34(offsetMat);

		//save off our offset from the parent so we know where to keep the vehicle in superdummy mode
		//spawnVeh->SetParentTrailerEntity1Offset(offsetMat.d);
		
		//offset.d is where the vehicle is in trailer space.
		//offsetRot is the rotation of the vehicle in trailer space
		//Vector3 vEntity2Offset = creationMat.Transform(trailerAttachMat.d);
		spawnVeh->SetParentTrailerEntity2Offset(offsetMat.d);
		spawnVeh->SetParentTrailerEntityRotation(offsetRot);

		spawnVeh->AttachToPhysicalBasic(trailer, (s16)trailer->GetBoneIndex(TRAILER_ATTACH), ATTACH_STATE_BASIC | ATTACH_FLAG_INITIAL_WARP | ATTACH_FLAG_COL_ON | ATTACH_FLAG_DELETE_WITH_PARENT, &offsetMat.d, &offsetRot);
		trailer->AddCargoVehicle(i, spawnVeh);
		spawnVeh->SetParentTrailer(trailer);
	}
}

//
// name:		CVehicleFactory::AddBoatOnTrailer
// description:	Adds a boat on the trailer specified. the boat to be spawned needs to be loaded in memory.
// parameters:	trailer pointer
//				location for creation
//				owned by identifier
//				population type identifier
//				optional boat type to spawn
// returns true if a boat is created.
bool CVehicleFactory::AddBoatOnTrailer(CTrailer* trailer, fwInteriorLocation location, eEntityOwnedBy ownedBy, ePopType popType, fwModelId* boatToSpawn)
{
	CVehicleModelInfo* mi = trailer->GetVehicleModelInfo();
	Assert(mi);

	if (!mi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPAWN_BOAT_ON_TRAILER))
		return false;

	if (ms_uCurrentBoatTrailersWithBoats >= ms_uMaxBoatTrailersWithBoats)
	{
		boatTrailerDebugf3("AddBoatOnTrailer: BLOCKED boat creation, ms_uCurrentBoatTrailersWithBoats = %u", ms_uCurrentBoatTrailersWithBoats);
		return false;
	}

	fwModelId vehModel;
	if (!boatToSpawn || !boatToSpawn->IsValid())
	{
		CLoadedModelGroup loadedBoats = gPopStreaming.GetLoadedBoats();
		loadedBoats.RemoveVehModelsWithFlagSet(CVehicleModelInfoFlags::FLAG_SPAWN_BOAT_ON_TRAILER, false);
		s32 numBoats = loadedBoats.CountMembers();

		if (numBoats == 0)
			return false;

		vehModel = fwModelId(strLocalIndex(loadedBoats.GetMember(fwRandom::GetRandomNumberInRange(0, numBoats))));
	}
	else
	{
		vehModel = *boatToSpawn;
	}

	if (!vehModel.IsValid())
		return false;

	// make sure vehicle has loaded
	if(!CModelInfo::HaveAssetsLoaded(vehModel))
		return false;

	// create our boat
	Matrix34 trailerMat = MAT34V_TO_MATRIX34(trailer->GetMatrix());
	Matrix34 creationMat = trailerMat;
	creationMat.d.z -= mi->GetBoundingBoxMin().z;

	// FA: these are hardcoded values for the boattrailer, if we end up with several layouts we'll need to expose these to meta data
	Vector3 offsets[3] = 
	{
		Vector3(0.42f, -1.58f, -0.19f),	// low rest shelf
		Vector3(0.86f, -3.30f, 0.13f),	// high support pad at the back
		Vector3(0.f, 2.55f, 0.58f)	// front support, trace from this horizontally backwards
	};

	// move the boat backwards a bit so it doesn't intersect the car
	Vector3 offset(0.f, -1.5f, 0.f);
	trailerMat.Transform3x3(offset);
	creationMat.d += offset;

	CVehicle* spawnBoat = CVehicleFactory::GetFactory()->Create(vehModel, ownedBy, popType, &creationMat);
	if (!Verifyf(spawnBoat, "Vehicle factory failed to created boat! id: %d", vehModel.GetModelIndex()))
		return false;

	trailerMat.Transform3x3(offsets[0]);
	trailerMat.Transform3x3(offsets[1]);
	trailerMat.Transform3x3(offsets[2]);
	ProbeForBoatPosition(spawnBoat, &creationMat, trailerMat.d, offsets[0], offsets[1], offsets[2]);

	CGameWorld::Add(spawnBoat, location);

	Matrix34 trailerAttachMat;
	trailer->GetGlobalMtx(trailer->GetBoneIndex(TRAILER_ATTACH), trailerAttachMat);
	Matrix34 offsetMat;
	offsetMat.DotTranspose(creationMat, trailerAttachMat);
	Quaternion offsetRot;
	offsetRot.FromMatrix34(offsetMat);

	spawnBoat->SetParentTrailerEntity2Offset(offsetMat.d);
	spawnBoat->SetParentTrailerEntityRotation(offsetRot);

	spawnBoat->AttachToPhysicalBasic(trailer, (s16)trailer->GetBoneIndex(TRAILER_ATTACH), ATTACH_STATE_BASIC | ATTACH_FLAG_INITIAL_WARP | ATTACH_FLAG_COL_ON | ATTACH_FLAG_DELETE_WITH_PARENT, &offsetMat.d, &offsetRot);
	trailer->AddCargoVehicle(0, spawnBoat);
	spawnBoat->SetParentTrailer(trailer);

	++ms_uCurrentBoatTrailersWithBoats;
	if(ms_uCurrentBoatTrailersWithoutBoats > 0)
	{
		--ms_uCurrentBoatTrailersWithoutBoats;
	}
	boatTrailerDebugf3("AddBoatOnTrailer: ms_uCurrentBoatTrailersWithoutBoats = %u, ms_uCurrentBoatTrailersWithBoats = %u", ms_uCurrentBoatTrailersWithoutBoats, ms_uCurrentBoatTrailersWithBoats);
	
	return true;
}

void CVehicleFactory::DetachedBoatOnTrailer()
{
	if(ms_uCurrentBoatTrailersWithBoats > 0)
	{
		--ms_uCurrentBoatTrailersWithBoats;
	}
	++ms_uCurrentBoatTrailersWithoutBoats;
	boatTrailerDebugf3("DetachedBoatOnTrailer: ms_uCurrentBoatTrailersWithoutBoats = %u, ms_uCurrentBoatTrailersWithBoats = %u", ms_uCurrentBoatTrailersWithoutBoats, ms_uCurrentBoatTrailersWithBoats);
}

bool CVehicleFactory::DestroyBoatTrailerIfNecessary(CTrailer** ppOutTrailer)
{
	if (!ppOutTrailer)
	{
		return false;
	}
	if (!*ppOutTrailer)
	{
		return false;
	}

	CVehicleModelInfo* mi = (*ppOutTrailer)->GetVehicleModelInfo();
	Assert(mi);

	// If for some reason you called this function on a non-boat-trailer, return.
	if (!mi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPAWN_BOAT_ON_TRAILER))
	{
		return false;
	}

	// Check to see if a boat exists.
	if (!(*ppOutTrailer)->HasCargoVehicles())	
	{
		if (ms_uCurrentBoatTrailersWithoutBoats > ms_uMaxBoatTrailersWithoutBoats)
		{
			// Destroy will decrement ms_uCurrentBoatTrailersWithoutBoats
			GetFactory()->Destroy(*ppOutTrailer);
			*ppOutTrailer = NULL;
			boatTrailerDebugf3("DestroyBoatTrailerIfNecessary: DESTROYING trailer without boat, ms_uCurrentBoatTrailersWithoutBoats = %u", ms_uCurrentBoatTrailersWithoutBoats);
			return true;
		}
	}

#if !__NO_OUTPUT
	if(ms_uCurrentBoatTrailersWithBoats > ms_uMaxBoatTrailersWithBoats)
	{
		boatTrailerWarningf("Too many trailers with boats (%u, max %u)", ms_uCurrentBoatTrailersWithBoats, ms_uMaxBoatTrailersWithBoats);
	}
	if(ms_uCurrentBoatTrailersWithoutBoats > (ms_uMaxBoatTrailersWithoutBoats + 1)) // Allow one extra, as it could turn into a trailer with a boat.
	{				
		boatTrailerWarningf("Too many trailers without boats (%u, max %u + 1)", ms_uCurrentBoatTrailersWithoutBoats, ms_uMaxBoatTrailersWithoutBoats);
	}
#endif // !__NO_OUTPUT

	return false;
}


void CVehicleFactory::DestroyChildrenVehicles(CVehicle * pVehicle, bool UNUSED_PARAM(cached))
{
	// Destroy dummy children
	if (pVehicle->HasDummyAttachmentChildren())
	{
		if(pVehicle->IsNetworkClone())
		{
			Assertf(0, "Warning: Trying to delete clone %s attachments!", pVehicle->GetNetworkObject()->GetLogName());
		}
		else
		{
			for (int i=0; i < pVehicle->GetMaxNumDummyAttachmentChildren(); i++)
			{
				CVehicle* pDummyAttachedVeh = pVehicle->GetDummyAttachmentChild(i);
				if (pDummyAttachedVeh)
				{
					pVehicle->DummyDetachChild(i);

					const bool bCanDelete = !pDummyAttachedVeh->GetNetworkObject() || pDummyAttachedVeh->GetNetworkObject()->CanDelete();

					Assertf(bCanDelete, "Can't delete child vehicle! This should have been checked before calling this function!");

					if (bCanDelete REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))
					{
						// TODO: Any reason we don't cache the attached vehicles?
						Destroy(pDummyAttachedVeh);
					}
				}
			}
		}
	}

	// Detach any mission vehicle attached to a tow-arm to prevent it from being destroyed below.
	for(int i=0; i<pVehicle->GetNumberOfVehicleGadgets(); i++)
	{
		CVehicleGadget * pGadget = pVehicle->GetVehicleGadget(i);
		if(pGadget && pGadget->GetType()==VGT_TOW_TRUCK_ARM)
		{
			CVehicleGadgetTowArm * pTowArm = static_cast<CVehicleGadgetTowArm*>(pGadget);
			if(const CVehicle * pAttachedVehicle = pTowArm->GetAttachedVehicle())
			{
				if(pAttachedVehicle->PopTypeIsMission())
				{
					pTowArm->DetachVehicle(pVehicle,true,true);
				}
			}
		}
		else if(pGadget && (pGadget->GetType()==VGT_PICK_UP_ROPE || pGadget->GetType()==VGT_PICK_UP_ROPE_MAGNET))
		{
			CVehicleGadgetPickUpRope * pPickUpRope = static_cast<CVehicleGadgetPickUpRope*>(pGadget);
			if(const CVehicle * pAttachedVehicle = pPickUpRope->GetAttachedVehicle())
			{
				if(pAttachedVehicle->PopTypeIsMission())
				{
					pPickUpRope->DetachEntity(pVehicle,true,true);
				}
			}
		}
	}

	// Destroy children attached via CPhysical. Only recurses through vehicle attachments.  This used to happen in ~CPhysical,
	// but now CGameWorld::Remove() detaches objects so the connection is lost.  This may be creating orphaned objects elsewhere.
	fwAttachmentEntityExtension *attachExt = pVehicle->GetAttachmentExtension();
	if(attachExt)
	{
		CPhysical* pCurChild = static_cast<CPhysical*>(attachExt->GetChildAttachment());
		while(pCurChild)
		{
			fwAttachmentEntityExtension &curChildAttachExt = pCurChild->GetAttachmentExtensionRef();
			CPhysical* pNextChild = static_cast<CPhysical*>(curChildAttachExt.GetSiblingAttachment());
			if(pCurChild->GetIsTypeVehicle())
			{
				CVehicle* pChildVeh = static_cast<CVehicle*>(pCurChild);
				if (pChildVeh)
				{
					const bool bCanDelete = !pChildVeh->GetNetworkObject() || pChildVeh->GetNetworkObject()->CanDelete();

					Assertf(bCanDelete, "Can't delete child vehicle! This should have been checked before calling this function!");

					if (bCanDelete REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive())) 
					{
						// TODO: Any reason we don't cache the attached vehicles?
						Destroy(pChildVeh);
					}
				}
			}
			pCurChild = pNextChild;
		}
	}

	if (pVehicle->InheritsFromTrailer())
	{
		for (s32 i = 0; i < MAX_CARGO_VEHICLES; ++i)
		{
			CVehicle* pCargoVehicle = ((CTrailer*)pVehicle)->GetCargoVehicle(i);
			if (pCargoVehicle)
			{
				((CTrailer*)pVehicle)->RemoveCargoVehicle(pCargoVehicle);
				REPLAY_ONLY(if(!CReplayMgr::IsEditModeActive()))
				{
					Destroy(pCargoVehicle);
				}
			}
		}
	}

}

void CVehicleFactory::DeleteOrCacheVehicle(CVehicle * pVehicle, bool cached)
{
	// don't add helis to the cache, it won't give us any wins and they don't seem to play nice with it
	if (cached && !pVehicle->GetIsRotaryAircraft() && !gPopStreaming.IsCarDiscarded(pVehicle->GetModelIndex()))
	{
		AddDestroyedVehToCache(pVehicle);
	}
	else
	{
		delete pVehicle;
	}
}

#if __BANK
void CVehicleFactory::LogDestroyedVehicle(CVehicle * pVehicle, const char * pCallerDesc, s32 iTextOffsetY, Color32 iTextCol)
{
	if(ms_bLogDestroyedVehicles)
	{
		CViewport* gameViewport = gVpMan.GetGameViewport();
		if(gameViewport)
		{
			char text[128];

			const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
			const Vector3 vCameraPos = VEC3V_TO_VECTOR3(gameViewport->GetGrcViewport().GetCameraPosition());
			const float fDist = (vCameraPos - vVehiclePos).Mag();

			const char *pLogName;
			if(NetworkInterface::IsGameInProgress())
			{
				pLogName = pVehicle->GetNetworkObject() ? pVehicle->GetNetworkObject()->GetLogName() : "";
			}
			else
			{
				pLogName = pVehicle->GetDebugNameFromObjectID();
			}

			sprintf(text, "%i : %s %s", ms_iDestroyedVehicleCount, pVehicle->GetModelName(), pLogName);
			grcDebugDraw::Text(vVehiclePos, iTextCol, 0, -grcDebugDraw::GetScreenSpaceTextHeight() + iTextOffsetY, text, false, -(s32)(10.0f / fwTimer::GetTimeStep()));
			grcDebugDraw::Sphere(vVehiclePos, 0.25f, Color_yellow4, false, -(s32)(10.0f / fwTimer::GetTimeStep()));

			Displayf("**************************************************************************************************");
			Displayf("%s : %i", pCallerDesc, ms_iDestroyedVehicleCount);
			Displayf("NAME : %s %s, DISTANCE : %.1f, POSITION : %.1f, %.1f, %.1f, VEHICLE : 0x%p, RANGESCALE : %.1f", pVehicle->GetModelName(), pLogName, fDist, vVehiclePos.x, vVehiclePos.y, vVehiclePos.z, pVehicle, CVehiclePopulation::GetPopulationRangeScale());
			Displayf("REASON : %s", CVehiclePopulation::GetLastRemovalReason());
			sysStack::PrintStackTrace();
			Displayf("**************************************************************************************************");

			ms_iDestroyedVehicleCount++;
		}
	}
}
#endif // __BANK

//
// name:		CVehicleFactory::Destroy
// description:	Default factory destroy function. Individual projects can override this
// parameters:	pVehicle
void CVehicleFactory::Destroy(CVehicle* pVehicle, bool cached)
{
	Assert(pVehicle);

#if __BANK
	LogDestroyedVehicle(pVehicle, "CVehicleFactory::Destroy", 0, Color_yellow);
#endif

	CVehiclePopulation::IncrementNumVehiclesDestroyedThisCycle();

	// don't destroy vehicle if it's on the population cache list
	if (IsVehInDestroyedCache(pVehicle))
	{
		pVehicle->ClearAllKnownRefs();
		return;
	}

	// inform network object manager about this vehicle being deleted
	if (pVehicle->GetNetworkObject())
	{
		if (pVehicle->IsNetworkClone())
		{
			if (pVehicle->GetNetworkObject()->IsScriptObject() && !pVehicle->PopTypeIsMission())
				Assertf(0, "Warning: clone %s is being destroyed (it is not a mission vehicle anymore)!", pVehicle->GetNetworkObject()->GetLogName());
			else
				Assertf(0, "Warning: clone %s is being destroyed!", pVehicle->GetNetworkObject()->GetLogName());

			// bail out
			return;
		}

		NetworkInterface::UnregisterObject(pVehicle);
	}

	if (pVehicle && pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAILER)
	{
		CVehicleModelInfo* tmi = pVehicle->GetVehicleModelInfo();
		if (tmi && tmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPAWN_BOAT_ON_TRAILER))
		{
			CTrailer* pTrailer = static_cast<CTrailer*>(pVehicle);
			if (pTrailer->HasCargoVehicles())
			{
				if (ms_uCurrentBoatTrailersWithBoats > 0)
				{
					--ms_uCurrentBoatTrailersWithBoats;
				}
#if !__NO_OUTPUT
				else
				{
					boatTrailerWarningf("Incorrect accounting for ms_uCurrentBoatTrailersWithBoats - deleting one when ms_uCurrentBoatTrailersWithBoats == 0");
				}
#endif // !__NO_OUTPUT
				boatTrailerDebugf3("Destroy: removed trailer with boat, ms_uCurrentBoatTrailersWithBoats = %u", ms_uCurrentBoatTrailersWithBoats);
			}
			else
			{
				if (ms_uCurrentBoatTrailersWithoutBoats > 0)
				{
					--ms_uCurrentBoatTrailersWithoutBoats;
				}
#if !__NO_OUTPUT
				else
				{
					boatTrailerWarningf("Incorrect accounting for ms_uCurrentBoatTrailersWithoutBoats - deleting one when ms_uCurrentBoatTrailersWithBoats == 0");
				}
#endif // !__NO_OUTPUT
				boatTrailerDebugf3("Destroy: removed trailer without boat, ms_uCurrentBoatTrailersWithoutBoats = %u", ms_uCurrentBoatTrailersWithoutBoats);
			}
		}
	}

	Assert(pVehicle->m_nVehicleFlags.bCreatedByFactory);
	pVehicle->m_nVehicleFlags.bCreatedByFactory = false;

	DestroyChildrenVehicles(pVehicle,cached);

	CSpatialArrayNode& node = pVehicle->GetSpatialArrayNode();
	if(node.IsInserted())
	{
		CVehicle::GetSpatialArray().Remove(node);
	}

	REPLAY_ONLY(CReplayMgr::OnDelete(pVehicle));

	CGameWorld::Remove(pVehicle);

	CTheScripts::UnregisterEntity(pVehicle, false);

#if 0
	DeleteOrCacheVehicle(pVehicle,cached);
#else
	delete pVehicle;
#endif
}

CVehicle* CVehicleFactory::CreateVehicle(s32 nVehicleType, const eEntityOwnedBy ownedBy, const u32 popType)
{
	CVehicle *pReturnVehicle = NULL;
#if __BANK
	if(CVehicle::GetPool() && CVehicle::GetPool()->GetNoOfFreeSpaces() == 0)
	{
		if (NetworkInterface::IsGameInProgress())
		{
			Displayf("*** VEHICLE POOL FULL ****\n");

			CVehicle::Pool* pool = CVehicle::GetPool();
			for(int i= 0; i< pool->GetSize(); i++)
			{
				CVehicle* pVehicle = pool->GetSlot(i);

				if(pVehicle)
				{
					Vector3 pos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
					Displayf("%d: 0x%p  (Model: %s. Net obj: %s. Pos: %f, %f, %f. Script created: %s)\n", i, pVehicle, pVehicle->GetModelName(), pVehicle->GetNetworkObject() ? pVehicle->GetNetworkObject()->GetLogName() : "-none-", pos.x, pos.y, pos.z, pVehicle->PopTypeIsMission() ? "true" : "false");
				}
				else
				{
					Displayf("%d: Empty\n", i);
				}
			}
		}
		Assertf(false, "Vehicle Pool Full, Size == %d (you need to raise PoolSize in common/data/gameconfig.xml)", (s32)CVehicle::GetPool()->GetSize());
		return NULL;
	}
#endif

	switch(nVehicleType)
	{
		case VEHICLE_TYPE_CAR:
			pReturnVehicle =  rage_new CAutomobile(ownedBy, popType, VEHICLE_TYPE_CAR);
			break;
		case VEHICLE_TYPE_QUADBIKE:
			pReturnVehicle =  rage_new CQuadBike(ownedBy, popType);
			break;
		case VEHICLE_TYPE_BIKE:
			pReturnVehicle =  rage_new CBike(ownedBy, popType, VEHICLE_TYPE_BIKE);
			break;
		case VEHICLE_TYPE_BICYCLE:
			pReturnVehicle =  rage_new CBmx(ownedBy, popType);
			break;
		case VEHICLE_TYPE_BOAT:
			pReturnVehicle =  rage_new CBoat(ownedBy, popType);
			break;
		case VEHICLE_TYPE_TRAIN:
			pReturnVehicle =  rage_new CTrain(ownedBy, popType);
			break;
		case VEHICLE_TYPE_HELI:
			pReturnVehicle =  rage_new CHeli(ownedBy, popType);
			break;
		case VEHICLE_TYPE_PLANE:
			pReturnVehicle =  rage_new CPlane(ownedBy, popType);
			break;
		case VEHICLE_TYPE_AUTOGYRO:
			pReturnVehicle = rage_new CAutogyro(ownedBy, popType);
			break;
		case VEHICLE_TYPE_SUBMARINE:
			pReturnVehicle = rage_new CSubmarine(ownedBy, popType);
			break;
		case VEHICLE_TYPE_TRAILER:
			pReturnVehicle = rage_new CTrailer(ownedBy, popType);
			break;
#if ENABLE_DRAFT_VEHICLE
		case VEHICLE_TYPE_DRAFT:
			pReturnVehicle = rage_new CDraftVeh(ownedBy, popType);
			break;
#endif
		case VEHICLE_TYPE_SUBMARINECAR:
			pReturnVehicle = rage_new CSubmarineCar(ownedBy, popType);
			break;
		case VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE:
			pReturnVehicle = rage_new CAmphibiousAutomobile(ownedBy, popType);
			break;
		case VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE:
			pReturnVehicle = rage_new CAmphibiousQuadBike(ownedBy, popType);
			break;
		case VEHICLE_TYPE_BLIMP:
			pReturnVehicle = rage_new CBlimp(ownedBy, popType);
			break;
		default:
			Assertf(false, "Unknown vehicle type");
			break;
	}

#if NAVMESH_EXPORT
	// Don't attempt to init audio for collision exporter, it will crash
	if(CNavMeshDataExporter::WillExportCollision())
		return pReturnVehicle;
#endif

	if(pReturnVehicle)
	{
		switch(nVehicleType)
		{
		case VEHICLE_TYPE_HELI:
		case VEHICLE_TYPE_AUTOGYRO:
		case VEHICLE_TYPE_BLIMP:
			pReturnVehicle->m_VehicleAudioEntity = rage_new audHeliAudioEntity();
			break;
		case VEHICLE_TYPE_TRAIN:
			pReturnVehicle->m_VehicleAudioEntity = rage_new audTrainAudioEntity();
			break;
		case VEHICLE_TYPE_PLANE:
			pReturnVehicle->m_VehicleAudioEntity = rage_new audPlaneAudioEntity();
			break;
		case VEHICLE_TYPE_BOAT:
		case VEHICLE_TYPE_SUBMARINE:
			pReturnVehicle->m_VehicleAudioEntity = rage_new audBoatAudioEntity();
			break;
		case VEHICLE_TYPE_CAR:
		case VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE:
		case VEHICLE_TYPE_BIKE:
		case VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE:
		case VEHICLE_TYPE_SUBMARINECAR:
		case VEHICLE_TYPE_QUADBIKE:
#if ENABLE_DRAFT_VEHICLE
		case VEHICLE_TYPE_DRAFT:
#endif
			pReturnVehicle->m_VehicleAudioEntity = rage_new audCarAudioEntity();
			break;
		case VEHICLE_TYPE_BICYCLE:
			pReturnVehicle->m_VehicleAudioEntity = rage_new audBicycleAudioEntity();
			break;
		case VEHICLE_TYPE_TRAILER:
			pReturnVehicle->m_VehicleAudioEntity = rage_new audTrailerAudioEntity();
			break;
		default:
			audWarningf("Vehicle type [%d] does not have a custom audVehicleAudioEntity this can lead to asserts looking like Assert(m_EngineSubmixIndex >= 0 && m_ExhaustSubmixIndex >= 0)", nVehicleType);
			pReturnVehicle->m_VehicleAudioEntity = rage_new audVehicleAudioEntity();
			break;
		}

		// audDisplayf("%s (0x%p) created %s audio entity (0x%p)", ms_modelTypes[nVehicleType], pReturnVehicle, pReturnVehicle->m_VehicleAudioEntity? audVehicleAudioEntity::GetVehicleTypeName(pReturnVehicle->m_VehicleAudioEntity->GetAudioVehicleType()) : "NULL", pReturnVehicle->m_VehicleAudioEntity);

		pReturnVehicle->m_VehicleAudioEntity->Init(pReturnVehicle);

		REPLAY_ONLY(CReplayMgr::OnCreateEntity(pReturnVehicle));
	}

	return pReturnVehicle;
}

bool CVehicleFactory::GetVehicleType(char* name, s32& nVehicleType)
{
	if(!strcmp(name, "bicycle"))
	{
		nVehicleType = VEHICLE_TYPE_BICYCLE;
	}
	else if(!strcmp(name, "car"))
	{
		nVehicleType = VEHICLE_TYPE_CAR;
	}
	else if(!strcmp(name, "quadbike"))
	{
		nVehicleType = VEHICLE_TYPE_QUADBIKE;
	}
	else if(!strcmp(name, "bike"))
	{
		nVehicleType = VEHICLE_TYPE_BIKE;
	}
	else if(!strcmp(name, "boat"))
	{
		nVehicleType = VEHICLE_TYPE_BOAT;
	}
	else if(!strcmp(name, "train"))
	{
		nVehicleType = VEHICLE_TYPE_TRAIN;
	}
	else if(!strcmp(name, "heli"))
	{
		nVehicleType = VEHICLE_TYPE_HELI;
	}
	else if(!strcmp(name, "plane"))
	{
		nVehicleType = VEHICLE_TYPE_PLANE;
	}
	else if(!strcmp(name, "autogyro"))
	{
		nVehicleType = VEHICLE_TYPE_AUTOGYRO;
	}
	else if(!strcmp(name, "submarine"))
	{
		nVehicleType = VEHICLE_TYPE_SUBMARINE;
	}
	else if(!strcmp(name, "trailer"))
	{
		nVehicleType = VEHICLE_TYPE_TRAILER;
	}
#if ENABLE_DRAFT_VEHICLE
	else if(!strcmp(name, "draft"))
	{
		nVehicleType = VEHICLE_TYPE_DRAFT;
	}
#endif
	else if(!strcmp(name, "submarinecar"))
	{
		nVehicleType = VEHICLE_TYPE_SUBMARINECAR;
	}
	else if(strcmp(name, "amphibiousAutomobile"))
	{
		nVehicleType = VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE;
	}
	else if(strcmp(name, "amphibiousQuadbike"))
	{
		nVehicleType = VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE;
	}
	else if(!strcmp(name, "blimp"))
	{
		nVehicleType = VEHICLE_TYPE_BLIMP;
	}
	else
	{
		Assertf(false, "Unknown vehicle type name");
		return false;
	}

	return true;
}


ObjectNameIdAssociation* CVehicleFactory::GetBaseVehicleTypeDesc(int UNUSED_PARAM(nType))
{
	return ms_modelDesc[0];
}

ObjectNameIdAssociation* CVehicleFactory::GetExtraVehicleTypeDesc(int nType)
{
	Assert(nType < VEHICLE_TYPE_NUM);
	return ms_modelDesc[nType];
}

ObjectNameIdAssociation* CVehicleFactory::GetExtraLandingGearTypeDesc()
{
	return pLandingGearModelOverrideDesc;
}

u16* CVehicleFactory::GetBaseVehicleTypeDescHashes(int UNUSED_PARAM(nType))
{
	return ms_modelDescHashes[0];
}

u16* CVehicleFactory::GetExtraVehicleTypeDescHashes(int nType)
{
	Assert(nType < VEHICLE_TYPE_NUM);
	return ms_modelDescHashes[nType];
}

u16* CVehicleFactory::GetExtraLandingGearTypeDescHashes()
{
	return pLandingGearModelOverrideDescHashes;
}

void CVehicleFactory::AssignVehicleGadgets(CVehicle* pVehicle)
{
	CVehicleGadget* pGadget = NULL;
	if(pVehicle)
	{
		// scan for vehicle weapons

		// Stuff will be added here for special case vehicles

		if(pVehicle->pHandling->mFlags & MF_HAS_TRACKS)
		{
			ENABLE_FRAG_OPTIMIZATION_ONLY(pVehicle->GiveFragCacheEntry(true);)
			pGadget = rage_new CVehicleTracks();
			pVehicle->AddVehicleGadget(pGadget);
		}
		if(pVehicle->pHandling->hFlags & HF_ENABLE_LEAN)
		{
			pGadget = rage_new CVehicleLeanHelper();
			pVehicle->AddVehicleGadget(pGadget);
		}

		if(pVehicle->GetVehicleModelInfo() && pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_PARKING_SENSORS))
		{
			static dev_float sParkingSensorRadius = 0.4f;
			CVehicleGadgetParkingSensor *pParkingSensor = rage_new CVehicleGadgetParkingSensor(pVehicle, sParkingSensorRadius);
			pVehicle->AddVehicleGadget(pParkingSensor);
		}

		// Search for attach point
		int iAttachHelperBoneIndex = pVehicle->GetBoneIndex(VEH_ATTACH);
		if(iAttachHelperBoneIndex > -1)
		{
			pGadget = rage_new CVehicleTrailerAttachPoint(iAttachHelperBoneIndex);
			pVehicle->AddVehicleGadget(pGadget);
			pVehicle->m_nVehicleFlags.bMayBecomeParkedSuperDummy = false;
		}

		// Search for a digger arm
		iAttachHelperBoneIndex = pVehicle->GetBoneIndex(VEH_DIGGER_ARM);
		if(iAttachHelperBoneIndex > -1)
		{
			ENABLE_FRAG_OPTIMIZATION_ONLY(pVehicle->GiveFragCacheEntry(true);)
			static dev_float fDiggerArmSleepMoveSpeed = 1.0f;
			static dev_float fDiggerArmDefaultRatio = 0.25f;

			CVehicleGadgetDiggerArm *pDiggerArmGadget = rage_new CVehicleGadgetDiggerArm(iAttachHelperBoneIndex, fDiggerArmSleepMoveSpeed);
			pDiggerArmGadget->InitJointLimits(pVehicle->GetVehicleModelInfo());
			pVehicle->AddVehicleGadget(pDiggerArmGadget);
			
			pDiggerArmGadget->DriveJointToRatio(pVehicle, DIGGER_JOINT_BASE, fDiggerArmDefaultRatio);
			pDiggerArmGadget->InitAudio(pVehicle);
			return;
		}

		if(pVehicle->InheritsFromTrailer() &&
			( !MI_TRAILER_TRAILERLARGE.IsValid() || MI_TRAILER_TRAILERLARGE.GetName().GetHash() != pVehicle->GetModelNameHash() ) &&
			( !MI_TRAILER_TRAILERSMALL2.IsValid() || MI_TRAILER_TRAILERSMALL2.GetName().GetHash() != pVehicle->GetModelNameHash() ) )// This is a gadget for tr3 trailer
		{
			// Search for a boat boom
			iAttachHelperBoneIndex = pVehicle->GetBoneIndex(VEH_MISC_E);
			if(iAttachHelperBoneIndex > -1)
			{
				ENABLE_FRAG_OPTIMIZATION_ONLY(pVehicle->GiveFragCacheEntry(true);)
				static dev_float fBooomMoveSpeed = 1.0f;

				CVehicleGadgetBoatBoom *pBoatBoomGadget = rage_new CVehicleGadgetBoatBoom(iAttachHelperBoneIndex, fBooomMoveSpeed);
				pBoatBoomGadget->InitJointLimits(pVehicle->GetVehicleModelInfo());
				pVehicle->AddVehicleGadget(pBoatBoomGadget);
			}
		}


		// Search for a tow truck arm.
		iAttachHelperBoneIndex = pVehicle->GetBoneIndex(VEH_TOW_ARM);
		if(iAttachHelperBoneIndex > -1)
		{		
			ENABLE_FRAG_OPTIMIZATION_ONLY(pVehicle->GiveFragCacheEntry(true);)
			s32 iMountAAttachHelperBoneIndex = pVehicle->GetBoneIndex(VEH_TOW_MOUNT_A);
			s32 iMountBAttachHelperBoneIndex = pVehicle->GetBoneIndex(VEH_TOW_MOUNT_B);

			static dev_float fTowArmMoveSpeed = 0.125f;

			//Make sure the bones are valid
			Assert(pVehicle->GetSkeleton() && iMountAAttachHelperBoneIndex < pVehicle->GetSkeleton()->GetBoneCount() && iMountBAttachHelperBoneIndex < pVehicle->GetSkeleton()->GetBoneCount());


			CVehicleGadgetTowArm *pTowArmGadget = rage_new CVehicleGadgetTowArm(iAttachHelperBoneIndex, iMountAAttachHelperBoneIndex, iMountBAttachHelperBoneIndex, fTowArmMoveSpeed );
			pTowArmGadget->Init(pVehicle, pVehicle->GetVehicleModelInfo());
			pVehicle->AddVehicleGadget(pTowArmGadget);
		}

		// Search for the forks on a forklift
		iAttachHelperBoneIndex = pVehicle->GetBoneIndex(VEH_FORKS);
		s32 iAttachHelperBoneIndexA = pVehicle->GetBoneIndex(VEH_CARRIAGE);
		if(iAttachHelperBoneIndex > -1 && iAttachHelperBoneIndexA > -1)
		{
			ENABLE_FRAG_OPTIMIZATION_ONLY(pVehicle->GiveFragCacheEntry(true);)
			static dev_float fForkliftArmSleepMoveSpeed = 0.5f;

			CVehicleGadgetForks *pForkLiftGadget = rage_new CVehicleGadgetForks(iAttachHelperBoneIndex, iAttachHelperBoneIndexA, fForkliftArmSleepMoveSpeed );
			pForkLiftGadget->Init(pVehicle);
			pForkLiftGadget->InitJointLimits(pVehicle->GetVehicleModelInfo());
			pVehicle->AddVehicleGadget(pForkLiftGadget);
		}

		// Search for the frame on a Handler
		iAttachHelperBoneIndex = pVehicle->GetBoneIndex(VEH_HANDLER_FRAME_1);
		iAttachHelperBoneIndexA = pVehicle->GetBoneIndex(VEH_HANDLER_FRAME_2);
		if(iAttachHelperBoneIndex > -1 && iAttachHelperBoneIndexA > -1)
		{
			ENABLE_FRAG_OPTIMIZATION_ONLY(pVehicle->GiveFragCacheEntry(true);)
			static dev_float fHandlerFrameSleepMoveSpeed = 0.5f;

			CVehicleGadgetHandlerFrame* pForkLiftGadget = rage_new CVehicleGadgetHandlerFrame(iAttachHelperBoneIndex, iAttachHelperBoneIndexA, fHandlerFrameSleepMoveSpeed);
			pForkLiftGadget->Init(pVehicle);
			pForkLiftGadget->InitJointLimits(pVehicle->GetVehicleModelInfo());
			pVehicle->AddVehicleGadget(pForkLiftGadget);
		}

		// Search for bomb bay
		iAttachHelperBoneIndex = pVehicle->GetBoneIndex(VEH_DOOR_HATCH_R);
		iAttachHelperBoneIndexA = pVehicle->GetBoneIndex(VEH_DOOR_HATCH_L);
		if(iAttachHelperBoneIndex > -1 && iAttachHelperBoneIndexA > -1)
		{
			ENABLE_FRAG_OPTIMIZATION_ONLY(pVehicle->GiveFragCacheEntry(true);)
			static dev_float fBombBayDoorsMoveSpeed = 1.0f;

			CVehicleGadgetBombBay *pBombBayGadget = rage_new CVehicleGadgetBombBay(iAttachHelperBoneIndex, iAttachHelperBoneIndexA, fBombBayDoorsMoveSpeed);
			pBombBayGadget->InitJointLimits(pVehicle->GetVehicleModelInfo());
			pVehicle->AddVehicleGadget(pBombBayGadget);
		}

		// Search for a thresher.
		if(pVehicle->GetBoneIndex(VEH_PICKUP_REEL) > -1 && pVehicle->GetBoneIndex(VEH_AUGER) > -1)
		{
			ENABLE_FRAG_OPTIMIZATION_ONLY(pVehicle->GiveFragCacheEntry(true);)
			static dev_float fThreserRotationSpeed = 0.5f;

			s32 iPickupReelBoneIndex = pVehicle->GetBoneIndex(VEH_PICKUP_REEL);
			s32 iAugerBoneIndex = pVehicle->GetBoneIndex(VEH_AUGER);

			CVehicleGadgetThresher *pThresherGadget = rage_new CVehicleGadgetThresher(pVehicle, iPickupReelBoneIndex, iAugerBoneIndex, fThreserRotationSpeed );

			pThresherGadget->InitJointLimits(pVehicle->GetVehicleModelInfo());

			pVehicle->AddVehicleGadget(pThresherGadget);
		}

		//search for an articulated digger arm boom stick and bucket bone
		int iBaseAttachHelperBoneIndex = pVehicle->GetBoneIndex(VEH_ARTICULATED_DIGGER_ARM_BASE);
		int iBoomAttachHelperBoneIndex = pVehicle->GetBoneIndex(VEH_ARTICULATED_DIGGER_ARM_BOOM);
		int iStickAttachHelperBoneIndex = pVehicle->GetBoneIndex(VEH_ARTICULATED_DIGGER_ARM_STICK);
		int iBucketAttachHelperBoneIndex = pVehicle->GetBoneIndex(VEH_ARTICULATED_DIGGER_ARM_BUCKET);
		if(iBaseAttachHelperBoneIndex > VEH_INVALID_ID && (iBoomAttachHelperBoneIndex > VEH_INVALID_ID || iStickAttachHelperBoneIndex > VEH_INVALID_ID || iBucketAttachHelperBoneIndex > VEH_INVALID_ID) )
		{
			ENABLE_FRAG_OPTIMIZATION_ONLY(pVehicle->GiveFragCacheEntry(true);)
			static dev_float fDiggerArmSleepMoveSpeed = 1.0f;
			static dev_float fDiggerBoomJointInitRatio = 0.25f;//Keep the digger bucket off the ground when spawning

			CVehicleGadgetArticulatedDiggerArm *pDiggerArmGadget = rage_new CVehicleGadgetArticulatedDiggerArm( fDiggerArmSleepMoveSpeed );
			pDiggerArmGadget->SetDiggerJointBone(DIGGER_JOINT_BASE,iBaseAttachHelperBoneIndex);
			pDiggerArmGadget->SetDiggerJointBone(DIGGER_JOINT_BOOM,iBoomAttachHelperBoneIndex);
			pDiggerArmGadget->SetDiggerJointBone(DIGGER_JOINT_STICK,iStickAttachHelperBoneIndex);
			pDiggerArmGadget->SetDiggerJointBone(DIGGER_JOINT_BUCKET,iBucketAttachHelperBoneIndex);
			pDiggerArmGadget->InitJointLimits(pVehicle->GetVehicleModelInfo());
			pDiggerArmGadget->DriveJointToRatio(pVehicle, DIGGER_JOINT_BOOM, fDiggerBoomJointInitRatio);
			pDiggerArmGadget->InitAudio(pVehicle);
			pVehicle->AddVehicleGadget(pDiggerArmGadget);
		}



		//search for an articulated EOD arm and just set it up like a digger for now
		iBaseAttachHelperBoneIndex = pVehicle->GetBoneIndex(VEH_ARM1);
		iBoomAttachHelperBoneIndex = pVehicle->GetBoneIndex(VEH_ARM2);
		iStickAttachHelperBoneIndex = pVehicle->GetBoneIndex(VEH_ARM3);
		iBucketAttachHelperBoneIndex = pVehicle->GetBoneIndex(VEH_ARM4);
		if(iBoomAttachHelperBoneIndex > -1 && iStickAttachHelperBoneIndex > -1 && iBucketAttachHelperBoneIndex > -1)
		{
			ENABLE_FRAG_OPTIMIZATION_ONLY(pVehicle->GiveFragCacheEntry(true);)
			static dev_float fEodArmSleepMoveSpeed = 1.0f;

			CVehicleGadgetArticulatedDiggerArm *pDiggerArmGadget = rage_new CVehicleGadgetArticulatedDiggerArm( fEodArmSleepMoveSpeed );
			pDiggerArmGadget->SetDiggerJointBone(DIGGER_JOINT_BASE,iBaseAttachHelperBoneIndex);
			pDiggerArmGadget->SetDiggerJointBone(DIGGER_JOINT_BOOM,iBoomAttachHelperBoneIndex);
			pDiggerArmGadget->SetDiggerJointBone(DIGGER_JOINT_STICK,iStickAttachHelperBoneIndex);
			pDiggerArmGadget->SetDiggerJointBone(DIGGER_JOINT_BUCKET,iBucketAttachHelperBoneIndex);
			pDiggerArmGadget->InitJointLimits(pVehicle->GetVehicleModelInfo());
			pVehicle->AddVehicleGadget(pDiggerArmGadget);
		}

		// Make sure that each turret is registered for audio
		if(pVehicle->GetVehicleWeaponMgr())
		{
			for(u32 loop = 0; loop < pVehicle->GetVehicleWeaponMgr()->GetNumTurrets(); loop++)
			{
				pVehicle->GetVehicleWeaponMgr()->GetTurret(loop)->InitAudio(pVehicle);
			}
		}

		// Add a convertible roof audio handler if required
		if(pVehicle->DoesVehicleHaveAConvertibleRoofAnimation())
		{
			pVehicle->GetVehicleAudioEntity()->CreateVehicleGadget(audVehicleGadget::AUD_VEHICLE_GADGET_ROOF);
		}

		if( ( pVehicle->GetBoneIndex(VEH_SPOILER) != -1 && pVehicle->GetBoneIndex(VEH_STRUTS) != -1 ) ||
			( !CWheel::ms_bUseExtraWheelDownForce && pVehicle->pHandling->m_fDownforceModifier > 1.0f ) )
		{
			CVehicleGadgetDynamicSpoiler *pDynamicSpoilerGadget = rage_new CVehicleGadgetDynamicSpoiler();
			pDynamicSpoilerGadget->InitAudio(pVehicle);			
			pVehicle->AddVehicleGadget(pDynamicSpoilerGadget);
		}	

		if( pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR &&
			pVehicle->GetBoneIndex(VEH_FLAP_L) != -1 && 
			pVehicle->GetBoneIndex(VEH_FLAP_R) != -1 )
		{
			CVehicleGadgetDynamicFlaps *pDynamicFlapsGadget = rage_new CVehicleGadgetDynamicFlaps();
			pVehicle->AddVehicleGadget(pDynamicFlapsGadget);
		}	
	}	
}

void CVehicleFactory::ModifyVehicleTopSpeed(CVehicle* pVehicle, const float fPercentAdjustment)
{
	if(pVehicle)
	{
		pVehicle->SetTopSpeedPercentage(fPercentAdjustment);

		float adjustmentMultiplier = 1.0f + (fPercentAdjustment/100.0f);

		// do drive and gear ratio conversion stuff
		float fDriveForce = pVehicle->pHandling->m_fInitialDriveForce * adjustmentMultiplier;
		float fDriveMaxFlatVel = pVehicle->pHandling->m_fInitialDriveMaxFlatVel * adjustmentMultiplier;
		float fDriveMaxVel = fDriveMaxFlatVel * 1.2f; // add 20% to velocity used for gearing

        // Setup the transmission
        pVehicle->m_Transmission.SetupTransmission( fDriveForce, 
                                                    fDriveMaxFlatVel, 
                                                    fDriveMaxVel, 
                                                    pVehicle->m_Transmission.GetNumGears(),
													pVehicle );

		if(pVehicle->GetSecondTransmission())
		{
			pVehicle->GetSecondTransmission()->SetupTransmission(	fDriveForce, 
																	fDriveMaxFlatVel, 
																	fDriveMaxVel, 
																	1,
																	pVehicle );
		}

		pVehicle->m_fDragCoeff = pVehicle->pHandling->m_fInitialDragCoeff;
		if(adjustmentMultiplier != 0.0f)
		{
			pVehicle->m_fDragCoeff *= (1.0f/adjustmentMultiplier);
		}

		phArchetypeDamp *pArchetype = (phArchetypeDamp *)pVehicle->GetCurrentPhysicsInst()->GetArchetype();
		pArchetype->ActivateDamping(phArchetypeDamp::LINEAR_V2, Vector3(pVehicle->m_fDragCoeff, pVehicle->m_fDragCoeff, 0.0f));

		// Call this to make sure UpdateAirResistance() updates everything, since we have
		// messed with the damping values and m_fDragCoeff, potentially.
		pVehicle->RefreshAirResistance();
	}
}

/////////////////////////////
/////////////////////////////
// Debug widgets
/////////////////////////////
//
#if __BANK

s32 GetPoolId(u32 nModelId)
{
	CVehicleModelInfo *pVehModelInfo = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(nModelId)));
	fwArchetypeDynamicFactory<CVehicleModelInfo>& vehModelInfoStore = CModelInfo::GetVehicleModelInfoStore();
	atArray<CVehicleModelInfo*> vehicleTypes;
	vehModelInfoStore.GatherPtrs(vehicleTypes);

	for(int i=0; i<vehicleTypes.GetCount(); i++)
	{
		if(vehicleTypes[i] == pVehModelInfo)
			return i;
	}

	return -1;
}

u32 GetModelIndex(s32 nPoolId)
{
	fwArchetypeDynamicFactory<CVehicleModelInfo>& vehModelInfoStore = CModelInfo::GetVehicleModelInfoStore();
	atArray<CVehicleModelInfo*> vehicleTypes;
	vehModelInfoStore.GatherPtrs(vehicleTypes);

	Assert(vehicleTypes[nPoolId]);

	fwModelId  carId;
	CBaseModelInfo *pModelInfo = CModelInfo::GetBaseModelInfoFromName(vehicleTypes[nPoolId]->GetModelName(), &carId);

	if(pModelInfo)
		return carId.GetModelIndex();

	return fwModelId::MI_INVALID;
}


void UpdateAvailableCarsCB(void)
{
	fwArchetypeDynamicFactory<CVehicleModelInfo>& vehModelInfoStore = CModelInfo::GetVehicleModelInfoStore();
	atArray<CVehicleModelInfo*> vehicleTypePtrs;
	vehModelInfoStore.GatherPtrs(vehicleTypePtrs);

	numCarsAvailable = vehicleTypePtrs.GetCount();

//	pPedNumSlider->SetRange(0.0f, float(numCarsAvailable - 1));
}

void CVehicleFactory::CarNumChangeCB(void)
{
	carModelId = GetModelIndex(vehicleSorts[currVehicleNameSelection]);

	ms_variationDebugColor = 0;
	ms_variationDebugWindowTint = -1;
	if (carModelId != fwModelId::MI_INVALID)
	{
		CVehicleModelInfo* mi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(carModelId)));
		if (mi && ms_variationColorSlider)
		{
			ms_variationColorSlider->SetRange(0.f, (float)mi->GetNumPossibleColours() - 1.f);
		}

		if (ms_variationWindowTintSlider)
		{
			ms_variationWindowTintSlider->SetRange(-1.f, (float)CVehicleModelInfo::GetVehicleColours()->m_WindowColors.GetCount() - 1.f);
		}
	}
}

void CVehicleFactory::TrailerNumChangeCB(void)
{
	if(currVehicleTrailerNameSelection > 0)//0 is no trailer
		trailerModelId = GetModelIndex(vehicleTrailerSorts[currVehicleTrailerNameSelection-1]);
	else
		trailerModelId = fwModelId::MI_INVALID;//no trailer selected
}


CVehicle *CreateCarAtMatrix(u32 modelId, Matrix34 *pMatrix, Vector3::Param offset = ORIGIN, bool bOnlyIfTheresRoom = false)
{
	if(modelId != fwModelId::MI_INVALID)
	{
		fwModelId vehModelId((strLocalIndex(modelId)));
		CVehicleModelInfo *pVehModelInfo = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(vehModelId);

		bool bForceLoad = false;
		if(!CModelInfo::HaveAssetsLoaded(vehModelId))
		{
			CModelInfo::RequestAssets(vehModelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			bForceLoad = true;
		}

		if(bForceLoad)
		{
			CStreaming::LoadAllRequestedObjects(true);
		}

		if(!CModelInfo::HaveAssetsLoaded(vehModelId))
		{
			return NULL;
		}

		Matrix34 matrix = *pMatrix;
		if (pVehModelInfo)
		{
			matrix.d += Vector3(offset) * pVehModelInfo->GetBoundingSphereRadius();
		}

		if (bOnlyIfTheresRoom && pVehModelInfo)
		{
			if (!pVehModelInfo->GetFragType())
			{
				Displayf("CreateCar aborted due to car model info not having a frag type");
				return NULL;
			}

			phBound *bound = (phBound *)pVehModelInfo->GetFragType()->GetPhysics(0)->GetCompositeBounds();
			int nTestTypeFlags = ArchetypeFlags::GTA_BOX_VEHICLE_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_PED_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_MAP_TYPE_VEHICLE;

			WorldProbe::CShapeTestBoundDesc shapeTestDesc;
			shapeTestDesc.SetBound(bound);
			shapeTestDesc.SetTransform(&matrix);
			shapeTestDesc.SetIncludeFlags(nTestTypeFlags);

			if(WorldProbe::GetShapeTestManager()->SubmitTest(shapeTestDesc))
			{
				Displayf("CreateCar aborted due to an object blocking the spawn location");
				return NULL;
			}
		}

		CVehicle *pNewVehicle = CVehicleFactory::GetFactory()->Create(vehModelId, bCreateAsMissionCar ? ENTITY_OWNEDBY_SCRIPT : ENTITY_OWNEDBY_DEBUG, bCreateAsMissionCar ? POPTYPE_MISSION : POPTYPE_RANDOM_AMBIENT, &matrix, true, bPlaceOnRoadProperly);
		if(pNewVehicle)
		{
			pNewVehicle->SetIsAbandoned();

			pNewVehicle->m_nPhysicalFlags.bNotDamagedByBullets |= bBulletProof;
			pNewVehicle->m_nPhysicalFlags.bNotDamagedByFlames |= bFireProof;
			pNewVehicle->m_nPhysicalFlags.bNotDamagedByMelee |= bMeleeProof;
			pNewVehicle->m_nPhysicalFlags.bNotDamagedByCollisions |= bCollisionProof;
			pNewVehicle->m_nPhysicalFlags.bIgnoresExplosions |= bExplosionProof;

			CGameWorld::Add(pNewVehicle, CGameWorld::OUTSIDE);

			CPortalTracker* pPT = pNewVehicle->GetPortalTracker();
			if (pPT)
			{
				pPT->RequestRescanNextUpdate();
				pPT->Update(VEC3V_TO_VECTOR3(pNewVehicle->GetTransform().GetPosition()));
			}

			if(bCreateAsGangCar && pVehModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_EXTRAS_GANG))
			{
				pNewVehicle->InitExtraFlags(EXTRAS_SET_GANG);
				pNewVehicle->Fix();
			}

			if(nForceExtraOn > 0)
			{
				pNewVehicle->m_nDisableExtras &= ~BIT(nForceExtraOn);
				pNewVehicle->Fix();
			}

			if (pNewVehicle->GetCarDoorLocks() == CARLOCK_LOCKED_INITIALLY)		// The game creates police cars with doors locked. For debug cars we don't want this.
			{
				pNewVehicle->SetCarDoorLocks(CARLOCK_UNLOCKED);
			}

			// can turn off hotwiring anims when testing stuff with cars...
			pNewVehicle->m_nVehicleFlags.bCarNeedsToBeHotwired = bVehicleNeedsToBeHotwired;
			pNewVehicle->m_nVehicleFlags.bHasBeenOwnedByPlayer = !bVehicleNeedsToBeHotwired;

			if(bPlaceOnRoadProperly)
			{
				pNewVehicle->PlaceOnRoadAdjust();
				pMatrix->Set(MAT34V_TO_MATRIX34(pNewVehicle->GetMatrix()));
			}

			if(bLockDoors)
			{
				pNewVehicle->SetCarDoorLocks(CARLOCK_LOCKED_BUT_CAN_BE_DAMAGED);
			}

			// If this is a train we set it to be de-railed. This way the process function wont move it to a train track
			if (pNewVehicle->GetVehicleType() == VEHICLE_TYPE_TRAIN)
			{
				((CTrain*)pNewVehicle)->m_nTrainFlags.bCreatedAsScriptVehicle = true;
			}

#if VEHICLE_GLASS_SMASH_TEST
			VehicleGlassSmashTest(pNewVehicle);
#endif // VEHICLE_GLASS_SMASH_TEST
		}
		
		return pNewVehicle;
	}

	return NULL;
}

void ReloadVehicleMetaDataCB()
{
	CPopulationStreaming::FlushAllVehicleModelsHard();

	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::VEHICLE_METADATA_FILE);
	while(DATAFILEMGR.IsValid(pData))
	{
		struct CVehicleModelInfo::InitDataList* pInitDataList = NULL;
		if (PARSER.LoadObjectPtr(pData->m_filename, "meta", pInitDataList))
		{
			for (s32 i = 0; i < pInitDataList->m_InitDatas.GetCount(); ++i)
			{
				CVehicleModelInfo* pModelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfoFromName(pInitDataList->m_InitDatas[i].m_modelName.c_str(), NULL);
				if (!pModelInfo)
				{
					Assertf(false, "We don't support dynamically adding new vehicle models!");
					// FA: though we probably could, is it as easy as adding the model here?
					//CVehicleModelInfo* pModelInfo = CModelInfo::AddVehicleModel(pInitDataList->m_InitDatas[i].m_modelName.c_str());
					//pModelInfo->Init(pInitDataList->m_InitDatas[i]);
				}
				else
				{
					pModelInfo->ReInit(pInitDataList->m_InitDatas[i]);
				}
			}
		}
		delete pInitDataList;

		pData = DATAFILEMGR.GetNextFile(pData);
	}
}

void CreateCar(bool bOnlyIfTheresRoom)
{
	if(bCreateAsPersonalCar)
	{
		bCreateAsMissionCar = true;
	}

	CVehicle *pVehicle = NULL;
	if(CVehicleFactory::carModelId != fwModelId::MI_INVALID)
	{
#if VEHICLE_GLASS_SMASH_TEST
		if (g_vehicleGlassSmashTest.m_smashTestAuto && pVehModelInfo)
		{
			static bool bShowVehicleName = false; // can turn this on from the debugger if we need to show the vehicle name (e.g. to catch a vehicle which crashes)

			if (bShowVehicleName)
			{
				Displayf("creating \"%s\"", pVehModelInfo->GetModelName());
			}

			// [HACK GTAV]
			if (stricmp(pVehModelInfo->GetModelName(), "blimp"   ) == 0 || // don't create blimp during smash test
				stricmp(pVehModelInfo->GetModelName(), "barracks") == 0 || // don't create barracks during smash test (crashes) -- see BS#919428
				stricmp(pVehModelInfo->GetModelName(), "rdrcoch1") == 0 || // don't create rdrcoch1 during smash test (crashes) -- see BS#919198
				stricmp(pVehModelInfo->GetModelName(), "rdrcoch2") == 0 || // don't create rdrcoch2 during smash test (crashes)
				false)
			{
				return;
			}
		}
#endif // VEHICLE_GLASS_SMASH_TEST

		Matrix34 tempMat;
		tempMat.Identity();

		float fRotation = camInterface::GetDebugDirector().IsFreeCamActive() && bUseDebugCamRotation ? camInterface::GetHeading() : carDropRotation;
		tempMat.RotateZ(fRotation);

		tempMat.d = camInterface::GetDebugDirector().IsFreeCamActive() ? camInterface::GetPos() : VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition());

		Vector3 vecCreateOffset = camInterface::GetFront();
		vecCreateOffset.z = 0.0f;
		vecCreateOffset.NormalizeSafe();

		pVehicle = CreateCarAtMatrix(CVehicleFactory::carModelId, &tempMat, vecCreateOffset, bOnlyIfTheresRoom);
		if( pVehicle )
		{
			pVehicle->GetPortalTracker()->ScanUntilProbeTrue();

			//keep a record of the last created vehicle
			if (CVehicleFactory::ms_pLastCreatedVehicle) 
				CVehicleFactory::ms_pLastLastCreatedVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
			CVehicleFactory::ms_pLastCreatedVehicle = pVehicle;
			CVehicleFactory::ms_bForceHd = pVehicle->IsForcedHd();
			CVehicleFactory::ms_fLodMult = pVehicle->GetLodMultiplier();
			CVehicleFactory::ms_clampedRenderLod = pVehicle->GetClampedRenderLod();

			if(bCreateAsPersonalCar)
			{
				CVehicleDebug::SetVehicleAsPersonalCB();
			}

			if (CVehicleFactory::ms_liverySlider)
			{
				float fMaxRange = -1.0f; 
				if(pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_LIVERY))
					fMaxRange = pVehicle->GetVehicleModelInfo()->GetLiveriesCount() - 1.0f;
				
				CVehicleFactory::ms_liverySlider->SetRange(-1.0f, fMaxRange);
			}
			
			if (CVehicleFactory::ms_livery2Slider)
			{
				float fMaxRange = -1.0f; 
				if(pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_LIVERY))
					fMaxRange = pVehicle->GetVehicleModelInfo()->GetLiveries2Count() - 1.0f;
				
				CVehicleFactory::ms_livery2Slider->SetRange(-1.0f, fMaxRange);
			}

			CVehicleFactory::VariationDebugColorChangeCB();
			for (s32 i = 0; i <= VEH_LAST_EXTRA - VEH_EXTRA_1; ++i)
			{
				CVehicleFactory::ms_variationExtras[i] = CVehicleFactory::ms_pLastCreatedVehicle->GetIsExtraOn((eHierarchyId)(VEH_EXTRA_1 + i));
			}

			if (pVehicle->IsModded())
			{
				CVehicleFactory::VariationDebugUpdateMods();
			}


            if( sSpawnCarsWithNoBreaking &&
                pVehicle->GetFragInst() )
            {
                pVehicle->GetFragInst()->SetDisableBreakable( true );
            }
		}
	}

	if(CVehicleFactory::trailerModelId != fwModelId::MI_INVALID && pVehicle)//should we add a trailer to this vehicle?
	{
		Assertf(pVehicle->GetBoneIndex(VEH_ATTACH) > -1, "Vehicle missing trailer attach point");
		if(pVehicle->GetBoneIndex(VEH_ATTACH) > -1)//the vehicle must have an attach bone for this to work
		{
			Matrix34 tempMat;
			tempMat.Identity();
			tempMat.d = camInterface::GetDebugDirector().IsFreeCamActive() ? camInterface::GetPos() : VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition());

			CVehicle* pTrailerVehicle = NULL;
			if(CVehicleFactory::carModelId != fwModelId::MI_INVALID)
			{
				fwModelId trailerModelId((strLocalIndex(CVehicleFactory::trailerModelId)));

                bool bForceLoad = false;
		        if(!CModelInfo::HaveAssetsLoaded(trailerModelId))
		        {
			        CModelInfo::RequestAssets(trailerModelId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			        bForceLoad = true;
		        }

		        if(bForceLoad)
		        {
			        CStreaming::LoadAllRequestedObjects(true);
		        }

				const bool bUseBiggerBoxesForCollisionTest = false;
				pTrailerVehicle = CVehicleFactory::GetFactory()->CreateAndAttachTrailer(trailerModelId, pVehicle, tempMat, bCreateAsMissionCar ? ENTITY_OWNEDBY_SCRIPT : ENTITY_OWNEDBY_POPULATION, bCreateAsMissionCar ? POPTYPE_MISSION : POPTYPE_RANDOM_AMBIENT, bUseBiggerBoxesForCollisionTest);
			}

			//warp the trailer onto the vehicle
			if(pTrailerVehicle && pTrailerVehicle->GetVehicleType() == VEHICLE_TYPE_TRAILER)
			{
				pTrailerVehicle->GetPortalTracker()->ScanUntilProbeTrue();

				CTrailer* pTrailer = static_cast<CTrailer*>(pTrailerVehicle);

				// create cars on trailer
				if (sSpawnCarsOnTrailer)
				{
					if (pTrailer->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPAWN_BOAT_ON_TRAILER))
					{
						fwModelId boats[3] = 
						{
							CModelInfo::GetModelIdFromName("squalo"),
							CModelInfo::GetModelIdFromName("dinghy"),
							CModelInfo::GetModelIdFromName("tropic")
						};

						fwModelId b = boats[fwRandom::GetRandomNumberInRange(0,3)];
						if (b.IsValid() && !CModelInfo::HaveAssetsLoaded(b))
						{
							CModelInfo::RequestAssets(b, STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
							CStreaming::LoadAllRequestedObjects();
						}

						if (!CVehicleFactory::GetFactory()->AddBoatOnTrailer(pTrailer, CGameWorld::OUTSIDE, ENTITY_OWNEDBY_POPULATION, POPTYPE_RANDOM_PARKED, &b))
						{
							// We failed to create a boat. Check to see if we're still allowed to have this trailer
							CVehicleFactory::DestroyBoatTrailerIfNecessary((CTrailer**)&pTrailer);
						}
					}
					else
					{
						CVehicleFactory::GetFactory()->AddCarsOnTrailer(pTrailer, CGameWorld::OUTSIDE, ENTITY_OWNEDBY_POPULATION, POPTYPE_RANDOM_PARKED, NULL);
					}
				}
			}
		}
	}
}

void CreateCarCB()
{
	CreateCar(false);
}

void CreateChosenCarCB(void)
{
	Vector3 pos = camInterface::GetDebugDirector().IsFreeCamActive() ? camInterface::GetPos() : VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition());
	Vector3 vecCreateOffset = camInterface::GetFront();
	vecCreateOffset.z = 0.0f;
	vecCreateOffset.NormalizeSafe();
	vecCreateOffset *= 10.f;
	pos += vecCreateOffset;

	bool police = false;
	CVehicleFactory::carModelId = CVehiclePopulation::ChooseModel(&police, pos, false, false, false, 0, false, false, 2);

	CreateCarCB();
}

static void ExplodeCarCB(void)
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CVehicleFactory::ms_pLastCreatedVehicle->BlowUpCar(NULL, false, true, false);
	}
}

static void DisableVehicleCollisionCB(void)
{
	CVehicle* pVeh = CVehicleDebug::GetFocusVehicle();
	if(!pVeh && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVeh = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVeh)
	{
		pVeh->DisableCollision();
	}
}

static void DisableVehicleCollisionCompletelyCB(void)
{
	CVehicle* pVeh = CVehicleDebug::GetFocusVehicle();
	if(!pVeh && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVeh = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVeh)
	{
		pVeh->DisableCollision(NULL, true);
	}
}

static void EnableVehicleCollisionCB(void)
{
	CVehicle* pVeh = CVehicleDebug::GetFocusVehicle();
	if(!pVeh && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVeh = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVeh)
	{
		pVeh->EnableCollision();
	}
}

static void ResetCarSuspensionCB(void)
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		for(int i=0; i<CVehicleFactory::ms_pLastCreatedVehicle->GetNumWheels(); i++)
			CVehicleFactory::ms_pLastCreatedVehicle->GetWheel(i)->Reset(false);
	}
}

#if __DEV
static void ChangePetrolTankLevelCarCB(void)
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CVehicleFactory::ms_pLastCreatedVehicle->GetVehicleDamage()->SetPetrolTankLevel(sfPetrolTankLevel);
	}
}
#endif

static void SetVehicleOnFire(CVehicle* pVeh)
{
	if(!pVeh)
	{
		return;
	}
	
	if(g_fireMan.IsEntityOnFire(pVeh))
	{
		return;
	}
	
	int nFireBoneIndex = pVeh->GetBoneIndex(VEH_ENGINE);
	if(nFireBoneIndex < 0)
	{
		return;
	}
	
	Matrix34 mFireBone;
	pVeh->GetGlobalMtx(nFireBoneIndex, mFireBone);

	const float fBurnTime	  = fwRandom::GetRandomNumberInRange(20.0f, 35.0f);
	const float fBurnStrength = fwRandom::GetRandomNumberInRange(0.75f, 1.0f);
	const float fPeakTime	  = fwRandom::GetRandomNumberInRange(0.5f, 1.0f);

	// Start vehicle fire
	Vec3V vPosLcl = CVfxHelper::GetLocalEntityPos(*pVeh, RCC_VEC3V(mFireBone.d));
	Vec3V vNormLcl = CVfxHelper::GetLocalEntityDir(*pVeh, Vec3V(V_Z_AXIS_WZERO));

	g_fireMan.StartVehicleFire(pVeh, -1, vPosLcl, vNormLcl, CGameWorld::FindLocalPlayer(), fBurnTime, fBurnStrength, fPeakTime, FIRE_DEFAULT_NUM_GENERATIONS, Vec3V(V_ZERO), BANK_ONLY(Vec3V(V_ZERO),) false);
}

static void SetVehicleOnFireCB(void)
{
	CVehicle* pVeh = CVehicleDebug::GetFocusVehicle();
	if(!pVeh && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVeh = CVehicleFactory::ms_pLastCreatedVehicle;
	}
	
	if(pVeh)
	{
		SetVehicleOnFire(pVeh);
	}
}

static void SetVehicleOnFireAndClearHealthCB(void)
{
	CVehicle* pVeh = CVehicleDebug::GetFocusVehicle();
	if(!pVeh && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVeh = CVehicleFactory::ms_pLastCreatedVehicle;
	}
	
	if(pVeh)
	{
		SetVehicleOnFire(pVeh);
		pVeh->SetHealth(0.0f);
	}	
}

static void SetVehicleSearchlightCB(void)
{
	CVehicle* pVeh = CVehicleDebug::GetFocusVehicle();
	if(!pVeh && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVeh = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVeh)
	{
		CVehicleWeaponMgr* pWeaponMgr = pVeh->GetVehicleWeaponMgr();
		if(pWeaponMgr)
		{
			for(int i = 0; i < pWeaponMgr->GetNumVehicleWeapons(); i++)
			{
				CVehicleWeapon* pWeapon = pWeaponMgr->GetVehicleWeapon(i);
				if(pWeapon)
				{
					if(pWeapon->GetType() == VGT_SEARCHLIGHT)
					{
						CSearchLight* pSearchLight = static_cast<CSearchLight*>(pWeapon);
						bool bLightToggle = !pSearchLight->GetLightOn();
						pSearchLight->SetLightOn(bLightToggle);
						return;
					}
				}
			}
		}
	}	
}

void DeleteCarCB(void)
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		g_ptFxManager.RemovePtFxFromEntity(CVehicleFactory::ms_pLastCreatedVehicle);

		CVehicleFactory::GetFactory()->Destroy(CVehicleFactory::ms_pLastCreatedVehicle);
		CVehicleFactory::ms_pLastCreatedVehicle = NULL;
		CVehicleFactory::ms_bForceHd = false;
		CVehicleFactory::ms_fLodMult = 1.f;
		CVehicleFactory::ms_clampedRenderLod = -1;

// Obbe was going to tell me how to stream the car out again, but he left
//		if(pModelInfo->GetNumRefs()==0)
//		{
//			CStreaming::D
//		}
	}
}

void CreateNextCar(bool bOnlyIfTheresRoom)
{
	// delete existing car if it exists - this widget is meant for cycling through vehicles
	DeleteCarCB();

	CVehicleFactory::currVehicleNameSelection++;
	if(CVehicleFactory::currVehicleNameSelection >= numVehicleNames)
	{
		atDisplayf("Last vehicle has been loaded.");
		CVehicleFactory::currVehicleNameSelection = 0;
	}
	CVehicleFactory::CarNumChangeCB();
	CVehicleFactory::TrailerNumChangeCB();

	CreateCar(bOnlyIfTheresRoom);
}

void CreateNextCarCB(void)
{
	CreateNextCar(false);
}

void CreateNextBikeCB(void)
{
	// delete existing car if it exists - this widget is meant for cycling through vehicles
	DeleteCarCB();

	int numVehiclesChecked = 0;
	while(CVehicleFactory::currVehicleNameSelection < numCarsAvailable)
	{
		CVehicleFactory::currVehicleNameSelection++;

		// Wrap around
		if(CVehicleFactory::currVehicleNameSelection >= numCarsAvailable)
		{
			CVehicleFactory::currVehicleNameSelection = 0;
		}

		// Make sure we don't loop forever if a bike isn't found.
		numVehiclesChecked++;
		if(numVehiclesChecked >= numCarsAvailable)
		{
			CVehicleFactory::currVehicleNameSelection = 0;
			break;
		}

		u32 carModelId = GetModelIndex(vehicleSorts[CVehicleFactory::currVehicleNameSelection]);

		if (carModelId != fwModelId::MI_INVALID)
		{
			CVehicleModelInfo* mi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(carModelId)));

			if(mi->GetIsBike())// Have we found a bike?
			{
				break;
			}
		}
	}

	CVehicleFactory::CarNumChangeCB();
	CVehicleFactory::TrailerNumChangeCB();

	CreateCarCB();
}

void CycleNextColourCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		u8 nNewColour1, nNewColour2, nNewColour3, nNewColour4, nNewColour5, nNewColour6;
		CVehicleFactory::ms_pLastCreatedVehicle->GetVehicleModelInfo()->ChooseVehicleColour(nNewColour1, nNewColour2, nNewColour3, nNewColour4, nNewColour5, nNewColour6, 1);

		CVehicleFactory::ms_pLastCreatedVehicle->SetBodyColour1(nNewColour1); 
		CVehicleFactory::ms_pLastCreatedVehicle->SetBodyColour2(nNewColour2); 
		CVehicleFactory::ms_pLastCreatedVehicle->SetBodyColour3(nNewColour3); 
		CVehicleFactory::ms_pLastCreatedVehicle->SetBodyColour4(nNewColour4);
		CVehicleFactory::ms_pLastCreatedVehicle->SetBodyColour5(nNewColour5);
		CVehicleFactory::ms_pLastCreatedVehicle->SetBodyColour6(nNewColour6);

		// let shaders know, that body colours changed
		CVehicleFactory::ms_pLastCreatedVehicle->UpdateBodyColourRemapping();
	}
}

void ChooseColorCB()
{
	CVehicle* veh = NULL;
	CEntity* selected = (CEntity*)g_PickerManager.GetSelectedEntity();
	if (selected && selected->GetIsTypeVehicle())
		veh = (CVehicle*)selected;

	if (!veh)
		veh = CVehicleFactory::ms_pLastCreatedVehicle;

	if(veh)
	{
		u8 nNewColour1, nNewColour2, nNewColour3, nNewColour4, nNewColour5, nNewColour6;
		veh->GetVehicleModelInfo()->ChooseVehicleColourFancy(veh, nNewColour1, nNewColour2, nNewColour3, nNewColour4, nNewColour5, nNewColour6);

		veh->SetBodyColour1(nNewColour1); 
		veh->SetBodyColour2(nNewColour2); 
		veh->SetBodyColour3(nNewColour3); 
		veh->SetBodyColour4(nNewColour4);
		veh->SetBodyColour5(nNewColour5);
		veh->SetBodyColour6(nNewColour6);

		// let shaders know, that body colours changed
		veh->UpdateBodyColourRemapping();
	}
}

fiStream *pLayoutsFile = NULL;
fiStream *pMinSeatHeightFile = NULL;

void VehicleModelInfoIterator(CVehicleModelInfo *pVehicleModelInfo)
{
	Displayf("%s...", pVehicleModelInfo->GetModelName());

	char szBuffer[256]; szBuffer[0] = '\0';

	fwModelId vehicleModelId; fwArchetypeManager::GetArchetypeFromHashKey(pVehicleModelInfo->GetModelNameHash(), vehicleModelId);
	CModelInfo::RequestAssets(vehicleModelId, STRFLAG_DONTDELETE | STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD);
	CStreaming::LoadAllRequestedObjects(true);
	Assert(CModelInfo::HaveAssetsLoaded(vehicleModelId));

	Matrix34 mVehicle; mVehicle.Identity();
	CVehicle *pVehicle = CVehicleFactory::GetFactory()->Create(vehicleModelId, ENTITY_OWNEDBY_DEBUG, POPTYPE_TOOL, &mVehicle, false, true);
	Assert(pVehicle);

	sprintf(szBuffer, "%s", pVehicleModelInfo->GetModelName());
	pLayoutsFile->WriteByte(szBuffer, istrlen(szBuffer));

	const CVehicleLayoutInfo *pVehicleLayoutInfo = pVehicleModelInfo->GetVehicleLayoutInfo();
	const CModelSeatInfo *pModelSeatInfo = pVehicleModelInfo->GetModelSeatInfo();
	if(pVehicleLayoutInfo && pModelSeatInfo)
	{
		const char *szVehiclePack = pVehicleModelInfo->GetVehicleDLCPack().TryGetCStr();

		const char *szVehicleType;
		switch(pVehicle->GetVehicleType())
		{
		case VEHICLE_TYPE_CAR					: szVehicleType = "VEHICLE_TYPE_CAR"					; break;
		case VEHICLE_TYPE_PLANE					: szVehicleType = "VEHICLE_TYPE_PLANE"					; break;
		case VEHICLE_TYPE_TRAILER				: szVehicleType = "VEHICLE_TYPE_TRAILER"				; break;
		case VEHICLE_TYPE_QUADBIKE				: szVehicleType = "VEHICLE_TYPE_QUADBIKE"				; break;
		case VEHICLE_TYPE_DRAFT					: szVehicleType = "VEHICLE_TYPE_DRAFT"					; break;
		case VEHICLE_TYPE_SUBMARINECAR			: szVehicleType = "VEHICLE_TYPE_SUBMARINECAR"			; break;
		case VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE	: szVehicleType = "VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE"	; break;
		case VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE	: szVehicleType = "VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE"	; break;
		case VEHICLE_TYPE_HELI					: szVehicleType = "VEHICLE_TYPE_HELI"					; break;
		case VEHICLE_TYPE_BLIMP					: szVehicleType = "VEHICLE_TYPE_BLIMP"					; break;
		case VEHICLE_TYPE_AUTOGYRO				: szVehicleType = "VEHICLE_TYPE_AUTOGYRO"				; break;
		case VEHICLE_TYPE_BIKE					: szVehicleType = "VEHICLE_TYPE_BIKE"					; break;
		case VEHICLE_TYPE_BICYCLE				: szVehicleType = "VEHICLE_TYPE_BICYCLE"				; break;
		case VEHICLE_TYPE_BOAT					: szVehicleType = "VEHICLE_TYPE_BOAT"					; break;
		case VEHICLE_TYPE_TRAIN					: szVehicleType = "VEHICLE_TYPE_TRAIN"					; break;
		case VEHICLE_TYPE_SUBMARINE				: szVehicleType = "VEHICLE_TYPE_SUBMARINE"				; break;
		default:
			{
				Assert(false);
				szVehicleType = "UNKNOWN";
			} break;
		}

		sprintf(szBuffer, ",%s,%s,%s,%s", szVehiclePack, szVehicleType, pVehicle->CarHasRoof() ? "TRUE" : "FALSE", pVehicleLayoutInfo->GetName().TryGetCStr());
		pLayoutsFile->WriteByte(szBuffer, istrlen(szBuffer));
	
		s32 iNumSeatsLayout = pVehicleLayoutInfo->GetNumSeats();
		s32 iNumSeatsActual = pModelSeatInfo->GetNumSeats();
		sprintf(szBuffer, ",%i,%i,%s", iNumSeatsLayout, iNumSeatsActual, iNumSeatsLayout == iNumSeatsActual ? "TRUE" : "FALSE");
		pLayoutsFile->WriteByte(szBuffer, istrlen(szBuffer));

		float fMinSeatHeight = pVehicleModelInfo->GetMinSeatHeight();

		for(int i = 0, count = pModelSeatInfo->GetNumSeats(); i < count; i ++)
		{
			bool bHasHit = false;
			float fHeight = 10.0f;

			s32 seatBoneIdx = pModelSeatInfo->GetBoneIndexFromSeat(i);
			if (seatBoneIdx >= 0)
			{
				Matrix34 mSeatTemp; pVehicle->GetGlobalMtx(seatBoneIdx, mSeatTemp);
				Mat34V mSeat = RC_MAT34V(mSeatTemp);
				Vec3V vSeat(V_ZERO); vSeat = Transform(mSeat, vSeat);

				Vector3 startPosition = RC_VECTOR3(vSeat); startPosition.z += 0.6f;
				Vector3 endPosition = RC_VECTOR3(vSeat); endPosition.z += 9.4f;

				WorldProbe::CShapeTestProbeDesc probeTest;
				WorldProbe::CShapeTestFixedResults<1> probeTestResults;
				probeTest.SetResultsStructure(&probeTestResults);
				probeTest.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE);
				probeTest.SetIsDirected(false);
				probeTest.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
				probeTest.SetIncludeEntity(pVehicle, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
				probeTest.SetStartAndEnd(startPosition, endPosition);

				bHasHit = WorldProbe::GetShapeTestManager()->SubmitTest(probeTest);
				if(bHasHit)
				{
					if(probeTestResults.GetNumHits() > 0)
					{
						WorldProbe::CShapeTestHitPoint &hitPoint = probeTestResults[0];
						fHeight = (hitPoint.GetHitPosition().z - startPosition.z) + 0.6f;
					}
				}

				if(fMinSeatHeight == -1.0f || fMinSeatHeight > fHeight)
				{
					fMinSeatHeight = fHeight;
				}
			}
			else
			{
				fHeight = -1.0f;
			}

			const CVehicleSeatInfo *pSeatInfo = pVehicleLayoutInfo->GetSeatInfo(i);
			if(pSeatInfo)
			{
				sprintf(szBuffer, ",%s,%.3f,%.3f", pSeatInfo->GetName().TryGetCStr(), pSeatInfo->GetHairScale(), fHeight);
				pLayoutsFile->WriteByte(szBuffer, istrlen(szBuffer));
			}
		}

		if(pModelSeatInfo->GetNumSeats() > 0)
		{
			pVehicleModelInfo->SetMinSeatHeight(fMinSeatHeight);

			sprintf(szBuffer, "%s,%.3f\n", pVehicleModelInfo->GetModelName(), fMinSeatHeight);
			pMinSeatHeightFile->WriteByte(szBuffer, istrlen(szBuffer));
		}
	}

	sprintf(szBuffer, "\n");
	pLayoutsFile->WriteByte(szBuffer, istrlen(szBuffer));

	CVehicleFactory::GetFactory()->Destroy(pVehicle); pVehicle = NULL;

	CStreaming::ClearRequiredFlag(vehicleModelId.ConvertToStreamingIndex(), CModelInfo::GetStreamingModuleId(), STRFLAG_DONTDELETE);
}

void DumpVehicleLayoutsCB()
{
	Displayf("Dumping vehicle layouts...");

	ASSET.PushFolder(RS_PROJROOT);

	// Open CSV file
	pLayoutsFile = ASSET.Create("VehicleLayouts", "csv");
	pMinSeatHeightFile = ASSET.Create("VehicleMinSeatHeights", "csv");

	char szBuffer[256]; szBuffer[0] = '\0';
	sprintf(szBuffer, "Model Name,Pack,Vehicle Type,Car Has Roof?,Layout Name, Seats (Layout), Seats (Validated), Matching Seat Data?\n");
	pLayoutsFile->WriteByte(szBuffer, istrlen(szBuffer));

	sprintf(szBuffer, "Vehicle,MinSeatHeight\n");
	pMinSeatHeightFile->WriteByte(szBuffer, istrlen(szBuffer));

	fwArchetypeDynamicFactory<CVehicleModelInfo>& vehModelInfoStore = CModelInfo::GetVehicleModelInfoStore();
	vehModelInfoStore.ForAllItemsUsed(VehicleModelInfoIterator);

	pLayoutsFile->Close(); pLayoutsFile = NULL;
	pMinSeatHeightFile->Close(); pMinSeatHeightFile = NULL;

	ASSET.PopFolder();

	Displayf("Dumping vehicle layouts... done!");
}

void FireCarCB()
{
	if(CVehicleFactory::ms_bRegenerateFireVehicle)
	{
		DeleteCarCB();
		CreateCarCB();
	}
	else if(!CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CreateCarCB();
	}

	if(CVehicleFactory::ms_pLastCreatedVehicle && sFireCarPos.IsNonZero())
	{
		Matrix34 matSet;
		matSet.MakeRotateZ(sFireCarOrient.z);
		matSet.RotateLocalX(sFireCarOrient.x);
		matSet.RotateLocalY(sFireCarOrient.y);
		matSet.d = sFireCarPos;
		CVehicleFactory::ms_pLastCreatedVehicle->Teleport(matSet.d);
		CVehicleFactory::ms_pLastCreatedVehicle->SetMatrix(matSet, true, true, true);

		Vector3 vecSpeed = sFireCarDirn;
		vecSpeed.NormalizeSafe(Vector3(1.0f,0.0f,0.0f));
		vecSpeed.Scale(sFireCarVel);
		CVehicleFactory::ms_pLastCreatedVehicle->SetVelocity(sFireCarVel * sFireCarDirn);

		vecSpeed = svFireCarSpin;
		matSet.Transform3x3(vecSpeed);

		CVehicleFactory::ms_pLastCreatedVehicle->SetAngVelocity(svFireCarSpin);
		
	}
}

void FireCarFromDebugCamCB()
{
	const camFrame& freeCamFrame = camInterface::GetDebugDirector().GetFreeCamFrame();
	sFireCarPos		= freeCamFrame.GetPosition();
	sFireCarDirn	= freeCamFrame.GetFront();

	FireCarCB();
}



#if ENABLE_CDEBUG
extern void DisplayObject(bool bDeletePrevious);
extern RegdEnt gpDisplayObject;
#endif // ENABLE_CDEBUG

void FireCarWithObjectCB()
{
	if(CVehicleFactory::ms_bRegenerateFireVehicle)
	{
		DeleteCarCB();
		CreateCarCB();
	}
	else if(!CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CreateCarCB();
	}

	if(CVehicleFactory::ms_pLastCreatedVehicle && sFireCarPos.IsNonZero())
	{
		Matrix34 matSet;
		matSet.MakeRotateZ(sFireCarOrient.z);
		matSet.RotateLocalX(sFireCarOrient.x);
		matSet.RotateLocalY(sFireCarOrient.y);
		matSet.d = sFireCarPos;
		CVehicleFactory::ms_pLastCreatedVehicle->Teleport(matSet.d);
		CVehicleFactory::ms_pLastCreatedVehicle->SetMatrix(matSet, true, true, true);

		Vector3 vecSpeed = sFireCarDirn;
		vecSpeed.NormalizeSafe(Vector3(1.0f,0.0f,0.0f));
		vecSpeed.Scale(sFireCarVel);
		CVehicleFactory::ms_pLastCreatedVehicle->SetVelocity(sFireCarVel * sFireCarDirn);

		vecSpeed = svFireCarSpin;
		matSet.Transform3x3(vecSpeed);

		CVehicleFactory::ms_pLastCreatedVehicle->SetAngVelocity(svFireCarSpin);
	}

#if ENABLE_CDEBUG
	DisplayObject(true);

	if(gpDisplayObject && gpDisplayObject->GetIsPhysical())
	{
		CPhysical* pPhysicalObject = (CPhysical*)gpDisplayObject.Get();
		Matrix34 matSet;
		matSet.MakeRotateZ(sFireCarOrient.z);
		
		Vector3 offset = sFireObjectOffset;
		matSet.Transform3x3(offset);
		matSet.d = sFireCarPos + offset;

		pPhysicalObject->Teleport(matSet.d, true, false, true, false, true, false, true);
		pPhysicalObject->SetMatrix(matSet, true, true, true);
	}
#endif  //ENABLE_CDEBUG
}

void FireCarWithObjectFromDebugCamCB()
{
	const camFrame& freeCamFrame = camInterface::GetDebugDirector().GetFreeCamFrame();
	sFireCarPos		= freeCamFrame.GetPosition();
	sFireCarDirn	= freeCamFrame.GetFront();

	FireCarWithObjectCB();
}

void FireCarOrientCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle && sFireCarPos.IsNonZero())
	{
		Matrix34 matSet;
		matSet.MakeRotateZ(sFireCarOrient.z);
		matSet.RotateLocalX(sFireCarOrient.x);
		matSet.RotateLocalY(sFireCarOrient.y);
		matSet.d = sFireCarPos;
		CVehicleFactory::ms_pLastCreatedVehicle->Teleport(matSet.d);
		CVehicleFactory::ms_pLastCreatedVehicle->SetMatrix(matSet, true, true, true);

		CVehicleFactory::ms_pLastCreatedVehicle->DeActivatePhysics();
	}
}

void FireCarForcePos()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle && sFireCarPos.IsNonZero())
	{
		Matrix34 matSet;
		matSet.MakeRotateZ(sFireCarOrient.z);
		matSet.RotateLocalX(sFireCarOrient.x);
		matSet.RotateLocalY(sFireCarOrient.y);
		matSet.d = sFireCarPos;
		CVehicleFactory::ms_pLastCreatedVehicle->Teleport(matSet.d);
		CVehicleFactory::ms_pLastCreatedVehicle->SetMatrix(matSet, true, true, true);
		CVehicleFactory::ms_pLastCreatedVehicle->ActivatePhysics();
	}
}

void FirePlayerRagdollCB()
{
	if(CGameWorld::FindLocalPlayer() && !CGameWorld::FindLocalPlayer()->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		CPed* pPed = CGameWorld::FindLocalPlayer();

		Matrix34 matSet;
		matSet.MakeRotateZ(sFireCarOrient.z);
		matSet.d = sFireCarPos;
		pPed->Teleport(matSet.d, pPed->GetCurrentHeading(), false);
		pPed->SetMatrix(matSet, true, true, true);
		CPhysics::ClearManifoldsForInst(pPed->GetCurrentPhysicsInst());

		CTaskNMBehaviour* pTaskNM = rage_new CTaskNMBalance(1000, 10000, NULL, 0);
		CEventSwitch2NM event(10000, pTaskNM);
		pTaskNM = NULL;
		pPed->SwitchToRagdoll(event);

		Vector3 vecSpeed = sFireCarDirn;
		vecSpeed.NormalizeSafe(Vector3(1.0f,0.0f,0.0f));
		vecSpeed.Scale(sFireCarVel);
		pPed->SetVelocity(sFireCarVel * sFireCarDirn);
	}
}

void FirePlayerRagdollFromDebugCamCB()
{
	const camFrame& freeCamFrame = camInterface::GetDebugDirector().GetFreeCamFrame();
	sFireCarPos		= freeCamFrame.GetPosition();
	sFireCarDirn	= freeCamFrame.GetFront();

	FirePlayerRagdollCB();
}

void FireObjectCB()
{
#if ENABLE_CDEBUG
	DisplayObject(true);

	if(gpDisplayObject && gpDisplayObject->GetIsPhysical())
	{
		CPhysical* pPhysicalObject = (CPhysical*)gpDisplayObject.Get();
		Matrix34 matSet;
		matSet.MakeRotateZ(sFireCarOrient.z);
		matSet.d = sFireCarPos;
		pPhysicalObject->Teleport(matSet.d, true, false, true, false, true, false, true);
		pPhysicalObject->SetMatrix(matSet, true, true, true);

		Vector3 vecSpeed = sFireCarDirn;
		vecSpeed.NormalizeSafe(Vector3(1.0f,0.0f,0.0f));
		vecSpeed.Scale(sFireCarVel);

		if (fragInst* pFragInst = pPhysicalObject->GetFragInst())
		{
			pFragInst->BreakOffAboveGroup(0);
		}

		pPhysicalObject->SetVelocity(sFireCarVel * sFireCarDirn);
	}
#endif // ENABLE_CDEBUG
}

void FireObjectFromDebugCamCB()
{
#if ENABLE_CDEBUG
	const camFrame& freeCamFrame = camInterface::GetDebugDirector().GetFreeCamFrame();
	sFireCarPos		= freeCamFrame.GetPosition();
	sFireCarDirn	= freeCamFrame.GetFront();

	FireObjectCB();
#endif // ENABLE_CDEBUG
}

const char* g_vehicleLaunchSettingsLocation = "C:\\VehicleLaunchSettings.txt";
void SaveSettingsCB()
{
	// Open the file for writing
	fiStream* stream = fiStream::Open(g_vehicleLaunchSettingsLocation, false);
	if(!stream)
	{
		stream = fiStream::Create(g_vehicleLaunchSettingsLocation);
	}

	if(stream)
	{
		// Initialise the tokens
		fiAsciiTokenizer token;
		token.Init("launchSettings", stream);

		// Write all the field names and values

		// Vel
		token.PutStrLine("Vel %f", sFireCarVel);

		// Pos
		token.PutStrLine("PosX %f", sFireCarPos.x);
		token.PutStrLine("PosY %f", sFireCarPos.y);
		token.PutStrLine("PosZ %f", sFireCarPos.z);

		// Dir
		token.PutStrLine("DirX %f", sFireCarDirn.x);
		token.PutStrLine("DirY %f", sFireCarDirn.y);
		token.PutStrLine("DirZ %f", sFireCarDirn.z);

		// Orient
		token.PutStrLine("OrientX %f", sFireCarOrient.x);
		token.PutStrLine("OrientY %f", sFireCarOrient.y);
		token.PutStrLine("OrientZ %f", sFireCarOrient.z);

		// Offset
		token.PutStrLine("OffsetX %f", sFireObjectOffset.x);
		token.PutStrLine("OffsetY %f", sFireObjectOffset.y);
		token.PutStrLine("OffsetZ %f", sFireObjectOffset.z);

		stream->Close();
	}
}

void LoadSettingsCB()
{
	// Open the file for reading
	fiStream* stream = fiStream::Open(g_vehicleLaunchSettingsLocation, true);
	if(stream)
	{
		// Initialise the tokens
		fiAsciiTokenizer token;
		token.Init("launchSettings", stream);

		// Read in all the field names and values
		// We're just assuming everything will be there and in order - So just tossing the names rather than checking
		char charBuff[128];

		// Vel
		token.GetToken(charBuff, 128);
		sFireCarVel = token.GetFloat();

		// PosX
		token.GetToken(charBuff, 128);
		sFireCarPos.x = token.GetFloat();
		// PosY
		token.GetToken(charBuff, 128);
		sFireCarPos.y = token.GetFloat();
		// PosZ
		token.GetToken(charBuff, 128);
		sFireCarPos.z = token.GetFloat();

		// DirX
		token.GetToken(charBuff, 128);
		sFireCarDirn.x = token.GetFloat();
		// DirY
		token.GetToken(charBuff, 128);
		sFireCarDirn.y = token.GetFloat();
		// DirZ
		token.GetToken(charBuff, 128);
		sFireCarDirn.z = token.GetFloat();

		// OrientX
		token.GetToken(charBuff, 128);
		sFireCarOrient.x = token.GetFloat();
		// OrientY
		token.GetToken(charBuff, 128);
		sFireCarOrient.y = token.GetFloat();
		// OrientZ
		token.GetToken(charBuff, 128);
		sFireCarOrient.z = token.GetFloat();

		// OffsetX
		token.GetToken(charBuff, 128);
		sFireObjectOffset.x = token.GetFloat();
		// OffsetY
		token.GetToken(charBuff, 128);
		sFireObjectOffset.y = token.GetFloat();
		// OffsetZ
		token.GetToken(charBuff, 128);
		sFireObjectOffset.z = token.GetFloat();

		stream->Close();
	}
}

void DropFocusedVehicleAtCameraPosCB(void)
{
#if __DEV//def CAM_DEBUG
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypeVehicle() )
		{
			CVehicle* pFocusVehicle = static_cast<CVehicle*>(pEnt);

			camDebugDirector& debugDirector = camInterface::GetDebugDirector();
			if(debugDirector.IsFreeCamActive())
			{
				const Vector3& pos = debugDirector.GetFreeCamFrame().GetPosition();
				pFocusVehicle->Teleport(pos);
			}
		}
	}
#endif // __DEV
}

void WarpPlayerIntoRecentCarCB()
{	
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if( pVehicle )
	{
		s32 seat = CVehicleFactory::ms_iSeatToEnter;
		CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
		pPlayerPed->SetPedInVehicle(pVehicle, seat, CPed::PVF_IfForcedTestVehConversionCollision|CPed::PVF_Warp);
	}
	else if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		// skip the engine-start anim?
		if (bVehicleInstantStartEngineOnWarp &&
			!CVehicleFactory::ms_pLastCreatedVehicle->m_nVehicleFlags.bCarNeedsToBeHotwired)
			CVehicleFactory::ms_pLastCreatedVehicle->SwitchEngineOn(true);

		CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
		s32 seat = -1;
		if( pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) &&
			pPlayerPed->GetMyVehicle() )
		{
			seat = pPlayerPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPlayerPed);
			pPlayerPed->SetPedOutOfVehicle(CPed::PVF_Warp);
			if( CVehicleFactory::ms_pLastCreatedVehicle != pPlayerPed->GetMyVehicle() )
			{
				seat = 0;
			}
			else
			{
				++seat;
				if( seat >= pPlayerPed->GetMyVehicle()->GetSeatManager()->GetMaxSeats() )
					seat = 0;
			}
		}
		else
		{
			pPlayerPed->GetPedIntelligence()->FlushImmediately();
			seat = 0;
		}
		pPlayerPed->SetPedInVehicle(CVehicleFactory::ms_pLastCreatedVehicle, seat, CPed::PVF_IfForcedTestVehConversionCollision|CPed::PVF_Warp);
	}
}


void WarpPlayerOutOfCarCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
		pPlayerPed->SetPedOutOfVehicle(CPed::PVF_Warp);
	}
}

void SetLaunchDirectionMatchOrientationCB()
{
	Matrix34 matSet;
	matSet.MakeRotateZ(sFireCarOrient.z);
	matSet.RotateLocalX(sFireCarOrient.x);
	matSet.RotateLocalY(sFireCarOrient.y);

	sFireCarDirn = matSet.b;
}

void FireCarWithPlayerCB()
{
	if(CVehicleFactory::ms_bRegenerateFireVehicle)
	{
		DeleteCarCB();
		CreateCarCB();
	}
	else if(!CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CreateCarCB();
	}

	if(CVehicleFactory::ms_pLastCreatedVehicle && sFireCarPos.IsNonZero())
	{

		WarpPlayerIntoRecentCarCB();

		Matrix34 matSet;
		matSet.MakeRotateZ(sFireCarOrient.z);
		matSet.RotateLocalX(sFireCarOrient.x);
		matSet.RotateLocalY(sFireCarOrient.y);
		matSet.d = sFireCarPos;
		CVehicleFactory::ms_pLastCreatedVehicle->Teleport(matSet.d);
		CVehicleFactory::ms_pLastCreatedVehicle->SetMatrix(matSet, true, true, true);

		Vector3 vecSpeed = sFireCarDirn;
		vecSpeed.NormalizeSafe(Vector3(1.0f,0.0f,0.0f));
		vecSpeed.Scale(sFireCarVel);
		CVehicleFactory::ms_pLastCreatedVehicle->SetVelocity(sFireCarVel * sFireCarDirn);

		vecSpeed = svFireCarSpin;
		matSet.Transform3x3(vecSpeed);

		CVehicleFactory::ms_pLastCreatedVehicle->SetAngVelocity(svFireCarSpin);
	}
}

void FireCarWithPlayerFromDebugCamCB()
{
	const camFrame& freeCamFrame = camInterface::GetDebugDirector().GetFreeCamFrame();
	sFireCarPos		= freeCamFrame.GetPosition();
	sFireCarDirn	= freeCamFrame.GetFront();

	FireCarWithPlayerCB();
}

void GetPlayerToEnterSeat()
{
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if( pPlayerPed && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		s32 seat = CVehicleFactory::ms_iSeatToEnter;

		// Return to the vehicle
		pPlayerPed->GetPedIntelligence()->AddTaskAtPriority( rage_new CTaskEnterVehicle(CVehicleFactory::ms_pLastCreatedVehicle, SR_Specific, seat), PED_TASK_PRIORITY_PRIMARY);
	}
}

void GetFocusPedToEnterSeat()
{
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
		{
			CPed* pFocusPed = static_cast<CPed*>(pEnt);
			if( pFocusPed && CVehicleFactory::ms_pLastCreatedVehicle)
			{
				// Return to the vehicle
				pFocusPed->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskEnterVehicle(CVehicleFactory::ms_pLastCreatedVehicle, SR_Specific, CVehicleFactory::ms_iSeatToEnter), PED_TASK_PRIORITY_PRIMARY);
			}
		}
	}
}

void FireVehiclesRocketCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVehicle)
	{
		CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();

		if(!pVehicle->GetVehicleWeaponMgr())
		{
			pVehicle->CreateVehicleWeaponMgr();
		}

		CVehicleWeaponMgr* pVehWeaponMgr = pVehicle->GetVehicleWeaponMgr();

		if(pVehWeaponMgr)
		{
			s32 seatIndex = 0; //Assume 0 is weapon firers seat

			s32 numWeapons = pVehWeaponMgr->GetNumWeaponsForSeat(seatIndex);
			CVehicleWeapon* pVehicleProjectileWeapon = NULL;

			if( numWeapons > 0 )
			{
				atArray<CVehicleWeapon*> weaponArray;
				pVehicle->GetVehicleWeaponMgr()->GetWeaponsForSeat(seatIndex,weaponArray);

				for(int weaponIndex=0; weaponIndex<numWeapons;weaponIndex++)
				{
					CVehicleWeapon* pVehicleWeapon = weaponArray[weaponIndex];
					const CWeaponInfo* pWeaponInfo = pVehicleWeapon->GetWeaponInfo();

					if(pWeaponInfo && pWeaponInfo->GetIsProjectile())
					{
						pVehicleProjectileWeapon = pVehicleWeapon;
						break;
					}
				}

				if (pVehicleProjectileWeapon)
				{
					pVehicleProjectileWeapon->Fire(pLocalPlayer,pVehicle,VEC3_ZERO,NULL, true); //
				}
			}
		}
	}
}

void SetEngineHealthCB()
{
	CEntity* pEnt = CDebugScene::FocusEntities_Get(0);
	if (pEnt && pEnt->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(pEnt);
		pVehicle->GetVehicleDamage()->SetEngineHealth(CVehicleFactory::ms_fSetEngineNewHealth);
	}
}


void breakOffLeftWingCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVehicle && pVehicle->InheritsFromPlane())
	{
		CPlane* pPlane = static_cast<CPlane*>(pVehicle);
		pPlane->GetAircraftDamage().BreakOffSection(pPlane, CAircraftDamage::WING_L);
	}
}

void breakOffRightWingCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVehicle && pVehicle->InheritsFromPlane())
	{
		CPlane* pPlane = static_cast<CPlane*>(pVehicle);
		pPlane->GetAircraftDamage().BreakOffSection(pPlane, CAircraftDamage::WING_R);
	}
}

void removeLandingGearCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVehicle && pVehicle->InheritsFromPlane())
	{
		CPlane* pPlane = static_cast<CPlane*>(pVehicle);
		pPlane->GetLandingGear().ControlLandingGear(pPlane,CLandingGear::COMMAND_RETRACT_INSTANT);
	}
	else if( pVehicle && pVehicle->InheritsFromHeli() )
	{
		CHeli* pHeli = static_cast<CHeli*>( pVehicle );

		if( pHeli->HasLandingGear() )
		{
			pHeli->GetLandingGear().ControlLandingGear( pHeli, CLandingGear::COMMAND_RETRACT_INSTANT );
		}
	} 
}

void randomlyOpenAndCloseDoorsCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVehicle)
	{
		for(int d=0; d<pVehicle->GetNumDoors(); d++)
		{
			CCarDoor * pDoor = pVehicle->GetDoor(d);
			const float fRatio = fwRandom::GetRandomNumberInRange(0.0f,1.0f);
			pDoor->SetTargetDoorOpenRatio(fRatio, CCarDoor::DRIVEN_NORESET);
		}
	}
}

void setOutOfControlCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVehicle)
	{
		pVehicle->SetIsOutOfControl();
	}
}

void openRoofCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CTaskVehicleConvertibleRoof *convertibleRoof= rage_new CTaskVehicleConvertibleRoof();
		convertibleRoof->SetState(CTaskVehicleConvertibleRoof::State_Lower_Roof);
		CVehicleFactory::ms_pLastCreatedVehicle->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->SetTask(convertibleRoof, VEHICLE_TASK_SECONDARY_ANIM);
	}
}

void closeRoofCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CTaskVehicleConvertibleRoof *convertibleRoof= rage_new CTaskVehicleConvertibleRoof();
		convertibleRoof->SetState(CTaskVehicleConvertibleRoof::State_Raise_Roof);
		CVehicleFactory::ms_pLastCreatedVehicle->GetIntelligence()->GetTaskManager()->GetTree(VEHICLE_TASK_TREE_SECONDARY)->SetTask(convertibleRoof, VEHICLE_TASK_SECONDARY_ANIM);
	}
}

void triggerCarAlarmCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CVehicleFactory::ms_pLastCreatedVehicle->TriggerCarAlarm();
	}
}

void outputDoorStuckDebugCB()
{
	CPed *pPlayerPed = CPedFactory::GetFactory()->GetLocalPlayer();
	if(pPlayerPed)
	{		
		if(pPlayerPed->GetMyVehicle() && pPlayerPed->GetMyVehicle()->InheritsFromAutomobile())
		{
			CAutomobile *pPlayerVeh = static_cast<CAutomobile *>(pPlayerPed->GetMyVehicle());
			for(int nDoor = 0; nDoor < pPlayerVeh->GetNumDoors(); ++nDoor)
			{
				CCarDoor *pDoor = pPlayerVeh->GetDoor(nDoor);
				if(pDoor)
				{					
					pDoor->OutputDoorStuckDebug();					
				}
			}
		}
	}
}

void deployFoldingWingsCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CVehicleFactory::ms_pLastCreatedVehicle->DeployFoldingWings(!CVehicleFactory::ms_pLastCreatedVehicle->GetAreFoldingWingsDeployed(),false);
	}
}

void lowerRoofCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CVehicleFactory::ms_pLastCreatedVehicle->LowerConvertibleRoof(bLowerOrRaiseRoofInstantly);
	}
}

void raiseRoofCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CVehicleFactory::ms_pLastCreatedVehicle->RaiseConvertibleRoof(bLowerOrRaiseRoofInstantly);
	}
}

void openDoorCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVehicle)
	{
		CCarDoor * pDoor = pVehicle->GetDoorFromId(CVehicleModelInfo::GetSetupDoorIds()[CVehicleFactory::ms_iDoorToOpen]);

		if(pDoor && pDoor->GetIsIntact(pVehicle))
		{
			pDoor->SetTargetDoorOpenRatio(sDoorOpenAmount, bDoorCanBeReset ? CCarDoor::DRIVEN_AUTORESET :CCarDoor::DRIVEN_NORESET, pVehicle);
		}
	}
}

void closeDoorCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVehicle)
	{
		CCarDoor * pDoor = pVehicle->GetDoorFromId(CVehicleModelInfo::GetSetupDoorIds()[CVehicleFactory::ms_iDoorToOpen]);

		if(pDoor && pDoor->GetIsIntact(pVehicle))
		{
			pDoor->SetShutAndLatched(pVehicle);
		}
	}
}

void unlatchDoorCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVehicle)
	{
		CCarDoor * pDoor = pVehicle->GetDoorFromId(CVehicleModelInfo::GetSetupDoorIds()[CVehicleFactory::ms_iDoorToOpen]);

		if(pDoor && pDoor->GetIsIntact(pVehicle))
		{
			pDoor->SetSwingingFree(pVehicle);
		}
	}
}

void breakOffDoorCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVehicle && !pVehicle->IsNetworkClone())
	{
		CCarDoor * pDoor = pVehicle->GetDoorFromId(CVehicleModelInfo::GetSetupDoorIds()[CVehicleFactory::ms_iDoorToOpen]);

		if(pDoor && pDoor->GetIsIntact(pVehicle))
		{
			// need to break the latch order for the physics to be activated .. otherwise the door falls through the world
			pDoor->BreakLatch(pVehicle);
			pDoor->BreakOff(pVehicle);
		}
	}
}

void HydraulicSuspensionCB()
{
	CVehicle* pVehicle = CVehicleFactory::ms_pLastCreatedVehicle.Get();

	if(pVehicle && pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR)
	{
		CAutomobile *pAutomobile = static_cast<CAutomobile*>(pVehicle);
		pAutomobile->SetHydraulicsBounds(	CVehicle::ms_fHydraulicVarianceUpperBoundMaxModifier * CVehicle::ms_fHydraulicSuspension,
											-CVehicle::ms_fHydraulicVarianceLowerBoundMaxModifier * CVehicle::ms_fHydraulicSuspension);
		pAutomobile->ActivatePhysics();
	}
}

void resetSuspensionCB()
{
	CVehicle* pVehicle = CVehicleFactory::ms_pLastCreatedVehicle.Get();

	if(pVehicle)
	{
		pVehicle->ResetSuspension();
	}
}

void setGroundClearanceCB()
{
	CVehicle* pVehicle = CVehicleFactory::ms_pLastCreatedVehicle.Get();

	if(pVehicle && pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR)
	{
		CAutomobile *pAutomobile = static_cast<CAutomobile*>(pVehicle);
		pAutomobile->UpdateDesiredGroundGroundClearance(CVehicle::ms_fForcedGroundClearance);
	}
}

void openAllDoorsCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVehicle)
	{
		for(int i=0; i<pVehicle->GetNumDoors(); i++)
		{
			CCarDoor * pDoor = pVehicle->GetDoor(i);

			if(pDoor && pDoor->GetIsIntact(pVehicle))
			{
				pDoor->SetTargetDoorOpenRatio(1.0f, CCarDoor::DRIVEN_NORESET|CCarDoor::DRIVEN_SPECIAL);
			}
		}
	}
}

void closeAllDoorsCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVehicle)
	{
		for(int i=0; i<pVehicle->GetNumDoors(); i++)
		{
			CCarDoor * pDoor = pVehicle->GetDoor(i);

			if(pDoor && pDoor->GetIsIntact(pVehicle))
			{
				pDoor->SetShutAndLatched(pVehicle);
			}
		}
	}
}

void unlatchAllDoorsCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVehicle)
	{
		for(int i=0; i<pVehicle->GetNumDoors(); i++)
		{
			CCarDoor * pDoor = pVehicle->GetDoor(i);

			if(pDoor && pDoor->GetIsIntact(pVehicle))
			{
				pVehicle->GetDoor(i)->SetSwingingFree(pVehicle);
			}
		}
	}
}

void breakOffAllDoorsCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVehicle)
	{
		for(int i=0; i<pVehicle->GetNumDoors(); i++)
		{
			CCarDoor * pDoor = pVehicle->GetDoor(i);

			if(pDoor && pDoor->GetIsIntact(pVehicle))
			{
				// need to break the latch order for the physics to be activated .. otherwise the door falls through the world
				pDoor->BreakLatch(pVehicle);
				pDoor->BreakOff(pVehicle);
			}
		}
	}
}

void openBombBayDoorsCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		for(int i = 0; i < CVehicleFactory::ms_pLastCreatedVehicle->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = CVehicleFactory::ms_pLastCreatedVehicle->GetVehicleGadget(i);

			//do we have a bombbay gadget
			if(pVehicleGadget->GetType() == VGT_BOMBBAY)
			{
				CVehicleGadgetBombBay *pBombBay = static_cast<CVehicleGadgetBombBay*>(pVehicleGadget);

				pBombBay->OpenDoors(CVehicleFactory::ms_pLastCreatedVehicle);
			}
		}
	}
}

void closeBombBayDoorsCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		for(int i = 0; i < CVehicleFactory::ms_pLastCreatedVehicle->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = CVehicleFactory::ms_pLastCreatedVehicle->GetVehicleGadget(i);

			//do we have a bombbay gadget
			if(pVehicleGadget->GetType() == VGT_BOMBBAY)
			{
				CVehicleGadgetBombBay *pBombBay = static_cast<CVehicleGadgetBombBay*>(pVehicleGadget);

				pBombBay->CloseDoors(CVehicleFactory::ms_pLastCreatedVehicle);
			}
		}
	}
}

void swingOpenBooatBoomCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVehicle)
	{
		for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

			//do we have a boat boom gadget
			if(pVehicleGadget->GetType() == VGT_BOATBOOM)
			{
				CVehicleGadgetBoatBoom *pBombBay = static_cast<CVehicleGadgetBoatBoom*>(pVehicleGadget);

				pBombBay->SwingOutBoatBoom(pVehicle);
			}
		}
	}
}

void swingInBooatBoomCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVehicle)
	{
		for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
		{
			CVehicleGadget *pVehicleGadget = pVehicle->GetVehicleGadget(i);

			//do we have a boat boom gadget
			if(pVehicleGadget->GetType() == VGT_BOATBOOM)
			{
				CVehicleGadgetBoatBoom *pBombBay = static_cast<CVehicleGadgetBoatBoom*>(pVehicleGadget);

				pBombBay->SwingInBoatBoom(pVehicle);
			}
		}
	}
}

void repairTyreCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		eHierarchyId nWheelId = CWheel::GetScriptWheelId(eScriptWheelList(CVehicleFactory::ms_iTyreNumber));

		if(CVehicleFactory::ms_pLastCreatedVehicle->GetWheelFromId(nWheelId))
		{
			CVehicleFactory::ms_pLastCreatedVehicle->GetWheelFromId(nWheelId)->ResetDamage();
		}
	}
}

void breakOffTyreCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVehicle && !pVehicle->IsNetworkClone())
	{
		eHierarchyId nWheelId = CWheel::GetScriptWheelId(eScriptWheelList(CVehicleFactory::ms_iTyreNumber));

		if(pVehicle->GetWheelFromId(nWheelId))
		{
			pVehicle->PartHasBrokenOff(pVehicle->GetWheelFromId(nWheelId)->GetHierarchyId());
			fragInst *pFraginst = pVehicle->GetFragInst();
			pFraginst->BreakOffAbove(pVehicle->GetWheelFromId(nWheelId)->GetFragChild());
			pVehicle->SetWheelBroken(pVehicle->GetWheelIndexFromId(nWheelId));
			CVehicle::ClearLastBrokenOffPart();
		}
	}
}

void ToggleBurnoutModeCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CVehicle *pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
		if( pVehicle->InheritsFromAutomobile())
		{
			CAutomobile *pAutomobile = static_cast<CAutomobile*>(pVehicle);
			pAutomobile->EnableBurnoutMode(!pAutomobile->GetBurnoutMode());
		}
	}
}

void ToggleReduceGripModeCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CVehicle *pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
		if( pVehicle->InheritsFromAutomobile())
		{
			CAutomobile *pAutomobile = static_cast<CAutomobile*>(pVehicle);
			pAutomobile->EnableReduceGripMode(!pAutomobile->GetReduceGripMode());
		}

		pVehicle->GetVehicleDamage()->GetIsDriveable();
	}
}

void ToggleDriftTyresCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CVehicle *pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
		pVehicle->m_bDriftTyres = !pVehicle->m_bDriftTyres;
	}
}

void ToggleHoldGearWithWheelspinFlagCB()
{
	CVehicle::ms_bDebugIgnoreHoldGearWithWheelspinFlag = !CVehicle::ms_bDebugIgnoreHoldGearWithWheelspinFlag;
}

void ToggleReducedSuspensionForceCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if (pVehicle)
	{
		bool bSuspensionForceReduced = false;
		for(s32 index = 0; index < pVehicle->GetNumWheels(); index++)
		{
			CWheel *wheel = pVehicle->GetWheel(index);
			gnetAssert(wheel);

			if(wheel && wheel->GetReducedSuspensionForce())
			{
				bSuspensionForceReduced = true;
				break;
			}
		}

		if(pVehicle->InheritsFromAutomobile())
		{
			CAutomobile* pAutomobile = static_cast< CAutomobile* >( pVehicle );
			pAutomobile->SetReducedSuspensionForce(!bSuspensionForceReduced);
		}
	}
}


void toggleGravityCB()
{
	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	if(!pVehicle && CVehicleFactory::ms_pLastCreatedVehicle)
	{
		pVehicle = CVehicleFactory::ms_pLastCreatedVehicle;
	}

	if(pVehicle)
	{
		if(pVehicle->GetGravityForWheellIntegrator() > 0.0f)
		{
			pVehicle->SetGravityForWheellIntegrator(0.0f);
		}
		else
		{
			pVehicle->SetGravityForWheellIntegrator(-GRAVITY);
		}
	}
}

void popOutWindscreenCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CVehicleFactory::ms_pLastCreatedVehicle->GetVehicleDamage()->PopOutWindScreen();
	}
}


void popOffRoofCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CVehicleFactory::ms_pLastCreatedVehicle->GetVehicleDamage()->PopOffRoof(sRoofImpulse);
	}
}

void ReloadHandlingDataCB()
{
	CVehicle::Pool* pool = CVehicle::GetPool();
	for(int i= 0; i< pool->GetSize(); i++)
	{
		CVehicle* pVehicle = pool->GetSlot(i);
		if(pVehicle)
			CVehicleFactory::GetFactory()->Destroy(pVehicle);
	}
	CHandlingDataMgr::ReloadHandlingData();
	CModelInfo::UpdateVehicleHandlingInfos();	
	CModelInfo::UpdateVehicleClassInfos();
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CVehicleModelInfo* pModelInfo = CVehicleFactory::ms_pLastCreatedVehicle->GetVehicleModelInfo();
		pModelInfo->InitPhys();
        CVehicleFactory::ms_pLastCreatedVehicle->m_Transmission.SetupTransmission(  CVehicleFactory::ms_pLastCreatedVehicle->pHandling->m_fInitialDriveForce,
                                                                                    CVehicleFactory::ms_pLastCreatedVehicle->pHandling->m_fInitialDriveMaxFlatVel, 
                                                                                    CVehicleFactory::ms_pLastCreatedVehicle->pHandling->m_fInitialDriveMaxVel, 
                                                                                    CVehicleFactory::ms_pLastCreatedVehicle->pHandling->m_nInitialDriveGears,
																					CVehicleFactory::ms_pLastCreatedVehicle );

		if(CVehicleFactory::ms_pLastCreatedVehicle->GetSecondTransmission())
		{
			CVehicleFactory::ms_pLastCreatedVehicle->GetSecondTransmission()->SetupTransmission(CVehicleFactory::ms_pLastCreatedVehicle->pHandling->m_fInitialDriveForce,
																								CVehicleFactory::ms_pLastCreatedVehicle->pHandling->m_fInitialDriveMaxFlatVel, 
																								CVehicleFactory::ms_pLastCreatedVehicle->pHandling->m_fInitialDriveMaxVel, 
																								1,
																								CVehicleFactory::ms_pLastCreatedVehicle);
		}

/*
		CVehicleFactory::ms_pLastCreatedVehicle->SetupWheels(NULL);
		CVehicleFactory::ms_pLastCreatedVehicle->hFlagsLocal = CVehicleFactory::ms_pLastCreatedVehicle->pHandling->hFlags;

		phArchetypeDamp *pArchetype = (phArchetypeDamp *)CVehicleFactory::ms_pLastCreatedVehicle->GetCurrentPhysicsInst()->GetArchetype();
		pArchetype->ActivateDamping(phArchetypeDamp::LINEAR_V2, Vector3(CVehicleFactory::ms_pLastCreatedVehicle->pHandling->m_fDragCoeff, CVehicleFactory::ms_pLastCreatedVehicle->pHandling->m_fDragCoeff, CVehicleFactory::ms_pLastCreatedVehicle->pHandling->m_fDragCoeff));

		if(CVehicleFactory::ms_pLastCreatedVehicle->GetVehicleType()==VEHICLE_TYPE_HELI || CVehicleFactory::ms_pLastCreatedVehicle->GetVehicleType()==VEHICLE_TYPE_PLANE)
		{
			//if(CVehicleFactory::ms_pLastCreatedVehicle->pHandling->GetFlyingHandlingData()->fMoveRes > 0.0f)
				pArchetype->ActivateDamping(phArchetypeDamp::LINEAR_V, Vector3(CVehicleFactory::ms_pLastCreatedVehicle->pHandling->GetFlyingHandlingData()->fMoveRes, CVehicleFactory::ms_pLastCreatedVehicle->pHandling->GetFlyingHandlingData()->fMoveRes, CVehicleFactory::ms_pLastCreatedVehicle->pHandling->GetFlyingHandlingData()->fMoveRes));

			//if(CVehicleFactory::ms_pLastCreatedVehicle->pHandling->GetFlyingHandlingData()->vecTurnRes.Mag2() > 0.0f)
				pArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V, CVehicleFactory::ms_pLastCreatedVehicle->pHandling->GetFlyingHandlingData()->vecTurnRes);

			//if(CVehicleFactory::ms_pLastCreatedVehicle->pHandling->GetFlyingHandlingData()->vecSpeedRes.Mag2() > 0.0f)
				pArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V2, CVehicleFactory::ms_pLastCreatedVehicle->pHandling->GetFlyingHandlingData()->vecSpeedRes);
		}
*/
	}
/*
	if(FindPlayerVehicle() && FindPlayerVehicle()!=CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CVehicleModelInfo* pModelInfo = (CVehicleModelInfo*)CModelInfo::GetModelInfo (FindPlayerVehicle()->GetModelIndex());
		pModelInfo->InitPhys();

		FindPlayerVehicle()->SetupWheels(NULL);
		FindPlayerVehicle()->hFlagsLocal = FindPlayerVehicle()->pHandling->hFlags;

		phArchetypeDamp *pArchetype = (phArchetypeDamp *)FindPlayerVehicle()->GetCurrentPhysicsInst()->GetArchetype();
		pArchetype->ActivateDamping(phArchetypeDamp::LINEAR_V2, Vector3(FindPlayerVehicle()->pHandling->m_fDragCoeff, FindPlayerVehicle()->pHandling->m_fDragCoeff, FindPlayerVehicle()->pHandling->m_fDragCoeff));

		if(FindPlayerVehicle()->GetVehicleType()==VEHICLE_TYPE_HELI || FindPlayerVehicle()->GetVehicleType()==VEHICLE_TYPE_PLANE)
		{
			//if(FindPlayerVehicle()->pHandling->GetFlyingHandlingData()->fMoveRes > 0.0f)
				pArchetype->ActivateDamping(phArchetypeDamp::LINEAR_V, Vector3(FindPlayerVehicle()->pHandling->GetFlyingHandlingData()->fMoveRes, FindPlayerVehicle()->pHandling->GetFlyingHandlingData()->fMoveRes, FindPlayerVehicle()->pHandling->GetFlyingHandlingData()->fMoveRes));

			//if(FindPlayerVehicle()->pHandling->GetFlyingHandlingData()->vecTurnRes.Mag2() > 0.0f)
				pArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V, FindPlayerVehicle()->pHandling->GetFlyingHandlingData()->vecTurnRes);

			//if(FindPlayerVehicle()->pHandling->GetFlyingHandlingData()->vecSpeedRes.Mag2() > 0.0f)
				pArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V2, FindPlayerVehicle()->pHandling->GetFlyingHandlingData()->vecSpeedRes);
		}
	}
*/
}

void CycleHandlingDebugCB()
{
	CVehicle::ms_nVehicleDebug += 1;
	if(CVehicle::ms_nVehicleDebug >= VEH_DEBUG_END)
		CVehicle::ms_nVehicleDebug = VEH_DEBUG_OFF;
	if((CVehicle::ms_nVehicleDebug==VEH_DEBUG_PERFORMANCE || CVehicle::ms_nVehicleDebug==VEH_DEBUG_HANDLING)
		&& NetworkDebug::GetDebugDisplaySettings().m_displayBasicInfo)
	{
		// Turn off network debug displays, so that the text won't overlap on top of each other
		NetworkDebug::GetDebugDisplaySettings().m_displayBasicInfo = false;
	}
}

void DisplayDownforceDebugCB()
{
    CVehicle::ms_bVehicleDownforceDebug = !CVehicle::ms_bVehicleDownforceDebug;
}

void DisplayQADebugCB()
{
    CVehicle::ms_bVehicleQADebug = !CVehicle::ms_bVehicleQADebug;
}

void RemoveTrailersCB()
{
	s32 count = 0;
	CVehicle::Pool* pool = CVehicle::GetPool();
	s32 i = (s32) pool->GetSize();	
	while(i--)
	{
		CVehicle* pVehicle = pool->GetSlot(i);
		if(pVehicle && pVehicle->GetVehicleType() == VEHICLE_TYPE_TRAILER)
		{
			CVehicleFactory::GetFactory()->Destroy(pVehicle);
			count++;
		}
	}

	Displayf("Removed %d trailers\n", count);
}

void DetachTrailerCB()
{
    CVehicle *pParentVehicle = CVehicleFactory::ms_pLastCreatedVehicle;

    if(pParentVehicle && pParentVehicle->GetAttachedTrailer())
    {
        pParentVehicle->GetAttachedTrailer()->DetachFromParent(0);
    }
}

void ToggleOutriggersCB()
{
	CVehicle *pParentVehicle = CVehicleFactory::ms_pLastCreatedVehicle;

	if( pParentVehicle )
	{
		pParentVehicle->SetDeployOutriggers( !pParentVehicle->GetAreOutriggersBeingDeployed() );
	}
}

void DropCarCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		Vector3 vecPos = VEC3V_TO_VECTOR3(CVehicleFactory::ms_pLastCreatedVehicle->GetTransform().GetPosition());
		vecPos.z += carDropHeight;

		float fSetHeading = -10.0f;
		if(CVehicleFactory::ms_pLastCreatedVehicle->GetTransform().GetC().GetZf() < 0.3f)
			fSetHeading = CVehicleFactory::ms_pLastCreatedVehicle->GetTransform().GetHeading();

		CVehicleFactory::ms_pLastCreatedVehicle->Teleport(vecPos, fSetHeading, false);
	}
}

void TestCarAgainstWorldCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		Matrix34 matTest = MAT34V_TO_MATRIX34(CVehicleFactory::ms_pLastCreatedVehicle->GetMatrix());
		matTest.d.z += carTestHeight;
		const CEntity** ppExcludeInst = (const CEntity**)&CVehicleFactory::ms_pLastCreatedVehicle;
		if(CVehicle::TestVehicleBoundForCollision(
			&matTest, CVehicleFactory::ms_pLastCreatedVehicle, fwModelId::MI_INVALID, ppExcludeInst, 1, NULL, ArchetypeFlags::GTA_ALL_MAP_TYPES))
		{
			Displayf("\nCar Test Hit");
		}
	}
}

void LockLastCar()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CVehicleFactory::ms_pLastCreatedVehicle->SetCarDoorLocks(CARLOCK_LOCKED_BUT_CAN_BE_DAMAGED);
	}
}

void ToggleDummyPhysicsCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle && CVehicleFactory::ms_pLastCreatedVehicle->GetVehicleType()==VEHICLE_TYPE_CAR)
	{
		CAutomobile * pAutomobile = ((CAutomobile*)CVehicleFactory::ms_pLastCreatedVehicle.Get());

		if(CVehicleFactory::ms_pLastCreatedVehicle->GetCurrentPhysicsInst()==((CAutomobile*)CVehicleFactory::ms_pLastCreatedVehicle.Get())->GetVehicleFragInst())
		{
			pAutomobile->TryToMakeIntoDummy(VDM_DUMMY);
		}
		else
		{
			pAutomobile->TryToMakeFromDummy();
		}
	}
}

void FreezeVehicleCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle && !CVehicleFactory::ms_pLastCreatedVehicle->GetIsAnyFixedFlagSet())
	{
		CVehicleFactory::ms_pLastCreatedVehicle->SetFixedPhysics(true);
	}
}

void UnfreezeVehicleCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle && CVehicleFactory::ms_pLastCreatedVehicle->GetIsAnyFixedFlagSet())
	{
		CVehicleFactory::ms_pLastCreatedVehicle->SetFixedPhysics(false);
	}
}

void ToggleAnchorBoatCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		if(CVehicleFactory::ms_pLastCreatedVehicle->GetVehicleType()==VEHICLE_TYPE_BOAT)
		{
			CBoat* pBoat = (CBoat*)CVehicleFactory::ms_pLastCreatedVehicle.Get();
			pBoat->GetAnchorHelper().Anchor(!pBoat->GetAnchorHelper().IsAnchored());
		}
		else if(CVehicleFactory::ms_pLastCreatedVehicle->GetVehicleType()==VEHICLE_TYPE_SUBMARINE)
		{
			CSubmarine* pSub = static_cast<CSubmarine*>(CVehicleFactory::ms_pLastCreatedVehicle.Get());
			CSubmarineHandling* pSubHandling = pSub->GetSubHandling();

			pSubHandling->GetAnchorHelper().Anchor(!pSubHandling->GetAnchorHelper().IsAnchored());
		}
		else if(CVehicleFactory::ms_pLastCreatedVehicle->GetVehicleType()==VEHICLE_TYPE_PLANE)
		{
			CSeaPlaneExtension* pSeaPlaneExtension = static_cast<CPlane*>(CVehicleFactory::ms_pLastCreatedVehicle.Get())->GetExtension<CSeaPlaneExtension>();
			if(pSeaPlaneExtension)
			{
				CAnchorHelper& anchorHelper = pSeaPlaneExtension->GetAnchorHelper();
				anchorHelper.Anchor(!anchorHelper.IsAnchored());
			}
		}
		else if(CVehicleFactory::ms_pLastCreatedVehicle->GetVehicleType()==VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE ||
				CVehicleFactory::ms_pLastCreatedVehicle->GetVehicleType()==VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE )
		{
			CAmphibiousAutomobile* pAmphiVehicle = (CAmphibiousAutomobile*)CVehicleFactory::ms_pLastCreatedVehicle.Get();
			pAmphiVehicle->GetAnchorHelper().Anchor(!pAmphiVehicle->GetAnchorHelper().IsAnchored());
		}
	}
}

void SoundCarHornCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle && CVehicleFactory::ms_pLastCreatedVehicle->GetIsTypeVehicle())
	{
		vehicle_commands::CommandSoundCarHorn(CTheScripts::GetGUIDFromEntity(*(CVehicleFactory::ms_pLastCreatedVehicle.Get())), carSoundHornTime * 1000);
	}
}

void ApplyCheatsToLastCarCB()
{
	if(CVehicleFactory::ms_pLastCreatedVehicle)
	{
		u32 nCheats = 0;
		if(sbCheatAbs)				nCheats |= VEH_SET_ABS;
		if(sbCheatTractionControl)	nCheats |= VEH_SET_TC;
		if(sbCheatStabilityControl)	nCheats |= VEH_SET_SC;
		if(sbCheatGrip1)			nCheats |= VEH_SET_GRIP1;
		if(sbCheatGrip2)			nCheats |= VEH_SET_GRIP2;
		if(sbCheatPower1)			nCheats |= VEH_SET_POWER1;
		if(sbCheatPower2)			nCheats |= VEH_SET_POWER2;

		CVehicleFactory::ms_pLastCreatedVehicle->SetCheatFlags(nCheats);
	}
}

void ApplyCheatsToSelectedCarCB()
{
	CVehicle *pVehicle = CVehicleDebug::GetFocusVehicle();
	if(pVehicle)
	{
		u32 nCheats = 0;
		if(sbCheatAbs)				nCheats |= VEH_SET_ABS;
		if(sbCheatTractionControl)	nCheats |= VEH_SET_TC;
		if(sbCheatStabilityControl)	nCheats |= VEH_SET_SC;
		if(sbCheatGrip1)			nCheats |= VEH_SET_GRIP1;
		if(sbCheatGrip2)			nCheats |= VEH_SET_GRIP2;
		if(sbCheatPower1)			nCheats |= VEH_SET_POWER1;
		if(sbCheatPower2)			nCheats |= VEH_SET_POWER2;

		pVehicle->SetCheatFlags(nCheats);
	}
}

void ClearZeroSixtySixtyZeroGForceCarCB()
{
	CAutomobile::ms_fBestSixtyToZeroTime = 0.0f;
	CAutomobile::ms_fBestZeroToSixtyTime = 0.0f;
	CAutomobile::ms_fHighestGForce = 0.0f;
}

void ApplyTopSpeedToLastCarCB()
{
	CVehicleFactory::ModifyVehicleTopSpeed(CVehicleFactory::ms_pLastCreatedVehicle, sfTopSpeedAdjustment);
}

void ForceHdCB()
{
	if (CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CVehicleFactory::ms_pLastCreatedVehicle->SetForceHd(CVehicleFactory::ms_bForceHd);
	}
}

void LodMultCB()
{
	if (CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CVehicleFactory::ms_pLastCreatedVehicle->SetLodMultiplier(CVehicleFactory::ms_fLodMult);
	}
}

void ClampRenderLodCB()
{
	if (CVehicleFactory::ms_pLastCreatedVehicle)
	{
		CVehicleFactory::ms_pLastCreatedVehicle->SetClampedRenderLod(CVehicleFactory::ms_clampedRenderLod);
	}
}

static atArray<CVehicleModelInfo*> g_typeArray;

////////////////////////////////////////////////////////////////////////////
static int CbCompareModelInfos(const void* pVoidA, const void* pVoidB)
{
	const s32* pA = (const s32*)pVoidA;
	const s32* pB = (const s32*)pVoidB;

	return stricmp(g_typeArray[*pA]->GetModelName(), g_typeArray[*pB]->GetModelName());
}

////////////////////////////////////////////////////////////////////////////
// name:	UpdateMloList
// purpose: 
// params:  none
// returns: nothing
////////////////////////////////////////////////////////////////////////////
void CVehicleFactory::UpdateVehicleList()
{
	u32 i=0;

	emptyNames.Reset();

	//get the modelInfo associated with this ped ID
	fwArchetypeDynamicFactory<CVehicleModelInfo>& vehModelInfoStore = CModelInfo::GetVehicleModelInfoStore();
	vehModelInfoStore.GatherPtrs(g_typeArray);

	numVehicleNames = g_typeArray.GetCount();

	delete []vehicleSorts;
	vehicleSorts = rage_new s32[numVehicleNames];

	int actualNum = 0;
	for(i=0;i<numVehicleNames;i++)
	{
		if((currVehicleTypeSelection == BANK_VEHICLE_TYPE_ALL || g_typeArray[i]->GetVehicleType() == currVehicleTypeSelection) && 
		   (currVehicleLayoutSelection == CVehicleMetadataMgr::GetVehicleLayoutInfoCount() || (pLayoutCombo && !strcmp(g_typeArray[i]->GetVehicleLayoutInfo()->GetName().GetCStr(), pLayoutCombo->GetString(currVehicleLayoutSelection)))) &&
		   (currVehicleDLCSelection == numDLCNames || (pDLCCombo && !strcmp(g_typeArray[i]->GetVehicleDLCPack().GetCStr(),pDLCCombo->GetString(currVehicleDLCSelection)))) &&
		   (currVehicleSwankSelection == BANK_VEHICLE_SWANK_ALL || g_typeArray[i]->GetVehicleSwankness() == currVehicleSwankSelection) &&
			( currVehicleClassSelection == BANK_VEHICLE_CLASS_ALL || g_typeArray[ i ]->GetVehicleClass() == currVehicleClassSelection ) )
		{
			vehicleSorts[actualNum] = i;
			++actualNum;
		}
	}

	numVehicleNames = actualNum;

	numVehicleNames = actualNum;

	// when we restart the game currVehicleNameSelection will possibly be higher than the total number of cars since it had all the DLC/patches vehicles
	// to fix this we simply clamp the value
	currVehicleNameSelection = (currVehicleNameSelection>=numVehicleNames && currVehicleNameSelection!=0) ? numVehicleNames-1 : currVehicleNameSelection;

	if (currVehicleNameSelection < 0)
	{
		currVehicleNameSelection = 0;
	}

	qsort(vehicleSorts,numVehicleNames,sizeof(s32),CbCompareModelInfos);
	vehicleNames.Reset();

	for(i=0;i<numVehicleNames;i++)
	{		
		vehicleNames.PushAndGrow(g_typeArray[vehicleSorts[i]]->GetModelName());
		emptyNames.PushAndGrow("");
	}

	numVehicleTrailerNames = 0;

	delete []vehicleTrailerSorts;
	vehicleTrailerSorts = rage_new s32[numVehicleNames];	// A bit wasteful, could do a counting pass first and then a population pass to get around that.

	//now create the list of trailers.
	//loop through vehicles and find vehicles which are trailers.
	for(i=0;i<numVehicleNames;i++)
	{
		if (g_typeArray[vehicleSorts[i]]->GetVehicleType() == VEHICLE_TYPE_TRAILER)
		{
			vehicleTrailerSorts[numVehicleTrailerNames] = vehicleSorts[i];
			numVehicleTrailerNames++;
		}
	}

	if(!pVehicleCombo)//make sure we have acombo box to add to.
		return;

	if(numVehicleNames == 0)
	{
		emptyNames.PushAndGrow("");
		pVehicleCombo->UpdateCombo("Vehicle name", &currVehicleNameSelection, 1, &emptyNames[0]);
		pVehicleCombo->SetString(0, "No Vehicle");
	}
	else
	{
		// this breaks!
		//pVehicleCombo->UpdateCombo("Vehicle name", &currVehicleNameSelection, numVehicleNames, &vehicleNames[0], CarNumChangeCB);

		// this works!
		pVehicleCombo->UpdateCombo("Vehicle name", &currVehicleNameSelection, numVehicleNames, &emptyNames[0], CarNumChangeCB);
	
		for(s32 i=0;i<vehicleNames.GetCount();i++)
		{
			// Model names are a mix of lower/upper/capital, converting all to lower makes things easier to read
			atString vehicleName(vehicleNames[i]);
			vehicleName.Lowercase();
			pVehicleCombo->SetString(i, vehicleName.c_str());
		}
	}

	if(numVehicleTrailerNames == 0)//no trailers found
	{
		emptyNames.PushAndGrow("");
		pVehicleTrailerCombo->UpdateCombo("Trailer name", &currVehicleTrailerNameSelection, 1, &emptyNames[0]);
		pVehicleTrailerCombo->SetString(0, "No trailer");
	}
	else
	{
		// As we are adding 1 to num vehicles below, we need to make sure emptyNames is big enough for this (only if we have to).
		// This stops a crash, url:bugstar:1129262.
		if((numVehicleTrailerNames+1) > emptyNames.GetCount())
		{
			emptyNames.PushAndGrow("");
		}

		//add 1 to num vehicles so we can have a no trailer selection
		pVehicleTrailerCombo->UpdateCombo("Trailer name", &currVehicleTrailerNameSelection, numVehicleTrailerNames+1, &emptyNames[0], TrailerNumChangeCB);

		pVehicleTrailerCombo->SetString(0, "No trailer");

		//fill in the trailer names
		for(s32 i=0;i<numVehicleTrailerNames;i++)
		{
			//now loop through the vehiclesorts created by the cars combo box to find the names of the trailers, I know its not great but it's better then storing them twice imho.
			for(int k = 0; k < numVehicleNames; k++)
			{
				if(vehicleSorts[k] == vehicleTrailerSorts[i])
				{
					pVehicleTrailerCombo->SetString(i+1, vehicleNames[k]);//+1 off i as we have a "no trailer" selection that takes up slot 0
					k = numVehicleNames;//break out of this loop
				}
			}
		}
	}

	currVehicleNameSelection = 0;
	currVehicleTrailerNameSelection = 0;

	u32 defaultHash = MI_CAR_TAILGATER.GetName().GetHash();
	if(PARAM_defaultVehicle.Get())
	{
		const char * defaultName = NULL;
		PARAM_defaultVehicle.Get(defaultName);
		
		atHashString name(defaultName);
		defaultHash = name.GetHash();
	}


	for(u32 i = 0; i < numVehicleNames; i++)
	{
		// HARD CODE THE TAILGATER AS THE DEFAULT SPAWN VEHICLE AND IF WE CAN'T FIND THAT, CARRY ON AS BEFORE.
		if (g_typeArray[vehicleSorts[i]]->GetModelNameHash() == defaultHash)
		{
			currVehicleNameSelection = i;
			break;
		}
	}
	
	u32 defaultTrailerHash = 0;
	if(PARAM_defaultVehicleTrailer.Get())
	{
		const char * defaultTrailerName = NULL;
		PARAM_defaultVehicleTrailer.Get(defaultTrailerName);

		atHashString trailerName(defaultTrailerName);
		defaultTrailerHash = trailerName.GetHash();
	}
	
	for(u32 i = 0; i < numVehicleTrailerNames; i++)
	{
		if (g_typeArray[vehicleTrailerSorts[i]]->GetModelNameHash() == defaultTrailerHash)
		{
			//0 is reserved for no trailer, consumers of this variable always subtract 1 for this reason.
			currVehicleTrailerNameSelection = i + 1;
			break;
		}
	}

	g_typeArray.Reset();
}

void ValidateAllVehiclesCB(void)
{
#if __DEV
	Displayf("Validation started!\n");

	char pLine[255];
	FileHandle fid;
	bool fileOpen = false;

	// Try and open a file for the results
	CFileMgr::SetDir("common:/DATA");
	fid = CFileMgr::OpenFileForWriting("VehicleValidation.csv");
	CFileMgr::SetDir("");
	if ( CFileMgr::IsValidFileHandle(fid) )
	{
		fileOpen = true;
		sprintf(&pLine[0], "model name, num LODs\n");
		ASSERT_ONLY(s32 ReturnVal =) CFileMgr::Write(fid, &pLine[0], istrlen(pLine));
		Assert(ReturnVal > 0);
	}
	else
	{
		Displayf("\\common\\data\\VehicleValidation.csv is read only");
		return;
	}

	u32 nVehicles = 0;

	// start at current ped selection
	for(u32 i = CVehicleFactory::currVehicleNameSelection; i<numCarsAvailable; i++)
	{
		// get the modelInfo associated with this ped ID
		fwArchetypeDynamicFactory<CVehicleModelInfo>& vehicleModelInfoStore = CModelInfo::GetVehicleModelInfoStore();
		atArray<CVehicleModelInfo*> vehicleTypesArray;
		vehicleModelInfoStore.GatherPtrs(vehicleTypesArray);

		CVehicleModelInfo& vehicleModelInfo = *vehicleTypesArray[ vehicleSorts[i] ];
		u32 vehicleInfoIndex = CModelInfo::GetModelIdFromName(vehicleModelInfo.GetModelName()).GetModelIndex();

		// get the name of the model used by this model info and display it
		Displayf("Validating vehicle. Model name : %s\n", vehicleModelInfo.GetModelName());

		// ensure that the model is loaded
		fwModelId vehicleInfoId((strLocalIndex(vehicleInfoIndex)));
		if (!CModelInfo::HaveAssetsLoaded(vehicleInfoId))
		{
			CModelInfo::RequestAssets(vehicleInfoId, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			CStreaming::LoadAllRequestedObjects(true);
		}

		if (fileOpen)
		{
			sprintf(&pLine[0], "%s, %d\n",
						vehicleModelInfo.GetModelName(),
						vehicleModelInfo.GetNumAvailableLODs());

			ASSERT_ONLY(s32 ReturnVal =) CFileMgr::Write(fid, &pLine[0], istrlen(pLine));
			Assert(ReturnVal > 0);

			nVehicles++;
		}

		// remove streamed in modelinfo if we can (ie. ref count is 0 and don't delete flags not set)
		if (vehicleModelInfo.GetNumRefs() == 0)
		{
			fwModelId vehicleInfoId((strLocalIndex(vehicleInfoIndex)));
			if  (!(CModelInfo::GetAssetStreamFlags(vehicleInfoId) & STR_DONTDELETE_MASK))
			{
				CModelInfo::RemoveAssets(vehicleInfoId);
			}
		}

	}

	Displayf("Summary\n");
	Displayf("Total number of vehicles %d\n",							nVehicles);

	if (fileOpen)
	{
		Displayf("Saved \\common\\data\\VehicleValidation.csv");
		CFileMgr::CloseFile(fid);
	}

	Displayf("Validation complete!\n");
#endif //__DEV
}

void CVehicleFactory::UpdateVehicleTypeList()
{
	currVehicleNameSelection = 0;
	currVehicleTrailerNameSelection = 0;

	CVehicleFactory::UpdateVehicleList();

	if(numVehicleNames)
	{
		carModelId = GetModelIndex(vehicleSorts[currVehicleNameSelection]);
	}
}

extern void populateCarWithCrowd0CB(void);
extern void populateCarWithCrowd1CB(void);
//
// Widget to create a button for creating the vehicles bank
//
bool CVehicleFactory::InitWidgets(void)
{
	pBank = &BANKMGR.CreateBank("Vehicles");

	if(vehicleVerifyf(pBank, "Failed to create Vehicles bank"))
	{
		pCreateBankButton = pBank->AddButton("Create vehicle widgets", &CVehicleFactory::CreateBank);
	}

	return true;
}


extern BankFloat bfCarDoorBreakThreshold; // from door.cpp
extern BankBool bbApplyCarDoorDeformation; // from door.cpp
extern BankFloat bfDeformationThresholdMPMult; // from vehicleDamage.cpp
BANK_ONLY(extern bool sbDisableMidairWheelIntegrator;)
//
// Widget to create test vehicles
//
void CVehicleFactory::CreateBank( void )
{
	Assertf( pBank, "Vehicles bank needs to be created first" );

	if( pCreateBankButton )//delete the create bank button
	{
		pCreateBankButton->Destroy();
		pCreateBankButton = NULL;
	}
	else
	{
		//bank must already be setup as the create button doesn't exist so just return.
		return;
	}

	pBank->AddToggle( "Toggle Allow Vehicle Pop to Add and Remove", &gbAllowVehGenerationOrRemoval );

	pBank->PushGroup( "Create, Populate, and Warp", false );
	fwArchetypeDynamicFactory<CVehicleModelInfo>& vehModelInfoStore = CModelInfo::GetVehicleModelInfoStore();
	atArray<CVehicleModelInfo*> vehicleTypePtrs;
	vehModelInfoStore.GatherPtrs( vehicleTypePtrs );
	numCarsAvailable = vehicleTypePtrs.GetCount();

	pBank->AddSlider( "No. Vehicles Available", &numCarsAvailable, 1, 256, 0 );

	vehicleNames.Reset();
	vehicleNames.PushAndGrow( "Inactive" );
	vehicleNames.PushAndGrow( "Activate" );
	pBank->AddCombo( "Type Filter", &currVehicleTypeSelection, BANK_VEHICLE_TYPES, &ms_modelTypes[ 0 ], UpdateVehicleTypeList );
	AddLayoutFilterWidget();
	AddDLCFilterWidget();
	pBank->AddCombo( "Swank Filter", &currVehicleSwankSelection, BANK_VEHICLE_SWANK, &ms_swankTypes[ 0 ], UpdateVehicleTypeList );
	pBank->AddCombo( "Class Filter", &currVehicleClassSelection, BANK_VEHICLE_CLASS, &ms_classTypes[ 0 ], UpdateVehicleTypeList );
	pVehicleCombo = pBank->AddCombo( "Vehicle name", &currVehicleNameSelection, 2, &vehicleNames[ 0 ], UpdateVehicleList );
	pVehicleTrailerCombo = pBank->AddCombo( "Trailer name", &currVehicleTrailerNameSelection, 2, &vehicleNames[ 0 ], UpdateVehicleList );
	pBank->AddSlider( "Vehicle Id", &carModelId, 0, 1000, 0 );

	pBank->AddButton( "Create Vehicle", CreateCarCB );
	pBank->AddButton( "Create Next Vehicle", CreateNextCarCB );
	pBank->AddButton( "Create Next Bike", CreateNextBikeCB );
	pBank->AddButton( "Create Chosen Vehicle", CreateChosenCarCB );
	pBank->AddButton( "Delete Vehicle", DeleteCarCB );
	pBank->AddSeparator();
	pBank->AddButton( "Disable Vehicle Collision", DisableVehicleCollisionCB );
	pBank->AddButton( "Disable Vehicle Collision Completely", DisableVehicleCollisionCompletelyCB );
	pBank->AddButton( "Enable Vehicle Collision", EnableVehicleCollisionCB );
	pBank->AddButton( "Explode Vehicle", ExplodeCarCB );
	pBank->AddButton( "Reset Car Suspension", ResetCarSuspensionCB );
	pBank->AddButton( "Warp plyr into last Vehicle", WarpPlayerIntoRecentCarCB );
	pBank->AddButton( "Warp plyr out of Vehicle", WarpPlayerOutOfCarCB );
	pBank->AddButton( "Drop Vehicle At Camera Position", DropFocusedVehicleAtCameraPosCB, "Drops the focus vehicle at the camera position" );

	pBank->AddButton( "Populate w'Crowd 1", populateCarWithCrowd0CB );
	pBank->AddButton( "Populate w'Crowd 2", populateCarWithCrowd1CB );

	pBank->AddButton( "Reload Handling", ReloadHandlingDataCB );
	pBank->AddButton( "Cycle Handling Info", CycleHandlingDebugCB );
    pBank->AddButton( "Display Downforce", DisplayDownforceDebugCB );
    pBank->AddButton( "Display New QA Debug", DisplayQADebugCB );
    
	pBank->AddButton( "Remove trailers from world", RemoveTrailersCB );
	pBank->AddButton( "Detach trailer from vehicle", DetachTrailerCB );
	pBank->AddButton( "Toggle outrigger legs", ToggleOutriggersCB );

	pBank->AddToggle( "Allow trailers to spawn", &ms_bSpawnTrailers );
	pBank->AddToggle( "Spawn cars on trailer", &sSpawnCarsOnTrailer );
    pBank->AddToggle( "Spawn cars with no breaking", &sSpawnCarsWithNoBreaking );
    
	pBank->AddButton( "Reload vehicle lod data", ReloadVehicleMetaDataCB );
	pBank->PopGroup();

	pBank->PushGroup( "Handling debug" );
	pBank->AddSlider( "Debug draw scale", &CVehicle::ms_fVehicleDebugScale, 0.0f, 5.0f, 1.0f );
	pBank->AddToggle( "Draw Rev Ratio", &CVehicle::ms_bDebugDrawRevRatio );
	pBank->AddToggle( "Draw Speed Ratio", &CVehicle::ms_bDebugDrawSpeedRatio );
	pBank->AddToggle( "Draw Drive Force", &CVehicle::ms_bDebugDrawDriveForce );
	pBank->AddToggle( "Draw Clutch Ratio", &CVehicle::ms_bDebugDrawClutchRatio );
	pBank->AddToggle( "Draw Wheel Speed", &CVehicle::ms_bDebugDrawWheelSpeed );
	pBank->AddToggle( "Draw Manifold Pressure", &CVehicle::ms_bDebugDrawManifoldPressure );
	pBank->AddToggle( "Draw Large Text", &CVehicle::ms_bDebugDrawLargeText );
	
	pBank->PopGroup();
#if STENCIL_VEHICLE_INTERIOR
	pBank->PushGroup( "Interior Stenciling", false );
	pBank->AddToggle( "Enable", &CVehicle::ms_StencilOutInterior );
	pBank->PopGroup();
#endif

	pBank->PushGroup( "Creation Options", false );
	pBank->AddToggle( "Created Vehicle Needs To Be Hotwired", &bVehicleNeedsToBeHotwired );
	pBank->AddToggle( "Instant-Start Vehicle Engine on Warp", &bVehicleInstantStartEngineOnWarp );
	pBank->AddToggle( "Create as mission Vehicle", &bCreateAsMissionCar );
	pBank->AddToggle( "Create as personal Vehicle", &bCreateAsPersonalCar );
	pBank->AddToggle( "Create as gang Vehicle", &bCreateAsGangCar );
	pBank->AddToggle( "Place on road properly", &bPlaceOnRoadProperly );
	pBank->AddToggle( "Lock doors", &bLockDoors );
	pBank->AddSlider( "Force extra on", &nForceExtraOn, 0, 14, 1 );
	pBank->AddButton( "Next Colour", CycleNextColourCB );
	pBank->AddButton( "Choose color on selected vehicle", ChooseColorCB );
	pBank->PushGroup( "Vehicle Proofs", false );
	pBank->AddToggle( "Fire proof", &bFireProof );
	pBank->AddToggle( "Explosion proof", &bExplosionProof );
	pBank->AddToggle( "Collision proof", &bCollisionProof );
	pBank->AddToggle( "Bullet proof", &bBulletProof );
	pBank->AddToggle( "Melee proof", &bMeleeProof );
	pBank->PopGroup();
	pBank->PopGroup();

	pBank->PushGroup( "Enter tasks", false );
	pBank->AddSlider( "Seat", &ms_iSeatToEnter, 0, MAX_VEHICLE_SEATS, 1 );
	pBank->AddButton( "Player enter car in seat", GetPlayerToEnterSeat );
	pBank->AddButton( "Focus ped enter car in seat", GetFocusPedToEnterSeat );
	pBank->PopGroup();

	pBank->PushGroup( "Wheel impacts and shapetest" );
	pBank->AddToggle( "Async wheel shapetests", &CAutomobile::ms_bUseAsyncWheelProbes );
	pBank->AddToggle( "Draw wheel shapetest results", &CAutomobile::ms_bDrawWheelProbeResults );
	pBank->AddToggle( "ms_bActivateWheelImpacts", &CWheel::ms_bActivateWheelImpacts );
	pBank->AddToggle( "Don't compress on recently broken fragments", &CWheel::ms_bDontCompressOnRecentlyBrokenFragments );
	pBank->AddToggle( "Precompute Extra Wheel Bound Impacts", &CVehicle::ms_bPrecomputeExtraWheelBoundImpacts );
	pBank->PopGroup();


	pBank->PushGroup( "Debug Options", false );
	pBank->AddToggle( "Display Vehicle Names", &ms_bDisplayVehicleNames );
	pBank->AddToggle( "Display Vehicle Addresses", &CVehicleIntelligence::m_bDisplayCarAddresses );
	pBank->AddToggle( "Display Vehicle Layout", &ms_bDisplayVehicleLayout );
	pBank->AddButton( "Dump Vehicle Layouts", DumpVehicleLayoutsCB );
	pBank->AddToggle( "Display creation location", &ms_bDisplayCreateLocation );
	pBank->AddToggle( "Vehicle status debug", &CVehicle::ms_bDebugVehicleStatus );
	pBank->AddToggle( "Vehicle is driveable failure debug", &CVehicle::ms_bDebugVehicleIsDriveableFail );
	pBank->AddToggle( "Vehicles consume petrol", CVehicle::GetConsumePetrolPtr() );
	pBank->AddToggle( "Visualise petrol level", &CVehicle::m_bDebugVisPetrolLevel );
    pBank->AddToggle( "Display Tyre Wear", &CVehicleFactory::ms_bDisplayTyreWearRate );
	pBank->PushGroup( "Wheels debug info", false);
	pBank->AddToggle( "Display steer angle", &CVehicleFactory::ms_bDisplaySteerAngle);
	pBank->PopGroup();
	pBank->PushGroup( "Fire Vehicle", false );
	pBank->AddButton( "Fire Vehicle", FireCarCB );
	pBank->AddButton( "Fire Vehicle from Debug Cam", FireCarFromDebugCamCB );
	pBank->AddButton( "Fire Vehicle with Player", FireCarWithPlayerCB );
	pBank->AddButton( "Fire Vehicle with Player from Debug Cam", FireCarWithPlayerFromDebugCamCB );
	pBank->AddButton( "Fire Vehicle and Object", FireCarWithObjectCB );
	pBank->AddButton( "Fire Vehicle and Object from Debug Cam", FireCarWithObjectFromDebugCamCB );
	pBank->AddButton( "Fire Player Ragdoll", FirePlayerRagdollCB );
	pBank->AddButton( "Fire Player Ragdoll from Debug Cam", FirePlayerRagdollFromDebugCamCB );
	pBank->AddButton( "Fire Object", FireObjectCB );
	pBank->AddButton( "Fire Object from Debug Cam", FireObjectFromDebugCamCB );
	pBank->AddButton( "Set launch direction to vehicle heading", SetLaunchDirectionMatchOrientationCB );
	pBank->AddButton( "SAVE Launch Settings", SaveSettingsCB );
	pBank->AddButton( "LOAD Launch Settings", LoadSettingsCB );
	pBank->AddSlider( "Vel", &sFireCarVel, 0.0f, 100.0f, 1.0f );
	pBank->AddSlider( "Pos X", &sFireCarPos.x, -8000.0f, 8000.0f, 0.1f );
	pBank->AddSlider( "Pos Y", &sFireCarPos.y, -8000.0f, 8000.0f, 0.1f );
	pBank->AddSlider( "Pos Z", &sFireCarPos.z, -1000.0f, 5000.0f, 0.1f );
	pBank->AddSlider( "Dirn X", &sFireCarDirn.x, -1.0f, 1.0f, 0.01f );
	pBank->AddSlider( "Dirn Y", &sFireCarDirn.y, -1.0f, 1.0f, 0.01f );
	pBank->AddSlider( "Dirn Z", &sFireCarDirn.z, -1.0f, 1.0f, 0.01f );
	pBank->AddSlider( "Orient X", &sFireCarOrient.x, -PI, PI, 0.05f, FireCarOrientCB );
	pBank->AddSlider( "Orient Y", &sFireCarOrient.y, -PI, PI, 0.05f, FireCarOrientCB );
	pBank->AddSlider( "Orient Z", &sFireCarOrient.z, -PI, PI, 0.05f, FireCarOrientCB );
	pBank->AddSlider( "Object offset X", &sFireObjectOffset.x, -3000.0f, 3000.0f, 0.1f );
	pBank->AddSlider( "Object offset Y", &sFireObjectOffset.y, -3000.0f, 3000.0f, 0.1f );
	pBank->AddSlider( "Object offset Z", &sFireObjectOffset.z, -3000.0f, 3000.0f, 0.1f );
	pBank->PushGroup( "Turn speed" );
	Vector3 vLimit( 100.0f, 100.0f, 100.0f );
	pBank->AddSlider( "Turn speed", &svFireCarSpin, -vLimit, vLimit, vLimit / 1000.0f );
	pBank->PopGroup();
	pBank->AddToggle( "Force Pos", &ms_bFireCarForcePos );
	pBank->AddToggle( "Regenerate vehicle", &ms_bRegenerateFireVehicle );
	pBank->PopGroup();

	pBank->PushGroup( "Weapon", false );
	pBank->AddButton( "Fire Rocket If Available", FireVehiclesRocketCB );
	pBank->PopGroup();

	pBank->PushGroup( "Health", false );
	pBank->AddButton( "Set Engine Health", SetEngineHealthCB );
	pBank->AddSlider( "New Engine Health", &ms_fSetEngineNewHealth, ENGINE_DAMAGE_FINSHED, ENGINE_HEALTH_MAX, 100.0f );
	pBank->PopGroup();

	pBank->PushGroup( "Planes", false );
	pBank->AddButton( "Break Off Left Wing", breakOffLeftWingCB );
	pBank->AddButton( "Break Off Right Wing", breakOffRightWingCB );
	pBank->AddButton( "Remove Landing Gears", removeLandingGearCB );
	pBank->PopGroup();

	pBank->PushGroup( "Doors", false );
	pBank->AddButton( "Randomly open/close doors", randomlyOpenAndCloseDoorsCB );
	pBank->AddButton( "Open all doors fully", openAllDoorsCB );
	pBank->AddButton( "Close all doors", closeAllDoorsCB );
	pBank->AddButton( "Unlatch all doors", unlatchAllDoorsCB );
	pBank->AddButton( "Break off all doors", breakOffAllDoorsCB );
	pBank->AddToggle( "Apply vehicle deformation to door hinges", &bbApplyCarDoorDeformation );
	pBank->AddSlider( "Door break off threshold (meters)", &bfCarDoorBreakThreshold, 0.0f, 0.3f, 0.01f );
	const char* doorNames[] = // same order as CVehicleModelInfo::ms_aSetupDoorIds
	{
		"VEH_DOOR_DSIDE_F",
		"VEH_DOOR_DSIDE_R",
		"VEH_DOOR_PSIDE_F",
		"VEH_DOOR_PSIDE_R",
		"VEH_BONNET",
		"VEH_BOOT",
		"VEH_BOOT_2",
		"VEH_FOLDING_WING_L",
		"VEH_FOLDING_WING_R"
	};
	CompileTimeAssert( NELEM( doorNames ) == NUM_VEH_DOORS_MAX );
	pBank->AddCombo( "Door", &ms_iDoorToOpen, NELEM( doorNames ), doorNames );
	pBank->AddSlider( "Open door ratio", &sDoorOpenAmount, 0.0f, 1.0f, 0.05f );
	pBank->AddButton( "Open door", openDoorCB );
	pBank->AddButton( "Close door", closeDoorCB );
	pBank->AddButton( "Unlatch door", unlatchDoorCB );
	pBank->AddButton( "Break off door", breakOffDoorCB );
	pBank->AddToggle( "Door Can Be Reset", &bDoorCanBeReset );

	pBank->AddButton( "Open bomb bay door", openBombBayDoorsCB );
	pBank->AddButton( "Close bomb bay door", closeBombBayDoorsCB );

	pBank->AddToggle( "Allow rear doors to open", &ms_rearDoorsCanOpen );
	pBank->AddButton( "Trigger car alarm", triggerCarAlarmCB );
	pBank->AddButton( "Output door stuck debug for local player", outputDoorStuckDebugCB );
	pBank->AddButton( "Deploy Folding Wings", deployFoldingWingsCB );
	pBank->PopGroup();

	pBank->PushGroup( "Joints", false );
	pBank->AddButton( "Swing open boat boom", swingOpenBooatBoomCB );
	pBank->AddButton( "Swing in boat boom", swingInBooatBoomCB );
	pBank->PopGroup();

	pBank->PushGroup( "Tyres", false );
	pBank->AddSlider( "Tyre index", &ms_iTyreNumber, 0, NUM_VEH_CWHEELS_MAX, 1 );
	pBank->AddButton( "Repair Tyre", repairTyreCB );
	pBank->AddButton( "Break off Tyre", breakOffTyreCB );
	pBank->AddButton( "Toggle Burnout Mode", ToggleBurnoutModeCB );
	pBank->AddButton( "Toggle Reduce Grip Mode", ToggleReduceGripModeCB );
	pBank->AddButton( "Toggle Reduced Suspension Force", ToggleReducedSuspensionForceCB );
	pBank->AddButton( "Toggle Drift Tyres", ToggleDriftTyresCB );
	pBank->AddButton( "Toggle HOLD_GEAR_WITH_WHEELSPIN_FLAG", ToggleHoldGearWithWheelspinFlagCB );
	pBank->PopGroup();

	pBank->PushGroup( "Top Speed", false );
	pBank->AddSlider( "Top Speed Modifier", &sfTopSpeedAdjustment, -50.0f, 5000.0f, 1.0f );
	pBank->AddButton( "Apply To Last Car", ApplyTopSpeedToLastCarCB );
	pBank->AddButton( "Clear 0-60, 60-0 and highest g force", ClearZeroSixtySixtyZeroGForceCarCB );
	pBank->PopGroup();

	pBank->PushGroup( "Convertible Roof", false );
	pBank->AddButton( "Lower roof", lowerRoofCB );
	pBank->AddButton( "Raise roof", raiseRoofCB );
	pBank->AddToggle( "Lower/Raise instantly", &bLowerOrRaiseRoofInstantly );
	pBank->PopGroup();

	pBank->PushGroup( "Self Righting", false );
	pBank->AddToggle( "Apply new self-righting", &CVehicle::sm_bUseNewSelfRighting );
	pBank->AddSlider( "In air torque x", &CAutomobile::CAR_INAIR_ROT_X, 0.0f, 10.0f, 0.1f );
	pBank->AddSlider( "In air torque y", &CAutomobile::CAR_INAIR_ROT_Y, 0.0f, 10.0f, 0.1f );
	pBank->AddSlider( "In air torque z", &CAutomobile::CAR_INAIR_ROT_Z, -10.0f, 0.0f, 0.1f );
	pBank->AddSlider( "Stuck torque x", &CAutomobile::CAR_STUCK_ROT_X, 0.0f, 10.0f, 0.1f );
	pBank->AddSlider( "Stuck torque y", &CAutomobile::CAR_STUCK_ROT_Y, 0.0f, 10.0f, 0.1f );
	pBank->AddSlider( "Stuck torque y in self righting direction", &CAutomobile::CAR_STUCK_ROT_Y_SELF_RIGHT, 0.0f, 30.0f, 0.1f );
	pBank->AddSlider( "In air angular speed limit", &CAutomobile::CAR_INAIR_ROTLIM, 0.0f, 10.0f, 0.1f );
	pBank->AddSlider( "Possibly stuck speed threshold", &CAutomobile::POSSIBLY_STUCK_SPEED, 0.0f, 5.0f, 0.01f );
	pBank->AddSlider( "Completely stuck speed threshold", &CAutomobile::COMPLETELY_STUCK_SPEED, 0.0f, 5.0f, 0.01f );
	pBank->AddSlider( "self righting bias cg offset z", &CVehicle::smVecSelfRightBiasCGOffset.z, -10.0f, 0.0f, 0.1f );
	pBank->PopGroup();

	pBank->AddButton( "Toggle Gravity", toggleGravityCB );
	pBank->AddButton( "Pop out windscreen", popOutWindscreenCB );
	pBank->AddButton( "Set out of Control", setOutOfControlCB );
	pBank->AddSlider( "Roof impulse X", &sRoofImpulse.x, -50.0f, 50.0f, 0.1f );
	pBank->AddSlider( "Roof impulse Y", &sRoofImpulse.y, -50.0f, 50.0f, 0.1f );
	pBank->AddSlider( "Roof impulse Z", &sRoofImpulse.z, -50.0f, 50.0f, 0.1f );
	pBank->AddButton( "Pop off roof", popOffRoofCB );
	pBank->AddSlider( "Drop last Vehicle", &carDropHeight, 1.0f, 100.0f, 1.0f, DropCarCB );
	pBank->AddSlider( "Test Vehicle against world", &carTestHeight, -10.0f, 10.0f, 0.1f, TestCarAgainstWorldCB );
	pBank->AddToggle( "Use debug cam rotation", &bUseDebugCamRotation );
	pBank->AddSlider( "Vehicle rotation", &carDropRotation, -3.141f, 3.141f, 0.1f );
	pBank->AddButton( "Lock last Vehicle", LockLastCar );
	pBank->AddButton( "Switch dummy physics", ToggleDummyPhysicsCB );
	pBank->AddButton( "Freeze Vehicle", FreezeVehicleCB );
	pBank->AddButton( "Unfreeze Vehicle", UnfreezeVehicleCB );
	pBank->AddSlider( "Hydraulic suspension", &CVehicle::ms_fHydraulicSuspension, 0, 1.0f, 0.01f, HydraulicSuspensionCB );
	pBank->AddSlider( "Test Gravity Scale", &CVehicle::ms_fGravityScale, 0.1f, 10.0f, 0.01f );
	pBank->AddToggle( "Force bike lean", &CVehicle::ms_bForceBikeLean );
	pBank->AddToggle( "Use Automobile BVH update", &CVehicle::ms_bUseAutomobileBVHupdate );
	pBank->AddToggle( "Always update composite bound", &CVehicle::ms_bAlwaysUpdateCompositeBound );
	pBank->AddToggle( "Force slow zone", &CVehicle::ms_bForceSlowZone );
	pBank->AddButton( "Anchor boat", ToggleAnchorBoatCB );
	pBank->AddButton( "Reset Suspension", resetSuspensionCB );
	pBank->AddSlider( "Ground Clearance", &CVehicle::ms_fForcedGroundClearance, 0.0f, 2.5f, 0.01f );
	pBank->AddButton( "Set Ground Clearance", setGroundClearanceCB );
	pBank->AddSlider( "Bike Max Jump Time", &CBike::ms_fBikeJumpFullPowerTime, 0.0f, 10000.0f, 1.0f );
	pBank->AddToggle( "Stop Vehicles Damaging", &gbStopVehiclesDamaging );
	pBank->AddToggle( "Don't Display Dirt on Vehicles", &gbDontDisplayDirtOnVehicles );
	pBank->AddToggle( "Use wheel integration task", &CWheel::ms_bUseWheelIntegrationTask );
	pBank->AddToggle( "Disable mid-air integrator", &sbDisableMidairWheelIntegrator );
#if __DEV
	pBank->AddToggle( "Stop Vehicles Exploding", &gbStopVehiclesExploding );
	extern bool g_UseAdditionalWheelSweep;
	pBank->AddToggle( "Use wheel additional sweep and don't add velocity", &g_UseAdditionalWheelSweep );
	extern bool gbAlwaysRemoveWheelImpacts;
	pBank->AddToggle( "Always remove wheel impacts", &gbAlwaysRemoveWheelImpacts );
	pBank->AddSlider( "Force Vehicle Colors Mode", &CCustomShaderEffectVehicle::ms_nForceColorCars, 0, 2, 1 );
	pBank->AddSlider( "Sound Horn", &carSoundHornTime, 0, 10, 1, SoundCarHornCB );
	pBank->AddSlider( "Petrol Level", &sfPetrolTankLevel, 0.0f, 200.0f, 0.1f );
	pBank->AddButton( "Set Petrol Level", ChangePetrolTankLevelCarCB );
	pBank->AddToggle( "Render petrol tank damage", &gbRenderPetrolTankDamage );
#endif
	pBank->AddSlider( "Player car inverse mass scale", &gfPlayerCarInvMassScale, 0.0f, 10.0f, 0.01f );
	pBank->AddButton( "Set vehicle on fire", SetVehicleOnFireCB );
	pBank->AddButton( "Set vehicle on fire and clear health", SetVehicleOnFireAndClearHealthCB );
	pBank->AddButton( "Turn on searchlight", SetVehicleSearchlightCB );
	pBank->AddSlider( "Ambient Occlusion height cutoff", &CVehicle::ms_fAOHeightCutoff, 1.0f, 75.0f, 0.5f );
	pBank->PopGroup();

	pBank->PushGroup( "Roof", false );
	pBank->AddSlider( "Max speed squared for opening/closing roof(m/s)", &CTaskVehicleConvertibleRoof::ms_fMaximumSpeedToOpenRoofSq, 0.0f, 2000.0f, 0.001f );
	pBank->AddButton( "Open roof", openRoofCB );
	pBank->AddButton( "Close roof", closeRoofCB );
	pBank->PopGroup();

	pBank->PushGroup( "Handling cheats", false );
	pBank->AddToggle( "Abs", &sbCheatAbs );
	pBank->AddToggle( "Tc", &sbCheatTractionControl );
	pBank->AddToggle( "Sc", &sbCheatStabilityControl );
	pBank->AddToggle( "Grip1", &sbCheatGrip1 );
	pBank->AddToggle( "Grip2", &sbCheatGrip2 );
	pBank->AddToggle( "Power1", &sbCheatPower1 );
	pBank->AddToggle( "Power2", &sbCheatPower2 );
	pBank->AddButton( "Apply to last car", ApplyCheatsToLastCarCB );
	pBank->AddButton( "Apply to selected car", ApplyCheatsToSelectedCarCB );
	pBank->PopGroup();

	pBank->PushGroup( "Passengers", false );
	pBank->AddSlider( "Passenger Mass Mult", &CAutomobile::m_sfPassengerMassMult, -0.5f, 1.5f, 0.001f );
	pBank->PopGroup();

	pBank->PushGroup( "LOD", false );
	gDrawListMgr->SetupLODWidgets( *pBank, (int*)1, (int*)1, NULL, (int*)1 );
	pBank->AddToggle( "Force HD", &CVehicleFactory::ms_bForceHd, ForceHdCB );
	pBank->AddSlider( "Lod multiplier", &CVehicleFactory::ms_fLodMult, 0.1f, 10.f, 0.01f, LodMultCB );
	pBank->AddSlider( "Clamp render lod", &CVehicleFactory::ms_clampedRenderLod, -1, 3, 1, ClampRenderLodCB );
	pBank->PopGroup();

	pBank->PushGroup( "Flying Skill", false );
	pBank->AddToggle( "Ace Pilot", &CVehicleFactory::ms_bflyingAce );
	pBank->PopGroup();

#if __BANK
	pBank->PushGroup( "Physics", false );
	pBank->AddToggle( "PreComputeImpacts", &CVehicle::sm_PreComputeImpacts );
	pBank->AddToggle( "Door Debug Output", &CCarDoor::sm_debugDoorPhysics );
	pBank->PopGroup();
#endif

	pBank->PushGroup( "Dials", false );
	pBank->AddToggle( "Add Dials To Selected Vehicle", &CVehicleFactory::ms_bForceDials );
	pBank->PopGroup();

	CBike::InitWidgets( *pBank );
	CQuadBike::InitWidgets( *pBank );
	CPlane::InitWidgets( *pBank );
	CPropeller::InitWidgets( *pBank );
	CRotaryWingAircraft::InitWidgets();
	CSearchLight::InitWidgets( *pBank );
	CBoat::InitWidgets( *pBank );
	CTrain::InitWidgets( *pBank );
	CTrailer::InitWidgets( pBank );

	CBoardingPointEditor::CreateWidgets( pBank );
	CAIHandlingInfoMgr::AddWidgets( *pBank );

	CVehicleGadget::AddWidgets( *pBank );


#if __DEV
	CHeadlightTuningData::SetupHeadlightTuningBank( *pBank );
	CNeonTuningData::SetupNeonTuningBank( *pBank );
#endif // __DEV

	pBank->PushGroup( "Vehicle Damage" );
	pBank->PushGroup( "Wheel Damage", false );
	pBank->AddSlider( "Burst Chance", &CWheel::ms_fWheelBurstDamageChance, 0.0f, 1.0f, 0.001f );
	pBank->AddSlider( "Break Chance", &CVehicleDamage::ms_fChanceToBreak, 0.0f, 1.0f, 0.001f );
	pBank->AddSlider( "Damage Threshold", &CVehicle::sm_WheelDamageRadiusThreshold, 0.0f, 20.0f, 0.01f );
	pBank->AddSlider( "Additional Undamaged Damage Thresh", &CVehicle::sm_WheelDamageRadiusThreshWheelDamageMax, 0.0f, 20.0f, 0.01f );
	pBank->AddSlider( "Damaged Damage Thresh Reduction Scale", &CVehicle::sm_WheelDamageRadiusThreshWheelDamageScale, 0.0f, 20.0f, 0.01f );
	pBank->AddSlider( "Damage Radius Scale", &CVehicle::sm_WheelDamageRadiusScale, 0.0f, 50.0f, 0.01f );
	pBank->AddSlider( "Damage Inner Radius Scale", &CVehicle::sm_WheelSelfRadiusScale, 0.0f, 50.0f, 0.01f );
	pBank->AddSlider( "Damage Heavy Radius Scale", &CVehicle::sm_WheelHeavyDamageRadiusScale, 0.0f, 50.0f, 0.01f );
	pBank->AddSlider( "Damage Heavy Inner Radius Scale", &CVehicle::sm_WheelSelfHeavyWheelRadiusScale, 0.0f, 50.0f, 0.01f );
	pBank->AddSlider( "Friction Damage Thresh For Burst", &CWheel::ms_fFrictionDamageThreshForBurst, 0.0f, 2.0f, 0.001f );
	pBank->AddSlider( "Friction Damage Thresh For Burst", &CWheel::ms_fFrictionDamageThreshForBreak, 0.0f, 2.0f, 0.001f );
	pBank->AddButton( "Break off all wheels", CVehicleDamage::BreakOffWheelsCB );
	pBank->PopGroup();
	pBank->AddSlider( "Deformation threshold multiplier for MP game", &bfDeformationThresholdMPMult, 0.0f, 5.0f, 0.01f );
	pBank->AddSlider( "Max Vehicle Breaks Per Second", &CVehicle::ms_MaxVehicleBreaksPerSecond, 0.0f, 20.0f, 1.0f );
	pBank->AddSlider( "Vehicle Breaks Per Second", &CVehicle::ms_VehicleBreaksPerSecond, 0.0f, 30.0f, 1.0f );
	pBank->AddSlider( "Vehicle Break Rolling Window Size (seconds)", &CVehicle::ms_VehicleBreakWindowSize, 1, 30, 1 );
	pBank->PushGroup( "Vehicle Deformation", true );
	pBank->AddToggle( "Show both texture and rendertarget", &CVehicleDeformation::ms_bShowTextureAndRenderTarget );
	pBank->AddSlider( "Texture and Rendertarget scale", &CVehicleDeformation::ms_fDisplayTextureScale, 0.0f, 10.0f, 0.1f );
	pBank->AddToggle( "Display the damage map", &CVehicleDeformation::ms_bDisplayDamageMap );
	pBank->AddToggle( "Display the damage multiplier", &CVehicleDeformation::ms_bDisplayDamageMult );
	pBank->AddToggle( "Force global damage map scale", &CVehicleDeformation::ms_bForceDamageMapScale );
	pBank->AddToggle( "Full damage (similar to explosion) for collisions below", &CVehicleDeformation::ms_bFullDamage );
	pBank->AddSlider( "Global damage map scale", &CVehicleDeformation::ms_fForcedDamageMapScale, 0.0f, 1.0f, 0.01f );
	pBank->AddSlider( "Weapon impact damage scale", &CVehicleDeformation::ms_fWeaponImpactDamageScale, 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Collision X Multiplier", &CVehicleDeformation::ms_fVehDefColMultX, 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Collision Y Multiplier", &CVehicleDeformation::ms_fVehDefColMultY, 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Collision Z Multiplier", &CVehicleDeformation::ms_fVehDefColMultZ, 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Collision Negative Y Multiplier", &CVehicleDeformation::ms_fVehDefColMultYNeg, 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Collision Negative Z Multiplier", &CVehicleDeformation::ms_fVehDefColMultZNeg, 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Roof Collision Negative Z Multiplier", &CVehicleDeformation::ms_fVehDefRoofColMultZNeg, 0.0f, 10.0f, 0.01f );

	pBank->AddToggle( "Enable max z deformation clamping", &CVehicleDeformation::ms_bEnableRoofZDeformationClamping );

	pBank->AddSlider( "Roof Max Z Deformation", &CVehicleDeformation::ms_fDeformationCarRoofMinZ, -0.5f, -0.1f, 0.01f );
	pBank->AddSlider( "Car mod bone deformation multiplier", &CVehicleDeformation::ms_fCarModBoneDeformMult, 0.0f, 2.0f, 0.01f );

	pBank->AddSlider( "Mods damage offset X", &CVehicleDeformation::m_OffsetDamageToMods_X, -2.0f, 2.0f, 0.01f );
	pBank->AddSlider( "Mods damage offset Y", &CVehicleDeformation::m_OffsetDamageToMods_Y, -2.0f, 2.0f, 0.01f );
	pBank->AddSlider( "Mods damage offset Z", &CVehicleDeformation::m_OffsetDamageToMods_Z, -2.0f, 2.0f, 0.01f );

	pBank->AddSlider( "Car Damage lookup offset X", &CVehicleDeformation::m_DamageTextureOffset_X, -20.0f, 20.0f, 0.01f );
	pBank->AddSlider( "Car Damage lookup offset Y", &CVehicleDeformation::m_DamageTextureOffset_Y, -20.0f, 20.0f, 0.01f );
	pBank->AddSlider( "Car Damage lookup offset Z", &CVehicleDeformation::m_DamageTextureOffset_Z, -20.0f, 20.0f, 0.01f );

	pBank->AddSlider( "Car Body Damage X coordinate", &CVehicleDeformation::m_ImpactPositionLocal_X, -20.0f, 20.0f, 0.01f );
	pBank->AddSlider( "Car Body Damage Y coordinate", &CVehicleDeformation::m_ImpactPositionLocal_Y, -20.0f, 20.0f, 0.01f );
	pBank->AddSlider( "Car Body Damage Z coordinate", &CVehicleDeformation::m_ImpactPositionLocal_Z, -20.0f, 20.0f, 0.01f );

	pBank->AddSlider( "Car Body Impulse X", &CVehicleDeformation::m_Impulse_X, -1.0f, 1.0f, 0.01f );
	pBank->AddSlider( "Car Body Impulse Y", &CVehicleDeformation::m_Impulse_Y, -1.0f, 1.0f, 0.01f );
	pBank->AddSlider( "Car Body Impulse Z", &CVehicleDeformation::m_Impulse_Z, -1.0f, 1.0f, 0.01f );

	pBank->AddSlider( "Car Body Damage Amount", &CVehicleDeformation::ms_fDamageAmount, 0.0f, 100000.0f, 0.01f );
	pBank->AddSlider( "Car mod bone deformation multiplier", &CVehicleDeformation::ms_fCarModBoneDeformMult, 0.0f, 2.0f, 0.01f );

	pBank->AddSlider( "Damage % to apply", &CVehicleDeformation::ms_fDamagePercent, 1.0f, 100.0f, 1.0f );
	pBank->AddButton( "Damage car by (above) % of max health", CVehicleDamage::DamageCurrentCarByPercentage );

	pBank->AddButton( "Damage car body now", CVehicleDamage::DamageCurrentCar );
	pBank->AddButton( "Damage Driver side roof", CVehicleDamage::DamageDriverSideRoof );
	pBank->AddButton( "Head On Collision", CVehicleDamage::HeadOnCollision );
	pBank->AddButton( "Rear End Collision", CVehicleDamage::RearEndCollision );
	pBank->AddButton( "Left side Collision", CVehicleDamage::LeftSideCollision );
	pBank->AddButton( "Right side Collision", CVehicleDamage::RightSideCollision );
	pBank->AddButton( "Tail Rotor Collision", CVehicleDamage::TailRotorCollision );
	pBank->AddButton( "Submarine Implosion", CVehicleDamage::ImplodeSubmarine );

	pBank->AddSlider( "Extra explosion impulses", &CVehicleDeformation::ms_iExtraExplosionDeformations, 0, MAX_IMPACTS_PER_VEHICLE_PER_FRAME - 1, 1 );
	pBank->AddSlider( "Extra explosion damage", &CVehicleDeformation::ms_fExtraExplosionsDamage, 0.0f, 100.0f, 1.0f );
	pBank->AddButton( "Random Smash", CVehicleDamage::RandomSmash );

	pBank->AddToggle( "Enable AutoFix", &CVehicleDeformation::ms_bAutoFix );
	pBank->AddButton( "Fix Damage", &CVehicleDamage::FixDamageForTests );
	pBank->AddToggle( "Enable AutoSaving Damage Texture", &CVehicleDeformation::ms_bAutoSaveDamageTexture );

	pBank->AddToggle( "Enable Bounds Update", &CVehicleDeformation::ms_bUpdateBoundsEnabled );
	pBank->AddToggle( "Enable Bones  Update", &CVehicleDeformation::ms_bUpdateBonesEnabled );

#if GPU_DAMAGE_WRITE_ENABLED
	pBank->AddToggle( "Update Physics pre-sim", &CVehicleDeformation::ms_bUpdateDamageFromPhysicsThread );
	pBank->AddToggle( "Enable GPU Damage", &CVehicleDamage::ms_bEnableGPUDamage );
	pBank->AddSlider( "Forced Damage", &CVehicleDamage::ms_iForcedDamageEveryFrame, 0, MAX_IMPACTS_PER_VEHICLE_PER_FRAME, 1 );
#endif
	pBank->AddToggle( "Display Damage Vectors", &CVehicleDamage::ms_bDisplayDamageVectors );
	pBank->AddButton( "Save DamageTexture.jpg", CVehicleDamage::SaveDamageTexture );
	pBank->AddButton( "Load DamageTexture.jpg", CVehicleDamage::LoadDamageTexture );

	pBank->AddSlider( "Force propeller speed", &CVehicleDeformation::ms_fForcedPropellerSpeed, -100.0f, 100.0f, 1.0f );
	pBank->AddSlider( "Large Vehicle Radius Multiplier", &CVehicleDeformation::ms_fLargeVehicleRadiusMultiplier, 0.0f, 1.5f, 0.1f );

#if VEHICLE_DEFORMATION_PROPORTIONAL
	pBank->AddSlider( "Vehicle Damage Magnitude Multiplier", &CVehicleDeformation::ms_fDamageMagnitudeMult, 0.01f, 10.0f, 0.01f, CVehicleDamage::UpdateDamageMultiplier, "" );
#endif
	pBank->AddToggle( "Enable Damaging around bone bounds", &CVehicleDeformation::ms_bEnableExtraBoneDeformations );
	pBank->AddToggle( "Enable Car Roof Z-Deformation Clamping", &CVehicleDeformation::ms_bEnableRoofZDeformationClamping );

	pBank->AddButton( "Drop a 5-ton container on this object at X meters height", CVehicleDamage::DropFiveTonContainer );
	pBank->AddButton( "Remove wheel", CVehicleDamage::RemoveWheel );

	pBank->AddToggle( "Hide Propellers", &CVehicleDeformation::ms_bHidePropellers );
	pBank->AddSlider( "Force propeller speed", &CVehicleDeformation::ms_fForcedPropellerSpeed, -60.0f, 60.0f, 1.0f );
	pBank->AddButton( "Break off helicopter tail", CVehicleDamage::RemoveHelicopterTail );
	pBank->AddButton( "Break off helicopter propellers", CVehicleDamage::RemoveHelicopterPropellers );

	pBank->PushGroup( "Network Damage Modifiers", false );

	pBank->AddSlider( "Network front left damage", &CVehicleDamage::g_DamageDebug[ 0 ], 0, 3, 1 );
	pBank->AddSlider( "Network front right damage", &CVehicleDamage::g_DamageDebug[ 1 ], 0, 3, 1 );
	pBank->AddSlider( "Network middle left damage", &CVehicleDamage::g_DamageDebug[ 2 ], 0, 3, 1 );
	pBank->AddSlider( "Network middle right damage", &CVehicleDamage::g_DamageDebug[ 3 ], 0, 3, 1 );
	pBank->AddSlider( "Network rear left damage", &CVehicleDamage::g_DamageDebug[ 4 ], 0, 3, 1 );
	pBank->AddSlider( "Network rear right damage", &CVehicleDamage::g_DamageDebug[ 5 ], 0, 3, 1 );

	pBank->AddButton( "Network Smash", CVehicleDamage::NetworkSmashDebug );
	pBank->AddButton( "Network Smash Random", CVehicleDamage::NetworkSmashDebugRandom );

	pBank->AddSlider( "Network front left damage value", &CVehicleDamage::g_DamageValue[ 0 ], -1.0f, 10.0f, 0.0f );
	pBank->AddSlider( "Network front right damage value", &CVehicleDamage::g_DamageValue[ 1 ], -1.0f, 10.0f, 0.0f );
	pBank->AddSlider( "Network middle left damage value", &CVehicleDamage::g_DamageValue[ 2 ], -1.0f, 10.0f, 0.0f );
	pBank->AddSlider( "Network middle right damage value", &CVehicleDamage::g_DamageValue[ 3 ], -1.0f, 10.0f, 0.0f );
	pBank->AddSlider( "Network rear left damage value", &CVehicleDamage::g_DamageValue[ 4 ], -1.0f, 10.0f, 0.0f );
	pBank->AddSlider( "Network rear right damage value", &CVehicleDamage::g_DamageValue[ 5 ], -1.0f, 10.0f, 0.0f );

	pBank->AddSlider( "Front left level 0", &CVehicleDeformation::networkDamageModifiers[ 0 ][ 0 ], 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Front left level 1", &CVehicleDeformation::networkDamageModifiers[ 0 ][ 1 ], 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Front left level 2", &CVehicleDeformation::networkDamageModifiers[ 0 ][ 2 ], 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Front left level 3", &CVehicleDeformation::networkDamageModifiers[ 0 ][ 3 ], 0.0f, 10.0f, 0.01f );

	pBank->AddSlider( "Front right level 0", &CVehicleDeformation::networkDamageModifiers[ 1 ][ 0 ], 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Front right level 1", &CVehicleDeformation::networkDamageModifiers[ 1 ][ 1 ], 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Front right level 2", &CVehicleDeformation::networkDamageModifiers[ 1 ][ 2 ], 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Front right level 3", &CVehicleDeformation::networkDamageModifiers[ 1 ][ 3 ], 0.0f, 10.0f, 0.01f );

	pBank->AddSlider( "Middle left level 0", &CVehicleDeformation::networkDamageModifiers[ 2 ][ 0 ], 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Middle left level 1", &CVehicleDeformation::networkDamageModifiers[ 2 ][ 1 ], 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Middle left level 2", &CVehicleDeformation::networkDamageModifiers[ 2 ][ 2 ], 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Middle left level 3", &CVehicleDeformation::networkDamageModifiers[ 2 ][ 3 ], 0.0f, 10.0f, 0.01f );

	pBank->AddSlider( "Middle right level 0", &CVehicleDeformation::networkDamageModifiers[ 3 ][ 0 ], 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Middle right level 1", &CVehicleDeformation::networkDamageModifiers[ 3 ][ 1 ], 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Middle right level 2", &CVehicleDeformation::networkDamageModifiers[ 3 ][ 2 ], 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Middle right level 3", &CVehicleDeformation::networkDamageModifiers[ 3 ][ 3 ], 0.0f, 10.0f, 0.01f );

	pBank->AddSlider( "Rear left level 0", &CVehicleDeformation::networkDamageModifiers[ 4 ][ 0 ], 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Rear left level 1", &CVehicleDeformation::networkDamageModifiers[ 4 ][ 1 ], 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Rear left level 2", &CVehicleDeformation::networkDamageModifiers[ 4 ][ 2 ], 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Rear left level 3", &CVehicleDeformation::networkDamageModifiers[ 4 ][ 3 ], 0.0f, 10.0f, 0.01f );

	pBank->AddSlider( "Rear right level 0", &CVehicleDeformation::networkDamageModifiers[ 5 ][ 0 ], 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Rear right level 1", &CVehicleDeformation::networkDamageModifiers[ 5 ][ 1 ], 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Rear right level 2", &CVehicleDeformation::networkDamageModifiers[ 5 ][ 2 ], 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Rear right level 3", &CVehicleDeformation::networkDamageModifiers[ 5 ][ 3 ], 0.0f, 10.0f, 0.01f );

	pBank->PopGroup();

	pBank->AddSlider( "Deformation Random Multiplier - Min", &CVehicleDeformation::ms_fVehDefColVarMultMin, 0.5f, 0.99f, 0.01f );
	pBank->AddSlider( "Deformation Random Multiplier - Max", &CVehicleDeformation::ms_fVehDefColVarMultMax, 1.0f, 1.5f, 0.01f );
	pBank->AddSlider( "Deformation Random Addition   - Min", &CVehicleDeformation::ms_fVehDefColVarAddMin, -1.0f, 0.0f, 0.01f );
	pBank->AddSlider( "Deformation Random Addition   - Max", &CVehicleDeformation::ms_fVehDefColVarAddMax, 0.01f, 0.5f, 0.01f );

	pBank->AddSlider( "Deformation Tuning - m_sfVehDefColMax1", &CVehicleDeformation::m_sfVehDefColMax1, 0.0f, 1.5f, 0.01f );
	pBank->AddSlider( "Deformation Tuning - m_sfVehDefColMax2", &CVehicleDeformation::m_sfVehDefColMax2, 0.0f, 1.5f, 0.01f );
	pBank->AddSlider( "Deformation Tuning - m_sfVehDefColMult2", &CVehicleDeformation::m_sfVehDefColMult2, 0.0f, 2.0f, 0.01f );

	pBank->AddButton( "Set selected vehicle as source", CVehicleDamage::SetSourceVehicleCB );
	pBank->PushGroup( "Texture Cache", true );
	pBank->AddToggle( "Display Cache Stats", &CVehicleDeformation::ms_bDisplayTexCacheStats );
	pBank->AddButton( "Force Cache clear", CVehicleDeformation::ClearTexCache );
	pBank->PopGroup();
	pBank->PopGroup();

	pBank->AddToggle( "Never avoid vehicle explosion chain reactions", &CVehicleDamage::ms_bNeverAvoidVehicleExplosionChainReactions );
	pBank->AddToggle( "Always avoid vehicle explosion chain reactions", &CVehicleDamage::ms_bAlwaysAvoidVehicleExplosionChainReactions );
	pBank->AddToggle( "Use Vehicle Break Chance Multiplier Override", &CVehicleDamage::ms_bUseVehicleExplosionBreakChanceMultiplierOverride );
	pBank->AddSlider( "Vehicle Break Chance Multiplier Override", &CVehicleDamage::ms_fVehicleExplosionBreakChanceMultiplierOverride, 0.0f, 1.0f, 0.05f );
	CVehicleMetadataMgr::AddVehicleExplosionWidgets( *pBank );

	pBank->PushGroup( "Damage Duplication" );
	pBank->AddButton( "Set selected vehicle as source", CVehicleDamage::SetSourceVehicleCB );
	pBank->AddButton( "Set selected vehicle as destination", CVehicleDamage::SetDestinationVehicleCB );
	pBank->AddButton( "Duplicate damages from source to destination", CVehicleDamage::DuplicateDamageCB );
	pBank->AddText( "Source", (int*)( &CVehicleDamage::ms_SourceVehicle ) );
	pBank->AddText( "Destination", (int*)( &CVehicleDamage::ms_DestinationVehicle ) );
	pBank->PopGroup();

	pBank->PopGroup();

	pBank->PushGroup( "Transmission Tuning", false );
	CTransmission::AddWidgets( pBank );
	pBank->PopGroup();
#if HACK_GTA4_BOUND_GEOM_SECOND_SURFACE
	pBank->PushGroup( "Second Surface Tuning", false );
	pBank->PushGroup( "standard" );
	CVehicle::ms_vehicleSecondSurfaceConfig.AddWidgetsToBank( *pBank );
	pBank->PopGroup();
	pBank->PushGroup( "less sink" );
	CVehicle::ms_vehicleSecondSurfaceConfigLessSink.AddWidgetsToBank( *pBank );
	pBank->PopGroup();
	pBank->PopGroup();
#endif	// HACK_GTA4_BOUND_GEOM_SECOND_SURFACE

#if __BANK
	SetupVehicleBank( *pBank );
#endif

#if CSE_VEHICLE_EDITABLEVALUES
	CCustomShaderEffectVehicle::InitWidgets( *pBank );
#endif

	pBank->PushGroup( "Animation" );
	pBank->AddButton( "Play test on focus", PlayTestVehicleAnimCB );
	pBank->AddToggle( "Play anim physically", &ms_bPlayAnimPhysical );
	pBank->PopGroup();

	pBank->PushGroup( "Tyre temperature" );
	pBank->AddSlider( "Burst threshold", &CWheel::ms_fTyreTempBurstThreshold, 0.0f, 200.0f, 0.1f );
	pBank->AddSlider( "Heat mult", &CWheel::ms_fTyreTempHeatMult, 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Cool mult", &CWheel::ms_fTyreTempCoolMult, 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Moving Cool mult", &CWheel::ms_fTyreTempCoolMovingMult, 0.0f, 10.0f, 0.01f );
	pBank->PopGroup();

	pBank->PushGroup( "Vehicle Burnout", false );
	pBank->AddSlider( "Car burn out torque z", &CAutomobile::CAR_BURNOUT_ROT_Z, -10.0f, 0.0f, 0.1f );
	pBank->AddSlider( "Car burn out angular speed limit", &CAutomobile::CAR_BURNOUT_ROTLIM, 0.0f, 10.0f, 0.1f );
	pBank->AddSlider( "Quad bike burn out torque z", &CAutomobile::QUADBIKE_BURNOUT_ROT_Z, -10.0f, 0.0f, 0.1f );
	pBank->AddSlider( "Quad bike burn out speed limit", &CAutomobile::QUADBIKE_BURNOUT_ROTLIM, 0.0f, 10.0f, 0.1f );
	pBank->AddSlider( "Burn out wheel speed multiplier", &CAutomobile::BURNOUT_WHEEL_SPEED_MULT, 0.0f, 10.0f, 0.1f );
	pBank->PopGroup();

	pBank->PushGroup( "Franklin Vehicle Special", false );
	pBank->AddSlider( "Steer Time Step Multiplier", &CTaskVehiclePlayerDriveAutomobile::sfSpecialAbilitySteeringTimeStepMult, 0.0f, 10.0f, 0.01f );
	pBank->AddSlider( "Steer Speed Max", &CTaskVehiclePlayerDriveAutomobile::ms_fCAR_STEER_SMOOTH_RATE_STOPPED_SPECIAL, 0.0f, 50.0f, 0.1f );
	pBank->AddSlider( "Steer Speed Min", &CTaskVehiclePlayerDriveAutomobile::ms_fCAR_STEER_SMOOTH_RATE_ATSPEED_SPECIAL, 0.0f, 50.0f, 0.1f );
	pBank->PopGroup();

	CVehicleModelInfo::AddWidgets( *pBank );

	//update the car list if it's already been attempted to be updated once, as we don't want to call this before the level has loaded
	if( bNeedToUpdateVehicleList )
		CVehicleFactory::UpdateCarList();


	pBank->PushGroup( "Vehicle Variations Debug", false );
	u32 modelIndex = vehicleSorts ? GetModelIndex( vehicleSorts[ currVehicleNameSelection ] ) : fwModelId::MI_INVALID;
	u32 maxColors = 1;
	if( modelIndex != fwModelId::MI_INVALID )
	{
		CVehicleModelInfo* mi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo( fwModelId( strLocalIndex( modelIndex ) ) );
		if( mi )
		{
			VariationDebugUpdateColor();
			maxColors = mi->GetNumPossibleColours();
		}
	}


	ms_variationColorSlider = pBank->AddSlider( "Color", &ms_variationDebugColor, 0, Max( (int)1, (int)( maxColors - 1 ) ), 1, VariationDebugColorChangeCB );
	ms_variationWindowTintSlider = pBank->AddSlider( "Window tint", &ms_variationDebugWindowTint, -1, CVehicleModelInfo::GetVehicleColours()->m_WindowColors.GetCount() - 1, 1, VariationDebugWindowTintChangeCB );

	const char* modNames[] = // same order as CVehicleModelInfo::ms_aSetupDoorIds
	{
		"SPOILER",
		"BUMPER_F",
		"BUMPER_R",
		"SKIRT",
		"EXHAUST",
		"CHASSIS",
		"GRILL",
		"BONNET",
		"WING_L",
		"WING_R",
		"ROOF",

		// Start new visual mods
		"PLTHOLDER",
		"PLTVANITY",

		"INTERIOR1",
		"INTERIOR2",
		"INTERIOR3",
		"INTERIOR4",
		"INTERIOR5",
		"SEATS",
		"STEERING",
		"KNOB",
		"PLAQUE",
		"ICE",

		"TRUNK",
		"HYDRO",

		"ENGINEBAY1",
		"ENGINEBAY2",
		"ENGINEBAY3",

		"CHASSIS2",
		"CHASSIS3",
		"CHASSIS4",
		"CHASSIS5",

		"DOOR_L",
		"DOOR_R",
		
		"LIVERY_MOD",
		"LIGHTBAR",
		// End new visual mods

		// non visual mods (stats only)
		"ENGINE",
		"BRAKES",
		"GEARBOX",
		"HORN",
		"SUSPENSION",
		"ARMOUR",

		"NITROUS",
		"TURBO",
		"SUBWOOFER",
		"TYRE_SMOKE",
		"HYDRAULICS",
		"XENON_LIGHTS",

		// other mods that require special care
		"WHEELS",
		"WHEELS_REAR_OR_HYDRAULICS",
	};

	for( int i = 0; i < VMT_MAX; i++ )
	{
		ms_variationSliders[ i ] = pBank->AddSlider( modNames[ i ], &ms_variationMod[ i ], -1, -1, 1, VariationDebugModChangeCB );
	}

	pBank->AddButton("Add mod kit to car", VariationDebugAddModKitCB);
	pBank->AddToggle("Full randomization when adding mod kit to car", &ms_variationRandomMods);
	pBank->AddToggle("Render mods", &ms_variationRenderMods);

	pBank->AddButton("Set sprayed", SetSprayedCB);
	pBank->AddButton("Clear sprayed", ClearSprayedCB);

	pBank->PushGroup("Extras", false);
	pBank->AddToggle("All extras", &ms_variationAllExtras, VariationDebugAllExtrasChangeCB);
	char buf[32];
	for (s32 i = 0; i <= VEH_LAST_EXTRA - VEH_EXTRA_1; ++i)
	{
		formatf(buf, "Extra %d", i + 1);
		pBank->AddToggle(buf, &ms_variationExtras[i], VariationDebugExtrasChangeCB);
	}
	pBank->PopGroup();
	pBank->AddToggle("Display car variation", &ms_variationDebugDraw);
	pBank->PopGroup();

	pBank->PushGroup("Liveries", false);
		// these values will be updated when a car is created to track
		ms_liverySlider = pBank->AddSlider("Current livery", &ms_currentLivery, -1, -1, 1, CarLiveryChangeCB);
		ms_livery2Slider = pBank->AddSlider("Current livery2", &ms_currentLivery2, -1, -1, 1, CarLivery2ChangeCB);
	pBank->PopGroup();

	pBank->PushGroup("LOD debug tools", false);
	// these values will be updated when a car is created to track
	pBank->AddToggle("Show Vehicle LOD level", &ms_bShowVehicleLODLevel);
	pBank->AddToggle("Show Vehicle LOD data", &ms_bShowVehicleLODLevelData);
	pBank->AddButton("Validate all vehicles", ValidateAllVehiclesCB);
	pBank->AddSlider("Vehicle LOD scaling", &ms_vehicleLODScaling, 0.1f, 2.0f, 0.1f);
	pBank->AddSlider("Vehicle Hi Stream scaling", &ms_vehicleHiStreamScaling, 0.1f, 2.0f, 0.1f);
	pBank->PopGroup();

	pBank->PushGroup("Vehicle Rendering", false);
		pBank->AddToggle("Render initial spawn point", &CVehicleDebug::ms_bRenderInitialSpawnPoint);
	pBank->PopGroup();

	//pBank->PushGroup( "Bobblehead", false );
	//	pBank->AddSlider("Stiffness X", &CBobbleHead::ms_bobbleHeadStiffness[ 0 ], 0.0f, 5000.0f, 10.0f);
	//	pBank->AddSlider("Stiffness Y", &CBobbleHead::ms_bobbleHeadStiffness[ 1 ], 0.0f, 5000.0f, 10.0f);
	//	pBank->AddSlider("Damping X", &CBobbleHead::ms_bobbleHeadDamping[ 0 ], 0.0f, 5.0f, 0.01f);
	//	pBank->AddSlider("Damping Y", &CBobbleHead::ms_bobbleHeadDamping[ 1 ], 0.0f, 5.0f, 0.01f);
	//	pBank->AddSlider("Limit X", &CBobbleHead::ms_bobbleHeadLimits[ 0 ], 0.0f, 1.5f, 0.01f);
	//	pBank->AddSlider("Limit Y", &CBobbleHead::ms_bobbleHeadLimits[ 1 ], 0.0f, 1.5f, 0.01f);
	//	pBank->AddSlider("Mass", &CBobbleHead::ms_bobbleHeadMass, 0.1f, 100.0f, 0.1f );
	//pBank->PopGroup();

	CVehicleDamage::SetupVehicleDoorDamageBank(*pBank);
	CHeli::InitTurbulenceWidgets(*pBank);
}

void CVehicleFactory::UpdateCarList(void)
{
	//if there is no combo box don't update the vehicle list
	if(pVehicleCombo == NULL)
	{
		bNeedToUpdateVehicleList = true;
		UpdateVehicleList();
	}
	else
	{
		bNeedToUpdateVehicleList = false;
		UpdateAvailableCarsCB();
		UpdateVehicleList();
		CarNumChangeCB();
		TrailerNumChangeCB();
	}
}


// this function allows access to the create car callback for a debug key checked in debug.cpp
void CVehicleFactory::CreateCar(u32 modelId)
{
	if(modelId != fwModelId::MI_INVALID )
	{
		carModelId = modelId;
	}
	CreateCarCB();
}
void CVehicleFactory::WarpPlayerOutOfCar()
{
	WarpPlayerOutOfCarCB();
}
void CVehicleFactory::WarpPlayerIntoRecentCar()
{
	WarpPlayerIntoRecentCarCB();
}

void CVehicleFactory::PlayTestVehicleAnimCB()
{
	static strRequest animRequester;

	CVehicle* pVehicle = CVehicleDebug::GetFocusVehicle();
	
	if(!pVehicle)
	{
		return;
	}

	animRequester.Release();


	static bool testDump = true;

	const char* pDictName;
	const char* pAnimName;

	if(testDump)
	{
		// Find the clip
		pDictName = "va_dump";
		pAnimName = "dump_tipper";
	}
	else
	{
		pDictName = "va_feltzer2010";
		pAnimName = "feltzer2010_roof";
	}


	// Stream the anim
	strLocalIndex iSlot = fwAnimManager::FindSlot(pDictName);

	if(iSlot != -1)
	{
		if(!animRequester.Request(iSlot,fwAnimManager::GetStreamingModuleId(),STRFLAG_PRIORITY_LOAD))
		{
			// Force load
			CStreaming::LoadAllRequestedObjects(true);
		}

		Assert(animRequester.HasLoaded());

		if(animRequester.HasLoaded())
		{
			// Play the anim on the focus vehicle
			pVehicle->InitAnimLazy();
			fragInstGta* pFragInst = pVehicle->GetVehicleFragInst();
			Assert(pFragInst);

			pFragInst->GetCacheEntry()->LazyArticulatedInit();
			pVehicle->SetDriveMusclesToAnimation(ms_bPlayAnimPhysical);

			pVehicle->GetMoveVehicle().GetClipHelper().BlendInClipByClip(pVehicle, fwAnimManager::GetClipIfExistsByName(pDictName, pAnimName), FAST_BLEND_IN_DELTA);
		}
	
	}
	
}

void CVehicleFactory::VariationDebugUpdateColor()
{
	if (ms_pLastCreatedVehicle)
	{
		CVehicleModelInfo* mi = ms_pLastCreatedVehicle->GetVehicleModelInfo();
		if (mi)
		{
			// find out which color combination the vehicle has
			u8 col1 = ms_pLastCreatedVehicle->GetBodyColour1();
			u8 col2 = ms_pLastCreatedVehicle->GetBodyColour2();
			u8 col3 = ms_pLastCreatedVehicle->GetBodyColour3();
			u8 col4 = ms_pLastCreatedVehicle->GetBodyColour4();
			u8 col5 = ms_pLastCreatedVehicle->GetBodyColour5();
			u8 col6 = ms_pLastCreatedVehicle->GetBodyColour6();

			ms_variationDebugColor = 0;
			for (s32 i = 0; i < mi->GetNumPossibleColours(); ++i)
			{
				if (mi->GetPossibleColours(0, i) == col1 &&
					mi->GetPossibleColours(1, i) == col2 &&
					mi->GetPossibleColours(2, i) == col3 &&
					mi->GetPossibleColours(3, i) == col4 &&
					mi->GetPossibleColours(4, i) == col5 &&
					mi->GetPossibleColours(5, i) == col6 )
				{
					ms_variationDebugColor = i;
					return;
				}
			}
		}

		ms_variationDebugWindowTint = ms_pLastCreatedVehicle->GetVariationInstance().GetWindowTint();
	}
}

inline int CVehicleFactory::VariationDebugGetLastVehMod(eVehicleModType slot) {
	// Work around GetModIndexForType returning -1 as a u8 - i.e. 255
	static bool dummy = false;
	int idxMod = ms_pLastCreatedVehicle->GetVariationInstance().GetModIndexForType(slot, ms_pLastCreatedVehicle, dummy);

	return idxMod == 255 ? -1 : idxMod;
}

void CVehicleFactory::VariationDebugUpdateMods()
{
	if (!ms_pLastCreatedVehicle || !ms_pLastCreatedVehicle->IsModded())
		return;

	for( int i = 0; i < VMT_MAX; i++ )
	{
		if( ms_variationMod[ i ] )
		{
			ms_variationMod[ i ] = VariationDebugGetLastVehMod( (eVehicleModType)i );
		}
	}

	//if (ms_variationSpoilerMod)
	//	ms_variationSpoilerMod = VariationDebugGetLastVehMod(VMT_SPOILER);
	//if (ms_variationBumperFMod)
	//	ms_variationBumperFMod = VariationDebugGetLastVehMod(VMT_BUMPER_F);
	//if (ms_variationBumperRMod)
	//	ms_variationBumperRMod = VariationDebugGetLastVehMod(VMT_BUMPER_R);
	//if (ms_variationSkirtMod)
	//	ms_variationSkirtMod = VariationDebugGetLastVehMod(VMT_SKIRT);
	//if (ms_variationExhaustMod)
	//	ms_variationExhaustMod = VariationDebugGetLastVehMod(VMT_EXHAUST);
	//if (ms_variationChassisMod)
	//	ms_variationChassisMod = VariationDebugGetLastVehMod(VMT_CHASSIS);
	//if (ms_variationGrillMod)
	//	ms_variationGrillMod = VariationDebugGetLastVehMod(VMT_GRILL);
	//if (ms_variationBonnetMod)
	//	ms_variationBonnetMod = VariationDebugGetLastVehMod(VMT_BONNET);
	//if (ms_variationWingLMod)
	//	ms_variationWingLMod = VariationDebugGetLastVehMod(VMT_WING_L);
	//if (ms_variationWingRMod)
	//	ms_variationWingRMod = VariationDebugGetLastVehMod(VMT_WING_R);
	//if (ms_variationRoofMod)
	//	ms_variationRoofMod = VariationDebugGetLastVehMod(VMT_ROOF);
	//if (ms_variationEngineMod)
	//	ms_variationEngineMod = VariationDebugGetLastVehMod(VMT_ENGINE);
	//if (ms_variationBrakesMod)
	//	ms_variationBrakesMod = VariationDebugGetLastVehMod(VMT_BRAKES);
	//if (ms_variationGearboxMod)
	//	ms_variationGearboxMod = VariationDebugGetLastVehMod(VMT_GEARBOX);
	//if (ms_variationHornMod)
	//	ms_variationHornMod = VariationDebugGetLastVehMod(VMT_HORN);
	//if (ms_variationSuspensionMod)
	//	ms_variationSuspensionMod = VariationDebugGetLastVehMod(VMT_SUSPENSION);
	//if (ms_variationArmourMod)
	//	ms_variationArmourMod = VariationDebugGetLastVehMod(VMT_ARMOUR);
	//if (ms_variationWheelMod)
	//	ms_variationWheelMod = VariationDebugGetLastVehMod(VMT_WHEELS);
	//if (ms_variationRearWheelMod)
	//	ms_variationRearWheelMod = VariationDebugGetLastVehMod(VMT_WHEELS_REAR_OR_HYDRAULICS);
	//if (ms_variationLiveryMod)
	//	ms_variationLiveryMod = VariationDebugGetLastVehMod(VMT_LIVERY_MOD);


	for( int i = 0; i < VMT_MAX; i++ )
	{
		if( ms_variationSliders[ i ] )
		{
			ms_variationSliders[ i ]->SetRange( -1.f, (float)ms_pLastCreatedVehicle->GetVariationInstance().GetNumModsForType( (eVehicleModType)i, ms_pLastCreatedVehicle ) - 1.f );
		}
	}

	//if (ms_variationSpoilerSlider)
	//	ms_variationSpoilerSlider->SetRange(-1.f, (float)ms_pLastCreatedVehicle->GetVariationInstance().GetNumModsForType(VMT_SPOILER, ms_pLastCreatedVehicle) - 1.f);
	//if (ms_variationBumperFSlider)
	//	ms_variationBumperFSlider->SetRange(-1.f, (float)ms_pLastCreatedVehicle->GetVariationInstance().GetNumModsForType(VMT_BUMPER_F, ms_pLastCreatedVehicle) - 1.f);
	//if (ms_variationBumperRSlider)
	//	ms_variationBumperRSlider->SetRange(-1.f, (float)ms_pLastCreatedVehicle->GetVariationInstance().GetNumModsForType(VMT_BUMPER_R, ms_pLastCreatedVehicle) - 1.f);
	//if (ms_variationSkirtSlider)
	//	ms_variationSkirtSlider->SetRange(-1.f, (float)ms_pLastCreatedVehicle->GetVariationInstance().GetNumModsForType(VMT_SKIRT, ms_pLastCreatedVehicle) - 1.f);
	//if (ms_variationExhaustSlider)
	//	ms_variationExhaustSlider->SetRange(-1.f, (float)ms_pLastCreatedVehicle->GetVariationInstance().GetNumModsForType(VMT_EXHAUST, ms_pLastCreatedVehicle) - 1.f);
	//if (ms_variationChassisSlider)
	//	ms_variationChassisSlider->SetRange(-1.f, (float)ms_pLastCreatedVehicle->GetVariationInstance().GetNumModsForType(VMT_CHASSIS, ms_pLastCreatedVehicle) - 1.f);
	//if (ms_variationGrillSlider)
	//	ms_variationGrillSlider->SetRange(-1.f, (float)ms_pLastCreatedVehicle->GetVariationInstance().GetNumModsForType(VMT_GRILL, ms_pLastCreatedVehicle) - 1.f);
	//if (ms_variationBonnetSlider)
	//	ms_variationBonnetSlider->SetRange(-1.f, (float)ms_pLastCreatedVehicle->GetVariationInstance().GetNumModsForType(VMT_BONNET, ms_pLastCreatedVehicle) - 1.f);
	//if (ms_variationWingLSlider)
	//	ms_variationWingLSlider->SetRange(-1.f, (float)ms_pLastCreatedVehicle->GetVariationInstance().GetNumModsForType(VMT_WING_L, ms_pLastCreatedVehicle) - 1.f);
	//if (ms_variationWingRSlider)
	//	ms_variationWingRSlider->SetRange(-1.f, (float)ms_pLastCreatedVehicle->GetVariationInstance().GetNumModsForType(VMT_WING_R, ms_pLastCreatedVehicle) - 1.f);
	//if (ms_variationRoofSlider)
	//	ms_variationRoofSlider->SetRange(-1.f, (float)ms_pLastCreatedVehicle->GetVariationInstance().GetNumModsForType(VMT_ROOF, ms_pLastCreatedVehicle) - 1.f);
	//if (ms_variationEngineSlider)
	//	ms_variationEngineSlider->SetRange(-1.f, (float)ms_pLastCreatedVehicle->GetVariationInstance().GetNumModsForType(VMT_ENGINE, ms_pLastCreatedVehicle) - 1.f);
	//if (ms_variationBrakesSlider)
	//	ms_variationBrakesSlider->SetRange(-1.f, (float)ms_pLastCreatedVehicle->GetVariationInstance().GetNumModsForType(VMT_BRAKES, ms_pLastCreatedVehicle) - 1.f);
	//if (ms_variationGearboxSlider)
	//	ms_variationGearboxSlider->SetRange(-1.f, (float)ms_pLastCreatedVehicle->GetVariationInstance().GetNumModsForType(VMT_GEARBOX, ms_pLastCreatedVehicle) - 1.f);
	//if (ms_variationHornSlider)
	//	ms_variationHornSlider->SetRange(-1.f, (float)ms_pLastCreatedVehicle->GetVariationInstance().GetNumModsForType(VMT_HORN, ms_pLastCreatedVehicle) - 1.f);
	//if (ms_variationSuspensionSlider)
	//	ms_variationSuspensionSlider->SetRange(-1.f, (float)ms_pLastCreatedVehicle->GetVariationInstance().GetNumModsForType(VMT_SUSPENSION, ms_pLastCreatedVehicle) - 1.f);
	//if (ms_variationArmourSlider)
	//	ms_variationArmourSlider->SetRange(-1.f, (float)ms_pLastCreatedVehicle->GetVariationInstance().GetNumModsForType(VMT_ARMOUR, ms_pLastCreatedVehicle) - 1.f);
	//if (ms_variationWheelSlider)
	//	ms_variationWheelSlider->SetRange(-1.f, (float)ms_pLastCreatedVehicle->GetVariationInstance().GetNumModsForType(VMT_WHEELS, ms_pLastCreatedVehicle) - 1.f);
	//if (ms_variationRearWheelSlider)
	//	ms_variationRearWheelSlider->SetRange(-1.f, (float)ms_pLastCreatedVehicle->GetVariationInstance().GetNumModsForType(VMT_WHEELS_REAR_OR_HYDRAULICS, ms_pLastCreatedVehicle) - 1.f);
	//if (ms_variationLiveryModSlider)
	//	ms_variationLiveryModSlider->SetRange(-1.f, (float)ms_pLastCreatedVehicle->GetVariationInstance().GetNumModsForType(VMT_LIVERY_MOD, ms_pLastCreatedVehicle) - 1.f);
}

void CVehicleFactory::VariationDebugColorChangeCB()
{
	//if (ms_pLastCreatedVehicle)
	//{
	//	CVehicleModelInfo* mi = ms_pLastCreatedVehicle->GetVehicleModelInfo();
	//	if (mi)
	//	{
	//		//ms_pLastCreatedVehicle->SetBodyColour1(mi->GetPossibleColours(0, ms_variationDebugColor));
	//		//ms_pLastCreatedVehicle->SetBodyColour2(mi->GetPossibleColours(1, ms_variationDebugColor));
	//		//ms_pLastCreatedVehicle->SetBodyColour3(mi->GetPossibleColours(2, ms_variationDebugColor));
	//		//ms_pLastCreatedVehicle->SetBodyColour4(mi->GetPossibleColours(3, ms_variationDebugColor));
	//		//ms_pLastCreatedVehicle->SetBodyColour5(mi->GetPossibleColours(4, ms_variationDebugColor));
	//		//ms_pLastCreatedVehicle->SetBodyColour6(mi->GetPossibleColours(5, ms_variationDebugColor));

	//		// let shaders know, that body colours changed
	//		//CVehicleFactory::ms_pLastCreatedVehicle->UpdateBodyColourRemapping();
	//	}
	//}
}

void CVehicleFactory::VariationDebugWindowTintChangeCB()
{
	if (ms_pLastCreatedVehicle)
	{
		ms_pLastCreatedVehicle->GetVariationInstance().SetWindowTint((u8)ms_variationDebugWindowTint);
	}
}

void CVehicleFactory::VariationDebugModChangeCB()
{
	if (ms_pLastCreatedVehicle)
	{
		for( int i = 0; i < VMT_MAX; i++ )
		{
			ms_pLastCreatedVehicle->GetVariationInstance().SetModIndexForType( (eVehicleModType)i, (u8)ms_variationMod[ i ], ms_pLastCreatedVehicle, false );
		}
		//ms_pLastCreatedVehicle->GetVariationInstance().SetModIndexForType(VMT_SPOILER, (u8)ms_variationSpoilerMod, ms_pLastCreatedVehicle, false);
		//ms_pLastCreatedVehicle->GetVariationInstance().SetModIndexForType(VMT_BUMPER_F, (u8)ms_variationBumperFMod, ms_pLastCreatedVehicle, false);
		//ms_pLastCreatedVehicle->GetVariationInstance().SetModIndexForType(VMT_BUMPER_R, (u8)ms_variationBumperRMod, ms_pLastCreatedVehicle, false);
		//ms_pLastCreatedVehicle->GetVariationInstance().SetModIndexForType(VMT_SKIRT, (u8)ms_variationSkirtMod, ms_pLastCreatedVehicle, false);
		//ms_pLastCreatedVehicle->GetVariationInstance().SetModIndexForType(VMT_EXHAUST, (u8)ms_variationExhaustMod, ms_pLastCreatedVehicle, false);
		//ms_pLastCreatedVehicle->GetVariationInstance().SetModIndexForType(VMT_CHASSIS, (u8)ms_variationChassisMod, ms_pLastCreatedVehicle, false);
		//ms_pLastCreatedVehicle->GetVariationInstance().SetModIndexForType(VMT_GRILL, (u8)ms_variationGrillMod, ms_pLastCreatedVehicle, false);
		//ms_pLastCreatedVehicle->GetVariationInstance().SetModIndexForType(VMT_BONNET, (u8)ms_variationBonnetMod, ms_pLastCreatedVehicle, false);
		//ms_pLastCreatedVehicle->GetVariationInstance().SetModIndexForType(VMT_WING_L, (u8)ms_variationWingLMod, ms_pLastCreatedVehicle, false);
		//ms_pLastCreatedVehicle->GetVariationInstance().SetModIndexForType(VMT_WING_R, (u8)ms_variationWingRMod, ms_pLastCreatedVehicle, false);
		//ms_pLastCreatedVehicle->GetVariationInstance().SetModIndexForType(VMT_ROOF, (u8)ms_variationRoofMod, ms_pLastCreatedVehicle, false);
		//ms_pLastCreatedVehicle->GetVariationInstance().SetModIndexForType(VMT_ENGINE, (u8)ms_variationEngineMod, ms_pLastCreatedVehicle, false);
		//ms_pLastCreatedVehicle->GetVariationInstance().SetModIndexForType(VMT_BRAKES, (u8)ms_variationBrakesMod, ms_pLastCreatedVehicle, false);
		//ms_pLastCreatedVehicle->GetVariationInstance().SetModIndexForType(VMT_GEARBOX, (u8)ms_variationGearboxMod, ms_pLastCreatedVehicle, false);
		//ms_pLastCreatedVehicle->GetVariationInstance().SetModIndexForType(VMT_HORN, (u8)ms_variationHornMod, ms_pLastCreatedVehicle, false);
		//ms_pLastCreatedVehicle->GetVariationInstance().SetModIndexForType(VMT_SUSPENSION, (u8)ms_variationSuspensionMod, ms_pLastCreatedVehicle, false);
		//ms_pLastCreatedVehicle->GetVariationInstance().SetModIndexForType(VMT_ARMOUR, (u8)ms_variationArmourMod, ms_pLastCreatedVehicle, false); 
		//ms_pLastCreatedVehicle->GetVariationInstance().SetModIndexForType(VMT_WHEELS, (u8)ms_variationWheelMod, ms_pLastCreatedVehicle, false);
		//ms_pLastCreatedVehicle->GetVariationInstance().SetModIndexForType(VMT_WHEELS_REAR_OR_HYDRAULICS, (u8)ms_variationRearWheelMod, ms_pLastCreatedVehicle, false);
		//ms_pLastCreatedVehicle->GetVariationInstance().SetModIndexForType(VMT_LIVERY_MOD, (u8)ms_variationLiveryMod, ms_pLastCreatedVehicle, false);
	}
}

void CVehicleFactory::VariationDebugAddModKitCB()
{
	if (ms_pLastCreatedVehicle)
	{
		ms_pLastCreatedVehicle->GetVehicleModelInfo()->GenerateVariation(ms_pLastCreatedVehicle, ms_pLastCreatedVehicle->GetVariationInstance(), ms_variationRandomMods);
		VariationDebugUpdateMods();
	}
	else
	{
		if (CPed* localPlayer = CPedFactory::GetFactory()->GetLocalPlayer())
		{
			if (CVehicle* vehicle = localPlayer->GetMyVehicle())
			{
				ms_pLastCreatedVehicle = vehicle;

				ms_pLastCreatedVehicle->GetVehicleModelInfo()->GenerateVariation(ms_pLastCreatedVehicle, ms_pLastCreatedVehicle->GetVariationInstance(), ms_variationRandomMods);
				VariationDebugUpdateMods();
			}
		}
	}
}

void CVehicleFactory::SetSprayedCB()
{
	CVehicle* pVeh = CGameWorld::FindLocalPlayerVehicle();
	if (!pVeh)
		return;
	
	fwExtensibleBase* pBase = (fwExtensibleBase*)pVeh;


	atHashWithStringBank hash("Sprayed_Vehicle_Decorator");
	bool val = true;
	fwDecoratorExtension::Set((*pBase), hash, val);
}

void CVehicleFactory::ClearSprayedCB()
{
	CVehicle* pVeh = CGameWorld::FindLocalPlayerVehicle();
	if (!pVeh)
		return;

	fwExtensibleBase* pBase = (fwExtensibleBase*)pVeh;

	atHashWithStringBank hash("Sprayed_Vehicle_Decorator");
	fwDecoratorExtension::RemoveFrom((*pBase),hash);
}

void CVehicleFactory::VariationDebugExtrasChangeCB()
{
	if (!CVehicleFactory::ms_pLastCreatedVehicle)
		return;

	for (s32 i = 0; i < VEH_LAST_EXTRA - VEH_EXTRA_1 + 1; ++i)
	{
		// since callback is called on each change, there's only one entry that can vary so we can break when we find the first one
		// turning an extra on might cause other ones in an inclusion group to turn on so we have to break or else we might
		// override and lose the correct state
		if (CVehicleFactory::ms_pLastCreatedVehicle->GetIsExtraOn((eHierarchyId)(VEH_EXTRA_1 + i)) != ms_variationExtras[i])
		{
			CVehicleFactory::ms_pLastCreatedVehicle->TurnOffExtra((eHierarchyId)(VEH_EXTRA_1 + i), !ms_variationExtras[i]);
			break;
		}
	}

	// update the check boxes since inclusion groups could have changed the state of some extras
	for (s32 f = 0; f <= VEH_LAST_EXTRA - VEH_EXTRA_1; ++f)
	{
		CVehicleFactory::ms_variationExtras[f] = CVehicleFactory::ms_pLastCreatedVehicle->GetIsExtraOn((eHierarchyId)(VEH_EXTRA_1 + f));
	}
}

void CVehicleFactory::VariationDebugAllExtrasChangeCB()
{
	for (s32 i = 0; i < VEH_LAST_EXTRA - VEH_EXTRA_1 + 1; ++i)
	{
		ms_variationExtras[i] = ms_variationAllExtras;
	}

	if (!CVehicleFactory::ms_pLastCreatedVehicle)
		return;

	for (s32 i = 0; i < VEH_LAST_EXTRA - VEH_EXTRA_1 + 1; ++i)
	{
		CVehicleFactory::ms_pLastCreatedVehicle->TurnOffExtra((eHierarchyId)(VEH_EXTRA_1 + i), !ms_variationExtras[i]);
	}
}

void CVehicleFactory::UpdateWindowTintSlider()
{
	if (ms_variationWindowTintSlider)
		ms_variationWindowTintSlider = pBank->AddSlider("Window tint", &ms_variationDebugWindowTint, -1, CVehicleModelInfo::GetVehicleColours()->m_WindowColors.GetCount() - 1, 1, VariationDebugWindowTintChangeCB);
}

void CVehicleFactory::CarLiveryChangeCB(void)
{
	if (ms_pLastCreatedVehicle)
	{
		ms_pLastCreatedVehicle->SetLiveryId(ms_currentLivery);
		ms_pLastCreatedVehicle->UpdateBodyColourRemapping();
	}
};

void CVehicleFactory::CarLivery2ChangeCB(void)
{
	if (ms_pLastCreatedVehicle)
	{
		ms_pLastCreatedVehicle->SetLivery2Id(ms_currentLivery2);
		ms_pLastCreatedVehicle->UpdateBodyColourRemapping();
	}
};

#undef VISUALISE_RANGE
#define VISUALISE_RANGE	(30.f * 30.f)
void CVehicleFactory::DebugUpdate()
{
	if ( pCreateBankButton && (PARAM_debugvehicle.Get() || PARAM_debugtrain.Get()) )
	{
		CreateBank();
	}

	if (!ms_variationDebugDraw)
		return;

	CVehicle::Pool* pool = CVehicle::GetPool();
	s32 i = (s32) pool->GetSize();	
	char buf[64];
	while(i--)
	{
		CVehicle* pVehicle = pool->GetSlot(i);
		if (!pVehicle)
			continue;

		CVehicleModelInfo* mi = pVehicle->GetVehicleModelInfo();
		if (!mi)
			continue;

		Vector3	pos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
		Vector3 vDiff = pos - camInterface::GetPos();
		float fDist = vDiff.Mag2();

		if(fDist >= VISUALISE_RANGE) 
			continue;

		float fScale = 1.0f - (fDist / VISUALISE_RANGE);
		Vector3 WorldCoors = pos + Vector3(0,0,1.1f);
		u8 iAlpha = (u8)Clamp((int)(255.0f * fScale), 0, 255);

		// find out which color combination the vehicle has
		u8 col1 = pVehicle->GetBodyColour1();
		u8 col2 = pVehicle->GetBodyColour2();
		u8 col3 = pVehicle->GetBodyColour3();
		u8 col4 = pVehicle->GetBodyColour4();
		u8 col5 = pVehicle->GetBodyColour5();
		u8 col6 = pVehicle->GetBodyColour6();

		s32 colIdx = 0;
		for (s32 i = 0; i < mi->GetNumPossibleColours(); ++i)
		{
			if (mi->GetPossibleColours(0, i) == col1 &&
				mi->GetPossibleColours(1, i) == col2 &&
				mi->GetPossibleColours(2, i) == col3 &&
				mi->GetPossibleColours(3, i) == col4 &&
				mi->GetPossibleColours(4, i) == col5 &&
				mi->GetPossibleColours(5, i) == col6 )
			{
				colIdx = i;
				break;
			}
		}

		s32 iNumLines = 0;
		const CVehicleVariationInstance& variation = pVehicle->GetVariationInstance();

		formatf(buf, "Local color idx: %d", colIdx);
		grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
		formatf(buf, "Body color 1: (%s)", CVehicleModelInfo::GetVehicleColours()->GetVehicleColourText(col1));
		grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
		formatf(buf, "Body color 2: (%s)", CVehicleModelInfo::GetVehicleColours()->GetVehicleColourText(col2));
		grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
		formatf(buf, "Body color 3: (%s)", CVehicleModelInfo::GetVehicleColours()->GetVehicleColourText(col3));
		grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
		formatf(buf, "Body color 4: (%s)", CVehicleModelInfo::GetVehicleColours()->GetVehicleColourText(col4));
		grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
		formatf(buf, "Body color 5: (%s)", CVehicleModelInfo::GetVehicleColours()->GetVehicleColourText(col5));
		grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
		formatf(buf, "Body color 6: (%s)", CVehicleModelInfo::GetVehicleColours()->GetVehicleColourText(col6));
		grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
		formatf(buf, "Window tint: (%s)", CVehicleModelInfo::GetVehicleColours()->GetVehicleColourText(variation.GetWindowTint()));
		grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);

		for (s32 l = 0; l < MAX_NUM_LIVERIES; ++l)
		{
			formatf(buf, "Livery %d colors: ", l);
			for (s32 c = 0; c < MAX_NUM_LIVERY_COLORS; ++c)
			{
				u8 livCol = mi->GetLiveryColorIndex(l, c);
				if (livCol != 255)
				{
					char buf2[16] = {0};
					formatf(buf2, "%d ", livCol);
					safecat(buf, buf2, sizeof(buf));
				}
			}

			grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
		}

		if (variation.GetKit())
		{
			switch (variation.GetKit()->GetType())
			{
			case MKT_STANDARD:
				safecpy(buf, "Standard mod kit");
				break;
			case MKT_SPORT:
				safecpy(buf, "Sport mod kit");
				break;
			case MKT_SUV:
				safecpy(buf, "SUV mod kit");
				break;
			case MKT_SPECIAL:
				safecpy(buf, "Special mod kit");
				break;
			default:
				safecpy(buf, "Unknown (!) mod kit");
			}
			grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
		}

		if (variation.IsToggleModOn(VMT_TYRE_SMOKE))
		{
			formatf(buf, "Tyre smoke (%d, %d, %d)", variation.GetSmokeColorR(), variation.GetSmokeColorG(), variation.GetSmokeColorB());
			grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
		}

		if (variation.GetModIndex(VMT_WHEELS) != INVALID_MOD)
		{
			const CVehicleWheel& wheel = CVehicleModelInfo::GetVehicleColours()->m_Wheels[pVehicle->GetVehicleModelInfo()->GetWheelType()][variation.GetModIndex(VMT_WHEELS)];
			formatf(buf, "Wheels %s", wheel.GetNameHashString().GetCStr());
			grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
		}

		if (variation.GetModIndex(VMT_WHEELS_REAR_OR_HYDRAULICS) != INVALID_MOD)
		{
			Assertf(pVehicle->GetVehicleModelInfo()->GetWheelType() == VWT_BIKE, "Rear wheel mod on non bike type wheel for vehicle '%s'", pVehicle->GetVehicleModelInfo()->GetModelName());
			const CVehicleWheel& wheel = CVehicleModelInfo::GetVehicleColours()->m_Wheels[pVehicle->GetVehicleModelInfo()->GetWheelType()][variation.GetModIndex(VMT_WHEELS_REAR_OR_HYDRAULICS)];
			formatf(buf, "Rear Wheels %s", wheel.GetNameHashString().GetCStr());
			grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
		}

		if (variation.GetModIndex(VMT_SPOILER) != INVALID_MOD)
		{
			grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), "Spoiler mod enabled");
		}

		for (s32 i = 0; i <= VEH_LAST_EXTRA - VEH_EXTRA_1; ++i)
		{
			if (!pVehicle->GetDoesExtraExist((eHierarchyId)(VEH_EXTRA_1 + i)))
				continue;

			formatf(buf, "VEH_EXTRA_%d is %s", i + 1, pVehicle->GetIsExtraOn((eHierarchyId)(VEH_EXTRA_1 + i)) ? "on" : "off");
			grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
		}

		if (variation.GetNumMods() > 0)
		{
			formatf(buf, "Engine modifier: %d%s", variation.GetEngineModifier(), "%");
			grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
			formatf(buf, "Brakes modifier: %d%s", variation.GetBrakesModifier(), "%");
			grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
			formatf(buf, "Gearbox modifier: %d%s", variation.GetGearboxModifier(), "%");
			grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
			formatf(buf, "Armour modifier: %d%s", variation.GetArmourModifier(), "%");
			grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
			formatf(buf, "Suspension modifier: %d%s", variation.GetSuspensionModifier(), "%");
			grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
			formatf(buf, "Weight modifier: %d%s", variation.GetTotalWeightModifier(), "%");
			grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);


			if (variation.IsToggleModOn(VMT_NITROUS))
			{
				formatf(buf, "Nitrous ON!");
				grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
			}
			if (variation.IsToggleModOn(VMT_TURBO))
			{
				formatf(buf, "Turbo ON!");
				grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
			}
			if (variation.IsToggleModOn(VMT_SUBWOOFER))
			{
				formatf(buf, "Subwoofer ON!");
				grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
			}
			if (variation.IsToggleModOn(VMT_XENON_LIGHTS))
			{
				formatf(buf, "Xenon Lights ON!");
				grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
			}
			if (variation.IsToggleModOn(VMT_HYDRAULICS))
			{
				formatf(buf, "Hydraulics ON!");
				grcDebugDraw::Text(WorldCoors, CRGBA(255, 0, 0, iAlpha), 0, (iNumLines++) * grcDebugDraw::GetScreenSpaceTextHeight(), buf);
			}
		}
	}
}

int CVehicleFactory::CbCompareNames(const char* cLeft, const char* cRight)
{
	return (cLeft && cRight)? 0 > strcmp(cLeft, cRight) : cLeft < cRight;
}

void CVehicleFactory::AddLayoutFilterWidget()
{
	CVehicleMetadataMgr& pInstance = CVehicleMetadataMgr::GetInstance();
	if (!&pInstance)
	{
		return;
	}

	atArray<const char*> layoutNames;
	for(s32 i = pInstance.GetVehicleLayoutInfoCount()-1; i >= 0; i--)
	{
		atHashWithStringNotFinal pLayoutHash = pInstance.GetVehicleLayoutInfoAtIndex(i)->GetName();
		const char* cLayoutName = pLayoutHash.GetCStr();
		layoutNames.PushAndGrow(cLayoutName);
	}

	if (layoutNames.GetCount() > 0)
	{
		std::sort(&layoutNames[0],&layoutNames[0] + layoutNames.GetCount(), CVehicleFactory::CbCompareNames);

		layoutNames.PushAndGrow("All");

		currVehicleLayoutSelection = layoutNames.GetCount()-1;
		pLayoutCombo = pBank->AddCombo("Layout Filter", &currVehicleLayoutSelection, layoutNames.GetCount(), &layoutNames[0], UpdateVehicleTypeList);
	}
}

void CVehicleFactory::AddDLCFilterWidget()
{
	atArray<const char*> DLCNames;
	DLCNames.Reset();
	u32 iNumVehicles = g_typeArray.GetCount();

	for (int i=0; i<iNumVehicles; i++)
	{
		atHashString dlcHashName = g_typeArray[i]->GetVehicleDLCPack();
		const char* dlcName = dlcHashName.GetCStr();
		if ( DLCNames.Find(dlcName) < 0)
		{
			DLCNames.PushAndGrow(dlcName);
		}
	}

	numDLCNames = DLCNames.GetCount();

	if (DLCNames.GetCount() > 0)
	{
		std::sort(&DLCNames[0],&DLCNames[0] + DLCNames.GetCount(), CVehicleFactory::CbCompareNames);

		DLCNames.PushAndGrow("All");
		currVehicleDLCSelection = numDLCNames;

		pDLCCombo = pBank->AddCombo("DLC Pack Filter", &currVehicleDLCSelection, numDLCNames+1, &DLCNames[0], UpdateVehicleTypeList);
	}
}

//*******************************************************************************
//	CBoardingPointEditor - this class handles adding of boarding-points to boats.

bool CBoardingPointEditor::ms_bPlacementToolEnabled = false;
int CBoardingPointEditor::ms_iSelectedBoardingPoint = -1;
bool CBoardingPointEditor::ms_bRespositioningBoardingPoint = false;
float g_fNavMaxHeightChange = 0.0f;
float g_fNavMinZDist = 0.75f;
float g_fNavRes = 1.0f;


CBoardingPointEditor::CBoardingPointEditor()
{

}
CBoardingPointEditor::~CBoardingPointEditor()
{
	Shutdown();
}

void CBoardingPointEditor::Init()
{

}
CBoardingPoints * GetBoardingPointsFromVehicle(CVehicle * pVehicle)
{
	return pVehicle->pHandling->m_pBoardingPoints;
}
CBoardingPoints * GetBoardingPointsFromVehicleFactoryIndex(int iVehicle)
{
	int iModelNum = vehicleSorts[iVehicle];
	u32 iModelIndex = GetModelIndex(iModelNum);
	if(iModelIndex != fwModelId::MI_INVALID)
	{
		CVehicleModelInfo * pVehModelInfo = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(iModelIndex)));
		if(pVehModelInfo)
		{
			CHandlingData * pBaseHandling = CHandlingDataMgr::GetHandlingDataByName(CVehicleFactory::vehicleNames[iVehicle]);
			if(pBaseHandling)
			{
				return pBaseHandling->m_pBoardingPoints;
			}
		}
	}
	return NULL;
}

void CBoardingPointEditor::Process()
{
	if(!ms_bPlacementToolEnabled)
		return;

	// Draw the boarding-points for the current boat
	if(CVehicleFactory::ms_pLastCreatedVehicle &&
		(CVehicleFactory::ms_pLastCreatedVehicle->GetVehicleType()==VEHICLE_TYPE_BOAT ||
		CVehicleFactory::ms_pLastCreatedVehicle->GetVehicleType()==VEHICLE_TYPE_PLANE ||
		CVehicleFactory::ms_pLastCreatedVehicle->GetVehicleType()==VEHICLE_TYPE_HELI ||
		CVehicleFactory::ms_pLastCreatedVehicle->GetVehicleType()==VEHICLE_TYPE_TRAIN))
	{
		CBoardingPoints * pBoardingPoints = GetBoardingPointsFromVehicle( CVehicleFactory::ms_pLastCreatedVehicle.Get() );
	
		if(pBoardingPoints && ms_bRespositioningBoardingPoint && ms_iSelectedBoardingPoint>=0 && ms_iSelectedBoardingPoint >= 0 && ms_iSelectedBoardingPoint < pBoardingPoints->m_iNumBoardingPoints)
		{
			Vector3 mouseScreen, mouseFar;
			CDebugScene::GetMousePointing(mouseScreen, mouseFar);

			WorldProbe::CShapeTestProbeDesc probeDesc;
			WorldProbe::CShapeTestFixedResults<> probeResult;
			probeDesc.SetResultsStructure(&probeResult);
			probeDesc.SetStartAndEnd(mouseScreen, mouseFar);
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE);
			if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
			{
				CEntity * pHitEntity = CPhysics::GetEntityFromInst(probeResult[0].GetHitInst());

				if(pHitEntity && pHitEntity==CVehicleFactory::ms_pLastCreatedVehicle)
				{
					Vector3 vHitPos = VEC3V_TO_VECTOR3(pHitEntity->GetTransform().UnTransform(VECTOR3_TO_VEC3V(probeResult[0].GetHitPosition())));

					pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fPosition[0] = vHitPos.x;
					pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fPosition[1] = vHitPos.y;
					pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fPosition[2] = vHitPos.z;
				}
			}
		}

		if(ioMouse::GetPressedButtons()&ioMouse::MOUSE_LEFT)
			ms_bRespositioningBoardingPoint = false;

		if(pBoardingPoints)
		{
			for(int p=0; p<pBoardingPoints->m_iNumBoardingPoints; p++)
			{
				Vector3 vPos(pBoardingPoints->m_BoardingPoints[p].m_fPosition[0], pBoardingPoints->m_BoardingPoints[p].m_fPosition[1], pBoardingPoints->m_BoardingPoints[p].m_fPosition[2]);
				Vector3 vDir(rage::Sinf(pBoardingPoints->m_BoardingPoints[p].m_fHeading), -rage::Cosf(pBoardingPoints->m_BoardingPoints[p].m_fHeading), 0.0f);

				vPos = VEC3V_TO_VECTOR3(CVehicleFactory::ms_pLastCreatedVehicle.Get()->GetTransform().Transform(VECTOR3_TO_VEC3V(vPos)));
				vDir = VEC3V_TO_VECTOR3(CVehicleFactory::ms_pLastCreatedVehicle.Get()->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vDir)));

				static u32 iFlashTime = 0;
				static bool bToggle = false;

				if(fwTimer::GetTimeInMilliseconds()-iFlashTime > 600)
				{
					iFlashTime = fwTimer::GetTimeInMilliseconds();
					bToggle = !bToggle;
				}

				const bool bAltCol = (bToggle && p==ms_iSelectedBoardingPoint);

				const Color32 iSphereCol = bAltCol ? Color_turquoise : Color_turquoise4;
				const Color32 iHdgCol = bAltCol ? Color_red : Color_red4;

				grcDebugDraw::Sphere(vPos, PED_HUMAN_RADIUS, iSphereCol, false);
				grcDebugDraw::Line(vPos, vPos + vDir, iHdgCol);
			}
		}
	}
}

void CBoardingPointEditor::Shutdown()
{

}

void CBoardingPointEditor::CreateWidgets(bkBank * bank)
{
	bank->PushGroup("Boat/Plane/Heli/Train Boarding-Points", false);
	bank->AddToggle("Placement Tool Enabled", &ms_bPlacementToolEnabled);

	bank->AddButton("Create Next Boat/Plane/Heli/Train", CreateNextBoatCB);
	bank->AddButton("Create Prev Boat/Plane/Heli/Train", CreatePrevBoatCB);
	bank->AddButton("Add Boarding-Point", AddBoardingPointCB);
	bank->AddButton("Select Next", SelectNextBoardingPointCB);
	bank->AddButton("Select Prev", SelectPrevBoardingPointCB);
	bank->AddButton("Delete Current", DeleteCurrentBoardingPointCB);
	bank->AddButton("Move Current", MoveCurrentBoardingPointCB);
	bank->AddButton("Rotate Clockwise", RotateClockwiseCurrentBoardingPointCB);
	bank->AddButton("Rotate AntiClockwise", RotateAntiClockwiseCurrentBoardingPointCB);
	bank->AddButton("Move up", MoveUp);
	bank->AddButton("Move down", MoveDown);
	bank->AddButton("Move forward", MoveForward);
	bank->AddButton("Move backward", MoveBackward);
	bank->AddButton("Move left", MoveLeft);
	bank->AddButton("Move right", MoveRight);
	
	bank->AddButton("Save Boarding-Points Data", SaveBoardingPointsCB);

	bank->PopGroup();
}

void CBoardingPointEditor::CreatePrevBoatCB()
{
	CreateNextBoat(-1);
}

void CBoardingPointEditor::CreateNextBoatCB()
{
	CreateNextBoat(1);
}

void CBoardingPointEditor::CreateNextBoat(int iDir)
{
	if(!ms_bPlacementToolEnabled) return;

	// delete existing car if it exists - this widget is meant for cycling through vehicles
	DeleteCarCB();

	bool bFoundBoat = false;
	bool bFoundPlane = false;
	bool bFoundHeli = false;
	bool bFoundTrain = false;

	while(!bFoundBoat && !bFoundPlane && !bFoundHeli && !bFoundTrain)
	{
		CVehicleFactory::currVehicleNameSelection += iDir;

		if(CVehicleFactory::currVehicleNameSelection < 0)
			CVehicleFactory::currVehicleNameSelection = numVehicleNames-1;
		if(CVehicleFactory::currVehicleNameSelection >= numVehicleNames)
			CVehicleFactory::currVehicleNameSelection = 0;

		CVehicleFactory::carModelId = GetModelIndex(vehicleSorts[CVehicleFactory::currVehicleNameSelection]);

		CVehicleModelInfo * pVehModelInfo = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(CVehicleFactory::carModelId)));
		if(pVehModelInfo->GetVehicleType()==VEHICLE_TYPE_BOAT)
			bFoundBoat=true;
		if(pVehModelInfo->GetVehicleType()==VEHICLE_TYPE_PLANE)
			bFoundPlane=true;
		if(pVehModelInfo->GetVehicleType()==VEHICLE_TYPE_HELI)
			bFoundHeli=true;
		if(pVehModelInfo->GetVehicleType()==VEHICLE_TYPE_TRAIN)
			bFoundTrain=true;
	}

	CVehicleFactory::CarNumChangeCB();
	CreateCarCB();

	if(!CVehicleFactory::ms_pLastCreatedVehicle)
		return;
	CBoardingPoints * pBoardingPoints = GetBoardingPointsFromVehicle( CVehicleFactory::ms_pLastCreatedVehicle.Get() );
	if(!pBoardingPoints)
		return;

	g_fNavMaxHeightChange = pBoardingPoints->m_fNavMaxHeightChange;
	g_fNavMinZDist = pBoardingPoints->m_fNavMinZDist;
	g_fNavRes = pBoardingPoints->m_fNavRes;
}

void CBoardingPointEditor::AddBoardingPointCB()
{
	if(!ms_bPlacementToolEnabled) return;
	if(!CVehicleFactory::ms_pLastCreatedVehicle)
		return;

	CBoardingPoints * pBoardingPoints = GetBoardingPointsFromVehicle( CVehicleFactory::ms_pLastCreatedVehicle.Get() );
	if(!pBoardingPoints)
	{
		CVehicle * pVehicle = CVehicleFactory::ms_pLastCreatedVehicle.Get();
		Assert(!pVehicle->pHandling->m_pBoardingPoints);
		pVehicle->pHandling->m_pBoardingPoints = rage_new CBoardingPoints();
		pBoardingPoints = pVehicle->pHandling->m_pBoardingPoints;
	}

	if(pBoardingPoints->m_iNumBoardingPoints < MAX_NUM_BOARDING_POINTS)
	{
		ms_iSelectedBoardingPoint = pBoardingPoints->m_iNumBoardingPoints;
		pBoardingPoints->m_iNumBoardingPoints++;

		pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fPosition[0] = 0.0f;
		pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fPosition[1] = 0.0f;
		pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fPosition[2] = 0.0f;
		pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fHeading = 0.0f;

		ms_bRespositioningBoardingPoint = true;
	}
}

void CBoardingPointEditor::SelectNextBoardingPointCB()
{
	if(!ms_bPlacementToolEnabled) return;
	if(!CVehicleFactory::ms_pLastCreatedVehicle)
		return;

	CBoardingPoints * pBoardingPoints = GetBoardingPointsFromVehicle( CVehicleFactory::ms_pLastCreatedVehicle.Get() );
	if(!pBoardingPoints)
		return;

	if(pBoardingPoints->m_iNumBoardingPoints==0)
	{
		ms_iSelectedBoardingPoint=-1;
		return;
	}

	ms_iSelectedBoardingPoint++;

	if(ms_iSelectedBoardingPoint >= pBoardingPoints->m_iNumBoardingPoints)
		ms_iSelectedBoardingPoint = 0;
}

void CBoardingPointEditor::SelectPrevBoardingPointCB()
{
	if(!ms_bPlacementToolEnabled) return;
	if(!CVehicleFactory::ms_pLastCreatedVehicle)
		return;

	CBoardingPoints * pBoardingPoints = GetBoardingPointsFromVehicle( CVehicleFactory::ms_pLastCreatedVehicle.Get() );
	if(!pBoardingPoints)
		return;

	if(pBoardingPoints->m_iNumBoardingPoints==0)
	{
		ms_iSelectedBoardingPoint=-1;
		return;
	}

	ms_iSelectedBoardingPoint--;

	if(ms_iSelectedBoardingPoint < 0)
		ms_iSelectedBoardingPoint = pBoardingPoints->m_iNumBoardingPoints-1;
}

void CBoardingPointEditor::DeleteCurrentBoardingPointCB()
{
	if(!ms_bPlacementToolEnabled) return;
	if(!CVehicleFactory::ms_pLastCreatedVehicle)
		return;

	CBoardingPoints * pBoardingPoints = GetBoardingPointsFromVehicle( CVehicleFactory::ms_pLastCreatedVehicle.Get() );
	if(!pBoardingPoints)
		return;

	if(ms_iSelectedBoardingPoint < 0 || ms_iSelectedBoardingPoint >= pBoardingPoints->m_iNumBoardingPoints || pBoardingPoints->m_iNumBoardingPoints==0)
	{
		return;
	}

	for(int p=ms_iSelectedBoardingPoint; p<pBoardingPoints->m_iNumBoardingPoints-1; p++)
	{
		pBoardingPoints->m_BoardingPoints[p] = pBoardingPoints->m_BoardingPoints[p+1];
	}
	pBoardingPoints->m_iNumBoardingPoints--;

	ms_iSelectedBoardingPoint = -1;

	if(pBoardingPoints->m_iNumBoardingPoints == 0)
	{
		CVehicle * pVehicle = CVehicleFactory::ms_pLastCreatedVehicle.Get();
		Assert(pVehicle->pHandling->m_pBoardingPoints);
		delete pVehicle->pHandling->m_pBoardingPoints;
		pVehicle->pHandling->m_pBoardingPoints = NULL;
	}
}

void CBoardingPointEditor::MoveCurrentBoardingPointCB()
{
	if(!ms_bPlacementToolEnabled) return;
	if(!CVehicleFactory::ms_pLastCreatedVehicle)
		return;

	CBoardingPoints * pBoardingPoints = GetBoardingPointsFromVehicle( CVehicleFactory::ms_pLastCreatedVehicle.Get() );
	if(!pBoardingPoints)
		return;

	if(ms_iSelectedBoardingPoint < 0 || ms_iSelectedBoardingPoint >= pBoardingPoints->m_iNumBoardingPoints || pBoardingPoints->m_iNumBoardingPoints==0)
	{
		return;
	}

	ms_bRespositioningBoardingPoint = true;
}

void CBoardingPointEditor::RotateClockwiseCurrentBoardingPointCB()
{
	if(!ms_bPlacementToolEnabled) return;
	if(!CVehicleFactory::ms_pLastCreatedVehicle)
		return;

	CBoardingPoints * pBoardingPoints = GetBoardingPointsFromVehicle( CVehicleFactory::ms_pLastCreatedVehicle.Get() );
	if(!pBoardingPoints)
		return;

	if(ms_iSelectedBoardingPoint < 0 || ms_iSelectedBoardingPoint >= pBoardingPoints->m_iNumBoardingPoints || pBoardingPoints->m_iNumBoardingPoints==0)
	{
		return;
	}

	pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fHeading -= ( DtoR * 10.0f);
	if(pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fHeading < 0.0f)
		pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fHeading += PI*2.0f;
}

void CBoardingPointEditor::RotateAntiClockwiseCurrentBoardingPointCB()
{
	if(!ms_bPlacementToolEnabled) return;
	if(!CVehicleFactory::ms_pLastCreatedVehicle)
		return;

	CBoardingPoints * pBoardingPoints = GetBoardingPointsFromVehicle( CVehicleFactory::ms_pLastCreatedVehicle.Get() );
	if(!pBoardingPoints)
		return;

	if(ms_iSelectedBoardingPoint < 0 || ms_iSelectedBoardingPoint >= pBoardingPoints->m_iNumBoardingPoints || pBoardingPoints->m_iNumBoardingPoints==0)
	{
		return;
	}

	pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fHeading += ( DtoR * 10.0f);
	if(pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fHeading >= PI*2.0f)
		pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fHeading -= PI*2.0f;
}
dev_float MOVE_SPEED = 0.05f;
void CBoardingPointEditor::MoveUp()
{
	if(!ms_bPlacementToolEnabled) return;
	if(!CVehicleFactory::ms_pLastCreatedVehicle)
		return;

	CBoardingPoints * pBoardingPoints = GetBoardingPointsFromVehicle( CVehicleFactory::ms_pLastCreatedVehicle.Get() );
	if(!pBoardingPoints)
		return;

	if(ms_iSelectedBoardingPoint < 0 || ms_iSelectedBoardingPoint >= pBoardingPoints->m_iNumBoardingPoints || pBoardingPoints->m_iNumBoardingPoints==0)
	{
		return;
	}

	pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fPosition[2] += MOVE_SPEED;
}

void CBoardingPointEditor::MoveDown()
{
	if(!ms_bPlacementToolEnabled) return;
	if(!CVehicleFactory::ms_pLastCreatedVehicle)
		return;

	CBoardingPoints * pBoardingPoints = GetBoardingPointsFromVehicle( CVehicleFactory::ms_pLastCreatedVehicle.Get() );
	if(!pBoardingPoints)
		return;

	if(ms_iSelectedBoardingPoint < 0 || ms_iSelectedBoardingPoint >= pBoardingPoints->m_iNumBoardingPoints || pBoardingPoints->m_iNumBoardingPoints==0)
	{
		return;
	}

	pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fPosition[2] -= MOVE_SPEED;
}

void CBoardingPointEditor::MoveForward()
{
	if(!ms_bPlacementToolEnabled) return;
	if(!CVehicleFactory::ms_pLastCreatedVehicle)
		return;

	CBoardingPoints * pBoardingPoints = GetBoardingPointsFromVehicle( CVehicleFactory::ms_pLastCreatedVehicle.Get() );
	if(!pBoardingPoints)
		return;

	if(ms_iSelectedBoardingPoint < 0 || ms_iSelectedBoardingPoint >= pBoardingPoints->m_iNumBoardingPoints || pBoardingPoints->m_iNumBoardingPoints==0)
	{
		return;
	}

	Vector3 vHeading(0.0f, 1.0f, 0.0f);
	vHeading.RotateZ(pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fHeading);

	
	pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fPosition[0] -= vHeading.x * MOVE_SPEED;
	pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fPosition[1] -= vHeading.y * MOVE_SPEED;
}

void CBoardingPointEditor::MoveBackward()
{
	if(!ms_bPlacementToolEnabled) return;
	if(!CVehicleFactory::ms_pLastCreatedVehicle)
		return;

	CBoardingPoints * pBoardingPoints = GetBoardingPointsFromVehicle( CVehicleFactory::ms_pLastCreatedVehicle.Get() );
	if(!pBoardingPoints)
		return;

	if(ms_iSelectedBoardingPoint < 0 || ms_iSelectedBoardingPoint >= pBoardingPoints->m_iNumBoardingPoints || pBoardingPoints->m_iNumBoardingPoints==0)
	{
		return;
	}

	Vector3 vHeading(0.0f, 1.0f, 0.0f);
	vHeading.RotateZ(pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fHeading);

	pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fPosition[0] += vHeading.x * MOVE_SPEED;
	pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fPosition[1] += vHeading.y * MOVE_SPEED;
}

void CBoardingPointEditor::MoveLeft()
{
	if(!ms_bPlacementToolEnabled) return;
	if(!CVehicleFactory::ms_pLastCreatedVehicle)
		return;

	CBoardingPoints * pBoardingPoints = GetBoardingPointsFromVehicle( CVehicleFactory::ms_pLastCreatedVehicle.Get() );
	if(!pBoardingPoints)
		return;

	if(ms_iSelectedBoardingPoint < 0 || ms_iSelectedBoardingPoint >= pBoardingPoints->m_iNumBoardingPoints || pBoardingPoints->m_iNumBoardingPoints==0)
	{
		return;
	}

	Vector3 vHeading(1.0f, 0.0f, 0.0f);
	vHeading.RotateZ(pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fHeading);

	pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fPosition[0] += vHeading.x * MOVE_SPEED;
	pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fPosition[1] += vHeading.y * MOVE_SPEED;
}

void CBoardingPointEditor::MoveRight()
{
	if(!ms_bPlacementToolEnabled) return;
	if(!CVehicleFactory::ms_pLastCreatedVehicle)
		return;

	CBoardingPoints * pBoardingPoints = GetBoardingPointsFromVehicle( CVehicleFactory::ms_pLastCreatedVehicle.Get() );
	if(!pBoardingPoints)
		return;

	if(ms_iSelectedBoardingPoint < 0 || ms_iSelectedBoardingPoint >= pBoardingPoints->m_iNumBoardingPoints || pBoardingPoints->m_iNumBoardingPoints==0)
	{
		return;
	}

	Vector3 vHeading(1.0f, 0.0f, 0.0f);
	vHeading.RotateZ(pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fHeading);

	pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fPosition[0] -= vHeading.x * MOVE_SPEED;
	pBoardingPoints->m_BoardingPoints[ms_iSelectedBoardingPoint].m_fPosition[1] -= vHeading.y * MOVE_SPEED;
}

void CBoardingPointEditor::SaveBoardingPointsCB()
{
	if(!ms_bPlacementToolEnabled) return;

	Assertf(0, "The game will now prompt for a text file to ouput the boarding points.\nThese should then be **copied/pasted** into \"VehicleExtras.dat\" in the appropriate section.\nDo not save the boarding-points data directly over \"VehicleExtras.dat\"!!");

	char filename[512];
	bool bOk = BANKMGR.OpenFile(filename, 512, "*.txt", true, "Save boarding-points to text file");

	if(!bOk)
		return;

	fiStream * pFile = fiStream::Create(filename);
	Assert(pFile);
	if(!pFile)
		return;

	const char * pDesc = ";Start of boarding-points section\r\n;This section should be copied into \"VehicleExtras.dat\"\r\n\r\n";
	pFile->Write(pDesc, istrlen(pDesc));

	// First count how many boats we have
	int iNumVehiclesWithBoardingPoints = 0;
	int iVehicle;

	for(iVehicle=0; iVehicle<numVehicleNames; iVehicle++)
	{
		CBoardingPoints * pBoardingPoints = GetBoardingPointsFromVehicleFactoryIndex(iVehicle);
		if(pBoardingPoints)
			iNumVehiclesWithBoardingPoints++;
	}

	char buffer[256];

	sprintf(buffer, "START_BOARDING_POINTS\r\n\r\n");
	pFile->Write(buffer, istrlen(buffer));

	for(iVehicle=0; iVehicle<numVehicleNames; iVehicle++)
	{
		CBoardingPoints * pBoardingPoints = GetBoardingPointsFromVehicleFactoryIndex(iVehicle);
		if(pBoardingPoints)
		{
			if(pBoardingPoints && pBoardingPoints->m_iNumBoardingPoints)
			{
				sprintf(buffer, "VEHICLE_NAME %s NUM_BOARDING_POINTS %i NAV_MAXHEIGHTCHANGE %f NAV_MINZDIST %f NAV_RES %f\r\n\r\n", CVehicleFactory::vehicleNames[iVehicle], pBoardingPoints->m_iNumBoardingPoints, pBoardingPoints->m_fNavMaxHeightChange, pBoardingPoints->m_fNavMinZDist, pBoardingPoints->m_fNavRes);
				pFile->Write(buffer, istrlen(buffer));

				for(int p=0; p<pBoardingPoints->m_iNumBoardingPoints; p++)
				{
					CBoardingPoint & bp = pBoardingPoints->m_BoardingPoints[p];

					sprintf(buffer, "X %.2f Y %.2f Z %.2f HEADING %.2f\r\n", bp.m_fPosition[0], bp.m_fPosition[1], bp.m_fPosition[2], bp.m_fHeading);
					pFile->Write(buffer, istrlen(buffer));
				}

				sprintf(buffer, "\r\n");
				pFile->Write(buffer, istrlen(buffer));
			}
		}
	}
	sprintf(buffer, "END_BOARDING_POINTS\r\n\r\n");
	pFile->Write(buffer, istrlen(buffer));
	sprintf(buffer, ";End of boarding-points section\r\n\r\n");
	pFile->Write(buffer, istrlen(buffer));

	pFile->Close();
}


#endif //__BANK

void CVehicleFactory::AddDestroyedVehToCache(CVehicle* veh)
{
	if (!veh)
		return;

	GPU_VEHICLE_DAMAGE_ONLY(CApplyDamage::RemoveVehicleDeformation(veh));
	GPU_VEHICLE_DAMAGE_ONLY(CPhysics::RemoveVehicleDeformation(veh));

	REPLAY_ONLY(CReplayMgr::OnDelete(veh));

	// if we have the cache turned off or this is a modded vehicle, free it now
	if (!ms_reuseDestroyedVehs || veh->GetStatus() == STATUS_WRECKED || veh->IsModded() || veh == CGameWorld::FindLocalPlayerVehicle())
	{
		delete veh;
		return;
	}

	// Ensure that any vehicle being removed to the cache is removed from the pathserver also
	CPathServerGta::MaybeRemoveDynamicObject(veh);

	// Remove refs on this vehicle's dynamic navmesh if it has one
	CPathServerGta::MaybeRemoveDynamicObjectNavMesh(veh);


	// make sure we don't already have this pointer in the list
	s32 firstFree = -1;
	for (s32 i = 0; i < MAX_DESTROYED_VEHS_CACHED; ++i)
	{
		// skip if this vehicle is already on the list
		if (ms_reuseDestroyedVehArray[i].veh == veh)
			return;

		if (firstFree == -1 && ms_reuseDestroyedVehArray[i].veh == NULL)
			firstFree = i;
	}

	// we have a slot, cache the vehicle
	if (firstFree > -1)
	{
		// Clean dummy transition surface
		CWheel * const * ppWheels = veh->GetWheels();
		for(int i=0; i<veh->GetNumWheels(); i++)
		{
			ppWheels[i]->EndDummyTransition();
		}

		veh->SetIsInPopulationCache(true);

		// clean up peds in vehicle
		for( s32 i = 0; i < MAX_VEHICLE_SEATS; i++ )
		{
			CPed* pPed = veh->GetSeatManager()->GetPedInSeat(i);
			if (pPed)
			{
				Assert (!pPed->IsNetworkClone());
				const bool bIsNetworkClone = pPed->GetNetworkObject() && !pPed->GetNetworkObject()->CanDelete();

				// Dont delete mission or player peds or network clones, flushing immediately sets them outside the vehicle.
				if (bIsNetworkClone || pPed->IsAPlayerPed() || pPed->PopTypeIsMission() || !CPedFactory::GetFactory()->DestroyPed(pPed, true))
					pPed->GetPedIntelligence()->FlushImmediately();

				Assertf(!veh->GetSeatManager()->GetPedInSeat(i), "CVehicleFactory::AddDestroyedVehToCache - expected the vehicle's passenger to have been removed");
			}
		}
		veh->ResetSeatManagerForReuse();
		veh->ResetExclusiveDriverForReuse();
		BANK_ONLY(veh->ResetDebugObjectID());
		veh->SetLastTimeHomedAt(0);
		veh->SetEasyToLand(false);

		for( s32 i = 0; i < MAX_NUM_VEHICLE_WEAPONS; i++ )
		{
			veh->SetRestrictedAmmoCount(i, -1);
		}

		veh->m_nCloneFlaggedForWreckedOrEmptyDeletionTime = 0;
		
		if(veh->GetVehicleAudioEntity()) 
		{
			veh->GetVehicleAudioEntity()->Shutdown();
		}

		veh->m_nVehicleFlags.bUsingPretendOccupants = false;
		veh->GetIntelligence()->FlushPretendOccupantEventGroup();
		veh->GetIntelligence()->ResetPretendOccupantEventData();
		veh->GetIntelligence()->SetNumPedsThatNeedTaskFromPretendOccupant(0);

		veh->CarAlarmState = CAR_ALARM_NONE;

		veh->RemoveBlip(BLIP_TYPE_CAR);

		// detach trailers and towed cars
		for (s32 i = 0; i < veh->GetNumberOfVehicleGadgets(); ++i)
		{
			CVehicleGadget* gadget = veh->GetVehicleGadget(i);
			if (!gadget)
				continue;

			if(gadget->GetType() == VGT_TRAILER_ATTACH_POINT)
			{
				CVehicleTrailerAttachPoint* attachPoint = static_cast<CVehicleTrailerAttachPoint*>(gadget);
				attachPoint->DetachTrailer(veh);
			}
			else if(gadget->GetType() == VGT_TOW_TRUCK_ARM)
			{
				CVehicleGadgetTowArm* towArm = static_cast<CVehicleGadgetTowArm*>(gadget);
				towArm->DeleteRopesHookAndConstraints(veh);
			}
			else if(gadget->GetType() == VGT_PICK_UP_ROPE || gadget->GetType() == VGT_PICK_UP_ROPE_MAGNET)
			{
				CVehicleGadgetPickUpRope* pPickUpRope = static_cast<CVehicleGadgetPickUpRope*>(gadget);
				pPickUpRope->DeleteRopesPropAndConstraints(veh);
			}
		}

		veh->SetIsScheduled(false);
		veh->GetPortalTracker()->DontRequestScaneNextUpdate();
		veh->GetPortalTracker()->StopScanUntilProbeTrue();

		// flush tasks
		veh->GetIntelligence()->GetTaskManager()->AbortTasks();

		veh->GetFrameCollisionHistory()->Reset();
		veh->GetIntelligence()->ClearScanners();

		if(veh->GetParentTrailer())
			veh->GetParentTrailer()->RemoveCargoVehicle(veh);
		veh->SetParentTrailer(NULL);

		if(veh->GetDummyAttachmentParent())
			veh->GetDummyAttachmentParent()->DummyDetachChild(veh);

		if(veh->HasDummyAttachmentChildren())
			veh->DummyDetachAllChildren();

		// remove script guid so script can't grab this ped pointer while it's in the cache
		fwScriptGuid::DeleteGuid(*veh);

		veh->GetIntelligence()->SetCarWeAreBehind(NULL);

		veh->SetVelocity(VEC3_ZERO);
		veh->SetAngVelocity(VEC3_ZERO);
		veh->SetSuperDummyVelocity(VEC3_ZERO);

		if (veh->InheritsFromAutomobile())
			((CAutomobile*)veh)->RemoveConstraints();
		else if (veh->InheritsFromBike())
			((CBike*)veh)->RemoveConstraint();
		else if (veh->InheritsFromBoat())
		{
			CBoat* boat = (CBoat*)veh;
			boat->DetachFromParentAndChildren(DETACH_FLAG_DONT_REMOVE_BASIC_ATTACHMENTS);
		}

		if (veh->GetDummyInst())
		{
			veh->GetDummyInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, false);
			
			if (veh->GetDummyInst()->IsInLevel())
			{
				CPhysics::GetLevel()->SetInstanceTypeAndIncludeFlags(veh->GetDummyInst()->GetLevelIndex(), 0, 0);
				Vec3V vPos = veh->GetDummyInst()->GetPosition();
				vPos.SetZf(-100.0f);
				veh->GetDummyInst()->SetPosition(vPos);
				if (CPhysics::GetLevel()->IsActive(veh->GetDummyInst()->GetLevelIndex()))
					CPhysics::GetSimulator()->DeactivateObject(veh->GetDummyInst()->GetLevelIndex());

				veh->RemoveConstraints(veh->GetDummyInst());
			}
		}

		if (veh->GetVehicleFragInst())
		{
			veh->GetVehicleFragInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, false);

			if (veh->GetVehicleFragInst()->IsInLevel())
			{
				CPhysics::GetLevel()->SetInstanceTypeAndIncludeFlags(veh->GetVehicleFragInst()->GetLevelIndex(), 0, 0);
				Vec3V vPos = veh->GetVehicleFragInst()->GetPosition();
				vPos.SetZf(-100.0f);
				veh->GetVehicleFragInst()->SetPosition(vPos);
				if (CPhysics::GetLevel()->IsActive(veh->GetVehicleFragInst()->GetLevelIndex()))
					CPhysics::GetSimulator()->DeactivateObject(veh->GetVehicleFragInst()->GetLevelIndex());

				veh->RemoveConstraints(veh->GetFragInst());
			}
		}

		veh->ClearAllKnownRefs();

		ms_reuseDestroyedVehArray[firstFree].veh = veh;
		ms_reuseDestroyedVehArray[firstFree].nukeTime = fwTimer::GetTimeInMilliseconds() + ms_reuseDestroyedVehCacheTime;
		ms_reuseDestroyedVehArray[firstFree].framesUntilReuse = 5;
		ms_reuseDestroyedVehCount++;

		Assert(ms_reuseDestroyedVehArray[firstFree].veh);

#if __ASSERT
		if(veh->InheritsFromAutomobile())
		{
			CAutomobile * pAuto = static_cast<CAutomobile*>(veh);
			Assert(!pAuto->m_dummyStayOnRoadConstraintHandle.IsValid());
			Assert(!pAuto->m_dummyRotationalConstraint.IsValid());
		}
#endif

		veh->PopTypeSet(POPTYPE_CACHE);

		return;
	}

	// no space left, destroy vehicle
	delete veh;
}

bool CVehicleFactory::IsVehInDestroyedCache(CVehicle* veh)
{
	if (!veh)
		return true;

	for (s32 i = 0; i < MAX_DESTROYED_VEHS_CACHED; ++i)
	{
		if (ms_reuseDestroyedVehArray[i].veh && ms_reuseDestroyedVehArray[i].veh == veh)
			return true;
	}

	return false;
}

void CVehicleFactory::ClearDestroyedVehCache()
{
	for (s32 i = 0; i < MAX_DESTROYED_VEHS_CACHED; ++i)
	{
		if (ms_reuseDestroyedVehArray[i].veh)
		{
			CVehicle* veh = ms_reuseDestroyedVehArray[i].veh;
			ms_reuseDestroyedVehArray[i].veh = NULL;
			veh->SetIsInPopulationCache(false);

			// we can delete the vehicle like this because we've done all work of removing it from the world when it was added to this list
			delete veh;
		}
	}
	ms_reuseDestroyedVehCount = 0;
}

void CVehicleFactory::RemoveVehGroupFromCache(const CLoadedModelGroup& vehGroup)
{
	const u32 numVehsInGroup = vehGroup.CountMembers();
	if (!numVehsInGroup)
		return;

	for (s32 i = 0; i < MAX_DESTROYED_VEHS_CACHED; ++i)
	{
		for (s32 f = 0; f < numVehsInGroup; ++f)
		{
			u32 vehModelIndex = vehGroup.GetMember(f);
			if (vehModelIndex == fwModelId::MI_INVALID)
				continue;

			if (ms_reuseDestroyedVehArray[i].veh && ms_reuseDestroyedVehArray[i].veh->GetModelIndex() == vehModelIndex)
			{
				CVehicle* veh = ms_reuseDestroyedVehArray[i].veh;
				ms_reuseDestroyedVehArray[i].veh = NULL;
				veh->SetIsInPopulationCache(false);

				// we can delete the vehicle like this because we've done all work of removing it from the world when it was added to this list
				delete veh;
				ms_reuseDestroyedVehCount--;
			}
		}
	}
}

void CVehicleFactory::ProcessDestroyedVehsCache()
{
	u32 curTime = fwTimer::GetTimeInMilliseconds();
	for (s32 i = 0; i < MAX_DESTROYED_VEHS_CACHED; ++i)
	{
		if (ms_reuseDestroyedVehArray[i].veh)
		{
			if (curTime >= ms_reuseDestroyedVehArray[i].nukeTime)
			{
				CVehicle* veh = ms_reuseDestroyedVehArray[i].veh;
				ms_reuseDestroyedVehArray[i].veh = NULL;
				veh->SetIsInPopulationCache(false);

				// we can delete the vehicle like this because we've done all work of removing it from the world when it was added to this list
				delete veh;
				ms_reuseDestroyedVehCount--;
			}
			else
				ms_reuseDestroyedVehArray[i].framesUntilReuse = (s8)rage::Max(0, ms_reuseDestroyedVehArray[i].framesUntilReuse - 1);
		}
	}

#if __BANK
	if (ms_reuseDestroyedVehsDebugOutput)
	{
		u32 numCreatedCars = ms_numCachedCarsCreated + ms_numNonCachedCarsCreated;
		float usage = (numCreatedCars > 0) ? (ms_numCachedCarsCreated / (float)numCreatedCars) : 0.f;
		grcDebugDraw::AddDebugOutput("Cache usage: %.0f%% (cached %d - non cached %d)", usage * 100.f, ms_numCachedCarsCreated, ms_numNonCachedCarsCreated);
		grcDebugDraw::AddDebugOutput("Cached destroyed vehs: %d", ms_reuseDestroyedVehCount);
		u32 testCount = 0;

		for (s32 i = 0; i < MAX_DESTROYED_VEHS_CACHED; ++i)
		{
			if (ms_reuseDestroyedVehArray[i].veh)
			{
				CVehicle* veh = ms_reuseDestroyedVehArray[i].veh;
				grcDebugDraw::AddDebugOutput("Veh %2d - 0x%8x - %s - %d", testCount + 1, veh, veh->GetModelName(), (ms_reuseDestroyedVehArray[i].nukeTime - fwTimer::GetTimeInMilliseconds()) / 1000);
				testCount++;
			}
		}
		Assertf(testCount == ms_reuseDestroyedVehCount, "Reuse destroyed vehicle count is out of sync with array! (%d - %d)", testCount, ms_reuseDestroyedVehCount);
	}

	if (ms_resetCacheStats)
	{
		ms_numCachedCarsCreated = 0;
		ms_numNonCachedCarsCreated = 0;
		ms_resetCacheStats = false;
	}
#endif
}

CVehicle* CVehicleFactory::CreateVehFromDestroyedCache(fwModelId modelId, const eEntityOwnedBy ownedBy, u32 popType, const Matrix34 *pMat, bool bCreateAsInactive)
{
	// find a vehicle with the same model id in the cache
	CVehicle* veh = NULL;
	s32 cacheIndex = -1;
	for (s32 i = 0; i < MAX_DESTROYED_VEHS_CACHED; ++i)
	{
		if (ms_reuseDestroyedVehArray[i].veh && ms_reuseDestroyedVehArray[i].veh->GetModelIndex() == modelId.GetModelIndex() && ms_reuseDestroyedVehArray[i].framesUntilReuse == 0)
		{
			veh = ms_reuseDestroyedVehArray[i].veh;
			cacheIndex = i;
			break;
		}
	}

	if (!veh)
		return NULL;
	Assert(cacheIndex != -1);

#if __ASSERT
	if (veh->HasAnExclusiveDriver())
	{
		aiAssertf(0, "Newly created vehicle %s didn't have it's exclusive driver cleared", veh->GetNetworkObject() ? veh->GetNetworkObject()->GetLogName() : veh->GetModelName());
	}
#endif // __ASSERT

	if (!aiVerifyf(veh->IsCollisionEnabled(), "Newly created vehicle %s doesn't have collision enabled from after reuse, force enabling collision", veh->GetNetworkObject() ? veh->GetNetworkObject()->GetLogName() : veh->GetModelName()))
	{
		veh->EnableCollision();
	}

	// set default values, as if ped was newly created by the factory
	veh->SetOwnedBy(ownedBy);
	veh->DelayedRemovalTimeReset();

	veh->m_nVehicleFlags.bCreatedByFactory = true;
	veh->m_nVehicleFlags.bInfluenceWantedLevel = true;
	veh->m_nVehicleFlags.bCreatedByCarGen = false;

	if (veh->GetDummyInst() && veh->GetDummyInst()->IsInLevel())
	{
		CPhysics::GetLevel()->SetInstanceTypeAndIncludeFlags(veh->GetDummyInst()->GetLevelIndex(), ArchetypeFlags::GTA_BOX_VEHICLE_TYPE, ArchetypeFlags::GTA_BOX_VEHICLE_INCLUDE_TYPES);
		veh->SwitchCurrentPhysicsInst(veh->GetDummyInst());
		//Assertf(!CPhysics::GetLevel()->IsActive(veh->GetDummyInst()->GetLevelIndex()), "Vehicle '%s' in cache has its dummy instance activated!", veh->GetModelName());
	}
	if (veh->GetVehicleFragInst() && veh->GetVehicleFragInst()->IsInLevel())
	{
		CPhysics::GetLevel()->SetInstanceTypeAndIncludeFlags(veh->GetVehicleFragInst()->GetLevelIndex(), ArchetypeFlags::GTA_VEHICLE_TYPE, ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES);
		veh->SwitchCurrentPhysicsInst(veh->GetVehicleFragInst());
		//Assertf(!CPhysics::GetLevel()->IsActive(veh->GetVehicleFragInst()->GetLevelIndex()), "Vehicle '%s' in cache has its frag instance activated!", veh->GetModelName());
	}
	Assert(veh->GetCurrentPhysicsInst()->IsInLevel());

	veh->SetIsInPopulationCache(false);

	veh->Teleport(pMat->d, -10.0f, false, false);
	if(pMat)
		veh->SetMatrix(*pMat, true, true, true);
	else
	{
		Matrix34 identity(Matrix34::IdentityType);
		veh->SetMatrix(identity, true, true, true);
	}

	veh->m_nVehicleFlags.bCreatingAsInactive = bCreateAsInactive;

	veh->m_TimeOfCreation = fwTimer::GetTimeInMilliseconds();
	veh->m_CarGenThatCreatedUs = -1;

	veh->CalculateInitialLightStateBasedOnClock();

	veh->m_dummyConvertAttemptTimeMs = 0;
	veh->m_realConvertAttemptTimeMs = 0;
	veh->m_nRealVehicleIncludeFlags = 0;
	//veh->m_NoHornCount = 0;

	// intelligence
	veh->GetIntelligence()->ResetVariables(fwTimer::GetTimeStep());
	veh->GetIntelligence()->m_pCarThatIsBlockingUs = NULL;
	veh->GetIntelligence()->LastTimeNotStuck = fwTimer::GetTimeInMilliseconds();
	veh->GetIntelligence()->LastTimeHonkedAt = 0;

	// Properly remove from any junction we might be in.
	CVehicleIntelligence* pIntel = veh->GetIntelligence();
	if(!pIntel->GetJunctionNode().IsEmpty())
	{
		CJunctions::RemoveVehicleFromJunction(veh, pIntel->GetJunctionNode());
	}
	pIntel->ResetCachedJunctionInfo();

	veh->GetIntelligence()->InvalidateCachedNodeList();
	veh->GetIntelligence()->InvalidateCachedFollowRoute();
	veh->GetIntelligence()->MillisecondsNotMoving = 0;
	veh->GetIntelligence()->m_fSmoothedSpeedSq = 0.0f;
	veh->GetIntelligence()->DeleteScenarioPointHistory();
	veh->GetIntelligence()->m_BackupFollowRoute.Invalidate();
	veh->GetIntelligence()->m_nTimeLastTriedToShakePedFromRoof = 0;
	veh->m_nVehicleFlags.bAvoidanceDirtyFlag = false;	
	veh->m_nVehicleFlags.SetSirenOn(false, false);
	veh->m_nVehicleFlags.SetSirenMutedByScript(false);
	veh->m_nVehicleFlags.bMadDriver = false;
	veh->m_nVehicleFlags.bSlowChillinDriver = false;
	veh->m_nVehicleFlags.bInMotionFromExplosion = false;
	veh->m_nVehicleFlags.bIsAmbulanceOnDuty = false;
	veh->m_nVehicleFlags.bIsFireTruckOnDuty = false;
	veh->m_nVehicleFlags.bIsRacing = false;
	veh->GetIntelligence()->SetPretendOccupantAggressivness(-1.0f);
	veh->GetIntelligence()->SetPretendOccupantAbility(-1.0f);
	veh->GetIntelligence()->SetCustomPathNodeStreamingRadius(-1.0f);

	veh->GetPortalTracker()->Teleport();
	veh->GetPortalTracker()->RequestRescanNextUpdate();

	veh->PopTypeSet((ePopType)popType);

	if (veh->IsDummy())
	{
		if(veh->InheritsFromTrailer())
		{
			// CTrailer::TryToMakeFromDummy fails because it expect to be converted from the parent vehicle's TryToMakeFromDummy.  Just call directly here.
			veh->CVehicle::TryToMakeFromDummy();
		}
		else
		{
			veh->TryToMakeFromDummy();
		}
	}

	veh->Fix(false);

	if(veh->GetVehicleAudioEntity())
	{
		veh->GetVehicleAudioEntity()->Init(veh);
	}

	// clear cache entry
	ms_reuseDestroyedVehArray[cacheIndex].veh = NULL;
	ms_reuseDestroyedVehCount--;

	BANK_ONLY(veh->AssignDebugObjectID());

	REPLAY_ONLY(CReplayMgr::OnCreateEntity(veh));

	return veh;
}

void CVehicleFactory::SetFarDraw(CVehicle* pVehicle)
{
	if (ms_bFarDrawVehicles)
	{
		if (!pVehicle->m_nVehicleFlags.bIsFarDrawEnabled)
		{
			pVehicle->SetLodDistance((u32)(2.0f * pVehicle->GetLodDistance()));		// far draw doubles the fade distance
			pVehicle->m_nVehicleFlags.bIsFarDrawEnabled = true;
		}
	} else {
		if (pVehicle->m_nVehicleFlags.bIsFarDrawEnabled)
		{
			pVehicle->SetLodDistance((u32)(0.5f * pVehicle->GetLodDistance()));		// reset fade distance to original value
			pVehicle->m_nVehicleFlags.bIsFarDrawEnabled = false;
		}
	}
}

void CVehicleFactory::SetFarDrawAllVehicles(bool val)
{
	 ms_bFarDrawVehicles = val;

	 CVehicle::Pool* pVehiclePool = CVehicle::GetPool();
	 Assert(pVehiclePool);

	 for(u32 i = 0; i<pVehiclePool->GetSize(); i++){
		 CVehicle* pVehicle = pVehiclePool->GetSlot(i);
		 if (pVehicle)
		 {
			SetFarDraw(pVehicle);
		 }
	 }

}

